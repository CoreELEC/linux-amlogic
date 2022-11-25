// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Google LLC
 * Author: Quentin Perret <qperret@google.com>
 */

#include <linux/kvm_host.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pgtable.h>
#include <asm/kvm_pkvm.h>
#include <asm/stage2_pgtable.h>

#include <hyp/adjust_pc.h>
#include <hyp/fault.h>

#include <nvhe/gfp.h>
#include <nvhe/iommu.h>
#include <nvhe/memory.h>
#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>
#include <nvhe/pkvm.h>

#define KVM_HOST_S2_FLAGS (KVM_PGTABLE_S2_NOFWB | KVM_PGTABLE_S2_IDMAP)

extern unsigned long hyp_nr_cpus;
struct host_kvm host_kvm;

static struct hyp_pool host_s2_pool;

static pkvm_id pkvm_guest_id(struct kvm_vcpu *vcpu)
{
	return vcpu->arch.hw_mmu->vmid.vmid;

}

static DEFINE_PER_CPU(struct kvm_shadow_vm *, __current_vm);
#define current_vm (*this_cpu_ptr(&__current_vm))

static void __guest_lock(struct kvm_shadow_vm *vm)
{
	hyp_spin_lock(&vm->lock);
	current_vm = vm;
}

static void __guest_unlock(struct kvm_shadow_vm *vm)
{
	current_vm = NULL;
	hyp_spin_unlock(&vm->lock);
}

static void host_lock_component(void)
{
	hyp_spin_lock(&host_kvm.lock);
}

static void host_unlock_component(void)
{
	hyp_spin_unlock(&host_kvm.lock);
}

static void hyp_lock_component(void)
{
	hyp_spin_lock(&pkvm_pgd_lock);
}

static void hyp_unlock_component(void)
{
	hyp_spin_unlock(&pkvm_pgd_lock);
}

static void guest_lock_component(struct kvm_vcpu *vcpu)
{
	__guest_lock(vcpu->arch.pkvm.shadow_vm);
}

static void guest_unlock_component(struct kvm_vcpu *vcpu)
{
	__guest_unlock(vcpu->arch.pkvm.shadow_vm);
}

static void *host_s2_zalloc_pages_exact(size_t size)
{
	void *addr = hyp_alloc_pages(&host_s2_pool, get_order(size));

	hyp_split_page(hyp_virt_to_page(addr));

	/*
	 * The size of concatenated PGDs is always a power of two of PAGE_SIZE,
	 * so there should be no need to free any of the tail pages to make the
	 * allocation exact.
	 */
	WARN_ON(size != (PAGE_SIZE << get_order(size)));

	return addr;
}

static void *host_s2_zalloc_page(void *pool)
{
	return hyp_alloc_pages(pool, 0);
}

static void host_s2_get_page(void *addr)
{
	hyp_get_page(&host_s2_pool, addr);
}

static void host_s2_put_page(void *addr)
{
	hyp_put_page(&host_s2_pool, addr);
}

static int prepare_s2_pool(void *pgt_pool_base)
{
	unsigned long nr_pages, pfn;
	int ret;

	pfn = hyp_virt_to_pfn(pgt_pool_base);
	nr_pages = host_s2_pgtable_pages();
	ret = hyp_pool_init(&host_s2_pool, pfn, nr_pages, 0);
	if (ret)
		return ret;

	host_kvm.mm_ops = (struct kvm_pgtable_mm_ops) {
		.zalloc_pages_exact = host_s2_zalloc_pages_exact,
		.zalloc_page = host_s2_zalloc_page,
		.phys_to_virt = hyp_phys_to_virt,
		.virt_to_phys = hyp_virt_to_phys,
		.page_count = hyp_page_count,
		.get_page = host_s2_get_page,
		.put_page = host_s2_put_page,
	};

	return 0;
}

static void prepare_host_vtcr(void)
{
	u32 parange, phys_shift;

	/* The host stage 2 is id-mapped, so use parange for T0SZ */
	parange = kvm_get_parange(id_aa64mmfr0_el1_sys_val);
	phys_shift = id_aa64mmfr0_parange_to_phys_shift(parange);

	host_kvm.arch.vtcr = kvm_get_vtcr(id_aa64mmfr0_el1_sys_val,
					  id_aa64mmfr1_el1_sys_val, phys_shift);
}

static bool host_stage2_force_pte_cb(u64 addr, u64 end, enum kvm_pgtable_prot prot);

int kvm_host_prepare_stage2(void *pgt_pool_base)
{
	struct kvm_s2_mmu *mmu = &host_kvm.arch.mmu;
	int ret;

	prepare_host_vtcr();
	hyp_spin_lock_init(&host_kvm.lock);
	mmu->arch = &host_kvm.arch;

	ret = prepare_s2_pool(pgt_pool_base);
	if (ret)
		return ret;

	ret = __kvm_pgtable_stage2_init(&host_kvm.pgt, mmu,
					&host_kvm.mm_ops, KVM_HOST_S2_FLAGS,
					host_stage2_force_pte_cb);
	if (ret)
		return ret;

	mmu->pgd_phys = __hyp_pa(host_kvm.pgt.pgd);
	mmu->pgt = &host_kvm.pgt;
	WRITE_ONCE(mmu->vmid.vmid_gen, 0);
	WRITE_ONCE(mmu->vmid.vmid, 0);

	return 0;
}

static bool guest_stage2_force_pte_cb(u64 addr, u64 end, enum kvm_pgtable_prot prot)
{
	return true;
}

static void *guest_s2_zalloc_pages_exact(size_t size)
{
	void *addr = hyp_alloc_pages(&current_vm->pool, get_order(size));

	WARN_ON(size != (PAGE_SIZE << get_order(size)));
	hyp_split_page(hyp_virt_to_page(addr));

	return addr;
}

static void guest_s2_free_pages_exact(void *addr, unsigned long size)
{
	u8 order = get_order(size);
	unsigned int i;

	for (i = 0; i < (1 << order); i++)
		hyp_put_page(&current_vm->pool, addr + (i * PAGE_SIZE));
}

static void *guest_s2_zalloc_page(void *mc)
{
	struct hyp_page *p;
	void *addr;

	addr = hyp_alloc_pages(&current_vm->pool, 0);
	if (addr)
		return addr;

	addr = pop_hyp_memcache(mc, hyp_phys_to_virt);
	if (!addr)
		return addr;

	memset(addr, 0, PAGE_SIZE);
	p = hyp_virt_to_page(addr);
	memset(p, 0, sizeof(*p));
	p->refcount = 1;

	return addr;
}

static void guest_s2_get_page(void *addr)
{
	hyp_get_page(&current_vm->pool, addr);
}

static void guest_s2_put_page(void *addr)
{
	hyp_put_page(&current_vm->pool, addr);
}

static void clean_dcache_guest_page(void *va, size_t size)
{
	__clean_dcache_guest_page(hyp_fixmap_map(__hyp_pa(va)), size);
	hyp_fixmap_unmap();
}

static void invalidate_icache_guest_page(void *va, size_t size)
{
	__invalidate_icache_guest_page(hyp_fixmap_map(__hyp_pa(va)), size);
	hyp_fixmap_unmap();
}

