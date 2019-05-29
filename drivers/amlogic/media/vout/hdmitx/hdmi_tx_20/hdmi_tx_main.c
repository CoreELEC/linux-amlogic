/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdmi_tx_main.c
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
#include <linux/pinctrl/consumer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/ctype.h>
#include <linux/extcon.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/extcon.h>
#include <linux/i2c.h>

#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_SND_SOC
#include <linux/amlogic/media/sound/aout_notify.h>
#endif
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ddc.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_config.h>
#include "hw/tvenc_conf.h"
#include "hw/common.h"
#include "hw/hw_clk.h"
#include "hdmi_tx_hdcp.h"

#define DEVICE_NAME "amhdmitx"
#define HDMI_TX_COUNT 32
#define HDMI_TX_POOL_NUM  6
#define HDMI_TX_RESOURCE_NUM 4
#define HDMI_TX_PWR_CTRL_NUM	6

static struct class *hdmitx_class;
static int set_disp_mode_auto(void);
static void hdmitx_get_edid(struct hdmitx_dev *hdev);
static void hdmitx_set_drm_pkt(struct master_display_info_s *data);
static void hdmitx_set_vsif_pkt(enum eotf_type type, enum mode_type
	tunnel_mode, struct dv_vsif_para *data);
static void hdmitx_set_hdr10plus_pkt(unsigned int flag,
	struct hdr10plus_para *data);
static void hdmitx_set_emp_pkt(unsigned char *data,
	unsigned int type, unsigned int size);
static int check_fbc_special(unsigned char *edid_dat);
static struct vinfo_s *hdmitx_get_current_vinfo(void);

static DEFINE_MUTEX(setclk_mutex);
static DEFINE_MUTEX(getedid_mutex);

static struct hdmitx_dev hdmitx_device = {
	.frac_rate_policy = 1,
};

struct vout_device_s hdmitx_vdev = {
	.dv_info = &(hdmitx_device.RXCap.dv_info),
	.fresh_tx_hdr_pkt = hdmitx_set_drm_pkt,
	.fresh_tx_vsif_pkt = hdmitx_set_vsif_pkt,
	.fresh_tx_hdr10plus_pkt = hdmitx_set_hdr10plus_pkt,
	.fresh_tx_emp_pkt = hdmitx_set_emp_pkt,
};

struct hdmi_config_platform_data *hdmi_pdata;

static const unsigned int hdmi_cable[] = {
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};

struct extcon_dev *hdmitx_extcon_hdmi;
struct extcon_dev *hdmitx_extcon_audio;
struct extcon_dev *hdmitx_extcon_power;
struct extcon_dev *hdmitx_extcon_hdr;
struct extcon_dev *hdmitx_extcon_rxsense;
struct extcon_dev *hdmitx_extcon_hdcp;

void control_hdmiphy(bool on)
{
	if (on)
		hdmitx_device.HWOp.CntlMisc(&hdmitx_device,
			MISC_TMDS_PHY_OP,
			TMDS_PHY_ENABLE);
	else
		hdmitx_device.HWOp.CntlMisc(&hdmitx_device,
			MISC_TMDS_PHY_OP,
			TMDS_PHY_DISABLE);
}

static struct hdmi_cea_timing custom_timing;
struct hdmi_cea_timing *get_custom_timing(void)
{
	return &custom_timing;
}
EXPORT_SYMBOL(get_custom_timing);

static inline void hdmitx_notify_hpd(int hpd)
{
	if (hpd)
		hdmitx_event_notify(HDMITX_PLUG, NULL);
	else
		hdmitx_event_notify(HDMITX_UNPLUG, NULL);
}
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static void hdmitx_early_suspend(struct early_suspend *h)
{
	struct hdmitx_dev *phdmi = (struct hdmitx_dev *)h->param;

	phdmi->ready = 0;
	phdmi->hpd_lock = 1;
	msleep(20);
	phdmi->HWOp.CntlMisc(phdmi, MISC_AVMUTE_OP, SET_AVMUTE);
	mdelay(100);
	pr_info(SYS "HDMITX: early suspend\n");
	phdmi->HWOp.Cntl((struct hdmitx_dev *)h->param,
		HDMITX_EARLY_SUSPEND_RESUME_CNTL, HDMITX_EARLY_SUSPEND);
	phdmi->cur_VIC = HDMI_Unknown;
	phdmi->output_blank_flag = 0;
	phdmi->HWOp.CntlDDC(phdmi, DDC_HDCP_MUX_INIT, 1);
	phdmi->HWOp.CntlDDC(phdmi, DDC_HDCP_OP, HDCP14_OFF);
	extcon_set_state_sync(hdmitx_extcon_power, EXTCON_DISP_HDMI, 0);
	phdmi->HWOp.CntlConfig(&hdmitx_device, CONF_CLR_AVI_PACKET, 0);
	phdmi->HWOp.CntlConfig(&hdmitx_device, CONF_CLR_VSDB_PACKET, 0);
	/*close vpu clk*/
	if (phdmi->hdmitx_clk_tree.hdmi_clk_vapb != NULL)
		clk_disable_unprepare(phdmi->hdmitx_clk_tree.hdmi_clk_vapb);
	if (phdmi->hdmitx_clk_tree.hdmi_clk_vpu != NULL)
		clk_disable_unprepare(phdmi->hdmitx_clk_tree.hdmi_clk_vpu);
}

static int hdmitx_is_hdmi_vmode(char *mode_name)
{
	enum hdmi_vic vic = hdmitx_edid_vic_tab_map_vic(mode_name);

	if (vic == HDMI_Unknown)
		return 0;

	return 1;
}

static void hdmitx_late_resume(struct early_suspend *h)
{
	const struct vinfo_s *info = hdmitx_get_current_vinfo();
	struct hdmitx_dev *phdmi = (struct hdmitx_dev *)h->param;

	/*open vpu clk*/
	if (phdmi->hdmitx_clk_tree.hdmi_clk_vapb != NULL)
		clk_prepare_enable(phdmi->hdmitx_clk_tree.hdmi_clk_vapb);

	if (phdmi->hdmitx_clk_tree.hdmi_clk_vpu != NULL)
		clk_prepare_enable(phdmi->hdmitx_clk_tree.hdmi_clk_vpu);

	if (info && (hdmitx_is_hdmi_vmode(info->name) == 1))
		phdmi->HWOp.CntlMisc(&hdmitx_device, MISC_HPLL_FAKE, 0);

	phdmi->hpd_lock = 0;

	/* update status for hpd and switch/state */
	hdmitx_device.hpd_state = !!(hdmitx_device.HWOp.CntlMisc(&hdmitx_device,
		MISC_HPD_GPI_ST, 0));

	pr_info("hdmitx hpd state: %d\n", hdmitx_device.hpd_state);
	hdmitx_notify_hpd(hdmitx_device.hpd_state);

	/*force to get EDID after resume for Amplifer Power case*/
	if (hdmitx_device.hpd_state)
		hdmitx_get_edid(phdmi);

	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	set_disp_mode_auto();

	extcon_set_state_sync(hdmitx_extcon_hdmi, EXTCON_DISP_HDMI,
		hdmitx_device.hpd_state);
	extcon_set_state_sync(hdmitx_extcon_power, EXTCON_DISP_HDMI, 1);
	extcon_set_state_sync(hdmitx_extcon_audio, EXTCON_DISP_HDMI,
		hdmitx_device.hpd_state);

	pr_info("amhdmitx: late resume module %d\n", __LINE__);
	phdmi->HWOp.Cntl((struct hdmitx_dev *)h->param,
		HDMITX_EARLY_SUSPEND_RESUME_CNTL, HDMITX_LATE_RESUME);
	pr_info(SYS "late resume\n");
}

/* Set avmute_set signal to HDMIRX */
static int hdmitx_reboot_notifier(struct notifier_block *nb,
	unsigned long action, void *data)
{
	struct hdmitx_dev *hdev = container_of(nb, struct hdmitx_dev, nb);

	hdev->ready = 0;
	hdev->HWOp.CntlMisc(hdev, MISC_AVMUTE_OP, SET_AVMUTE);
	mdelay(100);
	hdev->HWOp.CntlMisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	hdev->HWOp.Cntl(hdev, HDMITX_EARLY_SUSPEND_RESUME_CNTL,
		HDMITX_EARLY_SUSPEND);

	return NOTIFY_OK;
}

static struct early_suspend hdmitx_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10,
	.suspend = hdmitx_early_suspend,
	.resume = hdmitx_late_resume,
	.param = &hdmitx_device,
};
#endif

#define INIT_FLAG_VDACOFF		0x1
	/* unplug powerdown */
#define INIT_FLAG_POWERDOWN	  0x2

#define INIT_FLAG_NOT_LOAD 0x80

static unsigned char init_flag;
#undef DISABLE_AUDIO

int get_cur_vout_index(void)
/*
 * return value: 1, vout; 2, vout2;
 */
{
	int vout_index = 1;
	return vout_index;
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
	else
		;/* nothing */

	if (strncmp(mode, "1080p60hz", strlen("1080p60hz")) == 0)
		vic = HDMI_1080p60;
	if (strncmp(mode, "1080p50hz", strlen("1080p50hz")) == 0)
		vic = HDMI_1080p50;

	if (vic != HDMI_Unknown) {
		hdmitx_device.mux_hpd_if_pin_high_flag = 1;
		if (hdmitx_device.vic_count == 0) {
			if (hdmitx_device.unplug_powerdown)
				return 0;
		}
	}

	hdmitx_device.cur_VIC = HDMI_Unknown;
	ret = hdmitx_set_display(&hdmitx_device, vic);
	if (ret >= 0) {
		hdmitx_device.HWOp.Cntl(&hdmitx_device,
			HDMITX_AVMUTE_CNTL, AVMUTE_CLEAR);
		hdmitx_device.cur_VIC = vic;
		hdmitx_device.audio_param_update_flag = 1;
		hdmitx_device.auth_process_timer = AUTH_PROCESS_TIME;
	}

	if (hdmitx_device.cur_VIC == HDMI_Unknown) {
		if (hdmitx_device.hpdmode == 2) {
			/* edid will be read again when hpd is muxed and
			 * it is high
			 */
			hdmitx_edid_clear(&hdmitx_device);
			hdmitx_device.mux_hpd_if_pin_high_flag = 0;
		}
		if (hdmitx_device.HWOp.Cntl) {
			hdmitx_device.HWOp.Cntl(&hdmitx_device,
				HDMITX_HWCMD_TURNOFF_HDMIHW,
				(hdmitx_device.hpdmode == 2)?1:0);
		}
	}
	return ret;
}

static void hdmitx_pre_display_init(void)
{
	hdmitx_device.cur_VIC = HDMI_Unknown;
	hdmitx_device.auth_process_timer = AUTH_PROCESS_TIME;
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_MUX_INIT, 1);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_OP, HDCP14_OFF);
	/* msleep(10); */
	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_TMDS_PHY_OP,
		TMDS_PHY_DISABLE);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_CLR_AVI_PACKET, 0);
	hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
		CONF_CLR_VSDB_PACKET, 0);
}

/* fr_tab[]
 * 1080p24hz, 24:1
 * 1080p23.976hz, 2997:125
 * 25/50/100/200hz, no change
 */
static struct frac_rate_table fr_tab[] = {
	{"24hz", 24, 1, 2997, 125},
	{"30hz", 30, 1, 2997, 100},
	{"60hz", 60, 1, 2997, 50},
	{"120hz", 120, 1, 2997, 25},
	{"240hz", 120, 1, 5994, 25},
	{NULL},
};

static void recalc_vinfo_sync_duration(struct vinfo_s *info, unsigned int frac)
{
	struct frac_rate_table *fr = &fr_tab[0];

	pr_info(SYS "recalc before %s %d %d\n", info->name,
		info->sync_duration_num, info->sync_duration_den);

	while (fr->hz) {
		if (strstr(info->name, fr->hz)) {
			if (frac) {
				info->sync_duration_num = fr->sync_num_dec;
				info->sync_duration_den = fr->sync_den_dec;
			} else {
				info->sync_duration_num = fr->sync_num_int;
				info->sync_duration_den = fr->sync_den_int;
			}
			break;
		}
		fr++;
	}

	pr_info(SYS "recalc after %s %d %d\n", info->name,
		info->sync_duration_num, info->sync_duration_den);
}

static void hdmi_physcial_size_update(struct hdmitx_dev *hdev)
{
	unsigned int width, height;
	struct vinfo_s *info = NULL;

	info = hdmitx_get_current_vinfo();
	if ((info == NULL) || (info->name == NULL)) {
		pr_info(SYS "cann't get valid mode\n");
		return;
	}

	if (info->mode == VMODE_HDMI) {
		width = hdev->RXCap.physcial_weight;
		height = hdev->RXCap.physcial_height;
		if ((width == 0) || (height == 0)) {
			info->screen_real_width = info->aspect_ratio_num;
			info->screen_real_height = info->aspect_ratio_den;
		} else {
			/* transfer mm */
			info->screen_real_width = width * 10;
			info->screen_real_height = height * 10;
		}
		pr_info(SYS "update physcial size: %d %d\n",
			info->screen_real_width, info->screen_real_height);
	}

}

static void hdrinfo_to_vinfo(struct vinfo_s *info, struct hdmitx_dev *hdev)
{
	unsigned int i, j;
	/*static hdr*/
	info->hdr_info.hdr_support = (hdev->RXCap.hdr_sup_eotf_sdr << 0)
			| (hdev->RXCap.hdr_sup_eotf_hdr << 1)
			| (hdev->RXCap.hdr_sup_eotf_smpte_st_2084 << 2)
			| (hdev->RXCap.hdr_sup_eotf_hlg << 3);
	/*dynamic hdr*/
	for (i = 0; i < 4; i++) {
		if (hdev->RXCap.hdr_dynamic_info[i].type == 0) {
			memset(&info->hdr_info.dynamic_info[i],
				0, sizeof(struct hdr_dynamic));
			continue;
		}
		info->hdr_info.dynamic_info[i].type =
			hdev->RXCap.hdr_dynamic_info[i].type;
		info->hdr_info.dynamic_info[i].of_len =
			hdev->RXCap.hdr_dynamic_info[i].hd_len - 3;
		info->hdr_info.dynamic_info[i].support_flags =
			hdev->RXCap.hdr_dynamic_info[i].support_flags;

		for (j = 0; j < hdev->RXCap.hdr_dynamic_info[i].hd_len - 3; j++)
			info->hdr_info.dynamic_info[i].optional_fields[j] =
			hdev->RXCap.hdr_dynamic_info[i].optional_fields[j];
	}
	/*hdr 10+*/
	memcpy(&(info->hdr_info.hdr10plus_info),
		&(hdev->RXCap.hdr10plus_info), sizeof(struct hdr10_plus_info));

	info->hdr_info.colorimetry_support =
		hdev->RXCap.colorimetry_data;
	info->hdr_info.lumi_max = hdev->RXCap.hdr_lum_max;
	info->hdr_info.lumi_avg = hdev->RXCap.hdr_lum_avg;
	info->hdr_info.lumi_min = hdev->RXCap.hdr_lum_min;
	pr_info(SYS "update rx hdr info %x\n",
		info->hdr_info.hdr_support);
}

static void rxlatency_to_vinfo(struct vinfo_s *info, struct rx_cap *rx)
{
	if (!info || !rx)
		return;
	info->rx_latency.vLatency = rx->vLatency;
	info->rx_latency.aLatency = rx->aLatency;
	info->rx_latency.i_vLatency = rx->i_vLatency;
	info->rx_latency.i_aLatency = rx->i_aLatency;
}

static int set_disp_mode_auto(void)
{
	int ret =  -1;

	struct vinfo_s *info = NULL;
	struct hdmitx_dev *hdev = &hdmitx_device;
	struct hdmi_format_para *para = NULL;
	unsigned char mode[32];
	enum hdmi_vic vic = HDMI_Unknown;
	/* vic_ready got from IP */
	enum hdmi_vic vic_ready = hdev->HWOp.GetState(
		hdev, STAT_VIDEO_VIC, 0);

	memset(mode, 0, sizeof(mode));
	hdev->ready = 0;

	/* get current vinfo */
	info = hdmitx_get_current_vinfo();
	if ((info == NULL) || (info->name == NULL))
		return -1;

	pr_info(SYS "get current mode: %s\n", info->name);

	if (!((strncmp(info->name, "480cvbs", 7) == 0) ||
		(strncmp(info->name, "576cvbs", 7) == 0) ||
		(strncmp(info->name, "null", 4) == 0))) {
		hdrinfo_to_vinfo(info, hdev);
		rxlatency_to_vinfo(info, &hdev->RXCap);
	}

	hdmi_physcial_size_update(hdev);

	/* If info->name equals to cvbs, then set mode to I mode to hdmi
	 */
	if ((strncmp(info->name, "480cvbs", 7) == 0) ||
		(strncmp(info->name, "576cvbs", 7) == 0) ||
		(strncmp(info->name, "ntsc_m", 6) == 0) ||
		(strncmp(info->name, "pal_m", 5) == 0) ||
		(strncmp(info->name, "pal_n", 5) == 0) ||
		(strncmp(info->name, "panel", 5) == 0) ||
		(strncmp(info->name, "null", 4) == 0)) {
		pr_info(SYS "%s not valid hdmi mode\n", info->name);
		hdev->HWOp.CntlConfig(hdev, CONF_CLR_AVI_PACKET, 0);
		hdev->HWOp.CntlConfig(hdev, CONF_CLR_VSDB_PACKET, 0);
		hdev->HWOp.CntlMisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
		hdev->para = hdmi_get_fmt_name("invalid", hdev->fmt_attr);
		return -1;
	}
	strncpy(mode, info->name, sizeof(mode));
	mode[31] = '\0';
	if (strstr(mode, "fp")) {
		int i = 0;

		for (; mode[i]; i++) {
			if ((mode[i] == 'f') && (mode[i + 1] == 'p')) {
				/* skip "f", 1080fp60hz -> 1080p60hz */
				do {
					mode[i] = mode[i + 1];
					i++;
				} while (mode[i]);
				break;
			}
		}
	}

	/* In the file hdmi_common/hdmi_parameters.c,
	 * the data array all_fmt_paras[] treat 2160p60hz and 2160p60hz420
	 * as two different modes, such Scrambler
	 * So if node "attr" contains 420, need append 420 to mode.
	 */
	if (strstr(hdev->fmt_attr, "420")) {
		if (!strstr(mode, "420"))
			strncat(mode, "420", 3);
	}

	para = hdmi_get_fmt_name(mode, hdev->fmt_attr);
	hdev->para = para;
	vic = hdmitx_edid_get_VIC(hdev, mode, 1);
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
	if ((vic_ready != HDMI_Unknown) && (vic_ready == vic)) {
		pr_info(SYS "[%s] ALREADY init VIC = %d\n",
			__func__, vic);
#if defined(CONFIG_ARCH_MESON64_ODROID_COMMON)
		if (odroid_voutmode() == VOUTMODE_HDMI) {
			hdev->HWOp.CntlConfig(hdev, CONF_HDMI_DVI_MODE,
				HDMI_MODE);
			pr_info(SYS "change to HDMI mode\n");
		} else if (odroid_voutmode() == VOUTMODE_DVI) {
			hdev->HWOp.CntlConfig(hdev, CONF_HDMI_DVI_MODE,
				DVI_MODE);
			pr_info(SYS "change to DVI mode\n");
		} else if (hdev->RXCap.ieeeoui == 0) {
#else
		if (hdev->RXCap.ieeeoui == 0) {
#endif
			/* DVI case judgement. In uboot, directly output HDMI
			 * mode
			 */
			hdev->HWOp.CntlConfig(hdev, CONF_HDMI_DVI_MODE,
				DVI_MODE);
			pr_info(SYS "change to DVI mode\n");
		} else if ((hdev->RXCap.ieeeoui == 0xc03) &&
		(hdev->HWOp.CntlConfig(hdev, CONF_GET_HDMI_DVI_MODE, 0)
			== DVI_MODE)) {
			hdev->HWOp.CntlConfig(hdev, CONF_HDMI_DVI_MODE,
				HDMI_MODE);
			pr_info(SYS "change to HDMI mode\n");
		}
		hdev->cur_VIC = vic;
		hdev->output_blank_flag = 1;
		hdev->ready = 1;
		return 1;
	}

	hdmitx_pre_display_init();

	hdev->cur_VIC = HDMI_Unknown;
/* if vic is HDMI_Unknown, hdmitx_set_display will disable HDMI */
	ret = hdmitx_set_display(hdev, vic);

	if (ret >= 0) {
		hdev->HWOp.Cntl(hdev, HDMITX_AVMUTE_CNTL, AVMUTE_CLEAR);
		hdev->cur_VIC = vic;
		hdev->audio_param_update_flag = 1;
		hdev->auth_process_timer = AUTH_PROCESS_TIME;
	}
	if (hdev->cur_VIC == HDMI_Unknown) {
		if (hdev->hpdmode == 2) {
			/* edid will be read again when hpd is muxed
			 * and it is high
			 */
			hdmitx_edid_clear(hdev);
			hdev->mux_hpd_if_pin_high_flag = 0;
		}
		/* If current display is NOT panel, needn't TURNOFF_HDMIHW */
		if (strncmp(mode, "panel", 5) == 0) {
			hdev->HWOp.Cntl(hdev, HDMITX_HWCMD_TURNOFF_HDMIHW,
				(hdev->hpdmode == 2)?1:0);
		}
	}
	hdmitx_set_audio(hdev, &(hdev->cur_audio_param));
	hdev->output_blank_flag = 1;
	hdev->ready = 1;
	return ret;
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
	return count;
}

static ssize_t show_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%s\n\r", hdmitx_device.fmt_attr);
	return pos;
}

