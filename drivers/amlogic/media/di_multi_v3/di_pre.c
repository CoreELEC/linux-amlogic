/*
 * drivers/amlogic/media/di_multi_v3/di_pre.c
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
#include "di_pres.h"

#include "nr_downscale.h"
#include "register.h"

unsigned int div3_get_other_ch(unsigned int curr)
{
	return curr ? 0 : 1;
}
#if 0
bool is_bypass_i_p(void)
{
	bool ret = false;
	struct di_hpre_s  *pre = get_hw_pre();
	struct di_in_inf_s *vc = &pre->vinf_curr;
	#if 0
	struct di_vinfo_s *vl = &pre->vinf_lst;

	if (vl->ch != vc->ch			&&
	    vf_type_is_interlace(vl->vtype)	&&
	    vfv3_type_is_prog(vc->vtype)) {
		ret = true;
	}
	#else
	unsigned int ch_c, ch_l;

	struct di_pre_stru_s *ppre_c, *ppre_l;

	if (!get_reg_flag(0)	||
	    !get_reg_flag(1))
		return ret;

	ch_c = vc->ch;
	ch_l = (ch_c ? 0 : 1);
	ppre_c = get_pre_stru(ch_c);
	ppre_l = get_pre_stru(ch_l);
	if (vf_type_is_interlace(ppre_l->cur_inp_type)	&&
	    vfv3_type_is_prog(ppre_c->cur_inp_type)) {
		ret = true;
		dimv3_print("ch[%d]:bypass p\n", ch_c);
	}
	#endif

	return ret;
}
#endif
void dprev3_clear(void)
{
	struct di_hpre_s  *pre = get_hw_pre();

	memset(pre, 0, sizeof(struct di_hpre_s));
}

void dprev3_init(void)
{/*reg:*/
	struct di_hpre_s  *pre = get_hw_pre();

	pre->pre_st = eDI_PRE_ST_IDLE;

	/*timer out*/
	div3_tout_int(&pre->tout, 40);	/*ms*/
}

void pwv3_use_hw_pre(enum eDI_SUB_ID channel, bool on)
{
	struct di_hpre_s  *pre = get_hw_pre();

	pre->hw_flg_busy_pre = on;
	if (on)
		pre->curr_ch = channel;
}

enum eDI_SUB_ID pwv3_ch_next_count(enum eDI_SUB_ID channel)
{
	int i;
	unsigned int lch, nch;

	nch = channel;
	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		lch = channel + i + 1;
		if (lch >= DI_CHANNEL_NUB)
			lch -= DI_CHANNEL_NUB;
		#if 0
		if (pbm->sub_act_flg[lch]) {
			nch = lch;
			break;
		}
		#else
		if (get_reg_flag(lch)		&&
		    !get_flag_trig_unreg(lch)	&&
		    !isv3_bypss2_complete(lch)) {
			nch = lch;
			break;
		}
		#endif
	}
	return nch;
}

/****************************************/
static bool pw_try_sw_ch_next_pre(enum eDI_SUB_ID channel)
{
	bool ret = false;
	struct di_hpre_s  *pre = get_hw_pre();
	enum eDI_SUB_ID lst_ch, nch;

	lst_ch = channel;

	nch = pwv3_ch_next_count(lst_ch);
	if (!get_reg_flag(nch)		||
	    get_flag_trig_unreg(nch)	||
	    isv3_bypss2_complete(nch))
		return false;

	pre->curr_ch = nch;
	pre->hw_flg_busy_pre = true;
	ret = true;

	/*dim_print("%s:%d->%d:%d\n", __func__, lst_ch, nch, ret);*/
	return ret;
}

/*****************************/
/* debug */
/*****************************/

unsigned int div3_dbg_pre_cnt;

void dbgv3_cnt_begin(void)
{
	div3_dbg_pre_cnt = 0x10;
}

void dbgv3_cnt_print(void)
{
	if (div3_dbg_pre_cnt < 0xf)
		return;

	if (div3_dbg_pre_cnt > 0x10) {
		div3_dbg_pre_cnt++;
		PR_INF("[%d]\n", div3_dbg_pre_cnt);
	}

	if (div3_dbg_pre_cnt > 0x15)
		div3_dbg_pre_cnt = 0;
}

