/*
 * drivers/amlogic/media/deinterlace/di_afbc.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/of_fdt.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/of_device.h>

#include <linux/amlogic/media/vfm/vframe.h>

/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "deinterlace_dbg.h"
#include "deinterlace.h"

#include "register.h"
#include "nr_downscale.h"
#include "deinterlace_hw.h"
#include "register_nr4.h"
#include "di_afbc.h"

#include <linux/amlogic/media/vpu/vpu.h>
#if 1
#define DI_BIT0		0x00000001
#define DI_BIT1		0x00000002
#define DI_BIT2		0x00000004
#define DI_BIT3		0x00000008
#define DI_BIT4		0x00000010
#define DI_BIT5		0x00000020
#define DI_BIT6		0x00000040
#define DI_BIT7		0x00000080

static unsigned int reg_rd(unsigned int addr)
{
	return aml_read_vcbus(addr);
}

static unsigned int reg_rdb(unsigned int adr, unsigned int start,
			    unsigned int len)
{
	return Rd_reg_bits(adr, start, len);
}

static void reg_wr(unsigned int addr, unsigned int val)
{
	Wr(addr, val);
}

static unsigned int reg_wrb(unsigned int adr, unsigned int val,
			    unsigned int start, unsigned int len)
{
	Wr_reg_bits(adr, val, start, len);
	return 1;
}

const struct reg_acc di_normal_regset = {
	.wr = reg_wr,
	.rd = reg_rd,
	.bwr = reg_wrb,
	.brd = reg_rdb,
};

/*tmp*/
/*static struct afd_s di_afd;*/
/************************************
 * bit[0]: on/off
 * bit[1]: p -> need proce p use 2 i buf
 * bit[2]: mode
 ************************************/
static u32 afbc_cfg;

module_param_named(afbc_cfg, afbc_cfg, uint, 0664);

#ifdef DBG_AFBC
static u32 afbc_cfg_vf;
module_param_named(afbc_cfg_vf, afbc_cfg_vf, uint, 0664);
static u32 afbc_cfg_bt;
module_param_named(afbc_cfg_bt, afbc_cfg_bt, uint, 0664);
#endif
/************************************************
 * [0]: enable afbc
 * [1]: p use 2 ibuf mode
 * [2]: 2afbcd + 1afbce mode ([1] must 1)
 * [3]: 2afbcd + 1afbce test mode (mem use inp vf)
 * [4]: 4k test mode; i need bypass;
 * [5]: pre_link mode
 * [6]: stop when first frame display
 * [7]: change level always 3
 ***********************************************/

static bool is_cfg(enum EAFBCV1_CFG cfg_cmd)
{
	bool ret;

	ret = false;
	switch (cfg_cmd) {
	case EAFBCV1_CFG_EN:
		if (afbc_cfg & DI_BIT0)
			ret = true;
		break;
	case EAFBCV1_CFG_PMODE:
		if (afbc_cfg & DI_BIT1)
			ret = true;
		break;
	case EAFBCV1_CFG_EMODE:
		if ((afbc_cfg & DI_BIT2) &&
		    (afbc_cfg & DI_BIT1))
			ret = true;
		break;
	case EAFBCV1_CFG_ETEST:
		if (afbc_cfg & DI_BIT3)
			ret = true;
		break;
	case EAFBCV1_CFG_4K:
		if (afbc_cfg & DI_BIT4)
			ret = true;
		break;
	case EAFBCV1_CFG_PRE_LINK:
		if (afbc_cfg & DI_BIT5)
			ret = true;
		break;
	case EAFBCV1_CFG_PAUSE:
		if (afbc_cfg & DI_BIT6)
			ret = true;
		break;
	case EAFBCV1_CFG_LEVE3:
		if (afbc_cfg & DI_BIT7)
			ret = true;
		break;
	}
	return ret;
}

bool prelink_status;

bool dbg_di_prelink(void)
{
	if (is_cfg(EAFBCV1_CFG_PRE_LINK))
		return true;

	return false;
}
EXPORT_SYMBOL(dbg_di_prelink);

void dbg_di_prelink_reg_check(void)
{
	unsigned int val;

	if (!prelink_status && is_cfg(EAFBCV1_CFG_PRE_LINK)) {
		/* set on*/
		reg_wrb(DI_AFBCE_CTRL, 1, 3, 1);
		reg_wr(VD1_AFBCD0_MISC_CTRL, 0x100100);
		prelink_status = true;
	} else if (prelink_status && !is_cfg(EAFBCV1_CFG_PRE_LINK)) {
		/* set off */
		reg_wrb(DI_AFBCE_CTRL, 0, 3, 1);
		prelink_status = false;
	}

	if (!is_cfg(EAFBCV1_CFG_PRE_LINK))
		return;

	val = reg_rd(VD1_AFBCD0_MISC_CTRL);
	if (val != 0x100100) {
		di_pr_info("%s:change 0x%x\n", __func__, val);
		reg_wr(VD1_AFBCD0_MISC_CTRL, 0x100100);
	}
}

#ifdef HIS_CODE
static struct afbcdv1_ctr_s *div1_get_afd_ctr(void)
{
	return &di_afd.ctr;
}
#endif

static const unsigned int reg_AFBC[AFBC_DEC_NUB][AFBC_REG_INDEX_NUB] = {
	{
		AFBC_ENABLE,
		AFBC_MODE,
		AFBC_SIZE_IN,
		AFBC_DEC_DEF_COLOR,
		AFBC_CONV_CTRL,
		AFBC_LBUF_DEPTH,
		AFBC_HEAD_BADDR,
		AFBC_BODY_BADDR,
		AFBC_SIZE_OUT,
		AFBC_OUT_YSCOPE,
		AFBC_STAT,
		AFBC_VD_CFMT_CTRL,
		AFBC_VD_CFMT_W,
		AFBC_MIF_HOR_SCOPE,
		AFBC_MIF_VER_SCOPE,
		AFBC_PIXEL_HOR_SCOPE,
		AFBC_PIXEL_VER_SCOPE,
		AFBC_VD_CFMT_H,
	},
	{
		VD2_AFBC_ENABLE,
		VD2_AFBC_MODE,
		VD2_AFBC_SIZE_IN,
		VD2_AFBC_DEC_DEF_COLOR,
		VD2_AFBC_CONV_CTRL,
		VD2_AFBC_LBUF_DEPTH,
		VD2_AFBC_HEAD_BADDR,
		VD2_AFBC_BODY_BADDR,
		VD2_AFBC_OUT_XSCOPE,
		VD2_AFBC_OUT_YSCOPE,
		VD2_AFBC_STAT,
		VD2_AFBC_VD_CFMT_CTRL,
		VD2_AFBC_VD_CFMT_W,
		VD2_AFBC_MIF_HOR_SCOPE,
		VD2_AFBC_MIF_VER_SCOPE,
		VD2_AFBC_PIXEL_HOR_SCOPE,
		VD2_AFBC_PIXEL_VER_SCOPE,
		VD2_AFBC_VD_CFMT_H,

	},
	{
		DI_INP_AFBC_ENABLE,
		DI_INP_AFBC_MODE,
		DI_INP_AFBC_SIZE_IN,
		DI_INP_AFBC_DEC_DEF_COLOR,
		DI_INP_AFBC_CONV_CTRL,
		DI_INP_AFBC_LBUF_DEPTH,
		DI_INP_AFBC_HEAD_BADDR,
		DI_INP_AFBC_BODY_BADDR,
		DI_INP_AFBC_SIZE_OUT,
		DI_INP_AFBC_OUT_YSCOPE,
		DI_INP_AFBC_STAT,
		DI_INP_AFBC_VD_CFMT_CTRL,
		DI_INP_AFBC_VD_CFMT_W,
		DI_INP_AFBC_MIF_HOR_SCOPE,
		DI_INP_AFBC_MIF_VER_SCOPE,
		DI_INP_AFBC_PIXEL_HOR_SCOPE,
		DI_INP_AFBC_PIXEL_VER_SCOPE,
		DI_INP_AFBC_VD_CFMT_H,

	},
	{
		DI_MEM_AFBC_ENABLE,
		DI_MEM_AFBC_MODE,
		DI_MEM_AFBC_SIZE_IN,
		DI_MEM_AFBC_DEC_DEF_COLOR,
		DI_MEM_AFBC_CONV_CTRL,
		DI_MEM_AFBC_LBUF_DEPTH,
		DI_MEM_AFBC_HEAD_BADDR,
		DI_MEM_AFBC_BODY_BADDR,
		DI_MEM_AFBC_SIZE_OUT,
		DI_MEM_AFBC_OUT_YSCOPE,
		DI_MEM_AFBC_STAT,
		DI_MEM_AFBC_VD_CFMT_CTRL,
		DI_MEM_AFBC_VD_CFMT_W,
		DI_MEM_AFBC_MIF_HOR_SCOPE,
		DI_MEM_AFBC_MIF_VER_SCOPE,
		DI_MEM_AFBC_PIXEL_HOR_SCOPE,
		DI_MEM_AFBC_PIXEL_VER_SCOPE,
		DI_MEM_AFBC_VD_CFMT_H,
	},

};

