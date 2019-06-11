/*
 * drivers/amlogic/drm/vpu-hw/meson_osd_afbc.c
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

#include "meson_vpu_pipeline.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"
#include "osd.h"

/* osd_mafbc_irq_clear& irq_mask */
#define OSD_MAFBC_SURFACES_COMPLETED		BIT(0)
#define OSD_MAFBC_CONFI_SWAPPED				BIT(1)
#define OSD_MAFBC_DECODE_ERROR				BIT(2)
#define OSD_MAFBC_DETILING_ERROR			BIT(3)
#define OSD_MAFBC_AXI_ERROR					BIT(4)
#define OSD_MAFBC_SECURE_ID_ERROR			BIT(5)

/* osd_mafbc_command */
#define OSD_MAFBC_DIRECT_SWAP
#define OSD_MAFBC_PENDING_SWAP

/* osd_mafbc_surface_cfg */
#define OSD_MAFBC_S0_ENABLE					BIT(0)
#define OSD_MAFBC_S1_ENABLE					BIT(1)
#define OSD_MAFBC_S2_ENABLE					BIT(2)
#define OSD_MAFBC_S3_ENABLE					BIT(3)
#define OSD_MAFBC_DECODE_ENABLE				BIT(16)

/* osd_mafbc_axi_cfg */
#define OSD_MAFBC_AXI_QOS(val)		FIELD_PREP(GENMASK(3, 0), val)
#define OSD_MAFBC_AXI_CACHE(val)	FIELD_PREP(GENMASK(7, 4), val)

/* osd_mafbc_format_specifier */
#define OSD_MAFBC_PIXEL_FORMAT(val)	FIELD_PREP(GENMASK(3, 0), val)
#define OSD_MAFBC_YUV_TRANSFORM		BIT(8)
#define OSD_MAFBC_BLOCK_SPLIT		BIT(9)
#define OSD_MAFBC_SUPER_BLOCK_ASPECT(val) \
		FIELD_PREP(GENMASK(17, 16), val)
#define OSD_MAFBC_TILED_HEADER_EN	BIT(18)
#define OSD_MAFBC_PAYLOAD_LIMIT_EN	BIT(19)

/* osd_mafbc_prefetch_cfg */
#define OSD_MAFBC_PREFETCH_READ_DIR_X	BIT(0)
#define OSD_MAFBC_PREFETCH_READ_DIR_Y	BIT(1)

static int afbc_pix_format(u32 fmt_mode)
{
	u32 pix_format = RGBA8888;

	switch (fmt_mode) {
	case COLOR_INDEX_YUV_422:
		pix_format = YUV422_8B;
		break;
	case COLOR_INDEX_16_565:
		pix_format = RGB565;
		break;
	case COLOR_INDEX_16_1555_A:
		pix_format = RGBA5551;
		break;
	case COLOR_INDEX_16_4444_R:
	case COLOR_INDEX_16_4444_A:
		pix_format = RGBA4444;
		break;
	case COLOR_INDEX_32_BGRX:
	case COLOR_INDEX_32_XBGR:
	case COLOR_INDEX_32_RGBX:
	case COLOR_INDEX_32_XRGB:
	case COLOR_INDEX_32_BGRA:
	case COLOR_INDEX_32_ABGR:
	case COLOR_INDEX_32_RGBA:
	case COLOR_INDEX_32_ARGB:
		pix_format = RGBA8888;
		break;
	case COLOR_INDEX_24_888_B:
	case COLOR_INDEX_24_RGB:
		pix_format = RGB888;
		break;
	case COLOR_INDEX_RGBA_1010102:
		pix_format = RGBA1010102;
		break;
	default:
		osd_log_err("unsupport fmt:%x\n", fmt_mode);
		break;
	}
	return pix_format;
}

static u32 line_stride_calc_afbc(
		u32 fmt_mode,
		u32 hsize,
		u32 stride_align_32bytes)
{
	u32 line_stride = 0;

	switch (fmt_mode) {
	case R8:
		line_stride = ((hsize << 3) + 127) >> 7;
		break;
	case YUV422_8B:
	case RGB565:
	case RGBA5551:
	case RGBA4444:
		line_stride = ((hsize << 4) + 127) >> 7;
		break;
	case RGBA8888:
	case RGB888:
	case YUV422_10B:
	case RGBA1010102:
		line_stride = ((hsize << 5) + 127) >> 7;
		break;
	}
	/* need wr ddr is 32bytes aligned */
	if (stride_align_32bytes)
		line_stride = ((line_stride+1) >> 1) << 1;
	else
		line_stride = line_stride;
	return line_stride;
}

