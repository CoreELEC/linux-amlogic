/*
 * drivers/amlogic/media/common/resource_mgr/resourcemanage.c
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "resourcemanage.h"

#define DEVICE_NAME "amresource_mgr"
#define DEVICE_CLASS_NAME "resource_mgr"

struct resman_session {
	struct list_head list;
	bool used_resource[MAX_AVAILAB_RES];
	char app_name[32];
	int app_type;
};

struct session_ptr {
	struct list_head node;
	struct resman_session *sess_ptr;
};

struct video_resource {
	char res_name[32];
	u32 max_rescn;
	u32 res_count;
	struct session_ptr ptr_listhead;
};

static dev_t resman_devno;

static struct cdev *resman_cdev;
static struct class *resman_class;
static DEFINE_MUTEX(resman_mutex);
static struct resman_session resman_listhead;
static struct video_resource resman_src[MAX_AVAILAB_RES] = {
		{.res_name = "vfm_default", .max_rescn = 1},
		{.res_name = "amvideo", .max_rescn = 1},
		{.res_name = "videopip", .max_rescn = 1},
		{.res_name = "sec_tvp", .max_rescn = 0},
		{.res_name = "tsparser", .max_rescn = 1}
		};

static void all_resource_uninit(void)
{
	int i = 0;
	struct session_ptr *pos, *tmp;

	for (i = BASE_AVAILAB_RES; i < MAX_AVAILAB_RES; i++)
		list_for_each_entry_safe(pos, tmp,
		&resman_src[i].ptr_listhead.node, node) {
			list_del(&pos->node);
			kfree(pos);
		}
}

static void all_resource_init(void)
{
	int i = 0;

	INIT_LIST_HEAD(&resman_listhead.list);

	for (i = BASE_AVAILAB_RES; i < MAX_AVAILAB_RES; i++) {
		INIT_LIST_HEAD(&resman_src[i].ptr_listhead.node);
		resman_src[i].res_count = 0;
	}
}

static int resman_release_res(struct resman_session *sess_ptr,
			       int selec_res)
{
	struct session_ptr *pos, *tmp;

	if (!sess_ptr->used_resource[selec_res])
		return 0;

	list_for_each_entry_safe(pos, tmp,
	&resman_src[selec_res].ptr_listhead.node, node) {
		if (pos->sess_ptr == sess_ptr) {
			list_del(&pos->node);
			kfree(pos);
			if (resman_src[selec_res].res_count == 0) {
				pr_err("relese res %s error\n",
					resman_src[selec_res].res_name);
				return -1;
			}

			resman_src[selec_res].res_count--;
			sess_ptr->used_resource[selec_res] = false;

			if (selec_res == SEC_TVP)
				codec_mm_disable_tvp();

			pr_info("resman release res [%s], proc:%s, (tgid:%d), (pid:%d)\n",
				resman_src[selec_res].res_name,
				current->comm,
				current->tgid,
				current->pid);
			break;
		}
	}

	return 0;
}

static void resman_recov_resource(struct resman_session *session_ptr)
{
	int i = 0;

	for (i = BASE_AVAILAB_RES; i < MAX_AVAILAB_RES; i++)
		resman_release_res(session_ptr, i);
}

static long resman_ioctl_acquire(struct file *filp, unsigned long arg)
{
	long r = 0;
	int selec_res = (int)arg;
	struct resman_session *session_ptr;
	struct session_ptr *ptr;

	session_ptr = filp->private_data;
	if (selec_res < BASE_AVAILAB_RES ||
	    selec_res >= MAX_AVAILAB_RES || !session_ptr)
		return -EINVAL;

	if (session_ptr->used_resource[selec_res])
		return r;

	if (resman_src[selec_res].res_count < resman_src[selec_res].max_rescn
		|| resman_src[selec_res].max_rescn == 0) {
		ptr = kzalloc(sizeof(*ptr), GFP_KERNEL);
		if (!ptr)
			return -ENOMEM;

		ptr->sess_ptr = session_ptr;
		list_add_tail(&ptr->node,
			&resman_src[selec_res].ptr_listhead.node);
		resman_src[selec_res].res_count++;
		session_ptr->used_resource[selec_res] = true;
		if (selec_res == SEC_TVP)
			codec_mm_enable_tvp();

		pr_info("resman acquire res [%s], proc:%s, (tgid:%d), (pid:%d)\n",
			resman_src[selec_res].res_name,
			current->comm,
			current->tgid,
			current->pid);
	} else {
		r = -EBUSY;
	}

	return r;
}

static long resman_ioctl_query(struct file *filp, unsigned long arg)
{
	long r = 0;
	ssize_t size = 0;
	int selec_res = 0;
	struct resman_para __user *argp = (void __user *)arg;
	char usage[32];
	struct resman_para resman;

	memset(usage, 0, sizeof(usage));

	if (copy_from_user((void *)&resman, argp, sizeof(resman))) {
		r = -EINVAL;
	} else {
		selec_res = resman.para_in;
		if (selec_res >= BASE_AVAILAB_RES &&
			selec_res < MAX_AVAILAB_RES) {
			size += sprintf(usage + size, "%s:%d:%d",
				resman_src[selec_res].res_name,
				resman_src[selec_res].max_rescn,
				resman_src[selec_res].res_count);

			if (copy_to_user((void *)argp->para_str,
					usage, sizeof(usage)))
				r = -EFAULT;
		} else {
			r = -EINVAL;
		}
	}

	return r;
}

static long resman_ioctl_release(struct file *filp, unsigned long arg)
{
	long r = 0;
	int selec_res = (int)arg;
	struct resman_session *session_ptr;

	session_ptr = filp->private_data;
	if (selec_res >= BASE_AVAILAB_RES &&
	    selec_res < MAX_AVAILAB_RES && session_ptr) {
		if (resman_release_res(session_ptr, selec_res) < 0)
			r = -EINVAL;
	} else {
		r = -EINVAL;
	}

	return r;
}

static long resman_ioctl_setappinfo(struct file *filp, unsigned long arg)
{
	long r = 0;
	struct app_info __user *argp = (void __user *)arg;
	struct resman_session *session_ptr = filp->private_data;
	struct app_info appinfo;

	if (copy_from_user((void *)&appinfo, argp, sizeof(struct app_info)) ||
		!session_ptr) {
		r = -EINVAL;
	} else {
		strncpy(session_ptr->app_name, appinfo.app_name,
			sizeof(session_ptr->app_name));
		session_ptr->app_name[sizeof(session_ptr->app_name)-1] = '\0';
		session_ptr->app_type = appinfo.app_type;
		pr_info("resman set app name:%s, type=%d\n",
			session_ptr->app_name,
			session_ptr->app_type);
	}
	return r;
}

static long resman_ioctl_checksupportres(struct file *filp, unsigned long arg)
{
	long r = 0;
	int i = 0;
	struct resman_para __user *argp = (void __user *)arg;
	struct resman_para resman;

	memset(&resman, 0, sizeof(resman));

	if (copy_from_user((void *)&resman, argp, sizeof(resman))) {
		r = -EINVAL;
	} else {
		if (resman.para_in > sizeof(resman.para_str)-1)
			return -EINVAL;

		for (i = BASE_AVAILAB_RES; i < MAX_AVAILAB_RES; i++)
			if (!strncmp(resman_src[i].res_name, resman.para_str,
					resman.para_in))
				return 0;
		r = -EINVAL;
	}

	return r;
}

static ssize_t res_usage_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	int i = 0;
	ssize_t size = 0;
	struct list_head *curr;
	struct session_ptr *p = NULL;

	for (i = BASE_AVAILAB_RES; i < MAX_AVAILAB_RES; i++) {
		size += sprintf(buf + size, "(%02d) %-12s:total %d, used %d;",
				i,
				resman_src[i].res_name,
				resman_src[i].max_rescn,
				resman_src[i].res_count);

		if (resman_src[i].res_count > 0) {
			size += sprintf(buf + size, " used_by:");
			list_for_each(curr, &resman_src[i].ptr_listhead.node) {
				p = list_entry(curr, struct session_ptr, node);
				if (p->sess_ptr)
					size += sprintf(buf + size, " %s,",
							p->sess_ptr->app_name);
			}
		}
		size += sprintf(buf + size, "\n");
	}

	return size;
}

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
int resman_open(struct inode *inode, struct file *filp)
{
	struct resman_session *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	mutex_lock(&resman_mutex);
	filp->private_data = priv;
	list_add_tail(&priv->list, &resman_listhead.list);
	mutex_unlock(&resman_mutex);
	return 0;
}

ssize_t resman_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	long retval = 1;
	return retval;
}

long resman_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval = 0;

	if (_IOC_TYPE(cmd) != RESMAN_IOC_MAGIC) {
		pr_info("[%s] error cmd!\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&resman_mutex);
	switch (cmd) {
	case RESMAN_IOC_ACQUIRE_RES:
		retval = resman_ioctl_acquire(filp, arg);
		break;
	case RESMAN_IOC_QUERY_RES:
		retval = resman_ioctl_query(filp, arg);
		break;
	case RESMAN_IOC_RELEASE_RES:
		retval = resman_ioctl_release(filp, arg);
		break;
	case RESMAN_IOC_SETAPPINFO:
		retval = resman_ioctl_setappinfo(filp, arg);
		break;
	case RESMAN_IOC_SUPPORT_RES:
		retval = resman_ioctl_checksupportres(filp, arg);
		break;
	default:
		retval = -EINVAL;
		break;
	}
	mutex_unlock(&resman_mutex);
	return retval;
}

#ifdef CONFIG_COMPAT
static long resman_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = resman_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

int resman_close(struct inode *inode, struct file *filp)
{
	struct resman_session *priv;

	mutex_lock(&resman_mutex);
	priv = filp->private_data;
	if (priv) {
		list_del(&priv->list);
		resman_recov_resource(priv);
	}
	mutex_unlock(&resman_mutex);
	kfree(priv);

	return 0;
}

const struct file_operations resman_fops = {
	.owner = THIS_MODULE,
	.open = resman_open,
	.read = resman_read,
	.release = resman_close,
	.unlocked_ioctl = resman_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = resman_compat_ioctl,
#endif
};

/* -----------------------------------------------------------------
 * Initialization and module stuff
 * -----------------------------------------------------------------
 */