int kvm_guest_prepare_stage2(struct kvm_shadow_vm *vm, void *pgd)
{
	struct kvm_s2_mmu *mmu = &vm->arch.mmu;
	unsigned long nr_pages;
	int ret;

	nr_pages = kvm_pgtable_stage2_pgd_size(vm->arch.vtcr) >> PAGE_SHIFT;

	ret = hyp_pool_init(&vm->pool, hyp_virt_to_pfn(pgd), nr_pages, 0);
	if (ret)
		return ret;

	hyp_spin_lock_init(&vm->lock);
	vm->mm_ops = (struct kvm_pgtable_mm_ops) {
		.zalloc_pages_exact	= guest_s2_zalloc_pages_exact,
		.free_pages_exact	= guest_s2_free_pages_exact,
		.zalloc_page		= guest_s2_zalloc_page,
		.phys_to_virt		= hyp_phys_to_virt,
		.virt_to_phys		= hyp_virt_to_phys,
		.page_count		= hyp_page_count,
		.get_page		= guest_s2_get_page,
		.put_page		= guest_s2_put_page,
		.dcache_clean_inval_poc	= clean_dcache_guest_page,
		.icache_inval_pou	= invalidate_icache_guest_page,
	};

	__guest_lock(vm);
	ret = __kvm_pgtable_stage2_init(mmu->pgt, mmu, &vm->mm_ops, 0,
					guest_stage2_force_pte_cb);
	__guest_unlock(vm);
	if (ret)
		return ret;

	vm->arch.mmu.pgd_phys = __hyp_pa(vm->pgt.pgd);

	return 0;
}

static int reclaim_walker(u64 addr, u64 end, u32 level, kvm_pte_t *ptep,
		enum kvm_pgtable_walk_flags flag,
		void * const arg)
{
	kvm_pte_t pte = *ptep;
	struct hyp_page *page;

	if (!kvm_pte_valid(pte))
		return 0;

	page = hyp_phys_to_page(kvm_pte_to_phys(pte));
	switch (pkvm_getstate(kvm_pgtable_stage2_pte_prot(pte))) {
	case PKVM_PAGE_OWNED:
		page->flags |= HOST_PAGE_NEED_POISONING;
		fallthrough;
	case PKVM_PAGE_SHARED_BORROWED:
	case PKVM_PAGE_SHARED_OWNED:
		page->flags |= HOST_PAGE_PENDING_RECLAIM;
		break;
	default:
		return -EPERM;
	}

	return 0;
}

void reclaim_guest_pages(struct kvm_shadow_vm *vm, struct kvm_hyp_memcache *mc)
{
	struct kvm_pgtable_walker walker = {
		.cb     = reclaim_walker,
		.flags  = KVM_PGTABLE_WALK_LEAF
	};
	void *addr;

	host_lock_component();
	__guest_lock(vm);

	/* Reclaim all guest pages, and dump all pgtable pages in the hyp_pool */
	BUG_ON(kvm_pgtable_walk(&vm->pgt, 0, BIT(vm->pgt.ia_bits), &walker));
	kvm_pgtable_stage2_destroy(&vm->pgt);
	vm->arch.mmu.pgd_phys = 0ULL;

	__guest_unlock(vm);
	host_unlock_component();

	/* Drain the hyp_pool into the memcache */
	addr = hyp_alloc_pages(&vm->pool, 0);
	while (addr) {
		memset(hyp_virt_to_page(addr), 0, sizeof(struct hyp_page));
		push_hyp_memcache(mc, addr, hyp_virt_to_phys);
		WARN_ON(__pkvm_hyp_donate_host(hyp_virt_to_pfn(addr), 1));
		addr = hyp_alloc_pages(&vm->pool, 0);
	}
}

int __pkvm_prot_finalize(void)
{
	struct kvm_s2_mmu *mmu = &host_kvm.arch.mmu;
	struct kvm_nvhe_init_params *params = this_cpu_ptr(&kvm_init_params);

	if (params->hcr_el2 & HCR_VM)
		return -EPERM;

	params->vttbr = kvm_get_vttbr(mmu);
	params->vtcr = host_kvm.arch.vtcr;
	params->hcr_el2 |= HCR_VM;
	kvm_flush_dcache_to_poc(params, sizeof(*params));

	write_sysreg(params->hcr_el2, hcr_el2);
	__load_stage2(&host_kvm.arch.mmu, &host_kvm.arch);

	/*
	 * Make sure to have an ISB before the TLB maintenance below but only
	 * when __load_stage2() doesn't include one already.
	 */
	asm(ALTERNATIVE("isb", "nop", ARM64_WORKAROUND_SPECULATIVE_AT));

	/* Invalidate stale HCR bits that may be cached in TLBs */
	__tlbi(vmalls12e1);
	dsb(nsh);
	isb();

	return 0;
}

int host_stage2_unmap_dev_locked(phys_addr_t start, u64 size)
{
	int ret;

	hyp_assert_lock_held(&host_kvm.lock);

	ret = kvm_pgtable_stage2_unmap(&host_kvm.pgt, start, size);
	if (ret)
		return ret;

	pkvm_iommu_host_stage2_idmap(start, start + size, 0);
	return 0;
}

static int host_stage2_unmap_dev_all(void)
{
	struct kvm_pgtable *pgt = &host_kvm.pgt;
	struct memblock_region *reg;
	u64 addr = 0;
	int i, ret;

	/* Unmap all non-memory regions to recycle the pages */
	for (i = 0; i < hyp_memblock_nr; i++, addr = reg->base + reg->size) {
		reg = &hyp_memory[i];
		ret = host_stage2_unmap_dev_locked(addr, reg->base - addr);
		if (ret)
			return ret;
	}
	return host_stage2_unmap_dev_locked(addr, BIT(pgt->ia_bits) - addr);
}

struct kvm_mem_range {
	u64 start;
	u64 end;
};

static struct memblock_region *find_mem_range(phys_addr_t addr, struct kvm_mem_range *range)
{
	int cur, left = 0, right = hyp_memblock_nr;
	struct memblock_region *reg;
	phys_addr_t end;

	range->start = 0;
	range->end = ULONG_MAX;

	/* The list of memblock regions is sorted, binary search it */
	while (left < right) {
		cur = (left + right) >> 1;
		reg = &hyp_memory[cur];
		end = reg->base + reg->size;
		if (addr < reg->base) {
			right = cur;
			range->end = reg->base;
		} else if (addr >= end) {
			left = cur + 1;
			range->start = end;
		} else {
			range->start = reg->base;
			range->end = end;
			return reg;
		}
	}

	return NULL;
}

bool addr_is_memory(phys_addr_t phys)
{
	struct kvm_mem_range range;

	return !!find_mem_range(phys, &range);
}

static bool addr_is_allowed_memory(phys_addr_t phys)
{
	struct memblock_region *reg;
	struct kvm_mem_range range;

	reg = find_mem_range(phys, &range);

	return reg && !(reg->flags & MEMBLOCK_NOMAP);
}

static bool is_in_mem_range(u64 addr, struct kvm_mem_range *range)
{
	return range->start <= addr && addr < range->end;
}

static bool range_is_memory(u64 start, u64 end)
{
	struct kvm_mem_range r;

	if (!find_mem_range(start, &r))
		return false;

	return is_in_mem_range(end - 1, &r);
}

static inline int __host_stage2_idmap(u64 start, u64 end,
				      enum kvm_pgtable_prot prot,
				      bool update_iommu)
{
	int ret;

	ret = kvm_pgtable_stage2_map(&host_kvm.pgt, start, end - start, start,
				     prot, &host_s2_pool);
	if (ret)
		return ret;

	if (update_iommu)
		pkvm_iommu_host_stage2_idmap(start, end, prot);
	return 0;
}

/*
 * The pool has been provided with enough pages to cover all of memory with
 * page granularity, but it is difficult to know how much of the MMIO range
 * we will need to cover upfront, so we may need to 'recycle' the pages if we
 * run out.
 */
#define host_stage2_try(fn, ...)					\
	({								\
		int __ret;						\
		hyp_assert_lock_held(&host_kvm.lock);			\
		__ret = fn(__VA_ARGS__);				\
		if (__ret == -ENOMEM) {					\
			__ret = host_stage2_unmap_dev_all();		\
			if (!__ret)					\
				__ret = fn(__VA_ARGS__);		\
		}							\
		__ret;							\
	 })

