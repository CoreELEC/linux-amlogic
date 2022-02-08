/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_VPU_OSD_MIF_H_
#define _MESON_VPU_OSD_MIF_H_

#define HW_OSD_MIF_NUM 4
#define MAX_HOLD_LINE     0x1f
#define MIN_HOLD_LINE     0x04
#define VIU1_DEFAULT_HOLD_LINE  0x08
#define VIU2_DEFAULT_HOLD_LINE  0x04

#define VIU_OSD1_CTRL_STAT 0x1a10
#define VIU_OSD1_CTRL_STAT2 0x1a2d
#define VIU_OSD1_COLOR_ADDR 0x1a11
#define VIU_OSD1_COLOR 0x1a12
#define VIU_OSD1_TCOLOR_AG0 0x1a17
#define VIU_OSD1_TCOLOR_AG1 0x1a18
#define VIU_OSD1_TCOLOR_AG2 0x1a19
#define VIU_OSD1_TCOLOR_AG3 0x1a1a
#define VIU_OSD1_BLK0_CFG_W0 0x1a1b
#define VIU_OSD1_BLK1_CFG_W0 0x1a1f
#define VIU_OSD1_BLK2_CFG_W0 0x1a23
#define VIU_OSD1_BLK3_CFG_W0 0x1a27
#define VIU_OSD1_BLK0_CFG_W1 0x1a1c
#define VIU_OSD1_BLK1_CFG_W1 0x1a20
#define VIU_OSD1_BLK2_CFG_W1 0x1a24
#define VIU_OSD1_BLK3_CFG_W1 0x1a28
#define VIU_OSD1_BLK0_CFG_W2 0x1a1d
#define VIU_OSD1_BLK1_CFG_W2 0x1a21
#define VIU_OSD1_BLK2_CFG_W2 0x1a25
#define VIU_OSD1_BLK3_CFG_W2 0x1a29
#define VIU_OSD1_BLK0_CFG_W3 0x1a1e
#define VIU_OSD1_BLK1_CFG_W3 0x1a22
#define VIU_OSD1_BLK2_CFG_W3 0x1a26
#define VIU_OSD1_BLK3_CFG_W3 0x1a2a
#define VIU_OSD1_BLK0_CFG_W4 0x1a13
#define VIU_OSD1_BLK1_CFG_W4 0x1a14
#define VIU_OSD1_BLK2_CFG_W4 0x1a15
#define VIU_OSD1_BLK3_CFG_W4 0x1a16
#define VIU_OSD1_FIFO_CTRL_STAT 0x1a2b
#define VIU_OSD1_TEST_RDDATA 0x1a2c
#define VIU_OSD1_PROT_CTRL 0x1a2e
#define VIU_OSD1_MALI_UNPACK_CTRL 0x1a2f
#define VIU_OSD1_DIMM_CTRL 0x1adf

#define VIU_OSD2_CTRL_STAT 0x1a30
#define VIU_OSD2_CTRL_STAT2 0x1a4d
#define VIU_OSD2_COLOR_ADDR 0x1a31
#define VIU_OSD2_COLOR 0x1a32
#define VIU_OSD2_HL1_H_START_END 0x1a33
#define VIU_OSD2_HL1_V_START_END 0x1a34
#define VIU_OSD2_HL2_H_START_END 0x1a35
#define VIU_OSD2_HL2_V_START_END 0x1a36
#define VIU_OSD2_TCOLOR_AG0 0x1a37
#define VIU_OSD2_TCOLOR_AG1 0x1a38
#define VIU_OSD2_TCOLOR_AG2 0x1a39
#define VIU_OSD2_TCOLOR_AG3 0x1a3a
#define VIU_OSD2_BLK0_CFG_W0 0x1a3b
#define VIU_OSD2_BLK1_CFG_W0 0x1a3f
#define VIU_OSD2_BLK2_CFG_W0 0x1a43
#define VIU_OSD2_BLK3_CFG_W0 0x1a47
#define VIU_OSD2_BLK0_CFG_W1 0x1a3c
#define VIU_OSD2_BLK1_CFG_W1 0x1a40
#define VIU_OSD2_BLK2_CFG_W1 0x1a44
#define VIU_OSD2_BLK3_CFG_W1 0x1a48
#define VIU_OSD2_BLK0_CFG_W2 0x1a3d
#define VIU_OSD2_BLK1_CFG_W2 0x1a41
#define VIU_OSD2_BLK2_CFG_W2 0x1a45
#define VIU_OSD2_BLK3_CFG_W2 0x1a49
#define VIU_OSD2_BLK0_CFG_W3 0x1a3e
#define VIU_OSD2_BLK1_CFG_W3 0x1a42
#define VIU_OSD2_BLK2_CFG_W3 0x1a46
#define VIU_OSD2_BLK3_CFG_W3 0x1a4a
#define VIU_OSD2_BLK0_CFG_W4 0x1a64
#define VIU_OSD2_BLK1_CFG_W4 0x1a65
#define VIU_OSD2_BLK2_CFG_W4 0x1a66
#define VIU_OSD2_BLK3_CFG_W4 0x1a67
#define VIU_OSD2_FIFO_CTRL_STAT 0x1a4b
#define VIU_OSD2_TEST_RDDATA 0x1a4c
#define VIU_OSD2_PROT_CTRL 0x1a4e
#define VIU_OSD2_MALI_UNPACK_CTRL 0x1abd
#define VIU_OSD2_DIMM_CTRL 0x1acf