ssize_t store_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	strncpy(hdmitx_device.fmt_attr, buf, sizeof(hdmitx_device.fmt_attr));
	hdmitx_device.fmt_attr[15] = '\0';
	return count;
}
/*aud_mode attr*/

void setup_attr(const char *buf)
{
	char attr[16] = {0};

	memcpy(attr, buf, sizeof(attr));
	memcpy(hdmitx_device.fmt_attr, attr, sizeof(hdmitx_device.fmt_attr));
}
EXPORT_SYMBOL(setup_attr);


/* for android application data exchange / swap */
static char *tmp_swap;
static DEFINE_MUTEX(mutex_swap);

static ssize_t store_swap(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	pr_info("store_swap: %s\n", buf);
	mutex_lock(&mutex_swap);

	kfree(tmp_swap);
	tmp_swap = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp_swap) {
		mutex_unlock(&mutex_swap);
		return count;
	}
	memcpy(tmp_swap, buf, count);
	tmp_swap[count] = '\0'; /* padding end string */
	mutex_unlock(&mutex_swap);
	return count;
}

static ssize_t show_swap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i = 0;
	int n = 0;
	struct hdmitx_dev *hdev = &hdmitx_device;
	struct rx_cap *pRXCap = &hdev->RXCap;
	struct hdcprp14_topo *topo14 = &hdev->topo_info->topo.topo14;

	mutex_lock(&mutex_swap);

	if (!tmp_swap ||
		(!hdev->edid_parsing && !strstr(tmp_swap, "hdcp.topo"))) {
		mutex_unlock(&mutex_swap);
		return n;
	}

	/* VSD: Video Short Descriptor */
	if (strstr(tmp_swap, "edid.vsd"))
		for (i = 0; i < pRXCap->vsd.len; i++)
			n += snprintf(buf + n, PAGE_SIZE - n, "%02x",
				pRXCap->vsd.raw[i]);
	/* ASD: Audio Short Descriptor */
	if (strstr(tmp_swap, "edid.asd"))
		for (i = 0; i < pRXCap->asd.len; i++)
			n += snprintf(buf + n, PAGE_SIZE - n, "%02x",
				pRXCap->asd.raw[i]);
	/* CEC: Physical Address */
	if (strstr(tmp_swap, "edid.cec"))
		n += snprintf(buf + n, PAGE_SIZE - n, "%x%x%x%x",
			hdev->hdmi_info.vsdb_phy_addr.a,
			hdev->hdmi_info.vsdb_phy_addr.b,
			hdev->hdmi_info.vsdb_phy_addr.c,
			hdev->hdmi_info.vsdb_phy_addr.d);
	/* HDCP TOPO */
	if (strstr(tmp_swap, "hdcp.topo")) {
		char *tmp = (char *)topo14;

		pr_info("max_cascade_exceeded %d\n",
			topo14->max_cascade_exceeded);
		pr_info("depth %d\n", topo14->depth);
		pr_info("max_devs_exceeded %d\n", topo14->max_devs_exceeded);
		pr_info("device_count %d\n", topo14->device_count);
		for (i = 0; i < sizeof(struct hdcprp14_topo); i++)
			n += snprintf(buf + n, PAGE_SIZE - n, "%02x", tmp[i]);
	}

	kfree(tmp_swap);
	tmp_swap = NULL;

	mutex_unlock(&mutex_swap);
	return n;
}

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

static int dump_edid_data(unsigned int type, char *path)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	char line[128] = {0};
	mm_segment_t old_fs = get_fs();
	unsigned int i = 0, j = 0, k = 0, size = 0, block_cnt = 0;

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR|O_CREAT, 0666);
	if (IS_ERR(filp)) {
		pr_info("[%s] failed to open/create file: |%s|\n",
			__func__, path);
		goto PROCESS_END;
	}

	block_cnt = hdmitx_device.EDID_buf[0x7e] + 1;
	if (type == 1) {
		/* dump as bin file*/
		size = vfs_write(filp, hdmitx_device.EDID_buf,
							block_cnt*128, &pos);
	} else if (type == 2) {
		/* dump as txt file*/

		for (i = 0; i < block_cnt; i++) {
			for (j = 0; j < 8; j++) {
				for (k = 0; k < 16; k++) {
					snprintf((char *)&line[k*6], 7,
					"0x%02x, ",
					hdmitx_device.EDID_buf[i*128+j*16+k]);
				}
				line[16*6-1] = '\n';
				line[16*6] = 0x0;
				pos = (i*8+j)*16*6;
				size += vfs_write(filp, line, 16*6, &pos);
			}
		}
	}

	pr_info("[%s] write %d bytes to file %s\n", __func__, size, path);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);

PROCESS_END:
	set_fs(old_fs);
	return 0;
}

static int load_edid_data(unsigned int type, char *path)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();

	struct kstat stat;
	unsigned int length = 0, max_len = EDID_MAX_BLOCK * 128;
	char *buf = NULL;

	set_fs(KERNEL_DS);

	filp = filp_open(path, O_RDONLY, 0444);
	if (IS_ERR(filp)) {
		pr_info("[%s] failed to open file: |%s|\n", __func__, path);
		goto PROCESS_END;
	}

	vfs_stat(path, &stat);

	length = (stat.size > max_len)?max_len:stat.size;

	buf = kmalloc(length, GFP_KERNEL);
	if (buf == NULL)
		goto PROCESS_END;

	vfs_read(filp, buf, length, &pos);

	memcpy(hdmitx_device.EDID_buf, buf, length);

	kfree(buf);
	filp_close(filp, NULL);

	pr_info("[%s] %d bytes loaded from file %s\n", __func__, length, path);

	hdmitx_edid_clear(&hdmitx_device);
	hdmitx_edid_parse(&hdmitx_device);
	pr_info("[%s] new edid loaded!\n", __func__);

PROCESS_END:
	set_fs(old_fs);
	return 0;
}

static ssize_t store_edid(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int argn = 0;
	char *p = NULL, *para = NULL, *argv[8] = {NULL};
	unsigned int path_length = 0;

	p = kstrdup(buf, GFP_KERNEL);
	if (p == NULL)
		return count;

	do {
		para = strsep(&p, " ");
		if (para != NULL) {
			argv[argn] = para;
			argn++;
			if (argn > 7)
				break;
		}
	} while (para != NULL);

	if (buf[0] == 'h') {
		int i;

		pr_info(EDID "EDID hash value:\n");
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
					pr_info(EDID "%02x ",
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
					pr_info(EDID "%02x ",
		hdmitx_device.EDID_buf1[block_idx*128+ii*16+jj]);
				}
				pr_info("\n");
			}
			pr_info("\n");
		}
	}

	if (!strncmp(argv[0], "save", strlen("save"))) {
		unsigned int type = 0;

		if (argn != 3) {
			pr_info("[%s] cmd format: save bin/txt edid_file_path\n",
						__func__);
			goto PROCESS_END;
		}
		if (!strncmp(argv[1], "bin", strlen("bin")))
			type = 1;
		else if (!strncmp(argv[1], "txt", strlen("txt")))
			type = 2;

		if ((type == 1) || (type == 2)) {
			/* clean '\n' from file path*/
			path_length = strlen(argv[2]);
			if (argv[2][path_length-1] == '\n')
				argv[2][path_length-1] = 0x0;

			dump_edid_data(type, argv[2]);
		}
	} else if (!strncmp(argv[0], "load", strlen("load"))) {
		if (argn != 2) {
			pr_info("[%s] cmd format: load edid_file_path\n",
						__func__);
			goto PROCESS_END;
		}

		/* clean '\n' from file path*/
		path_length = strlen(argv[1]);
		if (argv[1][path_length-1] == '\n')
			argv[1][path_length-1] = 0x0;
		load_edid_data(0, argv[1]);
	}

PROCESS_END:
	kfree(p);
	return 16;
}

/* rawedid attr */
static ssize_t show_rawedid(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int pos = 0;
	int i;
	struct hdmitx_dev *hdev = &hdmitx_device;
	int num;

	/* prevent null prt */
	if (!hdev->edid_ptr)
		hdev->edid_ptr = hdev->EDID_buf;

	if (hdev->edid_ptr[0x7e] < 4)
		num = (hdev->edid_ptr[0x7e]+1)*0x80;
	else
		num = 0x100;

	for (i = 0; i < num; i++)
		pos += snprintf(buf+pos, PAGE_SIZE, "%02x", hdev->edid_ptr[i]);

	pos += snprintf(buf+pos, PAGE_SIZE, "\n");

	return pos;
}

/*
 * edid_parsing attr
 * If RX edid data are all correct, HEAD(00 ff ff ff ff ff ff 00), checksum,
 * version, etc), then return "ok". Otherwise, "ng"
 * Actually, in some old televisions, EDID is stored in EEPROM.
 * some bits in EEPROM may reverse with time.
 * But it does not affect  edid_parsing.
 * Therefore, we consider the RX edid data are all correct, return "OK"
 */
static ssize_t show_edid_parsing(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "ok\n");
	return pos;
}

/*
 * sink_type attr
 * sink, or repeater
 */
static ssize_t show_sink_type(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = &hdmitx_device;

	if (!hdev->hpd_state) {
		pos += snprintf(buf+pos, PAGE_SIZE, "none\n");
		return pos;
	}

	if (hdev->hdmi_info.vsdb_phy_addr.b)
		pos += snprintf(buf+pos, PAGE_SIZE, "repeater\n");
	else
		pos += snprintf(buf+pos, PAGE_SIZE, "sink\n");

	return pos;
}

/*
 * hdcp_repeater attr
 * For hdcp 22, hdcp_tx22 will write to store_hdcp_repeater
 * For hdcp 14, directly get bcaps bit
 */
static ssize_t show_hdcp_repeater(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = &hdmitx_device;

	if (hdev->hdcp_mode == 1)
		hdev->hdcp_bcaps_repeater = hdev->HWOp.CntlDDC(hdev,
			DDC_HDCP14_GET_BCAPS_RP, 0);

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n", hdev->hdcp_bcaps_repeater);

	return pos;
}

static ssize_t store_hdcp_repeater(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	if (hdev->hdcp_mode == 2)
		hdev->hdcp_bcaps_repeater = (buf[0] == '1');

	return count;
}

/*
 * hdcp_topo_info attr
 * For hdcp 22, hdcp_tx22 will write to store_hdcp_topo_info
 * For hdcp 14, directly get from HW
 */

static ssize_t show_hdcp_topo_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = &hdmitx_device;
	struct hdcprp_topo *topoinfo = hdev->topo_info;

	if (!hdev->hdcp_mode) {
		pos += snprintf(buf+pos, PAGE_SIZE, "hdcp mode: 0\n");
		return pos;
	}
	if (!topoinfo)
		return pos;

	if (hdev->hdcp_mode == 1) {
		memset(topoinfo, 0, sizeof(struct hdcprp_topo));
		hdev->HWOp.CntlDDC(hdev, DDC_HDCP14_GET_TOPO_INFO,
			(unsigned long)&topoinfo->topo.topo14);
	}

	pos += snprintf(buf+pos, PAGE_SIZE, "hdcp mode: %s\n",
		hdev->hdcp_mode == 1 ? "14" : "22");
	if (hdev->hdcp_mode == 2) {
		topoinfo->hdcp_ver = HDCPVER_22;
		pos += snprintf(buf+pos, PAGE_SIZE, "max_devs_exceeded: %d\n",
			topoinfo->topo.topo22.max_devs_exceeded);
		pos += snprintf(buf+pos, PAGE_SIZE,
			"max_cascade_exceeded: %d\n",
			topoinfo->topo.topo22.max_cascade_exceeded);
		pos += snprintf(buf+pos, PAGE_SIZE, "v2_0_repeater_down: %d\n",
			topoinfo->topo.topo22.v2_0_repeater_down);
		pos += snprintf(buf+pos, PAGE_SIZE, "v1_X_device_down: %d\n",
			topoinfo->topo.topo22.v1_X_device_down);
		pos += snprintf(buf+pos, PAGE_SIZE, "device_count: %d\n",
			topoinfo->topo.topo22.device_count);
		pos += snprintf(buf+pos, PAGE_SIZE, "depth: %d\n",
			topoinfo->topo.topo22.depth);
		return pos;
	}
	if (hdev->hdcp_mode == 1) {
		topoinfo->hdcp_ver = HDCPVER_14;
		pos += snprintf(buf+pos, PAGE_SIZE, "max_devs_exceeded: %d\n",
			topoinfo->topo.topo14.max_devs_exceeded);
		pos += snprintf(buf+pos, PAGE_SIZE,
			"max_cascade_exceeded: %d\n",
			topoinfo->topo.topo14.max_cascade_exceeded);
		pos += snprintf(buf+pos, PAGE_SIZE, "device_count: %d\n",
			topoinfo->topo.topo14.device_count);
		pos += snprintf(buf+pos, PAGE_SIZE, "depth: %d\n",
			topoinfo->topo.topo14.depth);
		return pos;
	}

	return pos;
}

static ssize_t store_hdcp_topo_info(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	struct hdcprp_topo *topoinfo = hdev->topo_info;
	int cnt;

	if (!topoinfo)
		return count;

	if (hdev->hdcp_mode == 2) {
		memset(topoinfo, 0, sizeof(struct hdcprp_topo));
		cnt = sscanf(buf, "%x %x %x %x %x %x",
			(int *)&topoinfo->topo.topo22.max_devs_exceeded,
			(int *)&topoinfo->topo.topo22.max_cascade_exceeded,
			(int *)&topoinfo->topo.topo22.v2_0_repeater_down,
			(int *)&topoinfo->topo.topo22.v1_X_device_down,
			(int *)&topoinfo->topo.topo22.device_count,
			(int *)&topoinfo->topo.topo22.depth);
		if (cnt < 0)
			return count;
	}

	return count;
}

static ssize_t show_hdcp22_type(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n", hdmitx_device.hdcp22_type);

	return pos;
}

static ssize_t store_hdcp22_type(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int type = 0;
	struct hdmitx_dev *hdev = &hdmitx_device;

	if (buf[0] == '1')
		type = 1;
	else
		type = 0;
	hdev->hdcp22_type = type;

	pr_info("hdmitx: set hdcp22 content type %d\n", type);
	hdev->HWOp.CntlDDC(hdev, DDC_HDCP_SET_TOPO_INFO, type);

	return count;
}

static ssize_t show_hdcp22_base(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "0x%x\n", get_hdcp22_base());

	return pos;
}

void hdmitx_audio_mute_op(unsigned int flag)
{
	hdmitx_device.tx_aud_cfg = flag;
	if (flag == 0)
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	else
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AUDIO_MUTE_OP, AUDIO_UNMUTE);
}
EXPORT_SYMBOL(hdmitx_audio_mute_op);

void hdmitx_video_mute_op(unsigned int flag)
{
	if (flag == 0)
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_VIDEO_MUTE_OP, VIDEO_MUTE);
	else
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_VIDEO_MUTE_OP, VIDEO_UNMUTE);
}
EXPORT_SYMBOL(hdmitx_video_mute_op);

/*
 *  SDR/HDR uevent
 *  1: SDR to HDR
 *  0: HDR to SDR
 */
static void hdmitx_sdr_hdr_uevent(struct hdmitx_dev *hdev)
{
	if ((hdev->hdmi_last_hdr_mode == 0) &&
		(hdev->hdmi_current_hdr_mode != 0)) {
		/* SDR -> HDR*/
		hdev->hdmi_last_hdr_mode = hdev->hdmi_current_hdr_mode;
		extcon_set_state_sync(hdmitx_extcon_hdr, EXTCON_DISP_HDMI, 1);
	} else if ((hdev->hdmi_last_hdr_mode != 0) &&
			(hdev->hdmi_current_hdr_mode == 0)) {
		/* HDR -> SDR*/
		hdev->hdmi_last_hdr_mode = hdev->hdmi_current_hdr_mode;
		extcon_set_state_sync(hdmitx_extcon_hdr, EXTCON_DISP_HDMI, 0);
	}
}

static void hdr_work_func(struct work_struct *work)
{
	struct hdmitx_dev *hdev =
		container_of(work, struct hdmitx_dev, work_hdr);

	if (hdev->hdr_transfer_feature == T_BT709 &&
		hdev->hdr_color_feature == C_BT709) {
		unsigned char DRM_HB[3] = {0x87, 0x1, 26};
		unsigned char DRM_DB[26] = {0x0};

		hdev->HWOp.SetPacket(HDMI_PACKET_DRM, DRM_DB, DRM_HB);
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AVI_BT2020, hdev->colormetry);

		msleep(1500);/*delay 1.5s*/
		if (hdev->hdr_transfer_feature != T_BT709 ||
			hdev->hdr_color_feature != C_BT709)
			return;

		hdev->HWOp.SetPacket(HDMI_PACKET_DRM, NULL, NULL);
		hdev->hdmi_current_hdr_mode = 0;
		hdmitx_sdr_hdr_uevent(hdev);
	} else {
		hdmitx_sdr_hdr_uevent(hdev);
	}
}

