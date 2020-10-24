/*
 * drivers/amlogic/drm/meson_hdcp.c
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

#include <linux/component.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/arm-smccc.h>
#include <linux/workqueue.h>
#include "meson_drv.h"
#include "meson_hdmi.h"
#include "meson_hdcp.h"

static int hdcp_topo_st = -1;
static int hdmitx_hdcp_opr(unsigned int val)
{
	struct arm_smccc_res res;

	if (val == 1) { /* HDCP14_ENABLE */
		arm_smccc_smc(0x82000010, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 2) { /* HDCP14_RESULT */
		arm_smccc_smc(0x82000011, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0) & 0xffffffff);
	}
	if (val == 0) { /* HDCP14_INIT */
		arm_smccc_smc(0x82000012, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 3) { /* HDCP14_EN_ENCRYPT */
		arm_smccc_smc(0x82000013, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 4) { /* HDCP14_OFF */
		arm_smccc_smc(0x82000014, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 5) {	/* HDCP_MUX_22 */
		arm_smccc_smc(0x82000015, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 6) {	/* HDCP_MUX_14 */
		arm_smccc_smc(0x82000016, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 7) { /* HDCP22_RESULT */
		arm_smccc_smc(0x82000017, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0) & 0xffffffff);
	}
	if (val == 0xa) { /* HDCP14_KEY_LSTORE */
		arm_smccc_smc(0x8200001a, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0) & 0xffffffff);
	}
	if (val == 0xb) { /* HDCP22_KEY_LSTORE */
		arm_smccc_smc(0x8200001b, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0) & 0xffffffff);
	}
	if (val == 0xc) { /* HDCP22_KEY_SET_DUK */
		arm_smccc_smc(0x8200001c, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0) & 0xffffffff);
	}
	if (val == 0xd) { /* HDCP22_SET_TOPO */
		arm_smccc_smc(0x82000083, hdcp_topo_st, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 0xe) { /* HDCP22_GET_TOPO */
		arm_smccc_smc(0x82000084, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0) & 0xffffffff);
	}
	return -1;
}

static int am_hdcp14_enable(struct am_hdmi_tx *am_hdmi)
{
	/*call hdmitx hdcp14 function*/
	drm_hdcp14_on((ulong)am_hdmi);
	return 0;
}

static int am_hdcp22_enable(struct am_hdmi_tx *am_hdmi)
{
	hdmitx_set_reg_bits(HDMITX_DWC_MC_CLKDIS, 1, 6, 1);
	hdmitx_set_reg_bits(HDMITX_DWC_HDCP22REG_CTRL, 3, 1, 2);
	return 0;
}

int am_hdcp22_init(struct am_hdmi_tx *am_hdmi)
{
	am_hdmi->hdcp_mode = HDCP_MODE22;
	drm_hdcp22_init();

	return 0;
}

int get_hdcp_downstream_ver(struct am_hdmi_tx *am_hdmi)
{
	unsigned int hdcp_downstream_type = 0x1;
	int st;

	/*if tx has hdcp22, then check if downstream support hdcp22*/
	if (am_hdmi->hdcp_tx_type & 0x2) {
		hdmitx_ddc_hw_op(DDC_MUX_DDC);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, HDCP_SLAVE);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, HDCP2_VERSION);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 0);
		mdelay(2);
		if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0)) {
			st = 0;
			DRM_INFO("ddc rd8b error 0x%02x 0x%02x\n",
				 HDCP_SLAVE, HDCP2_VERSION);
		} else
			st = 1;
		hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);
		if (hdmitx_rd_reg(HDMITX_DWC_I2CM_DATAI) & (1 << 2))
			hdcp_downstream_type = 0x3;
	} else {
	/* if tx has hdcp14 or no key,
	 * then downstream support hdcp14 acquiescently
	 */
		hdcp_downstream_type = 0x1;
	}
	am_hdmi->hdcp_downstream_type = hdcp_downstream_type;

	DRM_INFO("downstream support hdcp14: %d\n",
		 hdcp_downstream_type & 0x1);
	DRM_INFO("downstream support hdcp22: %d\n",
		 (hdcp_downstream_type & 0x2) >> 1);
	return hdcp_downstream_type;
}

static int am_get_hdcp_exe_type(struct am_hdmi_tx *am_hdmi)
{
	int type;

	if (am_hdmi->hdcp_user_type == 0)
		return HDCP_NULL;
	if (!am_hdmi->hdcp_downstream_type &&
		am_hdmi->hdcp_tx_type)
		get_hdcp_downstream_ver(am_hdmi);
	switch (am_hdmi->hdcp_tx_type & 3) {
	case 3:
		if ((am_hdmi->hdcp_downstream_type & 0x2) &&
			(am_hdmi->hdcp_user_type & 0x2))
			type = HDCP_MODE22;
		else if ((am_hdmi->hdcp_downstream_type & 0x1) &&
			 (am_hdmi->hdcp_user_type & 0x1))
			type = HDCP_MODE14;
		else
			type = HDCP_NULL;
		break;
	case 2:
		if ((am_hdmi->hdcp_downstream_type & 0x2) &&
			(am_hdmi->hdcp_user_type & 0x2))
			type = HDCP_MODE22;
		else
			type = HDCP_NULL;
		break;
	case 1:
		if ((am_hdmi->hdcp_downstream_type & 0x1) &&
			(am_hdmi->hdcp_user_type & 0x1))
			type = HDCP_MODE14;
		else
			type = HDCP_NULL;
		break;
	default:
		type = HDCP_NULL;
	}
	return type;
}

