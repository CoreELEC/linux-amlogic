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

#define DEVICE_NAME "amhdmitx"
struct am_hdmi_tx am_hdmi_info;

struct am_vout_mode {
	char name[DRM_DISPLAY_MODE_LEN];
	enum vmode_e mode;
	int width, height, vrefresh;
	unsigned int flags;
};

static struct am_vout_mode am_vout_modes[] = {
	{ "1080p60hz", VMODE_HDMI, 1920, 1080, 60, 0},
	{ "1080p30hz", VMODE_HDMI, 1920, 1080, 30, 0},
	{ "1080p50hz", VMODE_HDMI, 1920, 1080, 50, 0},
	{ "1080p25hz", VMODE_HDMI, 1920, 1080, 25, 0},
	{ "1080p24hz", VMODE_HDMI, 1920, 1080, 24, 0},
	{ "2160p30hz", VMODE_HDMI, 3840, 2160, 30, 0},
	{ "2160p60hz", VMODE_HDMI, 3840, 2160, 60, 0},
	{ "2160p50hz", VMODE_HDMI, 3840, 2160, 50, 0},
	{ "2160p25hz", VMODE_HDMI, 3840, 2160, 25, 0},
	{ "2160p24hz", VMODE_HDMI, 3840, 2160, 24, 0},
	{ "smpte30hz", VMODE_HDMI, 4096, 2160, 30, 0},
	{ "smpte60hz", VMODE_HDMI, 4096, 2160, 60, 0},
	{ "smpte50hz", VMODE_HDMI, 4096, 2160, 50, 0},
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

char *am_meson_hdmi_get_voutmode(struct drm_display_mode *mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(am_vout_modes); i++) {
		if (am_vout_modes[i].width == mode->hdisplay &&
		    am_vout_modes[i].height == mode->vdisplay &&
		    am_vout_modes[i].vrefresh == mode->vrefresh &&
		    am_vout_modes[i].flags ==
		    (mode->flags & DRM_MODE_FLAG_INTERLACE))
			return am_vout_modes[i].name;
	}
	return NULL;
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
	struct edid *edid;
	int count = 0;

	DRM_INFO("get_edid\n");
	edid = meson_read_edid(connector);
	if (edid) {
		drm_mode_connector_update_edid_property(connector, edid);
		count = drm_add_edid_modes(connector, edid);
		kfree(edid);
	} else {
		add_default_modes(connector);
	}
	return count;
}

enum drm_mode_status am_hdmi_tx_check_mode(struct drm_connector *connector,
					   struct drm_display_mode *mode)
{
	if (am_meson_hdmi_get_voutmode(mode))
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
	uint8_t count;
	const struct drm_prop_enum_list *iterator;
	uint8_t iter_num;

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
	const char attr1[16] = "rgb,8bit";
	const char attr2[16] = "420,8bit";
	char attr_tmp[16];
	int vic;
	struct am_hdmi_tx *am_hdmi = &am_hdmi_info;

	DRM_INFO("mode : %s, adjusted_mode : %s\n",
		mode->name,  adjusted_mode->name);
	am_hdmi->hdmi_info.vic = drm_match_cea_mode(adjusted_mode);
	vic = am_hdmi->hdmi_info.vic;
	DRM_INFO("the hdmi mode vic : %d\n", am_hdmi->hdmi_info.vic);
	/* Store the display mode for plugin/DPMS poweron events */
	memcpy(&am_hdmi->previous_mode, adjusted_mode,
	       sizeof(am_hdmi->previous_mode));
	get_attr(attr_tmp);
	if (strlen(attr_tmp) == 0) {
		if (vic == 96 || vic == 97 || vic == 101 || vic == 102 ||
		    vic == 106 || vic == 107)
			setup_attr(attr2);
		else
			setup_attr(attr1);
	}
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
	int stat;

	if (!i2c->is_regaddr) {
		/* Use the first write byte as register address */
		i2c->slave_reg = buf[0];
		length--;
		buf++;
		i2c->is_regaddr = 1;
	}

	while (length--) {
		reinit_completion(&i2c->cmp);

		hdmitx_wr_reg(HDMITX_DWC_I2CM_DATAO, *buf++);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, i2c->slave_reg++);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 4);

		stat = wait_for_completion_timeout(&i2c->cmp, HZ / 100);

		stat = 1;
		/* Check for error condition on the bus */
		if (i2c->stat & 1)
			return -EIO;
	}

	return 0;
}