static void osd_afbc_enable(u32 osd_index, bool flag)
{
	if (flag) {

		VSYNCOSD_WR_MPEG_REG_BITS(
				VPU_MAFBC_SURFACE_CFG,
				1, osd_index, 1);
		VSYNCOSD_WR_MPEG_REG(
				VPU_MAFBC_IRQ_MASK, 0xf);
	} else
		VSYNCOSD_WR_MPEG_REG_BITS(
				VPU_MAFBC_SURFACE_CFG,
				0, osd_index, 1);
}

static int afbc_check_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);

	//vpu_block_check_input(vblk, state, mvps);

	if (state->checked)
		return 0;

	state->checked = true;

	DRM_DEBUG("%s check_state called.\n", afbc->base.name);

	return 0;
}

static void osd1_afbc_set_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
	u32 pixel_format, line_stride, output_stride;
	u32 plane_index, osd_index;
	u64 header_addr, out_addr;
	u32 aligned_32, afbc_color_reorder;
	unsigned int depth;
	int bpp;
	struct meson_vpu_afbc *afbc;
	struct meson_vpu_afbc_state *afbc_state;
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_pipeline_state *pipeline_state;

	afbc = to_afbc_block(vblk);
	afbc_state = to_afbc_state(state);
	pipeline = vblk->pipeline;
	osd_index = vblk->index;
	pipeline_state = priv_to_pipeline_state(pipeline->obj.state);
	plane_index = pipeline_state->ratio_plane_index[osd_index];
	plane_info = &pipeline_state->plane_info[plane_index];

	if (!plane_info->afbc_en)
		return;

	osd_afbc_enable(0, 1);

	aligned_32 = 1;
	afbc_color_reorder = 0x1234;
	pixel_format = afbc_pix_format(plane_info->pixel_format);
	drm_fb_get_bpp_depth(plane_info->pixel_format, &depth, &bpp);
	header_addr = plane_info->phy_addr;

	line_stride = line_stride_calc_afbc(pixel_format,
		plane_info->src_w, aligned_32);

	output_stride = plane_info->src_w * bpp;

	header_addr = plane_info->phy_addr;
	out_addr = ((u64)(vblk->index + 1)) << 24;

	/* set osd path misc ctrl */
	VSYNCOSD_WR_MPEG_REG_BITS(OSD_PATH_MISC_CTRL, 0x1, (osd_index + 4), 1);

	/* set linear addr */
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_CTRL_STAT, 0x1, 2, 1);
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_CTRL_STAT2, 1, 1, 1);

	/* set read from mali */
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_BLK0_CFG_W0, 0x1, 30, 1);
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_BLK0_CFG_W0, 0, 15, 1);

	/* set line_stride */
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_BLK2_CFG_W4, line_stride, 0, 12);

	/* set frame addr */
	VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK1_CFG_W4, out_addr & 0xffffffff);

	/* set afbc color reorder and mali src*/
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_MALI_UNPACK_CTRL,
			afbc_color_reorder, 0, 16);
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_MALI_UNPACK_CTRL, 0x1, 31, 1);

	/* set header addr */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0,
			header_addr & 0xffffffff);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S0,
			(header_addr >> 32) & 0xffffffff);

	/* set format specifier */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_FORMAT_SPECIFIER_S0,
		plane_info->afbc_inter_format | (pixel_format & 0x0f));

	/* set pic size */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BUFFER_WIDTH_S0, plane_info->src_w);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BUFFER_HEIGHT_S0, plane_info->src_h);

	/* set buf stride */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_OUTPUT_BUF_STRIDE_S0, output_stride);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S0,
			out_addr & 0xffffffff);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S0,
			(out_addr >> 32) & 0xffffffff);

	/* set bounding box */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_X_START_S0,
			plane_info->src_x);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_X_END_S0,
		(plane_info->src_x + plane_info->src_w - 1));
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_Y_START_S0,
		plane_info->src_y);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_Y_END_S0,
		(plane_info->src_y + plane_info->src_h - 1));

	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_COMMAND, 1);

	DRM_DEBUG("%s set_state called.\n", afbc->base.name);
}