static inline bool range_included(struct kvm_mem_range *child,
				  struct kvm_mem_range *parent)
{
	return parent->start <= child->start && child->end <= parent->end;
}

static int host_stage2_adjust_range(u64 addr, struct kvm_mem_range *range)
{
	struct kvm_mem_range cur;
	kvm_pte_t pte;
	u32 level;
	int ret;

	hyp_assert_lock_held(&host_kvm.lock);
	ret = kvm_pgtable_get_leaf(&host_kvm.pgt, addr, &pte, &level);
	if (ret)
		return ret;

	if (kvm_pte_valid(pte))
		return -EAGAIN;

	if (pte)
		return -EPERM;

	do {
		u64 granule = kvm_granule_size(level);
		cur.start = ALIGN_DOWN(addr, granule);
		cur.end = cur.start + granule;
		level++;
	} while ((level < KVM_PGTABLE_MAX_LEVELS) &&
			!(kvm_level_supports_block_mapping(level) &&
			  range_included(&cur, range)));

	*range = cur;

	return 0;
}

int host_stage2_idmap_locked(phys_addr_t addr, u64 size,
			     enum kvm_pgtable_prot prot, bool update_iommu)
{
	return host_stage2_try(__host_stage2_idmap, addr, addr + size, prot, update_iommu);
}

#define KVM_INVALID_PTE_OWNER_MASK	GENMASK(32, 1)
static kvm_pte_t kvm_init_invalid_leaf_owner(pkvm_id owner_id)
{
	return FIELD_PREP(KVM_INVALID_PTE_OWNER_MASK, owner_id);
}

int host_stage2_set_owner_locked(phys_addr_t addr, u64 size,
				 pkvm_id owner_id)
{
	kvm_pte_t annotation = kvm_init_invalid_leaf_owner(owner_id);
	enum kvm_pgtable_prot prot;
	int ret;

	ret = host_stage2_try(kvm_pgtable_stage2_annotate, &host_kvm.pgt,
			      addr, size, &host_s2_pool, annotation);
	if (ret)
		return ret;

	prot = owner_id == pkvm_host_id ? PKVM_HOST_MEM_PROT : 0;
	pkvm_iommu_host_stage2_idmap(addr, addr + size, prot);
	return 0;
}

static bool host_stage2_force_pte_cb(u64 addr, u64 end, enum kvm_pgtable_prot prot)
{
	/*
	 * Block mappings must be used with care in the host stage-2 as a
	 * kvm_pgtable_stage2_map() operation targeting a page in the range of
	 * an existing block will delete the block under the assumption that
	 * mappings in the rest of the block range can always be rebuilt lazily.
	 * That assumption is correct for the host stage-2 with RWX mappings
	 * targeting memory or RW mappings targeting MMIO ranges (see
	 * host_stage2_idmap() below which implements some of the host memory
	 * abort logic). However, this is not safe for any other mappings where
	 * the host stage-2 page-table is in fact the only place where this
	 * state is stored. In all those cases, it is safer to use page-level
	 * mappings, hence avoiding to lose the state because of side-effects in
	 * kvm_pgtable_stage2_map().
	 */
	if (range_is_memory(addr, end))
		return prot != PKVM_HOST_MEM_PROT;
	else
		return prot != PKVM_HOST_MMIO_PROT;
}

static int host_stage2_idmap(u64 addr)
{
	struct kvm_mem_range range;
	bool is_memory = !!find_mem_range(addr, &range);
	enum kvm_pgtable_prot prot;
	int ret;

	hyp_assert_lock_held(&host_kvm.lock);

	prot = is_memory ? PKVM_HOST_MEM_PROT : PKVM_HOST_MMIO_PROT;

	/*
	 * Adjust against IOMMU devices first. host_stage2_adjust_range() should
	 * be called last for proper alignment.
	 */
	if (!is_memory) {
		ret = pkvm_iommu_host_stage2_adjust_range(addr, &range.start,
							  &range.end);
		if (ret)
			return ret;
	}

	ret = host_stage2_adjust_range(addr, &range);
	if (ret)
		return ret;

	return host_stage2_idmap_locked(range.start, range.end - range.start, prot, false);
}

static bool is_dabt(u64 esr)
{
	return ESR_ELx_EC(esr) == ESR_ELx_EC_DABT_LOW;
}

static void host_inject_abort(struct kvm_cpu_context *host_ctxt)
{
	u64 spsr = read_sysreg_el2(SYS_SPSR);
	u64 esr = read_sysreg_el2(SYS_ESR);
	u64 ventry, ec;

	/* Repaint the ESR to report a same-level fault if taken from EL1 */
	if ((spsr & PSR_MODE_MASK) != PSR_MODE_EL0t) {
		ec = ESR_ELx_EC(esr);
		if (ec == ESR_ELx_EC_DABT_LOW)
			ec = ESR_ELx_EC_DABT_CUR;
		else if (ec == ESR_ELx_EC_IABT_LOW)
			ec = ESR_ELx_EC_IABT_CUR;
		else
			WARN_ON(1);
		esr &= ~ESR_ELx_EC_MASK;
		esr |= ec << ESR_ELx_EC_SHIFT;
	}

	/*
	 * Since S1PTW should only ever be set for stage-2 faults, we're pretty
	 * much guaranteed that it won't be set in ESR_EL1 by the hardware. So,
	 * let's use that bit to allow the host abort handler to differentiate
	 * this abort from normal userspace faults.
	 *
	 * Note: although S1PTW is RES0 at EL1, it is guaranteed by the
	 * architecture to be backed by flops, so it should be safe to use.
	 */
	esr |= ESR_ELx_S1PTW;

	write_sysreg_el1(esr, SYS_ESR);
	write_sysreg_el1(spsr, SYS_SPSR);
	write_sysreg_el1(read_sysreg_el2(SYS_ELR), SYS_ELR);
	write_sysreg_el1(read_sysreg_el2(SYS_FAR), SYS_FAR);

	ventry = read_sysreg_el1(SYS_VBAR);
	ventry += get_except64_offset(spsr, PSR_MODE_EL1h, except_type_sync);
	write_sysreg_el2(ventry, SYS_ELR);

	spsr = get_except64_cpsr(spsr, system_supports_mte(),
				 read_sysreg_el1(SYS_SCTLR), PSR_MODE_EL1h);
	write_sysreg_el2(spsr, SYS_SPSR);
}

void handle_host_mem_abort(struct kvm_cpu_context *host_ctxt)
{
	struct kvm_vcpu_fault_info fault;
	u64 esr, addr;
	int ret = -EPERM;

	esr = read_sysreg_el2(SYS_ESR);
	BUG_ON(!__get_fault_info(esr, &fault));

	addr = (fault.hpfar_el2 & HPFAR_MASK) << 8;
	addr |= fault.far_el2 & FAR_MASK;

	host_lock_component();

	/* Check if an IOMMU device can handle the DABT. */
	if (is_dabt(esr) && !addr_is_memory(addr) &&
	    pkvm_iommu_host_dabt_handler(host_ctxt, esr, addr))
		ret = 0;

	/* If not handled, attempt to map the page. */
	if (ret == -EPERM)
		ret = host_stage2_idmap(addr);

	host_unlock_component();

	if (ret == -EPERM)
		host_inject_abort(host_ctxt);
	else
		BUG_ON(ret && ret != -EAGAIN);
}

/* This corresponds to locking order */
enum pkvm_component_id {
	PKVM_ID_HOST,
	PKVM_ID_HYP,
	PKVM_ID_GUEST,
	PKVM_ID_FFA,
};

