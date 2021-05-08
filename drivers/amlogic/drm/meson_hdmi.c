/*
 * drivers/amlogic/drm/meson_hdmi.c
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

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>

#include <linux/component.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/device.h>

#include <linux/workqueue.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "meson_drv.h"
#include "meson_hdmi.h"
#include "meson_hdcp.h"
#include "meson_vpu.h"

#define DEVICE_NAME "amhdmitx"
struct am_hdmi_tx am_hdmi_info;

static struct am_vout_mode am_vout_modes[] = {
	{ "1080p60hz", VMODE_HDMI, 1920, 1080, 60, 0},
	{ "1080p30hz", VMODE_HDMI, 1920, 1080, 30, 0},
	{ "1080p50hz", VMODE_HDMI, 1920, 1080, 50, 0},
	{ "1080p25hz", VMODE_HDMI, 1920, 1080, 25, 0},
	{ "1080p24hz", VMODE_HDMI, 1920, 1080, 24, 0},
	{ "2160p30hz", VMODE_HDMI, 3840, 2160, 30, 0},
	{ "2160p25hz", VMODE_HDMI, 3840, 2160, 25, 0},
	{ "2160p24hz", VMODE_HDMI, 3840, 2160, 24, 0},
	{ "smpte30hz", VMODE_HDMI, 4096, 2160, 30, 0},
	{ "smpte25hz", VMODE_HDMI, 4096, 2160, 25, 0},
	{ "smpte24hz", VMODE_HDMI, 4096, 2160, 24, 0},
	{ "1080i60hz", VMODE_HDMI, 1920, 1080, 60, DRM_MODE_FLAG_INTERLACE},
	{ "1080i50hz", VMODE_HDMI, 1920, 1080, 50, DRM_MODE_FLAG_INTERLACE},
	{ "720p60hz", VMODE_HDMI, 1280, 720, 60, 0},
	{ "720p50hz", VMODE_HDMI, 1280, 720, 50, 0},
	{ "480p60hz", VMODE_HDMI, 720, 480, 60, 0},
	{ "480i60hz", VMODE_HDMI, 720, 480, 60, DRM_MODE_FLAG_INTERLACE},
	{ "576p50hz", VMODE_HDMI, 720, 576, 50, 0},
	{ "576i50hz", VMODE_HDMI, 720, 576, 50, DRM_MODE_FLAG_INTERLACE},
	{ "480p60hz", VMODE_HDMI, 720, 480, 60, 0},
	{ "1280x1024p60hz", VMODE_HDMI, 1280, 1024, 60, 0},
	{ "1680x1050p60hz", VMODE_HDMI, 1680, 1050, 60, 0},
};

static int am_hdmi_create_mode_default_attr(struct drm_display_mode *mode)
{
	u8 tmp;

	memset(mode->default_attr, 0, 16);
	if (mode->mode_color_420)
		strcpy(mode->default_attr, "420");
	else if (mode->mode_color_444)
		strcpy(mode->default_attr, "444");
	else if (mode->mode_color_422)
		strcpy(mode->default_attr, "422");
	else
		strcpy(mode->default_attr, "rgb");
	tmp = strlen(mode->default_attr);
	mode->default_attr[tmp] = ',';
	strcat(mode->default_attr, "8bit");

	return 0;
}

static struct am_vout_mode am_vout_modes_hdmi20[] = {
	{ "2160p60hz", VMODE_HDMI, 3840, 2160, 60, 0},
	{ "2160p50hz", VMODE_HDMI, 3840, 2160, 50, 0},
	{ "smpte60hz", VMODE_HDMI, 4096, 2160, 60, 0},
	{ "smpte50hz", VMODE_HDMI, 4096, 2160, 50, 0}
};

char *am_meson_hdmitx_get_mode(struct drm_display_mode *mode)
{
	int i;
	char *pname;
	struct hdmi20_vendor_para *phdmi20_para;
	struct am_hdmi_tx *am_hdmi = &am_hdmi_info;

	pname = NULL;
	phdmi20_para = &am_hdmi->connector.hdmi20_para;
	for (i = 0; i < ARRAY_SIZE(am_vout_modes); i++) {
		if (am_vout_modes[i].width == mode->hdisplay &&
		    am_vout_modes[i].height == mode->vdisplay &&
		    am_vout_modes[i].vrefresh == mode->vrefresh &&
		    am_vout_modes[i].flags ==
		    (mode->flags & DRM_MODE_FLAG_INTERLACE))
			pname = am_vout_modes[i].name;
	}
	if (!pname) {
		for (i = 0; i < ARRAY_SIZE(am_vout_modes_hdmi20); i++) {
			if (am_vout_modes_hdmi20[i].width == mode->hdisplay &&
			    am_vout_modes_hdmi20[i].height == mode->vdisplay &&
			    am_vout_modes_hdmi20[i].vrefresh ==
				mode->vrefresh &&
			    am_vout_modes_hdmi20[i].flags ==
				(mode->flags & DRM_MODE_FLAG_INTERLACE))
				pname = am_vout_modes_hdmi20[i].name;
		}
	}
	return pname;
}

char *am_meson_hdmi_get_voutmode(struct drm_display_mode *mode)
{
	int i;
	char *pname;
	struct hdmi20_vendor_para *phdmi20_para;
	struct am_hdmi_tx *am_hdmi = &am_hdmi_info;

	pname = NULL;
	phdmi20_para = &am_hdmi->connector.hdmi20_para;
	for (i = 0; i < ARRAY_SIZE(am_vout_modes); i++) {
		if (am_vout_modes[i].width == mode->hdisplay &&
		    am_vout_modes[i].height == mode->vdisplay &&
		    am_vout_modes[i].vrefresh == mode->vrefresh &&
		    am_vout_modes[i].flags ==
		    (mode->flags & DRM_MODE_FLAG_INTERLACE))
			pname = am_vout_modes[i].name;
	}
	if (!pname) {
		for (i = 0; i < ARRAY_SIZE(am_vout_modes_hdmi20); i++) {
			if (am_vout_modes_hdmi20[i].width == mode->hdisplay &&
			    am_vout_modes_hdmi20[i].height ==
			    mode->vdisplay &&
			    am_vout_modes_hdmi20[i].vrefresh ==
			    mode->vrefresh &&
			    am_vout_modes_hdmi20[i].flags ==
			    (mode->flags & DRM_MODE_FLAG_INTERLACE))
				pname = am_vout_modes_hdmi20[i].name;
		}
		if (pname &&
		    am_hdmi->color_space == COLORSPACE_YUV420) {
		switch (am_hdmi->color_depth) {
		case COLORDEPTH_30B:
			if (!phdmi20_para->dc_30bit_420)
				pname = NULL;
			break;
		case COLORDEPTH_36B:
			if (!phdmi20_para->dc_36bit_420)
				pname = NULL;
			break;
		case COLORDEPTH_48B:
			if (!phdmi20_para->dc_48bit_420)
				pname = NULL;
			break;
		case COLORDEPTH_24B:
		case COLORDEPTH_RESERVED:
			break;
			}
		}
	}
	return pname;
}

static const struct drm_prop_enum_list am_color_space_enum_names[] = {
	{ COLORSPACE_RGB444, "rgb" },
	{ COLORSPACE_YUV422, "422" },
	{ COLORSPACE_YUV444, "444" },
	{ COLORSPACE_YUV420, "420" },
	{ COLORSPACE_RESERVED, "reserved" },
};

static const struct drm_prop_enum_list am_color_depth_enum_names[] = {
	{ COLORDEPTH_24B, "8bit" },
	{ COLORDEPTH_30B, "10bit" },
	{ COLORDEPTH_36B, "12bit" },
	{ COLORDEPTH_48B, "16bit" },
	{ COLORDEPTH_RESERVED, "reserved" },
};

static bool check_edid_all_zeros(unsigned char *buf)
{
	unsigned int i = 0, j = 0;
	unsigned int chksum = 0;

	for (j = 0; j < EDID_MAX_BLOCK; j++) {
		chksum = 0;
		for (i = 0; i < 128; i++)
			chksum += buf[i + j * 128];
		if (chksum != 0)
			return false;
	}
	return true;
}

static void ddc_glitch_filter_reset(void)
{
	hdmitx_set_reg_bits(HDMITX_TOP_SW_RESET, 1, 6, 1);
	/*keep resetting DDC for some time*/
	usleep_range(1000, 2000);
	hdmitx_set_reg_bits(HDMITX_TOP_SW_RESET, 0, 6, 1);
	/*wait recover for resetting DDC*/
	usleep_range(1000, 2000);
}

