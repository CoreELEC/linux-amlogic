/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_ctl.h
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

#ifndef __TVIN_VDIN_CTL_H
#define __TVIN_VDIN_CTL_H
#include <linux/highmem.h>
#include <linux/page-flags.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include "vdin_drv.h"

#define DV_SWAP_EN	(1 << 0)
#define DV_BUF_START_RESET	(1 << 1)
#define DV_FRAME_BUF_START_RESET	(1 << 2)
#define DV_UPDATE_DATA_MODE_DELBY_WORK	(1 << 4)
#define DV_CLEAN_UP_MEM	(1 << 5)
#define DV_READ_MODE_AXI	(1 << 6)
#define DV_CRC_CHECK	(1 << 7)

enum vdin_output_mif_e {
	VDIN_OUTPUT_TO_MIF = 0,
	VDIN_OUTPUT_TO_AFBCE = 1,
};

enum wr_sel_vdin_e {
	WR_SEL_DIS = 0,
	WR_SEL_VDIN0_NOR = 1,
	WR_SEL_VDIN0_SML = 2,
	WR_SEL_VDIN1_NOR = 3,
	WR_SEL_VDIN1_SML = 4,
};

/* *************************************************** */
/* *** structure definitions ************************* */
/* *************************************************** */
struct vdin_matrix_lup_s {
	unsigned int pre_offset0_1;
	unsigned int pre_offset2;
	unsigned int coef00_01;
	unsigned int coef02_10;
	unsigned int coef11_12;
	unsigned int coef20_21;
	unsigned int coef22;
	unsigned int post_offset0_1;
	unsigned int post_offset2;
};

#ifdef CONFIG_AML_LOCAL_DIMMING
struct ldim_max_s {
    /* general parameters */
	int ld_pic_rowmax;
	int ld_pic_colmax;
	int ld_stamax_hidx[11];  /* U12* 9 */
	int ld_stamax_vidx[11];  /* u12x 9 */
};
#endif

extern unsigned int game_mode;
extern bool vdin_dbg_en;

/* ************************************************************************ */
/* ******** GLOBAL FUNCTION CLAIM ******** */
/* ************************************************************************ */
extern u8 *vdin_vmap(ulong addr, u32 size);
extern void vdin_unmap_phyaddr(u8 *vaddr);
extern void vdin_dma_flush(struct vdin_dev_s *devp, void *vaddr,
		int size, enum dma_data_direction dir);

extern void vdin_set_vframe_prop_info(struct vframe_s *vf,
		struct vdin_dev_s *devp);
extern void LDIM_Initial_2(int pic_h, int pic_v, int BLK_Vnum,
	int BLK_Hnum, int BackLit_mode, int ldim_bl_en, int ldim_hvcnt_bypass);
extern void vdin_get_format_convert(struct vdin_dev_s *devp);
extern enum vdin_format_convert_e vdin_get_format_convert_matrix0(
		struct vdin_dev_s *devp);
extern enum vdin_format_convert_e vdin_get_format_convert_matrix1(
		struct vdin_dev_s *devp);
extern void vdin_set_prob_xy(unsigned int offset, unsigned int x,
		unsigned int y, struct vdin_dev_s *devp);
extern void vdin_prob_get_rgb(unsigned int offset, unsigned int *r,
		unsigned int *g, unsigned int *b);
extern void vdin_set_all_regs(struct vdin_dev_s *devp);
extern void vdin_set_default_regmap(unsigned int offset);
extern void vdin_set_def_wr_canvas(struct vdin_dev_s *devp);
void vdin_hw_enable(struct vdin_dev_s *devp);
void vdin_hw_disable(struct vdin_dev_s *devp);
extern unsigned int vdin_get_field_type(unsigned int offset);
extern int vdin_vsync_reset_mif(int index);
extern bool vdin_check_vdi6_afifo_overflow(unsigned int offset);
extern void vdin_clear_vdi6_afifo_overflow_flg(unsigned int offset);
extern void vdin_set_cutwin(struct vdin_dev_s *devp);
extern void vdin_set_decimation(struct vdin_dev_s *devp);
extern void vdin_fix_nonstd_vsync(struct vdin_dev_s *devp);
extern unsigned int vdin_get_meas_hcnt64(unsigned int offset);
extern unsigned int vdin_get_meas_vstamp(unsigned int offset);
extern unsigned int vdin_get_active_h(unsigned int offset);
extern unsigned int vdin_get_active_v(unsigned int offset);
extern unsigned int vdin_get_total_v(unsigned int offset);
extern unsigned int vdin_get_canvas_id(unsigned int offset);
extern void vdin_set_canvas_id(struct vdin_dev_s *devp,
			unsigned int rdma_enable, unsigned int canvas_id);
