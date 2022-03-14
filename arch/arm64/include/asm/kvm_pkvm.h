// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 - Google LLC
 * Author: Quentin Perret <qperret@google.com>
 * Author: Fuad Tabba <tabba@google.com>
 */
#ifndef __ARM64_KVM_PKVM_H__
#define __ARM64_KVM_PKVM_H__

#include <linux/arm_ffa.h>
#include <linux/memblock.h>
#include <linux/scatterlist.h>
#include <asm/kvm_pgtable.h>
#include <asm/sysreg.h>

/* Maximum number of protected VMs that can be created. */
#define KVM_MAX_PVMS 255

#define HYP_MEMBLOCK_REGIONS 128
#define PVMFW_INVALID_LOAD_ADDR	(-1)

int kvm_arm_vm_ioctl_pkvm(struct kvm *kvm, struct kvm_enable_cap *cap);
int kvm_init_pvm(struct kvm *kvm, unsigned long type);
int create_el2_shadow(struct kvm *kvm);
void kvm_shadow_destroy(struct kvm *kvm);

/*
 * Definitions for features to be allowed or restricted for guest virtual
 * machines, depending on the mode KVM is running in and on the type of guest
 * that is running.
 *
 * The ALLOW masks represent a bitmask of feature fields that are allowed
 * without any restrictions as long as they are supported by the system.
 *
 * The RESTRICT_UNSIGNED masks, if present, represent unsigned fields for
 * features that are restricted to support at most the specified feature.
 *
 * If a feature field is not present in either, than it is not supported.
 *
 * The approach taken for protected VMs is to allow features that are:
 * - Needed by common Linux distributions (e.g., floating point)
 * - Trivial to support, e.g., supporting the feature does not introduce or
 * require tracking of additional state in KVM
 * - Cannot be trapped or prevent the guest from using anyway
 */

/*
 * Allow for protected VMs:
 * - Floating-point and Advanced SIMD
 * - Data Independent Timing
 */
#define PVM_ID_AA64PFR0_ALLOW (\
	ARM64_FEATURE_MASK(ID_AA64PFR0_FP) | \
	ARM64_FEATURE_MASK(ID_AA64PFR0_ASIMD) | \
	ARM64_FEATURE_MASK(ID_AA64PFR0_DIT) \
	)

/*
 * Restrict to the following *unsigned* features for protected VMs:
 * - AArch64 guests only (no support for AArch32 guests):
 *	AArch32 adds complexity in trap handling, emulation, condition codes,
 *	etc...
 * - RAS (v1)
 *	Supported by KVM
 */
#define PVM_ID_AA64PFR0_RESTRICT_UNSIGNED (\
	FIELD_PREP(ARM64_FEATURE_MASK(ID_AA64PFR0_EL0), ID_AA64PFR0_ELx_64BIT_ONLY) | \
	FIELD_PREP(ARM64_FEATURE_MASK(ID_AA64PFR0_EL1), ID_AA64PFR0_ELx_64BIT_ONLY) | \
	FIELD_PREP(ARM64_FEATURE_MASK(ID_AA64PFR0_EL2), ID_AA64PFR0_ELx_64BIT_ONLY) | \
	FIELD_PREP(ARM64_FEATURE_MASK(ID_AA64PFR0_EL3), ID_AA64PFR0_ELx_64BIT_ONLY) | \
	FIELD_PREP(ARM64_FEATURE_MASK(ID_AA64PFR0_RAS), ID_AA64PFR0_RAS_V1) \
	)

/*
 * Allow for protected VMs:
 * - Branch Target Identification
 * - Speculative Store Bypassing
 */
#define PVM_ID_AA64PFR1_ALLOW (\
	ARM64_FEATURE_MASK(ID_AA64PFR1_BT) | \
	ARM64_FEATURE_MASK(ID_AA64PFR1_SSBS) \
	)

/*
 * Allow for protected VMs:
 * - Mixed-endian
 * - Distinction between Secure and Non-secure Memory
 * - Mixed-endian at EL0 only
 * - Non-context synchronizing exception entry and exit
 */
#define PVM_ID_AA64MMFR0_ALLOW (\
	ARM64_FEATURE_MASK(ID_AA64MMFR0_BIGENDEL) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR0_SNSMEM) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR0_BIGENDEL0) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR0_EXS) \
	)