#define GET_LOW8BIT(a)	((a) & 0xff)
#define GET_HIGH8BIT(a)	(((a) >> 8) & 0xff)
static void hdmitx_set_drm_pkt(struct master_display_info_s *data)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	unsigned char DRM_HB[3] = {0x87, 0x1, 26};
	unsigned char DRM_DB[26] = {0x0};

	/*
	 *hdr_color_feature: bit 23-16: color_primaries
	 *	1:bt709  0x9:bt2020
	 *hdr_transfer_feature: bit 15-8: transfer_characteristic
	 *	1:bt709 0xe:bt2020-10 0x10:smpte-st-2084 0x12:hlg(todo)
	 */
	if (data) {
		hdev->hdr_transfer_feature = (data->features >> 8) & 0xff;
		hdev->hdr_color_feature = (data->features >> 16) & 0xff;
		hdev->colormetry = (data->features >> 30) & 0x1;
	}
	if ((!data) || (!(hdev->RXCap.hdr_sup_eotf_smpte_st_2084) &&
		!(hdev->RXCap.hdr_sup_eotf_hdr) &&
		!(hdev->RXCap.hdr_sup_eotf_sdr) &&
		!(hdev->RXCap.hdr_sup_eotf_hlg))) {
		DRM_HB[1] = 0;
		DRM_HB[2] = 0;
		hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM, NULL, NULL);
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AVI_BT2020, hdev->colormetry);
		return;
	}

	/*SDR*/
	if (hdev->hdr_transfer_feature == T_BT709 &&
		hdev->hdr_color_feature == C_BT709) {
		schedule_work(&hdev->work_hdr);
		return;
	}

	DRM_DB[1] = 0x0;
	DRM_DB[2] = GET_LOW8BIT(data->primaries[0][0]);
	DRM_DB[3] = GET_HIGH8BIT(data->primaries[0][0]);
	DRM_DB[4] = GET_LOW8BIT(data->primaries[0][1]);
	DRM_DB[5] = GET_HIGH8BIT(data->primaries[0][1]);
	DRM_DB[6] = GET_LOW8BIT(data->primaries[1][0]);
	DRM_DB[7] = GET_HIGH8BIT(data->primaries[1][0]);
	DRM_DB[8] = GET_LOW8BIT(data->primaries[1][1]);
	DRM_DB[9] = GET_HIGH8BIT(data->primaries[1][1]);
	DRM_DB[10] = GET_LOW8BIT(data->primaries[2][0]);
	DRM_DB[11] = GET_HIGH8BIT(data->primaries[2][0]);
	DRM_DB[12] = GET_LOW8BIT(data->primaries[2][1]);
	DRM_DB[13] = GET_HIGH8BIT(data->primaries[2][1]);
	DRM_DB[14] = GET_LOW8BIT(data->white_point[0]);
	DRM_DB[15] = GET_HIGH8BIT(data->white_point[0]);
	DRM_DB[16] = GET_LOW8BIT(data->white_point[1]);
	DRM_DB[17] = GET_HIGH8BIT(data->white_point[1]);
	DRM_DB[18] = GET_LOW8BIT(data->luminance[0]);
	DRM_DB[19] = GET_HIGH8BIT(data->luminance[0]);
	DRM_DB[20] = GET_LOW8BIT(data->luminance[1]);
	DRM_DB[21] = GET_HIGH8BIT(data->luminance[1]);
	DRM_DB[22] = GET_LOW8BIT(data->max_content);
	DRM_DB[23] = GET_HIGH8BIT(data->max_content);
	DRM_DB[24] = GET_LOW8BIT(data->max_frame_average);
	DRM_DB[25] = GET_HIGH8BIT(data->max_frame_average);

	/* bt2020 + gamma transfer */
	if (hdev->hdr_transfer_feature == T_BT709 &&
		hdev->hdr_color_feature == C_BT2020) {
		if (hdev->sdr_hdr_feature == 0) {
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				NULL, NULL);
			hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
				CONF_AVI_BT2020, SET_AVI_BT2020);
		} else if (hdev->sdr_hdr_feature == 1) {
			memset(DRM_DB, 0, sizeof(DRM_DB));
			hdev->HWOp.SetPacket(HDMI_PACKET_DRM,
				DRM_DB, DRM_HB);
			hdev->HWOp.CntlConfig(&hdmitx_device,
				CONF_AVI_BT2020, SET_AVI_BT2020);
		} else {
			DRM_DB[0] = 0x02; /* SMPTE ST 2084 */
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				DRM_DB, DRM_HB);
			hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
				CONF_AVI_BT2020, SET_AVI_BT2020);
		}
		return;
	}

	/*must clear hdr mode*/
	hdev->hdmi_current_hdr_mode = 0;

	/* SMPTE ST 2084 and (BT2020 or NON_STANDARD) */
	if (hdev->RXCap.hdr_sup_eotf_smpte_st_2084) {
		if (hdev->hdr_transfer_feature == T_SMPTE_ST_2084 &&
			hdev->hdr_color_feature == C_BT2020)
			hdev->hdmi_current_hdr_mode = 1;
		else if (hdev->hdr_transfer_feature == T_SMPTE_ST_2084 &&
			hdev->hdr_color_feature != C_BT2020)
			hdev->hdmi_current_hdr_mode = 2;
	}

	/*HLG and BT2020*/
	if (hdev->RXCap.hdr_sup_eotf_hlg) {
		if (hdev->hdr_color_feature == C_BT2020 &&
			(hdev->hdr_transfer_feature == T_BT2020_10 ||
			hdev->hdr_transfer_feature == T_HLG))
			hdev->hdmi_current_hdr_mode = 3;
	}

	switch (hdev->hdmi_current_hdr_mode) {
	case 1:
		/*standard HDR*/
		DRM_DB[0] = 0x02; /* SMPTE ST 2084 */
		hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
			DRM_DB, DRM_HB);
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AVI_BT2020, SET_AVI_BT2020);
		break;
	case 2:
		/*non standard*/
		DRM_DB[0] = 0x02; /* no standard SMPTE ST 2084 */
		hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
			DRM_DB, DRM_HB);
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AVI_BT2020, CLR_AVI_BT2020);
		break;
	case 3:
		/*HLG*/
		DRM_DB[0] = 0x03;/* HLG is 0x03 */
		hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
			DRM_DB, DRM_HB);
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AVI_BT2020, SET_AVI_BT2020);
		break;
	case 0:
	default:
		/*other case*/
		hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM, NULL, NULL);
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AVI_BT2020, CLR_AVI_BT2020);
		break;
	}

	/* if sdr/hdr mode change ,notify uevent to userspace*/
	if (hdev->hdmi_current_hdr_mode != hdev->hdmi_last_hdr_mode)
		schedule_work(&hdev->work_hdr);

}

static void hdmitx_set_vsif_pkt(enum eotf_type type,
	enum mode_type tunnel_mode, struct dv_vsif_para *data)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	struct dv_vsif_para para = {0};
	unsigned char VEN_HB[3] = {0x81, 0x01};
	unsigned char VEN_DB1[24] = {0x00};
	unsigned char VEN_DB2[27] = {0x00};
	unsigned char len = 0;
	unsigned int vic = hdev->cur_VIC;
	unsigned int hdmi_vic_4k_flag = 0;
	static enum eotf_type ltype = EOTF_T_NULL;
	static uint8_t ltmode = -1;

	if ((hdev->ready == 0) || (hdev->RXCap.dv_info.ieeeoui
		!= DV_IEEE_OUI)) {
		ltype = EOTF_T_NULL;
		ltmode = -1;
		return;
	}

	if ((hdev->chip_type) < MESON_CPU_ID_GXL) {
		pr_info("hdmitx: not support DolbyVision\n");
		return;
	}

	hdev->hdmi_current_eotf_type = type;
	hdev->hdmi_current_tunnel_mode = tunnel_mode;
	/*ver0 and ver1_15 and ver1_12bit with ll= 0 use hdmi 1.4b VSIF*/
	if ((hdev->RXCap.dv_info.ver == 0) || ((hdev->RXCap.dv_info.ver == 1)
		&& (hdev->RXCap.dv_info.length == 0xE))
		|| ((hdev->RXCap.dv_info.ver == 1)
		&& (hdev->RXCap.dv_info.length == 0xB)
		&& (hdev->RXCap.dv_info.low_latency == 0))) {
		if ((vic == HDMI_3840x2160p30_16x9) ||
		    (vic == HDMI_3840x2160p25_16x9) ||
		    (vic == HDMI_3840x2160p24_16x9) ||
		    (vic == HDMI_4096x2160p24_256x135))
			hdmi_vic_4k_flag = 1;

		switch (type) {
		case EOTF_T_DOLBYVISION:
			len = 0x18;
			hdev->dv_src_feature = 1;
			break;
		case EOTF_T_HDR10:
		case EOTF_T_SDR:
		case EOTF_T_NULL:
		default:
			len = 0x05;
			hdev->dv_src_feature = 0;
			break;
		}

		VEN_HB[2] = len;
		VEN_DB1[0] = 0x03;
		VEN_DB1[1] = 0x0c;
		VEN_DB1[2] = 0x00;
		VEN_DB1[3] = 0x00;

		if (hdmi_vic_4k_flag) {
			VEN_DB1[3] = 0x20;
			if (vic == HDMI_3840x2160p30_16x9)
				VEN_DB1[4] = 0x1;
			else if (vic == HDMI_3840x2160p25_16x9)
				VEN_DB1[4] = 0x2;
			else if (vic == HDMI_3840x2160p24_16x9)
				VEN_DB1[4] = 0x3;
			else/*vic == HDMI_4096x2160p24_256x135*/
				VEN_DB1[4] = 0x4;
		}
		if (type == EOTF_T_DOLBYVISION) {
			/*first disable drm package*/
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				NULL, NULL);
			hdev->HWOp.SetPacket(HDMI_PACKET_VEND, VEN_DB1, VEN_HB);
			hdev->HWOp.CntlConfig(hdev, CONF_AVI_BT2020,
				CLR_AVI_BT2020);/*BT709*/
			if (tunnel_mode == RGB_8BIT) {
				hdev->HWOp.CntlConfig(hdev,
					CONF_AVI_RGBYCC_INDIC,
					COLORSPACE_RGB444);
				hdev->HWOp.CntlConfig(hdev, CONF_AVI_Q01,
					RGB_RANGE_FUL);
			} else {
				hdev->HWOp.CntlConfig(hdev,
					CONF_AVI_RGBYCC_INDIC,
					COLORSPACE_YUV422);
				hdev->HWOp.CntlConfig(hdev, CONF_AVI_YQ01,
					YCC_RANGE_FUL);
			}
		} else {
			if (hdmi_vic_4k_flag)
				hdev->HWOp.SetPacket(
					HDMI_PACKET_VEND, VEN_DB1, VEN_HB);
			else
				hdev->HWOp.SetPacket(
					HDMI_PACKET_VEND, NULL, NULL);
			hdev->HWOp.CntlConfig(hdev,
				CONF_AVI_RGBYCC_INDIC, hdev->para->cs);
			hdev->HWOp.CntlConfig(hdev,
				CONF_AVI_Q01, RGB_RANGE_LIM);
			hdev->HWOp.CntlConfig(hdev,
				CONF_AVI_YQ01, YCC_RANGE_LIM);
			hdev->HWOp.CntlConfig(hdev, CONF_AVI_BT2020,
				CLR_AVI_BT2020);/*BT709*/
		}

	}
	/*ver1_12  with low_latency = 1 and ver2 use Dolby VSIF*/
	if ((hdev->RXCap.dv_info.ver == 2) || ((hdev->RXCap.dv_info.ver == 1)
		&& (hdev->RXCap.dv_info.length == 0xB)
		&& (hdev->RXCap.dv_info.low_latency == 1))
		|| (type == EOTF_T_LL_MODE)) {

		if (data == NULL)
			data = &para;
		/*4k vsif package */
		if ((vic == HDMI_3840x2160p30_16x9) ||
		    (vic == HDMI_3840x2160p25_16x9) ||
		    (vic == HDMI_3840x2160p24_16x9) ||
		    (vic == HDMI_4096x2160p24_256x135))
			hdmi_vic_4k_flag = 1;

		switch (type) {
		case EOTF_T_DOLBYVISION:
		case EOTF_T_LL_MODE:
			len = 0x1b;
			hdev->dv_src_feature = 1;
			break;
		case EOTF_T_HDR10:
		case EOTF_T_SDR:
		case EOTF_T_NULL:
		default:
			len = 0x5;
			hdev->dv_src_feature = 0;
			break;
		}
		VEN_HB[2] = len;
		VEN_DB2[0] = 0x46;
		VEN_DB2[1] = 0xd0;
		VEN_DB2[2] = 0x00;
		VEN_DB2[3] = (data->vers.ver2.low_latency) |
			(data->vers.ver2.dobly_vision_signal << 1);
		VEN_DB2[4] = (data->vers.ver2.eff_tmax_PQ_hi)
			| (data->vers.ver2.auxiliary_MD_present << 6)
			| (data->vers.ver2.backlt_ctrl_MD_present << 7);
		VEN_DB2[5] = data->vers.ver2.eff_tmax_PQ_low;
		VEN_DB2[6] = data->vers.ver2.auxiliary_runmode;
		VEN_DB2[7] = data->vers.ver2.auxiliary_runversion;
		VEN_DB2[8] = data->vers.ver2.auxiliary_debug0;

		/*Dolby Vision standard case*/
		if (type == EOTF_T_DOLBYVISION) {
			/*first disable drm package*/
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				NULL, NULL);
			hdev->HWOp.SetPacket(HDMI_PACKET_VEND, VEN_DB2, VEN_HB);
			hdev->HWOp.CntlConfig(hdev, CONF_AVI_BT2020,
				CLR_AVI_BT2020);/*BT709*/
			if (tunnel_mode == RGB_8BIT) {/*RGB444*/
				hdev->HWOp.CntlConfig(hdev,
					CONF_AVI_RGBYCC_INDIC,
					COLORSPACE_RGB444);
				hdev->HWOp.CntlConfig(hdev, CONF_AVI_Q01,
					RGB_RANGE_FUL);
			} else {/*YUV422*/
				hdev->HWOp.CntlConfig(hdev,
					CONF_AVI_RGBYCC_INDIC,
					COLORSPACE_YUV422);
				hdev->HWOp.CntlConfig(hdev, CONF_AVI_YQ01,
					YCC_RANGE_FUL);
			}
		}
		/*Dolby Vision low-latency case*/
		else if  (type == EOTF_T_LL_MODE) {
			/*first disable drm package*/
			hdmitx_device.HWOp.SetPacket(HDMI_PACKET_DRM,
				NULL, NULL);
			hdev->HWOp.SetPacket(HDMI_PACKET_VEND, VEN_DB2, VEN_HB);
			if (hdev->RXCap.colorimetry_data & 0xe0)
				/*if RX support BT2020, then output BT2020*/
				hdev->HWOp.CntlConfig(hdev, CONF_AVI_BT2020,
					SET_AVI_BT2020);/*BT2020*/
			else
				hdev->HWOp.CntlConfig(hdev, CONF_AVI_BT2020,
					CLR_AVI_BT2020);/*BT709*/
			if (tunnel_mode == RGB_10_12BIT) {/*10/12bit RGB444*/
				hdev->HWOp.CntlConfig(hdev,
					CONF_AVI_RGBYCC_INDIC,
					COLORSPACE_RGB444);
				hdev->HWOp.CntlConfig(hdev, CONF_AVI_Q01,
					RGB_RANGE_LIM);
			} else if (tunnel_mode == YUV444_10_12BIT) {
				/*10/12bit YUV444*/
				hdev->HWOp.CntlConfig(hdev,
					CONF_AVI_RGBYCC_INDIC,
					COLORSPACE_YUV444);
				hdev->HWOp.CntlConfig(hdev, CONF_AVI_YQ01,
					YCC_RANGE_LIM);
			} else {/*YUV422*/
				hdev->HWOp.CntlConfig(hdev,
					CONF_AVI_RGBYCC_INDIC,
					COLORSPACE_YUV422);
				hdev->HWOp.CntlConfig(hdev, CONF_AVI_YQ01,
					YCC_RANGE_LIM);
			}
		}
		/*SDR case*/
		else {
			if (hdmi_vic_4k_flag) {
				VEN_DB1[0] = 0x03;
				VEN_DB1[1] = 0x0c;
				VEN_DB1[2] = 0x00;
				hdev->HWOp.SetPacket(
					HDMI_PACKET_VEND, VEN_DB2, VEN_HB);
			} else
				hdev->HWOp.SetPacket(
					HDMI_PACKET_VEND, NULL, NULL);
			hdev->HWOp.CntlConfig(hdev,
				CONF_AVI_RGBYCC_INDIC, hdev->para->cs);
			hdev->HWOp.CntlConfig(hdev,
				CONF_AVI_Q01, RGB_RANGE_LIM);
			hdev->HWOp.CntlConfig(hdev,
				CONF_AVI_YQ01, YCC_RANGE_LIM);
			hdev->HWOp.CntlConfig(hdev, CONF_AVI_BT2020,
				CLR_AVI_BT2020);/*BT709*/
		}
	}
}
static void hdmitx_set_hdr10plus_pkt(unsigned int flag,
	struct hdr10plus_para *data)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	unsigned char VEN_HB[3] = {0x81, 0x01, 0x1b};
	unsigned char VEN_DB[27] = {0x00};

	if ((!data) || (!flag)) {
		hdev->HWOp.SetPacket(HDMI_PACKET_VEND, NULL, NULL);
		hdev->HWOp.CntlConfig(hdev, CONF_AVI_BT2020,
			CLR_AVI_BT2020);
		return;
	}

	VEN_DB[0] = 0x8b;
	VEN_DB[1] = 0x84;
	VEN_DB[2] = 0x90;

	VEN_DB[3] = ((data->application_version & 0x3) << 6) |
		 ((data->targeted_max_lum & 0x1f) << 1);
	VEN_DB[4] = data->average_maxrgb;
	VEN_DB[5] = data->distribution_values[0];
	VEN_DB[6] = data->distribution_values[1];
	VEN_DB[7] = data->distribution_values[2];
	VEN_DB[8] = data->distribution_values[3];
	VEN_DB[9] = data->distribution_values[4];
	VEN_DB[10] = data->distribution_values[5];
	VEN_DB[11] = data->distribution_values[6];
	VEN_DB[12] = data->distribution_values[7];
	VEN_DB[13] = data->distribution_values[8];
	VEN_DB[14] = ((data->num_bezier_curve_anchors & 0xf) << 4) |
		((data->knee_point_x >> 6) & 0xf);
	VEN_DB[15] = ((data->knee_point_x & 0x3f) << 2) |
		((data->knee_point_y >> 8) & 0x3);
	VEN_DB[16] = data->knee_point_y  & 0xff;
	VEN_DB[17] = data->bezier_curve_anchors[0];
	VEN_DB[18] = data->bezier_curve_anchors[1];
	VEN_DB[19] = data->bezier_curve_anchors[2];
	VEN_DB[20] = data->bezier_curve_anchors[3];
	VEN_DB[21] = data->bezier_curve_anchors[4];
	VEN_DB[22] = data->bezier_curve_anchors[5];
	VEN_DB[23] = data->bezier_curve_anchors[6];
	VEN_DB[24] = data->bezier_curve_anchors[7];
	VEN_DB[25] = data->bezier_curve_anchors[8];
	VEN_DB[26] = ((data->graphics_overlay_flag & 0x1) << 7)|
		((data->no_delay_flag & 0x1) << 6);

	hdev->HWOp.SetPacket(HDMI_PACKET_VEND, VEN_DB, VEN_HB);
	hdev->HWOp.CntlConfig(hdev, CONF_AVI_BT2020,
			SET_AVI_BT2020);

}

#define  EMP_FIRST 0x80
#define  EMP_LAST 0x40
static void hdmitx_set_emp_pkt(unsigned char *data, unsigned int type,
	unsigned int size)
{
	unsigned int number;
	unsigned int remainder;
	unsigned char *virt_ptr;
	unsigned char *virt_ptr_align32bit;
	unsigned long phys_ptr;
	unsigned int i;
	struct hdmitx_dev *hdev = &hdmitx_device;
	unsigned int DS_Type = 0;
	unsigned char AFR = 0;
	unsigned char VFR = 0;
	unsigned char Sync = 0;
	unsigned char  New = 0;
	unsigned char  End = 0;
	unsigned int Organzation_ID = 0;
	unsigned int Data_Set_Tag = 0;
	unsigned int Data_Set_Lemgth = 0;

	if (hdmitx_device.chip_type < MESON_CPU_ID_G12A) {
		pr_info("this chip doesn't support emp function\n");
		return;
	}

	if (!data) {
		pr_info("the data is null\n");
		return;
	}
	if (size <= 21) {
		number = 1;
		remainder = size;
	} else {
		number = ((size-21)/28) + 2;
		remainder = (size - 21) % 28;
	}

	virt_ptr = kzalloc(sizeof(unsigned char)*(number + 0x1f),
		GFP_KERNEL);
	if (virt_ptr == NULL)
		return;
	pr_info("emp_pkt virt_ptr: %p\n", virt_ptr);
	virt_ptr_align32bit = (unsigned char *)
		((((unsigned long)virt_ptr) + 0x1f) & (~0x1f));
	pr_info("emp_pkt virt_ptr_align32bit: %p\n", virt_ptr_align32bit);

	memset(virt_ptr_align32bit, 0, sizeof(unsigned char)*(number + 0x1f));

	switch (type) {
	case VENDOR_SPECIFIC_EM_DATA:
		break;
	case COMPRESS_VIDEO_TRAMSPORT:
		break;
	case HDR_DYNAMIC_METADATA:
			DS_Type = 1;
			Sync = 1;
			VFR = 1;
			AFR = 0;
			New = 0x1; /*todo*/
			End = 0x1; /*todo*/
			Organzation_ID = 2;
		break;
	case VIDEO_TIMING_EXTENDED:
		break;
	default:
		break;
	}

	for (i = 0; i < number; i++) {
		/*HB[0]-[2]*/
		virt_ptr_align32bit[i * 32 + 0] = 0x7F;
		if (i == 0)
			virt_ptr_align32bit[i * 32 + 1] |=  EMP_FIRST;
		if (i == number)
			virt_ptr_align32bit[i * 32 + 1] |= EMP_LAST;
		virt_ptr_align32bit[i * 32 + 2] = number;
		/*PB[0]-[6]*/
		if (i == 0) {
			virt_ptr_align32bit[3] = (New << 7) | (End << 6) |
				(DS_Type << 4) | (AFR << 3) |
				(VFR << 2) | (Sync << 1);
			virt_ptr_align32bit[4] = 0;/*Rsvd*/
			virt_ptr_align32bit[5] = Organzation_ID;
			virt_ptr_align32bit[6] = (Data_Set_Tag >> 8) & 0xFF;
			virt_ptr_align32bit[7] = Data_Set_Tag & 0xFF;
			virt_ptr_align32bit[8] = (Data_Set_Lemgth >> 8)
				& 0xFF;
			virt_ptr_align32bit[9] = Data_Set_Lemgth & 0xFF;
		}
		if (number == 1) {
			memcpy(&virt_ptr_align32bit[10], &data[0],
				sizeof(unsigned char) * remainder);
		} else {
			if ((i == 0)) {
			/*MD: first package need PB[7]-[27]*/
				memcpy(&virt_ptr_align32bit[10], &data[0],
					sizeof(unsigned char) * 21);
			} else if (i != number) {
			/*MD: following package need PB[0]-[27]*/
				memcpy(&virt_ptr_align32bit[i * 32 + 10],
					&data[(i - 1) * 28 + 21],
					sizeof(unsigned char) * 28);
			} else {
			/*MD: the last package need PB[0] to end */
				memcpy(&virt_ptr_align32bit[0],
					&data[(i - 1) * 28 + 21],
					sizeof(unsigned char) * remainder);
			}
		}
			/*PB[28]*/
		virt_ptr_align32bit[i * 32 + 31] = 0;
	}

	phys_ptr = virt_to_phys(virt_ptr_align32bit);
	pr_info("emp_pkt phys_ptr: %lx\n", phys_ptr);

	hdev->HWOp.CntlConfig(hdev, CONF_EMP_NUMBER, number);
	hdev->HWOp.CntlConfig(hdev, CONF_EMP_PHY_ADDR, phys_ptr);
}