static int parsing_drm_static(struct am_hdmi_ceadrm_static_cap *ceadrm_scap,
			      unsigned char *ceadrm_loc,
			      unsigned char ceadrm_len)
{
	unsigned int pos = 0;
	int ret;

	ceadrm_scap->hdr_sup_eotf_sdr =
		!!(ceadrm_loc[pos] & (0x1 << 0));
	ceadrm_scap->hdr_sup_eotf_hdr =
		!!(ceadrm_loc[pos] & (0x1 << 1));
	ceadrm_scap->hdr_sup_eotf_smpte_st_2084 =
		!!(ceadrm_loc[pos] & (0x1 << 2));
	ceadrm_scap->hdr_sup_eotf_hlg =
		!!(ceadrm_loc[pos] & (0x1 << 3));
	pos++;
	ceadrm_scap->hdr_sup_smd_type1 =
		!!(ceadrm_loc[pos] & (0x1 << 0));
	pos++;
	switch (ceadrm_len) {
	case 2:
		ret = 0;
		break;
	case 3:
		ceadrm_scap->hdr_lum_max = ceadrm_loc[pos];
		ret = 0;
		break;
	case 4:
		ceadrm_scap->hdr_lum_max = ceadrm_loc[pos];
		ceadrm_scap->hdr_lum_avg = ceadrm_loc[pos + 1];
		ret = 0;
		break;
	case 5:
		ceadrm_scap->hdr_lum_max = ceadrm_loc[pos];
		ceadrm_scap->hdr_lum_avg = ceadrm_loc[pos + 1];
		ceadrm_scap->hdr_lum_min = ceadrm_loc[pos + 2];
		ret = 0;
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

static int parsing_drm_dynamic(struct am_hdmi_ceadrm_dynamic_cap *ceadrm_dcap,
			       unsigned char *ceadrm_loc,
			       unsigned char ceadrm_len)
{
	unsigned int pos = 0;
	unsigned int type;
	unsigned int type_length;
	unsigned int i;
	unsigned int num;

	while (ceadrm_len) {
		type_length = ceadrm_loc[pos];
		pos++;
		type = (ceadrm_loc[pos + 1] << 8) | ceadrm_loc[pos];
		pos += 2;
		switch (type) {
		case TS_103_433_SPEC_TYPE:
			num = 1;
			break;
		case ITU_T_H265_SPEC_TYPE:
			num = 2;
			break;
		case TYPE_4_HDR_METADATA_TYPE:
			num = 3;
			break;
		case TYPE_1_HDR_METADATA_TYPE:
		default:
			num = 0;
			break;
		}
		ceadrm_dcap[num].hd_len = type_length;
		ceadrm_dcap[num].type = type;
		ceadrm_dcap[num].support_flags = ceadrm_loc[pos];
		pos++;
		for (i = 0; i < type_length - 3; i++) {
			ceadrm_dcap[num].optional_fields[i]
				= ceadrm_loc[pos];
			pos++;
		}
		ceadrm_len = ceadrm_len - (type_length + 1);
	}

	return 0;
}

static int parsing_vend_spec_hdr10p(struct am_hdmi_hdr10p_info *hdr10plus_cap,
				    unsigned char *hdr10p_loc,
				    unsigned char hdr10p_len)
{
	memset(hdr10plus_cap, 0, sizeof(struct am_hdmi_hdr10p_info));
	hdr10plus_cap->length = hdr10p_len + 4;
	hdr10plus_cap->ieeeoui = 0x90848B;
	hdr10plus_cap->application_version = hdr10p_loc[0] & 0x3;
	return 0;
}

static int parsing_vend_spec_dv(struct am_hdmi_dv_info *dolby_vision_cap,
				unsigned char *dv_info_loc,
				unsigned char dv_info_len)
{
	unsigned char *dv_start;
	unsigned char pos = 0;
	unsigned int ieeeoui = DV_IEEE_OUI;

	/* it is a Dovi block*/
	memset(dolby_vision_cap, 0, sizeof(struct am_hdmi_dv_info));
	dolby_vision_cap->block_flag = AM_CORRECT;
	dolby_vision_cap->length = dv_info_len + 4;
	dv_start = dv_info_loc - 5; /* 5 = 3(oui) + 2*/
	memcpy(dolby_vision_cap->rawdata, dv_start, dv_info_len + 5);
	dolby_vision_cap->ieeeoui = ieeeoui;
	dolby_vision_cap->ver = (dv_info_loc[pos] >> 5) & 0x7;
	if ((dolby_vision_cap->ver) > 2) {
		dolby_vision_cap->block_flag = AM_ERROR_VER;
		return -1;
	}
	/* Refer to DV 2.9 Page 27 */
	if (dolby_vision_cap->ver == 0) {
		if (dolby_vision_cap->length == 0x19) {
			dolby_vision_cap->sup_yuv422_12bit =
				dv_info_loc[pos] & 0x1;
			dolby_vision_cap->sup_2160p60hz =
				(dv_info_loc[pos] >> 1) & 0x1;
			dolby_vision_cap->sup_global_dimming =
				(dv_info_loc[pos] >> 2) & 0x1;
			pos++;
			dolby_vision_cap->redx =
				(dv_info_loc[pos + 1] << 4) |
				(dv_info_loc[pos] >> 4);
			dolby_vision_cap->redy =
				(dv_info_loc[pos + 2] << 4) |
				(dv_info_loc[pos] & 0xf);
			pos += 3;
			dolby_vision_cap->greenx =
				(dv_info_loc[pos + 1] << 4) |
				(dv_info_loc[pos] >> 4);
			dolby_vision_cap->greeny =
				(dv_info_loc[pos + 2] << 4) |
				(dv_info_loc[pos] & 0xf);
			pos += 3;
			dolby_vision_cap->bluex =
				(dv_info_loc[pos + 1] << 4) |
				(dv_info_loc[pos] >> 4);
			dolby_vision_cap->bluey =
				(dv_info_loc[pos + 2] << 4) |
				(dv_info_loc[pos] & 0xf);
			pos += 3;
			dolby_vision_cap->whitex =
				(dv_info_loc[pos + 1] << 4) |
				(dv_info_loc[pos] >> 4);
			dolby_vision_cap->whitey =
				(dv_info_loc[pos + 2] << 4) |
				(dv_info_loc[pos] & 0xf);
			pos += 3;
			dolby_vision_cap->tmin_pq =
				(dv_info_loc[pos + 1] << 4) |
				(dv_info_loc[pos] >> 4);
			dolby_vision_cap->tmax_pq =
				(dv_info_loc[pos + 2] << 4) |
				(dv_info_loc[pos] & 0xf);
			pos += 3;
			dolby_vision_cap->dm_major_ver =
				dv_info_loc[pos] >> 4;
			dolby_vision_cap->dm_minor_ver =
				dv_info_loc[pos] & 0xf;
			pos++;
			DRM_INFO("v0 VSVDB: len=%d, sup_2160p60hz=%d\n",
				 dolby_vision_cap->length,
				 dolby_vision_cap->sup_2160p60hz);
		} else {
			dolby_vision_cap->block_flag = AM_ERROR_LENGTH;
		}
	}

	if (dolby_vision_cap->ver == 1) {
		if (dolby_vision_cap->length == 0x0B) {
			/* Refer to DV 2.9 Page 33 */
			dolby_vision_cap->dm_version =
				(dv_info_loc[pos] >> 2) & 0x7;
			dolby_vision_cap->sup_yuv422_12bit =
				dv_info_loc[pos] & 0x1;
			dolby_vision_cap->sup_2160p60hz =
				(dv_info_loc[pos] >> 1) & 0x1;
			pos++;
			dolby_vision_cap->sup_global_dimming =
				dv_info_loc[pos] & 0x1;
			dolby_vision_cap->tmax_lum =
				dv_info_loc[pos] >> 1;
			pos++;
			dolby_vision_cap->colorimetry =
				dv_info_loc[pos] & 0x1;
			dolby_vision_cap->tmin_lum =
				dv_info_loc[pos] >> 1;
			pos++;
			dolby_vision_cap->low_latency =
				dv_info_loc[pos] & 0x3;
			dolby_vision_cap->bluex =
				0x20 | ((dv_info_loc[pos] >> 5) & 0x7);
			dolby_vision_cap->bluey =
				0x08 | ((dv_info_loc[pos] >> 2) & 0x7);
			pos++;
			dolby_vision_cap->greenx =
				0x00 | (dv_info_loc[pos] >> 1);
			dolby_vision_cap->redy =
				0x40 | ((dv_info_loc[pos] & 0x1) |
				((dv_info_loc[pos + 1] & 0x1) << 1) |
				((dv_info_loc[pos + 2] & 0x3) << 2));
			pos++;
			dolby_vision_cap->greeny = 0x80 |
				(dv_info_loc[pos] >> 1);
			pos++;
			dolby_vision_cap->redx =
				0xA0 | (dv_info_loc[pos] >> 3);
			pos++;
			DRM_INFO("v1 VSVDB: len=%d, sup_2160p60hz=%d,ll=%d\n",
				 dolby_vision_cap->length,
				 dolby_vision_cap->sup_2160p60hz,
				 dolby_vision_cap->low_latency);
		} else if (dolby_vision_cap->length == 0x0E) {
			dolby_vision_cap->dm_version =
				(dv_info_loc[pos] >> 2) & 0x7;
			dolby_vision_cap->sup_yuv422_12bit =
				dv_info_loc[pos] & 0x1;
			dolby_vision_cap->sup_2160p60hz =
				(dv_info_loc[pos] >> 1) & 0x1;
			pos++;
			dolby_vision_cap->sup_global_dimming =
				dv_info_loc[pos] & 0x1;
			dolby_vision_cap->tmax_lum =
				dv_info_loc[pos] >> 1;
			pos++;
			dolby_vision_cap->colorimetry =
				dv_info_loc[pos] & 0x1;
			dolby_vision_cap->tmin_lum =
				dv_info_loc[pos] >> 1;
			pos += 2; /* byte8 is reserved as 0 */
			dolby_vision_cap->redx = dv_info_loc[pos++];
			dolby_vision_cap->redy = dv_info_loc[pos++];
			dolby_vision_cap->greenx = dv_info_loc[pos++];
			dolby_vision_cap->greeny = dv_info_loc[pos++];
			dolby_vision_cap->bluex = dv_info_loc[pos++];
			dolby_vision_cap->bluey = dv_info_loc[pos++];
			DRM_INFO("v1 VSVDB: len=%d, sup_2160p60hz=%d\n",
				 dolby_vision_cap->length,
				 dolby_vision_cap->sup_2160p60hz);
		} else {
			dolby_vision_cap->block_flag = AM_ERROR_LENGTH;
		}
	}
	if (dolby_vision_cap->ver == 2) {
		/* v2 VSVDB length could be greater than 0xB
		 * and should not be treated as unrecognized
		 * block. Instead, we should parse it as a regular
		 * v2 VSVDB using just the remaining 11 bytes here
		 */
		if (dolby_vision_cap->length >= 0x0B) {
			dolby_vision_cap->sup_2160p60hz = 0x1;/*default*/
			dolby_vision_cap->dm_version =
				(dv_info_loc[pos] >> 2) & 0x7;
			dolby_vision_cap->sup_yuv422_12bit =
				dv_info_loc[pos] & 0x1;
			dolby_vision_cap->sup_backlight_control =
				(dv_info_loc[pos] >> 1) & 0x1;
			pos++;
			dolby_vision_cap->sup_global_dimming =
				(dv_info_loc[pos] >> 2) & 0x1;
			dolby_vision_cap->backlt_min_luma =
				dv_info_loc[pos] & 0x3;
			dolby_vision_cap->tmin_pq =
				dv_info_loc[pos] >> 3;
			pos++;
			dolby_vision_cap->inter_face =
				dv_info_loc[pos] & 0x3;
			dolby_vision_cap->tmax_pq =
				dv_info_loc[pos] >> 3;
			pos++;
			dolby_vision_cap->sup_10b_12b_444 =
				((dv_info_loc[pos] & 0x1) << 1) |
				(dv_info_loc[pos + 1] & 0x1);
			dolby_vision_cap->greenx = 0x00 |
				(dv_info_loc[pos] >> 1);
			pos++;
			dolby_vision_cap->greeny =
				0x80 | (dv_info_loc[pos] >> 1);
			pos++;
			dolby_vision_cap->redx =
				0xA0 | (dv_info_loc[pos] >> 3);
			dolby_vision_cap->bluex =
				0x20 | (dv_info_loc[pos] & 0x7);
			pos++;
			dolby_vision_cap->redy =
				0x40  | (dv_info_loc[pos] >> 3);
			dolby_vision_cap->bluey =
				0x08  | (dv_info_loc[pos] & 0x7);
			pos++;
			DRM_INFO("v2VSVDB: len=%d, sup_2160p60hz=%d, int=%d\n",
				 dolby_vision_cap->length,
				 dolby_vision_cap->sup_2160p60hz,
				 dolby_vision_cap->inter_face);
		} else {
			dolby_vision_cap->block_flag = AM_ERROR_LENGTH;
		}
	}

	if (pos > (dolby_vision_cap->length + 1))
		DRM_INFO("hdmitx: edid: maybe invalid dv%d data\n",
			 dolby_vision_cap->ver);

	return 0;
}

static bool drm_detect_cea_hdr_dv(struct drm_connector *connector,
				  struct edid *edid,
		struct am_hdmi_downstream_hdr_dv_cap *downstream_hdr_dv_cap)
{
	unsigned char *ceadrm_loc, *vend_spec_loc;
	unsigned char ceadrm_len, vend_spec_len;
	int ret;

	/*parse DRM Static Block*/
	ret = drm_parse_ceadrm_static(connector, edid,
				      &ceadrm_loc, &ceadrm_len);
	if (ret == 0)
		parsing_drm_static(&downstream_hdr_dv_cap->ceadrm_scap,
				   ceadrm_loc, ceadrm_len);
	/*parse DRM Dynamic Block*/
	ret = drm_parse_ceadrm_dynamic(connector, edid,
				       &ceadrm_loc, &ceadrm_len);
	if (ret == 0)
		parsing_drm_dynamic(downstream_hdr_dv_cap->ceadrm_dcap,
				    ceadrm_loc, ceadrm_len);
	/*parse vendor spec dolby vision*/
	ret = drm_parse_vend_spec_dv(connector, edid,
				     &vend_spec_loc, &vend_spec_len);
	if (ret == 0)
		parsing_vend_spec_dv(&downstream_hdr_dv_cap->dolby_vision_cap,
				     vend_spec_loc, vend_spec_len);
	else
		downstream_hdr_dv_cap->dolby_vision_cap.block_flag =
			AM_ERROR_OUI;
	/*parse vendor spec hdr10plus*/
	ret = drm_parse_vend_spec_hdr10p(connector, edid,
					 &vend_spec_loc, &vend_spec_len);
	if (ret == 0)
		parsing_vend_spec_hdr10p(&downstream_hdr_dv_cap->hdr10plus_cap,
					 vend_spec_loc, vend_spec_len);

	return true;
}

void drm_print_cea_hdr_dv(struct am_hdmi_downstream_hdr_dv_cap
			  *downstream_hdr_dv_cap)
{
	struct am_hdmi_ceadrm_static_cap *pceadrm_scap;
	struct am_hdmi_ceadrm_dynamic_cap *pceadrm_dcap;
	struct am_hdmi_dv_info *pdolby_vision_cap;
	struct am_hdmi_hdr10p_info *phdr10plus_cap;
	int hdr10plugsupported = 0;
	unsigned int count;

	pceadrm_scap = &downstream_hdr_dv_cap->ceadrm_scap;
	pceadrm_dcap = &downstream_hdr_dv_cap->ceadrm_dcap[0];
	pdolby_vision_cap = &downstream_hdr_dv_cap->dolby_vision_cap;
	phdr10plus_cap = &downstream_hdr_dv_cap->hdr10plus_cap;

	if ((phdr10plus_cap->ieeeoui == HDR10_PLUS_IEEE_OUI) &&
	    (phdr10plus_cap->application_version != 0xFF))
		hdr10plugsupported = 1;

	for (count = 0; count < 4; count++) {
		pceadrm_dcap =
			&downstream_hdr_dv_cap->ceadrm_dcap[count];
		if (pceadrm_dcap->type == 0)
			continue;
		DRM_INFO("\nmetadata_version: %x\n",
			 pceadrm_dcap->type);
	}

	if (pdolby_vision_cap->ieeeoui != DV_IEEE_OUI)
		return;
	if (pdolby_vision_cap->block_flag != AM_CORRECT) {
		DRM_INFO("DolbyVision block is error\n");
		return;
	}

	if (pdolby_vision_cap->ver == 0)
		DRM_INFO("VSVDB Version: V%d\n",
			 pdolby_vision_cap->ver);

	if (pdolby_vision_cap->ver == 1) {
		DRM_INFO("VSVDB Version: V%d(%d-byte)\n",
			 pdolby_vision_cap->ver,
			 pdolby_vision_cap->length + 1);
		if (pdolby_vision_cap->length == 0xB &&
		    pdolby_vision_cap->low_latency == 0x01)
			DRM_INFO("  LL_YCbCr_422_12BIT\n");
	}
	if (pdolby_vision_cap->ver == 2) {
		DRM_INFO("VSVDB Version: V%d\n", pdolby_vision_cap->ver);
		DRM_INFO("  LL_YCbCr_422_12BIT\n");
	}
}

int am_hdmi_update_downstream_cap_property(struct drm_connector *connector,
					   struct edid *edid)
{
	struct drm_device *dev = connector->dev;
	struct am_hdmi_tx *am_hdmi = to_am_hdmi(connector);
	struct am_hdmi_downstream_hdr_dv_cap *downstream_hdr_dv_cap;
	struct drm_property_blob *tmp;
	struct drm_property *prp_tmp;
	size_t size = 0;
	int ret;

	size = sizeof(struct am_hdmi_downstream_hdr_dv_cap);
	downstream_hdr_dv_cap = kmalloc(size, GFP_KERNEL);
	memset(downstream_hdr_dv_cap, 0, size);
	drm_detect_cea_hdr_dv(connector, edid, downstream_hdr_dv_cap);
	drm_print_cea_hdr_dv(downstream_hdr_dv_cap);
	prp_tmp = am_hdmi->downstream_hdr_dv_cap_property;
	tmp = am_hdmi->downstream_hdr_dv_cap_blob_ptr;
	ret =
	drm_property_replace_global_blob(dev,
					 &tmp,
					 size,
					 downstream_hdr_dv_cap,
					 &connector->base,
					 prp_tmp);
	kfree(downstream_hdr_dv_cap);
	am_hdmi->downstream_hdr_dv_cap_blob_ptr = tmp;
	return ret;
}

struct edid *meson_read_edid(struct drm_connector *connector)
{
	struct am_hdmi_tx *am_hdmi = to_am_hdmi(connector);
	struct edid *edid;
	unsigned int retry_cnt;
	bool valid;

	retry_cnt = 4;
	do {
		edid = drm_get_edid(connector, am_hdmi->ddc);
		if (!edid) {
			ddc_glitch_filter_reset();
			valid = false;
		} else {
			valid = !check_edid_all_zeros((unsigned char *)edid);
			if (!valid) {
				DRM_INFO("EDID ALL ZEROS\n");
				ddc_glitch_filter_reset();
				kfree(edid);
			}
		}
	} while (--retry_cnt && !valid);
	if (!valid)
		edid = NULL;

	return edid;
}

int am_hdmi_tx_get_modes(struct drm_connector *connector)
{
	struct edid *edid = NULL;
	int count = 0;

	if (connector->edid_blob_ptr)
		edid = (struct edid *)connector->edid_blob_ptr->data;
	if (edid)
		count = drm_add_edid_modes(connector, edid);
	else
		add_default_modes(connector);
	return count;
}

enum drm_mode_status am_hdmi_tx_check_mode(struct drm_connector *connector,
					   struct drm_display_mode *mode)
{
	if (am_meson_hdmitx_get_mode(mode))
		return MODE_OK;
	else
		return MODE_NOMODE;
}

static struct drm_encoder *am_hdmi_connector_best_encoder
	(struct drm_connector *connector)
{
	struct am_hdmi_tx *am_hdmi = to_am_hdmi(connector);

	return &am_hdmi->encoder;
}

static enum drm_connector_status am_hdmi_connector_detect
	(struct drm_connector *connector, bool force)
{
	struct am_hdmi_tx *am_hdmi = to_am_hdmi(connector);
	static bool edid_fsh_flag;
	struct edid *edid = NULL;

	/* HPD rising */
	if (am_hdmi->hpd_flag == 1) {
		DRM_INFO("connector_status_connected\n");
		return connector_status_connected;
	}
	/* HPD falling */
	if (am_hdmi->hpd_flag == 2) {
		DRM_INFO("connector_status_disconnected\n");
		return connector_status_disconnected;
	}
	/*if the status is unknown, read GPIO*/
	if (hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO)) {
		DRM_INFO("connector_status_connected\n");
		if (!edid_fsh_flag) {
			edid = meson_read_edid(connector);
			if (edid) {
				drm_mode_connector_update_edid_property(
					connector, edid);
				am_hdmi_update_downstream_cap_property(
					connector, edid);
				hdmi_tx_edid_proc((unsigned char *)edid);
				kfree(edid);
				edid_fsh_flag = true;
			}
		}
		return connector_status_connected;
	}
	if (!(hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO))) {
		DRM_INFO("connector_status_disconnected\n");
		am_hdcp_disconnect(am_hdmi);
		return connector_status_disconnected;
	}

	DRM_INFO("connector_status_unknown\n");
	return connector_status_unknown;
}

