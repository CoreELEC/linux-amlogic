/*
 * drivers/amlogic/media/di_multi_v3/di_task.c
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

#include <linux/kthread.h>	/*ary add*/
#include <linux/freezer.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>

#include "deinterlace.h"
#include "di_data_l.h"

#include "di_prc.h"

#include "di_task.h"
#include "di_vframe.h"

static void task_wakeup(struct di_task *tsk);

unsigned int div3_dbg_task_flg;	/*debug only*/

bool taskv3_send_cmd(unsigned int cmd)
{
	struct di_task *tsk = get_task();
	unsigned int val;

	dbg_reg("%s:cmd[%d]:\n", __func__, cmd);
	if (kfifo_is_full(&tsk->fifo_cmd)) {
		if (kfifo_out(&tsk->fifo_cmd, &val, sizeof(unsigned int))
		    != sizeof(unsigned int)) {
			PR_ERR("%s:can't out\n", __func__);
			return false;
		}

		PR_ERR("%s:lost cmd[%d]\n", __func__, val);
		tsk->err_cmd_cnt++;
		/*return false;*/
	}
	kfifo_in_spinlocked(&tsk->fifo_cmd, &cmd, sizeof(unsigned int),
			    &tsk->lock_cmd);

	task_wakeup(tsk);
	return true;
}

void taskv3_send_ready(void)
{
	struct di_task *tsk = get_task();

	task_wakeup(tsk);
}

#ifdef HIS_V3
bool task_have_vf(unsigned int ch)
{
	struct di_task *tsk = get_task();

	task_wakeup(tsk);
}
#endif
bool taskv3_get_cmd(unsigned int *cmd)
{
	struct di_task *tsk = get_task();
	unsigned int val;

	if (kfifo_is_empty(&tsk->fifo_cmd))
		return false;

	if (kfifo_out(&tsk->fifo_cmd, &val, sizeof(unsigned int))
		!= sizeof(unsigned int))
		return false;

	*cmd = val;
	return true;
}

void taskv3_polling_cmd(void)
{
	int i;
	union DI_L_CMD_BITS cmdbyte;

	for (i = 0; i < MAX_KFIFO_L_CMD_NUB; i++) {
		if (!taskv3_get_cmd(&cmdbyte.cmd32))
			break;
		if (cmdbyte.b.id == ECMD_RL_KEEP) {
			dimv3_post_keep_cmd_proc(cmdbyte.b.ch, cmdbyte.b.p2);
			continue;
		} else if (cmdbyte.b.id == ECMD_RL_KEEP_ALL) {
			dimv3_post_keep_release_all_2free(cmdbyte.b.ch);
			continue;
		}
		dipv3_chst_process_reg(cmdbyte.b.ch);
	}
}

static int task_is_exiting(struct di_task *tsk)
{
	if (tsk->exit)
		return 1;

/*	if (afepriv->dvbdev->writers == 1)
 *		if (time_after_eq(jiffies, fepriv->release_jiffies +
 *				  dvb_shutdown_timeout * HZ))
 *			return 1;
 */
	return 0;
}

static int task_should_wakeup(struct di_task *tsk)
{
	if (tsk->wakeup) {
		tsk->wakeup = 0;
		/*dbg only dbg_tsk("wkg[%d]\n", di_dbg_task_flg);*/
		return 1;
	}
	return task_is_exiting(tsk);
}

static void task_wakeup(struct di_task *tsk)
{
	tsk->wakeup = 1;
	wake_up_interruptible(&tsk->wait_queue);
	/*dbg_tsk("wks[%d]\n", di_dbg_task_flg);*/
}

