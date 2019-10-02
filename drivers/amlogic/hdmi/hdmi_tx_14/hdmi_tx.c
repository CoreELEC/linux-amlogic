/*
 * drivers/amlogic/hdmi/hdmi_tx_14/hdmi_tx.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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


#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/switch.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
/* #include <mach/am_regs.h> */

/* #include <linux/amlogic/osd/osd_dev.h> */
#include <linux/amlogic/aml_gpio_consumer.h>

#include "hdmi_tx_hdcp.h"

#include <linux/input.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/of.h>

#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/sound/aout_notify.h>
#include <linux/amlogic/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_cec_14.h>
#include <linux/amlogic/hdmi_tx/hdmi_config.h>

#include "ts/tvenc_conf.h"

#define DEVICE_NAME "amhdmitx"
#define HDMI_TX_COUNT 32
#define HDMI_TX_POOL_NUM  6
#define HDMI_TX_RESOURCE_NUM 4
#define HDMI_TX_PWR_CTRL_NUM	6

static dev_t hdmitx_id;
static struct class *hdmitx_class;
static struct device *hdmitx_dev;
static struct gpio_desc *hdmi_gd;
static struct device *hdmi_dev;
static int set_disp_mode_auto(void);
struct vinfo_s *hdmi_get_current_vinfo(void);

#ifndef CONFIG_AM_TV_OUTPUT
/* Fake vinfo */
const struct vinfo_s vinfo_1080p60hz = {
	.name = "1080p60hz",
	.mode = VMODE_1080P,
	.width = 1920,
	.height = 1080,
	.field_height = 1080,
	.aspect_ratio_num = 16,
	.aspect_ratio_den  = 9,
	.sync_duration_num = 60,
	.sync_duration_den = 1,
	.video_clk		 = 148500000,
};
const struct vinfo_s *get_current_vinfo(void)
{
	return &vinfo_1080p60hz;
}
#endif

struct hdmi_config_platform_data *hdmi_pdata;
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
static int suspend_flag;
#endif

static struct hdmitx_dev hdmitx_device;
static struct switch_dev sdev = { /* android ics switch device */
	.name = "hdmi",
};
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static void hdmitx_early_suspend(struct early_suspend *h)
{
	const struct vinfo_s *info = hdmi_get_current_vinfo();
	struct hdmitx_dev *phdmi = (struct hdmitx_dev *)h->param;
	if (info && (strncmp(info->name, "panel", 5) == 0
		|| strncmp(info->name, "null", 4) == 0))
		return;
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	suspend_flag = 1;
#endif
	phdmi->hpd_lock = 1;
	phdmi->HWOp.Cntl((struct hdmitx_dev *)h->param,
		HDMITX_EARLY_SUSPEND_RESUME_CNTL, HDMITX_EARLY_SUSPEND);
	phdmi->cur_VIC = HDMI_Unkown;
	phdmi->output_blank_flag = 0;
	phdmi->HWOp.CntlDDC(phdmi, DDC_HDCP_OP, HDCP14_OFF);
	phdmi->HWOp.CntlDDC(phdmi, DDC_HDCP_OP, DDC_RESET_HDCP);
	phdmi->HWOp.CntlConfig(&hdmitx_device, CONF_CLR_AVI_PACKET, 0);
	phdmi->HWOp.CntlConfig(&hdmitx_device, CONF_CLR_VSDB_PACKET, 0);
	hdmi_print(IMP, SYS "HDMITX: early suspend\n");
}

static void hdmitx_late_resume(struct early_suspend *h)
{
	const struct vinfo_s *info = hdmi_get_current_vinfo();
	struct hdmitx_dev *phdmi = (struct hdmitx_dev *)h->param;
	if (info && (strncmp(info->name, "panel", 5) == 0 ||
		strncmp(info->name, "null", 4) == 0)) {
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_VIDEO_BLANK_OP, VIDEO_UNBLANK);
		return;
	} else {
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_VIDEO_BLANK_OP, VIDEO_BLANK);
	}
	phdmi->hpd_lock = 0;
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_OP,
		HDCP14_OFF);
	hdmitx_device.internal_mode_change = 0;
	set_disp_mode_auto();
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	suspend_flag = 0;
#endif
	pr_info("amhdmitx: late resume module %d\n", __LINE__);
	phdmi->HWOp.Cntl((struct hdmitx_dev *)h->param,
		HDMITX_EARLY_SUSPEND_RESUME_CNTL, HDMITX_LATE_RESUME);
	hdmi_print(INF, SYS "late resume\n");
}

static struct early_suspend hdmitx_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10,
	.suspend = hdmitx_early_suspend,
	.resume = hdmitx_late_resume,
	.param = &hdmitx_device,
};
#endif

/* static struct hdmitx_info hdmi_info; */
#define INIT_FLAG_VDACOFF		0x1
	/* unplug powerdown */
#define INIT_FLAG_POWERDOWN	  0x2

#define INIT_FLAG_NOT_LOAD 0x80

int hdmi_ch = 1;		/* 1: 2ch */

static unsigned char init_flag;
#undef DISABLE_AUDIO
/* if set to 1, then HDMI will output no audio */
/* In KTV case, HDMI output Picture only, and Audio is driven by other
 * sources.
 */
unsigned char hdmi_audio_off_flag = 0;
static int hpdmode = 1; /*
				0, do not unmux hpd when off or unplug ;
				1, unmux hpd when unplug;
				2, unmux hpd when unplug  or off;
			*/
#ifdef CONFIG_AM_TV_OUTPUT2
static int force_vout_index;
#endif
/* 0xffff=disable; 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 31*/
static int hdmi_prbs_mode = 0xffff;
static int hdmi_480p_force_clk; /* 200, 225, 250, 270 */
static int hdmi_detect_when_booting = 1;
/* 1: error  2: important  3: normal  4: detailed */
static int debug_level = INF;

/*****************************
*	hdmitx attr management :
*	enable
*	mode
*	reg
******************************/
static void set_test_mode(void)
{
#ifdef ENABLE_TEST_MODE
/* when it is used as test source (PRBS and 20,22.5,25MHz) */
	if ((hdmi_480p_force_clk) &&
		((hdmitx_device.cur_VIC == HDMI_480p60) ||
		(hdmitx_device.cur_VIC == HDMI_480p60_16x9) ||
		(hdmitx_device.cur_VIC == HDMI_480i60) ||
		(hdmitx_device.cur_VIC == HDMI_480i60_16x9) ||
		(hdmitx_device.cur_VIC == HDMI_576p50) ||
		(hdmitx_device.cur_VIC == HDMI_576p50_16x9) ||
		(hdmitx_device.cur_VIC == HDMI_576i50) ||
		(hdmitx_device.cur_VIC == HDMI_576i50_16x9))
		) {
		if (hdmitx_device.HWOp.Cntl)
			hdmitx_device.HWOp.Cntl(&hdmitx_device,
				HDMITX_FORCE_480P_CLK,
				hdmi_480p_force_clk);
		}
		if (hdmi_prbs_mode != 0xffff)
			if (hdmitx_device.HWOp.Cntl)
				hdmitx_device.HWOp.Cntl(&hdmitx_device,
					HDMITX_HWCMD_TURN_ON_PRBS,
					hdmi_prbs_mode);
	}
#endif
}

int get_cur_vout_index(void)
/*
return value: 1, vout; 2, vout2;
*/
{
	int vout_index = 1;
#ifdef CONFIG_AM_TV_OUTPUT2
	if (force_vout_index)
		vout_index = force_vout_index;
	else {
		/* VPU_VIU_VENC_MUX_CTRL
		 * [ 3: 2] cntl_viu2_sel_venc. Select which one of the
		 * encI/P/T that VIU2 connects to:
		 * 0=No connection, 1=ENCI, 2=ENCP, 3=ENCT.
		 * [ 1: 0] cntl_viu1_sel_venc. Select which one of the
		 * encI/P/T that VIU1 connects to:
		 * 0=No connection, 1=ENCI, 2=ENCP, 3=ENCT.
		 */
		int viu2_sel = (aml_read_reg32
			(P_VPU_VIU_VENC_MUX_CTRL)>>2)&0x3;
		int viu1_sel = aml_read_reg32
			(P_VPU_VIU_VENC_MUX_CTRL)&0x3;
		if (((viu2_sel == 1) || (viu2_sel == 2)) &&
			(viu1_sel != 1) && (viu1_sel != 2))
			vout_index = 2;
	}
#endif
	return vout_index;
}

struct vinfo_s *hdmi_get_current_vinfo(void)
{
	struct vinfo_s *info;
#ifdef CONFIG_AM_TV_OUTPUT2
	if (get_cur_vout_index() == 2) {
		info = get_current_vinfo2();
	/* add to fix problem when dual display is not enabled in UI */
		if (info == NULL)
			info = get_current_vinfo();
	} else
		info = get_current_vinfo();
#else
	info = get_current_vinfo();
#endif
	return info;
}

