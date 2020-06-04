/*
 * drivers/amlogic/media/di_multi_v3/di_vframe.c
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

#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>

#include "deinterlace.h"
#include "di_data_l.h"
#include "di_pre.h"
#include "di_pres.h"
#include "di_prc.h"
#include "di_dbg.h"
#include "di_que.h"
#include "di_task.h"

#include "di_vframe.h"
#include "di_vfm_test.h"

static DEFINE_SPINLOCK(di_lock3);

#define dim_lock_irqfiq_save(irq_flag) \
	spin_lock_irqsave(&di_lock3, irq_flag)

#define dim_unlock_irqfiq_restore(irq_flag) \
	spin_unlock_irqrestore(&di_lock3, irq_flag)

struct dev_vfram_t *getv3_dev_vframe(unsigned int ch)
{
	if (ch < DI_CHANNEL_NUB)
		//return &get_datal()->ch_data[ch].vfm;
		return &get_datal()->ch_data[ch].interf.u.dvfm;

	PR_ERR("%s ch overflow %d\n", __func__, ch);
	return &get_datal()->ch_data[0].interf.u.dvfm;
}

#if 1//TST_NEW_INS_RUN_Q
const char * const div3_rev_name[4] = {
	"dimulti.1",
	"deinterlace",
	"dimulti.2",
	"dimulti.3",
};
#else
const char * const div3_rev_name[4] = {
	"deinterlace",
	"dimulti.1",
	"dimulti.2",
	"dimulti.3",
};

#endif
static bool dev_vframe_reg(void *data, struct dim_inter_s *pintf)
{
	struct vframe_receiver_s *preceiver;
	struct dev_vfram_t *pvfm;
	char *provider_name = (char *)data;
	const char *receiver_name = NULL;

	pvfm = &pintf->u.dvfm;
	if (pintf->reg) {
		PR_WARN("duplicate reg\n");
		return false;
	}
	vf_reg_provider(&pvfm->di_vf_prov);
	vf_notify_receiver(pintf->name, VFRAME_EVENT_PROVIDER_START, NULL);
	pintf->reg = 1;

	if (!provider_name) {
		PR_WARN("%s:provider null\n", __func__);
		return false;
	}
	preceiver = vf_get_receiver(pintf->name);

	if (!preceiver) {
		PR_WARN("%s:preceiver null\n", __func__);
		return false;
	}

	receiver_name = preceiver->name;

	if (!receiver_name) {
		PR_WARN("%s:receiver null\n", __func__);
		return false;
	}

	dbg_ev("reg:%s,%s\n", provider_name, receiver_name);
	return true;
}

void devv3_vframe_unreg(struct dim_inter_s *pintf)
{
	struct dev_vfram_t *pvfm;

	pvfm = &pintf->u.dvfm;
	if (pintf->reg) {
		vf_unreg_provider(&pvfm->di_vf_prov);
		pintf->reg = 0;
	} else {
		PR_WARN("duplicate ureg\n");
	}
}

static int di_vf_l_states(struct vframe_states *states, unsigned int ch)
{
	struct di_mm_s *mm = dim_mm_get(ch);
	struct dim_sum_s *psumx = get_sumx(ch);

	if (!states)
		return -1;
	states->vf_pool_size = mm->sts.num_local;
	states->buf_free_num = psumx->b_pre_free;

	states->buf_avail_num = psumx->b_pst_ready;
	states->buf_recycle_num = psumx->b_recyc;
	if (dimp_get(eDI_MP_di_dbg_mask) & 0x1) {
		di_pr_info("di-pre-ready-num:%d\n", psumx->b_pre_ready);
		di_pr_info("di-display-num:%d\n", psumx->b_display);
	}
	return 0;
}

static int div3_ori_event_qurey_state(unsigned int channel)
{
	/*int in_buf_num = 0;*/
	struct vframe_states states;

	#ifdef HIS_V3
	if (recovery_flag)
		return RECEIVER_INACTIVE;
	#endif
	/*fix for ucode reset method be break by di.20151230*/
	di_vf_l_states(&states, channel);
	if (states.buf_avail_num > 0)
		return RECEIVER_ACTIVE;

	if (pwv3_vf_notify_receiver(
		channel,
		VFRAME_EVENT_PROVIDER_QUREY_STATE,
		NULL) == RECEIVER_ACTIVE)
		return RECEIVER_ACTIVE;

	return RECEIVER_INACTIVE;
}

#ifdef HIS_V3 /*no use*/
void di_vframe_reg(unsigned int ch)
{
	struct dev_vfram_t *pvfm;

	pvfm = getv3_dev_vframe(ch);

	dev_vframe_reg(pvfm);
}

void di_vframe_unreg(unsigned int ch)
{
	struct dev_vfram_t *pvfm;

	pvfm = getv3_dev_vframe(ch);
	devv3_vframe_unreg(pvfm);
}
#endif

/*--------------------------*/

static const char * const di_receiver_event_cmd[] = {
	"",
	"_UNREG",
	"_LIGHT_UNREG",
	"_START",
	NULL,	/* "_VFRAME_READY", */
	NULL,	/* "_QUREY_STATE", */
	"_RESET",
	NULL,	/* "_FORCE_BLACKOUT", */
	"_REG",
	"_LIGHT_UNREG_RETURN_VFRAME",
	NULL,	/* "_DPBUF_CONFIG", */
	NULL,	/* "_QUREY_VDIN2NR", */
	NULL,	/* "_SET_3D_VFRAME_INTERLEAVE", */
	NULL,	/* "_FR_HINT", */
	NULL,	/* "_FR_END_HINT", */
	NULL,	/* "_QUREY_DISPLAY_INFO", */
	NULL	/* "_PROPERTY_CHANGED", */
};

#define VFRAME_EVENT_PROVIDER_CMD_MAX	16

static int di_receiver_event_fun(int type, void *data, void *arg)
{
	struct dev_vfram_t *pvfm;
	unsigned int ch;
	int ret = 0;
	struct dim_inter_s *pintf;
//	bool retb = false;
	ch = *(int *)arg;

	pintf = get_dev_intf(ch);
	pvfm = getv3_dev_vframe(ch);

	if (type <= VFRAME_EVENT_PROVIDER_CMD_MAX	&&
	    di_receiver_event_cmd[type]) {
		dbg_ev("ch[%d]:%s,%d:%s\n", ch, __func__,
		       type,
		       di_receiver_event_cmd[type]);
	}

	switch (type) {
	case VFRAME_EVENT_PROVIDER_UNREG:
		devv3_vframe_unreg(pintf);
		dipv3_event_unreg_chst(ch);
		//dev_vframe_unreg(pintf);
/*		task_send_cmd(LCMD1(eCMD_UNREG, 0));*/
		break;
	case VFRAME_EVENT_PROVIDER_REG:
		if (dev_vframe_reg(data, pintf))
			ret = dipv3_event_reg_chst(ch);
		break;
	case VFRAME_EVENT_PROVIDER_START:
		break;

	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		ret = div3_ori_event_light_unreg(ch);
		break;
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		/*ret = di_ori_event_ready(ch);*/
		if (isv3_bypss_complete(pintf)) {
			vf_notify_receiver
				(pintf->name,
				 VFRAME_EVENT_PROVIDER_VFRAME_READY,
				 NULL);
		}
		ret = 0;
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		ret = div3_ori_event_qurey_state(ch);
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		ret = div3_ori_event_reset(ch);
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME:
		ret = div3_ori_event_light_unreg_revframe(ch);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR:
		ret = div3_ori_event_qurey_vdin2nr(ch);
		break;
	case VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE:
		div3_ori_event_set_3D(type, data, ch);
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
		vf_notify_receiver(pintf->name, type, data);
		break;

	default:
		break;
	}

	return ret;
}

