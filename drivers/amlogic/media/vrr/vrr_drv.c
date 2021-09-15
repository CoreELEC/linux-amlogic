// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vrr/vrr.h>
#include "vrr_drv.h"
#include "vrr_reg.h"

#include <linux/amlogic/gki_module.h>

#define VRR_CDEV_NAME  "aml_vrr"
struct vrr_cdev_s {
	dev_t           devno;
	struct class    *class;
};

static struct vrr_cdev_s *vrr_cdev;
/* for driver global resource init:
 *  0: none
 *  n: initialized cnt
 */
static unsigned char vrr_global_init_flag;
static unsigned int vrr_drv_init_state;
static struct aml_vrr_drv_s *vrr_drv[VRR_MAX_DRV];

static unsigned int vrr_debug_print;

static int vrr_config_load(struct aml_vrr_drv_s *vdrv,
			   struct platform_device *pdev)
{
	return 0;
}

static void vrr_lcd_enable(struct aml_vrr_drv_s *vdrv, unsigned int mode)
{
	unsigned int vsp_in, vsp_sel;
	unsigned int v_max, v_min;
	unsigned int offset = 0;

	if (!vdrv->vrr_dev) {
		VRRERR("[%d]: %s: vrr_dev is null\n", vdrv->index, __func__);
		return;
	}
	if (vdrv->state & VRR_STATE_EN)
		return;

	offset = vdrv->data->offset[vdrv->index];
	v_max = vdrv->vrr_dev->vmax;
	v_min = vdrv->vrr_dev->vmin;

	if (mode) {
		vsp_in = 2;  //gpu source
		vsp_sel = 0; //gpu input
		vdrv->state |= VRR_STATE_MODE_SW;
	} else {
		vsp_in = 1;  //hmdi source
		vsp_sel = 1;  //hdmi input
		vdrv->state |= VRR_STATE_MODE_HW;
	}

	//vrr setting
	vrr_reg_write((VENC_VRR_CTRL + offset),
			(10 << 8) | //cfg_vsp_dly_num (pixel clock) number
			(0 << 4) |  //cfg_vrr_frm_ths frame delay number
			//cfg_vrr_vsp_en  bit[2]=hdmi_in, bit[3]=gpu_in
			(vsp_in << 2) |
			(1 << 1) |      //cfg_vrr_mode    0:normal      1:vrr
			(vsp_sel << 0)); //cfg_vrr_vsp_sel 1:hdmi in  0:gpu in
	vrr_reg_write((VENC_VRR_ADJ_LMT + offset),
			(v_min << 16) | //cfg_vrr_min_vnum <= am_spdat[31:16]
			(v_max << 0));  //cfg_vrr_max_vnum <= am_spdat[15:0]

	vdrv->state |= (VRR_STATE_ENCL | VRR_STATE_EN);

	if (vrr_debug_print & VRR_DBG_PR_NORMAL) {
		VRRPR("VENC_VRR_CTRL = 0x%x\n",
		      vrr_reg_read(VENC_VRR_CTRL + offset));
		VRRPR("VENC_VRR_ADJ_LMT = 0x%x\n",
		      vrr_reg_read(VENC_VRR_ADJ_LMT + offset));
	}
	VRRPR("[%d]: %s: state = 0x%x\n", vdrv->index, __func__, vdrv->state);
}

