/*
 * drivers/amlogic/media/di_multi_v3/deinterlace.h
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

#ifndef _DI_H
#define _DI_H
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/amlogic/media/di/di_interface.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>

#include "../di_local/di_local.h"
#include <linux/clk.h>
#include <linux/atomic.h>
#include "deinterlace_hw.h"
#include "../deinterlace/di_pqa.h"
/************************************************
 * config define
 ***********************************************/
#define DIM_OUT_NV21	(1)

/*trigger_pre_di_process param*/
#define TRIGGER_PRE_BY_PUT			'p'
#define TRIGGER_PRE_BY_DE_IRQ			'i'
#define TRIGGER_PRE_BY_UNREG			'u'
/*di_timer_handle*/
#define TRIGGER_PRE_BY_TIMER			't'
#define TRIGGER_PRE_BY_FORCE_UNREG		'f'
#define TRIGGER_PRE_BY_VFRAME_READY		'r'
#define TRIGGER_PRE_BY_PROVERDER_UNREG		'n'
#define TRIGGER_PRE_BY_DEBUG_DISABLE		'd'
#define TRIGGER_PRE_BY_PROVERDER_REG		'R'

#define DI_RUN_FLAG_RUN				0
#define DI_RUN_FLAG_PAUSE			1
#define DI_RUN_FLAG_STEP			2
#define DI_RUN_FLAG_STEP_DONE			3

#define USED_LOCAL_BUF_MAX			3
#define BYPASS_GET_MAX_BUF_NUM			4

/* buffer management related */
#define MAX_IN_BUF_NUM				(4)
#define MAX_LOCAL_BUF_NUM			(7)
#define MAX_POST_BUF_NUM			(7)	/*(5)*/ /* 16 */
/****************************************************
 * one post buf: 0x4fb000
 ***************************************************/
#define VFRAME_TYPE_IN				1
#define VFRAME_TYPE_LOCAL			2
#define VFRAME_TYPE_POST			3
#define VFRAME_TYPE_NUM				3

#define DI_POST_GET_LIMIT			4
#define DI_PRE_READY_LIMIT			4
/*vframe define*/
#define vframe_t struct vframe_s

//no use#define is_from_vdin(vframe) ((vframe)->type & VIDTYPE_VIU_422)
/***********************************/
/*replace vframe_s:*/
struct dim_itf_u {
	struct vframe_s		vfm;
	struct di_buffer	dbuf; /*option for ins*/
};
/**********************************/

/* canvas defination */
#define DI_USE_FIXED_CANVAS_IDX
/*#define	DET3D */
#undef SUPPORT_MPEG_TO_VDIN
#define CLK_TREE_SUPPORT
#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
#ifndef VSYNC_WR_MPEG_REG
#define VSYNC_WR_MPEG_REG(adr, val) aml_write_vcbus(adr, val)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len)	\
		aml_vcbus_update_bits((adr),		\
		((1 << (len)) - 1) << (start), (val) << (start))

#define VSYNC_RD_MPEG_REG(adr) aml_read_vcbus(adr)
#endif
#endif

#define IS_VDIN_SRC(src) (				\
	((src) == VFRAME_SOURCE_TYPE_TUNER)	||	\
	((src) == VFRAME_SOURCE_TYPE_CVBS)	||	\
	((src) == VFRAME_SOURCE_TYPE_COMP)	||	\
	((src) == VFRAME_SOURCE_TYPE_HDMI))

#define VFMT_IS_I(vftype) ((vftype) & VIDTYPE_INTERLACE_BOTTOM)

#define IS_COMP_MODE(vftype) ((vftype) & VIDTYPE_COMPRESS)

#define VFMT_SET_TOP(vftype)	(((vftype) & (~VIDTYPE_TYPEMASK)) | \
				 VIDTYPE_INTERLACE_TOP)

#define VFMT_SET_BOTTOM(vftype)	(((vftype) & (~VIDTYPE_TYPEMASK)) | \
				 VIDTYPE_INTERLACE_BOTTOM)

#define VFMT_IS_TOP(vfm)	(((vfm) & VIDTYPE_TYPEMASK) ==	\
				VIDTYPE_INTERLACE_TOP)

