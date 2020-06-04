/*
 * drivers/amlogic/media/di_multi_v3/di_pres.c
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
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vpu/vpu.h>
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include "register.h"
#include "deinterlace.h"
#include "deinterlace_dbg.h"
#include "nr_downscale.h"


#include "di_data_l.h"
#include "di_dbg.h"
#include "di_pps.h"
#include "di_pre.h"
#include "di_prc.h"
#include "di_task.h"
#include "di_vframe.h"
#include "di_que.h"
#include "di_api.h"
#include "di_sys.h"
#include "di_pre_hw.h"

/*2018-07-18 add debugfs*/
#include <linux/seq_file.h>
#include <linux/debugfs.h>

/*
 * add dummy buffer to pre ready queue
 */
//static
void addv3_dummy_vframe_type_pre(struct di_buf_s *src_buf, unsigned int ch)
{
	struct di_buf_s *di_buf_tmp = NULL;

	if (!queuev3_empty(ch, QUEUE_LOCAL_FREE)) {
		di_buf_tmp = getv3_di_buf_head(ch, QUEUE_LOCAL_FREE);
		if (di_buf_tmp) {
			dimv3_print("addv3_dummy_vframe_type_pre\n");
			queuev3_out(ch, di_buf_tmp);
			di_buf_tmp->c.pre_ref_count = 0;
			di_buf_tmp->c.post_ref_count = 0;
			di_buf_tmp->c.post_proc_flag = 3;
			di_buf_tmp->c.new_format_flag = 0;
			di_buf_tmp->c.sts |= EDI_ST_DUMMY;
			if (!IS_ERR_OR_NULL(src_buf)) {
				#if 0
				memcpy(di_buf_tmp->vframe, src_buf->vframe,
				       sizeof(vframe_t));
				#endif
				di_buf_tmp->c.wmode = src_buf->c.wmode;
			}
			div3_que_in(ch, QUE_PRE_READY, di_buf_tmp);
		}
	}
}