int am_hdcp_disable(struct am_hdmi_tx *am_hdmi)
{
	if (am_hdmi->hdcp_execute_type == HDCP_MODE22) {
		hdmitx_hdcp_opr(6);
		am_hdmi->hdcp_report = HDCP_TX22_STOP;
		wake_up(&am_hdmi->hdcp_comm_queue);
	}
	/*call hdmitx hdcp14 function*/
	if (am_hdmi->hdcp_execute_type  == HDCP_MODE14)
		drm_hdcp14_off((ulong)am_hdmi);
	am_hdmi->hdcp_execute_type = HDCP_NULL;
	am_hdmi->hdcp14_en_flag = 0;
	am_hdmi->hdcp22_en_flag = 0;
	am_hdmi->hdcp_result = 0;
	return 0;
}

int am_hdcp_disconnect(struct am_hdmi_tx *am_hdmi)
{
	int hpd;

	hpd = hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO);
	if (hpd)
		return -1;
	am_hdmi->hdcp_report = HDCP_TX22_DISCONNECT;
	am_hdmi->hdcp_downstream_type = 0;
	am_hdmi->hdcp_execute_type = HDCP_NULL;
	am_hdmi->hdcp14_en_flag = 0;
	am_hdmi->hdcp22_en_flag = 0;
	am_hdmi->hdcp_result = 0;
	wake_up(&am_hdmi->hdcp_comm_queue);
	return 0;
}

void am_hdcp_enable(struct work_struct *work)
{
	int hpd;
	struct am_hdmi_tx *am_hdmi;

	hpd = hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO);
	if (!hpd)
		return;
	am_hdmi = container_of((struct delayed_work *)work,
		struct am_hdmi_tx, hdcp_prd_proc);
	if (am_hdmi->hdcp_execute_type == HDCP_MODE22 &&
		am_hdmi->hdcp22_en_flag)
		am_hdmi->hdcp_result = hdmitx_hdcp_opr(7);
	if (am_hdmi->hdcp_execute_type == HDCP_MODE14 &&
		am_hdmi->hdcp14_en_flag) {
		/*read hdcp14 result*/
		am_hdmi->hdcp_result = hdmitx_hdcp_opr(2);
		if (am_hdmi->hdcp_result != 1 &&
			am_hdmi->hdcp_retry_cnt++ > 1) {
			DRM_INFO("[%s]: hdcp14 fail\n",
				 __func__);
			am_hdmi->hdcp_retry_cnt = 0;
		}
	}

	if (!am_hdmi->hdcp_execute_type)
		am_hdmi->hdcp_execute_type =
			am_get_hdcp_exe_type(am_hdmi);
	if (am_hdmi->hdcp_execute_type == HDCP_MODE22 &&
		!am_hdmi->hdcp22_en_flag) {
		am_hdcp22_enable(am_hdmi);
		am_hdmi->hdcp22_en_flag = 1;
		am_hdmi->hdcp_report = HDCP_TX22_START;
		wake_up(&am_hdmi->hdcp_comm_queue);
		DRM_INFO("[%s]: report=%d, use_type=%u, execute=%u\n",
			 __func__, am_hdmi->hdcp_report,
			 am_hdmi->hdcp_user_type, am_hdmi->hdcp_execute_type);
	}
	if (am_hdmi->hdcp_execute_type == HDCP_MODE14 &&
		!am_hdmi->hdcp14_en_flag) {
		am_hdcp14_enable(am_hdmi);
		am_hdmi->hdcp14_en_flag = 1;
		am_hdmi->hdcp_retry_cnt = 0;
		DRM_INFO("[%s]: report=%d, use_type=%u, execute=%u\n",
			 __func__, am_hdmi->hdcp_report,
			 am_hdmi->hdcp_user_type, am_hdmi->hdcp_execute_type);
	}

	queue_delayed_work(am_hdmi->hdcp_wq,
		&am_hdmi->hdcp_prd_proc, HZ * 3);
}

int am_hdcp_init(struct am_hdmi_tx *am_hdmi)
{
	int ret;

	ret = drm_connector_attach_content_protection_property(
			&am_hdmi->connector);
	if (ret)
		return ret;
	return 0;
}

int get_hdcp_hdmitx_version(struct am_hdmi_tx *am_hdmi)
{
	unsigned int hdcp_tx_type = 0;

	hdcp_tx_type |= hdmitx_hdcp_opr(0xa);
	hdcp_tx_type |= ((hdmitx_hdcp_opr(0xb)) << 1);
	am_hdmi->hdcp_tx_type = hdcp_tx_type;
	DRM_INFO("hdmitx support hdcp14: %d\n", hdcp_tx_type & 0x1);
	DRM_INFO("hdmitx support hdcp22: %d\n", (hdcp_tx_type & 0x2) >> 1);
	return hdcp_tx_type;
}