enum process_fun_index_e {
	PROCESS_FUN_NULL = 0,
	PROCESS_FUN_DI,
	PROCESS_FUN_PD,
	PROCESS_FUN_PROG,
	PROCESS_FUN_BOB
};

#define process_fun_index_t enum process_fun_index_e

enum canvas_idx_e {
	NR_CANVAS,
	MTN_CANVAS,
	MV_CANVAS,
};

#define pulldown_mode_t enum pulldown_mode_e

struct mcinfo_pre_s {
	unsigned int highvertfrqflg;
	unsigned int motionparadoxflg;
	unsigned int regs[26];/* reg 0x2fb0~0x2fc9 */
};
/**********************************************************
 *
 *********************************************************/
enum EDIM_TMODE {
	EDIM_TMODE_NONE,
	EDIM_TMODE_1_PW_VFM,
	/* EDIM_TMODE_1_PW_LOCAL ******
	 * pre + post write
	 * all buf alloc by di
	 * use vframe event
	 ******************************/
	EDIM_TMODE_2_PW_OUT,
	/* EDIM_TMODE_2_PRE_OUT ******
	 * pre + post write
	 * post buf alloc by other module
	 * not use vframe path
	 * add 2019-11-26 for zhouzhi
	 ******************************/
	EDIM_TMODE_3_PW_LOCAL,
	/* EDIM_TMODE_2_PRE_OUT ******
	 * pre + post write
	 * post buf alloc by self
	 * not use vframe path
	 * add 2019-12-04 for test
	 ******************************/
};

struct dim_wmode_s {
	enum EDIM_TMODE		tmode;
	unsigned int		buf_type;	/*add this to split kinds */
	unsigned int	is_afbc		:1,
		is_vdin			:1,
		is_i			:1,
		need_bypass		:1,
		is_bypass		:1,
		pre_bypass		:1,
		post_bypass		:1,
		flg_keep		:1, /*keep buf*/

		trick_mode		:1,
		prog_proc_config	:1, /*debug only: proc*/
	/**************************************
	 *prog_proc_config:	same as p_as_i?
	 *1: process p from decoder as field
	 *0: process p from decoder as frame
	 ***************************************/
		is_invert_tp		:1,
		p_as_i			:1,
		p_use_2i		:1,
		is_angle		:1,
		is_top			:1, /*include */
		is_eos			:1,

		reserved		:16;
	unsigned int vtype;	/*vfm->type*/
	//unsigned int h;	/*taget h*/
	//unsigned int w;	/*taget w*/
	unsigned int src_h;
	unsigned int src_w;
	unsigned int tgt_h;
	unsigned int tgt_w;
	unsigned int o_h;
	unsigned int o_w;
	unsigned int seq;
	unsigned int seq_pre;
};

#define DIM_VFM_PROGRESSIVE             0x000000
#define DIM_VFM_INTERLACE_TOP           0x000001
#define DIM_VFM_INTERLACE_BOTTOM        0x000003
#define DIM_VFM_TYPEMASK                0x000007
#define DIM_VFM_INTERLACE               0x000001
#define DIM_VFM_VIU_NV12                0x80
#define DIM_VFM_VIU_422                 0x000800
#define DIM_VFM_VIU_444                 0x004000
#define DIM_VFM_VIU_NV21                0x008000
#define DIM_VFM_COMPRESS                0x100000

/*apply:*/
#define DIM_VFM_MASK_ALL		0x10c887
#define DIM_VFM_LOCAL_T			(DIM_VFM_INTERLACE_TOP |	\
					 DIM_VFM_VIU_422)
#define DIM_VFM_LOCAL_B			(DIM_VFM_INTERLACE_BOTTOM |	\
						 DIM_VFM_VIU_422)

#define DIM_VFMT_IS_I(vftype) ((vftype) & DIM_VFM_INTERLACE_BOTTOM)
#define DIM_VFMT_IS_TOP(vfm)	(((vfm) & DIM_VFM_TYPEMASK) ==	\
				DIM_VFM_INTERLACE_TOP)
#define DIM_VFMT_IS_P(vftype) (((vftype) & DIM_VFM_TYPEMASK) == 0)


/*use this replace vframe data in di work*/
struct dim_vmode_s {
	unsigned int vtype;
	unsigned int h;
	unsigned int w;
	unsigned int canvas0Addr;
	unsigned int canvas1Addr;
	unsigned int bitdepth; /*copy*/
	unsigned int bit_mode;	/*count*/
	unsigned int omx_index;
	u64 ready_jiffies64; /*copy from vframe u64 ready_jiffies64;*/
};