extern unsigned int vdin_get_chma_canvas_id(unsigned int offset);
extern void vdin_set_chma_canvas_id(struct vdin_dev_s *devp,
		unsigned int rdma_enable, unsigned int canvas_id);
void vdin_enable_module(struct vdin_dev_s *devp, bool enable);
extern void vdin_set_matrix(struct vdin_dev_s *devp);
extern void vdin_set_matrixs(struct vdin_dev_s *devp, unsigned char no,
		enum vdin_format_convert_e csc);
extern bool vdin_check_cycle(struct vdin_dev_s *devp);
extern bool vdin_write_done_check(unsigned int offset,
		struct vdin_dev_s *devp);
extern void vdin_calculate_duration(struct vdin_dev_s *devp);
extern void vdin_wr_reverse(unsigned int offset, bool hreverse,
		bool vreverse);
extern void vdin_set_hvscale(struct vdin_dev_s *devp);
extern void vdin_set_bitdepth(struct vdin_dev_s *devp);
extern void vdin_set_cm2(unsigned int offset, unsigned int w,
		unsigned int h, unsigned int *data);
extern void vdin_bypass_isp(unsigned int offset);
extern void vdin_set_mpegin(struct vdin_dev_s *devp);
extern void vdin_force_gofiled(struct vdin_dev_s *devp);
extern void vdin_adjust_tvafesnow_brightness(void);
extern void vdin_set_config(struct vdin_dev_s *devp);
extern void vdin_set_wr_mif(struct vdin_dev_s *devp);
extern void vdin_dolby_config(struct vdin_dev_s *devp);
extern void vdin_dolby_buffer_update(struct vdin_dev_s *devp,
	unsigned int index);
extern void vdin_dolby_addr_update(struct vdin_dev_s *devp, unsigned int index);
extern void vdin_dolby_addr_alloc(struct vdin_dev_s *devp, unsigned int size);
extern void vdin_dolby_addr_release(struct vdin_dev_s *devp, unsigned int size);
extern int vdin_event_cb(int type, void *data, void *op_arg);
extern void vdin_hdmiin_patch(struct vdin_dev_s *devp);
extern void vdin_set_top(struct vdin_dev_s *devp, unsigned int offset,
		enum tvin_port_e port,
		enum tvin_color_fmt_e input_cfmt, unsigned int h,
		enum bt_path_e bt_path);
extern void vdin_set_wr_ctrl_vsync(struct vdin_dev_s *devp,
	unsigned int offset, enum vdin_format_convert_e format_convert,
	unsigned int color_depth_mode, unsigned int source_bitdeth,
	unsigned int rdma_enable);

extern void vdin_urgent_patch_resume(unsigned int offset);
extern void vdin_set_drm_data(struct vdin_dev_s *devp,
		struct vframe_s *vf);
extern u32 vdin_get_curr_field_type(struct vdin_dev_s *devp);
extern void vdin_set_source_type(struct vdin_dev_s *devp,
		struct vframe_s *vf);
extern void vdin_set_source_mode(struct vdin_dev_s *devp,
	   struct vframe_s *vf);
extern void vdin_set_source_bitdepth(struct vdin_dev_s *devp,
		struct vframe_s *vf);
extern void vdin_set_pixel_aspect_ratio(struct vdin_dev_s *devp,
		struct vframe_s *vf);
extern void vdin_set_display_ratio(struct vdin_dev_s *devp,
		struct vframe_s *vf);
extern void vdin_source_bitdepth_reinit(struct vdin_dev_s *devp);
extern void set_invert_top_bot(bool invert_flag);
extern void vdin_clk_onoff(struct vdin_dev_s *devp, bool onoff);
extern enum tvin_force_color_range_e color_range_force;

extern void vdin_vlock_input_sel(unsigned int type,
	enum vframe_source_type_e source_type);
extern void vdin_set_dolby_ll_tunnel(struct vdin_dev_s *devp);
extern void vdin_check_hdmi_hdr(struct vdin_dev_s *devp);
extern void vdin_dobly_mdata_write_en(unsigned int offset, unsigned int en);
extern void vdin_prob_set_xy(unsigned int offset,
		unsigned int x, unsigned int y, struct vdin_dev_s *devp);
extern void vdin_prob_set_before_or_after_mat(unsigned int offset,
		unsigned int x, struct vdin_dev_s *devp);
extern void vdin_prob_get_yuv(unsigned int offset,
		unsigned int *rgb_yuv0,	unsigned int *rgb_yuv1,
		unsigned int *rgb_yuv2);
extern void vdin_prob_matrix_sel(unsigned int offset,
		unsigned int sel, struct vdin_dev_s *devp);
void vdin_change_matrix(unsigned int offset,
			unsigned int matrix_csc);
void vdin_dolby_desc_sc_enable(struct vdin_dev_s *devp,
			       unsigned int  onoff);

#endif

