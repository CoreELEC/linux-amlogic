// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include "ldim_drv.h"
#include "ldim_reg.h"

static int ld_blk_hidx[33] = {
	/* S14* 33	*/
	-1920, -1440, -960, -480, 0, 480,
	960, 1440, 1920, 2400, 2880, 3360,
	3840, 4320, 4800, 5280, 5760, 6240,
	6720, 7200, 7680, 8160, 8191, 8191,
	8191, 8191, 8191, 8191, 8191, 8191,
	8191, 8191, 8191
};

static int ld_blk_vidx[25] = {
	/*  S14* 25 */
	-8192, -8192, -8192, -4320, 0, 4320,
	8191, 8191, 8191, 8191, 8191, 8191,
	8191, 8191, 8191, 8191, 8191, 8191,
	8191, 8191, 8191, 8191, 8191, 8191, 8191
};

static int ld_lut_hdg1[32] = {
	 /*	u10	*/
	503, 501, 494, 481, 465, 447, 430, 409, 388, 369, 354,
	343, 334, 326, 318, 311, 305, 299, 293, 286, 279, 272,
	266, 261, 257, 252, 245, 235, 226, 218, 214, 213
};

static int ld_lut_vdg1[32] = {
	/*	u10	*/
	373, 371, 367, 364, 359, 353, 346, 337, 328, 318, 308,
	297, 286, 274, 261, 247, 232, 218, 204, 191, 180, 169,
	158, 148, 138, 130, 122, 115, 108, 104, 100, 97
};

static int ld_lut_vhk1[32] = {
	 /*	u10	*/
	492, 492, 492, 492, 427, 356, 328, 298, 272, 251, 229,
	206, 191, 175, 162, 151, 144, 139, 131, 127, 119, 110,
	105, 101, 99, 98, 94, 85, 83, 77, 74, 73
};

static int ld_lut_vho_pos[] = {
	104, 75, 50, 20, -15, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25
};

static int ld_lut_vho_neg[] = {
	104, 75, 50, 20, -15, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
	-25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25
};

static int ld_lut_hdg1_txlx[32] = {
	455, 487, 498, 505, 506, 509, 503, 494,
	493, 483, 484, 480, 478, 476, 472, 472,
	468, 465, 459, 449, 448, 439, 436, 432,
	430, 413, 402, 386, 361, 343, 317, 307
};

static int ld_lut_vdg1_txlx[32] = {
	485, 483, 474, 465, 451, 435, 406, 381,
	350, 320, 283, 251, 211, 178, 147, 113,
	88, 65, 52, 37, 27, 20, 16, 8,
	3, 2, 0, 0, 0, 0, 0, 0
};

static int ld_lut_vhk1_txlx[32] = {
	490, 410, 356, 317, 288, 272, 266, 260,
	258, 253, 249, 246, 242, 240, 236, 236,
	232, 229, 226, 224, 224, 222, 221, 221,
	221, 219, 219, 221, 219, 225, 228, 237
};

static int reg_ld_lut_hdg_txlx[8][32] = {
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
};

static int reg_ld_lut_vdg_txlx[8][32] = {
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
	{254, 248, 239, 226, 211, 194, 176, 156,
		137, 119, 101, 85, 70, 57, 45, 36,
		28, 21, 16, 12, 9, 6, 4, 3,
		2, 1, 1, 1, 0, 0, 0, 0},
};

static int reg_ld_lut_vhk_txlx[8][32] = {
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
	{128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128},
};

static int ld_lut_vho_pos_txlx[32] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int ld_lut_vho_neg_txlx[32] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int ld_lut_hdg_lext_txlx[8] = {
	260, 260, 260, 260, 260, 260, 260, 260
};

static int ld_lut_vdg_lext_txlx[8] = {
	260, 260, 260, 260, 260, 260, 260, 260
};

static int ld_lut_vhk_lext_txlx[8] = {
	128, 128, 128, 128, 128, 128, 128, 128
};

/*	public function	*/
int ldim_round(int ix, int ib)
{
	int ld_rst = 0;

	if (ix == 0)
		ld_rst = 0;
	else if (ix > 0)
		ld_rst = (ix + ib / 2) / ib;
	else
		ld_rst = (ix - ib / 2) / ib;

	return ld_rst;
}

void ld_func_init_data(int pdata[], int size, int val)
{
	int i;

	for (i = 0; i < size; i++)
		pdata[i] = val;
}

int ld_func_get_blmtx_avg(int pmtx[], int size, int mode)
{
	int i;
	int da;

	if (mode == 0) {
		da = 0;
	} else if (mode == 1) {
		da = 0;
		for (i = 0; i < size; i++)
			da += pmtx[i];
		da = da / size;
	} else if (mode == 2) {
		da = pmtx[0];
		for (i = 1; i < size; i++) {
			if (da > pmtx[i])
				da = pmtx[i];
		}
	} else {
		da = pmtx[0];
		for (i = 1; i < size; i++) {
			if (da < pmtx[i])
				da = pmtx[i];
		}
	}
	return da;
}

void ld_func_mtx_inv(int *odat, int *idat, int nrow, int ncol)
{
	int nt1 = 0;
	int nt2 = 0;
	int ny = 0;
	int nx = 0;

	for (ny = 0; ny < nrow; ny++) {
		for (nx = 0; nx < ncol; nx++) {
			nt1 = ny * ncol + nx;
			nt2 = nx * nrow + ny;
			odat[nt2] = idat[nt1];
		}
	}
}

