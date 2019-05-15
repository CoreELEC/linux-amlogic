/*
 * drivers/amlogic/mtd/aml_env.c
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

#include "aml_mtd.h"

#ifndef AML_NAND_UBOOT
#include<linux/cdev.h>
#include <linux/device.h>

#ifndef CONFIG_MTD_ENV_IN_NAND
#define ENV_NAME "nand_env"
static DEFINE_MUTEX(env_mutex);
static dev_t uboot_env_no;
struct cdev uboot_env;
struct device *uboot_dev;
struct class *uboot_env_class;
#endif  /* AML_NAND_UBOOT */
#endif

struct aml_nand_chip *aml_chip_env;

int amlnand_save_info_by_name_mtd(struct aml_nand_chip *aml_chip,
	unsigned char *buf, int size)
{
	struct mtd_info *mtd = aml_chip->mtd;
	int ret = 0;

	aml_nand_save_env(mtd, buf);
	return ret;
}

int amlnand_read_info_by_name_mtd(struct aml_nand_chip *aml_chip,
	unsigned char *buf, int size)
{
	struct mtd_info *mtd = aml_chip->mtd;
	size_t offset = 0;
	int ret = 0;

	aml_nand_read_env(mtd, offset, buf);
	return ret;
}


int amlnf_env_save(u8 *buf, int len)
{
	u8 *env_buf = NULL;
	int ret = 0;

	pr_info("uboot env amlnf_env_save : ####\n");

	if (aml_chip_env == NULL) {
		pr_info("uboot env not init yet!,%s\n", __func__);
		return -EFAULT;
	}

	if (len > CONFIG_ENV_SIZE) {
		pr_info("uboot env data len too much,%s\n", __func__);
		return -EFAULT;
	}

	env_buf = kzalloc(CONFIG_ENV_SIZE, GFP_KERNEL);
	if (env_buf == NULL) {
		/*pr_info("nand malloc for uboot env failed\n");*/
		ret = -1;
		goto exit_err;
	}
	memset(env_buf, 0, CONFIG_ENV_SIZE);
	memcpy(env_buf, buf, len);

	ret = amlnand_save_info_by_name_mtd(aml_chip_env,
		env_buf,
		CONFIG_ENV_SIZE);
	if (ret) {
		pr_info("nand uboot env error,%s\n", __func__);
		ret = -EFAULT;
		goto exit_err;
	}
exit_err:
	kfree(env_buf);
	env_buf = NULL;
	return ret;
}

int amlnf_env_read(u8 *buf, int len)
{
	u8 *env_buf = NULL;
	int ret = 0;

	pr_info("uboot env amlnf_env_read : ####\n");

	if (len > CONFIG_ENV_SIZE) {
		pr_info("uboot env data len too much, %s\n",
			__func__);
		return -EFAULT;
	}
	if (aml_chip_env == NULL) {
		memset(buf, 0x0, len);
		pr_info("uboot env arg_valid = 0 invalid, %s\n",
			__func__);
		return 0;
	}

	env_buf = kzalloc(CONFIG_ENV_SIZE, GFP_KERNEL);
	if (env_buf == NULL) {
		ret = -1;
		goto exit_err;
	}
	memset(env_buf, 0, CONFIG_ENV_SIZE);

	ret = amlnand_read_info_by_name_mtd(aml_chip_env,
		(u8 *)env_buf,
		CONFIG_ENV_SIZE);
	if (ret) {
		pr_info("nand uboot env error,%s\n",
			__func__);
		ret = -EFAULT;
		goto exit_err;
	}

	memcpy(buf, env_buf, len);
exit_err:
	kfree(env_buf);
	env_buf = NULL;

	return ret;
}
#ifndef CONFIG_MTD_ENV_IN_NAND
#ifndef AML_NAND_UBOOT

int uboot_env_open(struct inode *node, struct file *file)
{
	return 0;
}

static loff_t uboot_env_llseek(struct file *file, loff_t off, int whence)
{
	loff_t newpos;

	mutex_lock(&env_mutex);
	switch (whence) {
	case 0: /* SEEK_SET (start postion)*/
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = file->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = (loff_t)(CONFIG_ENV_SIZE) - 1;
		newpos = newpos - off;
		break;

	default: /* can't happen */
		mutex_unlock(&env_mutex);
		return -EINVAL;
	}

	if ((newpos < 0) || (newpos >= (loff_t)CONFIG_ENV_SIZE)) {
		mutex_unlock(&env_mutex);
		return -EINVAL;
	}

	file->f_pos = newpos;
	mutex_unlock(&env_mutex);

	return newpos;
}