static  int  set_disp_mode(const char *mode)
{
	int ret =  -1;
	enum hdmi_vic vic;

	vic = hdmitx_edid_get_VIC(&hdmitx_device, mode, 1);
	if (strncmp(mode, "2160p30hz", strlen("2160p30hz")) == 0)
		vic = HDMI_4k2k_30;
	else if (strncmp(mode, "2160p25hz", strlen("2160p25hz")) == 0)
		vic = HDMI_4k2k_25;
	else if (strncmp(mode, "2160p24hz", strlen("2160p24hz")) == 0)
		vic = HDMI_4k2k_24;
	else if (strncmp(mode, "smpte24hz", strlen("smpte24hz")) == 0)
		vic = HDMI_4k2k_smpte_24;
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	else if (strncmp(mode, "4k2k29hz", strlen("4k2k29hz")) == 0)
		vic = HDMI_4k2k_30;
	else if (strncmp(mode, "4k2k23hz", strlen("4k2k23hz")) == 0)
		vic = HDMI_4k2k_24;
#endif
	else
		;/* nothing */

	if (strncmp(mode, "1080p60hz", strlen("1080p60hz")) == 0)
		vic = HDMI_1080p60;
	if (strncmp(mode, "1080p50hz", strlen("1080p50hz")) == 0)
		vic = HDMI_1080p50;

	if (vic != HDMI_Unkown) {
		hdmitx_device.mux_hpd_if_pin_high_flag = 1;
		if (hdmitx_device.vic_count == 0) {
			if (hdmitx_device.unplug_powerdown)
				return 0;
		}
	}
#if 0
	set_vmode_enc_hw(vic);
#endif
	hdmitx_device.cur_VIC = HDMI_Unkown;
	ret = hdmitx_set_display(&hdmitx_device, vic);
	if (ret >= 0) {
		hdmitx_device.HWOp.Cntl(&hdmitx_device,
			HDMITX_AVMUTE_CNTL, AVMUTE_CLEAR);
		hdmitx_device.cur_VIC = vic;
		hdmitx_device.audio_param_update_flag = 1;
		hdmitx_device.auth_process_timer = AUTH_PROCESS_TIME;
		hdmitx_device.internal_mode_change = 0;
		set_test_mode();
	}

	if (hdmitx_device.cur_VIC == HDMI_Unkown) {
		if (hpdmode == 2) {
			/* edid will be read again when hpd is muxed and
			 * it is high
			 */
			hdmitx_edid_clear(&hdmitx_device);
			hdmitx_device.mux_hpd_if_pin_high_flag = 0;
		}
		if (hdmitx_device.HWOp.Cntl) {
			hdmitx_device.HWOp.Cntl(&hdmitx_device,
				HDMITX_HWCMD_TURNOFF_HDMIHW,
				(hpdmode == 2)?1:0);
		}
	}

	return ret;
}

static void hdmitx_pre_display_init(void)
{
	hdmitx_device.cur_VIC = HDMI_Unkown;
	hdmitx_device.auth_process_timer = AUTH_PROCESS_TIME;
	hdmitx_device.internal_mode_change = 1;
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_VIDEO_BLANK_OP, VIDEO_BLANK);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_OP,
		HDCP14_OFF);
	/* msleep(10); */
	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_TMDS_PHY_OP,
		TMDS_PHY_DISABLE);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_CLR_AVI_PACKET, 0);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_CLR_VSDB_PACKET, 0);
	hdmitx_device.internal_mode_change = 0;
}

#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
/*  */
/* input para: name of vmode, such as "1080p50hz" */
/* return values: */
/* 0: not supported in edid */
/* 1: supported in edid */
/* 2: no edid */
/*  */
int hdmitx_is_vmode_supported(char *mode_name)
{
	enum hdmi_vic vic;

	if (hdmitx_device.tv_no_edid)
		return 2;

	vic = hdmitx_edid_get_VIC(&hdmitx_device, mode_name, 0);
	if (vic != HDMI_Unkown)
		return 1;
	else
		return 0;

	return 0;
}
EXPORT_SYMBOL(hdmitx_is_vmode_supported);

#endif

static int set_disp_mode_auto(void)
{
	int ret =  -1;
	const struct vinfo_s *info = NULL;
	unsigned char mode[16];
	enum hdmi_vic vic = HDMI_Unkown;
	/* vic_ready got from IP */
	enum hdmi_vic vic_ready = hdmitx_device.HWOp.GetState(
		&hdmitx_device, STAT_VIDEO_VIC, 0);

	memset(mode, 0, 10);

	/* if HDMI plug-out, directly return */
	if (!(hdmitx_device.HWOp.CntlMisc(&hdmitx_device,
		MISC_HPD_GPI_ST, 0))) {
		hdmi_print(ERR, HPD "HPD deassert!\n");
	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_TMDS_PHY_OP,
		TMDS_PHY_DISABLE);
		return -1;
	}

	/* get current vinfo */
	info = hdmi_get_current_vinfo();
	if (info == NULL) {
		hdmi_print(ERR, VID "cann't get valid mode\n");
		return -1;
	} else
		hdmi_print(IMP, VID "get current mode: %s\n", info->name);

	/* If info->name equals to cvbs, then set mode to I mode to hdmi
	 */
	if ((strncmp(info->name, "480cvbs", 7) == 0) ||
		(strncmp(info->name, "576cvbs", 7) == 0) ||
		(strncmp(info->name, "ntsc_m", 6) == 0) ||
		(strncmp(info->name, "pal_m", 5) == 0) ||
		(strncmp(info->name, "pal_n", 5) == 0) ||
		(strncmp(info->name, "panel", 5) == 0) ||
		(strncmp(info->name, "null", 4) == 0)) {
		hdmi_print(ERR, VID "%s not valid hdmi mode\n", info->name);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_CLR_AVI_PACKET, 0);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_CLR_VSDB_PACKET, 0);
	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_TMDS_PHY_OP,
		TMDS_PHY_DISABLE);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_VIDEO_BLANK_OP, VIDEO_UNBLANK);
	return -1;
	} else
		memcpy(mode, info->name, strlen(info->name));

	/* msleep(500); */
	vic = hdmitx_edid_get_VIC(&hdmitx_device, mode, 1);
	if (strncmp(info->name, "2160p30hz", strlen("2160p30hz")) == 0) {
		vic = HDMI_4k2k_30;
	} else if (strncmp(info->name, "2160p25hz",
		strlen("2160p25hz")) == 0) {
		vic = HDMI_4k2k_25;
	} else if (strncmp(info->name, "2160p24hz",
		strlen("2160p24hz")) == 0) {
		vic = HDMI_4k2k_24;
	} else if (strncmp(info->name, "smpte24hz",
		strlen("smpte24hz")) == 0)
		vic = HDMI_4k2k_smpte_24;
	else {
	/* nothing */
	}

#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	if (suspend_flag == 1)
		vic_ready = HDMI_Unkown;
#endif

	if ((vic_ready != HDMI_Unkown) && (vic_ready == vic)) {
		hdmi_print(IMP, SYS "[%s] ALREADY init VIC = %d\n",
			__func__, vic);
	if (hdmitx_device.RXCap.IEEEOUI == 0) {
		/* DVI case judgement. In uboot, directly output HDMI
		 * mode
		 */
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_HDMI_DVI_MODE, DVI_MODE);
		hdmi_print(IMP, SYS "change to DVI mode\n");
	}
	hdmitx_device.cur_VIC = vic;
	hdmitx_device.output_blank_flag = 1;
	return 1;
	} else
		hdmitx_pre_display_init();

	hdmitx_device.cur_VIC = HDMI_Unkown;
/* if vic is HDMI_Unkown, hdmitx_set_display will disable HDMI */
	ret = hdmitx_set_display(&hdmitx_device, vic);
	if (ret >= 0) {
		hdmitx_device.HWOp.Cntl(&hdmitx_device,
			HDMITX_AVMUTE_CNTL, AVMUTE_CLEAR);
		hdmitx_device.cur_VIC = vic;
		hdmitx_device.audio_param_update_flag = 1;
		hdmitx_device.auth_process_timer = AUTH_PROCESS_TIME;
		hdmitx_device.internal_mode_change = 0;
		set_test_mode();
	}
	if (hdmitx_device.cur_VIC == HDMI_Unkown) {
		if (hpdmode == 2) {
			/* edid will be read again when hpd is muxed
			 * and it is high
			 */
			hdmitx_edid_clear(&hdmitx_device);
			hdmitx_device.mux_hpd_if_pin_high_flag = 0;
		}
	/* If current display is NOT panel, needn't TURNOFF_HDMIHW */
	if (strncmp(mode, "panel", 5) == 0) {
		hdmitx_device.HWOp.Cntl(&hdmitx_device,
			HDMITX_HWCMD_TURNOFF_HDMIHW,
			(hpdmode == 2)?1:0);
	}
	}
	hdmitx_set_audio(&hdmitx_device,
		&(hdmitx_device.cur_audio_param), hdmi_ch);
	hdmitx_device.output_blank_flag = 1;
	return ret;
}
#if 0
static unsigned int set_cec_code(const char *buf, size_t count)
{
	char tmpbuf[128];
	int i = 0;
	int ret;
	/* int j; */
	unsigned int cec_code;
	/* unsigned int value=0; */

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
	i++;
	}
	tmpbuf[i] = 0;

	ret = kstrtoul(tmpbuf, NULL, 16, &cec_code);

	input_event(remote_cec_dev, EV_KEY, cec_code, 1);
	input_event(remote_cec_dev, EV_KEY, cec_code, 0);
	input_sync(remote_cec_dev);
	return cec_code;
}
#endif
static unsigned char is_dispmode_valid_for_hdmi(void)
{
	enum hdmi_vic vic;
	const struct vinfo_s *info = hdmi_get_current_vinfo();

	vic = hdmitx_edid_get_VIC(&hdmitx_device, info->name, 1);

	return vic != HDMI_Unkown;
}