struct pkvm_mem_transition {
	u64				nr_pages;

	struct {
		enum pkvm_component_id	id;
		/* Address in the initiator's address space */
		u64			addr;

		union {
			struct {
				/* Address in the completer's address space */
				u64	completer_addr;
			} host;
			struct {
				u64	completer_addr;
			} hyp;

			struct {
				struct kvm_vcpu *vcpu;
			} guest;
		};
	} initiator;

	struct {
		enum pkvm_component_id	id;

		union {
			struct {
				struct kvm_vcpu *vcpu;
				phys_addr_t phys;
			} guest;
		};
	} completer;
};

struct pkvm_mem_share {
	const struct pkvm_mem_transition	tx;
	const enum kvm_pgtable_prot		completer_prot;
};

struct pkvm_mem_donation {
	const struct pkvm_mem_transition	tx;
};

static pkvm_id initiator_owner_id(const struct pkvm_mem_transition *tx)
{
	switch (tx->initiator.id) {
	case PKVM_ID_HOST:
		return pkvm_host_id;
	case PKVM_ID_HYP:
		return pkvm_hyp_id;
	case PKVM_ID_GUEST:
		return pkvm_guest_id(tx->initiator.guest.vcpu);
	default:
		WARN_ON(1);
		return -1;
	}
}

static pkvm_id completer_owner_id(const struct pkvm_mem_transition *tx)
{
	switch (tx->completer.id) {
	case PKVM_ID_HOST:
		return pkvm_host_id;
	case PKVM_ID_HYP:
		return pkvm_hyp_id;
	case PKVM_ID_GUEST:
		return pkvm_guest_id(tx->completer.guest.vcpu);
	default:
		WARN_ON(1);
		return -1;
	}
}

struct check_walk_data {
	enum pkvm_page_state	desired;
	enum pkvm_page_state	(*get_page_state)(kvm_pte_t pte);
};

static int __check_page_state_visitor(u64 addr, u64 end, u32 level,
				      kvm_pte_t *ptep,
				      enum kvm_pgtable_walk_flags flag,
				      void * const arg)
{
	struct check_walk_data *d = arg;
	kvm_pte_t pte = *ptep;

	if (kvm_pte_valid(pte) && !addr_is_allowed_memory(kvm_pte_to_phys(pte)))
		return -EINVAL;

	return d->get_page_state(pte) == d->desired ? 0 : -EPERM;
}

static int check_page_state_range(struct kvm_pgtable *pgt, u64 addr, u64 size,
				  struct check_walk_data *data)
{
	struct kvm_pgtable_walker walker = {
		.cb	= __check_page_state_visitor,
		.arg	= data,
		.flags	= KVM_PGTABLE_WALK_LEAF,
	};

	return kvm_pgtable_walk(pgt, addr, size, &walker);
}

static enum pkvm_page_state host_get_page_state(kvm_pte_t pte)
{
	if (!kvm_pte_valid(pte) && pte)
		return PKVM_NOPAGE;

	return pkvm_getstate(kvm_pgtable_stage2_pte_prot(pte));
}

static int __host_check_page_state_range(u64 addr, u64 size,
					 enum pkvm_page_state state)
{
	struct check_walk_data d = {
		.desired	= state,
		.get_page_state	= host_get_page_state,
	};

	hyp_assert_lock_held(&host_kvm.lock);
	return check_page_state_range(&host_kvm.pgt, addr, size, &d);
}

static int __host_set_page_state_range(u64 addr, u64 size,
				       enum pkvm_page_state state)
{
	enum kvm_pgtable_prot prot = pkvm_mkstate(PKVM_HOST_MEM_PROT, state);

	return host_stage2_idmap_locked(addr, size, prot, true);
}

static int host_request_owned_transition(u64 *completer_addr,
					 const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;
	u64 addr = tx->initiator.addr;

	*completer_addr = tx->initiator.host.completer_addr;
	return __host_check_page_state_range(addr, size, PKVM_PAGE_OWNED);
}

static int host_request_unshare(u64 *completer_addr,
				const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;
	u64 addr = tx->initiator.addr;

	*completer_addr = tx->initiator.host.completer_addr;
	return __host_check_page_state_range(addr, size, PKVM_PAGE_SHARED_OWNED);
}

static int host_initiate_share(u64 *completer_addr,
			       const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;
	u64 addr = tx->initiator.addr;

	*completer_addr = tx->initiator.host.completer_addr;
	return __host_set_page_state_range(addr, size, PKVM_PAGE_SHARED_OWNED);
}

static int host_initiate_unshare(u64 *completer_addr,
				 const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;
	u64 addr = tx->initiator.addr;

	*completer_addr = tx->initiator.host.completer_addr;
	return __host_set_page_state_range(addr, size, PKVM_PAGE_OWNED);
}

static int host_initiate_donation(u64 *completer_addr,
				  const struct pkvm_mem_transition *tx)
{
	pkvm_id owner_id = completer_owner_id(tx);
	u64 size = tx->nr_pages * PAGE_SIZE;

	*completer_addr = tx->initiator.host.completer_addr;
	return host_stage2_set_owner_locked(tx->initiator.addr, size, owner_id);
}

static bool __host_ack_skip_pgtable_check(const struct pkvm_mem_transition *tx)
{
	return !(IS_ENABLED(CONFIG_NVHE_EL2_DEBUG) ||
		 tx->initiator.id != PKVM_ID_HYP);
}

static int __host_ack_transition(u64 addr, const struct pkvm_mem_transition *tx,
				 enum pkvm_page_state state)
{
	u64 size = tx->nr_pages * PAGE_SIZE;

	if (__host_ack_skip_pgtable_check(tx))
		return 0;

	return __host_check_page_state_range(addr, size, state);
}

static int host_ack_share(u64 addr, const struct pkvm_mem_transition *tx,
			  enum kvm_pgtable_prot perms)
{
	if (perms != PKVM_HOST_MEM_PROT)
		return -EPERM;

	return __host_ack_transition(addr, tx, PKVM_NOPAGE);
}

static int host_ack_donation(u64 addr, const struct pkvm_mem_transition *tx)
{
	return __host_ack_transition(addr, tx, PKVM_NOPAGE);
}

static int host_ack_unshare(u64 addr, const struct pkvm_mem_transition *tx)
{
	return __host_ack_transition(addr, tx, PKVM_PAGE_SHARED_BORROWED);
}

static int host_complete_share(u64 addr, const struct pkvm_mem_transition *tx,
			       enum kvm_pgtable_prot perms)
{
	u64 size = tx->nr_pages * PAGE_SIZE;

	if (tx->initiator.id == PKVM_ID_GUEST)
		psci_mem_protect_dec();

	return __host_set_page_state_range(addr, size, PKVM_PAGE_SHARED_BORROWED);
}

static int host_complete_unshare(u64 addr, const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;
	pkvm_id owner_id = initiator_owner_id(tx);

	if (tx->initiator.id == PKVM_ID_GUEST)
		psci_mem_protect_inc();

	return host_stage2_set_owner_locked(addr, size, owner_id);
}

static int host_complete_donation(u64 addr, const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;
	pkvm_id host_id = completer_owner_id(tx);

	return host_stage2_set_owner_locked(addr, size, host_id);
}

static enum pkvm_page_state hyp_get_page_state(kvm_pte_t pte)
{
	if (!kvm_pte_valid(pte))
		return PKVM_NOPAGE;

	return pkvm_getstate(kvm_pgtable_stage2_pte_prot(pte));
}

static int __hyp_check_page_state_range(u64 addr, u64 size,
					enum pkvm_page_state state)
{
	struct check_walk_data d = {
		.desired	= state,
		.get_page_state	= hyp_get_page_state,
	};