/*config attr*/
static ssize_t show_config(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	unsigned char *conf;
	struct hdmitx_dev *hdev = &hdmitx_device;

	pos += snprintf(buf+pos, PAGE_SIZE, "cur_VIC: %d\n", hdev->cur_VIC);
	if (hdev->cur_video_param)
		pos += snprintf(buf+pos, PAGE_SIZE,
			"cur_video_param->VIC=%d\n",
			hdev->cur_video_param->VIC);
	if (hdev->para) {
		pos += snprintf(buf+pos, PAGE_SIZE, "cd = %d\n",
			hdev->para->cd);
		pos += snprintf(buf+pos, PAGE_SIZE, "cs = %d\n",
			hdev->para->cs);
	}

	switch (hdev->tx_aud_cfg) {
	case 0:
		conf = "off";
		break;
	case 1:
		conf = "on";
		break;
	case 2:
		conf = "auto";
		break;
	default:
		conf = "none";
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "audio config: %s\n", conf);

	switch (hdev->hdmi_audio_off_flag) {
	case 0:
		conf = "on";
		break;
	case 1:
		conf = "off";
		break;
	default:
		conf = "none";
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "audio on/off: %s\n", conf);

	switch (hdev->tx_aud_src) {
	case 0:
		conf = "SPDIF";
		break;
	case 1:
		conf = "I2S";
		break;
	default:
		conf = "none";
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "audio source: %s\n", conf);

	switch (hdev->cur_audio_param.type) {
	case CT_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
	case CT_PCM:
		conf = "L-PCM";
		break;
	case CT_AC_3:
		conf = "AC-3";
		break;
	case CT_MPEG1:
		conf = "MPEG1";
		break;
	case CT_MP3:
		conf = "MP3";
		break;
	case CT_MPEG2:
		conf = "MPEG2";
		break;
	case CT_AAC:
		conf = "AAC";
		break;
	case CT_DTS:
		conf = "DTS";
		break;
	case CT_ATRAC:
		conf = "ATRAC";
		break;
	case CT_ONE_BIT_AUDIO:
		conf = "One Bit Audio";
		break;
	case CT_DOLBY_D:
		conf = "Dolby Digital+";
		break;
	case CT_DTS_HD:
		conf = "DTS_HD";
		break;
	case CT_MAT:
		conf = "MAT";
		break;
	case CT_DST:
		conf = "DST";
		break;
	case CT_WMA:
		conf = "WMA";
		break;
	default:
		conf = "MAX";
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "audio type: %s\n", conf);

	switch (hdev->cur_audio_param.channel_num) {
	case CC_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
	case CC_2CH:
		conf = "2 channels";
		break;
	case CC_3CH:
		conf = "3 channels";
		break;
	case CC_4CH:
		conf = "4 channels";
		break;
	case CC_5CH:
		conf = "5 channels";
		break;
	case CC_6CH:
		conf = "6 channels";
		break;
	case CC_7CH:
		conf = "7 channels";
		break;
	case CC_8CH:
		conf = "8 channels";
		break;
	default:
		conf = "MAX";
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "audio channel num: %s\n", conf);

	switch (hdev->cur_audio_param.sample_rate) {
	case FS_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
	case FS_32K:
		conf = "32kHz";
		break;
	case FS_44K1:
		conf = "44.1kHz";
		break;
	case FS_48K:
		conf = "48kHz";
		break;
	case FS_88K2:
		conf = "88.2kHz";
		break;
	case FS_96K:
		conf = "96kHz";
		break;
	case FS_176K4:
		conf = "176.4kHz";
		break;
	case FS_192K:
		conf = "192kHz";
		break;
	case FS_768K:
		conf = "768kHz";
		break;
	default:
		conf = "MAX";
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "audio sample rate: %s\n", conf);

	switch (hdev->cur_audio_param.sample_size) {
	case SS_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
	case SS_16BITS:
		conf = "16bit";
		break;
	case SS_20BITS:
		conf = "20bit";
		break;
	case SS_24BITS:
		conf = "24bit";
		break;
	default:
		conf = "MAX";
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "audio sample size: %s\n", conf);

	if (hdev->flag_3dfp)
		conf = "FramePacking";
	else if (hdev->flag_3dss)
		conf = "SidebySide";
	else if (hdev->flag_3dtb)
		conf = "TopButtom";
	else
		conf = "off";
	pos += snprintf(buf+pos, PAGE_SIZE, "3D config: %s\n", conf);
	return pos;
}

static ssize_t store_config(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct master_display_info_s data = {0};
	struct hdr10plus_para hdr_data = {0x1, 0x2, 0x3};

	pr_info("hdmitx: config: %s\n", buf);

	if (strncmp(buf, "unplug_powerdown", 16) == 0) {
		if (buf[16] == '0')
			hdmitx_device.unplug_powerdown = 0;
		else
			hdmitx_device.unplug_powerdown = 1;
	} else if (strncmp(buf, "3d", 2) == 0) {
		/* Second, set 3D parameters */
		if (strncmp(buf+2, "tb", 2) == 0) {
			hdmitx_device.flag_3dtb = 1;
			hdmitx_device.flag_3dss = 0;
			hdmitx_device.flag_3dfp = 0;
			hdmi_set_3d(&hdmitx_device, T3D_TAB, 0);
		} else if ((strncmp(buf+2, "lr", 2) == 0) ||
			(strncmp(buf+2, "ss", 2) == 0)) {
			unsigned long sub_sample_mode = 0;

			hdmitx_device.flag_3dtb = 0;
			hdmitx_device.flag_3dss = 1;
			hdmitx_device.flag_3dfp = 0;
			if (buf[2])
				ret = kstrtoul(buf+2, 10,
					&sub_sample_mode);
			/* side by side */
			hdmi_set_3d(&hdmitx_device, T3D_SBS_HALF,
				sub_sample_mode);
		} else if (strncmp(buf+2, "fp", 2) == 0) {
			hdmitx_device.flag_3dtb = 0;
			hdmitx_device.flag_3dss = 0;
			hdmitx_device.flag_3dfp = 1;
			hdmi_set_3d(&hdmitx_device, T3D_FRAME_PACKING, 0);
		} else if (strncmp(buf+2, "off", 3) == 0) {
			hdmitx_device.flag_3dfp = 0;
			hdmitx_device.flag_3dtb = 0;
			hdmitx_device.flag_3dss = 0;
			hdmi_set_3d(&hdmitx_device, T3D_DISABLE, 0);
		}
	} else if (strncmp(buf, "sdr", 3) == 0) {
		data.features = 0x00010100;
		hdmitx_set_drm_pkt(&data);
	} else if (strncmp(buf, "hdr", 3) == 0) {
		data.features = 0x00091000;
		hdmitx_set_drm_pkt(&data);
	} else if (strncmp(buf, "hlg", 3) == 0) {
		data.features = 0x00091200;
		hdmitx_set_drm_pkt(&data);
	} else if (strncmp(buf, "vsif", 4) == 0)
		hdmitx_set_vsif_pkt(buf[4] - '0', buf[5] == '1', NULL);
	else if (strncmp(buf, "emp", 3) == 0) {
		if (hdmitx_device.chip_type >= MESON_CPU_ID_G12A)
			hdmitx_set_emp_pkt(NULL, 1, 1);
	} else if (strncmp(buf, "hdr10+", 6) == 0) {
		hdmitx_set_hdr10plus_pkt(1, &hdr_data);
	}
	return count;
}

void hdmitx_ext_set_audio_output(int enable)
{
	hdmitx_audio_mute_op(enable);
}

int hdmitx_ext_get_audio_status(void)
{
	return !!hdmitx_device.tx_aud_cfg;
}

void hdmitx_ext_set_i2s_mask(char ch_num, char ch_msk)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	static unsigned int update_flag = -1;

	if (!((ch_num == 2) || (ch_num == 4) || (ch_num == 6)
		|| (ch_num == 8))) {
		pr_info("err chn setting, must be 2, 4, 6 or 8, Rst as def\n");
		hdev->aud_output_ch = 0;
		if (update_flag != hdev->aud_output_ch) {
			update_flag = hdev->aud_output_ch;
			hdev->hdmi_ch = 0;
			hdmitx_set_audio(hdev, &(hdev->cur_audio_param));
		}
	}
	if (ch_msk == 0) {
		pr_info("err chn msk, must larger than 0\n");
		return;
	}
	hdev->aud_output_ch = (ch_num << 4) + ch_msk;
	if (update_flag != hdev->aud_output_ch) {
		update_flag = hdev->aud_output_ch;
		hdev->hdmi_ch = 0;
		hdmitx_set_audio(hdev, &(hdev->cur_audio_param));
	}
}

char hdmitx_ext_get_i2s_mask(void)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	return hdev->aud_output_ch & 0xf;
}

static ssize_t show_vid_mute(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n",
		atomic_read(&(hdmitx_device.kref_video_mute)));
	return pos;
}

static ssize_t store_vid_mute(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	atomic_t kref_video_mute = hdmitx_device.kref_video_mute;

	if (buf[0] == '1') {
		atomic_inc(&kref_video_mute);
		if (atomic_read(&kref_video_mute) == 1)
			hdmitx_video_mute_op(0);
	}
	if (buf[0] == '0') {
		if (!(atomic_sub_and_test(0, &kref_video_mute))) {
			atomic_dec(&kref_video_mute);
			if (atomic_sub_and_test(0, &kref_video_mute))
				hdmitx_video_mute_op(1);
		}
	}

	hdmitx_device.kref_video_mute = kref_video_mute;

	return count;
}

static ssize_t store_debug(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	hdmitx_device.HWOp.DebugFun(&hdmitx_device, buf);
	return count;
}

/* support format lists */
const char *disp_mode_t[] = {
#if 1
	"480i60hz",
	"576i50hz",
#endif
	"480p60hz",
	"576p50hz",
	"720p60hz",
	"1080i60hz",
	"1080p60hz",
	"720p50hz",
	"1080i50hz",
	"1080p30hz",
	"1080p50hz",
	"1080p25hz",
	"1080p24hz",
	"2160p30hz",
	"2160p25hz",
	"2160p24hz",
	"smpte24hz",
	"smpte25hz",
	"smpte30hz",
	"smpte50hz",
	"smpte60hz",
	"2160p50hz",
	"2160p60hz",
	NULL
};

static int is_4k50_fmt(char *mode)
{
	int i;
	static char const *hdmi4k50[] = {
		"2160p50hz",
		"2160p60hz",
		"smpte50hz",
		"smpte60hz",
		NULL
	};

	for (i = 0; hdmi4k50[i]; i++) {
		if (strcmp(hdmi4k50[i], mode) == 0)
		return 1;
	}
	return 0;
}

static int is_4k_fmt(char *mode)
{
	int i;
	static char const *hdmi4k[] = {
		"2160p",
		"smpte",
		NULL
	};

	for (i = 0; hdmi4k[i]; i++) {
		if (strstr(mode, hdmi4k[i]))
			return 1;
	}
	return 0;
}

/* below items has feature limited, may need extra judgement */
static bool hdmitx_limited_1080p(void)
{
	if (is_meson_gxl_package_805X())
		return 1;
	else if (is_meson_gxl_package_805Y())
		return 1;
	else
		return 0;
}

/**/
static ssize_t show_disp_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, pos = 0;
	const char *native_disp_mode =
		hdmitx_edid_get_native_VIC(&hdmitx_device);
	enum hdmi_vic vic;
	char mode_tmp[32];

	if (hdmitx_device.tv_no_edid) {
		pos += snprintf(buf+pos, PAGE_SIZE, "null edid\n");
	} else {
		for (i = 0; disp_mode_t[i]; i++) {
			memset(mode_tmp, 0, sizeof(mode_tmp));
			strncpy(mode_tmp, disp_mode_t[i], 31);
			if (hdmitx_limited_1080p() && is_4k_fmt(mode_tmp))
				continue;
			vic = hdmitx_edid_get_VIC(&hdmitx_device, mode_tmp, 0);
			/* Handling only 4k420 mode */
			if (vic == HDMI_Unknown) {
			if (is_4k50_fmt(mode_tmp)) {
				strcat(mode_tmp, "420");
				vic = hdmitx_edid_get_VIC(&hdmitx_device,
					mode_tmp, 0);
			}
		}
		if (vic != HDMI_Unknown) {
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

static ssize_t show_preferred_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct rx_cap *pRXCap = &hdmitx_device.RXCap;

	pos += snprintf(buf+pos, PAGE_SIZE, "%s\n",
		hdmitx_edid_vic_to_string(pRXCap->preferred_mode));

	return pos;
}

/**/
static int local_support_3dfp(enum hdmi_vic vic)
{
	switch (vic) {
	case HDMI_1280x720p50_16x9:
	case HDMI_1280x720p60_16x9:
	case HDMI_1920x1080p24_16x9:
	case HDMI_1920x1080p25_16x9:
	case HDMI_1920x1080p30_16x9:
	case HDMI_1920x1080p50_16x9:
	case HDMI_1920x1080p60_16x9:
		return 1;
	default:
		return 0;
	}
}
static ssize_t show_disp_cap_3d(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, pos = 0;
	int j = 0;
	enum hdmi_vic vic;

	pos += snprintf(buf+pos, PAGE_SIZE, "3D support lists:\n");
	for (i = 0; disp_mode_t[i]; i++) {
		/* 3D is not supported under 4k modes */
		if (strstr(disp_mode_t[i], "2160p") ||
			strstr(disp_mode_t[i], "smpte"))
			continue;
		vic = hdmitx_edid_get_VIC(&hdmitx_device,
			disp_mode_t[i], 0);
		for (j = 0; j < hdmitx_device.RXCap.VIC_count; j++) {
			if (vic == hdmitx_device.RXCap.VIC[j])
				break;
		}
		pos += snprintf(buf+pos, PAGE_SIZE, "\n%s ",
			disp_mode_t[i]);
		if (local_support_3dfp(vic)
			&& (hdmitx_device.RXCap.support_3d_format[
			hdmitx_device.RXCap.VIC[j]].frame_packing == 1)) {
			pos += snprintf(buf+pos, PAGE_SIZE,
				"FramePacking ");
		}
		if (hdmitx_device.RXCap.support_3d_format[
			hdmitx_device.RXCap.VIC[j]].top_and_bottom == 1) {
			pos += snprintf(buf+pos, PAGE_SIZE,
				"TopBottom ");
		}
		if (hdmitx_device.RXCap.support_3d_format[
			hdmitx_device.RXCap.VIC[j]].side_by_side == 1) {
			pos += snprintf(buf+pos, PAGE_SIZE,
				"SidebySide ");
		}
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "\n");

	return pos;
}

static void _show_pcm_ch(struct rx_cap *pRXCap, int i,
	int *ppos, char *buf)
{
	const char * const aud_sample_size[] = {"ReferToStreamHeader",
		"16", "20", "24", NULL};
	int j = 0;

	for (j = 0; j < 3; j++) {
		if (pRXCap->RxAudioCap[i].cc3 & (1 << j))
			*ppos += snprintf(buf + *ppos, PAGE_SIZE, "%s/",
				aud_sample_size[j+1]);
	}
	*ppos += snprintf(buf + *ppos - 1, PAGE_SIZE, " bit\n");
}

/**/
static ssize_t show_aud_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, pos = 0, j;
	static const char * const aud_ct[] =  {
		"ReferToStreamHeader", "PCM", "AC-3", "MPEG1", "MP3",
		"MPEG2", "AAC", "DTS", "ATRAC",	"OneBitAudio",
		"Dolby_Digital+", "DTS-HD", "MAT", "DST", "WMA_Pro",
		"Reserved", NULL};
	static const char * const aud_sampling_frequency[] = {
		"ReferToStreamHeader", "32", "44.1", "48", "88.2", "96",
		"176.4", "192", NULL};
	struct rx_cap *pRXCap = &(hdmitx_device.RXCap);

	pos += snprintf(buf + pos, PAGE_SIZE,
		"CodingType MaxChannels SamplingFreq SampleSize\n");
	for (i = 0; i < pRXCap->AUD_count; i++) {
		pos += snprintf(buf + pos, PAGE_SIZE, "%s",
			aud_ct[pRXCap->RxAudioCap[i].audio_format_code]);
		if ((pRXCap->RxAudioCap[i].audio_format_code == CT_DOLBY_D) &&
			(pRXCap->RxAudioCap[i].cc3 & 1))
			pos += snprintf(buf + pos, PAGE_SIZE, "/ATMOS");
		pos += snprintf(buf + pos, PAGE_SIZE, ", %d ch, ",
			pRXCap->RxAudioCap[i].channel_num_max + 1);
		for (j = 0; j < 7; j++) {
			if (pRXCap->RxAudioCap[i].freq_cc & (1 << j))
				pos += snprintf(buf + pos, PAGE_SIZE, "%s/",
					aud_sampling_frequency[j+1]);
		}
		pos += snprintf(buf + pos - 1, PAGE_SIZE, " kHz, ");
		switch (pRXCap->RxAudioCap[i].audio_format_code) {
		case CT_PCM:
			_show_pcm_ch(pRXCap, i, &pos, buf);
			break;
		case CT_AC_3:
		case CT_MPEG1:
		case CT_MP3:
		case CT_MPEG2:
		case CT_AAC:
		case CT_DTS:
		case CT_ATRAC:
		case CT_ONE_BIT_AUDIO:
			pos += snprintf(buf + pos, PAGE_SIZE,
				"MaxBitRate %dkHz\n",
				pRXCap->RxAudioCap[i].cc3 * 8);
			break;
		case CT_DOLBY_D:
		case CT_DTS_HD:
		case CT_MAT:
		case CT_DST:
			pos += snprintf(buf + pos, PAGE_SIZE, "DepVaule 0x%x\n",
				pRXCap->RxAudioCap[i].cc3);
			break;
		case CT_WMA:
		default:
			break;
		}
	}
	return pos;
}

/**/
static ssize_t show_dc_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	enum hdmi_vic vic = HDMI_Unknown;
	int pos = 0;
	struct rx_cap *pRXCap = &(hdmitx_device.RXCap);

#if 0
	if (pRXCap->dc_48bit_420)
		pos += snprintf(buf + pos, PAGE_SIZE, "420,16bit\n");
#endif
	if (pRXCap->dc_36bit_420)
		pos += snprintf(buf + pos, PAGE_SIZE, "420,12bit\n");
	if (pRXCap->dc_30bit_420) {
		pos += snprintf(buf + pos, PAGE_SIZE, "420,10bit\n");
		pos += snprintf(buf + pos, PAGE_SIZE, "420,8bit\n");
	} else {
		vic = hdmitx_edid_get_VIC(&hdmitx_device, "2160p60hz420", 0);
		if (vic != HDMI_Unknown) {
			pos += snprintf(buf + pos, PAGE_SIZE, "420,8bit\n");
			goto next444;
		}
		vic = hdmitx_edid_get_VIC(&hdmitx_device, "2160p50hz420", 0);
		if (vic != HDMI_Unknown) {
			pos += snprintf(buf + pos, PAGE_SIZE, "420,8bit\n");
			goto next444;
		}
		vic = hdmitx_edid_get_VIC(&hdmitx_device, "smpte60hz420", 0);
		if (vic != HDMI_Unknown) {
			pos += snprintf(buf + pos, PAGE_SIZE, "420,8bit\n");
			goto next444;
		}
		vic = hdmitx_edid_get_VIC(&hdmitx_device, "smpte50hz420", 0);
		if (vic != HDMI_Unknown) {
			pos += snprintf(buf + pos, PAGE_SIZE, "420,8bit\n");
			goto next444;
		}
	}
next444:
	if (pRXCap->dc_y444) {
		if (pRXCap->dc_36bit)
			pos += snprintf(buf + pos, PAGE_SIZE, "444,12bit\n");
		if (pRXCap->dc_30bit) {
			pos += snprintf(buf + pos, PAGE_SIZE, "444,10bit\n");
			pos += snprintf(buf + pos, PAGE_SIZE, "444,8bit\n");
		}
#if 0
		if (pRXCap->dc_48bit)
			pos += snprintf(buf + pos, PAGE_SIZE, "444,16bit\n");
#endif
		if (pRXCap->dc_36bit)
			pos += snprintf(buf + pos, PAGE_SIZE, "422,12bit\n");
		if (pRXCap->dc_30bit) {
			pos += snprintf(buf + pos, PAGE_SIZE, "422,10bit\n");
			pos += snprintf(buf + pos, PAGE_SIZE, "422,8bit\n");
			goto nextrgb;
		}
	} else {
		if (pRXCap->native_Mode & (1 << 5))
			pos += snprintf(buf + pos, PAGE_SIZE, "444,8bit\n");
		if (pRXCap->native_Mode & (1 << 4))
			pos += snprintf(buf + pos, PAGE_SIZE, "422,8bit\n");
	}
nextrgb:
#if 0
	if (pRXCap->dc_48bit)
		pos += snprintf(buf + pos, PAGE_SIZE, "rgb,16bit\n");
#endif
	if (pRXCap->dc_36bit)
		pos += snprintf(buf + pos, PAGE_SIZE, "rgb,12bit\n");
	if (pRXCap->dc_30bit)
		pos += snprintf(buf + pos, PAGE_SIZE, "rgb,10bit\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "rgb,8bit\n");
	return pos;
}

static bool valid_mode;
static char cvalid_mode[32];

static bool pre_process_str(char *name)
{
	int i;
	unsigned int flag = 0;
	char *color_format[4] = {"444", "422", "420", "rgb"};

	for (i = 0 ; i < 4 ; i++) {
		if (strstr(name, color_format[i]) != NULL)
			flag++;
	}
	if (flag >= 2)
		return 0;
	else
		return 1;
}

static ssize_t show_valid_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmi_format_para *para = NULL;

	if (cvalid_mode[0]) {
		valid_mode = pre_process_str(cvalid_mode);
		if (valid_mode == 0) {
			pos += snprintf(buf + pos, PAGE_SIZE, "%d\n\r",
				valid_mode);
			return pos;
		}
		para = hdmi_get_fmt_name(cvalid_mode, cvalid_mode);
	}
	if (para) {
		pr_info(SYS "sname = %s\n", para->sname);
		pr_info(SYS "char_clk = %d\n", para->tmds_clk);
		pr_info(SYS "cd = %d\n", para->cd);
		pr_info(SYS "cs = %d\n", para->cs);
	}

	valid_mode = hdmitx_edid_check_valid_mode(&hdmitx_device, para);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n\r", valid_mode);

	return pos;
}

