/*
 * drivers/amlogic/media/camera/common/vmcls.h
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

#define VM_CLASS_NAME				"vm"

extern void interleave_uv(unsigned char *pU, unsigned char *pV,
			  unsigned char *pUV, unsigned int size_u_or_v);

static ssize_t show_vm_info(struct class *cla, struct class_attribute *attr,
			    char *buf)
{
#if 0
	resource_size_t bstart;
	unsigned int bsize;

	get_vm_buf_info(&bstart, &bsize, NULL);
	return snprintf(buf, 80, "buffer:\n start:%x.\tsize:%d\n",
		(unsigned int)bstart, bsize / (1024 * 1024));
#endif
	return 0;
}

static char attr_dat0[3] = "-1";
static ssize_t read_attr0(struct class *cla, struct class_attribute *attr,
			  char *buf)
{
	return snprintf(buf, 3, "%s", attr_dat0);
}

static ssize_t write_attr0(struct class *cla,
			   struct class_attribute *attr,
			   const char *buf, size_t count)
{
	/* struct display_device *dsp = dev_get_drvdata(dev); */
	ssize_t ret = -EINVAL;

	if (count <= 2) {
		int i = 0;

		if (buf[0] == '-') {
			attr_dat0[0] = '-';
			i = 1;
			ret++;
		}
		if ((buf[i] >= '0') && (buf[i] <= '9')) {
			attr_dat0[i] = buf[i];
			attr_dat0[i + 1] = '\0';
			ret++;
		} else {
			attr_dat0[0] = '-';
			attr_dat0[1] = '1'; /* default -1; */
			ret = -EINVAL;
		}
	}

	return ret;
}

static char attr_dat1[3] = "-1";
static ssize_t read_attr1(struct class *cla, struct class_attribute *attr,
			  char *buf)
{
	return snprintf(buf, 3, "%s", attr_dat1);
}

static ssize_t write_attr1(struct class *cla,
			   struct class_attribute *attr,
			   const char *buf, size_t count)
{
	/* struct display_device *dsp = dev_get_drvdata(dev); */
	ssize_t ret = -EINVAL;

	if (count <= 2) {
		int i = 0;

		if (buf[0] == '-') {
			attr_dat1[0] = '-';
			i = 1;
			ret++;
		}
		if ((buf[i] >= '0') && (buf[i] <= '9')) {
			attr_dat1[i] = buf[i];
			attr_dat1[i + 1] = '\0';
			ret++;
		} else {
			attr_dat1[0] = '-';
			attr_dat1[1] = '1'; /* default -1; */
			ret = -EINVAL;
		}
	}

	return ret;
}
int disable_gt2005;

static ssize_t read_attr2(struct class *cla, struct class_attribute *attr,
			  char *buf)
{
	return disable_gt2005;
}

static ssize_t write_attr2(struct class *cla,
			   struct class_attribute *attr,
			   const char *buf, size_t count)
{
	/* struct display_device *dsp = dev_get_drvdata(dev); */
	ssize_t ret = -EINVAL;

	if (count <= 2)
		disable_gt2005 = buf[0];

	return ret;
}

int camera_mirror_flag;  /* 0: disable, 1: l&r mirror,2 t-b mirror */

static ssize_t mirror_read(struct class *cla, struct class_attribute *attr,
			   char *buf)
{
	if (camera_mirror_flag == 1)
		return snprintf(buf, 80,
		"currnet mirror mode is l-r mirror mode. value is: %d.\n",
		camera_mirror_flag);
	else if (camera_mirror_flag == 2)
		return snprintf(buf, 80,
		"currnet mirror mode is t-b mirror mode. value is: %d.\n",
		camera_mirror_flag);
	else
		return snprintf(buf, 80,
		"currnet mirror mode is normal mode. value is: %d.\n",
		camera_mirror_flag);
}

static ssize_t mirror_write(struct class *cla,
			    struct class_attribute *attr,
			    const char *buf, size_t count){
	unsigned long endp;

	camera_mirror_flag = kstrtoul(buf, 0, &endp);
	return count;
}

static struct class_attribute vm_class_attrs[] = {
				__ATTR(info, 0644,
				show_vm_info, NULL),
				__ATTR(attr0, 0644,
				read_attr0, write_attr0),
				__ATTR(attr1, 0644,
				read_attr1, write_attr1),
				__ATTR(attr2, 0644,
				read_attr2, write_attr2),
				__ATTR(mirror, 0644,
					mirror_read, mirror_write),
				__ATTR_NULL
};

static struct class vm_class = {
		.name = VM_CLASS_NAME,
		.class_attrs = vm_class_attrs,
	};

struct class *init_vm_cls()
	{
		int ret = 0;

		ret = class_register(&vm_class);
		if (ret < 0) {
			pr_err("error create vm class\n");
			return NULL;
		}
		return &vm_class;
	}