/*disp_mode attr*/
static ssize_t show_disp_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	pos += snprintf(buf+pos, PAGE_SIZE, "VIC:%d\n",
		hdmitx_device.cur_VIC);
	return pos;
}

static ssize_t store_disp_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	set_disp_mode(buf);
	return 16;
}

/*cec attr*/
static ssize_t show_cec(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t t = 0;  /* cec_usrcmd_get_global_info(buf); */
	return t;
}

static ssize_t store_cec(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	cec_usrcmd_set_dispatch(buf, count);
	return count;
}

static ssize_t show_cec_config(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
/*	pos+=snprintf(buf+pos, PAGE_SIZE, "0x%x\n",
		aml_read_reg32(P_AO_DEBUG_REG0));
*/
	return pos;
}

static ssize_t store_cec_config(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	cec_usrcmd_set_config(buf, count);
	return count;
}

static ssize_t store_cec_lang_config(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 16, &val);
	hdmi_print(INF, CEC "store_cec_lang_config\n");
	cec_global_info.cec_node_info[cec_global_info.my_node_index].
		menu_lang = (int)val;
	cec_usrcmd_set_lang_config(buf, count);
	return count;
}

static ssize_t show_cec_lang_config(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	hdmi_print(INF, CEC "show_cec_lang_config\n");
	pos += snprintf(buf+pos, PAGE_SIZE, "%x\n", cec_global_info.
		cec_node_info[cec_global_info.my_node_index].menu_lang);
	return pos;
}

/*aud_mode attr*/
static ssize_t show_aud_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t store_aud_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* set_disp_mode(buf); */
	struct hdmitx_audpara *audio_param =
		&(hdmitx_device.cur_audio_param);
	if (strncmp(buf, "32k", 3) == 0)
		audio_param->sample_rate = FS_32K;
	else if (strncmp(buf, "44.1k", 5) == 0)
		audio_param->sample_rate = FS_44K1;
	else if (strncmp(buf, "48k", 3) == 0)
		audio_param->sample_rate = FS_48K;
	else {
		hdmitx_device.force_audio_flag = 0;
		return count;
	}
	audio_param->type = CT_PCM;
	audio_param->channel_num = CC_2CH;
	audio_param->sample_size = SS_16BITS;

	hdmitx_device.audio_param_update_flag = 1;
	hdmitx_device.force_audio_flag = 1;

	return count;
}

/*edid attr*/
static ssize_t show_edid(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return hdmitx_edid_dump(&hdmitx_device, buf, PAGE_SIZE);
}

static ssize_t store_edid(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == 'h') {
		int i;
		hdmi_print(INF, EDID "EDID hash value:\n");
		for (i = 0; i < 20; i++)
			pr_info("%02x", hdmitx_device.EDID_hash[i]);
		pr_info("\n");
	}
	if (buf[0] == 'd') {
		int ii, jj;
		unsigned long block_idx;
		int ret;
		ret = kstrtoul(buf+1, 16, &block_idx);
		if (block_idx < EDID_MAX_BLOCK) {
			for (ii = 0; ii < 8; ii++) {
				for (jj = 0; jj < 16; jj++) {
					pr_info("%02x ",
			hdmitx_device.EDID_buf[block_idx*128+ii*16+jj]);
				}
				pr_info("\n");
			}
		pr_info("\n");
	}
	}
	if (buf[0] == 'e') {
		int ii, jj;
		unsigned long block_idx;
		int ret;
		ret = kstrtoul(buf+1, 16, &block_idx);
		if (block_idx < EDID_MAX_BLOCK) {
			for (ii = 0; ii < 8; ii++) {
				for (jj = 0; jj < 16; jj++) {
					pr_info("%02x ",
		hdmitx_device.EDID_buf1[block_idx*128+ii*16+jj]);
				}
				pr_info("\n");
			}
			pr_info("\n");
		}
	}
	return 16;
}

/*config attr*/
static ssize_t show_config(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	unsigned char *aud_conf;
	switch (hdmitx_device.tx_aud_cfg) {
	case 0:
	aud_conf = "off";
	break;
	case 1:
	aud_conf = "on";
	break;
	case 2:
	aud_conf = "auto";
	break;
	default:
	aud_conf = "none";
	}
	pos += snprintf(buf+pos, PAGE_SIZE,
		"disp switch (force or edid): %s\n",
		(hdmitx_device.disp_switch_config ==
		DISP_SWITCH_FORCE)?"force":"edid");
	pos += snprintf(buf+pos, PAGE_SIZE, "audio config: %s\n",
		aud_conf);
	return pos;
}

static ssize_t store_config(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	if (strncmp(buf, "force", 5) == 0)
		hdmitx_device.disp_switch_config = DISP_SWITCH_FORCE;
	else if (strncmp(buf, "edid", 4) == 0)
		hdmitx_device.disp_switch_config = DISP_SWITCH_EDID;
	else if (strncmp(buf, "unplug_powerdown", 16) == 0) {
		if (buf[16] == '0')
			hdmitx_device.unplug_powerdown = 0;
		else
			hdmitx_device.unplug_powerdown = 1;
	} else if (strncmp(buf, "3d", 2) == 0) {
		/* First, disable HDMI TMDS */
		hdmitx_device.HWOp.CntlMisc(&hdmitx_device,
			MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
			/* Second, set 3D parameters */
		if (strncmp(buf+2, "tb", 2) == 0)
			hdmi_set_3d(&hdmitx_device, 6, 0);
		else if (strncmp(buf+2, "lr", 2) == 0) {
			unsigned long sub_sample_mode = 0;
			if (buf[2])
				ret = kstrtoul(buf+2, 10,
					&sub_sample_mode);
		/* side by side */
			hdmi_set_3d(&hdmitx_device, 8, sub_sample_mode);
		} else if (strncmp(buf+2, "off", 3) == 0)
			hdmi_set_3d(&hdmitx_device, 0xf, 0);
		/* Last, delay sometime and enable HDMI TMDS */
		msleep(20);
		hdmitx_device.HWOp.CntlMisc(&hdmitx_device,
			MISC_TMDS_PHY_OP, TMDS_PHY_ENABLE);
	} else if (strncmp(buf, "audio_", 6) == 0) {
		if (strncmp(buf+6, "off", 3) == 0) {
			hdmitx_device.tx_aud_cfg = 0;
			hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
				CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
			hdmi_print(IMP, AUD "configure off\n");
		} else if (strncmp(buf+6, "on", 2) == 0) {
			hdmitx_device.tx_aud_cfg = 1;
			hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
				CONF_AUDIO_MUTE_OP, AUDIO_UNMUTE);
			hdmi_print(IMP, AUD "configure on\n");
		} else if (strncmp(buf+6, "auto", 4) == 0) {
			/* auto mode. if sink doesn't support current
			 * audio format, then no audio output
			 */
			hdmitx_device.tx_aud_cfg = 2;
			hdmi_print(IMP, AUD "configure auto\n");
		} else
			hdmi_print(ERR, AUD "configure error\n");
	}
	return 16;
}


static ssize_t store_debug(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	hdmitx_device.HWOp.DebugFun(&hdmitx_device, buf);
	return 16;
}

/* support format lists */
const char *disp_mode_t[] = {
	"480i60hz",
	"480i_rpt",
	"480p60hz",
	"480p_rpt",
	"576i50hz",
	"576i_rpt",
	"576p50hz",
	"576p_rpt",
	"720p60hz",
	"1080i60hz",
	"1080p60hz",
	"720p50hz",
	"1080i50hz",
	"1080p50hz",
	"1080p24hz",
	"2160p30hz",
	"2160p25hz",
	"2160p24hz",
	"smpte24hz",
	NULL
};

/**/
static ssize_t show_disp_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, pos = 0;
	const char *native_disp_mode =
		hdmitx_edid_get_native_VIC(&hdmitx_device);
	enum hdmi_vic vic;
	if (hdmitx_device.tv_no_edid) {
		pos += snprintf(buf+pos, PAGE_SIZE, "null edid\n");
	} else {
		for (i = 0; disp_mode_t[i]; i++) {
			vic = hdmitx_edid_get_VIC(&hdmitx_device,
				disp_mode_t[i], 0);
		if (vic != HDMI_Unkown) {
			pos += snprintf(buf+pos, PAGE_SIZE, "%s",
				disp_mode_t[i]);
			if (native_disp_mode && (strcmp(
				native_disp_mode,
				disp_mode_t[i]) == 0)) {
				pos += snprintf(buf+pos, PAGE_SIZE,
					"*\n");
			} else
			pos += snprintf(buf+pos, PAGE_SIZE, "\n");
		}
		}
	}
	return pos;
}