static ssize_t store_valid_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	memset(cvalid_mode, 0, sizeof(cvalid_mode));
	strncpy(cvalid_mode, buf, sizeof(cvalid_mode));
	cvalid_mode[31] = '\0';
	return count;
}

static ssize_t show_allm_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = &hdmitx_device;
	struct rx_cap *pRXCap = &(hdmitx_device.RXCap);

	if (!pRXCap->allm) { /* Rx not support ALLM */
		pos += snprintf(buf + pos, PAGE_SIZE, "0\n\r");
		return pos;
	}

	if (hdev->allm_mode == 1)
		pos += snprintf(buf + pos, PAGE_SIZE, "game\n\r");
	else if (hdev->allm_mode == 2)
		pos += snprintf(buf + pos, PAGE_SIZE, "graphics\n\r");
	else if (hdev->allm_mode == 3)
		pos += snprintf(buf + pos, PAGE_SIZE, "photo\n\r");
	else if (hdev->allm_mode == 4)
		pos += snprintf(buf + pos, PAGE_SIZE, "cinema\n\r");
	else
		pos += snprintf(buf + pos, PAGE_SIZE, "0\n\r");

	return pos;
}

static ssize_t store_allm_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	struct rx_cap *pRXCap = &hdev->RXCap;

	pr_info("hdmitx: store allm_mode as %s\n", buf);

	if (!pRXCap->allm) /* Rx not support ALLM */
		return count;

#define CMP_STR(str)	(strncmp(buf, str, strlen(str)) == 0)
	if (CMP_STR("0")) {
		// disable ALLM
		hdev->allm_mode = 0;
		hdev->HWOp.CntlConfig(hdev, CONF_ALLM_MODE, CLEAR_ALLM_MODE);
		hdmitx_construct_vsif(hdev, VT_ALLM, 0, NULL);
		if (is_hdmi14_4k(hdev->cur_VIC))
			hdmitx_construct_vsif(hdev, VT_HDMI14_4K, 1, NULL);
		return count;
	}
	if (CMP_STR("1") || CMP_STR("game") || CMP_STR("graphics")
		|| CMP_STR("photo") || CMP_STR("cinema")) {
		hdmitx_construct_vsif(hdev, VT_ALLM, 1, NULL);
	}
	if (CMP_STR("1") || CMP_STR("game")) {
		hdev->allm_mode = 1;
		// enable the default GAME ALLM
		hdev->HWOp.CntlConfig(hdev, CONF_ALLM_MODE, SET_ALLM_GAME);
	}
	if (CMP_STR("graphics")) {
		hdev->allm_mode = 2;
		hdev->HWOp.CntlConfig(hdev, CONF_ALLM_MODE, SET_ALLM_GRAPHICS);
	}
	if (CMP_STR("photo")) {
		hdev->allm_mode = 3;
		hdev->HWOp.CntlConfig(hdev, CONF_ALLM_MODE, SET_ALLM_PHOTO);
	}
	if (CMP_STR("cinema")) {
		hdev->allm_mode = 4;
		hdev->HWOp.CntlConfig(hdev, CONF_ALLM_MODE, SET_ALLM_CINEMA);
	}

	return count;
}

/**/
static ssize_t show_hdr_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	unsigned int i, j;
	struct rx_cap *pRXCap = &(hdmitx_device.RXCap);

	pos += snprintf(buf + pos, PAGE_SIZE, "HDR Static Metadata:\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "    Supported EOTF:\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "        Traditional SDR: %d\n",
		pRXCap->hdr_sup_eotf_sdr);
	pos += snprintf(buf + pos, PAGE_SIZE, "        Traditional HDR: %d\n",
		pRXCap->hdr_sup_eotf_hdr);
	pos += snprintf(buf + pos, PAGE_SIZE, "        SMPTE ST 2084: %d\n",
		pRXCap->hdr_sup_eotf_smpte_st_2084);
	pos += snprintf(buf + pos, PAGE_SIZE, "        Hybrif Log-Gamma: %d\n",
		pRXCap->hdr_sup_eotf_hlg);
	pos += snprintf(buf + pos, PAGE_SIZE, "    Supported SMD type1: %d\n",
		pRXCap->hdr_sup_SMD_type1);
	pos += snprintf(buf + pos, PAGE_SIZE, "    Luminance Data\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "        Max: %d\n",
		pRXCap->hdr_lum_max);
	pos += snprintf(buf + pos, PAGE_SIZE, "        Avg: %d\n",
		pRXCap->hdr_lum_avg);
	pos += snprintf(buf + pos, PAGE_SIZE, "        Min: %d\n\n",
		pRXCap->hdr_lum_min);
	pos += snprintf(buf + pos, PAGE_SIZE, "HDR Dynamic Metadata:");

	for (i = 0; i < 4; i++) {
		if (pRXCap->hdr_dynamic_info[i].type == 0)
			continue;
		pos += snprintf(buf + pos, PAGE_SIZE,
			"\n    metadata_version: %x\n",
			pRXCap->hdr_dynamic_info[i].type);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"        support_flags: %x\n",
			pRXCap->hdr_dynamic_info[i].support_flags);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"        optional_fields:");
		for (j = 0; j <
			(pRXCap->hdr_dynamic_info[i].hd_len - 3); j++)
			pos += snprintf(buf + pos, PAGE_SIZE, " %x",
				pRXCap->hdr_dynamic_info[i].optional_fields[j]);
	}

	pos += snprintf(buf + pos, PAGE_SIZE, "\n\ncolorimetry_data: %x\n",
		pRXCap->colorimetry_data);

	return pos;
}

static ssize_t show_dv_cap(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	int i;
	const struct dv_info *dv = &(hdmitx_device.RXCap.dv_info);

	if (dv->ieeeoui != DV_IEEE_OUI) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"The Rx don't support DolbyVision\n");
		return pos;
	}
	if (dv->block_flag != CORRECT) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"DolbyVision block is error\n");
		return pos;
	}
	pos += snprintf(buf + pos, PAGE_SIZE,
		"DolbyVision RX support list:\n");

	if (dv->ver == 0) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"VSVDB Version: V%d\n", dv->ver);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"2160p%shz: 1\n",
			dv->sup_2160p60hz ? "60" : "30");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"Support mode:\n");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  DV_RGB_444_8BIT\n");
		if (dv->sup_yuv422_12bit)
			pos += snprintf(buf + pos, PAGE_SIZE,
				"  DV_YCbCr_422_12BIT\n");
	}
	if (dv->ver == 1) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"VSVDB Version: V%d(%d-byte)\n",
			dv->ver, dv->length + 1);
		if (dv->length == 0xB) {
			pos += snprintf(buf + pos, PAGE_SIZE,
				"2160p%shz: 1\n",
				dv->sup_2160p60hz ? "60" : "30");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"Support mode:\n");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  DV_RGB_444_8BIT\n");
		if (dv->sup_yuv422_12bit)
			pos += snprintf(buf + pos, PAGE_SIZE,
			"  DV_YCbCr_422_12BIT\n");
		if (dv->low_latency == 0x01)
			pos += snprintf(buf + pos, PAGE_SIZE,
				"  LL_YCbCr_422_12BIT\n");
		}

		if (dv->length == 0xE) {
			pos += snprintf(buf + pos, PAGE_SIZE,
				"2160p%shz: 1\n",
				dv->sup_2160p60hz ? "60" : "30");
			pos += snprintf(buf + pos, PAGE_SIZE,
				"Support mode:\n");
			pos += snprintf(buf + pos, PAGE_SIZE,
				"  DV_RGB_444_8BIT\n");
			if (dv->sup_yuv422_12bit)
				pos += snprintf(buf + pos, PAGE_SIZE,
				"  DV_YCbCr_422_12BIT\n");
		}
	}
	if (dv->ver == 2) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"VSVDB Version: V%d\n", dv->ver);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"2160p60hz: 1\n");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"Support mode:\n");
		if ((dv->Interface != 0x00) && (dv->Interface != 0x01)) {
			pos += snprintf(buf + pos, PAGE_SIZE,
				"  DV_RGB_444_8BIT\n");
			if (dv->sup_yuv422_12bit)
				pos += snprintf(buf + pos, PAGE_SIZE,
					"  DV_YCbCr_422_12BIT\n");
		}
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  LL_YCbCr_422_12BIT\n");
		if ((dv->Interface == 0x01) || (dv->Interface == 0x03)) {
			if (dv->sup_10b_12b_444 == 0x1) {
				pos += snprintf(buf + pos, PAGE_SIZE,
					"  LL_RGB_444_10BIT\n");
			}
			if (dv->sup_10b_12b_444 == 0x2) {
				pos += snprintf(buf + pos, PAGE_SIZE,
					"  LL_RGB_444_12BIT\n");
			}
		}
	}
	pos += snprintf(buf + pos, PAGE_SIZE,
		"IEEEOUI: 0x%06x\n", dv->ieeeoui);
	pos += snprintf(buf + pos, PAGE_SIZE, "VSVDB: ");
	for (i = 0; i < (dv->length + 1); i++)
		pos += snprintf(buf+pos, PAGE_SIZE, "%02x",
		dv->rawdata[i]);
	pos += snprintf(buf + pos, PAGE_SIZE, "\n");
	return pos;
}

static ssize_t show_aud_ch(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	   int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE,
		"hdmi_channel = %d ch\n",
		hdmitx_device.hdmi_ch ? hdmitx_device.hdmi_ch + 1 : 0);
	return pos;
}

static ssize_t store_aud_ch(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (strncmp(buf, "6ch", 3) == 0)
		hdmitx_device.hdmi_ch = 5;
	else if (strncmp(buf, "8ch", 3) == 0)
		hdmitx_device.hdmi_ch = 7;
	else if (strncmp(buf, "2ch", 3) == 0)
		hdmitx_device.hdmi_ch = 1;
	else
		return count;

	hdmitx_device.audio_param_update_flag = 1;
	hdmitx_device.force_audio_flag = 1;

	return count;
}

/*
 *  1: set avmute
 * -1: clear avmute
 *  0: off avmute
 */
static ssize_t store_avmute(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int cmd = OFF_AVMUTE;
	static int mask0;
	static int mask1;
	static DEFINE_MUTEX(avmute_mutex);

	pr_info("store_avmute %s\n", buf);
	mutex_lock(&avmute_mutex);
	if (strncmp(buf, "-1", 2) == 0) {
		cmd = CLR_AVMUTE;
		mask0 = -1;
	} else if (strncmp(buf, "0", 1) == 0) {
		cmd = OFF_AVMUTE;
		mask0 = 0;
	} else if (strncmp(buf, "1", 1) == 0) {
		cmd = SET_AVMUTE;
		mask0 = 1;
	}
	if (strncmp(buf, "r-1", 3) == 0) {
		cmd = CLR_AVMUTE;
		mask1 = -1;
	} else if (strncmp(buf, "r0", 2) == 0) {
		cmd = OFF_AVMUTE;
		mask1 = 0;
	} else if (strncmp(buf, "r1", 2) == 0) {
		cmd = SET_AVMUTE;
		mask1 = 1;
	}
	if ((mask0 == 1) || (mask1 == 1))
		cmd = SET_AVMUTE;
	else if ((mask0 == -1) && (mask1 == -1))
		cmd = CLR_AVMUTE;
	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_AVMUTE_OP, cmd);
	mutex_unlock(&avmute_mutex);

	return count;
}

static ssize_t show_avmute(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	int ret = 0;
	int pos = 0;

	ret = hdev->HWOp.CntlMisc(hdev, MISC_READ_AVMUTE_OP, 0);
	pos += snprintf(buf + pos, PAGE_SIZE, "%d", ret);

	return pos;
}

/*
 * 0: clear vic
 */
static ssize_t store_vic(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	if (strncmp(buf, "0", 1) == 0) {
		hdev->HWOp.CntlConfig(hdev, CONF_CLR_AVI_PACKET, 0);
		hdev->HWOp.CntlConfig(hdev, CONF_CLR_VSDB_PACKET, 0);
	}

	return count;
}

static ssize_t show_vic(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	enum hdmi_vic vic = HDMI_Unknown;
	int pos = 0;

	vic = hdev->HWOp.GetState(hdev, STAT_VIDEO_VIC, 0);
	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", vic);

	return pos;
}

/*
 *  1: enable hdmitx phy
 *  0: disable hdmitx phy
 */
static ssize_t store_phy(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int cmd = TMDS_PHY_ENABLE;

	pr_info(SYS "store_phy %s\n", buf);

	if (strncmp(buf, "0", 1) == 0)
		cmd = TMDS_PHY_DISABLE;
	else if (strncmp(buf, "1", 1) == 0)
		cmd = TMDS_PHY_ENABLE;
	else
		pr_info(SYS "set phy wrong: %s\n", buf);

	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_TMDS_PHY_OP, cmd);
	return count;
}

static ssize_t show_phy(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t store_rxsense_policy(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info(SYS "hdmitx: set rxsense_policy as %d\n", val);
		if ((val == 0) || (val == 1))
			hdmitx_device.rxsense_policy = val;
		else
			pr_info(SYS "only accept as 0 or 1\n");
	}
	if (hdmitx_device.rxsense_policy)
		queue_delayed_work(hdmitx_device.rxsense_wq,
			&hdmitx_device.work_rxsense, 0);
	else
		cancel_delayed_work(&hdmitx_device.work_rxsense);


	return count;
}

static ssize_t show_rxsense_policy(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdmitx_device.rxsense_policy);

	return pos;
}

static ssize_t store_sspll(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info(SYS "set sspll : %d\n", val);
		if ((val == 0) || (val == 1))
			hdmitx_device.sspll = val;
		else
			pr_info(SYS "sspll only accept as 0 or 1\n");
	}

	return count;
}

static ssize_t show_sspll(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdmitx_device.sspll);

	return pos;
}

static ssize_t store_frac_rate(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info(SYS "set frac_rate_policy as %d\n", val);
		if ((val == 0) || (val == 1))
			hdmitx_device.frac_rate_policy = val;
		else
			pr_info(SYS "only accept as 0 or 1\n");
	}

	return count;
}

static ssize_t show_frac_rate(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdmitx_device.frac_rate_policy);

	return pos;
}

static ssize_t store_hdcp_clkdis(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_HDCP_CLKDIS,
		buf[0] == '1' ? 1 : 0);
	return count;
}

static ssize_t show_hdcp_clkdis(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t store_hdcp_pwr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '1') {
		hdmitx_device.hdcp_tst_sig = 1;
		pr_info(SYS "set hdcp_pwr 1\n");
	}

	return count;
}

static ssize_t show_hdcp_pwr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	if (hdmitx_device.hdcp_tst_sig == 1) {
		pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
			hdmitx_device.hdcp_tst_sig);
		hdmitx_device.hdcp_tst_sig = 0;
		pr_info(SYS "restore hdcp_pwr 0\n");
	}

	return pos;
}

static ssize_t store_hdcp_byp(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	pr_info(SYS "store_hdcp_byp\n");

	hdmitx_device.HWOp.CntlMisc(&hdmitx_device, MISC_HDCP_CLKDIS,
		buf[0] == '1' ? 1 : 0);

	return count;
}

static ssize_t show_hdcp_lstore(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	/* if current TX is RP-TX, then return lstore as 00 */
	/* hdcp_lstore is used under only TX */
	if (hdmitx_device.repeater_tx == 1) {
		pos += snprintf(buf + pos, PAGE_SIZE, "00\n");
		return pos;
	}

	if (hdmitx_device.lstore < 0x10) {
		hdmitx_device.lstore = 0;
		if (hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_14_LSTORE, 0))
			hdmitx_device.lstore += 1;
		if (hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_22_LSTORE, 0))
			hdmitx_device.lstore += 2;
	}
	if (hdmitx_device.lstore & 0x1)
		pos += snprintf(buf + pos, PAGE_SIZE, "14\n");
	if (hdmitx_device.lstore & 0x2)
		pos += snprintf(buf + pos, PAGE_SIZE, "22\n");
	if ((hdmitx_device.lstore & 0xf) == 0)
		pos += snprintf(buf + pos, PAGE_SIZE, "00\n");
	return pos;
}

static ssize_t store_hdcp_lstore(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	pr_info("hdcp: set lstore as %s\n", buf);
	if (strncmp(buf, "0", 1) == 0)
		hdmitx_device.lstore = 0x10;
	if (strncmp(buf, "11", 2) == 0)
		hdmitx_device.lstore = 0x11;
	if (strncmp(buf, "12", 2) == 0)
		hdmitx_device.lstore = 0x12;
	if (strncmp(buf, "13", 2) == 0)
		hdmitx_device.lstore = 0x13;

	return count;
}