static struct class_attribute resman_class_attrs[] = {
	__ATTR_RO(res_usage),
	__ATTR_NULL
};

static void remove_resmansub_attrs(struct class *class)
{
	int i = 0;

	for (i = 0; resman_class_attrs[i].attr.name; i++)
		class_remove_file(class, &resman_class_attrs[i]);
}

static void create_resmansub_attrs(struct class *class)
{
	int i = 0;

	for (i = 0; resman_class_attrs[i].attr.name; i++) {
		if (class_create_file(class, &resman_class_attrs[i]) < 0)
			break;
	}
}

int __init resman_init(void)
{
	int result;
	struct device *resman_dev;

	result = alloc_chrdev_region(&resman_devno, 0, 1, DEVICE_NAME);

	if (result < 0) {
		pr_info("failed to allocate amresource_mgr dev region\n");
		result = -ENODEV;
		return result;
	}

	resman_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);

	if (IS_ERR(resman_class)) {
		result = PTR_ERR(resman_class);
		goto fail1;
	}
	create_resmansub_attrs(resman_class);

	resman_cdev = kmalloc(sizeof(*resman_cdev), GFP_KERNEL);
	if (!resman_cdev) {
		result = -ENOMEM;
		goto fail2;
	}
	cdev_init(resman_cdev, &resman_fops);
	resman_cdev->owner = THIS_MODULE;
	result = cdev_add(resman_cdev, resman_devno, 1);
	if (result) {
		pr_info("resman: failed to add cdev\n");
		goto fail3;
	}
	resman_dev = device_create(resman_class,
				  NULL, MKDEV(MAJOR(resman_devno), 0),
				  NULL, DEVICE_NAME);

	if (IS_ERR(resman_dev)) {
		pr_err("Can't create %s device\n", DEVICE_NAME);
		result = PTR_ERR(resman_dev);
		goto fail4;
	}
	all_resource_init();
	pr_info("%s init success\n", DEVICE_NAME);

	return 0;
fail4:
	cdev_del(resman_cdev);
fail3:
	kfree(resman_cdev);
fail2:
	remove_resmansub_attrs(resman_class);
	class_destroy(resman_class);
fail1:
	unregister_chrdev_region(resman_devno, 1);
	return result;
}

void __exit resman_exit(void)
{
	all_resource_uninit();
	cdev_del(resman_cdev);
	kfree(resman_cdev);
	device_destroy(resman_class, MKDEV(MAJOR(resman_devno), 0));
	remove_resmansub_attrs(resman_class);
	class_destroy(resman_class);
	unregister_chrdev_region(resman_devno, 1);
	pr_info("uninstall sourmanage module\n");
}

module_init(resman_init);
module_exit(resman_exit);
MODULE_LICENSE("GPL");