static const struct vframe_receiver_op_s di_vf_receiver = {
	.event_cb	= di_receiver_event_fun
};

bool vfv3_type_is_prog(unsigned int type)
{
	bool ret = (type & VIDTYPE_TYPEMASK) == 0 ? true : false;

	return ret;
}

bool vfv3_type_is_interlace(unsigned int type)
{
	bool ret = (type & VIDTYPE_INTERLACE) ? true : false;

	return ret;
}

bool vfv3_type_is_top(unsigned int type)
{
	bool ret = ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP)
		? true : false;
	return ret;
}

bool vfv3_type_is_bottom(unsigned int type)
{
	bool ret = ((type & VIDTYPE_INTERLACE_BOTTOM)
		== VIDTYPE_INTERLACE_BOTTOM)
		? true : false;

	return ret;
}

bool vfv3_type_is_inter_first(unsigned int type)
{
	bool ret = (type & VIDTYPE_INTERLACE_TOP) ? true : false;

	return ret;
}

bool vfv3_type_is_mvc(unsigned int type)
{
	bool ret = (type & VIDTYPE_MVC) ? true : false;

	return ret;
}

bool vfv3_type_is_no_video_en(unsigned int type)
{
	bool ret = (type & VIDTYPE_NO_VIDEO_ENABLE) ? true : false;

	return ret;
}

bool vfv3_type_is_VIU422(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_422) ? true : false;

	return ret;
}

bool vfv3_type_is_VIU_FIELD(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_FIELD) ? true : false;

	return ret;
}

bool vfv3_type_is_VIU_SINGLE(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_SINGLE_PLANE) ? true : false;

	return ret;
}

bool vfv3_type_is_VIU444(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_444) ? true : false;

	return ret;
}

bool vfv3_type_is_VIUNV21(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_NV21) ? true : false;

	return ret;
}

bool vfv3_type_is_vscale_dis(unsigned int type)
{
	bool ret = (type & VIDTYPE_VSCALE_DISABLE) ? true : false;

	return ret;
}

bool vfv3_type_is_canvas_toggle(unsigned int type)
{
	bool ret = (type & VIDTYPE_CANVAS_TOGGLE) ? true : false;

	return ret;
}

bool vfv3_type_is_pre_interlace(unsigned int type)
{
	bool ret = (type & VIDTYPE_PRE_INTERLACE) ? true : false;

	return ret;
}

bool vfv3_type_is_highrun(unsigned int type)
{
	bool ret = (type & VIDTYPE_HIGHRUN) ? true : false;

	return ret;
}

bool vfv3_type_is_compress(unsigned int type)
{
	bool ret = (type & VIDTYPE_COMPRESS) ? true : false;

	return ret;
}

bool vfv3_type_is_pic(unsigned int type)
{
	bool ret = (type & VIDTYPE_PIC) ? true : false;

	return ret;
}

bool vfv3_type_is_scatter(unsigned int type)
{
	bool ret = (type & VIDTYPE_SCATTER) ? true : false;

	return ret;
}

bool vfv3_type_is_vd2(unsigned int type)
{
	bool ret = (type & VIDTYPE_VD2) ? true : false;

	return ret;
}

bool isv3_bypss_complete(struct dim_inter_s *pintf)
{
	return pintf->bypass_complete;
}

#ifdef HIS_V3
bool is_reg(unsigned int ch)
{
	struct dev_vfram_t *pvfm;

	pvfm = getv3_dev_vframe(ch);

	return pvfm->reg;
}
#endif

void setv3_bypass_complete(struct dim_inter_s *pintf, bool on)
{
	if (on)
		pintf->bypass_complete = true;
	else
		pintf->bypass_complete = false;
}

void setv3_bypass2_complete(unsigned int ch, bool on)
{
	struct dim_inter_s *pintf;

	pintf = get_dev_intf(ch);
	setv3_bypass_complete(pintf, on);
}

bool isv3_bypss2_complete(unsigned int ch)
{
	struct dim_inter_s *pintf;

	pintf = get_dev_intf(ch);

	return isv3_bypss_complete(pintf);
}

/**********************************************************
 * vf_fill_post_ready
 *	default for CONFIG_AMLOGIC_MEDIA_MULTI_DEC
 *	for pw en
 **********************************************************/
void vfv3_fill_post_ready(struct di_ch_s *pch, struct di_buf_s *di_buf)
{
	struct vframe_s *tvfm = NULL;
	struct di_buf_s *nr_buf = NULL;
	struct canvas_config_s *pcvs;
	unsigned int pw_en = (dimp_get(eDI_MP_post_wr_en)	&&
			      dimp_get(eDI_MP_post_wr_support));
	struct dim_dvfm_s	*pdvfm;
	struct dim_dvfm_s	*pdvfmg;

	if (!pw_en) {
		PR_ERR("%s:not pw mode, do nothing\n", __func__);
		return;
	}
	pdvfm = di_buf->c.pdvfm;
	if (!pdvfm) {
		PR_ERR("%s:no pdvfm, do nothing\n", __func__);
		return;
	}

	tvfm = di_buf->vframe;
	memcpy(tvfm, &pdvfm->vframe.vfm, sizeof(*tvfm));
	tvfm->private_data	= di_buf;

	if (di_buf->c.wmode.is_bypass) { /*bypass*/
		tvfm->type |= VIDTYPE_PRE_INTERLACE;
		pdvfmg = dvfmv3_get(pch, QUED_T_PRE);
		di_buf->c.pdvfm = NULL;
		if (pdvfmg->index != pdvfm->index) {
			PR_ERR("%s:not map:%d->%d\n", __func__,
			       pdvfm->index,
			       pdvfmg->index);
			return;
		}

		qued_ops.in(pch, QUED_T_RECYCL, pdvfm->index);
		return;
	}

	/*bitdepth*/
	tvfm->bitdepth &= ~(BITDEPTH_YMASK | FULL_PACK_422_MODE);
	tvfm->bitdepth |= pch->cfgt.vfm_bitdepth;

	/*type*/
	tvfm->type	= VIDTYPE_PROGRESSIVE		|
			  VIDTYPE_VIU_422		|
			  VIDTYPE_VIU_SINGLE_PLANE	|
			  VIDTYPE_VIU_FIELD;
	/* ? add for vpp skip line ref*/
	tvfm->type |= VIDTYPE_PRE_INTERLACE;

	tvfm->type &= ~TB_DETECT_MASK;

	/*canvans*/
	pcvs = &tvfm->canvas0_config[0];
	if (di_buf->c.process_fun_index != PROCESS_FUN_NULL) {
		nr_buf = di_buf->c.di_buf_dup_p[1];

		pcvs->phy_addr		= di_buf->nr_adr;
		pcvs->width		= di_buf->canvas_width[NR_CANVAS];
		pcvs->height		= di_buf->canvas_height;
		pcvs->block_mode	= 0;

		if (dimp_get(eDI_MP_show_nrwr)) {
			pcvs->phy_addr	= nr_buf->nr_adr;
			pcvs->width	= nr_buf->canvas_width[NR_CANVAS];
			pcvs->height	= nr_buf->canvas_height;
		}

	} else {
		/* p use 2 i buf*/

		if (!di_buf->c.wmode.p_use_2i) {
			PR_ERR("%s:not p use 2i\n", __func__);
			return;
		}
		nr_buf = di_buf->c.di_buf[0];
		pcvs->phy_addr	= nr_buf->nr_adr;
		pcvs->width	= nr_buf->canvas_width[NR_CANVAS],
		pcvs->height	= nr_buf->canvas_height << 1;
		pcvs->block_mode = 0;
	}
	tvfm->plane_num = 1;
	tvfm->canvas0Addr = -1;
	tvfm->canvas1Addr = -1;

	tvfm->early_process_fun = NULL;
	tvfm->process_fun = NULL;

	/* 2019-04-22 Suggestions from brian.zhu*/
	tvfm->mem_handle = NULL;
	tvfm->type |= VIDTYPE_DI_PW;
	/* 2019-04-22 */

	/*ary tmp : dvfm recycle*/
	//pch = get_chdata(channel);
	pdvfmg = dvfmv3_get(pch, QUED_T_PRE);
	if (pdvfmg->index != pdvfm->index) {
		PR_ERR("%s:not map:%d->%d\n", __func__,
		       pdvfm->index,
		       pdvfmg->index);
		return;
	}
	dbg_vfmv3(tvfm, 6);

	qued_ops.in(pch, QUED_T_RECYCL, pdvfm->index);
}