struct di_in_inf_s {
	/*use this for judge type change or not */
	unsigned int ch;
	unsigned int vtype_ori;	/*only debug*/
	unsigned int src_type;
	unsigned int trans_fmt;
	unsigned int sig_fmt;/*use for mtn*/
	unsigned int h;
	unsigned int w;
};

enum EDIM_DISP_TYPE {
	EDIM_DISP_T_NONE,
	EDIM_DISP_T_IN,		/*need bypass*/
	EDIM_DISP_T_PRE,
	EDIM_DISP_T_NR,
	EDIM_DISP_T_PST,
};

struct dim_dvfm_s {
//	unsigned int		code_name;/*0x12345678*/
	unsigned int		index;

	enum EDIM_DISP_TYPE	etype;
	struct di_in_inf_s	in_inf;
	struct dim_wmode_s	wmode;
	struct dim_vmode_s	vmode;
	//struct vframe_s		vframe;/*input cp*/
	struct dim_itf_u		vframe;
	void	*vfm_in;
	void	*vfm_out;	/*only for ins*/
	void	*di_buf;/*vfm out is in di_buf*/
};

struct di_buf_c_s {
	struct dim_wmode_s	wmode;
	struct dim_vmode_s	vmode;
	struct dim_dvfm_s	*pdvfm;
	int			post_proc_flag;
	/* post_proc_flag:
	 * 0: no post di;
	 * 1: normal post di;
	 * 2, edge only; 3, dummy
	 ***********************/
	int			new_format_flag;
	int			throw_flag;
	int			invert_top_bot_flag;
	int			seq;
	int			pre_ref_count;
	/* pre_ref_count:none zero, is used by mem_mif,
	 * chan2_mif, or wr_buf
	 */
	int			post_ref_count;
	/* post_ref_count:	none zero, is used by post process */
	struct mcinfo_pre_s	curr_field_mcinfo;
	/*curr_field_mcinfo for type of VFRAME_TYPE_LOCAL */
	struct pulldown_detected_s	pd_config;
	/* pd_config:	blend window */
	unsigned int		privated; /*?*/
	/* privated:tff bff check result bit[1:0]*/
	unsigned int		canvas_config_flag;
	/**canvas_config_flag************
	 * 0,configed; 1,config type 1 (prog);
	 * 2, config type 2 (interlace)
	 ********************************/
	enum process_fun_index_e process_fun_index;
	int			early_process_fun_index;
	int			left_right;
	/* left_right:1,left eye; 0,right eye in field alternative*/
	struct di_buf_s		*di_buf[2];
	/* di_buf:**********************
	 * di_buf[0]:pre_ready
	 * for type of VFRAME_TYPE_POST?
	 *******************************/
	struct di_buf_s		*di_buf_dup_p[5];
	/* di_buf_dup_p *****************
	 *0~4: n-2, n-1, n, n+1, n+2;	n is the field to display
	 *0: n-2
	 *1: n-1
	 *2: n
	 *3: n+1
	 *4: n+2*************************/
	struct di_buf_s		*di_wr_linked_buf;
	atomic_t		di_cnt;
	/* di_cnt************************
	 * debug for di-vf-get/put
	 * 1: after get
	 * 0: after put
	 ********************************/
	unsigned int		width_bk;
	unsigned int		blend_mode; /*ary add for debug*/

	unsigned int sts;

	struct di_buf_s *in_buf; /*keep dec vf: link in buf*/
	unsigned int dec_vf_state;	/*keep dec vf:*/
};

struct di_buf_s {
	/********************************/
	unsigned int	code_name;/*0x87654321*/
	unsigned int	channel;
	int		index;

	int		type;

	/*******
	 * index in vframe_in_dup[] or vframe_in[],
	 * only for type of VFRAME_TYPE_IN
	 */
	int		queue_index;
	struct vframe_s	*vframe;

