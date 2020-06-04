/*
 * drivers/amlogic/media/di_multi_v3/di_que.c
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
#include <linux/kfifo.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/debug_ftrace_ramoops.h>
#include "deinterlace.h"

#include "di_data_l.h"
#include "di_que.h"
#include "di_vframe.h"

#include "di_prc.h"

#ifdef DIM_DEBUG_QUE_ERR
static void dim_dbg_que(union dim_dbg_que_u *dbg)
{
	struct di_dbg_que_s *pd = di_get_dbg_que(0);
	unsigned int pos;
	union dim_dbg_que_u ldbg;

	if (!pd->en)
		return;
	pd->cnt++;

	if (pd->cnt > 254)
		pd->cnt = 0;
	pos = pd->pos;
	ldbg = *dbg;
	pd->que_inf[pos].u32 = ldbg.u32;
	pd->que_inf[pos].b.cnt = pd->cnt;
	pd->pos++;

	if (pd->pos >= DIM_DBG_QUE_SIZE)
		pd->pos = 0;

}

void dim_dbg_que_int(void)
{
	struct di_dbg_que_s *pd = di_get_dbg_que(0);

	memset(pd, 0, sizeof(*pd));
	pd->en = 1;
}

void dim_dbg_que_disable(void)
{
	struct di_dbg_que_s *pd = di_get_dbg_que(0);

	pd->en = 0;
}

void dim_dbg_que_out(void)
{
	struct di_dbg_que_s *pd = di_get_dbg_que(0);
	int i;

	for (i = 0; i < DIM_DBG_QUE_SIZE; i++)
		pr_info("%d:0x%x\n", i, pd->que_inf[i].u32);

	memset(pd, 0, sizeof(*pd));
	pd->en = 1;
}

void trace_buf(struct di_buf_s *di_buf,
	       unsigned int q_api, unsigned int q_index,
	       unsigned int tag)
{
	union dim_dbg_que_u dbg;

	if (q_index != QUE_POST_DOING &&
		q_index != QUE_POST_NOBUF)
		return;

	dbg.b.buf_index = di_buf->index;
	dbg.b.buf_type = di_buf->type;
	dbg.b.omx_index = (di_buf->c.vmode.omx_index)&0xff;
	dbg.b.que_api	= q_api;
	dbg.b.que_index	= q_index;

	pstore_ftrace_io_tag(tag, dbg.u32);
}
#endif /*DIM_DEBUG_QUE_ERR*/

/*********************************************
 *also see enum QUE_TYPE
 ********************************************/
const char * const div3_name_new_que[QUE_NUB] = {
	"QUE_IN_FREE",	/*0*/
	"QUE_PRE_READY",	/*1*/
	"QUE_POST_FREE",	/*2*/
	"QUE_POST_READY",	/*3*/
	"QUE_POST_BACK",	/*4*/
	"QUE_POST_DOING",
	"QUE_POST_NOBUF",
	"QUE_POST_KEEP",
	"QUE_POST_KEEP_BACK",
	"QUE_DBG",
/*	"QUE_NUB",*/

};

#define que_dbg		dimv3_print

void pwv3_queue_clear(unsigned int ch, enum QUE_TYPE qtype)
{
	struct di_ch_s *pch = get_chdata(ch);

#if 0
	if (qtype >= QUE_NUB)
		return;
#endif
	dbg_dbg("%s:que reset t[%d]\n", __func__, qtype);
	kfifo_reset(&pch->fifo[qtype]);
}

bool pwv3_queue_in(unsigned int ch, enum QUE_TYPE qtype, unsigned int buf_index)
{
	struct di_ch_s *pch = get_chdata(ch);

#if 0
	if (qtype >= QUE_NUB)
		return false;
#endif
	if (kfifo_in(&pch->fifo[qtype], &buf_index, sizeof(unsigned int))
		!= sizeof(unsigned int))
		return false;
#if 0

	/*below for debug: save in que*/
	if (qtype <= QUE_POST_RECYC) {
		if (buf_index >= MAX_POST_BUF_NUM) {
			pr_err("%s:err:overflow?[%d]\n", __func__, buf_index);
		} else {
			ppw = &pch->lpost_buf[buf_index];
			ppw->in_qtype = qtype;
		}
	}
#endif
	return true;
}

bool pwv3_queue_out(unsigned int ch, enum QUE_TYPE qtype,
		  unsigned int *buf_index)
{
	struct di_ch_s *pch = get_chdata(ch);
	unsigned int index;

#if 0
	if (qtype >= QUE_NUB)
		return false;
#endif
	if (kfifo_out(&pch->fifo[qtype], &index, sizeof(unsigned int))
		!= sizeof(unsigned int)) {
		PR_ERR("%s:ch[%d],qtye[%d],buf[%d]\n",
		       __func__, ch, qtype, *buf_index);
		return false;
	}

	*buf_index = index;

	return true;
}

static bool pw_queue_peek(unsigned int ch, enum QUE_TYPE qtype,
			  unsigned int *buf_index)
{
	struct di_ch_s *pch = get_chdata(ch);
	unsigned int index;

#if 0
	if (qtype >= QUE_NUB)
		return false;
#endif

	if (kfifo_out_peek(&pch->fifo[qtype], &index, sizeof(unsigned int))
		!= sizeof(unsigned int))
		return false;

	*buf_index = index;

	return true;
}

bool pwv3_queue_move(unsigned int ch, enum QUE_TYPE qtypef,
		     enum QUE_TYPE qtypet, unsigned int *oindex)
{
	struct di_ch_s *pch = get_chdata(ch);
	unsigned int index;

	/*struct di_post_buf_s *ppw;*/ /*debug only*/

#if 0
	if (qtypef >= QUE_NUB || qtypet >= QUE_NUB)
		return false;
#endif
	if (kfifo_out(&pch->fifo[qtypef], &index, sizeof(unsigned int))
		!= sizeof(unsigned int)) {
		PR_ERR("qtypef[%d] is empty\n", qtypef);
		return false;
	}
	if (kfifo_in(&pch->fifo[qtypet], &index, sizeof(unsigned int))
			!= sizeof(unsigned int)) {
		PR_ERR("qtypet[%d] is full\n", qtypet);
		return false;
	}

	*oindex = index;
#if 0
	if (qtypet <= QUE_POST_RECYC) {
		/*below for debug: save in que*/
		if (index >= MAX_POST_BUF_NUM) {
			pr_err("%s:err:overflow?[%d]\n", __func__, index);
		} else {
		ppw = &pch->lpost_buf[index];
		ppw->in_qtype = qtypet;
		}
	}
#endif
	return true;
}

bool pwv3_queue_empty(unsigned int ch, enum QUE_TYPE qtype)
{
	struct di_ch_s *pch = get_chdata(ch);

	if (kfifo_is_empty(&pch->fifo[qtype]))
		return true;

	return false;
}