/**********************************************************
 * vf_fill_post_ready_ins
 *	default for CONFIG_AMLOGIC_MEDIA_MULTI_DEC
 *	for pw en
 *	1. vf no need cp
 *	2. only change type bitdepth
 **********************************************************/
void vfv3_fill_post_ready_ins(struct di_ch_s *pch, struct di_buf_s *di_buf,
			      struct di_buffer *buffer)
{
	struct vframe_s *tvfm = NULL;
	struct di_buffer *ins;
#ifdef DIM_OUT_NV21
	struct di_hpst_s  *pst = get_hw_pst();
#else
	struct di_buf_s *nr_buf = NULL;
#endif
	struct canvas_config_s *pcvs, *pcvsf;
	unsigned int pw_en = (dimp_get(eDI_MP_post_wr_en)	&&
			      dimp_get(eDI_MP_post_wr_support));
	struct dim_dvfm_s	*pdvfm;
	struct dim_dvfm_s	*pdvfmg;

	if (!pw_en) {
		PR_ERR("%s:not pw mode, do nothing\n", __func__);
		return;
	}
	pdvfm = di_buf->c.pdvfm;
	if (!pdvfm) {
		PR_ERR("%s:no pdvfm, do nothing\n", __func__);
		return;
	}
	if (!buffer) {
		PR_ERR("%s:no ins buf\n", __func__);
		return;
	}

	if (!di_buf->c.wmode.is_eos &&
	    !buffer->vf) {
		PR_ERR("%s:ins buf no vfm:indx[%d]\n", __func__,
		       buffer->mng.index);
		return;
	}

	if (di_buf->c.wmode.is_eos) {
		pdvfmg = dvfmv3_get(pch, QUED_T_PRE);
		di_buf->c.pdvfm = NULL;
		if (pdvfmg->index != pdvfm->index) {
			PR_ERR("%s:not map:%d->%d\n", __func__,
			       pdvfm->index,
			       pdvfmg->index);
			return;
		}

		buffer->flag |= DI_FLAG_EOS;
		buffer->flag |= DI_FLAG_BUF_BY_PASS;
		PR_INF("%s:%d:EOS\n", __func__, buffer->mng.index);
		qued_ops.in(pch, QUED_T_RECYCL, pdvfm->index);
		return;
	}

	if (di_buf->c.wmode.is_bypass) { /*bypass*/
		tvfm = buffer->vf;
		tvfm->type |= VIDTYPE_PRE_INTERLACE;
		pdvfmg = dvfmv3_get(pch, QUED_T_PRE);
		di_buf->c.pdvfm = NULL;
		if (pdvfmg->index != pdvfm->index) {
			PR_ERR("%s:not map:%d->%d\n", __func__,
			       pdvfm->index,
			       pdvfmg->index);
			return;
		}
		/*input*/
		ins = (struct di_buffer *)pdvfm->vfm_in;
		tvfm->vf_ext = ins->vf;
		dbg_vfmv3(tvfm->vf_ext, 5);
		buffer->flag |= DI_FLAG_BUF_BY_PASS;
		dimv3_print("%s:%p:bypass flg\n", __func__, buffer);
		qued_ops.in(pch, QUED_T_RECYCL, pdvfm->index);
		return;
	}

	tvfm = buffer->vf;
	dimv3_print("%s:ins_buf[%d],vf[%d]\n", __func__,
		    buffer->mng.index, buffer->vf->omx_index);
	dimv3_print("\t:di_buf[%d,%d]\n",
		    di_buf->index, di_buf->c.vmode.omx_index);

	buffer->vf->vf_ext = NULL;
#ifdef DIM_OUT_NV21
	/*bitdepth*/
	tvfm->bitdepth = pst->vf_post.bitdepth;

	/*type*/
	tvfm->type	= pst->vf_post.type;

	/* ? add for vpp skip line ref*/
	tvfm->type |= VIDTYPE_PRE_INTERLACE;

	/*canvans*/
	tvfm->plane_num = pst->vf_post.plane_num;
#else
	/*bitdepth*/
	tvfm->bitdepth &= ~(BITDEPTH_YMASK | FULL_PACK_422_MODE);
	tvfm->bitdepth |= pch->cfgt.vfm_bitdepth;

	/*type*/
	tvfm->type	&= ~(0xf807); /*clear*/
	tvfm->type	= VIDTYPE_PROGRESSIVE		|
			  VIDTYPE_VIU_422		|
			  VIDTYPE_VIU_SINGLE_PLANE	|
			  VIDTYPE_VIU_FIELD;
	/* ? add for vpp skip line ref*/
	tvfm->type |= VIDTYPE_PRE_INTERLACE;

	tvfm->type &= ~TB_DETECT_MASK;
#endif
	pcvs = &tvfm->canvas0_config[0];
	//pcvs->phy_addr = buffer->phy_addr;
	//pcvs->block_mode	= 0;
#ifndef DIM_OUT_NV21
	#if 1
	if (di_buf->c.process_fun_index != PROCESS_FUN_NULL) {
		nr_buf = di_buf->c.di_buf_dup_p[1];

		//pcvs->phy_addr		= di_buf->nr_adr;
		pcvs->width		= di_buf->canvas_width[NR_CANVAS];
		pcvs->height		= di_buf->canvas_height;
		pcvs->block_mode	= 0;

		if (dimp_get(eDI_MP_show_nrwr)) {
			pcvs->phy_addr	= nr_buf->nr_adr;
			pcvs->width	= nr_buf->canvas_width[NR_CANVAS];
			pcvs->height	= nr_buf->canvas_height;
		}

	} else {
		/* p use 2 i buf*/

		if (!di_buf->c.wmode.p_use_2i) {
			PR_ERR("%s:not p use 2i\n", __func__);
			return;
		}
		nr_buf = di_buf->c.di_buf[0];
		//pcvs->phy_addr	= nr_buf->nr_adr;
		pcvs->width	= nr_buf->canvas_width[NR_CANVAS],
		pcvs->height	= nr_buf->canvas_height << 1;
		pcvs->block_mode = 0;
	}
	#endif
	tvfm->plane_num = 1;
#endif
	tvfm->canvas0Addr = -1;
	tvfm->canvas1Addr = -1;
#ifdef DIM_OUT_NV21
	#ifdef HIS_V3
	memcpy(&tvfm->canvas0_config[0],
	       &pst->vf_post.canvas0_config[0],
	       sizeof(tvfm->canvas0_config[0]));
	memcpy(&tvfm->canvas0_config[1],
	       &pst->vf_post.canvas0_config[1],
	       sizeof(tvfm->canvas0_config[1]));
	#else
	pcvsf = &pst->vf_post.canvas0_config[0];
	if (pcvs->phy_addr != pcvsf->phy_addr)
		PR_ERR("ready not map phy 0x%x, 0x%x", pcvs->phy_addr,
		       pcvsf->phy_addr);
	if (pcvs->width != pcvsf->width)
		PR_ERR("ready not map width 0x%x, 0x%x", pcvs->width,
		       pcvsf->width);
	if (pcvs->height != pcvsf->height)
		PR_ERR("ready not map height 0x%x, 0x%x", pcvs->height,
		       pcvsf->height);
	pcvs->block_mode = pcvsf->block_mode;
	pcvs->endian = pcvsf->endian;

