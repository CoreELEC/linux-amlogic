/*
 * drivers/amlogic/media/di_multi_v3/deinterlace_hw.h
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

#ifndef _DI_HW_H
#define _DI_HW_H
#include <linux/amlogic/media/amvecm/amvecm.h>

#include "../deinterlace/di_pqa.h"

/* if post size < 80, filter of ei can't work */
#define MIN_POST_WIDTH	80
#define MIN_BLEND_WIDTH	27

#define	SKIP_CTRE_NUM	13
/*move from deinterlace.c*/
enum eAFBC_REG {
	eAFBC_ENABLE,
	eAFBC_MODE,
	eAFBC_SIZE_IN,
	eAFBC_DEC_DEF_COLOR,
	eAFBC_CONV_CTRL,
	eAFBC_LBUF_DEPTH,
	eAFBC_HEAD_BADDR,
	eAFBC_BODY_BADDR,
	eAFBC_SIZE_OUT,
	eAFBC_OUT_YSCOPE,
	eAFBC_STAT,
	eAFBC_VD_CFMT_CTRL,
	eAFBC_VD_CFMT_W,
	eAFBC_MIF_HOR_SCOPE,
	eAFBC_MIF_VER_SCOPE,
	eAFBC_PIXEL_HOR_SCOPE,
	eAFBC_PIXEL_VER_SCOPE,
	eAFBC_VD_CFMT_H,
};

enum eAFBC_DEC {
	eAFBC_DEC0,
	eAFBC_DEC1,
};

#define AFBC_REG_INDEX_NUB	(18)
#define AFBC_DEC_NUB		(2)

struct DI_MIF_s {
	unsigned short	luma_x_start0;
	unsigned short	luma_x_end0;
	unsigned short	luma_y_start0;
	unsigned short	luma_y_end0;
	unsigned short	chroma_x_start0;
	unsigned short	chroma_x_end0;
	unsigned short	chroma_y_start0;
	unsigned short	chroma_y_end0;
	unsigned int	nocompress;
	unsigned		set_separate_en:2;
	unsigned		src_field_mode:1;
	unsigned		src_prog:1;
	unsigned		video_mode:1;
	unsigned		output_field_num:1;
	unsigned		bit_mode:2;
	/*
	 * unsigned		burst_size_y:2; set 3 as default
	 * unsigned		burst_size_cb:2;set 1 as default
	 * unsigned		burst_size_cr:2;set 1 as default
	 */
	unsigned		canvas0_addr0:8;
	unsigned		canvas0_addr1:8;
	unsigned		canvas0_addr2:8;
};

struct DI_SIM_MIF_s {
	unsigned short	start_x;
	unsigned short	end_x;
	unsigned short	start_y;
	unsigned short	end_y;
	unsigned short	canvas_num;
	unsigned int	bit_mode	:4;
	unsigned int	set_separate_en	:4; /*ary add below is only for wr buf*/

	unsigned int	video_mode	:4;
	unsigned int	ddr_en		:1;
	unsigned int	urgent		:1;
	unsigned int	l_endian	:1;
	unsigned int	cbcr_swap	:1;
	unsigned int	reg_swap	:1; /* 64bits swap */

	unsigned int	reserved	:15;
};

struct DI_MC_MIF_s {
	unsigned short start_x;
	unsigned short start_y;
	unsigned short end_y;
	unsigned short size_x;
	unsigned short size_y;
	unsigned short canvas_num;
	unsigned short blend_en;
	unsigned short vecrd_offset;
};

enum gate_mode_e {
	GATE_AUTO,
	GATE_ON,
	GATE_OFF,
};

struct mcinfo_lmv_s {
	unsigned char	lock_flag;
	char		lmv;
	unsigned short	lock_cnt;
};

struct di_pq_parm_s {
	struct am_pq_parm_s pq_parm;
	struct am_reg_s *regs;
	struct list_head list;
};

/***********************************************
 * setting rebuild
 *	by ary
 ***********************************************/
#ifdef MARK_HIS
enum EDI_MIFSM {
	EDI_MIFSM_NR,
	EDI_MIFSM_WR,
};
#endif