/*****************************/
/* STEP */
/*****************************/
void dprev3_recyc(unsigned int ch)
{
	struct di_hpre_s  *pre = get_hw_pre();

	pre->check_recycle_buf_cnt = 0;
	while (dimv3_check_recycle_buf(ch) & 1) {
		if (pre->check_recycle_buf_cnt++ > MAX_IN_BUF_NUM) {
			di_pr_info("%s: dimv3_check_recycle_buf time out!!\n",
				   __func__);
			break;
		}
	}
}

void dprev3_vdoing(unsigned int ch)
{
	struct di_post_stru_s *ppost = get_post_stru(ch);

	ppost->di_post_process_cnt = 0;
	while (dimv3_pst_vframe_top(ch)) {
		if (ppost->di_post_process_cnt++ >
			MAX_POST_BUF_NUM) {
			di_pr_info("%s: dimv3_process_post_vframe time out!!\n",
				   __func__);
			break;
		}
	}
}

bool dprev3_can_exit(unsigned int ch)
{
	struct di_hpre_s  *pre = get_hw_pre();
	bool ret = false;

	if (ch != pre->curr_ch) {
		ret = true;
	} else {
		if (pre->pre_st <= eDI_PRE_ST4_IDLE)
			ret = true;
	}
	PR_INF("%s:ch[%d]:curr[%d]:stat[%s] ret[%d]\n",
	       __func__,
	       ch, pre->curr_ch,
	       dprev3_state4_name_get(pre->pre_st),
	       ret);
	return ret;
}

void dprev3_dbg_f_trig(unsigned int cmd)
{
	struct di_task *tsk = get_task();

	struct di_hpre_s  *pre = get_hw_pre();

	if (down_interruptible(&tsk->sem)) {
		PR_ERR("%s:can't get sem\n", __func__);
		return;
	}

	/*set on/off and trig*/
	if (cmd & 0x10) {
		pre->dbg_f_en = 1;
		pre->dbg_f_cnt = cmd & 0xf;
		pre->dbg_f_lstate = pre->pre_st;
	} else {
		pre->dbg_f_en = 0;
	}

	up(&tsk->sem);
}

void dprev3_process(void)
{
	bool reflesh;
	struct di_hpre_s  *pre = get_hw_pre();

	if (pre->dbg_f_en) {
		if (pre->dbg_f_cnt) {
			dprev3_process_step4();
			pre->dbg_f_cnt--;
		}
		if (pre->dbg_f_lstate != pre->pre_st) {
			PR_INF("ch[%d]:state:%s->%s\n",
			       pre->curr_ch,
			       dprev3_state4_name_get(pre->dbg_f_lstate),
			       dprev3_state4_name_get(pre->pre_st));

			pre->dbg_f_lstate = pre->pre_st;
		}
		return;
	}

	reflesh = true;

	while (reflesh) {
		reflesh = dprev3_process_step4();
		#if 0	/*debug only*/
		dbg_tsk("ch[%d]:st[%s]r[%d]\n", pre->curr_ch,
			dprev3_state4_name_get(pre->pre_st), reflesh);
		#endif
	}
}

enum eDI_PRE_MT {
	eDI_PRE_MT_CHECK = K_DO_TABLE_ID_START,
	eDI_PRE_MT_SET,
	eDI_PRE_MT_WAIT_INT,
	eDI_PRE_MT_TIME_OUT,
};

/*use do_table:*/
unsigned int dprev3_mtotal_check(void *data)
{
	struct di_hpre_s  *pre = get_hw_pre();
	unsigned int ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);//K_DO_R_NOT_FINISH;

	if ((prev3_run_flag == DI_RUN_FLAG_RUN)	||
	    (prev3_run_flag == DI_RUN_FLAG_STEP)) {
		if (prev3_run_flag == DI_RUN_FLAG_STEP)
			prev3_run_flag = DI_RUN_FLAG_STEP_DONE;
		/*dim_print("%s:\n", __func__);*/
		if (dim_pre_de_buf_config_top(pre->curr_ch))
			ret = K_DO_R_FINISH;
		else
			ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);

		dimv3_dbg_pre_cnt(pre->curr_ch, "x");
	}

	return ret;
}

unsigned int dprev3_mtotal_set(void *data)
{
	struct di_hpre_s  *pre = get_hw_pre();
	ulong flags = 0;

	/*dim_print("%s:\n", __func__);*/
	spin_lock_irqsave(&plistv3_lock, flags);
	dimv3_pre_de_process(pre->curr_ch);
	spin_unlock_irqrestore(&plistv3_lock, flags);
	/*begin to count timer*/
	div3_tout_contr(eDI_TOUT_CONTR_EN, &pre->tout);

	return K_DO_R_FINISH;
}

