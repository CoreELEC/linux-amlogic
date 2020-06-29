/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/sc2/di_hw_v3.h
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

#ifndef __DI_HW_V3_H__
#define __DI_HW_V3_H__

#include "deinterlace_hw.h"

enum EAFBCE_INDEX_V3 {
	AFBCEX_ENABLE,
	AFBCEX_MODE,
	AFBCEX_SIZE_IN,
	AFBCEX_BLK_SIZE_IN,
	AFBCEX_HEAD_BADDR,
	AFBCEX_MIF_SIZE,
	AFBCEX_PIXEL_IN_HOR_SCOPE,
	AFBCEX_PIXEL_IN_VER_SCOPE,
	AFBCEX_CONV_CTRL,
	AFBCEX_MIF_HOR_SCOPE,
	AFBCEX_MIF_VER_SCOPE,
	AFBCEX_STAT1,
	AFBCEX_STAT2,
	AFBCEX_FORMAT,
	AFBCEX_MODE_EN,
	AFBCEX_DWSCALAR,
	AFBCEX_DEFCOLOR_1,
	AFBCEX_DEFCOLOR_2,
	AFBCEX_QUANT_ENABLE,
	AFBCEX_IQUANT_LUT_1,
	AFBCEX_IQUANT_LUT_2,
	AFBCEX_IQUANT_LUT_3,
	AFBCEX_IQUANT_LUT_4,
	AFBCEX_RQUANT_LUT_1,
	AFBCEX_RQUANT_LUT_2,
	AFBCEX_RQUANT_LUT_3,
	AFBCEX_RQUANT_LUT_4,
	AFBCEX_YUV_FORMAT_CONV_MODE,
	AFBCEX_DUMMY_DATA,
	AFBCEX_CLR_FLAG,
	AFBCEX_STA_FLAGT,
	AFBCEX_MMU_NUM,
	AFBCEX_MMU_RMIF_CTRL1,
	AFBCEX_MMU_RMIF_CTRL2,
	AFBCEX_MMU_RMIF_CTRL3,
	AFBCEX_MMU_RMIF_CTRL4,
	AFBCEX_MMU_RMIF_SCOPE_X,
	AFBCEX_MMU_RMIF_SCOPE_Y,
	AFBCEX_MMU_RMIF_RO_STAT,
	AFBCEX_PIP_CTRL,
	AFBCEX_ROT_CTRL,

};

#define AFBC_ENC_V3_NUB		(3)
#define DIM_AFBCE_V3_NUB	(41)

#define DIM_ERR		(0xffffffff)

struct AFBCD_S {
	u32  index      ;//3bit: 0-5 for di_m0/m5, 6:vd1 7:vd2
	u32  hsize      ;//input size
	u32  vsize;
	u32  head_baddr;
	u32  body_baddr;
	u32  compbits   ;//2 bits   0-8bits 1-9bits 2-10bits
	u32  fmt_mode   ;//2 bits   default = 2, 0:yuv444 1:yuv422 2:yuv420
	u32  ddr_sz_mode;//1 bits   1:mmu mode
	u32  fmt444_comb;//1 bits
	u32  dos_uncomp ;//1 bits   0:afbce   1:dos
	u32  rot_en;
	u32  rot_hbgn;
	u32  rot_vbgn;
	u32  h_skip_en;
	u32  v_skip_en;

	u32  rev_mode;
	u32  lossy_en;
	u32  def_color_y;
	u32  def_color_u;
	u32  def_color_v;
	u32  win_bgn_h;
	u32  win_end_h;
	u32  win_bgn_v;
	u32  win_end_v;
	u32  rot_vshrk;
	u32  rot_hshrk;
	u32  rot_drop_mode;
	u32  rot_ofmt_mode;
	u32  rot_ocompbit;
	u32  pip_src_mode;
	//ary add:
	u32 hold_line_num;		/* def 2*/
	u32 blk_mem_mode;		/* def 0*/
	unsigned int out_horz_bgn;	/* def 0*/
	unsigned int out_vert_bgn;	/* def 0*/
	unsigned int hz_ini_phase;	/* def 0*/
	unsigned int vt_ini_phase;	/* def 0*/
	unsigned int hz_rpt_fst0_en;	/* def 0*/
	unsigned int vt_rpt_fst0_en;	/* def 0*/
	//unsigned int rev_mode;		/* def 0*/
	unsigned int def_color;		/* def no use */
	unsigned int reg_lossy_en;	/* def 0*/
	//unsigned int pip_src_mode;	/* def 0*/
	//unsigned int rot_drop_mode;	/* def 0*/
	//unsigned int rot_vshrk;		/* def 0*/
	//unsigned int rot_hshrk;		/* def 0*/

};

