/*
 * drivers/amlogic/media/di_multi_v3/di_interface.c
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
#include "di_prc.h"
#include "di_dbg.h"
#include "di_que.h"
#include "di_task.h"

#include "di_vframe.h"

#ifdef HIS_V3
const struct di_init_parm dim_cfg_parm_default = {
	.work_mode	= WORK_MODE_PRE_POST,
	.buffer_mode	= BUFFER_MODE_ALLOC_BUF,
	.ops = {
		.empty_input_done	= NULL,
		.fill_output_done	= NULL,
	},
};
#endif
/**********************************************************
 * ops
 **********************************************************/
static void *ins_peek(struct di_ch_s *pch)
{
	unsigned int index;

	if (!qued_ops.peek(pch, QUED_T_IS_IN, &index))
		return NULL;
	return (void *)pch->interf.u.dinst.in[index];
}

static void *ins_get(struct di_ch_s *pch)
{
	unsigned int index;
	struct di_buffer *dbuf;
	unsigned int dbg_cnt1, dbg_cnt2;

	if (!qued_ops.move(pch,
			   QUED_T_IS_IN,
			   QUED_T_IS_FREE, &index)) {
		dbg_cnt1 = qued_ops.listv3_count(pch, QUED_T_IS_IN);
		dbg_cnt2 = qued_ops.listv3_count(pch, QUED_T_IS_FREE);
		PR_ERR("ins_get:no space,%d,%d\n", dbg_cnt1, dbg_cnt2);
		return NULL;
	}
	sum_g_inc(pch->ch_id);
	dbuf = pch->interf.u.dinst.in[index];
	pch->interf.u.dinst.in[index] = NULL;
	return (void *)dbuf;
}

static void ins_put(void *data, struct di_ch_s *pch)
{
	struct dim_inter_s *pintf = &pch->interf;
	struct di_operations_s *ops;

	ops = &pintf->u.dinst.parm.ops;
	if (ops->empty_input_done) {
		sum_p_inc(pch->ch_id);
		ops->empty_input_done((struct di_buffer *)data);
		//debug only dump_stack();
	}
}

const struct dim_itf_ops_s inst_in_ops = {
	.peek	= ins_peek,
	.get	= ins_get,
	.put	= ins_put,
};

