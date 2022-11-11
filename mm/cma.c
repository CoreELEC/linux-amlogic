// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Contiguous Memory Allocator
 *
 * Copyright (c) 2010-2011 by Samsung Electronics.
 * Copyright IBM Corporation, 2013
 * Copyright LG Electronics Inc., 2014
 * Written by:
 *	Marek Szyprowski <m.szyprowski@samsung.com>
 *	Michal Nazarewicz <mina86@mina86.com>
 *	Aneesh Kumar K.V <aneesh.kumar@linux.vnet.ibm.com>
 *	Joonsoo Kim <iamjoonsoo.kim@lge.com>
 */

#define pr_fmt(fmt) "cma: " fmt

#ifdef CONFIG_CMA_DEBUG
#ifndef DEBUG
#ifndef CONFIG_AMLOGIC_MODIFY
#  define DEBUG
#endif
#endif
#endif
#define CREATE_TRACE_POINTS

#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/log2.h>
#include <linux/cma.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/kmemleak.h>
#include <trace/events/cma.h>
#ifdef CONFIG_AMLOGIC_CMA
#include <asm/pgtable.h>
#include <linux/amlogic/aml_cma.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>
#endif /* CONFIG_AMLOGIC_CMA */
#ifdef CONFIG_AMLOGIC_SEC
#include <linux/amlogic/secmon.h>
#endif

#include "cma.h"

struct cma cma_areas[MAX_CMA_AREAS];
unsigned cma_area_count;

#ifdef CONFIG_AMLOGIC_CMA
static DEFINE_MUTEX(cma_mutex);
void cma_init_clear(struct cma *cma, bool clear)
{
	cma->clear_map = clear;
}

#ifdef CONFIG_ARM64
static int clear_cma_pagemap2(struct cma *cma)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	unsigned long addr, end;
	struct mm_struct *mm;

	addr = (unsigned long)pfn_to_kaddr(cma->base_pfn);
	end  = addr + cma->count * PAGE_SIZE;
	mm = &init_mm;
	for (; addr < end; addr += SECTION_SIZE) {
		pgd = pgd_offset(mm, addr);
		if (pgd_none(*pgd) || pgd_bad(*pgd))
			break;

		pud = pud_offset(pgd, addr);
		if (pud_none(*pud) || pud_bad(*pud))
			break;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd))
			break;

		pr_debug("%s, addr:%lx, pgd:%p %llx, pmd:%p %llx\n",
			 __func__, addr, pgd,
			 pgd_val(*pgd), pmd, pmd_val(*pmd));
		pmd_clear(pmd);
	}

	return 0;
}
#endif

int setup_cma_full_pagemap(struct cma *cma)
{
#ifdef CONFIG_ARM
	/*
	 * arm already create level 3 mmu mapping for lowmem cma.
	 * And if high mem cma, there is no mapping. So nothing to
	 * do for arch arm.
	 */
	return 0;
#elif defined(CONFIG_ARM64)
	struct vm_area_struct vma = {};
	unsigned long addr, size;
	int ret;

	clear_cma_pagemap2(cma);
	addr = (unsigned long)pfn_to_kaddr(cma->base_pfn);
	size = cma->count * PAGE_SIZE;
	vma.vm_mm    = &init_mm;
	vma.vm_start = addr;
	vma.vm_end   = addr + size;
	vma.vm_page_prot = PAGE_KERNEL;
	ret = remap_pfn_range(&vma, addr, cma->base_pfn,
			      size, vma.vm_page_prot);
	if (ret < 0)
		pr_info("%s, remap pte failed:%d, cma:%lx\n",
			__func__, ret, cma->base_pfn);
	return 0;
#else
	#error "NOT supported ARCH"
#endif
}

static struct cma *find_cma(struct page *page)
{
	unsigned long pfn;
	struct cma *cma;
	int i;

	pfn = page_to_pfn(page);
	for (i = 0; i < cma_area_count; i++) {
		cma = &cma_areas[i];
		if (cma->base_pfn <= pfn && pfn < cma->base_pfn + cma->count)
			return cma;
	}
	return NULL;
}