enum EDI_MIFSR {
	EDI_MIFS_X,
	EDI_MIFS_Y,
	EDI_MIFS_CTRL,
};

/*keep same as EDI_MIFSM*/
#define EDI_MIFS_NUB	(2)

/*keep same as EDI_MIFSR*/
#define EDI_MIFS_REG_NUB (3)

struct cfg_mifset_s {
	bool ddr_en;
	bool urgent;
	bool l_endian; /* little_endian */
	bool cbcr_swap;
	bool reg_swap; /* 64bits swap */
	//unsigned int bit_mode;
};

/**********************************************/
void dimv3_read_pulldown_info(unsigned int *glb_frm_mot_num,
			    unsigned int *glb_fid_mot_num);

#if 0

void read_new_pulldown_info(struct FlmModReg_t *pFMRegp);
#endif
void dimv3_pulldown_info_clear_g12a(void);
void dimhv3_combing_pd22_window_config(unsigned int width, unsigned int height);
void dimhv3_hw_init(bool pulldown_en, bool mc_enable);
void dimhv3_hw_uninit(void);
void dimhv3_enable_di_pre_aml(struct DI_MIF_s	*di_inp_mif,
			    struct DI_MIF_s	*di_mem_mif,
			    struct DI_MIF_s	*di_chan2_mif,
			    struct DI_SIM_MIF_s	*di_nrwr_mif,
			    struct DI_SIM_MIF_s	*di_mtnwr_mif,
			    struct DI_SIM_MIF_s	*di_contp2rd_mif,
			    struct DI_SIM_MIF_s	*di_contprd_mif,
			    struct DI_SIM_MIF_s	*di_contwr_mif,
			    unsigned char madi_en,
			    unsigned char pre_field_num,
			    unsigned char pre_vdin_link);
void dimhv3_enable_afbc_input(struct vframe_s *vf);

void dimhv3_mc_pre_mv_irq(void);
void dimhv3_enable_mc_di_pre(struct DI_MC_MIF_s *di_mcinford_mif,
			   struct DI_MC_MIF_s *di_mcinfowr_mif,
			   struct DI_MC_MIF_s *di_mcvecwr_mif,
			   unsigned char mcdi_en);
void dimhv3_enable_mc_di_pre_g12(struct DI_MC_MIF_s *di_mcinford_mif,
			       struct DI_MC_MIF_s *di_mcinfowr_mif,
			       struct DI_MC_MIF_s *di_mcvecwr_mif,
			       unsigned char mcdi_en);

void dimhv3_enable_mc_di_post(struct DI_MC_MIF_s *di_mcvecrd_mif,
			    int urgent, bool reverse, int invert_mv);
void dimhv3_enable_mc_di_post_g12(struct DI_MC_MIF_s *di_mcvecrd_mif,
				int urgent, bool reverse, int invert_mv);

void dimhv3_disable_post_deinterlace_2(void);
void dimhv3_initial_di_post_2(int hsize_post, int vsize_post,
			    int hold_line, bool write_en);
void dimhv3_enable_di_post_2(
	struct DI_MIF_s		*di_buf0_mif,
	struct DI_MIF_s		*di_buf1_mif,
	struct DI_MIF_s		*di_buf2_mif,
	struct DI_SIM_MIF_s	*di_diwr_mif,
	struct DI_SIM_MIF_s	*di_mtnprd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en,
	int post_field_num, int hold_line, int urgent,
	int invert_mv, int vskip_cnt
);
void dimhv3_post_switch_buffer(
	struct DI_MIF_s		*di_buf0_mif,
	struct DI_MIF_s		*di_buf1_mif,
	struct DI_MIF_s		*di_buf2_mif,
	struct DI_SIM_MIF_s	*di_diwr_mif,
	struct DI_SIM_MIF_s	*di_mtnprd_mif,
	struct DI_MC_MIF_s	*di_mcvecrd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en,
	int post_field_num, int hold_line, int urgent,
	int invert_mv, bool pd_en, bool mc_enable,
	int vskip_cnt
);
void dimv3_post_read_reverse_irq(bool reverse,
			       unsigned char mc_pre_flag, bool mc_enable);