static int reg_idle_ch(void)
{
	unsigned int ch;
	struct dim_inter_s *pintf;
	int ret = -1;
	//int i;
	struct di_ch_s *pch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pintf = get_dev_intf(ch);
		pch = get_chdata(ch);
		if ((pbm->tmode_pre[ch] == EDIM_TMODE_2_PW_OUT ||
		     pbm->tmode_pre[ch] == EDIM_TMODE_3_PW_LOCAL) &&
		    !pintf->reg) {
	#ifdef HIS_V3	//move to dip_init_value_reg
			/*que buf*/
			for (i = 0; i < DIM_K_BUF_IN_LIMIT; i++)
				qued_ops.in(pch, QUED_T_IS_FREE, i);

			for (i = 0; i < DIM_K_BUF_OUT_LIMIT; i++)
				qued_ops.in(pch, QUED_T_IS_PST_FREE, i);
	#endif
			/*set reg flg*/
			pintf->reg = true;
			ret = (int)ch;
			dbg_dbg("%s:ch[%d], ret[%d]\n", __func__, ch, ret);
			break;
		}
		dbg_dbg("%s:ch[%d],tmode[%d],reg[%d]\n", __func__,
			ch, pbm->tmode_pre[ch], pintf->reg);
	}
	dbg_dbg("%s:%d\n", __func__, ret);
	return ret;
}
/* mode 2 for drop */
void ins_2_doing(struct di_ch_s *pch, bool bypass, struct di_buf_s *pstdi_buf)
{
	unsigned int ch = pch->ch_id;
	struct di_buf_s *di_buf = NULL; /*post nobuf buffer*/
	bool ret;
	struct di_buffer	*ins_buf, *ins_in;
	unsigned int idx;
	struct dim_inter_s	*pintf;
	struct vframe_s		*vfmin, *vfmout;
	struct canvas_config_s	cvs[2];

	if (div3_que_is_empty(ch, QUE_POST_NOBUF))
		return;

	if (qued_ops.is_empty(pch, QUED_T_IS_PST_DOBEF))
		return;

	di_buf = div3_que_peek(ch, QUE_POST_NOBUF);
	dimv3_print("%s:1:%p,buf[%d],t[%d]\n", __func__, di_buf,
		  di_buf->index, di_buf->type);
	trace_buf(di_buf, DIM_QUE_PEEK, QUE_POST_NOBUF,
		(DIM_DBG_MARK | 0x00000001));
	ret = div3_que_out(ch, QUE_POST_NOBUF, di_buf);
	if (!ret) {
		PR_ERR("%s:no buf?\n", __func__);
		return;
	}

	pintf = &pch->interf;
	qued_ops.out(pch, QUED_T_IS_PST_DOBEF, &idx);

	ins_buf = pintf->u.dinst.out[idx];
	if (!ins_buf) {
		PR_ERR("%s:out is null\n", __func__);
		return;
	}
	di_buf->c.pdvfm->vfm_out = pintf->u.dinst.out[idx];
	pintf->u.dinst.out[idx] = NULL;
	qued_ops.in(pch, QUED_T_IS_PST_FREE, idx);

	trace_buf(di_buf, DIM_QUE_OUT, QUE_POST_NOBUF,
			(DIM_DBG_MARK | 0x00000002));

	dbg_dbg("%s:ch[%d],type[%d], id[%d]\n", __func__, pch->ch_id,
		di_buf->type, di_buf->index);
	dbg_dbg("ins_buf,type[%d], id[%d]\n",
		ins_buf->mng.type,
		ins_buf->mng.index);
	if (!ins_buf)
		PR_ERR("%s:out2 is null\n", __func__);

	/*in*/
	ins_in = (struct di_buffer *)di_buf->c.pdvfm->vfm_in;
	vfmin = ins_in->vf;
	vfmout = ins_buf->vf;
	if (!di_buf->c.wmode.is_eos) {
		/*copy vfm */
		memcpy(&cvs[0], &vfmout->canvas0_config[0], sizeof(cvs));
		memcpy(vfmout, vfmin, sizeof(*vfmout));
		memcpy(&vfmout->canvas0_config[0],
			&cvs[0], sizeof(cvs));
	}

	if (!di_buf->c.wmode.is_bypass) {
		//di_buf->nr_adr	= ins_buf->phy_addr;
		//di_buf->nr_adr = ins_buf->vf->canvas0_config[0].phy_addr;
		//ins_buf->phy_addr;
		dim_ins_cnt_post_cvs_size2(di_buf, ins_buf, ch);
	} else {
		/* bypass */
		if (di_buf->c.wmode.is_eos) {
			ins_buf->flag |= DI_FLAG_EOS;
			PR_INF("%s:eos\n", __func__);
		} else {
			ins_buf->flag |= DI_FLAG_BUF_BY_PASS;
		}
		dimv3_print("%s:%p bypass\n", __func__, ins_buf);
	}
	dbg_vfmv3(vfmout, 1);
	trace_buf(di_buf, DIM_QUE_IN, QUE_POST_DOING,
		(DIM_DBG_MARK | 0x00000003));
	dimv3_print("%s:%p,buf[%d],t[%d]\n", __func__, di_buf,
		  di_buf->index, di_buf->type);
	div3_que_in(ch, QUE_POST_DOING, di_buf);

}
#define ERR_INDEX	(0xffff)
static unsigned int index_2_ch(int index)
{
	unsigned int ret;

	if (index >= DI_CHANNEL_NUB || index < 0) {
		PR_ERR("instance index is overflow:%d\n", index);
		ret = ERR_INDEX;
	} else {
		ret = (unsigned int)index;
	}

	return ret;
}

/**********************************************************
 * @brief  di_create_instance  creat di instance
 * @param[in]  parm    Pointer of parm structure
 * @return      di index for success, or fail type if < 0
 **********************************************************/