int cma_mmu_op(struct page *page, int count, bool set)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long addr, end;
	struct mm_struct *mm;
	struct cma *cma;

	if (!page || PageHighMem(page))
		return -EINVAL;

	cma  = find_cma(page);
	if (!cma || !cma->clear_map) {
		pr_debug("%s, page:%lx is not cma or no clear-map, cma:%px\n",
			 __func__, page_to_pfn(page), cma);
		return -EINVAL;
	}

	addr = (unsigned long)page_address(page);
	end  = addr + count * PAGE_SIZE;
	mm = &init_mm;
	for (; addr < end; addr += PAGE_SIZE) {
		pgd = pgd_offset(mm, addr);
		if (pgd_none(*pgd) || pgd_bad(*pgd))
			break;

		pud = pud_offset(pgd, addr);
		if (pud_none(*pud) || pud_bad(*pud))
			break;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd))
			break;

		pte = pte_offset_map(pmd, addr);
		if (set)
			set_pte_at(mm, addr, pte, mk_pte(page, PAGE_KERNEL));
		else
			pte_clear(mm, addr, pte);
		pte_unmap(pte);
	#ifdef CONFIG_ARM
		pr_debug("%s, add:%lx, pgd:%p %x, pmd:%p %x, pte:%p %x\n",
			 __func__, addr, pgd, (int)pgd_val(*pgd),
			 pmd, (int)pmd_val(*pmd), pte, (int)pte_val(*pte));
	#elif defined(CONFIG_ARM64)
		pr_debug("%s, add:%lx, pgd:%p %llx, pmd:%p %llx, pte:%p %llx\n",
			 __func__, addr, pgd, pgd_val(*pgd),
			 pmd, pmd_val(*pmd), pte, pte_val(*pte));
	#endif
		page++;
	}
	return 0;
}
EXPORT_SYMBOL(cma_mmu_op);
#endif
phys_addr_t cma_get_base(const struct cma *cma)
{
	return PFN_PHYS(cma->base_pfn);
}

unsigned long cma_get_size(const struct cma *cma)
{
	return cma->count << PAGE_SHIFT;
}

const char *cma_get_name(const struct cma *cma)
{
	return cma->name ? cma->name : "(undefined)";
}
EXPORT_SYMBOL_GPL(cma_get_name);

static unsigned long cma_bitmap_aligned_mask(const struct cma *cma,
					     unsigned int align_order)
{
	if (align_order <= cma->order_per_bit)
		return 0;
	return (1UL << (align_order - cma->order_per_bit)) - 1;
}

/*
 * Find the offset of the base PFN from the specified align_order.
 * The value returned is represented in order_per_bits.
 */
static unsigned long cma_bitmap_aligned_offset(const struct cma *cma,
					       unsigned int align_order)
{
	return (cma->base_pfn & ((1UL << align_order) - 1))
		>> cma->order_per_bit;
}

static unsigned long cma_bitmap_pages_to_bits(const struct cma *cma,
					      unsigned long pages)
{
	return ALIGN(pages, 1UL << cma->order_per_bit) >> cma->order_per_bit;
}

static void cma_clear_bitmap(struct cma *cma, unsigned long pfn,
			     unsigned int count)
{
	unsigned long bitmap_no, bitmap_count;

	bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;
	bitmap_count = cma_bitmap_pages_to_bits(cma, count);

	mutex_lock(&cma->lock);
	bitmap_clear(cma->bitmap, bitmap_no, bitmap_count);
	mutex_unlock(&cma->lock);
}

