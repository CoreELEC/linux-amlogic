/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/arch/ve_regs.h
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

#ifndef _VE_REG_H
#define _VE_REG_H

/* ********************************************************************* */
/* ******** GAMMA REGISTERS ******** */
/* ********************************************************************* */
#define L_GAMMA_CNTL_PORT                            0x1400
   #define  GAMMA_VCOM_POL    7     /* RW */
   #define  GAMMA_RVS_OUT     6     /* RW */
   #define  ADR_RDY           5     /* Read Only */
   #define  WR_RDY            4     /* Read Only */
   #define  RD_RDY            3     /* Read Only */
   #define  GAMMA_TR          2     /* RW */
   #define  GAMMA_SET         1     /* RW */
   #define  GAMMA_EN          0     /* RW */
#define L_GAMMA_DATA_PORT                            0x1401
#define L_GAMMA_ADDR_PORT                            0x1402
   #define  H_RD              12
   #define  H_AUTO_INC        11
   #define  H_SEL_R           10
   #define  H_SEL_G           9
   #define  H_SEL_B           8
   #define  HADR_MSB          7            /* 7:0 */
   #define  HADR              0            /* 7:0 */
/* ********************************************************************* */
/* ******** VIDEO ENHANCEMENT REGISTERS ******** */
/* ********************************************************************* */

/* #define VPP_BLACKEXT_CTRL             0x1d80 */
#define BEXT_START_BIT         24
#define BEXT_START_WID          8
#define BEXT_SLOPE1_BIT        16
#define BEXT_SLOPE1_WID         8
#define BEXT_MIDPT_BIT          8
#define BEXT_MIDPT_WID          8
#define BEXT_SLOPE2_BIT         0
#define BEXT_SLOPE2_WID         8

/* #define VPP_DNLP_CTRL_00              0x1d81 */
#define DNLP_GAMMA03_BIT       24
#define DNLP_GAMMA03_WID        8
#define DNLP_GAMMA02_BIT       16
#define DNLP_GAMMA02_WID        8
#define DNLP_GAMMA01_BIT        8
#define DNLP_GAMMA01_WID        8
#define DNLP_GAMMA00_BIT        0
#define DNLP_GAMMA00_WID        8

/* #define VPP_DNLP_CTRL_01              0x1d82 */
#define DNLP_GAMMA07_BIT       24
#define DNLP_GAMMA07_WID        8
#define DNLP_GAMMA06_BIT       16
#define DNLP_GAMMA06_WID        8
#define DNLP_GAMMA05_BIT        8
#define DNLP_GAMMA05_WID        8
#define DNLP_GAMMA04_BIT        0
#define DNLP_GAMMA04_WID        8

/* #define VPP_DNLP_CTRL_02              0x1d83 */
#define DNLP_GAMMA11_BIT       24
#define DNLP_GAMMA11_WID        8
#define DNLP_GAMMA10_BIT       16
#define DNLP_GAMMA10_WID        8
#define DNLP_GAMMA09_BIT        8
#define DNLP_GAMMA09_WID        8
#define DNLP_GAMMA08_BIT        0
#define DNLP_GAMMA08_WID        8

/* #define VPP_DNLP_CTRL_03              0x1d84 */
#define DNLP_GAMMA15_BIT       24
#define DNLP_GAMMA15_WID        8
#define DNLP_GAMMA14_BIT       16
#define DNLP_GAMMA14_WID        8
#define DNLP_GAMMA13_BIT        8
#define DNLP_GAMMA13_WID        8
#define DNLP_GAMMA12_BIT        0
#define DNLP_GAMMA12_WID        8

/* #define VPP_DNLP_CTRL_04              0x1d85 */
#define DNLP_GAMMA19_BIT       24
#define DNLP_GAMMA19_WID        8
#define DNLP_GAMMA18_BIT       16
#define DNLP_GAMMA18_WID        8
#define DNLP_GAMMA17_BIT        8
#define DNLP_GAMMA17_WID        8
#define DNLP_GAMMA16_BIT        0
#define DNLP_GAMMA16_WID        8

