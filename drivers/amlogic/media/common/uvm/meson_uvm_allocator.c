/*
 * drivers/amlogic/media/common/uvm/meson_uvm_allocator.c
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

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/pagemap.h>
#include <ion/ion.h>
#include <ion/ion_priv.h>
#include <linux/meson_ion.h>

#include "meson_uvm_allocator.h"
#include "meson_uvm_conf.h"

static struct mua_device *mdev;

static int enable_screencap;
module_param_named(enable_screencap, enable_screencap, int, 0664);
static int mua_debug_level;
module_param(mua_debug_level, int, 0644);

#define MUA_PRINTK(level, fmt, arg...) \
	do {	\
		if (mua_debug_level >= (level))     \
			pr_info("MUA: " fmt, ## arg); \
	} while (0)

static void mua_handle_free(struct uvm_buf_obj *obj)
{
	struct mua_buffer *buffer;

	buffer = container_of(obj, struct mua_buffer, base);
	MUA_PRINTK(1, "%s get ion_handle, %px\n", __func__, obj);
	if (buffer->handle)
		ion_free(mdev->client, buffer->handle);
	kfree(buffer);
}

static int meson_uvm_fill_pattern(struct mua_buffer *buffer,
					struct dma_buf *dmabuf, void *vaddr)
{
	struct v4l_data_t val_data;

	memset(&val_data, 0, sizeof(val_data));

	val_data.dst_addr = vaddr;
	val_data.byte_stride = buffer->byte_stride;
	val_data.width = buffer->width;
	val_data.height = buffer->height;
	val_data.phy_addr[0] = buffer->paddr;
	MUA_PRINTK(1, "%s. width=%d height=%d byte_stride=%d\n",
			__func__, buffer->width,
			buffer->height, buffer->byte_stride);
	v4lvideo_data_copy(&val_data, dmabuf);

	vunmap(vaddr);
	return 0;
}

static int mua_process_gpu_realloc(struct dma_buf *dmabuf,
					struct uvm_buf_obj *obj, int scalar)
{
	int i, j, num_pages;
	struct ion_handle *handle;
	struct uvm_alloc_info info;
	struct mua_buffer *buffer;
	struct page **tmp;
	struct page **page_array;
	pgprot_t pgprot;
	void *vaddr;
	phys_addr_t pat;
	size_t len;
	struct sg_table *src_sgt = NULL;
	struct scatterlist *sg = NULL;

	buffer = container_of(obj, struct mua_buffer, base);
	MUA_PRINTK(1, "%s. buf_scalar=%d WxH: %dx%d\n",
			__func__, scalar, buffer->width, buffer->height);
	memset(&info, 0, sizeof(info));

	MUA_PRINTK(1, "%p, current->tgid:%d mdev->pid:%d buffer->commit_display:%d.\n",
		__func__, current->tgid, mdev->pid, buffer->commit_display);

	if (!enable_screencap && current->tgid == mdev->pid &&
		buffer->commit_display) {
		MUA_PRINTK(0, "screen cap should not access the uvm buffer.\n");
		return -ENODEV;
	}
	dmabuf->size = buffer->size * scalar * scalar;
	MUA_PRINTK(1, "buffer->size:%zu realloc dmabuf->size=%zu\n",
			buffer->size, dmabuf->size);
	if (1 /*!buffer->handle*/) {
		handle = ion_alloc(mdev->client, dmabuf->size,
				0, (1 << ION_HEAP_TYPE_CUSTOM), 0);
		if (IS_ERR(handle)) {
			MUA_PRINTK(0, "%s: ion_alloc fail.\n", __func__);
			return -ENOMEM;
		}
		ion_phys(mdev->client, handle, (ion_phys_addr_t *)&pat, &len);
		buffer->paddr = pat;
		buffer->handle = handle;
		buffer->sg_table = handle->buffer->sg_table;
		info.sgt = handle->buffer->sg_table;
		dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
	}

	//start to do vmap
	if (!buffer->sg_table) {
		MUA_PRINTK(0, "none uvm buffer allocated.\n");
		return -ENODEV;
	}
	src_sgt = buffer->sg_table;
	num_pages = PAGE_ALIGN(dmabuf->size) / PAGE_SIZE;
	tmp = vmalloc(sizeof(struct page *) * num_pages);
	page_array = tmp;

	pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(src_sgt->sgl, sg, src_sgt->nents, i) {
		int npages_this_entry =
			PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}

	vaddr = vmap(page_array, num_pages, VM_MAP, pgprot);
	if (!vaddr) {
		MUA_PRINTK(0, "vmap fail, size: %d\n",
			   num_pages << PAGE_SHIFT);
		vfree(page_array);
		return -ENOMEM;
	}
	vfree(page_array);
	MUA_PRINTK(1, "buffer vaddr: %p.\n", vaddr);

	//start to filldata
	meson_uvm_fill_pattern(buffer, dmabuf, vaddr);

	return 0;
}