/**/
static ssize_t show_disp_cap_3d(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, pos = 0;
	int j = 0;
	enum hdmi_vic vic;

	for (i = 0; disp_mode_t[i]; i++) {
		vic = hdmitx_edid_get_VIC(&hdmitx_device,
			disp_mode_t[i], 0);
	if (vic == hdmitx_device.cur_VIC) {
		for (j = 0; j < hdmitx_device.RXCap.VIC_count; j++) {
			if (vic == hdmitx_device.RXCap.VIC[j])
				break;
		}
		pos += snprintf(buf+pos, PAGE_SIZE, "%s ",
			disp_mode_t[i]);
		if (hdmitx_device.RXCap.support_3d_format[
			hdmitx_device.RXCap.VIC[j]].frame_packing == 1){
			pos += snprintf(buf+pos, PAGE_SIZE,
				"FramePacking ");
		}
		if (hdmitx_device.RXCap.support_3d_format[
			hdmitx_device.RXCap.VIC[j]].top_and_bottom == 1){
			pos += snprintf(buf+pos, PAGE_SIZE,
				"TopBottom ");
		}
		if (hdmitx_device.RXCap.support_3d_format[
			hdmitx_device.RXCap.VIC[j]].side_by_side == 1) {
			pos += snprintf(buf+pos, PAGE_SIZE,
				"SidebySide ");
		}
	}
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "\n");

	return pos;
}

/**/
static ssize_t show_aud_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, pos = 0, j;
	const char *aud_coding_type[] =  {
		"ReferToStreamHeader", "PCM", "AC-3", "MPEG1", "MP3",
		"MPEG2", "AAC", "DTS", "ATRAC",	"OneBitAudio",
		"Dobly_Digital+", "DTS-HD", "MAT", "DST", "WMA_Pro",
		"Reserved", NULL};
	const char *aud_sampling_frequency[] = {
		"ReferToStreamHeader", "32", "44.1", "48", "88.2", "96",
		"176.4", "192", NULL};
	const char *aud_sample_size[] = {"ReferToStreamHeader", "16",
		"20", "24", NULL};

	struct rx_cap *pRXCap = &(hdmitx_device.RXCap);
	pos += snprintf(buf + pos, PAGE_SIZE,
		"CodingType MaxChannels SamplingFreq SampleSize\n");
	for (i = 0; i < pRXCap->AUD_count; i++) {
		pos += snprintf(buf + pos, PAGE_SIZE, "%s, %d ch, ",
			aud_coding_type[pRXCap->RxAudioCap[i].
				audio_format_code],
			pRXCap->RxAudioCap[i].channel_num_max + 1);
	for (j = 0; j < 7; j++) {
		if (pRXCap->RxAudioCap[i].freq_cc & (1 << j))
			pos += snprintf(buf + pos, PAGE_SIZE, "%s/",
				aud_sampling_frequency[j+1]);
	}
	pos += snprintf(buf + pos - 1, PAGE_SIZE, " kHz, ");
	for (j = 0; j < 3; j++) {
		if (pRXCap->RxAudioCap[i].cc3 & (1 << j))
			pos += snprintf(buf + pos, PAGE_SIZE, "%s/",
				aud_sample_size[j+1]);
	}
	pos += snprintf(buf + pos - 1, PAGE_SIZE, " bit\n");
	}

	return pos;
}

static ssize_t show_aud_ch(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	   int pos = 0;
	   pos += snprintf(buf + pos, PAGE_SIZE,
		"hdmi_channel = %d ch\n", hdmi_ch ? hdmi_ch + 1 : 0);
	   return pos;
}

static ssize_t store_aud_ch(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (strncmp(buf, "6ch", 3) == 0)
		hdmi_ch = 5;
	else if (strncmp(buf, "8ch", 3) == 0)
		hdmi_ch = 7;
	else if (strncmp(buf, "2ch", 3) == 0)
		hdmi_ch = 1;
	else
		return count;

	hdmitx_device.audio_param_update_flag = 1;
	hdmitx_device.force_audio_flag = 1;

	return count;
}

static ssize_t show_hdcp_ksv_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0, i;
	char aksv_buf[5];
	char bksv_buf[5];

	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_GET_AKSV,
		(unsigned int)aksv_buf);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_GET_BKSV,
		(unsigned int)bksv_buf);

	pos += snprintf(buf+pos, PAGE_SIZE, "AKSV: ");
	for (i = 0; i < 5; i++) {
		pos += snprintf(buf+pos, PAGE_SIZE, "%02x",
			aksv_buf[i]);
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "  %s\n",
		hdcp_ksv_valid(aksv_buf) ? "Valid" : "Invalid");

	pos += snprintf(buf+pos, PAGE_SIZE, "BKSV: ");
	for (i = 0; i < 5; i++) {
		pos += snprintf(buf+pos, PAGE_SIZE, "%02x",
			bksv_buf[i]);
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "  %s\n",
		hdcp_ksv_valid(bksv_buf) ? "Valid" : "Invalid");

	return pos;
}

static ssize_t show_hpd_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	hdmitx_device.hpd_state = hdmitx_device.HWOp.CntlMisc(
		&hdmitx_device, MISC_HPD_GPI_ST, 0);
	pos += snprintf(buf+pos, PAGE_SIZE, "%d",
		hdmitx_device.hpd_state);
	return pos;
}

static ssize_t show_support_3d(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n",
		hdmitx_device.RXCap.threeD_present);
	return pos;
}

void hdmi_print(int dbg_lvl, const char *fmt, ...)
{
	va_list args;
#if 0
	if (dbg_lvl == OFF)
		return;
#endif
	if (dbg_lvl <= debug_level) {
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
	}
}

static DEVICE_ATTR(disp_mode, S_IWUSR | S_IRUGO | S_IWGRP,
	show_disp_mode, store_disp_mode);
static DEVICE_ATTR(aud_mode, S_IWUSR | S_IRUGO, show_aud_mode,
	store_aud_mode);
static DEVICE_ATTR(edid, S_IWUSR | S_IRUGO, show_edid, store_edid);
static DEVICE_ATTR(config, S_IWUSR | S_IRUGO | S_IWGRP, show_config,
	store_config);
static DEVICE_ATTR(debug, S_IWUSR, NULL, store_debug);
static DEVICE_ATTR(disp_cap, S_IRUGO, show_disp_cap, NULL);
static DEVICE_ATTR(aud_cap, S_IRUGO, show_aud_cap, NULL);
static DEVICE_ATTR(aud_ch, S_IWUSR | S_IRUGO | S_IWGRP, show_aud_ch,
	store_aud_ch);
static DEVICE_ATTR(disp_cap_3d, S_IRUGO, show_disp_cap_3d,
	NULL);
static DEVICE_ATTR(hdcp_ksv_info, S_IRUGO, show_hdcp_ksv_info,
	NULL);
static DEVICE_ATTR(hpd_state, S_IRUGO, show_hpd_state, NULL);
static DEVICE_ATTR(support_3d, S_IRUGO, show_support_3d,
	NULL);
static DEVICE_ATTR(cec, S_IWUSR | S_IRUGO, show_cec, store_cec);
static DEVICE_ATTR(cec_config, S_IWUSR | S_IRUGO | S_IWGRP,
	show_cec_config, store_cec_config);
static DEVICE_ATTR(cec_lang_config, S_IWUSR | S_IRUGO | S_IWGRP,
	show_cec_lang_config, store_cec_lang_config);

/*****************************
*	hdmitx display client interface
*
******************************/
static int hdmitx_notify_callback_v(struct notifier_block *block,
	unsigned long cmd , void *para)
{
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	const struct vinfo_s *info = NULL;
	enum hdmi_vic vic_ready = HDMI_Unkown;
	enum hdmi_vic vic_now = HDMI_Unkown;
	enum fine_tune_mode_e fine_tune_mode = get_hpll_tune_mode();
#endif
	if (get_cur_vout_index() != 1)
		return 0;

	if (cmd != VOUT_EVENT_MODE_CHANGE)
		return 0;

#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	if (suspend_flag == 1)
		return 0;
	/* vic_ready got from IP */
	vic_ready = hdmitx_device.HWOp.GetState(&hdmitx_device,
		STAT_VIDEO_VIC, 0);
	/* get current vinfo */
	info = hdmi_get_current_vinfo();
	if (info == NULL) {
		hdmi_print(ERR, VID "cann't get valid mode\n");
		return -1;
	} else
		hdmi_print(IMP, VID "get current mode: %s\n",
			info->name);

	vic_now = hdmitx_edid_get_VIC(&hdmitx_device, info->name, 1);
	if ((HDMI_Unkown != vic_ready) && (vic_ready == vic_now)) {
		if (KEEP_HPLL != fine_tune_mode) {
			hdmitx_device.HWOp.CntlMisc(&hdmitx_device,
				MISC_FINE_TUNE_HPLL, fine_tune_mode);
			return 0;
		}
	}
#endif
	if (hdmitx_device.vic_count == 0) {
		if (is_dispmode_valid_for_hdmi()) {
			hdmitx_device.mux_hpd_if_pin_high_flag = 1;
			if (hdmitx_device.unplug_powerdown)
				return 0;
		}
	}

	set_disp_mode_auto();

	return 0;
}

#ifdef CONFIG_AM_TV_OUTPUT2
static int hdmitx_notify_callback_v2(struct notifier_block *block,
	unsigned long cmd , void *para)
{
	if (get_cur_vout_index() != 2)
		return 0;

	if (cmd != VOUT_EVENT_MODE_CHANGE)
		return 0;

	if (hdmitx_device.vic_count == 0) {
		if (is_dispmode_valid_for_hdmi()) {
			hdmitx_device.mux_hpd_if_pin_high_flag = 1;
			if (hdmitx_device.unplug_powerdown)
				return 0;
		}
	}

	set_disp_mode_auto();

	return 0;
}
#endif

static struct notifier_block hdmitx_notifier_nb_v = {
	.notifier_call	= hdmitx_notify_callback_v,
};