	pcvs = &tvfm->canvas0_config[1];
	pcvsf = &pst->vf_post.canvas0_config[1];
	pcvs->block_mode = pcvsf->block_mode;
	pcvs->endian = pcvsf->endian;
	#endif
#endif
	tvfm->early_process_fun = NULL;
	tvfm->process_fun = NULL;

	/* 2019-04-22 Suggestions from brian.zhu*/
	tvfm->mem_handle = NULL;
	tvfm->type |= VIDTYPE_DI_PW;
	/* 2019-04-22 */
	dbg_vfmv3(tvfm, 4);
	/*ary tmp : dvfm recycle*/
	//pch = get_chdata(channel);
	pdvfmg = dvfmv3_get(pch, QUED_T_PRE);
	if (pdvfmg->index != pdvfm->index) {
		PR_ERR("%s:not map:%d->%d\n", __func__,
		       pdvfm->index,
		       pdvfmg->index);
		return;
	}

	qued_ops.in(pch, QUED_T_RECYCL, pdvfm->index);
}

/**************************************
 * get and put in QUED_T_IN
 **************************************/
struct dim_dvfm_s *dvfmv3_fill_in(struct di_ch_s *pch)
{
	unsigned int in_nub, free_nub;
//	unsigned int dvfm_id;
	int i;
	struct vframe_s *pvfm;
	struct vframe_s *pvfm_ori;
	//union dim_itf_u		*pvfm;
	//union dim_itf_u		*pvfm_ori;
	struct dim_dvfm_s	*pdvfm;
	unsigned int cfg_prog_proc;
	struct di_dev_s *de_devp = getv3_dim_de_devp();
	unsigned int tmpu;
	struct dim_itf_ops_s *pvfmops;// = &pch->interf.dvfm.pre_ops;
	unsigned int reason = 0;

	if (!pch)
		return NULL;
	pvfmops = &pch->interf.opsi;

	in_nub = qued_ops.listv3_count(pch, QUED_T_IN);
	free_nub = qued_ops.listv3_count(pch, QUED_T_FREE);
	if (in_nub > DIM_K_VFM_IN_LIMIT ||
	    !free_nub) {
		pdvfm = dvfmv3_peek(pch, QUED_T_IN);
		return pdvfm;
	}
	for (i = 0; i < DIM_K_VFM_IN_LIMIT - in_nub; i++) {
		pvfm_ori = (struct vframe_s *)pvfmops->get(pch);
		if (!pvfm_ori)
			break;
		/*****move from pre cfg*********/
		/* if bad vfm put back*/
		if (pch->pret.bad_frame > 0	||
		    pvfm_ori->width > 10000		||
		    pvfm_ori->height > 10000	||
		    div3_cfgx_get(pch->ch_id, EDI_CFGX_HOLD_VIDEO)) {
			if (pvfm_ori->width > 10000 || pvfm_ori->height > 10000)
				pch->pret.bad_frame = 10;
			pch->pret.bad_frame--;
			PR_WARN("%s:ch[%d]:h[%d],w[%d],cnt[%d]\n",
				__func__, pch->ch_id,
				pvfm_ori->height, pvfm_ori->width,
				pch->pret.bad_frame);
			//pw_vf_put(pvfm_ori, pch->ch_id);
			//pw_vfprov_note_put(&pch->interf.dvfm);
			pvfmops->put((void *)pvfm_ori, pch);
			continue;
		}
		/*******************************/
		pdvfm = dvfmv3_get(pch, QUED_T_FREE);

		if (!pdvfm) {
			PR_ERR("%s:2\n", __func__);
			//pw_vf_put(pvfm_ori, pch->ch_id);
			pvfmops->put((void *)pvfm_ori, pch);
			break;
		}
		pvfm = &pdvfm->vframe.vfm;
		pdvfm->vfm_in = pvfm_ori;
		memcpy(pvfm, pvfm_ori, sizeof(*pvfm));
		/*update info*/
		pdvfm->etype = EDIM_DISP_T_IN;
		pdvfm->in_inf.ch	= pch->ch_id;
		pdvfm->in_inf.vtype_ori	= pvfm->type;

		pdvfm->in_inf.src_type	= pvfm->source_type;
		pdvfm->in_inf.trans_fmt = pvfm->trans_fmt;
		pdvfm->in_inf.sig_fmt	= pvfm->sig_fmt;
		pdvfm->in_inf.h		= pvfm->height;
		pdvfm->in_inf.w		= pvfm->width;

		if (IS_COMP_MODE(pvfm->type)) {
			pdvfm->in_inf.h	= pvfm->compHeight;
			pdvfm->in_inf.w	= pvfm->compWidth;
			pdvfm->wmode.is_afbc = 1;
		}

		pdvfm->wmode.o_h	= pdvfm->in_inf.h;
		pdvfm->wmode.o_w	= pdvfm->in_inf.w;

		pdvfm->wmode.vtype	= pvfm->type;
		if (VFMT_IS_I(pvfm->type)) {
			pdvfm->wmode.is_i = 1;
			if (VFMT_IS_TOP(pvfm->type))
				pdvfm->wmode.is_top = 1;
		}

		if (IS_VDIN_SRC(pvfm->source_type))
			pdvfm->wmode.is_vdin = 1;

		if (dimv3_need_bypass2(&pdvfm->in_inf, pvfm)) {
			pdvfm->wmode.need_bypass	= 1;
			pdvfm->wmode.is_bypass		= 1;
		}

		if (isv3_bypass2(&pdvfm->vframe.vfm, pch, &reason))
			pdvfm->wmode.is_bypass		= 1;

		if (dimv3_get_trick_mode()) {
			pdvfm->wmode.is_bypass	= 1;
			pdvfm->wmode.trick_mode	= 1;
		}

		/*top botom invert ?*/
		if (!pdvfm->wmode.need_bypass &&
		    pdvfm->wmode.is_i) {
			pdvfm->wmode.is_invert_tp =
				(pvfm->type &  TB_DETECT_MASK) ? 1 : 0;
			if (dimv3_get_invert_tb())
				pdvfm->wmode.is_invert_tp = 1;

			/*sw top and bottom flg*/
			if (pdvfm->wmode.is_invert_tp) {
				/*re set*/
				pdvfm->wmode.vtype =
					topv3_bot_config(pvfm->type);
			}

			/*note: change input vframe*/
			/*pvfm->type &= ~TB_DETECT_MASK;*/
		}

		if (!pdvfm->wmode.need_bypass &&
		    !pdvfm->wmode.is_bypass) {
			pdvfm->wmode.src_w = roundup(pdvfm->in_inf.w,
							pch->cfgt.w_rdup);
			/*debug mode: force h/w*/
			if (pch->cfgt.f_w) {
				pdvfm->wmode.src_w	= roundup(pch->cfgt.f_w,
							pch->cfgt.w_rdup);
				pdvfm->wmode.o_w	= pch->cfgt.f_w;
			}

			pdvfm->wmode.src_h = pdvfm->in_inf.h;
			if (pch->cfgt.f_h) {
				pdvfm->wmode.src_h	= pch->cfgt.f_h;
				pdvfm->wmode.o_h	= pch->cfgt.f_h;
			}
			#ifdef V_DIV4
			if (pdvfm->wmode.is_i) {
			/*v need % 4*/
				pdvfm->wmode.src_h = roundup(pdvfm->wmode.src_h,
							4);
			}
			#endif

			pdvfm->wmode.tgt_w = pdvfm->wmode.src_w;
			pdvfm->wmode.tgt_h = pdvfm->wmode.src_h;
			/*pp scaler*/
			if (de_devp->pps_enable &&
			    dimp_get(eDI_MP_pps_position)) {
				tmpu = dimp_get(eDI_MP_pps_dstw);
				if (tmpu != pdvfm->wmode.src_w) {
					pdvfm->wmode.tgt_w	= tmpu;
					pdvfm->wmode.o_w	= tmpu;
				}

				tmpu = dimp_get(eDI_MP_pps_dsth);
				if (tmpu != pdvfm->wmode.src_h) {
					pdvfm->wmode.tgt_h = tmpu;
					pdvfm->wmode.o_h = tmpu;
				}
			} else if (pch->cfgt.ens.b.h_sc_down) {
				tmpu = dimp_get(eDI_MP_pre_hsc_down_width);
				if (tmpu != pdvfm->wmode.src_w) {
					PR_INF("hscd %d to %d\n",
					       pdvfm->wmode.src_w,
					       tmpu);
					pdvfm->wmode.tgt_w = tmpu;
					pdvfm->wmode.o_w = tmpu;
				}
			}

			cfg_prog_proc =
				(unsigned int)dimp_get(eDI_MP_prog_proc_config);
			if (cfg_prog_proc & 0x20)
				pdvfm->wmode.prog_proc_config = 1;

			if (!pdvfm->wmode.is_i) { /*p*/
				if (pdvfm->wmode.is_vdin || cfgg(PMODE) == 0) {
				/*p as p*/
					;
				} else if (pdvfm->wmode.prog_proc_config) {
					pdvfm->wmode.p_as_i = 1;
					pdvfm->wmode.is_top = 1;
				} else if (cfgg(PMODE) == 2) {
					pdvfm->wmode.p_use_2i = 1;
				}
			}

			if (pvfm->video_angle)
				pdvfm->wmode.is_angle = 1;

			/*vtype*/
			pdvfm->vmode.vtype = pvfm->type & DIM_VFM_MASK_ALL;
			/* use this to check*/
			pdvfm->vmode.canvas0Addr = pvfm->canvas0Addr;
			pdvfm->vmode.canvas1Addr = pvfm->canvas0Addr;
			pdvfm->vmode.h		= pdvfm->wmode.src_h;
			pdvfm->vmode.w		= pdvfm->wmode.src_w;
			pdvfm->vmode.omx_index	= pvfm->omx_index;
			pdvfm->vmode.ready_jiffies64 = pvfm->ready_jiffies64;
			pdvfm->vmode.bitdepth	= pvfm->bitdepth;
			/* vtype->bit_mode */
			if (pvfm->bitdepth & BITDEPTH_Y10) {
				if (pvfm->type & VIDTYPE_VIU_444)
					pdvfm->vmode.bit_mode =
					(pvfm->bitdepth & FULL_PACK_422_MODE) ?
					3 : 2;
				else if (pvfm->type & VIDTYPE_VIU_422)
					pdvfm->vmode.bit_mode =
					(pvfm->bitdepth & FULL_PACK_422_MODE) ?
					3 : 1;
			} else {
				pdvfm->vmode.bit_mode = 0;
			}
		}

		pdvfm->di_buf = NULL;

		/*put*/
		qued_ops.in(pch, QUED_T_IN, pdvfm->index);
	}