struct AFBCE_S {
	u32 head_baddr     ;//head_addr of afbce
	u32 mmu_info_baddr ;//mmu_linear_addr
	u32 reg_init_ctrl  ;//pip init frame flag
	u32 reg_pip_mode   ;//pip open bit
	u32 reg_ram_comb   ;//ram split bit open in di mult write case case
	u32 reg_format_mode;//0:444 1:422 2:420
	u32 reg_compbits_y ;//bits num after compression
	u32 reg_compbits_c ;//bits num after compression
	u32 hsize_in       ;//input data hsize
	u32 vsize_in       ;//input data hsize
	u32 hsize_bgnd     ;//hsize of background
	u32 vsize_bgnd     ;//hsize of background
	u32 enc_win_bgn_h  ;//scope in background buffer
	u32 enc_win_end_h  ;//scope in background buffer
	u32 enc_win_bgn_v  ;//scope in background buffer
	u32 enc_win_end_v  ;//scope in background buffer
	u32 loosy_mode;
	//0:close 1:luma loosy 2:chrma loosy 3: luma & chrma loosy
	u32 rev_mode       ;//0:normal mode
	u32 def_color_0    ;//def_color
	u32 def_color_1    ;//def_color
	u32 def_color_2    ;//def_color
	u32 def_color_3    ;//def_color
	u32 force_444_comb ;//def_color
	u32 rot_en;
	u32 din_swt;
};

struct SHRK_S {
	u32 hsize_in;
	u32 vsize_in;
	u32 h_shrk_mode;
	u32 v_shrk_mode;
	u32 shrk_en;
	u32 frm_rst;

};

#ifdef MARK_SC2	//ary use DI_MIF_S
struct DI_MIF0_S {
	u16  luma_x_start0;
	u16  luma_x_end0;
	u16  luma_y_start0;
	u16  luma_y_end0;
	u16  chroma_x_start0;
	u16  chroma_x_end0;
	u16  chroma_y_start0;
	u16  chroma_y_end0;
	u16  set_separate_en : 2;
	// 00 : one canvas 01 : 3 canvas(old 4:2:0).10: 2 canvas. (NV21).
	uint16_t  src_field_mode  : 1;   // 1 frame . 0 field.
	uint16_t  video_mode      : 2;   //00: 4:2:0; 01: 4:2:2; 10: 4:4:4
	uint16_t  output_field_num: 1;   // 0 top field  1 bottom field.
	uint16_t  bits_mode       : 2;
	// 0:8 bits  1:10 bits 422(old mode,12bit)
	// 2: 10bit 444  3:10bit 422(full pack) or 444

	uint16_t  burst_size_y    : 2;
	uint16_t  burst_size_cb   : 2;
	uint16_t  burst_size_cr   : 2;
	uint16_t  canvas0_addr0 : 8;
	uint16_t  canvas0_addr1 : 8;
	uint16_t  canvas0_addr2 : 8;
	uint16_t  rev_x : 1;
	uint16_t  rev_y : 1;
	//ary add----
	unsigned int urgent;
	unsigned int hold_line;
};
#endif

