/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/deinterlace_hw.h
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
//#include "di_pqa.h"


/* if post size < 80, filter of ei can't work */
#define MIN_POST_WIDTH	80
#define MIN_BLEND_WIDTH	27

#define	SKIP_CTRE_NUM	16
#ifdef MARK_SC2
/*move from deinterlace.c*/
enum EAFBC_REG {
	EAFBC_ENABLE,
	EAFBC_MODE,
	EAFBC_SIZE_IN,
	EAFBC_DEC_DEF_COLOR,
	EAFBC_CONV_CTRL,
	EAFBC_LBUF_DEPTH,
	EAFBC_HEAD_BADDR,
	EAFBC_BODY_BADDR,
	EAFBC_SIZE_OUT,
	EAFBC_OUT_YSCOPE,
	EAFBC_STAT,
	EAFBC_VD_CFMT_CTRL,
	EAFBC_VD_CFMT_W,
	EAFBC_MIF_HOR_SCOPE,
	EAFBC_MIF_VER_SCOPE,
	EAFBC_PIXEL_HOR_SCOPE,
	EAFBC_PIXEL_VER_SCOPE,
	EAFBC_VD_CFMT_H,
};

enum EAFBC_DEC {
	EAFBC_DEC0,
	EAFBC_DEC1,
};

#define AFBC_REG_INDEX_NUB	(18)
#define AFBC_DEC_NUB		(2)
#endif

/* from sc2 */
enum DI_MIF0_ID {
	DI_MIF0_ID_INP		= 0,
	DI_MIF0_ID_CHAN2	= 1,
	DI_MIF0_ID_MEM		= 2,
	DI_MIF0_ID_IF1		= 3,
	DI_MIF0_ID_IF0		= 4,
	DI_MIF0_ID_IF2		= 5, /* end */
};

enum DI_MIF0_SEL {
	DI_MIF0_SEL_INP		= 0,
	DI_MIF0_SEL_CHAN2	= 1,
	DI_MIF0_SEL_MEM		= 2,
	DI_MIF0_SEL_IF1		= 0x10,
	DI_MIF0_SEL_IF0		= 0x20,
	DI_MIF0_SEL_IF2		= 0x40, /* end */

	DI_MIF0_SEL_PRE_ALL	= 0x0f,
	DI_MIF0_SEL_PST_ALL	= 0xf0,
	DI_MIF0_SEL_ALL		= 0xff,
	DI_MIF0_SEL_VD1_IF0	= 0x100, /* before g12a */
	DI_MIF0_SEL_VD2_IF0	= 0x200, /* before g12a */
};

enum DI_MIFS_ID {
	 DI_MIFS_ID_MTNWR,
	 DI_MIFS_ID_MCVECWR,
	 DI_MIFS_ID_MCINFRD,
	 DI_MIFS_ID_MCINFWR,
	 DI_MIFS_ID_CONTP1,
	 DI_MIFS_ID_CONTP2,
	 DI_MIFS_ID_CONTWR,
	 DI_MIFS_ID_MCVECRD,
	 DI_MIFS_ID_MTNRD,
};

#define MIF_NUB		6
#define MIF_REG_NUB	15
/* keep order with mif_contr_reg*/
/* keep order with mif_contr_reg_v3 */
/* keep order with mif_reg_name */
/* refer to dim_get_mif_reg_name */
enum EDI_MIF_REG_INDEX {
	MIF_GEN_REG,
	MIF_GEN_REG2,
	MIF_GEN_REG3,
	MIF_CANVAS0,
	MIF_LUMA_X0,
	MIF_LUMA_Y0,
	MIF_CHROMA_X0,
	MIF_CHROMA_Y0,
	MIF_RPT_LOOP,
	MIF_LUMA0_RPT_PAT,
	MIF_CHROMA0_RPT_PAT,
	MIF_DUMMY_PIXEL,
	MIF_FMT_CTRL,
	MIF_FMT_W,
	MIF_LUMA_FIFO_SIZE,
};

const char *dim_get_mif_reg_name(enum EDI_MIF_REG_INDEX idx);