	hyp_assert_lock_held(&pkvm_pgd_lock);
	return check_page_state_range(&pkvm_pgtable, addr, size, &d);
}

static int hyp_request_donation(u64 *completer_addr,
				const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;
	u64 addr = tx->initiator.addr;

	*completer_addr = tx->initiator.hyp.completer_addr;
	return __hyp_check_page_state_range(addr, size, PKVM_PAGE_OWNED);
}

static int hyp_initiate_donation(u64 *completer_addr,
				 const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;
	int ret;

	*completer_addr = tx->initiator.hyp.completer_addr;
	ret = kvm_pgtable_hyp_unmap(&pkvm_pgtable, tx->initiator.addr, size);
	return (ret != size) ? -EFAULT : 0;
}

static bool __hyp_ack_skip_pgtable_check(const struct pkvm_mem_transition *tx)
{
	return !(IS_ENABLED(CONFIG_NVHE_EL2_DEBUG) ||
		 tx->initiator.id != PKVM_ID_HOST);
}

static int hyp_ack_share(u64 addr, const struct pkvm_mem_transition *tx,
			 enum kvm_pgtable_prot perms)
{
	u64 size = tx->nr_pages * PAGE_SIZE;

	if (perms != PAGE_HYP)
		return -EPERM;

	if (__hyp_ack_skip_pgtable_check(tx))
		return 0;

	return __hyp_check_page_state_range(addr, size, PKVM_NOPAGE);
}

static int hyp_ack_unshare(u64 addr, const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;

	if (tx->initiator.id == PKVM_ID_HOST && hyp_page_count((void *)addr))
		return -EBUSY;

	if (__hyp_ack_skip_pgtable_check(tx))
		return 0;

	return __hyp_check_page_state_range(addr, size,
					    PKVM_PAGE_SHARED_BORROWED);
}

static int hyp_ack_donation(u64 addr, const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;

	if (__hyp_ack_skip_pgtable_check(tx))
		return 0;

	return __hyp_check_page_state_range(addr, size, PKVM_NOPAGE);
}

static int hyp_complete_share(u64 addr, const struct pkvm_mem_transition *tx,
			      enum kvm_pgtable_prot perms)
{
	void *start = (void *)addr, *end = start + (tx->nr_pages * PAGE_SIZE);
	enum kvm_pgtable_prot prot;

	prot = pkvm_mkstate(perms, PKVM_PAGE_SHARED_BORROWED);
	return pkvm_create_mappings_locked(start, end, prot);
}

static int hyp_complete_unshare(u64 addr, const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;
	int ret = kvm_pgtable_hyp_unmap(&pkvm_pgtable, addr, size);

	return (ret != size) ? -EFAULT : 0;
}

static int hyp_complete_donation(u64 addr,
				 const struct pkvm_mem_transition *tx)
{
	void *start = (void *)addr, *end = start + (tx->nr_pages * PAGE_SIZE);
	enum kvm_pgtable_prot prot = pkvm_mkstate(PAGE_HYP, PKVM_PAGE_OWNED);

	return pkvm_create_mappings_locked(start, end, prot);
}

static enum pkvm_page_state guest_get_page_state(kvm_pte_t pte)
{
	if (!kvm_pte_valid(pte))
		return PKVM_NOPAGE;

	return pkvm_getstate(kvm_pgtable_stage2_pte_prot(pte));
}

static int __guest_check_page_state_range(struct kvm_vcpu *vcpu, u64 addr,
					  u64 size, enum pkvm_page_state state)
{
	struct kvm_shadow_vm *vm = vcpu->arch.pkvm.shadow_vm;
	struct check_walk_data d = {
		.desired	= state,
		.get_page_state	= guest_get_page_state,
	};

	hyp_assert_lock_held(&vm->lock);
	return check_page_state_range(&vm->pgt, addr, size, &d);
}

static int guest_ack_share(u64 addr, const struct pkvm_mem_transition *tx,
			   enum kvm_pgtable_prot perms)
{
	u64 size = tx->nr_pages * PAGE_SIZE;

	if (perms != KVM_PGTABLE_PROT_RWX)
		return -EPERM;

	return __guest_check_page_state_range(tx->completer.guest.vcpu, addr,
					      size, PKVM_NOPAGE);
}

static int guest_ack_donation(u64 addr, const struct pkvm_mem_transition *tx)
{
	u64 size = tx->nr_pages * PAGE_SIZE;

	return __guest_check_page_state_range(tx->completer.guest.vcpu, addr,
					      size, PKVM_NOPAGE);
}

static int guest_complete_share(u64 addr, const struct pkvm_mem_transition *tx,
				enum kvm_pgtable_prot perms)
{
	struct kvm_vcpu *vcpu = tx->completer.guest.vcpu;
	struct kvm_shadow_vm *vm = vcpu->arch.pkvm.shadow_vm;
	u64 size = tx->nr_pages * PAGE_SIZE;
	enum kvm_pgtable_prot prot;

	prot = pkvm_mkstate(perms, PKVM_PAGE_SHARED_BORROWED);
	return kvm_pgtable_stage2_map(&vm->pgt, addr, size, tx->completer.guest.phys,
				      prot, &vcpu->arch.pkvm_memcache);
}

static int guest_complete_donation(u64 addr, const struct pkvm_mem_transition *tx)
{
	enum kvm_pgtable_prot prot = pkvm_mkstate(KVM_PGTABLE_PROT_RWX, PKVM_PAGE_OWNED);
	struct kvm_vcpu *vcpu = tx->completer.guest.vcpu;
	struct kvm_shadow_vm *vm = vcpu->arch.pkvm.shadow_vm;
	phys_addr_t phys = tx->completer.guest.phys;
	u64 size = tx->nr_pages * PAGE_SIZE;
	int err;

	if (tx->initiator.id == PKVM_ID_HOST) {
		psci_mem_protect_inc();

		if (ipa_in_pvmfw_region(vm, addr)) {
			err = pkvm_load_pvmfw_pages(vm, addr, phys, size);
			if (err)
				return err;
		}
	}

	return kvm_pgtable_stage2_map(&vm->pgt, addr, size, phys, prot,
				      &vcpu->arch.pkvm_memcache);
}