/*keep order with struct afbce_bits_s*/
static const unsigned int reg_afbc_e[AFBC_ENC_NUB][DIM_AFBCE_NUB] = {
	{
		DI_AFBCE_ENABLE,		/*  0 */
		DI_AFBCE_MODE,			/*  1 */
		DI_AFBCE_SIZE_IN,		/*  2 */
		DI_AFBCE_BLK_SIZE_IN,		/*  3 */
		DI_AFBCE_HEAD_BADDR,		/*  4 */

		DI_AFBCE_MIF_SIZE,		/*  5 */
		DI_AFBCE_PIXEL_IN_HOR_SCOPE,	/*  6 */
		DI_AFBCE_PIXEL_IN_VER_SCOPE,	/*  7 */
		DI_AFBCE_CONV_CTRL,		/*  8 */
		DI_AFBCE_MIF_HOR_SCOPE,		/*  9 */
		DI_AFBCE_MIF_VER_SCOPE,		/* 10 */
		/**/
		DI_AFBCE_FORMAT,		/* 11 */
		/**/
		DI_AFBCE_DEFCOLOR_1,		/* 12 */
		DI_AFBCE_DEFCOLOR_2,		/* 13 */
		DI_AFBCE_QUANT_ENABLE,		/* 14 */
		//unsigned int mmu_num,
		DI_AFBCE_MMU_RMIF_CTRL1,	/* 15 */
		DI_AFBCE_MMU_RMIF_CTRL2,	/* 16 */
		DI_AFBCE_MMU_RMIF_CTRL3,	/* 17 */
		DI_AFBCE_MMU_RMIF_CTRL4,	/* 18 */
		DI_AFBCE_MMU_RMIF_SCOPE_X,	/* 19 */
		DI_AFBCE_MMU_RMIF_SCOPE_Y,	/* 20 */

		/**********************/
		DI_AFBCE_MODE_EN,
		DI_AFBCE_DWSCALAR,
		DI_AFBCE_IQUANT_LUT_1,
		DI_AFBCE_IQUANT_LUT_2,
		DI_AFBCE_IQUANT_LUT_3,
		DI_AFBCE_IQUANT_LUT_4,
		DI_AFBCE_RQUANT_LUT_1,
		DI_AFBCE_RQUANT_LUT_2,
		DI_AFBCE_RQUANT_LUT_3,
		DI_AFBCE_RQUANT_LUT_4,
		DI_AFBCE_YUV_FORMAT_CONV_MODE,
		DI_AFBCE_DUMMY_DATA,
		DI_AFBCE_CLR_FLAG,
		DI_AFBCE_STA_FLAGT,
		DI_AFBCE_MMU_NUM,
		DI_AFBCE_STAT1,		/*read only*/
		DI_AFBCE_STAT2,
		DI_AFBCE_MMU_RMIF_RO_STAT,
	},
};

static const unsigned int *afbc_get_addrp(enum EAFBC_DEC eidx)
{
	return &reg_AFBC[eidx][0];
}

static const unsigned int *afbce_get_addrp(enum EAFBC_ENC eidx)
{
	return &reg_afbc_e[eidx][0];
}

static void dump_afbcd_reg(void)
{
	u32 i;
	u32 afbc_reg;

	pr_info("---- dump afbc EAFBC_DEC0 reg -----\n");
	for (i = 0; i < AFBC_REG_INDEX_NUB; i++) {
		afbc_reg = reg_AFBC[EAFBC_DEC0][i];
		pr_info("reg 0x%x val:0x%x\n", afbc_reg, reg_rd(afbc_reg));
	}
	pr_info("---- dump afbc EAFBC_DEC1 reg -----\n");
	for (i = 0; i < AFBC_REG_INDEX_NUB; i++) {
		afbc_reg = reg_AFBC[EAFBC_DEC1][i];
		pr_info("reg 0x%x val:0x%x\n", afbc_reg, reg_rd(afbc_reg));
	}
	pr_info("reg 0x%x val:0x%x\n",
		VD1_AFBCD0_MISC_CTRL, reg_rd(VD1_AFBCD0_MISC_CTRL));
	pr_info("reg 0x%x val:0x%x\n",
		VD2_AFBCD1_MISC_CTRL, reg_rd(VD2_AFBCD1_MISC_CTRL));
}

int dbg_afbc_cfg_show(struct seq_file *s, void *v)
{
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (!pafd_ctr)
		return 0;

	seq_printf(s, "%10s:%d\n", "version", pafd_ctr->fb.ver);
	seq_printf(s, "%10s:inp[%d],mem[%d]\n", "support",
		   pafd_ctr->fb.sp_inp,
		   pafd_ctr->fb.sp_mem);
	seq_printf(s, "%10s:%d\n", "mode", pafd_ctr->fb.mode);
	seq_printf(s, "%10s:inp[%d],mem[%d]\n", "dec",
		   pafd_ctr->fb.pre_dec,
		   pafd_ctr->fb.mem_dec);

	seq_printf(s, "%10s:%d\n", "int_flg", pafd_ctr->b.int_flg);
	seq_printf(s, "%10s:%d\n", "en", pafd_ctr->b.en);
	seq_printf(s, "%10s:%d\n", "level", pafd_ctr->b.chg_level);
	return 0;
}
EXPORT_SYMBOL(dbg_afbc_cfg_show);

static u32 di_requeset_afbc(bool onoff)
{
	u32 afbc_busy;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (!pafd_ctr || pafd_ctr->fb.ver >= AFBCD_V4) {
		afbc_busy = 0;
		return afbc_busy;
	}

	if (onoff)
		afbc_busy = di_request_afbc_hw(pafd_ctr->fb.pre_dec, true);
	else
		afbc_busy = di_request_afbc_hw(pafd_ctr->fb.pre_dec, false);

	di_pr_info("%s:busy:%d\n", __func__, afbc_busy);
	return afbc_busy;
}

