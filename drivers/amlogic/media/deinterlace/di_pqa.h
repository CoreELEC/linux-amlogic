/*
 * drivers/amlogic/media/deinterlace/di_pqa.h
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

#ifndef __DI_PQA_H__
#define __DI_PQA_H__

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>

#include <linux/device.h>
#include "../deinterlace/pulldown_drv.h"
#include "../deinterlace/deinterlace_mtn.h"
#if 0 /*move from pulldown_drv.h to di_pq.h*/
enum pulldown_mode_e {
	PULL_DOWN_BLEND_0	= 0,/* buf1=dup[0] */
	PULL_DOWN_BLEND_2	= 1,/* buf1=dup[2] */
	PULL_DOWN_MTN		= 2,/* mtn only */
	PULL_DOWN_BUF1		= 3,/* do wave with dup[0] */
	PULL_DOWN_EI		= 4,/* ei only */
	PULL_DOWN_NORMAL	= 5,/* normal di */
	PULL_DOWN_NORMAL_2	= 6,/* di */
};

struct pulldown_vof_win_s {
	unsigned short win_vs;
	unsigned short win_ve;
	enum pulldown_mode_e blend_mode;
};

struct pulldown_detected_s {
	enum pulldown_mode_e global_mode;
	struct pulldown_vof_win_s regs[4];
};
#endif

#if 0	/*move from deinterlace_mtn.h*/
struct combing_status_s {
	unsigned int frame_diff_avg;
	unsigned int cmb_row_num;
	unsigned int field_diff_rate;
	int like_pulldown22_flag;
	unsigned int cur_level;
};
#endif
/**************************
 * pulldown
 *************************/
struct pulldown_op_s {
	unsigned char (*init)(unsigned short width, unsigned short height);
	unsigned int (*detection)(struct pulldown_detected_s *res,
				  struct combing_status_s *cmb_sts,
				  bool reverse,
				  struct vframe_s *vf);
	void (*vof_win_vshift)(struct pulldown_detected_s *wins,
			       unsigned short v_offset);
	int (*module_para)(struct seq_file *seq);
	void (*prob)(struct device *dev);
	void (*remove)(struct device *dev);
};

bool di_attach_ops_pulldown(const struct pulldown_op_s **ops);

/**************************
 * detect3d
 *************************/
struct detect3d_op_s {
	void (*det3d_config)(bool flag);
	enum tvin_trans_fmt (*det3d_fmt_detect)(void);
	int (*module_para)(struct seq_file *seq);
};

bool di_attach_ops_3d(const struct detect3d_op_s **ops);

/**************************
 * nr_drv
 *************************/
struct nr_op_s {
	void (*nr_hw_init)(void);
	void (*nr_gate_control)(bool gate);
	void (*nr_drv_init)(struct device *dev);
	void (*nr_drv_uninit)(struct device *dev);
	void (*nr_process_in_irq)(void);
	void (*nr_all_config)(unsigned short nCol, unsigned short nRow,
			      unsigned short type);
	bool (*set_nr_ctrl_reg_table)(unsigned int addr, unsigned int value);
	void (*cue_int)(struct vframe_s *vf);
	void (*adaptive_cue_adjust)(unsigned int frame_diff,
				    unsigned int field_diff);
	int (*module_para)(struct seq_file *seq);

};

bool di_attach_ops_nr(const struct nr_op_s **ops);

/**************************
 * deinterlace_mtn
 *************************/
struct mtn_op_s {
	void (*mtn_int_combing_glbmot)(void);
	void (*adpative_combing_exit)(void);
	void (*fix_tl1_1080i_patch_sel)(unsigned int mode);
	int (*adaptive_combing_fixing)(
		struct combing_status_s *cmb_status,
		unsigned int field_diff, unsigned int frame_diff,
		int bit_mode);
	/*adpative_combing_config*/
	struct combing_status_s *
		(*adpative_combing_config)(unsigned int width,
					   unsigned int height,
					   enum vframe_source_type_e src_type,
					   bool prog,
					   enum tvin_sig_fmt_e fmt);
	void (*com_patch_pre_sw_set)(unsigned int mode);
	int (*module_para)(struct seq_file *seq);
};