static int di_test_thread(void *data)
{
	struct di_task *tsk = data;
	bool semheld = false;

	tsk->delay = HZ;
	tsk->status = 0;
	tsk->wakeup = 0;
	#ifdef HIS_V3
	tsk->reinitialise = 0;
	tsk->needfinish = 0;
	tsk->finishflg = 0;
	#endif
	set_freezable();
	while (1) {
		up(&tsk->sem);/* is locked when we enter the thread... */
restart:
		wait_event_interruptible_timeout(tsk->wait_queue,
						 task_should_wakeup(tsk) ||
						 kthread_should_stop()	 ||
						 freezing(current),
						 tsk->delay);
		div3_dbg_task_flg = 1;

		if (kthread_should_stop() || task_is_exiting(tsk)) {
			/* got signal or quitting */
			if (!down_interruptible(&tsk->sem))
				semheld = true;
			tsk->exit = 1;
			break;
		}

		if (try_to_freeze())
			goto restart;

		if (down_interruptible(&tsk->sem))
			break;
#ifdef HIS_V3
		if (tsk->reinitialise) {
			/*dvb_frontend_init(fe);*/

			tsk->reinitialise = 0;
		}
#endif
		div3_dbg_task_flg = 2;
		taskv3_polling_cmd();
		div3_dbg_task_flg = 3;
		dipv3_chst_process_ch();
		div3_dbg_task_flg = 4;
		if (get_reg_flag_all())
			dipv3_hw_process();

		div3_dbg_task_flg = 0;
	}

	tsk->thread = NULL;
	if (kthread_should_stop())
		tsk->exit = 1;
	else
		tsk->exit = 0;
	/*mb();*/

	if (semheld)
		up(&tsk->sem);

	task_wakeup(tsk);/*?*/
	return 0;
}

void taskv3_stop(void/*struct di_task *tsk*/)
{
	struct di_task *tsk = get_task();

#if 1	/*not use cmd*/
	PR_INF(".");
	/*--------------------*/
	/*cmd buf*/
	if (tsk->flg_cmd) {
		kfifo_free(&tsk->fifo_cmd);
		tsk->flg_cmd = 0;
	}
	/*tsk->lock_cmd = SPIN_LOCK_UNLOCKED;*/
	spin_lock_init(&tsk->lock_cmd);
	tsk->err_cmd_cnt = 0;
	/*--------------------*/
#endif
	tsk->exit = 1;
	/*mb();*/

	if (!tsk->thread)
		return;

	kthread_stop(tsk->thread);

	sema_init(&tsk->sem, 1);
	tsk->status = 0;

	/* paranoia check in case a signal arrived */
	if (tsk->thread)
		PR_ERR("warning: thread %p won't exit\n", tsk->thread);
}

int taskv3_start(void)
{
	int ret;
	int flg_err;
	struct di_task *tsk = get_task();

	struct task_struct *fe_thread;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	PR_INF(".");
	flg_err = 0;
#if 1	/*not use cmd*/
	/*--------------------*/
	/*cmd buf*/
	/*tsk->lock_cmd = SPIN_LOCK_UNLOCKED;*/
	spin_lock_init(&tsk->lock_cmd);
	tsk->err_cmd_cnt = 0;
	ret = kfifo_alloc(&tsk->fifo_cmd,
			  sizeof(unsigned int) * MAX_KFIFO_L_CMD_NUB,
			  GFP_KERNEL);
	if (ret < 0) {
		tsk->flg_cmd = false;
		PR_ERR("%s:can't get kfifo\n", __func__);
		return -1;
	}
	tsk->flg_cmd = true;

#endif
	/*--------------------*/
	sema_init(&tsk->sem, 1);
	init_waitqueue_head(&tsk->wait_queue);

	if (tsk->thread) {
		if (!tsk->exit)
			return 0;

		taskv3_stop();
	}

	if (signal_pending(current)) {
		if (tsk->flg_cmd) {
			kfifo_free(&tsk->fifo_cmd);
			tsk->flg_cmd = 0;
		}
		return -EINTR;
	}
	if (down_interruptible(&tsk->sem)) {
		if (tsk->flg_cmd) {
			kfifo_free(&tsk->fifo_cmd);
			tsk->flg_cmd = 0;
		}
		return -EINTR;
	}

	tsk->status = 0;
	tsk->exit = 0;
	tsk->thread = NULL;
	/*mb();*/

	fe_thread = kthread_run(di_test_thread, tsk, "aml-ditest-0");
	if (IS_ERR(fe_thread)) {
		ret = PTR_ERR(fe_thread);
		PR_ERR(" failed to start kthread (%d)\n", ret);
		up(&tsk->sem);
		tsk->flg_init = 0;
		return ret;
	}

	sched_setscheduler_nocheck(fe_thread, SCHED_FIFO, &param);
	tsk->flg_init = 1;
	tsk->thread = fe_thread;
	return 0;
}