/**********************************************************
 *
 **********************************************************/
int div3_que_list_count(unsigned int ch, enum QUE_TYPE qtype)
{
	struct di_ch_s *pch = get_chdata(ch);
	unsigned int length;

#if 0
	if (qtype >= QUE_NUB)
		return -1;
#endif
	length = kfifo_len(&pch->fifo[qtype]);
	length = length / sizeof(unsigned int);

	return length;
}

/***************************************/
/*outbuf : array size MAX_FIFO_SIZE*/
/***************************************/
bool div3_que_list(unsigned int ch, enum QUE_TYPE qtype, unsigned int *outbuf,
		 unsigned int *rsize)
{
	struct di_ch_s *pch = get_chdata(ch);
/*	unsigned int tmp[MAX_FIFO_SIZE + 1];*/
	int i;
	unsigned int index;
	bool ret = false;

	/*que_dbg("%s:begin\n", __func__);*/
	for (i = 0; i < MAX_FIFO_SIZE; i++)
		outbuf[i] = 0xff;

	if (kfifo_is_empty(&pch->fifo[qtype])) {
		que_dbg("\t%d:empty\n", qtype);
		*rsize = 0;
		return true;
	}

	ret = true;
	memcpy(&pch->fifo[QUE_DBG], &pch->fifo[qtype],
	       sizeof(pch->fifo[qtype]));

#if 0
	if (kfifo_is_empty(&pbm->fifo[QUE_DBG]))
		pr_err("%s:err, kfifo can not copy?\n", __func__);

#endif
	i = 0;
	*rsize = 0;

	while (kfifo_out(&pch->fifo[QUE_DBG], &index, sizeof(unsigned int))
		== sizeof(unsigned int)) {
		outbuf[i] = index;
		/*pr_info("%d->%d\n",i,index);*/
		i++;
	}
	*rsize = div3_que_list_count(ch, qtype);
	#if 0	/*debug only*/
	que_dbg("%s: size[%d]\n", div3_name_new_que[qtype], *rsize);
	for (i = 0; i < *rsize; i++)
		que_dbg("%d,", outbuf[i]);

	que_dbg("\n");
	#endif
	/*que_dbg("finish\n");*/

	return ret;
}

int div3_que_is_empty(unsigned int ch, enum QUE_TYPE qtype)
{
	struct di_ch_s *pch = get_chdata(ch);

#if 0
	if (qtype >= QUE_NUB)
		return -1;
#endif
	return kfifo_is_empty(&pch->fifo[qtype]);
}

void div3_que_init(unsigned int ch)
{
	int i;

	for (i = 0; i < QUE_NUB; i++) {
		if (i == QUE_POST_KEEP ||
		    i == QUE_POST_KEEP_BACK)
			continue;
		pwv3_queue_clear(ch, i);
	}
}

bool div3_que_alloc(unsigned int ch)
{
	int i;
	int ret;
	bool flg_err;
	struct di_ch_s *pch = get_chdata(ch);

	/*kfifo----------------------------*/
	flg_err = 0;
	for (i = 0; i < QUE_NUB; i++) {
		ret = kfifo_alloc(&pch->fifo[i],
				  sizeof(unsigned int) * MAX_FIFO_SIZE,
				  GFP_KERNEL);
		if (ret < 0) {
			flg_err = 1;
			PR_ERR("%s:%d:can't get kfifo\n", __func__, i);
			break;
		}
		pch->flg_fifo[i] = 1;
	}
#if 0
	/*canvas-----------------------------*/
	canvas_alloc();
#endif
/*	pdp_clear();*/

	if (!flg_err) {
		/*pbm->flg_fifo = 1;*/
		PR_INF("%s:ok\n", __func__);
		ret = true;
	} else {
		div3_que_release(ch);
		ret = false;
	}

	return ret;
}

void div3_que_release(unsigned int ch)
{
	struct di_ch_s *pch = get_chdata(ch);
	int i;

/*	canvas_release();*/
	for (i = 0; i < QUE_NUB; i++) {
		if (pch->flg_fifo[i]) {
			kfifo_free(&pch->fifo[i]);
			pch->flg_fifo[i] = 0;
		}
	}

	PR_INF("%s:ok\n", __func__);
}

/********************************************
 *get di_buf from index that same in que
 *	(di_buf->type << 8) | (di_buf->index)
 ********************************************/
struct di_buf_s *pwv3_qindex_2_buf(unsigned int ch, unsigned int qindex)
{
	union uDI_QBUF_INDEX index;
	struct di_buf_s *di_buf;
	struct di_buf_pool_s *pbuf_pool = get_buf_pool(ch);

	index.d32 = qindex;
	di_buf = &pbuf_pool[index.b.type - 1].di_buf_ptr[index.b.index];

	return di_buf;
}

/********************************************/
/*get di_buf from index that same in que*/
/*(di_buf->type << 8) | (di_buf->index)*/
/********************************************/
static unsigned int pw_buf_2_qindex(unsigned int ch, struct di_buf_s *pdi_buf)
{
	union	uDI_QBUF_INDEX index;

	index.b.index = pdi_buf->index;
	index.b.type = pdi_buf->type;
	return index.d32;
}

/*di_buf is out*/
struct di_buf_s *div3_que_out_to_di_buf(unsigned int ch, enum QUE_TYPE qtype)
{
	unsigned int q_index;
	struct di_buf_s *pdi_buf = NULL;

	if (!pw_queue_peek(ch, qtype, &q_index)) {
		PR_ERR("%s:no buf\n", __func__);
		return pdi_buf;
	}

	pdi_buf = pwv3_qindex_2_buf(ch, q_index);
	if (!pdi_buf) {
		PR_ERR("%s:buf is null[%d]\n", __func__, q_index);
		return NULL;
	}

	pwv3_queue_out(ch, qtype, &q_index);
	pdi_buf->queue_index = -1;

	return pdi_buf;
}