	pdvfm = dvfmv3_peek(pch, QUED_T_IN);
	return pdvfm;
}

struct dim_dvfm_s *dvfmv3_fill_in_ins(struct di_ch_s *pch)
{
	unsigned int in_nub, free_nub;
//	unsigned int dvfm_id;
	int i;
	struct vframe_s *pvfm;
	struct vframe_s *pvfm_ori;
	//union dim_itf_u		*pvfm;
	//union dim_itf_u		*pvfm_ori;
	struct dim_dvfm_s	*pdvfm;
	unsigned int cfg_prog_proc;
	struct di_dev_s *de_devp = getv3_dim_de_devp();
	unsigned int tmpu;
	struct dim_itf_ops_s *pvfmops;// = &pch->interf.dvfm.pre_ops;
	struct di_buffer *ins_buf;
	unsigned int reason = 0;

	if (!pch)
		return NULL;

//	if (pch->pause)
//		return NULL;

	pvfmops = &pch->interf.opsi;

	in_nub = qued_ops.listv3_count(pch, QUED_T_IN);
	free_nub = qued_ops.listv3_count(pch, QUED_T_FREE);
	if (in_nub > DIM_K_VFM_IN_LIMIT ||
	    !free_nub) {
		pdvfm = dvfmv3_peek(pch, QUED_T_IN);
		return pdvfm;
	}

	if (prev3_run_flag == DI_RUN_FLAG_RUN) /*avoid print when ppause*/
		dbg_dbg("%s:ch[%d],in[%d]\n", __func__, pch->ch_id, in_nub);

