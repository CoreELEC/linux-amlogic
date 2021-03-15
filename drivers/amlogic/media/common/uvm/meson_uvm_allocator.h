/*
 * drivers/amlogic/media/common/uvm/meson_uvm_allocator.h
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

#ifndef __MESON_UVM_ALLOCATOR_H
#define __MESON_UVM_ALLOCATOR_H

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

#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>

#define MUA_IMM_ALLOC	BIT(UVM_IMM_ALLOC)
#define MUA_DELAY_ALLOC	BIT(UVM_DELAY_ALLOC)

struct mua_device;
struct mua_buffer;

struct mua_buffer {
	struct uvm_buf_obj base;
	struct mua_device *dev;
	size_t size;
	struct ion_handle *handle;
	struct sg_table *sg_table;

	int byte_stride;
	u32 width;
	u32 height;
	phys_addr_t paddr;
	int commit_display;
	u32 index;
};

/**
 * struct mua_device - meson uvm allocator device
 *
 * @root:	rb tree root
 * @buffer_lock:	lock to protect rb tree
 */
struct mua_device {
	struct miscdevice dev;
	struct rb_root root;
	/* protects the rb tree root fields */
	struct mutex buffer_lock;
	struct ion_client *client;
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
	u32 width;
	u32 height;
	int scalar;
	int scaled_buf_size;
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