static void vrr_hdmi_enable(struct aml_vrr_drv_s *vdrv, unsigned int mode)
{
	unsigned int vsp_in, vsp_sel;
	unsigned int v_max, v_min;
	unsigned int offset = 0;

	if (!vdrv->vrr_dev) {
		VRRERR("[%d]: %s: vrr_dev is null\n", vdrv->index, __func__);
		return;
	}
	if (vdrv->state & VRR_STATE_EN)
		return;

	offset = vdrv->data->offset[vdrv->index];
	v_max = vdrv->vrr_dev->vmax;
	v_min = vdrv->vrr_dev->vmin;

	if (mode) {
		vsp_in = 2;  //gpu source
		vsp_sel = 0; //gpu input
		vdrv->state |= VRR_STATE_MODE_SW;
	} else {
		vsp_in = 1;  //hmdi source
		vsp_sel = 1;  //hdmi input
		vdrv->state |= VRR_STATE_MODE_HW;
	}

	//vrr setting
	vrr_reg_write(VENP_VRR_CTRL + offset,
			(10 << 8) | //cfg_vsp_dly_num (pixel clock) number
			(0 << 4) |  //cfg_vrr_frm_ths frame delay number
			//cfg_vrr_vsp_en  bit[2]=hdmi_in, bit[3]=gpu_in
			(vsp_in << 2) |
			(1 << 1) |    //cfg_vrr_mode    0:normal      1:vrr
			(vsp_sel << 0)); //cfg_vrr_vsp_sel 1:hdmi in  0:gpu in
	vrr_reg_write(VENP_VRR_ADJ_LMT + offset,
			(v_min << 16) |  //cfg_vrr_min_vnum
			(v_max << 0)); //cfg_vrr_max_vnum

	vdrv->state |= (VRR_STATE_ENCP | VRR_STATE_EN);

	if (vrr_debug_print & VRR_DBG_PR_NORMAL) {
		VRRPR("VENP_VRR_CTRL = 0x%x\n",
		      vrr_reg_read(VENP_VRR_CTRL + offset));
		VRRPR("VENP_VRR_ADJ_LMT = 0x%x\n",
		      vrr_reg_read(VENP_VRR_ADJ_LMT + offset));
	}
	VRRPR("[%d]: %s: state = 0x%x\n", vdrv->index, __func__, vdrv->state);
}

static void vrr_work_disable(struct aml_vrr_drv_s *vdrv)
{
	unsigned int offset = 0;

	offset = vdrv->data->offset[vdrv->index];

	if (vdrv->state & VRR_STATE_ENCL)
		vrr_reg_setb((VENC_VRR_CTRL + offset), 0, 1, 1);
	if (vdrv->state & VRR_STATE_ENCP)
		vrr_reg_setb((VENP_VRR_CTRL + offset), 0, 1, 1);

	vdrv->state = 0;
}

static void vrr_set_venc_vspin(struct aml_vrr_drv_s *vdrv)
{
	vrr_reg_setb(0x1202, 1, 28, 1);//vdin vsync input pulse
}

static int vrr_timert_start(struct aml_vrr_drv_s *vdrv)
{
	vdrv->sw_timer.expires = jiffies + msecs_to_jiffies(vdrv->sw_timer_cnt);
	add_timer(&vdrv->sw_timer);
	return 0;
}

static int vrr_timer_stop(struct aml_vrr_drv_s *vdrv)
{
	del_timer_sync(&vdrv->sw_timer);
	return 0;
}

static void vrr_timer_handler(struct timer_list *timer)
{
	struct aml_vrr_drv_s *vdrv = from_timer(vdrv, timer, sw_timer);

	vrr_set_venc_vspin(vdrv);
	if (vrr_debug_print & VRR_DBG_PR_ISR)
		VRRPR("%s\n", __func__);

	vrr_timert_start(vdrv);
}

static int vrr_func_en(struct aml_vrr_drv_s *vdrv, int flag)
{
	VRRPR("[%d]: %s, flag=%d\n", vdrv->index, __func__, flag);
	if (!vdrv->vrr_dev) {
		VRRERR("[%d]: %s: invalid vrr_dev\n",
		       vdrv->index, __func__);
		return -1;
	}

	if (flag) {
		if (vdrv->vrr_dev->enable == 0) {
			VRRERR("[%d]: %s: vrr_dev %s(%d) is forbidden\n",
			       vdrv->index, __func__,
			       vdrv->vrr_dev->name,
			       vdrv->vrr_dev->output_src);
			return -1;
		}
		if (vdrv->enable)
			return 0;

		vdrv->enable = 1;
		switch (vdrv->vrr_dev->output_src) {
		case VRR_OUTPUT_ENCP:
			vrr_hdmi_enable(vdrv, 0);
			break;
		case VRR_OUTPUT_ENCL:
			vrr_lcd_enable(vdrv, 0);
			break;
		default:
			break;
		}
	} else {
		if (vdrv->enable == 0)
			return 0;

		vdrv->enable = 0;
		if (vdrv->sw_timer_flag) {
			vrr_timer_stop(vdrv);
			vdrv->sw_timer_flag = 0;
			VRRPR("[%d]: %s: stop sw_timer\n",
			      vdrv->index, __func__);
		}
		vrr_work_disable(vdrv);
	}

	return 0;
}

