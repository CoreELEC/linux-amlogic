/*
 * drivers/amlogic/media/common/codec_mm/secmem.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "secmem.h"

static int debug;
module_param(debug, int, 0644);

#define  DEVICE_NAME "secmem"
#define  CLASS_NAME  "secmem"

#define dprintk(level, fmt, arg...)					      \
	do {								      \
		if (debug >= level)					      \
			pr_info(DEVICE_NAME ": %s: " fmt,  __func__, ## arg); \
	} while (0)

#define pr_dbg(fmt, args ...)  dprintk(6, fmt, ## args)
#define pr_error(fmt, args ...) dprintk(1, fmt, ## args)
#define pr_inf(fmt, args ...) dprintk(8, fmt, ## args)
#define pr_enter() pr_inf("enter")

struct ksecmem_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

static int dev_no;
static struct device *secmem_dev;


static int secmem_dma_attach(struct dma_buf *dbuf, struct device *device,
			     struct dma_buf_attachment *attachment)
{
	struct ksecmem_attachment *attach;
	struct secmem_block *info = dbuf->priv;
	struct sg_table *sgt;
	struct page *page;
	phys_addr_t phys = info->paddr;
	int ret;

	pr_enter();
	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach) {
		pr_error("kzalloc failed\n");
		return -ENOMEM;
	}

	sgt = &attach->sgt;
	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (ret) {
		kfree(attach);
		return -ENOMEM;
	}

	page = phys_to_page(phys);
	sg_set_page(sgt->sgl, page, PAGE_ALIGN(info->size), 0);

	attach->dma_dir = DMA_NONE;
	attachment->priv = attach;

	return 0;
}

static void secmem_dma_detach(struct dma_buf *dbuf,
			      struct dma_buf_attachment *attachment)
{
	struct ksecmem_attachment *attach = attachment->priv;
	struct sg_table *sgt;

	pr_enter();
	if (!attach)
		return;

	sgt = &attach->sgt;

//	if (attach->dma_dir != DMA_NONE)
//		dma_unmap_sg(attachment->dev, sgt->sgl, sgt->orig_nents,
//			     attach->dma_dir);
	sg_free_table(sgt);
	kfree(attach);
	attachment->priv = NULL;
}

static struct sg_table *secmem_dma_map_dma_buf(
		struct dma_buf_attachment *attachment,
		enum dma_data_direction dma_dir)
{
	struct ksecmem_attachment *attach = attachment->priv;
	struct secmem_block *info = attachment->dmabuf->priv;
	struct mutex *lock = &attachment->dmabuf->lock;
	struct sg_table *sgt;

	pr_enter();
	mutex_lock(lock);
	sgt = &attach->sgt;
	if (attach->dma_dir == dma_dir) {
		mutex_unlock(lock);
		return sgt;
	}

	sgt->sgl->dma_address = info->paddr;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
	sgt->sgl->dma_length = PAGE_ALIGN(info->size);
#else
	sgt->sgl->length = PAGE_ALIGN(info->size);
#endif
	pr_dbg("nents %d, %x, %d, %d\n", sgt->nents, info->paddr,
			sg_dma_len(sgt->sgl), info->size);
	attach->dma_dir = dma_dir;

	mutex_unlock(lock);
	return sgt;
}

static void secmem_dma_unmap_dma_buf(
		struct dma_buf_attachment *attachment,
		struct sg_table *sgt,
		enum dma_data_direction dma_dir)
{
	pr_enter();
}

static void secmem_dma_release(struct dma_buf *dbuf)
{
	struct secmem_block *info;

	pr_enter();
	info = (struct secmem_block *)dbuf->priv;
	pr_dbg("handle:%x\n", info->handle);
	kfree(info);
}

static void *secmem_dma_kmap_atomic(struct dma_buf *dbuf, unsigned long n)
{
	pr_enter();
	return NULL;
}

static void *secmem_dma_kmap(struct dma_buf *dbuf, unsigned long n)
{
	pr_enter();
	return NULL;
}

static int secmem_dma_mmap(struct dma_buf *dbuf, struct vm_area_struct *vma)
{
	pr_enter();
	return -EINVAL;
}

static struct dma_buf_ops secmem_dmabuf_ops = {
	.attach = secmem_dma_attach,
	.detach = secmem_dma_detach,
	.map_dma_buf = secmem_dma_map_dma_buf,
	.unmap_dma_buf = secmem_dma_unmap_dma_buf,
	.release = secmem_dma_release,
	.kmap_atomic = secmem_dma_kmap_atomic,
	.kmap = secmem_dma_kmap,
	.mmap = secmem_dma_mmap
};

static struct dma_buf *get_dmabuf(struct secmem_block *info,
				  unsigned long flags)
{
	struct dma_buf *dbuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	pr_dbg("export handle:%x, paddr:%x, size:%d\n",
		info->handle, info->paddr, info->size);
	exp_info.ops = &secmem_dmabuf_ops;
	exp_info.size = info->size;
	exp_info.flags = flags;
	exp_info.priv = (void *)info;
	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf))
		return NULL;
	return dbuf;
}

static long secmem_export(unsigned long args)
{
	int ret;
	struct secmem_block *info;
	struct dma_buf *dbuf;
	int fd;

	pr_enter();
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		pr_error("kmalloc failed\n");
		goto error_alloc_object;
	}
	ret = copy_from_user((void *)info, (void __user *)args,
			     sizeof(struct secmem_block));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}

	dbuf = get_dmabuf(info, 0);
	if (!dbuf) {
		pr_error("get_dmabuf failed\n");
		goto error_export;
	}

	fd = dma_buf_fd(dbuf, O_CLOEXEC);
	if (fd < 0) {
		pr_error("dma_buf_fd failed\n");
		goto error_fd;
	}

	return fd;
error_fd:
	dma_buf_put(dbuf);
error_export:
error_copy:
	kfree(info);
error_alloc_object:
	return -EFAULT;
}

static long secmem_get_handle(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct secmem_block *info;

	pr_enter();
	ret = copy_from_user((void *) &fd, (void __user *) args, sizeof(fd));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_error("dma_buf_get failed\n");
		goto error_get;
	}
	info = dbuf->priv;
	res = (long) (info->handle & (0xffffffff));
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}

static long secmem_get_phyaddr(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct secmem_block *info;

	pr_enter();
	ret = copy_from_user((void *) &fd, (void __user *) args, sizeof(fd));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_error("dma_buf_get failed\n");
		goto error_get;
	}
	info = dbuf->priv;
	res = (long) (info->paddr & (0xffffffff));
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}

static long secmem_import(unsigned long args)
{
	int ret;
	long res = -EFAULT;
	int fd;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t paddr;

	pr_enter();
	ret = copy_from_user((void *)&fd, (void __user *)args,
			     sizeof(fd));
	if (ret) {
		pr_error("copy_from_user failed\n");
		goto error_copy;
	}
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_error("dma_buf_get failed\n");
		goto error_get;
	}
	attach = dma_buf_attach(dbuf, secmem_dev);
	if (IS_ERR(attach)) {
		pr_error("dma_buf_attach failed\n");
		goto error_attach;
	}
	sgt = dma_buf_map_attachment(attach, DMA_FROM_DEVICE);
	if (IS_ERR(sgt)) {
		pr_error("dma_buf_map_attachment failed\n");
		goto error_map;
	}

	paddr = sg_dma_address(sgt->sgl);

	res = (long) (paddr & (0xffffffff));
	dma_buf_unmap_attachment(attach, sgt, DMA_FROM_DEVICE);
error_map:
	dma_buf_detach(dbuf, attach);
error_attach:
	dma_buf_put(dbuf);
error_get:
error_copy:
	return res;
}
static int secmem_open(struct inode *inodep, struct file *filep)
{
	pr_enter();
	return 0;
}

static ssize_t secmem_read(struct file *filep, char *buffer,
			   size_t len, loff_t *offset)
{
	pr_enter();
	return 0;
}

static ssize_t secmem_write(struct file *filep, const char *buffer,
			    size_t len, loff_t *offset)
{
	pr_enter();
	return 0;
}

static int secmem_release(struct inode *inodep, struct file *filep)
{
	pr_enter();
	return 0;
}

static long secmem_ioctl(struct file *filep, unsigned int cmd,
			 unsigned long args)
{
	long ret = -EINVAL;

	pr_inf("cmd %x\n", cmd);
	switch (cmd) {
	case SECMEM_EXPORT_DMA:
		ret = secmem_export(args);
		break;
	case SECMEM_GET_HANDLE:
		ret = secmem_get_handle(args);
		break;
	case SECMEM_GET_PHYADDR:
		ret = secmem_get_phyaddr(args);
		break;
	case SECMEM_IMPORT_DMA:
		ret = secmem_import(args);
		break;
	default:
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long secmem_compat_ioctl(struct file *filep, unsigned int cmd,
				unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = secmem_ioctl(filep, cmd, args);
	return ret;
}
#endif

const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = secmem_open,
	.read = secmem_read,
	.write = secmem_write,
	.release = secmem_release,
	.unlocked_ioctl = secmem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = secmem_compat_ioctl,
#endif
};

static struct class_attribute secmem_class_attrs[] = {
	__ATTR_NULL
};

static struct class secmem_class = {
	.name = CLASS_NAME,
	.class_attrs = secmem_class_attrs,
};

int __init secmem_init(void)
{
	int ret;

	ret = register_chrdev(0, DEVICE_NAME, &fops);
	if (ret < 0) {
		pr_error("register_chrdev failed\n");
		goto error_register;
	}
	dev_no = ret;
	ret = class_register(&secmem_class);
	if (ret < 0) {
		pr_error("class_register failed\n");
		goto error_class;
	}

	secmem_dev = device_create(&secmem_class,
				   NULL, MKDEV(dev_no, 0),
				   NULL, DEVICE_NAME);
	if (IS_ERR(secmem_dev)) {
		pr_error("device_create failed\n");
		ret = PTR_ERR(secmem_dev);
		goto error_create;
	}
	pr_dbg("init done\n");
	return 0;
error_create:
	class_unregister(&secmem_class);
error_class:
	unregister_chrdev(dev_no, DEVICE_NAME);
error_register:
	return ret;
}

void __exit secmem_exit(void)
{
	device_destroy(&secmem_class, MKDEV(dev_no, 0));
	class_unregister(&secmem_class);
	class_destroy(&secmem_class);
	unregister_chrdev(dev_no, DEVICE_NAME);
	pr_dbg("exit done\n");
}

module_init(secmem_init);
module_exit(secmem_exit);
MODULE_LICENSE("GPL");
