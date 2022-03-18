// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/video_sink/video.h>

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif

#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"
#include "meson_vpu_util.h"
#include "meson_vpu_postblend.h"

static struct postblend_reg_s postblend_reg = {
	VPP_OSD1_BLD_H_SCOPE,
	VPP_OSD1_BLD_V_SCOPE,
	VPP_OSD2_BLD_H_SCOPE,
	VPP_OSD2_BLD_V_SCOPE,
	VD1_BLEND_SRC_CTRL,
	VD2_BLEND_SRC_CTRL,
	OSD1_BLEND_SRC_CTRL,
	OSD2_BLEND_SRC_CTRL,
	VPP_OSD1_IN_SIZE,
};

/*vpp post&post blend for osd1 premult flag config as 0 default*/
static void osd1_blend_premult_set(struct postblend_reg_s *reg)
{
	meson_vpu_write_reg_bits(reg->osd1_blend_src_ctrl, 0, 4, 1);
	meson_vpu_write_reg_bits(reg->osd1_blend_src_ctrl, 0, 16, 1);
}

/*vpp pre&post blend for osd2 premult flag config as 0 default*/
static void osd2_blend_premult_set(struct postblend_reg_s *reg)
{
	meson_vpu_write_reg_bits(reg->osd2_blend_src_ctrl, 0, 4, 1);
	meson_vpu_write_reg_bits(reg->osd2_blend_src_ctrl, 0, 16, 1);
}

/*vpp osd1 blend sel*/
static void osd1_blend_switch_set(struct postblend_reg_s *reg,
				  enum vpp_blend_e blend_sel)
{
	meson_vpu_write_reg_bits(reg->osd1_blend_src_ctrl, blend_sel, 20, 1);
}

/*vpp osd2 blend sel*/
static void osd2_blend_switch_set(struct postblend_reg_s *reg,
				  enum vpp_blend_e blend_sel)
{
	meson_vpu_write_reg_bits(reg->osd2_blend_src_ctrl, blend_sel, 20, 1);
}

/*vpp osd1 preblend mux sel*/
static void vpp_osd1_preblend_mux_set(struct postblend_reg_s *reg,
				      enum vpp_blend_src_e src_sel)
{
	meson_vpu_write_reg_bits(reg->osd1_blend_src_ctrl, src_sel, 0, 4);
}

/*vpp osd2 preblend mux sel*/
static void vpp_osd2_preblend_mux_set(struct postblend_reg_s *reg,
				      enum vpp_blend_src_e src_sel)
{
	meson_vpu_write_reg_bits(reg->osd2_blend_src_ctrl, src_sel, 0, 4);
}

/*vpp osd1 postblend mux sel*/
static void vpp_osd1_postblend_mux_set(struct postblend_reg_s *reg,
				       enum vpp_blend_src_e src_sel)
{
	meson_vpu_write_reg_bits(reg->osd1_blend_src_ctrl, src_sel, 8, 4);
}

/*vpp osd2 postblend mux sel*/
static void vpp_osd2_postblend_mux_set(struct postblend_reg_s *reg,
				       enum vpp_blend_src_e src_sel)
{
	meson_vpu_write_reg_bits(reg->osd2_blend_src_ctrl, src_sel, 8, 4);
}

/*vpp osd1 blend scope set*/
static void vpp_osd1_blend_scope_set(struct postblend_reg_s *reg,
				     struct osd_scope_s scope)
{
	meson_vpu_write_reg(reg->vpp_osd1_bld_h_scope,
			    (scope.h_start << 16) | scope.h_end);
	meson_vpu_write_reg(reg->vpp_osd1_bld_v_scope,
			    (scope.v_start << 16) | scope.v_end);
}

/*vpp osd2 blend scope set*/
static void vpp_osd2_blend_scope_set(struct postblend_reg_s *reg,
				     struct osd_scope_s scope)
{
	meson_vpu_write_reg(reg->vpp_osd2_bld_h_scope,
			    (scope.h_start << 16) | scope.h_end);
	meson_vpu_write_reg(reg->vpp_osd2_bld_v_scope,
			    (scope.v_start << 16) | scope.v_end);
}

static void vpp_chk_crc(struct am_meson_crtc *amcrtc)
{
	if (amcrtc->force_crc_chk ||
		(amcrtc->vpp_crc_enable && cpu_after_eq(MESON_CPU_MAJOR_ID_SM1))) {
		meson_vpu_write_reg(VPP_CRC_CHK, 1);
		amcrtc->force_crc_chk--;
	}
}

/*background data R[32:47] G[16:31] B[0:15] alpha[48:63]->
 *dummy data Y[16:23] Cb[8:15] Cr[0:7]
 */
static void vpp_postblend_dummy_data_set(struct am_meson_crtc_state *crtc_state)
{
	int r, g, b, alpha, y, u, v;
	u32 vpp_index = 0;

	b = (crtc_state->crtc_bgcolor & 0xffff) / 256;
	g = ((crtc_state->crtc_bgcolor >> 16) & 0xffff) / 256;
	r = ((crtc_state->crtc_bgcolor >> 32) & 0xffff) / 256;
	alpha = ((crtc_state->crtc_bgcolor >> 48) & 0xffff) / 256;

	y = ((47 * r + 157 * g + 16 * b + 128) >> 8) + 16;
	u = ((-26 * r - 87 * g + 113 * b + 128) >> 8) + 128;
	v = ((112 * r - 102 * g - 10 * b + 128) >> 8) + 128;

	set_post_blend_dummy_data(vpp_index, 1 << 24 | y << 16 | u << 8 | v, alpha);
}