static int vrr_test_en(struct aml_vrr_drv_s *vdrv, int flag)
{
	VRRPR("[%d]: %s, flag=%d\n", vdrv->index, __func__, flag);
	switch (flag) {
	case 1: /* lcd hw */
		if (vdrv->state & VRR_STATE_EN)
			vrr_work_disable(vdrv);
		if (vdrv->sw_timer_flag) {
			vrr_timer_stop(vdrv);
			vdrv->sw_timer_flag = 0;
			VRRPR("[%d]: %s: stop sw_timer\n",
			      vdrv->index, __func__);
		}
		vrr_lcd_enable(vdrv, 0);
		break;
	case 2: /* lcd sw */
		if (vdrv->state & VRR_STATE_EN)
			vrr_work_disable(vdrv);
		vrr_lcd_enable(vdrv, 0);
		if (vdrv->sw_timer_flag == 0) {
			VRRPR("[%d]: %s: start sw_timer\n",
			      vdrv->index, __func__);
			vrr_timert_start(vdrv);
			vdrv->sw_timer_flag = 1;
		}
		break;
	case 3: /* hdmi hw */
		if (vdrv->state & VRR_STATE_EN)
			vrr_work_disable(vdrv);
		if (vdrv->sw_timer_flag) {
			vrr_timer_stop(vdrv);
			vdrv->sw_timer_flag = 0;
			VRRPR("[%d]: %s: stop sw_timer\n",
			      vdrv->index, __func__);
		}
		vrr_hdmi_enable(vdrv, 0);
		break;
	case 4: /* hdmi sw */
		if (vdrv->state & VRR_STATE_EN)
			vrr_work_disable(vdrv);
		vrr_hdmi_enable(vdrv, 0);
		if (vdrv->sw_timer_flag == 0) {
			VRRPR("[%d]: %s: start sw_timer\n",
			      vdrv->index, __func__);
			vrr_timert_start(vdrv);
			vdrv->sw_timer_flag = 1;
		}
		break;
	case 0: /* disable */
	default:
		if (vdrv->sw_timer_flag) {
			vrr_timer_stop(vdrv);
			vdrv->sw_timer_flag = 0;
			VRRPR("[%d]: %s: stop sw_timer\n",
			      vdrv->index, __func__);
		}
		vrr_work_disable(vdrv);
		break;
	}

	return 0;
}

static int vrr_func_init(struct aml_vrr_drv_s *vdrv)
{
	vdrv->sw_timer_flag = 0;
	vdrv->sw_timer_cnt = 20;
	timer_setup(&vdrv->sw_timer, &vrr_timer_handler, 0);
	vdrv->sw_timer.expires = jiffies + msecs_to_jiffies(vdrv->sw_timer_cnt);

	if (vrr_debug_print & VRR_DBG_PR_NORMAL)
		VRRPR("[%d]: %s\n", vdrv->index, __func__);
	return 0;
}

//===========================================================================
// For VRR Debug
//===========================================================================
static const char *vrr_debug_usage_str = {
"Usage:\n"
"    cat status ; dump vrr status\n"
};

static ssize_t vrr_debug_help(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", vrr_debug_usage_str);
}

static ssize_t vrr_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct aml_vrr_drv_s *vdrv = dev_get_drvdata(dev);
	unsigned int offset;
	ssize_t len = 0;

	offset = vdrv->data->offset[vdrv->index];

	len += sprintf(buf + len, "vrr[%d] info\n", vdrv->index);
	if (vdrv->vrr_dev) {
		len += sprintf(buf + len, "dev->name:       %s\n",
				vdrv->vrr_dev->name);
		len += sprintf(buf + len, "dev->output_src: %d\n",
				vdrv->vrr_dev->output_src);
		len += sprintf(buf + len, "dev->enable:     %d\n",
				vdrv->vrr_dev->enable);
		len += sprintf(buf + len, "dev->vmax:       %d\n",
				vdrv->vrr_dev->vmax);
		len += sprintf(buf + len, "dev->vmin:       %d\n",
				vdrv->vrr_dev->vmin);
	}

	/** vrr reg info **/
	len += sprintf(buf + len, "VENC_VRR_CTRL: 0x%x\n",
			vrr_reg_read(VENC_VRR_CTRL + offset));
	len += sprintf(buf + len, "VENC_VRR_ADJ_LMT: 0x%x\n",
			vrr_reg_read(VENC_VRR_ADJ_LMT + offset));
	len += sprintf(buf + len, "VENC_VRR_CTRL1: 0x%x\n",
			vrr_reg_read(VENC_VRR_CTRL1 + offset));
	len += sprintf(buf + len, "VENP_VRR_CTRL: 0x%x\n",
			vrr_reg_read(VENP_VRR_CTRL + offset));
	len += sprintf(buf + len, "VENP_VRR_ADJ_LMT: 0x%x\n",
			vrr_reg_read(VENP_VRR_ADJ_LMT + offset));
	len += sprintf(buf + len, "VENP_VRR_CTRL1: 0x%x\n",
			vrr_reg_read(VENP_VRR_CTRL1 + offset));

	/** vrr timer **/
	len += sprintf(buf + len, "sw_timer info\n");
	len += sprintf(buf + len, "sw_timer_cnt: %d\n", vdrv->sw_timer_cnt);
	len += sprintf(buf + len, "sw_timer_cnt state: %d\n",
		       vdrv->sw_timer_flag);

	return len;
}

