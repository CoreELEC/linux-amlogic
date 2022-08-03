// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "vpp_module_vadj.h"

enum _vadj_mode_e {
	EN_MODE_VADJ_01 = 0,
	EN_MODE_VADJ_02,
};

struct _vadj_bit_cfg_s {
	struct _bit_s bit_vadj1_ctrl_en;
	struct _bit_s bit_vadj2_ctrl_en;
	struct _bit_s bit_vadj_brightness;
	struct _bit_s bit_vadj_contrast;
};

struct _vadj_reg_cfg_s {
	unsigned char page;
	unsigned char reg_vadj1_ctrl;
	unsigned char reg_vadj1_y;
	unsigned char reg_vadj1_ma_mb;
	unsigned char reg_vadj1_mc_md;
	unsigned char reg_vadj2_ctrl;
	unsigned char reg_vadj2_y;
	unsigned char reg_vadj2_ma_mb;
	unsigned char reg_vadj2_mc_md;
};

/*Default table from T3*/
static struct _vadj_reg_cfg_s vadj_reg_cfg = {
	0x32,
	0x80,
	0x82,
	0x83,
	0x84,
	0xa0,
	0xa2,
	0xa3,
	0xa4
};

static struct _vadj_bit_cfg_s vadj_bit_cfg = {
	{0, 1},
	{0, 1},
	{8, 11},
	{0, 8}
};

/*For ai pq*/
static bool vadj_ai_pq_update;
static struct vadj_ai_pq_param_s vadj_ai_pq_offset;
static struct vadj_ai_pq_param_s vadj_ai_pq_base;

/*Internal functions*/
static int _set_vadj_ctrl(enum _vadj_mode_e mode, int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	switch (mode) {
	case EN_MODE_VADJ_01:
		addr = ADDR_PARAM(vadj_reg_cfg.page, vadj_reg_cfg.reg_vadj1_ctrl);
		break;
	case EN_MODE_VADJ_02:
		addr = ADDR_PARAM(vadj_reg_cfg.page, vadj_reg_cfg.reg_vadj2_ctrl);
		break;
	default:
		break;
	}

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static int _set_vadj_y(enum _vadj_mode_e mode, int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	switch (mode) {
	case EN_MODE_VADJ_01:
		addr = ADDR_PARAM(vadj_reg_cfg.page, vadj_reg_cfg.reg_vadj1_y);
		break;
	case EN_MODE_VADJ_02:
		addr = ADDR_PARAM(vadj_reg_cfg.page, vadj_reg_cfg.reg_vadj2_y);
		break;
	default:
		break;
	}

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);

	return 0;
}

static int _set_vadj_ma_mb(enum _vadj_mode_e mode, int val)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	switch (mode) {
	case EN_MODE_VADJ_01:
		addr = ADDR_PARAM(vadj_reg_cfg.page, vadj_reg_cfg.reg_vadj1_ma_mb);
		break;
	case EN_MODE_VADJ_02:
		addr = ADDR_PARAM(vadj_reg_cfg.page, vadj_reg_cfg.reg_vadj2_ma_mb);
		break;
	default:
		break;
	}

	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	return 0;
}

static int _set_vadj_mc_md(enum _vadj_mode_e mode, int val)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	switch (mode) {
	case EN_MODE_VADJ_01:
		addr = ADDR_PARAM(vadj_reg_cfg.page, vadj_reg_cfg.reg_vadj1_mc_md);
		break;
	case EN_MODE_VADJ_02:
		addr = ADDR_PARAM(vadj_reg_cfg.page, vadj_reg_cfg.reg_vadj2_mc_md);
		break;
	default:
		break;
	}

	WRITE_VPP_REG_BY_MODE(io_mode, addr, val);

	return 0;
}

/*External functions*/
int vpp_module_vadj_init(struct vpp_dev_s *pdev)
{
	enum vpp_chip_type_e chip_id;

	chip_id = pdev->pm_data->chip_id;

	if (chip_id < CHIP_G12A) {
		vadj_reg_cfg.page            = 0x1d;
		vadj_reg_cfg.reg_vadj1_ctrl  = 0x40;
		vadj_reg_cfg.reg_vadj1_y     = 0x41;
		vadj_reg_cfg.reg_vadj1_ma_mb = 0x42;
		vadj_reg_cfg.reg_vadj1_mc_md = 0x43;
		vadj_reg_cfg.reg_vadj2_ctrl  = 0x40;
		vadj_reg_cfg.reg_vadj2_y     = 0x44;
		vadj_reg_cfg.reg_vadj2_ma_mb = 0x45;
		vadj_reg_cfg.reg_vadj2_mc_md = 0x46;
		vadj_bit_cfg.bit_vadj1_ctrl_en.start = 1;
		vadj_bit_cfg.bit_vadj1_ctrl_en.len   = 1;
		vadj_bit_cfg.bit_vadj2_ctrl_en.start = 2;
		vadj_bit_cfg.bit_vadj2_ctrl_en.len   = 1;
		vadj_bit_cfg.bit_vadj_brightness.start = 8;
		vadj_bit_cfg.bit_vadj_brightness.len   = 10;
		vadj_bit_cfg.bit_vadj_contrast.start = 0;
		vadj_bit_cfg.bit_vadj_contrast.len   = 8;
	}

	vadj_ai_pq_update = false;
	memset(&vadj_ai_pq_offset, 0, sizeof(vadj_ai_pq_offset));
	memset(&vadj_ai_pq_base, 0, sizeof(vadj_ai_pq_base));

	return 0;
}