/* #define VPP_DNLP_CTRL_05              0x1d86 */
#define DNLP_GAMMA23_BIT       24
#define DNLP_GAMMA23_WID        8
#define DNLP_GAMMA22_BIT       16
#define DNLP_GAMMA22_WID        8
#define DNLP_GAMMA21_BIT        8
#define DNLP_GAMMA21_WID        8
#define DNLP_GAMMA20_BIT        0
#define DNLP_GAMMA20_WID        8

/* #define VPP_DNLP_CTRL_06              0x1d87 */
#define DNLP_GAMMA27_BIT       24
#define DNLP_GAMMA27_WID        8
#define DNLP_GAMMA26_BIT       16
#define DNLP_GAMMA26_WID        8
#define DNLP_GAMMA25_BIT        8
#define DNLP_GAMMA25_WID        8
#define DNLP_GAMMA24_BIT        0
#define DNLP_GAMMA24_WID        8

/* #define VPP_DNLP_CTRL_07              0x1d88 */
#define DNLP_GAMMA31_BIT       24
#define DNLP_GAMMA31_WID        8
#define DNLP_GAMMA30_BIT       16
#define DNLP_GAMMA30_WID        8
#define DNLP_GAMMA29_BIT        8
#define DNLP_GAMMA29_WID        8
#define DNLP_GAMMA28_BIT        0
#define DNLP_GAMMA28_WID        8

/* #define VPP_DNLP_CTRL_08              0x1d89 */
#define DNLP_GAMMA35_BIT       24
#define DNLP_GAMMA35_WID        8
#define DNLP_GAMMA34_BIT       16
#define DNLP_GAMMA34_WID        8
#define DNLP_GAMMA33_BIT        8
#define DNLP_GAMMA33_WID        8
#define DNLP_GAMMA32_BIT        0
#define DNLP_GAMMA32_WID        8

/* #define VPP_DNLP_CTRL_09              0x1d8a */
#define DNLP_GAMMA39_BIT       24
#define DNLP_GAMMA39_WID        8
#define DNLP_GAMMA38_BIT       16
#define DNLP_GAMMA38_WID        8
#define DNLP_GAMMA37_BIT        8
#define DNLP_GAMMA37_WID        8
#define DNLP_GAMMA36_BIT        0
#define DNLP_GAMMA36_WID        8

/* #define VPP_DNLP_CTRL_10              0x1d8b */
#define DNLP_GAMMA43_BIT       24
#define DNLP_GAMMA43_WID        8
#define DNLP_GAMMA42_BIT       16
#define DNLP_GAMMA42_WID        8
#define DNLP_GAMMA41_BIT        8
#define DNLP_GAMMA41_WID        8
#define DNLP_GAMMA40_BIT        0
#define DNLP_GAMMA40_WID        8

/* #define VPP_DNLP_CTRL_11              0x1d8c */
#define DNLP_GAMMA47_BIT       24
#define DNLP_GAMMA47_WID        8
#define DNLP_GAMMA46_BIT       16
#define DNLP_GAMMA46_WID        8
#define DNLP_GAMMA45_BIT        8
#define DNLP_GAMMA45_WID        8
#define DNLP_GAMMA44_BIT        0
#define DNLP_GAMMA44_WID        8

/* #define VPP_DNLP_CTRL_12              0x1d8d */
#define DNLP_GAMMA51_BIT       24
#define DNLP_GAMMA51_WID        8
#define DNLP_GAMMA50_BIT       16
#define DNLP_GAMMA50_WID        8
#define DNLP_GAMMA49_BIT        8
#define DNLP_GAMMA49_WID        8
#define DNLP_GAMMA48_BIT        0
#define DNLP_GAMMA48_WID        8

/* #define VPP_DNLP_CTRL_13              0x1d8e */
#define DNLP_GAMMA55_BIT       24
#define DNLP_GAMMA55_WID        8
#define DNLP_GAMMA54_BIT       16
#define DNLP_GAMMA54_WID        8
#define DNLP_GAMMA53_BIT        8
#define DNLP_GAMMA53_WID        8
#define DNLP_GAMMA52_BIT        0
#define DNLP_GAMMA52_WID        8