#ifdef CONFIG_AM_TV_OUTPUT2
static struct notifier_block hdmitx_notifier_nb_v2 = {
	.notifier_call	= hdmitx_notify_callback_v2,
};
#endif

/* Refer to CEA-861-D Page 88 */
#define AOUT_EVENT_REFER_TO_STREAM_HEADER	   0x0
#define AOUT_EVENT_IEC_60958_PCM				0x1
#define AOUT_EVENT_RAWDATA_AC_3				 0x2
#define AOUT_EVENT_RAWDATA_MPEG1				0x3
#define AOUT_EVENT_RAWDATA_MP3				  0x4
#define AOUT_EVENT_RAWDATA_MPEG2				0x5
#define AOUT_EVENT_RAWDATA_AAC				  0x6
#define AOUT_EVENT_RAWDATA_DTS				  0x7
#define AOUT_EVENT_RAWDATA_ATRAC				0x8
#define AOUT_EVENT_RAWDATA_ONE_BIT_AUDIO		0x9
#define AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS   0xA
#define AOUT_EVENT_RAWDATA_DTS_HD			   0xB
#define AOUT_EVENT_RAWDATA_MAT_MLP			  0xC
#define AOUT_EVENT_RAWDATA_DST				  0xD
#define AOUT_EVENT_RAWDATA_WMA_PRO			  0xE
#include <linux/soundcard.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>

static struct rate_map_fs map_fs[] = {
	{0,	  FS_REFER_TO_STREAM},
	{32000,  FS_32K},
	{44100,  FS_44K1},
	{48000,  FS_48K},
	{88200,  FS_88K2},
	{96000,  FS_96K},
	{176400, FS_176K4},
	{192000, FS_192K},
};

static enum hdmi_audio_fs aud_samp_rate_map(unsigned int rate)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(map_fs); i++) {
		if (map_fs[i].rate == rate) {
			hdmi_print(IMP, AUD "aout notify rate %d\n",
				rate);
			return map_fs[i].fs;
		}
	}
	hdmi_print(IMP, AUD "get FS_MAX\n");
	return FS_MAX;
}

static unsigned char *aud_type_string[] = {
	"CT_REFER_TO_STREAM",
	"CT_PCM",
	"CT_AC_3",
	"CT_MPEG1",
	"CT_MP3",
	"CT_MPEG2",
	"CT_AAC",
	"CT_DTS",
	"CT_ATRAC",
	"CT_ONE_BIT_AUDIO",
	"CT_DOLBY_D",
	"CT_DTS_HD",
	"CT_MAT",
	"CT_DST",
	"CT_WMA",
	"CT_MAX",
};

static struct size_map aud_size_map_ss[] = {
	{0,	 SS_REFER_TO_STREAM},
	{16,	SS_16BITS},
	{20,	SS_20BITS},
	{24,	SS_24BITS},
	{32,	SS_MAX},
};

static enum hdmi_audio_sampsize aud_size_map(unsigned int bits)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aud_size_map_ss); i++) {
		if (bits == aud_size_map_ss[i].sample_bits) {
			hdmi_print(IMP, AUD "aout notify size %d\n",
				bits);
			return aud_size_map_ss[i].ss;
		}
	}
	hdmi_print(IMP, AUD "get SS_MAX\n");
	return SS_MAX;
}

static int hdmitx_notify_callback_a(struct notifier_block *block,
	unsigned long cmd , void *para);
static struct notifier_block hdmitx_notifier_nb_a = {
	.notifier_call	= hdmitx_notify_callback_a,
};
static int hdmitx_notify_callback_a(struct notifier_block *block,
	unsigned long cmd , void *para)
{
	int i, audio_check = 0;
	struct rx_cap *pRXCap = &(hdmitx_device.RXCap);
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)para;
	struct hdmitx_audpara *audio_param =
		&(hdmitx_device.cur_audio_param);
	enum hdmi_audio_fs n_rate = aud_samp_rate_map(substream->runtime->rate);
	enum hdmi_audio_sampsize n_size =
		aud_size_map(substream->runtime->sample_bits);

	hdmitx_device.audio_param_update_flag = 0;
	hdmitx_device.audio_notify_flag = 0;

	if (audio_param->sample_rate != n_rate) {
		audio_param->sample_rate = n_rate;
		hdmitx_device.audio_param_update_flag = 1;
	}

	if (audio_param->type != cmd) {
		audio_param->type = cmd;
	hdmi_print(INF, AUD "aout notify format %s\n",
		aud_type_string[audio_param->type]);
	hdmitx_device.audio_param_update_flag = 1;
	}

	if (audio_param->sample_size != n_size) {
		audio_param->sample_size = n_size;
		hdmitx_device.audio_param_update_flag = 1;
	}

	if (audio_param->channel_num !=
		(substream->runtime->channels - 1)) {
		audio_param->channel_num =
		substream->runtime->channels - 1;
		hdmitx_device.audio_param_update_flag = 1;
	}
	if (hdmitx_device.tx_aud_cfg == 2) {
		hdmi_print(INF, AUD "auto mode\n");
	/* Detect whether Rx is support current audio format */
	for (i = 0; i < pRXCap->AUD_count; i++) {
		if (pRXCap->RxAudioCap[i].audio_format_code == cmd)
			audio_check = 1;
	}
	/* sink don't support current audio mode */
	if ((!audio_check) && (cmd != AOUT_EVENT_IEC_60958_PCM)) {
		pr_info("Sink not support this audio format %lu\n",
			cmd);
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
		hdmitx_device.audio_param_update_flag = 0;
	}
	}
	if (hdmitx_device.audio_param_update_flag == 0)
		hdmi_print(INF, AUD "no update\n");
	else
		hdmitx_device.audio_notify_flag = 1;


	if ((!hdmi_audio_off_flag) &&
		(hdmitx_device.audio_param_update_flag)) {
		/* plug-in & update audio param */
		if (hdmitx_device.hpd_state == 1) {
			hdmitx_set_audio(&hdmitx_device,
			&(hdmitx_device.cur_audio_param), hdmi_ch);
		if ((hdmitx_device.audio_notify_flag == 1) ||
			(hdmitx_device.audio_step == 1)) {
			hdmitx_device.audio_notify_flag = 0;
			hdmitx_device.audio_step = 0;
#ifndef CONFIG_AML_HDMI_TX_HDCP
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AUDIO_MUTE_OP, AUDIO_UNMUTE);
#endif
		}
		hdmitx_device.audio_param_update_flag = 0;
		hdmi_print(INF, AUD "set audio param\n");
	}
	}


	return 0;
}

/******************************
*  hdmitx kernel task
*******************************/
int tv_audio_support(int type, struct rx_cap *pRXCap)
{
	int i, audio_check = 0;
	for (i = 0; i < pRXCap->AUD_count; i++) {
		if (pRXCap->RxAudioCap[i].audio_format_code == type)
			audio_check = 1;
	}
	return audio_check;
}

