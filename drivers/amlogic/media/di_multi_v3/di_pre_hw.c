/*
 * drivers/amlogic/media/di_multi_v3/di_pre_hw.c
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/seq_file.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include "deinterlace.h"
#include "deinterlace_dbg.h"

#include "di_data_l.h"
#include "di_data.h"
#include "di_dbg.h"
#include "di_vframe.h"
#include "di_que.h"
#include "di_task.h"

#include "di_prc.h"
#include "di_pre.h"

#include "nr_downscale.h"
#include "register.h"

#if 0
/****************************************
 * 1. copy curr to last
 * 2. set curr
 ****************************************/
void prev3_vinfo_set(unsigned int ch,
		   struct vframe_s *ori_vframe)
{
	struct di_hpre_s  *pre = get_hw_pre();

	struct di_vinfo_s *vc = &pre->vinf_curr;
	struct di_vinfo_s *vl = &pre->vinf_lst;

	memcpy(vl, vc, sizeof(struct di_vinfo_s));

	vc->ch = ch;
	vc->vtype = ori_vframe->type;
	vc->src_type = ori_vframe->source_type;
	vc->trans_fmt = ori_vframe->trans_fmt;

	if (COM_ME(ori_vframe->type, VIDTYPE_COMPRESS)) {
		vc->h = ori_vframe->compWidth;
		vc->v = ori_vframe->compHeight;
	} else {
		vc->h = ori_vframe->width;
		vc->v = ori_vframe->height;
	}
}

/****************************************
 * compare current vframe info with last
 * return
 *	0. no change
 *	1. video format channge
 *	2. scan mode channge?
 ****************************************/
unsigned int isv3_vinfo_change(unsigned int ch)
{
	struct di_hpre_s  *pre = get_hw_pre();

	struct di_vinfo_s *vc = &pre->vinf_curr;
	struct di_vinfo_s *vl = &pre->vinf_lst;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	unsigned int ret = 0;

	if (vc->src_type != vl->src_type			||
	    !COM_M(DI_VFM_T_MASK_CHANGE, vc->vtype, vl->vtype)	||
	    vc->v != vl->v					||
	    vc->h != vl->h					||
	    vc->trans_fmt != vl->trans_fmt) {
		/* video format changed */
		ret = 1;
	} else if (!COM_M(VIDTYPE_VIU_FIELD, vc->vtype, vl->vtype))
		/* just scan mode changed */
		ret = 2;

	if (ret) {
		dimv3_print(
		"%s:ch[%d]: %dth source change 2: 0x%x/%d/%d/%d=>0x%x/%d/%d/%d\n",
			__func__,
			ch,
			/*jiffies_to_msecs(jiffies_64),*/
			ppre->in_seq,
			vl->vtype,
			vl->h,
			vl->v,
			vl->src_type,
			vc->vtype,
			vc->h,
			vc->v,
			vc->src_type);
	}

	return ret;
}
#endif
/****************************************
 * 1. copy curr to last
 * 2. set curr
 ****************************************/
void prev3_vinfo_set(unsigned int ch, struct di_in_inf_s *pinf)
{
	struct di_hpre_s  *pre = get_hw_pre();

	struct di_in_inf_s *vc = &pre->vinf_curr;
	struct di_in_inf_s *vl = &pre->vinf_lst;

	memcpy(vl, vc, sizeof(struct di_in_inf_s));

	*vc = *pinf;
}

/****************************************
 * compare current vframe info with last
 * return
 *	0. no change
 *	1. video format channge
 *	2. scan mode channge?
 ****************************************/
unsigned int isv3_vinfo_change(unsigned int ch)
{
	struct di_hpre_s  *pre = get_hw_pre();

	struct di_in_inf_s *vc = &pre->vinf_curr;
	struct di_in_inf_s *vl = &pre->vinf_lst;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	unsigned int ret = 0;

	if (vc->src_type != vl->src_type				||
	    !COM_M(DI_VFM_T_MASK_CHANGE, vc->vtype_ori, vl->vtype_ori)	||
	    vc->w != vl->w						||
	    vc->h != vl->h						||
	    vc->trans_fmt != vl->trans_fmt) {
		/* video format changed */
		ret = 1;
	} else if (!COM_M(VIDTYPE_VIU_FIELD, vc->vtype_ori, vl->vtype_ori))
		/* just scan mode changed */
		ret = 2;

	if (ret) {
		dimv3_print(
		"%s:ch[%d]: %dth source change 2: 0x%x/%d/%d/%d=>0x%x/%d/%d/%d\n",
			__func__,
			ch,
			/*jiffies_to_msecs(jiffies_64),*/
			ppre->in_seq,
			vl->vtype_ori,
			vl->h,
			vl->w,
			vl->src_type,
			vc->vtype_ori,
			vc->h,
			vc->w,
			vc->src_type);
	}

	return ret;
}