#define VIU_OSD3_CTRL_STAT 0x3d80
#define VIU_OSD3_CTRL_STAT2 0x3d81
#define VIU_OSD3_COLOR_ADDR 0x3d82
#define VIU_OSD3_COLOR 0x3d83
#define VIU_OSD3_TCOLOR_AG0 0x3d84
#define VIU_OSD3_TCOLOR_AG1 0x3d85
#define VIU_OSD3_TCOLOR_AG2 0x3d86
#define VIU_OSD3_TCOLOR_AG3 0x3d87
#define VIU_OSD3_BLK0_CFG_W0 0x3d88
#define VIU_OSD3_BLK0_CFG_W1 0x3d8c
#define VIU_OSD3_BLK0_CFG_W2 0x3d90
#define VIU_OSD3_BLK0_CFG_W3 0x3d94
#define VIU_OSD3_BLK0_CFG_W4 0x3d98
#define VIU_OSD3_BLK1_CFG_W4 0x3d99
#define VIU_OSD3_BLK2_CFG_W4 0x3d9a
#define VIU_OSD3_FIFO_CTRL_STAT 0x3d9c
#define VIU_OSD3_TEST_RDDATA 0x3d9d
#define VIU_OSD3_PROT_CTRL 0x3d9e
#define VIU_OSD3_MALI_UNPACK_CTRL 0x3d9f
#define VIU_OSD3_DIMM_CTRL 0x3da0

#define VIU_OSD3_MATRIX_COEF00_01 0x3db0
#define VIU_OSD3_MATRIX_COEF02_10 0x3db1
#define VIU_OSD3_MATRIX_COEF11_12 0x3db2
#define VIU_OSD3_MATRIX_COEF20_21 0x3db3
#define VIU_OSD3_MATRIX_COEF22 0x3db4
#define VIU_OSD3_MATRIX_COEF13_14 0x3db5
#define VIU_OSD3_MATRIX_COEF23_24 0x3db6
#define VIU_OSD3_MATRIX_COEF15_25 0x3db7
#define VIU_OSD3_MATRIX_CLIP 0x3db8
#define VIU_OSD3_MATRIX_OFFSET0_1 0x3db9
#define VIU_OSD3_MATRIX_OFFSET2 0x3dba
#define VIU_OSD3_MATRIX_PRE_OFFSET0_1 0x3dbb
#define VIU_OSD3_MATRIX_PRE_OFFSET2 0x3dbc
#define VIU_OSD3_MATRIX_EN_CTRL 0x3dbd

#define VIU_OSD4_CTRL_STAT 0x3dc0
#define VIU_OSD4_CTRL_STAT2 0x3dc1
#define VIU_OSD4_COLOR_ADDR 0x3dc2
#define VIU_OSD4_COLOR 0x3dc3
#define VIU_OSD4_TCOLOR_AG0 0x3dc4
#define VIU_OSD4_TCOLOR_AG1 0x3dc5
#define VIU_OSD4_TCOLOR_AG2 0x3dc6
#define VIU_OSD4_TCOLOR_AG3 0x3dc7
#define VIU_OSD4_BLK0_CFG_W0 0x3dc8
#define VIU_OSD4_BLK0_CFG_W1 0x3dcc
#define VIU_OSD4_BLK0_CFG_W2 0x3dd0
#define VIU_OSD4_BLK0_CFG_W3 0x3dd4
#define VIU_OSD4_BLK0_CFG_W4 0x3dd8
#define VIU_OSD4_BLK1_CFG_W4 0x3dd9
#define VIU_OSD4_BLK2_CFG_W4 0x3dda
#define VIU_OSD4_FIFO_CTRL_STAT 0x3ddc
#define VIU_OSD4_TEST_RDDATA 0x3ddd
#define VIU_OSD4_PROT_CTRL 0x3dde
#define VIU_OSD4_MALI_UNPACK_CTRL 0x3ddf
#define VIU_OSD4_DIMM_CTRL 0x3de0