static u32 dbg_requeset_afbc(bool onoff)
{
	u32 afbc_busy;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (!pafd_ctr)
		return 0;
	if (onoff)
		afbc_busy = di_request_afbc_hw(pafd_ctr->fb.pre_dec, true);
	else
		afbc_busy = di_request_afbc_hw(pafd_ctr->fb.pre_dec, false);

	return afbc_busy;
}

static const unsigned int *afbc_get_inp_base(void)
{
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (!pafd_ctr)
		return NULL;
	return &reg_AFBC[pafd_ctr->fb.pre_dec][0];
}

static bool afbc_is_supported(void)
{
	bool ret = false;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (!pafd_ctr || !is_cfg(EAFBCV1_CFG_EN))
		return false;

	if (pafd_ctr->fb.ver != AFBCD_NONE)
		ret = true;

	return ret;
}

static void afbc_prob(unsigned int cid)
{
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	afbc_cfg = 0;
	if (!pafd_ctr) {
		pr_error("%s:no data\n", __func__);
		return;
	}

	if (IS_IC_EF(cid, TM2B)) {
		pafd_ctr->fb.ver = AFBCD_V4;
		pafd_ctr->fb.sp_inp = 1;
		pafd_ctr->fb.sp_mem = 1;
		pafd_ctr->fb.pre_dec = EAFBC_DEC2_DI;
		pafd_ctr->fb.mem_dec = EAFBC_DEC3_MEM;
		pafd_ctr->fb.mode = 1;
		afbc_cfg = 0x1;
	} else if (IS_IC_EF(cid, TL1)) {
		pafd_ctr->fb.ver = AFBCD_V3;
		pafd_ctr->fb.sp_inp = 1;
		pafd_ctr->fb.sp_mem = 0;
		pafd_ctr->fb.pre_dec = EAFBC_DEC0;
		pafd_ctr->fb.mode = 0;
	} else if (IS_IC_EF(cid, G12A)) {
		pafd_ctr->fb.ver = AFBCD_V2;
		pafd_ctr->fb.sp_inp = 1;
		pafd_ctr->fb.sp_mem = 0;
		pafd_ctr->fb.pre_dec = EAFBC_DEC1;
		pafd_ctr->fb.mode = 0;
	} else if (IS_IC_EF(cid, GXL)) {
		pafd_ctr->fb.ver = AFBCD_V1;
		pafd_ctr->fb.sp_inp = 1;
		pafd_ctr->fb.sp_mem = 0;
		pafd_ctr->fb.pre_dec = EAFBC_DEC0;
		pafd_ctr->fb.mode = 0;
	} else {
		pafd_ctr->fb.ver = AFBCD_NONE;
		pafd_ctr->fb.sp_inp = 0;
		pafd_ctr->fb.sp_mem = 0;
		pafd_ctr->fb.pre_dec = EAFBC_DEC0;
		pafd_ctr->fb.mode = 0;
	}
}

/*
 * after g12a, framereset will not reset simple
 * wr mif of pre such as mtn&cont&mv&mcinfo wr
 */

static void afbc_reg_sw(bool on);

//unsigned int test_afbc_en;
static void afbc_sw(bool on);
#define AFBCP	(1)
static void afbc_check_chg_level(struct vframe_s *vf,
				 struct vframe_s *mem_vf,
				 struct afbcdv1_ctr_s *pctr)
{
#ifdef AFBCP
	struct di_buf_s *di_buf = NULL;
#endif
	/*check support*/
	if (pctr->fb.ver == AFBCD_NONE)
		return;

	if (!(vf->type & VIDTYPE_COMPRESS)) {
		if (pctr->b.en) {
			/*from en to disable*/
			pctr->b.en = 0;
		}

		return;
	}
	/* pach for not mask nv21 */
	if ((vf->type & AFBC_VTYPE_MASK_CHG) !=
	    (pctr->l_vtype & AFBC_VTYPE_MASK_CHG)	||
	    vf->height != pctr->l_h		||
	    vf->width != pctr->l_w		||
	    vf->bitdepth != pctr->l_bitdepth) {
		pctr->b.chg_level = 3;
		pctr->l_vtype = (vf->type & AFBC_VTYPE_MASK_SAV);
		pctr->l_h = vf->height;
		pctr->l_w = vf->width;
		pctr->l_bitdepth = vf->bitdepth;
	} else {
		if (vf->type & VIDTYPE_INTERLACE) {
			pctr->b.chg_level = 2;
			pctr->l_vtype = (vf->type & AFBC_VTYPE_MASK_SAV);
		} else {
			pctr->b.chg_level = 1;
		}
	}
	if (is_cfg(EAFBCV1_CFG_LEVE3))
		pctr->b.chg_level = 3;
#ifdef AFBCP
	/* mem */
	if (pctr->fb.mode == 1) {
		if ((mem_vf->type & AFBC_VTYPE_MASK_CHG) !=
		    (pctr->l_vt_mem & AFBC_VTYPE_MASK_CHG)) {
			pctr->b.chg_mem = 3;
			pctr->l_vt_mem = mem_vf->type;
		} else {
			pctr->b.chg_mem = 1;
		}
		di_buf = (struct di_buf_s *)mem_vf->private_data;
		if (di_buf && (di_buf->type == VFRAME_TYPE_LOCAL)) {
			#ifdef AFBC_MODE1
			di_print("buf t[%d]:nr_adr[0x%lx], afbc_adr[0x%lx]\n",
				 di_buf->index,
				 di_buf->nr_adr,
				 di_buf->afbc_adr);
			#endif
			//mem_vf->compBodyAddr = di_buf->nr_adr;
			//mem_vf->compHeadAddr = di_buf->afbc_adr;
		}
	}
#endif
	pctr->addr_h = vf->compHeadAddr;
	pctr->addr_b = vf->compBodyAddr;
	di_print("%s:vtype[0x%x],[0x%x],[0x%x]\n", __func__, pctr->l_vtype,
		 vf->type,
		 AFBC_VTYPE_MASK_SAV);
#ifdef AFBCP
	di_print("\t:mem_vtype[0x%x],[0x%x]\n", pctr->l_vt_mem,
		 mem_vf->type);
	di_print("\t:inp:addr body[0x%x] inf[0x%x]\n",
		 vf->compBodyAddr, vf->compHeadAddr);

	di_print("\t:mem:addr body[0x%x] inf[0x%x]\n",
		 mem_vf->compBodyAddr, mem_vf->compHeadAddr);
#endif
	di_print("\th[%d],w[%d],b[0x%x]\n", pctr->l_h,
		 pctr->l_w, pctr->l_bitdepth);
	di_print("\tchg_level[%d] chg_mem[%d]\n",
		 pctr->b.chg_level, pctr->l_vt_mem);
}

static void afbc_update_level1(struct vframe_s *vf, enum EAFBC_DEC dec)
{
	const unsigned int *reg = afbc_get_addrp(dec);

	reg_wr(reg[EAFBC_HEAD_BADDR], vf->compHeadAddr >> 4);
	reg_wr(reg[EAFBC_BODY_BADDR], vf->compBodyAddr >> 4);
}

