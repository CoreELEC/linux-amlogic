/*
 * drivers/amlogic/media/vout/vdac/vdac_dev.h
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

#ifndef _VDAC_DEV_H_
#define _VDAC_DEV_H_

#define HHI_VDAC_CNTL0        0xbd
#define HHI_VDAC_CNTL1        0xbe
#define HHI_VDAC_CNTL0_G12A   0xbb
#define HHI_VDAC_CNTL1_G12A   0xbc

#define HHI_VIID_CLK_DIV      0x4a
#define HHI_VIID_CLK_CNTL     0x4b
#define HHI_VIID_DIVIDER_CNTL 0x4c
#define HHI_VID_CLK_CNTL2     0x65
#define HHI_VID_DIVIDER_CNTL  0x66

#define VENC_VDAC_DACSEL0     0x1b78

#define VDAC_CTRL_MAX         10

enum vdac_cpu_type {
	VDAC_CPU_GX_L_M = 0,
	VDAC_CPU_TXL  = 1,
	VDAC_CPU_TXLX  = 2,
	VDAC_CPU_GXLX  = 3,
	VDAC_CPU_G12AB = 4,
	VDAC_CPU_TL1 = 5,
	VDAC_CPU_SM1 = 6,
	VDAC_CPU_TM2 = 7,
	VDAC_CPU_MAX,
};

#define VDAC_REG_MAX    0xffff

struct meson_vdac_data {
	enum vdac_cpu_type cpu_id;
	const char *name;

	unsigned int reg_cntl0;
	unsigned int reg_cntl1;
	struct meson_vdac_ctrl_s *ctrl_table;
};

struct meson_vdac_ctrl_s {
	unsigned int reg;
	unsigned int val;
	unsigned int bit;
	unsigned int len;
};

extern const struct of_device_id meson_vdac_dt_match[];
struct meson_vdac_data *aml_vdac_config_probe(struct platform_device *pdev);

#endif
