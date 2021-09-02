/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_fw_param.c
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
#include <linux/string.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>
#include "ldim_drv.h"

static struct LDReg_s nprm;
static struct FW_DAT_s fdat;

/*bl_matrix remap curve*/
static unsigned int bl_remap_curve[16] = {
	612, 654, 721, 851, 1001, 1181, 1339, 1516,
	1738, 1948, 2152, 2388, 2621, 2889, 3159, 3502
};

static unsigned int fw_ld_whist[16] = {
	32, 64, 96, 128, 160, 192, 224, 256,
	288, 320, 352, 384, 416, 448, 480, 512
};

static struct fw_ctrl_config_s ldim_fw_ctrl = {
	.fw_LD_ThSF_l = 1600,
	.fw_LD_ThTF_l = 128, /*same video sence alpha*/
	.fw_ld_thist = 0, /* 0 for default ((vnum * hnum * 5) >> 2) */
	.boost_gain = 326,//456, /*norm 256 to 1,T960 finally use*/
	.TF_alpha = 32, /*sc alpha*/
	.lpf_gain = 128,  /* [0~128~256], norm 128 as 1*/
	.boost_gain_neg = 3,
	.alpha_delta = 255,/* to fix flicker */

	.lpf_res = 14,    /* 1024/9*9 = 13,LPF_method=3 */
	.rgb_base = 127,

	.ov_gain = 16,

	.avg_gain = LD_DATA_MAX,

	.fw_rgb_diff_th = 32760,
	.max_luma = 4060,
	.lmh_avg_TH = 200,/*for woman flicker*/
	.fw_TF_sum_th = 32760,/*20180530*/

	.LPF_method = 3,
	.LD_TF_STEP_TH = 100,
	.TF_step_method = 3,
	.TF_FRESH_BL = 8,  //tf_step

	.TF_BLK_FRESH_BL = 5,
	.side_blk_diff_th = 100,
	.bbd_th = 200,
	.bbd_detect_en = 0,
	.diff_blk_luma_en = 1,

	.Sf_bypass = 0,
	.Boost_light_bypass = 1,
	.Lpf_bypass = 1,
	.Ld_remap_bypass = 0,

	.black_frm = 0,
	.black_frm_en = 0,
	.black_frm_rgbmax_th = 10,
	.black_frm_cnt_th = 120,

	.white_area_remap_en = 0,
	.white_area_th_max = 100,
	.white_area_th_min = 10,
	.white_lvl_th_max = 4095,
	.white_lvl_th_min = 2048,

	.fw_ld_blest_acmode = 1,
	.min_bl_alpha = 64,
	.glb_blend_alpha = 128,

	.sc_method = 3,
	.sc_step = 100,
};

static struct ldim_fw_para_s ldim_fw_para = {
	/* header */
	.para_ver = FW_PARA_VER,
	.para_size = sizeof(struct ldim_fw_para_s),
	.fw_ctrl_size = sizeof(struct fw_ctrl_config_s),
	.ver_str = "not installed",
	.ver_num = 0,
	.valid = 0,

	.hist_col = 1,
	.hist_row = 1,

	/* debug print flag */
	.fw_hist_print = 0,
	.fw_print_frequent = 8,
	.Dbprint_lv = 0,

	.nprm = &nprm,
	.fdat = &fdat,
	.bl_remap_curve = bl_remap_curve,
	.fw_ld_whist = fw_ld_whist,

	.ctrl = &ldim_fw_ctrl,

	.fw_alg_frm = NULL,
	.fw_alg_para_print = NULL,
};

struct ldim_fw_para_s *aml_ldim_get_fw_para(void)
{
	return &ldim_fw_para;
}
EXPORT_SYMBOL(aml_ldim_get_fw_para);
