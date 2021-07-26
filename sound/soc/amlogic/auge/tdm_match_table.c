/*
 * sound/soc/amlogic/auge/tdm_match_table.c
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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

/* For OE function V1:
 * OE is set by EE_AUDIO_TDMOUT_A_CTRL0 & EE_AUDIO_TDMOUT_A_CTRL1
 */
#define OE_FUNCTION_V1 1
/* For OE function V2:
 * OE is set by EE_AUDIO_TDMOUT_A_CTRL2
 */
#define OE_FUNCTION_V2 2

struct src_table {
	char name[32];
	unsigned int val;
};

unsigned int get_tdmin_src(struct src_table *table, char *src)
{
	for (; table->name[0]; table++) {
		if (strncmp(table->name, src, strlen(src)) == 0)
			return table->val;
	}

	return 0;
}

struct tdm_chipinfo {
	/* device id */
	unsigned int id;

	/* lane max count */
	unsigned int lane_cnt;

	/* no eco, sclk_ws_inv for out */
	bool sclk_ws_inv;

	/* output en (oe) for pinmux */
	unsigned int oe_fn;

	/* clk pad */
	bool no_mclkpad_ctrl;

	/* same source */
	bool same_src_fn;


	/* ACODEC_ADC function */
	bool adc_fn;

	/* mclk pad offset */
	bool mclkpad_no_offset;

	/* offset config for SW_RESET in reg.h */
	int reset_reg_offset;

	/* async fifo */
	bool async_fifo;

	/* from tm2_revb */
	bool separate_tohdmitx_en;

	/* only for A113D */
	bool reset_tdmin;

	struct src_table *tdmin_srcs;

	/* only for t5&t5d*/
	bool need_mute_tdm;
};

#define SRC_TDMIN_A     "tdmin_a"
#define SRC_TDMIN_B     "tdmin_b"
#define SRC_TDMIN_C     "tdmin_c"
#define SRC_TDMIND_A    "tdmind_a"
#define SRC_TDMIND_B    "tdmind_b"
#define SRC_TDMIND_C    "tdmind_c"
#define SRC_HDMIRX      "hdmirx"
#define SRC_ACODEC      "acodec_adc"
#define SRC_TDMOUT_A    "tdmout_a"
#define SRC_TDMOUT_B    "tdmout_b"
#define SRC_TDMOUT_C    "tdmout_c"

#define TDMIN_SRC_CONFIG(_name, _val) \
{	.name = (_name), .val = (_val)}

struct src_table tdmin_srcs_v1[] = {
	TDMIN_SRC_CONFIG(SRC_TDMIN_A, 0),
	TDMIN_SRC_CONFIG(SRC_TDMIN_B, 1),
	TDMIN_SRC_CONFIG(SRC_TDMIN_C, 2),
	TDMIN_SRC_CONFIG(SRC_TDMIND_A, 3),
	TDMIN_SRC_CONFIG(SRC_TDMIND_B, 4),
	TDMIN_SRC_CONFIG(SRC_TDMIND_C, 5),
	TDMIN_SRC_CONFIG(SRC_HDMIRX, 6),
	TDMIN_SRC_CONFIG(SRC_ACODEC, 7),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_A, 13),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_B, 14),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_B, 15),
	{ /* sentinel */ }
};

/* t5 afterwards */
struct src_table tdmin_srcs_v2[] = {
	TDMIN_SRC_CONFIG(SRC_TDMIN_A, 0),
	TDMIN_SRC_CONFIG(SRC_TDMIN_B, 1),
	TDMIN_SRC_CONFIG(SRC_TDMIN_C, 2),
	TDMIN_SRC_CONFIG(SRC_HDMIRX, 4),
	TDMIN_SRC_CONFIG(SRC_ACODEC, 5),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_A, 12),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_B, 13),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_B, 14),
	{ /* sentinel */ }
};

struct tdm_chipinfo axg_tdma_chipinfo = {
	.id          = TDM_A,
	.no_mclkpad_ctrl = true,
	.reset_tdmin = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo axg_tdmb_chipinfo = {
	.id          = TDM_B,
	.no_mclkpad_ctrl = true,
	.reset_tdmin = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo axg_tdmc_chipinfo = {
	.id          = TDM_C,
	.no_mclkpad_ctrl = true,
	.reset_tdmin = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo g12a_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = true,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo g12a_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo g12a_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo tl1_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.adc_fn      = true,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo tl1_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.adc_fn      = true,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo tl1_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.adc_fn      = true,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo sm1_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX0,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo sm1_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo sm1_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo tm2_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo tm2_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo tm2_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo tm2_revb_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo tm2_revb_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo tm2_revb_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

static const struct of_device_id aml_tdm_device_id[] = {
	{
		.compatible = "amlogic, axg-snd-tdma",
		.data       = &axg_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, axg-snd-tdmb",
		.data       = &axg_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, axg-snd-tdmc",
		.data       = &axg_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-tdma",
		.data       = &g12a_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-tdmb",
		.data       = &g12a_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-tdmc",
		.data       = &g12a_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-tdma",
		.data       = &tl1_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-tdmb",
		.data       = &tl1_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-tdmc",
		.data       = &tl1_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdma",
		.data       = &sm1_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdmb",
		.data       = &sm1_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdmc",
		.data       = &sm1_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-tdma",
		.data       = &tm2_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-tdmb",
		.data       = &tm2_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-tdmc",
		.data       = &tm2_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-snd-tdma",
		.data       = &tm2_revb_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-snd-tdmb",
		.data       = &tm2_revb_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-snd-tdmc",
		.data       = &tm2_revb_tdmc_chipinfo,
	},
	{}
};
MODULE_DEVICE_TABLE(of, aml_tdm_device_id);