enum eDI_WAIT_INT {
	eDI_WAIT_INT_NEED_WAIT,
	eDI_WAIT_INT_HAVE_INT,
	eDI_WAIT_INT_TIME_OUT,
};

/*
 *return:	enum eDI_WAIT_INT
 *
 */
enum eDI_WAIT_INT div3_pre_wait_int(void *data)
{
	struct di_hpre_s  *pre = get_hw_pre();
	ulong flags = 0;
	/*struct di_pre_stru_s *ppre;*/
	struct di_ch_s		*pch;

	enum eDI_WAIT_INT ret = eDI_WAIT_INT_NEED_WAIT;

	pch = get_chdata(pre->curr_ch);
	if (pre->flg_int_done) {
		/*have INT done flg*/
		/*DI_INTR_CTRL[bit 0], NRWR_done, set by
		 * hardware when NRWR is done,clear by write 1
		 * by code;[bit 1]
		 * MTNWR_done, set by hardware when MTNWR
		 * is done, clear by write 1 by code;these two
		 * bits have nothing to do with
		 * DI_INTR_CTRL[16](NRW irq mask, 0 to enable
		 * irq) and DI_INTR_CTRL[17]
		 * (MTN irq mask, 0 to enable irq).two
		 * interrupts are raised if both
		 * DI_INTR_CTRL[16] and DI_INTR_CTRL[17] are 0
		 */
#if 0
		data32 = Rd(DI_INTR_CTRL);
		if (((data32 & 0x1) &&
		    ((ppre->enable_mtnwr == 0) || (data32 & 0x2))) ||
		    (ppre->pre_de_clear_flag == 2)) {
			dimv3_RDMA_WR(DI_INTR_CTRL, data32);
		}
#endif
		di_pre_wait_irq_set(false);
		/*finish to count timer*/
		div3_tout_contr(eDI_TOUT_CONTR_FINISH, &pre->tout);
		spin_lock_irqsave(&plistv3_lock, flags);

		dimv3_pre_de_done_buf_config(pch, false);

		pre->flg_int_done = 0;

		dprev3_recyc(pre->curr_ch);
		dprev3_vdoing(pre->curr_ch);

		spin_unlock_irqrestore(&plistv3_lock, flags);
#if 0
		ppre = get_pre_stru(pre->curr_ch);

		if (ppre->field_count_for_cont == 1) {
			usleep_range(2000, 2001);
			pr_info("delay 1ms\n");
		}
#endif

		ret = eDI_WAIT_INT_HAVE_INT;

	} else {
		/*check if timeout:*/
		if (div3_tout_contr(eDI_TOUT_CONTR_CHECK, &pre->tout)) {
			di_pre_wait_irq_set(false);
			/*return K_DO_R_FINISH;*/
			ret = eDI_WAIT_INT_TIME_OUT;
		}
	}
	/*debug:*/
	if (dbgv3_first_cnt_pre)
		dbgv3_first_frame("ch[%d],w_int[%d]\n", pre->curr_ch, ret);

	return ret;
}

unsigned int dprev3_mtotal_wait_int(void *data)
{
	enum eDI_WAIT_INT wret;
	unsigned int ret = K_DO_R_NOT_FINISH;

	wret = div3_pre_wait_int(NULL);
	switch (wret) {
	case eDI_WAIT_INT_NEED_WAIT:
		ret = K_DO_R_NOT_FINISH;
		break;
	case eDI_WAIT_INT_HAVE_INT:
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
		break;
	case eDI_WAIT_INT_TIME_OUT:
		ret = K_DO_R_FINISH;
		break;
	}
	return ret;
}