static int rptxlstore;
static ssize_t show_hdcp_rptxlstore(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	/* if current TX is not RP-TX, then return rptxlstore as 00 */
	/* hdcp_rptxlstore is used under only RP-TX */
	if (hdmitx_device.repeater_tx == 0) {
		pos += snprintf(buf + pos, PAGE_SIZE, "00\n");
		return pos;
	}

	if (rptxlstore < 0x10) {
		rptxlstore = 0;
		if (hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_14_LSTORE, 0))
			rptxlstore += 1;
		if (hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_22_LSTORE, 0))
			rptxlstore += 2;
	}
	if (rptxlstore & 0x1)
		pos += snprintf(buf + pos, PAGE_SIZE, "14\n");
	if (rptxlstore & 0x2)
		pos += snprintf(buf + pos, PAGE_SIZE, "22\n");
	if ((rptxlstore & 0xf) == 0)
		pos += snprintf(buf + pos, PAGE_SIZE, "00\n");
	return pos;
}

static ssize_t store_hdcp_rptxlstore(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	pr_info("hdcp: set lstore as %s\n", buf);
	if (strncmp(buf, "0", 1) == 0)
		rptxlstore = 0x10;
	if (strncmp(buf, "11", 2) == 0)
		rptxlstore = 0x11;
	if (strncmp(buf, "12", 2) == 0)
		rptxlstore = 0x12;
	if (strncmp(buf, "13", 2) == 0)
		rptxlstore = 0x13;

	return count;
}

static ssize_t show_div40(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", hdmitx_device.div40);

	return pos;
}

static ssize_t store_div40(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	hdev->HWOp.CntlDDC(hdev, DDC_SCDC_DIV40_SCRAMB, buf[0] == '1');
	hdmitx_device.div40 = (buf[0] == '1');

	return count;
}

static ssize_t show_hdcp_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	switch (hdmitx_device.hdcp_mode) {
	case 1:
		pos += snprintf(buf + pos, PAGE_SIZE, "14");
		break;
	case 2:
		pos += snprintf(buf + pos, PAGE_SIZE, "22");
		break;
	default:
		pos += snprintf(buf + pos, PAGE_SIZE, "off");
		break;
	}

	return pos;
}

static ssize_t store_hdcp_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
/* temporarily mark, no use SW I2C any more */
#if 0
	/* Issue SW I2C transaction to take advantage of SDA recovery logic */
	char tmp[8];

	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_PIN_MUX_OP, PIN_UNMUX);
	edid_rx_data(0x0, tmp, sizeof(tmp));
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_PIN_MUX_OP, PIN_MUX);
#endif

	pr_info(SYS "hdcp: set mode as %s\n", buf);
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_MUX_INIT, 1);
	if (strncmp(buf, "0", 1) == 0) {
		hdmitx_device.hdcp_mode = 0;
		hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_OP, HDCP14_OFF);
		hdmitx_hdcp_do_work(&hdmitx_device);
	}
	if (strncmp(buf, "1", 1) == 0) {
		hdmitx_device.hdcp_mode = 1;
		hdmitx_hdcp_do_work(&hdmitx_device);
		hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_OP, HDCP14_ON);
	}
	if (strncmp(buf, "2", 1) == 0) {
		hdmitx_device.hdcp_mode = 2;
		hdmitx_hdcp_do_work(&hdmitx_device);
		hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
			DDC_HDCP_MUX_INIT, 2);
	}

	return count;
}

static bool hdcp_sticky_mode;
static ssize_t show_hdcp_stickmode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n", hdcp_sticky_mode);

	return pos;
}

static ssize_t store_hdcp_stickmode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '0')
		hdcp_sticky_mode = 0;
	if (buf[0] == '1')
		hdcp_sticky_mode = 1;

	return count;
}

static unsigned char hdcp_sticky_step;
static ssize_t show_hdcp_stickstep(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%x\n", hdcp_sticky_step);
	if (hdcp_sticky_step)
		hdcp_sticky_step = 0;

	return pos;
}

static ssize_t store_hdcp_stickstep(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (isdigit(buf[0]))
		hdcp_sticky_step = buf[0] - '0';

	return count;
}

/* Indicate whether a rptx under repeater */
static ssize_t show_repeater_tx(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n",
		!!hdmitx_device.repeater_tx);

	return pos;
}

#include <linux/amlogic/media/vout/hdmi_tx/hdmi_rptx.h>

void direct_hdcptx14_opr(enum rptx_hdcp14_cmd cmd, void *args)
{
	int rst;
	struct hdmitx_dev *hdev = &hdmitx_device;

	pr_info("%s[%d] cmd: %d\n", __func__, __LINE__, cmd);
	switch (cmd) {
	case RPTX_HDCP14_OFF:
		hdev->hdcp_mode = 0;
		hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP, HDCP14_OFF);
		break;
	case RPTX_HDCP14_ON:
		hdev->hdcp_mode = 1;
		hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP, HDCP14_ON);
		break;
	case RPTX_HDCP14_GET_AUTHST:
		rst = hdev->HWOp.CntlDDC(hdev, DDC_HDCP_GET_AUTH, 0);
		*(int *)args = rst;
		break;
	}
}
EXPORT_SYMBOL(direct_hdcptx14_opr);

static ssize_t store_hdcp_ctrl(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	if (hdev->HWOp.CntlDDC(hdev, DDC_HDCP_14_LSTORE, 0) == 0)
		return count;

	/* for repeater */
	if (hdev->repeater_tx) {
		dev_warn(dev, "hdmitx20: %s\n", buf);
		if (strncmp(buf, "rstop", 5) == 0) {
			if (strncmp(buf+5, "14", 2) == 0)
				hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP,
					HDCP14_OFF);
			if (strncmp(buf+5, "22", 2) == 0)
				hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP,
					HDCP22_OFF);
			hdev->hdcp_mode = 0;
			hdmitx_hdcp_do_work(hdev);
		}
		return count;
	}
	/* for non repeater */
	if (strncmp(buf, "stop", 4) == 0) {
		dev_warn(dev, "hdmitx20: %s\n", buf);
		if (strncmp(buf+4, "14", 2) == 0)
			hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP, HDCP14_OFF);
		if (strncmp(buf+4, "22", 2) == 0)
			hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP, HDCP22_OFF);
		hdev->hdcp_mode = 0;
		hdmitx_hdcp_do_work(hdev);
	}

	return count;
}

static ssize_t show_hdcp_ctrl(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t show_hdcp_ksv_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0, i;
	char bksv_buf[5];

	hdmitx_device.HWOp.CntlDDC(&hdmitx_device, DDC_HDCP_GET_BKSV,
		(unsigned long int)bksv_buf);

	pos += snprintf(buf+pos, PAGE_SIZE, "HDCP14 BKSV: ");
	for (i = 0; i < 5; i++) {
		pos += snprintf(buf+pos, PAGE_SIZE, "%02x",
			bksv_buf[i]);
	}
	pos += snprintf(buf+pos, PAGE_SIZE, "  %s\n",
		hdcp_ksv_valid(bksv_buf) ? "Valid" : "Invalid");

	return pos;
}

/* Special FBC check */
static int check_fbc_special(unsigned char *edid_dat)
{
	if ((edid_dat[250] == 0xfb) && (edid_dat[251] == 0x0c))
		return 1;
	else
		return 0;
}

static ssize_t show_hdcp_ver(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	uint32_t ver = 0U;

	if (check_fbc_special(&hdmitx_device.EDID_buf[0])
	    || check_fbc_special(&hdmitx_device.EDID_buf1[0])) {
		pos += snprintf(buf+pos, PAGE_SIZE, "00\n\r");
		return pos;
	}

	/* if TX don't have HDCP22 key, skip RX hdcp22 ver */
	if (hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
		DDC_HDCP_22_LSTORE, 0) == 0)
		goto next;

	/* Detect RX support HDCP22 */
	mutex_lock(&getedid_mutex);
	ver = hdcp_rd_hdcp22_ver();
	mutex_unlock(&getedid_mutex);
	if (ver) {
		pos += snprintf(buf+pos, PAGE_SIZE, "22\n\r");
		pos += snprintf(buf+pos, PAGE_SIZE, "14\n\r");
		return pos;
	}
next:	/* Detect RX support HDCP14 */
	/* Here, must assume RX support HDCP14, otherwise affect 1A-03 */
	pos += snprintf(buf+pos, PAGE_SIZE, "14\n\r");
	return pos;

}

static ssize_t show_hpd_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d",
		hdmitx_device.hpd_state);
	return pos;
}


static ssize_t show_rhpd_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	int st;

	st = hdev->HWOp.CntlMisc(hdev, MISC_HPD_GPI_ST, 0);

	return snprintf(buf, PAGE_SIZE, "%d", hdev->rhpd_state);
}

static ssize_t show_max_exceed_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	return snprintf(buf, PAGE_SIZE, "%d", hdev->hdcp_max_exceed_state);
}

static ssize_t show_hdmi_init(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n\r", hdmitx_device.hdmi_init);
	return pos;
}

static ssize_t show_ready(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\r\n",
		hdmitx_device.ready);
	return pos;
}

static ssize_t store_ready(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (strncmp(buf, "0", 1) == 0)
		hdmitx_device.ready = 0;
	if (strncmp(buf, "1", 1) == 0)
		hdmitx_device.ready = 1;
	return count;
}

static ssize_t show_support_3d(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%d\n",
		hdmitx_device.RXCap.threeD_present);
	return pos;
}

static DEVICE_ATTR(disp_mode, 0664, show_disp_mode, store_disp_mode);
static DEVICE_ATTR(attr, 0664, show_attr, store_attr);
static DEVICE_ATTR(aud_mode, 0644, show_aud_mode, store_aud_mode);
static DEVICE_ATTR(vid_mute, 0644, show_vid_mute, store_vid_mute);
static DEVICE_ATTR(edid, 0644, show_edid, store_edid);
static DEVICE_ATTR(rawedid, 0444, show_rawedid, NULL);
static DEVICE_ATTR(sink_type, 0444, show_sink_type, NULL);
static DEVICE_ATTR(edid_parsing, 0444, show_edid_parsing, NULL);
static DEVICE_ATTR(config, 0664, show_config, store_config);
static DEVICE_ATTR(debug, 0200, NULL, store_debug);
static DEVICE_ATTR(disp_cap, 0444, show_disp_cap, NULL);
static DEVICE_ATTR(preferred_mode, 0444, show_preferred_mode, NULL);
static DEVICE_ATTR(aud_cap, 0444, show_aud_cap, NULL);
static DEVICE_ATTR(hdr_cap, 0444, show_hdr_cap, NULL);
static DEVICE_ATTR(dv_cap, 0444, show_dv_cap, NULL);
static DEVICE_ATTR(dc_cap, 0444, show_dc_cap, NULL);
static DEVICE_ATTR(valid_mode, 0664, show_valid_mode, store_valid_mode);
static DEVICE_ATTR(allm_mode, 0664, show_allm_mode, store_allm_mode);
static DEVICE_ATTR(aud_ch, 0664, show_aud_ch, store_aud_ch);
static DEVICE_ATTR(avmute, 0664, show_avmute, store_avmute);
static DEVICE_ATTR(swap, 0644, show_swap, store_swap);
static DEVICE_ATTR(vic, 0664, show_vic, store_vic);
static DEVICE_ATTR(phy, 0664, show_phy, store_phy);
static DEVICE_ATTR(sspll, 0664, show_sspll, store_sspll);
static DEVICE_ATTR(frac_rate_policy, 0664, show_frac_rate, store_frac_rate);
static DEVICE_ATTR(rxsense_policy, 0644, show_rxsense_policy,
	store_rxsense_policy);
static DEVICE_ATTR(hdcp_clkdis, 0664, show_hdcp_clkdis, store_hdcp_clkdis);
static DEVICE_ATTR(hdcp_pwr, 0664, show_hdcp_pwr, store_hdcp_pwr);
static DEVICE_ATTR(hdcp_byp, 0200, NULL, store_hdcp_byp);
static DEVICE_ATTR(hdcp_mode, 0664, show_hdcp_mode, store_hdcp_mode);
static DEVICE_ATTR(hdcp_lstore, 0664, show_hdcp_lstore, store_hdcp_lstore);
static DEVICE_ATTR(hdcp_rptxlstore, 0664, show_hdcp_rptxlstore,
	store_hdcp_rptxlstore);
static DEVICE_ATTR(hdcp_repeater, 0644, show_hdcp_repeater,
	store_hdcp_repeater);
static DEVICE_ATTR(hdcp_topo_info, 0644, show_hdcp_topo_info,
	store_hdcp_topo_info);
static DEVICE_ATTR(hdcp22_type, 0644, show_hdcp22_type, store_hdcp22_type);
static DEVICE_ATTR(hdcp_stickmode, 0664, show_hdcp_stickmode,
	store_hdcp_stickmode);
static DEVICE_ATTR(hdcp_stickstep, 0664, show_hdcp_stickstep,
	store_hdcp_stickstep);
static DEVICE_ATTR(hdmi_repeater_tx, 0444, show_repeater_tx, NULL);
static DEVICE_ATTR(hdcp22_base, 0444, show_hdcp22_base, NULL);
static DEVICE_ATTR(div40, 0664, show_div40, store_div40);
static DEVICE_ATTR(hdcp_ctrl, 0664, show_hdcp_ctrl, store_hdcp_ctrl);
static DEVICE_ATTR(disp_cap_3d, 0444, show_disp_cap_3d, NULL);
static DEVICE_ATTR(hdcp_ksv_info, 0444, show_hdcp_ksv_info, NULL);
static DEVICE_ATTR(hdcp_ver, 0444, show_hdcp_ver, NULL);
static DEVICE_ATTR(hpd_state, 0444, show_hpd_state, NULL);
static DEVICE_ATTR(rhpd_state, 0444, show_rhpd_state, NULL);
static DEVICE_ATTR(max_exceed, 0444, show_max_exceed_state, NULL);
static DEVICE_ATTR(hdmi_init, 0444, show_hdmi_init, NULL);
static DEVICE_ATTR(ready, 0664, show_ready, store_ready);
static DEVICE_ATTR(support_3d, 0444, show_support_3d, NULL);

static struct vinfo_s *hdmitx_vinfo;
static struct vinfo_s *hdmitx_get_current_vinfo(void)
{
	return hdmitx_vinfo;
}

static int hdmitx_set_current_vmode(enum vmode_e mode)
{
	struct vinfo_s *vinfo;

	pr_info("%s[%d]\n", __func__, __LINE__);
	/* get current vinfo and refesh */
	vinfo = hdmitx_get_current_vinfo();
	if ((vinfo != NULL) && (vinfo->name != NULL))
		recalc_vinfo_sync_duration(vinfo,
			hdmitx_device.frac_rate_policy);
#if defined(CONFIG_ARCH_MESON64_ODROID_COMMON)
	if (!(mode & VMODE_INIT_BIT_MASK) && get_hpd_state())
#else
	if (!(mode & VMODE_INIT_BIT_MASK))
#endif
		set_disp_mode_auto();
	else
		pr_info("alread display in uboot\n");

	return 0;
}

static enum vmode_e hdmitx_validate_vmode(char *mode)
{
	struct vinfo_s *info = hdmi_get_valid_vinfo(mode);

	if (info) {
		hdmitx_vinfo = info;
		hdmitx_vinfo->info_3d = NON_3D;
		if (hdmitx_device.flag_3dfp)
			hdmitx_vinfo->info_3d = FP_3D;

		if (hdmitx_device.flag_3dtb)
			hdmitx_vinfo->info_3d = TB_3D;

		if (hdmitx_device.flag_3dss)
			hdmitx_vinfo->info_3d = SS_3D;

		hdmitx_vinfo->vout_device = &hdmitx_vdev;
		return VMODE_HDMI;
	}
	return VMODE_MAX;
}

static int hdmitx_vmode_is_supported(enum vmode_e mode)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_HDMI)
		return true;
	else
		return false;
}

static int hdmitx_module_disable(enum vmode_e cur_vmod)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	hdev->HWOp.CntlConfig(hdev, CONF_CLR_AVI_PACKET, 0);
	hdev->HWOp.CntlConfig(hdev, CONF_CLR_VSDB_PACKET, 0);
	hdev->HWOp.CntlMisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	if (hdev->para->hdmitx_vinfo.viu_mux == VIU_MUX_ENCI)
		hdmitx_disable_vclk2_enci(hdev);
	hdev->para = hdmi_get_fmt_name("invalid", hdev->fmt_attr);
	hdmitx_validate_vmode("null");

	return 0;
}

static int hdmitx_vout_state;
static int hdmitx_vout_set_state(int index)
{
	hdmitx_vout_state |= (1 << index);
	return 0;
}

static int hdmitx_vout_clr_state(int index)
{
	hdmitx_vout_state &= ~(1 << index);
	return 0;
}

static int hdmitx_vout_get_state(void)
{
	return hdmitx_vout_state;
}

static struct vout_server_s hdmitx_vout_server = {
	.name = "hdmitx_vout_server",
	.op = {
		.get_vinfo = hdmitx_get_current_vinfo,
		.set_vmode = hdmitx_set_current_vmode,
		.validate_vmode = hdmitx_validate_vmode,
		.vmode_is_supported = hdmitx_vmode_is_supported,
		.disable = hdmitx_module_disable,
		.set_state = hdmitx_vout_set_state,
		.clr_state = hdmitx_vout_clr_state,
		.get_state = hdmitx_vout_get_state,
		.set_bist = NULL,
#ifdef CONFIG_PM
		.vout_suspend = NULL,
		.vout_resume = NULL,
#endif
	},
};

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static struct vout_server_s hdmitx_vout2_server = {
	.name = "hdmitx_vout2_server",
	.op = {
		.get_vinfo = hdmitx_get_current_vinfo,
		.set_vmode = hdmitx_set_current_vmode,
		.validate_vmode = hdmitx_validate_vmode,
		.vmode_is_supported = hdmitx_vmode_is_supported,
		.disable = hdmitx_module_disable,
		.set_state = hdmitx_vout_set_state,
		.clr_state = hdmitx_vout_clr_state,
		.get_state = hdmitx_vout_get_state,
		.set_bist = NULL,
#ifdef CONFIG_PM
		.vout_suspend = NULL,
		.vout_resume = NULL,
#endif
	},
};
#endif


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
		if (map_fs[i].rate == rate)
			return map_fs[i].fs;
	}
	pr_info(AUD "get FS_MAX\n");
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
		if (bits == aud_size_map_ss[i].sample_bits)
			return aud_size_map_ss[i].ss;
	}
	pr_info(AUD "get SS_MAX\n");
	return SS_MAX;
}

static int hdmitx_notify_callback_a(struct notifier_block *block,
	unsigned long cmd, void *para);
static struct notifier_block hdmitx_notifier_nb_a = {
	.notifier_call	= hdmitx_notify_callback_a,
};
static int hdmitx_notify_callback_a(struct notifier_block *block,
	unsigned long cmd, void *para)
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
	pr_info(AUD "aout notify format %s\n",
		aud_type_string[audio_param->type & 0xff]);
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
		pr_info(AUD "auto mode\n");
	/* Detect whether Rx is support current audio format */
	for (i = 0; i < pRXCap->AUD_count; i++) {
		if (pRXCap->RxAudioCap[i].audio_format_code == cmd)
			audio_check = 1;
	}
	/* sink don't support current audio mode */
	if ((!audio_check) && (cmd != CT_PCM)) {
		pr_info("Sink not support this audio format %lu\n",
			cmd);
		hdmitx_device.HWOp.CntlConfig(&hdmitx_device,
			CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
		hdmitx_device.audio_param_update_flag = 0;
	}
	}
	if (hdmitx_device.audio_param_update_flag == 0)
		;
	else
		hdmitx_device.audio_notify_flag = 1;


	if ((!(hdmitx_device.hdmi_audio_off_flag)) &&
		(hdmitx_device.audio_param_update_flag)) {
		/* plug-in & update audio param */
		if (hdmitx_device.hpd_state == 1) {
			hdmitx_set_audio(&hdmitx_device,
				&(hdmitx_device.cur_audio_param));
		if ((hdmitx_device.audio_notify_flag == 1) ||
			(hdmitx_device.audio_step == 1)) {
			hdmitx_device.audio_notify_flag = 0;
			hdmitx_device.audio_step = 0;
		}
		hdmitx_device.audio_param_update_flag = 0;
		pr_info(AUD "set audio param\n");
	}
	}

	return 0;
}