static void osd2_afbc_set_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
	u32 pixel_format, line_stride, output_stride;
	u32 plane_index, osd_index;
	u64 header_addr, out_addr;
	u32 aligned_32, afbc_color_reorder;
	unsigned int depth;
	int bpp;
	struct meson_vpu_afbc *afbc;
	struct meson_vpu_afbc_state *afbc_state;
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_pipeline_state *pipeline_state;

	afbc = to_afbc_block(vblk);
	afbc_state = to_afbc_state(state);
	pipeline = vblk->pipeline;
	osd_index = vblk->index;
	pipeline_state = priv_to_pipeline_state(pipeline->obj.state);
	plane_index = pipeline_state->ratio_plane_index[osd_index];
	plane_info = &pipeline_state->plane_info[plane_index];

	if (!plane_info->afbc_en)
		return;

	osd_afbc_enable(1, 1);

	aligned_32 = 1;
	afbc_color_reorder = 0x1234;
	pixel_format = afbc_pix_format(plane_info->pixel_format);
	drm_fb_get_bpp_depth(plane_info->pixel_format, &depth, &bpp);
	header_addr = plane_info->phy_addr;

	line_stride = line_stride_calc_afbc(pixel_format,
		plane_info->src_w, aligned_32);

	output_stride = plane_info->src_w * bpp;

	header_addr = plane_info->phy_addr;
	out_addr = ((u64)(vblk->index + 1)) << 24;

	/* set osd path misc ctrl */
	VSYNCOSD_WR_MPEG_REG_BITS(OSD_PATH_MISC_CTRL, 0x1, (osd_index + 4), 1);

	/* set linear addr */
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_CTRL_STAT, 0x1, 2, 1);
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_CTRL_STAT2, 1, 1, 1);

	/* set read from mali */
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_BLK0_CFG_W0, 0x1, 30, 1);
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_BLK0_CFG_W0, 0, 15, 1);

	/* set line_stride */
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_BLK2_CFG_W4, line_stride, 0, 12);

	/* set frame addr */
	VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK1_CFG_W4, out_addr & 0xffffffff);

	/* set afbc color reorder and mali src*/
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_MALI_UNPACK_CTRL,
			afbc_color_reorder, 0, 16);
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_MALI_UNPACK_CTRL, 0x1, 31, 1);

	/* set header addr */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1,
			header_addr & 0xffffffff);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S1,
			(header_addr >> 32) & 0xffffffff);

	/* set format specifier */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_FORMAT_SPECIFIER_S1,
		plane_info->afbc_inter_format | (pixel_format & 0x0f));

	/* set pic size */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BUFFER_WIDTH_S1, plane_info->src_w);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BUFFER_HEIGHT_S1, plane_info->src_h);

	/* set buf stride */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_OUTPUT_BUF_STRIDE_S1, output_stride);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S1,
			out_addr & 0xffffffff);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S1,
			(out_addr >> 32) & 0xffffffff);

	/* set bounding box */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_X_START_S1,
			plane_info->src_x);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_X_END_S1,
			(plane_info->src_x + plane_info->src_w - 1));
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_Y_START_S1,
			plane_info->src_y);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_Y_END_S1,
		(plane_info->src_y + plane_info->src_h - 1));

	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_COMMAND, 1);

	DRM_DEBUG("%s set_state called.\n", afbc->base.name);
}