#if 0
/*di_buf is input*/
bool div3_que_out(unsigned int ch, enum QUE_TYPE qtype, struct di_buf_s *di_buf)
{
	unsigned int q_index;
	unsigned int q_index2;
#ifdef DIM_DEBUG_QUE_ERR
	union dim_dbg_que_u dbg;
#endif
	if (!pw_queue_peek(ch, qtype, &q_index)) {
		PR_ERR("%s:no buf ch[%d], qtype[%d], buf[%d,%d]\n", __func__,
		       ch, qtype, di_buf->type, di_buf->index);
#ifdef DIM_DEBUG_QUE_ERR
		dump_stack();
		dim_dbg_que_disable();
		dim_dbg_que_out();
#endif
		return false;
	}

	q_index2 = pw_buf_2_qindex(ch, di_buf);
	if (q_index2 != q_index) {
		PR_ERR("di:%s:%d not map[0x%x,0x%x]\n", __func__,
		       qtype, q_index2, q_index);
#ifdef DIM_DEBUG_QUE_ERR
		dump_stack();
		dim_dbg_que_disable();
		dim_dbg_que_out();
#endif
		return false;
	}
#ifdef DIM_DEBUG_QUE_ERR
	/*dbg*/
	dbg.b.buf_index = di_buf->index;
	dbg.b.buf_type = di_buf->type;
	dbg.b.omx_index = (di_buf->c.vmode.omx_index)&0xff;
	dbg.b.que_api	= DIM_QUE_OUT;
	dbg.b.que_index	= qtype;
	dim_dbg_que(&dbg);
#endif
	pwv3_queue_out(ch, qtype, &q_index);
	di_buf->queue_index = -1;
	return true;
}
#else
/*di_buf is input*/
bool div3_que_out(unsigned int ch, enum QUE_TYPE qtype, struct di_buf_s *di_buf)
{
	unsigned int q_index;
	unsigned int q_index2;
	struct di_buf_s *dbuf;

#ifdef DIM_DEBUG_QUE_ERR
	union dim_dbg_que_u dbg;
#endif
	if (!pw_queue_peek(ch, qtype, &q_index)) {
		PR_ERR("%s:no buf ch[%d], qtype[%d], buf[%d,%d]\n", __func__,
		       ch, qtype, di_buf->type, di_buf->index);
#ifdef DIM_DEBUG_QUE_ERR
		dump_stack();
		dim_dbg_que_disable();
		dim_dbg_que_out();
#endif
		return false;
	}

	pwv3_queue_out(ch, qtype, &q_index2);
	dbuf = pwv3_qindex_2_buf(ch, q_index2);
	dbg_qued("%s:%s:t[%d],id[%d]\n", __func__,
		 div3_name_new_que[qtype], dbuf->type, dbuf->index);

	/*check*/
	if (dbuf->index != di_buf->index)
		PR_ERR("%s:not map\n", __func__);

#ifdef DIM_DEBUG_QUE_ERR
	/*dbg*/
	dbg.b.buf_index = di_buf->index;
	dbg.b.buf_type = di_buf->type;
	dbg.b.omx_index = (di_buf->c.vmode.omx_index) & 0xff;
	dbg.b.que_api	= DIM_QUE_OUT;
	dbg.b.que_index = qtype;
	dim_dbg_que(&dbg);
	trace_buf(di_buf, DIM_QUE_OUT, qtype,
		  (DIM_DBG_MARK | 0x00008000));
#endif

	dbuf->queue_index = -1;
	return true;
}

#endif
bool div3_que_in(unsigned int ch, enum QUE_TYPE qtype, struct di_buf_s *di_buf)
{
	unsigned int q_index;
#ifdef DIM_DEBUG_QUE_ERR
	union dim_dbg_que_u dbg;
#endif

	if (!di_buf) {
		PR_ERR("%s:di_buf is NULL,ch[%d],qtype[%d]\n",
		       __func__, ch, qtype);
		return false;
	}
	if (di_buf->queue_index != -1) {
		PR_ERR("%s:buf in some que,ch[%d],qt[%d],qi[%d],bi[%d]\n",
		       __func__,
		       ch, qtype, di_buf->queue_index, di_buf->index);
#ifdef DIM_DEBUG_QUE_ERR
		dump_stack();
		dim_dbg_que_disable();
		dim_dbg_que_out();
#endif
		#ifdef HIS_V3
		return false;
		#else
		di_buf->queue_index = -1;
		#endif
	}
#ifdef DIM_DEBUG_QUE_ERR
	/*dbg*/
	dbg.b.buf_index = di_buf->index;
	dbg.b.buf_type = di_buf->type;
	dbg.b.omx_index = (di_buf->c.vmode.omx_index) & 0xff;
	dbg.b.que_api	= DIM_QUE_IN;
	dbg.b.que_index	= qtype;
	dim_dbg_que(&dbg);
	trace_buf(di_buf, DIM_QUE_IN, qtype,
		  (DIM_DBG_MARK | 0x00008000));
#endif
	q_index = pw_buf_2_qindex(ch, di_buf);

	if (!pwv3_queue_in(ch, qtype, q_index)) {
		PR_ERR("%s:can't que in,ch[%d],qtype[%d],q_index[%d]\n",
		       __func__,
		       ch, qtype, q_index);
		return false;
	}
	di_buf->queue_index = qtype + QUEUE_NUM;
	dbg_qued("%s:%s:t[%d],id[%d]\n", __func__,
		 div3_name_new_que[qtype], di_buf->type, di_buf->index);

	if (qtype == QUE_PRE_READY)
		dimv3_print("di:pre_ready in %d\n", di_buf->index);

	return true;
}

bool div3_que_is_in_que(unsigned int ch, enum QUE_TYPE qtype,
			struct di_buf_s *di_buf)
{
	unsigned int q_index;
	unsigned int arr[MAX_FIFO_SIZE + 1];
	unsigned int asize = 0;
	bool ret = false;
	unsigned int i;

	if (!di_buf)
		return false;

	q_index = pw_buf_2_qindex(ch, di_buf);

	div3_que_list(ch, qtype, &arr[0], &asize);

	if (asize == 0)
		return ret;

	for (i = 0; i < asize; i++) {
		if (arr[i] == q_index) {
			ret = true;
			break;
		}
	}
	return ret;
}

/* clear and rebuild que*/
bool div3_que_out_not_fifo(unsigned int ch, enum QUE_TYPE qtype,
			   struct di_buf_s *di_buf)
{
	unsigned int q_index;
	unsigned int arr[MAX_FIFO_SIZE + 1];
	unsigned int asize = 0;
	unsigned int i;
	bool ret = false;
#ifdef DIM_DEBUG_QUE_ERR
	union dim_dbg_que_u dbg;
#endif

	if (!pw_queue_peek(ch, qtype, &q_index))
		return false;

	q_index = pw_buf_2_qindex(ch, di_buf);

	div3_que_list(ch, qtype, &arr[0], &asize);

	pwv3_queue_clear(ch, qtype);

	if (asize == 0) {
		PR_ERR("%s:size 0\n", __func__);
		return ret;
	}

	for (i = 0; i < asize; i++) {
		if (arr[i] == q_index) {
			ret = true;
			di_buf->queue_index = -1;
			continue;
		}
		pwv3_queue_in(ch, qtype, arr[i]);
	}
	if (!ret) {
		PR_ERR("%s:not find:%d\n", __func__, di_buf->index);
	} else {
#ifdef DIM_DEBUG_QUE_ERR
		/*dbg*/
		dbg.b.buf_index = di_buf->index;
		dbg.b.buf_type = di_buf->type;
		dbg.b.omx_index = (di_buf->c.vmode.omx_index) & 0xff;
		dbg.b.que_api	= DIM_QUE_OUT_NOT_FIFO;
		dbg.b.que_index	= qtype;
		dim_dbg_que(&dbg);
#endif
	}