	struct page	*pages;
	unsigned long	nr_adr;
	int		nr_canvas_idx;
	unsigned long	mtn_adr;
	int		mtn_canvas_idx;
	unsigned long	cnt_adr;
	int		cnt_canvas_idx;
	unsigned long	mcinfo_adr;
	unsigned short	*mcinfo_adr_v;/**/
	bool		mcinfo_alloc_flg; /**/
	int		mcinfo_canvas_idx;
	unsigned long	mcvec_adr;
	int		mcvec_canvas_idx;
	unsigned int	canvas_height;
	unsigned int	canvas_height_mc;	/*ary add for mc h is diff*/
	unsigned int	canvas_width[3];/* nr/mtn/mv */
	struct di_buffer	*ins; /*for EDIM_TMODE_3_PW_LOCAL*/
	/********************************/
	struct di_buf_c_s	c;
};

#define RDMA_DET3D_IRQ			0x20
/* vdin0 rdma irq */
#define RDMA_DEINT_IRQ			0x2
#define RDMA_TABLE_SIZE			((PAGE_SIZE) << 1)

#define MAX_CANVAS_WIDTH		1920
#define MAX_CANVAS_HEIGHT		1088

/* #define DI_BUFFER_DEBUG */

#define DI_LOG_MTNINFO			0x02
#define DI_LOG_PULLDOWN			0x10
#define DI_LOG_BUFFER_STATE		0x20
#define DI_LOG_TIMESTAMP		0x100
#define DI_LOG_PRECISE_TIMESTAMP	0x200
#define DI_LOG_QUEUE			0x40
#define DI_LOG_VFRAME			0x80

#if 0
#define QUEUE_LOCAL_FREE		0
#define QUEUE_IN_FREE			1
#define QUEUE_PRE_READY			2
#define QUEUE_POST_FREE			3
#define QUEUE_POST_READY		4
#define QUEUE_RECYCLE			5
#define QUEUE_DISPLAY			6
#define QUEUE_TMP			7
#define QUEUE_POST_DOING		8
#define QUEUE_NUM			9
#else
#define QUEUE_LOCAL_FREE		0
#define QUEUE_RECYCLE			1	/* 5 */
#define QUEUE_DISPLAY			2	/* 6 */
#define QUEUE_TMP			3	/* 7 */
#define QUEUE_POST_DOING		4	/* 8 */

#define QUEUE_IN_FREE			5	/* 1 */
#define QUEUE_PRE_READY			6	/* 2 */
#define QUEUE_POST_FREE			7	/* 3 */
#define QUEUE_POST_READY		8	/* 4	QUE_POST_READY */

/*new use this for put back control*/
#define QUEUE_POST_PUT_BACK		(9)
#define QUEUE_POST_DOING2		(10)
#define QUEUE_POST_NOBUF		(11)
#define QUEUE_POST_KEEP			(12)/*below use pw_queue_in*/
#define QUEUE_POST_KEEP_BACK		(13)

#define QUEUE_NUM			5	/* 9 */
#define QUEUE_NEW_THD_MIN		(QUEUE_IN_FREE - 1)
#define QUEUE_NEW_THD_MAX		(QUEUE_POST_KEEP_BACK + 1)

#endif

#define queue_t struct queue_s

#define VFM_NAME		"deinterlace"

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
void enable_rdma(int enable_flag);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
bool is_vsync_rdma_enable(void);
#else
#ifndef VSYNC_WR_MPEG_REG
#define VSYNC_WR_MPEG_REG(adr, val) aml_write_vcbus(adr, val)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len)		\
			       aml_vcbus_update_bits((adr),	\
			       ((1 << (len)) - 1) << (start), (val) << (start))

#define VSYNC_RD_MPEG_REG(adr) aml_read_vcbus(adr)
#endif
#endif

#define DI_COUNT   1
#define DI_MAP_FLAG	0x1
#define DI_SUSPEND_FLAG 0x2
#define DI_LOAD_REG_FLAG 0x4
#define DI_VPU_CLKB_SET 0x8

struct dim_dvfm_s;

struct di_dev_s {
	dev_t			   devt;
	struct cdev		   cdev; /* The cdev structure */
	struct device	   *dev;
	struct platform_device	*pdev;
	dev_t	devno;
	struct class	*pclss;

	bool sema_flg;	/*di_sema_init_flag*/

	struct task_struct *task;
	struct clk	*vpu_clkb;
	unsigned long clkb_max_rate;
	unsigned long clkb_min_rate;
	struct list_head   pq_table_list;
	atomic_t	       pq_flag;
	unsigned char	   di_event;
	unsigned int	   pre_irq;
	unsigned int	   post_irq;
	unsigned int	   flags;
	unsigned long	   jiffy;

