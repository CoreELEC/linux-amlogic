/*
 * drivers/amlogic/uvm/meson_uvm.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __MESON_UVM_H
#define __MESON_UVM_H

#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/miscdevice.h>
#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>

#include <linux/amlogic/media/vfm/vframe.h>

struct uvm_device;
struct uvm_buffer;

struct uvm_buffer {
	struct rb_node node;
	struct uvm_device *dev;

	size_t size;
	unsigned long flags;
	unsigned long align;
	int byte_stride;
	uint32_t width;
	uint32_t height;

	struct ion_handle *handle;
	struct sg_table *sgt;

	void *vaddr;
	phys_addr_t paddr;
	struct page **pages;
	struct dma_buf *dmabuf;

	struct vframe_s *vf;
	u32 index;
};

struct uvm_device {
	struct miscdevice dev;
	struct rb_root root;

	struct mutex buffer_lock;

	struct ion_client *uvm_client;
};

struct uvm_alloc_data {
	int size;
	int align;
	unsigned int flags;
	int v4l2_fd;
	int fd;
	int byte_stride;
	uint32_t width;
	uint32_t height;
};

#define UVM_IOC_MAGIC 'U'
#define UVM_IOC_ALLOC _IOWR(UVM_IOC_MAGIC, 0, \
				struct uvm_alloc_data)
#define UVM_IOC_FREE _IOWR(UVM_IOC_MAGIC, 1, \
				struct uvm_alloc_data)
#endif