static int mua_process_delay_alloc(struct dma_buf *dmabuf,
					struct uvm_buf_obj *obj, u64 *flag)
{
	int i, j, num_pages;
	struct ion_handle *handle;
	struct uvm_alloc_info info;
	struct mua_buffer *buffer;
	struct page **tmp;
	struct page **page_array;
	pgprot_t pgprot;
	void *vaddr;
	phys_addr_t pat;
	size_t len;
	struct sg_table *src_sgt = NULL;
	struct scatterlist *sg = NULL;

	buffer = container_of(obj, struct mua_buffer, base);
	memset(&info, 0, sizeof(info));

	MUA_PRINTK(1, "%p, current->tgid:%d mdev->pid:%d buffer->commit_display:%d.\n",
		__func__, current->tgid, mdev->pid, buffer->commit_display);

	if (!enable_screencap && current->tgid == mdev->pid &&
		buffer->commit_display) {
		*flag |= BIT(UVM_SKIP_REALLOC);
		MUA_PRINTK(0, "screen cap should not access the uvm buffer. *flag:%llu\n", *flag);
		return -ENODEV;
	}

	if (!buffer->handle) {
		handle = ion_alloc(mdev->client, dmabuf->size, 0,
					(1 << ION_HEAP_TYPE_CUSTOM), 0);
		if (IS_ERR(handle)) {
			MUA_PRINTK(0, "%s: ion_alloc fail.\n", __func__);
			return -ENOMEM;
		}
		ion_phys(mdev->client, handle, (ion_phys_addr_t *)&pat, &len);
		buffer->paddr = pat;
		buffer->handle = handle;
		buffer->sg_table = handle->buffer->sg_table;
		info.sgt = handle->buffer->sg_table;

		dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
	}

	//start to do vmap
	if (!buffer->sg_table) {
		MUA_PRINTK(0, "none uvm buffer allocated.\n");
		return -ENODEV;
	}

	src_sgt = buffer->sg_table;
	num_pages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	tmp = vmalloc(sizeof(struct page *) * num_pages);
	page_array = tmp;

	pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(src_sgt->sgl, sg, src_sgt->nents, i) {
		int npages_this_entry =
			PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}

	vaddr = vmap(page_array, num_pages, VM_MAP, pgprot);
	if (!vaddr) {
		MUA_PRINTK(0, "vmap fail, size: %d\n",
			   num_pages << PAGE_SHIFT);
		vfree(page_array);
		return -ENOMEM;
	}
	vfree(page_array);
	MUA_PRINTK(1, "buffer vaddr: %p.\n", vaddr);

	//start to filldata
	meson_uvm_fill_pattern(buffer, dmabuf, vaddr);

	return 0;
}

static int mua_handle_alloc(struct dma_buf *dmabuf,
		struct uvm_alloc_data *data, int alloc_buf_size)
{
	struct mua_buffer *buffer;
	struct uvm_alloc_info info;
	struct ion_handle *handle;

	memset(&info, 0, sizeof(info));
	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	buffer->size = alloc_buf_size;
	buffer->dev = mdev;
	buffer->byte_stride = data->byte_stride;
	buffer->width = data->width;
	buffer->height = data->height;

	if (data->flags & MUA_IMM_ALLOC) {
		handle = ion_alloc(mdev->client, alloc_buf_size, 0,
				   (1 << ION_HEAP_TYPE_CUSTOM), 0);
		if (IS_ERR(handle)) {
			MUA_PRINTK(0, "%s: ion_alloc fail.\n", __func__);
			kfree(buffer);
			return -ENOMEM;
		}
		buffer->handle = handle;
		info.sgt = handle->buffer->sg_table;
		info.obj = &buffer->base;
		info.flags = data->flags;
		info.size = alloc_buf_size;
		info.scalar = data->scalar;
		info.gpu_realloc = mua_process_gpu_realloc;
		info.free = mua_handle_free;
		MUA_PRINTK(1, "UVM FLAGS is MUA_IMM_ALLOC, %px\n", info.obj);
	} else if (data->flags & MUA_DELAY_ALLOC) {
		info.size = data->size;
		info.obj = &buffer->base;
		info.flags = data->flags;
		info.delay_alloc = mua_process_delay_alloc;
		info.free = mua_handle_free;
		MUA_PRINTK(1, "UVM FLAGS is MUA_DELAY_ALLOC, %px\n", info.obj);
	} else {
		MUA_PRINTK(0, "unsupported MUA FLAGS.\n");
		kfree(buffer);
		return -EINVAL;
	}