	bool	mem_flg;	/*ary add for make sure mem is ok*/
	unsigned int	   buf_num_avail;
	int		rdma_handle;
	/* is support nr10bit */
	unsigned int	   nr10bit_support;
	/* is DI support post wr to mem for OMX */
	unsigned int       post_wr_support;
	unsigned int nrds_enable;
	unsigned int pps_enable;
	unsigned int h_sc_down_en;/*sm1, tm2 ...*/
	/*struct	mutex      cma_mutex;*/

	struct dentry *dbg_root;	/*dbg_fs*/
	/***************************/
	/*struct di_data_l_s	data_l;*/
	void *data_l;

};
#if 1
struct di_pre_stru_s {
/* pre input */
	struct DI_MIF_s	di_inp_mif;
	struct DI_MIF_s	di_mem_mif;
	struct DI_MIF_s	di_chan2_mif;
	struct di_buf_s *di_inp_buf;
	struct di_buf_s *di_post_inp_buf;
	struct di_buf_s *di_inp_buf_next;
	/* p_asi_next: ary:add for p */
	struct di_buf_s *p_asi_next;
	struct di_buf_s *di_mem_buf_dup_p;
	struct di_buf_s *di_chan2_buf_dup_p;
/* pre output */
	struct DI_SIM_MIF_s	di_nrwr_mif;
	struct DI_SIM_MIF_s	di_mtnwr_mif;
	struct di_buf_s *di_wr_buf;	/**/
	struct di_buf_s *di_post_wr_buf;
	struct DI_SIM_MIF_s	di_contp2rd_mif;
	struct DI_SIM_MIF_s	di_contprd_mif;
	struct DI_SIM_MIF_s	di_contwr_mif;
	unsigned int	field_count;
	/* field_count
	 * old name: field_count_for_cont
	 *ary: one type play cnt
	 ******************************/
/*
 * 0 (f0,null,f0)->nr0,
 * 1 (f1,nr0,f1)->nr1_cnt,
 * 2 (f2,nr1_cnt,nr0)->nr2_cnt
 * 3 (f3,nr2_cnt,nr1_cnt)->nr3_cnt
 */
	struct DI_MC_MIF_s		di_mcinford_mif;
	struct DI_MC_MIF_s		di_mcvecwr_mif;
	struct DI_MC_MIF_s		di_mcinfowr_mif;
/* pre state */
	int	in_seq;
	int	recycle_seq;
	int	pre_ready_seq;

	int	pre_de_busy;            /* 1 if pre_de is not done */
	int	pre_de_process_flag;    /* flag when dim_pre_de_process done */
	int	pre_de_clear_flag;
	/* flag is set when VFRAME_EVENT_PROVIDER_UNREG*/
	int	unreg_req_flag_cnt;

	int	reg_req_flag_cnt;
	int	force_unreg_req_flag;
	int	disable_req_flag;
	/* current source info */
	int	cur_width;
	int	cur_height;
	int	cur_inp_type;
	int	cur_source_type;
	int	cur_sig_fmt;
	unsigned int orientation;
	int	cur_prog_flag;
	/* 1 for progressive source */
	/* valid only when prog_proc_type is 0, for
	 * progressive source: top field 1, bot field 0
	 */
	int	source_change_flag;

	bool input_size_change_flag;
	/* input_size_change_flag,
	 * 1: need reconfig pre/nr/dnr size
	 * 0: not need config pre/nr/dnr size
	 ******************************************/

/* true: bypass di all logic, false: not bypass */
	bool bypass_flag;
	unsigned char prog_proc_type;
/* set by prog_proc_config when source is vdin,
 *	0: i mode or p as i mode
 *	1: use 1 p buffer;
 *	2: p use 2 i buffer;
 *	3:use 2 i paralleling buffer ary:x
 */

	unsigned char madi_enable;
	unsigned char mcdi_enable;
	unsigned int  pps_dstw;	/*no use ?*/
	unsigned int  pps_dsth;	/*no use ?*/
	int	left_right;/*1,left eye; 0,right eye in field alternative*/
/*input2pre*/
	int	bypass_start_count;
/* need discard some vframe when input2pre => bypass */
	unsigned char vdin2nr;
	enum tvin_trans_fmt	source_trans_fmt;
	enum tvin_trans_fmt	det3d_trans_fmt;