	return ret;
}

/*same as get_di_buf_head*/
struct di_buf_s *div3_que_peek(unsigned int ch, enum QUE_TYPE qtype)
{
	struct di_buf_s *di_buf = NULL;
	unsigned int q_index;

	if (!pw_queue_peek(ch, qtype, &q_index))
		return di_buf;
	di_buf = pwv3_qindex_2_buf(ch, q_index);
	trace_buf(di_buf, DIM_QUE_PEEK, qtype,
		  (DIM_DBG_MARK | 0x00008000));

	return di_buf;
}

bool div3_que_type_2_new(unsigned int q_type, enum QUE_TYPE *nqtype)
{
	if (!F_IN(q_type, QUEUE_NEW_THD_MIN, QUEUE_NEW_THD_MAX))
		return false;
	*nqtype = (enum QUE_TYPE)(q_type - QUEUE_NUM);

	return true;
}

/**********************************************************/
/**********************************************************/
/*ary add this function for reg ini value, no need wait peek*/
void queuev3_init2(unsigned int channel)
{
	int i, j;
	struct queue_s *pqueue = get_queue(channel);

	for (i = 0; i < QUEUE_NUM; i++) {
		queue_t *q = &pqueue[i];

		for (j = 0; j < MAX_QUEUE_POOL_SIZE; j++)
			q->pool[j] = 0;

		q->in_idx = 0;
		q->out_idx = 0;
		q->num = 0;
		q->type = 0;
		if ((i == QUEUE_RECYCLE) ||
		    (i == QUEUE_DISPLAY) ||
		    (i == QUEUE_TMP)
		    /*||   (i == QUEUE_POST_DOING)*/)
			q->type = 1;

#ifdef HIS_V3
		if ((i == QUEUE_LOCAL_FREE) && dim_get_use_2_int_buf())
			q->type = 2;
#endif
	}
}

void queuev3_init(unsigned int channel, int local_buffer_num)
{
	int i, j;
	struct di_buf_s *pbuf_local = get_buf_local(channel);
	struct di_buf_s *pbuf_in = get_buf_in(channel);
	struct di_buf_s *pbuf_post = get_buf_post(channel);
	struct queue_s *pqueue = get_queue(channel);
	struct di_buf_pool_s *pbuf_pool = get_buf_pool(channel);

	for (i = 0; i < QUEUE_NUM; i++) {
		queue_t *q = &pqueue[i];

		for (j = 0; j < MAX_QUEUE_POOL_SIZE; j++)
			q->pool[j] = 0;

		q->in_idx = 0;
		q->out_idx = 0;
		q->num = 0;
		q->type = 0;
		if ((i == QUEUE_RECYCLE) ||
		    (i == QUEUE_DISPLAY) ||
		    (i == QUEUE_TMP)
		    /*||(i == QUEUE_POST_DOING)*/
		    )
			q->type = 1;

		if ((i == QUEUE_LOCAL_FREE) &&
		    dimp_get(eDI_MP_use_2_interlace_buff))
			q->type = 2;
	}
	if (local_buffer_num > 0) {
		pbuf_pool[VFRAME_TYPE_IN - 1].di_buf_ptr = &pbuf_in[0];
		pbuf_pool[VFRAME_TYPE_IN - 1].size = MAX_IN_BUF_NUM;

		pbuf_pool[VFRAME_TYPE_LOCAL - 1].di_buf_ptr = &pbuf_local[0];
		pbuf_pool[VFRAME_TYPE_LOCAL - 1].size = local_buffer_num;

		pbuf_pool[VFRAME_TYPE_POST - 1].di_buf_ptr = &pbuf_post[0];
		pbuf_pool[VFRAME_TYPE_POST - 1].size = MAX_POST_BUF_NUM;
	}
}

struct di_buf_s *getv3_di_buf_head(unsigned int channel, int queue_idx)
{
	struct queue_s *pqueue = get_queue(channel);
	queue_t *q = &pqueue[queue_idx];
	int idx;
	unsigned int pool_idx, di_buf_idx;
	struct di_buf_s *di_buf = NULL;
	struct di_buf_pool_s *pbuf_pool = get_buf_pool(channel);
	enum QUE_TYPE nqtype;/*new que*/

	if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_QUEUE)
		dimv3_print("%s:<%d:%d,%d,%d>\n", __func__, queue_idx,
			    q->num, q->in_idx, q->out_idx);
	/* ****new que***** */
	if (div3_que_type_2_new(queue_idx, &nqtype))
		return div3_que_peek(channel, nqtype);

	/* **************** */

	if (q->num > 0) {
		if (q->type == 0) {
			idx = q->out_idx;
		} else {
			for (idx = 0; idx < MAX_QUEUE_POOL_SIZE; idx++)
				if (q->pool[idx] != 0)
					break;
		}
		if (idx < MAX_QUEUE_POOL_SIZE) {
			pool_idx = ((q->pool[idx] >> 8) & 0xff) - 1;
			di_buf_idx = q->pool[idx] & 0xff;

	if (pool_idx < VFRAME_TYPE_NUM) {
		if (di_buf_idx < pbuf_pool[pool_idx].size)
			di_buf = &pbuf_pool[pool_idx].di_buf_ptr[di_buf_idx];
	}
		}
	}

	if ((di_buf) && ((((pool_idx + 1) << 8) | di_buf_idx) !=
			 ((di_buf->type << 8) | (di_buf->index)))) {
		pr_dbg("%s: Error (%x,%x)\n", __func__,
		       (((pool_idx + 1) << 8) | di_buf_idx),
		       ((di_buf->type << 8) | (di_buf->index)));

		if (dimv3_vcry_get_flg() == 0) {
			dimv3_vcry_set_log_reason(2);
			dimv3_vcry_set_log_q_idx(queue_idx);
			dimv3_vcry_set_log_di_buf(di_buf);
		}
		dimv3_vcry_flg_inc();
		di_buf = NULL;
	}

	if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_QUEUE) {
		if (di_buf)
			dimv3_print("%s: 0x%p(%d,%d)\n", __func__, di_buf,
				    pool_idx, di_buf_idx);
		else
			dimv3_print("%s: 0x%p\n", __func__, di_buf);
	}

	return di_buf;
}