struct DI_MIF1_S { //dbg only
	   u16  start_x;
	   u16  end_x;
	   u16  start_y;
	   u16  end_y;
	   u16  canvas_num;
	   u16  rev_x;
	   u16  rev_y;
#ifdef MARK_SC2
	// bit_mode 0:8 bits 1:10 bits 422(old mode,12bit)
	// 2:10bit 444 3:10bit 422(full pack) or 444
	unsigned int    bit_mode	   :4;
	//set_separate_en00 : one canvas01 : 3
	//canvas(old 4:2:0).10: 2 canvas. (NV21).
	unsigned int    set_separate_en :4; /*ary add below is only for wr buf*/
	// video_mode :00: 4:2:0;01: 4:2:2;10: 4:4:4
	//2020-06-02 from 1bit to 2bit
	unsigned int    video_mode	   :4;
	unsigned int    ddr_en	   :1;
	unsigned int    urgent	   :1;
	unsigned int    l_endian	   :1;
	unsigned int    cbcr_swap	   :1;

	unsigned int    en		   :1; /* add for sc2*/
	unsigned int    src_i	   :1; /* ary add for sc2 */
	unsigned int    reserved	   : 15;
#endif
	enum DI_MIFS_ID mif_index; /* */
};

struct DI_PRE_S {
	struct DI_MIF_S *inp_mif;
	struct DI_MIF_S *mem_mif;
	struct DI_MIF_S *chan2_mif;
	struct DI_MIF_S *nrwr_mif;
	struct DI_MIF1_S *mtnwr_mif;
	struct DI_MIF1_S *mcvecwr_mif;
	struct DI_MIF1_S *mcinfrd_mif;
	struct DI_MIF1_S *mcinfwr_mif;
	struct DI_MIF1_S *contp1_mif;
	struct DI_MIF1_S *contp2_mif;
	struct DI_MIF1_S *contwr_mif;
	struct AFBCD_S *inp_afbc;
	struct AFBCD_S *mem_afbc;
	struct AFBCD_S *chan2_afbc;
	struct AFBCE_S *nrwr_afbc;

	int afbc_en;//
	int nr_en;
	int mcdi_en;
	int mtn_en;
	int dnr_en;
	int cue_en;
	int cont_ini_en;
	int mcinfo_rd_en;
	int pd32_check_en;
	int pd22_check_en;
	int hist_check_en;
	int pre_field_num;
	int pre_viu_link;// pre link to VPP
	int hold_line;
};

struct DI_PST_S {
	struct DI_MIF_S *buf0_mif;
	struct DI_MIF_S *buf1_mif;
	struct DI_MIF_S *buf2_mif;
	struct DI_MIF_S *wr_mif;
	struct DI_MIF1_S *mtn_mif;
	struct DI_MIF1_S *mcvec_mif;
	struct AFBCD_S *buf0_afbc;
	struct AFBCD_S *buf1_afbc;
	struct AFBCD_S *buf2_afbc;
	struct AFBCE_S *wr_afbc;

	int afbc_en;
	int post_en;
	int blend_en;
	int ei_en;
	int mux_en;
	int mc_en;
	int ddr_en;
	int vpp_en;
	int post_field_num;
	int hold_line;

};

struct DI_MULTI_WR_S {
	u32 pre_path_sel;
	u32 post_path_sel;

	struct DI_MIF_S *pre_mif;
	struct AFBCE_S *pre_afbce;
	struct DI_MIF_S *post_mif;
	struct AFBCE_S *post_afbce;

};

struct DI_PREPST_AFBC_S {
	struct AFBCD_S     *inp_afbc	   ;	 // PRE  : current input date
	struct AFBCD_S     *mem_afbc	    ;	  // PRE  : pre 2 data
	struct AFBCD_S     *chan2_afbc    ;	  // PRE  : pre 1 data
	struct AFBCE_S     *nrwr_afbc     ;	  // PRE  : nr write
	struct DI_MIF1_S   *mtnwr_mif     ;	  // PRE  : motion write
	struct DI_MIF1_S   *contp2rd_mif  ;	  // PRE  : 1bit motion read p2
	struct DI_MIF1_S   *contprd_mif   ;	  // PRE  : 1bit motion read p1
	struct DI_MIF1_S   *contwr_mif    ;	  // PRE  : 1bit motion write
	struct DI_MIF1_S   *mcinford_mif  ;	  // PRE  : mcdi lmv info read
	struct DI_MIF1_S   *mcinfowr_mif  ;	  // PRE  : mcdi lmv info write
	struct DI_MIF1_S   *mcvecwr_mif   ; // PRE  : mcdi motion vector write
	struct AFBCD_S     *if1_afbc	    ;	  // POST : pre field data
	struct AFBCE_S     *diwr_afbc     ;	  // POST : post write
	struct DI_MIF1_S   *mcvecrd_mif   ;	  // POST : mc vector read
	struct DI_MIF1_S   *mtnrd_mif     ;	  // POST : motion read
	int	link_vpp;
	int	post_wr_en;
	int	cont_ini_en;
	int	mcinfo_rd_en;
	int	pre_field_num;     //
	int	hold_line;
};