/* #define VPP_DNLP_CTRL_14              0x1d8f */
#define DNLP_GAMMA59_BIT       24
#define DNLP_GAMMA59_WID        8
#define DNLP_GAMMA58_BIT       16
#define DNLP_GAMMA58_WID        8
#define DNLP_GAMMA57_BIT        8
#define DNLP_GAMMA57_WID        8
#define DNLP_GAMMA56_BIT        0
#define DNLP_GAMMA56_WID        8

/* #define VPP_DNLP_CTRL_15              0x1d90 */
#define DNLP_GAMMA63_BIT       24
#define DNLP_GAMMA63_WID        8
#define DNLP_GAMMA62_BIT       16
#define DNLP_GAMMA62_WID        8
#define DNLP_GAMMA61_BIT        8
#define DNLP_GAMMA61_WID        8
#define DNLP_GAMMA60_BIT        0
#define DNLP_GAMMA60_WID        8

/* #define VPP_PEAKING_HGAIN             0x1d91 */
#define VLTI_STEP_BIT          28
#define VLTI_STEP_WID           1
#define VLTI_STEP2_BIT         27
#define VLTI_STEP2_WID          1
#define HLTI_STEP_BIT          25
#define HLTI_STEP_WID           2
#define PEAK_GAIN_H1_BIT       20
#define PEAK_GAIN_H1_WID        5
#define PEAK_GAIN_H2_BIT       15
#define PEAK_GAIN_H2_WID        5
#define PEAK_GAIN_H3_BIT       10
#define PEAK_GAIN_H3_WID        5
#define PEAK_GAIN_H4_BIT        5
#define PEAK_GAIN_H4_WID        5
#define PEAK_GAIN_H5_BIT        0
#define PEAK_GAIN_H5_WID        5

/* #define VPP_PEAKING_VGAIN             0x1d92 */
#define VCTI_BUF_EN_BIT        31
#define VCTI_BUF_EN_WID         1
#define VCTI_BUF_MODE_C5L_BIT  30
#define VCTI_BUF_MODE_C5L_WID   1
#define PEAK_GAIN_V1_BIT       25
#define PEAK_GAIN_V1_WID        5
#define PEAK_GAIN_V2_BIT       20
#define PEAK_GAIN_V2_WID        5
#define PEAK_GAIN_V3_BIT       15
#define PEAK_GAIN_V3_WID        5
#define PEAK_GAIN_V4_BIT       10
#define PEAK_GAIN_V4_WID        5
#define PEAK_GAIN_V5_BIT        5
#define PEAK_GAIN_V5_WID        5
#define PEAK_GAIN_V6_BIT        0
#define PEAK_GAIN_V6_WID        5

/* #define VPP_PEAKING_NLP_1             0x1d93 */
#define HPEAK_SLOPE1_BIT       26
#define HPEAK_SLOPE1_WID        6
#define HPEAK_SLOPE2_BIT       20
#define HPEAK_SLOPE2_WID        6
#define HPEAK_THR1_BIT         12
#define HPEAK_THR1_WID          8
#define VPEAK_SLOPE1_BIT        6
#define VPEAK_SLOPE1_WID        6
#define VPEAK_SLOPE2_BIT        0
#define VPEAK_SLOPE2_WID        6

/* #define VPP_PEAKING_NLP_2             0x1d94 */
#define HPEAK_THR2_BIT         24
#define HPEAK_THR2_WID          8
#define HPEAK_NLP_COR_THR_BIT  16
#define HPEAK_NLP_COR_THR_WID   8
#define HPEAK_NLP_GAIN_POS_BIT  8
#define HPEAK_NLP_GAIN_POS_WID  8
#define HPEAK_NLP_GAIN_NEG_BIT  0
#define HPEAK_NLP_GAIN_NEG_WID  8

/* #define VPP_PEAKING_NLP_3             0x1d95 */
#define VPEAK_THR1_BIT         24
#define VPEAK_THR1_WID          8
#define SPEAK_SLOPE1_BIT       18
#define SPEAK_SLOPE1_WID        6
#define SPEAK_SLOPE2_BIT       12
#define SPEAK_SLOPE2_WID        6
#define SPEAK_THR1_BIT          4
#define SPEAK_THR1_WID          8
#define PEAK_COR_GAIN_BIT       0
#define PEAK_COR_GAIN_WID       4