/*ary: note:*/
/*a. di_buf->queue_index = -1*/
/*b. */
void queuev3_out(unsigned int channel, struct di_buf_s *di_buf)
{
	int i;
	queue_t *q;
	struct queue_s *pqueue = get_queue(channel);
	enum QUE_TYPE nqtype;/*new que*/
#ifdef DIM_DEBUG_QUE_ERR
	union dim_dbg_que_u dbg;
#endif

	if (!di_buf) {
		PR_ERR("queuev3_out:Error\n");

		if (dimv3_vcry_get_flg() == 0)
			dimv3_vcry_set_log_reason(3);

		dimv3_vcry_flg_inc();
		return;
	}
	/* ****new que***** */
	if (div3_que_type_2_new(di_buf->queue_index, &nqtype)) {
		div3_que_out(channel, nqtype, di_buf);	/*?*/
		return;
	}
#ifdef DIM_DEBUG_QUE_ERR
	/*dbg*/
	dbg.b.buf_index = di_buf->index;
	dbg.b.buf_type = di_buf->type;
	dbg.b.omx_index = (di_buf->c.vmode.omx_index) & 0xff;
	dbg.b.que_api	= DIM_O_OUT;
	dbg.b.que_index = di_buf->queue_index;
	dim_dbg_que(&dbg);

#endif
	/* **************** */

	if (di_buf->queue_index >= 0 && di_buf->queue_index < QUEUE_NUM) {
		q = &pqueue[di_buf->queue_index];

		if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_QUEUE)
			dimv3_print("%s:<%d:%d,%d,%d> 0x%p\n", __func__,
				    di_buf->queue_index, q->num, q->in_idx,
				    q->out_idx, di_buf);

		if (q->num > 0) {
			if (q->type == 0) {
				if (q->pool[q->out_idx] ==
				    ((di_buf->type << 8) | (di_buf->index))) {
					q->num--;
					q->pool[q->out_idx] = 0;
					q->out_idx++;
					if (q->out_idx >= MAX_QUEUE_POOL_SIZE)
						q->out_idx = 0;
					di_buf->queue_index = -1;
				} else {
					PR_ERR(
						"%s: Error (%d, %x,%x)\n",
						__func__,
						di_buf->queue_index,
						q->pool[q->out_idx],
						((di_buf->type << 8) |
						(di_buf->index)));

			if (dimv3_vcry_get_flg() == 0) {
				dimv3_vcry_set_log_reason(4);
				dimv3_vcry_set_log_q_idx(di_buf->queue_index);
				dimv3_vcry_set_log_di_buf(di_buf);
			}
					dimv3_vcry_flg_inc();
				}
			} else if (q->type == 1) {
				int pool_val =
					(di_buf->type << 8) | (di_buf->index);
				for (i = 0; i < MAX_QUEUE_POOL_SIZE; i++) {
					if (q->pool[i] == pool_val) {
						q->num--;
						q->pool[i] = 0;
						di_buf->queue_index = -1;
						break;
					}
				}
				if (i == MAX_QUEUE_POOL_SIZE) {
					PR_ERR("%s: Error\n", __func__);

			if (dimv3_vcry_get_flg() == 0) {
				dimv3_vcry_set_log_reason(5);
				dimv3_vcry_set_log_q_idx(di_buf->queue_index);
				dimv3_vcry_set_log_di_buf(di_buf);
			}
					dimv3_vcry_flg_inc();
				}
			} else if (q->type == 2) {
				int pool_val =
					(di_buf->type << 8) | (di_buf->index);
				if ((di_buf->index < MAX_QUEUE_POOL_SIZE) &&
				    (q->pool[di_buf->index] == pool_val)) {
					q->num--;
					q->pool[di_buf->index] = 0;
					di_buf->queue_index = -1;
				} else {
					PR_ERR("%s: Error\n", __func__);

		if (dimv3_vcry_get_flg() == 0) {
			dimv3_vcry_set_log_reason(5);
			dimv3_vcry_set_log_q_idx(di_buf->queue_index);
			dimv3_vcry_set_log_di_buf(di_buf);
		}
					dimv3_vcry_flg_inc();
				}
			}
		}
	} else {
		PR_ERR("%s: t[%d] d[%d] is not right\n",
		       __func__, di_buf->type, di_buf->index);
		dump_stack();
		if (dimv3_vcry_get_flg() == 0) {
			dimv3_vcry_set_log_reason(6);
			dimv3_vcry_set_log_q_idx(0);
			dimv3_vcry_set_log_di_buf(di_buf);
		}
		dimv3_vcry_flg_inc();
	}

	if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_QUEUE)
		dimv3_print("%s done\n", __func__);
}

void queuev3_out_dbg(unsigned int channel, struct di_buf_s *di_buf)
{
	int i;
	queue_t *q;
	struct queue_s *pqueue = get_queue(channel);
	enum QUE_TYPE nqtype;/*new que*/

	if (!di_buf) {
		PR_ERR("%s:Error\n", __func__);

		if (dimv3_vcry_get_flg() == 0)
			dimv3_vcry_set_log_reason(3);

		dimv3_vcry_flg_inc();
		return;
	}
	/* ****new que***** */
	if (div3_que_type_2_new(di_buf->queue_index, &nqtype)) {
		div3_que_out(channel, nqtype, di_buf);	/*?*/
		PR_INF("dbg1:nqtype=%d\n", nqtype);
		return;
	}
	/* **************** */

	if (di_buf->queue_index >= 0 && di_buf->queue_index < QUEUE_NUM) {
		q = &pqueue[di_buf->queue_index];

		if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_QUEUE)
			dimv3_print("%s:<%d:%d,%d,%d> 0x%p\n", __func__,
				    di_buf->queue_index, q->num, q->in_idx,
				    q->out_idx, di_buf);

		if (q->num > 0) {
			if (q->type == 0) {
				pr_info("dbg3\n");
				if (q->pool[q->out_idx] ==
				    ((di_buf->type << 8) | (di_buf->index))) {
					q->num--;
					q->pool[q->out_idx] = 0;
					q->out_idx++;
					if (q->out_idx >= MAX_QUEUE_POOL_SIZE)
						q->out_idx = 0;
					di_buf->queue_index = -1;
				} else {
					PR_ERR(
						"%s: Error (%d, %x,%x)\n",
						__func__,
						di_buf->queue_index,
						q->pool[q->out_idx],
						((di_buf->type << 8) |
						(di_buf->index)));

			if (dimv3_vcry_get_flg() == 0) {
				dimv3_vcry_set_log_reason(4);
				dimv3_vcry_set_log_q_idx(di_buf->queue_index);
				dimv3_vcry_set_log_di_buf(di_buf);
			}
					dimv3_vcry_flg_inc();
				}
			} else if (q->type == 1) {
				int pool_val =
					(di_buf->type << 8) | (di_buf->index);
				for (i = 0; i < MAX_QUEUE_POOL_SIZE; i++) {
					if (q->pool[i] == pool_val) {
						q->num--;
						q->pool[i] = 0;
						di_buf->queue_index = -1;
						break;
					}
				}
				PR_INF("dbg2:i=%d,qindex=%d\n", i,
				       di_buf->queue_index);
				if (i == MAX_QUEUE_POOL_SIZE) {
					PR_ERR("%s: Error\n", __func__);

			if (dimv3_vcry_get_flg() == 0) {
				dimv3_vcry_set_log_reason(5);
				dimv3_vcry_set_log_q_idx(di_buf->queue_index);
				dimv3_vcry_set_log_di_buf(di_buf);
			}
					dimv3_vcry_flg_inc();
				}
			} else if (q->type == 2) {
				int pool_val =
					(di_buf->type << 8) | (di_buf->index);

				PR_INF("dbg4\n");
				if ((di_buf->index < MAX_QUEUE_POOL_SIZE) &&
				    (q->pool[di_buf->index] == pool_val)) {
					q->num--;
					q->pool[di_buf->index] = 0;
					di_buf->queue_index = -1;
				} else {
					PR_ERR("%s: Error\n", __func__);

					if (dimv3_vcry_get_flg() == 0) {
						dimv3_vcry_set
							(5,
							 di_buf->queue_index,
							 di_buf);
					}
					dimv3_vcry_flg_inc();
				}
			}
		}
	} else {
		PR_ERR("%s: Error, queue_index %d is not right\n",
		       __func__, di_buf->queue_index);

		if (dimv3_vcry_get_flg() == 0) {
			dimv3_vcry_set_log_reason(6);
			dimv3_vcry_set_log_q_idx(0);
			dimv3_vcry_set_log_di_buf(di_buf);
		}
		dimv3_vcry_flg_inc();
	}

	if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_QUEUE)
		dimv3_print("%s done\n", __func__);
}