	for (i = 0; i < DIM_K_VFM_IN_LIMIT - in_nub; i++) {
		ins_buf = pvfmops->peek(pch);
		if (!ins_buf)
			break;
		ins_buf	= (struct di_buffer *)pvfmops->get(pch);
		if (!ins_buf) {
			PR_ERR("%s:can peek but can't get\n", __func__);
			break;
		}
		/*************************************************************/
		if (ins_buf->flag & DI_FLAG_EOS) {
			dbg_dbg("%s:eos\n", __func__);

			pdvfm = dvfmv3_get(pch, QUED_T_FREE);

			if (!pdvfm) {
				PR_ERR("%s:2\n", __func__);
				//pw_vf_put(pvfm_ori, pch->ch_id);
				//pvfmops->put((void *)pvfm_ori, pch);
				break;
			}
			pdvfm->vfm_in = ins_buf;
			pdvfm->wmode.is_eos	= 1;
			pdvfm->wmode.is_bypass	= 1;

			pdvfm->di_buf = NULL;

			/*put*/
			qued_ops.in(pch, QUED_T_IN, pdvfm->index);

			/* pause by EOS */
//			pch->pause |= DI_BIT0;
			break;
		}
		/*************************************************************/
		pvfm_ori = ins_buf->vf;
		if (!pvfm_ori) {
			PR_ERR("%s:no vf\n", __func__);
			break;
		}
		dbg_dbg("\t:ins_buf:c[0x%x],id[%d]\n",
			ins_buf->mng.code,
			ins_buf->mng.index);
		dbg_dbg("\t:ad[%p],h[%d],v[%d],tp[0x%x]\n",
			pvfm_ori,
			pvfm_ori->height,
			pvfm_ori->width,
			pvfm_ori->type);

		if (pch->pret.bad_frame > 0	||
		    /*****move from pre cfg*********/
		    /* if bad vfm put back*/
		    pvfm_ori->width > 10000		||
		    pvfm_ori->height > 10000	||
		    div3_cfgx_get(pch->ch_id, EDI_CFGX_HOLD_VIDEO)) {
			if (pvfm_ori->width > 10000 || pvfm_ori->height > 10000)
				pch->pret.bad_frame = 10;
			pch->pret.bad_frame--;
			PR_WARN("%s:ch[%d]:h[%d],w[%d],cnt[%d]\n",
				__func__, pch->ch_id,
				pvfm_ori->height, pvfm_ori->width,
				pch->pret.bad_frame);
			//pw_vf_put(pvfm_ori, pch->ch_id);
			//pw_vfprov_note_put(&pch->interf.dvfm);
			pvfmops->put((void *)ins_buf, pch);
			continue;
		}
		/*******************************/
		pdvfm = dvfmv3_get(pch, QUED_T_FREE);

		if (!pdvfm) {
			PR_ERR("%s:2\n", __func__);
			//pw_vf_put(pvfm_ori, pch->ch_id);
			pvfmops->put((void *)pvfm_ori, pch);
			break;
		}
		pvfm = &pdvfm->vframe.vfm;
		pdvfm->vfm_in = ins_buf;
		memcpy(pvfm, pvfm_ori, sizeof(*pvfm));
		memcpy(&pdvfm->vframe.dbuf, ins_buf,
		       sizeof(pdvfm->vframe.dbuf));
		/*update info*/
		pdvfm->etype = EDIM_DISP_T_IN;
		pdvfm->in_inf.ch	= pch->ch_id;
		pdvfm->in_inf.vtype_ori	= pvfm->type;

		pdvfm->in_inf.src_type	= pvfm->source_type;
		pdvfm->in_inf.trans_fmt = pvfm->trans_fmt;
		pdvfm->in_inf.sig_fmt	= pvfm->sig_fmt;
		pdvfm->in_inf.h		= pvfm->height;
		pdvfm->in_inf.w		= pvfm->width;

		if (IS_COMP_MODE(pvfm->type)) {
			pdvfm->in_inf.h	= pvfm->compHeight;
			pdvfm->in_inf.w	= pvfm->compWidth;
			pdvfm->wmode.is_afbc = 1;
		}

		pdvfm->wmode.o_h	= pdvfm->in_inf.h;
		pdvfm->wmode.o_w	= pdvfm->in_inf.w;

		pdvfm->wmode.vtype	= pvfm->type;
		if (VFMT_IS_I(pvfm->type)) {
			pdvfm->wmode.is_i = 1;
			if (VFMT_IS_TOP(pvfm->type))
				pdvfm->wmode.is_top = 1;
		}

		if (IS_VDIN_SRC(pvfm->source_type))
			pdvfm->wmode.is_vdin = 1;

		if (dimv3_need_bypass2(&pdvfm->in_inf, pvfm)) {
			pdvfm->wmode.need_bypass	= 1;
			pdvfm->wmode.is_bypass		= 1;
		}

		if (isv3_bypass2(&pdvfm->vframe.vfm, pch, &reason))
			pdvfm->wmode.is_bypass		= 1;

		if (dimv3_get_trick_mode()) {
			pdvfm->wmode.is_bypass	= 1;
			pdvfm->wmode.trick_mode	= 1;
		}

		/*top botom invert ?*/
		if (!pdvfm->wmode.need_bypass &&
		    pdvfm->wmode.is_i) {
			pdvfm->wmode.is_invert_tp =
				(pvfm->type &  TB_DETECT_MASK) ? 1 : 0;
			if (dimv3_get_invert_tb())
				pdvfm->wmode.is_invert_tp = 1;

			/*sw top and bottom flg*/
			if (pdvfm->wmode.is_invert_tp) {
				/*re set*/
				pdvfm->wmode.vtype =
					topv3_bot_config(pvfm->type);
			}

			/*note: change input vframe*/
			/*pvfm->type &= ~TB_DETECT_MASK;*/
		}

		if (!pdvfm->wmode.need_bypass &&
		    !pdvfm->wmode.is_bypass) {
			pdvfm->wmode.src_w = roundup(pdvfm->in_inf.w,
							pch->cfgt.w_rdup);
			/*debug mode: force h/w*/
			if (pch->cfgt.f_w) {
				pdvfm->wmode.src_w	= roundup(pch->cfgt.f_w,
							pch->cfgt.w_rdup);
				pdvfm->wmode.o_w	= pch->cfgt.f_w;
			}

			pdvfm->wmode.src_h = pdvfm->in_inf.h;
			if (pch->cfgt.f_h) {
				pdvfm->wmode.src_h	= pch->cfgt.f_h;
				pdvfm->wmode.o_h	= pch->cfgt.f_h;
			}
			#ifdef V_DIV4
			if (pdvfm->wmode.is_i) {
			/*v need % 4*/
				pdvfm->wmode.src_h = roundup(pdvfm->wmode.src_h,
							4);
			}
			#endif
			pdvfm->wmode.tgt_w = pdvfm->wmode.src_w;
			pdvfm->wmode.tgt_h = pdvfm->wmode.src_h;
			/*pp scaler*/
			if (de_devp->pps_enable &&
			    dimp_get(eDI_MP_pps_position)) {
				tmpu = dimp_get(eDI_MP_pps_dstw);
				if (tmpu != pdvfm->wmode.src_w) {
					pdvfm->wmode.tgt_w	= tmpu;
					pdvfm->wmode.o_w	= tmpu;
				}

				tmpu = dimp_get(eDI_MP_pps_dsth);
				if (tmpu != pdvfm->wmode.src_h) {
					pdvfm->wmode.tgt_h = tmpu;
					pdvfm->wmode.o_h = tmpu;
				}
			} else if (pch->cfgt.ens.b.h_sc_down) {
				tmpu = dimp_get(eDI_MP_pre_hsc_down_width);
				if (tmpu != pdvfm->wmode.src_w) {
					PR_INF("hscd %d to %d\n",
					       pdvfm->wmode.src_w,
					       tmpu);
					pdvfm->wmode.tgt_w = tmpu;
					pdvfm->wmode.o_w = tmpu;
				}
			}

			cfg_prog_proc =
				(unsigned int)dimp_get(eDI_MP_prog_proc_config);
			if (cfg_prog_proc & 0x20)
				pdvfm->wmode.prog_proc_config = 1;

			if (!pdvfm->wmode.is_i) { /*p*/
				if (pdvfm->wmode.is_vdin || cfgg(PMODE) == 0) {
				/*p as p*/
					;
				} else if (pdvfm->wmode.prog_proc_config) {
					pdvfm->wmode.p_as_i = 1;
					pdvfm->wmode.is_top = 1;
				} else if (cfgg(PMODE) == 2) {
					pdvfm->wmode.p_use_2i = 1;
				}
			}

			if (pvfm->video_angle)
				pdvfm->wmode.is_angle = 1;

			/*vtype*/
			pdvfm->vmode.vtype = pvfm->type & DIM_VFM_MASK_ALL;
			/* use this to check*/
			pdvfm->vmode.canvas0Addr = pvfm->canvas0Addr;
			pdvfm->vmode.canvas1Addr = pvfm->canvas0Addr;
			pdvfm->vmode.h		= pdvfm->wmode.src_h;
			pdvfm->vmode.w		= pdvfm->wmode.src_w;
			pdvfm->vmode.omx_index	= pvfm->index_disp;
			pdvfm->vmode.ready_jiffies64 = pvfm->ready_jiffies64;
			pdvfm->vmode.bitdepth	= pvfm->bitdepth;
			/* vtype->bit_mode */
			if (pvfm->bitdepth & BITDEPTH_Y10) {
				if (pvfm->type & VIDTYPE_VIU_444)
					pdvfm->vmode.bit_mode =
					(pvfm->bitdepth & FULL_PACK_422_MODE) ?
					3 : 2;
				else if (pvfm->type & VIDTYPE_VIU_422)
					pdvfm->vmode.bit_mode =
					(pvfm->bitdepth & FULL_PACK_422_MODE) ?
					3 : 1;
			} else {
				pdvfm->vmode.bit_mode = 0;
			}
		}
		if (pdvfm->wmode.is_bypass) {
			ins_buf->flag |= DI_FLAG_BUF_BY_PASS;
			dimv3_print("%s:set bypass\n", __func__);
		}
		pdvfm->di_buf = NULL;
		dbg_dbg("\t:index[%d]bypass:is[%d]:n[%d]:r[%d]\n",
			pdvfm->index,
			pdvfm->wmode.is_bypass,
			pdvfm->wmode.need_bypass, reason);

		/*put*/
		qued_ops.in(pch, QUED_T_IN, pdvfm->index);
	}