static int am_hdmi_check_attr(struct am_hdmi_tx *am_hdmi,
			      struct drm_property *property,
			      uint64_t val)
{
	int rtn_val;
	struct drm_connector *connector;

	rtn_val = 0;
	connector = &am_hdmi->connector;
	if ((property != am_hdmi->color_depth_property) &&
	    (property != am_hdmi->color_space_property) &&
		(property != connector->content_protection_property)) {
		rtn_val = -EINVAL;
	}

	if (property == am_hdmi->color_depth_property) {
		if ((val != COLORDEPTH_24B) &&
		    (val != COLORDEPTH_30B) &&
			(val != COLORDEPTH_36B) &&
			(val != COLORDEPTH_48B)) {
			DRM_INFO("[%s]: Color Depth: %llu\n", __func__, val);
			rtn_val = -EINVAL;
		}
	}

	if (property == am_hdmi->color_space_property) {
		if ((val != COLORSPACE_RGB444) &&
		    (val != COLORSPACE_YUV422) &&
			(val != COLORSPACE_YUV444) &&
			(val != COLORSPACE_YUV420)) {
			DRM_INFO("[%s]: Color Space: %llu\n", __func__, val);
			rtn_val = -EINVAL;
		}
	}

	return rtn_val;
}

static int am_hdmi_create_attr(struct am_hdmi_tx *am_hdmi,
			       char attr[16])
{
	u8 count;
	const struct drm_prop_enum_list *iterator;
	u8 iter_num;

	memset(attr, 0, 16);
	iterator = &am_color_space_enum_names[0];
	iter_num = ARRAY_SIZE(am_color_space_enum_names);
	for (count = 0; count < iter_num; count++) {
		if (iterator->type == (int)am_hdmi->color_space) {
			strcpy(attr, iterator->name);
			attr[strlen(attr)] = ',';
			break;
		}
		iterator++;
	}
	if (count >= iter_num) {
		DRM_INFO("[%s]: color_space %d error!",
			 __func__, (int)am_hdmi->color_space);
		return -EINVAL;
	}

	iterator = &am_color_depth_enum_names[0];
	iter_num = ARRAY_SIZE(am_color_depth_enum_names);
	for (count = 0; count < iter_num; count++) {
		if (iterator->type == (int)am_hdmi->color_depth) {
			strcat(attr, iterator->name);
			break;
		}
		iterator++;
	}
	if (count >= iter_num) {
		DRM_INFO("[%s]: color_depth %d error!",
			 __func__, (int)am_hdmi->color_depth);
		return -EINVAL;
	}

	return 0;
}