static ssize_t vrr_debug_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct aml_vrr_drv_s *vdrv = dev_get_drvdata(dev);
	unsigned int temp;
	int ret = 0;

	switch (buf[0]) {
	case 'e': /* en */
		ret = sscanf(buf, "en %d", &temp);
		if (ret == 1) {
			vrr_func_en(vdrv, temp);
		} else {
			VRRERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 't': /* test */
		ret = sscanf(buf, "test %d", &temp);
		if (ret == 1) {
			vrr_test_en(vdrv, temp);
		} else {
			VRRERR("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'p':
		ret = sscanf(buf, "print %x", &temp);
		if (ret == 1)
			vrr_debug_print = temp;
		VRRPR("vrr_debug_print: 0x%x\n", temp);
		break;
	case 'c':
		ret = sscanf(buf, "cnt %d", &temp);
		if (ret == 1)
			vdrv->sw_timer_cnt = temp;
		VRRPR("sw_timer_cnt: %d\n", temp);
		break;
	default:
		VRRERR("wrong command\n");
		break;
	}
	return count;
}

static struct device_attribute vrr_debug_attrs[] = {
	__ATTR(help, 0444, vrr_debug_help, NULL),
	__ATTR(status, 0444, vrr_status_show, NULL),
	__ATTR(debug, 0644, vrr_debug_help, vrr_debug_store),
};

static int vrr_debug_file_creat(struct aml_vrr_drv_s *vdrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vrr_debug_attrs); i++) {
		if (device_create_file(vdrv->dev, &vrr_debug_attrs[i])) {
			VRRERR("[%d]: create lcd debug attribute %s fail\n",
			       vdrv->index, vrr_debug_attrs[i].attr.name);
		}
	}

	return 0;
}

static int vrr_debug_file_remove(struct aml_vrr_drv_s *vdrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vrr_debug_attrs); i++)
		device_remove_file(vdrv->dev, &vrr_debug_attrs[i]);

	return 0;
}

/* ************************************************************* */
int aml_vrr_register_device(struct vrr_device_s *vrr_dev, int index)
{
	if (vrr_global_init_flag == 0)
		return 0;

	if (!vrr_dev) {
		VRRERR("%s: vrr_dev is null\n", __func__);
		return -1;
	}
	if (index >= VRR_MAX_DRV) {
		VRRERR("%s: invalid index: %d, (%s %d)\n",
		       __func__, index, vrr_dev->name, vrr_dev->output_src);
		return -1;
	}
	if (!vrr_drv[index]) {
		VRRERR("[%d]: %s: vrr_drv is null\n", index, __func__);
		return -1;
	}
	if (vrr_drv[index]->vrr_dev) {
		VRRERR("[%d]: %s: dev(%s %d) is busy, register failed: %s %d\n",
		       index, __func__,
		       vrr_drv[index]->vrr_dev->name,
		       vrr_drv[index]->vrr_dev->output_src,
		       vrr_dev->name, vrr_dev->output_src);
		return -1;
	}

	vrr_drv[index]->vrr_dev = vrr_dev;
	VRRPR("[%d]: %s: %s %d successfully\n",
	      index, __func__, vrr_dev->name, vrr_dev->output_src);

	return 0;
}

int aml_vrr_unregister_device(int index)
{
	if (vrr_global_init_flag == 0)
		return 0;

	if (index >= VRR_MAX_DRV) {
		VRRERR("%s: invalid index: %d\n", __func__, index);
		return -1;
	}
	if (!vrr_drv[index]) {
		VRRERR("[%d]: %s: vrr_drv is null\n", index, __func__);
		return -1;
	}

	if (!vrr_drv[index]->vrr_dev)
		return 0;

	if (vrr_drv[index]->enable) {
		vrr_drv[index]->enable = 0;
		vrr_work_disable(vrr_drv[index]);
	}
	vrr_drv[index]->vrr_dev = NULL;
	VRRPR("[%d]: %s: %s %d unregister successfully\n",
		index, __func__,
		vrr_drv[index]->vrr_dev->name,
		vrr_drv[index]->vrr_dev->output_src);

	return 0;
}

