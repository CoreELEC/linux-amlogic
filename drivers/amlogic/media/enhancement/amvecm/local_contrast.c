/*
 * drivers/amlogic/media/enhancement/amvecm/local_contrast.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/uaccess.h>
#include "arch/vpp_regs.h"
#include "local_contrast.h"

int amlc_debug;
#define pr_amlc_dbg(fmt, args...)\
	do {\
		if (amlc_debug&0x1)\
			pr_info("AMVE: " fmt, ## args);\
	} while (0)

int lc_en;
int lc_demo_mode;
int lc_en_chflg = 0xff;
static int lc_flag = 0xff;
static int *lc_szcurve;

/*local contrast begin*/
static void lc_mtx_set(enum lc_mtx_sel_e mtx_sel,
	enum lc_mtx_csc_e mtx_csc,
	int mtx_en)
{
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;

	switch (mtx_sel) {
	case INP_MTX:
		matrix_coef00_01 = SRSHARP1_LC_YUV2RGB_MAT_0_1;
		matrix_coef02_10 = SRSHARP1_LC_YUV2RGB_MAT_2_3;
		matrix_coef11_12 = SRSHARP1_LC_YUV2RGB_MAT_4_5;
		matrix_coef20_21 = SRSHARP1_LC_YUV2RGB_MAT_6_7;
		matrix_coef22 = SRSHARP1_LC_YUV2RGB_MAT_8;
		matrix_pre_offset0_1 = SRSHARP1_LC_YUV2RGB_OFST;
		matrix_clip = SRSHARP1_LC_YUV2RGB_CLIP;
		break;
	case OUTP_MTX:
		matrix_coef00_01 = SRSHARP1_LC_RGB2YUV_MAT_0_1;
		matrix_coef02_10 = SRSHARP1_LC_RGB2YUV_MAT_2_3;
		matrix_coef11_12 = SRSHARP1_LC_RGB2YUV_MAT_4_5;
		matrix_coef20_21 = SRSHARP1_LC_RGB2YUV_MAT_6_7;
		matrix_coef22 = SRSHARP1_LC_RGB2YUV_MAT_8;
		matrix_offset0_1 = SRSHARP1_LC_RGB2YUV_OFST;
		matrix_clip = SRSHARP1_LC_RGB2YUV_CLIP;
		break;
	case STAT_MTX:
		matrix_coef00_01 = LC_STTS_MATRIX_COEF00_01;
		matrix_coef02_10 = LC_STTS_MATRIX_COEF02_10;
		matrix_coef11_12 = LC_STTS_MATRIX_COEF11_12;
		matrix_coef20_21 = LC_STTS_MATRIX_COEF20_21;
		matrix_coef22 = LC_STTS_MATRIX_COEF22;
		matrix_offset0_1 = LC_STTS_MATRIX_OFFSET0_1;
		matrix_offset2 = LC_STTS_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = LC_STTS_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = LC_STTS_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = LC_STTS_CTRL0;
		break;
	default:
		break;
	}

	if (mtx_sel & STAT_MTX)
		WRITE_VPP_REG_BITS(matrix_en_ctrl, mtx_en, 2, 1);

	if (!mtx_en)
		return;

	switch (mtx_csc) {
	case LC_MTX_RGB_YUV709L:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG(matrix_coef00_01, 0x1070204);
			WRITE_VPP_REG(matrix_coef02_10, 0x640f68);
			WRITE_VPP_REG(matrix_coef11_12, 0xed601c2);
			WRITE_VPP_REG(matrix_coef20_21, 0x01c20e87);
			WRITE_VPP_REG(matrix_coef22, 0x0000fb7);
			WRITE_VPP_REG(matrix_offset0_1, 0x00400200);
		} else if (mtx_sel & STAT_MTX) {
			WRITE_VPP_REG(matrix_coef00_01, 0x00bb0275);
			WRITE_VPP_REG(matrix_coef02_10, 0x003f1f99);
			WRITE_VPP_REG(matrix_coef11_12, 0x1ea601c2);
			WRITE_VPP_REG(matrix_coef20_21, 0x01c21e67);
			WRITE_VPP_REG(matrix_coef22, 0x00001fd7);
			WRITE_VPP_REG(matrix_offset0_1, 0x00400200);
			WRITE_VPP_REG(matrix_offset2, 0x00000200);
			WRITE_VPP_REG(matrix_pre_offset0_1, 0x0);
			WRITE_VPP_REG(matrix_pre_offset2, 0x0);
		}
		break;
	case LC_MTX_YUV709L_RGB:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG(matrix_coef00_01, 0x012a0000);
			WRITE_VPP_REG(matrix_coef02_10, 0x198012a);
			WRITE_VPP_REG(matrix_coef11_12, 0xf9c0f30);
			WRITE_VPP_REG(matrix_coef20_21, 0x12a0204);
			WRITE_VPP_REG(matrix_coef22, 0x0);
			WRITE_VPP_REG(matrix_pre_offset0_1, 0x00400200);
		} else if (mtx_sel & STAT_MTX) {
			WRITE_VPP_REG(matrix_coef00_01, 0x04A80000);
			WRITE_VPP_REG(matrix_coef02_10, 0x072C04A8);
			WRITE_VPP_REG(matrix_coef11_12, 0x1F261DDD);
			WRITE_VPP_REG(matrix_coef20_21, 0x04A80876);
			WRITE_VPP_REG(matrix_coef22, 0x0);
			WRITE_VPP_REG(matrix_offset0_1, 0x0);
			WRITE_VPP_REG(matrix_offset2, 0x0);
			WRITE_VPP_REG(matrix_pre_offset0_1, 0x7c00600);
			WRITE_VPP_REG(matrix_pre_offset2, 0x00000600);
		}
		break;
	case LC_MTX_NULL:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG(matrix_coef00_01, 0x04000000);
			WRITE_VPP_REG(matrix_coef02_10, 0x0);
			WRITE_VPP_REG(matrix_coef11_12, 0x04000000);
			WRITE_VPP_REG(matrix_coef20_21, 0x0);
			WRITE_VPP_REG(matrix_coef22, 0x400);
			WRITE_VPP_REG(matrix_offset0_1, 0x0);
		} else if (mtx_sel & STAT_MTX) {

		}
		break;
	default:
		break;
	}
}