	unsigned int width_bk;
#ifdef DET3D
	unsigned int det_lr;	/*ary DET3D*/
	unsigned int det_tp;	/*ary DET3D*/
	unsigned int det_la;	/*ary DET3D*/
	unsigned int det_null;	/*ary DET3D*/
	int	vframe_interleave_flag;
#endif
/**/
	int	pre_de_irq_timeout_count;
	int	pre_throw_flag; /*ary: for what?*/
	int	bad_frame_throw_count;
/*for static pic*/
	int	static_frame_count;/*ary: no use*/
	bool force_interlace;
	bool bypass_pre;
	bool invert_flag;
	//bool vdin_source; /*ary no use*/

	int cma_release_req;
	/* for performance debug */
	unsigned long irq_time[2];
	/* combing adaptive */
	struct combing_status_s *mtn_status;
	bool combing_fix_en;
	unsigned int comb_mode;
	//struct dim_dvfm_s lst_dvfm;
	struct dim_dvfm_s *dvfm;
	unsigned int cnt_put;
	unsigned int cnt_get;

};
#endif
struct di_post_stru_s {
	struct DI_MIF_s	di_buf0_mif;
	struct DI_MIF_s	di_buf1_mif;
	struct DI_MIF_s	di_buf2_mif;
	struct DI_SIM_MIF_s di_diwr_mif;
	struct DI_SIM_MIF_s	di_mtnprd_mif;
	struct DI_MC_MIF_s	di_mcvecrd_mif;
	/*post doing buf and write buf to post ready*/
	struct di_buf_s *cur_post_buf;
	struct di_buf_s *keep_buf;
	struct di_buf_s *keep_buf_post;	/*ary add for keep post buf*/
	int		update_post_reg_flag;
	int		run_early_proc_fun_flag; /*ary ?*/
	int		cur_disp_index;
	int		canvas_id;
	int		next_canvas_id;
	bool		toggle_flag;
	bool		vscale_skip_flag;
//	uint		start_pts;
	int		buf_type;
	int de_post_process_done;
	int post_de_busy;
	int di_post_num;
	unsigned int post_peek_underflow;
	unsigned int di_post_process_cnt;
	unsigned int check_recycle_buf_cnt;/*cp to di_hpre_s*/
	/* performance debug */
	unsigned int  post_wr_cnt;
	unsigned long irq_time;

	/*frame cnt*/
	unsigned int frame_cnt;	/*cnt for post process*/
};

#define MAX_QUEUE_POOL_SIZE   256
struct queue_s {
	unsigned int num;
	unsigned int in_idx;
	unsigned int out_idx;
	unsigned int type;
/* 0, first in first out;
 * 1, general;2, fix position for di buf
 */
	unsigned int pool[MAX_QUEUE_POOL_SIZE];
};

struct di_buf_pool_s {
	struct di_buf_s *di_buf_ptr;
	unsigned int size;
};

//unsigned char dim_is_bypass(vframe_t *vf_in, unsigned int channel);
bool dimv3_bypass_first_frame(unsigned int ch);

int div3_cnt_buf(int width, int height, int prog_flag, int mc_mm,
	       int bit10_support, int pack422);
int dim_ins_cnt_post_cvs_size(struct di_buf_s	*di_buf,
			      struct di_buffer	*ins_buf);
int dim_ins_cnt_post_cvs_size2(struct di_buf_s	*di_buf,
			      struct di_buffer	*ins_buf,
			      unsigned int ch);

unsigned int dim_ins_cnt_post_size(unsigned int w, unsigned int h);

/*---get di state parameter---*/
struct di_dev_s *getv3_dim_de_devp(void);

const char *dimv3_get_version_s(void);
int dimv3_get_dump_state_flag(void);

int dimv3_get_blocking(void);

struct di_buf_s *dimv3_get_recovery_log_di_buf(void);

unsigned long dimv3_get_reg_unreg_timeout_cnt(void);
//struct vframe_s **dim_get_vframe_in(unsigned int ch);
int dimv3_check_recycle_buf(unsigned int ch);