static u32 enable_afbc_input_local(struct vframe_s *vf, enum EAFBC_DEC dec)
{
	unsigned int r, u, v, w_aligned, h_aligned;
	const unsigned int *reg = afbc_get_addrp(dec);
	unsigned int vfmt_rpt_first = 1, vt_ini_phase = 0;
	unsigned int out_height = 0;
	/*ary add*/
	unsigned int cvfmt_en = 0;
	unsigned int cvfm_h, rpt_pix, phase_step = 16, hold_line = 8;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	di_print("afbc_in:vf typ[0x%x]\n", vf->type);

	w_aligned = round_up((vf->width), 32);
	/*if (di_pre_stru.cur_inp_type & VIDTYPE_INTERLACE)*/
	if ((vf->type & VIDTYPE_INTERLACE) &&
	    (vf->type & VIDTYPE_VIU_422)) /*from vdin and is i */
		h_aligned = round_up((vf->height / 2), 4);
	else
		h_aligned = round_up((vf->height), 4);

	/*AFBCD working mode config*/
	r = (3 << 24) |
	    (hold_line << 16) |		/* hold_line_num : 2020 from 10 to 8*/
	    (2 << 14) | /*burst1 1:2020:ary change from 1 to 2*/
	    (vf->bitdepth & BITDEPTH_MASK);

	if (vf->bitdepth & BITDEPTH_SAVING_MODE)
		r |= (1 << 28); /* mem_saving_mode */
	if (vf->type & VIDTYPE_SCATTER)
		r |= (1 << 29);

	out_height = h_aligned;
	if (!(vf->type & VIDTYPE_VIU_422)) {
		/*from dec, process P as i*/
		if ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
			r |= 0x40;
			vt_ini_phase = 0xc;
			vfmt_rpt_first = 1;
			out_height = h_aligned >> 1;
		} else if ((vf->type & VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_BOTTOM) {
			r |= 0x80;
			vt_ini_phase = 0x4;
			vfmt_rpt_first = 0;
			out_height = h_aligned >> 1;
		}
	}

	if (IS_420P_SRC(vf->type)) {
		cvfmt_en = 1;
		vt_ini_phase = 0xc;
		cvfm_h = out_height >> 1;
		rpt_pix = 1;
		phase_step = 8;
	} else {
		cvfm_h = out_height;
		rpt_pix = 0;
	}
	reg_wr(reg[EAFBC_MODE], r);

	r = 0x1600;
	//if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
	if (pafd_ctr->fb.ver >= AFBCD_V3) {
	/* un compress mode data from vdin bit block order is
	 * different with from dos
	 */
		if (!(vf->type & VIDTYPE_VIU_422))
			r |= (1 << 19); /* dos_uncomp */

		if (vf->type & VIDTYPE_COMB_MODE)
			r |= (1 << 20);
	}
	reg_wr(reg[EAFBC_ENABLE], r);

	/*pr_info("AFBC_ENABLE:0x%x\n", reg_rd(reg[eAFBC_ENABLE]));*/

	r = 0x100;
	/* TL1 add bit[13:12]: fmt_mode; 0:yuv444; 1:yuv422; 2:yuv420
	 * di does not support yuv444, so for fmt yuv444 di will bypass+
	 */
	//if (is_meson_tl1_cpu() || is_meson_tm2_cpu()) {
	if (pafd_ctr->fb.ver >= AFBCD_V3) {
		if (vf->type & VIDTYPE_VIU_444)
			r |= (0 << 12);
		else if (vf->type & VIDTYPE_VIU_422)
			r |= (1 << 12);
		else
			r |= (2 << 12);
	}
	reg_wr(reg[EAFBC_CONV_CTRL], r);

	u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
	v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
	reg_wr(reg[EAFBC_DEC_DEF_COLOR],
	       0x3FF00000	| /*Y,bit20+*/
	       0x80 << (u + 10)	|
	       0x80 << v);

	u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
	v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
	reg_wr(reg[EAFBC_DEC_DEF_COLOR],
	       0x3FF00000	| /*Y,bit20+*/
	       0x80 << (u + 10)	|
	       0x80 << v);

	/* chroma formatter */
	reg_wr(reg[EAFBC_VD_CFMT_CTRL],
	       (rpt_pix << 28)	|
	       (1 << 21)	| /* HFORMATTER_YC_RATIO_2_1 */
	       (1 << 20)	| /* HFORMATTER_EN */
	       (vfmt_rpt_first << 16)	| /* VFORMATTER_RPTLINE0_EN */
	       (vt_ini_phase << 8)	|
	       (phase_step << 1)	| /* VFORMATTER_PHASE_BIT */
	       cvfmt_en);/* different with inp */

	/*if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) { *//*ary add for g12a*/
	if (pafd_ctr->fb.ver >= AFBCD_V3) {
		if (vf->type & VIDTYPE_VIU_444)
			reg_wr(reg[EAFBC_VD_CFMT_W],
			       (w_aligned << 16) | (w_aligned / 2));
		else
			reg_wr(reg[EAFBC_VD_CFMT_W],
			       (w_aligned << 16) | (w_aligned));
	} else {	/*ary add for g12a*/
		reg_wr(reg[EAFBC_VD_CFMT_W],
		       (w_aligned << 16) | (w_aligned / 2));
	}
	reg_wr(reg[EAFBC_MIF_HOR_SCOPE],
	       (0 << 16) | ((w_aligned >> 5) - 1));
	reg_wr(reg[EAFBC_MIF_VER_SCOPE],
	       (0 << 16) | ((h_aligned >> 2) - 1));

	reg_wr(reg[EAFBC_PIXEL_HOR_SCOPE],
	       (0 << 16) | (vf->width - 1));
	reg_wr(reg[EAFBC_PIXEL_VER_SCOPE],
	       0 << 16 | (vf->height - 1));

	reg_wr(reg[EAFBC_VD_CFMT_H], /*out_height*/cvfm_h);

	reg_wr(reg[EAFBC_SIZE_IN], (vf->height) | w_aligned << 16);
	reg_wr(reg[EAFBC_SIZE_OUT], out_height | w_aligned << 16);

	reg_wr(reg[EAFBC_HEAD_BADDR], vf->compHeadAddr >> 4);
	reg_wr(reg[EAFBC_BODY_BADDR], vf->compBodyAddr >> 4);

	return true;
}

void afbc_update_level2_inp(struct afbcdv1_ctr_s *pctr)
{
	const unsigned int *reg = afbc_get_addrp(pctr->fb.pre_dec);
	unsigned int vfmt_rpt_first = 1, vt_ini_phase = 12;
	unsigned int old_mode, old_cfmt_ctrl;

	di_print("%s\n", __func__);
	old_mode = reg_rd(reg[EAFBC_MODE]);
	old_cfmt_ctrl = reg_rd(reg[EAFBC_VD_CFMT_CTRL]);
	old_mode &= (~(0x03 << 6));
	if (!(pctr->l_vtype & VIDTYPE_VIU_422)) {
		/*from dec, process P as i*/
		if ((pctr->l_vtype & VIDTYPE_TYPEMASK) ==
		    VIDTYPE_INTERLACE_TOP) {
			old_mode |= 0x40;

			vt_ini_phase = 0xc;
			vfmt_rpt_first = 1;
			//out_height = h_aligned>>1;
		} else if ((pctr->l_vtype & VIDTYPE_TYPEMASK) ==
			   VIDTYPE_INTERLACE_BOTTOM) {
			old_mode |= 0x80;
			vt_ini_phase = 0x4;
			vfmt_rpt_first = 0;
			//out_height = h_aligned>>1;
		} else { //for p as p?
			//out_height = h_aligned>>1;
		}
	}
	reg_wr(reg[EAFBC_MODE], old_mode);
	/* chroma formatter */
	reg_wr(reg[EAFBC_VD_CFMT_CTRL],
	       old_cfmt_ctrl		|
	       (vfmt_rpt_first << 16)	| /* VFORMATTER_RPTLINE0_EN */
	       (vt_ini_phase << 8)); /* different with inp */

	reg_wr(reg[EAFBC_HEAD_BADDR], pctr->addr_h >> 4);
	reg_wr(reg[EAFBC_BODY_BADDR], pctr->addr_b >> 4);
}