int ld_remap_lut[16][32] = {
	{
		128, 258, 387, 517, 646, 776, 905, 1034,
		1163, 1291, 1420, 1548, 1676, 1804, 1932, 2059,
		2187, 2314, 2441, 2569, 2696, 2823, 2950, 3077,
		3204, 3331, 3458, 3586, 3713, 3840, 3968, 4095,
	},
	{
		114,  230,  345,  461,  577,  693,  808,  923,
		1038, 1153, 1267, 1382, 1496, 1610, 1724, 1844,
		1987, 2130, 2272, 2415, 2557, 2700, 2842, 2985,
		3127, 3270, 3412, 3555, 3697, 3840, 3981, 4095,
	},
	{
		114,  230,  345,  461,  577,  692,  808,  923,
		1038, 1152, 1267, 1381, 1496, 1610, 1731, 1874,
		2017, 2160, 2303, 2445, 2588, 2731, 2873, 3015,
		3158, 3300, 3443, 3585, 3728, 3868, 3981, 4095,
	},
	{
		115,  230,  346,  462,  579,  694,  810,  926,
		1041, 1155, 1270, 1384, 1499, 1637, 1781, 1923,
		2066, 2209, 2351, 2494, 2636, 2778, 2921, 3063,
		3205, 3347, 3490, 3632, 3754, 3868, 3982, 4095,
	},
	{
		119,  243,  367,  493,  620,  744,  867,  988,
		1106, 1224, 1340, 1456, 1596, 1739, 1881, 2021,
		2161, 2300, 2438, 2576, 2713, 2850, 2987, 3124,
		3261, 3399, 3536, 3652, 3761, 3871, 3982, 4095,
	},
	{
		116,  240,  364,  490,  619,  742,  863,  982,
		1096, 1208, 1319, 1469, 1620, 1769, 1917, 2063,
		2208, 2352, 2494, 2636, 2777, 2918, 3059, 3200,
		3341, 3482, 3589, 3687, 3786, 3887, 3990, 4095,
	},
	{
		114,  237,  361,  487,  615,  737,  857,  973,
		1085, 1221, 1380, 1536, 1691, 1844, 1994, 2144,
		2291, 2438, 2583, 2727, 2870, 3014, 3157, 3300,
		3424, 3518, 3610, 3704, 3798, 3894, 3994, 4095,
	},
	{
		113,  238,  363,  490,  620,  744,  863,  978,
		1091, 1256, 1419, 1579, 1737, 1893, 2046, 2198,
		2348, 2496, 2642, 2788, 2933, 3077, 3222, 3366,
		3459, 3547, 3633, 3722, 3811, 3903, 3997, 4095,
	},
	{
		120,  255,  390,  528,  669,  801,  928, 1050,
		1194, 1356, 1515, 1671, 1825, 1975, 2123, 2268,
		2412, 2553, 2693, 2832, 2970, 3107, 3244, 3361,
		3448, 3536, 3623, 3713, 3804, 3897, 3994, 4095,
	},
	{
		134,  292,  449,  611,  775,  924, 1063, 1193,
		1340, 1503, 1661, 1814, 1963, 2108, 2249, 2387,
		2522, 2654, 2785, 2914, 3042, 3169, 3297, 3405,
		3484, 3565, 3644, 3727, 3813, 3901, 3996, 4095,
	},
	{
		160,  358,  550,  746,  943, 1113, 1266, 1406,
		1552, 1708, 1858, 2000, 2138, 2269, 2397, 2520,
		2640, 2757, 2872, 2985, 3098, 3211, 3325, 3422,
		3496, 3572, 3647, 3727, 3811, 3898, 3993, 4095,
	},
	{
		201,  456,  693,  929, 1162, 1354, 1520, 1671,
		1822, 1963, 2095, 2220, 2339, 2451, 2559, 2662,
		2762, 2859, 2955, 3049, 3143, 3239, 3335, 3407,
		3479, 3556, 3630, 3712, 3797, 3887, 3987, 4095,
	},
	{
		243,  546,  815, 1079, 1334, 1536, 1707, 1857,
		1996, 2125, 2245, 2357, 2462, 2561, 2655, 2746,
		2833, 2917, 3000, 3083, 3166, 3250, 3336, 3405,
		3476, 3551, 3624, 3706, 3791, 3882, 3984, 4095,
	},
	{
		280,  622,  914, 1195, 1465, 1672, 1844, 1993,
		2122, 2240, 2349, 2450, 2545, 2634, 2718, 2798,
		2875, 2950, 3023, 3097, 3171, 3247, 3325, 3393,
		3464, 3540, 3614, 3697, 3784, 3876, 3981, 4095,
	},
	{
		308,  675,  979, 1268, 1544, 1752, 1922, 2067,
		2191, 2304, 2408, 2505, 2595, 2679, 2758, 2834,
		2907, 2978, 3048, 3118, 3189, 3262, 3338, 3404,
		3473, 3546, 3618, 3700, 3785, 3877, 3981, 4095,
	},
	{
		342,  738, 1058, 1360, 1648, 1862, 2037, 2184,
		2295, 2396, 2488, 2573, 2651, 2724, 2792, 2857,
		2919, 2979, 3038, 3098, 3158, 3221, 3287, 3357,
		3431, 3510, 3588, 3674, 3766, 3864, 3975, 4095,
	},
};

static int remap_lut2[16][16] = {};
static void ld_func_lut_init(struct ld_reg_s *reg)
{
	int i, j, k, t;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);

	switch (bdrv->data->chip_type) {
	case LCD_CHIP_TM2:
	case LCD_CHIP_TM2B:
		for (i = 0; i < 16; i++) {
			for (j = 0; j < 16; j++)
				remap_lut2[i][j] = ld_remap_lut[i][j * 2] |
					(ld_remap_lut[i][j * 2 + 1] << 16);
		}
		/* Emulate the FW to set the LUTs */
		for (k = 0; k < 16; k++) {
			/*set the LUT to be inverse of the Lit_value,*/
			/* lit_idx distribute equal space, set by FW */
			reg->x_idx[0][k] = 4095 - 256 * k;
			reg->x_nrm[0][k] = 8;
			for (t = 0; t < 16; t++) {
				reg->x_lut2[0][k][t] = remap_lut2[k][t];
				reg->x_lut2[1][k][t] = remap_lut2[k][t];
				reg->x_lut2[2][k][t] = remap_lut2[k][t];
			}
		}
		break;
	default:
		break;
	}
}