/* #define VPP_PEAKING_NLP_4             0x1d96 */
#define VPEAK_THR2_BIT         24
#define VPEAK_THR2_WID          8
#define VPEAK_NLP_COR_THR_BIT  16
#define VPEAK_NLP_COR_THR_WID   8
#define VPEAK_NLP_GAIN_POS_BIT  8
#define VPEAK_NLP_GAIN_POS_WID  8
#define VPEAK_NLP_GAIN_NEG_BIT  0
#define VPEAK_NLP_GAIN_NEG_WID  8

/* #define VPP_PEAKING_NLP_5             0x1d97 */
#define SPEAK_THR2_BIT         24
#define SPEAK_THR2_WID          8
#define SPEAK_NLP_COR_THR_BIT  16
#define SPEAK_NLP_COR_THR_WID   8
#define SPEAK_NLP_GAIN_POS_BIT  8
#define SPEAK_NLP_GAIN_POS_WID  8
#define SPEAK_NLP_GAIN_NEG_BIT  0
#define SPEAK_NLP_GAIN_NEG_WID  8

/* #define VPP_HSVS_LIMIT                0x1d98 */
#define PEAK_COR_THR_L_BIT     24
#define PEAK_COR_THR_L_WID      8
#define PEAK_COR_THR_H_BIT     16
#define PEAK_COR_THR_H_WID      8
#define VLIMIT_COEF_H_BIT      12
#define VLIMIT_COEF_H_WID       4
#define VLIMIT_COEF_L_BIT       8
#define VLIMIT_COEF_L_WID       4
#define HLIMIT_COEF_H_BIT       4
#define HLIMIT_COEF_H_WID       4
#define HLIMIT_COEF_L_BIT       0
#define HLIMIT_COEF_L_WID       4

/* #define VPP_VLTI_CTRL                 0x1d99 */
#define VLTI_GAIN_NEG_BIT      24
#define VLTI_GAIN_NEG_WID       8
#define VLTI_GAIN_POS_BIT      16
#define VLTI_GAIN_POS_WID       8
#define VLTI_THR_BIT            8
#define VLTI_THR_WID            8
#define VLTI_BLEND_FACTOR_BIT   0
#define VLTI_BLEND_FACTOR_WID   8

/* #define VPP_HLTI_CTRL                 0x1d9a */
#define HLTI_GAIN_NEG_BIT      24
#define HLTI_GAIN_NEG_WID       8
#define HLTI_GAIN_POS_BIT      16
#define HLTI_GAIN_POS_WID       8
#define HLTI_THR_BIT            8
#define HLTI_THR_WID            8
#define HLTI_BLEND_FACTOR_BIT   0
#define HLTI_BLEND_FACTOR_WID   8

/* #define VPP_CTI_CTRL                  0x1d9b */
#define CTI_C444TO422_EN_BIT   30
#define CTI_C444TO422_EN_WID    1
/* 2'b00: no filter, 2'b01: (1, 0, 1), */
/* 2'b10: (1, 2, 1), 2'b11: (1, 2, 2, 2, 1), */
#define VCTI_FILTER_BIT        28
#define VCTI_FILTER_WID         2
#define CTI_C422TO444_EN_BIT   27
#define CTI_C422TO444_EN_WID    1
#define HCTI_STEP2_BIT         24
#define HCTI_STEP2_WID          3
#define HCTI_STEP_BIT          21
#define HCTI_STEP_WID           3
#define CTI_BLEND_FACTOR_BIT   16
#define CTI_BLEND_FACTOR_WID    5
#define HCTI_MODE_MEDIAN_BIT   15
#define HCTI_MODE_MEDIAN_WID    1
#define HCTI_THR_BIT            8
#define HCTI_THR_WID            7
#define HCTI_GAIN_BIT           0
#define HCTI_GAIN_WID           8