int di_create_instance(struct di_init_parm parm)
{
	int ret;
	unsigned int ch;
	struct dim_inter_s *pintf;

	ret = reg_idle_ch();
	if (ret < 0) {
		PR_ERR("%s:no idle ch\n", __func__);
		return DI_ERR_REG_NO_IDLE_CH;
	}

	ch = (unsigned int)ret;
	pintf = get_dev_intf(ch);
	/*parm*/
	memcpy(&pintf->u.dinst.parm, &parm, sizeof(struct di_init_parm));
	/*ops in*/
	memcpy(&pintf->opsi, &inst_in_ops, sizeof(struct dim_itf_ops_s));
	pintf->tmode = EDIM_TMODE_2_PW_OUT;
	if (pintf->u.dinst.parm.work_mode == WORK_MODE_PRE_POST) {
		switch (pintf->u.dinst.parm.buffer_mode) {
		case BUFFER_MODE_ALLOC_BUF:
			pintf->tmode = EDIM_TMODE_3_PW_LOCAL;
			break;
		case BUFFER_MODE_USE_BUF:
			pintf->tmode = EDIM_TMODE_2_PW_OUT;
			break;
		default:
			PR_ERR("%s:bmode[%d]\n", __func__,
			       pintf->u.dinst.parm.buffer_mode);
			break;
		}
	} else {
		PR_ERR("%s:wmode[%d]\n", __func__,
		       pintf->u.dinst.parm.work_mode);
	}

	pintf->op_dvfm_fill = dvfmv3_fill_in_ins;
	if (pintf->tmode == EDIM_TMODE_2_PW_OUT) {
		pintf->op_post_done = dimv3_post_de_done_buf_config_ins;
		pintf->op_ins_2_doing = ins_2_doing;
	} else if (pintf->tmode == EDIM_TMODE_3_PW_LOCAL) {
		pintf->op_post_done = dimv3_post_de_done_buf_config_ins_local;
		pintf->op_ins_2_doing = NULL;
	} else {
		pintf->op_ins_2_doing = NULL;
	}
	dipv3_event_reg_chst(ch);
	PR_INF("%s:ch[%d],tmode[%d]\n", __func__, ch, pintf->tmode);
	return ch;
}
EXPORT_SYMBOL(di_create_instance);

/**********************************************************
 **
 * @brief  di_set_parameter  set parameter to di for init
 *
 * @param[in]  index   instance index
 *
 * @return      0 for success, or fail type if < 0
 *
 **********************************************************/
int di_destroy_instance(int index)
{
	struct dim_inter_s *pintf;
	unsigned int ch;

	ch = index_2_ch(index);
	if (ch == ERR_INDEX) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	pintf = get_dev_intf(ch);
	if (!pintf->reg)
		PR_WARN("%s:now not unreg\n", __func__);

	pintf->reg = 0;
	dipv3_event_unreg_chst(ch);

	return 0;
}
EXPORT_SYMBOL(di_destroy_instance);

/**********************************************************
 **
 * @brief  di_empty_input_buffer  send input date to di
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 *
 * @return      Success or fail type
 **********************************************************/
enum DI_ERRORTYPE di_empty_input_buffer(int index, struct di_buffer *buffer)
{
	struct dim_inter_s *pintf;
	unsigned int ch;
	struct di_ch_s *pch;
	unsigned int buf_index = 0xff;

	ch = index_2_ch(index);
	if (ch == ERR_INDEX) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}
	pch = get_chdata(ch);
	pintf = get_dev_intf(ch);
	if (!pintf->reg) {
		PR_WARN("%s:ch[%d] not reg\n", __func__, ch);
		return DI_ERR_INDEX_NOT_ACTIVE;
	}

	#if 0
	/*que in IS_*/
	if (!qued_ops.move(pch, QUED_T_IS_FREE, QUED_T_IS_IN, &buf_index)) {
		PR_WARN("%s:ch[%d]:no space for 0x%p\n", __func__, ch, buffer);
		return DI_ERR_IN_NO_SPACE;
	}
	pintf->u.dinst.in[buf_index] = buffer;
	#else
	qued_ops.peek(pch, QUED_T_IS_FREE, &buf_index);
	if (buf_index == 0xff) {
		PR_ERR("%s:no free\n", __func__);
		return DI_ERR_IN_NO_SPACE;
	}
	qued_ops.out(pch, QUED_T_IS_FREE, &buf_index);
	pintf->u.dinst.in[buf_index] = buffer;
	qued_ops.in(pch, QUED_T_IS_IN, buf_index);

	#endif


	return DI_ERR_NONE;
}
EXPORT_SYMBOL(di_empty_input_buffer);

static enum DI_ERRORTYPE di_fill_output_buffer_mode2(struct dim_inter_s *pintf,
						     struct di_buffer *buffer)
{
	/*from QUED_T_IS_PST_FREE to QUED_T_IS_PST_DOBEF */
	struct di_ch_s *pch;
	unsigned int buf_index = 0xff;