static void hdmitx_get_edid(struct hdmitx_dev *hdev)
{
	mutex_lock(&getedid_mutex);
	/* TODO hdmitx_edid_ram_buffer_clear(hdev); */
	hdev->HWOp.CntlDDC(hdev, DDC_RESET_EDID, 0);
	hdev->HWOp.CntlDDC(hdev, DDC_PIN_MUX_OP, PIN_MUX);
	/* start reading edid frist time */
	hdev->HWOp.CntlDDC(hdev, DDC_EDID_READ_DATA, 0);
	hdev->HWOp.CntlDDC(hdev, DDC_EDID_GET_DATA, 0);
	/* If EDID is not correct at first time, then retry */
	if (!check_dvi_hdmi_edid_valid(hdev->EDID_buf)) {
		msleep(100);
		/* start reading edid second time */
		hdev->HWOp.CntlDDC(hdev, DDC_EDID_READ_DATA, 0);
		hdev->HWOp.CntlDDC(hdev, DDC_EDID_GET_DATA, 1);
	}
	hdmitx_edid_clear(hdev);
	hdmitx_edid_parse(hdev);
	hdmitx_edid_buf_compare_print(hdev);
	mutex_unlock(&getedid_mutex);
}

static void hdmitx_rxsense_process(struct work_struct *work)
{
	int sense;
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_rxsense);

	sense = hdev->HWOp.CntlMisc(hdev, MISC_TMDS_RXSENSE, 0);
	extcon_set_state_sync(hdmitx_extcon_rxsense, EXTCON_DISP_HDMI, sense);
	queue_delayed_work(hdev->rxsense_wq, &hdev->work_rxsense, HZ);
}

static void hdmitx_hpd_plugin_handler(struct work_struct *work)
{
	char bksv_buf[5];
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_hpd_plugin);

	if (!(hdev->hdmitx_event & (HDMI_TX_HPD_PLUGIN)))
		return;
	if (hdev->rxsense_policy) {
		cancel_delayed_work(&hdev->work_rxsense);
		queue_delayed_work(hdev->rxsense_wq, &hdev->work_rxsense, 0);
		while (!(hdmitx_extcon_rxsense->state))
			msleep_interruptible(1000);
	}
	mutex_lock(&setclk_mutex);
	pr_info(SYS "plugin\n");
	hdev->HWOp.CntlMisc(hdev, MISC_I2C_REACTIVE, 0);
	hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGIN;
	/* start reading E-EDID */
	if (hdev->repeater_tx)
		rx_repeat_hpd_state(1);
	hdmitx_get_edid(hdev);
	hdmi_physcial_size_update(hdev);
#if defined(CONFIG_ARCH_MESON64_ODROID_COMMON)
	if (odroid_voutmode() == VOUTMODE_HDMI) {
		pr_info(VID "Sink is HDMI device\n");
		hdev->HWOp.CntlConfig(hdev,
			CONF_HDMI_DVI_MODE, HDMI_MODE);
	} else if (odroid_voutmode() == VOUTMODE_DVI) {
		pr_info(VID "Sink is DVI device\n");
		hdev->HWOp.CntlConfig(hdev,
			CONF_HDMI_DVI_MODE, DVI_MODE);
	} else
#endif
{
	if (hdev->RXCap.ieeeoui != HDMI_IEEEOUI)
		hdev->HWOp.CntlConfig(hdev,
			CONF_HDMI_DVI_MODE, DVI_MODE);
	else
		hdev->HWOp.CntlConfig(hdev,
			CONF_HDMI_DVI_MODE, HDMI_MODE);
}

	mutex_lock(&getedid_mutex);
	mutex_unlock(&getedid_mutex);
	if (hdev->repeater_tx) {
		if (check_fbc_special(&hdev->EDID_buf[0])
			|| check_fbc_special(&hdev->EDID_buf1[0]))
			rx_set_repeater_support(0);
		else
			rx_set_repeater_support(1);
		hdev->HWOp.CntlDDC(hdev, DDC_HDCP_GET_BKSV,
			(unsigned long int)bksv_buf);
		rx_set_receive_hdcp(bksv_buf, 1, 1, 0, 0);
	}

	set_disp_mode_auto();
	hdmitx_set_audio(hdev, &(hdev->cur_audio_param));
	hdev->hpd_state = 1;
	hdmitx_notify_hpd(hdev->hpd_state);

	extcon_set_state_sync(hdmitx_extcon_hdmi, EXTCON_DISP_HDMI, 1);
	extcon_set_state_sync(hdmitx_extcon_audio, EXTCON_DISP_HDMI, 1);
	mutex_unlock(&setclk_mutex);
}

static void clear_rx_vinfo(struct hdmitx_dev *hdev)
{
	struct vinfo_s *info = hdmitx_get_current_vinfo();

	if (info) {
		memset(&info->hdr_info, 0, sizeof(info->hdr_info));
		memset(&info->rx_latency, 0, sizeof(info->rx_latency));
	}
}

static void hdmitx_hpd_plugout_handler(struct work_struct *work)
{
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_hpd_plugout);

	if (!(hdev->hdmitx_event & (HDMI_TX_HPD_PLUGOUT)))
		return;
	hdev->hdcp_mode = 0;
	hdev->hdcp_bcaps_repeater = 0;
	hdev->HWOp.CntlDDC(hdev, DDC_HDCP_MUX_INIT, 1);
	hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP, HDCP14_OFF);
	mutex_lock(&setclk_mutex);
	pr_info(SYS "plugout\n");
	if (!!(hdev->HWOp.CntlMisc(hdev, MISC_HPD_GPI_ST, 0))) {
		pr_info(SYS "hpd gpio high\n");
		hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGOUT;
		mutex_unlock(&setclk_mutex);
		return;
	}
	/*after plugout, DV mode can't be supported*/
	hdmitx_set_vsif_pkt(0, 0, NULL);
	hdmitx_set_hdr10plus_pkt(0, NULL);
	hdev->ready = 0;
	if (hdev->repeater_tx)
		rx_repeat_hpd_state(0);
	hdev->HWOp.CntlConfig(hdev, CONF_CLR_AVI_PACKET, 0);
	hdev->HWOp.CntlDDC(hdev, DDC_HDCP_MUX_INIT, 1);
	hdev->HWOp.CntlDDC(hdev, DDC_HDCP_OP, HDCP14_OFF);
	hdev->HWOp.CntlDDC(hdev, DDC_HDCP_SET_TOPO_INFO, 0);
	hdev->HWOp.CntlMisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGOUT;
	hdev->HWOp.CntlMisc(hdev, MISC_ESM_RESET, 0);
	clear_rx_vinfo(hdev);
	rx_edid_physical_addr(0, 0, 0, 0);
	hdmitx_edid_clear(hdev);
	hdmi_physcial_size_update(hdev);
	hdmitx_edid_ram_buffer_clear(hdev);
	hdev->hpd_state = 0;
	hdmitx_notify_hpd(hdev->hpd_state);
	extcon_set_state_sync(hdmitx_extcon_hdmi, EXTCON_DISP_HDMI, 0);
	extcon_set_state_sync(hdmitx_extcon_audio, EXTCON_DISP_HDMI, 0);
	mutex_unlock(&setclk_mutex);
}

static void hdmitx_internal_intr_handler(struct work_struct *work)
{
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_internal_intr);

	hdev->HWOp.DebugFun(hdev, "dumpintr");
}

int get_hpd_state(void)
{
	int ret;

	mutex_lock(&setclk_mutex);
	ret = hdmitx_device.hpd_state;
	mutex_unlock(&setclk_mutex);

	return ret;
}
EXPORT_SYMBOL(get_hpd_state);

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

	hdmitx_extcon_hdmi->state = !!(hdmitx_device->HWOp.CntlMisc(
		hdmitx_device, MISC_HPD_GPI_ST, 0));
	hdmitx_device->hpd_state = hdmitx_extcon_hdmi->state;
	hdmitx_notify_hpd(hdmitx_device->hpd_state);
	extcon_set_state_sync(hdmitx_extcon_power, EXTCON_DISP_HDMI,
						hdmitx_device->hpd_state);
	INIT_WORK(&hdmitx_device->work_hdr, hdr_work_func);

/* When init hdmi, clear the hdmitx module edid ram and edid buffer. */
	hdmitx_edid_clear(hdmitx_device);
	hdmitx_edid_ram_buffer_clear(hdmitx_device);
	hdmitx_device->hdmi_wq = alloc_workqueue(DEVICE_NAME,
		WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	INIT_DELAYED_WORK(&hdmitx_device->work_hpd_plugin,
		hdmitx_hpd_plugin_handler);
	INIT_DELAYED_WORK(&hdmitx_device->work_hpd_plugout,
		hdmitx_hpd_plugout_handler);
	INIT_DELAYED_WORK(&hdmitx_device->work_internal_intr,
		hdmitx_internal_intr_handler);

	/* for rx sense feature */
	hdmitx_device->rxsense_wq = alloc_workqueue(hdmitx_extcon_rxsense->name,
		WQ_SYSFS | WQ_FREEZABLE, 0);
	INIT_DELAYED_WORK(&hdmitx_device->work_rxsense, hdmitx_rxsense_process);

	hdmitx_device->tx_aud_cfg = 1; /* default audio configure is on */

	/*Direct Rander Management use another irq*/
	if (hdmitx_device->drm_feature == 0)
		hdmitx_device->HWOp.SetupIRQ(hdmitx_device);

	/* Trigger HDMITX IRQ*/
	hdmitx_device->HWOp.CntlMisc(hdmitx_device, MISC_HPD_MUX_OP, PIN_UNMUX);
	mdelay(20);
	hdmitx_device->HWOp.CntlMisc(hdmitx_device, MISC_HPD_MUX_OP, PIN_MUX);

	hdmitx_device->hdmi_init = 1;
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
	return 0;
}

static const struct file_operations amhdmitx_fops = {
	.owner	= THIS_MODULE,
	.open	 = amhdmitx_open,
	.release  = amhdmitx_release,
};

struct hdmitx_dev *get_hdmitx_device(void)
{
	return &hdmitx_device;
}
EXPORT_SYMBOL(get_hdmitx_device);

static int get_dt_vend_init_data(struct device_node *np,
	struct vendor_info_data *vend)
{
	int ret;

	ret = of_property_read_string(np, "vendor_name",
		(const char **)&(vend->vendor_name));
	if (ret)
		pr_info(SYS "not find vendor name\n");

	ret = of_property_read_u32(np, "vendor_id", &(vend->vendor_id));
	if (ret)
		pr_info(SYS "not find vendor id\n");

	ret = of_property_read_string(np, "product_desc",
		(const char **)&(vend->product_desc));
	if (ret)
		pr_info(SYS "not find product desc\n");
	return 0;
}

static void hdmitx_init_fmt_attr(struct hdmitx_dev *hdev)
{
	if (strlen(hdev->fmt_attr) >= 8) {
		pr_info(SYS "fmt_attr %s\n", hdev->fmt_attr);
		return;
	}
	if ((hdev->para->cd == COLORDEPTH_RESERVED) &&
	    (hdev->para->cs == COLORSPACE_RESERVED)) {
		strcpy(hdev->fmt_attr, "default");
	} else {
		switch (hdev->para->cs) {
		case COLORSPACE_RGB444:
			memcpy(hdev->fmt_attr, "rgb,", 4);
			break;
		case COLORSPACE_YUV422:
			memcpy(hdev->fmt_attr, "422,", 4);
			break;
		case COLORSPACE_YUV444:
			memcpy(hdev->fmt_attr, "444,", 4);
			break;
		case COLORSPACE_YUV420:
			memcpy(hdev->fmt_attr, "420,", 4);
			break;
		default:
			break;
		}
		switch (hdev->para->cd) {
		case COLORDEPTH_24B:
			strcat(hdev->fmt_attr, "8bit");
			break;
		case COLORDEPTH_30B:
			strcat(hdev->fmt_attr, "10bit");
			break;
		case COLORDEPTH_36B:
			strcat(hdev->fmt_attr, "12bit");
			break;
		case COLORDEPTH_48B:
			strcat(hdev->fmt_attr, "16bit");
			break;
		default:
			break;
		}
	}
	pr_info(SYS "fmt_attr %s\n", hdev->fmt_attr);
}

/* for notify to cec */
static BLOCKING_NOTIFIER_HEAD(hdmitx_event_notify_list);
int hdmitx_event_notifier_regist(struct notifier_block *nb)
{
	int ret = 0;

	if (!nb)
		return ret;

	ret = blocking_notifier_chain_register(&hdmitx_event_notify_list, nb);
	/* update status when register */
	if (!ret && nb->notifier_call) {
		hdmitx_notify_hpd(hdmitx_device.hpd_state);
		if (hdmitx_device.physical_addr != 0xffff)
			hdmitx_event_notify(HDMITX_PHY_ADDR_VALID,
					    &hdmitx_device.physical_addr);
	}

	return ret;
}
EXPORT_SYMBOL(hdmitx_event_notifier_regist);

int hdmitx_event_notifier_unregist(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_unregister(&hdmitx_event_notify_list, nb);

	return ret;
}
EXPORT_SYMBOL(hdmitx_event_notifier_unregist);

void hdmitx_event_notify(unsigned long state, void *arg)
{
	blocking_notifier_call_chain(&hdmitx_event_notify_list, state, arg);
}

void hdmitx_hdcp_status(int hdmi_authenticated)
{
	extcon_set_state_sync(hdmitx_extcon_hdcp, EXTCON_DISP_HDMI,
							hdmi_authenticated);
}

void hdmitx_extcon_register(struct platform_device *pdev, struct device *dev)
{
	struct extcon_dev *edev;
	int ret;

	/*hdmitx extcon hdmi*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		pr_info(SYS "failed to allocate hdmitx extcon hdmi\n");
		return;
	}
	edev->dev.parent = dev;
	edev->name = "hdmitx_extcon_hdmi";
	dev_set_name(&edev->dev, "hdmi");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		pr_info(SYS "failed to register hdmitx extcon hdmi\n");
		return;
	}
	hdmitx_extcon_hdmi = edev;

	/*hdmitx extcon audio*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		pr_info(SYS "failed to allocate hdmitx extcon audio\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_extcon_audio";
	dev_set_name(&edev->dev, "hdmi_audio");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		pr_info(SYS "failed to register hdmitx extcon audio\n");
		return;
	}
	hdmitx_extcon_audio = edev;

	/*hdmitx extcon power*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		pr_info(SYS "failed to allocate hdmitx extcon power\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_extcon_power";
	dev_set_name(&edev->dev, "hdmi_power");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		pr_info(SYS "failed to register extcon power\n");
		return;
	}
	hdmitx_extcon_power = edev;

	/*hdmitx extcon hdr*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		pr_info(SYS "failed to allocate hdmitx extcon hdr\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_extcon_hdr";
	dev_set_name(&edev->dev, "hdmi_hdr");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		pr_info(SYS "failed to register hdmitx extcon hdr\n");
		return;
	}
	hdmitx_extcon_hdr = edev;

	/*hdmitx extcon rxsense*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		pr_info(SYS "failed to allocate extcon rxsense\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_extcon_rxsense";
	dev_set_name(&edev->dev, "hdmi_rxsense");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		pr_info(SYS "failed to register extcon rxsense\n");
		return;
	}
	hdmitx_extcon_rxsense = edev;

	/*hdmitx extcon hdcp*/
	edev = extcon_dev_allocate(hdmi_cable);
	if (IS_ERR(edev)) {
		pr_info(SYS "failed to allocate extcon hdcp\n");
		return;
	}

	edev->dev.parent = dev;
	edev->name = "hdmitx_extcon_hdcp";
	dev_set_name(&edev->dev, "hdcp");
	ret = extcon_dev_register(edev);
	if (ret < 0) {
		pr_info(SYS "failed to register extcon hdcp\n");
		return;
	}
	hdmitx_extcon_hdcp = edev;

}

static void hdmitx_init_parameters(struct hdmitx_info *info)
{
	memset(info, 0, sizeof(struct hdmitx_info));

	info->video_out_changing_flag = 1;

	info->audio_flag = 1;
	info->audio_info.type = CT_REFER_TO_STREAM;
	info->audio_info.format = AF_I2S;
	info->audio_info.fs = FS_44K1;
	info->audio_info.ss = SS_16BITS;
	info->audio_info.channels = CC_2CH;
	info->audio_out_changing_flag = 1;

	info->auto_hdcp_ri_flag = 1;
	info->hw_sha_calculator_flag = 1;
}

static int amhdmitx_device_init(struct hdmitx_dev *hdmi_dev)
{
	if (hdmi_dev == NULL)
		return 1;

	pr_info(SYS "Ver: %s\n", HDMITX_VER);

	hdmi_dev->hdtx_dev = NULL;

	hdmitx_device.physical_addr = 0xffff;
	/* init para for NULL protection */
	hdmitx_device.para = hdmi_get_fmt_name("invalid",
						hdmitx_device.fmt_attr);
	hdmitx_device.hdmi_last_hdr_mode = 0;
	hdmitx_device.hdmi_current_hdr_mode = 0;
	hdmitx_device.unplug_powerdown = 0;
	hdmitx_device.vic_count = 0;
	hdmitx_device.auth_process_timer = 0;
	hdmitx_device.force_audio_flag = 0;
	hdmitx_device.hdcp_mode = 0;
	hdmitx_device.ready = 0;
	hdmitx_device.rxsense_policy = 0; /* no RxSense by default */
	/* enable or disable HDMITX SSPLL, enable by default */
	hdmitx_device.sspll = 1;
	/*
	 * 0, do not unmux hpd when off or unplug ;
	 * 1, unmux hpd when unplug;
	 * 2, unmux hpd when unplug  or off;
	 */
	hdmitx_device.hpdmode = 1;

	hdmitx_device.flag_3dfp = 0;
	hdmitx_device.flag_3dss = 0;
	hdmitx_device.flag_3dtb = 0;

	if ((init_flag&INIT_FLAG_POWERDOWN) && (hdmitx_device.hpdmode == 2))
		hdmitx_device.mux_hpd_if_pin_high_flag = 0;
	else
		hdmitx_device.mux_hpd_if_pin_high_flag = 1;

	hdmitx_device.audio_param_update_flag = 0;
	/* 1: 2ch */
	hdmitx_device.hdmi_ch = 1;
	hdmitx_device.topo_info =
		kmalloc(sizeof(struct hdcprp_topo), GFP_KERNEL);
	if (!hdmitx_device.topo_info)
		pr_info("failed to alloc hdcp topo info\n");
	hdmitx_init_parameters(&hdmitx_device.hdmi_info);

	return 0;
}