static int hdmi_task_handle(void *data)
{
	struct hdmitx_dev *hdmitx_device = (struct hdmitx_dev *)data;

	sdev.state = !!(hdmitx_device->HWOp.CntlMisc(hdmitx_device,
		MISC_HPD_GPI_ST, 0));
	hdmitx_device->hpd_state = sdev.state;

	/* When init hdmi, clear the hdmitx module edid ram and
	 * edid buffer.
	 */
/*TODO	hdmitx_edid_ram_buffer_clear(hdmitx_device);*/

	/* default audio configure is on */
	hdmitx_device->tx_aud_cfg = 1;

	hdmitx_device->HWOp.SetupIRQ(hdmitx_device);
	if (init_flag&INIT_FLAG_POWERDOWN) {
		/* power down */
		hdmitx_device->unplug_powerdown = 1;
		if (hdmitx_device->HWOp.Cntl) {
			hdmitx_device->HWOp.Cntl(hdmitx_device,
				HDMITX_HWCMD_TURNOFF_HDMIHW,
				(hpdmode != 0)?1:0);
		}
	} else {
		if (hdmitx_device->HWOp.Cntl)
			hdmitx_device->HWOp.Cntl(hdmitx_device,
			HDMITX_HWCMD_MUX_HPD, 0);
	}
	if (hdmitx_device->HWOp.Cntl)
		hdmitx_device->HWOp.Cntl(hdmitx_device,
			HDMITX_IP_INTR_MASN_RST, 0);
	while (hdmitx_device->hpd_event != 0xff) {
		if (hdmitx_device->mux_hpd_if_pin_high_flag) {
			if (hdmitx_device->HWOp.Cntl)
				hdmitx_device->HWOp.Cntl(hdmitx_device,
				HDMITX_HWCMD_MUX_HPD_IF_PIN_HIGH, 0);
	}
	if (hdmitx_device->HWOp.Cntl) {
		static int st;
		st = hdmitx_device->HWOp.CntlMisc(hdmitx_device,
			MISC_HPD_GPI_ST, 0);
wait:
		if (hdmitx_device->hpd_lock == 1) {
			msleep_interruptible(2000);
			goto wait;
		}
		if ((st == 0) && (hdmitx_device->hpd_state == 1))
			hdmitx_device->hpd_event = 2;
		if ((st == 1) && (hdmitx_device->hpd_state == 0))
			hdmitx_device->hpd_event = 1;
/* Check audio status */
#ifndef CONFIG_AML_HDMI_TX_HDCP
		if ((hdmitx_device->cur_VIC != HDMI_Unkown)
			&& (!(hdmitx_device->HWOp.GetState(
				hdmitx_device, STAT_AUDIO_PACK, 0)))
			&& (tv_audio_support(
				hdmitx_device->cur_audio_param.type,
				&hdmitx_device->RXCap))) {
			hdmitx_device->HWOp.CntlConfig(hdmitx_device,
				CONF_AUDIO_MUTE_OP, AUDIO_UNMUTE);
		}
#endif
	}

	if (hdmitx_device->hpd_event == 1) {
		hdmitx_device->hpd_event = 0;
		hdmitx_device->hpd_state = 1;
/*TODO		hdmitx_edid_ram_buffer_clear(hdmitx_device);*/
		hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_PIN_MUX_OP, PIN_MUX);
		hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_RESET_EDID, 0);
		/* start reading edid frist time */
		hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_EDID_READ_DATA, 0);
		msleep(200);	/* wait 200ms to read edid */

		/* hardware i2c read fail */
		if (!(hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_IS_EDID_DATA_READY, 0))) {
			hdmi_print(ERR, EDID "edid failed\n");
		hdmitx_device->tv_no_edid = 1;
		/* read edid again */
		hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_RESET_EDID, 0);
		/* start reading edid second time */
		hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_EDID_READ_DATA, 0);
		if (!(hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_IS_EDID_DATA_READY, 0))) {
			hdmi_print(ERR, EDID "edid failed\n");
			hdmitx_device->tv_no_edid = 1;
		} else
			goto edid_op;
		} else {
edid_op:
		/* save edid raw data to EDID_buf1[] */
		hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_EDID_GET_DATA, 1);
		hdmi_print(IMP, EDID "edid ready\n");
		/* read edid again */
				hdmitx_device->cur_edid_block = 0;
				hdmitx_device->cur_phy_block_ptr = 0;
		hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_RESET_EDID, 0);
		/* start reading edid second time */
		hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_EDID_READ_DATA, 0);
		msleep(200);
		if (hdmitx_device->HWOp.CntlDDC(hdmitx_device,
			DDC_IS_EDID_DATA_READY, 0)) {
			/* save edid raw data to EDID_buf[] */
			hdmitx_device->HWOp.CntlDDC(hdmitx_device,
				DDC_EDID_GET_DATA, 0);
			hdmi_print(IMP, EDID "edid ready\n");
		} else {
			hdmi_print(ERR, EDID "edid failed\n");
			hdmitx_device->tv_no_edid = 1;
		}
		/* compare EDID_buf & EDID_buf1 */
		hdmitx_edid_buf_compare_print(hdmitx_device);
		hdmitx_edid_clear(hdmitx_device);
		hdmitx_edid_parse(hdmitx_device);
		hdmitx_device->tv_no_edid = 0;
		}
		set_disp_mode_auto();
		hdmitx_set_audio(hdmitx_device,
			&(hdmitx_device->cur_audio_param), hdmi_ch);
		switch_set_state(&sdev, 1);
		cec_node_init(hdmitx_device);
	}
	if (hdmitx_device->hpd_event == 2) {
		hdmitx_device->hpd_event = 0;
		hdmitx_device->hpd_state = 0;
		hdmitx_device->cur_VIC = HDMI_Unkown;
		hdmitx_device->HWOp.CntlConfig(hdmitx_device,
			CONF_CLR_AVI_PACKET, 0);
		hdmitx_device->HWOp.CntlConfig(hdmitx_device,
			CONF_CLR_VSDB_PACKET, 0);
		/* if VIID PLL is using, then disable HPLL */
		#if 0
		if (hdmitx_device->HWOp.CntlMisc(hdmitx_device,
			MISC_VIID_IS_USING, 0)) {
			hdmitx_device->HWOp.CntlMisc(hdmitx_device,
				MISC_HPLL_OP, HPLL_DISABLE);
		}
		#endif
		hdmitx_device->HWOp.CntlDDC(hdmitx_device, DDC_HDCP_OP,
			HDCP14_OFF);
		hdmitx_device->HWOp.CntlMisc(hdmitx_device,
			MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
		hdmitx_device->HWOp.Cntl(hdmitx_device,
			HDMITX_IP_SW_RST, TX_SYS_SW_RST);
		hdmitx_edid_clear(hdmitx_device);
/* When unplug hdmi, clear the hdmitx module edid ram and edid buffer.*/
/*TODO		hdmitx_edid_ram_buffer_clear(hdmitx_device);*/
		if (hdmitx_device->unplug_powerdown) {
			hdmitx_set_display(hdmitx_device, HDMI_Unkown);
		if (hdmitx_device->HWOp.Cntl)
			hdmitx_device->HWOp.Cntl(hdmitx_device,
				HDMITX_HWCMD_TURNOFF_HDMIHW,
				(hpdmode != 0)?1:0);
		}
		hdmitx_device->tv_cec_support = 0;
		switch_set_state(&sdev, 0);
		hdmitx_device->vic_count = 0;
	}
	msleep_interruptible(100);
	}
	return 0;
}

/* Linux */
/*****************************
*	hdmitx driver file_operations
*
******************************/
static int amhdmitx_open(struct inode *node, struct file *file)
{
	struct hdmitx_dev *hdmitx_in_devp;

	/* Get the per-device structure that contains this cdev */
	hdmitx_in_devp = container_of(node->i_cdev, struct hdmitx_dev, cdev);
	file->private_data = hdmitx_in_devp;

	return 0;

}


static int amhdmitx_release(struct inode *node, struct file *file)
{
	/* struct hdmitx_dev *hdmitx_in_devp = file->private_data; */

	/* Reset file pointer */

	/* Release some other fields */
	/* ... */
	return 0;
}


#if 0
static int amhdmitx_ioctl(struct inode *node, struct file *file,
	unsigned int cmd,   unsigned long args)
{
	int   r = 0;
	switch (cmd) {
	default:
		break;
	}
	return r;
}
#endif

static const struct file_operations amhdmitx_fops = {
	.owner	= THIS_MODULE,
	.open	 = amhdmitx_open,
	.release  = amhdmitx_release,
/* .ioctl	= amhdmitx_ioctl, */
};

struct hdmitx_dev *get_hdmitx_device(void)
{
	return &hdmitx_device;
}
EXPORT_SYMBOL(get_hdmitx_device);

int get_hpd_state(void)
{
	return hdmitx_device.hpd_state;
}
EXPORT_SYMBOL(get_hpd_state);

static int get_dt_vend_init_data(struct device_node *np,
	struct vendor_info_data *vend)
{
	int ret;

	ret = of_property_read_string(np, "vendor_name",
		(const char **)&(vend->vendor_name));
	if (ret) {
		hdmi_print(INF, SYS "not find vendor name\n");
		return 1;
	}

	ret = of_property_read_u32(np, "vendor_id", &(vend->vendor_id));
	if (ret) {
		hdmi_print(INF, SYS "not find vendor id\n");
		return 1;
	}

	ret = of_property_read_string(np, "product_desc",
		(const char **)&(vend->product_desc));
	if (ret) {
		hdmi_print(INF, SYS "not find product desc\n");
		return 1;
	}

	ret = of_property_read_string(np, "cec_osd_string",
		(const char **)&(vend->cec_osd_string));
	if (ret) {
		hdmi_print(INF, SYS "not find cec osd string\n");
		return 1;
	}

	ret = of_property_read_u32(np, "cec_config",
		&(vend->cec_config));
	if (ret) {
		hdmi_print(INF, SYS "not find cec config\n");
		return 1;
	}
	ret = of_property_read_u32(np, "ao_cec", &(vend->ao_cec));
	if (ret) {
		hdmi_print(INF, SYS "not find ao cec\n");
		return 1;
	}
	return 0;
}

static int pwr_type_match(struct device_node *np, const char *str,
	int idx, struct hdmi_pwr_ctl *pwr, char *pwr_col)
{
	static char * const pwr_types_id[] = {"none", "cpu", "pmu",
		NULL};	 /* match with dts file */
	int i = 0;
	int ret = 0;

	struct pwr_ctl_var *var = (struct pwr_ctl_var *)pwr;

	while (pwr_types_id[i]) {
		if (strcasecmp(pwr_types_id[i], str) == 0) {
			ret = 1;
			break;
		}
		i++;
	}

	var += idx;

	switch (i) {
	case CPU_GPO:
	var->type = CPU_GPO;
	ret = of_property_read_string_index(np, pwr_col, 1, &str);
	if (!ret) {
		hdmi_gd = gpiod_get(hdmi_dev, str);
		if (!IS_ERR(hdmi_gd)) {
			ret = of_property_read_string_index(np,
				pwr_col, 2, &str);
			if (!ret)
				var->var.gpo.val =
					(strcmp(str, "H") == 0);
		}
	}
	break;
	case PMU:
	var->type = PMU;
/* TODO later */
	break;
	default:
	var->type = NONE;
	};
	return ret;
}

static int get_dt_pwr_init_data(struct device_node *np,
	struct hdmi_pwr_ctl *pwr)
{
	int ret = 0;
	int idx = 0;
	const char *str = NULL;
	char *hdmi_pwr_string[] = {"pwr_5v_on", "pwr_5v_off",
		"pwr_3v3_on", "pwr_3v3_off", "pwr_hpll_vdd_on",
		"pwr_hpll_vdd_off", NULL};  /* match with dts file */