static void lc_stts_blk_config(int enable,
	unsigned int height, unsigned int width)
{
	int h_num, v_num;
	int blk_height,  blk_width;
	int row_start, col_start;
	int data32;
	int hend0, hend1, hend2, hend3, hend4, hend5, hend6;
	int hend7, hend8, hend9, hend10, hend11;
	int vend0, vend1, vend2, vend3, vend4, vend5, vend6, vend7;

	row_start = 0;
	col_start = 0;
	h_num = 12;
	v_num = 8;
	blk_height = height / v_num;
	blk_width = width / h_num;

	hend0 = col_start + blk_width - 1;
	hend1 = hend0 + blk_width;
	hend2 = hend1 + blk_width;
	hend3 = hend2 + blk_width;
	hend4 = hend3 + blk_width;
	hend5 = hend4 + blk_width;
	hend6 = hend5 + blk_width;
	hend7 = hend6 + blk_width;
	hend8 = hend7 + blk_width;
	hend9 = hend8 + blk_width;
	hend10 = hend9 + blk_width;
	hend11 = width - 1;

	vend0 = row_start + blk_height - 1;
	vend1 = vend0 + blk_height;
	vend2 = vend1 + blk_height;
	vend3 = vend2 + blk_height;
	vend4 = vend3 + blk_height;
	vend5 = vend4 + blk_height;
	vend6 = vend5 + blk_height;
	vend7 = height - 1;

	data32 = READ_VPP_REG(LC_STTS_HIST_REGION_IDX);
	WRITE_VPP_REG(LC_STTS_HIST_REGION_IDX, 0xffe0ffff & data32);
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((((row_start & 0x1fff) << 16) & 0xffff0000) |
		(col_start & 0x1fff)));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((hend1 & 0x1fff)<<16) | (hend0 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((vend1 & 0x1fff)<<16) | (vend0 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((hend3 & 0x1fff)<<16) | (hend2 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((vend3 & 0x1fff)<<16) | (vend2 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((hend5 & 0x1fff)<<16) | (hend4 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((vend5 & 0x1fff)<<16) | (vend4 & 0x1fff));

	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((hend7 & 0x1fff)<<16) | (hend6 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((vend7 & 0x1fff)<<16) | (vend6 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((hend9 & 0x1fff)<<16) | (hend8 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		((hend11 & 0x1fff)<<16) | (hend10 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION, h_num);
}

static void lc_stts_en(int enable,
	unsigned int height,
	unsigned int width,
	int pix_drop_mode,
	int eol_en,
	int hist_mode,
	int lpf_en,
	int din_sel)
{
	int data32;

	WRITE_VPP_REG(LC_STTS_GCLK_CTRL0, 0x0);
	WRITE_VPP_REG(LC_STTS_WIDTHM1_HEIGHTM1,
		((width - 1) << 16) | (height - 1));
	data32 = 0x80000000 |  ((pix_drop_mode & 0x3) << 29);
	data32 = data32 | ((eol_en & 0x1) << 28);
	data32 = data32 | ((hist_mode & 0x3) << 22);
	data32 = data32 | ((lpf_en & 0x1) << 21);
	WRITE_VPP_REG(LC_STTS_HIST_REGION_IDX, data32);

	lc_mtx_set(STAT_MTX, LC_MTX_YUV709L_RGB, enable);

	WRITE_VPP_REG_BITS(LC_STTS_CTRL0, din_sel, 3, 3);
	/*lc hist stts enable*/
	WRITE_VPP_REG_BITS(LC_STTS_HIST_REGION_IDX, enable, 31, 1);
}

static void lc_curve_ctrl_config(int enable,
	unsigned int height, unsigned int width)
{
	unsigned int lc_histvld_thrd;
	unsigned int lc_blackbar_mute_thrd;
	unsigned int lmtrat_minmax;
	int h_num, v_num;

	h_num = 12;
	v_num = 8;

	lmtrat_minmax = (READ_VPP_REG(LC_CURVE_LMT_RAT) >> 8) & 0xff;
	lc_histvld_thrd =  ((height * width) /
		(h_num * v_num) * lmtrat_minmax) >> 10;
	lc_blackbar_mute_thrd = ((height * width) / (h_num * v_num)) >> 3;

	if (!enable)
		WRITE_VPP_REG_BITS(LC_CURVE_CTRL, enable, 0, 1);
	else {
		WRITE_VPP_REG(LC_CURVE_HV_NUM, (h_num << 8) | v_num);
		WRITE_VPP_REG(LC_CURVE_HISTVLD_THRD, lc_histvld_thrd);
		WRITE_VPP_REG(LC_CURVE_BB_MUTE_THRD, lc_blackbar_mute_thrd);

		WRITE_VPP_REG_BITS(LC_CURVE_CTRL, enable, 0, 1);
		WRITE_VPP_REG_BITS(LC_CURVE_CTRL, enable, 31, 1);
	}
}


static void lc_blk_bdry_config(unsigned int height, unsigned int width)
{
	width /= 12;
	height /= 8;

	/*lc curve mapping block IDX default 4k panel*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_0_1,
		0, 16, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_0_1,
		width, 0, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_2_3,
		width * 2, 16, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_2_3,
		width * 3, 0, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_4_5,
		width * 4, 16, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_4_5,
		width * 5, 0, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_6_7,
		width * 6, 16, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_6_7,
		width * 7, 0, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_8_9,
		width * 8, 16, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_8_9,
		width * 9, 0, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_10_11,
		width * 10, 16, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_10_11,
		width * 11, 0, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_HIDX_12,
		width, 0, 14);

	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_VIDX_0_1,
		0, 16, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_VIDX_0_1,
		height, 0, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_VIDX_2_3,
		height * 2, 16, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_VIDX_2_3,
		height * 3, 0, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_VIDX_4_5,
		height * 4, 16, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_VIDX_4_5,
		height * 5, 0, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_VIDX_6_7,
		height * 6, 16, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_VIDX_6_7,
		height * 7, 0, 14);
	WRITE_VPP_REG_BITS(SRSHARP1_LC_CURVE_BLK_VIDX_8,
		height, 0, 14);
}

static void lc_top_config(int enable, int h_num, int v_num,
	unsigned int height, unsigned int width)
{
	/*lcinput_ysel*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_INPUT_MUX, 5, 4, 3);
	/*lcinput_csel*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_INPUT_MUX, 5, 0, 3);

	/*lc ram write h num*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_HV_NUM, h_num, 8, 5);
	/*lc ram write v num*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_HV_NUM, v_num, 0, 5);

	/*lc hblank*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 8, 8, 8);
	/*lc blend mode,default 1*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 1, 0, 1);
	/*lc curve mapping  config*/
	lc_blk_bdry_config(height, width);
	/*LC sync ctl*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 0, 16, 1);
	/*lc enable need set at last*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, enable, 4, 1);
	lc_mtx_set(INP_MTX, LC_MTX_YUV709L_RGB, 1);
	lc_mtx_set(OUTP_MTX, LC_MTX_RGB_YUV709L, 1);
}

static void lc_disable(void)
{
	/*lc enable need set at last*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 0, 4, 1);
	WRITE_VPP_REG_BITS(LC_CURVE_CTRL, 0, 0, 1);
	WRITE_VPP_REG_BITS(LC_CURVE_RAM_CTRL, 0, 0, 1);
	/*lc hist stts enable*/
	WRITE_VPP_REG_BITS(LC_STTS_HIST_REGION_IDX, 0, 31, 1);
	lc_en_chflg = 0x0;
}

static void lc_config(int enable,
	struct vframe_s *vf,
	unsigned int sps_h_en,
	unsigned int sps_v_en)
{
	int h_num, v_num;
	unsigned int height, width;
	static unsigned int vf_height, vf_width;
	const struct vinfo_s *vinfo = get_current_vinfo();

	height = vinfo->height;
	width = vinfo->width;
	h_num = 12;
	v_num = 8;

	if (vf == NULL) {
		vf_height = 0;
		vf_width = 0;
		return;
	}

	if ((vf_height == vf->height) &&
		(vf_width == vf->width)) {
		if (!lc_en_chflg)
			lc_en_chflg = 0xff;
		else
			return;
	}

	vf_height = vf->height;
	vf_width = vf->width;

	lc_top_config(enable, h_num, v_num, height, width);

	if (sps_h_en == 1)
		width /= 2;
	if (sps_v_en == 1)
		height /= 2;

	lc_curve_ctrl_config(enable, height, width);
	lc_stts_blk_config(enable, height, width);
	lc_stts_en(enable, height, width, 0, 0, 1, 1, 4);
}

static void read_lc_curve(void)
{
	int blk_hnum;
	int blk_vnum;
	int i, j;
	unsigned int dwTemp;
	unsigned int temp1, temp2;

	if (!lc_szcurve) {
		pr_amlc_dbg("%s: lc_szcurve not init!", __func__);
		return;
	}
	dwTemp = READ_VPP_REG(LC_CURVE_HV_NUM);
	blk_hnum = (dwTemp >> 8) & 0x1f;
	blk_vnum = (dwTemp) & 0x1f;
	WRITE_VPP_REG(LC_CURVE_RAM_CTRL, 1);
	WRITE_VPP_REG(LC_CURVE_RAM_ADDR, 0);
	for (i = 0; i < blk_vnum; i++) {
		for (j = 0; j < blk_hnum; j++) {
			temp1 = READ_VPP_REG(LC_CURVE_RAM_DATA);
			temp2 = READ_VPP_REG(LC_CURVE_RAM_DATA);
			lc_szcurve[(i*blk_hnum + j)*6+0] =
				temp1 & 0x3ff;/*bit0:9*/
			lc_szcurve[(i*blk_hnum + j)*6+1] =
				(temp1>>10) & 0x3ff;/*bit10:19*/
			lc_szcurve[(i*blk_hnum + j)*6+2] =
				(temp1>>20) & 0x3ff;/*bit20:29*/
			lc_szcurve[(i*blk_hnum + j)*6+3] =
				temp2 & 0x3ff;/*bit0:9*/
			lc_szcurve[(i*blk_hnum + j)*6+4] =
				(temp2>>10) & 0x3ff;/*bit10:19*/
			lc_szcurve[(i*blk_hnum + j)*6+5] =
				(temp2>>20) & 0x3ff;/*bit20:29*/
		}
	}
	WRITE_VPP_REG(LC_CURVE_RAM_CTRL, 0);
}

static void lc_demo_wr_curve(int h_num, int v_num)
{
	int i, j, temp1, temp2;

	for (i = 0; i < v_num; i++) {
		for (j = 0; j < h_num / 2; j++) {
			temp1 = lc_szcurve[6 * (i * h_num + j) + 0] |
				(lc_szcurve[6 * (i * h_num + j) + 1] << 10) |
				(lc_szcurve[6 * (i * h_num + j) + 2] << 20);
			temp2 = lc_szcurve[6 * (i * h_num + j) + 3] |
				(lc_szcurve[6 * (i * h_num + j) + 4] << 10) |
				(lc_szcurve[6 * (i * h_num + j) + 5] << 20);
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA, temp1);
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA, temp2);
		}
		for (j = h_num / 2; j < h_num; j++) {
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
				(0|(0<<10)|(512<<20)));
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
				(1023|(1023<<10)|(512<<20)));
		}
	}
}

static int lc_demo_check_curve(int h_num, int v_num)
{
	int i, j, temp, temp1, flag;

	flag = 0;
	for (i = 0; i < v_num; i++) {
		for (j = 0; j < h_num / 2; j++) {
			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			temp1 = lc_szcurve[6 * (i * h_num + j) + 0] |
				(lc_szcurve[6 * (i * h_num + j) + 1] << 10) |
				(lc_szcurve[6 * (i * h_num + j) + 2] << 20);
			if (temp != temp1)
				flag = (2 * (i * h_num + j) + 0) | (1 << 31);

			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			temp1 = lc_szcurve[6 * (i * h_num + j) + 3] |
				(lc_szcurve[6 * (i * h_num + j) + 4] << 10) |
				(lc_szcurve[6 * (i * h_num + j) + 5] << 20);
			if (temp != temp1)
				flag = (2 * (i * h_num + j) + 1) | (1 << 31);
		}
		for (j = h_num / 2; j < h_num; j++) {
			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			if (temp != (0|(0<<10)|(512<<20)))
				flag = (2 * (i * h_num + j) + 0) | (1 << 31);
			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			if (temp != (1023|(1023<<10)|(512<<20)))
				flag = (2 * (i * h_num + j) + 1) | (1 << 31);
		}
	}
	return flag;
}
static int set_lc_curve(int binit, int bcheck)
{
	int i, h_num, v_num;
	unsigned int hvTemp;
	int rflag;
	int temp, temp1;

	if (!lc_szcurve) {
		pr_amlc_dbg("%s: lc_szcurve not init!", __func__);
		return -EINVAL;
	}
	rflag = 0;
	hvTemp = READ_VPP_REG(SRSHARP1_LC_HV_NUM);
	h_num = (hvTemp >> 8) & 0x1f;
	v_num = hvTemp & 0x1f;
	WRITE_VPP_REG_BITS(SRSHARP1_LC_MAP_RAM_CTRL, 1, 0, 1);
	/*data sequence: ymin/minBv/pkBv/maxBv/ymaxv/ypkBv*/
	if (binit) {
		WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_ADDR, 0);
		for (i = 0; i < h_num * v_num; i++) {
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
				(0|(0<<10)|(512<<20)));
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
				(1023|(1023<<10)|(512<<20)));
		}
	} else {
		WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_ADDR, 0);
		if (lc_demo_mode)
			lc_demo_wr_curve(h_num, v_num);
		else
			for (i = 0; i < h_num * v_num; i++) {
				WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
					lc_szcurve[6 * i + 0]|
					(lc_szcurve[6 * i + 1]<<10)|
					(lc_szcurve[6 * i + 2]<<20));
				WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
					lc_szcurve[6 * i + 3]|
					(lc_szcurve[6 * i + 4]<<10)|
					(lc_szcurve[6 * i + 5]<<20));
			}
	}
	WRITE_VPP_REG_BITS(SRSHARP1_LC_MAP_RAM_CTRL, 0, 0, 1);

	if (bcheck) {
		WRITE_VPP_REG_BITS(SRSHARP1_LC_MAP_RAM_CTRL, 1, 0, 1);
		WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_ADDR, 0 | (1 << 31));
		if (lc_demo_mode)
			rflag = lc_demo_check_curve(h_num, v_num);
		else
			for (i = 0; i < h_num * v_num; i++) {
				temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
				temp1 = lc_szcurve[6 * i + 0]|
					(lc_szcurve[6 * i + 1]<<10)|
					(lc_szcurve[6 * i + 2]<<20);
				if (temp != temp1)
					rflag = (2 * i + 0) | (1 << 31);
				temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
				temp1 = lc_szcurve[6 * i + 3]|
					(lc_szcurve[6 * i + 4]<<10)|
					(lc_szcurve[6 * i + 5]<<20);
				if (temp != temp1)
					rflag = (2 * i + 1) | (1 << 31);
			}
		WRITE_VPP_REG_BITS(SRSHARP1_LC_MAP_RAM_CTRL, 0, 0, 1);
	}

	return rflag;
}