ssize_t uboot_env_read(struct file *file,
		char __user *buf,
		size_t count,
		loff_t *ppos)
{
	u8 *env_ptr = NULL;
	ssize_t read_size = 0;
	int ret = 0;
	struct nand_chip *chip = &aml_chip_env->chip;
	struct mtd_info *mtd = aml_chip_env->mtd;
	loff_t addr;
	int chipnr;

	if (*ppos == CONFIG_ENV_SIZE)
		return 0;

	if (*ppos >= CONFIG_ENV_SIZE) {
		pr_info("nand env: data access out of space!\n");
		return -EFAULT;
	}

	env_ptr = vmalloc(CONFIG_ENV_SIZE + 2048);
	if (env_ptr == NULL)
		return -ENOMEM;

	addr = *ppos;
	nand_get_device(mtd, FL_READING);
	chipnr = (int)(addr >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	/*amlnand_get_device(aml_chip_env, CHIP_READING);*/
	ret = amlnf_env_read((u8 *)env_ptr, CONFIG_ENV_SIZE);
	if (ret) {
		pr_info("nand_env_read: nand env read failed:%d\n",
			ret);
		ret = -EFAULT;
		goto exit;
	}

	if ((*ppos + count) > CONFIG_ENV_SIZE)
		read_size = CONFIG_ENV_SIZE - *ppos;
	else
		read_size = count;

	ret = copy_to_user(buf, (env_ptr + *ppos), read_size);
	*ppos += read_size;
exit:
	chip->select_chip(mtd, -1);
	nand_release_device(mtd);
	/*amlnand_release_device(aml_chip_env);*/
	vfree(env_ptr);
	return read_size;
}

ssize_t uboot_env_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *ppos)
{
	u8 *env_ptr = NULL;
	ssize_t write_size = 0;
	int ret = 0;
	struct nand_chip *chip = &aml_chip_env->chip;
	struct mtd_info *mtd = aml_chip_env->mtd;
	loff_t addr;
	int chipnr;

	if (*ppos == CONFIG_ENV_SIZE)
		return 0;

	if (*ppos >= CONFIG_ENV_SIZE) {
		pr_info("nand env: data access out of space!\n");
		return -EFAULT;
	}

	env_ptr = vmalloc(CONFIG_ENV_SIZE + 2048);
	if (env_ptr == NULL)
		return -ENOMEM;

	addr = *ppos;
	nand_get_device(mtd, FL_WRITING);
	chipnr = (int)(addr >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	if ((*ppos + count) > CONFIG_ENV_SIZE)
		write_size = CONFIG_ENV_SIZE - *ppos;
	else
		write_size = count;

	ret = copy_from_user((env_ptr + *ppos), buf, write_size);

	ret = amlnf_env_save(env_ptr, CONFIG_ENV_SIZE);
	if (ret) {
		pr_info("nand_env_read: nand env read failed\n");
		ret = -EFAULT;
		goto exit;
	}

	*ppos += write_size;
exit:
	chip->select_chip(mtd, -1);
	nand_release_device(mtd);
	vfree(env_ptr);
	return write_size;
}

long uboot_env_ioctl(struct file *file, u32 cmd, unsigned long args)
{
	return 0;
}

static const struct file_operations uboot_env_ops = {
	.open = uboot_env_open,
	.read = uboot_env_read,
	.write = uboot_env_write,
	.llseek	= uboot_env_llseek,
	.unlocked_ioctl = uboot_env_ioctl,
};
#endif /* AML_NAND_UBOOT */

int aml_ubootenv_init(struct aml_nand_chip *aml_chip)
{
	int ret = 0;
	u8 *env_buf = NULL;

	aml_chip_env = aml_chip;

	env_buf = kzalloc(CONFIG_ENV_SIZE, GFP_KERNEL);
	if (env_buf == NULL) {
		ret = -1;
		goto exit_err;
	}
	memset(env_buf, 0x0, CONFIG_ENV_SIZE);

#ifndef AML_NAND_UBOOT
	pr_info("%s: register env chardev\n", __func__);
	ret = alloc_chrdev_region(&uboot_env_no, 0, 1, ENV_NAME);
	if (ret < 0) {
		pr_info("alloc uboot env dev_t no failed\n");
		ret = -1;
		goto exit_err;
	}

	cdev_init(&uboot_env, &uboot_env_ops);
	uboot_env.owner = THIS_MODULE;
	ret = cdev_add(&uboot_env, uboot_env_no, 1);
	if (ret) {
		pr_info("uboot env dev add failed\n");
		ret = -1;
		goto exit_err1;
	}

	uboot_env_class = class_create(THIS_MODULE, ENV_NAME);
	if (IS_ERR(uboot_env_class)) {
		pr_info("uboot env dev add failed\n");
		ret = -1;
		goto exit_err2;
	}

	uboot_dev = device_create(uboot_env_class,
		NULL,
		uboot_env_no,
		NULL,
		ENV_NAME);
	if (IS_ERR(uboot_dev)) {
		pr_info("uboot env dev add failed\n");
		ret = -1;
		goto exit_err3;
	}

	pr_info("%s: register env chardev OK\n", __func__);

	kfree(env_buf);
	env_buf = NULL;

	return ret;
exit_err3:
	class_destroy(uboot_env_class);
exit_err2:
	cdev_del(&uboot_env);
exit_err1:
	unregister_chrdev_region(uboot_env_no, 1);

#endif /* AML_NAND_UBOOT */
exit_err:
	kfree(env_buf);
	env_buf = NULL;

	return ret;
}

/* update env if only it is still readable! */
int aml_nand_update_ubootenv(struct aml_nand_chip *aml_chip, char *env_ptr)
{
	int ret = 0;
	char *env_buf = NULL;

	env_buf = kzalloc(CONFIG_ENV_SIZE, GFP_KERNEL);
	if (env_buf == NULL)
		return -ENOMEM;
	memset(env_buf, 0, CONFIG_ENV_SIZE);
	ret = amlnand_read_info_by_name_mtd(aml_chip,
		(u8 *)env_buf,
		CONFIG_ENV_SIZE);
	if (ret) {
		pr_info("read ubootenv error,%s\n", __func__);
		ret = -EFAULT;
		goto exit;
	}

	ret = amlnand_save_info_by_name_mtd(aml_chip,
		(u8 *)env_buf,
		CONFIG_ENV_SIZE);
	if (ret < 0)
		pr_info("aml_nand_update_secure : update secure failed\n");
exit:
	if (env_buf) {
		kfree(env_buf);
		env_buf = NULL;
	}
	return 0;
}
#endif