/* #define VPP_BLUE_STRETCH_1            0x1d9c */
#define BENH_CB_INC_BIT        29
#define BENH_CB_INC_WID         1
#define BENH_CR_INC_BIT        28
#define BENH_CR_INC_WID         1
#define BENH_ERR_CRP_INV_H_BIT 27
#define BENH_ERR_CRP_INV_H_WID  1
#define BENH_ERR_CRN_INV_H_BIT 26
#define BENH_ERR_CRN_INV_H_WID  1
#define BENH_ERR_CBP_INV_H_BIT 25
#define BENH_ERR_CBP_INV_H_WID  1
#define BENH_ERR_CBN_INV_H_BIT 24
#define BENH_ERR_CBN_INV_H_WID  1
#define BENH_GAIN_CR_BIT       16
#define BENH_GAIN_CR_WID        8
#define BENH_GAIN_CB4CR_BIT     8
#define BENH_GAIN_CB4CR_WID     8
#define BENH_LUMA_H_BIT         0
#define BENH_LUMA_H_WID         8

/* #define VPP_BLUE_STRETCH_2            0x1d9d */
#define BENH_ERR_CRP_BIT       27
#define BENH_ERR_CRP_WID        5
#define BENH_ERR_CRP_INV_L_BIT 16
#define BENH_ERR_CRP_INV_L_WID 11
#define BENH_ERR_CRN_BIT       11
#define BENH_ERR_CRN_WID        5
#define BENH_ERR_CRN_INV_L_BIT  0
#define BENH_ERR_CRN_INV_L_WID 11

/* #define VPP_BLUE_STRETCH_3            0x1d9e */
#define BENH_ERR_CBP_BIT       27
#define BENH_ERR_CBP_WID        5
#define BENH_ERR_CBP_INV_L_BIT 16
#define BENH_ERR_CBP_INV_L_WID 11
#define BENH_ERR_CBN_BIT       11
#define BENH_ERR_CBN_WID        5
#define BENH_ERR_CBN_INV_L_BIT  0
#define BENH_ERR_CBN_INV_L_WID 11

/* #define VPP_CCORING_CTRL              0x1da0 */
#define CCOR_THR_BIT            8
#define CCOR_THR_WID            8
#define CCOR_SLOPE_BIT          0
#define CCOR_SLOPE_WID          4

/* #define VPP_VE_ENABLE_CTRL            0x1da1 */
#define DEMO_CCOR_BIT          20
#define DEMO_CCOR_WID           1
#define DEMO_BEXT_BIT          19
#define DEMO_BEXT_WID           1
#define DEMO_DNLP_BIT          18
#define DEMO_DNLP_WID           1
#define DEMO_HSVS_BIT          17
#define DEMO_HSVS_WID           1
#define DEMO_BENH_BIT          16
#define DEMO_BENH_WID           1
#if defined(CONFIG_ARCH_MESON)
/* 1'b0: demo adjust on right, 1'b1: demo adjust on left */
#define VE_DEMO_POS_BIT           15
#define VE_DEMO_POS_WID            1
#elif defined(CONFIG_ARCH_MESON2)
/* 2'b00: demo adjust on top, 2'b01: demo adjust on bottom, */
/* 2'b10: demo adjust on left, 2'b11: demo adjust on right */
#define VE_DEMO_POS_BIT        14
#define VE_DEMO_POS_WID         2
#endif
#define CCOR_EN_BIT             4
#define CCOR_EN_WID             1
#define BEXT_EN_BIT             3
#define BEXT_EN_WID             1
#define DNLP_EN_BIT             2
#define DNLP_EN_WID             1
#define HSVS_EN_BIT             1
#define HSVS_EN_WID             1
#define BENH_EN_BIT             0
#define BENH_EN_WID             1

#if defined(CONFIG_ARCH_MESON)
/* #define VPP_VE_DEMO_LEFT_SCREEN_WIDTH 0x1da2 */
#elif defined(CONFIG_ARCH_MESON2)
/* #define VPP_VE_DEMO_LEFT_TOP_SCREEN_WIDTH 0x1da2 */
#endif
#define VE_DEMO_WID_BIT         0
#define VE_DEMO_WID_WID        12

#if defined(CONFIG_ARCH_MESON2)
/* #define VPP_VE_DEMO_CENTER_BAR              0x1da3 */
#define VE_CBAR_EN_BIT         31  /* center bar enable */
#define VE_CBAR_EN_WID          1
#define VE_CBAR_WID_BIT        24  /* center bar width    (*2) */
#define VE_CBAR_WID_WID         4
#define VE_CBAR_CR_BIT         16  /* center bar Cr       (*4) */
#define VE_CBAR_CR_WID          8
#define VE_CBAR_CB_BIT          8  /* center bar Cb       (*4) */
#define VE_CBAR_CB_WID          8
#define VE_CBAR_Y_BIT           0  /* center bar y        (*4) */
#define VE_CBAR_Y_WID           8
#endif

