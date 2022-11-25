// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/android_debug_symbols.h>
#include <asm/stacktrace.h>
#include <asm/sections.h>

#include <linux/cma.h>
#include "../../mm/slab.h"
#include <linux/memblock.h>
#include <linux/page_owner.h>
#include <linux/swap.h>
#include <linux/mm.h>
#include <linux/security.h>

struct ads_entry {
	char *name;
	void *addr;
};

bool ads_page_owner;
bool ads_slub_debug;
unsigned long ads_vmalloc_nr_pages;
unsigned long ads_pcpu_nr_pages;

#define _ADS_ENTRY(index, symbol)			\
	[index] = { .name = #symbol, .addr = (void *)symbol }
#define ADS_ENTRY(index, symbol) _ADS_ENTRY(index, symbol)

#define _ADS_PER_CPU_ENTRY(index, symbol)			\
	[index] = { .name = #symbol, .addr = (void *)&symbol }
#define ADS_PER_CPU_ENTRY(index, symbol) _ADS_PER_CPU_ENTRY(index, symbol)

/*
 * This module maintains static array of symbol and address information.
 * Add all required core kernel symbols and their addresses into ads_entries[] array,
 * so that vendor modules can query and to find address of non-exported symbol.
 */
static const struct ads_entry ads_entries[ADS_END] = {
	ADS_ENTRY(ADS_SDATA, _sdata),
	ADS_ENTRY(ADS_BSS_END, __bss_stop),
	ADS_ENTRY(ADS_PER_CPU_START, __per_cpu_start),
	ADS_ENTRY(ADS_PER_CPU_END, __per_cpu_end),
	ADS_ENTRY(ADS_START_RO_AFTER_INIT, __start_ro_after_init),
	ADS_ENTRY(ADS_END_RO_AFTER_INIT, __end_ro_after_init),
	ADS_ENTRY(ADS_LINUX_BANNER, linux_banner),
#ifdef CONFIG_CMA
	ADS_ENTRY(ADS_TOTAL_CMA, &totalcma_pages),
#endif
	ADS_ENTRY(ADS_SLAB_CACHES, &slab_caches),
	ADS_ENTRY(ADS_SLAB_MUTEX, &slab_mutex),
	ADS_ENTRY(ADS_MIN_LOW_PFN, &min_low_pfn),
	ADS_ENTRY(ADS_MAX_PFN, &max_pfn),
	ADS_ENTRY(ADS_VMALLOC_NR_PAGES, &ads_vmalloc_nr_pages),
	ADS_ENTRY(ADS_PCPU_NR_PAGES, &ads_pcpu_nr_pages),
#ifdef CONFIG_PAGE_OWNER
	ADS_ENTRY(ADS_PAGE_OWNER_ENABLED, &ads_page_owner),
#endif
#ifdef CONFIG_SLUB_DEBUG
	ADS_ENTRY(ADS_SLUB_DEBUG, &ads_slub_debug),
#endif
#ifdef CONFIG_SWAP
	ADS_ENTRY(ADS_NR_SWAP_PAGES, &nr_swap_pages),
#endif
#ifdef CONFIG_MMU
	ADS_ENTRY(ADS_MMAP_MIN_ADDR, &mmap_min_addr),
#endif
	ADS_ENTRY(ADS_STACK_GUARD_GAP, &stack_guard_gap),
#ifdef CONFIG_SYSCTL
	ADS_ENTRY(ADS_SYSCTL_LEGACY_VA_LAYOUT, &sysctl_legacy_va_layout),
#endif
	ADS_ENTRY(ADS_SHOW_MEM, show_mem),
#ifdef CONFIG_ARM64
	ADS_ENTRY(ADS_PUT_TASK_STACK, put_task_stack),
#endif
};

/*
 * ads_per_cpu_entries array contains all the per_cpu variable address information.
 */
static const struct ads_entry ads_per_cpu_entries[ADS_DEBUG_PER_CPU_END] = {
#ifdef CONFIG_ARM64
	ADS_PER_CPU_ENTRY(ADS_IRQ_STACK_PTR, irq_stack_ptr),
#endif
#ifdef CONFIG_X86
	ADS_PER_CPU_ENTRY(ADS_IRQ_STACK_PTR, hardirq_stack_ptr),
#endif
};

/*
 * android_debug_symbol - Provide address inforamtion of debug symbol.
 * @symbol: Index of debug symbol array.
 *
 * Return address of core kernel symbol on success and a negative errno will be
 * returned in error cases.
 *
 */
void *android_debug_symbol(enum android_debug_symbol symbol)
{
	if (symbol >= ADS_END)
		return ERR_PTR(-EINVAL);

	return ads_entries[symbol].addr;
}
EXPORT_SYMBOL_NS_GPL(android_debug_symbol, MINIDUMP);

/*
 * android_debug_per_cpu_symbol - Provide address inforamtion of per cpu debug symbol.
 * @symbol: Index of per cpu debug symbol array.
 *
 * Return address of core kernel symbol on success and a negative errno will be
 * returned in error cases.
 *
 */
void *android_debug_per_cpu_symbol(enum android_debug_per_cpu_symbol symbol)
{
	if (symbol >= ADS_DEBUG_PER_CPU_END)
		return ERR_PTR(-EINVAL);

	return ads_per_cpu_entries[symbol].addr;
}
EXPORT_SYMBOL_NS_GPL(android_debug_per_cpu_symbol, MINIDUMP);

static int __init debug_symbol_init(void)
{
#ifdef CONFIG_PAGE_OWNER
	ads_page_owner  = page_owner_ops.need();
#endif
#ifdef CONFIG_SLUB_DEBUG
	ads_slub_debug = __slub_debug_enabled();
#endif
	ads_vmalloc_nr_pages = vmalloc_nr_pages();
	ads_pcpu_nr_pages = pcpu_nr_pages();
	return 0;
}
module_init(debug_symbol_init);

static void __exit debug_symbol_exit(void)
{ }
module_exit(debug_symbol_exit);

MODULE_DESCRIPTION("Debug Symbol Driver");
MODULE_LICENSE("GPL v2");