static void ld_func_cfg_ldreg_tm2(struct ld_reg_s *reg)
{
	int i, j;
	unsigned int T = 0;
	unsigned int vnum = 0;
	unsigned int hnum = 0;
	unsigned int BSIZE = 0;

	/* General registers; */
	reg->reg_ld_pic_row_max = 2160;/* setting default */
	reg->reg_ld_pic_col_max = 3840;
	ld_func_init_data(reg->reg_ld_pic_yuv_sum, 3, 0);
	/* only output u16*3, (internal ACC will be u32x3)*/
	ld_func_init_data(reg->reg_ld_pic_rgb_sum, 3, 0);

	/* set same region division for statistics */
	reg->reg_ld_sta_vnum  = 1;
	reg->reg_ld_sta_hnum  = 8;

	/*Image Statistic options */
	reg->reg_ld_blk_vnum = 1;/*u5: Maximum to BLKVMAX */
	reg->reg_ld_blk_hnum  = 8;/*u5: Maximum to BLKHMAX */

	reg->reg_ld_sta1max_lpf = 1;
	/*u1: STA1max statistics on [1 2 1]/4 filtered results */
	reg->reg_ld_sta2max_lpf = 1;
	/*u1: STA2max statistics on [1 2 1]/4 filtered results*/
	reg->reg_ld_sta_hist_lpf  = 1;
	/*u1: STAhist statistics on [1 2 1]/4 filtered results*/
	reg->reg_ld_sta1max_hdlt = 0;
	/*u2: (2^x) extra pixels into Max calculation*/
	reg->reg_ld_sta1max_vdlt = 0;
	/*u4: extra pixels into Max calculation vertically*/
	reg->reg_ld_sta2max_hdlt = 0;
	/*u2: (2^x) extra pixels into Max calculation*/
	reg->reg_ld_sta2max_vdlt = 0;
	/*u4: extra pixels into Max calculation vertically*/
	reg->reg_ld_sta_hist_mode = 3;
	/* u3: histogram statistics on XX separately 20bits*16bins:
	 *	0:R-only,1:G-only 2:B-only 3:Y-only; 4:MAX(R,G,B),5/6/7:R&G&B
	 */

	/******	FBC3 fw_hw_alg_frm	*******/
	reg->reg_ldfw_blest_acmode = 0;
	/* u3: 0: est on BLmatrix; 1: est on (BL-DC);
	 *	2: est on (BL-MIN); 3: est on (BL-MAX) 4: 2048; 5:1024
	 */

	reg->reg_ldfw_blk_norm = 128;
	/* u8: normalization gain for blk number,
	 *	1/blk_num= norm>>(rs+8), norm = (1<<(rs+8))/blk_num
	 */

	reg->reg_ldfw_blk_norm_rs = 2;
	/*u3: 0~7,  1/blk_num= norm>>(rs+8)*/

	reg->reg_ldfw_bl_max = 4095;       /*maximum BL value*/

	reg->reg_ldfw_boost_enable = 1;
	/* u1: enable signal for Boost filter on the tbl_matrix */

	reg->reg_ldfw_boost_gain = 64;
	/* u8: boost gain for the region that is
	 *	larger than the average, norm to 16 as "1"
	 */

	reg->reg_ldfw_enable = 1;

	reg->reg_ldfw_hist_valid_ofst = 63;/* u8, hist valid bin upward offset*/

	reg->reg_ldfw_hist_valid_rate = 64;
	/* u8, norm to 512 as "1", if hist_matrix[i]>(rate*histavg)>>9 */

	reg->reg_ldfw_sedglit_RL = 1;/*u1: single edge lit right/bottom mode*/

	reg->reg_ldfw_sf_enable = 1;
	/* u1: enable signal for spatial filter on the tbl_matrix */

	reg->reg_ldfw_sf_thrd = 1600;
	/*u12: threshold of difference to enable the sf;*/

	reg->reg_ldfw_sta_hdg_vflt = 1;

	for (T = 0; T < 8; T++)
		reg->reg_ldfw_sta_hdg_weight[T] = 64;

	reg->reg_ldfw_sta_max_hist_mode = 0;
	/* u2: mode of reference max/hist mode:
	 *	0: MIN(max, hist), 1: MAX(max, hist) 2: (max+hist)/2,
	 *	3: (max(a,b)*3 + min(a,b))/4
	 */

	reg->reg_ldfw_sta_max_mode = 3;
	/* u2: maximum selection for components:
	 *	0: r_max, 1: g_max, 2: b_max; 3: max(r,g,b)
	 */

	reg->reg_ldfw_sta_norm         = 128;
	reg->reg_ldfw_sta_norm_rs      = 5;

	reg->reg_ldfw_tf_alpha_ofst = 32;
	/* u8: ofset to alpha SFB_BL_matrix from last frame difference;*/

	reg->reg_ldfw_tf_alpha_rate = 16;
	/*u8: rate to SFB_BL_matrix from last frame difference;*/

	reg->reg_ldfw_tf_disable_th = 255;
	/* u8: 4x is the threshold to disable tf to the alpha
	 *	(SFB_BL_matrix from last frame difference;
	 */

	reg->reg_ldfw_tf_enable = 1;

	vnum = reg->reg_ld_blk_vnum;
	hnum = reg->reg_ld_blk_hnum;
	BSIZE = vnum * hnum;
	/*Initialization */
	ld_func_init_data(reg->bl_matrix, BSIZE, 4095);

	/* BackLight Modeling control register setting*/
	reg->reg_ld_blk_xtlk = 1;
	/* u1: 0 no block to block Xtalk model needed;	 1: Xtalk model needed*/
	reg->reg_ld_blk_mode = 1;
	/* u2: 0- LEFT/RIGHT Edge Lit;
	 *	1- Top/Bot Edge Lit; 2 - DirectLit modeled
	 *	H/V independent; 3- DirectLit modeled HV Circle distribution
	 */
	reg->reg_ld_reflect_hnum = 3;
	/*u3: numbers of band reflection considered in Horizontal
	 *		direction; 0~4
	 */
	reg->reg_ld_reflect_vnum = 0;
	/*u3: numbers of band reflection considered in Horizontal
	 *		direction; 0~4
	 */
	reg->reg_ld_blk_curmod = 0;
	/*u1: 0: H/V separately, 1 Circle distribution*/
	reg->reg_ld_bklut_intmod = 1;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	reg->reg_ld_bklit_intmod = 1;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	reg->reg_ld_bklit_lpfmod = 7;
	/*u3: 0: no LPF, 1:[1 14 1]/16;2:[1 6 1]/8; 3: [1 2 1]/4;
	 *		4:[9 14 9]/32  5/6/7: [5 6 5]/16;
	 */
	reg->reg_ld_bklit_celnum = 121;
	/*u8:0:1920~61####((Reg->reg_ld_pic_col_max+1)/32)+1;*/
	reg->reg_bl_matrix_avg = 3167;
	/* u12: DC of whole picture BL to be subtract from BL_matrix
	 *	during modeling (Set by FW dynamically)
	 */
	reg->reg_bl_matrix_compensate = 3167;
	/*	u12: DC of whole picture BL to be compensated back to
	 *	Litfull after the model (Set by FW dynamically);
	 */
	ld_func_init_data(reg->reg_ld_reflect_hdgr, 20, 32);
	/*20*u6: cells 1~20 for H Gains of different dist of Left/Right;*/
	ld_func_init_data(reg->reg_ld_reflect_vdgr, 20, 32);
	/*20*u6: cells 1~20 for V Gains of different dist of Top/Bot; */
	ld_func_init_data(reg->reg_ld_reflect_xdgr, 4, 32);/*  4*u6: */

	reg->reg_ld_vgain	= 256;/* u12 */
	reg->reg_ld_hgain	= 128;/* u12 */
	reg->reg_ld_litgain = 230;/* u12 */
	reg->reg_ld_litshft = 3;
	/* u3	right shif of bits for the all Lit's sum */
	ld_func_init_data(reg->reg_ld_bklit_valid, 32, 1);
	/*u1x32: valid bits for the 32 cell Bklit to contribut to current
	 *	position (refer to the backlit padding pattern)
	 * region division index  1  2	3	4 5(0) 6(1) 7(2) 8(3) 9(4)
	 *	10(5)11(6)12(7)13(8) 14(9)15(10) 16   17   18	19
	 */
	for (T = 0; T < LD_BLK_LEN_H; T++)
		reg->reg_ld_blk_hidx[T] = ld_blk_hidx[T];/* S14* BLK_LEN_H */
	for (T = 0; T < LD_BLK_LEN_V; T++)
		reg->reg_ld_blk_vidx[T] = ld_blk_vidx[T];/* S14x BLK_LEN_V */

	for (j = 0; j < 8; j++) {
		for (i = 0; i < 32; i++) {
			reg->reg_ld_lut_hdg_txlx[j][i] =
				reg_ld_lut_hdg_txlx[j][i];
		}
	}

	for (j = 0; j < 8; j++) {
		for (i = 0; i < 32; i++) {
			reg->reg_ld_lut_vdg_txlx[j][i] =
				reg_ld_lut_vdg_txlx[j][i];
		}
	}

	for (j = 0; j < 8; j++) {
		for (i = 0; i < 32; i++) {
			reg->reg_ld_lut_vhk_txlx[j][i] =
				reg_ld_lut_vhk_txlx[j][i];
		}
	}
	/* led LUT only choose fist one */
	for (i = 0; i < (16 * 24); i++)
		reg->reg_ld_lut_id[i]  = 0;
	/* set the VHk_pos and VHk_neg value ,normalized to
	 *	128 as "1" 20150428
	 */
	for (T = 0; T < 32; T++) {
		reg->reg_ld_lut_vhk_pos[T] = 128;/* vdist>=0 */
		reg->reg_ld_lut_vhk_neg[T] = 128;/* vdist<0 */
		reg->reg_ld_lut_hhk[T] = 128;/* hdist gain */
		reg->reg_ld_lut_vho_pos[T] = ld_lut_vho_pos_txlx[T];
		reg->reg_ld_lut_vho_neg[T] = ld_lut_vho_neg_txlx[T];
	}
	reg->reg_ld_lut_vho_ls = 0;

	for (i = 0; i < 8; i++) {
		reg->reg_ld_lut_hdg_lext_txlx[i] = ld_lut_hdg_lext_txlx[i];
		reg->reg_ld_lut_vdg_lext_txlx[i] = ld_lut_vdg_lext_txlx[i];
		reg->reg_ld_lut_vhk_lext_txlx[i] = ld_lut_vhk_lext_txlx[i];
	}

	/* set the demo window */
	reg->reg_ld_xlut_demo_roi_xstart = (reg->reg_ld_pic_col_max / 4);
	     /* u14 start col index of the region of interest */
	reg->reg_ld_xlut_demo_roi_xend = (reg->reg_ld_pic_col_max * 3 / 4);
	  /* u14 end col index of the region of interest */
	reg->reg_ld_xlut_demo_roi_ystart = (reg->reg_ld_pic_row_max / 4);
	     /* u14 start row index of the region of interest */
	reg->reg_ld_xlut_demo_roi_yend = (reg->reg_ld_pic_row_max * 3 / 4);
	   /*  u14 end row index of the region of interest */
	reg->reg_ld_xlut_iroi_enable = 1;
	     /*  u1: enable rgb LUT remapping inside regon of interest:
	      *	0: no rgb remapping; 1: enable rgb remapping
	      */
	reg->reg_ld_xlut_oroi_enable = 1;
	    /* u1: enable rgb LUT remapping outside regon of interest:
	     * 0: no rgb remapping; 1: enable rgb remapping
	     */
	/*  Registers used in LD_RGB_LUT for RGB remaping */
	reg->reg_ld_rgb_mapping_demo = 0;
	/* u2: 0 no demo mode 1: display BL_fulpel on RGB */
	reg->reg_ld_x_lut_interp_mode[0] = 0;
	 /* U1 0: using linear interpolation between to neighbour LUT;
	  *	1: use the nearest LUT results
	  */
	reg->reg_ld_x_lut_interp_mode[1] = 0;
	 /*  U1 0: using linear interpolation between to neighbour LUT;
	  *	1: use the nearest LUT results
	  */
	reg->reg_ld_x_lut_interp_mode[2] = 0;
	 /* U1 0: using linear interpolation between to neighbour LUT;
	  *	 1: use the nearest LUT results
	  */
	ld_func_lut_init(reg);
}