static int __guest_get_completer_addr(u64 *completer_addr, phys_addr_t phys,
				      const struct pkvm_mem_transition *tx)
{
	switch (tx->completer.id) {
	case PKVM_ID_HOST:
		*completer_addr = phys;
		break;
	case PKVM_ID_HYP:
		*completer_addr = (u64)__hyp_va(phys);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int __guest_request_page_transition(u64 *completer_addr,
					   const struct pkvm_mem_transition *tx,
					   enum pkvm_page_state desired)
{
	struct kvm_vcpu *vcpu = tx->initiator.guest.vcpu;
	struct kvm_protected_vcpu *pkvm = &vcpu->arch.pkvm;
	struct kvm_shadow_vm *vm = pkvm->shadow_vm;
	enum pkvm_page_state state;
	phys_addr_t phys;
	kvm_pte_t pte;
	u32 level;
	int ret;

	if (tx->nr_pages != 1)
		return -E2BIG;

	ret = kvm_pgtable_get_leaf(&vm->pgt, tx->initiator.addr, &pte, &level);
	if (ret)
		return ret;

	state = guest_get_page_state(pte);
	if (state == PKVM_NOPAGE)
		return -EFAULT;

	if (state != desired)
		return -EPERM;

	/*
	 * We only deal with page granular mappings in the guest for now as
	 * the pgtable code relies on being able to recreate page mappings
	 * lazily after zapping a block mapping, which doesn't work once the
	 * pages have been donated.
	 */
	if (level != KVM_PGTABLE_MAX_LEVELS - 1)
		return -EINVAL;

	phys = kvm_pte_to_phys(pte);
	if (!addr_is_allowed_memory(phys))
		return -EINVAL;

	return __guest_get_completer_addr(completer_addr, phys, tx);
}

static int guest_request_share(u64 *completer_addr,
			       const struct pkvm_mem_transition *tx)
{
	return __guest_request_page_transition(completer_addr, tx,
					       PKVM_PAGE_OWNED);
}

static int guest_request_unshare(u64 *completer_addr,
				 const struct pkvm_mem_transition *tx)
{
	return __guest_request_page_transition(completer_addr, tx,
					       PKVM_PAGE_SHARED_OWNED);
}

static int __guest_initiate_page_transition(u64 *completer_addr,
					    const struct pkvm_mem_transition *tx,
					    enum pkvm_page_state state)
{
	struct kvm_vcpu *vcpu = tx->initiator.guest.vcpu;
	struct kvm_protected_vcpu *pkvm = &vcpu->arch.pkvm;
	struct kvm_shadow_vm *vm = pkvm->shadow_vm;
	struct kvm_hyp_memcache *mc = &vcpu->arch.pkvm_memcache;
	u64 size = tx->nr_pages * PAGE_SIZE;
	u64 addr = tx->initiator.addr;
	enum kvm_pgtable_prot prot;
	phys_addr_t phys;
	kvm_pte_t pte;
	int ret;

	ret = kvm_pgtable_get_leaf(&vm->pgt, addr, &pte, NULL);
	if (ret)
		return ret;

	phys = kvm_pte_to_phys(pte);
	prot = pkvm_mkstate(kvm_pgtable_stage2_pte_prot(pte), state);
	ret = kvm_pgtable_stage2_map(&vm->pgt, addr, size, phys, prot, mc);
	if (ret)
		return ret;

	return __guest_get_completer_addr(completer_addr, phys, tx);
}

static int guest_initiate_share(u64 *completer_addr,
				const struct pkvm_mem_transition *tx)
{
	return __guest_initiate_page_transition(completer_addr, tx,
						PKVM_PAGE_SHARED_OWNED);
}

static int guest_initiate_unshare(u64 *completer_addr,
				  const struct pkvm_mem_transition *tx)
{
	return __guest_initiate_page_transition(completer_addr, tx,
						PKVM_PAGE_OWNED);
}

static int check_share(struct pkvm_mem_share *share)
{
	const struct pkvm_mem_transition *tx = &share->tx;
	u64 completer_addr;
	int ret;

	switch (tx->initiator.id) {
	case PKVM_ID_HOST:
		ret = host_request_owned_transition(&completer_addr, tx);
		break;
	case PKVM_ID_GUEST:
		ret = guest_request_share(&completer_addr, tx);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret)
		return ret;

	switch (tx->completer.id) {
	case PKVM_ID_HOST:
		ret = host_ack_share(completer_addr, tx, share->completer_prot);
		break;
	case PKVM_ID_HYP:
		ret = hyp_ack_share(completer_addr, tx, share->completer_prot);
		break;
	case PKVM_ID_GUEST:
		ret = guest_ack_share(completer_addr, tx, share->completer_prot);
		break;
	case PKVM_ID_FFA:
		/*
		 * We only check the host; the secure side will check the other
		 * end when we forward the FFA call.
		 */
		ret = 0;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int __do_share(struct pkvm_mem_share *share)
{
	const struct pkvm_mem_transition *tx = &share->tx;
	u64 completer_addr;
	int ret;

	switch (tx->initiator.id) {
	case PKVM_ID_HOST:
		ret = host_initiate_share(&completer_addr, tx);
		break;
	case PKVM_ID_GUEST:
		ret = guest_initiate_share(&completer_addr, tx);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret)
		return ret;

	switch (tx->completer.id) {
	case PKVM_ID_HOST:
		ret = host_complete_share(completer_addr, tx, share->completer_prot);
		break;
	case PKVM_ID_HYP:
		ret = hyp_complete_share(completer_addr, tx, share->completer_prot);
		break;
	case PKVM_ID_GUEST:
		ret = guest_complete_share(completer_addr, tx, share->completer_prot);
		break;
	case PKVM_ID_FFA:
		/*
		 * We're not responsible for any secure page-tables, so there's
		 * nothing to do here.
		 */
		ret = 0;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*
 * do_share():
 *
 * The page owner grants access to another component with a given set
 * of permissions.
 *
 * Initiator: OWNED	=> SHARED_OWNED
 * Completer: NOPAGE	=> SHARED_BORROWED
 */
static int do_share(struct pkvm_mem_share *share)
{
	int ret;

	ret = check_share(share);
	if (ret)
		return ret;

	return WARN_ON(__do_share(share));
}

static int check_unshare(struct pkvm_mem_share *share)
{
	const struct pkvm_mem_transition *tx = &share->tx;
	u64 completer_addr;
	int ret;

	switch (tx->initiator.id) {
	case PKVM_ID_HOST:
		ret = host_request_unshare(&completer_addr, tx);
		break;
	case PKVM_ID_GUEST:
		ret = guest_request_unshare(&completer_addr, tx);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret)
		return ret;

	switch (tx->completer.id) {
	case PKVM_ID_HOST:
		ret = host_ack_unshare(completer_addr, tx);
		break;
	case PKVM_ID_HYP:
		ret = hyp_ack_unshare(completer_addr, tx);
		break;
	case PKVM_ID_FFA:
		/* See check_share() */
		ret = 0;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int __do_unshare(struct pkvm_mem_share *share)
{
	const struct pkvm_mem_transition *tx = &share->tx;
	u64 completer_addr;
	int ret;

	switch (tx->initiator.id) {
	case PKVM_ID_HOST:
		ret = host_initiate_unshare(&completer_addr, tx);
		break;
	case PKVM_ID_GUEST:
		ret = guest_initiate_unshare(&completer_addr, tx);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret)
		return ret;

	switch (tx->completer.id) {
	case PKVM_ID_HOST:
		ret = host_complete_unshare(completer_addr, tx);
		break;
	case PKVM_ID_HYP:
		ret = hyp_complete_unshare(completer_addr, tx);
		break;
	case PKVM_ID_FFA:
		/* See __do_share() */
		ret = 0;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*
 * do_unshare():
 *
 * The page owner revokes access from another component for a range of
 * pages which were previously shared using do_share().
 *
 * Initiator: SHARED_OWNED	=> OWNED
 * Completer: SHARED_BORROWED	=> NOPAGE
 */
static int do_unshare(struct pkvm_mem_share *share)
{
	int ret;

	ret = check_unshare(share);
	if (ret)
		return ret;

	return WARN_ON(__do_unshare(share));
}

static int check_donation(struct pkvm_mem_donation *donation)
{
	const struct pkvm_mem_transition *tx = &donation->tx;
	u64 completer_addr;
	int ret;

	switch (tx->initiator.id) {
	case PKVM_ID_HOST:
		ret = host_request_owned_transition(&completer_addr, tx);
		break;
	case PKVM_ID_HYP:
		ret = hyp_request_donation(&completer_addr, tx);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret)
		return ret;

	switch (tx->completer.id){
	case PKVM_ID_HOST:
		ret = host_ack_donation(completer_addr, tx);
		break;
	case PKVM_ID_HYP:
		ret = hyp_ack_donation(completer_addr, tx);
		break;
	case PKVM_ID_GUEST:
		ret = guest_ack_donation(completer_addr, tx);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int __do_donate(struct pkvm_mem_donation *donation)
{
	const struct pkvm_mem_transition *tx = &donation->tx;
	u64 completer_addr;
	int ret;

	switch (tx->initiator.id) {
	case PKVM_ID_HOST:
		ret = host_initiate_donation(&completer_addr, tx);
		break;
	case PKVM_ID_HYP:
		ret = hyp_initiate_donation(&completer_addr, tx);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret)
		return ret;

	switch (tx->completer.id){
	case PKVM_ID_HOST:
		ret = host_complete_donation(completer_addr, tx);
		break;
	case PKVM_ID_HYP:
		ret = hyp_complete_donation(completer_addr, tx);
		break;
	case PKVM_ID_GUEST:
		ret = guest_complete_donation(completer_addr, tx);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*
 * do_donate():
 *
 * The page owner transfers ownership to another component, losing access
 * as a consequence.
 *
 * Initiator: OWNED	=> NOPAGE
 * Completer: NOPAGE	=> OWNED
 */
static int do_donate(struct pkvm_mem_donation *donation)
{
	int ret;

	ret = check_donation(donation);
	if (ret)
		return ret;

	return WARN_ON(__do_donate(donation));
}

int __pkvm_host_share_hyp(u64 pfn)
{
	int ret;
	u64 host_addr = hyp_pfn_to_phys(pfn);
	u64 hyp_addr = (u64)__hyp_va(host_addr);
	struct pkvm_mem_share share = {
		.tx	= {
			.nr_pages	= 1,
			.initiator	= {
				.id	= PKVM_ID_HOST,
				.addr	= host_addr,
				.host	= {
					.completer_addr = hyp_addr,
				},
			},
			.completer	= {
				.id	= PKVM_ID_HYP,
			},
		},
		.completer_prot	= PAGE_HYP,
	};

	host_lock_component();
	hyp_lock_component();

	ret = do_share(&share);

	hyp_unlock_component();
	host_unlock_component();

	return ret;
}

int __pkvm_guest_share_host(struct kvm_vcpu *vcpu, u64 ipa)
{
	int ret;
	struct pkvm_mem_share share = {
		.tx	= {
			.nr_pages	= 1,
			.initiator	= {
				.id	= PKVM_ID_GUEST,
				.addr	= ipa,
				.guest	= {
					.vcpu = vcpu,
				},
			},
			.completer	= {
				.id	= PKVM_ID_HOST,
			},
		},
		.completer_prot	= PKVM_HOST_MEM_PROT,
	};

	host_lock_component();
	guest_lock_component(vcpu);

	ret = do_share(&share);

	guest_unlock_component(vcpu);
	host_unlock_component();

	return ret;
}

int __pkvm_guest_unshare_host(struct kvm_vcpu *vcpu, u64 ipa)
{
	int ret;
	struct pkvm_mem_share share = {
		.tx	= {
			.nr_pages	= 1,
			.initiator	= {
				.id	= PKVM_ID_GUEST,
				.addr	= ipa,
				.guest	= {
					.vcpu = vcpu,
				},
			},
			.completer	= {
				.id	= PKVM_ID_HOST,
			},
		},
		.completer_prot	= PKVM_HOST_MEM_PROT,
	};

	host_lock_component();
	guest_lock_component(vcpu);

	ret = do_unshare(&share);

	guest_unlock_component(vcpu);
	host_unlock_component();

	return ret;
}

int __pkvm_host_unshare_hyp(u64 pfn)
{
	int ret;
	u64 host_addr = hyp_pfn_to_phys(pfn);
	u64 hyp_addr = (u64)__hyp_va(host_addr);
	struct pkvm_mem_share share = {
		.tx	= {
			.nr_pages	= 1,
			.initiator	= {
				.id	= PKVM_ID_HOST,
				.addr	= host_addr,
				.host	= {
					.completer_addr = hyp_addr,
				},
			},
			.completer	= {
				.id	= PKVM_ID_HYP,
			},
		},
		.completer_prot	= PAGE_HYP,
	};

	host_lock_component();
	hyp_lock_component();

	ret = do_unshare(&share);

	hyp_unlock_component();
	host_unlock_component();

	return ret;
}

int __pkvm_host_donate_hyp(u64 pfn, u64 nr_pages)
{
	int ret;
	u64 host_addr = hyp_pfn_to_phys(pfn);
	u64 hyp_addr = (u64)__hyp_va(host_addr);
	struct pkvm_mem_donation donation = {
		.tx	= {
			.nr_pages	= nr_pages,
			.initiator	= {
				.id	= PKVM_ID_HOST,
				.addr	= host_addr,
				.host	= {
					.completer_addr = hyp_addr,
				},
			},
			.completer	= {
				.id	= PKVM_ID_HYP,
			},
		},
	};

	host_lock_component();
	hyp_lock_component();

	ret = do_donate(&donation);

	hyp_unlock_component();
	host_unlock_component();

	return ret;
}

int __pkvm_hyp_donate_host(u64 pfn, u64 nr_pages)
{
	int ret;
	u64 host_addr = hyp_pfn_to_phys(pfn);
	u64 hyp_addr = (u64)__hyp_va(host_addr);
	struct pkvm_mem_donation donation = {
		.tx	= {
			.nr_pages	= nr_pages,
			.initiator	= {
				.id	= PKVM_ID_HYP,
				.addr	= hyp_addr,
				.hyp	= {
					.completer_addr = host_addr,
				},
			},
			.completer	= {
				.id	= PKVM_ID_HOST,
			},
		},
	};

	host_lock_component();
	hyp_lock_component();

	ret = do_donate(&donation);

	hyp_unlock_component();
	host_unlock_component();

	return ret;
}

int hyp_pin_shared_mem(void *from, void *to)
{
	u64 cur, start = ALIGN_DOWN((u64)from, PAGE_SIZE);
	u64 end = PAGE_ALIGN((u64)to);
	u64 size = end - start;
	int ret;

	host_lock_component();
	hyp_lock_component();

	ret = __host_check_page_state_range(__hyp_pa(start), size,
					    PKVM_PAGE_SHARED_OWNED);
	if (ret)
		goto unlock;

	ret = __hyp_check_page_state_range(start, size,
					   PKVM_PAGE_SHARED_BORROWED);
	if (ret)
		goto unlock;

	for (cur = start; cur < end; cur += PAGE_SIZE)
		hyp_page_ref_inc(hyp_virt_to_page(cur));

unlock:
	hyp_unlock_component();
	host_unlock_component();

	return ret;
}

void hyp_unpin_shared_mem(void *from, void *to)
{
	u64 cur, start = ALIGN_DOWN((u64)from, PAGE_SIZE);
	u64 end = PAGE_ALIGN((u64)to);

	host_lock_component();
	hyp_lock_component();

	for (cur = start; cur < end; cur += PAGE_SIZE)
		hyp_page_ref_dec(hyp_virt_to_page(cur));

	hyp_unlock_component();
	host_unlock_component();
}

int __pkvm_host_share_guest(u64 pfn, u64 gfn, struct kvm_vcpu *vcpu)
{
	int ret;
	u64 host_addr = hyp_pfn_to_phys(pfn);
	u64 guest_addr = hyp_pfn_to_phys(gfn);
	struct pkvm_mem_share share = {
		.tx	= {
			.nr_pages	= 1,
			.initiator	= {
				.id	= PKVM_ID_HOST,
				.addr	= host_addr,
				.host	= {
					.completer_addr = guest_addr,
				},
			},
			.completer	= {
				.id	= PKVM_ID_GUEST,
				.guest	= {
					.vcpu = vcpu,
					.phys = host_addr,
				},
			},
		},
		.completer_prot	= KVM_PGTABLE_PROT_RWX,
	};

	host_lock_component();
	guest_lock_component(vcpu);

	ret = do_share(&share);

	guest_unlock_component(vcpu);
	host_unlock_component();

	return ret;
}

int __pkvm_host_donate_guest(u64 pfn, u64 gfn, struct kvm_vcpu *vcpu)
{
	int ret;
	u64 host_addr = hyp_pfn_to_phys(pfn);
	u64 guest_addr = hyp_pfn_to_phys(gfn);
	struct pkvm_mem_donation donation = {
		.tx	= {
			.nr_pages	= 1,
			.initiator	= {
				.id	= PKVM_ID_HOST,
				.addr	= host_addr,
				.host	= {
					.completer_addr = guest_addr,
				},
			},
			.completer	= {
				.id	= PKVM_ID_GUEST,
				.guest	= {
					.vcpu = vcpu,
					.phys = host_addr,
				},
			},
		},
	};

	host_lock_component();
	guest_lock_component(vcpu);

	ret = do_donate(&donation);

	guest_unlock_component(vcpu);
	host_unlock_component();

	return ret;
}

int __pkvm_host_share_ffa(u64 pfn, u64 nr_pages)
{
	int ret;
	struct pkvm_mem_share share = {
		.tx	= {
			.nr_pages	= nr_pages,
			.initiator	= {
				.id	= PKVM_ID_HOST,
				.addr	= hyp_pfn_to_phys(pfn),
			},
			.completer	= {
				.id	= PKVM_ID_FFA,
			},
		},
	};

	host_lock_component();
	ret = do_share(&share);
	host_unlock_component();

	return ret;
}

int __pkvm_host_unshare_ffa(u64 pfn, u64 nr_pages)
{
	int ret;
	struct pkvm_mem_share share = {
		.tx	= {
			.nr_pages	= nr_pages,
			.initiator	= {
				.id	= PKVM_ID_HOST,
				.addr	= hyp_pfn_to_phys(pfn),
			},
			.completer	= {
				.id	= PKVM_ID_FFA,
			},
		},
	};

	host_lock_component();
	ret = do_unshare(&share);
	host_unlock_component();

	return ret;
}

static int hyp_zero_page(phys_addr_t phys)
{
	void *addr;

	addr = hyp_fixmap_map(phys);
	if (!addr)
		return -EINVAL;
	memset(addr, 0, PAGE_SIZE);
	/*
	 * Prefer kvm_flush_dcache_to_poc() over __clean_dcache_guest_page()
	 * here as the latter may elide the CMO under the assumption that FWB
	 * will be enabled on CPUs that support it. This is incorrect for the
	 * host stage-2 and would otherwise lead to a malicious host potentially
	 * being able to read the content of newly reclaimed guest pages.
	 */
	kvm_flush_dcache_to_poc(addr, PAGE_SIZE);

	return hyp_fixmap_unmap();
}

int __pkvm_host_reclaim_page(u64 pfn)
{
	u64 addr = hyp_pfn_to_phys(pfn);
	struct hyp_page *page;
	kvm_pte_t pte;
	int ret;

	host_lock_component();

	ret = kvm_pgtable_get_leaf(&host_kvm.pgt, addr, &pte, NULL);
	if (ret)
		goto unlock;

	if (host_get_page_state(pte) == PKVM_PAGE_OWNED)
		goto unlock;

	page = hyp_phys_to_page(addr);
	if (!(page->flags & HOST_PAGE_PENDING_RECLAIM)) {
		ret = -EPERM;
		goto unlock;
	}

	if (page->flags & HOST_PAGE_NEED_POISONING) {
		ret = hyp_zero_page(addr);
		if (ret)
			goto unlock;
		page->flags &= ~HOST_PAGE_NEED_POISONING;
		psci_mem_protect_dec();
	}

	ret = host_stage2_set_owner_locked(addr, PAGE_SIZE, pkvm_host_id);
	if (ret)
		goto unlock;
	page->flags &= ~HOST_PAGE_PENDING_RECLAIM;

unlock:
	host_unlock_component();

	return ret;
}

/* Replace this with something more structured once day */
#define MMIO_NOTE	(('M' << 24 | 'M' << 16 | 'I' << 8 | 'O') << 1)

static bool __check_ioguard_page(struct kvm_vcpu *vcpu, u64 ipa)
{
	struct kvm_shadow_vm *vm = vcpu->arch.pkvm.shadow_vm;
	kvm_pte_t pte;
	u32 level;
	int ret;

	ret = kvm_pgtable_get_leaf(&vm->pgt, ipa, &pte, &level);
	if (ret)
		return false;

	/* Must be a PAGE_SIZE mapping with our annotation */
	return (BIT(ARM64_HW_PGTABLE_LEVEL_SHIFT(level)) == PAGE_SIZE &&
		pte == MMIO_NOTE);
}

int __pkvm_install_ioguard_page(struct kvm_vcpu *vcpu, u64 ipa)
{
	struct kvm_shadow_vm *vm;
	kvm_pte_t pte;
	u32 level;
	int ret;

	vm = vcpu->arch.pkvm.shadow_vm;

	if (!test_bit(KVM_ARCH_FLAG_MMIO_GUARD, &vm->arch.flags))
		return -EINVAL;

	if (ipa & ~PAGE_MASK)
		return -EINVAL;

	guest_lock_component(vcpu);

	ret = kvm_pgtable_get_leaf(&vm->pgt, ipa, &pte, &level);
	if (ret)
		goto unlock;

	if (pte && BIT(ARM64_HW_PGTABLE_LEVEL_SHIFT(level)) == PAGE_SIZE) {
		/*
		 * Already flagged as MMIO, let's accept it, and fail
		 * otherwise
		 */
		if (pte != MMIO_NOTE)
			ret = -EBUSY;

		goto unlock;
	}

	ret = kvm_pgtable_stage2_annotate(&vm->pgt, ipa, PAGE_SIZE,
					  &vcpu->arch.pkvm_memcache,
					  MMIO_NOTE);

unlock:
	guest_unlock_component(vcpu);
	return ret;
}

int __pkvm_remove_ioguard_page(struct kvm_vcpu *vcpu, u64 ipa)
{
	struct kvm_shadow_vm *vm = vcpu->arch.pkvm.shadow_vm;

	if (!test_bit(KVM_ARCH_FLAG_MMIO_GUARD, &vm->arch.flags))
		return -EINVAL;

	guest_lock_component(vcpu);

	if (__check_ioguard_page(vcpu, ipa)) {
		struct kvm_shadow_vm *vm = vcpu->arch.pkvm.shadow_vm;

		kvm_pgtable_stage2_unmap(&vm->pgt,
					 ALIGN_DOWN(ipa, PAGE_SIZE), PAGE_SIZE);
	}

	guest_unlock_component(vcpu);
	return 0;
}

bool __pkvm_check_ioguard_page(struct kvm_vcpu *vcpu)
{
	struct kvm_shadow_vm *vm = vcpu->arch.pkvm.shadow_vm;
	u64 ipa, end;
	bool ret;

	if (!kvm_vcpu_dabt_isvalid(vcpu))
		return false;

	if (!test_bit(KVM_ARCH_FLAG_MMIO_GUARD, &vm->arch.flags))
		return true;

	ipa  = kvm_vcpu_get_fault_ipa(vcpu);
	ipa |= kvm_vcpu_get_hfar(vcpu) & FAR_MASK;
	end = ipa + kvm_vcpu_dabt_get_as(vcpu) - 1;

	guest_lock_component(vcpu);
	ret = __check_ioguard_page(vcpu, ipa);
	if ((end & PAGE_MASK) != (ipa & PAGE_MASK))
		ret &= __check_ioguard_page(vcpu, end);
	guest_unlock_component(vcpu);

	return ret;
}