static int am_hdmi_i2c_read(struct am_hdmi_tx *am_hdmi,
	unsigned char *buf, unsigned int length)
{
	struct am_hdmi_i2c *i2c = am_hdmi->i2c;
	int stat;

	if (!i2c->is_regaddr) {
		dev_dbg(am_hdmi->dev, "set read register address to 0\n");
		i2c->slave_reg = 0x00;
		i2c->is_regaddr = 1;
	}

	while (length--) {
		reinit_completion(&i2c->cmp);

		hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, i2c->slave_reg++);
		if (i2c->is_segment)
			hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 1);
		else
			hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 0);

		stat = wait_for_completion_timeout(&i2c->cmp, HZ / 100);

		stat = 1;

		/* Check for error condition on the bus */
		if (i2c->stat & 0x1)
			return -EIO;

		*buf++ = hdmitx_rd_reg(HDMITX_DWC_I2CM_DATAI);
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

	/* Set slave device address taken from the first I2C message */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, addr);

	/* Set slave device register address on transfer */
	i2c->is_regaddr = 0;

	/* Set segment pointer for I2C extended read mode operation */
	i2c->is_segment = 0;

	for (i = 0; i < num; i++) {
		dev_dbg(am_hdmi->dev, "xfer: num: %d/%d, len: %d, flags: %#x\n",
			i + 1, num, msgs[i].len, msgs[i].flags);
		if (msgs[i].addr == DDC_SEGMENT_ADDR && msgs[i].len == 1) {
			i2c->is_segment = 1;
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR,
				DDC_SEGMENT_ADDR);
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR, *msgs[i].buf);
		} else {
			if (msgs[i].flags & I2C_M_RD)
				ret = am_hdmi_i2c_read(am_hdmi, msgs[i].buf,
						       msgs[i].len);
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
	init_completion(&i2c->cmp);

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

	data32 = hdmitx_rd_reg(HDMITX_TOP_INTR_STAT);
	/* Clear The Interrupt ot TX Controller IP*/
	hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, ~0);
	/* check HPD status */
	if ((data32 & (1 << 1)) && (data32 & (1 << 2))) {
		if (hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO))
			data32 &= ~(1 << 2);
		else
			data32 &= ~(1 << 1);
	}

	if ((data32 & (1 << 1)) || (data32 & (1 << 2))) {
		ret = IRQ_WAKE_THREAD;
		DRM_INFO("hotplug irq: %x\n", data32);
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

	drm_helper_hpd_irq_event(am_hdmi->connector.dev);
	return IRQ_HANDLED;
}

static const struct of_device_id am_meson_hdmi_dt_ids[] = {
	{ .compatible = "amlogic,drm-amhdmitx", },
	{},
};

MODULE_DEVICE_TABLE(of, am_meson_hdmi_dt_ids);

static void am_meson_hdmi_connector_init_property(struct drm_device *drm_dev,
		struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;
	/* Connector */
	am_hdmi->color_depth = MESON_DEFAULT_COLOR_DEPTH;
	prop = drm_property_create_enum(drm_dev, 0, "Color Depth",
			am_color_depth_enum_names,
			ARRAY_SIZE(am_color_depth_enum_names));
	if (prop) {
		am_hdmi->color_depth_property = prop;
		drm_object_attach_property(&am_hdmi->connector.base, prop,
				am_hdmi->color_depth);
	} else {
		DRM_ERROR("Failed to add Color Depth property\n");
	}

	am_hdmi->color_space = MESON_DEFAULT_COLOR_SPACE;
	prop = drm_property_create_enum(drm_dev, 0, "Color Space",
			am_color_space_enum_names,
			ARRAY_SIZE(am_color_space_enum_names));
	if (prop) {
		am_hdmi->color_space_property = prop;
		drm_object_attach_property(&am_hdmi->connector.base, prop,
				am_hdmi->color_space);
	} else {
		DRM_ERROR("Failed to add Color Space property\n");
	}

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

	am_hdmi = devm_kzalloc(priv->dev, sizeof(*am_hdmi),
				       GFP_KERNEL);
	if (!am_hdmi)
		return -ENOMEM;
	memcpy(&am_hdmi_info, am_hdmi, sizeof(*am_hdmi));
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
		am_hdmi_hardirq, am_hdmi_irq, IRQF_SHARED,
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
	am_hdcp_disable(&am_hdmi_info);
}

void am_hdmitx_hdcp_enable(void)
{
	am_hdcp_enable(&am_hdmi_info);
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
