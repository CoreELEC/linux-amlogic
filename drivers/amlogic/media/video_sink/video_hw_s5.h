/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_sink/video_hw_s5.h
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

#ifndef VIDEO_HW_S5_HH
#define VIDEO_HW_S5_HH
#include "video_reg_s5.h"

enum sr0_path_sel {
	SR0_IN_SLICE0,
	SR0_IN_SLICE1,
};

enum {
	VD1_4SLICES_MODE    = 0,   //
	VD1_2_2SLICES_MODE  = 1,   //slice0/1 and slice2/3
	VD1_SLICES01_MODE   = 2,   //slice0/1
	VD1_SLICES23_MODE   = 3,   //slice2/3
	VD1_1SLICES_MODE    = 4    //slice0
};

enum {
	SR0_AFTER_PPS   = 0,
	SR0_BEFORE_PPS  = 1
};

enum {
	VD1_SLICES_DOUT_PI   = 0,  //pi path
	VD1_SLICES_DOUT_4S4P = 1,  //4slice4ppc path
	VD1_SLICES_DOUT_2S4P = 2,
    /*1SLICES_MODE: blend_1ppc->1slice4ppc path, 4SLICES_MODE: blend_1ppc->1slices4ppc path */
	VD1_SLICES_DOUT_1S4P = 3
};

enum {
	/* pi  path (1ppc->4ppc) */
	VD2_DOUT_PI = 0,
    /* s2p path (2ppc->4ppc) */
	VD2_DOUT_S2P = 1,
    /* vd1s0 prebld path  */
	VD2_DOUT_PREBLD = 2,
    /* vd1s1 prebld path */
	VD2_DOUT_PREBLD1 = 3
};
struct vd_proc_slice_s {
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
};

struct vd_proc_pps_s {
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
	u32 slice_x_st;
	u32 pps_slice;
	u32 prehsc_en;
	u32 prevsc_en;
	u32 prehsc_rate;
	u32 prevsc_rate;
	u32 horz_phase_step;
	u32 vert_phase_step;
};

struct vd_proc_sr_s {
	u32 sr_en;
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
	u32 h_scaleup_en;
	u32 v_scaleup_en;
	u32 sr_support;
	u32 core_v_enable_width_max;
	u32 core_v_disable_width_max;
};

struct vd_proc_hwin_s {
	u32 hwin_en;
	u32 hwin_din_hsize;
	u32 hwin_bgn;
	u32 hwin_end;
};

struct vd_proc_pi_s {
	u32 pi_en;
	u32 pi_in_hsize;
	u32 pi_in_vsize;
};

struct vd_proc_blend_s {
	u32 bld_out_en;
	u32 bld_out_w;
	u32 bld_out_h;
	u32 bld_dummy_data;

	u32 bld_src1_sel;
	u32 bld_src2_sel;
	u32 bld_src3_sel;
	u32 bld_src4_sel;

	u32 bld_din0_h_start;
	u32 bld_din0_h_end;
	u32 bld_din0_v_start;
	u32 bld_din0_v_end;

	u32 bld_din1_h_start;
	u32 bld_din1_h_end;
	u32 bld_din1_v_start;
	u32 bld_din1_v_end;

	u32 bld_din2_h_start;
	u32 bld_din2_h_end;
	u32 bld_din2_v_start;
	u32 bld_din2_v_end;

	u32 bld_din3_h_start;
	u32 bld_din3_h_end;
	u32 bld_din3_v_start;
	u32 bld_din3_v_end;

	//usually the bottom layer set 1, for example postbld_src1_sel = 1,set 0x1
	u32 bld_din0_premult_en;
	u32 bld_din1_premult_en;
	u32 bld_din2_premult_en;
	u32 bld_din3_premult_en;
};

struct vd_proc_unit_s {
	u32 slice_index;
	u32 sr0_dpath_sel;
	u32 sr0_pps_dpsel;
	u32 sr0_en;
	u32 sr1_en;
	u32 reg_bypass_prebld;
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
	u32 dout_x_start;
	u32 dout_y_start;
	u32 prebld_hsize;
	u32 prebld_vsize;
	u32 bypass_detunnel;
	u32 bypass_hdr;
	u32 bypass_dv;
	u32 bypass_ve;
	struct vd_proc_slice_s vd_proc_slice;
	struct vd_proc_pps_s vd_proc_pps;
	struct vd_proc_sr_s vd_proc_sr0;
	struct vd_proc_sr_s vd_proc_sr1;
	struct vd_proc_hwin_s vd_proc_hwin;
};

struct vd_proc_mosaic_s {
	u32 vd1_proc_slice_pad_en[SLICE_NUM];
	u32 vd1_proc_slice_pad_h_bgn[SLICE_NUM];
	u32 vd1_proc_slice_pad_h_end[SLICE_NUM];
	u32 vd1_proc_slice_pad_v_bgn[SLICE_NUM];
	u32 vd1_proc_slice_pad_v_end[SLICE_NUM];
	u32 mosaic_vd1_dout_hsize;
	u32 mosaic_vd1_dout_vsize;
	u32 mosaic_vd2_dout_hsize;
	u32 mosaic_vd2_dout_vsize;
};

struct vd_proc_slice_info_s {
	/* vd1 Real input size to each slice */
	/* with overlap */
	u32 vd1_slice_din_hsize[SLICE_NUM];
	u32 vd1_slice_din_vsize[SLICE_NUM];
	u32 vd1_slice_x_st[SLICE_NUM];
	u32 vd1_slice_x_end[SLICE_NUM];
};