#if defined(CONFIG_ARCH_MESON2)
 /* reset bit, high active */
#define VDO_MEAS_RST_BIT       10
#define VDO_MEAS_RST_WID        1
/* 0: rising edge, 1: falling edge */
#define VDO_MEAS_EDGE_BIT       9
#define VDO_MEAS_EDGE_WID       1
/* 1: accumulate the counter number, 0: not */
#define VDO_MEAS_ACCUM_CNT_BIT  8
#define VDO_MEAS_ACCUM_CNT_WID  1
 /* how many vsync span need to measure */
#define VDO_MEAS_VS_SPAN_BIT    0
#define VDO_MEAS_VS_SPAN_WID    8
#endif

/* #if defined(CONFIG_ARCH_MESON2) */
/* #define VPP_VDO_MEAS_VS_COUNT_HI            0x1da9  //Read only */
/* every number of sync_span vsyncs, this counter add 1 */
#define VDO_IND_MEAS_CNT_N_BIT 16
#define VDO_IND_MEAS_CNT_N_WID  4
 /* high bit portion of counter */
#define VDO_MEAS_VS_CNT_HI_BIT  0
#define VDO_MEAS_VS_CNT_HI_WID 16
/* #endif */

/* #if defined(CONFIG_ARCH_MESON2) */
/* #define VPP_VDO_MEAS_VS_COUNT_LO            0x1daa  //Read only */
/* low bit portion of counter */
#define VDO_MEAS_VS_CNT_LO_BIT  0
#define VDO_MEAS_VS_CNT_LO_WID 32
/* #endif */
/* bit 15:8  peaking_factor */
/* bit 5     peaking_dnlp_demo_en */
/* bit 4     peaking_dnlp_en */
/* bit 3:0   peaking_filter_sel */
/* #define VPP_PEAKING_DNLP                            0x1db8 */
#define PEAKING_FACTOR_BIT          8
#define PEAKING_FACTOR_WID          8
#define PEAKING_DNLP_DEMO_EN_BIT    5
#define PEAKING_DNLP_DEMO_EN_WID    1
#define PEAKING_DNLP_EN_BIT         4
#define PEAKING_DNLP_EN_WID         1
#define PEAKING_FILTER_SEL_BIT      0
#define PEAKING_FILTER_SEL_WID      4

#define VPP_VE_H_V_SIZE             0x1da4

#define SRSHARP0_DNLP_00					0x3246
#define SRSHARP0_DNLP_01					0x3247
#define SRSHARP0_DNLP_02					0x3248
#define SRSHARP0_DNLP_03					0x3249
#define SRSHARP0_DNLP_04					0x324a
#define SRSHARP0_DNLP_05					0x324b
#define SRSHARP0_DNLP_06					0x324c
#define SRSHARP0_DNLP_07					0x324d
#define SRSHARP0_DNLP_08					0x324e
#define SRSHARP0_DNLP_09					0x324f
#define SRSHARP0_DNLP_10					0x3250
#define SRSHARP0_DNLP_11					0x3251
#define SRSHARP0_DNLP_12					0x3252
#define SRSHARP0_DNLP_13					0x3253
#define SRSHARP0_DNLP_14					0x3254
#define SRSHARP0_DNLP_15					0x3255

#define SRSHARP1_DNLP_00					0x32c6
#define SRSHARP1_DNLP_01					0x32c7
#define SRSHARP1_DNLP_02					0x32c8
#define SRSHARP1_DNLP_03					0x32c9
#define SRSHARP1_DNLP_04					0x32ca
#define SRSHARP1_DNLP_05					0x32cb
#define SRSHARP1_DNLP_06					0x32cc
#define SRSHARP1_DNLP_07					0x32cd
#define SRSHARP1_DNLP_08					0x32ce
#define SRSHARP1_DNLP_09					0x32cf
#define SRSHARP1_DNLP_10					0x32d0
#define SRSHARP1_DNLP_11					0x32d1
#define SRSHARP1_DNLP_12					0x32d2
#define SRSHARP1_DNLP_13					0x32d3
#define SRSHARP1_DNLP_14					0x32d4
#define SRSHARP1_DNLP_15					0x32d5