static void __init cma_activate_area(struct cma *cma)
{
	unsigned long base_pfn = cma->base_pfn, pfn = base_pfn;
	unsigned i = cma->count >> pageblock_order;
	struct zone *zone;

	cma->bitmap = bitmap_zalloc(cma_bitmap_maxno(cma), GFP_KERNEL);
	if (!cma->bitmap)
		goto out_error;

	WARN_ON_ONCE(!pfn_valid(pfn));
	zone = page_zone(pfn_to_page(pfn));

	do {
		unsigned j;

		base_pfn = pfn;
		for (j = pageblock_nr_pages; j; --j, pfn++) {
			WARN_ON_ONCE(!pfn_valid(pfn));
		}
		init_cma_reserved_pageblock(pfn_to_page(base_pfn));
	} while (--i);

	mutex_init(&cma->lock);

#ifdef CONFIG_AMLOGIC_CMA
	if (cma->clear_map)
		setup_cma_full_pagemap(cma);
#endif

#ifdef CONFIG_CMA_DEBUGFS
	INIT_HLIST_HEAD(&cma->mem_head);
	spin_lock_init(&cma->mem_head_lock);
#endif

	return;

out_error:
	cma->count = 0;
	pr_err("CMA area %s could not be activated\n", cma->name);
	return;
}

static int __init cma_init_reserved_areas(void)
{
	int i;

	for (i = 0; i < cma_area_count; i++)
		cma_activate_area(&cma_areas[i]);

#ifdef CONFIG_AMLOGIC_SEC
	/*
	 * A73 cache speculate prefetch may cause SError when boot.
	 * because it may prefetch cache line in secure memory range
	 * which have already reserved by bootloader. So we must
	 * clear mmu of secmon range before A73 core boot up
	 */
	secmon_clear_cma_mmu();
#endif
	return 0;
}

#ifdef CONFIG_AMLOGIC_CMA
early_initcall(cma_init_reserved_areas);
#else
core_initcall(cma_init_reserved_areas);
#endif

/**
 * cma_init_reserved_mem() - create custom contiguous area from reserved memory
 * @base: Base address of the reserved area
 * @size: Size of the reserved area (in bytes),
 * @order_per_bit: Order of pages represented by one bit on bitmap.
 * @name: The name of the area. If this parameter is NULL, the name of
 *        the area will be set to "cmaN", where N is a running counter of
 *        used areas.
 * @res_cma: Pointer to store the created cma region.
 *
 * This function creates custom contiguous area from already reserved memory.
 */
int __init cma_init_reserved_mem(phys_addr_t base, phys_addr_t size,
				 unsigned int order_per_bit,
				 const char *name,
				 struct cma **res_cma)
{
	struct cma *cma;
	phys_addr_t alignment;