/***************************************/
/* set di_buf->queue_index*/
/***************************************/
void queuev3_in(unsigned int channel, struct di_buf_s *di_buf, int queue_idx)
{
	queue_t *q = NULL;
	struct queue_s *pqueue = get_queue(channel);
	enum QUE_TYPE nqtype;/*new que*/

	if (!di_buf) {
		PR_ERR("%s:Error\n", __func__);
		if (dimv3_vcry_get_flg() == 0) {
			dimv3_vcry_set_log_reason(7);
			dimv3_vcry_set_log_q_idx(queue_idx);
			dimv3_vcry_set_log_di_buf(di_buf);
		}
		dimv3_vcry_flg_inc();
		return;
	}
	/* ****new que***** */
	if (div3_que_type_2_new(queue_idx, &nqtype)) {
		div3_que_in(channel, nqtype, di_buf);
		return;
	}
	/* **************** */
	if (di_buf->queue_index != -1) {
		dump_stack();
		PR_ERR("%s:%s[%d] queue_index(%d) is not -1, to que[%d]\n",
		       __func__, dimv3_get_vfm_type_name(di_buf->type),
		       di_buf->index, di_buf->queue_index, queue_idx);
		if (dimv3_vcry_get_flg() == 0) {
			dimv3_vcry_set_log_reason(8);
			dimv3_vcry_set_log_q_idx(queue_idx);
			dimv3_vcry_set_log_di_buf(di_buf);
		}
		dimv3_vcry_flg_inc();
		return;
	}
	q = &pqueue[queue_idx];
	if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_QUEUE)
		dimv3_print("%s:<%d:%d,%d,%d> 0x%p\n", __func__, queue_idx,
			    q->num, q->in_idx, q->out_idx, di_buf);

	if (q->type == 0) {
		q->pool[q->in_idx] = (di_buf->type << 8) | (di_buf->index);
		di_buf->queue_index = queue_idx;
		q->in_idx++;
		if (q->in_idx >= MAX_QUEUE_POOL_SIZE)
			q->in_idx = 0;

		q->num++;
	} else if (q->type == 1) {
		int i;

		for (i = 0; i < MAX_QUEUE_POOL_SIZE; i++) {
			if (q->pool[i] == 0) {
				q->pool[i] =
					(di_buf->type << 8) | (di_buf->index);
				di_buf->queue_index = queue_idx;
				q->num++;
				break;
			}
		}
		if (i == MAX_QUEUE_POOL_SIZE) {
			pr_dbg("%s: Error\n", __func__);
			if (dimv3_vcry_get_flg() == 0) {
				dimv3_vcry_set_log_reason(9);
				dimv3_vcry_set_log_q_idx(queue_idx);
			}
			dimv3_vcry_flg_inc();
		}
	} else if (q->type == 2) {
		if ((di_buf->index < MAX_QUEUE_POOL_SIZE) &&
		    (q->pool[di_buf->index] == 0)) {
			q->pool[di_buf->index] =
				(di_buf->type << 8) | (di_buf->index);
			di_buf->queue_index = queue_idx;
			q->num++;
		} else {
			pr_dbg("%s: Error\n", __func__);
			if (dimv3_vcry_get_flg() == 0) {
				dimv3_vcry_set_log_reason(9);
				dimv3_vcry_set_log_q_idx(queue_idx);
			}
			dimv3_vcry_flg_inc();
		}
	}

	if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_QUEUE)
		dimv3_print("%s done\n", __func__);
}

int listv3_count(unsigned int channel, int queue_idx)
{
	struct queue_s *pqueue;
	enum QUE_TYPE nqtype;/*new que*/

	/* ****new que***** */
	if (div3_que_type_2_new(queue_idx, &nqtype)) {
		PR_ERR("%s:err: over flow\n", __func__);
		return div3_que_list_count(channel, nqtype);
	}
	/* **************** */

	pqueue = get_queue(channel);
	return pqueue[queue_idx].num;
}

bool queuev3_empty(unsigned int channel, int queue_idx)
{
	struct queue_s *pqueue;
	bool ret;
	enum QUE_TYPE nqtype;/*new que*/

	/* ****new que***** */
	if (div3_que_type_2_new(queue_idx, &nqtype)) {
		PR_ERR("%s:err: over flow\n", __func__);
		return div3_que_is_empty(channel, nqtype);
	}
	/* **************** */

	pqueue = get_queue(channel);
	ret = (pqueue[queue_idx].num == 0);

	return ret;
}

bool isv3_in_queue(unsigned int channel, struct di_buf_s *di_buf, int queue_idx)
{
	bool ret = 0;
	struct di_buf_s *p = NULL;
	int itmp;
	unsigned int overflow_cnt;
	enum QUE_TYPE nqtype;/*new que*/

	/* ****new que***** */
	if (div3_que_type_2_new(queue_idx, &nqtype))
		return div3_que_is_in_que(channel, nqtype, di_buf);

	/* **************** */

	overflow_cnt = 0;
	if (!di_buf || (queue_idx < 0) || (queue_idx >= QUEUE_NUM)) {
		ret = 0;
		dimv3_print("%s: not in queue:%d!!!\n", __func__, queue_idx);
		return ret;
	}
	queue_for_each_entry(p, channel, queue_idx, list) {
		if (p == di_buf) {
			ret = 1;
			break;
		}
		if (overflow_cnt++ > MAX_QUEUE_POOL_SIZE) {
			ret = 0;
			dimv3_print("%s: overflow_cnt!!!\n", __func__);
			break;
		}
	}
	return ret;
}