int vpp_module_vadj_en(bool enable)
{
	return _set_vadj_ctrl(EN_MODE_VADJ_01, enable,
		vadj_bit_cfg.bit_vadj1_ctrl_en.start, vadj_bit_cfg.bit_vadj1_ctrl_en.len);
}

int vpp_module_vadj_post_en(bool enable)
{
	return _set_vadj_ctrl(EN_MODE_VADJ_02, enable,
		vadj_bit_cfg.bit_vadj2_ctrl_en.start, vadj_bit_cfg.bit_vadj2_ctrl_en.len);
}

int vpp_module_vadj_set_brightness(int val)
{
	return _set_vadj_y(EN_MODE_VADJ_01, val,
		vadj_bit_cfg.bit_vadj_brightness.start, vadj_bit_cfg.bit_vadj_brightness.len);
}

int vpp_module_vadj_set_brightness_post(int val)
{
	return _set_vadj_y(EN_MODE_VADJ_02, val,
		vadj_bit_cfg.bit_vadj_brightness.start, vadj_bit_cfg.bit_vadj_brightness.len);
}

int vpp_module_vadj_set_contrast(int val)
{
	return _set_vadj_y(EN_MODE_VADJ_01, val,
		vadj_bit_cfg.bit_vadj_contrast.start, vadj_bit_cfg.bit_vadj_contrast.len);
}

int vpp_module_vadj_set_contrast_post(int val)
{
	return _set_vadj_y(EN_MODE_VADJ_02, val,
		vadj_bit_cfg.bit_vadj_contrast.start, vadj_bit_cfg.bit_vadj_contrast.len);
}

int vpp_module_vadj_set_sat_hue(int val)
{
	short mc = 0;
	short md = 0;
	int mab = 0;
	int mcd = 0;

	vadj_ai_pq_base.sat_hue_mad = val;

	mab = (val + vadj_ai_pq_offset.sat_hue_mad) & 0x03ff03ff;

	mc = (0 - (short)(val & 0x000003ff));    /* mc = -mb */
	mc = vpp_check_range(mc, (-512), 511);
	md = (short)((val & 0x03ff0000) >> 16);  /* md =  ma; */
	mcd = ((mc & 0x03ff) << 16) | (md & 0x03ff);

	_set_vadj_ma_mb(EN_MODE_VADJ_01, mab);
	_set_vadj_mc_md(EN_MODE_VADJ_01, mcd);

	return 0;
}

int vpp_module_vadj_set_sat_hue_post(int val)
{
	short mc = 0;
	short md = 0;
	int mab = 0;
	int mcd = 0;

	mab = val & 0x03ff03ff;

	mc = (0 - (short)(val & 0x000003ff));    /* mc = -mb */
	mc = vpp_check_range(mc, (-512), 511);
	md = (short)((val & 0x03ff0000) >> 16);  /* md =  ma; */
	mcd = ((mc & 0x03ff) << 16) | (md & 0x03ff);

	_set_vadj_ma_mb(EN_MODE_VADJ_02, mab);
	_set_vadj_mc_md(EN_MODE_VADJ_02, mcd);

	return 0;
}

void vpp_module_vadj_on_vs(void)
{
	if (vadj_ai_pq_update) {
		vpp_module_vadj_set_sat_hue(vadj_ai_pq_base.sat_hue_mad);
		vadj_ai_pq_update = false;
	}
}

/*For ai pq*/
void vpp_module_vadj_get_ai_pq_base(struct vadj_ai_pq_param_s *pparam)
{
	if (!pparam)
		return;

	pparam->sat_hue_mad = vadj_ai_pq_base.sat_hue_mad;
}

void vpp_module_vadj_set_ai_pq_offset(struct vadj_ai_pq_param_s *pparam)
{
	if (!pparam)
		return;

	if (vadj_ai_pq_offset.sat_hue_mad != pparam->sat_hue_mad) {
		vadj_ai_pq_offset.sat_hue_mad = pparam->sat_hue_mad;
		vadj_ai_pq_update = true;
	}
}