void ld_func_cfg_ldreg(struct ld_reg_s *reg)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);

	switch (bdrv->data->chip_type) {
	case LCD_CHIP_TM2:
	case LCD_CHIP_TM2B:
		ld_func_cfg_ldreg_tm2(reg);
		break;
	default:
		break;
	}
}

void ldim_func_profile_update(struct ldim_fw_s *fw,
			      struct ldim_profile_s *profile)
{
	struct ld_reg_s *nprm = fw->ctrl->nprm;
	int i, j;

	if (!nprm || !profile) {
		LDIMERR("%s: nprm or ld_profile is null\n", __func__);
		return;
	}
	if (profile->mode != 1)
		return;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 32; j++) {
			nprm->reg_ld_lut_hdg_txlx[i][j] = profile->ld_lut_hdg[j];
			nprm->reg_ld_lut_vdg_txlx[i][j] = profile->ld_lut_vdg[j];
			nprm->reg_ld_lut_vhk_txlx[i][j] = profile->ld_lut_vhk[j];
		}
	}

	LDIMPR("%s\n", __func__);
}

#define LD_ONESIDE    1
	/* 0: left/top side,
	 *	1: right/bot side, others: non-one-side
	 */
void ld_func_fw_cfg_once(struct ld_reg_s *nprm)
{
	int k, dlt, j, i;
	int hofst = 4;
	int vofst = 4;
	int hnrm = 256;/* Hgain norm (256: for Hlen==2048) */
	int vnrm = 256;/* Vgain norm (256: for Vlen==2048),related to VHk */

	int drt_ld_lut_dg[] = {254, 248, 239, 226, 211, 194, 176, 156, 137,
				119, 101, 85, 70, 57, 45, 36, 28, 21, 16,
				12, 9, 6, 4, 3, 2, 1, 1, 1, 0, 0, 0, 0};

	/* demo mode to show the Lit map */
	nprm->reg_ld_rgb_mapping_demo = 0;

	/*  set Reg->reg_ld_bklit_celnum */
	nprm->reg_ld_bklit_celnum = (nprm->reg_ld_pic_col_max + 63) / 32;

	/* set same region division for statistics */
	/*nprm->reg_ld_sta_vnum  = nprm->reg_ld_blk_vnum;*/
	/*nprm->reg_ld_sta_hnum  = nprm->reg_ld_blk_hnum;*/

	/* STA1max_Hidx */
	nprm->reg_ld_sta1max_hidx[0] = 0;
	for (k = 1; k < LD_STA_LEN_H; k++) {
		nprm->reg_ld_sta1max_hidx[k] = ((nprm->reg_ld_pic_col_max +
			(nprm->reg_ld_sta_hnum) - 1) /
			(nprm->reg_ld_sta_hnum)) * k;
		if (nprm->reg_ld_sta1max_hidx[k] > 4095)
			nprm->reg_ld_sta1max_hidx[k] = 4095;/* clip U12 */
	}
	/* STA1max_Vidx */
	nprm->reg_ld_sta1max_vidx[0] = 0;
	for (k = 1; k < LD_STA_LEN_V; k++) {
		nprm->reg_ld_sta1max_vidx[k] = ((nprm->reg_ld_pic_row_max +
			(nprm->reg_ld_sta_vnum) - 1) /
			(nprm->reg_ld_sta_vnum)) * k;
		if (nprm->reg_ld_sta1max_vidx[k] > 4095)
			nprm->reg_ld_sta1max_vidx[k] = 4095;/* clip to U12 */
	}
	/* config LD_STA2max_H/Vidx/LD_STAhist_H/Vidx */
	for (k = 0; k < LD_STA_LEN_H; k++) {
		nprm->reg_ld_sta2max_hidx[k] = nprm->reg_ld_sta1max_hidx[k];
		nprm->reg_ld_sta_hist_hidx[k] = nprm->reg_ld_sta1max_hidx[k];
	}
	for (k = 0; k < LD_STA_LEN_V; k++) {
		nprm->reg_ld_sta2max_vidx[k] = nprm->reg_ld_sta1max_vidx[k];
		nprm->reg_ld_sta_hist_vidx[k] = nprm->reg_ld_sta1max_vidx[k];
	}
	if (nprm->reg_ld_blk_mode == 0) {/* Left/right EdgeLit */
		/*  set reflect num */
		nprm->reg_ld_reflect_hnum = 0;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		nprm->reg_ld_reflect_vnum = 3;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		/* config reg_ld_blk_hidx */
		for (k = 0; k < LD_BLK_LEN_H; k++) {
			dlt = nprm->reg_ld_pic_col_max /
				(nprm->reg_ld_blk_hnum) * 2;
#if (LD_ONESIDE == 1) /* bot/right one side */
			nprm->reg_ld_blk_hidx[k] = 0 + (k - hofst) * dlt;
#else
			nprm->reg_ld_blk_hidx[k] = (-1) * (dlt / 2) +
				(k - hofst) * dlt;
#endif
			nprm->reg_ld_blk_hidx[k] = (nprm->reg_ld_blk_hidx[k] >
				8191) ? 8191 : ((nprm->reg_ld_blk_hidx[k] <
				(-8192)) ? (-8192) :
				(nprm->reg_ld_blk_hidx[k]));/* Clip to S14 */
		}
		/*  config reg_ld_blk_vidx */
		for (k = 0; k < LD_BLK_LEN_V; k++) {
			dlt = (nprm->reg_ld_pic_row_max) /
				(nprm->reg_ld_blk_vnum);
			nprm->reg_ld_blk_vidx[k] = 0 + (k - vofst) * dlt;
			nprm->reg_ld_blk_vidx[k] = (nprm->reg_ld_blk_vidx[k] >
				8191) ? 8191 : ((nprm->reg_ld_blk_vidx[k] <
				(-8192)) ? (-8192) :
				(nprm->reg_ld_blk_vidx[k]));/*Clip to S14*/
		}
		/*  configure  Hgain/Vgain */
		nprm->reg_ld_hgain = (hnrm * 2048 / (nprm->reg_ld_pic_col_max));
		nprm->reg_ld_vgain = (vnrm * 2048 / (nprm->reg_ld_pic_row_max));
		nprm->reg_ld_hgain = (nprm->reg_ld_hgain >
			4095)  ? 4095 : (nprm->reg_ld_hgain);
		nprm->reg_ld_vgain = (nprm->reg_ld_vgain >
			4095)  ? 4095 : (nprm->reg_ld_vgain);

		/* if one side led, set the Hdg/Vdg/VHk differently */
		if (nprm->reg_ld_blk_hnum == 1) {
			for (k = 0; k < LD_LUT_LEN; k++) {
				nprm->reg_ld_lut_hdg[k] = ld_lut_hdg1[k];
				nprm->reg_ld_lut_vdg[k] = ld_lut_vdg1[k];
				nprm->reg_ld_lut_vhk[k] = ld_lut_vhk1[k];
				nprm->reg_ld_lut_vho_neg[k] = ld_lut_vho_neg[k];
				nprm->reg_ld_lut_vho_pos[k] = ld_lut_vho_pos[k];
			}
			nprm->reg_ld_lut_hdg_lext = 2 * nprm->reg_ld_lut_hdg[0]
				- nprm->reg_ld_lut_hdg[1];
			nprm->reg_ld_lut_vdg_lext = 2 * nprm->reg_ld_lut_vdg[0]
				- nprm->reg_ld_lut_vdg[1];
			nprm->reg_ld_lut_vhk_lext = 2 * nprm->reg_ld_lut_vhk[0]
				- nprm->reg_ld_lut_vhk[1];
			nprm->reg_ld_litgain = 230;
			/* u12 will be adjust according to pannel */
			/* specially for TXLX config of one
			 * side led mode(mode=1)--kite1023
			 */
			for (k = 0; k < LD_LUT_LEN; k++) {
				/* group 2 as one led lut--kite1023 */
				nprm->reg_ld_lut_hdg_txlx[1][k] =
					ld_lut_hdg1_txlx[k];
				nprm->reg_ld_lut_vdg_txlx[1][k] =
					ld_lut_vdg1_txlx[k];
				nprm->reg_ld_lut_vhk_txlx[1][k] =
					ld_lut_vhk1_txlx[k];
				/* same with gxtvbb,default is ok */
				nprm->reg_ld_lut_vho_neg[k] = ld_lut_vho_neg[k];
				nprm->reg_ld_lut_vho_pos[k] = ld_lut_vho_pos[k];
			}
			for (j = 0; j < 8; j++) {
				nprm->reg_ld_lut_hdg_lext_txlx[j] =
					2 * (nprm->reg_ld_lut_hdg_txlx[1][0]) -
					(nprm->reg_ld_lut_hdg_txlx[1][1]);
				nprm->reg_ld_lut_vdg_lext_txlx[j] =
					2 * (nprm->reg_ld_lut_vdg_txlx[1][0]) -
					(nprm->reg_ld_lut_vdg_txlx[1][1]);
				nprm->reg_ld_lut_vhk_lext_txlx[j] =
					2 * (nprm->reg_ld_lut_vhk_txlx[1][0]) -
					(nprm->reg_ld_lut_vhk_txlx[1][1]);
			}
			nprm->reg_ld_litgain = 230;
			/* led LUT only choose group 2--kite1023 */
			for (i = 0; i < 16 * 24; i++)
				nprm->reg_ld_lut_id[i]  = 1;
		}
	} else if (nprm->reg_ld_blk_mode == 1) {/* Top/Bot EdgeLit */
		/* set reflect num */
		nprm->reg_ld_reflect_hnum = 3;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		nprm->reg_ld_reflect_vnum = 0;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		/* config reg_ld_blk_hidx */
		for (k = 0; k < LD_BLK_LEN_H; k++) {
			dlt = nprm->reg_ld_pic_col_max / nprm->reg_ld_blk_hnum;
			nprm->reg_ld_blk_hidx[k] = 0 + (k - hofst) * dlt;
			nprm->reg_ld_blk_hidx[k] = (nprm->reg_ld_blk_hidx[k] >
				8191) ? 8191 : ((nprm->reg_ld_blk_hidx[k] <
				(-8192)) ? (-8192) :
				(nprm->reg_ld_blk_hidx[k]));/*Clip to S14*/
		}
		/* config reg_ld_blk_vidx */
		for (k = 0; k < LD_BLK_LEN_V; k++) {
			dlt = nprm->reg_ld_pic_row_max /
				(nprm->reg_ld_blk_vnum) * 2;
#if (LD_ONESIDE == 1) /*  bot/right one side */
			nprm->reg_ld_blk_vidx[k] = 0 + (k - vofst) * dlt;
#else
			nprm->reg_ld_blk_vidx[k] = (-1) * (dlt / 2) +
				(k - vofst) * dlt;
#endif
			nprm->reg_ld_blk_vidx[k] = (nprm->reg_ld_blk_vidx[k] >
				8191) ? 8191 : ((nprm->reg_ld_blk_vidx[k] <
				(-8192)) ? (-8192) :
				(nprm->reg_ld_blk_vidx[k])); /* Clip to S14*/
		}
		/*  configure  Hgain/Vgain */
		nprm->reg_ld_hgain = (hnrm * 2048 / (nprm->reg_ld_pic_row_max));
		nprm->reg_ld_vgain = (vnrm * 2048 / (nprm->reg_ld_pic_col_max));
		nprm->reg_ld_hgain = (nprm->reg_ld_hgain >
			4095) ? 4095 : (nprm->reg_ld_hgain);
		nprm->reg_ld_vgain = (nprm->reg_ld_vgain >
			4095) ? 4095 : (nprm->reg_ld_vgain);

		/* if one side led, set the Hdg/Vdg/VHk differently */
		if (nprm->reg_ld_blk_vnum == 1) {
			for (k = 0; k < LD_LUT_LEN; k++) {
				nprm->reg_ld_lut_hdg[k] = ld_lut_hdg1[k];
				nprm->reg_ld_lut_vdg[k] = ld_lut_vdg1[k];
				nprm->reg_ld_lut_vhk[k] = ld_lut_vhk1[k];
				nprm->reg_ld_lut_vho_neg[k] = ld_lut_vho_neg[k];
				nprm->reg_ld_lut_vho_pos[k] = ld_lut_vho_pos[k];
			}
			nprm->reg_ld_lut_hdg_lext = 2 * nprm->reg_ld_lut_hdg[0]
				- nprm->reg_ld_lut_hdg[1];
			nprm->reg_ld_lut_vdg_lext = 2 * nprm->reg_ld_lut_vdg[0]
				- nprm->reg_ld_lut_vdg[1];
			nprm->reg_ld_lut_vhk_lext = 2 * nprm->reg_ld_lut_vhk[0]
				- nprm->reg_ld_lut_vhk[1];
			/* specially for TXLX config--kite1023 */
			for (k = 0; k < LD_LUT_LEN; k++) {
				/* group 2 as one led lut--kite1023 */
				nprm->reg_ld_lut_hdg_txlx[1][k] =
					ld_lut_hdg1_txlx[k];
				nprm->reg_ld_lut_vdg_txlx[1][k] =
					ld_lut_vdg1_txlx[k];
				nprm->reg_ld_lut_vhk_txlx[1][k] =
					ld_lut_vhk1_txlx[k];
				/* same with gxtvbb,default is ok */
				nprm->reg_ld_lut_vho_neg[k] = ld_lut_vho_neg[k];
				nprm->reg_ld_lut_vho_pos[k] = ld_lut_vho_pos[k];
			}
			 /* 8 led lut */
			for (j = 0; j < 8; j++) {
				nprm->reg_ld_lut_hdg_lext_txlx[j] =
					2 * (nprm->reg_ld_lut_hdg_txlx[1][0]) -
					(nprm->reg_ld_lut_hdg_txlx[1][1]);
				nprm->reg_ld_lut_vdg_lext_txlx[j] =
					2 * (nprm->reg_ld_lut_vdg_txlx[1][0]) -
					(nprm->reg_ld_lut_vdg_txlx[1][1]);
				nprm->reg_ld_lut_vhk_lext_txlx[j] =
					2 * (nprm->reg_ld_lut_vhk_txlx[1][0]) -
					(nprm->reg_ld_lut_vhk_txlx[1][1]);
			}
			/*led LUT only choose group */
			for (i = 0; i < 16 * 24; i++)
				nprm->reg_ld_lut_id[i]  = 1;
			nprm->reg_ld_litgain = 256;
		}
	} else {/* DirectLit */
		/*  set reflect num */
		nprm->reg_ld_reflect_hnum = 2;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		nprm->reg_ld_reflect_vnum = 2;
		/* u3: numbers of band reflection considered in Horizontal
		 *	direction; 0~4
		 */
		/* config reg_ld_blk_hidx */
		for (k = 0; k < LD_BLK_LEN_H; k++) {
			dlt = nprm->reg_ld_pic_col_max / nprm->reg_ld_blk_hnum;
			nprm->reg_ld_blk_hidx[k] = 0 + (k - hofst) * dlt;
			nprm->reg_ld_blk_hidx[k] = (nprm->reg_ld_blk_hidx[k] >
				8191) ? 8191 : ((nprm->reg_ld_blk_hidx[k] <
				(-8192)) ? (-8192) :
				(nprm->reg_ld_blk_hidx[k]));/*Clip to S14*/
		}
		/* config reg_ld_blk_vidx */
		for (k = 0; k < LD_BLK_LEN_V; k++) {
			dlt = nprm->reg_ld_pic_row_max / nprm->reg_ld_blk_vnum;
			nprm->reg_ld_blk_vidx[k] = 0 + (k - vofst) * dlt;
			nprm->reg_ld_blk_vidx[k] = (nprm->reg_ld_blk_vidx[k] >
				8191) ? 8191 : ((nprm->reg_ld_blk_vidx[k] <
				(-8192)) ? (-8192) :
				(nprm->reg_ld_blk_vidx[k]));/*Clip to S14*/
		}
		/* configure  Hgain/Vgain */
		nprm->reg_ld_hgain = ((nprm->reg_ld_blk_hnum) * 73 * 2048 /
			(nprm->reg_ld_pic_col_max));
		nprm->reg_ld_vgain = ((nprm->reg_ld_blk_vnum) * 73 * 2048 /
			(nprm->reg_ld_pic_row_max));
		nprm->reg_ld_hgain = (nprm->reg_ld_hgain >
			4095) ? 4095 : (nprm->reg_ld_hgain);
		nprm->reg_ld_vgain = (nprm->reg_ld_vgain >
			4095) ? 4095 : (nprm->reg_ld_vgain);

		/*  configure */
		for (k = 0; k < LD_LUT_LEN; k++) {
			nprm->reg_ld_lut_hdg[k] = drt_ld_lut_dg[k];
			nprm->reg_ld_lut_vdg[k] = drt_ld_lut_dg[k];
			nprm->reg_ld_lut_vhk[k] = 128;
			/*config once to make sure */
			nprm->reg_ld_lut_vhk_neg[k] = 128;
			nprm->reg_ld_lut_vhk_pos[k] = 128;
			/* nprm->reg_ld_lut_vho_neg =
			 * nprm->reg_ld_lut_vho_neg_TXLX
			 *	register same with gxtvbb
			 */
			nprm->reg_ld_lut_vho_neg[k] = 0;
			/* nprm->reg_ld_lut_vho_pos_TXLX */
			nprm->reg_ld_lut_vho_pos[k] = 0;
		}
		/* just for confirm--kite1023 */
		for (j = 0; j < 8; j++) {
			for (k = 0; k < LD_LUT_LEN; k++) {
				nprm->reg_ld_lut_hdg_txlx[j][k] =
					reg_ld_lut_hdg_txlx[j][k];
				nprm->reg_ld_lut_vdg_txlx[j][k] =
					reg_ld_lut_vdg_txlx[j][k];
				nprm->reg_ld_lut_vhk_txlx[j][k] =
					reg_ld_lut_vhk_txlx[j][k];
			}
		}
		nprm->reg_ld_lut_vho_ls = 0;
		nprm->reg_ld_lut_hdg_lext = 2 * (nprm->reg_ld_lut_hdg[0])
			- (nprm->reg_ld_lut_hdg[1]);
		nprm->reg_ld_lut_vdg_lext = 2 * (nprm->reg_ld_lut_vdg[0])
			- (nprm->reg_ld_lut_vdg[1]);
		nprm->reg_ld_lut_vhk_lext = 2 * (nprm->reg_ld_lut_vhk[0])
			- (nprm->reg_ld_lut_vhk[1]);

		/*led LUT only choose fist one-- make confirm --kite1023 */
		for (i = 0; i < 16 * 24; i++)
			nprm->reg_ld_lut_id[i]  = 0;
		for (j = 0; j < 8; j++) { /*8 led lut */
			nprm->reg_ld_lut_hdg_lext_txlx[j] =
				2 * (nprm->reg_ld_lut_hdg_txlx[0][0]) -
				(nprm->reg_ld_lut_hdg_txlx[0][1]);/*260*/
			nprm->reg_ld_lut_vdg_lext_txlx[j] =
				2 * (nprm->reg_ld_lut_vdg_txlx[0][0]) -
				(nprm->reg_ld_lut_vdg_txlx[0][1]);/*260*/
			nprm->reg_ld_lut_vhk_lext_txlx[j] =
				2 * (nprm->reg_ld_lut_vhk_txlx[0][0]) -
				(nprm->reg_ld_lut_vhk_txlx[0][1]);/*128*/
		}
		nprm->reg_ld_litgain = 256;
		nprm->reg_ld_blk_curmod = 0;
	}
	/* set one time nprm here */
	nprm->reg_ld_sta1max_lpf = 1;
	/* u1: STA1max statistics on [1 2 1]/4 filtered results */
	nprm->reg_ld_sta2max_lpf = 1;
	/* u1: STA2max statistics on [1 2 1]/4 filtered results */
	nprm->reg_ld_sta_hist_lpf = 1;
	/*u1: STAhist statistics on [1 2 1]/4 filtered results */
	nprm->reg_ld_x_lut_interp_mode[0] = 0;
	nprm->reg_ld_x_lut_interp_mode[1] = 0;
	nprm->reg_ld_x_lut_interp_mode[2] = 0;
	/* set the demo window*/
	nprm->reg_ld_xlut_demo_roi_xstart = (nprm->reg_ld_pic_col_max / 4);
	/* u14 start col index of the region of interest */
	nprm->reg_ld_xlut_demo_roi_xend   = (nprm->reg_ld_pic_col_max * 3 / 4);
	/* u14 end col index of the region of interest */
	nprm->reg_ld_xlut_demo_roi_ystart = (nprm->reg_ld_pic_row_max / 4);
	/* u14 start row index of the region of interest */
	nprm->reg_ld_xlut_demo_roi_yend  = (nprm->reg_ld_pic_row_max * 3 / 4);
	/* u14 end row index of the region of interest */
	nprm->reg_ld_xlut_iroi_enable = 1;
	/* u1: enable rgb LUT remapping inside regon of interest:
	 *	 0: no rgb remapping; 1: enable rgb remapping
	 */
	nprm->reg_ld_xlut_oroi_enable = 1;
	/* u1: enable rgb LUT remapping outside regon of interest:
	 *	 0: no rgb remapping; 1: enable rgb remapping
	 */
}
