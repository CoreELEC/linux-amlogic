/*
 * drivers/staging/android/ion/ion_fb_heap.c
 *
 * Copyright (C) 2021 amlogic, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion.h"
#include "ion_priv.h"

#define ION_FB_ALLOCATE_FAIL	-1
#define ION_FLAG_EXTEND_MESON_HEAP          BIT(30)

struct ion_fb_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
};

static ion_phys_addr_t ion_fb_allocate(struct ion_heap *heap,
				       unsigned long size,
				       unsigned long align)
{
	struct ion_fb_heap *fb_heap =
		container_of(heap, struct ion_fb_heap, heap);
	unsigned long offset = gen_pool_alloc(fb_heap->pool, size);

	if (!offset)
		return ION_FB_ALLOCATE_FAIL;

	return offset;
}

static void ion_fb_free(struct ion_heap *heap, ion_phys_addr_t addr,
			unsigned long size)
{
	struct ion_fb_heap *fb_heap =
		container_of(heap, struct ion_fb_heap, heap);

	if (addr == ION_FB_ALLOCATE_FAIL)
		return;
	gen_pool_free(fb_heap->pool, addr, size);
}

static int ion_fb_heap_allocate(struct ion_heap *heap,
				struct ion_buffer *buffer,
				unsigned long size, unsigned long align,
				unsigned long flags)
{
	struct sg_table *table;
	ion_phys_addr_t paddr;
	int ret;

	if (align > PAGE_SIZE)
		return -EINVAL;

	if (!(flags & ION_FLAG_EXTEND_MESON_HEAP))
		return -ENOMEM;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr = ion_fb_allocate(heap, size, align);
	if (paddr == ION_FB_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->sg_table = table;

	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_fb_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	ion_fb_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops fb_heap_ops = {
	.allocate = ion_fb_heap_allocate,
	.free = ion_fb_heap_free,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_fb_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_fb_heap *fb_heap;
	int ret;
	struct page *page;
	size_t size;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	ion_pages_sync_for_device(NULL, page, size, DMA_BIDIRECTIONAL);

	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);
	fb_heap = kzalloc(sizeof(*fb_heap), GFP_KERNEL);
	if (!fb_heap)
		return ERR_PTR(-ENOMEM);

	fb_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!fb_heap->pool) {
		kfree(fb_heap);
		return ERR_PTR(-ENOMEM);
	}
	fb_heap->base = heap_data->base;
	gen_pool_add(fb_heap->pool, fb_heap->base, heap_data->size, -1);
	fb_heap->heap.ops = &fb_heap_ops;
	fb_heap->heap.type = ION_HEAP_TYPE_CUSTOM;
	fb_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	return &fb_heap->heap;
}

void ion_fb_heap_destroy(struct ion_heap *heap)
{
	struct ion_fb_heap *fb_heap =
		container_of(heap, struct  ion_fb_heap, heap);

	gen_pool_destroy(fb_heap->pool);
	kfree(fb_heap);
	fb_heap = NULL;
}
