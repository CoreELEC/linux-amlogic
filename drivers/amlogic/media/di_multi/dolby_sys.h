/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_vframe.h
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

#ifndef __DOLBY_SYS_H__
#define __DOLBY_SYS_H_
#if 1
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/cdev.h>

#define DIDEF_G2L_LUT_SIZE           0x100

struct didm_lut_ipcore_s {
	u32 TmLutI[64 * 4];
	u32 TmLutS[64 * 4];
	u32 SmLutI[64 * 4];
	u32 SmLutS[64 * 4];
	u32 G2L[DIDEF_G2L_LUT_SIZE];
};

struct didm_register_ipcore_1_s {
	u32 SRange;
	u32 Srange_Inverse;
	u32 Frame_Format_1;
	u32 Frame_Format_2;
	u32 Frame_Pixel_Def;
	u32 Y2RGB_Coefficient_1;
	u32 Y2RGB_Coefficient_2;
	u32 Y2RGB_Coefficient_3;
	u32 Y2RGB_Coefficient_4;
	u32 Y2RGB_Coefficient_5;
	u32 Y2RGB_Offset_1;
	u32 Y2RGB_Offset_2;
	u32 Y2RGB_Offset_3;
	u32 EOTF;
/*	u32 Sparam_1;*/
/*	u32 Sparam_2;*/
/*	u32 Sgamma; */
	u32 A2B_Coefficient_1;
	u32 A2B_Coefficient_2;
	u32 A2B_Coefficient_3;
	u32 A2B_Coefficient_4;
	u32 A2B_Coefficient_5;
	u32 C2D_Coefficient_1;
	u32 C2D_Coefficient_2;
	u32 C2D_Coefficient_3;
	u32 C2D_Coefficient_4;
	u32 C2D_Coefficient_5;
	u32 C2D_Offset;
	u32 Active_area_left_top;
	u32 Active_area_bottom_right;
};

struct dicomposer_register_ipcore_s {
	/* offset 0xc8 */
	u32 Composer_Mode;
	u32 VDR_Resolution;
	u32 Bit_Depth;
	u32 Coefficient_Log2_Denominator;
	u32 BL_Num_Pivots_Y;
	u32 BL_Pivot[5];
	u32 BL_Order;
	u32 BL_Coefficient_Y[8][3];
	u32 EL_NLQ_Offset_Y;
	u32 EL_Coefficient_Y[3];
	u32 Mapping_IDC_U;
	u32 BL_Num_Pivots_U;
	u32 BL_Pivot_U[3];
	u32 BL_Order_U;
	u32 BL_Coefficient_U[4][3];
	u32 MMR_Coefficient_U[22][2];
	u32 MMR_Order_U;
	u32 EL_NLQ_Offset_U;
	u32 EL_Coefficient_U[3];
	u32 Mapping_IDC_V;
	u32 BL_Num_Pivots_V;
	u32 BL_Pivot_V[3];
	u32 BL_Order_V;
	u32 BL_Coefficient_V[4][3];
	u32 MMR_Coefficient_V[22][2];
	u32 MMR_Order_V;
	u32 EL_NLQ_Offset_V;
	u32 EL_Coefficient_V[3];
};
#endif

#define DOLBY_CORE1C_REG_START                     0x1800
#define DOLBY_CORE1C_CLKGATE_CTRL                  0x18f2
#define DOLBY_CORE1C_SWAP_CTRL0                    0x18f3
#define DOLBY_CORE1C_SWAP_CTRL1                    0x18f4
#define DOLBY_CORE1C_SWAP_CTRL2                    0x18f5
#define DOLBY_CORE1C_SWAP_CTRL3                    0x18f6
#define DOLBY_CORE1C_SWAP_CTRL4                    0x18f7
#define DOLBY_CORE1C_SWAP_CTRL5                    0x18f8
#define DOLBY_CORE1C_DMA_CTRL                      0x18f9
#define DOLBY_CORE1C_DMA_STATUS                    0x18fa
#define DOLBY_CORE1C_STATUS0                       0x18fb
#define DOLBY_CORE1C_STATUS1                       0x18fc
#define DOLBY_CORE1C_STATUS2                       0x18fd
#define DOLBY_CORE1C_STATUS3                       0x18fe
#define DOLBY_CORE1C_DMA_PORT                      0x18ff

#define DI_SC_TOP_CTRL                             0x374f
#define DI_HDR_IN_HSIZE                            0x376e
#define DI_HDR_IN_VSIZE                            0x376f

void di_dolby_sw_init(void);
int di_dolby_do_setting(void);
void di_dolby_enable(bool enable);
#endif