static int am_hdmi_connector_atomic_set_property
	(struct drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property, uint64_t val)
{
	struct am_hdmi_tx *am_hdmi = to_am_hdmi(connector);
	char attr[16];
	int rtn_val;

	rtn_val = am_hdmi_check_attr(am_hdmi, property, val);
	if (rtn_val != 0) {
		DRM_INFO("[%s]: Check attr Fail!\n", __func__);
		am_hdmi->color_depth = COLORDEPTH_RESERVED;
		am_hdmi->color_space = COLORSPACE_RESERVED;
		return rtn_val;
	}
	if (property == connector->content_protection_property) {
		DRM_INFO("property:%s       val: %lld\n", property->name, val);
		/* For none atomic commit */
		/* atomic will be filter on drm_moder_object.c */
		if (val == DRM_MODE_CONTENT_PROTECTION_ENABLED) {
			DRM_DEBUG_KMS("only drivers can set CP Enabled\n");
			return -EINVAL;
		}
		state->content_protection = val;
	}

	if (property == am_hdmi->color_depth_property) {
		DRM_INFO("Set Color Depth to: %llu\n", val);
		am_hdmi->color_depth = val;
	}

	if (property == am_hdmi->color_space_property) {
		DRM_INFO("Set Color Space to: %llu\n", val);
		am_hdmi->color_space = val;
	}

	rtn_val = am_hdmi_create_attr(am_hdmi, attr);
	if (rtn_val == 0) {
		DRM_INFO("[%s]: %s\n", __func__, attr);
		setup_attr(attr);
	} else {
		DRM_INFO("[%s]: Create attr Fail!\n", __func__);
	}

	return rtn_val;
}