unsigned char isv3_source_change_dfvm(struct dim_dvfm_s *plstdvfm,
				    struct dim_dvfm_s *pdvfm,
				    unsigned int ch)
{
	struct di_pre_stru_s *ppre = get_pre_stru(ch);

#define VFRAME_FORMAT_MASK      \
	(VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_444 | \
	 VIDTYPE_MVC)

#if 0
	if ((ppre->cur_width != vframe->width)			||
	    (ppre->cur_height != vframe->height)		||
	    (((ppre->cur_inp_type & VFRAME_FORMAT_MASK)		!=
	    (vframe->type & VFRAME_FORMAT_MASK))		&&
	    (!is_handle_prog_frame_as_interlace(vframe)))	||
	    (ppre->cur_source_type != vframe->source_type)) {
		/* video format changed */
		return 1;
	} else if (((ppre->cur_prog_flag != is_progressive(vframe)) &&
		   (!is_handle_prog_frame_as_interlace(vframe))) ||
		   ((ppre->cur_inp_type & VIDTYPE_VIU_FIELD) !=
		   (vframe->type & VIDTYPE_VIU_FIELD))
	) {
		/* just scan mode changed */
		if (!ppre->force_interlace)
			pr_dbg("DI I<->P.\n");
		return 2;
	}
	return 0;
#endif
	/*********************************************/
//	struct dim_dvfm_s *plstdvfm = &ppre->lst_dvfm;
	if (plstdvfm->in_inf.h != pdvfm->in_inf.h		||
	    plstdvfm->in_inf.w != pdvfm->in_inf.w		||
	    !COM_M(VFRAME_FORMAT_MASK,
		   plstdvfm->wmode.vtype,
		   pdvfm->wmode.vtype)				||
	    plstdvfm->in_inf.src_type != pdvfm->in_inf.src_type) {
	/* video format changed */
		return 1;
	} else if (plstdvfm->wmode.is_i != pdvfm->wmode.is_i	||
		   !COM_M(VIDTYPE_VIU_FIELD,
			  plstdvfm->wmode.vtype,
			  pdvfm->wmode.vtype)) {
		/* just scan mode changed */
		if (!ppre->force_interlace)
			pr_dbg("DI I<->P.\n");
		return 2;
	}

	return 0;
}

void dimv3_set_pre_dfvm(struct dim_dvfm_s *plstdvfm,
		      struct dim_dvfm_s *pdvfm)
{
//	struct di_pre_stru_s *ppre = get_pre_stru(ch);
//	struct dim_dvfm_s *plstdvfm = &ppre->lst_dvfm;

	plstdvfm->in_inf	= pdvfm->in_inf;
	plstdvfm->wmode		= pdvfm->wmode;
}

void dimv3_set_pre_dfvm_last_bypass(struct di_ch_s *pch)
{
	struct dim_dvfm_s *plstdvfm;

	plstdvfm = &pch->lst_dvfm;
	plstdvfm->in_inf.h = 0;
	plstdvfm->in_inf.w = 0;
}
#if 0
bool dimv3_bypass_detect(unsigned int ch, struct vframe_s *vfm)
{
	bool ret = false;

	if (!vfm)
		return ret;
	prev3_vinfo_set(ch, vfm);
	if (isv3_vinfo_change(ch)) {
		if (!isv3_bypass2(vfm, ch)) {
			setv3_bypass2_complete(ch, false);
			PR_INF("%s:\n", __func__);
			/*task_send_ready();*/
			taskv3_send_cmd(LCMD1(eCMD_CHG, ch));
			ret = true;
		}
	}
	return ret;
}
#endif
/****************************************
 *
 ****************************************/
static void hpre_prob_hw(struct di_hpre_s *hpre)
{

}

static void hpre_reg_var(struct di_hpre_s *hpre)
{
#if 0
	struct di_pre_stru_s *ppre;

	set_h_ppre(ch);
	/*cfg for reg*/
	dimv3_tst_4k_reg_val();

	/*clear pre*/
	ppre = hpre->ops.pres;
	memset(ppre, 0, sizeof(struct di_pre_stru_s));
#endif
}

static void hpre_reg_hw(struct di_hpre_s *hpre)
{

}

static void hpre_remove_hw(struct di_hpre_s *hpre)
{

}

static void hpre_unreg_var(struct di_hpre_s *hpre)
{

}

static void hpre_unreg_hw(struct di_hpre_s *hpre)
{

}

static const struct dim_hpre_ops_s dim_hpre_ops = {
	.prob_hw	= hpre_prob_hw,
	.reg_var	= hpre_reg_var,
	.reg_hw		= hpre_reg_hw,
	.remove_hw	= hpre_remove_hw,
	.unreg_var	= hpre_unreg_var,
	.unreg_hw	= hpre_unreg_hw,

};

void hprev3_prob(void)
{
	struct di_hpre_s  *pre = get_hw_pre();

	memcpy(&pre->ops, &dim_hpre_ops, sizeof(struct dim_hpre_ops_s));
	set_h_ppre(0);
}

void hprev3_remove(void)
{
	struct di_hpre_s  *pre = get_hw_pre();

	memset(&pre->ops, 0, sizeof(struct dim_hpre_ops_s));
}