static int amhdmitx_get_dt_info(struct platform_device *pdev)
{
	int ret = 0;

#ifdef CONFIG_OF
	int val;
	phandle phandle;
	struct device_node *init_data;
	struct device_node *drm_node;
	unsigned char *drm_status;
#endif

	/* HDMITX pinctrl config for hdp and ddc*/
	if (pdev->dev.pins) {
		hdmitx_device.pdev = &pdev->dev;

		hdmitx_device.pinctrl_default =
			pinctrl_lookup_state(pdev->dev.pins->p, "default");
		if (IS_ERR(hdmitx_device.pinctrl_default))
			pr_info(SYS "no default of pinctrl state\n");

		hdmitx_device.pinctrl_i2c =
			pinctrl_lookup_state(pdev->dev.pins->p, "hdmitx_i2c");
		if (IS_ERR(hdmitx_device.pinctrl_i2c))
			pr_info(SYS "no hdmitx_i2c of pinctrl state\n");

		pinctrl_select_state(pdev->dev.pins->p,
			hdmitx_device.pinctrl_default);
	}

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		int dongle_mode = 0;
		memset(&hdmitx_device.config_data, 0,
			sizeof(struct hdmi_config_platform_data));
		/* Get ic type information */
		ret = of_property_read_u32(pdev->dev.of_node, "ic_type",
			&(hdmitx_device.chip_type));
		if (ret)
			pr_info(SYS "not find ic_type\n");
		else
			pr_info(SYS "hdmitx_device.chip_type : %d\n",
				hdmitx_device.chip_type);

		/* Get dongle_mode information */
		ret = of_property_read_u32(pdev->dev.of_node, "dongle_mode",
			&dongle_mode);
		hdmitx_device.dongle_mode = !!dongle_mode;
		if (!ret)
			pr_info(SYS "hdmitx_device.dongle_mode: %d\n",
				hdmitx_device.dongle_mode);

		ret = of_property_read_u32(pdev->dev.of_node,
			"repeater_tx", &val);
		if (!ret)
			hdmitx_device.repeater_tx = val;
		if (hdmitx_device.repeater_tx == 1)
			hdmitx_device.topo_info = kzalloc(
				sizeof(*hdmitx_device.topo_info), GFP_KERNEL);

		/* Get vendor information */
		ret = of_property_read_u32(pdev->dev.of_node,
				"vend-data", &val);
		if (ret)
			pr_info(SYS "not find match init-data\n");
		if (ret == 0) {
			phandle = val;
			init_data = of_find_node_by_phandle(phandle);
			if (!init_data)
				pr_info(SYS "not find device node\n");
			hdmitx_device.config_data.vend_data = kzalloc(
				sizeof(struct vendor_info_data), GFP_KERNEL);
			if (!(hdmitx_device.config_data.vend_data))
				pr_info(SYS "not allocate memory\n");
			ret = get_dt_vend_init_data(init_data,
				hdmitx_device.config_data.vend_data);
			if (ret)
				pr_info(SYS "not find vend_init_data\n");
		}
		/* Get power control */
		ret = of_property_read_u32(pdev->dev.of_node,
				"pwr-ctrl", &val);
		if (ret)
			pr_info(SYS "not find match pwr-ctl\n");
		if (ret == 0) {
			phandle = val;
			init_data = of_find_node_by_phandle(phandle);
			if (!init_data)
				pr_info(SYS "not find device node\n");
			hdmitx_device.config_data.pwr_ctl = kzalloc((sizeof(
				struct hdmi_pwr_ctl)) * HDMI_TX_PWR_CTRL_NUM,
				GFP_KERNEL);
			if (!hdmitx_device.config_data.pwr_ctl)
				pr_info(SYS"can not get pwr_ctl mem\n");
			memset(hdmitx_device.config_data.pwr_ctl, 0,
				sizeof(struct hdmi_pwr_ctl));
			if (ret)
				pr_info(SYS "not find pwr_ctl\n");
		}
		/* Get drm feature information */
		drm_node = of_find_node_by_path("/drm-amhdmitx");
		if (drm_node) {
			ret = of_property_read_string(drm_node, "status",
				(const char **)&(drm_status));
			if (ret)
				pr_info(SYS "not find drm_feature\n");
			else {
				if (memcmp(drm_status, "okay", 4) == 0)
					hdmitx_device.drm_feature = 1;
				else
					hdmitx_device.drm_feature = 0;
				pr_info(SYS "hdmitx_device.drm_feature : %d\n",
					hdmitx_device.drm_feature);
			}
		} else {
			pr_info(SYS "not find drm_amhdmitx\n");
		}
	}

#else
		hdmi_pdata = pdev->dev.platform_data;
		if (!hdmi_pdata) {
			pr_info(SYS "not get platform data\n");
			r = -ENOENT;
		} else {
			pr_info(SYS "get hdmi platform data\n");
		}
#endif
	hdmitx_device.irq_hpd = platform_get_irq_byname(pdev, "hdmitx_hpd");
	if (hdmitx_device.irq_hpd == -ENXIO) {
		pr_err("%s: ERROR: hdmitx hpd irq No not found\n",
				__func__);
			return -ENXIO;
	}
	pr_info(SYS "hpd irq = %d\n", hdmitx_device.irq_hpd);

	return ret;
}

/*
 * amhdmitx_clktree_probe
 * get clktree info from dts
 */
static void amhdmitx_clktree_probe(struct device *hdmitx_dev)
{
	struct clk *hdmi_clk_vapb, *hdmi_clk_vpu;
	struct clk *hdcp22_tx_skp, *hdcp22_tx_esm;
	struct clk *venci_top_gate, *venci_0_gate, *venci_1_gate;

	hdmi_clk_vapb = devm_clk_get(hdmitx_dev, "hdmi_vapb_clk");
	if (IS_ERR(hdmi_clk_vapb))
		pr_warn(SYS "vapb_clk failed to probe\n");
	else {
		hdmitx_device.hdmitx_clk_tree.hdmi_clk_vapb = hdmi_clk_vapb;
		clk_prepare_enable(hdmitx_device.hdmitx_clk_tree.hdmi_clk_vapb);
	}

	hdmi_clk_vpu = devm_clk_get(hdmitx_dev, "hdmi_vpu_clk");
	if (IS_ERR(hdmi_clk_vpu))
		pr_warn(SYS "vpu_clk failed to probe\n");
	else {
		hdmitx_device.hdmitx_clk_tree.hdmi_clk_vpu = hdmi_clk_vpu;
		clk_prepare_enable(hdmitx_device.hdmitx_clk_tree.hdmi_clk_vpu);
	}

	hdcp22_tx_skp = devm_clk_get(hdmitx_dev, "hdcp22_tx_skp");
	if (IS_ERR(hdcp22_tx_skp))
		pr_warn(SYS "hdcp22_tx_skp failed to probe\n");
	else
		hdmitx_device.hdmitx_clk_tree.hdcp22_tx_skp = hdcp22_tx_skp;

	hdcp22_tx_esm = devm_clk_get(hdmitx_dev, "hdcp22_tx_esm");
	if (IS_ERR(hdcp22_tx_esm))
		pr_warn(SYS "hdcp22_tx_esm failed to probe\n");
	else
		hdmitx_device.hdmitx_clk_tree.hdcp22_tx_esm = hdcp22_tx_esm;

	venci_top_gate = devm_clk_get(hdmitx_dev, "venci_top_gate");
	if (IS_ERR(venci_top_gate))
		pr_warn(SYS "venci_top_gate failed to probe\n");
	else
		hdmitx_device.hdmitx_clk_tree.venci_top_gate = venci_top_gate;

	venci_0_gate = devm_clk_get(hdmitx_dev, "venci_0_gate");
	if (IS_ERR(venci_0_gate))
		pr_warn(SYS "venci_0_gate failed to probe\n");
	else
		hdmitx_device.hdmitx_clk_tree.venci_0_gate = venci_0_gate;

	venci_1_gate = devm_clk_get(hdmitx_dev, "venci_1_gate");
	if (IS_ERR(venci_1_gate))
		pr_warn(SYS "venci_0_gate failed to probe\n");
	else
		hdmitx_device.hdmitx_clk_tree.venci_1_gate = venci_1_gate;
}

static int amhdmitx_probe(struct platform_device *pdev)
{
	int r, ret = 0;
	struct device *dev;

	pr_info(SYS "amhdmitx_probe start\n");

	amhdmitx_device_init(&hdmitx_device);

	ret = amhdmitx_get_dt_info(pdev);

	amhdmitx_clktree_probe(&(pdev->dev));

	r = alloc_chrdev_region(&(hdmitx_device.hdmitx_id), 0, HDMI_TX_COUNT,
		DEVICE_NAME);
	cdev_init(&(hdmitx_device.cdev), &amhdmitx_fops);
	hdmitx_device.cdev.owner = THIS_MODULE;
	r = cdev_add(&(hdmitx_device.cdev), hdmitx_device.hdmitx_id,
		HDMI_TX_COUNT);

	hdmitx_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(hdmitx_class)) {
		unregister_chrdev_region(hdmitx_device.hdmitx_id,
			HDMI_TX_COUNT);
		return -1;
	}

	dev = device_create(hdmitx_class, NULL, hdmitx_device.hdmitx_id, NULL,
		"amhdmitx%d", 0); /* kernel>=2.6.27 */

	if (dev == NULL) {
		pr_info(SYS "device_create create error\n");
		class_destroy(hdmitx_class);
		r = -EEXIST;
		return r;
	}
	hdmitx_device.hdtx_dev = dev;
	ret = device_create_file(dev, &dev_attr_disp_mode);
	ret = device_create_file(dev, &dev_attr_attr);
	ret = device_create_file(dev, &dev_attr_aud_mode);
	ret = device_create_file(dev, &dev_attr_vid_mute);
	ret = device_create_file(dev, &dev_attr_edid);
	ret = device_create_file(dev, &dev_attr_rawedid);
	ret = device_create_file(dev, &dev_attr_sink_type);
	ret = device_create_file(dev, &dev_attr_edid_parsing);
	ret = device_create_file(dev, &dev_attr_config);
	ret = device_create_file(dev, &dev_attr_debug);
	ret = device_create_file(dev, &dev_attr_disp_cap);
	ret = device_create_file(dev, &dev_attr_preferred_mode);
	ret = device_create_file(dev, &dev_attr_disp_cap_3d);
	ret = device_create_file(dev, &dev_attr_aud_cap);
	ret = device_create_file(dev, &dev_attr_hdr_cap);
	ret = device_create_file(dev, &dev_attr_dv_cap);
	ret = device_create_file(dev, &dev_attr_aud_ch);
	ret = device_create_file(dev, &dev_attr_avmute);
	ret = device_create_file(dev, &dev_attr_swap);
	ret = device_create_file(dev, &dev_attr_vic);
	ret = device_create_file(dev, &dev_attr_phy);
	ret = device_create_file(dev, &dev_attr_frac_rate_policy);
	ret = device_create_file(dev, &dev_attr_sspll);
	ret = device_create_file(dev, &dev_attr_rxsense_policy);
	ret = device_create_file(dev, &dev_attr_hdcp_clkdis);
	ret = device_create_file(dev, &dev_attr_hdcp_pwr);
	ret = device_create_file(dev, &dev_attr_hdcp_ksv_info);
	ret = device_create_file(dev, &dev_attr_hdcp_ver);
	ret = device_create_file(dev, &dev_attr_hdcp_byp);
	ret = device_create_file(dev, &dev_attr_hdcp_mode);
	ret = device_create_file(dev, &dev_attr_hdcp_repeater);
	ret = device_create_file(dev, &dev_attr_hdcp_topo_info);
	ret = device_create_file(dev, &dev_attr_hdcp22_type);
	ret = device_create_file(dev, &dev_attr_hdcp_stickmode);
	ret = device_create_file(dev, &dev_attr_hdcp_stickstep);
	ret = device_create_file(dev, &dev_attr_hdmi_repeater_tx);
	ret = device_create_file(dev, &dev_attr_hdcp22_base);
	ret = device_create_file(dev, &dev_attr_hdcp_lstore);
	ret = device_create_file(dev, &dev_attr_hdcp_rptxlstore);
	ret = device_create_file(dev, &dev_attr_div40);
	ret = device_create_file(dev, &dev_attr_hdcp_ctrl);
	ret = device_create_file(dev, &dev_attr_hpd_state);
	ret = device_create_file(dev, &dev_attr_rhpd_state);
	ret = device_create_file(dev, &dev_attr_max_exceed);
	ret = device_create_file(dev, &dev_attr_hdmi_init);
	ret = device_create_file(dev, &dev_attr_ready);
	ret = device_create_file(dev, &dev_attr_support_3d);
	ret = device_create_file(dev, &dev_attr_dc_cap);
	ret = device_create_file(dev, &dev_attr_valid_mode);
	ret = device_create_file(dev, &dev_attr_allm_mode);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	register_early_suspend(&hdmitx_early_suspend_handler);
#endif
	hdmitx_device.nb.notifier_call = hdmitx_reboot_notifier;
	register_reboot_notifier(&hdmitx_device.nb);

	vout_register_server(&hdmitx_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_register_server(&hdmitx_vout2_server);
#endif
#ifdef CONFIG_AMLOGIC_SND_SOC
	aout_register_client(&hdmitx_notifier_nb_a);
#else
	r = r ? (long int)&hdmitx_notifier_nb_a :
		(long int)&hdmitx_notifier_nb_a;
#endif

	hdmitx_extcon_register(pdev, dev);

	HDMITX_Meson_Init(&hdmitx_device);

	/* update fmt_attr */
	hdmitx_init_fmt_attr(&hdmitx_device);

	hdmitx_device.task = kthread_run(hdmi_task_handle,
		&hdmitx_device, "kthread_hdmi");

	pr_info(SYS "amhdmitx_probe end\n");

	return r;
}

static int amhdmitx_remove(struct platform_device *pdev)
{
	struct device *dev = hdmitx_device.hdtx_dev;

	cancel_work_sync(&hdmitx_device.work_hdr);

	if (hdmitx_device.HWOp.UnInit)
		hdmitx_device.HWOp.UnInit(&hdmitx_device);
	hdmitx_device.hpd_event = 0xff;
	kthread_stop(hdmitx_device.task);
	vout_unregister_server(&hdmitx_vout_server);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_unregister_server(&hdmitx_vout2_server);
#endif
#ifdef CONFIG_AMLOGIC_SND_SOC
	aout_unregister_client(&hdmitx_notifier_nb_a);
#endif

	/* Remove the cdev */
	device_remove_file(dev, &dev_attr_disp_mode);
	device_remove_file(dev, &dev_attr_attr);
	device_remove_file(dev, &dev_attr_aud_mode);
	device_remove_file(dev, &dev_attr_vid_mute);
	device_remove_file(dev, &dev_attr_edid);
	device_remove_file(dev, &dev_attr_rawedid);
	device_remove_file(dev, &dev_attr_sink_type);
	device_remove_file(dev, &dev_attr_edid_parsing);
	device_remove_file(dev, &dev_attr_config);
	device_remove_file(dev, &dev_attr_debug);
	device_remove_file(dev, &dev_attr_disp_cap);
	device_remove_file(dev, &dev_attr_preferred_mode);
	device_remove_file(dev, &dev_attr_disp_cap_3d);
	device_remove_file(dev, &dev_attr_hdr_cap);
	device_remove_file(dev, &dev_attr_dv_cap);
	device_remove_file(dev, &dev_attr_dc_cap);
	device_remove_file(dev, &dev_attr_valid_mode);
	device_remove_file(dev, &dev_attr_allm_mode);
	device_remove_file(dev, &dev_attr_hpd_state);
	device_remove_file(dev, &dev_attr_rhpd_state);
	device_remove_file(dev, &dev_attr_max_exceed);
	device_remove_file(dev, &dev_attr_hdmi_init);
	device_remove_file(dev, &dev_attr_ready);
	device_remove_file(dev, &dev_attr_support_3d);
	device_remove_file(dev, &dev_attr_avmute);
	device_remove_file(dev, &dev_attr_vic);
	device_remove_file(dev, &dev_attr_frac_rate_policy);
	device_remove_file(dev, &dev_attr_sspll);
	device_remove_file(dev, &dev_attr_rxsense_policy);
	device_remove_file(dev, &dev_attr_hdcp_pwr);
	device_remove_file(dev, &dev_attr_div40);
	device_remove_file(dev, &dev_attr_hdcp_repeater);
	device_remove_file(dev, &dev_attr_hdcp_topo_info);
	device_remove_file(dev, &dev_attr_hdcp22_type);
	device_remove_file(dev, &dev_attr_hdcp_stickmode);
	device_remove_file(dev, &dev_attr_hdcp_stickstep);
	device_remove_file(dev, &dev_attr_hdmi_repeater_tx);
	device_remove_file(dev, &dev_attr_hdcp22_base);
	device_remove_file(dev, &dev_attr_swap);

	cdev_del(&hdmitx_device.cdev);

	device_destroy(hdmitx_class, hdmitx_device.hdmitx_id);

	class_destroy(hdmitx_class);

	unregister_chrdev_region(hdmitx_device.hdmitx_id, HDMI_TX_COUNT);
	return 0;
}

#ifdef CONFIG_PM
static int amhdmitx_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	hdmitx_device.HWOp.CntlDDC(&hdmitx_device,
		DDC_RESET_HDCP, 0);
	pr_info("amhdmitx: suspend and reset hdcp\n");
	return 0;
}

static int amhdmitx_resume(struct platform_device *pdev)
{
	struct hdmitx_dev *hdev = &hdmitx_device;

	pr_debug("amhdmitx: I2C_REACTIVE\n");
	hdev->HWOp.CntlMisc(hdev, MISC_I2C_REACTIVE, 0);

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
#ifdef CONFIG_HIBERNATION
		.pm	= &amhdmitx_pm,
#endif
	}
};

static int  __init amhdmitx_init(void)
{
	if (init_flag&INIT_FLAG_NOT_LOAD)
		return 0;

	if (platform_driver_register(&amhdmitx_driver)) {
		pr_info(SYS
			"failed to register amhdmitx module\n");

		return -ENODEV;
	}

	return 0;
}

static void __exit amhdmitx_exit(void)
{
	pr_info(SYS "amhdmitx_exit\n");
	platform_driver_unregister(&amhdmitx_driver);
}

subsys_initcall(amhdmitx_init);
module_exit(amhdmitx_exit);

MODULE_DESCRIPTION("AMLOGIC HDMI TX driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

/* besides characters defined in separator, '\"' are used as separator;
 * and any characters in '\"' will not act as separator
 */
static char *next_token_ex(char *separator, char *buf, unsigned int size,
	unsigned int offset, unsigned int *token_len,
	unsigned int *token_offset)
{
	char *pToken = NULL;
	char last_separator = 0;
	char trans_char_flag = 0;

	if (buf) {
		for (; offset < size; offset++) {
			int ii = 0;
		char ch;

		if (buf[offset] == '\\') {
			trans_char_flag = 1;
			continue;
		}
		while (((ch = separator[ii++]) != buf[offset]) && (ch))
			;
		if (ch) {
			if (!pToken) {
				continue;
		} else {
			if (last_separator != '"') {
				*token_len = (unsigned int)
					(buf + offset - pToken);
				*token_offset = offset;
				return pToken;
			}
		}
		} else if (!pToken) {
			if (trans_char_flag && (buf[offset] == '"'))
				last_separator = buf[offset];
			pToken = &buf[offset];
		} else if ((trans_char_flag && (buf[offset] == '"'))
				&& (last_separator == '"')) {
			*token_len = (unsigned int)(buf + offset - pToken - 2);
			*token_offset = offset + 1;
			return pToken + 1;
		}
		trans_char_flag = 0;
	}
	if (pToken) {
		*token_len = (unsigned int)(buf + offset - pToken);
		*token_offset = offset;
	}
	}
	return pToken;
}

/* check the colorattribute from uboot */
static void check_hdmiuboot_attr(char *token)
{
	char attr[16] = {0};
	const char * const cs[] = {
		"444", "422", "rgb", "420", NULL};
	const char * const cd[] = {
		"8bit", "10bit", "12bit", "16bit", NULL};
	int i;

	if (hdmitx_device.fmt_attr[0] != 0)
		return;

	if (!token)
		return;

	for (i = 0; cs[i] != NULL; i++) {
		if (strstr(token, cs[i])) {
			if (strlen(cs[i]) < sizeof(attr))
				strncpy(attr, cs[i], strlen(cs[i]));
			strcat(attr, ",");
			break;
		}
	}
	for (i = 0; cd[i] != NULL; i++) {
		if (strstr(token, cd[i])) {
			if (strlen(cd[i]) < (sizeof(attr) - strlen(attr)))
				strncat(attr, cd[i], strlen(cd[i]));
			strncpy(hdmitx_device.fmt_attr, attr,
				sizeof(hdmitx_device.fmt_attr));
			hdmitx_device.fmt_attr[15] = '\0';
			break;
		}
	}
}

static  int __init hdmitx_boot_para_setup(char *s)
{
	char separator[] = {' ', ',', ';', 0x0};
	char *token;
	unsigned int token_len = 0;
	unsigned int token_offset = 0;
	unsigned int offset = 0;
	int size = strlen(s);

	memset(hdmitx_device.fmt_attr, 0, sizeof(hdmitx_device.fmt_attr));

	do {
		token = next_token_ex(separator, s, size, offset,
				&token_len, &token_offset);
		if (token) {
			if ((token_len == 3)
				&& (strncmp(token, "off", token_len) == 0)) {
				init_flag |= INIT_FLAG_NOT_LOAD;
			}
			check_hdmiuboot_attr(token);
		}
		offset = token_offset;
	} while (token);
	return 0;
}

__setup("hdmitx=", hdmitx_boot_para_setup);

static int __init hdmitx_boot_frac_rate(char *str)
{
	if (strncmp("0", str, 1) == 0)
		hdmitx_device.frac_rate_policy = 0;
	else
		hdmitx_device.frac_rate_policy = 1;

	pr_info("hdmitx boot frac_rate_policy: %d",
		hdmitx_device.frac_rate_policy);
	return 0;
}

__setup("frac_rate_policy=", hdmitx_boot_frac_rate);