static void osd3_afbc_set_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
	u32 pixel_format, line_stride, output_stride;
	u32 plane_index, osd_index;
	u64 header_addr, out_addr;
	u32 aligned_32, afbc_color_reorder;
	unsigned int depth;
	int bpp;
	struct meson_vpu_afbc *afbc;
	struct meson_vpu_afbc_state *afbc_state;
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_pipeline_state *pipeline_state;

	afbc = to_afbc_block(vblk);
	afbc_state = to_afbc_state(state);
	pipeline = vblk->pipeline;
	osd_index = vblk->index;
	pipeline_state = priv_to_pipeline_state(pipeline->obj.state);
	plane_index = pipeline_state->ratio_plane_index[osd_index];
	plane_info = &pipeline_state->plane_info[plane_index];

	if (!plane_info->afbc_en)
		return;

	osd_afbc_enable(2, 1);

	aligned_32 = 1;
	afbc_color_reorder = 0x1234;
	pixel_format = afbc_pix_format(plane_info->pixel_format);
	drm_fb_get_bpp_depth(plane_info->pixel_format, &depth, &bpp);
	header_addr = plane_info->phy_addr;

	line_stride = line_stride_calc_afbc(pixel_format,
		plane_info->src_w, aligned_32);

	output_stride = plane_info->src_w * bpp;

	header_addr = plane_info->phy_addr;
	out_addr = ((u64)(vblk->index + 1)) << 24;

	/* set osd path misc ctrl */
	VSYNCOSD_WR_MPEG_REG_BITS(OSD_PATH_MISC_CTRL, 0x1, (osd_index + 4), 1);

	/* set linear addr */
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD3_CTRL_STAT, 0x1, 2, 1);
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD3_CTRL_STAT2, 1, 1, 1);

	/* set read from mali */
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD3_BLK0_CFG_W0, 0x1, 30, 1);
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD3_BLK0_CFG_W0, 0, 15, 1);

	/* set line_stride */
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD3_BLK2_CFG_W4, line_stride, 0, 12);

	/* set frame addr */
	VSYNCOSD_WR_MPEG_REG(VIU_OSD3_BLK1_CFG_W4, out_addr & 0xffffffff);

	/* set afbc color reorder and mali src*/
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD3_MALI_UNPACK_CTRL,
			afbc_color_reorder, 0, 16);
	VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD3_MALI_UNPACK_CTRL, 0x1, 31, 1);

	/* set header addr */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_HEADER_BUF_ADDR_LOW_S2,
			header_addr & 0xffffffff);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S2,
			(header_addr >> 32) & 0xffffffff);

	/* set format specifier */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_FORMAT_SPECIFIER_S2,
		plane_info->afbc_inter_format | (pixel_format & 0x0f));

	/* set pic size */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BUFFER_WIDTH_S2, plane_info->src_w);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BUFFER_HEIGHT_S2, plane_info->src_h);

	/* set buf stride */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_OUTPUT_BUF_STRIDE_S2, output_stride);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S2,
			out_addr & 0xffffffff);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S2,
			(out_addr >> 32) & 0xffffffff);

	/* set bounding box */
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_X_START_S2,
			plane_info->src_x);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_X_END_S2,
		(plane_info->src_x + plane_info->src_w - 1));
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_Y_START_S2,
		plane_info->src_y);
	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_BOUNDING_BOX_Y_END_S2,
		(plane_info->src_y + plane_info->src_h - 1));

	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_COMMAND, 1);

	DRM_DEBUG("%s set_state called.\n", afbc->base.name);
}