static int am_hdmi_connector_atomic_get_property
	(struct drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property, uint64_t *val)
{
	struct am_hdmi_tx *am_hdmi = to_am_hdmi(connector);

	if (property == connector->content_protection_property) {
		DRM_INFO("get content_protection val: %d\n",
			 state->content_protection);
		*val = state->content_protection;
		return 0;
	}

	if (property == am_hdmi->color_depth_property) {
		*val = am_hdmi->color_depth;
		DRM_DEBUG_KMS("Get Color Depth val: %llu\n", *val);
		return 0;
	}

	if (property == am_hdmi->color_space_property) {
		*val = am_hdmi->color_space;
		DRM_DEBUG_KMS("Get Color Space val: %llu\n", *val);
		return 0;
	} else {
		DRM_DEBUG_ATOMIC("Unknown property %s\n", property->name);
		return -EINVAL;
	}
	return 0;
}

static void am_hdmi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const
struct drm_connector_helper_funcs am_hdmi_connector_helper_funcs = {
	.get_modes = am_hdmi_tx_get_modes,
	.mode_valid = am_hdmi_tx_check_mode,
	.best_encoder = am_hdmi_connector_best_encoder,
};

static const struct drm_connector_funcs am_hdmi_connector_funcs = {
	.dpms			= drm_atomic_helper_connector_dpms,
	.detect			= am_hdmi_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.set_property   = drm_atomic_helper_connector_set_property,
	.atomic_set_property	= am_hdmi_connector_atomic_set_property,
	.atomic_get_property	= am_hdmi_connector_atomic_get_property,
	.destroy		= am_hdmi_connector_destroy,
	.reset			= drm_atomic_helper_connector_reset,
	.atomic_duplicate_state	= drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_connector_destroy_state,
};

