/*
 * drivers/amlogic/media/common/uvm/meson_ion_delay_alloc.h
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

	struct file_private_data *file_private_data;
	u32 index;

	int commit_display;
};

struct uvm_device {
	struct miscdevice dev;
	struct rb_root root;

	struct mutex buffer_lock;

	struct ion_client *uvm_client;
	int pid;

	struct device *pdev;
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

struct uvm_pid_data {
	int pid;
};

struct uvm_fd_data {
	int fd;
	int commit_display;
};

union uvm_ioctl_arg {
	struct uvm_alloc_data alloc_data;
	struct uvm_pid_data pid_data;
	struct uvm_fd_data fd_data;
};

#define UVM_IOC_MAGIC 'U'
#define UVM_IOC_ALLOC _IOWR(UVM_IOC_MAGIC, 0, \
				struct uvm_alloc_data)
#define UVM_IOC_FREE _IOWR(UVM_IOC_MAGIC, 1, \
				struct uvm_alloc_data)
#define UVM_IOC_SET_PID _IOWR(UVM_IOC_MAGIC, 2, \
				struct uvm_pid_data)
#define UVM_IOC_SET_FD _IOWR(UVM_IOC_MAGIC, 3, \
				struct uvm_fd_data)
#endif