	pdvfm = dvfmv3_peek(pch, QUED_T_IN);
	return pdvfm;
}

bool dimv3_bypass_first_frame(unsigned int ch)
{
	struct di_buf_s *di_buf = NULL;
	struct di_buf_s *di_buf_post = NULL;
	struct vframe_s *vframe;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	void **pvframe_in = getv3_vframe_in(ch);
	ulong irq_flag2 = 0;
//	struct vframe_operations_s *pvfmops;
	struct di_ch_s *pch;

	struct dim_dvfm_s	*pdvfm = NULL;

	#ifdef HIS_V3
	pch = get_chdata(ch);
	if (!pch) {
		PR_ERR("%s:no pch\n", __func__);
		return false;
	}
	pvfmops = &pch->interf.u.dvfm.pre_ops;

	vframe = pvfmops->peek(pch);//pw_vf_peek(ch);

	if (!vframe)
		return false;
	if (div3_que_is_empty(ch, QUE_POST_FREE))
		return false;

	vframe = pvfmops->get(pch);//pw_vf_get(ch);

	di_buf = div3_que_out_to_di_buf(ch, QUE_IN_FREE);

	if (dimv3_check_di_buf(di_buf, 10, ch))
		return 0;

	memcpy(di_buf->vframe, vframe, sizeof(struct vframe_s));
	di_buf->vframe->private_data = di_buf;
	pvframe_in[di_buf->index] = vframe;
	di_buf->c.seq = ppre->in_seq;
	ppre->in_seq++;

	#ifdef HIS_V3

	if (vframe->type & VIDTYPE_COMPRESS) {	/*?*/
		vframe->width = vframe->compWidth;
		vframe->height = vframe->compHeight;
	}

	div3_que_in(ch, QUE_PRE_READY, di_buf);
	#endif

	di_buf_post = div3_que_out_to_di_buf(ch, QUE_POST_FREE);
	memcpy(di_buf_post->vframe, vframe, sizeof(struct vframe_s));
	di_buf_post->vframe->private_data = di_buf_post;
	di_lock_irqfiq_save(irq_flag2);

	div3_que_in(ch, QUE_POST_READY, di_buf_post);

	di_unlock_irqfiq_restore(irq_flag2);
	pwv3_vf_notify_receiver(ch,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);

	PR_INF("%s:ok\n", __func__);
	#endif

	/*************************************************/

	/*****************get dvfm****************/
	pch = get_chdata(ch);
	pdvfm = dvfmv3_peek(pch, QUED_T_IN);
	//plstdvfm = &pch->lst_dvfm;

	if (pdvfm)
		pdvfm = dvfmv3_get(pch, QUED_T_IN);
	if (!pdvfm || !pdvfm->vfm_in) {
		PR_ERR("%s:no pdvfm\n", __func__);
		return 0;
	}
	vframe = &pdvfm->vframe.vfm;

	di_buf = div3_que_out_to_di_buf(ch, QUE_IN_FREE);

	if (dimv3_check_di_buf(di_buf, 10, ch))
		return 0;
	#ifdef HIS_V3
	memcpy(di_buf->vframe, vframe, sizeof(struct vframe_s));
	di_buf->vframe->private_data = di_buf;
	#endif
	pvframe_in[di_buf->index] = pdvfm->vfm_in;

	di_buf->c.seq = ppre->in_seq;
	ppre->in_seq++;

	pdvfm->wmode.is_bypass = 1;
	di_buf->c.wmode = pdvfm->wmode;
	//------

	di_buf_post = div3_que_out_to_di_buf(ch, QUE_POST_FREE);
	memcpy(di_buf_post->vframe, vframe, sizeof(struct vframe_s));
	di_buf_post->vframe->private_data = di_buf_post;
	di_buf_post->c.wmode = di_buf->c.wmode;

	dim_lock_irqfiq_save(irq_flag2);

	div3_que_in(ch, QUE_POST_READY, di_buf_post);

	dim_unlock_irqfiq_restore(irq_flag2);
	qued_ops.in(pch, QUED_T_RECYCL, pdvfm->index);
	pwv3_vf_notify_receiver(ch,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	PR_INF("%s:ok\n", __func__);
	return true;
}

/**********************************************************
 * pre_inp_canvas_config
 *	config canvas from vframe
 **********************************************************/
void prev3_inp_canvas_config(struct di_buf_s *di_buf)
{
	struct dim_vmode_s *pvmode;
	struct vframe_s *vf;
	struct di_hpre_s  *phpre = get_hw_pre();

	pvmode	= &di_buf->c.vmode;
	vf	= &di_buf->c.pdvfm->vframe.vfm;

	if (pvmode->canvas0Addr == (u32)-1) {
		canvas_config_config(phpre->di_inp_idx[0],
				     &vf->canvas0_config[0]);
		canvas_config_config(phpre->di_inp_idx[1],
				     &vf->canvas0_config[1]);
		pvmode->canvas0Addr = (phpre->di_inp_idx[1] << 8) |
				      (phpre->di_inp_idx[0]);

		if (vf->plane_num == 2) {
			pvmode->canvas0Addr |= (phpre->di_inp_idx[1] << 16);
		} else if (vf->plane_num == 3) {
			canvas_config_config(phpre->di_inp_idx[2],
					     &vf->canvas0_config[2]);
			pvmode->canvas0Addr |= (phpre->di_inp_idx[2] << 16);
		}
		pvmode->canvas1Addr = vf->canvas0Addr;
	}
}

#ifdef HIS_V3
static void set_reg(unsigned int ch, int on)
{
	struct dev_vfram_t *pvfm;

	pvfm = getv3_dev_vframe(ch);

	if (on)
		pvfm->reg = true;
	else
		pvfm->reg = false;
}
#endif

static struct vframe_s *di_vf_peek(void *arg)
{
	unsigned int ch = *(int *)arg;
	struct di_ch_s		*pch;
	struct dim_dvfm_s	*pdvfm;

	/*dim_print("%s:ch[%d]\n",__func__,ch);*/
	if (div3_is_pause(ch))
		return NULL;

	pch = get_chdata(ch);

	if (isv3_bypss2_complete(ch)) {
		#ifdef HIS_V3
		return pw_vf_peek(ch);
		#else
		pdvfm = dvfmv3_peek(pch, QUED_T_IN);
		if (pdvfm && pdvfm->vfm_in &&
		    pdvfm->wmode.need_bypass) {
			return pdvfm->vfm_in;
		}
		//task_send_ready();/*hw timer*/
		pch->interf.op_dvfm_fill(pch);//dvfm_fill_in(pch);
		return NULL;

		#endif
	}

	//task_send_ready();/*hw timer*/
	return div3_vf_l_peek(pch);
}

static struct vframe_s *di_vf_get(void *arg)
{
	unsigned int ch = *(int *)arg;
	struct vframe_s *vfm = NULL;
	struct di_ch_s		*pch;
	struct dim_dvfm_s	*pdvfm;

	dimv3_tr_ops.post_get2(5);
	if (div3_is_pause(ch))
		return NULL;

	div3_pause_step_done(ch);

	/*pvfm = get_dev_vframe(ch);*/
	pch = get_chdata(ch);

	if (isv3_bypss2_complete(ch)) {
	#ifdef HIS_V3
		return pw_vf_get(ch);
	#endif
		pdvfm = dvfmv3_peek(pch, QUED_T_IN);
		if (pdvfm && pdvfm->vfm_in &&
		    pdvfm->wmode.need_bypass) {
			pdvfm = dvfmv3_get(pch, QUED_T_IN);
			if (pdvfm) {
				vfm = pdvfm->vfm_in;
				/*recycle:*/
				pdvfm->vfm_in = NULL;
				qued_ops.in(pch, QUED_T_RECYCL, pdvfm->index);
			}
		} else {
			vfm = NULL;
		}

		pch->interf.op_dvfm_fill(pch);//dvfm_fill_in(pch);
		//task_send_ready();/*hw timer*/
		return vfm;
	}

	return div3_vf_l_get(pch);
}

static void di_vf_put(struct vframe_s *vf, void *arg)
{
	unsigned int ch = *(int *)arg;
	struct di_ch_s *pch;

	pch = get_chdata(ch);

	if (isv3_bypss2_complete(ch)) {
		//pw_vf_put(vf, ch);
		//pw_vf_notify_provider(ch,
		//		      VFRAME_EVENT_RECEIVER_PUT, NULL);
		pch->interf.opsi.put(vf, pch);
		return;
	}

	div3_vf_l_put(vf, pch);
}

static int di_event_cb(int type, void *data, void *private_data)
{
	if (type == VFRAME_EVENT_RECEIVER_FORCE_UNREG) {
		PR_INF("%s: FORCE_UNREG return\n", __func__);
		return 0;
	}
	return 0;
}

static int di_vf_states(struct vframe_states *states, void *arg)
{
	unsigned int ch = *(int *)arg;

	if (!states)
		return -1;

	dimv3_print("%s:ch[%d]\n", __func__, ch);

	di_vf_l_states(states, ch);
	return 0;
}

static const struct vframe_operations_s deinterlace_vf_provider = {
	.peek		= di_vf_peek,
	.get		= di_vf_get,
	.put		= di_vf_put,
	.event_cb	= di_event_cb,
	.vf_states	= di_vf_states,
};

#if 1
#ifdef HIS_V3
struct vframe_s *pw_vf_get(unsigned int ch)
{
	sum_g_inc(ch);
	return vf_get(div3_rev_name[ch]);
}

struct vframe_s *pw_vf_peek(unsigned int ch)
{
	return vf_peek(div3_rev_name[ch]);
}

void pw_vf_put(struct vframe_s *vf, unsigned int ch)
{
	sum_p_inc(ch);
	vf_put(vf, div3_rev_name[ch]);
}

int pw_vf_notify_provider(unsigned int channel, int event_type, void *data)
{
	return vf_notify_provider(div3_rev_name[channel], event_type, data);
}

int pw_vfprov_note_put(struct dev_vfram_t *pvfm)
{
	return vf_notify_provider(pvfm->name, VFRAME_EVENT_RECEIVER_PUT, NULL);
}
#endif
int pwv3_vf_notify_receiver(unsigned int channel, int event_type, void *data)
{
	return vf_notify_receiver(div3_rev_name[channel], event_type, data);
}

void pwv3_vf_light_unreg_provider(unsigned int ch)
{
	struct dev_vfram_t *pvfm;
	struct vframe_provider_s *prov;

	pvfm = getv3_dev_vframe(ch);

	prov = &pvfm->di_vf_prov;
	vf_light_unreg_provider(prov);
}

/**************************/

#ifdef HIS_V3
struct vframe_s *pw_vf_get2(void *op_arg)
{
	struct di_ch_s *pch = (struct di_ch_s *)op_arg;

	sum_g_inc(pch->ch_id);
	return vf_get(pch->interf.name);
}

struct vframe_s *pw_vf_peek2(void *op_arg)
{
	struct di_ch_s *pch = (struct di_ch_s *)op_arg;

	return vf_peek(pch->interf.name);
}

void pw_vf_put2(struct vframe_s *vf, void *op_arg)
{
	struct di_ch_s *pch = (struct di_ch_s *)op_arg;

	sum_p_inc(pch->ch_id);
	vf_put(vf, pch->interf.name);
	vf_notify_provider(pch->interf.name,
			   VFRAME_EVENT_RECEIVER_PUT,
			   NULL);
}
#endif
#else
struct vframe_s *pw_vf_get(unsigned int channel)
{
	return vf_get(VFM_NAME);
}

struct vframe_s *pw_vf_peek(unsigned int channel)
{
	return vf_peek(VFM_NAME);
}

void pw_vf_put(struct vframe_s *vf, unsigned int channel)
{
	vf_put(vf, VFM_NAME);
}

int pw_vf_notify_provider(unsigned int channel, int event_type, void *data)
{
	return vf_notify_provider(VFM_NAME, event_type, data);
}

int pwv3_vf_notify_receiver(unsigned int channel, int event_type, void *data)
{
	return vf_notify_receiver(VFM_NAME, event_type, data);
}

#endif

/**********************************************************
 * ops for interface
 **********************************************************/
static void *vfm_peek(struct di_ch_s *pch)
{
	return vf_peek(pch->interf.name);
}

static void *vfm_get(struct di_ch_s *pch)
{
	sum_g_inc(pch->ch_id);
	return vf_get(pch->interf.name);
}

static void vfm_put(void *data, struct di_ch_s *pch)
{
	struct vframe_s *vf;

	vf = (struct vframe_s *)data;
	sum_p_inc(pch->ch_id);
	vf_put(vf, pch->interf.name);
	vf_notify_provider(pch->interf.name,
			   VFRAME_EVENT_RECEIVER_PUT,
			   NULL);
}

const struct dim_itf_ops_s vfm_in_ops = {
	.peek	= vfm_peek,
	.get	= vfm_get,
	.put	= vfm_put,
};

void devv3_vframe_exit(void)
{
	struct dev_vfram_t *pvfm;
	int ch;

	#ifdef TST_NEW_INS_INTERFACE
	vfmtst_exit();
	PR_INF("new ins interface test end\n");
	return;
	#endif

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pvfm = getv3_dev_vframe(ch);
		vf_unreg_provider(&pvfm->di_vf_prov);
		vf_unreg_receiver(&pvfm->di_vf_recv);
	}
	PR_INF("%s finish\n", __func__);
}