	/* Sanity checks */
	if (cma_area_count == ARRAY_SIZE(cma_areas)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	if (!size || !memblock_is_region_reserved(base, size))
		return -EINVAL;

	/* ensure minimal alignment required by mm core */
	alignment = PAGE_SIZE <<
			max_t(unsigned long, MAX_ORDER - 1, pageblock_order);

	/* alignment should be aligned with order_per_bit */
	if (!IS_ALIGNED(alignment >> PAGE_SHIFT, 1 << order_per_bit))
		return -EINVAL;

	if (ALIGN(base, alignment) != base || ALIGN(size, alignment) != size)
		return -EINVAL;

	/*
	 * Each reserved area must be initialised later, when more kernel
	 * subsystems (like slab allocator) are available.
	 */
	cma = &cma_areas[cma_area_count];
	if (name) {
		cma->name = name;
	} else {
		cma->name = kasprintf(GFP_KERNEL, "cma%d\n", cma_area_count);
		if (!cma->name)
			return -ENOMEM;
	}
	cma->base_pfn = PFN_DOWN(base);
	cma->count = size >> PAGE_SHIFT;
	cma->order_per_bit = order_per_bit;
	*res_cma = cma;
	cma_area_count++;
	totalcma_pages += (size / PAGE_SIZE);

	return 0;
}

/**
 * cma_declare_contiguous() - reserve custom contiguous area
 * @base: Base address of the reserved area optional, use 0 for any
 * @size: Size of the reserved area (in bytes),
 * @limit: End address of the reserved memory (optional, 0 for any).
 * @alignment: Alignment for the CMA area, should be power of 2 or zero
 * @order_per_bit: Order of pages represented by one bit on bitmap.
 * @fixed: hint about where to place the reserved area
 * @name: The name of the area. See function cma_init_reserved_mem()
 * @res_cma: Pointer to store the created cma region.
 *
 * This function reserves memory from early allocator. It should be
 * called by arch specific code once the early allocator (memblock or bootmem)
 * has been activated and all other subsystems have already allocated/reserved
 * memory. This function allows to create custom reserved areas.
 *
 * If @fixed is true, reserve contiguous area at exactly @base.  If false,
 * reserve in range from @base to @limit.
 */
int __init cma_declare_contiguous(phys_addr_t base,
			phys_addr_t size, phys_addr_t limit,
			phys_addr_t alignment, unsigned int order_per_bit,
			bool fixed, const char *name, struct cma **res_cma)
{
	phys_addr_t memblock_end = memblock_end_of_DRAM();
	phys_addr_t highmem_start;
	int ret = 0;

	/*
	 * We can't use __pa(high_memory) directly, since high_memory
	 * isn't a valid direct map VA, and DEBUG_VIRTUAL will (validly)
	 * complain. Find the boundary by adding one to the last valid
	 * address.
	 */
	highmem_start = __pa(high_memory - 1) + 1;
	pr_debug("%s(size %pa, base %pa, limit %pa alignment %pa)\n",
		__func__, &size, &base, &limit, &alignment);

	if (cma_area_count == ARRAY_SIZE(cma_areas)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	if (!size)
		return -EINVAL;

	if (alignment && !is_power_of_2(alignment))
		return -EINVAL;

	/*
	 * Sanitise input arguments.
	 * Pages both ends in CMA area could be merged into adjacent unmovable
	 * migratetype page by page allocator's buddy algorithm. In the case,
	 * you couldn't get a contiguous memory, which is not what we want.
	 */
	alignment = max(alignment,  (phys_addr_t)PAGE_SIZE <<
			  max_t(unsigned long, MAX_ORDER - 1, pageblock_order));
	if (fixed && base & (alignment - 1)) {
		ret = -EINVAL;
		pr_err("Region at %pa must be aligned to %pa bytes\n",
			&base, &alignment);
		goto err;
	}
	base = ALIGN(base, alignment);
	size = ALIGN(size, alignment);
	limit &= ~(alignment - 1);

	if (!base)
		fixed = false;

	/* size should be aligned with order_per_bit */
	if (!IS_ALIGNED(size >> PAGE_SHIFT, 1 << order_per_bit))
		return -EINVAL;

	/*
	 * If allocating at a fixed base the request region must not cross the
	 * low/high memory boundary.
	 */
	if (fixed && base < highmem_start && base + size > highmem_start) {
		ret = -EINVAL;
		pr_err("Region at %pa defined on low/high memory boundary (%pa)\n",
			&base, &highmem_start);
		goto err;
	}

	/*
	 * If the limit is unspecified or above the memblock end, its effective
	 * value will be the memblock end. Set it explicitly to simplify further
	 * checks.
	 */
	if (limit == 0 || limit > memblock_end)
		limit = memblock_end;

	if (base + size > limit) {
		ret = -EINVAL;
		pr_err("Size (%pa) of region at %pa exceeds limit (%pa)\n",
			&size, &base, &limit);
		goto err;
	}

	/* Reserve memory */
	if (fixed) {
		if (memblock_is_region_reserved(base, size) ||
		    memblock_reserve(base, size) < 0) {
			ret = -EBUSY;
			goto err;
		}
	} else {
		phys_addr_t addr = 0;

		/*
		 * All pages in the reserved area must come from the same zone.
		 * If the requested region crosses the low/high memory boundary,
		 * try allocating from high memory first and fall back to low
		 * memory in case of failure.
		 */
		if (base < highmem_start && limit > highmem_start) {
			addr = memblock_phys_alloc_range(size, alignment,
							 highmem_start, limit);
			limit = highmem_start;
		}

		if (!addr) {
			addr = memblock_phys_alloc_range(size, alignment, base,
							 limit);
			if (!addr) {
				ret = -ENOMEM;
				goto err;
			}
		}

		/*
		 * kmemleak scans/reads tracked objects for pointers to other
		 * objects but this address isn't mapped and accessible
		 */
		kmemleak_ignore_phys(addr);
		base = addr;
	}

	ret = cma_init_reserved_mem(base, size, order_per_bit, name, res_cma);
	if (ret)
		goto free_mem;

	pr_info("Reserved %ld MiB at %pa\n", (unsigned long)size / SZ_1M,
		&base);
	return 0;

free_mem:
	memblock_free(base, size);
err:
	pr_err("Failed to reserve %ld MiB\n", (unsigned long)size / SZ_1M);
	return ret;
}

#ifdef CONFIG_CMA_DEBUG
static void cma_debug_show_areas(struct cma *cma)
{
	unsigned long next_zero_bit, next_set_bit, nr_zero;
	unsigned long start = 0;
	unsigned long nr_part, nr_total = 0;
	unsigned long nbits = cma_bitmap_maxno(cma);

	mutex_lock(&cma->lock);
	pr_info("number of available pages: ");
	for (;;) {
		next_zero_bit = find_next_zero_bit(cma->bitmap, nbits, start);
		if (next_zero_bit >= nbits)
			break;
		next_set_bit = find_next_bit(cma->bitmap, nbits, next_zero_bit);
		nr_zero = next_set_bit - next_zero_bit;
		nr_part = nr_zero << cma->order_per_bit;
		pr_cont("%s%lu@%lu", nr_total ? "+" : "", nr_part,
			next_zero_bit);
		nr_total += nr_part;
		start = next_zero_bit + nr_zero;
	}
	pr_cont("=> %lu free of %lu total pages\n", nr_total, cma->count);
	mutex_unlock(&cma->lock);
}
#else
static inline void cma_debug_show_areas(struct cma *cma) { }
#endif

#if defined(CONFIG_AMLOGIC_CMA) && defined(CONFIG_AMLOGIC_PAGE_TRACE)
#include <linux/amlogic/page_trace.h>
#define POOL_SIZE		128
struct cma_owner {
	unsigned long ip;
	unsigned long cnt;
};

static int find_cma_owner(struct cma_owner *c, unsigned long ip)
{
	int i;

	if (!ip)
		return -1;

	for (i = 0; i < POOL_SIZE; i++) {
		if (!c[i].ip)
			c[i].ip = ip;

		if (c[i].ip == ip) {
			c[i].cnt++;
			return i;
		}
	}
	return -1;
}

static void show_cma_usage(struct cma *cma)
{
	struct cma_owner *c;
	unsigned long free = 0, ip;
	struct page *page;
	int i;

	if (!cma || !cma->count)
		return;

	c = kzalloc(sizeof(*c) * POOL_SIZE, GFP_KERNEL);
	if (!c)
		return;

	page = pfn_to_page(cma->base_pfn);
	for (i = 0; i < cma->count; i++) {
		ip = get_page_trace(page);
		if (find_cma_owner(c, ip) < 0)
			free++;
		page++;
	}
	for (i = 0; i < POOL_SIZE; i++) {
		if (!c[i].ip)
			break;
		cma_debug(0, NULL, "%s, count:%5ld, func:%ps\n",
			__func__, c[i].cnt, (void *)c[i].ip);
	}
	cma_debug(0, NULL, "%s, free pages:%ld, pool:%ld, base:%lx\n",
		__func__, free, cma->count, cma->base_pfn);
	kfree(c);
}
#endif

/**
 * cma_alloc() - allocate pages from contiguous area
 * @cma:   Contiguous memory region for which the allocation is performed.
 * @count: Requested number of pages.
 * @align: Requested alignment of pages (in PAGE_SIZE order).
 * @no_warn: Avoid printing message about failed allocation
 *
 * This function allocates part of contiguous memory on specific
 * contiguous memory area.
 */
struct page *cma_alloc(struct cma *cma, size_t count, unsigned int align,
		       bool no_warn)
{
	unsigned long mask, offset;
	unsigned long pfn = -1;
	unsigned long start = 0;
	unsigned long bitmap_maxno, bitmap_no, bitmap_count;
	size_t i;
	struct page *page = NULL;
	int ret = -ENOMEM;
#ifdef CONFIG_AMLOGIC_CMA
	int dummy;
	unsigned long long tick;
	unsigned long long in_tick, timeout;
#ifndef CONFIG_ARM64
	unsigned long pfn_limit;
	int ret_low, ret_high;
#endif

	in_tick = sched_clock();
#endif /* CONFIG_AMLOGIC_CMA */

	if (!cma || !cma->count)
		return NULL;

	pr_debug("%s(cma %p, count %zu, align %d)\n", __func__, (void *)cma,
		 count, align);
#ifdef CONFIG_AMLOGIC_CMA
	tick = sched_clock();
	cma_debug(0, NULL, "(cma %p, count %zu, align %d)\n",
		  (void *)cma, count, align);
	in_tick = sched_clock();
	timeout = 2ULL * 1000000 * (1 + ((count * PAGE_SIZE) >> 20));
#endif

	if (!count)
		return NULL;

	mask = cma_bitmap_aligned_mask(cma, align);
	offset = cma_bitmap_aligned_offset(cma, align);
	bitmap_maxno = cma_bitmap_maxno(cma);
	bitmap_count = cma_bitmap_pages_to_bits(cma, count);

#ifdef CONFIG_AMLOGIC_CMA
	if (bitmap_count > bitmap_maxno) { /* debug */
		pr_err("input too large, count:%ld, cma base:%lx, size:%lx, %s\n",
		       (unsigned long)count, cma->base_pfn, cma->count, cma->name);
	}
#endif
	if (bitmap_count > bitmap_maxno)
		return NULL;

#ifdef CONFIG_AMLOGIC_CMA
	aml_cma_alloc_pre_hook(&dummy, count);
#endif /* CONFIG_AMLOGIC_CMA */
	for (;;) {
		mutex_lock(&cma->lock);
		bitmap_no = bitmap_find_next_zero_area_off(cma->bitmap,
				bitmap_maxno, start, bitmap_count, mask,
				offset);
		if (bitmap_no >= bitmap_maxno) {
			mutex_unlock(&cma->lock);
			#if defined(CONFIG_AMLOGIC_CMA) && defined(CONFIG_AMLOGIC_PAGE_TRACE)
			show_cma_usage(cma);
			#endif
			break;
		}
		bitmap_set(cma->bitmap, bitmap_no, bitmap_count);
		/*
		 * It's safe to drop the lock here. We've marked this region for
		 * our exclusive use. If the migration fails we will take the
		 * lock again and unmark it.
		 */
		mutex_unlock(&cma->lock);

		pfn = cma->base_pfn + (bitmap_no << cma->order_per_bit);
#ifdef CONFIG_AMLOGIC_CMA
		mutex_lock(&cma_mutex);
#ifndef CONFIG_ARM64
		if (!PageHighMem(pfn_to_page(pfn)) &&
			PageHighMem(pfn_to_page(pfn + count - 1))) {
			pfn_limit = ((unsigned long)high_memory - PAGE_OFFSET)
				>> PAGE_SHIFT;
			ret_low = aml_cma_alloc_range(pfn, pfn_limit);
			ret_high = aml_cma_alloc_range(pfn_limit, pfn + count);
			if (ret_low == 0 && ret_high == 0)
				ret = 0;
			else if ((ret_low == -EBUSY) || (ret_high == -EBUSY))
				ret = -EBUSY;
			else
				ret = ret_low | ret_high;
		} else {
			ret = aml_cma_alloc_range(pfn, pfn + count);
		}
#else
		ret = aml_cma_alloc_range(pfn, pfn + count);
#endif
		mutex_unlock(&cma_mutex);
#else
		ret = alloc_contig_range(pfn, pfn + count, MIGRATE_CMA,
				     GFP_KERNEL | (no_warn ? __GFP_NOWARN : 0));
#endif /* CONFIG_AMLOGIC_CMA */
		if (ret == 0) {
			page = pfn_to_page(pfn);
			break;
		}

		cma_clear_bitmap(cma, pfn, count);
		if (ret != -EBUSY)
			break;

		pr_debug("%s(): memory range at %p is busy, retrying\n",
			 __func__, pfn_to_page(pfn));
		/* try again with a bit different memory target */
	#ifndef CONFIG_AMLOGIC_CMA
		start = bitmap_no + mask + 1;
	#else
		/*
		 * CMA allocation time out, may blocked on some pages
		 * relax CPU and try later
		 */
		if ((sched_clock() - in_tick) >= timeout)
			usleep_range(1000, 2000);
	#endif /* CONFIG_AMLOGIC_CMA */
	}

	trace_cma_alloc(pfn, page, count, align);

	/*
	 * CMA can allocate multiple page blocks, which results in different
	 * blocks being marked with different tags. Reset the tags to ignore
	 * those page blocks.
	 */
	if (page) {
		for (i = 0; i < count; i++)
			page_kasan_tag_reset(page + i);
	}

	if (ret && !no_warn) {
#ifdef CONFIG_AMLOGIC_CMA
		pr_err("%s: alloc failed, req-size: %zu pages, ret: %d from:%lx, %s\n",
			__func__, count, ret, cma->base_pfn, cma->name);
#else
		pr_err("%s: alloc failed, req-size: %zu pages, ret: %d\n",
			__func__, count, ret);
#endif
		cma_debug_show_areas(cma);
	}

#ifdef CONFIG_AMLOGIC_CMA
	if (!no_warn)
		WARN_ONCE(!page, "can't alloc from %lx with size:%ld, ret:%d\n",
			cma->base_pfn, (unsigned long)count, ret);
	aml_cma_alloc_post_hook(&dummy, count, page);
	cma_debug(0, NULL, "return page:%lx, tick:%16lld\n",
		  page ? page_to_pfn(page) : 0, sched_clock() - tick);
#endif /* CONFIG_AMLOGIC_CMA */
	pr_debug("%s(): returned %p\n", __func__, page);
	return page;
}
EXPORT_SYMBOL_GPL(cma_alloc);

/**
 * cma_release() - release allocated pages
 * @cma:   Contiguous memory region for which the allocation is performed.
 * @pages: Allocated pages.
 * @count: Number of allocated pages.
 *
 * This function releases memory allocated by cma_alloc().
 * It returns false when provided pages do not belong to contiguous area and
 * true otherwise.
 */
bool cma_release(struct cma *cma, const struct page *pages, unsigned int count)
{
	unsigned long pfn;

	if (!cma || !pages)
		return false;

	pr_debug("%s(page %p)\n", __func__, (void *)pages);

	pfn = page_to_pfn(pages);

	if (pfn < cma->base_pfn || pfn >= cma->base_pfn + cma->count)
		return false;

	VM_BUG_ON(pfn + count > cma->base_pfn + cma->count);

#ifdef CONFIG_AMLOGIC_CMA
	aml_cma_release_hook(count, (struct page *)pages);
	aml_cma_free(pfn, count);
#else
	free_contig_range(pfn, count);
#endif /* CONFIG_AMLOGIC_CMA */
	cma_clear_bitmap(cma, pfn, count);
	trace_cma_release(pfn, pages, count);

	return true;
}
EXPORT_SYMBOL_GPL(cma_release);

int cma_for_each_area(int (*it)(struct cma *cma, void *data), void *data)
{
	int i;

	for (i = 0; i < cma_area_count; i++) {
		int ret = it(&cma_areas[i], data);

		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cma_for_each_area);