	while (hdmi_pwr_string[idx]) {
		ret = of_property_read_string_index(np,
			hdmi_pwr_string[idx], 0, &str);
		if (!ret)
			pwr_type_match(np, str, idx, pwr,
				hdmi_pwr_string[idx]);
		idx++;
	}

	if (np != NULL)
		ret = of_property_read_u32(np, "pwr_level",
			&pwr->pwr_level);
#if 0
	struct pwr_ctl_var (*var)[HDMI_TX_PWR_CTRL_NUM] =
		(struct pwr_ctl_var (*)[HDMI_TX_PWR_CTRL_NUM])pwr;
	for (idx = 0; idx < HDMI_TX_PWR_CTRL_NUM; idx++) {
		hdmi_print(INF, SYS "%d %d %d\n", var[idx]->type,
			var[idx]->var.gpo.pin, var[idx]->var.gpo.val);
		return 1;
	}
#endif
	return 0;
}

static void hdmitx_pwr_init(struct hdmi_pwr_ctl *ctl)
{
	if (ctl) {
		if (ctl->pwr_5v_on.type == CPU_GPO)
			gpiod_direction_output(hdmi_gd,
					       ctl->pwr_5v_on.var.gpo.val);
		if (ctl->pwr_3v3_on.type == CPU_GPO)
			gpiod_direction_output(hdmi_gd,
					       ctl->pwr_3v3_on.var.gpo.val);
		if (ctl->pwr_hpll_vdd_on.type == CPU_GPO)
			gpiod_direction_output(hdmi_gd,
					ctl->pwr_hpll_vdd_on.var.gpo.val);
	}
}

static int amhdmitx_probe(struct platform_device *pdev)
{
/* extern struct switch_dev lang_dev; */
	int r, ret = 0;

#ifdef CONFIG_USE_OF
	int psize, val;
	phandle phandle;
	struct device_node *init_data;
#endif
	hdmi_dev = &pdev->dev;
	hdmi_print(IMP, SYS "amhdmitx_probe\n");

	r = alloc_chrdev_region(&hdmitx_id, 0, HDMI_TX_COUNT,
		DEVICE_NAME);
	if (r < 0) {
		hdmi_print(INF, SYS
			"Can't register major for amhdmitx device\n");
		return r;
	}

	hdmitx_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(hdmitx_class)) {
		unregister_chrdev_region(hdmitx_id, HDMI_TX_COUNT);
		return -1;
		/* return PTR_ERR(aoe_class); */
	}

	hdmitx_device.unplug_powerdown = 0;
	hdmitx_device.vic_count = 0;
	hdmitx_device.auth_process_timer = 0;
	hdmitx_device.force_audio_flag = 0;
	hdmitx_device.tv_cec_support = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&hdmitx_early_suspend_handler);
#endif

	if ((init_flag&INIT_FLAG_POWERDOWN) && (hpdmode == 2))
		hdmitx_device.mux_hpd_if_pin_high_flag = 0;
	else
		hdmitx_device.mux_hpd_if_pin_high_flag = 1;
	hdmitx_device.audio_param_update_flag = 0;
	cdev_init(&(hdmitx_device.cdev), &amhdmitx_fops);
	hdmitx_device.cdev.owner = THIS_MODULE;
	cdev_add(&(hdmitx_device.cdev), hdmitx_id, HDMI_TX_COUNT);

	hdmitx_dev = device_create(hdmitx_class, NULL, hdmitx_id, NULL,
		"amhdmitx%d", 0); /* kernel>=2.6.27 */

	ret = device_create_file(hdmitx_dev, &dev_attr_disp_mode);
	ret = device_create_file(hdmitx_dev, &dev_attr_aud_mode);
	ret = device_create_file(hdmitx_dev, &dev_attr_edid);
	ret = device_create_file(hdmitx_dev, &dev_attr_config);
	ret = device_create_file(hdmitx_dev, &dev_attr_debug);
	ret = device_create_file(hdmitx_dev, &dev_attr_disp_cap);
	ret = device_create_file(hdmitx_dev, &dev_attr_disp_cap_3d);
	ret = device_create_file(hdmitx_dev, &dev_attr_aud_cap);
	ret = device_create_file(hdmitx_dev, &dev_attr_aud_ch);
	ret = device_create_file(hdmitx_dev, &dev_attr_hdcp_ksv_info);
	ret = device_create_file(hdmitx_dev, &dev_attr_hpd_state);
	ret = device_create_file(hdmitx_dev, &dev_attr_support_3d);
	ret = device_create_file(hdmitx_dev, &dev_attr_cec);
	ret = device_create_file(hdmitx_dev, &dev_attr_cec_config);
	ret = device_create_file(hdmitx_dev, &dev_attr_cec_lang_config);

	if (hdmitx_dev == NULL) {
		hdmi_print(ERR, SYS "device_create create error\n");
		class_destroy(hdmitx_class);
		r = -EEXIST;
		return r;
	}
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	register_hdmi_edid_supported_func(hdmitx_is_vmode_supported);
#endif
	vout_register_client(&hdmitx_notifier_nb_v);
#ifdef CONFIG_AM_TV_OUTPUT2
	vout2_register_client(&hdmitx_notifier_nb_v2);
#endif
	aout_register_client(&hdmitx_notifier_nb_a);
	r = r ? (int)&hdmitx_notifier_nb_a : (int)&hdmitx_notifier_nb_a;

#ifdef CONFIG_USE_OF
	if (pdev->dev.of_node) {
		memset(&hdmitx_device.config_data, 0,
			sizeof(struct hdmi_config_platform_data));
/* Get physical setting data */
	ret = of_property_read_u32(pdev->dev.of_node, "phy-size", &psize);
	if (!ret) {
		hdmitx_device.config_data.phy_data = kzalloc(sizeof
			(struct hdmi_phy_set_data)*psize, GFP_KERNEL);
		if (!hdmitx_device.config_data.phy_data) {
			hdmi_print(INF, SYS
				"can not get phy_data mem\n");
		} else {
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"phy-data",
			(unsigned int *)
			(hdmitx_device.config_data.phy_data),
			(sizeof(struct hdmi_phy_set_data))*psize/sizeof
			(struct hdmi_phy_set_data *));
		if (ret)
			hdmi_print(INF, SYS "not find match psize\n");
		}
	}
/* Get vendor information */
		ret = of_property_read_u32(pdev->dev.of_node,
			"vend-data", &val);
	if (ret)
		hdmi_print(INF, SYS "not find match init-data\n");
	if (ret == 0) {
		phandle = val;
		init_data = of_find_node_by_phandle(phandle);
		if (!init_data)
			hdmi_print(INF, SYS "not find device node\n");
		hdmitx_device.config_data.vend_data = kzalloc(
			sizeof(struct vendor_info_data), GFP_KERNEL);
		if (!hdmitx_device.config_data.vend_data)
			hdmi_print(INF, SYS
				"can not get vend_data mem\n");
		ret = get_dt_vend_init_data(init_data,
			hdmitx_device.config_data.vend_data);
		if (ret)
			hdmi_print(INF, SYS"not find vend_init_data\n");
	}
/* Get power control */
		ret = of_property_read_u32(pdev->dev.of_node,
			"pwr-ctrl", &val);
	if (ret)
		hdmi_print(INF, SYS "not find match pwr-ctl\n");
	if (ret == 0) {
		phandle = val;
		init_data = of_find_node_by_phandle(phandle);
		if (!init_data)
			hdmi_print(INF, SYS "not find device node\n");
		hdmitx_device.config_data.pwr_ctl = kzalloc((sizeof(
			struct hdmi_pwr_ctl)) * HDMI_TX_PWR_CTRL_NUM,
			GFP_KERNEL);
		if (!hdmitx_device.config_data.pwr_ctl)
			hdmi_print(INF, SYS"can not get pwr_ctl mem\n");
		memset(hdmitx_device.config_data.pwr_ctl, 0,
			sizeof(struct hdmi_pwr_ctl));
		ret = get_dt_pwr_init_data(init_data,
			hdmitx_device.config_data.pwr_ctl);
		if (ret)
			hdmi_print(INF, SYS "not find pwr_ctl\n");
	}
	}
/* open hdmi power */
	hdmitx_pwr_init(hdmitx_device.config_data.pwr_ctl);

#else
	hdmi_pdata = pdev->dev.platform_data;
	if (!hdmi_pdata) {
		hdmi_print(INF, SYS "not get platform data\n");
		r = -ENOENT;
	} else {
		hdmi_print(INF, SYS "get hdmi platform data\n");
	}