#ifdef HIS_V3
void dimv3_pre_de_done_buf_config(struct di_ch_s *pch, bool flg_timeout)
{
	ulong irq_flag2 = 0;
	int tmp_cur_lev;
	struct di_buf_s *post_wr_buf = NULL;
	unsigned int glb_frame_mot_num = 0;
	unsigned int glb_field_mot_num = 0;
//	struct di_ch_s		*pch;
//	struct dim_dvfm_s	*pdvfm;
	unsigned int channel = pch->ch_id;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	dimv3_dbg_pre_cnt(channel, "d1");
	dimv3_ddbg_mod_save(eDI_DBG_MOD_PRE_DONEB,
			    channel, ppre->in_seq);/*dbg*/
	if (ppre->di_wr_buf) {
		//dim_tr_ops.pre_ready(ppre->di_wr_buf->vframe->omx_index);
		if (ppre->pre_throw_flag > 0) {
			ppre->di_wr_buf->c.throw_flag = 1;
			ppre->pre_throw_flag--;
		} else {
			ppre->di_wr_buf->c.throw_flag = 0;
		}

		ppre->di_post_wr_buf = ppre->di_wr_buf;
		post_wr_buf = ppre->di_post_wr_buf;

		if (post_wr_buf && !ppre->cur_prog_flag &&
		    !flg_timeout) {/*ary: i mode ?*/
			dimv3_read_pulldown_info(&glb_frame_mot_num,
					       &glb_field_mot_num);
			if (dimp_get(eDI_MP_pulldown_enable))
				/*pulldown_detection*/
			get_ops_pd()->detection(&post_wr_buf->c.pd_config,
						ppre->mtn_status,
						dimv3_get_overturn(),
						/*ppre->di_inp_buf->vframe*/
						&ppre->dvfm->vframe);
			/*if (combing_fix_en)*/
			//if (di_mpr(combing_fix_en)) {
			if (ppre->combing_fix_en) {
				tmp_cur_lev /*cur_lev*/
				= get_ops_mtn()->adaptive_combing_fixing(
					ppre->mtn_status,
					glb_field_mot_num,
					glb_frame_mot_num,
					dimp_get(eDI_MP_di_force_bit_mode));
				dimp_set(eDI_MP_cur_lev, tmp_cur_lev);
			}

			if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
				get_ops_nr()->adaptive_cue_adjust(
					glb_frame_mot_num,
					glb_field_mot_num);
			dimv3_pulldown_info_clear_g12a();
		}

		if (ppre->cur_prog_flag) { /*progressive*/
			if (ppre->prog_proc_type == 0) { /*p as i */
				/* di_mem_buf_dup->vfrme
				 * is either local vframe,
				 * or bot field of vframe from in_list
				 */
				ppre->di_mem_buf_dup_p->c.pre_ref_count = 0;
				ppre->di_mem_buf_dup_p
					= ppre->di_chan2_buf_dup_p;
				ppre->di_chan2_buf_dup_p
					= ppre->di_wr_buf;

			} else {
				ppre->di_mem_buf_dup_p->c.pre_ref_count = 0;
				/*recycle the progress throw buffer*/
				if (ppre->di_wr_buf->c.throw_flag) {
					ppre->di_wr_buf->
						c.pre_ref_count = 0;
					ppre->di_mem_buf_dup_p = NULL;

				} else {
					ppre->di_mem_buf_dup_p
						= ppre->di_wr_buf;
				}

			}

			ppre->di_wr_buf->c.seq
				= ppre->pre_ready_seq++;
			ppre->di_wr_buf->c.post_ref_count = 0;
			ppre->di_wr_buf->c.left_right
				= ppre->left_right;
			if (ppre->source_change_flag) {
				ppre->di_wr_buf->c.new_format_flag = 1;
				ppre->source_change_flag = 0;
			} else {
				ppre->di_wr_buf->c.new_format_flag = 0;
			}
			if (di_bypass_state_get(channel) == 1) {
				ppre->di_wr_buf->c.new_format_flag = 1;
				/*bypass_state = 0;*/
				di_bypass_state_set(channel, false);

			}
			if (ppre->di_post_wr_buf)
				div3_que_in(channel, QUE_PRE_READY,
					  ppre->di_post_wr_buf);

			if (ppre->di_wr_buf) {
				ppre->di_post_wr_buf = NULL;
				ppre->di_wr_buf = NULL;
			}
		} else {
		/*i mode*/
			ppre->di_mem_buf_dup_p->c.pre_ref_count = 0;
			ppre->di_mem_buf_dup_p = NULL;
			if (ppre->di_chan2_buf_dup_p) {
				ppre->di_mem_buf_dup_p =
					ppre->di_chan2_buf_dup_p;

			}
			ppre->di_chan2_buf_dup_p = ppre->di_wr_buf;

			if (ppre->source_change_flag) {
				/* add dummy buf, will not be displayed */
				addv3_dummy_vframe_type_pre(post_wr_buf,
							  channel);
			}
			ppre->di_wr_buf->c.seq = ppre->pre_ready_seq++;
			ppre->di_wr_buf->c.left_right = ppre->left_right;
			ppre->di_wr_buf->c.post_ref_count = 0;

			if (ppre->source_change_flag) {
				ppre->di_wr_buf->c.new_format_flag = 1;
				ppre->source_change_flag = 0;
			} else {
				ppre->di_wr_buf->c.new_format_flag = 0;
			}
			if (di_bypass_state_get(channel) == 1) {
				ppre->di_wr_buf->c.new_format_flag = 1;
				/*bypass_state = 0;*/
				di_bypass_state_set(channel, false);

			}

			if (ppre->di_post_wr_buf)
				div3_que_in(channel, QUE_PRE_READY,
					  ppre->di_post_wr_buf);

			dimv3_print("%s: %s[%d] => pre_ready_list\n", __func__,
				  vframe_type_name[ppre->di_wr_buf->type],
				  ppre->di_wr_buf->index);

			if (ppre->di_wr_buf) {
				ppre->di_post_wr_buf = NULL;

				ppre->di_wr_buf = NULL;
			}
		}
	}

	if (ppre->di_inp_buf) {
		di_lock_irqfiq_save(irq_flag2);
		queuev3_in(channel, ppre->di_inp_buf, QUEUE_RECYCLE);
		ppre->di_inp_buf = NULL;
		di_unlock_irqfiq_restore(irq_flag2);
	}
	dimv3_print("%s\n", __func__);
	#ifdef HIS_V3
	if (ppre->dvfm) { /*ary tmp : dvfm recycle*/
		pch = get_chdata(channel);
		pdvfm = dvfmv3_get(pch, QUED_T_PRE);
		if (ppre->dvfm->index != pdvfm->index) {
			PR_ERR("%s:not map:%d->%d\n", __func__,
			       ppre->dvfm->index,
			       pdvfm->index);
			return;
		}

		qued_ops.in(pch, QUED_T_RECYCL, ppre->dvfm->index);
		ppre->dvfm = NULL;
	} else{
		dimv3_print("%s:warn:no dvfm\n", __func__);
	}
	#else

	#endif
	dimv3_ddbg_mod_save(eDI_DBG_MOD_PRE_DONEE,
			    channel, ppre->in_seq); /*dbg*/

	dimv3_dbg_pre_cnt(channel, "d2");
}
#endif