int dimv3_seq_file_module_para_di(struct seq_file *seq);
int dimv3_seq_file_module_para_hw(struct seq_file *seq);

int dim_seq_file_module_para_film_fw1(struct seq_file *seq);
int dim_seq_file_module_para_mtn(struct seq_file *seq);

int dimv3_seq_file_module_para_pps(struct seq_file *seq);

int dim_seq_file_module_para_(struct seq_file *seq);

/***********************/
#ifdef HIS_V3
void dim_lock_irqfiq_save(ulong flg);
void dim_unlock_irqfiq_restore(ulong flg);
#endif

unsigned int div3_get_dts_nrds_en(void);
int div3_get_disp_cnt(void);
void dbg_vfmv3(struct vframe_s *vf, unsigned int cnt);

/*---------------------*/
long dimv3_pq_load_io(unsigned long arg);
int dimv3_get_canvas(void);
/*unsigned int dim_cma_alloc_total(struct di_dev_s *de_devp);*/
irqreturn_t dimv3_irq(int irq, void *dev_instance);
irqreturn_t dimv3_post_irq(int irq, void *dev_instance);

void dimv3_rdma_init(void);
void dimv3_rdma_exit(void);

void dimv3_set_di_flag(void);
void dimv3_get_vpu_clkb(struct device *dev, struct di_dev_s *pdev);

void dimv3_log_buffer_state(unsigned char *tag, unsigned int channel);

unsigned char dimv3_pre_de_buf_config(unsigned int ch);
unsigned char dim_pre_de_buf_config_bypass(unsigned int ch);
unsigned char dim_pre_de_buf_config_p_asi_t(unsigned int ch);
unsigned char dim_pre_de_buf_config_p_asi_b(unsigned int ch);
unsigned char dim_pre_de_buf_config_top(unsigned int ch);

void dimv3_pre_de_process(unsigned int channel);
//void dim_pre_de_done_buf_config(struct di_ch_s *pch, bool flg_timeout);
void dimv3_pre_de_done_buf_clear(unsigned int channel);

void div3_reg_setting(unsigned int channel, struct vframe_s *vframe,
		    struct dim_wmode_s	*pwmode);
//void di_reg_variable(unsigned int channel, struct dim_dvfm_s *pdvfm);
void di_reg_variable_needbypass(unsigned int channel,
				struct dim_dvfm_s *pdvfm);
void di_reg_variable_normal(unsigned int channel,
			    struct dim_dvfm_s *pdvfm);


/*void dim_unreg_process_irq(unsigned int channel);*/
void div3_unreg_variable(unsigned int channel);
void div3_unreg_setting(void);

void dimv3_uninit_buf(unsigned int disable_mirror, unsigned int channel);
//void dim_unreg_process(unsigned int channel);

int dimv3_process_post_vframe(unsigned int channel);
int dimv3_pst_vframe_top(unsigned int ch);

unsigned char dimv3_check_di_buf(struct di_buf_s *di_buf, int reason,
			       unsigned int channel);
int dimv3_do_post_wr_fun(void *arg, vframe_t *disp_vf);
int dimv3_post_process(void *arg, unsigned int zoom_start_x_lines,
		     unsigned int zoom_end_x_lines,
		     unsigned int zoom_start_y_lines,
		     unsigned int zoom_end_y_lines, vframe_t *disp_vf);
void dimv3_post_de_done_buf_config(unsigned int channel);
void dimv3_recycle_post_back(unsigned int channel);
void recyclev3_post_ready_local(struct di_buf_s *di_buf,
			      unsigned int channel);

/*--------------------------*/
unsigned char dimv3_vcry_get_flg(void);
void dimv3_vcry_flg_inc(void);
void dimv3_vcry_set_flg(unsigned char val);
/*--------------------------*/
unsigned int dimv3_vcry_get_log_reason(void);
void dimv3_vcry_set_log_reason(unsigned int val);
/*--------------------------*/
unsigned char dimv3_vcry_get_log_q_idx(void);
void dimv3_vcry_set_log_q_idx(unsigned int val);
/*--------------------------*/
struct di_buf_s **dimv3_vcry_get_log_di_buf(void);
void dimv3_vcry_set_log_di_buf(struct di_buf_s *di_bufp);
void dimv3_vcry_set(unsigned int reason, unsigned int idx,
		  struct di_buf_s *di_bufp);