static void afbce_set(struct vframe_s *vf);
static void afbce_update_level1(struct vframe_s *vf,
				const struct reg_acc *op,
				enum EAFBC_ENC enc);

static u32 enable_afbc_input(struct vframe_s *inp_vf,
			     struct vframe_s *mem_vf,
			     struct vframe_s *nr_vf)
{
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();
	//enum eAFBC_DEC dec = afbc_get_decnub();
	struct vframe_s *mem_vf2;

	if (!afbc_is_supported())
		return false;

	if (inp_vf->type & VIDTYPE_COMPRESS) {
		afbc_sw(true);
	} else {
		afbc_sw(false);
		return false;
	}

	if (is_cfg(EAFBCV1_CFG_ETEST))
		mem_vf2 = inp_vf;
	else
		mem_vf2 = mem_vf;

	afbc_check_chg_level(inp_vf, mem_vf, pafd_ctr);
	if (pafd_ctr->b.chg_level == 3 || pafd_ctr->b.chg_mem == 3) {
		/*inp*/
		enable_afbc_input_local(inp_vf, pafd_ctr->fb.pre_dec);
		if (pafd_ctr->fb.mode == 1 && pafd_ctr->b.chg_mem == 3) {
			/*mem*/
			enable_afbc_input_local(mem_vf2, pafd_ctr->fb.mem_dec);
			/*nr*/
			afbce_set(nr_vf);
		}
	} else if (pafd_ctr->b.chg_level == 2) {
		afbc_update_level2_inp(pafd_ctr);

		if (pafd_ctr->fb.mode == 1) { /*same as level 1*/
			/*mem*/
			afbc_update_level1(mem_vf2, pafd_ctr->fb.mem_dec);
			/*nr*/
			afbce_update_level1(nr_vf,
					    &di_normal_regset, EAFBC_ENC0);
		}
	} else if (pafd_ctr->b.chg_level == 1) {
		/*inp*/
		afbc_update_level1(inp_vf, pafd_ctr->fb.pre_dec);
		if (pafd_ctr->fb.mode == 1) {
			/*mem*/
			afbc_update_level1(mem_vf2, pafd_ctr->fb.mem_dec);
			/*nr*/
			afbce_update_level1(nr_vf,
					    &di_normal_regset, EAFBC_ENC0);
		}
	}
	return 0;
}

static void afbc_tm2_sw_inp(bool on)
{
	if (on)
		reg_wrb(DI_AFBCE_CTRL, 0x03, 10, 2);
	else
		reg_wrb(DI_AFBCE_CTRL, 0x00, 10, 2);
}

static void afbc_tm2_sw_mem(bool on)
{
	if (on)
		reg_wrb(DI_AFBCE_CTRL, 0x03, 12, 2);
	else
		reg_wrb(DI_AFBCE_CTRL, 0x00, 12, 2);
}

static void afbce_tm2_sw(bool on)
{
	if (on) {
		/*1: nr channel 0 to afbce*/
		reg_wrb(DI_AFBCE_CTRL, 0x01, 0, 1);
		/* nr_en: important! 1:enable nr write to DDR; */
		reg_wrb(DI_AFBCE_CTRL, 0x01, 4, 1);
	} else {
		reg_wrb(DI_AFBCE_CTRL, 0x00, 0, 1);
		reg_wrb(DI_AFBCE_CTRL, 0x01, 4, 1);
	}
}

static void afbcx_sw(bool on)	/*g12a*/
{
	unsigned int tmp;
	unsigned int mask;
	unsigned int reg_ctrl, reg_en;
	enum EAFBC_DEC dec_sel;
	const unsigned int *reg = afbc_get_inp_base();
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	dec_sel = pafd_ctr->fb.pre_dec;

	if (dec_sel == EAFBC_DEC0)
		reg_ctrl = VD1_AFBCD0_MISC_CTRL;
	else
		reg_ctrl = VD2_AFBCD1_MISC_CTRL;

	reg_en = reg[EAFBC_ENABLE];

	mask = (3 << 20) | (1 << 12) | (1 << 9);
	/*clear*/
	tmp = reg_rd(reg_ctrl) & (~mask);

	if (on) {
		tmp = tmp
			/*0:go_file 1:go_filed_pre*/
			| (2 << 20)
			/*0:afbc0 mif to axi 1:vd1 mif to axi*/
			| (1 << 12)
			/*0:afbc0 to vpp 1:afbc0 to di*/
			| (1 << 9);
		reg_wr(reg_ctrl, tmp);
		/*0:vd1 to di	1:vd2 to di */
		reg_wrb(VD2_AFBCD1_MISC_CTRL,
			(reg_ctrl == VD1_AFBCD0_MISC_CTRL) ? 0 : 1, 8, 1);
		/*reg_wr(reg_en, 0x1600);*/
		reg_wrb(VIUB_MISC_CTRL0, 1, 16, 1);
		/*TL1 add mem control bit */
		//if (is_meson_tl1_cpu() || is_meson_tm2_cpu())
		if (pafd_ctr->fb.ver == AFBCD_V3)
			reg_wrb(VD1_AFBCD0_MISC_CTRL, 1, 22, 1);
	} else {
		reg_wr(reg_ctrl, tmp);
		reg_wr(reg_en, 0x1600);
		reg_wrb(VIUB_MISC_CTRL0, 0, 16, 1);
		//if (is_meson_tl1_cpu() || is_meson_tm2_cpu())
		if (pafd_ctr->fb.ver == AFBCD_V3)
			reg_wrb(VD1_AFBCD0_MISC_CTRL, 0, 22, 1);
	}
}

static void afbc_sw_old(bool on)/*txlx*/
{
	enum EAFBC_DEC dec_sel;
	unsigned int reg_en;
	const unsigned int *reg = afbc_get_inp_base();
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	dec_sel = pafd_ctr->fb.pre_dec;
	reg_en = reg[EAFBC_ENABLE];

	if (on) {
		/* DI inp(current data) switch to AFBC */
		if (reg_rdb(VIU_MISC_CTRL0, 29, 1) != 1)
			reg_wrb(VIU_MISC_CTRL0, 1, 29, 1);
		if (reg_rdb(VIUB_MISC_CTRL0, 16, 1) != 1)
			reg_wrb(VIUB_MISC_CTRL0, 1, 16, 1);
		if (reg_rdb(VIU_MISC_CTRL1, 0, 1) != 1)
			reg_wrb(VIU_MISC_CTRL1, 1, 0, 1);
		if (dec_sel == EAFBC_DEC0) {
			/*gxl only?*/
			if (reg_rdb(VIU_MISC_CTRL0, 19, 1) != 1)
				reg_wrb(VIU_MISC_CTRL0, 1, 19, 1);
		}
		if (reg_rd(reg_en) != 0x1600)
			reg_wr(reg_en, 0x1600);

	} else {
		reg_wr(reg_en, 0);
		/* afbc to vpp(replace vd1) enable */
		if (reg_rdb(VIU_MISC_CTRL1, 0, 1)	!= 0 ||
		    reg_rdb(VIUB_MISC_CTRL0, 16, 1)	!= 0) {
			reg_wrb(VIU_MISC_CTRL1, 0, 0, 1);
			reg_wrb(VIUB_MISC_CTRL0, 0, 16, 1);
		}
	}
}