/* ************************************************************* */
static int vrr_io_open(struct inode *inode, struct file *file)
{
	struct aml_vrr_drv_s *vdrv;

	vdrv = container_of(inode->i_cdev, struct aml_vrr_drv_s, cdev);
	file->private_data = vdrv;

	return 0;
}

static int vrr_io_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long vrr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long vrr_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vrr_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations vrr_fops = {
	.owner          = THIS_MODULE,
	.open           = vrr_io_open,
	.release        = vrr_io_release,
	.unlocked_ioctl = vrr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = vrr_compat_ioctl,
#endif
};

static int vrr_cdev_add(struct aml_vrr_drv_s *vdrv, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!vrr_cdev || !vdrv) {
		ret = 1;
		VRRERR("%s: vdrv is null\n", __func__);
		return -1;
	}

	devno = MKDEV(MAJOR(vrr_cdev->devno), vdrv->index);

	cdev_init(&vdrv->cdev, &vrr_fops);
	vdrv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&vdrv->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto vrr_cdev_add_failed;
	}

	vdrv->dev = device_create(vrr_cdev->class, parent,
				  devno, NULL, "vrr%d", vdrv->index);
	if (IS_ERR_OR_NULL(vdrv->dev)) {
		ret = 3;
		goto vrr_cdev_add_failed1;
	}

	dev_set_drvdata(vdrv->dev, vdrv);
	vdrv->dev->of_node = parent->of_node;

	VRRPR("[%d]: %s OK\n", vdrv->index, __func__);
	return 0;

vrr_cdev_add_failed1:
	cdev_del(&vdrv->cdev);
vrr_cdev_add_failed:
	VRRERR("[%d]: %s: failed: %d\n", vdrv->index, __func__, ret);
	return -1;
}

static void vrr_cdev_remove(struct aml_vrr_drv_s *vdrv)
{
	dev_t devno;

	if (!vrr_cdev || !vdrv)
		return;

	devno = MKDEV(MAJOR(vrr_cdev->devno), vdrv->index);
	device_destroy(vrr_cdev->class, devno);
	cdev_del(&vdrv->cdev);
}

static int vrr_global_init_once(void)
{
	int ret;

	if (vrr_global_init_flag) {
		vrr_global_init_flag++;
		return 0;
	}
	vrr_global_init_flag++;

	vrr_cdev = kzalloc(sizeof(*vrr_cdev), GFP_KERNEL);
	if (!vrr_cdev)
		return -1;

	ret = alloc_chrdev_region(&vrr_cdev->devno, 0,
				  VRR_MAX_DRV, VRR_CDEV_NAME);
	if (ret) {
		ret = 1;
		goto vrr_global_init_once_err;
	}

	vrr_cdev->class = class_create(THIS_MODULE, "aml_vrr");
	if (IS_ERR_OR_NULL(vrr_cdev->class)) {
		ret = 2;
		goto vrr_global_init_once_err_1;
	}

	return 0;

vrr_global_init_once_err_1:
	unregister_chrdev_region(vrr_cdev->devno, VRR_MAX_DRV);
vrr_global_init_once_err:
	kfree(vrr_cdev);
	vrr_cdev = NULL;
	VRRERR("%s: failed: %d\n", __func__, ret);
	return -1;
}

static void vrr_global_remove_once(void)
{
	if (vrr_global_init_flag > 1) {
		vrr_global_init_flag--;
		return;
	}
	vrr_global_init_flag--;

	if (!vrr_cdev)
		return;

	class_destroy(vrr_cdev->class);
	unregister_chrdev_region(vrr_cdev->devno, VRR_MAX_DRV);
	kfree(vrr_cdev);
	vrr_cdev = NULL;
}

#ifdef CONFIG_OF

static struct vrr_data_s vrr_data_t7 = {
	.chip_type = VRR_CHIP_T7,
	.chip_name = "t7",
	.drv_max = 3,
	.offset = {0x0, 0x600, 0x800},
};

static struct vrr_data_s vrr_data_t3 = {
	.chip_type = VRR_CHIP_T3,
	.chip_name = "t3",
	.drv_max = 1,
	.offset = {0x0},
};