bool di_attach_ops_mtn(const struct mtn_op_s **ops);

/**********************************************************
 *
 * IC VERSION
 *
 **********************************************************/
#define DI_IC_ID_M8B		(0x01)
#define DI_IC_ID_GXBB		(0x02)
#define DI_IC_ID_GXTVBB		(0x03)
#define DI_IC_ID_GXL		(0x04)
#define DI_IC_ID_GXM		(0x05)
#define DI_IC_ID_TXL		(0x06)
#define DI_IC_ID_TXLX		(0x07)
#define DI_IC_ID_AXG		(0x08)
#define DI_IC_ID_GXLX		(0x09)
#define DI_IC_ID_TXHD		(0x0a)

#define DI_IC_ID_G12A		(0x10)
#define DI_IC_ID_G12B		(0x11)
#define DI_IC_ID_SM1		(0x13)
#define DI_IC_ID_TL1		(0x16)
#define DI_IC_ID_TM2		(0x17)
#define DI_IC_ID_TM2B		(0x18)

#define DI_IC_ID_SC2		(0x1B)

/* is_meson_g12a_cpu */
static inline bool is_ic_named(unsigned int crr_id, unsigned int ic_id)
{
	if (crr_id == ic_id)
		return true;
	return false;
}

/*cpu_after_eq*/
static inline bool is_ic_after_eq(unsigned int crr_id, unsigned int ic_id)
{
	if (crr_id >= ic_id)
		return true;
	return false;
}

static inline bool is_ic_before(unsigned int crr_id, unsigned int ic_id)
{
	if (crr_id < ic_id)
		return true;
	return false;
}

#define IS_IC(cid, cc)		is_ic_named(cid, DI_IC_ID_##cc)
#define IS_IC_EF(cid, cc)	is_ic_after_eq(cid, DI_IC_ID_##cc)
#define IS_IC_BF(cid, cc)	is_ic_before(cid, DI_IC_ID_##cc)

struct reg_acc {
	void (*wr)(unsigned int adr, unsigned int val);
	unsigned int (*rd)(unsigned int adr);
	unsigned int (*bwr)(unsigned int adr, unsigned int val,
			    unsigned int start, unsigned int len);
	unsigned int (*brd)(unsigned int adr, unsigned int start,
			    unsigned int len);

};

/************************************************
 * afbc
 ************************************************/
enum EAFBC_DEC {
	EAFBC_DEC0,
	EAFBC_DEC1,
	EAFBC_DEC2_DI,
	EAFBC_DEC3_MEM,
	EAFBC_DEC_CHAN2,
	EAFBC_DEC_IF0,
	EAFBC_DEC_IF1,
	EAFBC_DEC_IF2,
	/*sc2*/
};

enum EAFBC_ENC {
	EAFBC_ENC0,
	EAFBC_ENC1,
};

enum EAFBC_CFG {
	EAFBC_CFG_EN,
	EAFBC_CFG_PMODE,
	EAFBC_CFG_EMODE,
	EAFBC_CFG_ETEST,
	EAFBC_CFG_4K,
	EAFBC_CFG_PRE_LINK,
	EAFBC_CFG_PAUSE,
	EAFBC_CFG_LEVE3,
};

struct afbce_map_s {
	unsigned long tabadd;
	unsigned long bodyadd;
	unsigned int size_buf;
	unsigned int size_tab;
};

struct afbcd_ctr_s {
	struct {
		unsigned int ver	: 8,

			sp_inp		: 1,
			sp_mem		: 1,
			/*0:inp only, 1: inp + mem*/
			sp_chan2	: 1,
			sp_if0		: 1,
			sp_if1		: 1,
			sp_if2		: 1,
			rev1		: 2,

