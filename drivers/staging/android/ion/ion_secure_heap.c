/*
 * drivers/staging/android/ion/ion_secure_heap.c
 *
 * Copyright (C) 2019 amlogic, Inc.
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

#define ION_SECURE_ALLOCATE_FAIL	-1

struct ion_secure_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
};

static ion_phys_addr_t ion_secure_allocate(struct ion_heap *heap,
					   unsigned long size,
					   unsigned long align)
{
	struct ion_secure_heap *secure_heap =
		container_of(heap, struct ion_secure_heap, heap);
	unsigned long offset = gen_pool_alloc(secure_heap->pool, size);

	if (!offset)
		return ION_SECURE_ALLOCATE_FAIL;

	return offset;
}

static void ion_secure_free(struct ion_heap *heap, ion_phys_addr_t addr,
			    unsigned long size)
{
	struct ion_secure_heap *secure_heap =
		container_of(heap, struct ion_secure_heap, heap);

	if (addr == ION_SECURE_ALLOCATE_FAIL)
		return;
	gen_pool_free(secure_heap->pool, addr, size);
}

static int ion_secure_heap_allocate(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    unsigned long size, unsigned long align,
				    unsigned long flags)
{
	struct sg_table *table;
	ion_phys_addr_t paddr;
	int ret;

	if (align > PAGE_SIZE)
		return -EINVAL;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr = ion_secure_allocate(heap, size, align);
	if (paddr == ION_SECURE_ALLOCATE_FAIL) {
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

static void ion_secure_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	ion_secure_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops secure_heap_ops = {
	.allocate = ion_secure_heap_allocate,
	.free = ion_secure_heap_free,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_secure_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_secure_heap *secure_heap;
	int ret;
	struct page *page;
	size_t size;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	ion_pages_sync_for_device(NULL, page, size, DMA_BIDIRECTIONAL);

	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);
	secure_heap = kzalloc(sizeof(*secure_heap), GFP_KERNEL);
	if (!secure_heap)
		return ERR_PTR(-ENOMEM);

	secure_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!secure_heap->pool) {
		kfree(secure_heap);
		return ERR_PTR(-ENOMEM);
	}
	secure_heap->base = heap_data->base;
	gen_pool_add(secure_heap->pool, secure_heap->base, heap_data->size, -1);
	secure_heap->heap.ops = &secure_heap_ops;
	secure_heap->heap.type = ION_HEAP_TYPE_CUSTOM;
	secure_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	return &secure_heap->heap;
}

void ion_secure_heap_destroy(struct ion_heap *heap)
{
	struct ion_secure_heap *secure_heap =
		container_of(heap, struct  ion_secure_heap, heap);

	gen_pool_destroy(secure_heap->pool);
	kfree(secure_heap);
	secure_heap = NULL;
}