#define VIU_OSD4_MATRIX_COEF00_01 0x3df0
#define VIU_OSD4_MATRIX_COEF02_10 0x3df1
#define VIU_OSD4_MATRIX_COEF11_12 0x3df2
#define VIU_OSD4_MATRIX_COEF20_21 0x3df3
#define VIU_OSD4_MATRIX_COEF22 0x3df4
#define VIU_OSD4_MATRIX_COEF13_14 0x3df5
#define VIU_OSD4_MATRIX_COEF23_24 0x3df6
#define VIU_OSD4_MATRIX_COEF15_25 0x3df7
#define VIU_OSD4_MATRIX_CLIP 0x3df8
#define VIU_OSD4_MATRIX_OFFSET0_1 0x3df9
#define VIU_OSD4_MATRIX_OFFSET2 0x3dfa
#define VIU_OSD4_MATRIX_PRE_OFFSET0_1 0x3dfb
#define VIU_OSD4_MATRIX_PRE_OFFSET2 0x3dfc
#define VIU_OSD4_MATRIX_EN_CTRL 0x3dfd

struct osd_mif_reg_s {
	u32 viu_osd_ctrl_stat;
	u32 viu_osd_ctrl_stat2;
	u32 viu_osd_color_addr;
	u32 viu_osd_color;
	u32 viu_osd_tcolor_ag0;
	u32 viu_osd_tcolor_ag1;
	u32 viu_osd_tcolor_ag2;
	u32 viu_osd_tcolor_ag3;
	u32 viu_osd_blk0_cfg_w0;
	u32 viu_osd_blk0_cfg_w1;
	u32 viu_osd_blk0_cfg_w2;
	u32 viu_osd_blk0_cfg_w3;
	u32 viu_osd_blk0_cfg_w4;
	u32 viu_osd_blk1_cfg_w4;
	u32 viu_osd_blk2_cfg_w4;
	u32 viu_osd_fifo_ctrl_stat;
	u32 viu_osd_test_rddata;
	u32 viu_osd_prot_ctrl;
	u32 viu_osd_mali_unpack_ctrl;
	u32 viu_osd_dimm_ctrl;
};

/**
 * struct meson_drm_format_info - information about a DRM format
 * @format: 4CC format identifier (DRM_FORMAT_*)
 * @hw_blkmode: Define the OSD block’s input pixel format
 * @hw_colormat: Applicable only to 16-bit color mode (OSD_BLK_MODE=4),
 *	32-bit mode (OSD_BLK_MODE=5) and 24-bit mode (OSD_BLK_MODE=7),
 *	defines the bit-field allocation of the pixel data.
 */
struct meson_drm_format_info {
	u32 format;
	u8 hw_blkmode;
	u8 hw_colormat;
	u8 alpha_replace;
};

enum osd_blk_mode_e {
	BLOCK_MODE_2BIT_PER_PIXEL = 0,
	BLOCK_MODE_4BIT_PER_PIXEL,
	BLOCK_MODE_8BIT_PER_PIXEL,
	BLOCK_MODE_422,
	BLOCK_MODE_16BIT,
	BLOCK_MODE_32BIT,
	BLOCK_MODE_24BIT = 7,
	/*bellow is for afbc mode*/
	BLOCK_MODE_R8 = 0,
	BLOCK_MODE_YUV422_8BIT,
	BLOCK_MODE_RGB565,
	BLOCK_MODE_RGBA5551,
	BLOCK_MODE_RGBA4444,
	BLOCK_MODE_RGBA8888,
	BLOCK_MODE_RGB888 = 7,
	BLOCK_MODE_YUV422_10BIT,
	BLOCK_MODE_RGBA1010102,
};

enum osd_color_matrix_e {
	/*for 16bit:blk-mode=BLOCK_MODE_16BIT*/
	COLOR_MATRIX_655 = 0,
	COLOR_MATRIX_844,
	COLOR_MATRIX_565 = 4,
	/*for 32bit:blk-mode=BLOCK_MODE_32BIT*/
	COLOR_MATRIX_RGBA8888 = 0,
	COLOR_MATRIX_ARGB8888,
	COLOR_MATRIX_ABGR8888,
	COLOR_MATRIX_BGRA8888,
	/*for 24bit:blk-mode=BLOCK_MODE_24BIT*/
	COLOR_MATRIX_RGB888 = 0,
	COLOR_MATRIX_BGR888 = 5,
};

const struct meson_drm_format_info *meson_drm_format_info(u32 format,
							  bool afbc_en);
#endif