			/* mode
			 *	0: 1 afbcd
			 *	1: p: 2afbcd + 1 afbce
			 *	2: i: 6afbcd + 2 afbce
			 *	3:
			 */
			mode		: 2,
			rev2		: 2,
			rev3		: 4,

			pre_dec		: 4,
			mem_dec		: 4,

			ch2_dec		: 4,
			if0_dec		: 4,

			if1_dec		: 4,
			if2_dec		: 4,

			rev4		: 8;
	} fb;

	union {
		unsigned int d32;
		struct {
		unsigned int int_flg	: 1, /*addr ini*/
			en		: 1,
			enc_err		: 1,
			rev1		: 5,

			chg_level	: 2,
			chg_mem		: 2, /*add*/
			rev2		: 4,

			rev3		: 8,
			rev4		: 8;
		} b;
	};
	unsigned int size_tab;
	unsigned int size_info;
	unsigned int l_vtype;
	unsigned int l_vt_mem;
	unsigned int l_h;
	unsigned int l_w;
	unsigned int l_bitdepth;
	unsigned int addr_h;
	unsigned int addr_b;
	unsigned int mem_addr_h;
	unsigned int mem_addr_b;
};

struct afd_s {
	struct afbcd_ctr_s ctr;
};

struct afd_ops_s {
	void (*prob)(unsigned int cid, struct afd_s *p);
	bool (*is_supported)(void);
	u32 (*en_pre_set)(struct vframe_s *inp_vf,
			  struct vframe_s *mem_vf,
			  struct vframe_s *nr_vf);
	void (*inp_sw)(bool on);
	bool (*is_used)(void);
	bool (*is_free)(void);/*?*/
	bool (*is_cfg)(enum EAFBC_CFG cfg_cmd);
	unsigned int (*count_info_size)(unsigned int w, unsigned int h);
	unsigned int (*count_tab_size)(unsigned int buf_size);
	void (*int_tab)(struct device *dev,
			struct afbce_map_s *pcfg);
	void (*dump_reg)(void);
	void (*reg_sw)(bool on);
	void (*reg_val)(void);
	u32 (*rqst_share)(bool onoff);
	const unsigned int *(*get_d_addrp)(enum EAFBC_DEC eidx);
	const unsigned int *(*get_e_addrp)(enum EAFBC_ENC eidx);
	u32 (*dbg_rqst_share)(bool onoff);
};

enum EDI_MIFSM {
	EDI_MIFSM_NR,
	EDI_MIFSM_WR,
};

int dbg_afbc_cfg_show(struct seq_file *s, void *v);/*debug*/
void dbg_afbcd_bits_show(struct seq_file *s, enum EAFBC_DEC eidx);
void dbg_afd_reg(struct seq_file *s, enum EAFBC_DEC eidx);
void dbg_afbce_bits_show(struct seq_file *s, enum EAFBC_ENC eidx);
void dbg_afe_reg(struct seq_file *s, enum EAFBC_ENC eidx);
bool dbg_di_prelink(void);
void dbg_di_prelink_reg_check(void);

/**/
int dbg_afbc_cfg_v3_show(struct seq_file *s, void *v);/*debug*/
void dbg_afd_reg_v3(struct seq_file *s, enum EAFBC_DEC eidx);

bool di_attach_ops_afd_v3(const struct afd_ops_s **ops);

/************************************************
 * ext api for di
 ************************************************/

struct ext_ops_s {
	void (*switch_vpu_mem_pd_vmod)(unsigned int vmod, bool on);
/*	char *(*vf_get_receiver_name)(const char *provider_name);*/
	void (*switch_vpu_clk_gate_vmod)(unsigned int vmod, int flag);
	int (*get_current_vscale_skip_count)(struct vframe_s *vf);
	u32 (*cvs_alloc_table)(const char *owner, u32 *tab,
			       int size,
			       enum canvas_map_type_e type);
	u32 (*cvs_free_table)(u32 *tab, int size);
};

bool dil_attch_ext_api(const struct ext_ops_s **exp_4_di);

#endif	/*__DI_PQA_H__*/