#endif

	hdmitx_device.irq_hpd = platform_get_irq_byname(pdev, "hdmitx_hpd");
	if (hdmitx_device.irq_hpd == -ENXIO) {
		pr_err("%s: ERROR: hdmitx hpd irq No not found\n" ,
			__func__);
		return -ENXIO;
	}
	pr_info("hdmitx hpd irq = %d\n" , hdmitx_device.irq_hpd);
	hdmitx_device.irq_cec = platform_get_irq_byname(pdev, "hdmitx_cec");
	if (hdmitx_device.irq_cec == -ENXIO) {
		pr_err("%s: ERROR: hdmitx cec irq No not found\n" ,
			__func__);
		return -ENXIO;
	}
	pr_info("hdmitx cec irq = %d\n" , hdmitx_device.irq_cec);
	hdmitx_device.clk_sys = clk_get(&pdev->dev, "hdmitx_clk_sys");
	clk_prepare_enable(hdmitx_device.clk_sys);

	hdmitx_device.clk_encp = clk_get(&pdev->dev, "hdmitx_clk_encp");
	clk_prepare_enable(hdmitx_device.clk_encp);

	hdmitx_device.clk_enci = clk_get(&pdev->dev, "hdmitx_clk_enci");
	clk_prepare_enable(hdmitx_device.clk_enci);

	hdmitx_device.clk_pixel = clk_get(&pdev->dev, "hdmitx_clk_pixel");
	clk_prepare_enable(hdmitx_device.clk_pixel);

	hdmitx_device.clk_phy = clk_get(&pdev->dev, "hdmitx_clk_phy");
	clk_prepare_enable(hdmitx_device.clk_phy);

	switch_dev_register(&sdev);
/* switch_dev_register(&lang_dev); */

	hdmitx_init_parameters(&hdmitx_device.hdmi_info);
	HDMITX_Meson_Init(&hdmitx_device);
	hdmitx_device.task = kthread_run(hdmi_task_handle,
		&hdmitx_device, "kthread_hdmi");

	if (r < 0) {
		hdmi_print(INF, SYS "register switch dev failed\n");
		return r;
	}
	return r;
}

static int amhdmitx_remove(struct platform_device *pdev)
{
	switch_dev_unregister(&sdev);

	if (hdmitx_device.HWOp.UnInit)
		hdmitx_device.HWOp.UnInit(&hdmitx_device);
	hdmitx_device.hpd_event = 0xff;
	kthread_stop(hdmitx_device.task);

/* vout_unregister_client(&hdmitx_notifier_nb_v); */
#ifdef CONFIG_AM_TV_OUTPUT2
	vout2_unregister_client(&hdmitx_notifier_nb_v2);
#endif
	aout_unregister_client(&hdmitx_notifier_nb_a);

	/* Remove the cdev */
	device_remove_file(hdmitx_dev, &dev_attr_disp_mode);
	device_remove_file(hdmitx_dev, &dev_attr_aud_mode);
	device_remove_file(hdmitx_dev, &dev_attr_edid);
	device_remove_file(hdmitx_dev, &dev_attr_config);
	device_remove_file(hdmitx_dev, &dev_attr_debug);
	device_remove_file(hdmitx_dev, &dev_attr_disp_cap);
	device_remove_file(hdmitx_dev, &dev_attr_disp_cap_3d);
	device_remove_file(hdmitx_dev, &dev_attr_hpd_state);
	device_remove_file(hdmitx_dev, &dev_attr_support_3d);
	device_remove_file(hdmitx_dev, &dev_attr_cec);

	cdev_del(&hdmitx_device.cdev);

	device_destroy(hdmitx_class, hdmitx_id);

	class_destroy(hdmitx_class);

/* TODO */
/* kfree(hdmi_pdata->phy_data); */
/* kfree(hdmi_pdata); */

	unregister_chrdev_region(hdmitx_id, HDMI_TX_COUNT);
	return 0;
}

#ifdef CONFIG_PM
static int amhdmitx_suspend(struct platform_device *pdev,
	pm_message_t state)
{
#if 0
	pr_info("amhdmitx: hdmirx_suspend\n");
	hdmitx_pre_display_init();
	if (hdmi_pdata) {
		hdmi_pdata->hdmi_5v_ctrl ?
			hdmi_pdata->hdmi_5v_ctrl(0) : 0;
	/* prevent Voff leak current */
		hdmi_pdata->hdmi_3v3_ctrl ?
			hdmi_pdata->hdmi_3v3_ctrl(1) : 0;
	}
	if (hdmitx_device.HWOp.Cntl)
		hdmitx_device.HWOp.CntlMisc(&hdmitx_device,
			MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
#endif
	return 0;
}

static int amhdmitx_resume(struct platform_device *pdev)
{
#if 0
	pr_info("amhdmitx: resume module\n");
	if (hdmi_pdata) {
		hdmi_pdata->hdmi_5v_ctrl ?
			hdmi_pdata->hdmi_5v_ctrl(1) : 0;
	}
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_VIDEO_BLANK_OP, VIDEO_UNBLANK);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
		DDC_HDCP_OP, HDCP_OFF);
	hdmitx_device.internal_mode_change = 0;
	set_disp_mode_auto();
	pr_info("amhdmitx: resume module %d\n", __LINE__);
#endif
	return 0;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id meson_amhdmitx_dt_match[] = {
	{
	.compatible	 = "amlogic, amhdmitx",
	},
	{},
};
#else
#define meson_amhdmitx_dt_match NULL
#endif
static struct platform_driver amhdmitx_driver = {
	.probe	  = amhdmitx_probe,
	.remove	 = amhdmitx_remove,
#ifdef CONFIG_PM
	.suspend	= amhdmitx_suspend,
	.resume	 = amhdmitx_resume,
#endif
	.driver	 = {
	.name   = DEVICE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = meson_amhdmitx_dt_match,
	}
};



static int  __init amhdmitx_init(void)
{
	if (init_flag&INIT_FLAG_NOT_LOAD)
		return 0;

	hdmi_print(IMP, SYS "amhdmitx_init\n");
	hdmi_print(IMP, SYS "Ver: %s\n", HDMITX_VER);

	if (platform_driver_register(&amhdmitx_driver)) {
		hdmi_print(ERR, SYS
			"failed to register amhdmitx module\n");
#if 0
	platform_device_del(amhdmi_tx_device);
	platform_device_put(amhdmi_tx_device);
#endif
	return -ENODEV;
	}
	return 0;
}




static void __exit amhdmitx_exit(void)
{
	hdmi_print(INF, SYS "amhdmitx_exit\n");
	platform_driver_unregister(&amhdmitx_driver);
/* \\	platform_device_unregister(amhdmi_tx_device); */
/* \\	amhdmi_tx_device = NULL; */
	return;
}

/* module_init(amhdmitx_init); */
arch_initcall(amhdmitx_init);
module_exit(amhdmitx_exit);

MODULE_DESCRIPTION("AMLOGIC HDMI TX driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

/* besides characters defined in seperator, '\"' are used as seperator;
 * and any characters in '\"' will not act as seperator
 */
static char *next_token_ex(char *seperator, char *buf, unsigned size,
	unsigned offset, unsigned *token_len, unsigned *token_offset)
{
	char *pToken = NULL;
	char last_seperator = 0;
	char trans_char_flag = 0;
	if (buf) {
		for (; offset < size; offset++) {
			int ii = 0;
		char ch;
		if (buf[offset] == '\\') {
			trans_char_flag = 1;
			continue;
		}
		while (((ch = seperator[ii++]) != buf[offset]) && (ch))
			;
		if (ch) {
			if (!pToken) {
				continue;
		} else {
			if (last_seperator != '"') {
				*token_len = (unsigned)
					(buf + offset - pToken);
				*token_offset = offset;
				return pToken;
			}
		}
		} else if (!pToken) {
			if (trans_char_flag && (buf[offset] == '"'))
				last_seperator = buf[offset];
			pToken = &buf[offset];
		} else if ((trans_char_flag && (buf[offset] == '"'))
				&& (last_seperator == '"')) {
			*token_len = (unsigned)(buf + offset - pToken - 2);
			*token_offset = offset + 1;
			return pToken + 1;
		}
		trans_char_flag = 0;
	}
	if (pToken) {
		*token_len = (unsigned)(buf + offset - pToken);
		*token_offset = offset;
	}
	}
	return pToken;
}

static  int __init hdmitx_boot_para_setup(char *s)
{
	char separator[] = {' ', ',', ';', 0x0};
	char *token;
	unsigned token_len, token_offset, offset = 0;
	int size = strlen(s);
	unsigned long list;
	int ret = 0;

	do {
		token = next_token_ex(separator, s, size, offset,
				&token_len, &token_offset);
	if (token) {
		if ((token_len == 3)
			&& (strncmp(token, "off", token_len) == 0)) {
			init_flag |= INIT_FLAG_NOT_LOAD;
		} else if (strncmp(token, "cec", 3) == 0) {
			ret = kstrtoul(token+3, 16, &list);
		if ((list >= 0) && (list <= 0xf))
			hdmitx_device.cec_func_config = list;
		hdmi_print(INF, CEC "HDMI hdmi_cec_func_config:0x%x\n",
			hdmitx_device.cec_func_config);
		}
	}
		offset = token_offset;
	} while (token);
	return 0;
}

__setup("hdmitx=", hdmitx_boot_para_setup);

#ifdef CONFIG_AM_TV_OUTPUT2
MODULE_PARM_DESC(force_vout_index, "\n force_vout_index\n");
module_param(force_vout_index, uint, 0664);
#endif

MODULE_PARM_DESC(hdmi_detect_when_booting, "\n hdmi_detect_when_booting\n");
module_param(hdmi_detect_when_booting, int, 0664);


MODULE_PARM_DESC(hdmi_480p_force_clk, "\n hdmi_480p_force_clk\n");
module_param(hdmi_480p_force_clk, int, 0664);

MODULE_PARM_DESC(hdmi_prbs_mode, "\n hdmi_prbs_mode\n");
module_param(hdmi_prbs_mode, int, 0664);

MODULE_PARM_DESC(debug_level, "\n debug_level\n");
module_param(debug_level, int, 0664);
