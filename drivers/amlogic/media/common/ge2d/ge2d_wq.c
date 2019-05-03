/*
 * drivers/amlogic/media/common/ge2d/ge2d_wq.c
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

#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/delay.h>

/* Amlogic Headers */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/platform_device.h>
#include <meson_ion.h>

/* Local Headers */
#include "ge2dgen.h"
#include "ge2d_log.h"
#include "ge2d_io.h"
#include "ge2d_reg.h"
#include "ge2d_wq.h"
#include "ge2d_dmabuf.h"

#include "osd_io.h"
#include "osd_hw.h"

#define OSD1_CANVAS_INDEX 0x40
#define OSD2_CANVAS_INDEX 0x43
#define OSD3_CANVAS_INDEX 0x41
#define OSD4_CANVAS_INDEX 0x42
#define ALLOC_CANVAS_INDEX 0x44

static struct ge2d_manager_s ge2d_manager;
static int ge2d_irq = -ENXIO;
static struct clk *ge2d_clk;

static const int bpp_type_lut[] = {
	/* 16bit */
	COLOR_INDEX_16_655,	/* 0 */
	COLOR_INDEX_16_844,	/* 1 */
	COLOR_INDEX_16_6442,	/* 2 */
	COLOR_INDEX_16_4444_R,	/* 3 */
	COLOR_INDEX_16_565,		/* 4 */
	COLOR_INDEX_16_4444_A,	/* 5 */
	COLOR_INDEX_16_1555_A,	/* 6 */
	COLOR_INDEX_16_4642_R,	/* 7 */
	/* 24bit */
	COLOR_INDEX_24_RGB,	/* 0 */
	COLOR_INDEX_24_5658,	/* 1 */
	COLOR_INDEX_24_8565,	/* 2 */
	COLOR_INDEX_24_6666_R,	/* 3 */
	COLOR_INDEX_24_6666_A,	/* 4 */
	COLOR_INDEX_24_888_B,	/* 5 */
	0,
	0,
	/* 32bit */
	COLOR_INDEX_32_RGBA,	/* 0 */
	COLOR_INDEX_32_ARGB,	/* 1 */
	COLOR_INDEX_32_ABGR,	/* 2 */
	COLOR_INDEX_32_BGRA,	/* 3 */
	0, 0, 0, 0
};

static const int default_ge2d_color_lut[] = {
	0,
	0,
	0,/* BPP_TYPE_02_PAL4    = 2, */
	0,
	0,/* BPP_TYPE_04_PAL16   = 4, */
	0,
	0,
	0,
	0,/* BPP_TYPE_08_PAL256=8, */
	GE2D_FORMAT_S16_RGB_655,/* BPP_TYPE_16_655 =9, */
	GE2D_FORMAT_S16_RGB_844,/* BPP_TYPE_16_844 =10, */
	GE2D_FORMAT_S16_RGBA_6442,/* BPP_TYPE_16_6442 =11 , */
	GE2D_FORMAT_S16_RGBA_4444,/* BPP_TYPE_16_4444_R = 12, */
	GE2D_FORMAT_S16_RGBA_4642,/* BPP_TYPE_16_4642_R = 13, */
	GE2D_FORMAT_S16_ARGB_1555,/* BPP_TYPE_16_1555_A=14, */
	GE2D_FORMAT_S16_ARGB_4444,/* BPP_TYPE_16_4444_A = 15, */
	GE2D_FORMAT_S16_RGB_565,/* BPP_TYPE_16_565 =16, */
	0,
	0,
	GE2D_FORMAT_S24_ARGB_6666,/* BPP_TYPE_24_6666_A=19, */
	GE2D_FORMAT_S24_RGBA_6666,/* BPP_TYPE_24_6666_R=20, */
	GE2D_FORMAT_S24_ARGB_8565,/* BPP_TYPE_24_8565 =21, */
	GE2D_FORMAT_S24_RGBA_5658,/* BPP_TYPE_24_5658 = 22, */
	GE2D_FORMAT_S24_BGR,/* BPP_TYPE_24_888_B = 23, */
	GE2D_FORMAT_S24_RGB,/* BPP_TYPE_24_RGB = 24, */
	0,
	0,
	0,
	0,
	GE2D_FORMAT_S32_BGRA,/* BPP_TYPE_32_BGRA=29, */
	GE2D_FORMAT_S32_ABGR,/* BPP_TYPE_32_ABGR = 30, */
	GE2D_FORMAT_S32_RGBA,/* BPP_TYPE_32_RGBA=31, */
	GE2D_FORMAT_S32_ARGB,/* BPP_TYPE_32_ARGB=32, */
};

static void ge2d_pre_init(void)
{
	struct ge2d_gen_s ge2d_gen_cfg;

	ge2d_gen_cfg.interrupt_ctrl = 0x02;
	ge2d_gen_cfg.dp_on_cnt       = 0;
	ge2d_gen_cfg.dp_off_cnt      = 0;
	ge2d_gen_cfg.dp_onoff_mode   = 0;
	ge2d_gen_cfg.vfmt_onoff_en   = 0;
	/*  fifo size control, 00: 512, 01: 256, 10: 128 11: 96 */
	ge2d_gen_cfg.fifo_size = 0;
	/* fifo burst control, 00: 24x64, 01: 32x64
	 * 10: 48x64, 11:64x64
	 */
	ge2d_gen_cfg.burst_ctrl = 0;
	ge2d_set_gen(&ge2d_gen_cfg);
}

static int ge2d_clk_config(bool enable)
{
	if (ge2d_clk == NULL)
		return -1;
	if (enable)
		clk_prepare_enable(ge2d_clk);
	else
		clk_disable_unprepare(ge2d_clk);

	return 0;
}


void ge2d_pwr_config(bool enable)
{
	int i, table_size;
	struct ge2d_ctrl_s tmp;
	struct ge2d_ctrl_s *power_table;

	if (ge2d_meson_dev.has_self_pwr) {
		if (enable) {
			power_table = ge2d_meson_dev.poweron_table->power_table;
			table_size = ge2d_meson_dev.poweron_table->table_size;
		} else {
			power_table =
				ge2d_meson_dev.poweroff_table->power_table;
			table_size = ge2d_meson_dev.poweroff_table->table_size;
		}

		for (i = 0; i < table_size; i++) {
			tmp = power_table[i];
			ge2d_set_pwr_tbl_bits(tmp.table_type, tmp.reg, tmp.val,
					tmp.start, tmp.len);
			if (tmp.udelay > 0)
				udelay(tmp.udelay);
		}
	}

	ge2d_clk_config(enable);

	if (enable) {
		ge2d_soft_rst();
		ge2d_pre_init();
	}
}

static int get_queue_member_count(struct list_head *head)
{
	int member_count = 0;

	struct ge2d_queue_item_s *pitem;

	list_for_each_entry(pitem, head, list) {
		member_count++;
		if (member_count > MAX_GE2D_CMD) /* error has occurred */
			break;
	}

	return member_count;
}

ssize_t work_queue_status_show(struct class *cla, struct class_attribute *attr,
			       char *buf)
{
	struct ge2d_context_s *wq = ge2d_manager.current_wq;

	if (wq == 0)
		return 0;
	return snprintf(buf, 40, "cmd count in queue:%d\n",
			get_queue_member_count(&wq->work_queue));
}

ssize_t free_queue_status_show(struct class *cla, struct class_attribute *attr,
			       char *buf)
{
	struct ge2d_context_s *wq = ge2d_manager.current_wq;

	if (wq == 0)
		return 0;
	return snprintf(buf, 40, "free space :%d\n",
			get_queue_member_count(&wq->free_queue));
}

static inline  int  work_queue_no_space(struct ge2d_context_s *queue)
{
	return  list_empty(&queue->free_queue);
}

static void ge2d_dump_cmd(struct ge2d_cmd_s *cfg)
{
	ge2d_log_dbg("src1_x_start=%d,src1_y_start=%d\n",
		cfg->src1_x_start, cfg->src1_y_start);
	ge2d_log_dbg("src1_x_end=%d,src1_y_end=%d\n",
		cfg->src1_x_end, cfg->src1_y_end);
	ge2d_log_dbg("src1_x_rev=%d,src1_y_rev=%d\n",
		cfg->src1_x_rev, cfg->src1_y_rev);
	ge2d_log_dbg("src1_fill_color_en=%d\n",
		cfg->src1_fill_color_en);

	ge2d_log_dbg("src2_x_start=%d,src2_y_start=%d\n",
		cfg->src2_x_start, cfg->src2_y_start);
	ge2d_log_dbg("src2_x_end=%d,src2_y_end=%d\n",
		cfg->src2_x_end, cfg->src2_y_end);
	ge2d_log_dbg("src2_x_rev=%d,src2_y_rev=%d\n",
		cfg->src2_x_rev, cfg->src2_y_rev);
	ge2d_log_dbg("src2_fill_color_en=%d\n",
		cfg->src2_fill_color_en);

	ge2d_log_dbg("dst_x_start=%d,dst_y_start=%d\n",
		cfg->dst_x_start, cfg->dst_y_start);
	ge2d_log_dbg("dst_x_end=%d,dst_y_end=%d\n",
		cfg->dst_x_end, cfg->dst_y_end);
	ge2d_log_dbg("dst_x_rev=%d,dst_y_rev=%d\n",
		cfg->dst_x_rev, cfg->dst_y_rev);
	ge2d_log_dbg("dst_xy_swap=%d\n",
		cfg->dst_xy_swap);

	ge2d_log_dbg("color_blend_mode=0x%x\n",
		cfg->color_blend_mode);
	ge2d_log_dbg("color_src_blend_factor=0x%x\n",
		cfg->color_src_blend_factor);
	ge2d_log_dbg("color_dst_blend_factor=0x%x\n",
		cfg->color_dst_blend_factor);
	ge2d_log_dbg("color_logic_op=0x%x\n",
		cfg->color_logic_op);
	ge2d_log_dbg("alpha_blend_mode=0x%x\n",
		cfg->alpha_blend_mode);
	ge2d_log_dbg("alpha_src_blend_factor=0x%x\n",
		cfg->alpha_src_blend_factor);
	ge2d_log_dbg("alpha_src_blend_factor=0x%x\n",
		cfg->alpha_dst_blend_factor);
	ge2d_log_dbg("alpha_logic_op=0x%x\n",
		cfg->alpha_logic_op);

	ge2d_log_dbg("sc_prehsc_en=%d\n", cfg->sc_prehsc_en);
	ge2d_log_dbg("sc_prevsc_en=%d\n", cfg->sc_prevsc_en);
	ge2d_log_dbg("sc_hsc_en=%d\n", cfg->sc_hsc_en);
	ge2d_log_dbg("sc_vsc_en=%d\n", cfg->sc_vsc_en);
	ge2d_log_dbg("vsc_phase_step=%d\n", cfg->vsc_phase_step);
	ge2d_log_dbg("vsc_phase_slope=%d\n", cfg->vsc_phase_slope);
	ge2d_log_dbg("vsc_rpt_l0_num=%d\n", cfg->vsc_rpt_l0_num);
	ge2d_log_dbg("vsc_ini_phase=%d\n", cfg->vsc_ini_phase);
	ge2d_log_dbg("hsc_phase_step=%d\n", cfg->hsc_phase_step);
	ge2d_log_dbg("hsc_phase_slope=%d\n", cfg->hsc_phase_slope);
	ge2d_log_dbg("hsc_rpt_p0_num=%d\n", cfg->hsc_rpt_p0_num);
	ge2d_log_dbg("hsc_ini_phase=%d\n", cfg->hsc_ini_phase);
	ge2d_log_dbg("hsc_div_en=%d\n", cfg->hsc_div_en);
	ge2d_log_dbg("hsc_div_length=%d\n", cfg->hsc_div_length);
	ge2d_log_dbg("hsc_adv_num=%d\n", cfg->hsc_adv_num);
	ge2d_log_dbg("hsc_adv_phase=%d\n", cfg->hsc_adv_phase);
	ge2d_log_dbg("src1_cmult_asel=%d\n", cfg->src1_cmult_asel);
	ge2d_log_dbg("src2_cmult_asel=%d\n", cfg->src2_cmult_asel);

	ge2d_log_dbg("GE2D_STATUS0=0x%x\n", ge2d_reg_read(GE2D_STATUS0));
	ge2d_log_dbg("GE2D_STATUS1=0x%x\n", ge2d_reg_read(GE2D_STATUS1));
}