/*for prob*/
void devv3_vframe_init(void)
{
	struct dev_vfram_t *pvfm;
	int ch;
	struct di_mng_s *pbm = get_bufmng();
	struct dim_inter_s *pintf;

	#ifdef TST_NEW_INS_INTERFACE
	vfmtst_init();
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pintf = get_dev_intf(ch);
		pintf->ch = ch;
	}
	PR_INF("new ins interface test enable\n");
	return;
	#endif
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pintf = get_dev_intf(ch);
		pintf->ch = ch;
		if (pbm->tmode_pre[ch] != EDIM_TMODE_1_PW_VFM)
			continue;
		pvfm = getv3_dev_vframe(ch);
		pintf->name = div3_rev_name[ch];

		/*set_bypass_complete(pvfm, true);*/ /*test only*/

		/*receiver:*/
		vf_receiver_init(&pvfm->di_vf_recv, pintf->name,
				 &di_vf_receiver, &pintf->ch);
		vf_reg_receiver(&pvfm->di_vf_recv);

		/*provider:*/
		vf_provider_init(&pvfm->di_vf_prov, pintf->name,
				 &deinterlace_vf_provider, &pintf->ch);

		/*ops for local use*/
		//pvfm->pre_ops.peek	= pw_vf_peek2;
		//pvfm->pre_ops.put	= pw_vf_put2;
		//pvfm->pre_ops.get	= pw_vf_get2;

		memcpy(&pintf->opsi, &vfm_in_ops, sizeof(struct dim_itf_ops_s));
		pintf->op_post_done = dimv3_post_de_done_buf_config_vfm;
		pintf->op_dvfm_fill = dvfmv3_fill_in;
		pintf->op_ins_2_doing = NULL;
	}
	pr_info("%s finish\n", __func__);
}