static bool afbc_is_used(void)
{
	bool ret = false;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (pafd_ctr->b.en)
		ret = true;

	return ret;
}

static bool afbc_is_free(void)
{
	bool sts = 0;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();
	u32 afbc_num = pafd_ctr->fb.pre_dec;

	if (afbc_num == EAFBC_DEC0)
		sts = reg_rdb(VD1_AFBCD0_MISC_CTRL, 8, 2);
	else if (afbc_num == EAFBC_DEC1)
		sts = reg_rdb(VD2_AFBCD1_MISC_CTRL, 8, 2);

	if (sts)
		return true;
	else
		return false;

	return sts;
}

static void afbc_power_sw(bool on)
{
	/*afbc*/
	enum EAFBC_DEC dec_sel;
	unsigned int vpu_sel;
	unsigned int reg_ctrl;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	dec_sel = pafd_ctr->fb.pre_dec;
	if (dec_sel == EAFBC_DEC0)
		vpu_sel = VPU_AFBC_DEC;
	else
		vpu_sel = VPU_AFBC_DEC1;

	switch_vpu_mem_pd_vmod(vpu_sel,
			       on ? VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (dec_sel == EAFBC_DEC0)
			reg_ctrl = VD1_AFBCD0_MISC_CTRL;
		else
			reg_ctrl = VD2_AFBCD1_MISC_CTRL;
		if (on)
			reg_wrb(reg_ctrl, 0, 0, 8);
		else
			reg_wrb(reg_ctrl, 0x55, 0, 8);
	}
		/*afbcx_power_sw(dec_sel, on);*/
}

//int afbc_reg_unreg_flag;

static void afbc_sw_tl2(bool en)
{
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (!pafd_ctr->fb.mode) {
		afbc_tm2_sw_inp(en);
	} else if (pafd_ctr->fb.mode == 1) {
		afbc_tm2_sw_inp(en);
		afbc_tm2_sw_mem(en);
		afbce_tm2_sw(en);
	}
}

static void afbc_sw(bool on)
{
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();
	bool act = false;

	/**/
	if (pafd_ctr->b.en && !on)
		act = true;
	else if (!pafd_ctr->b.en && on)
		act = true;

	if (act) {
		if (pafd_ctr->fb.ver == AFBCD_V1)
			afbc_sw_old(on);
		else if (pafd_ctr->fb.ver <= AFBCD_V3)
			afbcx_sw(on);
		else if (pafd_ctr->fb.ver == AFBCD_V4)
			afbc_sw_tl2(on);

		pafd_ctr->b.en = on;
		pr_info("di:%s:%d\n", __func__, on);
	}
}

void dbg_afbc_sw(bool on)
{
	afbc_sw(on);
}

static void afbc_input_sw(bool on);

static void afbc_reg_variable(void)
{
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (pafd_ctr->fb.ver == AFBCD_V4) {
		if (is_cfg(EAFBCV1_CFG_EMODE))
			pafd_ctr->fb.mode = 1;
		else
			pafd_ctr->fb.mode = 0;
	}
}

static void afbc_reg_sw(bool on)
{
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (!afbc_is_supported())
		return;
	di_pr_info("%s:sw[%d]\n", __func__, on);
	if (on) {
		if (pafd_ctr->fb.ver <= AFBCD_V3) {
			afbc_power_sw(true);
		} else if (pafd_ctr->fb.ver == AFBCD_V4) {
			if (is_cfg(EAFBCV1_CFG_EMODE))
				pafd_ctr->fb.mode = 1;
			else
				pafd_ctr->fb.mode = 0;
			reg_wrb(DI_AFBCE_CTRL, 0x01, 4, 1);
		}
	}
	if (!on) {
		pafd_ctr->l_vtype = 0;
		pafd_ctr->l_vt_mem = 0;
		pafd_ctr->l_h = 0;
		pafd_ctr->l_w = 0;
		pafd_ctr->l_bitdepth = 0;
		if (pafd_ctr->fb.ver <= AFBCD_V3) {
			/*input*/
			afbc_input_sw(false);

			afbc_sw(false);

			afbc_power_sw(false);
		} else {
			/*AFBCD_V4*/
			afbc_sw(false);
		}
	}
}

void dbg_afbc_on_g12a(bool on)
{
	if (on) {
		afbc_power_sw(true);
		afbc_sw(true);
	} else {
		afbc_power_sw(false);
		afbc_sw(false);
	}
}

static unsigned int afbc_count_info_size(unsigned int w, unsigned int h)
{
	unsigned int length = 0;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (afbc_is_supported() && pafd_ctr->fb.mode == 1)
		length = PAGE_ALIGN((roundup(w, 32) * roundup(h, 4)) / 32);

	pafd_ctr->size_info = length;
	return length;
}

static unsigned int afbc_count_tab_size(unsigned int buf_size)
{
	unsigned int length = 0;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (afbc_is_supported() && pafd_ctr->fb.mode == 1)
		length = PAGE_ALIGN(((buf_size * 2 + 0xfff) >> 12) *
				    sizeof(unsigned int));

	pafd_ctr->size_tab = length;
	return length;
}

static void afbc_int_tab(struct device *dev,
			 struct afbce_map_s *pcfg)
{
	bool flg;
	unsigned int *p;
	int i, cnt, last;
	unsigned int body;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();
	//struct di_mm_s *mm = dim_mm_get(ch);
	//struct di_dev_s *de_devp = get_dim_de_devp();

	if (!afbc_is_supported()	||
	    pafd_ctr->fb.mode != 1	||
	    !pcfg			||
	    !dev)
		return;

	p = (unsigned int *)di_vmap(pcfg->tabadd, pcfg->size_buf, &flg);
	if (!p) {
		pafd_ctr->b.enc_err = 1;
		pr_error("%s:vmap:0x%lx\n", __func__, pcfg->tabadd);
		return;
	}

	cnt = (pcfg->size_buf * 2 + 0xfff) >> 12;
	body = (unsigned int)(pcfg->bodyadd >> 12);
	for (i = 0; i < cnt; i++) {
		*(p + i) = body;
		body++;
	}
	last = pcfg->size_tab - (cnt * sizeof(unsigned int));

	memset((p + cnt), 0, last);

	/*debug*/
	di_pr_info("%s:tab:[0x%lx]: body[0x%lx];cnt[%d];last[%d]\n",
		   __func__,
		   pcfg->tabadd, pcfg->bodyadd, cnt, last);

	dma_sync_single_for_device(dev,
				   pcfg->tabadd,
				   pcfg->size_tab,
				   DMA_TO_DEVICE);
	if (flg)
		di_unmap_phyaddr((u8 *)p);
}

static void afbc_input_sw(bool on)
{
	const unsigned int *reg;// = afbc_get_regbase();
	unsigned int reg_AFBC_ENABLE;
	struct afbcdv1_ctr_s *pafd_ctr = div1_get_afd_ctr();

	if (!afbc_is_supported())
		return;

	reg = afbc_get_addrp(pafd_ctr->fb.pre_dec);
	reg_AFBC_ENABLE = reg[EAFBC_ENABLE];

	di_print("%s:reg[0x%x] sw[%d]\n", __func__, reg_AFBC_ENABLE, on);
	if (on)
		reg_wrb(reg_AFBC_ENABLE, 1, 8, 1);
	else
		reg_wrb(reg_AFBC_ENABLE, 0, 8, 1);

	if (pafd_ctr->fb.mode == 1) {
		/*mem*/
		reg = afbc_get_addrp(pafd_ctr->fb.mem_dec);
		reg_AFBC_ENABLE = reg[EAFBC_ENABLE];
		if (on)
			reg_wrb(reg_AFBC_ENABLE, 1, 8, 1);
		else
			reg_wrb(reg_AFBC_ENABLE, 0, 8, 1);
	}
}