void am_hdmi_encoder_mode_set(struct drm_encoder *encoder,
			      struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	int vic;
	struct am_hdmi_tx *am_hdmi = &am_hdmi_info;
	struct drm_connector *connector;
	struct drm_display_mode *mmode;

	connector = &am_hdmi->connector;
	DRM_INFO("mode : %s, adjusted_mode : %s\n",
		 mode->name,  adjusted_mode->name);
	am_hdmi->hdmi_info.vic = drm_match_cea_mode(adjusted_mode);
	DRM_INFO("the hdmi mode vic : %d\n", am_hdmi->hdmi_info.vic);
	adjusted_mode->mode_color_444 = 0;
	adjusted_mode->mode_color_422 = 0;
	adjusted_mode->mode_color_420 = 0;
	adjusted_mode->mode_color_rgb = 0;
	list_for_each_entry(mmode, &connector->modes, head) {
		vic = drm_match_cea_mode(mmode);
		if (vic == am_hdmi->hdmi_info.vic) {
			DRM_INFO("mode_color:%d,%d,%d,%d\n",
				 mmode->mode_color_444,
				 mmode->mode_color_422, mmode->mode_color_420,
				 mmode->mode_color_rgb);
			adjusted_mode->mode_color_444 |= mmode->mode_color_444;
			adjusted_mode->mode_color_422 |= mmode->mode_color_422;
			adjusted_mode->mode_color_420 |= mmode->mode_color_420;
			adjusted_mode->mode_color_rgb |= mmode->mode_color_rgb;
		}
	}
	/* Store the display mode for plugin/DPMS poweron events */
	memcpy(&am_hdmi->previous_mode, adjusted_mode,
	       sizeof(am_hdmi->previous_mode));
	DRM_INFO("mode_color:%d,%d,%d\n", adjusted_mode->mode_color_444,
		 adjusted_mode->mode_color_422, adjusted_mode->mode_color_420);
	if (am_hdmi->color_depth == COLORDEPTH_RESERVED ||
	    am_hdmi->color_space == COLORSPACE_RESERVED) {
		am_hdmi_create_mode_default_attr(adjusted_mode);
		setup_attr(adjusted_mode->default_attr);
	}
	am_hdmi->color_depth = COLORDEPTH_RESERVED;
	am_hdmi->color_space = COLORSPACE_RESERVED;
}

void am_hdmi_encoder_enable(struct drm_encoder *encoder)
{
	enum vmode_e vmode = get_current_vmode();

	if (vmode == VMODE_HDMI)
		DRM_INFO("enable\n");
	else
		DRM_INFO("enable fail! vmode:%d\n", vmode);

	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &vmode);
	set_vout_vmode(vmode);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &vmode);
}

void am_hdmi_encoder_disable(struct drm_encoder *encoder)
{
	struct am_hdmi_tx *am_hdmi = to_am_hdmi(encoder);
	struct drm_connector_state *state = am_hdmi->connector.state;

	state->content_protection = DRM_MODE_CONTENT_PROTECTION_UNDESIRED;
}

static int am_hdmi_encoder_atomic_check(struct drm_encoder *encoder,
					struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	return 0;
}

static const struct drm_encoder_helper_funcs
	am_hdmi_encoder_helper_funcs = {
	.mode_set	= am_hdmi_encoder_mode_set,
	.enable		= am_hdmi_encoder_enable,
	.disable	= am_hdmi_encoder_disable,
	.atomic_check	= am_hdmi_encoder_atomic_check,
};

static const struct drm_encoder_funcs am_hdmi_encoder_funcs = {
	.destroy        = drm_encoder_cleanup,
};

static int am_hdmi_i2c_write(struct am_hdmi_tx *am_hdmi,
			     unsigned char *buf, unsigned int length)
{
	struct am_hdmi_i2c *i2c = am_hdmi->i2c;

	i2c->slave_reg = buf[0];
	length--;
	buf++;

	while (length--) {

		hdmitx_wr_reg(HDMITX_DWC_I2CM_DATAO, *buf++);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, i2c->slave_reg++);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 4);
		usleep_range(500, 520);
		if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0))
			DRM_INFO("ddc w1b error\n");
		hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);
	}

	return 0;
}

static int am_hdmi_i2c_read(struct am_hdmi_tx *am_hdmi,
			    unsigned char *buf, unsigned int length,
			    bool edid_flag)
{
	struct am_hdmi_i2c *i2c = am_hdmi->i2c;
	unsigned int timeout;
	unsigned char i2c_stat;
	unsigned int read_stride, i2c_oprt, us_tm;
	unsigned int count, read_reg;

	if (edid_flag && (length & 7) == 0) {
		read_stride = 8;
		i2c_oprt = 1 << 2;
		us_tm = 2000;
		read_reg = HDMITX_DWC_I2CM_READ_BUFF0;
	} else {
		read_stride = 1;
		i2c_oprt = 1 << 0;
		us_tm = 250;
		read_reg = HDMITX_DWC_I2CM_DATAI;
	}
	i2c->slave_reg = buf[0];
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 1 << 1);
	while (length) {
		hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, i2c->slave_reg);
		i2c->slave_reg += read_stride;
		if (i2c->is_segment)
			hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, i2c_oprt << 1);
		else
			hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, i2c_oprt << 0);

		timeout = 0;
		usleep_range(us_tm, us_tm + 20);
		i2c_stat = hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0);
		while ((!(i2c_stat & (1 << 1))) &&
		       (timeout < 10)) {
			usleep_range(us_tm, us_tm + 20);
			i2c_stat = hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0);
			timeout++;
		}
		hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 1 << 1);
		if (timeout == 10) {
			DRM_INFO("ddc timeout\n");
			return -1;
		}

		for (count = 0; count < read_stride; count++)
			*buf++ = hdmitx_rd_reg(read_reg + count);
		length -= read_stride;
	}
	i2c->is_segment = 0;

	return 0;
}