struct vd_proc_preblend_info_s {
	u32 vd1s0_vd2_prebld_en;/* input */
	u32 vd1s1_vd2_prebld_en;/* input */
	u32 prebld_dout_hsize;  /* input */
	u32 prebld_dout_vsize;  /* input */
};

struct vd_proc_vd2_info_s {
	u32 vd2_dout_dpsel;
	u32 vd2_din_hsize;
	u32 vd2_din_vsize;
	u32 vd2_dout_hsize;
	u32 vd2_dout_vsize;
};

struct vd_proc_vd1_info_s {
	u32 slice_num;
	u32 vd1_work_mode;
	u32 vd1_slices_dout_dpsel;
	u32 vd1_overlap_hsize;
	/* whole frame in hsize */
	u32 vd1_src_din_hsize[SLICE_NUM];
	u32 vd1_src_din_vsize[SLICE_NUM];
	/* without overlap */
	u32 vd1_proc_unit_dout_hsize[SLICE_NUM];
	u32 vd1_proc_unit_dout_vsize[SLICE_NUM];
	/* whole vd1 output size */
	u32 vd1_dout_hsize;
	u32 vd1_dout_vsize;
	u32 vd1_dout_x_start[SLICE_NUM];
	u32 vd1_dout_y_start[SLICE_NUM];
};

struct vd2_proc_s {
	u32 bypass_dv;
	u32 vd2_dout_dpsel;
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
	/* detunnel */
	u32 bypass_detunnel;
	/* hdr2 */
	u32 bypass_hdr;
	struct vd_proc_pps_s vd_proc_pps;
	struct vd_proc_pi_s vd_proc_pi;
};

struct vd_proc_s {
	/* input param */
	u32 vd1_used;
	u32 vd2_used;
	u32 bypass_detunnel;
	u32 bypass_hdr;
	u32 bypass_dv;
	u32 bypass_ve;
	/* vd1 */
	struct vd_proc_vd1_info_s vd_proc_vd1_info;
	/* vd2 */
	struct vd_proc_vd2_info_s vd_proc_vd2_info;
	/* preblend (sometimes input) */
	struct vd_proc_preblend_info_s vd_proc_preblend_info;
	/* mosaic (sometimes input) */
	struct vd_proc_mosaic_s vd_proc_mosaic;

	/* caculated param */
	/* slice calucated */
	struct vd_proc_slice_info_s vd_proc_slice_info;
	/* proc unit calucated */
	struct vd_proc_unit_s vd_proc_unit[SLICE_NUM];
	/* proc pi */
	struct vd_proc_pi_s vd_proc_pi;
	/* proc blend calucated */
	struct vd_proc_blend_s vd_proc_blend;

	struct vd_proc_blend_s vd_proc_preblend;
	/* vd2 proc */
	struct vd2_proc_s vd2_proc;
};

struct vd_proc_reg_s {
	struct vd_mif_reg_s vd_mif_reg[MAX_VD_LAYER_S5];
	struct vd_mif_linear_reg_s vd_mif_linear_reg[MAX_VD_LAYER_S5];
	struct vd_afbc_reg_s vd_afbc_reg[MAX_VD_LAYER_S5];
	struct vd_fg_reg_s vd_fg_reg[MAX_VD_LAYER_S5];
	struct vd_pps_reg_s vd_pps_reg[MAX_VD_LAYER_S5 + 1];
	struct vd_proc_slice_reg_s vd_proc_slice_reg[SLICE_NUM];
	struct vd_proc_sr_reg_s vd_proc_sr_reg;
	struct vd_proc_pi_reg_s vd_proc_pi_reg;
	struct vd_proc_blend_reg_s vd_proc_blend_reg;
	struct vd_proc_misc_reg_s vd_proc_misc_reg;
	struct vd1_slice_pad_reg_s vd1_slice_pad_size0_reg[SLICE_NUM];
	struct vd1_slice_pad_reg_s vd1_slice_pad_size1_reg[SLICE_NUM];
	struct vd2_pre_blend_reg_s vd2_pre_blend_reg;
	struct vd2_proc_misc_reg_s vd2_proc_misc_reg;
};

struct vd_proc_s *get_vd_proc_info(void);
void dump_s5_vd_proc_regs(void);
void set_video_slice_policy(struct video_layer_s *layer,
	struct vframe_s *vf);

void set_vd_mif_linear_cs_s5(struct video_layer_s *layer,
				   struct canvas_s *cs0,
				   struct canvas_s *cs1,
				   struct canvas_s *cs2,
				   struct vframe_s *vf,
				   u32 lr_select);
void set_vd_mif_linear_s5(struct video_layer_s *layer,
				   struct canvas_config_s *config,
				   u32 planes,
				   struct vframe_s *vf,
				   u32 lr_select);
void disable_vd1_blend_s5(struct video_layer_s *layer);
void disable_vd2_blend_s5(struct video_layer_s *layer);
void aisr_reshape_addr_set_s5(struct video_layer_s *layer,
				  struct aisr_setting_s *aisr_mif_setting);
bool is_sr_phase_changed_s5(void);
void vd_s5_hw_set(struct video_layer_s *layer,
	struct vframe_s *dispbuf, struct vpp_frame_par_s *frame_par);
void canvas_update_for_mif_slice(struct video_layer_s *layer,
			     struct vframe_s *vf,
			     u32 slice);

#endif