static int postblend_check_state(struct meson_vpu_block *vblk,
				 struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	u32 video_zorder, osd_zorder, top_flag, bottom_flag, i, j;

	if (state->checked)
		return 0;

	state->checked = true;
	for (i = 0; i < MESON_MAX_VIDEO &&
	     mvps->video_plane_info[i].enable; i++) {
		video_zorder = mvps->video_plane_info[i].zorder;
		top_flag = 0;
		bottom_flag = 0;
		for (j = 0; j < MESON_MAX_OSDS &&
		     mvps->plane_info[j].enable; j++) {
			osd_zorder = mvps->plane_info[j].zorder;
			if (video_zorder > osd_zorder)
				top_flag++;
			else
				bottom_flag++;
		}
		if (top_flag && bottom_flag) {
			DRM_DEBUG("unsupported zorder\n");
			return -1;
		} else if (top_flag) {
			set_video_zorder(video_zorder +
					 VPP_POST_BLEND_REF_ZORDER, i);
			DRM_DEBUG("video on the top\n");
		} else if (bottom_flag) {
			set_video_zorder(video_zorder, i);
			DRM_DEBUG("video on the bottom\n");
		}
	}

	DRM_DEBUG("%s check_state called.\n", postblend->base.name);
	return 0;
}

static void postblend_set_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state)
{
	int crtc_index;
	struct am_meson_crtc *amc;
	struct am_meson_crtc_state *meson_crtc_state;
	struct meson_vpu_pipeline_state *mvps;

	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct osd_scope_s scope = {0, 1919, 0, 1079};
	struct meson_vpu_pipeline *pipeline = postblend->base.pipeline;
	struct postblend_reg_s *reg = postblend->reg;

	crtc_index = vblk->index;
	amc = vblk->pipeline->priv->crtcs[crtc_index];
	meson_crtc_state = to_am_meson_crtc_state(amc->base.state);

	DRM_DEBUG("%s set_state called.\n", postblend->base.name);
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	scope.h_start = 0;
	scope.h_end = mvps->scaler_param[0].output_width - 1;
	scope.v_start = 0;
	scope.v_end = mvps->scaler_param[0].output_height - 1;
	vpp_osd1_blend_scope_set(reg, scope);
	if (0)
		vpp_osd2_blend_scope_set(reg, scope);

	if (amc->blank_enable) {
		vpp_osd1_postblend_mux_set(postblend->reg, VPP_NULL);
	} else {
		/*dout switch config*/
		osd1_blend_switch_set(postblend->reg, VPP_POSTBLEND);
		osd2_blend_switch_set(postblend->reg, VPP_POSTBLEND);
		/*vpp input config*/
		vpp_osd1_preblend_mux_set(postblend->reg, VPP_NULL);
		vpp_osd2_preblend_mux_set(postblend->reg, VPP_NULL);

		if (pipeline->osd_version == OSD_V7)
			vpp_osd1_postblend_mux_set(postblend->reg, VPP_OSD2);
		else
			vpp_osd1_postblend_mux_set(postblend->reg, VPP_OSD1);
		vpp_osd2_postblend_mux_set(postblend->reg, VPP_NULL);
	}

	vpp_chk_crc(amc);
	vpp_postblend_dummy_data_set(meson_crtc_state);
	osd1_blend_premult_set(reg);
	if (0)
		osd2_blend_premult_set(reg);
	DRM_DEBUG("scope h/v start/end [%d,%d,%d,%d].\n",
		  scope.h_start, scope.h_end, scope.v_start, scope.v_end);
}

static void postblend_hw_enable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	DRM_DEBUG("%s enable called.\n", postblend->base.name);
}

static void postblend_hw_disable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	vpp_osd1_postblend_mux_set(postblend->reg, VPP_NULL);
	DRM_DEBUG("%s disable called.\n", postblend->base.name);
}

static void postblend_dump_register(struct meson_vpu_block *vblk,
				    struct seq_file *seq)
{
	u32 value;
	struct meson_vpu_postblend *postblend;
	struct postblend_reg_s *reg;

	postblend = to_postblend_block(vblk);
	reg = postblend->reg;

	value = meson_drm_read_reg(reg->osd1_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD1_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->osd2_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD2_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vd1_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VD1_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vd2_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VD2_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_in_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_IN_SIZE:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_bld_h_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_BLD_H_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_bld_v_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_BLD_V_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd2_bld_h_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD2_BLD_H_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd2_bld_v_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD2_BLD_V_SCOPE:", value);
}

static void postblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	postblend->reg = &postblend_reg;
	DRM_DEBUG("%s hw_init called.\n", postblend->base.name);
}

struct meson_vpu_block_ops postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = postblend_hw_init,
};

struct meson_vpu_block_ops t7_postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = postblend_hw_init,
};
