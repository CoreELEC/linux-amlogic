/*
 * drivers/amlogic/media/di_multi_v3/di_post.c
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
#include "di_post.h"

#include "nr_downscale.h"
#include "register.h"

void dpostv3_clear(void)/*not been called*/
{
	struct di_hpst_s  *pst = get_hw_pst();

	memset(pst, 0, sizeof(struct di_hpst_s));
}

void dpostv3_init(void)
{/*reg:*/
	struct di_hpst_s  *pst = get_hw_pst();

	pst->state = eDI_PST_ST_IDLE;

	/*timer out*/
	div3_tout_int(&pst->tout, 40);	/*ms*/
}

void pwv3_use_hw_post(enum eDI_SUB_ID channel, bool on)
{
	struct di_hpst_s  *post = get_hw_pst();

	post->hw_flg_busy_post = on;
	if (on)
		post->curr_ch = channel;
}

static bool pwv3_try_sw_ch_next_post(enum eDI_SUB_ID channel)
{
	bool ret = false;

	struct di_hpst_s *post = get_hw_pst();
	enum eDI_SUB_ID lst_ch, nch;

	lst_ch = channel;

	nch = pwv3_ch_next_count(lst_ch);
	if (!get_reg_flag(nch) || get_flag_trig_unreg(nch))
		return false;

	if (nch != channel)
		dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_CH_CHG, nch, 0);/*dbg*/

	post->curr_ch = nch;
	post->hw_flg_busy_post = true;
	ret = true;

	/*dim_print("%s:%d->%d:%d\n", __func__, lst_ch, nch, ret);*/
	return ret;
}

/*****************************/
/* debug */
/*****************************/

/*****************************/
/* STEP */
/*****************************/

bool dpstv3_step_idle(void)
{
	struct di_hpst_s  *pst = get_hw_pst();
	bool reflesh = false;

	if (!pwv3_try_sw_ch_next_post(pst->curr_ch))
		return false;

	pst->pres = get_pre_stru(pst->curr_ch);
	pst->psts = get_post_stru(pst->curr_ch);
	pst->state++;/*tmp*/
	reflesh = true;

	return reflesh;
}

bool dpstv3_step_check(void)
{
	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int ch;
	struct di_post_stru_s *ppost;
	bool reflesh = false;

	ch = pst->curr_ch;
	ppost = get_post_stru(ch);

	//if (queue_empty(ch, QUEUE_POST_DOING)) {
	if (div3_que_is_empty(ch, QUE_POST_DOING)) {
		ppost->post_peek_underflow++;
		pst->state--;
		return reflesh;
	}

	pst->state++;
	reflesh = true;

	return reflesh;
}

bool dpstv3_step_set(void)
{
	struct di_buf_s *di_buf = NULL;
	//vframe_t *vf_p = NULL;
	struct di_post_stru_s *ppost;
	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int ch;
	bool reflesh = false;
	ulong flags = 0;
	struct dim_wmode_s	*pwmode;

	ch = pst->curr_ch;
	ppost = get_post_stru(ch);

	//di_buf = get_di_buf_head(ch, QUEUE_POST_DOING);
	di_buf = div3_que_peek(ch, QUE_POST_DOING);
	trace_buf(di_buf, DIM_QUE_PEEK, QUE_POST_DOING,
		(DIM_DBG_MARK | 0x00000010));
	if (dimv3_check_di_buf(di_buf, 20, ch)) {
		PR_ERR("%s:err1\n", __func__);
		return reflesh;
	}

	//vf_p = di_buf->vframe;
	#if 0	/*clear run_early_proc_fun_flag*/
	if (ppost->run_early_proc_fun_flag) {
		if (vf_p->early_process_fun)
			vf_p->early_process_fun = dimv3_do_post_wr_fun;
	}
	#endif
	pwmode = &di_buf->c.wmode;

	dimv3_print("%s:%p,%d, t[%d]\n", __func__, di_buf, di_buf->index,
		  di_buf->type);
	if (di_buf->c.process_fun_index) {	/*not bypass?*/

		ppost->post_wr_cnt++;
		spin_lock_irqsave(&plistv3_lock, flags);
		#if 0
		dimv3_post_process(di_buf, 0, vf_p->width - 1,
				 0, vf_p->height - 1, vf_p);
		#else
		dimv3_post_process(di_buf, 0,  pwmode->tgt_w - 1,
				 0, pwmode->tgt_h - 1, NULL);
		#endif
		spin_unlock_irqrestore(&plistv3_lock, flags);

		/*begin to count timer*/
		div3_tout_contr(eDI_TOUT_CONTR_EN, &pst->tout);

		ppost->post_de_busy = 1;
		ppost->irq_time = curv3_to_msecs();

		/*state*/
		pst->state++;
		/*reflesh = true;*/
	} else {
		ppost->de_post_process_done = 1;	/*trig done*/
		pst->flg_int_done = 1;

		/*state*/
		pst->state++;/*pst->state = eDI_PST_ST_DONE;*/
		reflesh = true;
	}
	ppost->cur_post_buf = di_buf;

	return reflesh;
}