static void lc_fw_curve_iir(struct vframe_s *vf)
{
	if (!vf)
		return;
}

/*print statistics hist and curve, vlsi suggest add*/
unsigned int lc_hist_vs = 4;
unsigned int lc_hist_ve = 6;
unsigned int lc_hist_hs;
unsigned int lc_hist_he = 11;
unsigned int lc_hist_prcnt;
unsigned int lc_curve_prcnt;
static int *lc_hist;

static void lc_read_region(void)
{
	int i, j, k;
	int data32;
	int rgb_min, rgb_max;
	int dwTemp, blk_hnum, blk_vnum;

	if (!lc_hist) {
		pr_amlc_dbg("%s: lc_hist not init!", __func__);
		return;
	}
	WRITE_VPP_REG_BITS(0x4037, 1, 14, 1);
	dwTemp = READ_VPP_REG(LC_CURVE_HV_NUM);
	blk_hnum = (dwTemp >> 8) & 0x1f;
	blk_vnum = (dwTemp) & 0x1f;
	data32 = READ_VPP_REG(LC_STTS_HIST_START_RD_REGION);

	for (i = 0; i < blk_vnum; i++)
		for (j = 0; j < blk_hnum; j++) {
			data32 = READ_VPP_REG(LC_STTS_HIST_START_RD_REGION);
			if ((i >= lc_hist_vs) && (i <= lc_hist_ve) &&
				(j >= lc_hist_hs) && (j <= lc_hist_he) &&
				(amlc_debug == 0x4))
				pr_info("========[r,c](%2d,%2d)======\n", i, j);

			for (k = 0; k < 17; k++) {
				data32 = READ_VPP_REG(LC_STTS_HIST_READ_REGION);
				lc_hist[(i*blk_hnum+j)*17 + k] = data32;
				if ((i >= lc_hist_vs) && (i <= lc_hist_ve) &&
					(j >= lc_hist_hs) && (j <= lc_hist_he)
					&& (amlc_debug == 0x4)) {
					/*print chosen hist*/
					if (k == 16) {/*last bin*/
						rgb_min =
							(data32 >> 10) & 0x3ff;
						rgb_max = (data32) & 0x3ff;
						pr_info("[%2d]:%d,%d\n",
							k, rgb_min, rgb_max);
					} else
						pr_info("[%2d]:%d\n",
							k, data32);
				}
			}
		}

	if (amlc_debug == 0x2)/*print all hist data*/
		for (i = 0; i < 8*12*17; i++)
			pr_info("%x\n", lc_hist[i]);
}