/*
 * Restrict to the following *unsigned* features for protected VMs:
 * - 40-bit IPA
 * - 16-bit ASID
 */
#define PVM_ID_AA64MMFR0_RESTRICT_UNSIGNED (\
	FIELD_PREP(ARM64_FEATURE_MASK(ID_AA64MMFR0_PARANGE), ID_AA64MMFR0_PARANGE_40) | \
	FIELD_PREP(ARM64_FEATURE_MASK(ID_AA64MMFR0_ASID), ID_AA64MMFR0_ASID_16) \
	)

/*
 * Allow for protected VMs:
 * - Hardware translation table updates to Access flag and Dirty state
 * - Number of VMID bits from CPU
 * - Hierarchical Permission Disables
 * - Privileged Access Never
 * - SError interrupt exceptions from speculative reads
 * - Enhanced Translation Synchronization
 */
#define PVM_ID_AA64MMFR1_ALLOW (\
	ARM64_FEATURE_MASK(ID_AA64MMFR1_HADBS) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR1_VMIDBITS) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR1_HPD) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR1_PAN) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR1_SPECSEI) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR1_ETS) \
	)

/*
 * Allow for protected VMs:
 * - Common not Private translations
 * - User Access Override
 * - IESB bit in the SCTLR_ELx registers
 * - Unaligned single-copy atomicity and atomic functions
 * - ESR_ELx.EC value on an exception by read access to feature ID space
 * - TTL field in address operations.
 * - Break-before-make sequences when changing translation block size
 * - E0PDx mechanism
 */
#define PVM_ID_AA64MMFR2_ALLOW (\
	ARM64_FEATURE_MASK(ID_AA64MMFR2_CNP) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR2_UAO) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR2_IESB) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR2_AT) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR2_IDS) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR2_TTL) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR2_BBM) | \
	ARM64_FEATURE_MASK(ID_AA64MMFR2_E0PD) \
	)

/*
 * No support for Scalable Vectors for protected VMs:
 *	Requires additional support from KVM, e.g., context-switching and
 *	trapping at EL2
 */
#define PVM_ID_AA64ZFR0_ALLOW (0ULL)

/*
 * No support for debug, including breakpoints, and watchpoints for protected
 * VMs:
 *	The Arm architecture mandates support for at least the Armv8 debug
 *	architecture, which would include at least 2 hardware breakpoints and
 *	watchpoints. Providing that support to protected guests adds
 *	considerable state and complexity. Therefore, the reserved value of 0 is
 *	used for debug-related fields.
 */
#define PVM_ID_AA64DFR0_ALLOW (0ULL)
#define PVM_ID_AA64DFR1_ALLOW (0ULL)

/*
 * No support for implementation defined features.
 */
#define PVM_ID_AA64AFR0_ALLOW (0ULL)
#define PVM_ID_AA64AFR1_ALLOW (0ULL)

/*
 * No restrictions on instructions implemented in AArch64.
 */
#define PVM_ID_AA64ISAR0_ALLOW (\
	ARM64_FEATURE_MASK(ID_AA64ISAR0_AES) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_SHA1) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_SHA2) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_CRC32) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_ATOMICS) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_RDM) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_SHA3) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_SM3) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_SM4) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_DP) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_FHM) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_TS) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_TLB) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR0_RNDR) \
	)

#define PVM_ID_AA64ISAR1_ALLOW (\
	ARM64_FEATURE_MASK(ID_AA64ISAR1_DPB) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_APA) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_API) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_JSCVT) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_FCMA) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_LRCPC) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_GPA) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_GPI) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_FRINTTS) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_SB) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_SPECRES) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_BF16) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_DGH) | \
	ARM64_FEATURE_MASK(ID_AA64ISAR1_I8MM) \
	)

/*
 * Returns the maximum number of breakpoints supported for protected VMs.
 */
static inline int pkvm_get_max_brps(void)
{
	int num = FIELD_GET(ARM64_FEATURE_MASK(ID_AA64DFR0_BRPS),
			    PVM_ID_AA64DFR0_ALLOW);

	/*
	 * If breakpoints are supported, the maximum number is 1 + the field.
	 * Otherwise, return 0, which is not compliant with the architecture,
	 * but is reserved and is used here to indicate no debug support.
	 */
	return num ? num + 1 : 0;
}