/* after G12a add more nodes for dnlp */
#define SRSHARP0_DNLP2_00     0x3e90
#define SRSHARP0_DNLP2_01     0x3e91
#define SRSHARP0_DNLP2_02     0x3e92
#define SRSHARP0_DNLP2_03     0x3e93
#define SRSHARP0_DNLP2_04     0x3e94
#define SRSHARP0_DNLP2_05     0x3e95
#define SRSHARP0_DNLP2_06     0x3e96
#define SRSHARP0_DNLP2_07     0x3e97
#define SRSHARP0_DNLP2_08     0x3e98
#define SRSHARP0_DNLP2_09     0x3e99
#define SRSHARP0_DNLP2_10     0x3e9a
#define SRSHARP0_DNLP2_11     0x3e9b
#define SRSHARP0_DNLP2_12     0x3e9c
#define SRSHARP0_DNLP2_13     0x3e9d
#define SRSHARP0_DNLP2_14     0x3e9e
#define SRSHARP0_DNLP2_15     0x3e9f
#define SRSHARP0_DNLP2_16     0x3ea0
#define SRSHARP0_DNLP2_17     0x3ea1
#define SRSHARP0_DNLP2_18     0x3ea2
#define SRSHARP0_DNLP2_19     0x3ea3
#define SRSHARP0_DNLP2_20     0x3ea4
#define SRSHARP0_DNLP2_21     0x3ea5
#define SRSHARP0_DNLP2_22     0x3ea6
#define SRSHARP0_DNLP2_23     0x3ea7
#define SRSHARP0_DNLP2_24     0x3ea8
#define SRSHARP0_DNLP2_25     0x3ea9
#define SRSHARP0_DNLP2_26     0x3eaa
#define SRSHARP0_DNLP2_27     0x3eab
#define SRSHARP0_DNLP2_28     0x3eac
#define SRSHARP0_DNLP2_29     0x3ead
#define SRSHARP0_DNLP2_30     0x3eae
#define SRSHARP0_DNLP2_31     0x3eaf

#define SRSHARP1_DNLP2_00     0x3f90
#define SRSHARP1_DNLP2_01     0x3f91
#define SRSHARP1_DNLP2_02     0x3f92
#define SRSHARP1_DNLP2_03     0x3f93
#define SRSHARP1_DNLP2_04     0x3f94
#define SRSHARP1_DNLP2_05     0x3f95
#define SRSHARP1_DNLP2_06     0x3f96
#define SRSHARP1_DNLP2_07     0x3f97
#define SRSHARP1_DNLP2_08     0x3f98
#define SRSHARP1_DNLP2_09     0x3f99
#define SRSHARP1_DNLP2_10     0x3f9a
#define SRSHARP1_DNLP2_11     0x3f9b
#define SRSHARP1_DNLP2_12     0x3f9c
#define SRSHARP1_DNLP2_13     0x3f9d
#define SRSHARP1_DNLP2_14     0x3f9e
#define SRSHARP1_DNLP2_15     0x3f9f
#define SRSHARP1_DNLP2_16     0x3fa0
#define SRSHARP1_DNLP2_17     0x3fa1
#define SRSHARP1_DNLP2_18     0x3fa2
#define SRSHARP1_DNLP2_19     0x3fa3
#define SRSHARP1_DNLP2_20     0x3fa4
#define SRSHARP1_DNLP2_21     0x3fa5
#define SRSHARP1_DNLP2_22     0x3fa6
#define SRSHARP1_DNLP2_23     0x3fa7
#define SRSHARP1_DNLP2_24     0x3fa8
#define SRSHARP1_DNLP2_25     0x3fa9
#define SRSHARP1_DNLP2_26     0x3faa
#define SRSHARP1_DNLP2_27     0x3fab
#define SRSHARP1_DNLP2_28     0x3fac
#define SRSHARP1_DNLP2_29     0x3fad
#define SRSHARP1_DNLP2_30     0x3fae
#define SRSHARP1_DNLP2_31     0x3faf

#endif /* _VE_REG_H */