void qued_remove(struct di_ch_s *pch)
{
	struct dim_que_s	*pdque;
	int i;

	if (!pch)
		return;

	for (i = 0; i < QUED_T_SPLIT; i++) {
		pdque = &pch->qued[i];
		if (pdque->f.flg) {
			kfifo_free(&pdque->f.fifo);
			pdque->f.flg = 0;
		}
	}

	PR_INF("%s:ok\n", __func__);
}

bool qued_prob(struct di_ch_s *pch)
{
	int i;
	int ret;
	bool flg_err;
	//struct di_ch_s *pch = get_chdata(ch);
	struct dim_que_s	*pdque;
	unsigned int cnt = sizeof(unsigned int);

	if (!pch)
		return false;

	//pdque = &pch->qued[0];
	/*kfifo----------------------------*/
	flg_err = 0;
	for (i = 0; i < QUED_T_SPLIT; i++) {
		pdque = &pch->qued[i];
		ret = kfifo_alloc(&pdque->f.fifo,
				  cnt * MAX_FIFO_SIZE,
				  GFP_KERNEL);
		if (ret < 0) {
			flg_err = 1;
			PR_ERR("%s:%d:can't get kfifo\n", __func__, i);
			bsetv3(&pch->err, DIM_ERR_QUE_FIFO);
			break;
		}
		pdque->f.flg = 1;
	}

	if (flg_err) {
		qued_remove(pch);
		return false;
	}
	bclrv3(&pch->err, DIM_ERR_QUE_FIFO);

	/*mutex*/
	for (i = QUED_T_SPLIT; i < QUED_T_NUB; i++) {
		pdque = &pch->qued[i];
		mutex_init(&pdque->n.mtex);
	}

	PR_INF("%s:ok\n", __func__);

	return true;
}

static const char * const qued_name[] = {
	"FREE",
	"IN",
	"PRE",
	"PRE_READY",
	"PST",
	"PST_READY",
	"BACK",
	"RECYCLE",
	"IS_IN",
	"IS_FREE",
	"IS_PST_FREE",
	"IS_PST_DOBEF",
	//"IS_PST_NOBUF",
	"PST_DOING",
	"DBG",
	"SPLIT", /*not use, only split*/
	"DIS",
	"NUB"
};

const char *qued_get_name(struct di_ch_s *pch, enum QUED_TYPE qtype)
{
	struct dim_que_s	*pdque;
//	unsigned int cnt = sizeof (unsigned int);

	if (!pch)
		return NULL;

	pdque = &pch->qued[qtype];

	return pdque->name;
}

bool qued_reg(struct di_ch_s *pch)
{
	//struct di_ch_s *pch = get_chdata(ch);
//	struct dim_dvfm_s	*pdvfm;
	struct dim_que_s	*pdque;
	int i;

	if (!pch)
		return false;

	for (i = 0; i < QUED_T_NUB; i++) {
		pdque = &pch->qued[i];
		pdque->index = i;
		if (i < sizeof(qued_name))
			pdque->name = qued_name[i];

		if (i < QUED_T_SPLIT) { /*fifo que*/
			pdque->type = QUED_K_FIFO;
			dbg_dbg("%s:que reset t[%s]\n", __func__, pdque->name);
			kfifo_reset(&pdque->f.fifo);
		} else {		/*QUED_K_N*/
			pdque->type = QUED_K_N;
			pdque->n.marsk = 0;
			pdque->n.nub = 0;
		}
	}
	return true;
}

bool qued_unreg(struct di_ch_s *pch)
{
	return true;
}

bool qued_in(struct di_ch_s *pch, enum QUED_TYPE qtype, unsigned int buf_index)
{
	struct dim_que_s	*pdque;
	unsigned int cnt = sizeof(unsigned int);
//	bool ret;

	if (!pch)
		return false;
	pdque = &pch->qued[qtype];

	if (pdque->type == QUED_K_FIFO) {
		if (kfifo_in(&pdque->f.fifo, &buf_index, cnt) != cnt) {
			PR_ERR("%s:ch[%d]f:q[%s][%d]\n", __func__,
			       pch->ch_id,
			       qued_get_name(pch, qtype),
			       buf_index);
			return false;
		}
		dbg_qued("%s:ch[%d],q[%s],id[%d]\n",
			 __func__,
			 pch->ch_id,
			 qued_name[qtype],
			 buf_index);
		return true;
	}

	/*QUED_K_N*/
	mutex_lock(&pdque->n.mtex);
	if (bgetv3(&pdque->n.marsk, buf_index)) { /*check*/
		PR_ERR("%s:ch[%d],q[%s],b[%d] have set\n",
		       __func__, pch->ch_id,
		       qued_get_name(pch, qtype),
		       buf_index);
		mutex_unlock(&pdque->n.mtex);
		return false;
	}
	bsetv3(&pdque->n.marsk, buf_index);
	pdque->n.nub++;
	mutex_unlock(&pdque->n.mtex);

	dbg_qued("%s:ch[%d],q[%s],id[%d]\n",
		 __func__,
		 pch->ch_id,
		 qued_name[qtype],
		 buf_index);
	return true;
}

bool qued_out(struct di_ch_s *pch, enum QUED_TYPE qtype,
	      unsigned int *buf_index)
{
	struct dim_que_s	*pdque;
	unsigned int cnt = sizeof(unsigned int);

	if (!pch)
		return false;
	pdque = &pch->qued[qtype];

	if (pdque->type == QUED_K_FIFO) {
		if (kfifo_out(&pdque->f.fifo, buf_index, cnt) != cnt)
			return false;

		dbg_qued("%s:ch[%d],q[%s],id[%d]\n",
			 __func__,
			 pch->ch_id,
			 qued_name[qtype],
			 *buf_index);
		return true;
	}

	/*QUED_K_N*/
	mutex_lock(&pdque->n.mtex);
	if (!bgetv3(&pdque->n.marsk, *buf_index)) { /*check*/
		PR_ERR("%s:ch[%d],q[%d],b[%d] have set\n",
		       __func__, pch->ch_id, qtype, *buf_index);
		mutex_unlock(&pdque->n.mtex);
		return false;
	}
	bclrv3(&pdque->n.marsk, *buf_index);
	pdque->n.nub--;
	mutex_unlock(&pdque->n.mtex);

	dbg_qued("%s:ch[%d],q[%s],id[%d]\n",
		 __func__,
		 pch->ch_id,
		 qued_name[qtype],
		 *buf_index);
	return true;
}

bool qued_move(struct di_ch_s *pch, enum QUED_TYPE qtypef,
	       enum QUED_TYPE qtypet, unsigned int *buf_index)
{
	struct dim_que_s	*pdquef, *pdquet;
	unsigned int cnt = sizeof(unsigned int);
//	bool ret;
	unsigned int index;

	if (!pch) {
		PR_ERR("%s:no pch\n", __func__);
		return false;
	}
	pdquef = &pch->qued[qtypef];
	pdquet = &pch->qued[qtypet];