static int ge2d_process_work_queue(struct ge2d_context_s *wq)
{
	struct ge2d_config_s *cfg;
	struct ge2d_queue_item_s *pitem;
	unsigned int  mask = 0x1;
	struct list_head  *head = &wq->work_queue, *pos;
	int ret = 0;
	unsigned int block_mode;
	int timeout = 0;
	if (wq->ge2d_request_exit)
		goto exit;
	ge2d_manager.ge2d_state = GE2D_STATE_RUNNING;
	pos = head->next;
	if (pos != head) { /* current work queue not empty. */
		if (wq != ge2d_manager.last_wq) { /* maybe */
			/* modify the first item . */
			pitem = (struct ge2d_queue_item_s *)pos;
			if (pitem)
				pitem->config.update_flag = UPDATE_ALL;
			else {
				ge2d_log_err("can't get pitem\n");
				ret = -1;
				goto  exit;
			}
		} else
			/* modify the first item . */
			pitem = (struct ge2d_queue_item_s *)pos;

	} else {
		ret = -1;
		goto  exit;
	}

	do {
		cfg = &pitem->config;
		mask = 0x1;
		while (cfg->update_flag && mask <= UPDATE_SCALE_COEF) {
			/* we do not change */
			switch (cfg->update_flag & mask) {
			case UPDATE_SRC_DATA:
				ge2d_set_src1_data(&cfg->src1_data);
				break;
			case UPDATE_SRC_GEN:
				ge2d_set_src1_gen(&cfg->src1_gen);
				break;
			case UPDATE_DST_DATA:
				ge2d_set_src2_dst_data(&cfg->src2_dst_data);
				break;
			case UPDATE_DST_GEN:
				ge2d_set_src2_dst_gen(&cfg->src2_dst_gen);
				break;
			case UPDATE_DP_GEN:
				ge2d_set_dp_gen(&cfg->dp_gen);
				break;
			case UPDATE_SCALE_COEF:
				ge2d_set_src1_scale_coef(cfg->v_scale_coef_type,
						cfg->h_scale_coef_type);
				break;
			}

			cfg->update_flag &= ~mask;
			mask = mask << 1;

		}
		pitem->cmd.hang_flag = 1;
		ge2d_set_cmd(&pitem->cmd);/* set START_FLAG in this func. */
		/* remove item */
		block_mode = pitem->cmd.wait_done_flag;
		/* spin_lock(&wq->lock); */
		/* pos=pos->next; */
		/* list_move_tail(&pitem->list,&wq->free_queue); */
		/* spin_unlock(&wq->lock); */

		while (ge2d_is_busy()) {
			timeout = wait_event_interruptible_timeout(
					ge2d_manager.event.cmd_complete,
					!ge2d_is_busy(),
					msecs_to_jiffies(1000));
			if (timeout == 0) {
				ge2d_log_err("ge2d timeout!!!\n");
				ge2d_dump_cmd(&pitem->cmd);
				ge2d_soft_rst();
				break;
			}
		}
		/* if block mode (cmd) */
		if (block_mode) {
			pitem->cmd.wait_done_flag = 0;
			wake_up_interruptible(&wq->cmd_complete);
		}
		spin_lock(&wq->lock);
		pos = pos->next;
		list_move_tail(&pitem->list, &wq->free_queue);
		spin_unlock(&wq->lock);
		/* if dma buf detach it */
		if (pitem->config.src_dma_cfg.dma_used) {
			ge2d_dma_buffer_unmap((struct aml_dma_cfg *
				)pitem->config.src_dma_cfg.dma_cfg);
			pitem->config.src_dma_cfg.dma_used = 0;
			kfree(pitem->config.src_dma_cfg.dma_cfg);
		}
		if (pitem->config.src2_dma_cfg.dma_used) {
			ge2d_dma_buffer_unmap((struct aml_dma_cfg *
				)pitem->config.src2_dma_cfg.dma_cfg);
			pitem->config.src2_dma_cfg.dma_used = 0;
			kfree(pitem->config.src2_dma_cfg.dma_cfg);
		}
		if (pitem->config.dst_dma_cfg.dma_used) {
			ge2d_dma_buffer_unmap((struct aml_dma_cfg *
				)pitem->config.dst_dma_cfg.dma_cfg);
			pitem->config.dst_dma_cfg.dma_used = 0;
			kfree(pitem->config.dst_dma_cfg.dma_cfg);
		}
		pitem = (struct ge2d_queue_item_s *)pos;
	} while (pos != head);
	ge2d_manager.last_wq = wq;
exit:
	if (wq->ge2d_request_exit)
		complete(&ge2d_manager.event.process_complete);
	ge2d_manager.ge2d_state = GE2D_STATE_IDLE;
	return ret;
}

static irqreturn_t ge2d_wq_handle(int irq_number, void *para)
{
	wake_up(&ge2d_manager.event.cmd_complete);
	return IRQ_HANDLED;
}

struct ge2d_dma_cfg_s *ge2d_wq_get_dma_cfg(struct ge2d_context_s *wq,
					   unsigned int data_type)
{
	struct ge2d_dma_cfg_s *dma_cfg = NULL;

	switch (data_type) {
	case AML_GE2D_SRC:
		dma_cfg = &wq->config.src_dma_cfg;
		break;
	case AML_GE2D_SRC2:
		dma_cfg = &wq->config.src2_dma_cfg;
		break;
	case AML_GE2D_DST:
		dma_cfg = &wq->config.dst_dma_cfg;
		break;
	default:
		ge2d_log_err("wrong data_type\n");
		break;
	}

	return dma_cfg;
}

struct ge2d_src1_data_s *ge2d_wq_get_src_data(struct ge2d_context_s *wq)
{
	return &wq->config.src1_data;
}

struct ge2d_src1_gen_s *ge2d_wq_get_src_gen(struct ge2d_context_s *wq)
{
	return &wq->config.src1_gen;
}

struct ge2d_src2_dst_data_s *ge2d_wq_get_dst_data(struct ge2d_context_s *wq)
{
	return &wq->config.src2_dst_data;
}

struct ge2d_src2_dst_gen_s *ge2d_wq_get_dst_gen(struct ge2d_context_s *wq)
{
	return &wq->config.src2_dst_gen;
}

struct ge2d_dp_gen_s *ge2d_wq_get_dp_gen(struct ge2d_context_s *wq)
{
	return &wq->config.dp_gen;
}

struct ge2d_cmd_s *ge2d_wq_get_cmd(struct ge2d_context_s *wq)
{
	return &wq->cmd;
}

void ge2d_wq_set_scale_coef(struct ge2d_context_s *wq,
			    unsigned int v_scale_coef_type,
			    unsigned int h_scale_coef_type)
{

	if (wq) {
		wq->config.v_scale_coef_type = v_scale_coef_type;
		wq->config.h_scale_coef_type = h_scale_coef_type;
		wq->config.update_flag |= UPDATE_SCALE_COEF;
	}
}

int ge2d_wq_add_work(struct ge2d_context_s *wq)
{

	struct ge2d_queue_item_s  *pitem;

	ge2d_log_dbg("add new work @@%s:%d\n", __func__, __LINE__);
	if (work_queue_no_space(wq)) {
		ge2d_log_dbg("work queue no space\n");
		/* we should wait for queue empty at this point. */
		#if 0
		while (work_queue_no_space(wq))
			interruptible_sleep_on_timeout(
					&ge2d_manager.event.cmd_complete, 3);
		#endif
		return -1;
	}

	pitem = list_entry(wq->free_queue.next, struct ge2d_queue_item_s, list);
	if (IS_ERR(pitem))
		goto error;
	memcpy(&pitem->cmd, &wq->cmd, sizeof(struct ge2d_cmd_s));
	memset(&wq->cmd, 0, sizeof(struct ge2d_cmd_s));
	memcpy(&pitem->config, &wq->config, sizeof(struct ge2d_config_s));
	wq->config.update_flag = 0; /* reset config set flag */
	spin_lock(&wq->lock);
	list_move_tail(&pitem->list, &wq->work_queue);
	spin_unlock(&wq->lock);
	ge2d_log_dbg("add new work ok\n");
	/* only read not need lock */
	if (ge2d_manager.event.cmd_in_sem.count == 0)
		up(&ge2d_manager.event.cmd_in_sem);/* new cmd come in */
	/* add block mode   if() */
	if (pitem->cmd.wait_done_flag) {
		wait_event_interruptible(wq->cmd_complete,
				pitem->cmd.wait_done_flag == 0);
		/* interruptible_sleep_on(&wq->cmd_complete); */
	}
	return 0;
error:
	return -1;
}

static inline struct ge2d_context_s *get_next_work_queue(
		struct ge2d_manager_s *manager)
{
	struct ge2d_context_s *pcontext;

	spin_lock(&ge2d_manager.event.sem_lock);
	list_for_each_entry(pcontext, &manager->process_queue, list) {
		/* not lock maybe delay to next time. */
		if (!list_empty(&pcontext->work_queue)) {
			/* move head . */
			list_move(&manager->process_queue, &pcontext->list);
			spin_unlock(&ge2d_manager.event.sem_lock);
			return pcontext;
		}
	}
	spin_unlock(&ge2d_manager.event.sem_lock);
	return NULL;
}

static int ge2d_monitor_thread(void *data)
{
	int ret;
	struct ge2d_manager_s *manager = (struct ge2d_manager_s *)data;

	ge2d_log_info("ge2d workqueue monitor start\n");
	/* setup current_wq here. */
	while (ge2d_manager.process_queue_state != GE2D_PROCESS_QUEUE_STOP) {
		ret = down_interruptible(&manager->event.cmd_in_sem);
		ge2d_pwr_config(true);
		while ((manager->current_wq =
				get_next_work_queue(manager)) != NULL)
			ge2d_process_work_queue(manager->current_wq);
		if (!ge2d_dump_reg_enable)
			ge2d_pwr_config(false);
	}
	ge2d_log_info("exit ge2d_monitor_thread\n");
	return 0;
}

static int ge2d_start_monitor(void)
{
	int ret = 0;

	ge2d_log_info("ge2d start monitor\n");
	ge2d_manager.process_queue_state = GE2D_PROCESS_QUEUE_START;
	ge2d_manager.ge2d_thread = kthread_run(ge2d_monitor_thread,
					       &ge2d_manager,
					       "ge2d_monitor");
	if (IS_ERR(ge2d_manager.ge2d_thread)) {
		ret = PTR_ERR(ge2d_manager.ge2d_thread);
		ge2d_log_err("ge2d failed to start kthread (%d)\n", ret);
	}
	return ret;
}

static int ge2d_stop_monitor(void)
{
	ge2d_log_info("stop ge2d monitor thread\n");
	if (ge2d_manager.ge2d_thread) {
		ge2d_manager.process_queue_state = GE2D_PROCESS_QUEUE_STOP;
		up(&ge2d_manager.event.cmd_in_sem);
		kthread_stop(ge2d_manager.ge2d_thread);
		ge2d_manager.ge2d_thread = NULL;
	}
	return  0;
}

static inline int bpp(unsigned int format)
{
	switch (format & GE2D_BPP_MASK) {
	case GE2D_BPP_8BIT:
		return 8;
	case GE2D_BPP_16BIT:
		return 16;
	case GE2D_BPP_24BIT:
		if ((GE2D_COLOR_MAP_NV21 == (format & GE2D_COLOR_MAP_NV21)) ||
			(GE2D_COLOR_MAP_NV12 == (format & GE2D_COLOR_MAP_NV12)))
			return 8;
		return 24;
	case GE2D_BPP_32BIT:
	default:
		return 32;
	}
}