void dprev3_mtotal_timeout_contr(void)
{
	struct di_hpre_s  *pre = get_hw_pre();
	struct di_ch_s		*pch;

	pch = get_chdata(pre->curr_ch);
	if (!pch) {
		PR_ERR("%s:no pch\n", __func__);
		return;
	}
	/*move from di_pre_trigger_work*/
	if (dimp_get(eDI_MP_di_dbg_mask) & 4)
		dimv3_dump_mif_size_state(pre->pres, pre->psts);


	dimhv3_enable_di_pre_mif(false, dimp_get(eDI_MP_mcpre_en));
	if (div3_get_dts_nrds_en())
		dimv3_nr_ds_hw_ctrl(false);
	pre->pres->pre_de_irq_timeout_count++;

	pre->pres->pre_de_busy = 0;
	pre->pres->pre_de_clear_flag = 2;
	if ((dimp_get(eDI_MP_di_dbg_mask) & 0x2)) {
		PR_INF("ch[%d]*****wait %d timeout 0x%x(%d ms)*****\n",
		       pre->curr_ch,
		       pre->pres->field_count,
		       Rd(DI_INTR_CTRL),
		       (unsigned int)(curv3_to_msecs() -
		       pre->pres->irq_time[1]));
	}
	/*******************************/

	/*******************************/
	dimv3_pre_de_done_buf_config(pch, true);

	dprev3_recyc(pre->curr_ch);
	dprev3_vdoing(pre->curr_ch);
	/*******************************/
	/*dpre_recyc(pre->curr_ch);*/
}

unsigned int dprev3_mtotal_timeout(void *data)
{
	ulong flags = 0;

	spin_lock_irqsave(&plistv3_lock, flags);
	dprev3_mtotal_timeout_contr();
	spin_unlock_irqrestore(&plistv3_lock, flags);

	return K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
}

const struct do_table_ops_s prv3_mode_total[] = {
	/*fix*/
	[K_DO_TABLE_ID_PAUSE] = {
	.id = K_DO_TABLE_ID_PAUSE,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "pause",
	},
	[K_DO_TABLE_ID_STOP] = {
	.id = K_DO_TABLE_ID_STOP,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "stop",
	},
	/******************/
	[K_DO_TABLE_ID_START] = {	/*eDI_PRE_MT_CHECK*/
	.id = K_DO_TABLE_ID_START,
	.mark = 0,
	.con = NULL,
	.do_op = dprev3_mtotal_check,
	.do_stop_op = NULL,
	.name = "start-check",
	},
	[eDI_PRE_MT_SET] = {
	.id = eDI_PRE_MT_SET,
	.mark = 0,
	.con = NULL,
	.do_op = dprev3_mtotal_set,
	.do_stop_op = NULL,
	.name = "set",
	},
	[eDI_PRE_MT_WAIT_INT] = {
	.id = eDI_PRE_MT_WAIT_INT,
	.mark = 0,
	.con = NULL,
	.do_op = dprev3_mtotal_wait_int,
	.do_stop_op = NULL,
	.name = "wait_int",
	},
	[eDI_PRE_MT_TIME_OUT] = {
	.id = eDI_PRE_MT_TIME_OUT,
	.mark = 0,
	.con = NULL,
	.do_op = dprev3_mtotal_timeout,
	.do_stop_op = NULL,
	.name = "timeout",
	}
};

/****************************
 *
 * mode for bypass all
 *
 ****************************/
unsigned int dpre_mbypass_set(void *data)
{
	struct di_hpre_s  *pre = get_hw_pre();
	struct di_ch_s *pch;
	unsigned int ret = K_DO_R_NOT_FINISH;

	pch = get_chdata(pre->curr_ch);

	if ((prev3_run_flag == DI_RUN_FLAG_RUN)	||
	    (prev3_run_flag == DI_RUN_FLAG_STEP)) {
		if (prev3_run_flag == DI_RUN_FLAG_STEP)
			prev3_run_flag = DI_RUN_FLAG_STEP_DONE;
		/*dim_print("%s:\n", __func__);*/
		dim_pre_de_buf_config_bypass(pch->ch_id);
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);

		dimv3_dbg_pre_cnt(pre->curr_ch, "x");
	}

	return ret;
}

const struct do_table_ops_s pr_mode_bypass[] = {
	/*fix*/
	[K_DO_TABLE_ID_PAUSE] = {
	.id = K_DO_TABLE_ID_PAUSE,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "pause",
	},
	[K_DO_TABLE_ID_STOP] = {
	.id = K_DO_TABLE_ID_STOP,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "stop",
	},
	/******************/
	[K_DO_TABLE_ID_START] = {	/*eDI_PRE_MT_CHECK*/
	.id = K_DO_TABLE_ID_START,
	.mark = 0,
	.con = NULL,
	.do_op = dpre_mbypass_set,
	.do_stop_op = NULL,
	.name = "bypass_set",
	}
};

/********************************************************************
 *
 * mode for p as i mode
 *
 ********************************************************************/
