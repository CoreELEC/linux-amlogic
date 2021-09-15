/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_VRR_DRV_H__
#define __AML_VRR_DRV_H__
#include <linux/amlogic/media/vrr/vrr.h>

/* ver:20210806: initial version */
#define VRR_DRV_VERSION  "20210806"

#define VRRPR(fmt, args...)      pr_info("vrr: " fmt "", ## args)
#define VRRERR(fmt, args...)     pr_err("vrr error: " fmt "", ## args)

#define VRR_DBG_PR_NORMAL       BIT(0)
#define VRR_DBG_PR_ISR          BIT(1)

#define VRR_MAX_DRV    3

enum vrr_chip_e {
	VRR_CHIP_T7 = 0,
	VRR_CHIP_T3 = 1,
	VRR_CHIP_MAX,
};

struct vrr_data_s {
	unsigned int chip_type;
	const char *chip_name;
	unsigned int drv_max;
	unsigned int offset[VRR_MAX_DRV];
};

#define VRR_STATE_EN          BIT(0)
#define VRR_STATE_MODE_HW     BIT(4)
#define VRR_STATE_MODE_SW     BIT(5)
#define VRR_STATE_ENCL        BIT(8)
#define VRR_STATE_ENCP        BIT(9)

struct aml_vrr_drv_s {
	unsigned int index;
	unsigned int state;
	unsigned int enable;
	unsigned int sw_timer_cnt;
	unsigned int sw_timer_flag;

	struct vrr_device_s *vrr_dev;
	struct cdev cdev;
	struct vrr_data_s *data;
	struct device *dev;
	struct timer_list sw_timer;
};

//===========================================================================
// For ENCL VRR
//===========================================================================
#define VRR_ENC_INDEX                              0

extern int lcd_venc_sel;
extern int lcd_vrr_timer_cnt;
int lcd_vrr_test_en(int mode);
int lcd_vrr_test_init(int venc_sel);
int lcd_get_vrr(char *buf);

#endif