bool dpstv3_step_wait_int(void)
{
	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int ch;
	struct di_post_stru_s *ppost;
	bool reflesh = false;
	ulong flags = 0;
	struct dim_inter_s	*pitf;
	struct di_ch_s		*pch;

	ch = pst->curr_ch;
	pch = get_chdata(ch);
	pitf = &pch->interf;
	dimv3_print("%s:ch[%d],done_flg[%d]\n", __func__,
		  pst->curr_ch, pst->flg_int_done);
	if (pst->flg_int_done) {
		/*finish to count timer*/
		div3_tout_contr(eDI_TOUT_CONTR_FINISH, &pst->tout);
		spin_lock_irqsave(&plistv3_lock, flags);
		//dim_post_de_done_buf_config(ch);
		pitf->op_post_done(pch);
		spin_unlock_irqrestore(&plistv3_lock, flags);
		pst->flg_int_done = false;
		/*state*/
		pst->state = eDI_PST_ST_IDLE;
		reflesh = true;
	} else {
		/*check if timeout:*/
		if (div3_tout_contr(eDI_TOUT_CONTR_CHECK, &pst->tout)) {
			ppost = get_post_stru(ch);
			PR_WARN("ch[%d]:post timeout[%d]\n", ch,
				ppost->cur_post_buf->c.seq);
			dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_TIMEOUT, ch, 0);
			/*state*/
			pst->state = eDI_PST_ST_TIMEOUT;
			reflesh = true;
		}
	}
	return reflesh;
}

void dpstv3_timeout(unsigned int ch)
{
	hpstv3_dbg_mem_pd_trig(0);
	postv3_close_new();
	#if 0
	div3_post_set_flow(1, eDI_POST_FLOW_STEP1_STOP);
	div3_post_reset();
	#endif
	dimhv3_pst_trig_resize();
}

bool dpstv3_step_timeout(void)
{
	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int ch;
	bool reflesh = false;
	ulong flags = 0;
	struct dim_inter_s	*pitf;
	struct di_ch_s		*pch;

	ch = pst->curr_ch;
	pch = get_chdata(ch);
	pitf = &pch->interf;
	dpstv3_timeout(ch);
	spin_lock_irqsave(&plistv3_lock, flags);
	//dim_post_de_done_buf_config(ch);
	pitf->op_post_done(pch);
	spin_unlock_irqrestore(&plistv3_lock, flags);
	pst->flg_int_done = false;

	/*state*/
	pst->state = eDI_PST_ST_IDLE;
	reflesh = true;

	return reflesh;
}

bool dpstv3_step_done(void)/*this step no use ?*/
{
	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int ch;
	bool reflesh = false;

	ch = pst->curr_ch;
/*	dim_post_de_done_buf_config(ch);*/

	/*state*/
	pst->state = eDI_PST_ST_IDLE;
	reflesh = true;

	return reflesh;
}

const struct di_func_tab_s div3_pst_func_tab[] = {
	{eDI_PST_ST_EXIT, NULL},
	{eDI_PST_ST_IDLE, dpstv3_step_idle},
	{eDI_PST_ST_CHECK, dpstv3_step_check},
	{eDI_PST_ST_SET, dpstv3_step_set},
	{eDI_PST_ST_WAIT_INT, dpstv3_step_wait_int},
	{eDI_PST_ST_TIMEOUT, dpstv3_step_timeout},
	{eDI_PST_ST_DONE, dpstv3_step_done}
};

const char * const dpstv3_state_name[] = {
	"EXIT",
	"IDLE",	/*swith to next channel?*/
	"CHECK",
	"SET",
	"WAIT_INT",
	"TIMEOUT",
	"DONE"
};

const char *dpstv3_state_name_get(enum eDI_PST_ST state)
{
	if (state > eDI_PST_ST_DONE)
		return "nothing";

	return dpstv3_state_name[state];
}

bool dpstv3_can_exit(unsigned int ch)
{
	struct di_hpst_s  *pst = get_hw_pst();
	bool ret = false;

	if (ch != pst->curr_ch) {
		ret = true;
	} else {
		if (pst->state <= eDI_PST_ST_IDLE)
			ret = true;
	}
	PR_INF("%s:ch[%d]:curr[%d]:stat[%s] ret[%d]\n",
	       __func__,
	       ch, pst->curr_ch,
	       dpstv3_state_name_get(pst->state),
	       ret);
	return ret;
}

static bool dpstv3_process_step2(void)
{
	struct di_hpst_s  *pst = get_hw_pst();
	enum eDI_PST_ST pst_st = pst->state;
	unsigned int ch;

	ch = pst->curr_ch;
	if (pst_st > eDI_PST_ST_EXIT)
		dimv3_recycle_post_back(ch);

	if ((pst_st <= eDI_PST_ST_DONE) &&
	    div3_pst_func_tab[pst_st].func)
		return div3_pst_func_tab[pst_st].func();
	else
		return false;
}

void dpstv3_dbg_f_trig(unsigned int cmd)
{
	struct di_task *tsk = get_task();

	struct di_hpst_s *pst = get_hw_pst();

	if (down_interruptible(&tsk->sem)) {
		PR_ERR("%s:can't get sem\n", __func__);
		return;
	}

	/*set on/off and trig*/
	if (cmd & 0x10) {
		pst->dbg_f_en = 1;
		pst->dbg_f_cnt = cmd & 0xf;
		pst->dbg_f_lstate = pst->state;
	} else {
		pst->dbg_f_en = 0;
	}

	up(&tsk->sem);
}

void dpstv3_process(void)
{
	bool reflesh;

	struct di_hpst_s *pst = get_hw_pst();

	if (pst->dbg_f_en) {
		if (pst->dbg_f_cnt) {
			dpstv3_process_step2();
			pst->dbg_f_cnt--;
		}
		if (pst->dbg_f_lstate != pst->state) {
			PR_INF("ch[%d]:state:%s->%s\n",
			       pst->curr_ch,
			       dpstv3_state_name_get(pst->dbg_f_lstate),
			       dpstv3_state_name_get(pst->state));

			pst->dbg_f_lstate = pst->state;
		}
		return;
	}

	reflesh = true;

	while (reflesh)
		reflesh = dpstv3_process_step2();
}