enum eDI_PRE_MP {
	eDI_PRE_MP_CHECK = K_DO_TABLE_ID_START,
	eDI_PRE_MP_SET,
	eDI_PRE_MP_WAIT_INT,
	eDI_PRE_MP_TIME_OUT,
	eDI_PRE_MP_CHECK2,
	eDI_PRE_MP_SET2,
	eDI_PRE_MP_WAIT_INT2,
	eDI_PRE_MP_TIME_OUT2,
};

static unsigned int dpre_mp_check(void *data)
{
	struct di_hpre_s  *pre = get_hw_pre();
	unsigned int ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);//K_DO_R_NOT_FINISH;

	if ((prev3_run_flag == DI_RUN_FLAG_RUN)	||
	    (prev3_run_flag == DI_RUN_FLAG_STEP)) {
		if (prev3_run_flag == DI_RUN_FLAG_STEP)
			prev3_run_flag = DI_RUN_FLAG_STEP_DONE;
			/*dim_print("%s:\n", __func__);*/
		if (dim_pre_de_buf_config_p_asi_t(pre->curr_ch)) {
			/*pre->flg_wait_int = false;*/
			/*pre_p_asi_set_next(pre->curr_ch);*/
			ret = K_DO_R_FINISH;
		} else {
			/*pre->flg_wait_int = false;*/
			ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
		}
		dimv3_dbg_pre_cnt(pre->curr_ch, "x");
	}

	return ret;
}

static unsigned int dpre_mp_check2(void *data)
{
	struct di_hpre_s  *pre = get_hw_pre();
	unsigned int ret = K_DO_R_NOT_FINISH;

	if (dim_pre_de_buf_config_p_asi_b(pre->curr_ch)) {
		/*pre->flg_wait_int = false;*/
		ret = K_DO_R_FINISH;
	}
	#if 0
	else {
		PR_ERR("%s:not second?ch[%d]\n", __func__, pre->curr_ch);
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
	}
	#endif

	return ret;
}

static unsigned int dpre_mp_wait_int(void *data)
{
	enum eDI_WAIT_INT wret;
	unsigned int ret = K_DO_R_NOT_FINISH;

	wret = div3_pre_wait_int(NULL);
	switch (wret) {
	case eDI_WAIT_INT_NEED_WAIT:
		ret = K_DO_R_NOT_FINISH;
		break;
	case eDI_WAIT_INT_HAVE_INT:
		ret = K_DO_R_JUMP(eDI_PRE_MP_CHECK2);
		break;
	case eDI_WAIT_INT_TIME_OUT:
		ret = K_DO_R_FINISH;
		break;
	}
	return ret;
}

static unsigned int dpre_mp_wait_int2(void *data)
{
	enum eDI_WAIT_INT wret;
	unsigned int ret = K_DO_R_NOT_FINISH;

	wret = div3_pre_wait_int(NULL);
	switch (wret) {
	case eDI_WAIT_INT_NEED_WAIT:
		ret = K_DO_R_NOT_FINISH;
		break;
	case eDI_WAIT_INT_HAVE_INT:
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
		break;
	case eDI_WAIT_INT_TIME_OUT:
		ret = K_DO_R_FINISH;
		break;
	}
	return ret;
}

static unsigned int dpre_mp_timeout(void *data)
{
	dprev3_mtotal_timeout_contr();

	return K_DO_R_FINISH;
}

static unsigned int dpre_mp_timeout2(void *data)
{
	dprev3_mtotal_timeout_contr();

	return K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
}