void dimv3_top_gate_control(bool top_en, bool mc_en);
void dimv3_pre_gate_control(bool enable, bool mc_enable);
void dimv3_post_gate_control(bool gate);
void dimv3_set_power_control(unsigned char enable);
void dimv3_hw_disable(bool mc_enable);
void dimhv3_enable_di_pre_mif(bool enable, bool mc_enable);
void dimhv3_enable_di_post_mif(enum gate_mode_e mode);

void dimhv3_combing_pd22_window_config(unsigned int width, unsigned int height);
void dimhv3_calc_lmv_init(void);
void dimhv3_calc_lmv_base_mcinfo(unsigned int vf_height,
			       unsigned short *mcinfo_adr_v,
			       unsigned int mcinfo_size);
void dimhv3_init_field_mode(unsigned short height);
void dimv3_film_mode_win_config(unsigned int width, unsigned int height);
void dimhv3_pulldown_vof_win_config(struct pulldown_detected_s *wins);
void dimhv3_load_regs(struct di_pq_parm_s *di_pq_ptr);
void dimv3_pre_frame_reset_g12(unsigned char madi_en, unsigned char mcdi_en);
void dimv3_pre_frame_reset(void);
void dimhv3_interrupt_ctrl(unsigned char ma_en,
			 unsigned char det3d_en, unsigned char nrds_en,
			 unsigned char post_wr, unsigned char mc_en);
void dimhv3_txl_patch_prog(int prog_flg, unsigned int cnt, bool mc_en);
bool dimhv3_afbc_is_supported(void);

void dimhv3_afbc_reg_sw(bool on);

void dumpv3_vd2_afbc(void);

int dimv3_print(const char *fmt, ...);

#define DI_MC_SW_OTHER	(1 << 0)
#define DI_MC_SW_REG	(1 << 1)
/*#define DI_MC_SW_POST	(1 << 2)*/
#define DI_MC_SW_IC	(1 << 2)

#define DI_MC_SW_ON_MASK	(DI_MC_SW_REG | DI_MC_SW_OTHER | DI_MC_SW_IC)

void dimhv3_patch_post_update_mc(void);
void dimhv3_patch_post_update_mc_sw(unsigned int cmd, bool on);

void dimv3_rst_protect(bool on);
void dimv3_pre_nr_wr_done_sel(bool on);
void dimv3_arb_sw(bool on);
void dbgv3_set_DI_PRE_CTRL(void);
void div3_async_reset2(void);	/*2019-04-05 add for debug*/

enum DI_HW_POST_CTRL {
	DI_HW_POST_CTRL_INIT,
	DI_HW_POST_CTRL_RESET,
};

void dimhv3_post_ctrl(enum DI_HW_POST_CTRL contr,
		    unsigned int post_write_en);
void dimhv3_int_ctr(unsigned int set_mod, unsigned char ma_en,
		  unsigned char det3d_en, unsigned char nrds_en,
		  unsigned char post_wr, unsigned char mc_en);

void hv3_dbg_reg_set(unsigned int val);

enum eDI_POST_FLOW {
	eDI_POST_FLOW_STEP1_STOP,
	eDI_POST_FLOW_STEP2_START,
/*	eDI_POST_FLOW_STEP3_RESET_INT,*/
};

void div3_post_set_flow(unsigned int post_wr_en, enum eDI_POST_FLOW step);
void postv3_mif_sw(bool on);
void postv3_dbg_contr(void);
void postv3_close_new(void);
void div3_post_reset(void);
void dimhv3_pst_trig_resize(void);

void hpstv3_power_ctr(bool on);
void hpstv3_dbg_power_ctr_trig(unsigned int cmd);
void hpstv3_dbg_mem_pd_trig(unsigned int cmd);
void hpstv3_dbg_trig_gate(unsigned int cmd);
void hpstv3_dbg_trig_mif(unsigned int cmd);
void hpstv3_mem_pd_sw(unsigned int on);
void hpstv3_vd1_sw(unsigned int on);
void hprev3_gl_sw(bool on);

void dimv3_init_setting_once(void);
void dimv3_hw_init_reg(void);

#endif