static void lc_prt_curve(void)
{/*print curve node*/
	int i, j;
	int dwTemp, blk_hnum, blk_vnum;

	dwTemp = READ_VPP_REG(LC_CURVE_HV_NUM);
	blk_hnum = (dwTemp >> 8) & 0x1f;
	blk_vnum = (dwTemp) & 0x1f;
	pr_amlc_dbg("======lc_prt curve node=======\n");
	for (i = 0; i < blk_hnum*blk_vnum; i++) {
		for (j = 0; j < 6 ; j++)
			pr_amlc_dbg("%d\n", lc_szcurve[i*6 + j]);
		pr_amlc_dbg("\n");
	}
}

void lc_init(void)
{
	int h_num, v_num;
	unsigned int height, width;
	const struct vinfo_s *vinfo = get_current_vinfo();

	height = vinfo->height;
	width = vinfo->width;
	h_num = 12;
	v_num = 8;

	if (!lc_en)
		return;
	lc_szcurve = kzalloc(580 * sizeof(int), GFP_KERNEL);
	if (!lc_szcurve)
		return;
	lc_hist = kzalloc(1632 * sizeof(int), GFP_KERNEL);
	if (!lc_hist) {
		kfree(lc_szcurve);
		return;
	}

	lc_top_config(0, h_num, v_num, height, width);
	lc_mtx_set(INP_MTX, LC_MTX_YUV709L_RGB, 1);
	lc_mtx_set(OUTP_MTX, LC_MTX_RGB_YUV709L, 1);
	WRITE_VPP_REG_BITS(LC_CURVE_RAM_CTRL, 0, 0, 1);

	if (set_lc_curve(1, 0))
		pr_amlc_dbg("%s: init fail", __func__);
}

void lc_process(struct vframe_s *vf,
	unsigned int sps_h_en,
	unsigned int sps_v_en)
{
	if (get_cpu_type() < MESON_CPU_MAJOR_ID_TL1)
		return;

	if (!lc_en) {
		lc_disable();
		return;
	}

	if (vf == NULL) {
		if (lc_flag == 0xff) {
			lc_disable();
			lc_flag = 0x0;
		}
		return;
	}

	lc_config(lc_en, vf, sps_h_en, sps_v_en);

	read_lc_curve();
	lc_fw_curve_iir(vf);

	if (lc_curve_prcnt > 0) { /*debug lc curve node*/
		lc_prt_curve();
		lc_curve_prcnt--;
	}
	if (lc_hist_prcnt > 0) { /*debug hist*/
		lc_read_region();
		lc_hist_prcnt--;
	}
	if (set_lc_curve(0, 1))
		pr_amlc_dbg("%s: set lc curve fail", __func__);

	lc_flag = 0xff;
}

void lc_free(void)
{
	kfree(lc_szcurve);
	kfree(lc_hist);
}