const struct do_table_ops_s pre_mode_prog_as_i[] = {
	/*fix*/
	[K_DO_TABLE_ID_PAUSE] = {
	.id = K_DO_TABLE_ID_PAUSE,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "pause",
	},
	[K_DO_TABLE_ID_STOP] = {
	.id = K_DO_TABLE_ID_STOP,
	.mark = 0,			/*stop / pause*/
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "stop",
	},
	/******************/
	[K_DO_TABLE_ID_START] = {	/*eDI_PRE_MP_CHECK*/
	.id = K_DO_TABLE_ID_START,
	.mark = 0,			/*stop / pause*/
	.con = NULL,
	.do_op = dpre_mp_check,
	.do_stop_op = NULL,
	.name = "start-check",
	},
	[eDI_PRE_MP_SET] = {
	.id = eDI_PRE_MP_SET,
	.mark = 0,			/*stop / pause*/
	.con = NULL,			/*condition*/
	.do_op = dprev3_mtotal_set,
	.do_stop_op = NULL,
	.name = "pset",
	},
	[eDI_PRE_MP_WAIT_INT] = {
	.id = eDI_PRE_MP_WAIT_INT,
	.mark = 0,			/*stop / pause*/
	.con = NULL,			/*condition*/
	.do_op = dpre_mp_wait_int,
	.do_stop_op = NULL,
	.name = "pwait_int",
	},
	[eDI_PRE_MP_TIME_OUT] = {
	.id = eDI_PRE_MP_TIME_OUT,
	.mark = 0,			/*stop / pause*/
	.con = NULL,			/*condition*/
	.do_op = dpre_mp_timeout,
	.do_stop_op = NULL,
	.name = "ptimeout",
	},
	/******/
	[eDI_PRE_MP_CHECK2] = {		/*eDI_PRE_MP_CHECK2*/
	.id = eDI_PRE_MP_CHECK2,
	.mark = 0,			/*stop / pause*/
	.con = NULL,			/*condition*/
	.do_op = dpre_mp_check2,
	.do_stop_op = NULL,
	.name = "start-check",
	},
	[eDI_PRE_MP_SET2] = {
	.id = eDI_PRE_MP_SET2,
	.mark = 0,			/*stop / pause*/
	.con = NULL,			/*condition*/
	.do_op = dprev3_mtotal_set,
	.do_stop_op = NULL,
	.name = "psetp2",
	},
	[eDI_PRE_MP_WAIT_INT2] = {
	.id = eDI_PRE_MP_WAIT_INT2,
	.mark = 0,			/*stop / pause*/
	.con = NULL,			/*condition*/
	.do_op = dpre_mp_wait_int2,
	.do_stop_op = NULL,
	.name = "pwait_int2",
	},
	[eDI_PRE_MP_TIME_OUT2] = {
	.id = eDI_PRE_MP_TIME_OUT2,
	.mark = 0,			/*stop / pause*/
	.con = NULL,			/*condition*/
	.do_op = dpre_mp_timeout2,
	.do_stop_op = NULL,
	.name = "ptimeout2",
	}

};

void prev3_mode_setting(void)
{
	struct di_hpre_s  *pre = get_hw_pre();

	if (pre->pre_st != eDI_PRE_ST4_DO_TABLE)
		return;

	dov3_table_working(&pre->sdt_mode);
}

/*--------------------------*/
#if 0
enum eDI_WORK_MODE pre_cfg_count_mode(unsigned int ch, struct vframe_s *vframe)
{
	enum eDI_WORK_MODE pmode;

	if (isv3_bypass2(vframe, ch)) {
		pmode = eDI_WORK_MODE_bypass_all;
		return pmode;
	}

	if (COM_ME(vframe->type, VIDTYPE_INTERLACE)) {
		/*interlace:*/
		pmode = eDI_WORK_MODE_i;
		return pmode;
	}

	if (dimp_get(eDI_MP_prog_proc_config) & 0x10)
		pmode = eDI_WORK_MODE_p_as_p;
	else if (is_from_vdin(vframe))
		pmode = eDI_WORK_MODE_p_use_ibuf;
	else
		pmode = eDI_WORK_MODE_p_as_i;

	return pmode;
}
#endif
unsigned int dprev3_check_mode(unsigned int ch)
{
	//struct vframe_s *vframe;
//	unsigned int mode;
	enum eDI_WORK_MODE pmode;
	struct di_ch_s		*pch;
	struct dim_dvfm_s	*pdvfm;

	pch = get_chdata(ch);
	pdvfm = dvfmv3_peek(pch, QUED_T_IN);
	if (!pdvfm || !pdvfm->vfm_in)
		return eDI_WORK_MODE_NONE;

	//vframe = pdvfm->vfm_in;

	if (pdvfm->wmode.is_bypass) {
		pmode = eDI_WORK_MODE_bypass_all;
		return pmode;
	}

	if (pdvfm->wmode.is_i) {
		/*interlace:*/
		pmode = eDI_WORK_MODE_i;
		return pmode;
	}

	#if 0
	if (dimp_get(eDI_MP_prog_proc_config) & 0x10)
		pmode = eDI_WORK_MODE_p_as_p;
	else if (is_from_vdin(vframe) ||
		(dimp_get(eDI_MP_use_2_interlace_buff) & 0x02))
		pmode = eDI_WORK_MODE_p_use_ibuf;
	else
		pmode = eDI_WORK_MODE_p_as_i;
	#else
	if (pdvfm->wmode.p_as_i)
		pmode = eDI_WORK_MODE_p_as_i;
	else if (pdvfm->wmode.is_vdin)
		pmode = eDI_WORK_MODE_p_as_p;
	else
		pmode = eDI_WORK_MODE_p_use_ibuf;
	#endif
	return pmode;

}