static int am_hdmi_i2c_xfer(struct i2c_adapter *adap,
			    struct i2c_msg *msgs, int num)
{
	struct am_hdmi_tx *am_hdmi =  i2c_get_adapdata(adap);
	struct am_hdmi_i2c *i2c = am_hdmi->i2c;
	u8 addr = msgs[0].addr;
	int i, ret = 0;
	unsigned int   data32;
	bool edid_flag = false;

	dev_dbg(am_hdmi->dev, "xfer: num: %d, addr: %#x\n", num, addr);

	for (i = 0; i < num; i++) {
		if (msgs[i].len == 0) {
			dev_dbg(am_hdmi->dev,
				"unsupported transfer %d/%d, no data\n",
				i + 1, num);
			return -EOPNOTSUPP;
		}
	}

	mutex_lock(&i2c->lock);

	/* Clear the EDID interrupt flag and unmute the interrupt */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SOFTRSTZ, 0);
	/* Mute The Interrupt of I2Cmasterdone, Otherwise Affect the Interrupt of HPD.*/
	data32  = 0;
	data32 |= (0 << 2);
	data32 |= (1 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_I2CM_STAT0, data32);
	/* TODO */
	hdmitx_ddc_hw_op(DDC_MUX_DDC);

	if ((addr == 0x50) || (addr == DDC_SEGMENT_ADDR)) {
		hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, 0x50);
		edid_flag = true;
	} else {
		i2c->slave_reg = 0;
		hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, addr);
	}
	/* Set segment pointer for I2C extended read mode operation */
	i2c->is_segment = 0;

	for (i = 0; i < num; i++) {
		if (msgs[i].addr == DDC_SEGMENT_ADDR) {
			if (msgs[i].buf[0] != 0) {
				i2c->is_segment = 1;
				hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR,
					      DDC_SEGMENT_ADDR);
				hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR,
					      *msgs[i].buf);
			} else {
				hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR, 0);
				hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR, 0);
			}
		} else {
			if (msgs[i].flags & I2C_M_RD)
				ret = am_hdmi_i2c_read(am_hdmi, msgs[i].buf,
						       msgs[i].len, edid_flag);
			else
				ret = am_hdmi_i2c_write(am_hdmi, msgs[i].buf,
							msgs[i].len);
		}
		if (ret < 0)
			break;
	}

	if (!ret)
		ret = num;

	mutex_unlock(&i2c->lock);

	return ret;
}

static u32 am_hdmi_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm am_hdmi_algorithm = {
	.master_xfer	= am_hdmi_i2c_xfer,
	.functionality	= am_hdmi_i2c_func,
};

static struct i2c_adapter *am_hdmi_i2c_adapter(struct am_hdmi_tx *am_hdmi)
{
	struct i2c_adapter *adap;
	struct am_hdmi_i2c *i2c;
	int ret;

	i2c = devm_kzalloc(am_hdmi->priv->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c) {
		ret = -ENOMEM;
		DRM_INFO("error : %d\n", ret);
	}

	mutex_init(&i2c->lock);

	adap = &i2c->adap;
	adap->class = I2C_CLASS_DDC;
	adap->owner = THIS_MODULE;
	adap->dev.parent = am_hdmi->priv->dev;
	adap->dev.of_node = am_hdmi->priv->dev->of_node;
	adap->algo = &am_hdmi_algorithm;
	strlcpy(adap->name, "Am HDMI", sizeof(adap->name));
	i2c_set_adapdata(adap, am_hdmi);

	ret = i2c_add_adapter(adap);
	if (ret) {
		DRM_INFO("cannot add %s I2C adapter\n",
			 adap->name);
		devm_kfree(am_hdmi->priv->dev, i2c);
		return ERR_PTR(ret);
	}
	am_hdmi->i2c = i2c;
	DRM_INFO("registered %s I2C bus driver\n", adap->name);

	return adap;
}

static irqreturn_t am_hdmi_hardirq(int irq, void *dev_id)
{
	unsigned int data32 = 0;
	irqreturn_t ret = IRQ_NONE;
	struct hdmitx_dev *hdmitx_dev;

	data32 = hdmitx_rd_reg(HDMITX_TOP_INTR_STAT);
	/* Clear The Interrupt ot TX Controller IP*/
	hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, ~0);
	DRM_INFO("hotplug irq: %x\n", data32);
	hdmitx_dev = get_hdmitx_device();
	if (hdmitx_dev->hpd_lock == 1) {
		DRM_INFO("[%s]: HDMI hpd locked\n", __func__);
		return ret;
	}

	/* check HPD status */
	if ((data32 & (1 << 1)) && (data32 & (1 << 2))) {
		if (hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO))
			data32 &= ~(1 << 2);
		else
			data32 &= ~(1 << 1);
	}

	if ((data32 & (1 << 1)) || (data32 & (1 << 2))) {
		ret = IRQ_WAKE_THREAD;
		am_hdmi_info.hpd_flag = 0;
		if (data32 & (1 << 1)) {
			am_hdmi_info.hpd_flag = 1;/* HPD rising */
		}
		if (data32 & (1 << 2)) {
			am_hdmi_info.hpd_flag = 2;/* HPD falling */
			am_hdcp_disconnect(&am_hdmi_info);
		}
		/* ack INTERNAL_INTR or else*/
		hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, data32 | 0x7);
	}
	return ret;
}

static irqreturn_t am_hdmi_irq(int irq, void *dev_id)
{
	struct am_hdmi_tx *am_hdmi = dev_id;
	struct hdmitx_dev *hdmitx_dev;
	struct edid *edid = NULL;
	struct drm_connector *connector;

	hdmitx_dev = get_hdmitx_device();
	if (hdmitx_dev->hpd_lock == 1) {
		DRM_INFO("[%s]: HDMI hpd locked\n", __func__);
		return IRQ_HANDLED;
	}
	if (am_hdmi->hpd_flag == 1) {
		connector = &am_hdmi->connector;
		edid = meson_read_edid(connector);
		if (edid) {
			drm_mode_connector_update_edid_property(
				connector, edid);
			am_hdmi_update_downstream_cap_property(connector, edid);
			hdmi_tx_edid_proc((unsigned char *)edid);
			kfree(edid);
		}
		hdmitx_dev->hpd_state = 1;
	} else if (am_hdmi->hpd_flag == 2) {
		hdmitx_dev->hpd_state = 0;
	}
	hdmitx_notify_hpd(hdmitx_dev->hpd_state, NULL);
	drm_helper_hpd_irq_event(am_hdmi->connector.dev);
	return IRQ_HANDLED;
}

static const struct of_device_id am_meson_hdmi_dt_ids[] = {
	{ .compatible = "amlogic,drm-amhdmitx", },
	{},
};