static void build_ge2d_config(struct config_para_s *cfg,
			      struct src_dst_para_s *src,
			      struct src_dst_para_s *dst,
			      int index)
{
	index &= 0xff;
	if (src) {
		src->xres = cfg->src_planes[0].w;
		src->yres = cfg->src_planes[0].h;
		src->ge2d_color_index = cfg->src_format;
		src->bpp = bpp(cfg->src_format);
		if (cfg->src_planes[0].addr) {
			if (ge2d_meson_dev.canvas_status == 1) {
				src->canvas_index = 0;
				src->phy_addr = cfg->src_planes[0].addr;
				src->stride = cfg->src_planes[0].w *
					src->bpp / 8;
			}
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			else {
				src->canvas_index = index;
				canvas_config(index++,
					cfg->src_planes[0].addr,
					cfg->src_planes[0].w * src->bpp / 8,
					cfg->src_planes[0].h,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
			}
#endif
		}
		/* multi-src_planes */
		if (cfg->src_planes[1].addr) {
			if (ge2d_meson_dev.canvas_status == 1)
				ge2d_log_info("ge2d not support src_planes[1]\n");
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			else {
				src->canvas_index |= index << 8;
				canvas_config(index++,
					cfg->src_planes[1].addr,
					cfg->src_planes[1].w * src->bpp / 8,
					cfg->src_planes[1].h,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
			}
#endif
		}
		if (cfg->src_planes[2].addr) {
			if (ge2d_meson_dev.canvas_status == 1)
				ge2d_log_info("ge2d not support src_planes[2]\n");
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			else {
				src->canvas_index |= index << 16;
				canvas_config(index++,
					cfg->src_planes[2].addr,
					cfg->src_planes[2].w * src->bpp / 8,
					cfg->src_planes[2].h,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
			}
#endif
		}
		if (cfg->src_planes[3].addr) {
			if (ge2d_meson_dev.canvas_status == 1)
				ge2d_log_info("ge2d not support src_planes[3]\n");
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			else {
				src->canvas_index |= index << 24;
				canvas_config(index++,
					cfg->src_planes[3].addr,
					cfg->src_planes[3].w * src->bpp / 8,
					cfg->src_planes[3].h,
					CANVAS_ADDR_NOWRAP,
					CANVAS_BLKMODE_LINEAR);
			}
#endif
		}
	}
	if (dst) {
		dst->xres = cfg->dst_planes[0].w;
		dst->yres = cfg->dst_planes[0].h;
		dst->ge2d_color_index = cfg->dst_format;
		dst->bpp = bpp(cfg->dst_format);
		if (cfg->dst_planes[0].addr) {
			if (ge2d_meson_dev.canvas_status == 1) {
				dst->canvas_index = 0;
				dst->phy_addr = cfg->dst_planes[0].addr;
				dst->stride = cfg->dst_planes[0].w *
					dst->bpp / 8;
			}
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			else {
				dst->canvas_index = index;
				canvas_config(index++ & 0xff,
					      cfg->dst_planes[0].addr,
					      cfg->dst_planes[0].w,
					      cfg->dst_planes[0].h,
					      CANVAS_ADDR_NOWRAP,
					      CANVAS_BLKMODE_LINEAR);
			}
#endif
		}
		/* multi-src_planes */
		if (cfg->dst_planes[1].addr) {
			if (ge2d_meson_dev.canvas_status == 1)
				ge2d_log_info("ge2d not support dst_planes[1]\n");
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			else {
				dst->canvas_index |= index << 8;
				canvas_config(index++,
					      cfg->dst_planes[1].addr,
					      cfg->dst_planes[1].w,
					      cfg->dst_planes[1].h,
					      CANVAS_ADDR_NOWRAP,
					      CANVAS_BLKMODE_LINEAR);
			}
#endif
		}
		if (cfg->dst_planes[2].addr) {
			if (ge2d_meson_dev.canvas_status == 1)
				ge2d_log_info("ge2d not support dst_planes[2]\n");
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			else {
				dst->canvas_index |= index << 16;
				canvas_config(index++,
					      cfg->dst_planes[2].addr,
					      cfg->dst_planes[2].w,
					      cfg->dst_planes[2].h,
					      CANVAS_ADDR_NOWRAP,
					      CANVAS_BLKMODE_LINEAR);
			}
#endif
		}
		if (cfg->dst_planes[3].addr) {
			if (ge2d_meson_dev.canvas_status == 1)
				ge2d_log_info("ge2d not support dst_planes[3]\n");
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			else {
				dst->canvas_index |= index << 24;
				canvas_config(index++,
					      cfg->dst_planes[3].addr,
					      cfg->dst_planes[3].w,
					      cfg->dst_planes[3].h,
					      CANVAS_ADDR_NOWRAP,
					      CANVAS_BLKMODE_LINEAR);
			}
#endif
		}
	}
}

static int setup_display_property(struct src_dst_para_s *src_dst, int index)
{
#define REG_OFFSET (0x20)
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
	struct canvas_s	canvas;
#endif
	u32 cs_width = 0, cs_height = 0, cs_addr = 0;
	unsigned	int	data32;
	unsigned	int	bpp;
	unsigned int	block_mode[] = {2, 4, 8, 16, 16, 32, 0, 24};

	src_dst->canvas_index = index;
	if (ge2d_meson_dev.canvas_status == 0) {
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
		canvas_read(index, &canvas);
		cs_width = canvas.width;
		cs_height = canvas.height;
#else
		cs_width = 0;
		cs_height = 0;
#endif
	}
	index = (index == OSD1_CANVAS_INDEX ? 0 : 1);
	ge2d_log_dbg("osd%d ", index);
#ifdef CONFIG_AMLOGIC_MEDIA_FB
	data32 = VSYNCOSD_RD_MPEG_REG(
		VIU_OSD1_BLK0_CFG_W0 + REG_OFFSET * index);
	src_dst->canvas_index = (data32 >> 16) & 0xff;
	if (ge2d_meson_dev.canvas_status == 1) {
		src_dst->canvas_index = 0;
		osd_get_info(index, &cs_addr,
			&cs_width, &cs_height);
		src_dst->phy_addr = cs_addr;
		src_dst->stride = cs_width;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
	else {
		canvas_read(src_dst->canvas_index, &canvas);
		cs_width = canvas.width;
		cs_height = canvas.height;
	}
#endif
#else
	data32 = 0;
#endif
	index = (data32 >> 8) & 0xf;
	bpp = block_mode[index]; /* OSD_BLK_MODE[8..11] */
	ge2d_log_dbg("%d bpp\n", bpp);

	if (bpp < 16)
		return -1;

	src_dst->bpp = bpp;
	src_dst->xres = cs_width / (bpp >> 3);
	src_dst->yres = cs_height;
	if (index == 3) /* yuv422 32bit for two pixel. */
		src_dst->ge2d_color_index =	GE2D_FORMAT_S16_YUV422;
	else { /* for block mode=4,5,7 */
		/* color mode [2..5] */
		index = bpp - 16 + ((data32 >> 2) & 0xf);
		index = bpp_type_lut[index]; /* get color mode */
		/* get matched ge2d color mode. */
		src_dst->ge2d_color_index = default_ge2d_color_lut[index];

		if (src_dst->xres <= 0 ||
			src_dst->yres <= 0 ||
			src_dst->ge2d_color_index == 0)
			return -2;
	}

	return 0;

}

int ge2d_antiflicker_enable(struct ge2d_context_s *context,
		unsigned long enable)
{
	/*
	 * antiflicker used in cvbs mode, if antiflicker is enabled,
	 * it represent that we want this feature be enabled for all ge2d work
	 */
	struct ge2d_context_s *pcontext;

	spin_lock(&ge2d_manager.event.sem_lock);
	list_for_each_entry(pcontext, &ge2d_manager.process_queue, list) {
		ge2dgen_antiflicker(pcontext, enable);
	}
	spin_unlock(&ge2d_manager.event.sem_lock);

	return 0;
}

int ge2d_context_config(struct ge2d_context_s *context,
		struct config_para_s *ge2d_config)
{
	struct src_dst_para_s src, dst, tmp;
	int type = ge2d_config->src_dst_type;

	ge2d_log_dbg(" ge2d init\n");
	memset(&src, 0, sizeof(struct src_dst_para_s));
	memset(&dst, 0, sizeof(struct src_dst_para_s));
	/* setup src and dst */
	switch (type) {
	case OSD0_OSD0:
	case OSD0_OSD1:
	case OSD1_OSD0:
	case ALLOC_OSD0:
		if (setup_display_property(&src, OSD1_CANVAS_INDEX) < 0)
			return -1;
		break;
	default:
		break;
	}
	switch (type) {
	case OSD0_OSD1:
	case OSD1_OSD1:
	case OSD1_OSD0:
	case ALLOC_OSD1:
		if (setup_display_property(&dst, OSD2_CANVAS_INDEX) < 0)
			return -1;
		break;
	case ALLOC_ALLOC:
	default:
		break;
	}
	ge2d_log_dbg("OSD ge2d type %d\n", type);
	switch (type) {
	case OSD0_OSD0:
		dst = src;
		break;
	case OSD0_OSD1:
		break;
	case OSD1_OSD1:
		src = dst;
		break;
	case OSD1_OSD0:
		tmp = src;
		src = dst;
		dst = tmp;
		break;
	case ALLOC_OSD0:
		dst = src;
		build_ge2d_config(ge2d_config, &src, NULL, ALLOC_CANVAS_INDEX);
		break;
	case ALLOC_OSD1:
		build_ge2d_config(ge2d_config, &src, NULL, ALLOC_CANVAS_INDEX);
		break;
	case ALLOC_ALLOC:
		build_ge2d_config(ge2d_config, &src, &dst, ALLOC_CANVAS_INDEX);
		break;
	}
	if (src.bpp < 16 || dst.bpp < 16)
		ge2d_log_dbg("src dst bpp type, src=%d,dst=%d\n",
				src.bpp, dst.bpp);

	/* next will config regs */
	ge2d_log_dbg("ge2d src.xres %d src.yres %d, dst.xres %d dst.yres %d\n",
		     src.xres, src.yres, dst.xres, dst.yres);
	ge2d_log_dbg("src_format: 0x%x, dst_format: 0x%x\n",
		     src.ge2d_color_index, dst.ge2d_color_index);

	ge2dgen_src(context, src.canvas_index,
		src.ge2d_color_index,
		src.phy_addr,
		src.stride);
	ge2dgen_src_clip(context,
			 0, 0, src.xres, src.yres);
	ge2dgen_src2(context, dst.canvas_index,
		dst.ge2d_color_index,
		dst.phy_addr,
		dst.stride);
	ge2dgen_src2_clip(context,
			  0, 0,  dst.xres, dst.yres);
	ge2dgen_const_color(context, ge2d_config->alu_const_color);
	ge2dgen_dst(context, dst.canvas_index,
		dst.ge2d_color_index,
		dst.phy_addr,
		dst.stride);
	ge2dgen_dst_clip(context,
			 0, 0, dst.xres, dst.yres, DST_CLIP_MODE_INSIDE);
	return  0;

}

static int build_ge2d_addr_config(
			struct config_planes_s *plane,
			unsigned int format,
			unsigned int *addr,
			unsigned int *stride
			)
{
	int ret = -1;
	int bpp_value = bpp(format);

	bpp_value /= 8;
	ge2d_log_dbg("build_ge2d_addr_config bpp_value=%d\n",
		bpp_value);
	if (plane) {
		if (plane[0].addr) {
			*addr = plane[0].addr;
			*stride = plane[0].w * bpp_value;
			ret = 0;
		}
		/* not support multi-src_planes */
		if ((plane[1].addr) ||
			(plane[2].addr) ||
			(plane[3].addr)) {
			ge2d_log_info("ge2d not support NV21 mode\n");
			ret = -1;
		}
	}
	return ret;
}

static int build_ge2d_addr_config_ion(
			struct config_planes_ion_s *plane,
			unsigned int format,
			unsigned int *addr,
			unsigned int *stride
			)
{
	int ret = -1;
	int bpp_value = bpp(format);
	unsigned long addr_temp = 0;

	bpp_value /= 8;
	ge2d_log_dbg("build_ge2d_addr_config_ion bpp_value=%d\n",
		bpp_value);
	if (plane) {
		if (plane[0].shared_fd) {
#ifdef CONFIG_AMLOGIC_ION
			size_t len = 0;

			ret = meson_ion_share_fd_to_phys(ge2d_ion_client,
				plane[0].shared_fd, &addr_temp, &len);
			if (ret != 0)
				return ret;
#else
			return ret;
#endif
		}
		plane[0].addr += addr_temp;
		if (plane[0].addr) {
			*addr = plane[0].addr;
			*stride = plane[0].w * bpp_value;
			ret = 0;
		}
		/* not support multi-src_planes */
		if ((plane[1].addr) ||
			(plane[2].addr) ||
			(plane[3].addr)) {
			ge2d_log_info("ge2d not support NV21 mode\n");
			ret = -1;
		}
	}
	return ret;
}

static int build_ge2d_addr_config_dma(
			struct ge2d_context_s *context,
			struct config_planes_ion_s *plane,
			unsigned int format,
			unsigned int *addr,
			unsigned int *stride,
			unsigned int dir,
			unsigned int data_type
			)
{
	int ret = -1;
	int bpp_value = bpp(format);
	unsigned long addr_temp = 0;

	bpp_value /= 8;
	ge2d_log_dbg("build_ge2d_addr_config_ion bpp_value=%d\n",
		bpp_value);
	if (plane) {
		if (plane[0].shared_fd) {
			struct ge2d_dma_cfg_s *cfg = NULL;
			struct aml_dma_cfg *dma_cfg = NULL;

			cfg = ge2d_wq_get_dma_cfg(context, data_type);
			if (!cfg)
				return -1;
			cfg->dma_used = 1;
			dma_cfg = kzalloc(sizeof(*dma_cfg), GFP_KERNEL);
			dma_cfg->fd = plane[0].shared_fd;
			dma_cfg->dev = &(ge2d_manager.pdev->dev);
			dma_cfg->dir = dir;
			cfg->dma_cfg = dma_cfg;
			ret = ge2d_dma_buffer_get_phys(dma_cfg, &addr_temp);
			if (ret != 0)
				return ret;
		}
		plane[0].addr += addr_temp;
		if (plane[0].addr) {
			*addr = plane[0].addr;
			*stride = plane[0].w * bpp_value;
			ret = 0;
		}
		/* not support multi-src_planes */
		if ((plane[1].addr) ||
			(plane[2].addr) ||
			(plane[3].addr)) {
			ge2d_log_info("ge2d not support NV21 mode\n");
			ret = -1;
		}
	}
	return ret;
}

static int build_ge2d_config_ex(struct config_planes_s *plane,
				unsigned int format,
				unsigned int *canvas_index,
				int index,
				unsigned int *r_offset)
{
	int bpp_value = bpp(format);
	int ret = -1;

	bpp_value /= 8;
	index &= 0xff;
	if (plane) {
		if (plane[0].addr) {
			*canvas_index = index;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[0].addr,
				      plane[0].w * bpp_value,
				      plane[0].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
			ret = 0;
		}
		/* multi-src_planes */
		if (plane[1].addr) {
			*canvas_index |= index << 8;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[1].addr,
				      plane[1].w * bpp_value,
				      plane[1].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
		}
		if (plane[2].addr) {
			*canvas_index |= index << 16;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[2].addr,
				      plane[2].w * bpp_value,
				      plane[2].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
		}
		if (plane[3].addr) {
			*canvas_index |= index << 24;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[3].addr,
				      plane[3].w * bpp_value,
				      plane[3].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
		}
	}
	return ret;
}


static int build_ge2d_config_ex_ion(struct config_planes_ion_s *plane,
				unsigned int format,
				unsigned int *canvas_index,
				int index,
				unsigned int *r_offset)
{
	int bpp_value = bpp(format);
	int ret = -1;
	unsigned long addr;
	size_t len;

	bpp_value /= 8;
	index &= 0xff;
	if (plane) {
		if (plane[0].shared_fd) {
#ifdef CONFIG_AMLOGIC_ION
			ret = meson_ion_share_fd_to_phys(ge2d_ion_client,
				plane[0].shared_fd, &addr, &len);
			if (ret != 0)
				return ret;
#else
			return ret;
#endif
			plane[0].addr += addr;
			*canvas_index = index;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[0].addr,
				      plane[0].w * bpp_value,
				      plane[0].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
			ret = 0;
		}
		/* multi-src_planes */
		if (plane[1].addr) {
			plane[1].addr += plane[0].addr;
			*canvas_index |= index << 8;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[1].addr,
				      plane[1].w * bpp_value,
				      plane[1].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
		}
		if (plane[2].addr) {
			plane[2].addr += plane[0].addr;
			*canvas_index |= index << 16;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[2].addr,
				      plane[2].w * bpp_value,
				      plane[2].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
		}
		if (plane[3].addr) {
			plane[3].addr += plane[0].addr;
			*canvas_index |= index << 24;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[3].addr,
				      plane[3].w * bpp_value,
				      plane[3].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
		}
	}
	return ret;
}
static int build_ge2d_config_ex_dma(struct ge2d_context_s *context,
				struct config_planes_ion_s *plane,
				unsigned int format,
				unsigned int *canvas_index,
				int index,
				unsigned int *r_offset,
				unsigned int dir,
				unsigned int data_type)
{
	int bpp_value = bpp(format);
	int ret = -1;
	unsigned long addr;

	bpp_value /= 8;
	index &= 0xff;
	if (plane) {
		if (plane[0].shared_fd) {
			struct ge2d_dma_cfg_s *cfg = NULL;
			struct aml_dma_cfg *dma_cfg = NULL;

			cfg = ge2d_wq_get_dma_cfg(context, data_type);
			if (!cfg)
				return -1;
			cfg->dma_used = 1;
			dma_cfg = kzalloc(sizeof(*dma_cfg), GFP_KERNEL);
			dma_cfg->fd = plane[0].shared_fd;
			dma_cfg->dev = &(ge2d_manager.pdev->dev);
			dma_cfg->dir = dir;
			cfg->dma_cfg = dma_cfg;
			ret = ge2d_dma_buffer_get_phys(dma_cfg, &addr);
			if (ret != 0)
				return ret;

			plane[0].addr += addr;
			*canvas_index = index;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[0].addr,
				      plane[0].w * bpp_value,
				      plane[0].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
			ret = 0;
		}
		/* multi-src_planes */
		if (plane[1].addr) {
			plane[1].addr += plane[0].addr;
			*canvas_index |= index << 8;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[1].addr,
				      plane[1].w * bpp_value,
				      plane[1].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
		}
		if (plane[2].addr) {
			plane[2].addr += plane[0].addr;
			*canvas_index |= index << 16;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[2].addr,
				      plane[2].w * bpp_value,
				      plane[2].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
		}
		if (plane[3].addr) {
			plane[3].addr += plane[0].addr;
			*canvas_index |= index << 24;
			*r_offset += 1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			canvas_config(index++, plane[3].addr,
				      plane[3].w * bpp_value,
				      plane[3].h,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#endif
		}
	}
	return ret;
}

int ge2d_context_config_ex(struct ge2d_context_s *context,
			   struct config_para_ex_s *ge2d_config)
{
	struct src_dst_para_s  tmp;
	unsigned int index = 0;
	unsigned int alloc_canvas_offset = 0;
	struct ge2d_src1_gen_s *src1_gen_cfg;
	struct ge2d_src2_dst_data_s *src2_dst_data_cfg;
	struct ge2d_src2_dst_gen_s *src2_dst_gen_cfg;
	struct ge2d_dp_gen_s *dp_gen_cfg;
	struct ge2d_cmd_s *ge2d_cmd_cfg;
	int top, left, width, height;
	unsigned int src_addr = 0, src2_addr = 0, dst_addr = 0;
	unsigned int src_stride = 0, src2_stride = 0, dst_stride = 0;

	/* setup src and dst */
	switch (ge2d_config->src_para.mem_type) {
	case CANVAS_OSD0:
	case CANVAS_OSD1:
		if (setup_display_property(&tmp,
			(ge2d_config->src_para.mem_type == CANVAS_OSD0) ?
			OSD1_CANVAS_INDEX : OSD2_CANVAS_INDEX) < 0)
			return -1;
		ge2d_config->src_para.canvas_index = tmp.canvas_index;
		ge2d_config->src_para.format = tmp.ge2d_color_index;
		src_addr = tmp.phy_addr;
		src_stride = tmp.stride;
		ge2d_log_dbg("ge2d: src1-->type: osd%d, format: 0x%x !!\n",
			     ge2d_config->src_para.mem_type - CANVAS_OSD0,
			     ge2d_config->src_para.format);

		top = ge2d_config->src_para.top;
		left = ge2d_config->src_para.left;
		width = ge2d_config->src_para.width;
		height = ge2d_config->src_para.height;
		if ((left + width > tmp.xres) ||
		    (top + height > tmp.yres)) {
			ge2d_log_dbg("ge2d src error: out of range\n");
			return -1;
		}
		ge2d_config->src_para.width = tmp.xres;
		ge2d_config->src_para.height = tmp.yres;
		ge2d_log_dbg("ge2d osd phy_addr:0x%x,stride=0x%x,format:0x%x\n",
			src_addr,
			src_stride,
			ge2d_config->src_para.format);
		break;
	case CANVAS_ALLOC:
		top = ge2d_config->src_para.top;
		left = ge2d_config->src_para.left;
		width = ge2d_config->src_para.width;
		height = ge2d_config->src_para.height;
		if ((left + width > ge2d_config->src_planes[0].w)
		    || (top + height > ge2d_config->src_planes[0].h)) {
			ge2d_log_dbg("ge2d error: src alloc, out of range\n");
			return -1;
		}
		if (ge2d_meson_dev.canvas_status == 1) {
			if (build_ge2d_addr_config(
				&ge2d_config->src_planes[0],
				ge2d_config->src_para.format,
				&src_addr,
				&src_stride) < 0)
				return -1;
			ge2d_log_dbg("ge2d alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
				src_addr,
				src_stride,
				ge2d_config->src_para.format);
		} else {
			if (build_ge2d_config_ex(&ge2d_config->src_planes[0],
				ge2d_config->src_para.format,
				&index,
				ALLOC_CANVAS_INDEX + alloc_canvas_offset,
				&alloc_canvas_offset) < 0)
				return -1;
			ge2d_config->src_para.canvas_index = index;
			ge2d_log_dbg("ge2d alloc canvas index:0x%x, format:0x%x\n",
				index, ge2d_config->src_para.format);
		}
		break;
	default:
		break;
	}

	switch (ge2d_config->src2_para.mem_type) {
	case  CANVAS_OSD0:
	case  CANVAS_OSD1:
		if (setup_display_property(&tmp,
			(ge2d_config->src2_para.mem_type == CANVAS_OSD0) ?
			OSD1_CANVAS_INDEX : OSD2_CANVAS_INDEX) < 0)
			return -1;
		ge2d_config->src2_para.canvas_index = tmp.canvas_index;
		ge2d_config->src2_para.format = tmp.ge2d_color_index;
		src2_addr = tmp.phy_addr;
		src2_stride = tmp.stride;
		ge2d_log_dbg("ge2d: src2-->type: osd%d, format: 0x%x !!\n",
			     ge2d_config->src2_para.mem_type - CANVAS_OSD0,
			     ge2d_config->src2_para.format);

		top = ge2d_config->src2_para.top;
		left = ge2d_config->src2_para.left;
		width = ge2d_config->src2_para.width;
		height = ge2d_config->src2_para.height;
		if ((left + width > tmp.xres) || (top + height > tmp.yres)) {
			ge2d_log_dbg("ge2d error: src2: osd%d, out of range\n",
				ge2d_config->src2_para.mem_type - CANVAS_OSD0);
			return -1;
		}
		ge2d_config->src2_para.width = tmp.xres;
		ge2d_config->src2_para.height = tmp.yres;
		ge2d_log_dbg("ge2d osd phy_addr:0x%x,stride=0x%x,format:0x%x\n",
			src2_addr,
			src_stride,
			ge2d_config->src2_para.format);
		break;
	case  CANVAS_ALLOC:
		top = ge2d_config->src2_para.top;
		left = ge2d_config->src2_para.left;
		width = ge2d_config->src2_para.width;
		height = ge2d_config->src2_para.height;
		if ((left + width > ge2d_config->src2_planes[0].w)
		    || (top + height > ge2d_config->src2_planes[0].h)) {
			ge2d_log_dbg("ge2d error: src2: alloc, out of range\n");
			return -1;
		}
		if (ge2d_config->src2_planes[0].addr ==
				ge2d_config->src_planes[0].addr) {
			ge2d_config->src2_para.canvas_index =
				ge2d_config->src_para.canvas_index;
			src2_addr = src_addr;
			src2_stride = src_stride;
		} else {
			if (ge2d_meson_dev.canvas_status == 1) {
				if (build_ge2d_addr_config(
					&ge2d_config->src2_planes[0],
					ge2d_config->src2_para.format,
					&src2_addr,
					&src2_stride) < 0)
					return -1;
				ge2d_log_dbg("ge2d alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
					src2_addr,
					src2_stride,
					ge2d_config->src2_para.format);
			} else {
				if (build_ge2d_config_ex(
					&ge2d_config->src2_planes[0],
					ge2d_config->src2_para.format,
					&index,
					ALLOC_CANVAS_INDEX +
					alloc_canvas_offset,
					&alloc_canvas_offset) < 0)
					return -1;
				ge2d_config->src2_para.canvas_index = index;
				ge2d_log_dbg("ge2d src2 alloc, canvas index:0x%x,format:0x%x\n",
					index, ge2d_config->src2_para.format);
			}
		}
		break;
	default:
		break;
	}

	switch (ge2d_config->dst_para.mem_type) {
	case  CANVAS_OSD0:
	case  CANVAS_OSD1:
		if (setup_display_property(&tmp,
			(ge2d_config->dst_para.mem_type == CANVAS_OSD0) ?
			OSD1_CANVAS_INDEX : OSD2_CANVAS_INDEX) < 0)
			return -1;
		ge2d_config->dst_para.canvas_index = tmp.canvas_index;
		ge2d_config->dst_para.format = tmp.ge2d_color_index;
		dst_addr = tmp.phy_addr;
		dst_stride = tmp.stride;

		ge2d_log_dbg("ge2d: dst-->type: osd%d, format: 0x%x !!\n",
			     ge2d_config->dst_para.mem_type - CANVAS_OSD0,
			     ge2d_config->dst_para.format);

		top = ge2d_config->dst_para.top;
		left = ge2d_config->dst_para.left;
		width = ge2d_config->dst_para.width;
		height = ge2d_config->dst_para.height;
		if ((left + width > tmp.xres) || (top + height > tmp.yres)) {
			ge2d_log_dbg("ge2d error: dst: osd%d, out of range\n",
				ge2d_config->dst_para.mem_type - CANVAS_OSD0);
			return -1;
		}
		ge2d_config->dst_para.width = tmp.xres;
		ge2d_config->dst_para.height = tmp.yres;
		ge2d_log_dbg("ge2d osd phy_addr:0x%x,stride=0x%x,format:0x%x\n",
			dst_addr,
			dst_stride,
			ge2d_config->dst_para.format);
		break;
	case  CANVAS_ALLOC:
		top = ge2d_config->dst_para.top;
		left = ge2d_config->dst_para.left;
		width = ge2d_config->dst_para.width;
		height = ge2d_config->dst_para.height;
		if ((left + width > ge2d_config->dst_planes[0].w)
		    || (top + height > ge2d_config->dst_planes[0].h)) {
			ge2d_log_dbg("ge2d error: dst: alloc, out of range\n");
			return -1;
		}
		if (ge2d_config->dst_planes[0].addr ==
				ge2d_config->src_planes[0].addr) {
			ge2d_config->dst_para.canvas_index =
				ge2d_config->src_para.canvas_index;
			dst_addr = src_addr;
			dst_stride = src_stride;
		}
		else if (ge2d_config->dst_planes[0].addr ==
				ge2d_config->src2_planes[0].addr) {
			ge2d_config->dst_para.canvas_index =
				ge2d_config->src2_para.canvas_index;
			dst_addr = src2_addr;
			dst_stride = src2_stride;
		} else {
			if (ge2d_meson_dev.canvas_status == 1) {
				if (build_ge2d_addr_config(
					&ge2d_config->dst_planes[0],
					ge2d_config->dst_para.format,
					&dst_addr,
					&dst_stride) < 0)
					return -1;
				ge2d_log_dbg("ge2d alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
					dst_addr,
					dst_stride,
					ge2d_config->dst_para.format);
			} else {
				if (build_ge2d_config_ex(
					&ge2d_config->dst_planes[0],
					ge2d_config->dst_para.format,
					&index,
					ALLOC_CANVAS_INDEX +
					alloc_canvas_offset,
					&alloc_canvas_offset) < 0)
					return -1;
				ge2d_config->dst_para.canvas_index = index;
				ge2d_log_dbg("ge2d: dst alloc, index:0x%x, format:0x%x\n",
					index, ge2d_config->dst_para.format);
			}
		}
		break;
	default:
		break;
	}

	ge2dgen_rendering_dir(context, ge2d_config->src_para.x_rev,
			      ge2d_config->src_para.y_rev,
			      ge2d_config->dst_para.x_rev,
			      ge2d_config->dst_para.y_rev,
			      ge2d_config->dst_xy_swap);
	ge2dgen_const_color(context, ge2d_config->alu_const_color);

	ge2dgen_src(context, ge2d_config->src_para.canvas_index,
		    ge2d_config->src_para.format,
		    src_addr,
		    src_stride);
	ge2dgen_src_clip(context, ge2d_config->src_para.left,
			ge2d_config->src_para.top,
			ge2d_config->src_para.width,
			ge2d_config->src_para.height);
	ge2dgen_src_key(context, ge2d_config->src_key.key_enable,
			ge2d_config->src_key.key_color,
			ge2d_config->src_key.key_mask,
			ge2d_config->src_key.key_mode);
#ifdef CONFIG_GE2D_SRC2
	ge2dgent_src_gbalpha(context, ge2d_config->src1_gb_alpha,
		ge2d_config->src2_gb_alpha);
#else
	ge2dgent_src_gbalpha(context, ge2d_config->src1_gb_alpha, 0);
#endif
	ge2dgen_src_color(context, ge2d_config->src_para.color);

	ge2dgen_src2(context, ge2d_config->src2_para.canvas_index,
		     ge2d_config->src2_para.format,
		     src2_addr,
		     src2_stride);
	ge2dgen_src2_clip(context, ge2d_config->src2_para.left,
			  ge2d_config->src2_para.top,
			  ge2d_config->src2_para.width,
			  ge2d_config->src2_para.height);

	ge2dgen_dst(context, ge2d_config->dst_para.canvas_index,
		    ge2d_config->dst_para.format,
		    dst_addr,
		    dst_stride);
	ge2dgen_dst_clip(context, ge2d_config->dst_para.left,
			 ge2d_config->dst_para.top,
			 ge2d_config->dst_para.width,
			 ge2d_config->dst_para.height,
			 DST_CLIP_MODE_INSIDE);

	src1_gen_cfg = ge2d_wq_get_src_gen(context);
	src1_gen_cfg->fill_mode = ge2d_config->src_para.fill_mode;
	src1_gen_cfg->chfmt_rpt_pix = 0;
	src1_gen_cfg->cvfmt_rpt_pix = 0;
	/* src1_gen_cfg->clipx_start_ex = 0; */
	/* src1_gen_cfg->clipx_end_ex = 1; */
	/* src1_gen_cfg->clipy_start_ex = 1; */
	/* src1_gen_cfg->clipy_end_ex = 1; */

	src2_dst_data_cfg = ge2d_wq_get_dst_data(context);
	src2_dst_data_cfg->src2_def_color = ge2d_config->src2_para.color;

	src2_dst_gen_cfg = ge2d_wq_get_dst_gen(context);
	src2_dst_gen_cfg->src2_fill_mode = ge2d_config->src2_para.fill_mode;

	dp_gen_cfg = ge2d_wq_get_dp_gen(context);

	dp_gen_cfg->src1_vsc_phase0_always_en =
		ge2d_config->src1_hsc_phase0_always_en;
	dp_gen_cfg->src1_hsc_phase0_always_en =
		ge2d_config->src1_vsc_phase0_always_en;
	if ((context->config.v_scale_coef_type == FILTER_TYPE_GAU0) ||
		(context->config.v_scale_coef_type == FILTER_TYPE_GAU0_BOT) ||
		(context->config.v_scale_coef_type == FILTER_TYPE_GAU1) ||
		(context->config.h_scale_coef_type == FILTER_TYPE_GAU0) ||
		(context->config.h_scale_coef_type == FILTER_TYPE_GAU0_BOT) ||
		(context->config.h_scale_coef_type == FILTER_TYPE_GAU1)) {
		/* 1bit, 0: using minus, 1: using repeat data */
		dp_gen_cfg->src1_hsc_rpt_ctrl = ge2d_config->src1_hsc_rpt_ctrl;
		/* 1bit, 0: using minus  1: using repeat data */
		dp_gen_cfg->src1_vsc_rpt_ctrl = ge2d_config->src1_vsc_rpt_ctrl;
	} else {
		/* 1bit, 0: using minus, 1: using repeat data */
		dp_gen_cfg->src1_hsc_rpt_ctrl = 1;
		/* 1bit, 0: using minus  1: using repeat data */
		dp_gen_cfg->src1_vsc_rpt_ctrl = 1;
	}
	dp_gen_cfg->src1_gb_alpha = 0xff;
	dp_gen_cfg->src1_gb_alpha_en = 0;
#ifdef CONFIG_GE2D_SRC2
	dp_gen_cfg->src2_gb_alpha = 0xff;
	dp_gen_cfg->src2_gb_alpha_en = 0;
#endif
	dp_gen_cfg->src2_key_en = ge2d_config->src2_key.key_enable;
	dp_gen_cfg->src2_key_mode = ge2d_config->src2_key.key_mode;
	dp_gen_cfg->src2_key =   ge2d_config->src2_key.key_color;
	dp_gen_cfg->src2_key_mask = ge2d_config->src2_key.key_mask;

	dp_gen_cfg->bitmask_en = ge2d_config->bitmask_en;
	dp_gen_cfg->bitmask = ge2d_config->bitmask;
	dp_gen_cfg->bytemask_only = ge2d_config->bytemask_only;

	ge2d_cmd_cfg = ge2d_wq_get_cmd(context);

	ge2d_cmd_cfg->src1_fill_color_en = ge2d_config->src_para.fill_color_en;

	ge2d_cmd_cfg->src2_x_rev = ge2d_config->src2_para.x_rev;
	ge2d_cmd_cfg->src2_y_rev = ge2d_config->src2_para.y_rev;
	ge2d_cmd_cfg->src2_fill_color_en =
		ge2d_config->src2_para.fill_color_en;

	ge2d_cmd_cfg->vsc_phase_slope = ge2d_config->vsc_phase_slope;
	ge2d_cmd_cfg->vsc_ini_phase = ge2d_config->vf_init_phase;
	ge2d_cmd_cfg->vsc_phase_step = ge2d_config->vsc_start_phase_step;
	ge2d_cmd_cfg->vsc_rpt_l0_num = ge2d_config->vf_rpt_num;

	/* let internal decide */
	ge2d_cmd_cfg->hsc_phase_slope = ge2d_config->hsc_phase_slope;
	ge2d_cmd_cfg->hsc_ini_phase = ge2d_config->hf_init_phase;
	ge2d_cmd_cfg->hsc_phase_step = ge2d_config->hsc_start_phase_step;
	ge2d_cmd_cfg->hsc_rpt_p0_num = ge2d_config->hf_rpt_num;

	ge2d_cmd_cfg->src1_cmult_asel = 0;
	ge2d_cmd_cfg->src2_cmult_asel = 0;
	context->config.update_flag = UPDATE_ALL;
	/* context->config.src1_data.ddr_burst_size_y = 3; */
	/* context->config.src1_data.ddr_burst_size_cb = 3; */
	/* context->config.src1_data.ddr_burst_size_cr = 3; */
	/* context->config.src2_dst_data.ddr_burst_size= 3; */

	return  0;
}
EXPORT_SYMBOL(ge2d_context_config_ex);

int ge2d_context_config_ex_ion(struct ge2d_context_s *context,
			   struct config_para_ex_ion_s *ge2d_config)
{
	struct src_dst_para_s  tmp;
	unsigned int index = 0;
	unsigned int alloc_canvas_offset = 0;
	struct ge2d_src1_gen_s *src1_gen_cfg;
	struct ge2d_src2_dst_data_s *src2_dst_data_cfg;
	struct ge2d_src2_dst_gen_s *src2_dst_gen_cfg;
	struct ge2d_dp_gen_s *dp_gen_cfg;
	struct ge2d_cmd_s *ge2d_cmd_cfg;
	int top, left, width, height;
	unsigned int src_addr = 0, src2_addr = 0, dst_addr = 0;
	unsigned int src_stride = 0, src2_stride = 0, dst_stride = 0;

	/* setup src and dst */
	switch (ge2d_config->src_para.mem_type) {
	case CANVAS_OSD0:
	case CANVAS_OSD1:
		if (setup_display_property(&tmp,
			(ge2d_config->src_para.mem_type == CANVAS_OSD0) ?
			OSD1_CANVAS_INDEX : OSD2_CANVAS_INDEX) < 0)
			return -1;
		ge2d_config->src_para.canvas_index = tmp.canvas_index;
		ge2d_config->src_para.format = tmp.ge2d_color_index;
		src_addr = tmp.phy_addr;
		src_stride = tmp.stride;

		ge2d_log_dbg("ge2d: src1-->type: osd%d, format: 0x%x !!\n",
			     ge2d_config->src_para.mem_type - CANVAS_OSD0,
			     ge2d_config->src_para.format);

		top = ge2d_config->src_para.top;
		left = ge2d_config->src_para.left;
		width = ge2d_config->src_para.width;
		height = ge2d_config->src_para.height;
		if ((left + width > tmp.xres) ||
		    (top + height > tmp.yres)) {
			ge2d_log_dbg("ge2d src error: out of range\n");
			return -1;
		}
		ge2d_config->src_para.width = tmp.xres;
		ge2d_config->src_para.height = tmp.yres;
		ge2d_log_dbg("ge2d osd phy_addr:0x%x,stride=0x%x,format:0x%x\n",
			src_addr,
			src_stride,
			ge2d_config->src_para.format);
		break;
	case CANVAS_ALLOC:
		top = ge2d_config->src_para.top;
		left = ge2d_config->src_para.left;
		width = ge2d_config->src_para.width;
		height = ge2d_config->src_para.height;
		if ((left + width > ge2d_config->src_planes[0].w)
		    || (top + height > ge2d_config->src_planes[0].h)) {
			ge2d_log_dbg("ge2d error: src alloc, out of range\n");
			return -1;
		}
		if (ge2d_meson_dev.canvas_status == 1) {
			if (build_ge2d_addr_config_ion(
				&ge2d_config->src_planes[0],
				ge2d_config->src_para.format,
				&src_addr,
				&src_stride) < 0)
				return -1;
			ge2d_log_dbg("ge2d alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
				src_addr,
				src_stride,
				ge2d_config->src_para.format);

		} else {
			if (build_ge2d_config_ex_ion(
				&ge2d_config->src_planes[0],
				ge2d_config->src_para.format,
				&index,
				ALLOC_CANVAS_INDEX + alloc_canvas_offset,
				&alloc_canvas_offset) < 0)
			return -1;
			ge2d_config->src_para.canvas_index = index;
			ge2d_log_dbg("ge2d alloc canvas index:0x%x, format:0x%x\n",
					 index, ge2d_config->src_para.format);
		}
		break;
	default:
		break;
	}

	switch (ge2d_config->src2_para.mem_type) {
	case  CANVAS_OSD0:
	case  CANVAS_OSD1:
		if (setup_display_property(&tmp,
			(ge2d_config->src2_para.mem_type == CANVAS_OSD0) ?
			OSD1_CANVAS_INDEX : OSD2_CANVAS_INDEX) < 0)
			return -1;
		ge2d_config->src2_para.canvas_index = tmp.canvas_index;
		ge2d_config->src2_para.format = tmp.ge2d_color_index;
		src2_addr = tmp.phy_addr;
		src2_stride = tmp.stride;

		ge2d_log_dbg("ge2d: src2-->type: osd%d, format: 0x%x !!\n",
			     ge2d_config->src2_para.mem_type - CANVAS_OSD0,
			     ge2d_config->src2_para.format);

		top = ge2d_config->src2_para.top;
		left = ge2d_config->src2_para.left;
		width = ge2d_config->src2_para.width;
		height = ge2d_config->src2_para.height;
		if ((left + width > tmp.xres) || (top + height > tmp.yres)) {
			ge2d_log_dbg("ge2d error: src2: osd%d, out of range\n",
				ge2d_config->src2_para.mem_type - CANVAS_OSD0);
			return -1;
		}
		ge2d_config->src2_para.width = tmp.xres;
		ge2d_config->src2_para.height = tmp.yres;
		ge2d_log_dbg("ge2d osd phy_addr:0x%x,stride=0x%x,format:0x%x\n",
			src2_addr,
			src2_stride,
			ge2d_config->src2_para.format);
		break;
	case  CANVAS_ALLOC:
		top = ge2d_config->src2_para.top;
		left = ge2d_config->src2_para.left;
		width = ge2d_config->src2_para.width;
		height = ge2d_config->src2_para.height;
		if ((left + width > ge2d_config->src2_planes[0].w)
		    || (top + height > ge2d_config->src2_planes[0].h)) {
			ge2d_log_dbg("ge2d error: src2: alloc, out of range\n");
			return -1;
		}
		/*if (ge2d_config->src2_planes[0].addr ==
		 *		ge2d_config->src_planes[0].addr)
		 *	index = ge2d_config->src_para.canvas_index;
		 * else
		 */
		if (ge2d_meson_dev.canvas_status == 1) {
			if (build_ge2d_addr_config_ion(
				&ge2d_config->src2_planes[0],
				ge2d_config->src2_para.format,
				&src2_addr,
				&src2_stride) < 0)
				return -1;
			ge2d_log_dbg("ge2d alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
				src2_addr,
				src2_stride,
				ge2d_config->src2_para.format);
		} else {
			if (build_ge2d_config_ex_ion(
				&ge2d_config->src2_planes[0],
				ge2d_config->src2_para.format,
				&index,
				ALLOC_CANVAS_INDEX + alloc_canvas_offset,
				&alloc_canvas_offset) < 0)
				return -1;
			ge2d_config->src2_para.canvas_index = index;
			ge2d_log_dbg("ge2d src2 alloc, canvas index:0x%x,format:0x%x\n",
				index, ge2d_config->src2_para.format);
		}
		break;
	default:
		break;
	}

	switch (ge2d_config->dst_para.mem_type) {
	case  CANVAS_OSD0:
	case  CANVAS_OSD1:
		if (setup_display_property(&tmp,
			(ge2d_config->dst_para.mem_type == CANVAS_OSD0) ?
			OSD1_CANVAS_INDEX : OSD2_CANVAS_INDEX) < 0)
			return -1;
		ge2d_config->dst_para.canvas_index = tmp.canvas_index;
		ge2d_config->dst_para.format = tmp.ge2d_color_index;
		dst_addr = tmp.phy_addr;
		dst_stride = tmp.stride;

		ge2d_log_dbg("ge2d: dst-->type: osd%d, format: 0x%x !!\n",
			     ge2d_config->dst_para.mem_type - CANVAS_OSD0,
			     ge2d_config->dst_para.format);

		top = ge2d_config->dst_para.top;
		left = ge2d_config->dst_para.left;
		width = ge2d_config->dst_para.width;
		height = ge2d_config->dst_para.height;
		if ((left + width > tmp.xres) || (top + height > tmp.yres)) {
			ge2d_log_dbg("ge2d error: dst: osd%d, out of range\n",
				ge2d_config->dst_para.mem_type - CANVAS_OSD0);
			return -1;
		}
		ge2d_config->dst_para.width = tmp.xres;
		ge2d_config->dst_para.height = tmp.yres;
		ge2d_log_dbg("ge2d osd phy_addr:0x%x,stride=0x%x,format:0x%x\n",
			dst_addr,
			dst_stride,
			ge2d_config->dst_para.format);
		break;
	case  CANVAS_ALLOC:
		top = ge2d_config->dst_para.top;
		left = ge2d_config->dst_para.left;
		width = ge2d_config->dst_para.width;
		height = ge2d_config->dst_para.height;
		if ((left + width > ge2d_config->dst_planes[0].w)
		    || (top + height > ge2d_config->dst_planes[0].h)) {
			ge2d_log_dbg("ge2d error: dst: alloc, out of range\n");
			return -1;
		}
		/*if (ge2d_config->dst_planes[0].addr ==
		 *		ge2d_config->src_planes[0].addr)
		 *	index = ge2d_config->src_para.canvas_index;
		 * else if (ge2d_config->dst_planes[0].addr ==
		 *		ge2d_config->src2_planes[0].addr)
		 *	index = ge2d_config->src2_para.canvas_index;
		 * else
		 */
		if (ge2d_meson_dev.canvas_status == 1) {
			if (build_ge2d_addr_config_ion(
				&ge2d_config->dst_planes[0],
				ge2d_config->dst_para.format,
				&dst_addr,
				&dst_stride) < 0)
				return -1;
			ge2d_log_dbg("ge2d alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
				dst_addr,
				dst_stride,
				ge2d_config->dst_para.format);
		} else {
			if (build_ge2d_config_ex_ion(
				&ge2d_config->dst_planes[0],
				ge2d_config->dst_para.format,
				&index,
				ALLOC_CANVAS_INDEX + alloc_canvas_offset,
				&alloc_canvas_offset) < 0)
				return -1;
			ge2d_config->dst_para.canvas_index = index;
			ge2d_log_dbg("ge2d: dst alloc, index:0x%x, format:0x%x\n",
				index, ge2d_config->dst_para.format);
		}
		break;
	default:
		break;
	}

	ge2dgen_rendering_dir(context, ge2d_config->src_para.x_rev,
			      ge2d_config->src_para.y_rev,
			      ge2d_config->dst_para.x_rev,
			      ge2d_config->dst_para.y_rev,
			      ge2d_config->dst_xy_swap);
	ge2dgen_const_color(context, ge2d_config->alu_const_color);

	ge2dgen_src(context, ge2d_config->src_para.canvas_index,
		    ge2d_config->src_para.format,
		    src_addr,
		    src_stride);
	ge2dgen_src_clip(context, ge2d_config->src_para.left,
			ge2d_config->src_para.top,
			ge2d_config->src_para.width,
			ge2d_config->src_para.height);
	ge2dgen_src_key(context, ge2d_config->src_key.key_enable,
			ge2d_config->src_key.key_color,
			ge2d_config->src_key.key_mask,
			ge2d_config->src_key.key_mode);
#ifdef CONFIG_GE2D_SRC2
	ge2dgent_src_gbalpha(context, ge2d_config->src1_gb_alpha,
		ge2d_config->src2_gb_alpha);
#else
	ge2dgent_src_gbalpha(context, ge2d_config->src1_gb_alpha, 0);
#endif
	ge2dgen_src_color(context, ge2d_config->src_para.color);

	ge2dgen_src2(context, ge2d_config->src2_para.canvas_index,
		     ge2d_config->src2_para.format,
		     src2_addr,
		     src2_stride);
	ge2dgen_src2_clip(context, ge2d_config->src2_para.left,
			  ge2d_config->src2_para.top,
			  ge2d_config->src2_para.width,
			  ge2d_config->src2_para.height);

	ge2dgen_dst(context, ge2d_config->dst_para.canvas_index,
		    ge2d_config->dst_para.format,
		    dst_addr,
		    dst_stride);
	ge2dgen_dst_clip(context, ge2d_config->dst_para.left,
			 ge2d_config->dst_para.top,
			 ge2d_config->dst_para.width,
			 ge2d_config->dst_para.height,
			 DST_CLIP_MODE_INSIDE);

	src1_gen_cfg = ge2d_wq_get_src_gen(context);
	src1_gen_cfg->fill_mode = ge2d_config->src_para.fill_mode;
	src1_gen_cfg->chfmt_rpt_pix = 0;
	src1_gen_cfg->cvfmt_rpt_pix = 0;
	/* src1_gen_cfg->clipx_start_ex = 0; */
	/* src1_gen_cfg->clipx_end_ex = 1; */
	/* src1_gen_cfg->clipy_start_ex = 1; */
	/* src1_gen_cfg->clipy_end_ex = 1; */

	src2_dst_data_cfg = ge2d_wq_get_dst_data(context);
	src2_dst_data_cfg->src2_def_color = ge2d_config->src2_para.color;

	src2_dst_gen_cfg = ge2d_wq_get_dst_gen(context);
	src2_dst_gen_cfg->src2_fill_mode = ge2d_config->src2_para.fill_mode;

	dp_gen_cfg = ge2d_wq_get_dp_gen(context);

	dp_gen_cfg->src1_vsc_phase0_always_en =
		ge2d_config->src1_hsc_phase0_always_en;
	dp_gen_cfg->src1_hsc_phase0_always_en =
		ge2d_config->src1_vsc_phase0_always_en;
	if ((context->config.v_scale_coef_type == FILTER_TYPE_GAU0) ||
		(context->config.v_scale_coef_type == FILTER_TYPE_GAU0_BOT) ||
		(context->config.v_scale_coef_type == FILTER_TYPE_GAU1) ||
		(context->config.h_scale_coef_type == FILTER_TYPE_GAU0) ||
		(context->config.h_scale_coef_type == FILTER_TYPE_GAU0_BOT) ||
		(context->config.h_scale_coef_type == FILTER_TYPE_GAU1)) {
		/* 1bit, 0: using minus, 1: using repeat data */
		dp_gen_cfg->src1_hsc_rpt_ctrl = ge2d_config->src1_hsc_rpt_ctrl;
		/* 1bit, 0: using minus  1: using repeat data */
		dp_gen_cfg->src1_vsc_rpt_ctrl = ge2d_config->src1_vsc_rpt_ctrl;
	} else {
		/* 1bit, 0: using minus, 1: using repeat data */
		dp_gen_cfg->src1_hsc_rpt_ctrl = 1;
		/* 1bit, 0: using minus  1: using repeat data */
		dp_gen_cfg->src1_vsc_rpt_ctrl = 1;
	}
	dp_gen_cfg->src1_gb_alpha = ge2d_config->src1_gb_alpha & 0xff;
	dp_gen_cfg->src1_gb_alpha_en = ge2d_config->src1_gb_alpha_en & 1;
#ifdef CONFIG_GE2D_SRC2
	dp_gen_cfg->src2_gb_alpha = ge2d_config->src2_gb_alpha & 0xff;
	dp_gen_cfg->src2_gb_alpha_en = ge2d_config->src2_gb_alpha_en & 1;
#endif
	dp_gen_cfg->src2_key_en = ge2d_config->src2_key.key_enable;
	dp_gen_cfg->src2_key_mode = ge2d_config->src2_key.key_mode;
	dp_gen_cfg->src2_key =   ge2d_config->src2_key.key_color;
	dp_gen_cfg->src2_key_mask = ge2d_config->src2_key.key_mask;

	dp_gen_cfg->bitmask_en = ge2d_config->bitmask_en;
	dp_gen_cfg->bitmask = ge2d_config->bitmask;
	dp_gen_cfg->bytemask_only = ge2d_config->bytemask_only;

	ge2d_cmd_cfg = ge2d_wq_get_cmd(context);

	ge2d_cmd_cfg->src1_fill_color_en = ge2d_config->src_para.fill_color_en;

	ge2d_cmd_cfg->src2_x_rev = ge2d_config->src2_para.x_rev;
	ge2d_cmd_cfg->src2_y_rev = ge2d_config->src2_para.y_rev;
	ge2d_cmd_cfg->src2_fill_color_en =
		ge2d_config->src2_para.fill_color_en;
#ifdef CONFIG_GE2D_SRC2
	ge2d_cmd_cfg->src2_cmult_ad = ge2d_config->src2_cmult_ad;
#endif

	ge2d_cmd_cfg->vsc_phase_slope = ge2d_config->vsc_phase_slope;
	ge2d_cmd_cfg->vsc_ini_phase = ge2d_config->vf_init_phase;
	ge2d_cmd_cfg->vsc_phase_step = ge2d_config->vsc_start_phase_step;
	ge2d_cmd_cfg->vsc_rpt_l0_num = ge2d_config->vf_rpt_num;

	/* let internal decide */
	ge2d_cmd_cfg->hsc_phase_slope = ge2d_config->hsc_phase_slope;
	ge2d_cmd_cfg->hsc_ini_phase = ge2d_config->hf_init_phase;
	ge2d_cmd_cfg->hsc_phase_step = ge2d_config->hsc_start_phase_step;
	ge2d_cmd_cfg->hsc_rpt_p0_num = ge2d_config->hf_rpt_num;
	ge2d_cmd_cfg->src1_cmult_asel = ge2d_config->src1_cmult_asel;
	ge2d_cmd_cfg->src2_cmult_asel = ge2d_config->src2_cmult_asel;

	ge2d_cmd_cfg->src1_fmt = ge2d_config->src_para.format;
	ge2d_cmd_cfg->src2_fmt = ge2d_config->src2_para.format;
	ge2d_cmd_cfg->dst_fmt = ge2d_config->dst_para.format;

	context->config.update_flag = UPDATE_ALL;
	/* context->config.src1_data.ddr_burst_size_y = 3; */
	/* context->config.src1_data.ddr_burst_size_cb = 3; */
	/* context->config.src1_data.ddr_burst_size_cr = 3; */
	/* context->config.src2_dst_data.ddr_burst_size= 3; */

	return  0;
}
int ge2d_context_config_ex_mem(struct ge2d_context_s *context,
			   struct config_para_ex_memtype_s *ge2d_config_mem)
{
	struct src_dst_para_s  tmp;
	unsigned int index = 0;
	unsigned int alloc_canvas_offset = 0;
	struct ge2d_src1_gen_s *src1_gen_cfg;
	struct ge2d_src2_dst_data_s *src2_dst_data_cfg;
	struct ge2d_src2_dst_gen_s *src2_dst_gen_cfg;
	struct ge2d_dp_gen_s *dp_gen_cfg;
	struct ge2d_cmd_s *ge2d_cmd_cfg;
	int top, left, width, height;
	unsigned int src_addr = 0, src2_addr = 0, dst_addr = 0;
	unsigned int src_stride = 0, src2_stride = 0, dst_stride = 0;
	struct config_para_ex_ion_s *ge2d_config;

	ge2d_config = &(ge2d_config_mem->_ge2d_config_ex);
	/* setup src and dst */
	switch (ge2d_config->src_para.mem_type) {
	case CANVAS_OSD0:
	case CANVAS_OSD1:
		if (setup_display_property(&tmp,
			(ge2d_config->src_para.mem_type == CANVAS_OSD0) ?
			OSD1_CANVAS_INDEX : OSD2_CANVAS_INDEX) < 0)
			return -1;
		ge2d_config->src_para.canvas_index = tmp.canvas_index;
		ge2d_config->src_para.format = tmp.ge2d_color_index;
		src_addr = tmp.phy_addr;
		src_stride = tmp.stride;

		ge2d_log_dbg("ge2d: src1-->type: osd%d, format: 0x%x !!\n",
			     ge2d_config->src_para.mem_type - CANVAS_OSD0,
			     ge2d_config->src_para.format);

		top = ge2d_config->src_para.top;
		left = ge2d_config->src_para.left;
		width = ge2d_config->src_para.width;
		height = ge2d_config->src_para.height;
		if ((left + width > tmp.xres) ||
		    (top + height > tmp.yres)) {
			ge2d_log_dbg("ge2d src error: out of range\n");
			return -1;
		}
		ge2d_config->src_para.width = tmp.xres;
		ge2d_config->src_para.height = tmp.yres;
		ge2d_log_dbg("ge2d osd phy_addr:0x%x,stride=0x%x,format:0x%x\n",
			src_addr,
			src_stride,
			ge2d_config->src_para.format);
		break;
	case CANVAS_ALLOC:
		top = ge2d_config->src_para.top;
		left = ge2d_config->src_para.left;
		width = ge2d_config->src_para.width;
		height = ge2d_config->src_para.height;
		if ((left + width > ge2d_config->src_planes[0].w)
		    || (top + height > ge2d_config->src_planes[0].h)) {
			ge2d_log_dbg("ge2d error: src alloc, out of range\n");
			return -1;
		}
		if (ge2d_meson_dev.canvas_status == 1) {
			if (ge2d_config_mem->src1_mem_alloc_type ==
				AML_GE2D_MEM_ION) {
				if (build_ge2d_addr_config_ion(
					&ge2d_config->src_planes[0],
					ge2d_config->src_para.format,
					&src_addr,
					&src_stride) < 0)
					return -1;
				ge2d_log_dbg("ge2d ion alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
					src_addr,
					src_stride,
					ge2d_config->src_para.format);
			} else if (ge2d_config_mem->src1_mem_alloc_type ==
				AML_GE2D_MEM_DMABUF) {
				if (build_ge2d_addr_config_dma(
					context,
					&ge2d_config->src_planes[0],
					ge2d_config->src_para.format,
					&src_addr,
					&src_stride,
					DMA_TO_DEVICE,
					AML_GE2D_SRC) < 0)
					return -1;
				ge2d_log_dbg("ge2d dma alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
					src_addr,
					src_stride,
					ge2d_config->src_para.format);
			}
		} else {
			if (ge2d_config_mem->src1_mem_alloc_type ==
				AML_GE2D_MEM_ION) {
				if (build_ge2d_config_ex_ion(
					&ge2d_config->src_planes[0],
					ge2d_config->src_para.format,
					&index,
					ALLOC_CANVAS_INDEX +
					alloc_canvas_offset,
					&alloc_canvas_offset) < 0)
					return -1;
				ge2d_config->src_para.canvas_index = index;
				ge2d_log_dbg("ge2d ion alloc canvas index:0x%x, format:0x%x\n",
					index, ge2d_config->src_para.format);
			} else if (ge2d_config_mem->src1_mem_alloc_type ==
				AML_GE2D_MEM_DMABUF) {
				if (build_ge2d_config_ex_dma(
					context,
					&ge2d_config->src_planes[0],
					ge2d_config->src_para.format,
					&index,
					ALLOC_CANVAS_INDEX +
					alloc_canvas_offset,
					&alloc_canvas_offset,
					DMA_TO_DEVICE,
					AML_GE2D_SRC) < 0)
					return -1;
				ge2d_config->src_para.canvas_index = index;
				ge2d_log_dbg("ge2d dma alloc canvas index:0x%x, format:0x%x\n",
					index, ge2d_config->src_para.format);
			}
		}
		break;
	default:
		break;
	}

	switch (ge2d_config->src2_para.mem_type) {
	case  CANVAS_OSD0:
	case  CANVAS_OSD1:
		if (setup_display_property(&tmp,
			(ge2d_config->src2_para.mem_type == CANVAS_OSD0) ?
			OSD1_CANVAS_INDEX : OSD2_CANVAS_INDEX) < 0)
			return -1;
		ge2d_config->src2_para.canvas_index = tmp.canvas_index;
		ge2d_config->src2_para.format = tmp.ge2d_color_index;
		src2_addr = tmp.phy_addr;
		src2_stride = tmp.stride;

		ge2d_log_dbg("ge2d: src2-->type: osd%d, format: 0x%x !!\n",
			     ge2d_config->src2_para.mem_type - CANVAS_OSD0,
			     ge2d_config->src2_para.format);

		top = ge2d_config->src2_para.top;
		left = ge2d_config->src2_para.left;
		width = ge2d_config->src2_para.width;
		height = ge2d_config->src2_para.height;
		if ((left + width > tmp.xres) || (top + height > tmp.yres)) {
			ge2d_log_dbg("ge2d error: src2: osd%d, out of range\n",
				ge2d_config->src2_para.mem_type - CANVAS_OSD0);
			return -1;
		}
		ge2d_config->src2_para.width = tmp.xres;
		ge2d_config->src2_para.height = tmp.yres;
		ge2d_log_dbg("ge2d osd phy_addr:0x%x,stride=0x%x,format:0x%x\n",
			src2_addr,
			src2_stride,
			ge2d_config->src2_para.format);
		break;
	case  CANVAS_ALLOC:
		top = ge2d_config->src2_para.top;
		left = ge2d_config->src2_para.left;
		width = ge2d_config->src2_para.width;
		height = ge2d_config->src2_para.height;
		if ((left + width > ge2d_config->src2_planes[0].w)
		    || (top + height > ge2d_config->src2_planes[0].h)) {
			ge2d_log_dbg("ge2d error: src2: alloc, out of range\n");
			return -1;
		}
		/*if (ge2d_config->src2_planes[0].addr ==
		 *		ge2d_config->src_planes[0].addr)
		 *	index = ge2d_config->src_para.canvas_index;
		 * else
		 */
		if (ge2d_meson_dev.canvas_status == 1) {
			if (ge2d_config_mem->src2_mem_alloc_type ==
				AML_GE2D_MEM_ION) {
				if (build_ge2d_addr_config_ion(
					&ge2d_config->src2_planes[0],
					ge2d_config->src2_para.format,
					&src2_addr,
					&src2_stride) < 0)
					return -1;
				ge2d_log_dbg("ge2d ion alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
					src2_addr,
					src2_stride,
					ge2d_config->src2_para.format);
			} else if (ge2d_config_mem->src2_mem_alloc_type ==
				AML_GE2D_MEM_DMABUF) {
				if (build_ge2d_addr_config_dma(
					context,
					&ge2d_config->src2_planes[0],
					ge2d_config->src2_para.format,
					&src2_addr,
					&src2_stride,
					DMA_TO_DEVICE,
					AML_GE2D_SRC2) < 0)
					return -1;
				ge2d_log_dbg("ge2d dma alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
					src2_addr,
					src2_stride,
					ge2d_config->src2_para.format);
			}
		} else {
			if (ge2d_config_mem->src2_mem_alloc_type ==
				AML_GE2D_MEM_ION) {
				if (build_ge2d_config_ex_ion(
					&ge2d_config->src2_planes[0],
					ge2d_config->src2_para.format,
					&index,
					ALLOC_CANVAS_INDEX +
					alloc_canvas_offset,
					&alloc_canvas_offset) < 0)
					return -1;
				ge2d_config->src2_para.canvas_index = index;
				ge2d_log_dbg("ge2d src2 ion alloc, canvas index:0x%x,format:0x%x\n",
					index, ge2d_config->src2_para.format);
			} else if (ge2d_config_mem->src2_mem_alloc_type ==
				AML_GE2D_MEM_DMABUF) {
				if (build_ge2d_config_ex_dma(
					context,
					&ge2d_config->src2_planes[0],
					ge2d_config->src2_para.format,
					&index,
					ALLOC_CANVAS_INDEX +
					alloc_canvas_offset,
					&alloc_canvas_offset,
					DMA_TO_DEVICE,
					AML_GE2D_SRC2) < 0)
					return -1;
				ge2d_config->src2_para.canvas_index = index;
				ge2d_log_dbg("ge2d src2 dma alloc, canvas index:0x%x,format:0x%x\n",
					index, ge2d_config->src2_para.format);
			}
		}
		break;
	default:
		break;
	}

	switch (ge2d_config->dst_para.mem_type) {
	case  CANVAS_OSD0:
	case  CANVAS_OSD1:
		if (setup_display_property(&tmp,
			(ge2d_config->dst_para.mem_type == CANVAS_OSD0) ?
			OSD1_CANVAS_INDEX : OSD2_CANVAS_INDEX) < 0)
			return -1;
		ge2d_config->dst_para.canvas_index = tmp.canvas_index;
		ge2d_config->dst_para.format = tmp.ge2d_color_index;
		dst_addr = tmp.phy_addr;
		dst_stride = tmp.stride;

		ge2d_log_dbg("ge2d: dst-->type: osd%d, format: 0x%x !!\n",
			     ge2d_config->dst_para.mem_type - CANVAS_OSD0,
			     ge2d_config->dst_para.format);

		top = ge2d_config->dst_para.top;
		left = ge2d_config->dst_para.left;
		width = ge2d_config->dst_para.width;
		height = ge2d_config->dst_para.height;
		if ((left + width > tmp.xres) || (top + height > tmp.yres)) {
			ge2d_log_dbg("ge2d error: dst: osd%d, out of range\n",
				ge2d_config->dst_para.mem_type - CANVAS_OSD0);
			return -1;
		}
		ge2d_config->dst_para.width = tmp.xres;
		ge2d_config->dst_para.height = tmp.yres;
		ge2d_log_dbg("ge2d osd phy_addr:0x%x,stride=0x%x,format:0x%x\n",
			dst_addr,
			dst_stride,
			ge2d_config->dst_para.format);
		break;
	case  CANVAS_ALLOC:
		top = ge2d_config->dst_para.top;
		left = ge2d_config->dst_para.left;
		width = ge2d_config->dst_para.width;
		height = ge2d_config->dst_para.height;
		if ((left + width > ge2d_config->dst_planes[0].w)
		    || (top + height > ge2d_config->dst_planes[0].h)) {
			ge2d_log_dbg("ge2d error: dst: alloc, out of range\n");
			return -1;
		}
		/*if (ge2d_config->dst_planes[0].addr ==
		 *		ge2d_config->src_planes[0].addr)
		 *	index = ge2d_config->src_para.canvas_index;
		 * else if (ge2d_config->dst_planes[0].addr ==
		 *		ge2d_config->src2_planes[0].addr)
		 *	index = ge2d_config->src2_para.canvas_index;
		 * else
		 */
		if (ge2d_meson_dev.canvas_status == 1) {
			if (ge2d_config_mem->dst_mem_alloc_type ==
				AML_GE2D_MEM_ION) {
				if (build_ge2d_addr_config_ion(
					&ge2d_config->dst_planes[0],
					ge2d_config->dst_para.format,
					&dst_addr,
					&dst_stride) < 0)
					return -1;
				ge2d_log_dbg("ge2d ion alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
					dst_addr,
					dst_stride,
					ge2d_config->dst_para.format);
			} else if (ge2d_config_mem->dst_mem_alloc_type ==
				AML_GE2D_MEM_DMABUF) {
				if (build_ge2d_addr_config_dma(
					context,
					&ge2d_config->dst_planes[0],
					ge2d_config->dst_para.format,
					&dst_addr,
					&dst_stride,
					DMA_FROM_DEVICE,
					AML_GE2D_DST) < 0)
					return -1;
				ge2d_log_dbg("ge2d dma alloc phy_addr:0x%x,stride=0x%x,format:0x%x\n",
					dst_addr,
					dst_stride,
					ge2d_config->dst_para.format);
			}
		} else {
			if (ge2d_config_mem->dst_mem_alloc_type ==
				AML_GE2D_MEM_ION) {
				if (build_ge2d_config_ex_ion(
					&ge2d_config->dst_planes[0],
					ge2d_config->dst_para.format,
					&index,
					ALLOC_CANVAS_INDEX +
					alloc_canvas_offset,
					&alloc_canvas_offset) < 0)
					return -1;
				ge2d_config->dst_para.canvas_index = index;
				ge2d_log_dbg("ge2d: dst ion alloc, index:0x%x, format:0x%x\n",
					index, ge2d_config->dst_para.format);
			} else if (ge2d_config_mem->dst_mem_alloc_type ==
				AML_GE2D_MEM_DMABUF) {
				if (build_ge2d_config_ex_dma(
					context,
					&ge2d_config->dst_planes[0],
					ge2d_config->dst_para.format,
					&index,
					ALLOC_CANVAS_INDEX +
					alloc_canvas_offset,
					&alloc_canvas_offset,
					DMA_FROM_DEVICE,
					AML_GE2D_DST) < 0)
					return -1;
				ge2d_config->dst_para.canvas_index = index;
				ge2d_log_dbg("ge2d: dst dma alloc, index:0x%x, format:0x%x\n",
					index, ge2d_config->dst_para.format);
			}
		}
		break;
	default:
		break;
	}

	ge2dgen_rendering_dir(context, ge2d_config->src_para.x_rev,
			      ge2d_config->src_para.y_rev,
			      ge2d_config->dst_para.x_rev,
			      ge2d_config->dst_para.y_rev,
			      ge2d_config->dst_xy_swap);
	ge2dgen_const_color(context, ge2d_config->alu_const_color);

	ge2dgen_src(context, ge2d_config->src_para.canvas_index,
		    ge2d_config->src_para.format,
		    src_addr,
		    src_stride);
	ge2dgen_src_clip(context, ge2d_config->src_para.left,
			ge2d_config->src_para.top,
			ge2d_config->src_para.width,
			ge2d_config->src_para.height);
	ge2dgen_src_key(context, ge2d_config->src_key.key_enable,
			ge2d_config->src_key.key_color,
			ge2d_config->src_key.key_mask,
			ge2d_config->src_key.key_mode);
#ifdef CONFIG_GE2D_SRC2
	ge2dgent_src_gbalpha(context, ge2d_config->src1_gb_alpha,
		ge2d_config->src2_gb_alpha);
#else
	ge2dgent_src_gbalpha(context, ge2d_config->src1_gb_alpha, 0);
#endif
	ge2dgen_src_color(context, ge2d_config->src_para.color);

	ge2dgen_src2(context, ge2d_config->src2_para.canvas_index,
		     ge2d_config->src2_para.format,
		     src2_addr,
		     src2_stride);
	ge2dgen_src2_clip(context, ge2d_config->src2_para.left,
			  ge2d_config->src2_para.top,
			  ge2d_config->src2_para.width,
			  ge2d_config->src2_para.height);

	ge2dgen_dst(context, ge2d_config->dst_para.canvas_index,
		    ge2d_config->dst_para.format,
		    dst_addr,
		    dst_stride);
	ge2dgen_dst_clip(context, ge2d_config->dst_para.left,
			 ge2d_config->dst_para.top,
			 ge2d_config->dst_para.width,
			 ge2d_config->dst_para.height,
			 DST_CLIP_MODE_INSIDE);

	src1_gen_cfg = ge2d_wq_get_src_gen(context);
	src1_gen_cfg->fill_mode = ge2d_config->src_para.fill_mode;
	src1_gen_cfg->chfmt_rpt_pix = 0;
	src1_gen_cfg->cvfmt_rpt_pix = 0;
	/* src1_gen_cfg->clipx_start_ex = 0; */
	/* src1_gen_cfg->clipx_end_ex = 1; */
	/* src1_gen_cfg->clipy_start_ex = 1; */
	/* src1_gen_cfg->clipy_end_ex = 1; */

	src2_dst_data_cfg = ge2d_wq_get_dst_data(context);
	src2_dst_data_cfg->src2_def_color = ge2d_config->src2_para.color;

	src2_dst_gen_cfg = ge2d_wq_get_dst_gen(context);
	src2_dst_gen_cfg->src2_fill_mode = ge2d_config->src2_para.fill_mode;

	dp_gen_cfg = ge2d_wq_get_dp_gen(context);

	dp_gen_cfg->src1_vsc_phase0_always_en =
		ge2d_config->src1_hsc_phase0_always_en;
	dp_gen_cfg->src1_hsc_phase0_always_en =
		ge2d_config->src1_vsc_phase0_always_en;
	if ((context->config.v_scale_coef_type == FILTER_TYPE_GAU0) ||
		(context->config.v_scale_coef_type == FILTER_TYPE_GAU0_BOT) ||
		(context->config.v_scale_coef_type == FILTER_TYPE_GAU1) ||
		(context->config.h_scale_coef_type == FILTER_TYPE_GAU0) ||
		(context->config.h_scale_coef_type == FILTER_TYPE_GAU0_BOT) ||
		(context->config.h_scale_coef_type == FILTER_TYPE_GAU1)) {
		/* 1bit, 0: using minus, 1: using repeat data */
		dp_gen_cfg->src1_hsc_rpt_ctrl = ge2d_config->src1_hsc_rpt_ctrl;
		/* 1bit, 0: using minus  1: using repeat data */
		dp_gen_cfg->src1_vsc_rpt_ctrl = ge2d_config->src1_vsc_rpt_ctrl;
	} else {
		/* 1bit, 0: using minus, 1: using repeat data */
		dp_gen_cfg->src1_hsc_rpt_ctrl = 1;
		/* 1bit, 0: using minus  1: using repeat data */
		dp_gen_cfg->src1_vsc_rpt_ctrl = 1;
	}
	dp_gen_cfg->src1_gb_alpha = ge2d_config->src1_gb_alpha & 0xff;
	dp_gen_cfg->src1_gb_alpha_en = ge2d_config->src1_gb_alpha_en & 1;
#ifdef CONFIG_GE2D_SRC2
	dp_gen_cfg->src2_gb_alpha = ge2d_config->src2_gb_alpha & 0xff;
	dp_gen_cfg->src2_gb_alpha_en = ge2d_config->src2_gb_alpha_en & 1;
#endif
	dp_gen_cfg->src2_key_en = ge2d_config->src2_key.key_enable;
	dp_gen_cfg->src2_key_mode = ge2d_config->src2_key.key_mode;
	dp_gen_cfg->src2_key =   ge2d_config->src2_key.key_color;
	dp_gen_cfg->src2_key_mask = ge2d_config->src2_key.key_mask;

	dp_gen_cfg->bitmask_en = ge2d_config->bitmask_en;
	dp_gen_cfg->bitmask = ge2d_config->bitmask;
	dp_gen_cfg->bytemask_only = ge2d_config->bytemask_only;

	ge2d_cmd_cfg = ge2d_wq_get_cmd(context);

	ge2d_cmd_cfg->src1_fill_color_en = ge2d_config->src_para.fill_color_en;

	ge2d_cmd_cfg->src2_x_rev = ge2d_config->src2_para.x_rev;
	ge2d_cmd_cfg->src2_y_rev = ge2d_config->src2_para.y_rev;
	ge2d_cmd_cfg->src2_fill_color_en =
		ge2d_config->src2_para.fill_color_en;
#ifdef CONFIG_GE2D_SRC2
	ge2d_cmd_cfg->src2_cmult_ad = ge2d_config->src2_cmult_ad;
#endif

	ge2d_cmd_cfg->vsc_phase_slope = ge2d_config->vsc_phase_slope;
	ge2d_cmd_cfg->vsc_ini_phase = ge2d_config->vf_init_phase;
	ge2d_cmd_cfg->vsc_phase_step = ge2d_config->vsc_start_phase_step;
	ge2d_cmd_cfg->vsc_rpt_l0_num = ge2d_config->vf_rpt_num;

	/* let internal decide */
	ge2d_cmd_cfg->hsc_phase_slope = ge2d_config->hsc_phase_slope;
	ge2d_cmd_cfg->hsc_ini_phase = ge2d_config->hf_init_phase;
	ge2d_cmd_cfg->hsc_phase_step = ge2d_config->hsc_start_phase_step;
	ge2d_cmd_cfg->hsc_rpt_p0_num = ge2d_config->hf_rpt_num;
	ge2d_cmd_cfg->src1_cmult_asel = ge2d_config->src1_cmult_asel;
	ge2d_cmd_cfg->src2_cmult_asel = ge2d_config->src2_cmult_asel;
	context->config.update_flag = UPDATE_ALL;
	/* context->config.src1_data.ddr_burst_size_y = 3; */
	/* context->config.src1_data.ddr_burst_size_cb = 3; */
	/* context->config.src1_data.ddr_burst_size_cr = 3; */
	/* context->config.src2_dst_data.ddr_burst_size= 3; */

	return  0;
}

int ge2d_buffer_alloc(struct ge2d_dmabuf_req_s *ge2d_req_buf)
{
	struct device *dev;

	dev = &(ge2d_manager.pdev->dev);
	return ge2d_dma_buffer_alloc(ge2d_manager.buffer,
		dev, ge2d_req_buf);
}

int ge2d_buffer_export(struct ge2d_dmabuf_exp_s *ge2d_exp_buf)
{
	return ge2d_dma_buffer_export(ge2d_manager.buffer, ge2d_exp_buf);
}

int ge2d_buffer_free(int index)
{
	return ge2d_dma_buffer_free(ge2d_manager.buffer, index);

}

void ge2d_buffer_dma_flush(int dma_fd)
{
	struct device *dev;

	dev = &(ge2d_manager.pdev->dev);
	ge2d_dma_buffer_dma_flush(dev, dma_fd);
}

void ge2d_buffer_cache_flush(int dma_fd)
{
	struct device *dev;

	dev = &(ge2d_manager.pdev->dev);
	ge2d_dma_buffer_cache_flush(dev, dma_fd);
}

struct ge2d_context_s *create_ge2d_work_queue(void)
{
	int  i;
	struct ge2d_queue_item_s *p_item;
	struct ge2d_context_s *ge2d_work_queue;
	int  empty;

	ge2d_work_queue = kzalloc(sizeof(struct ge2d_context_s), GFP_KERNEL);
	ge2d_work_queue->config.h_scale_coef_type = FILTER_TYPE_BILINEAR;
	ge2d_work_queue->config.v_scale_coef_type = FILTER_TYPE_BILINEAR;
	ge2d_work_queue->ge2d_request_exit = 0;
	if (IS_ERR(ge2d_work_queue)) {
		ge2d_log_err("can't create work queue\n");
		return NULL;
	}
	INIT_LIST_HEAD(&ge2d_work_queue->work_queue);
	INIT_LIST_HEAD(&ge2d_work_queue->free_queue);
	init_waitqueue_head(&ge2d_work_queue->cmd_complete);
	spin_lock_init(&ge2d_work_queue->lock);  /* for process lock. */
	for (i = 0; i < MAX_GE2D_CMD; i++) {
		p_item = kcalloc(1,
				sizeof(struct ge2d_queue_item_s),
				GFP_KERNEL);
		if (IS_ERR(p_item)) {
			ge2d_log_err("can't request queue item memory\n");
			return NULL;
		}
		list_add_tail(&p_item->list, &ge2d_work_queue->free_queue);
	}

	/* put this process queue  into manager queue list. */
	/* maybe process queue is changing . */
	spin_lock(&ge2d_manager.event.sem_lock);
	empty = list_empty(&ge2d_manager.process_queue);
	list_add_tail(&ge2d_work_queue->list, &ge2d_manager.process_queue);
	spin_unlock(&ge2d_manager.event.sem_lock);
	return ge2d_work_queue; /* find it */
}
EXPORT_SYMBOL(create_ge2d_work_queue);

int  destroy_ge2d_work_queue(struct ge2d_context_s *ge2d_work_queue)
{
	struct ge2d_queue_item_s *pitem, *tmp;
	struct list_head		*head;
	int empty, timeout = 0;

	if (ge2d_work_queue) {
		/* first detatch it from the process queue,then delete it . */
		/* maybe process queue is changing .so we lock it. */
		spin_lock(&ge2d_manager.event.sem_lock);
		list_del(&ge2d_work_queue->list);
		empty = list_empty(&ge2d_manager.process_queue);
		spin_unlock(&ge2d_manager.event.sem_lock);
		if ((ge2d_manager.current_wq == ge2d_work_queue) &&
		    (ge2d_manager.ge2d_state == GE2D_STATE_RUNNING)) {
			ge2d_work_queue->ge2d_request_exit = 1;
			timeout = wait_for_completion_timeout(
					&ge2d_manager.event.process_complete,
					msecs_to_jiffies(500));
			if (!timeout)
				ge2d_log_err("wait timeout\n");
			/* condition so complex ,simplify it . */
			ge2d_manager.last_wq = NULL;
		} /* else we can delete it safely. */

		head = &ge2d_work_queue->work_queue;
		list_for_each_entry_safe(pitem, tmp, head, list) {
			if (pitem) {
				list_del(&pitem->list);
				kfree(pitem);
			}
		}
		head = &ge2d_work_queue->free_queue;
		list_for_each_entry_safe(pitem, tmp, head, list) {
			if (pitem) {
				list_del(&pitem->list);
				kfree(pitem);
			}
		}

		kfree(ge2d_work_queue);
		ge2d_work_queue = NULL;
		return 0;
	}

	return  -1;
}
EXPORT_SYMBOL(destroy_ge2d_work_queue);

int ge2d_wq_init(struct platform_device *pdev,
	int irq, struct clk *clk)
{
	ge2d_manager.pdev = pdev;
	ge2d_irq = irq;
	ge2d_clk = clk;

	ge2d_log_info("ge2d: pdev=%p, irq=%d, clk=%p\n",
		pdev, irq, clk);

	ge2d_manager.irq_num = request_irq(ge2d_irq,
					ge2d_wq_handle,
					IRQF_SHARED,
					"ge2d",
					(void *)&ge2d_manager);
	if (ge2d_manager.irq_num < 0) {
		ge2d_log_err("ge2d request irq error\n");
		return -1;
	}

	/* prepare bottom half */
	spin_lock_init(&ge2d_manager.event.sem_lock);
	sema_init(&ge2d_manager.event.cmd_in_sem, 1);
	init_waitqueue_head(&ge2d_manager.event.cmd_complete);
	init_completion(&ge2d_manager.event.process_complete);
	INIT_LIST_HEAD(&ge2d_manager.process_queue);
	ge2d_manager.last_wq = NULL;
	ge2d_manager.ge2d_thread = NULL;
	ge2d_manager.buffer = ge2d_dma_buffer_create();
	if (!ge2d_manager.buffer)
		return -1;

	if (ge2d_start_monitor()) {
		ge2d_log_err("ge2d create thread error\n");
		return -1;
	}

	return 0;
}

int ge2d_wq_deinit(void)
{
	ge2d_stop_monitor();
	ge2d_log_info("deinit ge2d device\n");
	if (ge2d_manager.irq_num >= 0) {
		free_irq(ge2d_manager.irq_num, &ge2d_manager);
		ge2d_manager.irq_num = -1;
	}
	ge2d_irq = -1;
	clk_disable_unprepare(ge2d_clk);
	ge2d_dma_buffer_destroy(ge2d_manager.buffer);
	ge2d_manager.buffer = NULL;
	ge2d_manager.pdev = NULL;
	return  0;
}