/*--------------------------*/
bool dprev3_step4_idle(void)
{
	struct di_hpre_s  *pre = get_hw_pre();
	bool reflesh = false;
	unsigned int ch;

	ch = pre->curr_ch;
	if (!pw_try_sw_ch_next_pre(ch))
		return false;

	if (pre->idle_cnt >= DI_CHANNEL_NUB) {
		pre->idle_cnt = 0;
		return false;
	}
	pre->pres = get_pre_stru(pre->curr_ch);
	pre->psts = get_post_stru(pre->curr_ch);

	/*state*/
	pre->pre_st++;/*tmp*/
	reflesh = true;

	return reflesh;
}

bool dprev3_step4_check(void)
{
	struct di_hpre_s  *pre = get_hw_pre();
	bool reflesh = false;
	unsigned int mode;

	/*get vframe and select mode
	 * now: fix use total table
	 */

	mode = dprev3_check_mode(pre->curr_ch);

	if (mode == eDI_WORK_MODE_NONE) {
		pre->pre_st--;
		pre->idle_cnt++;
		return true;
	}
	pre->idle_cnt = 0;
	if (mode == eDI_WORK_MODE_p_as_i) {
		dov3_table_init(&pre->sdt_mode,
			      &pre_mode_prog_as_i[0],
			      ARRAY_SIZE(pre_mode_prog_as_i));

	} else if (mode == eDI_WORK_MODE_bypass_all) {
		dov3_table_init(&pre->sdt_mode,
			      &pr_mode_bypass[0],
			      ARRAY_SIZE(pr_mode_bypass));

	} else {
		dov3_table_init(&pre->sdt_mode,
			      &prv3_mode_total[0],
			      ARRAY_SIZE(prv3_mode_total));
	}
	dov3_talbe_cmd(&pre->sdt_mode, eDO_TABLE_CMD_START);

	/*state*/
	pre->pre_st++;
	reflesh = true;

	return reflesh;
}

bool dprev3_step4_do_table(void)
{
	struct di_hpre_s  *pre = get_hw_pre();
	bool reflesh = false;

	if (dov3_table_is_crr(&pre->sdt_mode, K_DO_TABLE_ID_STOP)) {
		pre->pre_st = eDI_PRE_ST4_IDLE;
		reflesh = true;
	}
	return reflesh;
}

const struct di_func_tab_s div3_pre_func_tab4[] = {
	{eDI_PRE_ST4_EXIT, NULL},
	{eDI_PRE_ST4_IDLE, dprev3_step4_idle},
	{eDI_PRE_ST4_CHECK, dprev3_step4_check},
	{eDI_PRE_ST4_DO_TABLE, dprev3_step4_do_table},
};

const char * const dprev3_state_name4[] = {
	"EXIT",
	"IDLE",	/*swith to next channel?*/
	"CHECK",
	"DO_TABLE"
};

const char *dprev3_state4_name_get(enum eDI_PRE_ST4 state)
{
	if (state > eDI_PRE_ST4_DO_TABLE)
		return "nothing";

	return dprev3_state_name4[state];
}

bool dprev3_process_step4(void)
{
	struct di_hpre_s  *pre = get_hw_pre();
	enum eDI_PRE_ST4 pre_st = pre->pre_st;
	ulong flags = 0;

	if (pre_st > eDI_PRE_ST4_EXIT) {
		spin_lock_irqsave(&plistv3_lock, flags);
		dimv3_recycle_post_back(pre->curr_ch);
		dprev3_recyc(pre->curr_ch);
		dprev3_vdoing(pre->curr_ch);
		spin_unlock_irqrestore(&plistv3_lock, flags);
	}
	if ((pre_st <= eDI_PRE_ST4_DO_TABLE)	&&
	    div3_pre_func_tab4[pre_st].func) {
		return div3_pre_func_tab4[pre_st].func();
	}

	return false;
}