void dbg_afd_reg(struct seq_file *s, enum EAFBC_DEC eidx)
{
	int i;
	unsigned int addr;

	seq_printf(s, "dump reg:afd[%d]\n", eidx);

	for (i = 0; i < AFBC_REG_INDEX_NUB; i++) {
		addr = reg_AFBC[eidx][i];
		seq_printf(s, "reg[0x%x]=0x%x.\n", addr, reg_rd(addr));
	}
}
EXPORT_SYMBOL(dbg_afd_reg);

void dbg_afe_reg(struct seq_file *s, enum EAFBC_ENC eidx)
{
	int i;
	unsigned int addr;

	seq_printf(s, "dump reg:afe[%d]\n", eidx);

	for (i = 0; i < DIM_AFBCE_NUB; i++) {
		addr = reg_afbc_e[eidx][i];
		seq_printf(s, "reg[0x%x]=0x%x.\n", addr, reg_rd(addr));
	}
}
EXPORT_SYMBOL(dbg_afe_reg);

struct afdv1_ops_s di_afd_ops = {
	.prob			= afbc_prob,
	.is_supported		= afbc_is_supported,
	.en_pre_set		= enable_afbc_input,
	.inp_sw			= afbc_input_sw,
	.reg_sw			= afbc_reg_sw,
	.reg_val		= afbc_reg_variable,
	.is_used		= afbc_is_used,
	.is_free		= afbc_is_free,
	.count_info_size	= afbc_count_info_size,
	.count_tab_size		= afbc_count_tab_size,
	.dump_reg		= dump_afbcd_reg,
	.rqst_share		= di_requeset_afbc,
	.get_d_addrp		= afbc_get_addrp,
	.get_e_addrp		= afbce_get_addrp,
	.dbg_rqst_share		= dbg_requeset_afbc,
	.int_tab		= afbc_int_tab,
	.is_cfg			= is_cfg,
};

bool di_attach_ops_afd(const struct afdv1_ops_s **ops)
{
	*ops = &di_afd_ops;
	return true;
}
EXPORT_SYMBOL(di_attach_ops_afd);

/*afbc e**********************************************************************/
/*****************************************************************************/

/*don't change order*/

static const unsigned int  afbce_default_val[DIM_AFBCE_UP_NUB] = {
	/*need add*/
	0x00000000,
	0x03044000,
	0x07800438,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
};

void vf_set_for_com(struct di_buf_s *di_buf)
{
	struct vframe_s *vf;

	vf = di_buf->vframe;

	vf->canvas0_config[0].phy_addr = di_buf->nr_adr;
	vf->type |= (VIDTYPE_COMPRESS | VIDTYPE_SCATTER);
	vf->canvas0_config[0].width = di_buf->canvas_width[NR_CANVAS],
	vf->canvas0_config[0].height = di_buf->canvas_height;
	vf->canvas0_config[0].block_mode = 0;
	vf->plane_num = 1;
	vf->canvas0Addr = -1;
	vf->canvas1Addr = -1;
	vf->compHeadAddr = di_buf->afbc_adr;
	vf->compBodyAddr = di_buf->nr_adr;
	vf->compHeight = vf->height;
	vf->compWidth  = vf->width;
	vf->bitdepth |= (BITDEPTH_U10 | BITDEPTH_V10);
#ifdef DBG_AFBC

	if (afbc_cfg_vf)
		vf->type |= afbc_cfg_vf;
	if (afbc_cfg_bt)
		vf->bitdepth |= afbc_cfg_bt;
#endif
}

#if 1
struct enc_cfg_s {
	int enable;
	int loosy_mode;
	/* loosy_mode:
	 * 0:close 1:luma loosy 2:chrma loosy 3: luma & chrma loosy
	 */

	int head_baddr;/*head addr*/
	int mmu_info_baddr;/*mmu info linear addr*/
	int reg_format_mode;/*0:444 1:422 2:420*/
	int reg_compbits_y;/*bits num after compression*/
	int reg_compbits_c;/*bits num after compression*/

	int hsize_in;/*input hsize*/
	int vsize_in;/*input hsize*/
	int enc_win_bgn_h;/*input scope*/
	int enc_win_end_h;/*input scope*/
	int enc_win_bgn_v;/*input scope*/
	int enc_win_end_v;/*input scope*/
};

/*enc: set_di_afbce_cfg ******************/
static void ori_afbce_cfg(struct enc_cfg_s *cfg,
			  const struct reg_acc *op,
			  enum EAFBC_ENC enc)
{
	const unsigned int *reg;
	int hold_line_num	= 4;
	int lbuf_depth		= 256;
	int rev_mode		= 0;
	int def_color_0		= 0x3ff;
	int def_color_1		= 0x80;
	int def_color_2		= 0x80;
	int def_color_3		= 0;
	int hblksize_out	= (cfg->hsize_in + 31) >> 5;
	int vblksize_out	= (cfg->vsize_in + 3) >> 2;
	/* output blk scope */
	int blk_out_end_h	=  cfg->enc_win_bgn_h >> 5;
	/* output blk scope */
	int blk_out_bgn_h	= (cfg->enc_win_end_h + 31) >> 5;
	/*output blk scope */
	int blk_out_end_v	=  cfg->enc_win_bgn_v >> 2;
	/* output blk scope */
	int blk_out_bgn_v	= (cfg->enc_win_end_v + 3) >> 2;

	int lossy_luma_en;
	int lossy_chrm_en;
	int cur_mmu_used	= 0;/*mmu info linear addr*/

	int reg_fmt444_comb;
	int uncmp_size       ;/*caculate*/
	int uncmp_bits       ;/*caculate*/
	int sblk_num         ;/*caculate*/

	reg = &reg_afbc_e[enc][0];

	/*yuv444 can only support 8bit,and must use comb_mode*/
	if (cfg->reg_format_mode == 0 && cfg->hsize_in > 2048)
		reg_fmt444_comb = 1;
	else
		reg_fmt444_comb = 0;

	/* 4*4 subblock number in every 32*4 mblk */
	if (cfg->reg_format_mode == 2)
		sblk_num = 12;
	else if (cfg->reg_format_mode == 1)
		sblk_num = 16;
	else
		sblk_num = 24;

	if (cfg->reg_compbits_y > cfg->reg_compbits_c)
		uncmp_bits = cfg->reg_compbits_y;
	else
		uncmp_bits = cfg->reg_compbits_c;

	/*bit size of uncompression mode*/
	uncmp_size = (((((16 * uncmp_bits * sblk_num) + 7) >> 3) +
			31) / 32) << 1;

	/*chose loosy mode of luma and chroma */
	if (cfg->loosy_mode == 0) {
		lossy_luma_en = 0;
		lossy_chrm_en = 0;
	} else if (cfg->loosy_mode == 1) {
		lossy_luma_en = 1;
		lossy_chrm_en = 0;
	} else if (cfg->loosy_mode == 2) {
		lossy_luma_en = 0;
		lossy_chrm_en = 1;
	} else {
		lossy_luma_en = 1;
		lossy_chrm_en = 1;
	}