	pch = get_chdata(pintf->ch);
	#if 0
	/*que IS_PST*/
	if (!qued_ops.move(pch,
			   QUED_T_IS_PST_FREE,
			   QUED_T_IS_PST_DOBEF, &buf_index)) {
		PR_WARN("%s:ch[%d]:no space for 0x%p\n", __func__, ch, buffer);
		return DI_ERR_IN_NO_SPACE;
	}

	pintf->u.dinst.out[buf_index] = buffer;
	#else
	qued_ops.peek(pch, QUED_T_IS_PST_FREE, &buf_index);
	if (buf_index == 0xff) {
		PR_ERR("%s: NO PST_FREE\n", __func__);
		return DI_ERR_IN_NO_SPACE;
	}
	qued_ops.out(pch, QUED_T_IS_PST_FREE, &buf_index);
	pintf->u.dinst.out[buf_index] = buffer;
	qued_ops.in(pch, QUED_T_IS_PST_DOBEF, buf_index);

	#endif
	return DI_ERR_NONE;
}

static enum DI_ERRORTYPE di_fill_output_buffer_mode3(struct dim_inter_s *pintf,
						     struct di_buffer *buffer)
{
	/*back buf to di */
	struct di_ch_s *pch;
//	unsigned int buf_index = 0xff;
	struct di_buf_s *di_buf = NULL;
//tmp	ulong irq_flag2 = 0;
	unsigned int ch;

	ch = pintf->ch;
	pch = get_chdata(ch);

	di_buf = (struct di_buf_s *)buffer->private_data;
	if (IS_ERR_OR_NULL(di_buf)) {
		pch->interf.opsi.put(buffer->vf, pch);
		PR_ERR("%s: get vframe %p without di buf\n",
		       __func__, buffer);
		return DI_ERR_NONE;
	}

	if (di_buf->type == VFRAME_TYPE_POST) {
	//	dim_lock_irqfiq_save(irq_flag2);

		/*check if in QUE_POST_DISPLAY*/
		/*di_buf->queue_index = QUEUE_DISPLAY;*/
		if (isv3_in_queue(ch, di_buf, QUEUE_DISPLAY)) {
			di_buf->queue_index = -1;
			div3_que_in(ch, QUE_POST_BACK, di_buf);
			//dim_unlock_irqfiq_restore(irq_flag2);
		} else {
			//dim_unlock_irqfiq_restore(irq_flag2);
			PR_ERR("%s:ch[%d]:not in display %d\n", __func__,
				ch,
			       di_buf->index);
		}

	} else {
	//	dim_lock_irqfiq_save(irq_flag2);
		queuev3_in(ch, di_buf, QUEUE_RECYCLE);
		//dim_unlock_irqfiq_restore(irq_flag2);
	}
	dimv3_print("%s: ch[%d];typ[%d];id[%d];omxp[%d]\n", __func__,
		  ch,
		  di_buf->type, di_buf->index,
		  di_buf->vframe->omx_index);

	return DI_ERR_NONE;
}

/**********************************************************
 * @brief  di_fill_output_buffer  send output buffer to di
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 *
 * @return      Success or fail type
 *********************************************************/
enum DI_ERRORTYPE di_fill_output_buffer(int index, struct di_buffer *buffer)
{
	struct dim_inter_s *pintf;
	unsigned int ch;
	//struct di_ch_s *pch;
	enum DI_ERRORTYPE ret = DI_ERR_NONE;

	/*check channel*/
	ch = index_2_ch(index);
	if (ch == ERR_INDEX) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	pintf = get_dev_intf(ch);
	dimv3_print("%s:ch[%d],ptf ch[%d]\n", __func__, ch, pintf->ch);
	if (!pintf->reg) {
		PR_WARN("%s:ch[%d] not reg\n", __func__, ch);
		return DI_ERR_INDEX_NOT_ACTIVE;
	}

	if (pintf->tmode == EDIM_TMODE_2_PW_OUT)
		ret = di_fill_output_buffer_mode2(pintf, buffer);
	else if (pintf->tmode == EDIM_TMODE_3_PW_LOCAL)
		ret = di_fill_output_buffer_mode3(pintf, buffer);

	return ret;
}
EXPORT_SYMBOL(di_fill_output_buffer);