void dbgv3_task(void)
{
	struct di_task *tsk = get_task();

	tsk->status = 1;
	task_wakeup(tsk);
}

/********************************************************************
 * hw timer
 ********************************************************************/
static enum hrtimer_restart dim_hrt_func(struct hrtimer *timer)
{
	struct di_timer_s *tm;
	int ch;

	tm = get_htimer();

	if (tm->con & DIM_HTM_REG) {
		for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
			if (!(tm->con & (DIM_HTM_REG_BIT << ch)))
				continue;
			hrtimer_forward_now(&tm->hrt, ms_to_ktime(15));
			taskv3_send_cmd(LCMD1(eCMD_REG, ch));
		}

		return HRTIMER_RESTART;
	}

	if (tm->con & DIM_HTM_CONDITION) {
		hrtimer_forward_now(&tm->hrt, ms_to_ktime(15));
		taskv3_send_ready();
		//PR_INF(",a,");
		return HRTIMER_RESTART;
	}
	PR_INF(",b,");
	return HRTIMER_NORESTART;
}

void dimv3_htr_prob(void)
{
	struct di_timer_s *tm;

	tm = get_htimer();
	if (tm->sts != EHTM_STS_NONE) {
		PR_ERR("%s:sts is not none %d\n", __func__, tm->sts);
		return;
	}
	hrtimer_init(&tm->hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	tm->hrt.function = dim_hrt_func;
	tm->sts = EHTM_STS_PROB;
}

void dimv3_htr_start(unsigned int ch)
{
	struct di_timer_s *tm;

	tm = get_htimer();

	tm->con |= ((DIM_HTM_WK_BIT | DIM_HTM_REG_BIT) << ch);

	if (tm->sts != EHTM_STS_PROB	&&
	    tm->sts != EHTM_STS_CANCEL) {
		PR_INF("%s:sts:%d,con[0x%x]\n", __func__, tm->sts, tm->con);
		return;
	}
	dbg_htm("%s:\n", __func__);

	hrtimer_start(&tm->hrt, ms_to_ktime(10), HRTIMER_MODE_REL);
	tm->sts = EHTM_STS_WRK;
}

void dimv3_htr_con_update(unsigned int mask, bool on)
{
	struct di_timer_s *tm;

	tm = get_htimer();

	if (on)
		tm->con |= mask;
	else
		tm->con &= (~mask);

	dbg_htm("%s:0x%x,%d:0x%x\n", __func__, mask, on, tm->con);
}

void dimv3_htr_stop(unsigned int ch)
{
	struct di_timer_s *tm;

	tm = get_htimer();
	tm->con &= (~(DIM_HTM_WK_BIT << ch));

	if (tm->con & DIM_HTM_WK) {
		PR_INF("%s:ch[%d],con[0x%x]\n", __func__, ch, tm->con);
		return;
	}

	if (tm->sts != EHTM_STS_WRK) {
		PR_ERR("%s:err sts:%d\n", __func__, tm->sts);
		return;
	}
	dbg_htm("%s:\n", __func__);
	tm->con = 0;
	hrtimer_cancel(&tm->hrt);
	tm->sts = EHTM_STS_CANCEL;
}

/*keep same order with EHTM_STS*/
const char * const dim_htr_sts_name[] = {
	"NONE",
	"PROB",
	"WRK",
	"CANCEL"
};

const char *dimv3_htr_get_stsname(void)
{
	struct di_timer_s *tm;

	tm = get_htimer();
	if (tm->sts < ARRAY_SIZE(dim_htr_sts_name))
		return dim_htr_sts_name[tm->sts];

	return "overflow";
}