/*
 * Returns the maximum number of watchpoints supported for protected VMs.
 */
static inline int pkvm_get_max_wrps(void)
{
	int num = FIELD_GET(ARM64_FEATURE_MASK(ID_AA64DFR0_WRPS),
			    PVM_ID_AA64DFR0_ALLOW);

	return num ? num + 1 : 0;
}

extern struct memblock_region kvm_nvhe_sym(hyp_memory)[];
extern unsigned int kvm_nvhe_sym(hyp_memblock_nr);

extern phys_addr_t kvm_nvhe_sym(pvmfw_base);
extern phys_addr_t kvm_nvhe_sym(pvmfw_size);

static inline unsigned long
hyp_vmemmap_memblock_size(struct memblock_region *reg, size_t vmemmap_entry_size)
{
	unsigned long nr_pages = reg->size >> PAGE_SHIFT;
	unsigned long start, end;

	start = (reg->base >> PAGE_SHIFT) * vmemmap_entry_size;
	end = start + nr_pages * vmemmap_entry_size;
	start = ALIGN_DOWN(start, PAGE_SIZE);
	end = ALIGN(end, PAGE_SIZE);

	return end - start;
}

static inline unsigned long hyp_vmemmap_pages(size_t vmemmap_entry_size)
{
	unsigned long res = 0, i;

	for (i = 0; i < kvm_nvhe_sym(hyp_memblock_nr); i++) {
		res += hyp_vmemmap_memblock_size(&kvm_nvhe_sym(hyp_memory)[i],
						 vmemmap_entry_size);
	}

	return res >> PAGE_SHIFT;
}

static inline unsigned long hyp_shadow_table_pages(size_t shadow_entry_size)
{
	return PAGE_ALIGN(KVM_MAX_PVMS * shadow_entry_size) >> PAGE_SHIFT;
}

static inline unsigned long __hyp_pgtable_max_pages(unsigned long nr_pages)
{
	unsigned long total = 0, i;

	/* Provision the worst case scenario */
	for (i = 0; i < KVM_PGTABLE_MAX_LEVELS; i++) {
		nr_pages = DIV_ROUND_UP(nr_pages, PTRS_PER_PTE);
		total += nr_pages;
	}

	return total;
}

static inline unsigned long __hyp_pgtable_total_pages(void)
{
	unsigned long res = 0, i;

	/* Cover all of memory with page-granularity */
	for (i = 0; i < kvm_nvhe_sym(hyp_memblock_nr); i++) {
		struct memblock_region *reg = &kvm_nvhe_sym(hyp_memory)[i];
		res += __hyp_pgtable_max_pages(reg->size >> PAGE_SHIFT);
	}

	return res;
}

static inline unsigned long hyp_s1_pgtable_pages(void)
{
	unsigned long res;

	res = __hyp_pgtable_total_pages();

	/* Allow 1 GiB for private mappings */
	res += __hyp_pgtable_max_pages(SZ_1G >> PAGE_SHIFT);

	return res;
}

static inline unsigned long host_s2_pgtable_pages(void)
{
	unsigned long res;

	/*
	 * Include an extra 16 pages to safely upper-bound the worst case of
	 * concatenated pgds.
	 */
	res = __hyp_pgtable_total_pages() + 16;

	/* Allow 1 GiB for MMIO mappings */
	res += __hyp_pgtable_max_pages(SZ_1G >> PAGE_SHIFT);

	return res;
}

#define KVM_FFA_MBOX_NR_PAGES	1

static inline unsigned long hyp_ffa_proxy_pages(void)
{
	size_t desc_max;

	/*
	 * The hypervisor FFA proxy needs enough memory to buffer a fragmented
	 * descriptor returned from EL3 in response to a RETRIEVE_REQ call.
	 */
	desc_max = sizeof(struct ffa_mem_region) +
		   sizeof(struct ffa_mem_region_attributes) +
		   sizeof(struct ffa_composite_mem_region) +
		   SG_MAX_SEGMENTS * sizeof(struct ffa_mem_region_addr_range);

	/* Plus a page each for the hypervisor's RX and TX mailboxes. */
	return (2 * KVM_FFA_MBOX_NR_PAGES) + DIV_ROUND_UP(desc_max, PAGE_SIZE);
}

#endif	/* __ARM64_KVM_PKVM_H__ */