#if 0
static void osd_afbc_set_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
	u32 bpp, pixel_format, line_stride, output_stride;
	u32 plane_index, osd_index;
	u64 header_addr, out_addr;
	const struct color_bit_define_s *color_info;
	struct meson_vpu_afbc *afbc;
	struct meson_vpu_afbc_state *afbc_state;
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_pipeline_state *pipeline_state;
	struct hw_osd_reg_s *osd_reg = &hw_osd_reg_array[index];

	afbc = to_afbc_block(vblk);
	afbc_state = to_afbc_state(state);
	pipeline = vblk->pipeline;
	osd_index = vblk->index;
	pipeline_state = priv_to_pipeline_state(pipeline->obj.state);
	plane_index = pipeline_state->ratio_plane_index[osd_index];
	plane_info = pipeline_state->plane_info[plane_index];

	u32 aligned_32 = 1;
	u32 afbc_color_reorder = 0x1234;

	pixel_format = afbc_pix_format(plane_info->pixel_format);
	color_info = convert_panel_format(pixel_format);
	header_addr = plane_info->phy_addr;

	bpp = color_info->bpp >> 3;
	line_stride = line_stride_calc_afbc(pixel_format,
		plane_info->src_w, aligned_32);

	output_stride = plane_info->src_w * bpp;

	header_addr = plane_info->phy_addr;
	out_addr = ((u64)(vblk->index + 1)) << 24;

	/* set osd path misc ctrl */
	VSYNCOSD_WR_MPEG_REG_BITS(OSD_PATH_MISC_CTRL, 0x1, (osd_index + 4), 1);

	/* set linear addr */
	VSYNCOSD_WR_MPEG_REG_BITS(osd_reg->osd_ctrl_stat, 0x1, 2, 1);

	/* set read from mali */
	VSYNCOSD_WR_MPEG_REG_BITS(osd_reg->osd_blk0_cfg_w0, 0x1, 30, 1);
	VSYNCOSD_WR_MPEG_REG_BITS(osd_reg->osd_blk0_cfg_w0, 0, 15, 1);

	/* set line_stride */
	VSYNCOSD_WR_MPEG_REG_BITS(osd_reg->osd_blk2_cfg_w4, line_stride, 0, 12);

	/* set frame addr */
	VSYNCOSD_WR_MPEG_REG(osd_reg->osd_blk1_cfg_w4, out_addr & 0xffffffff);

	/* set afbc color reorder and mali src*/
	VSYNCOSD_WR_MPEG_REG_BITS(osd_reg->osd_mali_unpack_ctrl,
			afbc_color_reorder, 0, 16);
	VSYNCOSD_WR_MPEG_REG_BITS(osd_reg->osd_mali_unpack_ctrl, 0x1, 31, 1);

	/* set header addr */
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_header_buf_addr_low_s,
			out_addr & 0xffffffff);
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_header_buf_addr_high_s,
			(out_addr >> 32) & 0xffffffff);

	/* set format specifier */
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_format_specifier_s,
		plane_info->inter_format | (pixel_format & 0x0f));

	/* set pic size */
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_buffer_width_s, plane_info->src_w);
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_buffer_hight_s, plane_info->src_h);

	/* set buf stride */
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_output_buf_stride_s, output_stride);
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_output_buf_addr_low_s,
			out_addr & 0xffffffff);
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_output_buf_addr_high_s,
			(out_addr >> 32) & 0xffffffff);

	/* set bounding box */
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_boundings_box_x_start_s,
			plane_info->src_x);
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_boundings_box_x_end_s,
		(palne_info->src_x + plane_info->src_w - 1));
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_boundings_box_y_start_s,
		plane_info->src_y);
	VSYNCOSD_WR_MPEG_REG(osd_reg->afbc_boundings_box_y_end_s,
		(plane_info->src_y + plane_info->src_h - 1));

	VSYNCOSD_WR_MPEG_REG(VPU_MAFBC_COMMAND, 1);

	DRM_DEBUG("%s set_state called.\n", afbc->base.name);
}
#endif

static void afbc_set_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
#if 0
	osd_afbc_set_state(vblk, state);
#else
	switch (vblk->index) {
	case 0:
		osd1_afbc_set_state(vblk, state);
		break;
	case 1:
		osd2_afbc_set_state(vblk, state);
		break;
	case 2:
		osd3_afbc_set_state(vblk, state);
		break;
	default:
		break;
	}
#endif

}

static void afbc_hw_enable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);
	u32 osd_index = vblk->index;

	osd_afbc_enable(osd_index, 1);

	DRM_DEBUG("%s enable called.\n", afbc->base.name);
}

static void afbc_hw_disable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);
	u32 osd_index = vblk->index;

	osd_afbc_enable(osd_index, 0);

	DRM_DEBUG("%s disable called.\n", afbc->base.name);
}

static void afbc_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_afbc *afbc = to_afbc_block(vblk);

	switch_vpu_mem_pd_vmod(VPU_MAIL_AFBCD,
			VPU_MEM_POWER_ON);
	/* disable osd1 afbc */
	osd_afbc_enable(0, 0);
	osd_afbc_enable(1, 0);
	osd_afbc_enable(2, 0);

	DRM_DEBUG("%s hw_init called.\n", afbc->base.name);
}

struct meson_vpu_block_ops afbc_ops = {
	.check_state = afbc_check_state,
	.update_state = afbc_set_state,
	.enable = afbc_hw_enable,
	.disable = afbc_hw_disable,
	.init = afbc_hw_init,
};