struct DI_PREPST_S {
	struct DI_MIF_S   *inp_mif	   ;	 // PRE  : current input date
	struct DI_MIF_S   *mem_mif       ;     // PRE  : pre 2 data
	struct DI_MIF_S   *chan2_mif     ;     // PRE  : pre 1 data
	struct DI_MIF_S   *nrwr_mif      ;     // PRE  : nr write
	struct DI_MIF1_S   *mtnwr_mif     ;     // PRE  : motion write
	struct DI_MIF1_S   *contp2rd_mif  ;     // PRE  : 1bit motion read p2
	struct DI_MIF1_S   *contprd_mif   ;     // PRE  : 1bit motion read p1
	struct DI_MIF1_S   *contwr_mif    ;     // PRE  : 1bit motion write
	struct DI_MIF1_S   *mcinford_mif  ;     // PRE  : mcdi lmv info read
	struct DI_MIF1_S   *mcinfowr_mif  ;     // PRE  : mcdi lmv info write
	struct DI_MIF1_S   *mcvecwr_mif   ; // PRE  : mcdi motion vector write
	struct DI_MIF_S   *if1_mif       ;     // POST : pre field data
	struct DI_MIF_S   *diwr_mif      ;     // POST : post write
	struct DI_MIF1_S   *mcvecrd_mif   ;     // POST : mc vector read
	struct DI_MIF1_S   *mtnrd_mif     ;     // POST : motion read
	int link_vpp;
	int post_wr_en;
	int cont_ini_en;
	int mcinfo_rd_en;
	int pre_field_num;     //
	int hold_line;
};

struct di_hw_ops_info_s {
	char name[128];
	char update[80];
	unsigned int version_main;
	unsigned int version_sub;
};

extern const struct dim_hw_opsv_s dim_ops_l1_v3;

struct hw_ops_s {
	struct di_hw_ops_info_s info;
	unsigned int (*afbcd_set)(int index,
				  struct AFBCD_S *inp_afbcd,
				  const struct reg_acc *op);
	unsigned int (*afbce_set)(int index,
				  //0:vdin_afbce 1:di_afbce0 2:di_afbce1
				  int enable,//open nbit of afbce
				  struct AFBCE_S  *afbce,
				  const struct reg_acc *op);
	void (*shrk_set)(struct SHRK_S *srkcfg,
			 const struct reg_acc *op);
	void (*wrmif_set)(int index, int enable,
			  struct DI_MIF_S *wr_mif, const struct reg_acc *op);
	void (*mult_wr)(struct DI_MULTI_WR_S *mwcfg, const struct reg_acc *op);
	void (*pre_set)(struct DI_PRE_S *pcfg, const struct reg_acc *op);
	void (*post_set)(struct DI_PST_S *ptcfg, const struct reg_acc *op);
	void (*prepost_link)(struct DI_PREPST_S *ppcfg,
			     const struct reg_acc *op);
	void (*prepost_link_afbc)(struct DI_PREPST_AFBC_S *pafcfg,
				  const struct reg_acc *op);
};

bool di_attach_ops_v3(const struct hw_ops_s **ops);
extern const struct reg_acc sc2reg;

enum SC2_REG_MSK {
	SC2_REG_MSK_GEN_PRE,
	SC2_REG_MSK_GEN_PST,
	SC2_REG_MSK_nr,
};

bool is_mask(unsigned int cmd);
void dim_sc2_contr_pre(struct hw_sc2_ctr_pre_s *cfg);
void dim_sc2_contr_pst(struct hw_sc2_ctr_pst_s *cfg);
void hpre_gl_read(void);

#endif /* __DI_HW_V3_H__ */