	op->wr(reg[EAFBCE_MODE],
	       (0                & 0x7)		<< 29 |
	       (rev_mode         & 0x3)		<< 26 |
	       (3                & 0x3)		<< 24 |
	       (hold_line_num    & 0x7f)	<< 16 |
	       (2                & 0x3)		<< 14 |
	       (reg_fmt444_comb  & 0x1));
	/* loosy */
	op->bwr(reg[EAFBCE_QUANT_ENABLE], (lossy_luma_en & 0x1), 0, 1);
	op->bwr(reg[EAFBCE_QUANT_ENABLE], (lossy_chrm_en & 0x1), 4, 1);

	/* hsize_in of afbc input*/
	/* vsize_in of afbc input*/
	op->wr(reg[EAFBCE_SIZE_IN],
	       ((cfg->hsize_in & 0x1fff) << 16) |
	       ((cfg->vsize_in & 0x1fff) << 0)
	);

	/* out blk hsize*/
	/* out blk vsize*/
	op->wr(reg[EAFBCE_BLK_SIZE_IN],
	       ((hblksize_out & 0x1fff) << 16) |
	       ((vblksize_out & 0x1fff) << 0)
	);

	/*head addr of compressed data*/
	op->wr(reg[EAFBCE_HEAD_BADDR], cfg->head_baddr);

	/*uncmp_size*/
	op->bwr(reg[EAFBCE_MIF_SIZE], (uncmp_size & 0x1fff), 16, 5);

	/* scope of hsize_in ,should be a integer multipe of 32*/
	/* scope of vsize_in ,should be a integer multipe of 4*/
	op->wr(reg[EAFBCE_PIXEL_IN_HOR_SCOPE],
	       ((cfg->enc_win_end_h & 0x1fff) << 16) |
	       ((cfg->enc_win_bgn_h & 0x1fff) << 0));

	/* scope of hsize_in ,should be a integer multipe of 32*/
	/* scope of vsize_in ,should be a integer multipe of 4*/
	op->wr(reg[EAFBCE_PIXEL_IN_VER_SCOPE],
	       ((cfg->enc_win_end_v & 0x1fff) << 16) |
	       ((cfg->enc_win_bgn_v & 0x1fff) << 0)
	);

	/*fix 256*/
	op->wr(reg[EAFBCE_CONV_CTRL], lbuf_depth);

	/* scope of out blk hsize*/
	/* scope of out blk vsize*/
	op->wr(reg[EAFBCE_MIF_HOR_SCOPE],
	       ((blk_out_bgn_h & 0x3ff) << 16) |
	       ((blk_out_end_h & 0xfff) << 0)
	);

	/* scope of out blk hsize*/
	/* scope of out blk vsize*/
	op->wr(reg[EAFBCE_MIF_VER_SCOPE],
	       ((blk_out_bgn_v & 0x3ff) << 16) |
	       ((blk_out_end_v & 0xfff) << 0)
	);

	op->wr(reg[EAFBCE_FORMAT],
	       (cfg->reg_format_mode	& 0x3) << 8 |
	       (cfg->reg_compbits_c	& 0xf) << 4 |
	       (cfg->reg_compbits_y	& 0xf));

	/* def_color_a*/
	/* def_color_y*/
	op->wr(reg[EAFBCE_DEFCOLOR_1],
	       ((def_color_3 & 0xfff) << 12) |
	       ((def_color_0 & 0xfff) << 0));

	/* def_color_v*/
	/* def_color_u*/
	op->wr(reg[EAFBCE_DEFCOLOR_2],
	       ((def_color_2 & 0xfff) << 12) |
	       ((def_color_1 & 0xfff) << 0));

	/*4k addr have used in every frame;*/
	/*cur_mmu_used += Rd(DI_AFBCE_MMU_NUM);*/

	op->bwr(reg[EAFBCE_MMU_RMIF_CTRL4], cfg->mmu_info_baddr, 0, 32);
	op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], cur_mmu_used, 0, 12);
	op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], 0x1ffe, 16, 13);
	op->bwr(reg[EAFBCE_MMU_RMIF_CTRL3], 0x1fff, 0, 13);

	if (is_meson_tm2b())//dis afbce mode from vlsi xianjun.fan
		op->bwr(reg[EAFBCE_MODE_EN], 0x1, 18, 1);
	op->bwr(reg[EAFBCE_ENABLE], cfg->enable, 8, 1);//enable afbce
	op->bwr(DI_AFBCE_CTRL, cfg->enable, 0, 1);//di pre to afbce
	op->bwr(DI_AFBCE_CTRL, cfg->enable, 4, 1);//di pre to afbce
}

/* set_di_afbce_cfg */
static void afbce_set(struct vframe_s *vf)
{
	struct enc_cfg_s cfg_data;
	struct enc_cfg_s *cfg = &cfg_data;
	struct di_buf_s	*di_buf;

	if (!vf) {
		pr_error("%s:0:no vf\n", __func__);
		return;
	}
	di_buf = (struct di_buf_s *)vf->private_data;

	if (!di_buf) {
		pr_error("%s:1:no di buf\n", __func__);
		return;
	}

	cfg->enable = 1;
	/* 0:close 1:luma lossy 2:chrma lossy 3: luma & chrma lossy*/
	cfg->loosy_mode = 0;
	cfg->head_baddr = di_buf->afbc_adr;//head_baddr_enc;/*head addr*/
	cfg->mmu_info_baddr = di_buf->afbct_adr;
	//vf->compHeadAddr = cfg->head_baddr;
	//vf->compBodyAddr = di_buf->nr_adr;
	vf_set_for_com(di_buf);
#ifdef AFBCP
	di_print("%s:buf[%d],hadd[0x%x],info[0x%x]\n",
		 __func__,
		 di_buf->index,
		 cfg->head_baddr,
		 cfg->mmu_info_baddr);
#endif
	cfg->reg_format_mode = 1;/*0:444 1:422 2:420*/
	cfg->reg_compbits_y = 10;//8;/*bits num after compression*/
	cfg->reg_compbits_c = 10;//8;/*bits num after compression*/

	/*input size*/
	cfg->hsize_in = vf->width;//src_w;
	cfg->vsize_in = vf->height;//src_h;
	/*input scope*/
	cfg->enc_win_bgn_h = 0;
	cfg->enc_win_end_h = vf->width - 1;
	cfg->enc_win_bgn_v = 0;
	cfg->enc_win_end_v = vf->height - 1;

	ori_afbce_cfg(cfg, &di_normal_regset, EAFBC_ENC0);
}

static void afbce_update_level1(struct vframe_s *vf,
				const struct reg_acc *op,
				enum EAFBC_ENC enc)
{
	const unsigned int *reg;
	struct di_buf_s *di_buf;
	unsigned int cur_mmu_used = 0;

	di_buf = (struct di_buf_s *)vf->private_data;

	if (!di_buf) {
		pr_error("%s:1:no di buf\n", __func__);
		return;
	}

	reg = &reg_afbc_e[enc][0];
	di_print("di_buf:t[%d][%d],adr[0x%lx],inf[0x%lx]\n",
		 di_buf->type, di_buf->index,
		 di_buf->nr_adr, di_buf->afbc_adr);
	//vf->compHeadAddr = di_buf->afbc_adr;
	//vf->compBodyAddr = di_buf->nr_adr;
	vf_set_for_com(di_buf);

	//head addr of compressed data
	op->wr(reg[EAFBCE_HEAD_BADDR], di_buf->afbc_adr);
	op->bwr(reg[EAFBCE_MMU_RMIF_CTRL4], di_buf->afbct_adr, 0, 32);
	op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], cur_mmu_used, 0, 12);
	op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], 0x1ffe, 16, 13);
	op->bwr(reg[EAFBCE_MMU_RMIF_CTRL3], 0x1fff, 0, 13);
}
#endif
#endif