	dmabuf_bind_uvm_alloc(dmabuf, &info);

	return 0;
}

static int mua_set_commit_display(int fd, int commit_display)
{
	struct dma_buf *dmabuf;
	struct uvm_buf_obj *obj;
	struct mua_buffer *buffer;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		MUA_PRINTK(0, "invalid dmabuf fd\n");
		return -EINVAL;
	}

	obj = dmabuf_get_uvm_buf_obj(dmabuf);
	buffer = container_of(obj, struct mua_buffer, base);
	buffer->commit_display = commit_display;

	dma_buf_put(dmabuf);
	return 0;
}

static long mua_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mua_device *md;
	union uvm_ioctl_arg data;
	struct dma_buf *dmabuf;
	int pid;
	int ret = 0;
	int fd = 0;
	int alloc_buf_size = 0;

	md = file->private_data;

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	switch (cmd) {
	case UVM_IOC_ALLOC:
		MUA_PRINTK(1, "%s. original buf size:%d width:%d height:%d\n",
					__func__, data.alloc_data.size,
					data.alloc_data.width,
					data.alloc_data.height);

		data.alloc_data.size = PAGE_ALIGN(data.alloc_data.size);
		data.alloc_data.scaled_buf_size =
				PAGE_ALIGN(data.alloc_data.scaled_buf_size);
		if (data.alloc_data.scalar > 1)
			alloc_buf_size = data.alloc_data.scaled_buf_size;
		else
			alloc_buf_size = data.alloc_data.size;
		MUA_PRINTK(1, "%s. buf_scalar=%d size=%d\n",
					__func__, data.alloc_data.scalar,
					data.alloc_data.scaled_buf_size);

		dmabuf = uvm_alloc_dmabuf(alloc_buf_size,
					  data.alloc_data.align,
					  data.alloc_data.flags);
		if (IS_ERR(dmabuf))
			return -ENOMEM;

		ret = mua_handle_alloc(dmabuf, &data.alloc_data,
				alloc_buf_size);
		if (ret < 0) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}

		fd = dma_buf_fd(dmabuf, O_CLOEXEC);
		if (fd < 0) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}

		data.alloc_data.fd = fd;

		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;

		break;
	case UVM_IOC_SET_PID:
		pid = data.pid_data.pid;
		if (pid < 0)
			return -ENOMEM;

		md->pid = pid;
		break;
	case UVM_IOC_SET_FD:
		fd = data.fd_data.fd;
		ret = mua_set_commit_display(fd, data.fd_data.commit_display);

		if (ret < 0) {
			MUA_PRINTK(0, "invalid dmabuf fd.\n");
			return -EINVAL;
		}
		break;
	default:
		return -ENOTTY;
	}

	return ret;
}

static int mua_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct mua_device *md = container_of(miscdev, struct mua_device, dev);

	MUA_PRINTK(1, "%s: %d\n", __func__, __LINE__);
	file->private_data = md;

	return 0;
}

static int mua_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations mua_fops = {
	.owner = THIS_MODULE,
	.open = mua_open,
	.release = mua_release,
	.unlocked_ioctl = mua_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mua_ioctl,
#endif
};

static int mua_probe(struct platform_device *pdev)
{
	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->dev.minor = MISC_DYNAMIC_MINOR;
	mdev->dev.name = "uvm";
	mdev->dev.fops = &mua_fops;
	mutex_init(&mdev->buffer_lock);

	mdev->client = meson_ion_client_create(-1, "meson-uvm-alloactor");
	if (!mdev->client) {
		pr_err("ion client create error.\n");
		goto err;
	}

	return misc_register(&mdev->dev);

err:
	kfree(mdev);
	return -ENOMEM;
}

static int mua_remove(struct platform_device *pdev)
{
	misc_deregister(&mdev->dev);
	return 0;
}

static const struct of_device_id mua_match[] = {
	{.compatible = "amlogic, meson_uvm",},
	{},
};

static struct platform_driver mua_driver = {
	.driver = {
		.name = "meson_uvm_allocator",
		.owner = THIS_MODULE,
		.of_match_table = mua_match,
	},
	.probe = mua_probe,
	.remove = mua_remove,
};

static int __init mua_init(void)
{
	if (use_uvm) {
		pr_info("meson_uvm_allocator call init\n");
		return platform_driver_register(&mua_driver);
	}

	return 0;
}

static void __exit mua_exit(void)
{
	if (use_uvm)
		platform_driver_unregister(&mua_driver);
}

module_init(mua_init);
module_exit(mua_exit);
MODULE_DESCRIPTION("AMLOGIC uvm dmabuf allocator device driver");
MODULE_LICENSE("GPL");