	if (pdquef->type != QUED_K_FIFO ||
	    pdquet->type != QUED_K_FIFO) {
		PR_ERR("%s: don't support nfiko\n", __func__);
		return false;
	}

	if (kfifo_is_empty(&pdquef->f.fifo) || kfifo_is_full(&pdquet->f.fifo)) {
		PR_ERR("%s:que full or empty\n", __func__);
		PR_ERR("f:q[%s],t:q[%s]\n", qued_name[qtypef],
		       qued_name[qtypet]);
		dump_stack();
		return false;
	}

	if (kfifo_out(&pdquef->f.fifo, &index, cnt) != cnt) {
		PR_ERR("%s:ch[%d]f:q[%s][%d]\n", __func__,
		       pch->ch_id,
		       qued_get_name(pch, qtypef),
		       index);
			return false;
	}

	if (kfifo_in(&pdquet->f.fifo, &index, cnt) != cnt) {
		PR_ERR("%s:ch[%d]f:q[%s][%d]\n", __func__,
		       pch->ch_id,
		       qued_get_name(pch, qtypet),
		       index);
		return false;
	}
	dbg_qued("%s:ch[%d],qf[%s],id[%d],ft[%s]\n",
		 __func__,
		 pch->ch_id,
		 qued_name[qtypef], index,
		 qued_name[qtypet]);

	*buf_index = index;
	return true;
}

bool qued_peek(struct di_ch_s *pch, enum QUED_TYPE qtype,
	       unsigned int *buf_index)
{
	struct dim_que_s	*pdque;
	unsigned int cnt = sizeof(unsigned int);

	if (!pch)
		return false;
	pdque = &pch->qued[qtype];

	if (pdque->type == QUED_K_FIFO) {
		if (kfifo_out_peek(&pdque->f.fifo, buf_index, cnt) != cnt)
			return false;

		return true;
	}

	PR_ERR("q:%s no peek function\n", qued_get_name(pch, qtype));
	return false;
}

#ifdef HIS_V3
bool qued_move(struct di_ch_s *pch, enum QUED_TYPE qtypef, enum QUE_TYPE qtypet,
		   unsigned int *oindex)
{
	struct dim_que_s	*pdquef, *pdquet;
	unsigned int cnt = sizeof(unsigned int);

	if (!pch)
		return;
	pdquef = &pch->qued[qtypef];
	pdquet = &pch->qued[qtypet];

	if (pdquef->type == QUED_K_FIFO &&
	    pdquet->type == QUED_K_FIFO) {
	}
}
#endif

bool qued_list(struct di_ch_s *pch, enum QUED_TYPE qtype, unsigned int *outbuf,
	       unsigned int *rsize)
{
	struct dim_que_s	*pdque, *pdque_tmp;
	unsigned int cnt = sizeof(unsigned int);
	int i;
	unsigned int index;
	unsigned int mask;

	if (!pch)
		return false;
	pdque = &pch->qued[qtype];

	/*que_dbg("%s:begin\n", __func__);*/
	for (i = 0; i < MAX_FIFO_SIZE; i++)
		outbuf[i] = 0xff;

	if (pdque->type == QUED_K_FIFO) {
		pdque_tmp = &pch->qued[QUED_T_DBG];

		if (kfifo_is_empty(&pdque->f.fifo)) {
			que_dbg("\t%d:empty\n", qtype);
			*rsize = 0;
			return true;
		}

		memcpy(&pdque_tmp->f.fifo, &pdque->f.fifo,
		       sizeof(pdque_tmp->f.fifo));

		i = 0;
		*rsize = 0;

		while (kfifo_out(&pdque_tmp->f.fifo, &index, cnt) == cnt) {
			outbuf[i] = index;
			/*pr_info("%d->%d\n",i,index);*/
			i++;
		}
		*rsize = i;

		return true;
	}

	/*QUED_K_N*/
	mutex_lock(&pdque->n.mtex);
	mask = pdque->n.marsk;
	*rsize = pdque->n.nub;
	mutex_unlock(&pdque->n.mtex);

	cnt = 0;
	for (i = 0; i < MAX_FIFO_SIZE; i++) {
		if (mask & 0x01) {
			outbuf[cnt] = i;
			cnt++;
		}
		mask >>= 1;
	}

	#ifdef HIS_V3	/*debug only*/
	que_dbg("%s: size[%d]\n", div3_name_new_que[qtype], *rsize);
	for (i = 0; i < *rsize; i++)
		que_dbg("%d,", outbuf[i]);

	que_dbg("\n");
	#endif

	return true;
}

unsigned int qued_list_count(struct di_ch_s *pch, enum QUED_TYPE qtype)
{
	struct dim_que_s	*pdque;
	unsigned int cnt = sizeof(unsigned int);
	unsigned int length = 0;

	if (!pch)
		return 0;
	pdque = &pch->qued[qtype];

	if (pdque->type == QUED_K_FIFO) {
		length = kfifo_len(&pdque->f.fifo);
		length = length / cnt;

		return length;
	}

	/*QUED_K_N*/
	mutex_lock(&pdque->n.mtex);
	length = pdque->n.nub;
	mutex_unlock(&pdque->n.mtex);

	return length;
}

bool qued_is_in_que(struct di_ch_s *pch, enum QUED_TYPE qtype,
		    unsigned int buf_index)
{
//	unsigned int q_index;
	unsigned int arr[MAX_FIFO_SIZE + 1];
	unsigned int asize = 0;
	bool ret = false;
	unsigned int i;
//	struct dim_que_s	*pdque;

	if (!pch)
		return false;

	qued_list(pch, qtype, &arr[0], &asize);

	if (asize == 0)
		return ret;

	for (i = 0; i < asize; i++) {
		if (arr[i] == buf_index) {
			ret = true;
			break;
		}
	}
	return ret;
}

bool qued_empty(struct di_ch_s *pch, enum QUED_TYPE qtype)
{
	struct dim_que_s	*pdque;
//	unsigned int cnt = sizeof (unsigned int);

	if (!pch)
		return true;

	pdque = &pch->qued[qtype];

	if (pdque->type == QUED_K_FIFO) {
		if (kfifo_is_empty(&pdque->f.fifo))
			return true;
		return false;
	}

	/*QUED_K_N*/
	if (pdque->n.nub == 0)
		return true;

	return false;
}

const struct qued_ops_s qued_ops = {
	.prob		= qued_prob,
	.remove		= qued_remove,
	.reg		= qued_reg,
	.unreg		= qued_unreg,
	.in		= qued_in,
	.out		= qued_out,
	.peek		= qued_peek,
	.list		= qued_list,
	.listv3_count	= qued_list_count,
	.is_in		= qued_is_in_que,
	.is_empty	= qued_empty,
	.get_name	= qued_get_name,
	.move		= qued_move,
};