static const struct of_device_id vrr_dt_match_table[] = {
	{
		.compatible = "amlogic, vrr-t7",
		.data = &vrr_data_t7,
	},
	{
		.compatible = "amlogic, vrr-t3",
		.data = &vrr_data_t3,
	},
	{}
};
#endif

static int vrr_probe(struct platform_device *pdev)
{
	struct aml_vrr_drv_s *vdrv;
	const struct of_device_id *match;
	struct vrr_data_s *vdata;
	unsigned int index = 0;
	int ret = 0;

	vrr_global_init_once();

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		if (vrr_debug_print & VRR_DBG_PR_NORMAL)
			VRRPR("%s: no index exist, default to 0\n", __func__);
		index = 0;
	}
	if (index >= VRR_MAX_DRV) {
		VRRERR("%s: invalid index %d\n", __func__, index);
		return -1;
	}
	if (vrr_drv_init_state & (1 << index)) {
		VRRERR("%s: index %d driver already registered\n",
		       __func__, index);
		return -1;
	}
	vrr_drv_init_state |= (1 << index);

	match = of_match_device(vrr_dt_match_table, &pdev->dev);
	if (!match) {
		VRRERR("%s: no match table\n", __func__);
		return -1;
	}
	vdata = (struct vrr_data_s *)match->data;
	if (index >= vdata->drv_max) {
		VRRERR("%s: invalid index %d\n", __func__, index);
		return -1;
	}

	vdrv = kzalloc(sizeof(*vdrv), GFP_KERNEL);
	if (!vdrv)
		return -ENOMEM;
	vdrv->index = index;
	vdrv->data = vdata;
	vrr_drv[index] = vdrv;
	VRRPR("[%d]: driver version: %s(%d-%s)\n",
	      index, VRR_DRV_VERSION,
	      vdata->chip_type, vdata->chip_name);

	/* set drvdata */
	platform_set_drvdata(pdev, vdrv);
	vrr_cdev_add(vdrv, &pdev->dev);

	ret = vrr_config_load(vdrv, pdev);
	if (ret)
		goto vrr_probe_err;

	vrr_debug_file_creat(vdrv);
	vrr_func_init(vdrv);

	VRRPR("[%d]: %s ok, init_state:0x%x\n",
	      index, __func__, vrr_drv_init_state);

	return 0;

vrr_probe_err:
	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	vrr_drv[index] = NULL;
	/* free drv */
	kfree(vdrv);

	VRRPR("[%d]: %s failed\n", index, __func__);
	return -1;
}

static int vrr_remove(struct platform_device *pdev)
{
	struct aml_vrr_drv_s *vdrv = platform_get_drvdata(pdev);
	int index;

	if (!vdrv)
		return 0;

	index = vdrv->index;

	vrr_debug_file_remove(vdrv);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	vrr_cdev_remove(vdrv);

	kfree(vdrv);
	vrr_drv[index] = NULL;

	vrr_global_remove_once();

	return 0;
}

static int vrr_resume(struct platform_device *pdev)
{
	return 0;
}

static int vrr_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct aml_vrr_drv_s *vdrv = platform_get_drvdata(pdev);

	if (vdrv->sw_timer_flag) {
		vrr_timer_stop(vdrv);
		vdrv->sw_timer_flag = 0;
		VRRPR("%s: stop sw_timer\n", __func__);
	}
	vrr_work_disable(vdrv);
	return 0;
}

static void vrr_shutdown(struct platform_device *pdev)
{
	struct aml_vrr_drv_s *vdrv = platform_get_drvdata(pdev);

	if (vdrv->sw_timer_flag) {
		vrr_timer_stop(vdrv);
		vdrv->sw_timer_flag = 0;
		VRRPR("%s: stop sw_timer\n", __func__);
	}
	vrr_work_disable(vdrv);
}

static struct platform_driver vrr_platform_driver = {
	.probe = vrr_probe,
	.remove = vrr_remove,
	.suspend = vrr_suspend,
	.resume = vrr_resume,
	.shutdown = vrr_shutdown,
	.driver = {
		.name = "meson_vrr",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(vrr_dt_match_table),
#endif
	},
};

int __init vrr_init(void)
{
	if (platform_driver_register(&vrr_platform_driver)) {
		VRRERR("failed to register lcd driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit vrr_exit(void)
{
	platform_driver_unregister(&vrr_platform_driver);
}