const char *dimv3_get_vfm_type_name(unsigned int nub);

bool dimv3_get_mcmem_alloc(void);
bool dimv3_get_overturn(void);

int dimv3_get_reg_unreg_cnt(void);
void dimv3_reg_timeout_inc(void);

//no use void dim_reg_process(unsigned int channel);
//bool is_bypass2(struct vframe_s *vf_in, unsigned int ch);
void dimv3_post_keep_cmd_proc(unsigned int ch, unsigned int index);
void dimv3_dbg_release_keep_all(unsigned int ch);
/*--------------------------*/
//int di_ori_event_unreg(unsigned int channel);
//int di_ori_event_reg(void *data, unsigned int channel);
int div3_ori_event_qurey_vdin2nr(unsigned int channel);
int div3_ori_event_reset(unsigned int channel);
int div3_ori_event_light_unreg(unsigned int channel);
int div3_ori_event_light_unreg_revframe(unsigned int channel);
//int di_ori_event_ready(unsigned int channel);
//int div3_ori_event_qurey_state(unsigned int channel);
void  div3_ori_event_set_3D(int type, void *data, unsigned int channel);

/*--------------------------*/
extern int prev3_run_flag;
extern unsigned int dbgv3_first_cnt_pre;
extern spinlock_t plistv3_lock;

void dimv3_dbg_pre_cnt(unsigned int channel, char *item);

void diextv3_clk_b_sw(bool on);

//int di_vf_l_states(struct vframe_states *states, unsigned int channel);


unsigned char dimv3_pre_de_buf_config_p_asi(unsigned int ch);
void dimv3_post_keep_back_recycle(unsigned int ch);
void dbgv3_mode(unsigned int position);
bool dimv3_post_keep_release_all_2free(unsigned int ch);


/*---------------------*/

ssize_t
storev3_config(struct device *dev,
	     struct device_attribute *attr,
	     const char *buf, size_t count);
ssize_t
storev3_dbg(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count);
ssize_t
storev3_dump_mem(struct device *dev, struct device_attribute *attr,
	       const char *buf, size_t len);
ssize_t
storev3_log(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count);
ssize_t
showv3_vframe_status(struct device *dev,
		   struct device_attribute *attr,
		   char *buf);

ssize_t dimv3_read_log(char *buf);
int dimv3_get_invert_tb(void);
enum EDPST_MODE;
struct di_win_s;

void div3_cnt_cvs(enum EDPST_MODE mode,
		struct di_win_s *in,
		struct di_win_s *out);

/*---------------------*/

struct di_buf_s *dimv3_get_buf(unsigned int channel,
			     int queue_idx, int *start_pos);

#define queue_for_each_entry(di_buf, channel, queue_idx, list)	\
	for (itmp = 0;						\
	    ((di_buf = dimv3_get_buf(channel, queue_idx, &itmp)) != NULL);)

#define di_dev_t struct di_dev_s

#define di_pr_info(fmt, args ...)   pr_info("DIV3: " fmt, ## args)

#define pr_dbg(fmt, args ...)       pr_debug("DIV3: " fmt, ## args)

#define pr_error(fmt, args ...)     pr_err("DIV3: " fmt, ## args)

/*this is debug for buf*/
/*#define DI_DEBUG_POST_BUF_FLOW	(1)*/
#define DIM_DEBUG_QUE_ERR		(1)
//#define TST_NEW_INS_INTERFACE		(1)
//#define TST_NEW_INS_RUN_Q		(1)
#define TST_OVFM_BY_DI			(1)
/* DI_MOVE_POST	2019-09-23 move post function*/
/********************************************************************
 *history:
 * 2019-09-20: test input 4k mode
 * 2019-09-24:
 *	add dpst_cma_alloc|dpst_cma_release
 *	add bset | bclr | bget
 * 2019-10-11: begin to split hw layer
 *	add di_pre_hw.c|.h
 *	add dim_mcinfo_v_alloc / release for mcinfo buf phy to v
 * 2019-11-14: add dim_hpre_ops (function is null now)
 *	move pre_vinfo_set from di_pre.c to di_pre_hw.c
 * 2019-11-15: add qued
 * 2019-12-10: v need %4 issue
 *	add V_DIV4
 *******************************************************************/
#endif