struct DI_MIF_S {
	unsigned short	luma_x_start0;
	unsigned short	luma_x_end0;
	unsigned short	luma_y_start0;
	unsigned short	luma_y_end0;
	unsigned short	chroma_x_start0;
	unsigned short	chroma_x_end0;
	unsigned short	chroma_y_start0;
	unsigned short	chroma_y_end0;
	/* set_separate_en
	 *	00 : one canvas
	 *	01 : 3 canvas(old 4:2:0).
	 *	10: 2 canvas. (NV21).
	 */
	unsigned int		set_separate_en	:2;
	/* src_field_mode
	 *	1 frame . 0 field.
	 */
	unsigned int		src_field_mode	:1;
	unsigned int		src_prog	:1;
	/*video_mode :
	 *	00: 4:2:0;
	 *	01: 4:2:2;
	 *	10: 4:4:4
	 * 2020-06-02 from 1bit to 2bit
	 */
	unsigned int		video_mode	:2;

	/*bit_mode
	 *	0:8 bits
	 *	1:10 bits 422(old mode,12bit)
	 *	2:10bit 444
	 *	3:10bit 422(full pack) or 444
	 */
	unsigned int		bit_mode	:2;

	unsigned int		canvas0_addr0:8;
	unsigned int		canvas0_addr1:8;
	unsigned int		canvas0_addr2:8;
	/* canvas_w: for input not 64 align*/
	unsigned int	canvas_w;
	/* ary move from parameter to here from sc2 */

	unsigned int hold_line	:8; /*ary: use 6bit*/
	unsigned int vskip_cnt		:4; /*ary 0~8*/
	unsigned int urgent		:1;
	unsigned int wr_en		:1;
	unsigned int rev_x		:1;
	unsigned int rev_y		:1;
	//enable after sc2
	unsigned int burst_size_y	:2; //set 3 as default
	unsigned int burst_size_cb	:2;//set 1 as default
	unsigned int burst_size_cr	:2;//set 1 as default
	/* ary no use*/
	unsigned int nocompress		:1;
	unsigned int		output_field_num:1;
	unsigned int reseved		:8;

};

struct DI_SIM_MIF_s {
	unsigned short	start_x;
	unsigned short	end_x;
	unsigned short	start_y;
	unsigned short	end_y;
	unsigned short	canvas_num;
	/*bit_mode
	 *	0:8 bits
	 *	1:10 bits 422(old mode,12bit)
	 *	2:10bit 444
	 *	3:10bit 422(full pack) or 444
	 */
	unsigned int	bit_mode	:4;
	/* set_separate_en
	 *	00 : one canvas
	 *	01 : 3 canvas(old 4:2:0).
	 *	10: 2 canvas. (NV21).
	 */
	unsigned int	set_separate_en	:4; /*ary add below is only for wr buf*/
	/*video_mode :
	 *	00: 4:2:0;
	 *	01: 4:2:2;
	 *	10: 4:4:4
	 * 2020-06-02 from 1bit to 2bit
	 */
	unsigned int	video_mode	:4;
	unsigned int	ddr_en		:1;
	unsigned int	urgent		:1;
	unsigned int	l_endian	:1;
	unsigned int	cbcr_swap	:1;

	unsigned int	en		:1; /* add for sc2*/
	unsigned int	src_i		:1; /* ary add for sc2 */
	unsigned int	reserved	: 15;
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

enum EDI_MIFSR {
	EDI_MIFS_X, //->WRMIF_X
	EDI_MIFS_Y,	//->WRMIF_Y
	EDI_MIFS_CTRL,	//->WRMIF_CTRL
};

/* keep order with reg_wrmif_v3 */
enum DI_WRMIF_INDEX_V3 {
	WRMIF_X,
	WRMIF_Y,
	WRMIF_CTRL,
	WRMIF_URGENT,
	WRMIF_CANVAS,
	WRMIF_DBG_AXI_CMD_CNT,
	WRMIF_DBG_AXI_DAT_CNT,
};

#define DIM_WRMIF_MIF_V3_NUB		(2)

#define DIM_WRMIF_SET_V3_NUB	(7)

/* keep order with reg_bits_wr */
/* mif bits define */
enum ENR_MIF_INDEX {
	ENR_MIF_INDEX_X_ST	= 0,
	ENR_MIF_INDEX_X_END,
	ENR_MIF_INDEX_Y_ST,
	ENR_MIF_INDEX_Y_END,
	ENR_MIF_INDEX_CVS,
	ENR_MIF_INDEX_EN,
	ENR_MIF_INDEX_BIT_MODE,
	ENR_MIF_INDEX_ENDIAN,
	ENR_MIF_INDEX_URGENT,
	ENR_MIF_INDEX_CBCR_SW,
	ENR_MIF_INDEX_RGB_MODE,
	/*below is only for sc2*/
	ENR_MIF_INDEX_DBG_CMD_CNT,
	ENR_MIF_INDEX_DBG_DAT_CNT,
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
	//unsigned int bit_mode;
};

struct hw_sc2_ctr_pre_s {
	unsigned int afbc_en		: 1;	//nrwr_path_sel
	unsigned int mif_en		: 1; /*nr mif*/
	unsigned int nr_ch0_en		: 1;
	unsigned int is_4k		: 1;
	/*0:internal  1:pre-post link  2:viu  3:vcp(vdin)*/
	unsigned int pre_frm_sel	: 4;

