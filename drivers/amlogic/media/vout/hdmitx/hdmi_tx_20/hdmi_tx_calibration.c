/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdmi_tx_calibration.c
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
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ddc.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "hdmi_tx_calibration.h"

static int evaldata_verbose;

void cedst_free_buf(struct hdmitx_dev *hdev)
{
	kfree(hdev->cedst_buf);
	hdev->cedst_buf = NULL;
}

void cedst_malloc_buf(struct hdmitx_dev *hdev)
{
	struct cedst_buf *cbuf;

	if (hdev->cedst_buf) {
		pr_info("hdmitx: already malloc cedst_buf\n");
		return;
	}
	/* malloc cedst_buf */
	cbuf = kmalloc(sizeof(*cbuf) * 3, GFP_KERNEL);
	if (cbuf) {
		hdev->cedst_buf = cbuf;
		memset(cbuf, 0, sizeof(struct cedst_buf) * 3);
	} else {
		pr_info("hdmitx: malloc cedst_buf failed\n");
	}
}

void cedst_store_buf(struct hdmitx_dev *hdev)
{
	struct cedst_buf *buf = hdev->cedst_buf;

	if (!hdev->cedst_buf)
		return;

	buf += hdev->phy_idx;
	if (buf->buf_idx < CEDST_BUF_NO) {
		buf->buf[buf->buf_idx].cnt = hdev->ced_cnt;
		buf->buf[buf->buf_idx].st = hdev->chlocked_st;
		buf->buf_idx++;
	}
}

static void test_print_cedst(struct hdmitx_dev *hdev)
{
	int i;
	int idx;
	const struct cedst_buf *buf = hdev->cedst_buf;

	for (idx = 0; idx < 3; idx++) {
		pr_info("------------- phy%d ---------------\n", idx);
		for (i = 0; i < buf->buf_idx && i < CEDST_BUF_NO; i++)
			pr_info("[%d] ch0/1/2 %x %x %x  st %d %d %d %d\n", i,
				buf->buf[i].cnt.ch0_cnt,
				buf->buf[i].cnt.ch1_cnt,
				buf->buf[i].cnt.ch2_cnt,
				buf->buf[i].st.clock_detected,
				buf->buf[i].st.ch0_locked,
				buf->buf[i].st.ch1_locked,
				buf->buf[i].st.ch2_locked);
		buf++;
	}
}

/* W1 + W2 + W3 should be equal to 100 */
#define W1_UNLOCK	20
#define W2_ECNT_MAXVAL	50
#define W3_ECNT_AVG	30

#define CNERR_LARGE_NO	1024

static unsigned int calc_phy_eval(struct cedst_buf *buf)
{
	unsigned int eval;
	unsigned int div;

	if (buf->buf_idx == 0) /* can't divided by 0 */
		div = 1;
	else
		div = buf->buf_idx;

	eval = (buf->unlock1_no * W1_UNLOCK) / div +
	       (buf->maxval_no * W2_ECNT_MAXVAL) / div +
	       (buf->cnt_avg * W3_ECNT_AVG / CNERR_LARGE_NO);

	return eval;
}

static int _phy_evaluation(struct hdmitx_dev *hdev)
{
	int i, j;
	unsigned int tmp;
	struct cedst_buf *buf = hdev->cedst_buf;
	unsigned int eval[3] = {0};

	/* count unlock/maxval times, and the average */
	for (i = 0; i < 3; i++) {
		int sum;

		sum = 0;
		buf->unlock1_no = 0;
		buf->maxval_no = 0;
		buf->cnt_avg = 0;
		for (j = 0; j < CEDST_BUF_NO && j < buf->buf_idx; j++) {
			buf->unlock1_no += (buf->buf[j].st.ch1_locked == 0);
			buf->maxval_no +=
				(buf->buf[j].cnt.ch1_cnt > CNERR_LARGE_NO);
			tmp = buf->buf[j].cnt.ch1_cnt;
			sum += (tmp > CNERR_LARGE_NO) ? 0 : tmp;
		}
		if (j == 0)
			buf->cnt_avg = 0;
		else
			buf->cnt_avg = sum / j;

		buf++;
	}

	/* evalute the phy quality */
	buf = hdev->cedst_buf;
	for (i = 0; i < 3; i++) {
		pr_info("hdmitx: phy%d unlock %d maxval %d avg %d\n", i,
			buf->unlock1_no, buf->maxval_no, buf->cnt_avg);
		eval[i] = calc_phy_eval(buf);
		buf++;
	}
	pr_info("hdmitx: phy eval %d %d %d\n", eval[0], eval[1], eval[2]);

	/* find the smallest value in eval[] */
	tmp = eval[0];
	j = 0;
	for (i = 0; i < 3; i++) {
		if (tmp > eval[i]) {
			tmp = eval[i];
			j = i;
		}
	}
	return j;
}

int cedst_phy_evaluation(struct hdmitx_dev *hdev)
{
	if (!hdev->cedst_buf)
		return 0;
	if (evaldata_verbose)
		test_print_cedst(hdev);

	return _phy_evaluation(hdev);
}

MODULE_PARM_DESC(evaldata_verbose, "\n evaldata_verbose\n");
module_param(evaldata_verbose, int, 0444);