MODULE_DEVICE_TABLE(of, am_meson_hdmi_dt_ids);

struct drm_connector *am_meson_hdmi_connector(void)
{
	return &am_hdmi_info.connector;
}

static void am_meson_hdmi_connector_init_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;
	/* Connector */
	am_hdmi->color_depth = COLORDEPTH_RESERVED;
	prop = drm_property_create_enum(drm_dev, 0, "max bpc",
					am_color_depth_enum_names,
			ARRAY_SIZE(am_color_depth_enum_names));
	if (prop) {
		am_hdmi->color_depth_property = prop;
		drm_object_attach_property(&am_hdmi->connector.base, prop,
					   am_hdmi->color_depth);
	} else {
		DRM_ERROR("Failed to add Color Depth property\n");
	}

	am_hdmi->color_space = COLORSPACE_RESERVED;
	prop = drm_property_create_enum(drm_dev, 0, "Colorspace",
					am_color_space_enum_names,
			ARRAY_SIZE(am_color_space_enum_names));
	if (prop) {
		am_hdmi->color_space_property = prop;
		drm_object_attach_property(&am_hdmi->connector.base, prop,
					   am_hdmi->color_space);
	} else {
		DRM_ERROR("Failed to add Color Space property\n");
	}

	prop = drm_property_create(drm_dev, DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "HDR DV Cap", 0);
	if (prop) {
		am_hdmi->downstream_hdr_dv_cap_property = prop;
		drm_object_attach_property(&am_hdmi->connector.base,
					   prop,
					   0);
	} else {
		DRM_ERROR("Failed to add HDR DV Cap property\n");
	}
	am_hdmi->downstream_hdr_dv_cap_blob_ptr = NULL;
}

static int am_meson_hdmi_bind(struct device *dev,
			      struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm = data;
	struct meson_drm *priv = drm->dev_private;
	struct am_hdmi_tx *am_hdmi;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	int ret;
	int irq;

	DRM_INFO("[%s] in\n", __func__);
	am_hdmi = &am_hdmi_info;
	DRM_INFO("drm hdmitx init and version:%s\n", DRM_HDMITX_VER);
	am_hdmi->priv = priv;
	encoder = &am_hdmi->encoder;
	connector = &am_hdmi->connector;

	/* Connector */
	am_hdmi->connector.polled = DRM_CONNECTOR_POLL_HPD;
	drm_connector_helper_add(connector,
				 &am_hdmi_connector_helper_funcs);

	ret = drm_connector_init(drm, connector, &am_hdmi_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi tx connector\n");
		return ret;
	}
	am_meson_hdmi_connector_init_property(drm, am_hdmi);

	connector->interlace_allowed = 1;

	/* Encoder */
	drm_encoder_helper_add(encoder, &am_hdmi_encoder_helper_funcs);

	ret = drm_encoder_init(drm, encoder, &am_hdmi_encoder_funcs,
			       DRM_MODE_ENCODER_TVDAC, "am_hdmi_encoder");
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi encoder\n");
		return ret;
	}

	encoder->possible_crtcs = BIT(0);

	drm_mode_connector_attach_encoder(connector, encoder);

	/*DDC init*/
	am_hdmi->ddc = am_hdmi_i2c_adapter(am_hdmi);
	DRM_INFO("hdmitx:DDC init complete\n");
	/*Hotplug irq*/
	irq = platform_get_irq(pdev, 0);
	DRM_INFO("hdmi connector irq:%d\n", irq);
	if (irq < 0)
		return irq;
	hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, 0x7);
	ret = devm_request_threaded_irq(am_hdmi->priv->dev, irq,
					am_hdmi_hardirq,
					am_hdmi_irq, IRQF_SHARED,
		dev_name(am_hdmi->priv->dev), am_hdmi);
	if (ret) {
		dev_err(am_hdmi->priv->dev,
			"failed to request hdmi irq: %d\n", ret);
	}

	/*HDCP INIT*/
	ret = am_hdcp_init(am_hdmi);
	if (ret)
		DRM_DEBUG_KMS("HDCP init failed, skipping.\n");
	DRM_INFO("[%s] out\n", __func__);
	return 0;
}

static void am_meson_hdmi_unbind(struct device *dev,
				 struct device *master, void *data)
{
	am_hdmi_info.connector.funcs->destroy(&am_hdmi_info.connector);
	am_hdmi_info.encoder.funcs->destroy(&am_hdmi_info.encoder);
}

static const struct component_ops am_meson_hdmi_ops = {
	.bind	= am_meson_hdmi_bind,
	.unbind	= am_meson_hdmi_unbind,
};

void am_hdmitx_hdcp_disable(void)
{
	cancel_delayed_work(&am_hdmi_info.hdcp_prd_proc);
	flush_workqueue(am_hdmi_info.hdcp_wq);
	am_hdcp_disable(&am_hdmi_info);
}

void am_hdmitx_hdcp_enable(void)
{
	queue_delayed_work(am_hdmi_info.hdcp_wq,
		&am_hdmi_info.hdcp_prd_proc, 0);
}

void am_hdmitx_hdcp_result(unsigned int *exe_type,
		unsigned int *result_type)
{
	*exe_type = am_hdmi_info.hdcp_execute_type;
	*result_type = (unsigned int)am_hdmi_info.hdcp_result;
}

void am_hdmitx_set_hdcp_mode(unsigned int user_type)
{
	am_hdmi_info.hdcp_user_type = user_type;
}
static int am_meson_hdmi_probe(struct platform_device *pdev)
{
	struct hdmitx_dev *hdmitx_dev;

	DRM_INFO("[%s] in\n", __func__);
	memset(&am_hdmi_info, 0, sizeof(am_hdmi_info));
	hdcp_comm_init(&am_hdmi_info);
	hdmitx_dev = get_hdmitx_device();
	hdmitx_dev->hwop.am_hdmitx_hdcp_disable = am_hdmitx_hdcp_disable;
	hdmitx_dev->hwop.am_hdmitx_hdcp_enable = am_hdmitx_hdcp_enable;
	hdmitx_dev->hwop.am_hdmitx_hdcp_result = am_hdmitx_hdcp_result;
	hdmitx_dev->hwop.am_hdmitx_set_hdcp_mode = am_hdmitx_set_hdcp_mode;
	return component_add(&pdev->dev, &am_meson_hdmi_ops);
}

static int am_meson_hdmi_remove(struct platform_device *pdev)
{
	hdcp_comm_exit(&am_hdmi_info);
	component_del(&pdev->dev, &am_meson_hdmi_ops);
	return 0;
}

static struct platform_driver am_meson_hdmi_pltfm_driver = {
	.probe  = am_meson_hdmi_probe,
	.remove = am_meson_hdmi_remove,
	.driver = {
		.name = "meson-amhdmitx",
		.of_match_table = am_meson_hdmi_dt_ids,
	},
};

module_platform_driver(am_meson_hdmi_pltfm_driver);

MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Meson Drm HDMI driver");
MODULE_LICENSE("GPL");