	unsigned int afbc_nr_en		: 1;
	unsigned int afbc_inp		: 1;
	unsigned int afbc_mem		: 1;
	unsigned int afbc_chan2		: 1;

	unsigned int reserve2		: 20;
};

struct hw_sc2_ctr_pst_s {
	unsigned int afbc_en		: 1;	//nrwr_path_sel
	unsigned int mif_en		: 1;
	unsigned int is_4k		: 1;
	unsigned int reserve1		: 1;
	/*0:viu  1:internal  2:pre-post link (post timming)*/
	unsigned int post_frm_sel	: 4;

	unsigned int afbc_if0		: 1;
	unsigned int afbc_if1		: 1;
	unsigned int afbc_if2		: 1;
	unsigned int afbc_wr		: 1;

	unsigned int reserve2		: 2;
};

/**********************************************/
void dim_read_pulldown_info(unsigned int *glb_frm_mot_num,
			    unsigned int *glb_fid_mot_num);

#ifdef MARK_HIS
void read_new_pulldown_info(struct FlmModReg_t *pFMRegp);
#endif
void dim_pulldown_info_clear_g12a(void);
void dimh_combing_pd22_window_config(unsigned int width, unsigned int height);
void dimh_hw_init(bool pulldown_en, bool mc_enable);
void dimh_hw_uninit(void);
void dimh_enable_di_pre_aml(struct DI_MIF_S	*di_inp_mif,
			    struct DI_MIF_S	*di_mem_mif,
			    struct DI_MIF_S	*di_chan2_mif,
			    struct DI_SIM_MIF_s	*di_nrwr_mif,
			    struct DI_SIM_MIF_s	*di_mtnwr_mif,
			    struct DI_SIM_MIF_s	*di_contp2rd_mif,
			    struct DI_SIM_MIF_s	*di_contprd_mif,
			    struct DI_SIM_MIF_s	*di_contwr_mif,
			    unsigned char madi_en,
			    unsigned char pre_field_num,
			    unsigned char pre_vdin_link);
//void dimh_enable_afbc_input(struct vframe_s *vf);

void dimh_mc_pre_mv_irq(void);
void dimh_enable_mc_di_pre(struct DI_MC_MIF_s *di_mcinford_mif,
			   struct DI_MC_MIF_s *di_mcinfowr_mif,
			   struct DI_MC_MIF_s *di_mcvecwr_mif,
			   unsigned char mcdi_en);
void dimh_enable_mc_di_pre_g12(struct DI_MC_MIF_s *di_mcinford_mif,
			       struct DI_MC_MIF_s *di_mcinfowr_mif,
			       struct DI_MC_MIF_s *di_mcvecwr_mif,
			       unsigned char mcdi_en);

void dimh_enable_mc_di_post(struct DI_MC_MIF_s *di_mcvecrd_mif,
			    int urgent, bool reverse, int invert_mv);
void dimh_en_mc_di_post_g12(struct DI_MC_MIF_s *di_mcvecrd_mif,
			    int urgent, bool reverse, int invert_mv);

void dimh_disable_post_deinterlace_2(void);
void dimh_initial_di_post_2(int hsize_post, int vsize_post,
			    int hold_line, bool write_en);
void dimh_enable_di_post_2(struct DI_MIF_S *di_buf0_mif,
			   struct DI_MIF_S *di_buf1_mif,
			   struct DI_MIF_S *di_buf2_mif,
			   struct DI_SIM_MIF_s *di_diwr_mif,
			   struct DI_SIM_MIF_s *di_mtnprd_mif,
			   int ei_en, int blend_en, int blend_mtn_en,
			   int blend_mode, int di_vpp_en,
			   int di_ddr_en, int post_field_num,
			   int hold_line, int urgent,
			   int invert_mv, int vskip_cnt
);

void dimh_post_switch_buffer(struct DI_MIF_S *di_buf0_mif,
			     struct DI_MIF_S *di_buf1_mif,
			     struct DI_MIF_S *di_buf2_mif,
			     struct DI_SIM_MIF_s *di_diwr_mif,
			     struct DI_SIM_MIF_s *di_mtnprd_mif,
			     struct DI_MC_MIF_s	*di_mcvecrd_mif,
			     int ei_en, int blend_en,
			     int blend_mtn_en, int blend_mode,
			     int di_vpp_en, int di_ddr_en,
			     int post_field_num, int hold_line,
			     int urgent,
			     int invert_mv, bool pd_en, bool mc_enable,
			     int vskip_cnt
);
void dim_post_read_reverse_irq(bool reverse,
			       unsigned char mc_pre_flag, bool mc_enable);
void dim_top_gate_control(bool top_en, bool mc_en);
void dim_top_gate_control_sc2(bool top_en, bool mc_en);
void dim_pre_gate_control(bool enable, bool mc_enable);
void dim_pre_gate_control_sc2(bool enable, bool mc_enable);
void dim_post_gate_control(bool gate);
void dim_post_gate_control_sc2(bool gate);
void dim_set_power_control(unsigned char enable);
void dim_hw_disable(bool mc_enable);
void dimh_enable_di_pre_mif(bool enable, bool mc_enable);
void dimh_enable_di_post_mif(enum gate_mode_e mode);

void dimh_combing_pd22_window_config(unsigned int width, unsigned int height);
void dimh_calc_lmv_init(void);
void dimh_calc_lmv_base_mcinfo(unsigned int vf_height,
			       unsigned short *mcinfo_adr_v,
			       unsigned int mcinfo_size);
void dimh_init_field_mode(unsigned short height);
void dim_film_mode_win_config(unsigned int width, unsigned int height);
void dimh_pulldown_vof_win_config(struct pulldown_detected_s *wins);
void dimh_load_regs(struct di_pq_parm_s *di_pq_ptr);
void dim_pre_frame_reset_g12(unsigned char madi_en, unsigned char mcdi_en);
void dim_pre_frame_reset(void);
void dimh_interrupt_ctrl(unsigned char ma_en,
			 unsigned char det3d_en, unsigned char nrds_en,
			 unsigned char post_wr, unsigned char mc_en);
void dimh_txl_patch_prog(int prog_flg, unsigned int cnt, bool mc_en);
#ifdef MARK_SC2
bool dimh_afbc_is_supported(void);

void dimh_afbc_reg_sw(bool on);

void dump_vd2_afbc(void);
#endif
int dim_print(const char *fmt, ...);

#define DI_MC_SW_OTHER	(0x1 << 0)
#define DI_MC_SW_REG	(0x1 << 1)
/*#define DI_MC_SW_POST	(1 << 2)*/
#define DI_MC_SW_IC	(0x1 << 2)

#define DI_MC_SW_ON_MASK	(DI_MC_SW_REG | DI_MC_SW_OTHER | DI_MC_SW_IC)

void dimh_patch_post_update_mc(void);
void dimh_patch_post_update_mc_sw(unsigned int cmd, bool on);

void dim_rst_protect(bool on);
void dim_pre_nr_wr_done_sel(bool on);
void dim_arb_sw(bool on);
void dbg_set_DI_PRE_CTRL(void);
void di_async_reset2(void);	/*2019-04-05 add for debug*/

enum DI_HW_POST_CTRL {
	DI_HW_POST_CTRL_INIT,
	DI_HW_POST_CTRL_RESET,
};

void dimh_post_ctrl(enum DI_HW_POST_CTRL contr,
		    unsigned int post_write_en);
void dimh_int_ctr(unsigned int set_mod, unsigned char ma_en,
		  unsigned char det3d_en, unsigned char nrds_en,
		  unsigned char post_wr, unsigned char mc_en);

void h_dbg_reg_set(unsigned int val);
enum EDI_POST_FLOW {
	EDI_POST_FLOW_STEP1_STOP,
	EDI_POST_FLOW_STEP2_START,
	EDI_POST_FLOW_STEP3_IRQ,
};

void di_post_set_flow(unsigned int post_wr_en, enum EDI_POST_FLOW step);
void post_mif_sw(bool on);
void post_dbg_contr(void);
void post_close_new(void);
void di_post_reset(void);
void dimh_pst_trig_resize(void);

void hpst_power_ctr(bool on);
void hpst_dbg_power_ctr_trig(unsigned int cmd);
void hpst_dbg_mem_pd_trig(unsigned int cmd);
void hpst_dbg_trig_gate(unsigned int cmd);
void hpst_dbg_trig_mif(unsigned int cmd);
void hpst_mem_pd_sw(unsigned int on);
void hpst_vd1_sw(unsigned int on);
void hpre_gl_sw(bool on);

void dim_init_setting_once(void);
void dim_hw_init_reg(void);

/******************************************************************
 * hw ops
 *****************************************************************/
struct dim_hw_opsv_info_s {
	char name[32];
	char update[32];
	unsigned int main_version;
	unsigned int sub_version;
};

struct regs_t {
	unsigned int add;/*add index*/
	unsigned int bit;
	unsigned int wid;
	unsigned int id;
//	unsigned int df_val;
	char *name;
};

struct di_buf_s;
struct di_win_s;

struct dim_hw_opsv_s {
	struct dim_hw_opsv_info_s	info;
	void (*pre_mif_set)(struct DI_MIF_S *mif,
			    enum DI_MIF0_ID mif_index,
			    const struct reg_acc *op);
	void (*pst_mif_set)(struct DI_MIF_S *mif,
			    enum DI_MIF0_ID mif_index,
			    const struct reg_acc *op);
	void (*pst_mif_update_csv)(struct DI_MIF_S *mif,
				   enum DI_MIF0_ID mif_index,
				   const struct reg_acc *op);
	void (*pre_mif_sw)(bool enable);
	void (*pst_mif_sw)(bool on, enum DI_MIF0_SEL sel);
	void (*pst_mif_rst)(enum DI_MIF0_SEL sel);
	void (*pst_mif_rev)(bool rev, enum DI_MIF0_SEL sel);
	void (*pst_dbg_contr)(void);
	void (*pst_set_flow)(unsigned int post_wr_en, enum EDI_POST_FLOW step);
	void (*pst_bit_mode_cfg)(unsigned char if0,
				 unsigned char if1,
				 unsigned char if2,
				 unsigned char post_wr);
	void (*wr_cfg_mif)(struct DI_SIM_MIF_s *wr_mif,
			   enum EDI_MIFSM index,
			   /*struct di_buf_s *di_buf,*/
			   void *di_vf,
			   struct di_win_s *win);
	void (*wrmif_set)(struct DI_SIM_MIF_s *cfg_mif,
			  const struct reg_acc *ops,
			  enum EDI_MIFSM mifsel);
	void (*wrmif_sw_buf)(struct DI_SIM_MIF_s *cfg_mif,
			     const struct reg_acc *ops,
			     enum EDI_MIFSM mifsel);
	void (*wrmif_trig)(enum EDI_MIFSM mifsel);
	void (*wr_rst_protect)(bool on);
	void (*hw_init)(void);
	void (*pre_hold_block_txlx)(void);
	void (*pre_cfg_mif)(struct DI_MIF_S *di_mif,
			    enum DI_MIF0_ID mif_index,
			    struct di_buf_s *di_buf,
			    unsigned int ch);
	void (*dbg_reg_pre_mif_print)(void);
	void (*dbg_reg_pst_mif_print)(void);
	void (*dbg_reg_pre_mif_print2)(void);
	void (*dbg_reg_pst_mif_print2)(void);
	void (*dbg_reg_pre_mif_show)(struct seq_file *s);
	void (*dbg_reg_pst_mif_show)(struct seq_file *s);

	/* control */
	void (*pre_gl_sw)(bool on);
	void (*pre_gl_thd)(void);
	void (*pst_gl_thd)(unsigned int hold_line);
	const unsigned int *reg_mif_tab[MIF_NUB];
	const struct reg_t *rtab_contr_bits_tab;
};

void dbg_mif_reg(struct seq_file *s, enum DI_MIF0_ID eidx);

extern const struct dim_hw_opsv_s dim_ops_l1_v2;
const unsigned int *mif_reg_get_v2(enum DI_MIF0_ID mif_index);//debug
extern const struct reg_acc di_pre_regset;
void config_di_mif_v3(struct DI_MIF_S *di_mif,
		      enum DI_MIF0_ID mif_index,
		      struct di_buf_s *di_buf,
		      unsigned int ch);//debug only

void set_di_mif_v3(struct DI_MIF_S *mif,
		   enum DI_MIF0_ID mif_index,
		   const struct reg_acc *op);//debug only

const char *dim_get_mif_id_name(enum EDI_MIF_REG_INDEX idx);
#endif
