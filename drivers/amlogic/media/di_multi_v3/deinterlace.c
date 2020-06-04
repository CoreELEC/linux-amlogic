/*
 * drivers/amlogic/media/di_multi_v3/deinterlace.c
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
#include "di_pres.h"
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
/*2018-07-18 -----------*/

#ifdef DET3D
#include "detect3d.h"
#endif
#define ENABLE_SPIN_LOCK_ALWAYS

static DEFINE_SPINLOCK(di_lock2);

#define di_lock_irqfiq_save(irq_flag) \
	spin_lock_irqsave(&di_lock2, irq_flag)

#define di_unlock_irqfiq_restore(irq_flag) \
	spin_unlock_irqrestore(&di_lock2, irq_flag)

#ifdef HIS_V3
void dim_lock_irqfiq_save(ulong flg)
{
	spin_lock_irqsave(&di_lock2, flg);
}

void dim_unlock_irqfiq_restore(ulong flg)
{
	spin_unlock_irqrestore(&di_lock2, flg);
}
#endif

#ifdef SUPPORT_MPEG_TO_VDIN
static int mpeg2vdin_flag;
static int mpeg2vdin_en;
#endif

static int di_reg_unreg_cnt = 40;
static bool overturn;

bool dimv3_get_overturn(void)
{
	return overturn;
}

int dimv3_get_reg_unreg_cnt(void)
{
	return di_reg_unreg_cnt;
}

static bool mc_mem_alloc;
bool dimv3_get_mcmem_alloc(void)
{
	return mc_mem_alloc;
}

//static unsigned int di_pre_rdma_enable;

/**************************************
 *
 *
 *************************************/
unsigned int div3_dbg = DBG_M_EVENT;//|DBG_M_DBG;//DBG_M_QUED|
module_param(div3_dbg, uint, 0664);
MODULE_PARM_DESC(div3_dbg, "debug print");

/* destroy unnecessary frames before display */

/*static unsigned int hold_video; change to EDI_CFGX_HOLD_VIDEO*/

DEFINE_SPINLOCK(plistv3_lock);

static const char version_s[] = "2019-04-25ma";

/*1:enable bypass pre,ei only;
 * 2:debug force bypass pre,ei for post
 */
static int bypass_pre;
/**************************
 * ary:
 * invert_top_bot
 *	[0]:control post
 *	[1]:control pre?
 */
static int invert_top_bot;

/* add avoid vframe put/get error */
static int di_blocking;
/*
 * bit[2]: enable bypass all when skip
 * bit[1:0]: enable bypass post when skip
 */
/*static int di_vscale_skip_enable;*/

/* 0: not support nr10bit, 1: support nr10bit */
/*static unsigned int nr10bit_support;*/

#ifdef RUN_DI_PROCESS_IN_IRQ
/*
 * di_process() run in irq,
 * dim_reg_process(), dim_unreg_process() run in kernel thread
 * dim_reg_process_irq(), di_unreg_process_irq() run in irq
 * di_vf_put(), di_vf_peek(), di_vf_get() run in irq
 * di_receiver_event_fun() run in task or irq
 */
/*
 * important:
 * to set input2pre, VFRAME_EVENT_PROVIDER_VFRAME_READY of
 * vdin should be sent in irq
 */

static int input2pre;
/*false:process progress by field;
 * true: process progress by frame with 2 interlace buffer
 */
static int input2pre_buf_miss_count;
static int input2pre_proc_miss_count;
static int input2pre_throw_count;
static int input2pre_miss_policy;
/* 0, do not force pre_de_busy to 0, use di_wr_buf after dim_irq happen;
 * 1, force pre_de_busy to 0 and call
 *	dim_pre_de_done_buf_clear to clear di_wr_buf
 */
#endif
/*false:process progress by field;
 * bit0: process progress by frame with 2 interlace buffer
 * bit1: temp add debug for 3d process FA,1:bit0 force to 1;
 */
/*static int use_2_interlace_buff;*/
/* prog_proc_config,
 * bit[2:1]: when two field buffers are used,
 * 0 use vpp for blending ,
 * 1 use post_di module for blending
 * 2 debug mode, bob with top field
 * 3 debug mode, bot with bot field
 * bit[0]:
 * 0 "prog vdin" use two field buffers,
 * 1 "prog vdin" use single frame buffer
 * bit[4]:
 * 0 "prog frame from decoder/vdin" use two field buffers,
 * 1 use single frame buffer
 * bit[5]:
 * when two field buffers are used for decoder (bit[4] is 0):
 * 1,handle prog frame as two interlace frames
 * bit[6]:(bit[4] is 0,bit[5] is 0,use_2_interlace_buff is 0): 0,
 * process progress frame as field,blend by post;
 * 1, process progress frame as field,process by normal di
 */
/*static int prog_proc_config = (1 << 5) | (1 << 1) | 1;*/
/*
 * for source include both progressive and interlace pictures,
 * always use post_di module for blending
 */
#define is_handle_prog_frame_as_interlace(vframe)			\
	(((dimp_get(eDI_MP_prog_proc_config) & 0x30) == 0x20) &&	\
	 (((vframe)->type & VIDTYPE_VIU_422) == 0))

static int frame_count;
static int disp_frame_count;
int div3_get_disp_cnt(void)
{
	return disp_frame_count;
}

int dimv3_get_invert_tb(void)
{
	return invert_top_bot;
}
static unsigned long reg_unreg_timeout_cnt;
#ifdef DET3D
static unsigned int det3d_mode;
static void set3d_view(enum tvin_trans_fmt trans_fmt, struct vframe_s *vf);
#endif

static void di_pq_parm_destroy(struct di_pq_parm_s *pq_ptr);
static struct di_pq_parm_s *di_pq_parm_create(struct am_pq_parm_s *);

//static unsigned int unreg_cnt;/*cnt for vframe unreg*/
//static unsigned int reg_cnt;/*cnt for vframe reg*/

static unsigned char recovery_flag;

static unsigned int recovery_log_reason;
static unsigned int recovery_log_queue_idx;
static struct di_buf_s *recovery_log_di_buf;

unsigned char dimv3_vcry_get_flg(void)
{
	return recovery_flag;
}

void dimv3_vcry_flg_inc(void)
{
	recovery_flag++;
}

void dimv3_vcry_set_flg(unsigned char val)
{
	recovery_flag = val;
}

void dimv3_reg_timeout_inc(void)
{
	reg_unreg_timeout_cnt++;
}

/********************************/
unsigned int dimv3_vcry_get_log_reason(void)
{
	return recovery_log_reason;
}

void dimv3_vcry_set_log_reason(unsigned int val)
{
	recovery_log_reason = val;
}

/********************************/
unsigned char dimv3_vcry_get_log_q_idx(void)
{
	return recovery_log_queue_idx;
}

void dimv3_vcry_set_log_q_idx(unsigned int val)
{
	recovery_log_queue_idx = val;
}

/********************************/
struct di_buf_s **dimv3_vcry_get_log_di_buf(void)
{
	return &recovery_log_di_buf;
}

void dimv3_vcry_set_log_di_buf(struct di_buf_s *di_bufp)
{
	recovery_log_di_buf = di_bufp;
}

void dimv3_vcry_set(unsigned int reason, unsigned int idx,
		  struct di_buf_s *di_bufp)
{
	recovery_log_reason = reason;
	recovery_log_queue_idx = idx;
	recovery_log_di_buf = di_bufp;
}

static long same_field_top_count;
static long same_field_bot_count;
/* bit 0:
 * 0, keep 3 buffers in pre_ready_list for checking;
 * 1, keep 4 buffers in pre_ready_list for checking;
 */

static struct queue_s *get_queue_by_idx(unsigned int channel, int idx);
//static void dump_state(unsigned int channel);
static void recycle_keep_buffer(unsigned int channel);

#define DI_PRE_INTERVAL         (HZ / 100)

/*
 * progressive frame process type config:
 * 0, process by field;
 * 1, process by frame (only valid for vdin source whose
 * width/height does not change)
 */

static struct di_buf_s *cur_post_ready_di_buf;

/************For Write register**********************/

static unsigned int num_di_stop_reg_addr = 4;
static unsigned int di_stop_reg_addr[4] = {0};

static unsigned int is_need_stop_reg(unsigned int addr)
{
	int idx = 0;

	if (dimp_get(eDI_MP_di_stop_reg_flag)) {
		for (idx = 0; idx < num_di_stop_reg_addr; idx++) {
			if (addr == di_stop_reg_addr[idx]) {
				pr_dbg("stop write addr: %x\n", addr);
				return 1;
			}
		}
	}

	return 0;
}

void dimv3_DI_Wr(unsigned int addr, unsigned int val)
{
	if (is_need_stop_reg(addr))
		return;
	ddbgv3_reg_save(addr, val, 0, 32);
	Wr(addr, val);
}

void dimv3_DI_Wr_reg_bits(unsigned int adr, unsigned int val,
			unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(adr))
		return;
	ddbgv3_reg_save(adr, val, start, len); /*ary add for debug*/
	Wr_reg_bits(adr, val, start, len);
}

void dimv3_VSYNC_WR_MPEG_REG(unsigned int addr, unsigned int val)
{
	if (is_need_stop_reg(addr))
		return;
	if (dimp_get(eDI_MP_post_wr_en) && dimp_get(eDI_MP_post_wr_support))
		dimv3_DI_Wr(addr, val);
	else
		VSYNC_WR_MPEG_REG(addr, val);
}

unsigned int dimv3_VSYNC_WR_MPEG_REG_BITS(unsigned int addr, unsigned int val,
				unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(addr))
		return 0;
	if (dimp_get(eDI_MP_post_wr_en) && dimp_get(eDI_MP_post_wr_support))
		dimv3_DI_Wr_reg_bits(addr, val, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(addr, val, start, len);
	return 0;
}

#ifdef DI_V2
unsigned int DI_POST_REG_RD(unsigned int addr)
{
	struct di_dev_s  *de_devp = getv3_dim_de_devp();

	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		PR_ERR("REG 0x%x access prohibited.\n", addr);
		return 0;
	}
	return VSYNC_RD_MPEG_REG(addr);
}
EXPORT_SYMBOL(DI_POST_REG_RD);

int DI_POST_WR_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	struct di_dev_s  *de_devp = getv3_dim_de_devp();

	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		PR_ERR("REG 0x%x access prohibited.\n", adr);
		return -1;
	}
	return VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
}
EXPORT_SYMBOL(DI_POST_WR_REG_BITS);
#else
unsigned int l_DIV3_POST_REG_RD(unsigned int addr)
{
	struct di_dev_s  *de_devp = getv3_dim_de_devp();

	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		PR_ERR("REG 0x%x access prohibited.\n", addr);
		return 0;
	}
	return VSYNC_RD_MPEG_REG(addr);
}

int l_DIV3_POST_WR_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	struct di_dev_s  *de_devp = getv3_dim_de_devp();

	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		PR_ERR("REG 0x%x access prohibited.\n", adr);
		return -1;
	}
	return VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
}

#endif
/**********************************/

/*****************************
 *	 di attr management :
 *	 enable
 *	 mode
 *	 reg
 ******************************/
/*config attr*/

int prev3_run_flag = DI_RUN_FLAG_RUN;
static int dump_state_flag;

const char *dimv3_get_version_s(void)
{
	return version_s;
}

int dimv3_get_blocking(void)
{
	return di_blocking;
}

unsigned long dimv3_get_reg_unreg_timeout_cnt(void)
{
	return reg_unreg_timeout_cnt;
}

struct di_buf_s *dimv3_get_recovery_log_di_buf(void)
{
	return recovery_log_di_buf;
}
#if 0
struct vframe_s **dim_get_vframe_in(unsigned int ch)
{
	return getv3_vframe_in(ch);
}
#endif
int dimv3_get_dump_state_flag(void)
{
	return dump_state_flag;
}

/*--------------------------*/

ssize_t
storev3_dbg(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count)
{
	unsigned int channel = get_current_channel();	/* debug only*/
	struct di_buf_s *pbuf_local = get_buf_local(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_ch_s *pch;

	pch = get_chdata(channel);

	if (strncmp(buf, "buf", 3) == 0) {
		struct di_buf_s *di_buf_tmp = 0;

		if (kstrtoul(buf + 3, 16, (unsigned long *)&di_buf_tmp))
			return count;
		dimv3_dump_di_buf(di_buf_tmp);
	} else if (strncmp(buf, "vframe", 6) == 0) {
		vframe_t *vf = 0;

		if (kstrtoul(buf + 6, 16, (unsigned long *)&vf))
			return count;
		dimv3_dump_vframe(vf);
	} else if (strncmp(buf, "pool", 4) == 0) {
		unsigned long idx = 0;

		if (kstrtoul(buf + 4, 10, &idx))
			return count;
		dimv3_dump_pool(get_queue_by_idx(channel, idx));
	} else if (strncmp(buf, "state", 4) == 0) {
		//dump_state(channel);
		pr_info("add new debugfs: cat /sys/kernel/debug/di/state\n");
	} else if (strncmp(buf, "prog_proc_config", 16) == 0) {
		if (buf[16] == '1')
			dimp_set(eDI_MP_prog_proc_config, 1);
		else
			dimp_set(eDI_MP_prog_proc_config, 0);
	} else if (strncmp(buf, "init_flag", 9) == 0) {
		if (buf[9] == '1')
			set_init_flag(0, true);/*init_flag = 1;*/
		else
			set_init_flag(0, false);/*init_flag = 0;*/
	} else if (strncmp(buf, "prun", 4) == 0) {
		prev3_run_flag = DI_RUN_FLAG_RUN;
	} else if (strncmp(buf, "ppause", 6) == 0) {
		prev3_run_flag = DI_RUN_FLAG_PAUSE;
	} else if (strncmp(buf, "pstep", 5) == 0) {
		prev3_run_flag = DI_RUN_FLAG_STEP;
	} else if (strncmp(buf, "dumpreg", 7) == 0) {
		pr_info("add new debugfs: cat /sys/kernel/debug/di/dumpreg\n");
	} else if (strncmp(buf, "dumpmif", 7) == 0) {
		dimv3_dump_mif_size_state(ppre, ppost);
	} else if (strncmp(buf, "recycle_buf", 11) == 0) {
		recycle_keep_buffer(channel);
	} else if (strncmp(buf, "recycle_post", 12) == 0) {
		if (div3_vf_l_peek(pch))
			div3_vf_l_put(div3_vf_l_get(pch), pch);
	} else if (strncmp(buf, "mem_map", 7) == 0) {
		dimv3_dump_buf_addr(pbuf_local, MAX_LOCAL_BUF_NUM * 2);
	} else {
		pr_info("DI no support cmd %s!!!\n", buf);
	}

	return count;
}

#ifdef ARY_TEMP
static int __init di_read_canvas_reverse(char *str)
{
	unsigned char *ptr = str;

	pr_dbg("%s: bootargs is %s.\n", __func__, str);
	if (strstr(ptr, "1")) {
		invert_top_bot |= 0x1;
		overturn = true;
	} else {
		invert_top_bot &= (~0x1);
		overturn = false;
	}

	return 0;
}

__setup("video_reverse=", di_read_canvas_reverse);
#endif

static unsigned char *di_log_buf;
static unsigned int di_log_wr_pos;
static unsigned int di_log_rd_pos;
static unsigned int di_log_buf_size;

static unsigned int buf_state_log_start;
/*  set to 1 by condition of "post_ready count < buf_state_log_threshold",
 * reset to 0 by set buf_state_log_threshold as 0
 */

static DEFINE_SPINLOCK(di_print_lock);

#define PRINT_TEMP_BUF_SIZE 128

static int di_print_buf(char *buf, int len)
{
	unsigned long flags;
	int pos;
	int di_log_rd_pos_;

	if (di_log_buf_size == 0)
		return 0;

	spin_lock_irqsave(&di_print_lock, flags);
	di_log_rd_pos_ = di_log_rd_pos;
	if (di_log_wr_pos >= di_log_rd_pos)
		di_log_rd_pos_ += di_log_buf_size;

	for (pos = 0; pos < len && di_log_wr_pos < (di_log_rd_pos_ - 1);
	     pos++, di_log_wr_pos++) {
		if (di_log_wr_pos >= di_log_buf_size)
			di_log_buf[di_log_wr_pos - di_log_buf_size] = buf[pos];
		else
			di_log_buf[di_log_wr_pos] = buf[pos];
	}
	if (di_log_wr_pos >= di_log_buf_size)
		di_log_wr_pos -= di_log_buf_size;
	spin_unlock_irqrestore(&di_print_lock, flags);
	return pos;
}

/* static int log_seq = 0; */
int dimv3_print(const char *fmt, ...)
{
	va_list args;
	int avail = PRINT_TEMP_BUF_SIZE;
	char buf[PRINT_TEMP_BUF_SIZE];
	int pos, len = 0;

	if (dimp_get(eDI_MP_di_printk_flag) & 1) {
		if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_PRECISE_TIMESTAMP)
			pr_dbg("%llu ms:", curv3_to_msecs());
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
		return 0;
	}

	if (di_log_buf_size == 0)
		return 0;

/* len += snprintf(buf+len, avail-len, "%d:",log_seq++); */
	if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_TIMESTAMP)
		len += snprintf(buf + len, avail - len, "%u:",
			dim_get_timerms(0));

	va_start(args, fmt);
	len += vsnprintf(buf + len, avail - len, fmt, args);
	va_end(args);

	if ((avail - len) <= 0)
		buf[PRINT_TEMP_BUF_SIZE - 1] = '\0';

	pos = di_print_buf(buf, len);
/* pr_dbg("dim_print:%d %d\n", di_log_wr_pos, di_log_rd_pos); */
	return pos;
}

ssize_t dimv3_read_log(char *buf)
{
	unsigned long flags;
	ssize_t read_size = 0;

	if (di_log_buf_size == 0)
		return 0;
/* pr_dbg("show_log:%d %d\n", di_log_wr_pos, di_log_rd_pos); */
	spin_lock_irqsave(&di_print_lock, flags);
	if (di_log_rd_pos < di_log_wr_pos)
		read_size = di_log_wr_pos - di_log_rd_pos;

	else if (di_log_rd_pos > di_log_wr_pos)
		read_size = di_log_buf_size - di_log_rd_pos;

	if (read_size > PAGE_SIZE)
		read_size = PAGE_SIZE;
	if (read_size > 0)
		memcpy(buf, di_log_buf + di_log_rd_pos, read_size);

	di_log_rd_pos += read_size;
	if (di_log_rd_pos >= di_log_buf_size)
		di_log_rd_pos = 0;
	spin_unlock_irqrestore(&di_print_lock, flags);
	return read_size;
}

ssize_t
storev3_log(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count)
{
	unsigned long flags, tmp;

	if (strncmp(buf, "bufsize", 7) == 0) {
		if (kstrtoul(buf + 7, 10, &tmp))
			return count;
		spin_lock_irqsave(&di_print_lock, flags);
		kfree(di_log_buf);
		di_log_buf = NULL;
		di_log_buf_size = 0;
		di_log_rd_pos = 0;
		di_log_wr_pos = 0;
		if (tmp >= 1024) {
			di_log_buf_size = 0;
			di_log_rd_pos = 0;
			di_log_wr_pos = 0;
			di_log_buf = kmalloc(tmp, GFP_KERNEL);
			if (di_log_buf)
				di_log_buf_size = tmp;
		}
		spin_unlock_irqrestore(&di_print_lock, flags);
		pr_dbg("di_store:set bufsize tmp %lu %u\n",
		       tmp, di_log_buf_size);
	} else if (strncmp(buf, "printk", 6) == 0) {
		if (kstrtoul(buf + 6, 10, &tmp))
			return count;

		dimp_set(eDI_MP_di_printk_flag, tmp);
	} else {
		dimv3_print("%s", buf);
	}
	return 16;
}

ssize_t
showv3_vframe_status(struct device *dev,
		   struct device_attribute *attr,
		   char *buf)
{
	int ret = 0;
	int get_ret = 0;

	struct vframe_states states;
	int ch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		if (!pbm->sub_act_flg[ch])
			continue;
		else
			ret += sprintf(buf + ret, "ch[%d]\n", ch);

		get_ret = vf_get_states_by_name(div3_rev_name[ch], &states);

		if (get_ret == 0) {
			ret += sprintf(buf + ret, "vframe_pool_size=%d\n",
				states.vf_pool_size);
			ret += sprintf(buf + ret, "vframe buf_free_num=%d\n",
				states.buf_free_num);
			ret += sprintf(buf + ret, "vframe buf_recycle_num=%d\n",
				states.buf_recycle_num);
			ret += sprintf(buf + ret, "vframe buf_avail_num=%d\n",
				states.buf_avail_num);
		} else {
			ret += sprintf(buf + ret, "vframe no states\n");
		}
	}
	return ret;
}

/***************************
 * di buffer management
 ***************************/

static const char * const vframe_type_name[] = {
	"", "di_buf_in", "di_buf_loc", "di_buf_post"
};

const char *dimv3_get_vfm_type_name(unsigned int nub)
{
	if (nub < 4)
		return vframe_type_name[nub];

	return "";
}

static unsigned int default_width = 1920;
static unsigned int default_height = 1080;

/***********************************************
 * test input 4k mode
 ***********************************************/
void dimv3_tst_4k_reg_val(void)
{
	static bool flg_4k;

	if (dimp_get(eDI_MP_4k_test)) {
		flg_4k = true;
		default_width = 3840;
		default_height = 2160;
		pr_info("%s:4k\n", __func__);
		return;
	}

	if (flg_4k) {
		default_width = 1920;
		default_height = 1080;

		flg_4k = false;
		pr_info("%s:disable 4k\n", __func__);
	}

}

/*
 * all buffers are in
 * 1) list of local_free_list,in_free_list,pre_ready_list,recycle_list
 * 2) di_pre_stru.di_inp_buf
 * 3) di_pre_stru.di_wr_buf
 * 4) cur_post_ready_di_buf
 * 5) (struct di_buf_s*)(vframe->private_data)->di_buf[]
 *
 * 6) post_free_list_head
 * 8) (struct di_buf_s*)(vframe->private_data)
 */

/*move to deinterlace .h #define queue_t struct queue_s*/

static struct queue_s *get_queue_by_idx(unsigned int channel, int idx)
{
	struct queue_s *pqueue = get_queue(channel);

	if (idx < QUEUE_NUM)
		return &pqueue[idx];
	else
		return NULL;
}

struct di_buf_s *dimv3_get_buf(unsigned int channel, int queue_idx,
			     int *start_pos)
{
	struct queue_s *pqueue = get_queue(channel);
	queue_t *q = &pqueue[queue_idx];
	int idx = 0;
	unsigned int pool_idx, di_buf_idx;
	struct di_buf_s *di_buf = NULL;
	int start_pos_init = *start_pos;
	struct di_buf_pool_s *pbuf_pool = get_buf_pool(channel);

	if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_QUEUE)
		dimv3_print("%s:<%d:%d,%d,%d> %d\n", __func__, queue_idx,
			  q->num, q->in_idx, q->out_idx, *start_pos);

	if (q->type == 0) {
		if ((*start_pos) < q->num) {
			idx = q->out_idx + (*start_pos);
			if (idx >= MAX_QUEUE_POOL_SIZE)
				idx -= MAX_QUEUE_POOL_SIZE;

			(*start_pos)++;
		} else {
			idx = MAX_QUEUE_POOL_SIZE;
		}
	} else if ((q->type == 1) || (q->type == 2)) {
		for (idx = (*start_pos); idx < MAX_QUEUE_POOL_SIZE; idx++) {
			if (q->pool[idx] != 0) {
				*start_pos = idx + 1;
				break;
			}
		}
	}
	if (idx < MAX_QUEUE_POOL_SIZE) {
		pool_idx = ((q->pool[idx] >> 8) & 0xff) - 1;
		di_buf_idx = q->pool[idx] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (di_buf_idx < pbuf_pool[pool_idx].size)
				di_buf =
					&(pbuf_pool[pool_idx].di_buf_ptr[
						  di_buf_idx]);
		}
	}

	if ((di_buf) && ((((pool_idx + 1) << 8) | di_buf_idx)
			 != ((di_buf->type << 8) | (di_buf->index)))) {
		PR_ERR("%s:(%x,%x)\n", __func__,
		       (((pool_idx + 1) << 8) | di_buf_idx),
		       ((di_buf->type << 8) | (di_buf->index)));
		if (recovery_flag == 0) {
			recovery_log_reason = 1;
			recovery_log_queue_idx =
				(start_pos_init << 8) | queue_idx;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
		di_buf = NULL;
	}

	if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_QUEUE) {
		if (di_buf)
			dimv3_print("%s: %p(%d,%d)\n", __func__, di_buf,
				  pool_idx, di_buf_idx);
		else
			dimv3_print("%s: %p\n", __func__, di_buf);
	}

	return di_buf;
}

/*--------------------------*/
#if 0 /*move to di_sys.c*/
u8 *dimv3_vmap(ulong addr, u32 size, bool *bflg)
{
	u8 *vaddr = NULL;
	ulong phys = addr;
	u32 offset = phys & ~PAGE_MASK;
	u32 npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = NULL;
	pgprot_t pgprot;
	int i;

	if (!PageHighMem(phys_to_page(phys)))
		return phys_to_virt(phys);

	if (offset)
		npages++;

	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	/*nocache*/
	pgprot = pgprot_writecombine(PAGE_KERNEL);

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		PR_ERR("the phy(%lx) vmaped fail, size: %d\n",
		       addr - offset, npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}

	vfree(pages);
#if 0
	if (debug_mode & 0x20) {
		dimv3_print("[HIGH-MEM-MAP] %s, pa(%lx) to va(%p), size: %d\n",
			  __func__, addr, vaddr + offset,
			  npages << PAGE_SHIFT);
	}
#endif
	*bflg = true;

	return vaddr + offset;
}

void dimv3_unmap_phyaddr(u8 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	vunmap(addr);
}
#endif

/*--------------------------*/
ssize_t
storev3_dump_mem(struct device *dev, struct device_attribute *attr,
	       const char *buf, size_t len)
{
	unsigned int n = 0, canvas_w = 0, canvas_h = 0;
	unsigned long nr_size = 0, dump_adr = 0;
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *post_vf = NULL;
	char *buf_orig, *ps, *token;
	char *parm[5] = { NULL };
	char delim1[3] = " ";
	char delim2[2] = "\n";
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buff = NULL;
	mm_segment_t old_fs;
	bool bflg_vmap = false;
	unsigned int channel = get_current_channel();/* debug only*/
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s  *de_devp = getv3_dim_de_devp();
	/*ary add 2019-07-2 being*/
	unsigned int indx;
	struct di_buf_s *pbuf_post;
	struct di_buf_s *pbuf_local;
	struct di_post_stru_s *ppost;
	struct di_mm_s *mm;
	struct di_ch_s *pch;
	/*************************/

	pch = get_chdata(channel);
	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (strcmp(parm[0], "capture") == 0) {
		di_buf = ppre->di_mem_buf_dup_p;
	} else if (strcmp(parm[0], "c_post") == 0) {
		/*ary add 2019-07-2*/
		/*p2: channel*/
		/*p3: index  */
		if (kstrtouint(parm[2], 0, &channel)) {
			PR_ERR("c_post:ch is not number\n");
			kfree(buf_orig);
			return 0;
		}
		if (kstrtouint(parm[3], 0, &indx)) {
			PR_ERR("c_post:ch is not number\n");
			kfree(buf_orig);
			return 0;
		}
		di_pr_info("c_post:ch[%d],index[%d]\n", channel, indx);
		mm = dim_mm_get(channel);

		ppre = get_pre_stru(channel);
		ppost = get_post_stru(channel);
		/*mm-0705	if (indx >= ppost->di_post_num) {*/
		if (indx >= /*mm->sts.num_post*/mm->cfg.num_post) {
			PR_ERR("c_post:index is overflow:%d[%d]\n", indx,
			       mm->cfg.num_post);
			kfree(buf_orig);
			return 0;
		}
		pbuf_post = get_buf_post(channel);
		di_buf = &pbuf_post[indx];
	} else if (strcmp(parm[0], "c_local") == 0) {
		/*ary add 2019-07-2*/
		/*p2: channel*/
		/*p3: index  */
		if (kstrtouint(parm[2], 0, &channel)) {
			PR_ERR("c_local:ch is not number\n");
			kfree(buf_orig);
			return 0;
		}
		if (kstrtouint(parm[3], 0, &indx)) {
			PR_ERR("c_local:ch is not number\n");
			kfree(buf_orig);
			return 0;
		}
		di_pr_info("c_local:ch[%d],index[%d]\n", channel, indx);

		ppre = get_pre_stru(channel);
		ppost = get_post_stru(channel);
		#if 0
		if (indx >= ppost->di_post_num) {
			PR_ERR("c_local:index is overflow:%d[%d]\n",
			       indx, ppost->di_post_num);
			kfree(buf_orig);
			return 0;
		}
		#endif
		pbuf_local = get_buf_local(channel);
		di_buf = &pbuf_local[indx];
	} else if (strcmp(parm[0], "capture_pready") == 0) {	/*ary add*/

		if (!div3_que_is_empty(channel, QUE_POST_READY)) {
			di_buf = div3_que_peek(channel, QUE_POST_READY);
			pr_info("get post ready di_buf:%d:0x%p\n",
				di_buf->index, di_buf);
		} else {
			pr_info("war:no post ready buf\n");
		}
	} else if (strcmp(parm[0], "capture_post") == 0) {
		if (div3_vf_l_peek(pch)) {
			post_vf = div3_vf_l_get(pch);
			if (!IS_ERR_OR_NULL(post_vf)) {
				di_buf = post_vf->private_data;
				div3_vf_l_put(post_vf, pch);
				pr_info("get post di_buf:%d:0x%p\n",
					di_buf->index, di_buf);
			} else {
				pr_info("war:peek no post buf, vfm[0x%p]\n",
					post_vf);
			}

			post_vf = NULL;
		} else {
			pr_info("war:can't peek post buf\n");
		}
	} else if (strcmp(parm[0], "capture_nrds") == 0) {
		dimv3_get_nr_ds_buf(&dump_adr, &nr_size);
	} else {
		PR_ERR("wrong dump cmd\n");
		kfree(buf_orig);
		return len;
	}
	if (nr_size == 0) {
		if (unlikely(!di_buf)) {
			pr_info("war:di_buf is null\n");
			kfree(buf_orig);
			return len;
		}
		canvas_w = di_buf->canvas_width[NR_CANVAS];
		canvas_h = di_buf->canvas_height;
		nr_size = canvas_w * canvas_h * 2;
		dump_adr = di_buf->nr_adr;

		pr_info("w=%d,h=%d,size=%ld,addr=%lu\n",
			canvas_w, canvas_h, nr_size, dump_adr);
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	/* pr_dbg("dump path =%s\n",dump_path); */
	filp = filp_open(parm[1], O_RDWR | O_CREAT, 0666);
	if (IS_ERR(filp)) {
		PR_ERR("create %s error.\n", parm[1]);
		kfree(buf_orig);
		return len;
	}
	dump_state_flag = 1;
	if (de_devp->flags & DI_MAP_FLAG) {
		/*buff = (void *)phys_to_virt(dump_adr);*/
		buff = dimv3_vmap(dump_adr, nr_size, &bflg_vmap);
		if (!buff) {
			if (nr_size <= 5222400) {
				pr_info("di_vap err\n");
				filp_close(filp, NULL);
				kfree(buf_orig);
				return len;

				/*try again:*/
				#ifdef HIS_V3
				PR_INF("vap err,size to 5222400, try again\n");
				nr_size = 5222400;
				buff = dimv3_vmap(dump_adr,
						  nr_size, &bflg_vmap);
				if (!buff) {
					filp_close(filp, NULL);
					kfree(buf_orig);
					return len;
				}
				#endif
			}
		}
	} else {
		buff = ioremap(dump_adr, nr_size);
	}
	if (IS_ERR_OR_NULL(buff))
		PR_ERR("%s: ioremap error.\n", __func__);
	vfs_write(filp, buff, nr_size, &pos);
/*	pr_dbg("di_chan2_buf_dup_p:\n	nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_chan2_buf_dup_p->nr_adr,
 * di_pre_stru.di_chan2_buf_dup_p->mtn_adr,
 * di_pre_stru.di_chan2_buf_dup_p->cnt_adr);
 * pr_dbg("di_inp_buf:\n	nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_inp_buf->nr_adr,
 * di_pre_stru.di_inp_buf->mtn_adr,
 * di_pre_stru.di_inp_buf->cnt_adr);
 * pr_dbg("di_wr_buf:\n	nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_wr_buf->nr_adr,
 * di_pre_stru.di_wr_buf->mtn_adr,
 * di_pre_stru.di_wr_buf->cnt_adr);
 * pr_dbg("di_mem_buf_dup_p:\n  nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_mem_buf_dup_p->nr_adr,
 * di_pre_stru.di_mem_buf_dup_p->mtn_adr,
 * di_pre_stru.di_mem_buf_dup_p->cnt_adr);
 * pr_dbg("di_mem_start=%u\n",di_mem_start);
 */
	vfs_fsync(filp, 0);
	pr_info("write buffer 0x%lx  to %s.\n", dump_adr, parm[1]);
	if (bflg_vmap)
		dimv3_unmap_phyaddr(buff);

	if (!(de_devp->flags & DI_MAP_FLAG))
		iounmap(buff);
	dump_state_flag = 0;
	filp_close(filp, NULL);
	set_fs(old_fs);
	kfree(buf_orig);
	return len;
}

#ifdef HIS_V3
/*ary add 2019-12-16*/
void dbg_dump_pic(void)
{
	unsigned int n = 0, canvas_w = 0, canvas_h = 0;
	unsigned long nr_size = 0, dump_adr = 0;
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *post_vf = NULL;
	char *buf_orig, *ps, *token;
	char *parm[5] = { NULL };
	char delim1[3] = " ";
	char delim2[2] = "\n";
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buff = NULL;
	mm_segment_t old_fs;
	bool bflg_vmap = false;
	unsigned int channel = get_current_channel();/* debug only*/
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s  *de_devp = getv3_dim_de_devp();
	/*ary add 2019-07-2 being*/
	unsigned int indx;
	struct di_buf_s *pbuf_post;
	struct di_buf_s *pbuf_local;
	struct di_post_stru_s *ppost;
	struct di_mm_s *mm;
	struct di_ch_s *pch;

	if (nr_size == 0) {
		if (unlikely(!di_buf)) {
			pr_info("war:di_buf is null\n");
			kfree(buf_orig);
			return len;
		}
		canvas_w = di_buf->canvas_width[NR_CANVAS];
		canvas_h = di_buf->canvas_height;
		nr_size = canvas_w * canvas_h * 2;
		dump_adr = di_buf->nr_adr;

		pr_info("w=%d,h=%d,size=%ld,addr=%lu\n",
			canvas_w, canvas_h, nr_size, dump_adr);
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	/* pr_dbg("dump path =%s\n",dump_path); */
	filp = filp_open(parm[1], O_RDWR | O_CREAT, 0666);
	if (IS_ERR(filp)) {
		PR_ERR("create %s error.\n", parm[1]);
		kfree(buf_orig);
		return len;
	}
	dump_state_flag = 1;
	if (de_devp->flags & DI_MAP_FLAG) {
		/*buff = (void *)phys_to_virt(dump_adr);*/
		buff = dimv3_vmap(dump_adr, nr_size, &bflg_vmap);
		if (!buff) {
			if (nr_size <= 5222400) {
				pr_info("di_vap err\n");
				filp_close(filp, NULL);
				kfree(buf_orig);
				return len;

				/*try again:*/
				PR_INF("vap err,size to 5222400, try again\n");
				nr_size = 5222400;
				buff = dimv3_vmap(dump_adr,
						  nr_size, &bflg_vmap);
				if (!buff) {
					filp_close(filp, NULL);
					kfree(buf_orig);
					return len;
				}
			}
		}
	} else {
		buff = ioremap(dump_adr, nr_size);
	}
	if (IS_ERR_OR_NULL(buff))
		PR_ERR("%s: ioremap error.\n", __func__);
	vfs_write(filp, buff, nr_size, &pos);
	vfs_fsync(filp, 0);
	pr_info("write buffer 0x%lx  to %s.\n", dump_adr, parm[1]);
	if (bflg_vmap)
		dimv3_unmap_phyaddr(buff);

	if (!(de_devp->flags & DI_MAP_FLAG))
		iounmap(buff);
	dump_state_flag = 0;
	filp_close(filp, NULL);
	set_fs(old_fs);
	kfree(buf_orig);
	return len;
}
#endif

static void recycle_vframe_type_pre(struct di_buf_s *di_buf,
				    unsigned int channel);
static void recycle_vframe_type_post(struct di_buf_s *di_buf,
				     unsigned int channel);
//static void add_dummy_vframe_type_pre(struct di_buf_s *src_buf,
//				      unsigned int channel);
#ifdef DI_BUFFER_DEBUG
static void
recycle_vframe_type_post_print(struct di_buf_s *di_buf,
			       const char *func,
			       const int line);
#endif

static void dis2_di(void)
{
	ulong flags = 0, irq_flag2 = 0;
	unsigned int channel = get_current_channel();/* debug only*/
	void **pvframe_in = getv3_vframe_in(channel);
	void *pitf;

	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_ch_s *pch;
	struct dim_itf_ops_s	*pvfmops;

	pch = get_chdata(channel);
	pvfmops = &pch->interf.opsi;

	set_init_flag(channel, false);/*init_flag = 0;*/
	di_lock_irqfiq_save(irq_flag2);
/* vf_unreg_provider(&di_vf_prov); */
	pwv3_vf_light_unreg_provider(channel);
	di_unlock_irqfiq_restore(irq_flag2);
	set_reg_flag(channel, false);
	spin_lock_irqsave(&plistv3_lock, flags);
	di_lock_irqfiq_save(irq_flag2);
	if (ppre->di_inp_buf) {
		pitf = pvframe_in[ppre->di_inp_buf->index];
		if (pitf) {
			//pw_vf_put(pvframe_in[ppre->di_inp_buf->index],
			//	  channel);

			//pw_vf_notify_provider(channel,
			//	VFRAME_EVENT_RECEIVER_PUT, NULL);
			pvfmops->put(pitf, pch);
			pitf = NULL;
		}
		ppre->di_inp_buf->c.invert_top_bot_flag = 0;

		div3_que_in(channel, QUE_IN_FREE, ppre->di_inp_buf);
		ppre->di_inp_buf = NULL;
	}
	dimv3_uninit_buf(0, channel);
	if (get_blackout_policy()) {
		dimv3_DI_Wr(DI_CLKG_CTRL, 0x2);
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
			dimhv3_enable_di_post_mif(GATE_OFF);
			dimv3_post_gate_control(false);
			dimv3_top_gate_control(false, false);
		}
	}

	if (dimp_get(eDI_MP_post_wr_en) && dimp_get(eDI_MP_post_wr_support))
		dimv3_set_power_control(0);

	di_unlock_irqfiq_restore(irq_flag2);
	spin_unlock_irqrestore(&plistv3_lock, flags);
}

ssize_t
storev3_config(struct device *dev,
	     struct device_attribute *attr,
	     const char *buf, size_t count)
{
	/*int rc = 0;*/
	char *parm[2] = { NULL }, *buf_orig;
	unsigned int channel = get_current_channel();/* debug only*/
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	dimv3_parse_cmd_params(buf_orig, (char **)(&parm));

	if (strncmp(buf, "disable", 7) == 0) {
		dimv3_print("%s: disable\n", __func__);

		if (get_init_flag(channel)) {/*if (init_flag) {*/
			ppre->disable_req_flag = 1;

			while (ppre->disable_req_flag)
				usleep_range(1000, 1001);
		}
	} else if (strncmp(buf, "dis2", 4) == 0) {
		dis2_di();
	}
	kfree(buf_orig);
	return count;
}
#if 0

static unsigned char is_progressive(vframe_t *vframe)
{
	unsigned char ret = 0;

	ret = ((vframe->type & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE);
	return ret;
}

static unsigned char is_source_change(vframe_t *vframe, unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

#define VFRAME_FORMAT_MASK      \
	(VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_444 | \
	 VIDTYPE_MVC)
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
}
#endif
/*
 * static unsigned char is_vframe_type_change(vframe_t* vframe)
 * {
 * if(
 * (di_pre_stru.cur_prog_flag!=is_progressive(vframe))||
 * ((di_pre_stru.cur_inp_type&VFRAME_FORMAT_MASK)!=
 * (vframe->type&VFRAME_FORMAT_MASK))
 * )
 * return 1;
 *
 * return 0;
 * }
 */
/*static int trick_mode; move to wmode*/
#if 0
unsigned char dim_is_bypass(struct vframe_s *vf_in, unsigned int channel)
{
	unsigned int vtype = 0;
	int ret = 0;
	static vframe_t vf_tmp;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (dimp_get(eDI_MP_di_debug_flag) & 0x10000) /* for debugging */ {
		if (cfgg(DBG) & DI_BIT1)
			PR_INF("%s:dbg flg\n", __func__);
		return (dimp_get(eDI_MP_di_debug_flag) >> 17) & 0x1;
	}

	if (div3_cfgx_get(channel, eDI_CFGX_BYPASS_ALL)) {
		if (cfgg(DBG) & DI_BIT1)
			PR_INF("%s:flg\n", __func__);
		return 1;
	}
	#if 0
	if (ppre->cur_prog_flag		&&
	    ((ppre->cur_width > 1920)	||
	    (ppre->cur_height > 1080)	||
	    (ppre->cur_inp_type & VIDTYPE_VIU_444))
	    )
		return 1;
	#else
	if (ppre->cur_prog_flag) {
		if (ppre->cur_inp_type & VIDTYPE_VIU_444) {
			if (cfgg(DBG) & DI_BIT1)
				PR_INF("%s:3\n", __func__);
			return 1;
		}
		if (!dimp_get(eDI_MP_4k_test)	&&
		    (ppre->cur_width > 1920	||
		     ppre->cur_height > 1080)) {
			if (cfgg(DBG) & DI_BIT1)
				PR_INF("%s:4\n", __func__);
			return 1;
		}
	}
	#endif
	if ((ppre->cur_width < 16) || (ppre->cur_height < 16))
		return 1;

	if (ppre->cur_inp_type & VIDTYPE_MVC)
		return 1;

	if (ppre->cur_source_type == VFRAME_SOURCE_TYPE_PPMGR)
		return 1;

	#if 0
	if (dimp_get(eDI_MP_bypass_trick_mode)) {
		int trick_mode_fffb = 0;
		int trick_mode_i = 0;

		if (dimp_get(eDI_MP_bypass_trick_mode) & 0x1)
			query_video_status(0, &trick_mode_fffb);
		if (dimp_get(eDI_MP_bypass_trick_mode) & 0x2)
			query_video_status(1, &trick_mode_i);
		trick_mode = trick_mode_fffb | (trick_mode_i << 1);
		if (trick_mode)
			return 1;
	}
	#else
	if (dimv3_get_trick_mode())
		return 1;

	#endif
	if (dimp_get(eDI_MP_bypass_3d)	&&
	    (ppre->source_trans_fmt != 0))
		return 1;

/*prot is conflict with di post*/
	if (vf_in && vf_in->video_angle)
		return 1;
	if (vf_in && (vf_in->type & VIDTYPE_PIC))
		return 1;
#if 0
	if (vf_in && (vf_in->type & VIDTYPE_COMPRESS))
		return 1;
#endif
	if ((dimp_get(eDI_MP_di_vscale_skip_enable) & 0x4) &&
	    vf_in && !dimp_get(eDI_MP_post_wr_en)) {
		/*--------------------------*/
		if (vf_in->type & VIDTYPE_COMPRESS) {
			vf_tmp.width = vf_in->compWidth;
			vf_tmp.height = vf_in->compHeight;
			if (vf_tmp.width > 1920 || vf_tmp.height > 1088)
				return 1;
		}
		/*--------------------------*/
		/*backup vtype,set type as progressive*/
		vtype = vf_in->type;
		vf_in->type &= (~VIDTYPE_TYPEMASK);
		vf_in->type &= (~VIDTYPE_VIU_NV21);
		vf_in->type |= VIDTYPE_VIU_SINGLE_PLANE;
		vf_in->type |= VIDTYPE_VIU_FIELD;
		vf_in->type |= VIDTYPE_PRE_INTERLACE;
		vf_in->type |= VIDTYPE_VIU_422;
		ret = extv3_ops.get_current_vscale_skip_count(vf_in);
		/*di_vscale_skip_count = (ret&0xff);*/
		dimp_set(eDI_MP_di_vscale_skip_count, ret & 0xff);
		/*vpp_3d_mode = ((ret>>8)&0xff);*/
		dimp_set(eDI_MP_vpp_3d_mode, ((ret >> 8) & 0xff));
		vf_in->type = vtype;
		if (dimp_get(eDI_MP_di_vscale_skip_count) > 0 ||
		    (dimp_get(eDI_MP_vpp_3d_mode)
			#ifdef DET3D
			&& (!dimp_get(eDI_MP_det3d_en))
			#endif
			)
			)
			return 1;
	}

	return 0;
}
#endif
//static bool need_bypass(struct vframe_s *vf);


static unsigned char is_bypass_post(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (dimp_get(eDI_MP_di_debug_flag) & 0x40000) /* for debugging */
		return (dimp_get(eDI_MP_di_debug_flag) >> 19) & 0x1;

	/*prot is conflict with di post*/
	if (ppre->orientation)
		return 1;
	if (dimp_get(eDI_MP_bypass_post))
		return 1;

#ifdef DET3D
	if (ppre->vframe_interleave_flag != 0)
		return 1;

#endif
	return 0;
}


#ifdef DI_USE_FIXED_CANVAS_IDX
static int di_post_idx[2][6];
static int di_pre_idx[2][10];
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
//static unsigned int di_inp_idx[3];
#else
static int di_wr_idx;
#endif

int dimv3_get_canvas(void)
{
	unsigned int pre_num = 7, post_num = 6;
	struct di_dev_s  *de_devp = getv3_dim_de_devp();
	struct di_hpre_s  *phpre = get_hw_pre();

	if (dimp_get(eDI_MP_mcpre_en)) {
		/* mem/chan2/nr/mtn/contrd/contrd2/
		 * contw/mcinfrd/mcinfow/mcvecw
		 */
		pre_num = 10;
		/* buf0/buf1/buf2/mtnp/mcvec */
		post_num = 6;
	}
	if (extv3_ops.cvs_alloc_table("di_pre",
				      &di_pre_idx[0][0],
				      pre_num,
				      CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s allocate di pre canvas error.\n", __func__);
		return 1;
	}

		#if 0
		for (i = 0; i < pre_num; i++)
			di_pre_idx[1][i] = di_pre_idx[0][i];
		#else
		memcpy(&di_pre_idx[1][0],
		       &di_pre_idx[0][0], sizeof(int) * pre_num);
		#endif

	if (extv3_ops.cvs_alloc_table("di_post",
				      &di_post_idx[0][0],
				      post_num,
				      CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s allocate di post canvas error.\n", __func__);
		return 1;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (extv3_ops.cvs_alloc_table("di_post",
				      &di_post_idx[1][0],
				      post_num,
				      CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s allocate di post canvas error.\n", __func__);
		return 1;
	}
#else
	for (i = 0; i < post_num; i++)
		di_post_idx[1][i] = di_post_idx[0][i];
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (extv3_ops.cvs_alloc_table("di_inp",
				      &phpre->di_inp_idx[0], 3,
				      CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s allocat di inp canvas error.\n", __func__);
		return 1;
	}
	PR_INF("support multi decoding %u~%u~%u.\n",
	       phpre->di_inp_idx[0],
	       phpre->di_inp_idx[1],
	       phpre->di_inp_idx[2]);
#endif
	if (de_devp->post_wr_support == 0)
		return 0;

#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (extv3_ops.cvs_alloc_table("di_wr",
				      &di_wr_idx, 1,
				      CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s allocat di write back canvas error.\n",
		       __func__);
		return 1;
	}
	PR_INF("support post write back %u.\n", di_wr_idx);
#endif
	return 0;
}

static void config_canvas_idx(struct di_buf_s *di_buf, int nr_canvas_idx,
			      int mtn_canvas_idx)
{
	unsigned int height = 0;

	if (!di_buf)
		return;
	if (di_buf->c.canvas_config_flag == 1) {
		if (nr_canvas_idx >= 0) {
			/* linked two interlace buffer should double height*/
			if (di_buf->c.di_wr_linked_buf)
				height = (di_buf->canvas_height << 1);
			else
				height =  di_buf->canvas_height;
			di_buf->nr_canvas_idx = nr_canvas_idx;
			canvas_config(nr_canvas_idx, di_buf->nr_adr,
				      di_buf->canvas_width[NR_CANVAS],
				      height, 0, 0);
		}
	} else if (di_buf->c.canvas_config_flag == 2) {
		if (nr_canvas_idx >= 0) {
			di_buf->nr_canvas_idx = nr_canvas_idx;
			canvas_config(nr_canvas_idx, di_buf->nr_adr,
				      di_buf->canvas_width[NR_CANVAS],
				      di_buf->canvas_height, 0, 0);
		}
		if (mtn_canvas_idx >= 0) {
			di_buf->mtn_canvas_idx = mtn_canvas_idx;
			canvas_config(mtn_canvas_idx, di_buf->mtn_adr,
				      di_buf->canvas_width[MTN_CANVAS],
				      di_buf->canvas_height, 0, 0);
		}
	} else {
		PR_ERR("%s:do nothing\n", __func__); /*ary add*/
	}
	if (nr_canvas_idx >= 0) {
		di_buf->c.vmode.canvas0Addr = di_buf->nr_canvas_idx;
		di_buf->c.vmode.canvas1Addr = di_buf->nr_canvas_idx;
	}
}

static void config_cnt_canvas_idx(struct di_buf_s *di_buf,
				  unsigned int cnt_canvas_idx)
{
	if (!di_buf)
		return;

	di_buf->cnt_canvas_idx = cnt_canvas_idx;
	canvas_config(cnt_canvas_idx, di_buf->cnt_adr,
		      di_buf->canvas_width[MTN_CANVAS],
		      di_buf->canvas_height, 0, 0);
}

static void config_mcinfo_canvas_idx(struct di_buf_s *di_buf,
				     int mcinfo_canvas_idx)
{
	if (!di_buf)
		return;

	di_buf->mcinfo_canvas_idx = mcinfo_canvas_idx;
	canvas_config(mcinfo_canvas_idx,
		      di_buf->mcinfo_adr,
		      di_buf->canvas_height_mc, 2, 0, 0);
}

static void config_mcvec_canvas_idx(struct di_buf_s *di_buf,
				    int mcvec_canvas_idx)
{
	if (!di_buf)
		return;

	di_buf->mcvec_canvas_idx = mcvec_canvas_idx;
	canvas_config(mcvec_canvas_idx,
		      di_buf->mcvec_adr,
		      di_buf->canvas_width[MV_CANVAS],
		      di_buf->canvas_height, 0, 0);
}

#else

static void config_canvas(struct di_buf_s *di_buf)
{
	unsigned int height = 0;

	if (!di_buf)
		return;

	if (di_buf->c.canvas_config_flag == 1) {
		/* linked two interlace buffer should double height*/
		if (di_buf->c.di_wr_linked_buf)
			height = (di_buf->canvas_height << 1);
		else
			height =  di_buf->canvas_height;
		canvas_config(di_buf->nr_canvas_idx, di_buf->nr_adr,
			      di_buf->canvas_width[NR_CANVAS], height, 0, 0);
		di_buf->c.canvas_config_flag = 0;
	} else if (di_buf->c.canvas_config_flag == 2) {
		canvas_config(di_buf->nr_canvas_idx, di_buf->nr_adr,
			      di_buf->canvas_width[MV_CANVAS],
			      di_buf->canvas_height, 0, 0);
		canvas_config(di_buf->mtn_canvas_idx, di_buf->mtn_adr,
			      di_buf->canvas_width[MTN_CANVAS],
			      di_buf->canvas_height, 0, 0);
		di_buf->c.canvas_config_flag = 0;
	}
}

#endif

#if 0	/*no use*/
/*******************************************
 *
 *
 ******************************************/
#define DI_KEEP_BUF_SIZE 3
static struct di_buf_s *di_keep_buf[DI_KEEP_BUF_SIZE];
static int di_keep_point;

void keep_buf_clear(void)
{
	int i;

	for (i = 0; i < DI_KEEP_BUF_SIZE; i++)
		di_keep_buf[i] = NULL;

	di_keep_point = -1;
}

void keep_buf_in(struct di_buf_s *ready_buf)
{
	di_keep_point++;
	if (di_keep_point >= DI_KEEP_BUF_SIZE)
		di_keep_point = 0;
	di_keep_buf[di_keep_point] = ready_buf;
}

void keep_buf_in_full(struct di_buf_s *ready_buf)
{
	int i;

	keep_buf_in(ready_buf);
	for (i = 0; i < DI_KEEP_BUF_SIZE; i++) {
		if (!di_keep_buf[i])
			di_keep_buf[i] = ready_buf;
	}
}
#endif

unsigned int dim_ins_cnt_post_size(unsigned int w, unsigned int h)
{
//	unsigned int w, h;
	unsigned int nr_width;
	unsigned int nr_canvas_width;
	unsigned int canvas_align_width = 32;
	unsigned int post_size;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	if (dimp_get(eDI_MP_nr10bit_support)) {
		if (dimp_get(eDI_MP_full_422_pack))
			nr_width = (w * 5) / 4;
		else
			nr_width = (w * 3) / 2;
	} else {
		nr_width = w;
	}
	nr_canvas_width = nr_width << 1;
	nr_canvas_width = roundup(nr_canvas_width, canvas_align_width);
	nr_width = nr_canvas_width >> 1;
	PR_INF("\t:w[%d]h[%d]\n", nr_width, h);
	post_size = nr_canvas_width * h * 2;
	PR_INF("size:post:0x%x\n", post_size);
	post_size = roundup(post_size, PAGE_SIZE);

	return post_size;
}

int div3_cnt_buf(int width, int height, int prog_flag, int mc_mm,
	       int bit10_support, int pack422)
{
	int canvas_height = height + 8;
	unsigned int di_buf_size = 0, di_post_buf_size = 0, mtn_size = 0;
	unsigned int nr_size = 0, count_size = 0, mv_size = 0, mc_size = 0;
	unsigned int nr_width = width, mtn_width = width, mv_width = width;
	unsigned int nr_canvas_width = width, mtn_canvas_width = width;
	unsigned int mv_canvas_width = width, canvas_align_width = 32;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	if (dimp_get(eDI_MP_nr10bit_support)) {
		if (dimp_get(eDI_MP_full_422_pack))
			nr_width = (width * 5) / 4;
		else
			nr_width = (width * 3) / 2;
	} else {
		nr_width = width;
	}
	/* make sure canvas width must be divided by 256bit|32byte align */
	nr_canvas_width = nr_width << 1;
	mtn_canvas_width = mtn_width >> 1;
	mv_canvas_width = (mv_width << 1) / 5;
	nr_canvas_width = roundup(nr_canvas_width, canvas_align_width);
	mtn_canvas_width = roundup(mtn_canvas_width, canvas_align_width);
	mv_canvas_width = roundup(mv_canvas_width, canvas_align_width);
	nr_width = nr_canvas_width >> 1;
	mtn_width = mtn_canvas_width << 1;
	mv_width = (mv_canvas_width * 5) >> 1;

	if (prog_flag) {
		di_buf_size = nr_width * canvas_height * 2;
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);

	} else {
		/*pr_info("canvas_height=%d\n", canvas_height);*/

		/*nr_size(bits)=w*active_h*8*2(yuv422)
		 * mtn(bits)=w*active_h*4
		 * cont(bits)=w*active_h*4 mv(bits)=w*active_h/5*16
		 * mcinfo(bits)=active_h*16
		 */
		nr_size = (nr_width * canvas_height) * 8 * 2 / 16;
		mtn_size = (mtn_width * canvas_height) * 4 / 16;
		count_size = (mtn_width * canvas_height) * 4 / 16;
		mv_size = (mv_width * canvas_height) / 5;
		/*mc_size = canvas_height;*/
		mc_size = roundup(canvas_height >> 1, canvas_align_width) << 1;
		if (mc_mm) {
			di_buf_size = nr_size + mtn_size + count_size +
				mv_size + mc_size;
		} else {
			di_buf_size = nr_size + mtn_size + count_size;
		}
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);
	}

	PR_INF("size:0x%x\n", di_buf_size);
	PR_INF("\t%-15s:0x%x\n", "nr_size", nr_size);
	PR_INF("\t%-15s:0x%x\n", "count", count_size);
	PR_INF("\t%-15s:0x%x\n", "mtn", mtn_size);
	PR_INF("\t%-15s:0x%x\n", "mv", mv_size);
	PR_INF("\t%-15s:0x%x\n", "mcinfo", mc_size);
	PR_INF("\t%-15s:0x%x\n", "cvs_w", nr_width);
	PR_INF("\t:w[%d]h[%d]\n", nr_width, canvas_height);
	di_post_buf_size = nr_width * canvas_height * 2;
	PR_INF("size:post:0x%x\n", di_post_buf_size);
	di_post_buf_size = roundup(di_post_buf_size, PAGE_SIZE);
	PR_INF("size:post:0x%x\n", di_post_buf_size);

	return 0;
}

int dim_ins_cnt_post_cvs_size(struct di_buf_s	*di_buf,
			      struct di_buffer	*ins_buf)
{
	unsigned int w, h;
	unsigned int nr_width;
	unsigned int nr_canvas_width;
	unsigned int canvas_align_width = 32;
	unsigned int fmt = 0; /*0: 422; 1: nv21 or nv12*/

	w = ins_buf->vf->width;
	h = ins_buf->vf->height;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	//di_buf->vframe->type = ins_buf->vf->type;
	if (ins_buf->vf->type & VIDTYPE_VIU_NV12 ||
	    ins_buf->vf->type & VIDTYPE_VIU_NV21)
		fmt = 1;
	else if (ins_buf->vf->type & VIDTYPE_VIU_422)
		fmt = 0;
	else {
		fmt = 0;
		PR_WARN("%s:vtype:0x%x\n", __func__, ins_buf->vf->type);
	}
#ifdef DIM_OUT_NV21
	/* base on vf */
	if (!fmt) {
		/* 422*/
		if (dimp_get(eDI_MP_nr10bit_support)	&&
		    (ins_buf->vf->bitdepth & BITDEPTH_Y10)) {
			if ((ins_buf->vf->bitdepth & FULL_PACK_422_MODE))
				nr_width = (w * 5) / 4;
			else
				nr_width = (w * 3) / 2;
		} else {
			nr_width = w;
		}
	} else {
		nr_width = w;
	}
#else

	if (dimp_get(eDI_MP_nr10bit_support)) {
		if (dimp_get(eDI_MP_full_422_pack))
			nr_width = (w * 5) / 4;
		else
			nr_width = (w * 3) / 2;
	} else {
		nr_width = w;
	}
#endif
	if (fmt == 1) {
		/* nv 21 or nv12 */
		nr_canvas_width = nr_width;
	} else {
		/* 422 */
		nr_canvas_width = nr_width << 1;
	}
	nr_canvas_width = roundup(nr_canvas_width, canvas_align_width);
	//nr_width = nr_canvas_width >> 1;

	di_buf->canvas_width[NR_CANVAS] = nr_canvas_width;
	di_buf->canvas_height = h;

	return 0;
}

/* size is from di_buf*/
int dim_ins_cnt_post_cvs_size2(struct di_buf_s	*di_buf,
			      struct di_buffer	*ins_buf,
			      unsigned int ch)
{
	unsigned int w, h;
	unsigned int nr_width;
	unsigned int nr_canvas_width;
	unsigned int canvas_align_width = 32;
	struct dim_inter_s *pintf;
	enum di_output_format fmt = 0; /*0: 422; 1: nv21 or nv12*/
	struct vframe_s *vfmout;

	pintf = get_dev_intf(ch);

	w = ins_buf->vf->width;
	h = ins_buf->vf->height;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	//di_buf->vframe->type = ins_buf->vf->type;
	fmt = pintf->u.dinst.parm.output_format;

#ifdef DIM_OUT_NV21
	/* base on vf */
	if (fmt == DI_OUTPUT_422) {
		/* 422*/
		if (dimp_get(eDI_MP_nr10bit_support)	&&
		    (ins_buf->vf->bitdepth & BITDEPTH_Y10)) {
			if ((ins_buf->vf->bitdepth & FULL_PACK_422_MODE))
				nr_width = (w * 5) / 4;
			else
				nr_width = (w * 3) / 2;
		} else {
			nr_width = w;
		}
	} else {
		nr_width = w;
	}
#else

	if (dimp_get(eDI_MP_nr10bit_support)) {
		if (dimp_get(eDI_MP_full_422_pack))
			nr_width = (w * 5) / 4;
		else
			nr_width = (w * 3) / 2;
	} else {
		nr_width = w;
	}
#endif
	if ((fmt == DI_OUTPUT_NV12) ||
	    (fmt == DI_OUTPUT_NV21)) {
		/* nv 21 or nv12 */
		nr_canvas_width = nr_width;
	} else {
		/* 422 */
		nr_canvas_width = nr_width << 1;
	}
	/* set out format */
	vfmout = ins_buf->vf;
	vfmout->type &= ~(VIDTYPE_VIU_NV12	|
			  VIDTYPE_VIU_444	|
			  VIDTYPE_VIU_NV21	|
			  VIDTYPE_VIU_422	|
			  VIDTYPE_VIU_SINGLE_PLANE	|
			  VIDTYPE_PRE_INTERLACE);

	vfmout->bitdepth &= ~(BITDEPTH_MASK | FULL_PACK_422_MODE);
	if (fmt == DI_OUTPUT_422) {
		vfmout->type |= VIDTYPE_VIU_422;

		vfmout->bitdepth |= (FULL_PACK_422_MODE	|
					BITDEPTH_Y10		|
					BITDEPTH_U10		|
					BITDEPTH_V10);
	} else if (fmt == DI_OUTPUT_NV21) {
		vfmout->type |= VIDTYPE_VIU_NV21;
		vfmout->bitdepth |= (BITDEPTH_Y8		|
					BITDEPTH_U8		|
					BITDEPTH_V8);
	} else {
		vfmout->type |= VIDTYPE_VIU_NV12;
		vfmout->bitdepth |= (BITDEPTH_Y8		|
					BITDEPTH_U8		|
					BITDEPTH_V8);
	}

	nr_canvas_width = roundup(nr_canvas_width, canvas_align_width);
	//nr_width = nr_canvas_width >> 1;
#if 0
	di_buf->canvas_width[NR_CANVAS] = nr_canvas_width;
	di_buf->canvas_height = h;

#endif
	return 0;
}


static int di_init_buf(int width, int height, unsigned char prog_flag,
		       unsigned int channel)
{
	int i;
	int canvas_height = height + 8;
	struct page *tmp_page = NULL;
	unsigned int di_buf_size = 0, di_post_buf_size = 0, mtn_size = 0;
	unsigned int nr_size = 0, count_size = 0, mv_size = 0, mc_size = 0;
	unsigned int nr_width = width, mtn_width = width, mv_width = width;
	unsigned int nr_canvas_width = width, mtn_canvas_width = width;
	unsigned int mv_canvas_width = width, canvas_align_width = 32;
	unsigned long di_post_mem = 0, nrds_mem = 0;
	void **pvframe_in = getv3_vframe_in(channel);
	struct vframe_s *pvframe_in_dup = get_vframe_in_dup(channel);
	struct vframe_s *pvframe_local = get_vframe_local(channel);
	struct vframe_s *pvframe_post = get_vframe_post(channel);
	struct di_buffer *pins = get_post_ins(channel);
	struct di_buf_s *pbuf_local = get_buf_local(channel);
	struct di_buf_s *pbuf_in = get_buf_in(channel);
	struct di_buf_s *pbuf_post = get_buf_post(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_buf_s *keep_buf = ppost->keep_buf;
	struct di_dev_s *de_devp = getv3_dim_de_devp();
	/*struct di_buf_s *keep_buf_post = ppost->keep_buf_post;*/
	struct di_mm_s *mm = dim_mm_get(channel); /*mm-0705*/
	struct dim_inter_s *pintf;

	unsigned int mem_st_local;

	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	//PR_INF("%s:begin\n", __func__);
	frame_count = 0;
	disp_frame_count = 0;
	cur_post_ready_di_buf = NULL;
	/* decoder'buffer had been releae no need put */
	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		pvframe_in[i] = NULL;
	/*pre init*/
	memset(ppre, 0, sizeof(struct di_pre_stru_s));

	if (dimp_get(eDI_MP_nr10bit_support)) {
		if (dimp_get(eDI_MP_full_422_pack))
			nr_width = (width * 5) / 4;
		else
			nr_width = (width * 3) / 2;
	} else {
		nr_width = width;
	}
	/* make sure canvas width must be divided by 256bit|32byte align */
	nr_canvas_width = nr_width << 1;
	mtn_canvas_width = mtn_width >> 1;
	mv_canvas_width = (mv_width << 1) / 5;
	nr_canvas_width = roundup(nr_canvas_width, canvas_align_width);
	mtn_canvas_width = roundup(mtn_canvas_width, canvas_align_width);
	mv_canvas_width = roundup(mv_canvas_width, canvas_align_width);
	nr_width = nr_canvas_width >> 1;
	mtn_width = mtn_canvas_width << 1;
	mv_width = (mv_canvas_width * 5) >> 1;

	if (prog_flag) {
		ppre->prog_proc_type = 1;
		mm->cfg.buf_alloc_mode = 1;
		di_buf_size = nr_width * canvas_height * 2;
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);
	} else {
		/*pr_info("canvas_height=%d\n", canvas_height);*/
		ppre->prog_proc_type = 0;
		mm->cfg.buf_alloc_mode = 0;
		/*nr_size(bits) = w * active_h * 8 * 2(yuv422)
		 * mtn(bits) = w * active_h * 4
		 * cont(bits) = w * active_h * 4 mv(bits) = w * active_h / 5*16
		 * mcinfo(bits) = active_h * 16
		 */
		nr_size = (nr_width * canvas_height) * 8 * 2 / 16;
		mtn_size = (mtn_width * canvas_height) * 4 / 16;
		count_size = (mtn_width * canvas_height) * 4 / 16;
		mv_size = (mv_width * canvas_height) / 5;
		/*mc_size = canvas_height;*/
		mc_size = roundup(canvas_height >> 1, canvas_align_width) << 1;
		if (mc_mem_alloc) {
			di_buf_size = nr_size + mtn_size + count_size +
				mv_size + mc_size;
		} else {
			di_buf_size = nr_size + mtn_size + count_size;
		}
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);
	}
	/*de_devp->buffer_size = di_buf_size;*/
	mm->cfg.size_local = di_buf_size;
	#if 0
	ppre->nr_size = nr_size;
	ppre->count_size = count_size;
	ppre->mtn_size = mtn_size;
	ppre->mv_size = mv_size;
	ppre->mcinfo_size = mc_size;
	#else
	mm->cfg.nr_size = nr_size;
	mm->cfg.count_size = count_size;
	mm->cfg.mtn_size = mtn_size;
	mm->cfg.mv_size = mv_size;
	mm->cfg.mcinfo_size = mc_size;
	#endif
	dimp_set(eDI_MP_same_field_top_count, 0);
	same_field_bot_count = 0;
	dbg_init("size:\n");
	dbg_init("\t%-15s:0x%x\n", "nr_size", mm->cfg.nr_size);
	dbg_init("\t%-15s:0x%x\n", "count", mm->cfg.count_size);
	dbg_init("\t%-15s:0x%x\n", "mtn", mm->cfg.mtn_size);
	dbg_init("\t%-15s:0x%x\n", "mv", mm->cfg.mv_size);
	dbg_init("\t%-15s:0x%x\n", "mcinfo", mm->cfg.mcinfo_size);

	/**********************************************/
	/* que init */
	/**********************************************/

	queuev3_init(channel, mm->cfg.num_local);
	div3_que_init(channel); /*new que*/

	mem_st_local = di_get_mem_start(channel);

	/**********************************************/
	/* local buf init */
	/**********************************************/

	for (i = 0; i < mm->cfg.num_local; i++) {
		struct di_buf_s *di_buf = &pbuf_local[i];
		int ii = USED_LOCAL_BUF_MAX;

		if (!IS_ERR_OR_NULL(keep_buf)) {
			for (ii = 0; ii < USED_LOCAL_BUF_MAX; ii++) {
				if (di_buf == keep_buf->c.di_buf_dup_p[ii]) {
					dimv3_print("%s skip %d\n", __func__,
						    i);
					break;
				}
			}
		}

		if (ii >= USED_LOCAL_BUF_MAX) {
			/* backup cma pages */
			tmp_page = di_buf->pages;
			/*clean di_buf*/
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->pages = tmp_page;
			di_buf->type = VFRAME_TYPE_LOCAL;
			di_buf->canvas_width[NR_CANVAS] = nr_canvas_width;
			di_buf->canvas_width[MTN_CANVAS] = mtn_canvas_width;
			di_buf->canvas_width[MV_CANVAS] = mv_canvas_width;
			/* di_buf->c.sts = 0; need set*/
			if (prog_flag) {
				di_buf->canvas_height = canvas_height;
				di_buf->canvas_height_mc = canvas_height;
				di_buf->nr_adr = mem_st_local +
					di_buf_size * i;
				di_buf->c.canvas_config_flag = 1;
			} else {
				di_buf->canvas_height = (canvas_height >> 1);
				di_buf->canvas_height_mc =
					roundup(di_buf->canvas_height,
						canvas_align_width);
				di_buf->nr_adr = mem_st_local +
					di_buf_size * i;
				di_buf->mtn_adr = mem_st_local +
					di_buf_size * i +
					nr_size;
				di_buf->cnt_adr = mem_st_local +
					di_buf_size * i +
					nr_size + mtn_size;

				if (mc_mem_alloc) {
					di_buf->mcvec_adr = mem_st_local +
						di_buf_size * i +
						nr_size + mtn_size
						+ count_size;
					di_buf->mcinfo_adr =
						mem_st_local +
						di_buf_size * i + nr_size +
						mtn_size + count_size
						+ mv_size;
				if (cfgeq(mem_flg, eDI_MEM_M_rev) ||
				    cfgeq(mem_flg, eDI_MEM_M_cma_all))
					dimv3_mcinfo_v_alloc(di_buf,
							   mm->cfg.mcinfo_size);
				}
				di_buf->c.canvas_config_flag = 2;
			}
			di_buf->code_name	= DIM_K_CODE_LOCAL;
			di_buf->channel		= channel;
			di_buf->index		= i;
			di_buf->queue_index	= -1;
			di_buf->vframe		= &pvframe_local[i];
			di_buf->vframe->private_data	= di_buf;

			#if 0
			di_buf->c.invert_top_bot_flag = 0;	/*ary:no use*/
			di_buf->c.pre_ref_count = 0;	/*ary:no use*/
			di_buf->c.post_ref_count = 0;	/*ary:no use*/
			#endif
			queuev3_in(channel, di_buf, QUEUE_LOCAL_FREE);
			dbg_init("buf[%d], addr=0x%lx\n", di_buf->index,
				 di_buf->nr_adr);

		}
	}

	if (cfgeq(mem_flg, eDI_MEM_M_cma)	||
	    cfgeq(mem_flg, eDI_MEM_M_codec_a)	||
	    cfgeq(mem_flg, eDI_MEM_M_codec_b)) {	/*trig cma alloc*/
		dipv3_wq_cma_run(channel, true);
	}

	dbg_init("one local buf size:0x%x\n", di_buf_size);
	/*mm-0705	di_post_mem = mem_st_local +*/
	/*mm-0705		di_buf_size*de_devp->buf_num_avail;*/
	di_post_mem = mem_st_local + di_buf_size * mm->cfg.num_local;
	if (dimp_get(eDI_MP_post_wr_en) && dimp_get(eDI_MP_post_wr_support)) {
		di_post_buf_size = nr_width * canvas_height * 2;
		#if 0	/*ary test for vout 25Hz*/
		/* pre buffer must 2 more than post buffer */
		if ((de_devp->buf_num_avail - 2) > MAX_POST_BUF_NUM)
			ppost->di_post_num = MAX_POST_BUF_NUM;
		else
			ppost->di_post_num = (de_devp->buf_num_avail - 2);
		#else
			/*mm-0705 ppost->di_post_num = MAX_POST_BUF_NUM;*/
		#endif
		dbg_init("DI: di post buffer size 0x%x byte.\n",
			 di_post_buf_size);
	} else {
		/*mm-0705 ppost->di_post_num = MAX_POST_BUF_NUM;*/
		di_post_buf_size = 0;
	}
	/*de_devp->post_buffer_size = di_post_buf_size;*/
	mm->cfg.size_post = di_post_buf_size;

	/**********************************************/
	/* input buf init */
	/**********************************************/

	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		struct di_buf_s *di_buf = &pbuf_in[i];

		if (di_buf) {
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->code_name	= DIM_K_CODE_DVFM;
			di_buf->channel		= channel;
			di_buf->type		= VFRAME_TYPE_IN;
			di_buf->index		= i;
			di_buf->queue_index	= -1;
			di_buf->vframe		= &pvframe_in_dup[i];
			di_buf->vframe->private_data = di_buf;

			#if 0
			di_buf->c.invert_top_bot_flag = 0; /*ary: no use*/
			di_buf->c.pre_ref_count = 0;	/*ary: no use*/
			di_buf->c.post_ref_count = 0;	/*ary: no use*/
			#endif
			div3_que_in(channel, QUE_IN_FREE, di_buf);
		}
	}
	/**********************************************/
	/* post buf init */
	/**********************************************/
	pintf = get_dev_intf(channel);
	/*mm-0705 for (i = 0; i < ppost->di_post_num; i++) {*/
	for (i = 0; i < mm->cfg.num_post; i++) {
		struct di_buf_s *di_buf = &pbuf_post[i];

		if (di_buf) {
			if (dimp_get(eDI_MP_post_wr_en) &&
			    dimp_get(eDI_MP_post_wr_support)) {
				if (div3_que_is_in_que(channel, QUE_POST_KEEP,
						     di_buf)) {
					dbg_reg("%s:post keep buf %d\n",
						__func__,
						di_buf->index);
					continue;
				}
			}
			tmp_page = di_buf->pages;
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->pages		= tmp_page;
			di_buf->code_name	= DIM_K_CODE_POST;
			di_buf->channel		= channel;
			di_buf->type		= VFRAME_TYPE_POST;
			di_buf->index		= i;
			di_buf->queue_index	= -1;
			di_buf->vframe		= &pvframe_post[i];
			di_buf->vframe->private_data = di_buf;

			//ary: di_buf->c.invert_top_bot_flag = 0;	//

			if (dimp_get(eDI_MP_post_wr_en) &&
			    dimp_get(eDI_MP_post_wr_support)) {
				di_buf->canvas_width[NR_CANVAS] =
					(nr_width << 1);
				di_buf->canvas_height = canvas_height;
				di_buf->c.canvas_config_flag = 1;
				di_buf->nr_adr = di_post_mem
					+ di_post_buf_size * i;
				dbg_init("[%d]post buf:%d: addr=0x%lx\n", i,
					 di_buf->index, di_buf->nr_adr);
				/*ins*/
				if (pintf->tmode == EDIM_TMODE_3_PW_LOCAL) {
					di_buf->ins = &pins[i];
					di_buf->ins->vf = &pvframe_post[i];
					di_buf->ins->mng.ch = channel;
					di_buf->ins->mng.index = i;
					di_buf->ins->private_data = di_buf;
					di_buf->code_name = DIM_K_CODE_INS_POST;
				}
			}

			div3_que_in(channel, QUE_POST_FREE, di_buf);

		} else {
			PR_ERR("%s:%d:post buf is null\n", __func__, i);
		}
	}
	if (cfgeq(mem_flg, eDI_MEM_M_rev) && de_devp->nrds_enable) {
		nrds_mem = di_post_mem + mm->cfg.num_post * di_post_buf_size;
		/*mm-0705 ppost->di_post_num * di_post_buf_size;*/
		dimv3_nr_ds_buf_init(cfgg(mem_flg), nrds_mem,
				   &de_devp->pdev->dev);
	}
	return 0;
}

#if 0
void dim_keep_mirror_buffer(unsigned int channel)	/*not use*/
{
	struct di_buf_s *p = NULL;
	int i = 0, ii = 0, itmp;
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_buf_s *pdup = NULL;

	queue_for_each_entry(p, channel, QUEUE_DISPLAY, list) {
	if (p->c.di_buf[0]->type != VFRAME_TYPE_IN	&&
	    (p->c.process_fun_index != PROCESS_FUN_NULL)	&&
	    (ii < USED_LOCAL_BUF_MAX)			&&
	    (p->index == ppost->cur_disp_index)) {
		ppost->keep_buf = p;
		for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
			pdup = p->c.di_buf_dup_p[i];
			if (IS_ERR_OR_NULL(pdup))
				continue;
			/* prepare for recycle
			 * the keep buffer
			 */
			pdup->c.pre_ref_count = 0;
			pdup->c.post_ref_count = 0;
			if ((pdup->queue_index >= 0) &&
			    (pdup->queue_index < QUEUE_NUM)) {
				if (is_in_queue(channel,
						pdup,
						pdup->queue_index
						))
					queuev3_out(channel, pdup);
					/*which que?*/
			}
			ii++;
			if (pdup->c.di_wr_linked_buf)
				p->c.di_buf_dup_p[i + 1] =
				pdup->c.di_wr_linked_buf;
		}
		queuev3_out(channel, p);	/*which que?*/
		break;
	}
	}
}
#endif
void dimv3_post_keep_mirror_buffer(unsigned int channel)
{
	struct di_buf_s *p = NULL;
	int itmp;
	bool flg = false;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	queue_for_each_entry(p, channel, QUEUE_DISPLAY, list) {
		if (p->type != VFRAME_TYPE_POST	||
		    !p->c.process_fun_index) {
			dbg_reg("%s:not post buf:%d\n",
				__func__, p->type);
			continue;
		}

		ppost->keep_buf_post = p;	/*only keep one*/
		flg = true;
		dbg_reg("%s %d\n", __func__, p->index);
	}

	if (flg && ppost->keep_buf_post) {
		ppost->keep_buf_post->queue_index = -1;
		ppost->keep_buf_post->c.invert_top_bot_flag = 0;
	}
}

void dimv3_post_keep_mirror_buffer2(unsigned int ch)
{
	struct di_buf_s *p = NULL;
	int itmp;

	queue_for_each_entry(p, ch, QUEUE_DISPLAY, list) {
		if (p->type != VFRAME_TYPE_POST	||
		    !p->c.process_fun_index) {
			dbg_keep("%s:not post buf:%d\n", __func__, p->type);
			continue;
		}
		if (div3_que_is_in_que(ch, QUE_POST_BACK, p)) {
			dbg_keep("%s:is in back[%d]\n", __func__, p->index);
			continue;
		}

		p->queue_index = -1;
		div3_que_in(ch, QUE_POST_KEEP, p);
		p->c.invert_top_bot_flag = 0;

		dbg_keep("%s %d\n", __func__, p->index);
	}
}

bool dimv3_post_keep_is_in(unsigned int ch, struct di_buf_s *di_buf)
{
	if (div3_que_is_in_que(ch, QUE_POST_KEEP, di_buf))
		return true;
	return false;
}

bool dimv3_post_keep_release_one(unsigned int ch, unsigned int di_buf_index)
{
	struct di_buf_s *pbuf_post;
	struct di_buf_s *di_buf;

	/*must post or err*/
	pbuf_post = get_buf_post(ch);
	di_buf = &pbuf_post[di_buf_index];

	if (!div3_que_is_in_que(ch, QUE_POST_KEEP, di_buf)) {
		PR_ERR("%s:buf[%d] is not in keep\n", __func__, di_buf_index);
		return false;
	}
	div3_que_out_not_fifo(ch, QUE_POST_KEEP, di_buf);
	div3_que_in(ch, QUE_POST_FREE, di_buf);
	dbg_keep("%s:buf[%d]\n", __func__, di_buf_index);
	return true;
}

bool dimv3_post_keep_release_all_2free(unsigned int ch)
{
	struct di_buf_s *di_buf;
	struct di_buf_s *pbuf_post;
	unsigned int i = 0;

	if (div3_que_is_empty(ch, QUE_POST_KEEP))
		return true;

	pbuf_post = get_buf_post(ch);
	dbg_keep("%s:ch[%d]\n", __func__, ch);

	while (i <= MAX_POST_BUF_NUM) {
		i++;
		di_buf = div3_que_peek(ch, QUE_POST_KEEP);
		if (!di_buf)
			break;

		if (!div3_que_out(ch, QUE_POST_KEEP, di_buf)) {
			PR_ERR("%s:out err\n", __func__);
			break;
		}
		memset(&di_buf->c, 0, sizeof(struct di_buf_c_s));
		div3_que_in(ch, QUE_POST_FREE, di_buf);

		dbg_keep("\ttype[%d],index[%d]\n", di_buf->type, di_buf->index);
	}

	return true;
}
#if 0
void dim_post_keep_cmd_release(unsigned int ch, struct vframe_s *vframe)
{
	struct di_buf_s *di_buf;

	if (!vframe)
		return;

	di_buf = (struct di_buf_s *)vframe->private_data;

	if (!di_buf) {
		PR_WARN("%s:ch[%d]:no di_buf\n", __func__, ch);
		return;
	}

	if (di_buf->type != VFRAME_TYPE_POST) {
		PR_WARN("%s:ch[%d]:not post\n", __func__, ch);
		return;
	}
	dbg_keep("release keep ch[%d],index[%d]\n",
		 di_buf->channel,
		 di_buf->index);
	taskv3_send_cmd(LCMD2(ECMD_RL_KEEP, di_buf->channel, di_buf->index));
}
EXPORT_SYMBOL(dim_post_keep_cmd_release);
#endif

void dimv3_post_keep_cmd_release2(struct vframe_s *vframe)
{
	struct di_buf_s *di_buf;
	struct di_buffer *ins;

	if (!vframe)
		return;

	di_buf = (struct di_buf_s *)vframe->private_data;

	if (!di_buf) {
		PR_WARN("%s:no di_buf\n", __func__);
		return;
	}

	if (di_buf->code_name == DIM_K_CODE_INS_POST) {
		ins = vframe->private_data;
		di_buf = ins->private_data;
		if (!di_buf) {
			PR_WARN("%s:no di_buf in ins\n", __func__);
			return;
		}
	}

	if (di_buf->type != VFRAME_TYPE_POST) {
		PR_WARN("%s:ch[%d]:not post\n", __func__, di_buf->channel);
		return;
	}

	if (!div3_que_is_in_que(di_buf->channel, QUE_POST_KEEP, di_buf)) {
		PR_ERR("%s:not keep buf %d\n", __func__, di_buf->index);
	} else {
		dbg_keep("release keep ch[%d],index[%d]\n",
			 di_buf->channel,
			 di_buf->index);
		taskv3_send_cmd(LCMD2(ECMD_RL_KEEP, di_buf->channel,
				    di_buf->index));
	}
}
EXPORT_SYMBOL(dimv3_post_keep_cmd_release2);

void dimv3_dbg_release_keep_all(unsigned int ch)
{
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize, itmp;
	struct di_buf_s *p;

	div3_que_list(ch, QUE_POST_KEEP, &tmpa[0], &psize);
	dbg_keep("post_keep: curr(%d)\n", psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]);
		dbg_keep("\ttype[%d],index[%d]\n", p->type, p->index);
		dimv3_post_keep_cmd_release2(p->vframe);
	}
	dbg_keep("%s:end\n", __func__);
}

void dimv3_post_keep_back_recycle(unsigned int ch)
{
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize, itmp;
	struct di_buf_s *p;

	if (div3_que_is_empty(ch, QUE_POST_KEEP_BACK))
		return;
	div3_que_list(ch, QUE_POST_KEEP_BACK, &tmpa[0], &psize);
	dbg_keep("post_keep_back: curr(%d)\n", psize);
	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]);
		dbg_keep("keep back recycle %d\n", p->index);
		dimv3_post_keep_release_one(ch, p->index);
	}
	pwv3_queue_clear(ch, QUE_POST_KEEP_BACK);
}

void dimv3_post_keep_cmd_proc(unsigned int ch, unsigned int index)
{
	struct di_dev_s *di_dev;
	enum EDI_TOP_STATE chst;
	unsigned int len_keep, len_back;
	struct di_buf_s *pbuf_post;
	struct di_buf_s *di_buf;

	/*must post or err*/
	di_dev = getv3_dim_de_devp();

	if (!di_dev || !di_dev->data_l) {
		PR_WARN("%s: no di_dev\n", __func__);
		return;
	}

	chst = dipv3_chst_get(ch);
	switch (chst) {
	case EDI_TOP_STATE_READY:
		dimv3_post_keep_release_one(ch, index);
		break;
	case EDI_TOP_STATE_IDLE:
	case eDI_TOP_STATE_BYPASS:
		pbuf_post = get_buf_post(ch);
		if (!pbuf_post) {
			PR_ERR("%s:no pbuf_post\n", __func__);
			break;
		}

		di_buf = &pbuf_post[index];
		di_buf->queue_index = -1;
		div3_que_in(ch, QUE_POST_KEEP_BACK, di_buf);
		len_keep = div3_que_list_count(ch, QUE_POST_KEEP);
		len_back = div3_que_list_count(ch, QUE_POST_KEEP_BACK);
		if (len_back >= len_keep) {
			/*release all*/
			pwv3_queue_clear(ch, QUE_POST_KEEP);
			pwv3_queue_clear(ch, QUE_POST_KEEP_BACK);
			dipv3_wq_cma_run(ch, false);
		}
		break;
	case eDI_TOP_STATE_REG_STEP1:
	case eDI_TOP_STATE_REG_STEP1_P1:
	case eDI_TOP_STATE_REG_STEP2:
		pbuf_post = get_buf_post(ch);
		if (!pbuf_post) {
			PR_ERR("%s:no pbuf_post\n", __func__);
			break;
		}

		di_buf = &pbuf_post[index];
		di_buf->queue_index = -1;
		div3_que_in(ch, QUE_POST_KEEP_BACK, di_buf);

		break;
	case eDI_TOP_STATE_UNREG_STEP1:
		pbuf_post = get_buf_post(ch);
		if (!pbuf_post) {
			PR_ERR("%s:no pbuf_post\n", __func__);
			break;
		}

		di_buf = &pbuf_post[index];
		di_buf->queue_index = -1;
		div3_que_in(ch, QUE_POST_KEEP_BACK, di_buf);
		break;
	default:
		PR_ERR("%s:do nothing? %s:%d\n",
		       __func__,
		       dipv3_chst_get_name(chst),
		       index);
		break;
	}
}

void dimv3_uninit_buf(unsigned int disable_mirror, unsigned int channel)
{
	/*int i = 0;*/
	void **pvframe_in = getv3_vframe_in(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_dev_s *de_devp = getv3_dim_de_devp();
	struct dim_inter_s *pintf;

	pintf = get_dev_intf(channel);

	if (!queuev3_empty(channel, QUEUE_DISPLAY) || disable_mirror)
		ppost->keep_buf = NULL;
#if 0
	if (disable_mirror != 1)
		dim_keep_mirror_buffer();

	if (!IS_ERR_OR_NULL(di_post_stru.keep_buf)) {
		keep_buf = di_post_stru.keep_buf;
		pr_dbg("%s keep cur di_buf %d (",
		       __func__, keep_buf->index);
		for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
			if (!IS_ERR_OR_NULL(keep_buf->di_buf_dup_p[i]))
				pr_dbg("%d\t",
				       keep_buf->di_buf_dup_p[i]->index);
		}
		pr_dbg(")\n");
	}
#else
	if (!disable_mirror &&
	    (pintf->tmode != EDIM_TMODE_2_PW_OUT))
		dimv3_post_keep_mirror_buffer2(channel);
#endif
	queuev3_init(channel, 0);
	div3_que_init(channel); /*new que*/

	/* decoder'buffer had been releae no need put */
	#if 0
	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		pvframe_in[i] = NULL;
	#else
	memset(pvframe_in, 0, sizeof(*pvframe_in) * MAX_IN_BUF_NUM);
	#endif
	ppre->pre_de_process_flag = 0;
	if (dimp_get(eDI_MP_post_wr_en) && dimp_get(eDI_MP_post_wr_support)) {
		ppost->cur_post_buf = NULL;
		ppost->post_de_busy = 0;
		ppost->de_post_process_done = 0;
		ppost->post_wr_cnt = 0;
	}
	if (cfgeq(mem_flg, eDI_MEM_M_rev) && de_devp->nrds_enable) {
		dimv3_nr_ds_buf_uninit(cfgg(mem_flg),
				     &de_devp->pdev->dev);
	}
}

void dimv3_log_buffer_state(unsigned char *tag, unsigned int channel)
{
	struct di_pre_stru_s *ppre;
	unsigned int tmpa[MAX_FIFO_SIZE]; /*new que*/
	unsigned int psize; /*new que*/

	if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_BUFFER_STATE) {
		struct di_buf_s *p = NULL;/* , *ptmp; */
		int itmp;
		int in_free = 0;
		int local_free = 0;
		int pre_ready = 0;
		int post_free = 0;
		int post_ready = 0;
		int post_ready_ext = 0;
		int display = 0;
		int display_ext = 0;
		int recycle = 0;
		int di_inp = 0;
		int di_wr = 0;
		ulong irq_flag2 = 0;

		ppre = get_pre_stru(channel);

		di_lock_irqfiq_save(irq_flag2);
		in_free = div3_que_list_count(channel, QUE_IN_FREE);
		local_free = listv3_count(channel, QUEUE_LOCAL_FREE);
		pre_ready = div3_que_list_count(channel, QUE_PRE_READY);
		post_free = div3_que_list_count(channel, QUE_POST_FREE);
		post_ready = div3_que_list_count(channel, QUE_POST_READY);

		div3_que_list(channel, QUE_POST_READY, &tmpa[0], &psize);
		/*di_que_for_each(channel, p, psize, &tmpa[0]) {*/
		for (itmp = 0; itmp < psize; itmp++) {
			p = pwv3_qindex_2_buf(channel, tmpa[itmp]);
			if (p->c.di_buf[0])
				post_ready_ext++;

			if (p->c.di_buf[1])
				post_ready_ext++;
		}
		queue_for_each_entry(p, channel, QUEUE_DISPLAY, list) {
			display++;
			if (p->c.di_buf[0])
				display_ext++;

			if (p->c.di_buf[1])
				display_ext++;
		}
		recycle = listv3_count(channel, QUEUE_RECYCLE);

		if (ppre->di_inp_buf)
			di_inp++;
		if (ppre->di_wr_buf)
			di_wr++;

		if (dimp_get(eDI_MP_buf_state_log_threshold) == 0)
			buf_state_log_start = 0;
		else if (post_ready < dimp_get(eDI_MP_buf_state_log_threshold))
			buf_state_log_start = 1;

		if (buf_state_log_start) {
			dimv3_print(
		"[%s]i i_f %d/%d, l_f %d, pre_r %d, post_f %d/%d,",
				tag,
				in_free, MAX_IN_BUF_NUM,
				local_free,
				pre_ready,
				post_free, MAX_POST_BUF_NUM);
			dimv3_print(
		"post_r (%d:%d), disp (%d:%d),rec %d, di_i %d, di_w %d\n",
				post_ready, post_ready_ext,
				display, display_ext,
				recycle,
				di_inp, di_wr
				);
		}
		di_unlock_irqfiq_restore(irq_flag2);
	}
}

unsigned char dimv3_check_di_buf(struct di_buf_s *di_buf, int reason,
			       unsigned int channel)
{
//	int error = 0;
	//struct vframe_s *pvframe_in_dup = get_vframe_in_dup(channel);
	//struct vframe_s *pvframe_local = get_vframe_local(channel);
	//struct vframe_s *pvframe_post = get_vframe_post(channel);

	if (!di_buf) {
		PR_ERR("%s: %d, di_buf is NULL\n", __func__, reason);
		return 1;
	}

	#if 0	/*remove vframe 12-03*/
	if (di_buf->type == VFRAME_TYPE_IN) {
		if (di_buf->vframe != &pvframe_in_dup[di_buf->index])
			error = 1;
	} else if (di_buf->type == VFRAME_TYPE_LOCAL) {
		if (di_buf->vframe != &pvframe_local[di_buf->index])
			error = 1;
	} else if (di_buf->type == VFRAME_TYPE_POST) {
		if (di_buf->vframe != &pvframe_post[di_buf->index])
			error = 1;
	} else {
		error = 1;
	}

	if (error) {
		PR_ERR("%s: %d, di_buf wrong\n", __func__, reason);
		if (recovery_flag == 0)
			recovery_log_reason = reason;
		recovery_flag++;
		dimv3_dump_di_buf(di_buf);
		return 1;
	}
	#endif
	return 0;
}

/*
 *  di pre process
 */
static void
config_di_mcinford_mif(struct DI_MC_MIF_s *di_mcinford_mif,
		       struct di_buf_s *di_buf)
{
	if (di_buf) {
		di_mcinford_mif->size_x = (di_buf->c.vmode.h + 2) / 4 - 1;
		di_mcinford_mif->size_y = 1;
		di_mcinford_mif->canvas_num = di_buf->mcinfo_canvas_idx;
	}
}

static void
config_di_pre_mc_mif(struct DI_MC_MIF_s *di_mcinfo_mif,
		     struct DI_MC_MIF_s *di_mcvec_mif,
		     struct di_buf_s *di_buf)
{
	unsigned int pre_size_w = 0, pre_size_h = 0;

	if (di_buf) {
		//pre_size_w = di_buf->vframe->width;
		//pre_size_h = (di_buf->vframe->height + 1) / 2;
		pre_size_w = di_buf->c.vmode.w;
		pre_size_h = (di_buf->c.vmode.h + 1) / 2;
		#ifdef V_DIV4
		di_mcinfo_mif->size_x = (pre_size_h + 1) / 2 - 1;
		#else
		di_mcinfo_mif->size_x = pre_size_h / 2 - 1;
		#endif
		di_mcinfo_mif->size_y = 1;
		di_mcinfo_mif->canvas_num = di_buf->mcinfo_canvas_idx;

		di_mcvec_mif->size_x = (pre_size_w + 4) / 5 - 1;
		di_mcvec_mif->size_y = pre_size_h - 1;
		di_mcvec_mif->canvas_num = di_buf->mcvec_canvas_idx;
	}
}

static void config_di_cnt_mif(struct DI_SIM_MIF_s *di_cnt_mif,
			      struct di_buf_s *di_buf)
{
	if (di_buf) {
		di_cnt_mif->start_x = 0;
		//di_cnt_mif->end_x = di_buf->vframe->width - 1;
		di_cnt_mif->end_x = di_buf->c.vmode.w - 1;
		di_cnt_mif->start_y = 0;
		//di_cnt_mif->end_y = di_buf->vframe->height / 2 - 1;
		di_cnt_mif->end_y = di_buf->c.vmode.h / 2 - 1;
		di_cnt_mif->canvas_num = di_buf->cnt_canvas_idx;
	}
}

static void
config_di_wr_mif(struct DI_SIM_MIF_s *di_nrwr_mif,
		 struct DI_SIM_MIF_s *di_mtnwr_mif,
		 struct di_buf_s *di_buf, unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	//vframe_t *vf = di_buf->vframe;

	di_nrwr_mif->canvas_num = di_buf->nr_canvas_idx;
	di_nrwr_mif->start_x = 0;
	//di_nrwr_mif->end_x = vf->width - 1;
	di_nrwr_mif->end_x = di_buf->c.vmode.w - 1;
	di_nrwr_mif->start_y = 0;
	#if 0
	if (di_buf->vframe->bitdepth & BITDEPTH_Y10)
		di_nrwr_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ?
			3 : 1;
	else
		di_nrwr_mif->bit_mode = 0;
	#else
		di_nrwr_mif->bit_mode = di_buf->c.vmode.bit_mode;
	#endif
	if (ppre->prog_proc_type == 0)
		//di_nrwr_mif->end_y = vf->height / 2 - 1;
		di_nrwr_mif->end_y = di_buf->c.vmode.h / 2 - 1;
	else
		//di_nrwr_mif->end_y = vf->height - 1;
		di_nrwr_mif->end_y = di_buf->c.vmode.h - 1;
	if (ppre->prog_proc_type == 0) {
		di_mtnwr_mif->start_x = 0;
		//di_mtnwr_mif->end_x = vf->width - 1;
		di_mtnwr_mif->end_x = di_buf->c.vmode.w - 1;
		di_mtnwr_mif->start_y = 0;
		//di_mtnwr_mif->end_y = vf->height / 2 - 1;
		di_mtnwr_mif->end_y = di_buf->c.vmode.h / 2 - 1;
		di_mtnwr_mif->canvas_num = di_buf->mtn_canvas_idx;
	}
}

#if 0
static void config_di_mif(struct DI_MIF_s *di_mif, struct di_buf_s *di_buf,
			  unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (!di_buf)
		return;
	di_mif->canvas0_addr0 =
		di_buf->vframe->canvas0Addr & 0xff;
	di_mif->canvas0_addr1 =
		(di_buf->vframe->canvas0Addr >> 8) & 0xff;
	di_mif->canvas0_addr2 =
		(di_buf->vframe->canvas0Addr >> 16) & 0xff;

	di_mif->nocompress = (di_buf->vframe->type & VIDTYPE_COMPRESS) ? 0 : 1;

	if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ?
			3 : 2;
		else if (di_buf->vframe->type & VIDTYPE_VIU_422)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ?
			3 : 1;
	} else {
		di_mif->bit_mode = 0;
	}
	if (di_buf->vframe->type & VIDTYPE_VIU_422) {
		/* from vdin or local vframe */
		if ((!is_progressive(di_buf->vframe))	||
		    (ppre->prog_proc_type)) {
			di_mif->video_mode = 0;
			di_mif->set_separate_en = 0;
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0;
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			if (ppre->prog_proc_type)
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 1;
			else
				di_mif->luma_y_end0 =
					di_buf->vframe->height / 2 - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 = 0;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 = 0;
			di_mif->canvas0_addr0 =
				di_buf->vframe->canvas0Addr & 0xff;
			di_mif->canvas0_addr1 =
				(di_buf->vframe->canvas0Addr >> 8) & 0xff;
			di_mif->canvas0_addr2 =
				(di_buf->vframe->canvas0Addr >> 16) & 0xff;
		}
	} else {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->video_mode = 1;
		else
			di_mif->video_mode = 0;
		if (di_buf->vframe->type & VIDTYPE_VIU_NV21)
			di_mif->set_separate_en = 2;
		else
			di_mif->set_separate_en = 1;

		if (is_progressive(di_buf->vframe) &&
		    (ppre->prog_proc_type)) {
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			di_mif->luma_y_end0 =
				di_buf->vframe->height - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 =
				di_buf->vframe->width / 2 - 1;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 =
				di_buf->vframe->height / 2 - 1;
		} else {
			/*move to mp	di_mif->src_prog = force_prog?1:0;*/
			if (ppre->cur_inp_type  & VIDTYPE_INTERLACE)
				di_mif->src_prog = 0;
			else
				di_mif->src_prog
				= dimp_get(eDI_MP_force_prog) ? 1 : 0;
			di_mif->src_field_mode = 1;
			if (
				(di_buf->vframe->type & VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP) {
				di_mif->output_field_num = 0; /* top */
				di_mif->luma_x_start0 = 0;
				di_mif->luma_x_end0 =
					di_buf->vframe->width - 1;
				di_mif->luma_y_start0 = 0;
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 2;
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0 =
					di_buf->vframe->width / 2 - 1;
				di_mif->chroma_y_start0 = 0;
				di_mif->chroma_y_end0 =
					di_buf->vframe->height / 2
						- (di_mif->src_prog ? 1 : 2);
			} else {
				di_mif->output_field_num = 1;
				/* bottom */
				di_mif->luma_x_start0 = 0;
				di_mif->luma_x_end0 =
					di_buf->vframe->width - 1;
				di_mif->luma_y_start0 = 1;
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 1;
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0 =
					di_buf->vframe->width / 2 - 1;
				di_mif->chroma_y_start0 =
					(di_mif->src_prog ? 0 : 1);
				di_mif->chroma_y_end0 =
					di_buf->vframe->height / 2 - 1;
			}
		}
	}
}
#else
static void config_di_mif(struct DI_MIF_s *di_mif, struct di_buf_s *di_buf,
			  unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct dim_vmode_s *pvmode;

	if (!di_buf)
		return;
	pvmode = &di_buf->c.vmode;

	di_mif->canvas0_addr0 =
		pvmode->canvas0Addr & 0xff;
	di_mif->canvas0_addr1 =
		(pvmode->canvas0Addr >> 8) & 0xff;
	di_mif->canvas0_addr2 =
		(pvmode->canvas0Addr >> 16) & 0xff;

	di_mif->nocompress = (pvmode->vtype & DIM_VFM_COMPRESS) ? 0 : 1;

	#if 0
	if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ?
			3 : 2;
		else if (di_buf->vframe->type & VIDTYPE_VIU_422)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ?
			3 : 1;
	} else {
		di_mif->bit_mode = 0;
	}
	#else
	di_mif->bit_mode = pvmode->bit_mode;
	#endif
	if (pvmode->vtype & DIM_VFM_VIU_422) {
		/* from vdin or local vframe */
		if (DIM_VFMT_IS_I(pvmode->vtype)	||
		    (ppre->prog_proc_type)) {
			di_mif->video_mode = 0;
			di_mif->set_separate_en = 0;
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0;
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 = pvmode->w - 1;
			di_mif->luma_y_start0 = 0;
			if (ppre->prog_proc_type)
				di_mif->luma_y_end0 = pvmode->h - 1;
			else
				di_mif->luma_y_end0 = pvmode->h / 2 - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 = 0;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 = 0;
			#if 0
			di_mif->canvas0_addr0 =
				pvmode->canvas0Addr & 0xff;
			di_mif->canvas0_addr1 =
				(pvmode->canvas0Addr >> 8) & 0xff;
			di_mif->canvas0_addr2 =
				(pvmode->canvas0Addr >> 16) & 0xff;
			#endif
		}
	} else {
		if (pvmode->vtype & DIM_VFM_VIU_444)
			di_mif->video_mode = 1;
		else
			di_mif->video_mode = 0;
		if ((pvmode->vtype & DIM_VFM_VIU_NV21) ||
		    (pvmode->vtype & DIM_VFM_VIU_NV12))
			di_mif->set_separate_en = 2;
		else
			di_mif->set_separate_en = 1;

		/* TODO: need check the linear flag more */
		if ((pvmode->vtype & DIM_VFM_VIU_NV12) &&
		    (di_mif == &ppre->di_inp_mif))
			di_mif->set_separate_en = 3;

		if (DIM_VFMT_IS_P(pvmode->vtype) &&
		    (ppre->prog_proc_type)) {
		    /*ary: p as p or a use 2 i buf mode*/
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 = pvmode->w - 1;
			di_mif->luma_y_start0 = 0;
			di_mif->luma_y_end0 = pvmode->h - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 = pvmode->w / 2 - 1;
			di_mif->chroma_y_start0 = 0;
			#ifdef V_DIV4
			di_mif->chroma_y_end0 = pvmode->h / 2 - 1;
			#else
			di_mif->chroma_y_end0 = (pvmode->h + 1) / 2 - 1;
			#endif
		} else {
			/*move to mp	di_mif->src_prog = force_prog?1:0;*/
			if (ppre->cur_inp_type  & DIM_VFM_INTERLACE)
				di_mif->src_prog = 0;
			else
				di_mif->src_prog
				= dimp_get(eDI_MP_force_prog) ? 1 : 0;
			di_mif->src_field_mode = 1;
			if (DIM_VFMT_IS_TOP(pvmode->vtype)) {
				di_mif->output_field_num = 0; /* top */
				di_mif->luma_x_start0 = 0;
				di_mif->luma_x_end0 = pvmode->w - 1;
				di_mif->luma_y_start0 = 0;
				#ifdef V_DIV4
				di_mif->luma_y_end0 = pvmode->h - 2;
				#else
				di_mif->luma_y_end0 = pvmode->h - 1;
				#endif
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0 =	pvmode->w / 2 - 1;
				di_mif->chroma_y_start0 = 0;
				#ifdef V_DIV4
				di_mif->chroma_y_end0 =	pvmode->h / 2
						- (di_mif->src_prog ? 1 : 2);
				#else
				di_mif->chroma_y_end0 =	(pvmode->h + 1) / 2 - 1;
				#endif
			} else {
				di_mif->output_field_num = 1;
				/* bottom */
				di_mif->luma_x_start0	= 0;
				di_mif->luma_x_end0	= pvmode->w - 1;
				di_mif->luma_y_start0	= 1;
				di_mif->luma_y_end0	= pvmode->h - 1;
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0	= pvmode->w / 2 - 1;
				di_mif->chroma_y_start0 =
					(di_mif->src_prog ? 0 : 1);
				#ifdef V_DIV4
				di_mif->chroma_y_end0	= pvmode->h / 2 - 1;
				#else
				di_mif->chroma_y_end0	= (pvmode->h + 1) / 2
							- 1;
				#endif
			}
		}
	}
}

#endif
static void di_pre_size_change(unsigned short width,
			       unsigned short height,
			       unsigned short vf_type,
			       unsigned int channel);



void dim_dbg_delay_mask_set(unsigned int position)
{
	if (dimp_get(EDI_MP_SUB_DBG_MODE) & position)
		get_dbg_data()->delay_cnt = 0;
	else
		get_dbg_data()->delay_cnt = dimp_get(EDI_MP_SUB_DELAY);
}

void dimv3_pre_de_process(unsigned int channel)
{
	ulong irq_flag2 = 0;
	unsigned short pre_width = 0, pre_height = 0;
	unsigned char chan2_field_num = 1;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	int canvases_idex = ppre->field_count % 2;
	unsigned short cur_inp_field_type = VIDTYPE_TYPEMASK;
	unsigned short int_mask = 0x7f;
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	ppre->pre_de_process_flag = 1;
	dimv3_ddbg_mod_save(eDI_DBG_MOD_PRE_SETB, channel, ppre->in_seq);/*dbg*/

	#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	prev3_inp_canvas_config(ppre->di_inp_buf);
	#endif

	config_di_mif(&ppre->di_inp_mif, ppre->di_inp_buf, channel);
	/* pr_dbg("set_separate_en=%d vframe->type %d\n",
	 * di_pre_stru.di_inp_mif.set_separate_en,
	 * di_pre_stru.di_inp_buf->vframe->type);
	 */
#ifdef DI_USE_FIXED_CANVAS_IDX
	if (ppre->di_mem_buf_dup_p	&&
	    ppre->di_mem_buf_dup_p != ppre->di_inp_buf) {
		config_canvas_idx(ppre->di_mem_buf_dup_p,
				  di_pre_idx[canvases_idex][0], -1);
		config_cnt_canvas_idx(ppre->di_mem_buf_dup_p,
				      di_pre_idx[canvases_idex][1]);
	} else {
		config_cnt_canvas_idx(ppre->di_wr_buf,
				      di_pre_idx[canvases_idex][1]);
		config_di_cnt_mif(&ppre->di_contp2rd_mif,
				  ppre->di_wr_buf);
	}
	if (ppre->di_chan2_buf_dup_p) {
		config_canvas_idx(ppre->di_chan2_buf_dup_p,
				  di_pre_idx[canvases_idex][2], -1);
		config_cnt_canvas_idx(ppre->di_chan2_buf_dup_p,
				      di_pre_idx[canvases_idex][3]);
	} else {
		config_cnt_canvas_idx(ppre->di_wr_buf,
				      di_pre_idx[canvases_idex][3]);
	}

	/*wr_buf*/
	config_canvas_idx(ppre->di_wr_buf,
			  di_pre_idx[canvases_idex][4],
			  di_pre_idx[canvases_idex][5]);
	config_cnt_canvas_idx(ppre->di_wr_buf,
			      di_pre_idx[canvases_idex][6]);

	/*mc*/
	if (dimp_get(eDI_MP_mcpre_en)) {
		if (ppre->di_chan2_buf_dup_p)
			config_mcinfo_canvas_idx(ppre->di_chan2_buf_dup_p,
						 di_pre_idx[canvases_idex][7]);
		else
			config_mcinfo_canvas_idx(ppre->di_wr_buf,
						 di_pre_idx[canvases_idex][7]);

		config_mcinfo_canvas_idx(ppre->di_wr_buf,
					 di_pre_idx[canvases_idex][8]);
		config_mcvec_canvas_idx(ppre->di_wr_buf,
					di_pre_idx[canvases_idex][9]);
	}
#endif
	config_di_mif(&ppre->di_mem_mif, ppre->di_mem_buf_dup_p, channel);
	if (!ppre->di_chan2_buf_dup_p) {
		config_di_mif(&ppre->di_chan2_mif,
			      ppre->di_inp_buf, channel);
	} else
		config_di_mif(&ppre->di_chan2_mif,
			      ppre->di_chan2_buf_dup_p, channel);
	config_di_wr_mif(&ppre->di_nrwr_mif, &ppre->di_mtnwr_mif,
			 ppre->di_wr_buf, channel);

	if (ppre->di_chan2_buf_dup_p)
		config_di_cnt_mif(&ppre->di_contprd_mif,
				  ppre->di_chan2_buf_dup_p);
	else
		config_di_cnt_mif(&ppre->di_contprd_mif,
				  ppre->di_wr_buf);

	config_di_cnt_mif(&ppre->di_contwr_mif, ppre->di_wr_buf);
	if (dimp_get(eDI_MP_mcpre_en)) {
		if (ppre->di_chan2_buf_dup_p)
			config_di_mcinford_mif(&ppre->di_mcinford_mif,
					       ppre->di_chan2_buf_dup_p);
		else
			config_di_mcinford_mif(&ppre->di_mcinford_mif,
					       ppre->di_wr_buf);

		config_di_pre_mc_mif(&ppre->di_mcinfowr_mif,
				     &ppre->di_mcvecwr_mif, ppre->di_wr_buf);
	}
	#if 0
	if ((ppre->di_chan2_buf_dup_p) &&
	    ((ppre->di_chan2_buf_dup_p->vframe->type & VIDTYPE_TYPEMASK)
	     == VIDTYPE_INTERLACE_TOP))
		chan2_field_num = 0;
	#else
	if (ppre->di_chan2_buf_dup_p &&
	    ppre->di_chan2_buf_dup_p->c.wmode.is_top)
		chan2_field_num = 0;
	#endif
	pre_width = ppre->di_nrwr_mif.end_x + 1;
	pre_height = ppre->di_nrwr_mif.end_y + 1;
	#if 0
	if (ppre->input_size_change_flag) {
		cur_inp_field_type =
		(ppre->di_inp_buf->vframe->type & VIDTYPE_TYPEMASK);
		cur_inp_field_type =
	ppre->cur_prog_flag ? VIDTYPE_PROGRESSIVE : cur_inp_field_type;
		/*di_async_reset2();*/
		di_pre_size_change(pre_width, pre_height,
				   cur_inp_field_type, channel);
		ppre->input_size_change_flag = false;
	}
	#else
	if (ppre->input_size_change_flag) {
		cur_inp_field_type =
			ppre->di_inp_buf->c.vmode.vtype & DIM_VFM_TYPEMASK;
		/*di_async_reset2();*/
		di_pre_size_change(pre_width, pre_height,
				   cur_inp_field_type, channel);
		ppre->input_size_change_flag = false;
	}
	#endif
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (de_devp->nrds_enable) {
			dimv3_nr_ds_mif_config();
			dimv3_nr_ds_hw_ctrl(true);
			int_mask = 0x3f;
		} else {
			dimv3_nr_ds_hw_ctrl(false);
		}
	}

	/* set interrupt mask for pre module.
	 * we need to only leave one mask open
	 * to prevent multiple entry for dim_irq
	 */

	/*dim_dbg_pre_cnt(channel, "s2");*/

	dimhv3_enable_di_pre_aml(&ppre->di_inp_mif,
			       &ppre->di_mem_mif,
			       &ppre->di_chan2_mif,
			       &ppre->di_nrwr_mif,
			       &ppre->di_mtnwr_mif,
			       &ppre->di_contp2rd_mif,
			       &ppre->di_contprd_mif,
			       &ppre->di_contwr_mif,
			       ppre->madi_enable,
			       chan2_field_num,
			       ppre->vdin2nr);

	/*dimh_enable_afbc_input(ppre->di_inp_buf->vframe);*/

	if (dimp_get(eDI_MP_mcpre_en)) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			dimhv3_enable_mc_di_pre_g12(&ppre->di_mcinford_mif,
						  &ppre->di_mcinfowr_mif,
						  &ppre->di_mcvecwr_mif,
						  ppre->mcdi_enable);
		else
			dimhv3_enable_mc_di_pre(&ppre->di_mcinford_mif,
					      &ppre->di_mcinfowr_mif,
					      &ppre->di_mcvecwr_mif,
					      ppre->mcdi_enable);
	}

	ppre->field_count++;
	dimhv3_txl_patch_prog(ppre->cur_prog_flag,
			    ppre->field_count,
			    dimp_get(eDI_MP_mcpre_en));

#ifdef SUPPORT_MPEG_TO_VDIN
	if (mpeg2vdin_flag) {
		struct vdin_arg_s vdin_arg;
		struct vdin_v4l2_ops_s *vdin_ops = get_vdin_v4l2_ops();

		vdin_arg.cmd = VDIN_CMD_FORCE_GO_FIELD;
		if (vdin_ops->tvin_vdin_func)
			vdin_ops->tvin_vdin_func(0, &vdin_arg);
	}
#endif
	/* **************************************************
	 * must make sure follow part issue without interrupts,
	 * otherwise may cause watch dog reboot
	 */
	di_lock_irqfiq_save(irq_flag2);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/*2019-12-25 from feijun*/
		/*after g12a: enable mif and then reset*/
		/* enable mc pre mif*/
		dimhv3_enable_di_pre_mif(true, dimp_get(eDI_MP_mcpre_en));
		dimv3_pre_frame_reset_g12(ppre->madi_enable,
					ppre->mcdi_enable);

	} else {
		dimv3_pre_frame_reset();
		/* enable mc pre mif*/
		dimhv3_enable_di_pre_mif(true, dimp_get(eDI_MP_mcpre_en));

	}

	di_pre_wait_irq_set(true);

	di_unlock_irqfiq_restore(irq_flag2);
	/****************************************************/
	/*reinit pre busy flag*/
	ppre->pre_de_busy = 1;

	dimv3_dbg_pre_cnt(channel, "s3");
	ppre->irq_time[0] = curv3_to_msecs();
	ppre->irq_time[1] = curv3_to_msecs();
	dimv3_ddbg_mod_save(eDI_DBG_MOD_PRE_SETE, channel, ppre->in_seq);/*dbg*/
	dimv3_tr_ops.pre_set(ppre->di_wr_buf->c.vmode.omx_index);

	ppre->pre_de_process_flag = 0;
}

void dimv3_pre_de_done_buf_clear(unsigned int channel)
{
	struct di_buf_s *wr_buf = NULL;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct dim_dvfm_s *pdvfm;
	struct di_ch_s		*pch;

	if (ppre->di_wr_buf) {
		wr_buf = ppre->di_wr_buf;
		if ((ppre->prog_proc_type == 2) &&
		    wr_buf->c.di_wr_linked_buf) {
			wr_buf->c.di_wr_linked_buf->c.pre_ref_count = 0;
			wr_buf->c.di_wr_linked_buf->c.post_ref_count = 0;
			queuev3_in(channel, wr_buf->c.di_wr_linked_buf,
				 QUEUE_RECYCLE);
			wr_buf->c.di_wr_linked_buf = NULL;
		}
		wr_buf->c.pre_ref_count = 0;
		wr_buf->c.post_ref_count = 0;
		queuev3_in(channel, wr_buf, QUEUE_RECYCLE);
		ppre->di_wr_buf = NULL;
	}
	if (ppre->di_inp_buf) {
		if (ppre->di_mem_buf_dup_p == ppre->di_inp_buf)
			ppre->di_mem_buf_dup_p = NULL;

		queuev3_in(channel, ppre->di_inp_buf, QUEUE_RECYCLE);
		ppre->di_inp_buf = NULL;
	}

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
	} else {
		PR_ERR("%s:no dvfm\n", __func__);
	}
}

#ifdef HIS_V3
static void topv3_bot_config(struct di_buf_s *di_buf)
{
	vframe_t *vframe = di_buf->vframe;

	if (((invert_top_bot & 0x1) != 0) && (!is_progressive(vframe))) {
		if (di_buf->invert_top_bot_flag == 0) {
			if ((vframe->type & VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_TOP) {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_BOTTOM;
			} else {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_TOP;
			}
			di_buf->invert_top_bot_flag = 1;
		}
	}
}
#endif

#if 1 /*move to di_pres.c have di_lock_irqfiq_save can't move*/
void dimv3_pre_de_done_buf_config(struct di_ch_s *pch,
				  bool flg_timeout)
{
	ulong irq_flag2 = 0;
	int tmp_cur_lev;
	struct di_buf_s *post_wr_buf = NULL;
	unsigned int glb_frame_mot_num = 0;
	unsigned int glb_field_mot_num = 0;
	unsigned int channel = pch->ch_id;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
//	struct di_ch_s		*pch;
//	struct dim_dvfm_s	*pdvfm;

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

		/*dec vf keep*/
		if (ppre->di_inp_buf &&
		    ppre->di_inp_buf->c.dec_vf_state & DI_BIT0) {
			ppre->di_wr_buf->c.in_buf = ppre->di_inp_buf;
			dimv3_print("dim:dec vf:l[%d],t[%d]\n",
				  ppre->di_wr_buf->c.in_buf->index,
				  ppre->di_wr_buf->c.in_buf->type);
			dimv3_print("dim:wr[%d],t[%d]\n",
				  ppre->di_wr_buf->index,
				  ppre->di_wr_buf->type);
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
						overturn,
						/*ppre->di_inp_buf->vframe*/
						&ppre->dvfm->vframe.vfm);
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
#ifdef DI_BUFFER_DEBUG
			dimv3_print("%s:set di_mem to di_chan2,", __func__);
			dimv3_print("%s:set di_chan2 to di_wr_buf\n", __func__);
#endif
			} else {
				ppre->di_mem_buf_dup_p->c.pre_ref_count = 0;
				/*recycle the progress throw buffer*/
				if (ppre->di_wr_buf->c.throw_flag) {
					ppre->di_wr_buf->
						c.pre_ref_count = 0;
					ppre->di_mem_buf_dup_p = NULL;
#ifdef DI_BUFFER_DEBUG
				dimv3_print(
				"%s set throw %s[%d] pre_ref_count to 0.\n",
				__func__,
				vframe_type_name[ppre->di_wr_buf->type],
				ppre->di_wr_buf->index);
#endif
				} else {
					ppre->di_mem_buf_dup_p
						= ppre->di_wr_buf;
				}
#ifdef DI_BUFFER_DEBUG
			dimv3_print(
				"%s: set di_mem_buf_dup_p to di_wr_buf\n",
				__func__);
#endif
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

#ifdef DI_BUFFER_DEBUG
			dimv3_print(
				"%s: %s[%d] => pre_ready_list\n", __func__,
				vframe_type_name[ppre->di_wr_buf->type],
				ppre->di_wr_buf->index);
#endif
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
#ifdef DI_BUFFER_DEBUG
				dimv3_print(
				"%s: di_mem_buf_dup_p = di_chan2_buf_dup_p\n",
				__func__);
#endif
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
			if (!(ppre->di_inp_buf->c.dec_vf_state & DI_BIT0)) {
				queuev3_in(channel,
					 ppre->di_inp_buf, QUEUE_RECYCLE);
				ppre->di_inp_buf = NULL;
		}
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
			    channel, ppre->in_seq);/*dbg*/

	dimv3_dbg_pre_cnt(channel, "d2");
}

#endif
static void recycle_vframe_type_pre(struct di_buf_s *di_buf,
				    unsigned int channel)
{
	ulong irq_flag2 = 0;

	di_lock_irqfiq_save(irq_flag2);

	queuev3_in(channel, di_buf, QUEUE_RECYCLE);

	di_unlock_irqfiq_restore(irq_flag2);
}

#if 0	/*move to di_pres.c*/
/*
 * add dummy buffer to pre ready queue
 */
static void addv3_dummy_vframe_type_pre(struct di_buf_s *src_buf,
				      unsigned int channel)
{
	struct di_buf_s *di_buf_tmp = NULL;

	if (!queuev3_empty(channel, QUEUE_LOCAL_FREE)) {
		di_buf_tmp = getv3_di_buf_head(channel, QUEUE_LOCAL_FREE);
		if (di_buf_tmp) {
			dimv3_print("%s\n");
			queuev3_out(channel, di_buf_tmp);
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
			div3_que_in(channel, QUE_PRE_READY, di_buf_tmp);
			#ifdef DI_BUFFER_DEBUG
			dimv3_print("%s: dummy %s[%d] => pre_ready_list\n",
				  __func__,
				  vframe_type_name[di_buf_tmp->type],
				  di_buf_tmp->index);
			#endif
		}
	}
}
#endif
/*
 * it depend on local buffer queue type is 2
 */
static int peek_free_linked_buf(unsigned int channel)
{
	struct di_buf_s *p = NULL;
	int itmp, p_index = -2;

	if (listv3_count(channel, QUEUE_LOCAL_FREE) < 2)
		return -1;

	queue_for_each_entry(p, channel, QUEUE_LOCAL_FREE, list) {
		if (abs(p->index - p_index) == 1)
			return min(p->index, p_index);
		p_index = p->index;
	}
	return -1;
}

/*
 * it depend on local buffer queue type is 2
 */
static struct di_buf_s *get_free_linked_buf(int idx, unsigned int channel)
{
	struct di_buf_s *di_buf = NULL, *di_buf_linked = NULL;
	int pool_idx = 0, di_buf_idx = 0;
	struct queue_s *pqueue = get_queue(channel);
	struct di_buf_pool_s *pbuf_pool = get_buf_pool(channel);

	queue_t *q = &pqueue[QUEUE_LOCAL_FREE];

	if (listv3_count(channel, QUEUE_LOCAL_FREE) < 2)
		return NULL;
	if (q->pool[idx] != 0 && q->pool[idx + 1] != 0) {
		pool_idx = ((q->pool[idx] >> 8) & 0xff) - 1;
		di_buf_idx = q->pool[idx] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (di_buf_idx < pbuf_pool[pool_idx].size) {
				di_buf = &(pbuf_pool[pool_idx].
					di_buf_ptr[di_buf_idx]);
				queuev3_out(channel, di_buf);
				di_buf->c.sts |= EDI_ST_AS_LINKA;
			}
		}
		pool_idx = ((q->pool[idx + 1] >> 8) & 0xff) - 1;
		di_buf_idx = q->pool[idx + 1] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (di_buf_idx < pbuf_pool[pool_idx].size) {
				di_buf_linked =	&(pbuf_pool[pool_idx].
					di_buf_ptr[di_buf_idx]);
				queuev3_out(channel, di_buf_linked);
				di_buf_linked->c.sts |= EDI_ST_AS_LINKB;
			}
		}
		if (IS_ERR_OR_NULL(di_buf)) {
			pr_error("%s:err 1\n", __func__);
			//di_buf->c.sts |= EDI_ST_AS_LINK_ERR1;
			return NULL;
		}
		di_buf->c.di_wr_linked_buf = di_buf_linked;
	}
	return di_buf;
}

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
#if 0
static void pre_inp_canvas_config(struct vframe_s *vf)
{
	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(di_inp_idx[0],
				     &vf->canvas0_config[0]);
		canvas_config_config(di_inp_idx[1],
				     &vf->canvas0_config[1]);
		vf->canvas0Addr = (di_inp_idx[1] << 8) | (di_inp_idx[0]);
		if (vf->plane_num == 2) {
			vf->canvas0Addr |= (di_inp_idx[1] << 16);
		} else if (vf->plane_num == 3) {
			canvas_config_config(di_inp_idx[2],
					     &vf->canvas0_config[2]);
			vf->canvas0Addr |= (di_inp_idx[2] << 16);
		}
		vf->canvas1Addr = vf->canvas0Addr;
	}
}
#else
/*move to di_vframe*/
#if 0
static void pre_inp_canvas_config(struct di_buf_s *di_buf)
{
	struct dim_vmode_s *pvmode;
	struct vframe_s *vf;

	pvmode	= &di_buf->c.vmode;
	vf	= di_buf->vframe;

	if (pvmode->canvas0Addr == (u32)-1) {
		canvas_config_config(di_inp_idx[0],
				     &vf->canvas0_config[0]);
		canvas_config_config(di_inp_idx[1],
				     &vf->canvas0_config[1]);
		pvmode->canvas0Addr = (di_inp_idx[1] << 8) | (di_inp_idx[0]);

		if (vf->plane_num == 2) {
			pvmode->canvas0Addr |= (di_inp_idx[1] << 16);
		} else if (vf->plane_num == 3) {
			canvas_config_config(di_inp_idx[2],
					     &vf->canvas0_config[2]);
			pvmode->canvas0Addr |= (di_inp_idx[2] << 16);
		}
		pvmode->canvas1Addr = vf->canvas0Addr;
	}
}
#endif
#endif
#endif

#if 0
bool di_get_pre_hsc_down_en(void)
{
	return pre_hsc_down_en;
}
#endif
bool dbgv3_first_frame;	/*debug */
unsigned int  dbgv3_first_cnt_pre;
unsigned int  dbgv3_first_cnt_post;
#define DI_DBG_CNT	(2)
void dimv3_dbg_pre_cnt(unsigned int channel, char *item)
{
	bool flgs = false;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (ppre->field_count < DI_DBG_CNT) {
		dbgv3_first_frame("%d:%s:%d\n", channel, item,
				ppre->field_count);
		dbgv3_first_frame = true;
		flgs = true;
		dbgv3_first_cnt_pre = DI_DBG_CNT * 5;
		dbgv3_first_cnt_post = DI_DBG_CNT * 4 + 1;
	} else if (ppre->field_count == DI_DBG_CNT) {/*don't use >=*/
		dbgv3_first_frame = false;
		dbgv3_first_cnt_pre = 0;
		dbgv3_first_cnt_post = 0;
	}

	if ((dbgv3_first_frame) && !flgs && dbgv3_first_cnt_pre) {
		dbgv3_first_frame("%d:n%s:%d\n", channel, item,
				ppre->field_count);
		dbgv3_first_cnt_pre--;
	}
}

static void dbg_post_cnt(unsigned int ch, char *item)
{
	struct di_post_stru_s *ppost = get_post_stru(ch);

	if (dbgv3_first_cnt_post) {
		dbgv3_first_frame("%d:%s:%d\n", ch, item,
				ppost->frame_cnt);
		dbgv3_first_cnt_post--;
	}
}

#if 0
/*must been called when dim_pre_de_buf_config return true*/
void pre_p_asi_set_next(unsigned int ch)
{
	struct di_pre_stru_s *ppre = get_pre_stru(ch);

	ppre->p_asi_next = ppre->di_inp_buf;
}
#endif

/*for first frame no need to ready buf*/


/********************************************************
 * for pre local buf's vframe:
 *	tbuf: taget local di buf
 *	fbuf: input di buf
 ********************************************************/

bool vfm_fill_for_di_pre_local(struct di_buf_s *tbuf, struct di_buf_s *fbuf,
			       struct di_ch_s *pch)
{
//	struct di_ch_s		*pch;
	//struct vframe_s		*tvfm;
	//struct vframe_s		*fvfm;
	struct dim_wmode_s	*fwmode;
	struct dim_vmode_s	*fvmode;
	struct dim_vmode_s	*tvtp;

//	pch = get_chdata(ch);
	if (!pch		||
	    !tbuf		||
	    !fbuf) {
		PR_ERR("%s:no buf\n", __func__);
		return false;
	}

	//tvfm	= tbuf->vframe;
	//fvfm	= fbuf->vframe;
	fwmode	= &fbuf->c.wmode;
	fvmode	= &fbuf->c.vmode;
	tvtp	= &tbuf->c.vmode;
	#if 0	/*remove vframe 12-03*/
	memcpy(tvfm, fbuf->vframe, sizeof(*tvfm));

	tvfm->private_data	= tbuf;
	#ifdef CAN
	/*CAN ? any use ?*/
	tvfm->canvas0Addr	= tbuf->nr_canvas_idx;
	tvfm->canvas1Addr	= tbuf->nr_canvas_idx;
	#endif

	/* set vframe bit info */
	tvfm->bitdepth &= ~(BITDEPTH_YMASK | FULL_PACK_422_MODE);
	tvfm->bitdepth |= pch->cfgt.vfm_bitdepth;

	if (fwmode->is_i || fwmode->p_as_i) {
		if (VFMT_IS_TOP(fvfm->type))
			tvfm->type = VIDTYPE_INTERLACE_TOP	|
				     VIDTYPE_VIU_422		|
				     VIDTYPE_VIU_SINGLE_PLANE	|
				     VIDTYPE_VIU_FIELD;
		else
			tvfm->type = VIDTYPE_INTERLACE_BOTTOM	|
				     VIDTYPE_VIU_422		|
				     VIDTYPE_VIU_SINGLE_PLANE	|
				     VIDTYPE_VIU_FIELD;
	} else {
		tvfm->type	= VIDTYPE_PROGRESSIVE		|
				  VIDTYPE_VIU_422		|
				  VIDTYPE_VIU_SINGLE_PLANE	|
				  VIDTYPE_VIU_FIELD;

	}
	/*add for vpp skip line ref*/
	tvfm->type |= VIDTYPE_PRE_INTERLACE;
	#endif /*remove vframe*/
	/*vtyp*/
	if (fwmode->is_i || fwmode->p_as_i) {
		if (fwmode->is_top)
			tvtp->vtype	= DIM_VFM_LOCAL_T;
		else
			tvtp->vtype	= DIM_VFM_LOCAL_B;
	} else {
		tvtp->vtype	= DIM_VFM_VIU_422;
	}
	if (fwmode->is_afbc)
		tvtp->vtype	|= DIM_VFM_COMPRESS;

	tvtp->canvas0Addr	= tbuf->nr_canvas_idx;
	tvtp->h			= fbuf->c.wmode.src_h;	/*?*/
	tvtp->w			= fbuf->c.wmode.src_w;	/*?*/
	tvtp->bitdepth &= ~(BITDEPTH_YMASK | FULL_PACK_422_MODE);
	tvtp->bitdepth |= pch->cfgt.vfm_bitdepth;
	tvtp->omx_index = fvmode->omx_index;
	tvtp->ready_jiffies64 = fvmode->ready_jiffies64;

	/*bit_mode copy from config_di_mif*/
	if (tvtp->bitdepth & BITDEPTH_Y10) {
		if (tvtp->vtype & DIM_VFM_VIU_444)
			tvtp->bit_mode =
			(tvtp->bitdepth & FULL_PACK_422_MODE) ?
			3 : 2;
		else if (tvtp->vtype & DIM_VFM_VIU_422)
			tvtp->bit_mode =
			(tvtp->bitdepth & FULL_PACK_422_MODE) ?
			3 : 1;
	} else {
		tvtp->bit_mode = 0;
	}

	return true;
}

/********************************************************
 * dim_pre_cfg_ele_change:
 * check if change or not;
 * set ppre
 * return: 0: not change; 1: change;
 ********************************************************/
unsigned int dim_pre_cfg_ele_change(struct di_ch_s *pch,
				    struct dim_dvfm_s *pdvfm)
{
	unsigned int ch;
	struct di_pre_stru_s *ppre;
	unsigned int ret;
	unsigned char change_type = 0;
	unsigned char change_type2 = 0;
	struct dim_dvfm_s	*plstdvfm;

	if (!pch)
		return 0;

	/*variable set*/
	ret = 0;
	ch = pch->ch_id;
	ppre = get_pre_stru(ch);
	plstdvfm = &pch->lst_dvfm;

	/*check*/
	prev3_vinfo_set(ch, &pdvfm->in_inf);
	change_type = isv3_source_change_dfvm(plstdvfm, pdvfm, ch);
	/*****************************/
	/*source change*/
	/* source change, when i mix p,force p as i*/
	if (change_type == 1 ||
	    (change_type == 2 && ppre->cur_prog_flag == 1)) {
	    /*last is p £¿*/
		if (ppre->di_mem_buf_dup_p) {
			/*avoid only 2 i field then p field*/
			if ((ppre->cur_prog_flag == 0) &&
			    dimp_get(eDI_MP_use_2_interlace_buff))
				ppre->di_mem_buf_dup_p->c.post_proc_flag = -1;

			ppre->di_mem_buf_dup_p->c.pre_ref_count = 0;
			ppre->di_mem_buf_dup_p = NULL;
		}
		if (ppre->di_chan2_buf_dup_p) {
			/*avoid only 1 i field then p field*/
			if ((ppre->cur_prog_flag == 0) &&
			    dimp_get(eDI_MP_use_2_interlace_buff))
				ppre->di_chan2_buf_dup_p->c.post_proc_flag = -1;

			ppre->di_chan2_buf_dup_p->c.pre_ref_count = 0;
			ppre->di_chan2_buf_dup_p = NULL;
		}
		PR_INF("%s:ch[%d]:%ums %dth source change:\n",
		       __func__,
			ch,
			dim_get_timerms(pdvfm->vmode.ready_jiffies64),
			ppre->in_seq);
		PR_INF("\t0x%x/%d/%d/%d=>0x%x/%d/%d/%d\n",
			plstdvfm->in_inf.vtype_ori,
			plstdvfm->in_inf.w,
			plstdvfm->in_inf.h,
			plstdvfm->in_inf.src_type,
			pdvfm->in_inf.vtype_ori,
			pdvfm->in_inf.w,
			pdvfm->in_inf.h,
			pdvfm->in_inf.src_type);

		ppre->cur_width		= pdvfm->in_inf.w;//pdvfm->wmode.w;
		ppre->cur_height	= pdvfm->in_inf.h;

		ppre->cur_prog_flag = !pdvfm->wmode.is_i;

		//this is set by mode ppre->prog_proc_type = 0; /*p as i*/

		ppre->cur_source_type	= pdvfm->in_inf.src_type;
		ppre->cur_sig_fmt	= pdvfm->in_inf.sig_fmt;
		ppre->orientation	= pdvfm->wmode.is_angle;

		ppre->source_change_flag	= 1;
		ppre->input_size_change_flag	= true;

		ppre->field_count = 0;
		ret = 1;
	/*source change end*/
	/********************************************************/
	}
	ppre->cur_inp_type = pdvfm->in_inf.vtype_ori;
	dimv3_set_pre_dfvm(plstdvfm, pdvfm);
	/********************************************************/
	if (!change_type) {
		change_type2 = isv3_vinfo_change(ch);
		if (change_type2) {
			/*ppre->source_change_flag = 1;*/
			ppre->input_size_change_flag = true;
		}
	}

	ppre->source_trans_fmt = pdvfm->in_inf.trans_fmt;
	ppre->left_right = ppre->left_right ? 0 : 1;

	ppre->invert_flag = pdvfm->wmode.is_invert_tp;
	return ret;
}

/********************************************************
 * for prog process as i top:
 ********************************************************/
unsigned char dim_pre_de_buf_config_p_asi_t(unsigned int ch)
{

	struct di_buf_s *di_buf = NULL;
	struct vframe_s *vframe;
//	unsigned char change_type = 0;
//	unsigned char change_type2 = 0;
	void **pvframe_in = getv3_vframe_in(ch);
	struct di_pre_stru_s *ppre = get_pre_stru(ch);

	struct di_ch_s		*pch;
	struct dim_dvfm_s	*pdvfm = NULL;
	struct dim_dvfm_s	*plstdvfm;// = &ppre->lst_dvfm;
	struct di_buf_s *di_buf_tmp = NULL;

	if (!dipv3_cma_st_is_ready(ch))
		return 0;

	if (div3_que_list_count(ch, QUE_IN_FREE) < 2 ||
	    queuev3_empty(ch, QUEUE_LOCAL_FREE))
		return 0;

	if (div3_que_list_count(ch, QUE_PRE_READY) >= DI_PRE_READY_LIMIT)
		return 0;

	/*****************get dvfm****************/
	pch = get_chdata(ch);
	pdvfm = dvfmv3_peek(pch, QUED_T_IN);
	plstdvfm = &pch->lst_dvfm;

	if (pdvfm)
		pdvfm = dvfmv3_get(pch, QUED_T_IN);
	if (!pdvfm || !pdvfm->vfm_in) {
		PR_ERR("%s:no pdvfm?\n", __func__);
		return 0;
	}

	vframe = &pdvfm->vframe.vfm;
	dimv3_tr_ops.pre_get(pdvfm->vmode.omx_index);
	//didbg_vframe_in_copy(ch, vframe);
	pdvfm->wmode.seq = ppre->in_seq;
	dimv3_print("DI:ch[%d] get %dth vf[0x%p] from frontend %u ms.\n",
		  ch,
		  ppre->in_seq, vframe,
		  dim_get_timerms(vframe->ready_jiffies64));

	/*change and set ppre*/
	dim_pre_cfg_ele_change(pch, pdvfm);
	ppre->prog_proc_type = 0; /*p as i*/
	/********************************************************/

	//vframe->type = pdvfm->wmode.vtype;
	//ppre->source_trans_fmt = vframe->trans_fmt;
	//ppre->left_right = ppre->left_right ? 0 : 1; /*?*/
	//ppre->invert_flag = 0;
	//ppre->width_bk = vframe->width;
	//vframe->width	= pdvfm->wmode.w;
	//vframe->height	= pdvfm->wmode.h;
	/* backup frame motion info */
	//vframe->combing_cur_lev = dimp_get(eDI_MP_cur_lev);/*cur_lev;*/

	dimv3_print("%s: vf_get => 0x%p\n", __func__, vframe);
	/*buf from IN_FREE************************/
	di_buf = div3_que_out_to_di_buf(ch, QUE_IN_FREE);

	if (dimv3_check_di_buf(di_buf, 10, ch))
		return 0;

	/*dbg*/
	dimv3_dbg_pre_cnt(ch, "cf1");
	/*****************************************/
	pvframe_in[di_buf->index] = NULL;// for p as i //vframe;

	#if 0
	/*di_buf[in] set vframe*/
	memcpy(di_buf->vframe, vframe, sizeof(vframe_t));
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->width	= pdvfm->wmode.src_w;
	di_buf->vframe->height	= pdvfm->wmode.src_h;
	di_buf->vframe->combing_cur_lev = dimp_get(eDI_MP_cur_lev);
	#endif
	/**/
	//di_buf->c.width_bk = ppre->width_bk;	/*ary.sui 2019-04-23*/
	di_buf->c.width_bk = pdvfm->wmode.o_w;
	di_buf->c.seq = ppre->in_seq;
	/*wmode*/
	di_buf->c.wmode = pdvfm->wmode;
	di_buf->c.sts |= EDI_ST_P_T;
	/*vmode*/
	di_buf->c.vmode	= pdvfm->vmode;
	di_buf->c.pdvfm = pdvfm;
	ppre->in_seq++;

	/*proc p as i only */
	//pvframe_in[di_buf->index] = NULL;

	//di_buf->vframe->type = VFMT_SET_TOP(di_buf->vframe->type);

	di_buf->c.post_proc_flag = 0;

	/*2sd********************************/
	di_buf_tmp = div3_que_out_to_di_buf(ch, QUE_IN_FREE);
	if (dimv3_check_di_buf(di_buf_tmp, 10, ch)) {
		recycle_vframe_type_pre(di_buf, ch);
		PR_ERR("DI:no free in_buffer for progressive skip.\n");
		return 0;
	}

	//di_buf_tmp->vframe->private_data = di_buf_tmp;
	di_buf_tmp->c.seq = ppre->in_seq;
	ppre->in_seq++;
	pvframe_in[di_buf_tmp->index] = pdvfm->vfm_in;

	//memcpy(di_buf_tmp->vframe, vframe, sizeof(*di_buf_tmp->vframe));
	ppre->di_inp_buf_next = di_buf_tmp;
	di_buf_tmp->c.wmode	= di_buf->c.wmode;
	di_buf_tmp->c.width_bk	= di_buf->c.width_bk;
	/*vmode*/
	di_buf_tmp->c.vmode	= di_buf->c.vmode;
	di_buf_tmp->c.pdvfm	= pdvfm;
	#if 0
	di_buf_tmp->vframe->type &= (~VIDTYPE_TYPEMASK);
	di_buf_tmp->vframe->type |= VIDTYPE_INTERLACE_BOTTOM;
	#else
	//di_buf_tmp->vframe->type = VFMT_SET_BOTTOM(di_buf_tmp->vframe->type);
	#endif
	di_buf_tmp->c.post_proc_flag = 0;
	/*2sd end****************************/
	ppre->di_inp_buf = di_buf;

	if (!ppre->di_mem_buf_dup_p) /*ary use ?*/
		ppre->di_mem_buf_dup_p = di_buf;

	/*proc p as i end */
	/*******************/

	/*is proc end*/
	/********************************************************/

/*main proc end*/
/*****************************************************************************/
	/*di_buf[local]*/
	di_buf = getv3_di_buf_head(ch, QUEUE_LOCAL_FREE);
	if (dimv3_check_di_buf(di_buf, 11, ch)) {
		/* recycle_keep_buffer();
		 *  pr_dbg("%s:recycle keep buffer\n", __func__);
		 */
		recycle_vframe_type_pre(ppre->di_inp_buf, ch);
		PR_ERR("%s:no local\n", __func__);
		return 0;
	}
	queuev3_out(ch, di_buf);/*QUEUE_LOCAL_FREE*/


	ppre->di_wr_buf = di_buf;
	//ppre->di_wr_buf->c.pre_ref_count = 1;
	/*set local buf vfram*/

	/* set local buf vframe */
	vfm_fill_for_di_pre_local(di_buf, ppre->di_inp_buf, pch);

	/*set local buf c*/
	di_buf->c.canvas_config_flag = 2;
	di_buf->c.di_wr_linked_buf = NULL;
	di_buf->c.wmode = ppre->di_inp_buf->c.wmode;
	di_buf->c.pre_ref_count = 1;

	//err: vmode have set in vfm_fill_for_di_pre_local
	//di_buf->c.vmode = ppre->di_inp_buf->c.vmode;//new
	//di_buf->c.width_bk = ppre->width_bk;	/*ary:2019-04-23*/
	di_buf->c.width_bk = pdvfm->wmode.o_w;

	if (is_bypass_post(ch)) {
		if (dimp_get(eDI_MP_bypass_post_state) == 0)
			ppre->source_change_flag = 1;

		dimp_set(eDI_MP_bypass_post_state, 1);
	} else {
		if (dimp_get(eDI_MP_bypass_post_state))
			ppre->source_change_flag = 1;

		dimp_set(eDI_MP_bypass_post_state, 0);
	}

	if (ppre->di_inp_buf->c.post_proc_flag == 0) {
		ppre->madi_enable = 0;
		ppre->mcdi_enable = 0;
		di_buf->c.post_proc_flag = 0;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	}

	if ((ppre->di_mem_buf_dup_p == ppre->di_wr_buf) ||
	    (ppre->di_chan2_buf_dup_p == ppre->di_wr_buf)) {
		pr_dbg("+++++++++++++++++++++++\n");
		if (recovery_flag == 0)
			recovery_log_reason = 12;

		recovery_flag++;
		return 0;
	}

	/**************************************/
	if (pdvfm) {
		pch = get_chdata(ch);
		ppre->dvfm = pdvfm;
		pdvfm->etype = EDIM_DISP_T_PRE;
		qued_ops.in(pch, QUED_T_PRE, pdvfm->index);

		di_buf->c.pdvfm = pdvfm; /*2019-12-03*/
	}
	return 1;
}


/********************************************************
 * for prog process as i bottom:
 ********************************************************/

unsigned char dim_pre_de_buf_config_p_asi_b(unsigned int ch)
{
	struct di_buf_s *di_buf = NULL;

	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	struct di_ch_s		*pch;

	if (!dipv3_cma_st_is_ready(ch))
		return 0;

	if (queuev3_empty(ch, QUEUE_LOCAL_FREE))
		return 0;

	pch = get_chdata(ch);
	if (!pch)
		return 0;

	ppre->di_inp_buf = ppre->di_inp_buf_next;
	ppre->di_inp_buf_next = NULL;

	if (!ppre->di_mem_buf_dup_p) {/* use n */
		ppre->di_mem_buf_dup_p = ppre->di_inp_buf;
	}

	/*di_buf[local]*/
	di_buf = getv3_di_buf_head(ch, QUEUE_LOCAL_FREE);
	if (dimv3_check_di_buf(di_buf, 11, ch)) {
		/* recycle_keep_buffer();
		 *  pr_dbg("%s:recycle keep buffer\n", __func__);
		 */
		recycle_vframe_type_pre(ppre->di_inp_buf, ch);
		PR_ERR("%s:no local\n", __func__);
		return 0;
	}

	queuev3_out(ch, di_buf);/*QUEUE_LOCAL_FREE*/

	ppre->di_wr_buf = di_buf;
	//ppre->di_wr_buf->c.pre_ref_count = 1;
	/*set local buf vfram*/

	/* set local buf vframe */
	vfm_fill_for_di_pre_local(di_buf, ppre->di_inp_buf, pch);

	/*set local buf c*/
	di_buf->c.canvas_config_flag = 2;
	di_buf->c.di_wr_linked_buf = NULL;
	di_buf->c.wmode = ppre->di_inp_buf->c.wmode;
	di_buf->c.wmode.is_top =  0;	//new
	di_buf->c.pre_ref_count = 1;
	// vmode have set in vfm_fill_for_di_pre_local
	//err di_buf->c.vmode = ppre->di_inp_buf->c.vmode; //new
	di_buf->c.sts |= EDI_ST_P_B;
	di_buf->c.width_bk = ppre->di_inp_buf->c.width_bk;

	if (is_bypass_post(ch)) {
		if (dimp_get(eDI_MP_bypass_post_state) == 0)
			ppre->source_change_flag = 1;

		dimp_set(eDI_MP_bypass_post_state, 1);
	} else {
		if (dimp_get(eDI_MP_bypass_post_state))
			ppre->source_change_flag = 1;

		dimp_set(eDI_MP_bypass_post_state, 0);
	}

	if (ppre->di_inp_buf->c.post_proc_flag == 0) {
		ppre->madi_enable = 0;
		ppre->mcdi_enable = 0;
		di_buf->c.post_proc_flag = 0;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	}

	#if 0
	if ((ppre->di_mem_buf_dup_p == ppre->di_wr_buf) ||
	    (ppre->di_chan2_buf_dup_p == ppre->di_wr_buf)) {
		pr_dbg("+++++++++++++++++++++++\n");
		if (recovery_flag == 0)
			recovery_log_reason = 12;

		recovery_flag++;
		return 0;
	}
	#endif
	/**************************************/
	return 1;
}


unsigned char dim_pre_de_buf_config_bypass(unsigned int ch)
{
	struct di_buf_s *di_buf = NULL;
	//struct vframe_s *vframe;
	int i;
	//int i, di_linked_buf_idx = -1;
	//unsigned char change_type = 0;
	//unsigned char change_type2 = 0;
	/*bool bit10_pack_patch = false; move to dim_cfg_init*/
	/*unsigned int width_roundup = 2; move to dim_cfg_init*/
	void **pvframe_in = getv3_vframe_in(ch);
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	//struct di_post_stru_s *ppost = get_post_stru(ch);
	//struct di_dev_s *de_devp = get_dim_de_devp();
	//int cfg_prog_proc = dimp_get(eDI_MP_prog_proc_config);
	struct di_ch_s		*pch;
	struct dim_dvfm_s	*pdvfm = NULL;
//	struct dim_dvfm_s	*plstdvfm;// = &ppre->lst_dvfm;
	int in_buf_num = 0;
	bool flg_eos = false;

	/*no local free*/
	if (div3_que_list_count(ch, QUE_IN_FREE) < 1)
		return 0;

	/*ary: */
	/* some provider has problem if receiver
	 * get all buffers of provider
	 */

	/*cur_lev = 0;*/
	dimp_set(eDI_MP_cur_lev, 0);
	/*ary: need change to count ready buf*/
	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		if (pvframe_in[i])
			in_buf_num++;
	if (in_buf_num > BYPASS_GET_MAX_BUF_NUM)
		return 0;

	dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);/*?*/

	/*****************get dvfm****************/
	pch = get_chdata(ch);
	//plstdvfm = &pch->lst_dvfm;
	pdvfm = dvfmv3_peek(pch, QUED_T_IN);

	if (pdvfm)
		pdvfm = dvfmv3_get(pch, QUED_T_IN);
	if (!pdvfm || !pdvfm->vfm_in)
		return 0;
	//vframe = pdvfm->vfm_in;
	flg_eos = pdvfm->wmode.is_eos;
	if (flg_eos)
		PR_INF("%s:EOS\n", __func__);

	dimv3_set_pre_dfvm_last_bypass(pch);
	/*****************************************/
	dimv3_tr_ops.pre_get(pdvfm->vmode.omx_index);
	ppre->invert_flag = pdvfm->wmode.is_invert_tp;

	/*buf from IN_FREE*************/
	/*in buf*/
	di_buf = div3_que_out_to_di_buf(ch, QUE_IN_FREE);

	if (dimv3_check_di_buf(di_buf, 10, ch))
		return 0;
	/******************************/

	/*di_buf[in] set*/
	//memcpy(di_buf->vframe, vframe, sizeof(vframe_t));
	//di_buf->vframe->private_data = di_buf;

	dimv3_dbg_pre_cnt(ch, "cf1");
	//di_buf->c.width_bk = ppre->width_bk;	/*ary.sui 2019-04-23*/

	pvframe_in[di_buf->index] = pdvfm->vfm_in;
	di_buf->c.seq = ppre->in_seq;
	pdvfm->wmode.seq = ppre->in_seq;
	ppre->in_seq++;

	/*is bypass*/
	/* bypass progressive */
	//di_buf->c.seq = ppre->pre_ready_seq++;/*?*/
	di_buf->c.post_ref_count = 0;
	/*cur_lev = 0;*/
	//dimp_set(eDI_MP_cur_lev, 0);

	ppre->source_change_flag = 0;

	di_buf->c.new_format_flag = 0;

	/*from other mode to bypass mode*/
	if (di_bypass_state_get(ch) == 0) {
		if (ppre->di_mem_buf_dup_p) {
			ppre->di_mem_buf_dup_p->c.pre_ref_count = 0;
			ppre->di_mem_buf_dup_p = NULL;
		}
		if (ppre->di_chan2_buf_dup_p) {
			ppre->di_chan2_buf_dup_p->c.pre_ref_count = 0;
			ppre->di_chan2_buf_dup_p = NULL;
		}

			if (ppre->di_wr_buf) {
				ppre->di_wr_buf->c.pre_ref_count = 0;
				ppre->di_wr_buf->c.post_ref_count = 0;
				recycle_vframe_type_pre(
					ppre->di_wr_buf, ch);

				ppre->di_wr_buf = NULL;
			}

		di_buf->c.new_format_flag = 1;
		di_bypass_state_set(ch, true);/*bypass_state:1;*/

		dimv3_print(
			"%s:bypass_state = 1\n",
			__func__);

	}
	/*di_bypass_state_get(channel) == 0 end*/
	/***************************************/
	//top_bot_config(di_buf);
	//di_buf->vframe.type = pdvfm->in_inf.vtype;

	/*if previous isn't bypass post_wr_buf not recycled */

	if ((bypass_pre & 0x2) && !ppre->cur_prog_flag)
		di_buf->c.post_proc_flag = -2;
	else
		di_buf->c.post_proc_flag = 0;

	di_buf->c.wmode = pdvfm->wmode;
	di_buf->c.vmode = pdvfm->vmode;/*for bypass*/
	di_buf->c.pdvfm = pdvfm;

	div3_que_in(ch, QUE_PRE_READY, di_buf);
	dimv3_print("di:cfg:post_proc_flag=%d\n",
		  di_buf->c.post_proc_flag);

	qued_ops.in(pch, QUED_T_PRE, pdvfm->index);
	return 0;
}

/********************************************************
 * for prog process use i buf:
 ********************************************************/
unsigned char dim_pre_de_buf_config_p_use_2ibuf(unsigned int ch)
{
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *vframe;

	int di_linked_buf_idx = -1;
//	unsigned char change_type = 0;
//	unsigned char change_type2 = 0;

	void **pvframe_in = getv3_vframe_in(ch);
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	struct di_ch_s		*pch;
	struct dim_dvfm_s	*pdvfm = NULL;
	struct dim_dvfm_s	*plstdvfm;// = &ppre->lst_dvfm;

	#if 0
	if (!dipv3_cma_st_is_ready(ch))
		return 0;

	if (div3_que_list_count(ch, QUE_IN_FREE) < 1)
		return 0;

	if (div3_que_list_count(ch, QUE_PRE_READY) >= DI_PRE_READY_LIMIT)
		return 0;
	#endif
/**************************************************************************/
	if (ppre->prog_proc_type == 2) {

	#if 0
		di_linked_buf_idx = peek_free_linked_buf(ch);
		if (di_linked_buf_idx == -1 &&
		    !IS_ERR_OR_NULL(ppost->keep_buf)) { /*ary: ?*/
			recycle_keep_buffer(ch);
			PR_INF("%s: recycle keep buffer null buf\n",
			       __func__);
		return 0;
		}
	#else
		di_linked_buf_idx = peek_free_linked_buf(ch);
		if (di_linked_buf_idx == -1) {
			dimv3_print("%s:check no link buf\n", __func__);
			return 0;
		}
	#endif
	}
/**************************************************************************/

/*****************************************************************************/
/*main proce */

	/*****************get dvfm****************/
	pch = get_chdata(ch);
	pdvfm = dvfmv3_peek(pch, QUED_T_IN);
	plstdvfm = &pch->lst_dvfm;

	if (pdvfm)
		pdvfm = dvfmv3_get(pch, QUED_T_IN);
	if (!pdvfm || !pdvfm->vfm_in) {
		PR_ERR("%s:no pdvfm?\n", __func__);
		return 0;
	}

	vframe = &pdvfm->vframe.vfm;

	/*****************************************/
	dimv3_tr_ops.pre_get(pdvfm->vmode.omx_index);
	//didbg_vframe_in_copy(ch, vframe);
	pdvfm->wmode.seq = ppre->in_seq;
	dimv3_print("DI:ch[%d] get %dth vf[0x%p] from frontend %u ms.\n",
		  ch,
		  ppre->in_seq, vframe,
		  dim_get_timerms(vframe->ready_jiffies64));
	/********************************************************/
	/*change and set ppre*/
	dim_pre_cfg_ele_change(pch, pdvfm);
	ppre->prog_proc_type = 2;
	/*now:cfg_prog_proc is 0x23*/

	//vframe->type = pdvfm->wmode.vtype;

	//ppre->source_trans_fmt = vframe->trans_fmt;
	//ppre->left_right = ppre->left_right ? 0 : 1; /*?*/

	//ppre->invert_flag = 0;

	//vframe->type &= ~TB_DETECT_MASK; /*?vfm*/

	//ppre->width_bk = vframe->width;

	/* backup frame motion info */
	//vframe->combing_cur_lev = dimp_get(eDI_MP_cur_lev);/*cur_lev;*/

	dimv3_print("%s: vf_get => 0x%p\n", __func__, vframe);

	/*buf from IN_FREE************************/
	di_buf = div3_que_out_to_di_buf(ch, QUE_IN_FREE);

	if (dimv3_check_di_buf(di_buf, 10, ch)) {
		PR_ERR("%s no IN_FREE\n", __func__);
		return 0;
	}
	/*dbg*/
	dimv3_dbg_pre_cnt(ch, "cf1");
	/*****************************************/
	pvframe_in[di_buf->index] = pdvfm->vfm_in;

	#if 0
	/*di_buf[in] set vframe*/
	memcpy(di_buf->vframe, vframe, sizeof(vframe_t));
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->width	= pdvfm->wmode.src_w;
	di_buf->vframe->height	= pdvfm->wmode.src_h;
	#endif
	/**/
	di_buf->c.width_bk = pdvfm->wmode.o_w;	/*ary.sui 2019-04-23*/
	di_buf->c.seq = ppre->in_seq;
	di_buf->c.wmode = pdvfm->wmode;
	di_buf->c.sts |= EDI_ST_P_U2I;
	/*vtyp*/
	di_buf->c.vmode	= pdvfm->vmode;
	di_buf->c.pdvfm = pdvfm;
	ppre->in_seq++;

	/********************************************************/
	/*is proc*/
	if (di_buf->c.wmode.p_use_2i) {
		/*ary: not process prog as i*/
		di_buf->c.post_proc_flag = 0;

		ppre->di_inp_buf = di_buf;

		dimv3_print("%s: %s[%d] => di_inp_buf\n",
			__func__,
			vframe_type_name[di_buf->type],
			di_buf->index);

		if (!ppre->di_mem_buf_dup_p) {
			/* use n */
			ppre->di_mem_buf_dup_p = di_buf;

			dimv3_print("%s: set mem to be di_inp_buf\n", __func__);

		}

	/*is proc end*/
	/********************************************************/
	}

/*main proc end*/
/*****************************************************************************/

	/*dim_dbg_pre_cnt(channel, "cfg");*/
	/* get local buf link buf*/
	if (ppre->prog_proc_type == 2) {
		di_linked_buf_idx = peek_free_linked_buf(ch);
		if (di_linked_buf_idx != -1)
			di_buf = get_free_linked_buf(di_linked_buf_idx, ch);
		else
			di_buf = NULL;
		if (!di_buf) {
			PR_ERR("%s no link local buf\n", __func__);
			return 0;
		}
		di_buf->c.post_proc_flag = 0;
		di_buf->c.di_wr_linked_buf->c.pre_ref_count = 0;
		di_buf->c.di_wr_linked_buf->c.post_ref_count = 0;
		di_buf->c.canvas_config_flag = 1;
	}

	ppre->di_wr_buf = di_buf;
	ppre->di_wr_buf->c.pre_ref_count = 1;

#ifdef DI_BUFFER_DEBUG
	dimv3_print("%s: %s[%d] => di_wr_buf\n", __func__,
		  vframe_type_name[di_buf->type], di_buf->index);
	if (di_buf->c.di_wr_linked_buf)
		dimv3_print("%s: linked %s[%d] => di_wr_buf\n", __func__,
			  vframe_type_name[di_buf->c.di_wr_linked_buf->type],
			  di_buf->c.di_wr_linked_buf->index);
#endif
#if 0
	/*di_buf[local]*/
	if (!di_buf)
		PR_ERR("%s:err1\n", __func__);
	if (!di_buf->vframe)
		PR_ERR("%s:err2\n", __func__);
	if (!ppre->di_inp_buf)
		PR_ERR("%s:err3\n", __func__);
	if (!ppre->di_inp_buf->vframe)
		PR_ERR("%s:err4\n", __func__);
#endif

	vfm_fill_for_di_pre_local(di_buf, ppre->di_inp_buf, pch);

	di_buf->c.wmode = ppre->di_inp_buf->c.wmode;
	// vmode have set in vfm_fill_for_di_pre_local
	//err: di_buf->c.vmode = ppre->di_inp_buf->c.vmode; //new
	di_buf->c.width_bk = pdvfm->wmode.o_w;	/*ary:2019-04-23*/

	if (is_bypass_post(ch)) {
		if (dimp_get(eDI_MP_bypass_post_state) == 0)
			ppre->source_change_flag = 1;

		dimp_set(eDI_MP_bypass_post_state, 1);
	} else {
		if (dimp_get(eDI_MP_bypass_post_state))
			ppre->source_change_flag = 1;

		dimp_set(eDI_MP_bypass_post_state, 0);
	}

	if (ppre->di_inp_buf->c.post_proc_flag == 0) {
		ppre->madi_enable = 0;
		ppre->mcdi_enable = 0;
		di_buf->c.post_proc_flag = 0;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	}

	if ((ppre->di_mem_buf_dup_p == ppre->di_wr_buf) ||
	    (ppre->di_chan2_buf_dup_p == ppre->di_wr_buf)) {
		PR_ERR("%s+++++++++++++++++++++++\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 12;

		recovery_flag++;
		return 0;
	}

	/**************************************/
	if (pdvfm) {
		pch = get_chdata(ch);
		ppre->dvfm = pdvfm;
		pdvfm->etype = EDIM_DISP_T_PRE;
		qued_ops.in(pch, QUED_T_PRE, pdvfm->index);
		dimv3_print("%s:qued in\n", __func__);
		di_buf->c.pdvfm = pdvfm; /*2019-12-03*/
	} else {
		PR_ERR("%s:no pdvfm\n", __func__);
	}
	return 1;
}

unsigned char dim_pre_de_buf_config_i(unsigned int ch)
{
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *vframe;
	#ifdef HIS_V3
	unsigned char change_type = 0;
	#endif
	//unsigned char change_type2 = 0;
	unsigned int change = 0;
	void **pvframe_in = getv3_vframe_in(ch);
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	struct di_ch_s		*pch;
	struct dim_dvfm_s	*pdvfm = NULL;
//	struct dim_dvfm_s	*plstdvfm;// = &ppre->lst_dvfm;

/*****************************************************************************/
/*main proce */

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

	/*****************************************/
	dimv3_tr_ops.pre_get(pdvfm->vmode.omx_index);
	//didbg_vframe_in_copy(ch, vframe);
	pdvfm->wmode.seq = ppre->in_seq;
	dimv3_print("DI:ch[%d] get %dth from frontend %u ms.\n",
		  ch,
		  ppre->in_seq,
		  dim_get_timerms(0));

	/*change and set ppre*/
	change = dim_pre_cfg_ele_change(pch, pdvfm);
	ppre->prog_proc_type = 0;
	#if 0
	vframe->type = pdvfm->wmode.vtype;
	vframe->type &= ~TB_DETECT_MASK;

	/* backup frame motion info */
	vframe->combing_cur_lev = dimp_get(eDI_MP_cur_lev);/*cur_lev;*/
	#endif
	//dim_print("%s: vf_get => 0x%p\n", __func__, vframe);

	/*buf from IN_FREE*/
	di_buf = div3_que_out_to_di_buf(ch, QUE_IN_FREE);

	if (dimv3_check_di_buf(di_buf, 10, ch)) {
		PR_ERR("%s no IN_FREE\n", __func__);
		return 0;
	}

	dimv3_dbg_pre_cnt(ch, "cf1");
	pvframe_in[di_buf->index] = pdvfm->vfm_in;

	/*di_buf[in] set*/
	#if 0
	memcpy(di_buf->vframe, vframe, sizeof(vframe_t));

	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->width	= pdvfm->wmode.src_w;
	di_buf->vframe->height	= pdvfm->wmode.src_h;
	#endif

	//di_buf->c.width_bk = ppre->width_bk;	/*ary.sui 2019-04-23*/
	di_buf->c.seq = ppre->in_seq;
	/*wmode*/
	di_buf->c.wmode = pdvfm->wmode;
	di_buf->c.sts |= EDI_ST_I;
	/*vmode*/
	di_buf->c.vmode	= pdvfm->vmode;
	di_buf->c.pdvfm = pdvfm;
	/*keep dec vf*/
	di_buf->c.dec_vf_state = DI_BIT0;
	ppre->in_seq++;

	if (ppre->cur_prog_flag == 0) { /*ary??*/
		/* check if top/bot interleaved */
		#ifdef HIS_V3
		if (change_type == 2)
			/* source is i interleaves p fields */
			ppre->force_interlace = true;
		#endif
		if ((ppre->cur_inp_type &
		DIM_VFM_TYPEMASK) == (di_buf->c.vmode.vtype &
		DIM_VFM_TYPEMASK)) {
			if (di_buf->c.wmode.is_top)
				same_field_top_count++;
			else
				same_field_bot_count++;
		}
	}
	ppre->cur_inp_type = vframe->type;

	/*********************************/
	if (!ppre->di_chan2_buf_dup_p) {
		ppre->field_count = 0;
		/* ignore contp2rd and contprd */
	}
	di_buf->c.post_proc_flag = 1;
	ppre->di_inp_buf = di_buf;
	dimv3_print("%s: %s[%d] => di_inp_buf\n", __func__,
		  vframe_type_name[di_buf->type],
		  di_buf->index);

	if (!ppre->di_mem_buf_dup_p) {/* use n */
		ppre->di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
		dimv3_print(
		"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
			__func__);
#endif
	}

		/*other end*/
		/********************************************************/
/*main proc end*/
/*****************************************************************************/

	/*dim_dbg_pre_cnt(channel, "cfg");*/
	/* di_wr_buf */

	/*di_buf[local]*/
	di_buf = getv3_di_buf_head(ch, QUEUE_LOCAL_FREE);
	if (dimv3_check_di_buf(di_buf, 11, ch)) {
		/* recycle_keep_buffer();
		 *  pr_dbg("%s:recycle keep buffer\n", __func__);
		 */
		recycle_vframe_type_pre(ppre->di_inp_buf, ch);
		PR_ERR("%s no local free\n", __func__);
		return 0;
	}
	queuev3_out(ch, di_buf);/*QUEUE_LOCAL_FREE*/

	di_buf->c.canvas_config_flag = 2;
	di_buf->c.di_wr_linked_buf = NULL;

	ppre->di_wr_buf = di_buf;
	ppre->di_wr_buf->c.pre_ref_count = 1;

#ifdef DI_BUFFER_DEBUG
	dimv3_print("%s: %s[%d] => di_wr_buf\n", __func__,
		  vframe_type_name[di_buf->type], di_buf->index);
	if (di_buf->c.di_wr_linked_buf)
		dimv3_print("%s: linked %s[%d] => di_wr_buf\n", __func__,
			  vframe_type_name[di_buf->c.di_wr_linked_buf->type],
			  di_buf->c.di_wr_linked_buf->index);
#endif
#if 0
	/*di_buf[local]*/
	if (!di_buf)
		PR_ERR("%s:err1\n", __func__);
	if (!di_buf->vframe)
		PR_ERR("%s:err2\n", __func__);
	if (!ppre->di_inp_buf)
		PR_ERR("%s:err3\n", __func__);
	if (!ppre->di_inp_buf->vframe)
		PR_ERR("%s:err4\n", __func__);
#endif

	vfm_fill_for_di_pre_local(di_buf, ppre->di_inp_buf, pch);

	di_buf->c.wmode		= ppre->di_inp_buf->c.wmode;
	di_buf->c.wmode.seq_pre = ppre->field_count;
	di_buf->c.width_bk	= pdvfm->wmode.o_w;

	if (is_bypass_post(ch)) {
		if (dimp_get(eDI_MP_bypass_post_state) == 0)
			ppre->source_change_flag = 1;

		dimp_set(eDI_MP_bypass_post_state, 1);
	} else {
		if (dimp_get(eDI_MP_bypass_post_state))
			ppre->source_change_flag = 1;

		dimp_set(eDI_MP_bypass_post_state, 0);
	}

	if (ppre->di_inp_buf->c.post_proc_flag == 0) {
		ppre->madi_enable = 0;
		ppre->mcdi_enable = 0;
		di_buf->c.post_proc_flag = 0;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else if (dimp_get(eDI_MP_bypass_post_state)) {
		ppre->madi_enable = 0;
		ppre->mcdi_enable = 0;
		di_buf->c.post_proc_flag = 0;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else {/*i :*/
		ppre->madi_enable = (dimp_get(eDI_MP_pre_enable_mask) & 1);
		ppre->mcdi_enable =
			((dimp_get(eDI_MP_pre_enable_mask) >> 1) & 1);
		di_buf->c.post_proc_flag = 1;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER,
					     dimp_get(eDI_MP_mcpre_en));/*en*/
	}

	if ((ppre->di_mem_buf_dup_p == ppre->di_wr_buf) ||
	    (ppre->di_chan2_buf_dup_p == ppre->di_wr_buf)) {
		PR_ERR("%s+++++++++++++++++++++++\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 12;

		recovery_flag++;
		return 0;
	}

	/**************************************/
	if (pdvfm) {
		pch = get_chdata(ch);
		ppre->dvfm = pdvfm;
		pdvfm->etype = EDIM_DISP_T_PRE;
		qued_ops.in(pch, QUED_T_PRE, pdvfm->index);

		di_buf->c.pdvfm = pdvfm; /*2019-12-03*/
		dimv3_print("%s:qued in\n", __func__);
	} else {
		PR_ERR("%s:no pdvfm\n", __func__);
	}
	return 1;
}

unsigned char dimv3_pre_de_buf_config(unsigned int ch)
{
	PR_ERR("%s:ch[%d]\n", __func__, ch);
	#ifdef HIS_V3
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *vframe;
	int i, di_linked_buf_idx = -1;
	unsigned char change_type = 0;
	unsigned char change_type2 = 0;
	/*bool bit10_pack_patch = false; move to dim_cfg_init*/
	/*unsigned int width_roundup = 2; move to dim_cfg_init*/
	struct vframe_s **pvframe_in = getv3_vframe_in(ch);
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
//	struct di_post_stru_s *ppost = get_post_stru(ch);
	struct di_dev_s *de_devp = getv3_dim_de_devp();
	int cfg_prog_proc = dimp_get(eDI_MP_prog_proc_config);
	struct di_ch_s		*pch;
	struct dim_dvfm_s	*pdvfm = NULL;
	struct dim_dvfm_s	*plstdvfm;// = &ppre->lst_dvfm;

	#ifdef HIS_V3
	if (di_blocking || !dipv3_cma_st_is_ready(ch))
		return 0;

	if (div3_que_list_count(ch, QUE_IN_FREE) < 1)
		return 0;
	#endif
	if ((div3_que_list_count(ch, QUE_IN_FREE) < 2	&&
	     (!ppre->di_inp_buf_next))				||
	    (queuev3_empty(ch, QUEUE_LOCAL_FREE)))
		return 0;

	#ifdef HIS_V3
	if (div3_que_list_count(ch, QUE_PRE_READY) >= DI_PRE_READY_LIMIT)
		return 0;
	#endif
/**************************************************************************/
	if (dim_is_bypass(NULL, ch)) {
		/*ary: */
		/* some provider has problem if receiver
		 * get all buffers of provider
		 */
		int in_buf_num = 0;
		/*cur_lev = 0;*/
		dimp_set(eDI_MP_cur_lev, 0);
		for (i = 0; i < MAX_IN_BUF_NUM; i++)
			if (pvframe_in[i])
				in_buf_num++;
		if (in_buf_num > BYPASS_GET_MAX_BUF_NUM)
			return 0;

		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else if (ppre->prog_proc_type == 2) {

		#ifdef HIS_V3
		di_linked_buf_idx = peek_free_linked_buf(ch);
		if (di_linked_buf_idx == -1 &&
		    !IS_ERR_OR_NULL(ppost->keep_buf)) {	/*ary: ?*/
			recycle_keep_buffer(ch);
			PR_INF("%s: recycle keep buffer null buf\n",
			       __func__);
		return 0;
		}
		#else
		di_linked_buf_idx = peek_free_linked_buf(ch);
		if (di_linked_buf_idx == -1) {
			dimv3_print("%s:check no link buf\n", __func__);
			return 0;
		}
		#endif
	}
/**************************************************************************/
/*p's second */
	if (ppre->di_inp_buf_next) {
		ppre->di_inp_buf = ppre->di_inp_buf_next;
		ppre->di_inp_buf_next = NULL;
#ifdef DI_BUFFER_DEBUG
		dimv3_print("%s: di_inp_buf_next %s[%d] => di_inp_buf\n",
			  __func__,
			  vframe_type_name[ppre->di_inp_buf->type],
			  ppre->di_inp_buf->index);
#endif
		if (!ppre->di_mem_buf_dup_p) {/* use n */
			ppre->di_mem_buf_dup_p = ppre->di_inp_buf;
#ifdef DI_BUFFER_DEBUG
			dimv3_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
				__func__);
#endif
		}
	} else {
/*****************************************************************************/
/*main proce */
	#ifdef HIS_V3
		/* check if source change */
		vframe = pw_vf_peek(channel);

		if (vframe && is_from_vdin(vframe)) {
#ifdef RUN_DI_PROCESS_IN_IRQ
			ppre->vdin2nr = is_input2pre(channel);
#endif
		}

		vframe = pw_vf_get(channel);

		if (!vframe)
			return 0;
	#else
		/*****************get dvfm****************/
		pch = get_chdata(ch);
		plstdvfm = &pch->lst_dvfm;
		pdvfm = dvfmv3_peek(pch, QUED_T_IN);

		if (pdvfm)
			pdvfm = dvfmv3_get(pch, QUED_T_IN);
		if (!pdvfm || !pdvfm->vfm_in) {
			PR_ERR("%s:no pdvfm\n", __func__);
			return 0;
		}
		vframe = pdvfm->vfm_in;
		#if 0
		/*recycle dvfm temp*/
		pdvfm->vfm_in = NULL;
		qued_ops.in(pch, QUED_T_RECYCL, pdvfm->index);

		#endif
	#endif	/*****************************************/
		dimv3_tr_ops.pre_get(vframe->omx_index);
		didbg_vframe_in_copy(ch, vframe);
		#ifdef HIS_V3 /*no use*/
		if (vframe->type & VIDTYPE_COMPRESS) {
			vframe->width = vframe->compWidth;
			vframe->height = vframe->compHeight;
		}
		#endif
		dimv3_print("DI:ch[%d]get %dth vf[0x%p] from frontend %u ms.\n",
			    ch,
			    ppre->in_seq, vframe,
			    dim_get_timerms(vframe->ready_jiffies64));

		vframe->type = pdvfm->wmode.vtype;
		/*ary: no use?*/
		vframe->prog_proc_config
			= (cfg_prog_proc & 0x20) >> 5;

		#if 0	/*move to dvfm_fill_in*/
		if (vframe->width > 10000 || vframe->height > 10000 ||
		    hold_video || ppre->bad_frame_throw_count > 0) {
			if (vframe->width > 10000 || vframe->height > 10000)
				ppre->bad_frame_throw_count = 10;
			ppre->bad_frame_throw_count--;
			pw_vf_put(vframe, channel);
			pw_vf_notify_provider(
				channel, VFRAME_EVENT_RECEIVER_PUT, NULL);
			return 0;
		}
		#endif
		#if 0	/*ary add cfgt and move to dvfm_fill_in*/
		bit10_pack_patch =  (is_meson_gxtvbb_cpu() ||
							is_meson_gxl_cpu() ||
							is_meson_gxm_cpu());
		width_roundup = bit10_pack_patch ? 16 : width_roundup;

		if (dimp_get(eDI_MP_di_force_bit_mode) == 10) {
			if (dimp_get(eDI_MP_force_width)) {
				dimp_set(eDI_MP_force_width,
					 roundup(dimp_get(eDI_MP_force_width),
						 width_roundup));
			} else {
				dimp_set(eDI_MP_force_width,
					 roundup(vframe->width,
						 width_roundup));
			}
		}
		#endif
		ppre->source_trans_fmt = vframe->trans_fmt;
		ppre->left_right = ppre->left_right ? 0 : 1;

		#if 0
		ppre->invert_flag =
			(vframe->type &  TB_DETECT_MASK) ? true : false;
		#else
		ppre->invert_flag = pdvfm->wmode.is_invert_tp;
		#endif
		vframe->type &= ~TB_DETECT_MASK;

		#if 0	/**/
		if ((((invert_top_bot & 0x2) != 0)	||
		     ppre->invert_flag)			&&
		    (!is_progressive(vframe))) {
			if (
				(vframe->type & VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP) {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_BOTTOM;
			} else {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_TOP;
			}
		}
		#else

		#endif
		ppre->width_bk = vframe->width;
		#if 0
		if (dimp_get(eDI_MP_force_width))
			vframe->width = dimp_get(eDI_MP_force_width);
		if (dimp_get(eDI_MP_force_height))
			vframe->height = dimp_get(eDI_MP_force_height);
		#else
		//vframe->width	= pdvfm->wmode.w;
		//vframe->height	= pdvfm->wmode.h;
		#endif
		/* backup frame motion info */
		vframe->combing_cur_lev = dimp_get(eDI_MP_cur_lev);/*cur_lev;*/

		dimv3_print("%s: vf_get => 0x%p\n", __func__, vframe);

		/*buf from IN_FREE*/
		di_buf = div3_que_out_to_di_buf(ch, QUE_IN_FREE);

		if (dimv3_check_di_buf(di_buf, 10, ch)) {
			PR_ERR("%s no IN_FREE\n", __func__);
			return 0;
		}
		if (dimp_get(eDI_MP_di_log_flag) & DI_LOG_VFRAME)
			dimv3_dump_vframe(vframe);
		/*di_buf[in] set*/
		memcpy(di_buf->vframe, vframe, sizeof(vframe_t));
		dimv3_dbg_pre_cnt(ch, "cf1");
		di_buf->c.width_bk = ppre->width_bk;	/*ary.sui 2019-04-23*/
		di_buf->vframe->private_data = di_buf;
		di_buf->vframe->width	= pdvfm->wmode.src_w;
		di_buf->vframe->height	= pdvfm->wmode.src_h;
		pvframe_in[di_buf->index] = vframe;
		di_buf->c.seq = ppre->in_seq;
		pdvfm->wmode.seq = ppre->in_seq;
		di_buf->c.wmode = pdvfm->wmode;
		ppre->in_seq++;

		prev3_vinfo_set(ch, &pdvfm->in_inf);
		change_type = isv3_source_change_dfvm(plstdvfm, pdvfm, ch);
		#if 0
		if (!change_type)
			change_type = isv3_vinfo_change(channel);
		#endif
		/********************************************************/
		/*source change*/
		/* source change, when i mix p,force p as i*/
		if (change_type == 1 ||
		    (change_type == 2 && ppre->cur_prog_flag == 1)) {
		    /*last is p £¿*/
			if (ppre->di_mem_buf_dup_p) {
				/*avoid only 2 i field then p field*/
				if (
					(ppre->cur_prog_flag == 0) &&
					dimp_get(eDI_MP_use_2_interlace_buff))
					ppre->di_mem_buf_dup_p->
					c.post_proc_flag = -1;
				ppre->di_mem_buf_dup_p->c.pre_ref_count = 0;
				ppre->di_mem_buf_dup_p = NULL;
			}
			if (ppre->di_chan2_buf_dup_p) {
				/*avoid only 1 i field then p field*/
				if (
					(ppre->cur_prog_flag == 0) &&
					dimp_get(eDI_MP_use_2_interlace_buff))
					ppre->di_chan2_buf_dup_p->
					c.post_proc_flag = -1;
				ppre->di_chan2_buf_dup_p->c.pre_ref_count = 0;
				ppre->di_chan2_buf_dup_p = NULL;
			}
			#if 0
			/* channel change will occur between atv and dtv,
			 * that need mirror
			 */
			if (!IS_ERR_OR_NULL(di_post_stru.keep_buf)) {
				if (di_post_stru.keep_buf->vframe
					->source_type !=
					di_buf->vframe->source_type) {
					recycle_keep_buffer();
					pr_info("%s: source type changed recycle buffer!!!\n",
						__func__);
				}
			}
			#endif
			PR_INF(
			"%s:ch[%d]:%ums %dth source change: 0x%x/%d/%d/%d=>0x%x/%d/%d/%d\n",
				__func__,
				ch,
				dim_get_timerms(0),
				ppre->in_seq,
				plstdvfm->in_inf.vtype_ori,
				plstdvfm->in_inf.w,
				plstdvfm->in_inf.h,
				plstdvfm->in_inf.src_type,
				pdvfm->in_inf.vtype_ori,
				pdvfm->in_inf.w,
				pdvfm->in_inf.h,
				pdvfm->in_inf.src_type);
			#if 0	/*no use*/
			if (di_buf->type & VIDTYPE_COMPRESS) {
				ppre->cur_width =
					di_buf->vframe->compWidth;
				ppre->cur_height =
					di_buf->vframe->compHeight;
			} else {
				ppre->cur_width = di_buf->vframe->width;
				ppre->cur_height = di_buf->vframe->height;
			}
			#else
				ppre->cur_width		= pdvfm->in_inf.w;
				ppre->cur_height	= pdvfm->in_inf.h;
			#endif
			ppre->cur_prog_flag = !pdvfm->wmode.is_i;
				//is_progressive(di_buf->vframe);
			if (ppre->cur_prog_flag) {
				if ((dimp_get(eDI_MP_use_2_interlace_buff)) &&
				    !(cfg_prog_proc & 0x10))
					ppre->prog_proc_type = 2;
					/*now:cfg_prog_proc is 0x23*/
				else
					ppre->prog_proc_type /*p as i is 0*/
						= cfg_prog_proc & 0x10;
			} else {
				ppre->prog_proc_type = 0;
			}
			#if 0
			ppre->cur_inp_type = di_buf->vframe->type;
			ppre->cur_source_type =
				di_buf->vframe->source_type;
			ppre->cur_sig_fmt = di_buf->vframe->sig_fmt;
			ppre->orientation = di_buf->vframe->video_angle;
			#else
			ppre->cur_source_type	= pdvfm->in_inf.src_type;
			ppre->cur_sig_fmt	= pdvfm->in_inf.sig_fmt;
			ppre->orientation	= pdvfm->wmode.is_angle;
			#endif
			ppre->source_change_flag	= 1;
			ppre->input_size_change_flag	= true;

			ppre->field_count = 0;
		/*source change end*/
		/********************************************************/
		} else if (ppre->cur_prog_flag == 0) {
			/* check if top/bot interleaved */
			if (change_type == 2)
				/* source is i interleaves p fields */
				ppre->force_interlace = true;
			if ((ppre->cur_inp_type &
			VIDTYPE_TYPEMASK) == (di_buf->vframe->type &
			VIDTYPE_TYPEMASK)) {
				if ((di_buf->vframe->type &
				VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP)
					same_field_top_count++;
				else
					same_field_bot_count++;
			}
		}
		ppre->cur_inp_type = pdvfm->in_inf.vtype_ori;
		dimv3_set_pre_dfvm(plstdvfm, pdvfm);
		/********************************************************/
		if (!change_type) {
			change_type2 = isv3_vinfo_change(ch);
			if (change_type2) {
				/*ppre->source_change_flag = 1;*/
				ppre->input_size_change_flag = true;
			}
		}

		/********************************************************/
		if (dim_is_bypass(di_buf->vframe, ch)
			/*|| is_bypass_i_p()*/
			/*|| ((ppre->pre_ready_seq % 5)== 0)*/
			/*|| (ppre->pre_ready_seq == 10)*/
			) {
		/********************************************************/
		/*is bypass*/
			/* bypass progressive */
			di_buf->c.seq = ppre->pre_ready_seq++;
			di_buf->c.post_ref_count = 0;
			/*cur_lev = 0;*/
			dimp_set(eDI_MP_cur_lev, 0);
			if (ppre->source_change_flag) {
				di_buf->c.new_format_flag = 1;
				ppre->source_change_flag = 0;
			} else {
				di_buf->c.new_format_flag = 0;
			}

		if (di_bypass_state_get(ch) == 0) {
			if (ppre->di_mem_buf_dup_p) {
				ppre->di_mem_buf_dup_p->c.pre_ref_count = 0;
				ppre->di_mem_buf_dup_p = NULL;
			}
			if (ppre->di_chan2_buf_dup_p) {
				ppre->di_chan2_buf_dup_p->c.pre_ref_count = 0;
				ppre->di_chan2_buf_dup_p = NULL;
			}

				if (ppre->di_wr_buf) {
					ppre->di_wr_buf->c.pre_ref_count = 0;
					ppre->di_wr_buf->c.post_ref_count = 0;
					recycle_vframe_type_pre(
						ppre->di_wr_buf, ch);

					ppre->di_wr_buf = NULL;
				}

			di_buf->c.new_format_flag = 1;
			di_bypass_state_set(ch, true);/*bypass_state:1;*/

			dimv3_print(
				"%s:bypass_state = 1, is_bypass() %d\n",
				__func__, dim_is_bypass(NULL, ch));

			}
			/*di_bypass_state_get(channel) == 0 end*/
			/***************************************/
			//top_bot_config(di_buf);
			//di_buf->vframe.type = pdvfm->in_inf.vtype;

			div3_que_in(ch, QUE_PRE_READY, di_buf);


			if ((bypass_pre & 0x2) && !ppre->cur_prog_flag)
				di_buf->c.post_proc_flag = -2;
			else
				di_buf->c.post_proc_flag = 0;

			dimv3_print("di:cfg:post_proc_flag=%d\n",
				  di_buf->c.post_proc_flag);
#ifdef DI_BUFFER_DEBUG
			dimv3_print(
				"%s: %s[%d] => pre_ready_list\n", __func__,
				vframe_type_name[di_buf->type], di_buf->index);
#endif
			qued_ops.in(pch, QUED_T_RECYCL, pdvfm->index);
			dimv3_print("%s is bypass\n", __func__);
			return 0;
		/*is bypass end*/
		/********************************************************/
		/*is proc*/
		} else if (is_progressive(di_buf->vframe)) {
			if (is_handle_prog_frame_as_interlace(vframe) &&
			    is_progressive(vframe)) {
			    /*proc p as i*/
				struct di_buf_s *di_buf_tmp = NULL;

				pvframe_in[di_buf->index] = NULL;
				di_buf->vframe->type &=
					(~VIDTYPE_TYPEMASK);
				di_buf->vframe->type |=
					VIDTYPE_INTERLACE_TOP;
				di_buf->c.post_proc_flag = 0;

		di_buf_tmp = div3_que_out_to_di_buf(ch, QUE_IN_FREE);
		if (dimv3_check_di_buf(di_buf_tmp, 10, ch)) {
			recycle_vframe_type_pre(di_buf, ch);
			PR_ERR("%s:DI:no IN_FREE for prog skip.\n", __func__);
			return 0;
		}

				di_buf_tmp->vframe->private_data = di_buf_tmp;
				di_buf_tmp->c.seq = ppre->in_seq;
				ppre->in_seq++;
				pvframe_in[di_buf_tmp->index] = vframe;
				memcpy(di_buf_tmp->vframe, vframe,
				       sizeof(vframe_t));
				ppre->di_inp_buf_next = di_buf_tmp;
				di_buf_tmp->c.wmode = di_buf->c.wmode;
				di_buf_tmp->vframe->type &=
					(~VIDTYPE_TYPEMASK);
				di_buf_tmp->vframe->type |=
					VIDTYPE_INTERLACE_BOTTOM;
				di_buf_tmp->c.post_proc_flag = 0;

				ppre->di_inp_buf = di_buf;
#ifdef DI_BUFFER_DEBUG
				dimv3_print(
			"%s: %s[%d] => di_inp_buf; %s[%d] => di_inp_buf_next\n",
					__func__,
					vframe_type_name[di_buf->type],
					di_buf->index,
					vframe_type_name[di_buf_tmp->type],
					di_buf_tmp->index);
#endif
				if (!ppre->di_mem_buf_dup_p) {
					ppre->di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
					dimv3_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
						__func__);
#endif
				}
			/*proc p as i end */
			/*******************/
			} else {/*ary: not process prog as i*/
				di_buf->c.post_proc_flag = 0;
				if ((cfg_prog_proc & 0x40) ||
				    ppre->force_interlace)
					di_buf->c.post_proc_flag = 1;

				ppre->di_inp_buf = di_buf;
#ifdef DI_BUFFER_DEBUG
				dimv3_print(
					"%s: %s[%d] => di_inp_buf\n",
					__func__,
					vframe_type_name[di_buf->type],
					di_buf->index);
#endif
				if (!ppre->di_mem_buf_dup_p) {
					/* use n */
					ppre->di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
					dimv3_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
						__func__);
#endif
				}
			}
		/*is proc end*/
		/********************************************************/
		} else {
		/********************************************************/
		#if 0
		/* is other : not bypass, not proce*/
		if ((di_buf->vframe->width >= 1920)	&&
		    (di_buf->vframe->height >= 1080)	&&
		    is_meson_tl1_cpu()) {
			/*if (combing_fix_en) {*/
			if (dimp_get(eDI_MP_combing_fix_en)) {
				/*combing_fix_en = false;*/
				dimp_set(eDI_MP_combing_fix_en, 0);
				get_ops_mtn()->fix_tl1_1080i_sawtooth_patch();
			}
		} else {
			/*combing_fix_en = true;*/
			dimp_set(eDI_MP_combing_fix_en, 1);
		}
		#endif
		/*********************************/
			if (!ppre->di_chan2_buf_dup_p) {
				ppre->field_count = 0;
				/* ignore contp2rd and contprd */
			}
			di_buf->c.post_proc_flag = 1;
			ppre->di_inp_buf = di_buf;
			dimv3_print("%s: %s[%d] => di_inp_buf\n", __func__,
				  vframe_type_name[di_buf->type],
				  di_buf->index);

			if (!ppre->di_mem_buf_dup_p) {/* use n */
				ppre->di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
				dimv3_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
					__func__);
#endif
			}
		}
		/*other end*/
		/********************************************************/
/*main proc end*/
/*****************************************************************************/
	}
	/*dim_dbg_pre_cnt(channel, "cfg");*/
	/* di_wr_buf */
	if (ppre->prog_proc_type == 2) {
		di_linked_buf_idx = peek_free_linked_buf(ch);
		if (di_linked_buf_idx != -1)
			di_buf = get_free_linked_buf(di_linked_buf_idx,
						     ch);
		else
			di_buf = NULL;
		if (!di_buf) {
			/* recycle_vframe_type_pre(di_pre_stru.di_inp_buf);
			 *save for next process
			 */
			recycle_keep_buffer(ch);
			ppre->di_inp_buf_next = ppre->di_inp_buf;
			PR_ERR("%s no link local buf\n", __func__);
			return 0;
		}
		di_buf->c.post_proc_flag = 0;
		di_buf->c.di_wr_linked_buf->c.pre_ref_count = 0;
		di_buf->c.di_wr_linked_buf->c.post_ref_count = 0;
		di_buf->c.canvas_config_flag = 1;
	} else {
		/*di_buf[local]*/
		di_buf = getv3_di_buf_head(ch, QUEUE_LOCAL_FREE);
		if (dimv3_check_di_buf(di_buf, 11, ch)) {
			/* recycle_keep_buffer();
			 *  pr_dbg("%s:recycle keep buffer\n", __func__);
			 */
			recycle_vframe_type_pre(ppre->di_inp_buf, ch);
			PR_ERR("%s no local free\n", __func__);
			return 0;
		}
		queuev3_out(ch, di_buf);/*QUEUE_LOCAL_FREE*/
		if (ppre->prog_proc_type & 0x10)
			di_buf->c.canvas_config_flag = 1;
		else
			di_buf->c.canvas_config_flag = 2;
		di_buf->c.di_wr_linked_buf = NULL;
		di_buf->c.sts |= EDI_ST_PRE;
	}

	ppre->di_wr_buf = di_buf;
	ppre->di_wr_buf->c.pre_ref_count = 1;

#ifdef DI_BUFFER_DEBUG
	dimv3_print("%s: %s[%d] => di_wr_buf\n", __func__,
		  vframe_type_name[di_buf->type], di_buf->index);
	if (di_buf->c.di_wr_linked_buf)
		dimv3_print("%s: linked %s[%d] => di_wr_buf\n", __func__,
			  vframe_type_name[di_buf->c.di_wr_linked_buf->type],
			  di_buf->c.di_wr_linked_buf->index);
#endif
	if (ppre->cur_inp_type & VIDTYPE_COMPRESS) {
		ppre->di_inp_buf->vframe->width =
			ppre->di_inp_buf->vframe->compWidth;
		ppre->di_inp_buf->vframe->height =
			ppre->di_inp_buf->vframe->compHeight;
	}
	/*di_buf[local]*/
	if (!di_buf)
		PR_ERR("%s:err1\n", __func__);
	if (!di_buf->vframe)
		PR_ERR("%s:err2\n", __func__);
	if (!ppre->di_inp_buf)
		PR_ERR("%s:err3\n", __func__);
	if (!ppre->di_inp_buf->vframe)
		PR_ERR("%s:err4\n", __func__);
	memcpy(di_buf->vframe,
	       ppre->di_inp_buf->vframe, sizeof(vframe_t));
	di_buf->vframe->private_data = di_buf;
	di_buf->c.wmode = ppre->di_inp_buf->c.wmode;

	#if 0	/*no use*/
	di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
	di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
	#endif
	/* set vframe bit info */
	di_buf->vframe->bitdepth &= ~(BITDEPTH_YMASK);
	di_buf->vframe->bitdepth &= ~(FULL_PACK_422_MODE);

	/*pp scaler*/
	if (de_devp->pps_enable && dimp_get(eDI_MP_pps_position)) {
		if (dimp_get(eDI_MP_pps_dstw) != di_buf->vframe->width) {
			di_buf->vframe->width = dimp_get(eDI_MP_pps_dstw);
			ppre->width_bk = dimp_get(eDI_MP_pps_dstw);
		}
		if (dimp_get(eDI_MP_pps_dsth) != di_buf->vframe->height)
			di_buf->vframe->height = dimp_get(eDI_MP_pps_dsth);
	} else if (de_devp->h_sc_down_en) {
		if (dimp_get(eDI_MP_pre_hsc_down_width)
			!= di_buf->vframe->width) {
			PR_INF("hscd %d to %d\n", di_buf->vframe->width,
			       dimp_get(eDI_MP_pre_hsc_down_width));
			di_buf->vframe->width =
				dimp_get(eDI_MP_pre_hsc_down_width);
			/*di_pre_stru.width_bk = pre_hsc_down_width;*/
			di_buf->c.width_bk =
				dimp_get(eDI_MP_pre_hsc_down_width);
		}
	}
	/**/
	if (dimp_get(eDI_MP_di_force_bit_mode) == 10) {
		di_buf->vframe->bitdepth |= (BITDEPTH_Y10);
		if (dimp_get(eDI_MP_full_422_pack))
			di_buf->vframe->bitdepth |= (FULL_PACK_422_MODE);
	} else {
		di_buf->vframe->bitdepth |= (BITDEPTH_Y8);
	}

	di_buf->c.width_bk = ppre->width_bk;	/*ary:2019-04-23*/

	if (ppre->prog_proc_type) {
		di_buf->vframe->type = VIDTYPE_PROGRESSIVE |
				       VIDTYPE_VIU_422 |
				       VIDTYPE_VIU_SINGLE_PLANE |
				       VIDTYPE_VIU_FIELD;
		if (ppre->cur_inp_type & VIDTYPE_PRE_INTERLACE)
			di_buf->vframe->type |= VIDTYPE_PRE_INTERLACE;
	} else {
		/*i mode: or pasi*/
		if (
			((ppre->di_inp_buf->vframe->type &
			  VIDTYPE_TYPEMASK) ==
			 VIDTYPE_INTERLACE_TOP))
			di_buf->vframe->type = VIDTYPE_INTERLACE_TOP |
					       VIDTYPE_VIU_422 |
					       VIDTYPE_VIU_SINGLE_PLANE |
					       VIDTYPE_VIU_FIELD;
		else
			di_buf->vframe->type = VIDTYPE_INTERLACE_BOTTOM |
					       VIDTYPE_VIU_422 |
					       VIDTYPE_VIU_SINGLE_PLANE |
					       VIDTYPE_VIU_FIELD;
		/*add for vpp skip line ref*/
		if (di_bypass_state_get(ch) == 0)
			di_buf->vframe->type |= VIDTYPE_PRE_INTERLACE;
	}

	if (is_bypass_post(ch)) {
		if (dimp_get(eDI_MP_bypass_post_state) == 0)
			ppre->source_change_flag = 1;

		dimp_set(eDI_MP_bypass_post_state, 1);
	} else {
		if (dimp_get(eDI_MP_bypass_post_state))
			ppre->source_change_flag = 1;

		dimp_set(eDI_MP_bypass_post_state, 0);
	}

	if (ppre->di_inp_buf->c.post_proc_flag == 0) {
		ppre->madi_enable = 0;
		ppre->mcdi_enable = 0;
		di_buf->c.post_proc_flag = 0;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else if (dimp_get(eDI_MP_bypass_post_state)) {
		ppre->madi_enable = 0;
		ppre->mcdi_enable = 0;
		di_buf->c.post_proc_flag = 0;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else {
		ppre->madi_enable = (dimp_get(eDI_MP_pre_enable_mask) & 1);
		ppre->mcdi_enable =
			((dimp_get(eDI_MP_pre_enable_mask) >> 1) & 1);
		di_buf->c.post_proc_flag = 1;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER,
					     dimp_get(eDI_MP_mcpre_en));/*en*/
	}

	if ((ppre->di_mem_buf_dup_p == ppre->di_wr_buf) ||
	    (ppre->di_chan2_buf_dup_p == ppre->di_wr_buf)) {
		PR_ERR("%s+++++++++++++++++++++++\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 12;

		recovery_flag++;
		return 0;
	}

	/**************************************/
	if (pdvfm) {
		pch = get_chdata(ch);
		ppre->dvfm = pdvfm;
		pdvfm->etype = EDIM_DISP_T_PRE;
		qued_ops.in(pch, QUED_T_PRE, pdvfm->index);
		dimv3_print("%s:qued in\n", __func__);
	} else {
		PR_ERR("%s:no pdvfm\n", __func__);
	}
	#endif
	return 1;
}

unsigned char dim_pre_de_buf_config_top(unsigned int ch)
{
	struct di_ch_s		*pch;
	struct dim_dvfm_s	*pdvfm = NULL;
	unsigned char ret;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	bool flg_1080i = false;
	bool flg_480i = false;

	if (di_blocking || !dipv3_cma_st_is_ready(ch))
		return 0;

	if (div3_que_list_count(ch, QUE_IN_FREE) < 1)
		return 0;

	if ((queuev3_empty(ch, QUEUE_LOCAL_FREE)))
		return 0;

	if (div3_que_list_count(ch, QUE_PRE_READY) >= DI_PRE_READY_LIMIT)
		return 0;

	pch = get_chdata(ch);
	pdvfm = dvfmv3_peek(pch, QUED_T_IN);
	if (!pdvfm)
		return 0;
	if (pdvfm->wmode.p_use_2i)
		ret = dim_pre_de_buf_config_p_use_2ibuf(ch);
	else if (pdvfm->wmode.is_i)
		ret = dim_pre_de_buf_config_i(ch);
	else
		ret = dimv3_pre_de_buf_config(ch);

	/*mtn*/
	if (pdvfm->wmode.is_i) {
		if ((pdvfm->in_inf.w >= 1920) &&
		    (pdvfm->in_inf.h >= 1080))
			flg_1080i = true;
		else if ((pdvfm->in_inf.w == 720) &&
			 (pdvfm->in_inf.h == 480))
			flg_480i = true;
	}
	if (is_meson_tl1_cpu()			&&
	    //ppre->comb_mode			&&
	    flg_1080i) {
		ppre->combing_fix_en = false;
		get_ops_mtn()->fix_tl1_1080i_patch_sel(ppre->comb_mode);
	} else {
		ppre->combing_fix_en = di_mpr(combing_fix_en);
	}

	if (ppre->combing_fix_en) {
		if (flg_1080i)
			get_ops_mtn()->com_patch_pre_sw_set(1);
		else if (flg_480i)
			get_ops_mtn()->com_patch_pre_sw_set(2);
		else
			get_ops_mtn()->com_patch_pre_sw_set(0);
	}

	return ret;
}
/******************************************
 * clear buf
 ******************************************/
void dimv3_buf_clean(struct di_buf_s *di_buf)
{
//	int i;
	if (!di_buf)
		return;
	memset(&di_buf->c, 0, sizeof(struct di_buf_c_s));
}

int dimv3_check_recycle_buf(unsigned int ch)
{
	struct di_buf_s *di_buf = NULL;/* , *ptmp; */
	struct di_buf_s *di_buf_link = NULL;
	int itmp;
	int ret = 0;
	void **pvframe_in = getv3_vframe_in(ch);
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	struct di_ch_s *pch;
	struct dim_itf_ops_s	*pvfmops;

	if (di_blocking)
		return ret;
	pch = get_chdata(ch);
	pvfmops = &pch->interf.opsi;
	queue_for_each_entry(di_buf, ch, QUEUE_RECYCLE, list) {
		if (di_buf->c.pre_ref_count ||
		    di_buf->c.post_ref_count > 0)
			continue;

		/*ary maybe <= */
		if (di_buf->type == VFRAME_TYPE_IN) {
			queuev3_out(ch, di_buf);
			if (pvframe_in[di_buf->index]) {
				//pw_vf_put(pvframe_in[di_buf->index], ch);
				//pw_vf_notify_provider(ch,
				//	VFRAME_EVENT_RECEIVER_PUT,
				//	NULL);
				pvfmops->put(pvframe_in[di_buf->index], pch);

		dimv3_print("%s\n", __func__);
		dimv3_print("ch[%d],vf_put(%d) %p, %u ms\n",
			ch,
			ppre->recycle_seq,
			pvframe_in[di_buf->index],
			dim_get_timerms(0));
				pvframe_in[di_buf->index] = NULL;
			}
			//di_buf->c.invert_top_bot_flag = 0;
			dimv3_buf_clean(di_buf);

			div3_que_in(ch, QUE_IN_FREE, di_buf);
			ppre->recycle_seq++;
			ret |= 1;
		} else {
			queuev3_out(ch, di_buf);
			di_buf->c.invert_top_bot_flag = 0;

			queuev3_in(ch, di_buf, QUEUE_LOCAL_FREE);
			di_buf->c.post_ref_count = 0;/*ary maybe*/
			if (di_buf->c.di_wr_linked_buf) {
				di_buf_link = di_buf->c.di_wr_linked_buf;
				queuev3_in(ch, di_buf_link, QUEUE_LOCAL_FREE);

				dimv3_print(
				"%s: linked %s[%d]=>recycle_list\n",
					__func__,
					vframe_type_name[di_buf_link->type],
					di_buf_link->index
				);

				di_buf->c.di_wr_linked_buf = NULL;
				dimv3_buf_clean(di_buf_link);
			}
			dimv3_buf_clean(di_buf);
			ret |= 2;
		}
		dimv3_print("%s: recycle %s[%d]\n", __func__,
			  vframe_type_name[di_buf->type],
			  di_buf->index);
	}

	return ret;
}

#ifdef DET3D
static void set3d_view(enum tvin_trans_fmt trans_fmt, struct vframe_s *vf)
{
	struct vframe_view_s *left_eye, *right_eye;

	left_eye = &vf->left_eye;
	right_eye = &vf->right_eye;

	switch (trans_fmt) {
	case TVIN_TFMT_3D_DET_LR:
	case TVIN_TFMT_3D_LRH_OLOR:
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = vf->width >> 1;
		left_eye->height = vf->height;
		right_eye->start_x = vf->width >> 1;
		right_eye->start_y = 0;
		right_eye->width = vf->width >> 1;
		right_eye->height = vf->height;
		break;
	case TVIN_TFMT_3D_DET_TB:
	case TVIN_TFMT_3D_TB:
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = vf->width;
		left_eye->height = vf->height >> 1;
		right_eye->start_x = 0;
		right_eye->start_y = vf->height >> 1;
		right_eye->width = vf->width;
		right_eye->height = vf->height >> 1;
		break;
	case TVIN_TFMT_3D_DET_INTERLACE:
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = vf->width;
		left_eye->height = vf->height >> 1;
		right_eye->start_x = 0;
		right_eye->start_y = 0;
		right_eye->width = vf->width;
		right_eye->height = vf->height >> 1;
		break;
	case TVIN_TFMT_3D_DET_CHESSBOARD:
/***
 * LRLRLR	  LRLRLR
 * LRLRLR  or RLRLRL
 * LRLRLR	  LRLRLR
 * LRLRLR	  RLRLRL
 */
		break;
	default: /* 2D */
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = 0;
		left_eye->height = 0;
		right_eye->start_x = 0;
		right_eye->start_y = 0;
		right_eye->width = 0;
		right_eye->height = 0;
		break;
	}
}

/*
 * static int get_3d_info(struct vframe_s *vf)
 * {
 * int ret = 0;
 *
 * vf->trans_fmt = det3d_fmt_detect();
 * pr_dbg("[det3d..]new 3d fmt: %d\n", vf->trans_fmt);
 *
 * vdin_set_view(vf->trans_fmt, vf);
 *
 * return ret;
 * }
 */
static unsigned int det3d_frame_cnt = 50;
module_param_named(det3d_frame_cnt, det3d_frame_cnt, uint, 0644);
static void det3d_irq(unsigned int channel)
{
	unsigned int data32 = 0, likely_val = 0;
	unsigned long frame_sum = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (!dimp_get(eDI_MP_det3d_en))
		return;

	data32 = get_ops_3d()->det3d_fmt_detect();/*det3d_fmt_detect();*/
	switch (data32) {
	case TVIN_TFMT_3D_DET_LR:
	case TVIN_TFMT_3D_LRH_OLOR:
		ppre->det_lr++;
		break;
	case TVIN_TFMT_3D_DET_TB:
	case TVIN_TFMT_3D_TB:
		ppre->det_tp++;
		break;
	case TVIN_TFMT_3D_DET_INTERLACE:
		ppre->det_la++;
		break;
	default:
		ppre->det_null++;
		break;
	}

	if (det3d_mode != data32) {
		det3d_mode = data32;
		dimv3_print("[det3d..]new 3d fmt: %d\n", det3d_mode);
	}
	if (frame_count > 20) {
		frame_sum = ppre->det_lr + ppre->det_tp
					+ ppre->det_la
					+ ppre->det_null;
		if ((frame_count % det3d_frame_cnt) || (frame_sum > UINT_MAX))
			return;
		likely_val = max3(ppre->det_lr,
				  ppre->det_tp,
				  ppre->det_la);
		if (ppre->det_null >= likely_val)
			det3d_mode = 0;
		else if (likely_val == ppre->det_lr)
			det3d_mode = TVIN_TFMT_3D_LRH_OLOR;
		else if (likely_val == ppre->det_tp)
			det3d_mode = TVIN_TFMT_3D_TB;
		else
			det3d_mode = TVIN_TFMT_3D_DET_INTERLACE;
		ppre->det3d_trans_fmt = det3d_mode;
	} else {
		ppre->det3d_trans_fmt = 0;
	}
}
#endif

static unsigned int ro_mcdi_col_cfd[26];
static void get_mcinfo_from_reg_in_irq(unsigned int channel)
{
	unsigned int i = 0, ncolcrefsum = 0, blkcount = 0, *reg = NULL;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct mcinfo_pre_s	*pmcinfo;

	pmcinfo = &ppre->di_wr_buf->c.curr_field_mcinfo;
/*get info for current field process by post*/
	pmcinfo->highvertfrqflg =
		(Rd(MCDI_RO_HIGH_VERT_FRQ_FLG) & 0x1);
/* post:MCDI_MC_REL_GAIN_OFFST_0 */
	pmcinfo->motionparadoxflg =
		(Rd(MCDI_RO_MOTION_PARADOX_FLG) & 0x1);
/* post:MCDI_MC_REL_GAIN_OFFST_0 */
	reg = pmcinfo->regs;
	for (i = 0; i < 26; i++) {
		ro_mcdi_col_cfd[i] = Rd(0x2fb0 + i);
		pmcinfo->regs[i] = 0;
		if (!dimp_get(eDI_MP_calc_mcinfo_en))
			*(reg + i) = ro_mcdi_col_cfd[i];
	}
	if (dimp_get(eDI_MP_calc_mcinfo_en)) {
		blkcount = (ppre->cur_width + 4) / 5;
		for (i = 0; i < blkcount; i++) {
			ncolcrefsum +=
				((ro_mcdi_col_cfd[i / 32] >> (i % 32)) & 0x1);
			if (
				((ncolcrefsum + (blkcount >> 1)) << 8) /
				blkcount > dimp_get(eDI_MP_colcfd_thr))
				for (i = 0; i < blkcount; i++)
					*(reg + i / 32) += (1 << (i % 32));
		}
	}
}

static unsigned int bit_reverse(unsigned int val)
{
	unsigned int i = 0, res = 0;

	for (i = 0; i < 16; i++) {
		res |= (((val & (1 << i)) >> i) << (31 - i));
		res |= (((val & (1 << (31 - i))) << i) >> (31 - i));
	}
	return res;
}

static void set_post_mcinfo(struct mcinfo_pre_s *curr_field_mcinfo)
{
	unsigned int i = 0, value = 0;

	dimv3_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_REL_GAIN_OFFST_0,
				   curr_field_mcinfo->highvertfrqflg, 24, 1);
	dimv3_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_REL_GAIN_OFFST_0,
				   curr_field_mcinfo->motionparadoxflg, 25, 1);
	for (i = 0; i < 26; i++) {
		if (overturn)
			value = bit_reverse(curr_field_mcinfo->regs[i]);
		else
			value = curr_field_mcinfo->regs[i];
		dimv3_VSYNC_WR_MPEG_REG(0x2f78 + i, value);
	}
}

static unsigned char intr_mode;

irqreturn_t dimv3_irq(int irq, void *dev_instance)
{
	unsigned int channel;
	struct di_pre_stru_s *ppre;
	struct di_dev_s *de_devp = getv3_dim_de_devp();
	struct di_hpre_s  *pre = get_hw_pre();

#ifndef CHECK_DI_DONE
	unsigned int data32;
	unsigned int mask32;
	unsigned int flag = 0;


	data32 = Rd(DI_INTR_CTRL);
	mask32 = (data32 >> 16) & 0x3ff;

	channel = pre->curr_ch;
	ppre = pre->pres;

	data32 &= 0x3fffffff;
	if ((data32 & 1) == 0 && dimp_get(eDI_MP_di_dbg_mask) & 8)
		PR_INF("irq[%d]pre|post=0 write done.\n", irq);
	if (ppre->pre_de_busy) {
		/* only one inetrrupr mask should be enable */
		if ((data32 & 2) && !(mask32 & 2)) {
			dimv3_print("irq pre MTNWR ==ch[%d]\n", channel);
			flag = 1;
		} else if ((data32 & 1) && !(mask32 & 1)) {
			dimv3_print("irq pre NRWR ==ch[%d]\n", channel);
			flag = 1;
		} else {
			dimv3_print("irq pre DI IRQ 0x%x ==\n", data32);
			flag = 0;
		}
	}

#else
	channel = pre->curr_ch;
	ppre = pre->pres;
#endif

#ifdef DET3D
	if (dimp_get(eDI_MP_det3d_en)) {
		if ((data32 & 0x100) && !(mask32 & 0x100) && flag) {
			dimv3_DI_Wr(DI_INTR_CTRL, data32);
			det3d_irq(channel);
		} else {
			goto end;
		}
	} else {
		dimv3_DI_Wr(DI_INTR_CTRL, data32);
	}
#else
	if (flag) {
		hprev3_gl_sw(false);
		dimv3_DI_Wr(DI_INTR_CTRL,
			  (data32 & 0xfffffffb) | (intr_mode << 30));
	}
#endif

	/*if (ppre->pre_de_busy == 0) {*/
	if (!di_pre_wait_irq_get()) {
		PR_ERR("%s:ch[%d]:enter:reg[0x%x]= 0x%x,dtab[%d]\n", __func__,
		       channel,
		       DI_INTR_CTRL,
		       Rd(DI_INTR_CTRL),
		       pre->sdt_mode.op_crr);
		return IRQ_HANDLED;
	}

	if (flag) {
		ppre->irq_time[0] =
			(curv3_to_msecs() - ppre->irq_time[0]);

		dimv3_tr_ops.pre(ppre->field_count, ppre->irq_time[0]);
		dimv3_tr_ops.pre_ready(ppre->di_wr_buf->c.vmode.omx_index);

		/*add from valsi wang.feng*/
		dimv3_arb_sw(false);
		dimv3_arb_sw(true);
		if (dimp_get(eDI_MP_mcpre_en)) {
			get_mcinfo_from_reg_in_irq(channel);
			if ((is_meson_gxlx_cpu()		&&
			     ppre->field_count >= 4)	||
			    is_meson_txhd_cpu())
				dimhv3_mc_pre_mv_irq();
			dimhv3_calc_lmv_base_mcinfo((ppre->cur_height >> 1),
						  ppre->di_wr_buf->mcinfo_adr_v,
						  /*ppre->mcinfo_size*/0);
		}
		get_ops_nr()->nr_process_in_irq();
		if ((data32 & 0x200) && de_devp->nrds_enable)
			dimv3_nr_ds_irq();
		/* disable mif */
		dimhv3_enable_di_pre_mif(false, dimp_get(eDI_MP_mcpre_en));

		ppre->pre_de_busy = 0;

		if (get_init_flag(channel))
			/* pr_dbg("%s:up di sema\n", __func__); */
			taskv3_send_ready();

		pre->flg_int_done = 1;
	}

	return IRQ_HANDLED;
}

irqreturn_t dimv3_post_irq(int irq, void *dev_instance)
{
	unsigned int data32 = Rd(DI_INTR_CTRL);
	unsigned int channel;
	struct di_post_stru_s *ppost;
	struct di_hpst_s  *pst = get_hw_pst();

	channel = pst->curr_ch;
	ppost = pst->psts;

	data32 &= 0x3fffffff;
	if ((data32 & 4) == 0) {
		if (dimp_get(eDI_MP_di_dbg_mask) & 8)
		PR_INF("irq[%d]post write undone.\n", irq);
			return IRQ_HANDLED;
	}

	if (pst->state != eDI_PST_ST_WAIT_INT &&
	    pst->state != eDI_PST_ST_SET) {
		PR_ERR("%s:ch[%d]:s[%d]\n", __func__, channel, pst->state);
		ddbgv3_sw(eDI_LOG_TYPE_MOD, false);
		return IRQ_HANDLED;
	}
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_IRQB, pst->curr_ch,
			  ppost->frame_cnt);
	dimv3_tr_ops.post_ir(0);

	if ((dimp_get(eDI_MP_post_wr_en)	&&
	     dimp_get(eDI_MP_post_wr_support))	&&
	    (data32 & 0x4)) {
		ppost->de_post_process_done = 1;
		ppost->post_de_busy = 0;
		ppost->irq_time =
			(curv3_to_msecs() - ppost->irq_time);

		dimv3_tr_ops.post(ppost->post_wr_cnt, ppost->irq_time);

		dimv3_DI_Wr(DI_INTR_CTRL,
			    (data32 & 0xffff0004) | (intr_mode << 30));
		/* disable wr back avoid pps sreay in g12a */
		dimv3_DI_Wr_reg_bits(DI_POST_CTRL, 0, 7, 1);
		div3_post_set_flow(1, eDI_POST_FLOW_STEP1_STOP); /*dbg a*/
		dimv3_print("irq p ch[%d]done\n", channel);
		pst->flg_int_done = true;
	}
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_IRQE, pst->curr_ch,
			  ppost->frame_cnt);

	if (get_init_flag(channel))
		taskv3_send_ready();

	return IRQ_HANDLED;
}

/*
 * di post process
 */
static void inc_post_ref_count(struct di_buf_s *di_buf)
{
	int i;	/*debug only:*/
	struct di_buf_s *pbuf_dup;/*debug only:*/

	/* int post_blend_mode; */
	if (IS_ERR_OR_NULL(di_buf)) {
		PR_ERR("%s:\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 13;

		recovery_flag++;
		return;
	}

	if (di_buf->c.di_buf_dup_p[1])
		di_buf->c.di_buf_dup_p[1]->c.post_ref_count++;

	if (di_buf->c.di_buf_dup_p[0])
		di_buf->c.di_buf_dup_p[0]->c.post_ref_count++;

	if (di_buf->c.di_buf_dup_p[2])
		di_buf->c.di_buf_dup_p[2]->c.post_ref_count++;

	/*debug only:*/
	for (i = 0; i < 3; i++) {
		pbuf_dup = di_buf->c.di_buf_dup_p[i];
		if (pbuf_dup)
			dbg_post_ref("%s:pst_buf[%d],dup_p[%d],pref[%d]\n",
				     __func__,
				     di_buf->index,
				     i,
				     pbuf_dup->c.post_ref_count);
	}
}

static void dec_post_ref_count(struct di_buf_s *di_buf)
{
	int i;	/*debug only:*/
	struct di_buf_s *pbuf_dup;/*debug only:*/

	if (IS_ERR_OR_NULL(di_buf)) {
		PR_ERR("%s:\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 14;

		recovery_flag++;
		return;
	}

	if (di_buf->c.pd_config.global_mode == PULL_DOWN_BUF1)
		return;
	if (di_buf->c.di_buf_dup_p[1])
		di_buf->c.di_buf_dup_p[1]->c.post_ref_count--;

	if (di_buf->c.di_buf_dup_p[0] &&
	    di_buf->c.di_buf_dup_p[0]->c.post_proc_flag != -2)
		di_buf->c.di_buf_dup_p[0]->c.post_ref_count--;

	if (di_buf->c.di_buf_dup_p[2])
		di_buf->c.di_buf_dup_p[2]->c.post_ref_count--;

	/*debug only:*/
	for (i = 0; i < 3; i++) {
		pbuf_dup = di_buf->c.di_buf_dup_p[i];
		if (pbuf_dup)
			dbg_post_ref("%s:pst_buf[%d],dup_p[%d],pref[%d]\n",
			     __func__,
			     di_buf->index,
			     i,
			     pbuf_dup->c.post_ref_count);
	}
}

#if 0	/*no use*/
static void vscale_skip_disable_post(struct di_buf_s *di_buf,
				     vframe_t *disp_vf, unsigned int channel)
{
	struct di_buf_s *di_buf_i = NULL;
	int canvas_height = di_buf->di_buf[0]->canvas_height;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	if (di_vscale_skip_enable & 0x2) {/* drop the bottom field */
		if ((di_buf->di_buf_dup_p[0]) && (di_buf->di_buf_dup_p[1]))
			di_buf_i =
				(di_buf->di_buf_dup_p[1]->vframe->type &
				 VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP ? di_buf->di_buf_dup_p[1]
				: di_buf->di_buf_dup_p[0];
		else
			di_buf_i = di_buf->di_buf[0];
	} else {
		if ((di_buf->di_buf[0]->post_proc_flag > 0) &&
		    (di_buf->di_buf_dup_p[1]))
			di_buf_i = di_buf->di_buf_dup_p[1];
		else
			di_buf_i = di_buf->di_buf[0];
	}
	disp_vf->type = di_buf_i->vframe->type;
	/* pr_dbg("%s (%x %x) (%x %x)\n", __func__,
	 * disp_vf, disp_vf->type, di_buf_i->vframe,
	 * di_buf_i->vframe->type);
	 */
	disp_vf->width = di_buf_i->vframe->width;
	disp_vf->height = di_buf_i->vframe->height;
	disp_vf->duration = di_buf_i->vframe->duration;
	disp_vf->pts = di_buf_i->vframe->pts;
	disp_vf->flag = di_buf_i->vframe->flag;
	disp_vf->canvas0Addr = di_post_idx[ppost->canvas_id][0];
	disp_vf->canvas1Addr = di_post_idx[ppost->canvas_id][0];
	canvas_config(
		di_post_idx[ppost->canvas_id][0],
		di_buf_i->nr_adr, di_buf_i->canvas_width[NR_CANVAS],
		canvas_height, 0, 0);
	dimhv3_disable_post_deinterlace_2();
	ppost->vscale_skip_flag = true;
}
#endif

/*early_process_fun*/
static int early_NONE(void)
{
	return 0;
}

int dimv3_do_post_wr_fun(void *arg, vframe_t *disp_vf)
{
	#if 0
	struct di_post_stru_s *ppost;
	int i;

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		ppost = get_post_stru(i);

		ppost->toggle_flag = true;
	}
	return 1;
	#else
	return early_NONE();
	#endif
}
#if 0
static int de_post_disable_fun(void *arg, vframe_t *disp_vf)
{
	#if 0
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;
	unsigned int channel = di_buf->channel;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	ppost->vscale_skip_flag = false;
	ppost->toggle_flag = true;

	PR_ERR("%s------------------------------\n", __func__);

	process_vscale_skip(di_buf, disp_vf, channel);
/* for atv static image flickering */
	if (di_buf->c.process_fun_index == PROCESS_FUN_NULL)
		dimhv3_disable_post_deinterlace_2();

	return 1;
/* called for new_format_flag, make
 * video set video_property_changed
 */
	#else
	return early_NONE();
	#endif
}
#endif
#if 0
static int do_nothing_fun(void *arg, vframe_t *disp_vf)
{
	#if 0
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;
	unsigned int channel = di_buf->channel;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	ppost->vscale_skip_flag = false;
	ppost->toggle_flag = true;

	PR_ERR("%s------------------------------\n", __func__);

	process_vscale_skip(di_buf, disp_vf, channel);

	if (di_buf->c.process_fun_index == PROCESS_FUN_NULL) {
		if (Rd(DI_IF1_GEN_REG) & 0x1 || Rd(DI_POST_CTRL) & 0xf)
			dimhv3_disable_post_deinterlace_2();
	/*if(di_buf->pulldown_mode == PULL_DOWN_EI && Rd(DI_IF1_GEN_REG)&0x1)
	 * dim_VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG, 0x3 << 30);
	 */
	}
	return 0;
	#else
	return early_NONE();
	#endif
}
#endif
//static /*ary no use*/
int do_pre_only_fun(void *arg, vframe_t *disp_vf)
{
	#if 0
	unsigned int channel;
	struct di_post_stru_s *ppost;

	PR_ERR("%s------------------------------\n", __func__);
#ifdef DI_USE_FIXED_CANVAS_IDX
	if (arg) {
		struct di_buf_s *di_buf = (struct di_buf_s *)arg;
		vframe_t *vf = di_buf->vframe;
		int width, canvas_height;

		channel = di_buf->channel;
		ppost = get_post_stru(channel);

		ppost->vscale_skip_flag = false;
		ppost->toggle_flag = true;

		if (!vf || !di_buf->di_buf[0]) {
			dimv3_print("error:%s,NULL point!!\n", __func__);
			return 0;
		}
		width = di_buf->di_buf[0]->canvas_width[NR_CANVAS];
		/* linked two interlace buffer should double height*/
		if (di_buf->di_buf[0]->di_wr_linked_buf)
			canvas_height =
			(di_buf->di_buf[0]->canvas_height << 1);
		else
			canvas_height =
			di_buf->di_buf[0]->canvas_height;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		if (is_vsync_rdma_enable()) {
			ppost->canvas_id = ppost->next_canvas_id;
		} else {
			ppost->canvas_id = 0;
			ppost->next_canvas_id = 1;
			if (post_wr_en && post_wr_support)
				ppost->canvas_id = ppost->next_canvas_id;
		}
#endif

		canvas_config(
			di_post_idx[ppost->canvas_id][0],
			di_buf->di_buf[0]->nr_adr,
			di_buf->di_buf[0]->canvas_width[NR_CANVAS],
			canvas_height, 0, 0);

		vf->canvas0Addr =
			di_post_idx[ppost->canvas_id][0];
		vf->canvas1Addr =
			di_post_idx[ppost->canvas_id][0];
#ifdef DET3D
		if (ppre->vframe_interleave_flag && di_buf->di_buf[1]) {
			canvas_config(
				di_post_idx[ppost->canvas_id][1],
				di_buf->di_buf[1]->nr_adr,
				di_buf->di_buf[1]->canvas_width[NR_CANVAS],
				canvas_height, 0, 0);
			vf->canvas1Addr =
				di_post_idx[ppost->canvas_id][1];
			vf->duration <<= 1;
		}
#endif
		ppost->next_canvas_id = ppost->canvas_id ? 0 : 1;

		if (di_buf->process_fun_index == PROCESS_FUN_NULL) {
			if (Rd(DI_IF1_GEN_REG) & 0x1 ||
			    Rd(DI_POST_CTRL) & 0x10f)
				dimhv3_disable_post_deinterlace_2();
		}
	}
#endif

	return 0;
#else
	return early_NONE();
#endif
}

static void get_vscale_skip_count(unsigned int par)
{
	/*di_vscale_skip_count_real = (par >> 24) & 0xff;*/
	dimp_set(eDI_MP_di_vscale_skip_count_real,
		 (par >> 24) & 0xff);
}

#define get_vpp_reg_update_flag(par) (((par) >> 16) & 0x1)

static unsigned int pldn_dly = 1;

#if 0
enum pulldown_mode_e {
	PULL_DOWN_BLEND_0 = 0,/* buf1=dup[0] */
	PULL_DOWN_BLEND_2 = 1,/* buf1=dup[2] */
	PULL_DOWN_MTN	  = 2,/* mtn only */
	PULL_DOWN_BUF1	  = 3,/* do wave with dup[0] */
	PULL_DOWN_EI	  = 4,/* ei only */
	PULL_DOWN_NORMAL  = 5,/* normal di */
	PULL_DOWN_NORMAL_2  = 6,/* di */
};

#endif

/******************************************
 *
 ******************************************/
#define edi_mp_post_wr_en		eDI_MP_post_wr_en
#define edi_mp_post_wr_support		eDI_MP_post_wr_support

static unsigned int cfg_nv21/* = DI_BIT0*/;
module_param_named(cfg_nv21, cfg_nv21, uint, 0664);

#ifdef DIM_OUT_NV21
#define NV21_DBG	(1)
#ifdef NV21_DBG
static unsigned int cfg_vf;
module_param_named(cfg_vf, cfg_vf, uint, 0664);

#endif

/**********************************************************
 * canvans
 *	set vfm canvas by config | planes | index
 *	set vf->canvas0Addr
 *
 **********************************************************/
static void dim_canvas_set2(struct vframe_s *vf, u32 *index)
{
	int i;
	u32 *canvas_index = index;
	unsigned int shift;
	struct canvas_config_s *cfg = &vf->canvas0_config[0];
	u32 planes = vf->plane_num;

	if (vf->canvas0Addr != ((u32)-1))
		return;
	if (planes > 3) {
		PR_ERR("%s:planes overflow[%d]\n", __func__, planes);
		return;
	}
	dimv3_print("%s:p[%d]\n", __func__, planes);

	vf->canvas0Addr = 0;
	for (i = 0; i < planes; i++, canvas_index++, cfg++) {
		canvas_config_config(*canvas_index, cfg);
		dimv3_print("\tw[%d],h[%d], endian[%d],cid[%d]\n",
			  cfg->width, cfg->height, cfg->endian, *canvas_index);
		shift = 8 * i;
		vf->canvas0Addr |= (*canvas_index << shift);
		//vf->plane_num = planes;
	}
}

static void di_cnt_cvs_nv21(unsigned int mode,
			    unsigned int *h,
			    unsigned int *v,
			    unsigned int ch)
{
	struct di_mm_s *mm = dim_mm_get(ch); /*mm-0705*/
	int width = mm->cfg.di_w;
	int height = mm->cfg.di_h;
	int canvas_height = height + 8;
	unsigned int nr_width = width;
	unsigned int nr_canvas_width = width;
	unsigned int canvas_align_width = 32;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	nr_width = width;
	nr_canvas_width = nr_width;//nr_width << 1;
	nr_canvas_width = roundup(nr_canvas_width, canvas_align_width);
	*h = nr_canvas_width;
	*v = canvas_height;
}

void div3_cnt_cvs(enum EDPST_MODE mode,
		struct di_win_s *in,
		struct di_win_s *out)
{

	unsigned int inh, inv, oh, ov;
	unsigned int canvas_align_width = 32;
	unsigned int nr_width = 0;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	inh = in->x_size;
	inv = in->y_size;

	switch (mode) {
	case EDPST_MODE_NV21_8BIT:
		nr_width = inh;
		break;
	case EDPST_MODE_422_10BIT_PACK:
		nr_width = (inh * 5) / 4;
		nr_width = nr_width << 1;
		break;
	case EDPST_MODE_422_10BIT:
		nr_width = (inh * 3) / 2;
		nr_width = nr_width << 1;
		break;
	case EDPST_MODE_422_8BIT:
		nr_width = inh;
		nr_width = nr_width << 1;
		break;
	}

	oh = roundup(nr_width, canvas_align_width);
	ov = roundup(inv, 32);
	out->x_size = oh;
	out->y_size = ov;
	if (cfg_nv21 & DI_BIT1) {
		PR_INF("%s:mode[%d], inx[%d],iny[%d], ox[%d], oy[%d]\n",
			__func__, mode, inh, inv, out->x_size, out->y_size);
	}
}
static void dimpst_fill_outvf(struct vframe_s *vfm,
			      struct di_buf_s *di_buf,
			      enum di_output_format mode)
{
	struct canvas_config_s *cvsp;
	unsigned int cvsh, cvsv, csize;
//	struct dim_dvfm_s	*pdvfm;

//	pdvfm = di_buf->c.pdvfm;

//	if (!pdvfm) {
//		PR_ERR("%s:no pdvfm, do nothing\n", __func__);
//		return;
//	}

	//memcpy(vfm, di_buf->vframe, sizeof(*vfm));
	vfm->width = di_buf->c.vmode.w;
	vfm->height = di_buf->c.vmode.h;
	vfm->type = di_buf->c.vmode.vtype;
	vfm->bitdepth = di_buf->c.vmode.bitdepth;
	/* canvas */
	vfm->canvas0Addr = (u32)-1;

	if (mode == DI_OUTPUT_422) {
		vfm->plane_num = 1;
		cvsp = &vfm->canvas0_config[0];
		cvsp->phy_addr = di_buf->nr_adr;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
		cvsp->width = di_buf->canvas_width[NR_CANVAS];
		cvsp->height = di_buf->canvas_height;
	} else {
		vfm->plane_num = 2;
		/* count canvs size */
		di_cnt_cvs_nv21(0, &cvsh, &cvsv, 0);
		/* 0 */
		cvsp = &vfm->canvas0_config[0];
		cvsp->phy_addr = di_buf->nr_adr;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
		cvsp->width = cvsh;
		cvsp->height = cvsv;
		csize = roundup((cvsh * cvsv), PAGE_SIZE);
		/* 1 */
		cvsp = &vfm->canvas0_config[1];
		cvsp->width = cvsh;
		cvsp->height = cvsv;
		cvsp->phy_addr = di_buf->nr_adr + csize;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
	}

	/* type */
	if (mode == DI_OUTPUT_NV21 ||
	    mode == DI_OUTPUT_NV12) {
		/*clear*/
		vfm->type &= ~(VIDTYPE_VIU_NV12	|
			       VIDTYPE_VIU_444	|
			       VIDTYPE_VIU_NV21	|
			       VIDTYPE_VIU_422	|
			       VIDTYPE_VIU_SINGLE_PLANE	|
			       VIDTYPE_PRE_INTERLACE);
		vfm->type |= VIDTYPE_VIU_FIELD;
		vfm->type |= VIDTYPE_DI_PW;
		if (mode == DI_OUTPUT_NV21)
			vfm->type |= VIDTYPE_VIU_NV21;
		else
			vfm->type |= VIDTYPE_VIU_NV12;

		/* bit */
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth &= ~(FULL_PACK_422_MODE);
		vfm->bitdepth |= (BITDEPTH_Y8	|
				  BITDEPTH_U8	|
				  BITDEPTH_V8);
	} else {
		/*temp */
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth &= ~(FULL_PACK_422_MODE);
		vfm->bitdepth |= (FULL_PACK_422_MODE|
				  BITDEPTH_Y10	|
				  BITDEPTH_U10	|
				  BITDEPTH_V10);

	}


#ifdef NV21_DBG
	if (cfg_vf)
		vfm->type = cfg_vf;
#endif
	dbg_vfmv3(vfm, 2);
}

/* when buf is from out, use this function*/
/* no input mode */
/* cvsh, cvsv is from di_buf, not */
/* type clear delet */
/* vfm from differ */
static void dimpst_fill_outvf2(struct vframe_s *vfm,
			      struct di_buf_s *di_buf)
{
	struct canvas_config_s *cvsp;
//	unsigned int cvsh, cvsv, csize;
	enum di_output_format mode;
	struct di_buffer	*ins_buf;

	ins_buf = (struct di_buffer *)di_buf->c.pdvfm->vfm_out;


	//memcpy(vfm, di_buf->vframe, sizeof(*vfm));
	memcpy(vfm, ins_buf->vf, sizeof(*vfm));

	/* canvas */
	vfm->canvas0Addr = (u32)-1;

	if (vfm->type & VIDTYPE_VIU_422)
		mode = DI_OUTPUT_422;
	else if (vfm->type & VIDTYPE_VIU_NV21)
		mode = DI_OUTPUT_NV21;
	else if (vfm->type & VIDTYPE_VIU_NV12)
		mode = DI_OUTPUT_NV12;
	else
		mode = DI_OUTPUT_422;

	if (mode == DI_OUTPUT_422) {
		vfm->plane_num = 1;
		cvsp = &vfm->canvas0_config[0];
		//cvsp->phy_addr = ins_buf->vf->canvas0_config[0].phy_addr;
		//di_buf->nr_adr;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
		//cvsp->width = di_buf->canvas_width[NR_CANVAS];
		//cvsp->height = di_buf->canvas_height;
	} else {
		vfm->plane_num = 2;
		/* count canvs size */
		//di_cnt_cvs_nv21(0, &cvsh, &cvsv, 0);
		//cvsh = di_buf->canvas_width[NR_CANVAS];
		//cvsv = di_buf->canvas_height;
		/* 0 */
		cvsp = &vfm->canvas0_config[0];
		//cvsp->phy_addr = ins_buf->vf->canvas0_config[0].phy_addr;
		//di_buf->nr_adr;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
		//cvsp->width = cvsh;
		//cvsp->height = cvsv;
		//csize = roundup((cvsh * cvsv), PAGE_SIZE);
		/* 1 */
		cvsp = &vfm->canvas0_config[1];
		//cvsp->width = cvsh;
		//cvsp->height = cvsv;
		//cvsp->phy_addr = ins_buf->vf->canvas0_config[1].phy_addr;
		//di_buf->nr_adr + csize;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
	}

	/* type */
#if 0
	/*clear*/
	vfm->type &= ~(VIDTYPE_VIU_NV12	|
		       VIDTYPE_VIU_444	|
		       VIDTYPE_VIU_NV21	|
		       VIDTYPE_VIU_422	|
		       VIDTYPE_VIU_SINGLE_PLANE	|
		       VIDTYPE_PRE_INTERLACE	|
		       VIDTYPE_TYPEMASK);
#else
	/*clear*/
	vfm->type &= ~(VIDTYPE_VIU_SINGLE_PLANE	|
		       VIDTYPE_PRE_INTERLACE	|
		       VIDTYPE_TYPEMASK		|
		       VIDTYPE_INTERLACE_FIRST);
#endif
	vfm->type |= VIDTYPE_VIU_FIELD;
	vfm->type |= VIDTYPE_DI_PW;

	if (mode == DI_OUTPUT_NV21 ||
	    mode == DI_OUTPUT_NV12) {

	#if 0
		if (mode == EDPST_OUT_MODE_NV21)
			vfm->type |= VIDTYPE_VIU_NV21;
		else
			vfm->type |= VIDTYPE_VIU_NV12;
	#endif
		/* bit */
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth &= ~(FULL_PACK_422_MODE);
		vfm->bitdepth |= (BITDEPTH_Y8	|
				  BITDEPTH_U8	|
				  BITDEPTH_V8);
	} else {
	/* 422*/
		vfm->type |= VIDTYPE_VIU_SINGLE_PLANE;
	}
#ifdef NV21_DBG
	if (cfg_vf)
		vfm->type = cfg_vf;
#endif

	dbg_vfmv3(vfm, 2);
}

#define edi_mp_pps_position	eDI_MP_pps_position
#define edi_mp_pps_dstw		eDI_MP_pps_dstw
#define edi_mp_pps_dsth		eDI_MP_pps_dsth

static void dim_cfg_s_mif(struct DI_SIM_MIF_s *smif,
			  struct vframe_s *vf,
			  struct di_win_s *win)
{
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	//vframe_t *vf = di_buf->vframe;

	//smif->canvas_num = di_buf->nr_canvas_idx;
	/* bit mode config */
	if (vf->bitdepth & BITDEPTH_Y10) {
		if (vf->type & VIDTYPE_VIU_444) {
			smif->bit_mode = (vf->bitdepth & FULL_PACK_422_MODE) ?
						3 : 2;
		} else {
			smif->bit_mode = (vf->bitdepth & FULL_PACK_422_MODE) ?
						3 : 1;
		}
	} else {
		smif->bit_mode = 0;
	}

	/* video mode */
	if (vf->type & VIDTYPE_VIU_444)
		smif->video_mode = 1;
	else
		smif->video_mode = 0;

	/* separate */
	if (vf->type & VIDTYPE_VIU_422)
		smif->set_separate_en = 0;
	else
		smif->set_separate_en = 2; /*nv12 ? nv 21?*/

	if (vf->type & VIDTYPE_VIU_NV12) {
		smif->cbcr_swap = 1;
		smif->l_endian = 1;
		smif->reg_swap = 0;
	} else {
		smif->cbcr_swap = 0;
		smif->l_endian = 0;
		smif->reg_swap = 1;
	}

	/*x,y,*/
	dimv3_print("nv12:vf[%d],sw[%d]\n", (vf->type & VIDTYPE_VIU_NV12),
		  smif->cbcr_swap);
	if (de_devp->pps_enable &&
	    dimp_get(edi_mp_pps_position) == 0) {
		//dimv3_pps_config(0, di_width, di_height,
		//	       dimp_get(edi_mp_pps_dstw),
		//	       dimp_get(edi_mp_pps_dsth));
		if (win) {
			smif->start_x = win->x_st;
			smif->end_x = win->x_st +
				dimp_get(edi_mp_pps_dstw) - 1;
			smif->start_y = win->y_st;
			smif->end_y = win->y_st +
				dimp_get(edi_mp_pps_dsth) - 1;
		} else {
			smif->start_x = 0;
			smif->end_x =
				dimp_get(edi_mp_pps_dstw) - 1;
			smif->start_y = 0;
			smif->end_y =
				dimp_get(edi_mp_pps_dsth) - 1;
		}
	} else {
		if (win) {
			smif->start_x = win->x_st;
			smif->end_x   = win->x_st + win->x_size - 1;
			smif->start_y = win->y_st;
			smif->end_y   = win->y_st + win->y_size - 1;

		} else {
			smif->start_x = 0;
			smif->end_x   = vf->width - 1;
			smif->start_y = 0;
			smif->end_y   = vf->height - 1;
		}
	}
}

#endif /* DIM_OUT_NV21 */

void dbg_vfmv3(struct vframe_s *vf, unsigned int dbgpos)
{
	int i;
	struct canvas_config_s *cvsp;

	if (!(cfg_nv21 & DI_BIT1))
		return;
	PR_INF("%d:addr:%p\n", dbgpos, vf);
	PR_INF("type=0x%x\n", vf->type);
	PR_INF("bitdepth=0x%x\n", vf->bitdepth);
	PR_INF("plane_num=0x%x\n", vf->plane_num);
	PR_INF("0Addr=0x%x\n", vf->canvas0Addr);
	PR_INF("1Addr=0x%x\n", vf->canvas1Addr);
	PR_INF("plane_num=0x%x\n", vf->plane_num);
	for (i = 0; i < vf->plane_num; i++) {
		PR_INF("%d:\n", i);
		cvsp = &vf->canvas0_config[i];
		PR_INF("\tph=0x%x\n", cvsp->phy_addr);
		PR_INF("\tw=%d\n", cvsp->width);
		PR_INF("\th=%d\n", cvsp->height);
		PR_INF("\tb=%d\n", cvsp->block_mode);
		PR_INF("\tendian=%d\n", cvsp->endian);
	}
}

int dimv3_post_process(void *arg, unsigned int zoom_start_x_lines,
		     unsigned int zoom_end_x_lines,
		     unsigned int zoom_start_y_lines,
		     unsigned int zoom_end_y_lines,
		     vframe_t *disp_vf)
{
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;
	struct di_buf_s *di_pldn_buf = NULL;
	unsigned int di_width, di_height, di_start_x, di_end_x, mv_offset;
	unsigned int di_start_y, di_end_y, hold_line;
	unsigned int post_blend_en = 0, post_blend_mode = 0,
		     blend_mtn_en = 0, ei_en = 0, post_field_num = 0;
	int di_vpp_en, di_ddr_en;
	unsigned char mc_pre_flag = 0;
	bool invert_mv = false;
	static int post_index = -1;
	unsigned char tmp_idx = 0;
	struct di_dev_s *de_devp = getv3_dim_de_devp();
	struct di_hpst_s  *pst = get_hw_pst();
	struct dim_vmode_s *pvmode;
#ifdef DIM_OUT_NV21
	struct dim_inter_s *pintf;
#endif
	unsigned char channel = pst->curr_ch;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
#ifdef DIM_OUT_NV21
	u32	cvs_nv21[2];
#endif

	dimp_inc(eDI_MP_post_cnt);
	if (ppost->vscale_skip_flag)
		return 0;

	get_vscale_skip_count(zoom_start_x_lines);

	if (IS_ERR_OR_NULL(di_buf))
		return 0;

	#if 0
	if (!di_buf->vframe)
		return 0;

	else
	#endif
	if (IS_ERR_OR_NULL(di_buf->c.di_buf_dup_p[0]))
		return 0;

	hold_line = dimp_get(eDI_MP_post_hold_line);
	di_pldn_buf = di_buf->c.di_buf_dup_p[pldn_dly];

	di_buf->c.canvas_config_flag = 1;/*ary add */
	if (div3_que_is_in_que(channel, QUE_POST_FREE, di_buf) &&
	    post_index != di_buf->index) {
		post_index = di_buf->index;
		PR_ERR("%s:post_buf[%d] is in post free list.\n",
		       __func__, di_buf->index);
		return 0;
	}
	hpstv3_dbg_mem_pd_trig(0);
	pvmode = &di_buf->c.vmode;/* input*/
#if 0	/*ary: no use?*/
	if (ppost->toggle_flag && di_buf->di_buf_dup_p[1])
		topv3_bot_config(di_buf->di_buf_dup_p[1]);

	ppost->toggle_flag = false;
#endif

	ppost->cur_disp_index = di_buf->index;

	if (get_vpp_reg_update_flag(zoom_start_x_lines)	||
	    dimp_get(eDI_MP_post_refresh))
		ppost->update_post_reg_flag = 1;

	zoom_start_x_lines = zoom_start_x_lines & 0xffff;
	zoom_end_x_lines = zoom_end_x_lines & 0xffff;
	zoom_start_y_lines = zoom_start_y_lines & 0xffff;
	zoom_end_y_lines = zoom_end_y_lines & 0xffff;

	if (!get_init_flag(channel) && IS_ERR_OR_NULL(ppost->keep_buf)) {
		PR_ERR("%s 2:\n", __func__);
		return 0;
	}
	/*dbg*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_SETB, channel, ppost->frame_cnt);
	dbg_post_cnt(channel, "ps1");
	di_start_x = zoom_start_x_lines;
	di_end_x = zoom_end_x_lines;
	di_width = di_end_x - di_start_x + 1;
	di_start_y = zoom_start_y_lines;
	di_end_y = zoom_end_y_lines;
	di_height = di_end_y - di_start_y + 1;
	di_height
	= di_height / (dimp_get(eDI_MP_di_vscale_skip_count_real) + 1);
	/* make sure the height is even number */
	if (di_height % 2) {
		/*for skip mode,post only half line-1*/
		if (!dimp_get(eDI_MP_post_wr_en)	&&
		    ((di_height > 150)			&&
		    (di_height < 1080))			&&
		    dimp_get(eDI_MP_di_vscale_skip_count_real))
			di_height = di_height - 3;
		else
			di_height++;
	}
	#if 0
	dimhv3_post_ctrl(DI_HW_POST_CTRL_INIT,
		       (post_wr_en && post_wr_support));
	#endif

#ifdef DIM_OUT_NV21
	pintf = get_dev_intf(channel);
	if (pintf->tmode == EDIM_TMODE_2_PW_OUT) {
		dimpst_fill_outvf2(&pst->vf_post, di_buf);
	} else {
		/* nv 21*/
		if (cfg_nv21 & DI_BIT0)
			dimpst_fill_outvf(&pst->vf_post,
					  di_buf,
					  DI_OUTPUT_NV21);
		else
			dimpst_fill_outvf(&pst->vf_post,
					  di_buf,
					  DI_OUTPUT_422);
	}
	/*************************************************/
#endif
	if (Rd(DI_POST_SIZE) != ((di_width - 1) | ((di_height - 1) << 16)) ||
	    ppost->buf_type != di_buf->c.di_buf_dup_p[0]->type ||
	    (ppost->di_buf0_mif.luma_x_start0 != di_start_x) ||
	    (ppost->di_buf0_mif.luma_y_start0 != di_start_y / 2)) {
		dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_RESIZE, channel,
				  ppost->frame_cnt);/*dbg*/
		ppost->buf_type = di_buf->c.di_buf_dup_p[0]->type;

		dimhv3_initial_di_post_2(di_width, di_height,
				       hold_line,
			(dimp_get(eDI_MP_post_wr_en) &&
			dimp_get(eDI_MP_post_wr_support)));

		#if 0
		if (!di_buf->c.di_buf_dup_p[0]->vframe ||
		    !di_buf->vframe) {
			PR_ERR("%s 3:\n", __func__);
			return 0;
		}
		#endif
#ifdef DIM_OUT_NV21
		/* nv 21*/
		dim_cfg_s_mif(&ppost->di_diwr_mif, &pst->vf_post, NULL);
#endif
		/* bit mode config */
		#if 0
		if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
			if (di_buf->vframe->type & VIDTYPE_VIU_444) {
				ppost->di_buf0_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;
				ppost->di_buf1_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;
				ppost->di_buf2_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;
				ppost->di_diwr_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;

			} else {
				ppost->di_buf0_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
				ppost->di_buf1_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
				ppost->di_buf2_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
				ppost->di_diwr_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
			}
		} else {
			ppost->di_buf0_mif.bit_mode = 0;
			ppost->di_buf1_mif.bit_mode = 0;
			ppost->di_buf2_mif.bit_mode = 0;
			ppost->di_diwr_mif.bit_mode = 0;
		}
		#else
		ppost->di_buf0_mif.bit_mode = pvmode->bit_mode;
		ppost->di_buf1_mif.bit_mode = pvmode->bit_mode;
		ppost->di_buf2_mif.bit_mode = pvmode->bit_mode;
#ifndef DIM_OUT_NV21
		ppost->di_diwr_mif.bit_mode = pvmode->bit_mode;
#endif
		#endif

		#if 0
		if (di_buf->vframe->type & VIDTYPE_VIU_444) {
			ppost->di_buf0_mif.video_mode = 1;
			ppost->di_buf1_mif.video_mode = 1;
			ppost->di_buf2_mif.video_mode = 1;
		} else {
			ppost->di_buf0_mif.video_mode = 0;
			ppost->di_buf1_mif.video_mode = 0;
			ppost->di_buf2_mif.video_mode = 0;
		}
		#else
		if (pvmode->vtype & DIM_VFM_VIU_444) {
			ppost->di_buf0_mif.video_mode = 1;
			ppost->di_buf1_mif.video_mode = 1;
			ppost->di_buf2_mif.video_mode = 1;
		} else {
			ppost->di_buf0_mif.video_mode = 0;
			ppost->di_buf1_mif.video_mode = 0;
			ppost->di_buf2_mif.video_mode = 0;
		}

		#endif
		#if 0
		if (ppost->buf_type == VFRAME_TYPE_IN &&
		    !(di_buf->c.di_buf_dup_p[0]->vframe->type &
		      VIDTYPE_VIU_FIELD)) {
			PR_ERR("%s:type in?\n", __func__);/*ary*/
			if (di_buf->vframe->type & VIDTYPE_VIU_NV21) {
				ppost->di_buf0_mif.set_separate_en = 1;
				ppost->di_buf1_mif.set_separate_en = 1;
				ppost->di_buf2_mif.set_separate_en = 1;
			} else {
				ppost->di_buf0_mif.set_separate_en = 0;
				ppost->di_buf1_mif.set_separate_en = 0;
				ppost->di_buf2_mif.set_separate_en = 0;
			}
			ppost->di_buf0_mif.luma_y_start0 = di_start_y;
			ppost->di_buf0_mif.luma_y_end0 = di_end_y;
		} else { /* from vdin or local vframe process by di pre */
			ppost->di_buf0_mif.set_separate_en = 0;
			ppost->di_buf0_mif.luma_y_start0 =
				di_start_y >> 1;
			ppost->di_buf0_mif.luma_y_end0 = di_end_y >> 1;
			ppost->di_buf1_mif.set_separate_en = 0;
			ppost->di_buf1_mif.luma_y_start0 =
				di_start_y >> 1;
			ppost->di_buf1_mif.luma_y_end0 = di_end_y >> 1;
			ppost->di_buf2_mif.set_separate_en = 0;
			ppost->di_buf2_mif.luma_y_end0 = di_end_y >> 1;
			ppost->di_buf2_mif.luma_y_start0 =
				di_start_y >> 1;
		}
		#else
		ppost->di_buf0_mif.set_separate_en = 0;
		ppost->di_buf0_mif.luma_y_start0 =
			di_start_y >> 1;
		ppost->di_buf0_mif.luma_y_end0 = di_end_y >> 1;
		ppost->di_buf1_mif.set_separate_en = 0;
		ppost->di_buf1_mif.luma_y_start0 =
			di_start_y >> 1;
		ppost->di_buf1_mif.luma_y_end0 = di_end_y >> 1;
		ppost->di_buf2_mif.set_separate_en = 0;
		ppost->di_buf2_mif.luma_y_end0 = di_end_y >> 1;
		ppost->di_buf2_mif.luma_y_start0 =
			di_start_y >> 1;

		#endif
		ppost->di_buf0_mif.luma_x_start0 = di_start_x;
		ppost->di_buf0_mif.luma_x_end0 = di_end_x;
		ppost->di_buf1_mif.luma_x_start0 = di_start_x;
		ppost->di_buf1_mif.luma_x_end0 = di_end_x;
		ppost->di_buf2_mif.luma_x_start0 = di_start_x;
		ppost->di_buf2_mif.luma_x_end0 = di_end_x;

		if (dimp_get(eDI_MP_post_wr_en) &&
		    dimp_get(eDI_MP_post_wr_support)) {
			if (de_devp->pps_enable &&
			    dimp_get(eDI_MP_pps_position) == 0) {
				dimv3_pps_config(0, di_width, di_height,
					       dimp_get(eDI_MP_pps_dstw),
					       dimp_get(eDI_MP_pps_dsth));
#ifndef DIM_OUT_NV21
				ppost->di_diwr_mif.start_x = 0;
				ppost->di_diwr_mif.end_x
					= dimp_get(eDI_MP_pps_dstw) - 1;
				ppost->di_diwr_mif.start_y = 0;
				ppost->di_diwr_mif.end_y
					= dimp_get(eDI_MP_pps_dsth) - 1;
			} else {
				ppost->di_diwr_mif.start_x = di_start_x;
				ppost->di_diwr_mif.end_x   = di_end_x;
				ppost->di_diwr_mif.start_y = di_start_y;
				ppost->di_diwr_mif.end_y   = di_end_y;
#endif
			}
		}

		ppost->di_mtnprd_mif.start_x = di_start_x;
		ppost->di_mtnprd_mif.end_x = di_end_x;
		ppost->di_mtnprd_mif.start_y = di_start_y >> 1;
		ppost->di_mtnprd_mif.end_y = di_end_y >> 1;
		if (dimp_get(eDI_MP_mcpre_en)) {
			ppost->di_mcvecrd_mif.start_x = di_start_x / 5;
			mv_offset = (di_start_x % 5) ? (5 - di_start_x % 5) : 0;
			ppost->di_mcvecrd_mif.vecrd_offset =
				overturn ? (di_end_x + 1) % 5 : mv_offset;
			ppost->di_mcvecrd_mif.start_y =
				(di_start_y >> 1);
			ppost->di_mcvecrd_mif.size_x =
				(di_end_x + 1 + 4) / 5 - 1 - di_start_x / 5;
			ppost->di_mcvecrd_mif.end_y =
				(di_end_y >> 1);
		}
		ppost->update_post_reg_flag = 1;
		/* if height decrease, mtn will not enough */
		if (di_buf->c.pd_config.global_mode != PULL_DOWN_BUF1 &&
		    !dimp_get(eDI_MP_post_wr_en))
			di_buf->c.pd_config.global_mode = PULL_DOWN_EI;
	}

#ifdef DI_USE_FIXED_CANVAS_IDX
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (is_vsync_rdma_enable()) {
#ifdef DIM_OUT_NV21
		if (dimp_get(edi_mp_post_wr_en) &&
		    dimp_get(edi_mp_post_wr_support))
			ppost->canvas_id = 0;
		else
			ppost->canvas_id = ppost->next_canvas_id;
#else
		ppost->canvas_id = ppost->next_canvas_id;
#endif
	} else {
		ppost->canvas_id = 0;
		ppost->next_canvas_id = 1;
		if (dimp_get(eDI_MP_post_wr_en) &&
		    dimp_get(eDI_MP_post_wr_support))
			ppost->canvas_id =
				ppost->next_canvas_id;
	}
#endif
	/*post_blend = di_buf->pd_config.global_mode;*/
	dimp_set(eDI_MP_post_blend, di_buf->c.pd_config.global_mode);
	dimv3_print("%s:ch[%d]:blend[%d]\n", __func__, channel,
		  dimp_get(eDI_MP_post_blend));
	switch (dimp_get(eDI_MP_post_blend)) {
	case PULL_DOWN_BLEND_0:
	case PULL_DOWN_NORMAL:
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[1],
			di_post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[2], -1,
			di_post_idx[ppost->canvas_id][2]);
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[0],
			di_post_idx[ppost->canvas_id][1], -1);
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[2],
			di_post_idx[ppost->canvas_id][3], -1);
		if (dimp_get(eDI_MP_mcpre_en))
			config_mcvec_canvas_idx(
				di_buf->c.di_buf_dup_p[2],
				di_post_idx[ppost->canvas_id][4]);
		break;
	case PULL_DOWN_BLEND_2:
	case PULL_DOWN_NORMAL_2:
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[0],
			di_post_idx[ppost->canvas_id][3], -1);
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[1],
			di_post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[2], -1,
			di_post_idx[ppost->canvas_id][2]);
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[2],
			di_post_idx[ppost->canvas_id][1], -1);
		if (dimp_get(eDI_MP_mcpre_en))
			config_mcvec_canvas_idx(
				di_buf->c.di_buf_dup_p[2],
				di_post_idx[ppost->canvas_id][4]);
		break;
	case PULL_DOWN_MTN:
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[1],
			di_post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[2], -1,
			di_post_idx[ppost->canvas_id][2]);
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[0],
			di_post_idx[ppost->canvas_id][1], -1);
		break;
	case PULL_DOWN_BUF1:/* wave with buf1 :ary: p as i*/
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[1],
			di_post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[1], -1,
			di_post_idx[ppost->canvas_id][2]);
		config_canvas_idx(
			di_buf->c.di_buf_dup_p[0],
			di_post_idx[ppost->canvas_id][1], -1);
		break;
	case PULL_DOWN_EI:
		if (di_buf->c.di_buf_dup_p[1])
			config_canvas_idx(
				di_buf->c.di_buf_dup_p[1],
				di_post_idx[ppost->canvas_id][0], -1);
		break;
	default:
		break;
	}
	ppost->next_canvas_id = ppost->canvas_id ? 0 : 1;
#endif
	if (!di_buf->c.di_buf_dup_p[1]) {
		PR_ERR("%s 4:\n", __func__);
		return 0;
	}
	#if 0
	if (!di_buf->c.di_buf_dup_p[1]->vframe ||
	    !di_buf->c.di_buf_dup_p[0]->vframe) {
		PR_ERR("%s 5:\n", __func__);
		return 0;
	}
	#endif
	if (is_meson_txl_cpu() && overturn && di_buf->c.di_buf_dup_p[2]) {
		/*sync from kernel 3.14 txl*/
		if (dimp_get(eDI_MP_post_blend) == PULL_DOWN_BLEND_2)
			dimp_set(eDI_MP_post_blend, PULL_DOWN_BLEND_0);
		else if (dimp_get(eDI_MP_post_blend) == PULL_DOWN_BLEND_0)
			dimp_set(eDI_MP_post_blend, PULL_DOWN_BLEND_2);
	}

	switch (dimp_get(eDI_MP_post_blend)) {
	case PULL_DOWN_BLEND_0:
	case PULL_DOWN_NORMAL:
		#if 0
		post_field_num =
		(di_buf->c.di_buf_dup_p[1]->vframe->type &
		 VIDTYPE_TYPEMASK)
		== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		#else
		post_field_num = (di_buf->c.di_buf_dup_p[1]->c.wmode.is_top) ?
				 0 : 1;
		#endif
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[0]->nr_canvas_idx;
		ppost->di_buf2_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[2]->nr_canvas_idx;
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->c.di_buf_dup_p[2]->mtn_canvas_idx;
		/*mc_pre_flag = is_meson_txl_cpu()?2:(overturn?0:1);*/
		if (is_meson_txl_cpu() && overturn) {
			/* swap if1&if2 mean negation of mv for normal di*/
			tmp_idx = ppost->di_buf1_mif.canvas0_addr0;
			ppost->di_buf1_mif.canvas0_addr0 =
				ppost->di_buf2_mif.canvas0_addr0;
			ppost->di_buf2_mif.canvas0_addr0 = tmp_idx;
		}
		mc_pre_flag = overturn ? 0 : 1;
		if (di_buf->c.pd_config.global_mode == PULL_DOWN_NORMAL) {
			post_blend_mode = 3;
			/*if pulldown, mcdi_mcpreflag is 1,*/
			/*it means use previous field for MC*/
			/*else not pulldown,mcdi_mcpreflag is 2*/
			/*it means use forward & previous field for MC*/
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 2;
		} else {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 1;
			post_blend_mode = 1;
		}
		if (is_meson_txl_cpu() && overturn)
			mc_pre_flag = 1;

		if (dimp_get(eDI_MP_mcpre_en)) {
			ppost->di_mcvecrd_mif.canvas_num =
				di_buf->c.di_buf_dup_p[2]->mcvec_canvas_idx;
		}
		blend_mtn_en = 1;
		ei_en = 1;
		dimp_set(eDI_MP_post_ei, 1);
		post_blend_en = 1;
		break;
	case PULL_DOWN_BLEND_2:
	case PULL_DOWN_NORMAL_2:
		#if 0
		post_field_num =
			(di_buf->c.di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		#else
		post_field_num = (di_buf->c.di_buf_dup_p[1]->c.wmode.is_top) ?
				 0 : 1;
		#endif
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[2]->nr_canvas_idx;
		ppost->di_buf2_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[0]->nr_canvas_idx;
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->c.di_buf_dup_p[2]->mtn_canvas_idx;
		if (is_meson_txl_cpu() && overturn) {
			ppost->di_buf1_mif.canvas0_addr0 =
			ppost->di_buf2_mif.canvas0_addr0;
		}
		if (dimp_get(eDI_MP_mcpre_en)) {
			ppost->di_mcvecrd_mif.canvas_num =
				di_buf->c.di_buf_dup_p[2]->mcvec_canvas_idx;
			mc_pre_flag = is_meson_txl_cpu() ? 0 :
				(overturn ? 1 : 0);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
				invert_mv = true;
			else if (!overturn)
				ppost->di_buf2_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[2]->nr_canvas_idx;
		}
		if (di_buf->c.pd_config.global_mode == PULL_DOWN_NORMAL_2) {
			post_blend_mode = 3;
			/*if pulldown, mcdi_mcpreflag is 1,*/
			/*it means use previous field for MC*/
			/*else not pulldown,mcdi_mcpreflag is 2*/
			/*it means use forward & previous field for MC*/
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 2;
		} else {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 1;
			post_blend_mode = 1;
		}
		blend_mtn_en = 1;
		ei_en = 1;
		dimp_set(eDI_MP_post_ei, 1);
		post_blend_en = 1;
		break;
	case PULL_DOWN_MTN:
		#if 0
		post_field_num =
			(di_buf->c.di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		#else
		post_field_num = (di_buf->c.di_buf_dup_p[1]->c.wmode.is_top) ?
				 0 : 1;
		#endif
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[0]->nr_canvas_idx;
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->c.di_buf_dup_p[2]->mtn_canvas_idx;
		post_blend_mode = 0;
		blend_mtn_en = 1;
		ei_en = 1;
		dimp_set(eDI_MP_post_ei, 1);
		post_blend_en = 1;
		break;
	case PULL_DOWN_BUF1:	/*wave p as i*/
		#if 0
		post_field_num =
			(di_buf->c.di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		#else
		post_field_num = (di_buf->c.di_buf_dup_p[1]->c.wmode.is_top) ?
				 0 : 1;
		#endif
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->c.di_buf_dup_p[1]->mtn_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->c.di_buf_dup_p[0]->nr_canvas_idx;
		post_blend_mode = 1;
		blend_mtn_en = 0;
		ei_en = 0;
		dimp_set(eDI_MP_post_ei, 0);
		post_blend_en = 0;
		break;
	case PULL_DOWN_EI:
		if (di_buf->c.di_buf_dup_p[1]) {
			ppost->di_buf0_mif.canvas0_addr0 =
				di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
			#if 0
			post_field_num =
				(di_buf->c.di_buf_dup_p[1]->vframe->type &
				 VIDTYPE_TYPEMASK)
				== VIDTYPE_INTERLACE_TOP ? 0 : 1;
			#else
			post_field_num =
				(di_buf->c.di_buf_dup_p[1]->c.wmode.is_top) ?
				 0 : 1;
			#endif
		} else {
			#if 0
			post_field_num =
				(di_buf->c.di_buf_dup_p[0]->vframe->type &
				 VIDTYPE_TYPEMASK)
				== VIDTYPE_INTERLACE_TOP ? 0 : 1;
			#else
			post_field_num =
				(di_buf->c.di_buf_dup_p[0]->c.wmode.is_top) ?
				 0 : 1;
			#endif
			ppost->di_buf0_mif.src_field_mode
				= post_field_num;
		}
		post_blend_mode = 2;
		blend_mtn_en = 0;
		ei_en = 1;
		dimp_set(eDI_MP_post_ei, 1);
		post_blend_en = 0;
		break;
	default:
		break;
	}

	if (dimp_get(eDI_MP_post_wr_en) && dimp_get(eDI_MP_post_wr_support)) {
		#ifdef DIM_OUT_NV21
		cvs_nv21[0] = di_post_idx[0][5];
		cvs_nv21[1] = di_post_idx[1][0];
		dim_canvas_set2(&pst->vf_post, &cvs_nv21[0]);
		ppost->di_diwr_mif.canvas_num = pst->vf_post.canvas0Addr;
		ppost->di_diwr_mif.ddr_en = 1;
		#else
		config_canvas_idx(di_buf,
				  di_post_idx[ppost->canvas_id][5], -1);
		ppost->di_diwr_mif.canvas_num = di_buf->nr_canvas_idx;
		#endif

		di_vpp_en = 0;
		di_ddr_en = 1;
	} else {
		di_vpp_en = 1;
		di_ddr_en = 0;
		#ifdef DIM_OUT_NV21
		ppost->di_diwr_mif.ddr_en = 0;
		#endif
	}

	/* if post size < MIN_POST_WIDTH, force ei */
	if ((di_width < MIN_BLEND_WIDTH) &&
	    (di_buf->c.pd_config.global_mode == PULL_DOWN_BLEND_0	||
	    di_buf->c.pd_config.global_mode == PULL_DOWN_BLEND_2	||
	    di_buf->c.pd_config.global_mode == PULL_DOWN_NORMAL
	    )) {
		post_blend_mode = 1;
		blend_mtn_en = 0;
		ei_en = 0;
		dimp_set(eDI_MP_post_ei, 0);
		post_blend_en = 0;
	}

	if (dimp_get(eDI_MP_mcpre_en))
		ppost->di_mcvecrd_mif.blend_en = post_blend_en;
	invert_mv = overturn ? (!invert_mv) : invert_mv;
	if (ppost->update_post_reg_flag) {
		dimhv3_enable_di_post_2(
			&ppost->di_buf0_mif,
			&ppost->di_buf1_mif,
			&ppost->di_buf2_mif,
			&ppost->di_diwr_mif,
			&ppost->di_mtnprd_mif,
			ei_en,                  /* ei enable */
			post_blend_en,          /* blend enable */
			blend_mtn_en,           /* blend mtn enable */
			post_blend_mode,        /* blend mode. */
			di_vpp_en,		/* di_vpp_en. */
			di_ddr_en,		/* di_ddr_en. */
			post_field_num,         /* 1 bottom generate top */
			hold_line,
			dimp_get(eDI_MP_post_urgent),
			(invert_mv ? 1 : 0),
			dimp_get(eDI_MP_di_vscale_skip_count_real)
			);
		if (dimp_get(eDI_MP_mcpre_en)) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
				dimhv3_enable_mc_di_post_g12(
					&ppost->di_mcvecrd_mif,
					dimp_get(eDI_MP_post_urgent),
					overturn, (invert_mv ? 1 : 0));
			else
				dimhv3_enable_mc_di_post(
					&ppost->di_mcvecrd_mif,
					dimp_get(eDI_MP_post_urgent),
					overturn, (invert_mv ? 1 : 0));
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
			dimv3_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 2);
		}
	} else {
		dimhv3_post_switch_buffer(
			&ppost->di_buf0_mif,
			&ppost->di_buf1_mif,
			&ppost->di_buf2_mif,
			&ppost->di_diwr_mif,
			&ppost->di_mtnprd_mif,
			&ppost->di_mcvecrd_mif,
			ei_en,                  /* ei enable */
			post_blend_en,          /* blend enable */
			blend_mtn_en,           /* blend mtn enable */
			post_blend_mode,        /* blend mode. */
			di_vpp_en,	/* di_vpp_en. */
			di_ddr_en,	/* di_ddr_en. */
			post_field_num,         /* 1 bottom generate top */
			hold_line,
			dimp_get(eDI_MP_post_urgent),
			(invert_mv ? 1 : 0),
			dimp_get(eDI_MP_pulldown_enable),
			dimp_get(eDI_MP_mcpre_en),
			dimp_get(eDI_MP_di_vscale_skip_count_real)
			);
	}

		if (is_meson_gxtvbb_cpu()	||
		    is_meson_txl_cpu()		||
		    is_meson_txlx_cpu()		||
		    is_meson_gxlx_cpu()		||
		    is_meson_txhd_cpu()		||
		    is_meson_g12a_cpu()		||
		    is_meson_g12b_cpu()		||
		    is_meson_tl1_cpu()		||
		    is_meson_tm2_cpu()		||
		    is_meson_sm1_cpu()) {
			if (div3_cfg_top_get(EDI_CFG_ref_2)	&&
			    mc_pre_flag				&&
			    dimp_get(eDI_MP_post_wr_en)) { /*OTT-3210*/
				dbg_once("mc_old=%d\n", mc_pre_flag);
				mc_pre_flag = 1;
			}
		dimv3_post_read_reverse_irq(overturn, mc_pre_flag,
			post_blend_en ? dimp_get(eDI_MP_mcpre_en) : false);
		/* disable mc for first 2 fieldes mv unreliable */
		if (di_buf->c.seq < 2)
			dimv3_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 2);
	}
	if (dimp_get(eDI_MP_mcpre_en)) {
		if (di_buf->c.di_buf_dup_p[2])
			set_post_mcinfo(&di_buf->c.di_buf_dup_p[2]
				->c.curr_field_mcinfo);
	} else if (is_meson_gxlx_cpu()	||
		   is_meson_txl_cpu()	||
		   is_meson_txlx_cpu()) {
		dimv3_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 2);
	}

/* set pull down region (f(t-1) */

	if (di_pldn_buf				&&
	    dimp_get(eDI_MP_pulldown_enable)	&&
	    !ppre->cur_prog_flag) {
		unsigned short offset = (di_start_y >> 1);

		#if 0
		if (overturn)
			offset = ((di_buf->vframe->height - di_end_y) >> 1);
		else
			offset = 0;
		#else
		if (overturn)
			offset = ((di_buf->c.wmode.o_h - di_end_y) >> 1);
		else
			offset = 0;

		#endif
		/*pulldown_vof_win_vshift*/
		get_ops_pd()->vof_win_vshift(&di_pldn_buf->c.pd_config, offset);
		dimhv3_pulldown_vof_win_config(&di_pldn_buf->c.pd_config);
	}
	postv3_mif_sw(true);	/*position?*/
	/*dimh_post_ctrl(DI_HW_POST_CTRL_RESET, di_ddr_en);*/
	/*add by wangfeng 2018-11-15*/
	dimv3_VSYNC_WR_MPEG_REG_BITS(DI_DIWR_CTRL, 1, 31, 1);
	dimv3_VSYNC_WR_MPEG_REG_BITS(DI_DIWR_CTRL, 0, 31, 1);
	/*ary add for post crash*/
	div3_post_set_flow((dimp_get(eDI_MP_post_wr_en)	&&
			  dimp_get(eDI_MP_post_wr_support)),
			 eDI_POST_FLOW_STEP2_START);

	if (ppost->update_post_reg_flag > 0)
		ppost->update_post_reg_flag--;

	/*dbg*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_SETE, channel, ppost->frame_cnt);
	dimv3_tr_ops.post_set(di_buf->c.vmode.omx_index);
	dbg_post_cnt(channel, "ps2");
	ppost->frame_cnt++;

	return 0;
}

#ifndef DI_DEBUG_POST_BUF_FLOW
//static /*ary no use*/
void post_ready_buf_set(unsigned int ch, struct di_buf_s *di_buf)
{
	vframe_t *vframe_ret = NULL;
	struct di_buf_s *nr_buf = NULL;
	unsigned int pw_en = (dimp_get(eDI_MP_post_wr_en)	&&
			      dimp_get(eDI_MP_post_wr_support));

	vframe_ret = di_buf->vframe;
	nr_buf = di_buf->c.di_buf_dup_p[1];
	if (pw_en) {
		if (di_buf->c.process_fun_index != PROCESS_FUN_NULL) {
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
			vframe_ret->canvas0_config[0].phy_addr =
				di_buf->nr_adr;
			vframe_ret->canvas0_config[0].width =
				di_buf->canvas_width[NR_CANVAS],
			vframe_ret->canvas0_config[0].height =
				di_buf->canvas_height;
			vframe_ret->canvas0_config[0].block_mode = 0;
			vframe_ret->plane_num = 1;
			vframe_ret->canvas0Addr = -1;
			vframe_ret->canvas1Addr = -1;
			if (dimp_get(eDI_MP_show_nrwr)) {
				vframe_ret->canvas0_config[0].phy_addr =
					nr_buf->nr_adr;
				vframe_ret->canvas0_config[0].width =
					nr_buf->canvas_width[NR_CANVAS];
				vframe_ret->canvas0_config[0].height =
					nr_buf->canvas_height;
			}
#else
			config_canvas_idx(di_buf, di_wr_idx, -1);
			vframe_ret->canvas0Addr = di_buf->nr_canvas_idx;
			vframe_ret->canvas1Addr = di_buf->nr_canvas_idx;
			if (dimp_get(eDI_MP_show_nrwr)) {
				config_canvas_idx(nr_buf,
						  di_wr_idx, -1);
				vframe_ret->canvas0Addr = di_wr_idx;
				vframe_ret->canvas1Addr = di_wr_idx;
			}
#endif
			vframe_ret->early_process_fun = dimv3_do_post_wr_fun;
			vframe_ret->process_fun = NULL;

			/* 2019-04-22 Suggestions from brian.zhu*/
			vframe_ret->mem_handle = NULL;
			vframe_ret->type |= VIDTYPE_DI_PW;
			/* 2019-04-22 */
		} else {
			/* p use 2 i buf*/
			if (!di_buf->c.wmode.p_use_2i) {
				PR_ERR("%s:not p use 2i\n", __func__);
				return;
			}
			vframe_ret->canvas0_config[0].phy_addr =
				di_buf->c.di_buf[0]->nr_adr;
			vframe_ret->canvas0_config[0].width =
				di_buf->c.di_buf[0]->canvas_width[NR_CANVAS],
			vframe_ret->canvas0_config[0].height =
				di_buf->c.di_buf[0]->canvas_height<<1;
			vframe_ret->canvas0_config[0].block_mode = 0;
			vframe_ret->plane_num = 1;
			vframe_ret->canvas0Addr = -1;
			vframe_ret->canvas1Addr = -1;

			vframe_ret->early_process_fun = dimv3_do_post_wr_fun;
			vframe_ret->process_fun = NULL;

			/* 2019-04-22 Suggestions from brian.zhu*/
			vframe_ret->mem_handle = NULL;
			vframe_ret->type |= VIDTYPE_DI_PW;
		}
	}
}

#endif
void dimv3_post_de_done_buf_config(unsigned int channel)
{
	ulong irq_flag2 = 0;
	struct di_buf_s *di_buf = NULL;
	struct di_post_stru_s *ppost = get_post_stru(channel);
//	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_ch_s		*pch;

	if (!ppost->cur_post_buf)
		return;
	dbg_post_cnt(channel, "pd1");
	/*dbg*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_DB, channel, ppost->frame_cnt);

	di_lock_irqfiq_save(irq_flag2);
	queuev3_out(channel, ppost->cur_post_buf);/*? which que?post free*/
	di_buf = ppost->cur_post_buf;

	#if 0	/*no use*/
	if (de_devp->pps_enable && dimp_get(eDI_MP_pps_position) == 0) {
		di_buf->vframe->width = dimp_get(eDI_MP_pps_dstw);
		di_buf->vframe->height = dimp_get(eDI_MP_pps_dsth);
	}
	#endif
	#ifdef DI_DEBUG_POST_BUF_FLOW
	#else
	//post_ready_buf_set(channel, di_buf);

	pch = get_chdata(channel);
	vfv3_fill_post_ready(pch, di_buf);
	#endif
	div3_que_in(channel, QUE_POST_READY, ppost->cur_post_buf);
	dimv3_print("post_done:ch[%d],typ[%d],id[%d],omx[%d]\n",
		  channel, ppost->cur_post_buf->type,
		  ppost->cur_post_buf->index,
		  ppost->cur_post_buf->c.vmode.omx_index);
	#ifdef DI_DEBUG_POST_BUF_FLOW
	#else
	/*add by ary:*/
	recyclev3_post_ready_local(ppost->cur_post_buf, channel);
	#endif
	di_unlock_irqfiq_restore(irq_flag2);
	dimv3_tr_ops.post_ready(di_buf->c.vmode.omx_index);
	pwv3_vf_notify_receiver(channel,
			      VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	ppost->cur_post_buf = NULL;
	/*dbg*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_DE, channel, ppost->frame_cnt);
	dbg_post_cnt(channel, "pd2");
}

void dimv3_post_de_done_buf_config_vfm(struct di_ch_s *pch)
{
	ulong irq_flag2 = 0;
	struct di_buf_s *di_buf = NULL;
//	struct di_dev_s *de_devp = get_dim_de_devp();
	//struct di_ch_s		*pch;
	unsigned int ch = pch->ch_id;
	struct di_post_stru_s *ppost = get_post_stru(ch);

	if (!ppost->cur_post_buf)
		return;
	dbg_post_cnt(ch, "pd1");
	/*dbg*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_DB, ch, ppost->frame_cnt);

	di_lock_irqfiq_save(irq_flag2);
	queuev3_out(ch, ppost->cur_post_buf);/*? which que?post free*/
	di_buf = ppost->cur_post_buf;

	#if 0	/*no use*/
	if (de_devp->pps_enable && dimp_get(eDI_MP_pps_position) == 0) {
		di_buf->vframe->width = dimp_get(eDI_MP_pps_dstw);
		di_buf->vframe->height = dimp_get(eDI_MP_pps_dsth);
	}
	#endif
	#ifdef DI_DEBUG_POST_BUF_FLOW
	#else
	//post_ready_buf_set(channel, di_buf);

	vfv3_fill_post_ready(pch, di_buf);
	#endif
	div3_que_in(ch, QUE_POST_READY, ppost->cur_post_buf);
	dimv3_print("post_done:ch[%d],typ[%d],id[%d],omx[%d]\n",
		  ch, ppost->cur_post_buf->type,
		  ppost->cur_post_buf->index,
		  ppost->cur_post_buf->c.vmode.omx_index);
	#ifdef DI_DEBUG_POST_BUF_FLOW
	#else
	/*add by ary:*/
	recyclev3_post_ready_local(ppost->cur_post_buf, ch);
	#endif
	di_unlock_irqfiq_restore(irq_flag2);
	dimv3_tr_ops.post_ready(di_buf->c.vmode.omx_index);
	pwv3_vf_notify_receiver(ch,
			      VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	ppost->cur_post_buf = NULL;
	/*dbg*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_DE, ch, ppost->frame_cnt);
	dbg_post_cnt(ch, "pd2");
}

void dimv3_post_de_done_buf_config_ins(struct di_ch_s *pch)
{
	ulong irq_flag2 = 0;
	struct di_buf_s *di_buf = NULL;
	unsigned int ch = pch->ch_id;
	struct di_post_stru_s *ppost = get_post_stru(ch);
//	struct di_dev_s *de_devp = get_dim_de_devp();
	//struct di_ch_s		*pch;
	struct dim_inter_s	*pitf;
	struct di_buffer *ins_buf;

	if (!ppost->cur_post_buf)
		return;
	pitf = &pch->interf;

	dbg_post_cnt(ch, "pd1");
	/*dbg*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_DB, ch, ppost->frame_cnt);

	di_lock_irqfiq_save(irq_flag2);
	//queue_out(ch, ppost->cur_post_buf);/*? which que?post free*/
	di_buf = ppost->cur_post_buf;

	ins_buf = (struct di_buffer *)di_buf->c.pdvfm->vfm_out;
	if (!ins_buf) {
		PR_ERR("%s:no vfm_out\n", __func__);
		PR_ERR("ch[%d]:type[%d], id[%d]\n", ch,
		       di_buf->type, di_buf->index);
		return;
	}
	//no use di_que_in(ch, QUE_POST_READY, ppost->cur_post_buf);
	dimv3_print("post_done:ch[%d],typ[%d],id[%d],omx[%d]\n",
		  ch, di_buf->type,
		  di_buf->index,
		  di_buf->c.vmode.omx_index);
	dimv3_print("post_done:%d,%d\n", pch->sum_pst_done, pch->sum_put);
	/*? for bypass mode do what ?*/
	vfv3_fill_post_ready_ins(pch, di_buf, ins_buf);
	/* check */
	if ((ins_buf->flag & DI_FLAG_BUF_BY_PASS) &&
	    ((ins_buf->flag & DI_FLAG_EOS) == 0)) {
		if (!ins_buf->vf->vf_ext) {
			PR_ERR("%s:bypass_no ext:%d,t[%d] ins[%d]\n", __func__,
				di_buf->index,
				di_buf->type,
				ins_buf->mng.index);
		}
	}
	ins_buf->caller_data = pitf->u.dinst.parm.caller_data;
	/* check end */
	//pitf->u.dinst.parm.ops.fill_output_done(ins_buf);
	pch->sum_pst_done++;
	recyclev3_post_ready_local(di_buf, ch);
	recycle_vframe_type_post(di_buf, ch);

	di_unlock_irqfiq_restore(irq_flag2);
	pitf->u.dinst.parm.ops.fill_output_done(ins_buf);

	dimv3_tr_ops.post_ready(di_buf->c.vmode.omx_index);

	ppost->cur_post_buf = NULL;
	/*dbg*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_DE, ch, ppost->frame_cnt);
	dbg_post_cnt(ch, "pd2");
}

void dimv3_post_de_done_buf_config_ins_local(struct di_ch_s *pch)
{
	ulong irq_flag2 = 0;
	struct di_buf_s *di_buf = NULL;
	unsigned int ch = pch->ch_id;
	struct di_post_stru_s *ppost = get_post_stru(ch);
//	struct di_dev_s *de_devp = get_dim_de_devp();
	//struct di_ch_s		*pch;
	struct dim_inter_s	*pitf;
	struct di_buffer *ins_buf;

	if (!ppost->cur_post_buf)
		return;
	pitf = &pch->interf;

	dbg_post_cnt(ch, "pd1");
	/*dbg*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_DB, ch, ppost->frame_cnt);

	di_lock_irqfiq_save(irq_flag2);
	queuev3_out(ch, ppost->cur_post_buf);/*? which que?post free*/
	di_unlock_irqfiq_restore(irq_flag2);
	di_buf = ppost->cur_post_buf;

	ins_buf = di_buf->ins;

	//no use di_que_in(ch, QUE_POST_READY, ppost->cur_post_buf);
	dimv3_print("post_done:ch[%d],typ[%d],id[%d],omx[%d]\n",
		  ch, di_buf->type,
		  di_buf->index,
		  di_buf->c.vmode.omx_index);

	/*? for bypass mode do what ?*/
	//vf_fill_post_ready_ins(pch, di_buf, ins_buf);
	vfv3_fill_post_ready(pch, di_buf);
	di_buf->vframe->private_data = ins_buf;
	atomic_set(&di_buf->c.di_cnt, 1);
	ins_buf->private_data = di_buf;
	ins_buf->caller_data = pitf->u.dinst.parm.caller_data;
	if (di_buf->c.wmode.is_i)
		ins_buf->flag = DI_FLAG_I;
	else
		ins_buf->flag = DI_FLAG_P;
	pitf->u.dinst.parm.ops.fill_output_done(ins_buf);
	di_lock_irqfiq_save(irq_flag2);
	queuev3_in(ch, di_buf, QUEUE_DISPLAY);
	recyclev3_post_ready_local(di_buf, ch);
	//recycle_vframe_type_post(di_buf, ch);

	di_unlock_irqfiq_restore(irq_flag2);
	//pitf->u.dinst.parm.ops.fill_output_done(ins_buf);
	dimv3_tr_ops.post_ready(di_buf->c.vmode.omx_index);

	ppost->cur_post_buf = NULL;
	/*dbg*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_POST_DE, ch, ppost->frame_cnt);
	dbg_post_cnt(ch, "pd2");
}

#if 0
static void di_post_process(unsigned int channel)
{
	struct di_buf_s *di_buf = NULL;
	vframe_t *vf_p = NULL;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	if (ppost->post_de_busy)
		return;
	if (queuev3_empty(channel, QUEUE_POST_DOING)) {
		ppost->post_peek_underflow++;
		return;
	}

	di_buf = getv3_di_buf_head(channel, QUEUE_POST_DOING);
	if (dimv3_check_di_buf(di_buf, 20, channel))
		return;
	vf_p = di_buf->vframe;
	if (ppost->run_early_proc_fun_flag) {
		if (vf_p->early_process_fun)
			vf_p->early_process_fun = dimv3_do_post_wr_fun;
	}
	if (di_buf->process_fun_index) {
		ppost->post_wr_cnt++;
		dimv3_post_process(di_buf, 0, vf_p->width - 1,
				 0, vf_p->height - 1, vf_p);
		ppost->post_de_busy = 1;
		ppost->irq_time = curv3_to_msecs();
	} else {
		ppost->de_post_process_done = 1;
	}
	ppost->cur_post_buf = di_buf;
}
#endif

static void recycle_vframe_type_post(struct di_buf_s *di_buf,
				     unsigned int channel)
{
	int i;
	struct di_buf_s *p;

	if (!di_buf) {
		PR_ERR("%s:\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 15;

		recovery_flag++;
		return;
	}
	if (di_buf->c.process_fun_index == PROCESS_FUN_DI)
		dec_post_ref_count(di_buf);

	for (i = 0; i < 2; i++) {
		if (di_buf->c.di_buf[i]) {
			/*dec vf keep*/
			p = di_buf->c.di_buf[i];
			if (p->c.in_buf) {
				queuev3_in(channel, p->c.in_buf, QUEUE_RECYCLE);
				p->c.in_buf = NULL;
				PR_INF("dec vf;%d,t[%d]\n",
					p->index, p->type);
			}

			queuev3_in(channel, di_buf->c.di_buf[i], QUEUE_RECYCLE);
			dimv3_print("%s: ch[%d]:di_buf[%d],type=%d\n", __func__,
				  channel, di_buf->c.di_buf[i]->index,
				  di_buf->c.di_buf[i]->type);
		}
	}
	dimv3_print("recycle post:typ:[%d],qtype[%d],indx[%d],vfm[%d]\n",
		  di_buf->type, di_buf->queue_index, di_buf->index,
		  di_buf->c.vmode.omx_index);
	/*QUEUE_DISPLAY*/
	queuev3_out(channel, di_buf); /* remove it from display_list_head */
	if (di_buf->queue_index != -1) {
		PR_ERR("qout err:index[%d],typ[%d],qindex[%d]\n",
		       di_buf->index, di_buf->type, di_buf->queue_index);
	/* queue_out_dbg(channel, di_buf);*/
	}
	di_buf->c.invert_top_bot_flag = 0;

	dimv3_buf_clean(di_buf);
	div3_que_in(channel, QUE_POST_FREE, di_buf);
}

void recyclev3_post_ready_local(struct di_buf_s *di_buf, unsigned int channel)
{
	int i;

	if (di_buf->type != VFRAME_TYPE_POST)
		return;

	if (di_buf->c.process_fun_index == PROCESS_FUN_NULL)	/*bypass?*/
		return;

	if (di_buf->c.process_fun_index == PROCESS_FUN_DI)
		dec_post_ref_count(di_buf);

	for (i = 0; i < 2; i++) {
		if (di_buf->c.di_buf[i]) {

			queuev3_in(channel, di_buf->c.di_buf[i], QUEUE_RECYCLE);
			dimv3_print("%s: ch[%d]:di_buf[%d],type=%d\n",
				  __func__,
				  channel,
				  di_buf->c.di_buf[i]->index,
				  di_buf->c.di_buf[i]->type);
			di_buf->c.di_buf[i] = NULL;
		}
	}
}

#ifdef DI_BUFFER_DEBUG
static void
recycle_vframe_type_post_print(struct di_buf_s *di_buf,
			       const char *func,
			       const int	line)
{
	int i;

	dimv3_print("%s:%d ", func, line);
	for (i = 0; i < 2; i++) {
		if (di_buf->c.di_buf[i])
			dimv3_print("%s[%d]<%d>=>recycle_list; ",
				  vframe_type_name[di_buf->di_buf[i]->type],
				  di_buf->c.di_buf[i]->index, i);
	}
	dimv3_print("%s[%d] =>post_free_list\n",
		  vframe_type_name[di_buf->type], di_buf->index);
}
#endif

static unsigned int pldn_dly1 = 1;
static void set_pulldown_mode(struct di_buf_s *di_buf, unsigned int channel)
{
	struct di_buf_s *pre_buf_p = di_buf->c.di_buf_dup_p[pldn_dly1];
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXBB)) {
		if (dimp_get(eDI_MP_pulldown_enable) &&
		    !ppre->cur_prog_flag) {
			if (pre_buf_p) {
				di_buf->c.pd_config.global_mode =
					pre_buf_p->c.pd_config.global_mode;
			} else {
				/* ary add 2019-06-19*/
				di_buf->c.pd_config.global_mode
					= PULL_DOWN_EI;
				PR_ERR("[%s]: index out of range.\n",
				       __func__);
			}
		} else {
			di_buf->c.pd_config.global_mode
				= PULL_DOWN_NORMAL;
		}
	}
}

static void drop_frame(int check_drop, int throw_flag, struct di_buf_s *di_buf,
		       unsigned int ch)
{
	ulong irq_flag2 = 0;
	int /*i = 0,*/drop_flag = 0;
//	struct di_post_stru_s *ppost = get_post_stru(channel);
	bool dbg_flg = false;
	struct di_ch_s *pch;
	struct dim_inter_s *pintf;

	pch = get_chdata(ch);
	if (!pch) {
		PR_ERR("%s:no pch\n", __func__);
		return;
	}

	pintf = &pch->interf;
	di_lock_irqfiq_save(irq_flag2);

	#if 0	/*remove vframe*/
	if ((frame_count == 0) && check_drop)
		ppost->start_pts = di_buf->vframe->pts;
	#endif
	if ((check_drop &&
	     (frame_count < dimp_get(eDI_MP_start_frame_drop_count))) ||
	     throw_flag) {
		drop_flag = 1;
	} else {
		if (check_drop && (frame_count
			== dimp_get(eDI_MP_start_frame_drop_count))) {
			#if 0	/*ary 2019-10-28*/
			if ((ppost->start_pts) &&
			    (di_buf->vframe->pts == 0))
				di_buf->vframe->pts = ppost->start_pts;
			ppost->start_pts = 0;
			#endif
		}
		#if 0
		for (i = 0; i < 3; i++) {
			if (di_buf->c.di_buf_dup_p[i] &&
			    (di_buf->c.di_buf_dup_p[i]->vframe->bitdepth !=
			     di_buf->vframe->bitdepth)) {
				PR_INF("%s buf[%d] not match bit mode\n",
				       __func__, i);
				drop_flag = 1;
				break;

			}
		}
		#endif
	}
	if (drop_flag) {
		queuev3_in(ch, di_buf, QUEUE_TMP);
		recycle_vframe_type_post(di_buf, ch);

		PR_INF("%s:recycle post %d\n", __func__, __LINE__);

	} else {
		if (dimp_get(eDI_MP_post_wr_en) &&
		    dimp_get(eDI_MP_post_wr_support)) {

			if (dimv3_tmode_is_localpost(ch))
				div3_que_in(ch, QUE_POST_DOING, di_buf);
			else
				div3_que_in(ch, QUE_POST_NOBUF, di_buf);

			dimv3_print("%s:%p,%d,t[%d]\n", __func__,
				  di_buf, di_buf->index, di_buf->type);

		} else {
			div3_que_in(ch, QUE_POST_READY, di_buf);
		}

		dimv3_tr_ops.post_do(di_buf->c.vmode.omx_index);
		dbg_flg = true;
	}
	if (pintf->op_ins_2_doing)
		pintf->op_ins_2_doing(pch, 0, di_buf);

	di_unlock_irqfiq_restore(irq_flag2);

	if (dbg_flg)
		dimv3_print("di:ch[%d]:%dth %s[%d] => post ready %u ms.\n",
			  ch,
			  frame_count,
			  vframe_type_name[di_buf->type], di_buf->index,
			  dim_get_timerms(di_buf->c.vmode.ready_jiffies64));
}

/***********************************************
 * split dim_process_post_vframe
 **********************************************/
static int dim_pst_vframe_i(unsigned int ch, struct di_buf_s *ready_di_buf)
{
	ulong irq_flag2 = 0;
	int i = 0;
	int ret = 0;
	int buffer_keep_count = 3;
	struct di_buf_s *di_buf = NULL;
	struct di_buf_s *p = NULL;
	struct di_buf_s *pdup1 = NULL;/*di_duf_dup_p[1]*/
	int itmp;

	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;

	/* di_buf: get post free */
	di_lock_irqfiq_save(irq_flag2);
	di_buf = div3_que_out_to_di_buf(ch, QUE_POST_FREE);
	if (dimv3_check_di_buf(di_buf, 17, ch)) {
		di_unlock_irqfiq_restore(irq_flag2);
		return 0;
	}
	di_unlock_irqfiq_restore(irq_flag2);
	/*****************/
	dimv3_print("%s\n", __func__);
	/* set post_free's di_buf_dup_p[0/1/2] */
	i = 0;
	div3_que_list(ch, QUE_PRE_READY, &tmpa[0], &psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]);
		dimv3_print("di:keep[%d]:t[%d]:idx[%d]\n",
			  i, tmpa[itmp], p->index);
		di_buf->c.di_buf_dup_p[i++] = p;

		if (i >= buffer_keep_count)
			break;
	}
	if (i < buffer_keep_count) {
		PR_ERR("%s:3\n", __func__);

		if (recovery_flag == 0)
			recovery_log_reason = 18;
		recovery_flag++;
		return 0;
	}

	/* set post_free's vfm by di_buf_dup_p[1]*/
	pdup1 = di_buf->c.di_buf_dup_p[1];

	#if 0	/*remove vframe 12-03*/
	memcpy(di_buf->vframe, pdup1->vframe, sizeof(*di_buf->vframe));
	di_buf->vframe->private_data = di_buf;
	#endif
	di_buf->c.wmode = pdup1->c.wmode;
	di_buf->c.vmode = pdup1->c.vmode;
	di_buf->c.pdvfm = pdup1->c.pdvfm;	/*2019-12-03*/

	if (pdup1->c.post_proc_flag == 3) {
		/* dummy, not for display */
		inc_post_ref_count(di_buf);
		di_buf->c.di_buf[0] = di_buf->c.di_buf_dup_p[0];
		di_buf->c.di_buf[1] = NULL;
		queuev3_out(ch, di_buf->c.di_buf[0]);
		di_buf->c.di_buf[0]->c.sts |= EDI_ST_VFM_RECYCLE;
		di_lock_irqfiq_save(irq_flag2);
		queuev3_in(ch, di_buf, QUEUE_TMP);
		recycle_vframe_type_post(di_buf, ch);

		di_unlock_irqfiq_restore(irq_flag2);
		PR_INF("%s <dummy>: ", __func__);
	} else {
		if (pdup1->c.post_proc_flag == 2) {
			di_buf->c.pd_config.global_mode = PULL_DOWN_BLEND_2;
			/* blend with di_buf->di_buf_dup_p[2] */
		} else if (di_buf->c.wmode.seq_pre < DIM_EI_CNT) {
			/*ary add for first frame*/
			di_buf->c.pd_config.global_mode = PULL_DOWN_EI;
		} else {
			set_pulldown_mode(di_buf, ch);
		}

		if (pdup1->type == VFRAME_TYPE_IN) {
			/* next will be bypass */
			PR_ERR("%s:type is in ?\n", __func__);
			#if 0
			di_buf->vframe->type = VIDTYPE_PROGRESSIVE	|
						VIDTYPE_VIU_422		|
						VIDTYPE_VIU_SINGLE_PLANE |
						VIDTYPE_VIU_FIELD	|
						VIDTYPE_PRE_INTERLACE;
			di_buf->vframe->height >>= 1;
			di_buf->vframe->canvas0Addr = di_buf->c.di_buf_dup_p[0]
						->nr_canvas_idx; /* top */
			di_buf->vframe->canvas1Addr =
				di_buf->c.di_buf_dup_p[0]
				->nr_canvas_idx;
			di_buf->vframe->process_fun = NULL;
			di_buf->c.process_fun_index = PROCESS_FUN_NULL;
			#endif
		} else {
			/*for debug ?*/
			if (dimp_get(eDI_MP_debug_blend_mode) != -1)
				di_buf->c.pd_config.global_mode
				= dimp_get(eDI_MP_debug_blend_mode);
			#if 0	/*remove vframe 12-03*/
			di_buf->vframe->process_fun =
					((dimp_get(eDI_MP_post_wr_en) &&
					  dimp_get(eDI_MP_post_wr_support)) ?
					 NULL : dimv3_post_process);
			#endif
			di_buf->c.process_fun_index = PROCESS_FUN_DI;
			inc_post_ref_count(di_buf);
		}
		/*ary:di_buf_di_buf*/
		di_buf->c.di_buf[0] = di_buf->c.di_buf_dup_p[0];
		di_buf->c.di_buf[1] = NULL;

		/*dec vf keep*/
		p = di_buf->c.di_buf[0];
		if (p->c.in_buf) {
			queuev3_in(ch, p->c.in_buf, QUEUE_RECYCLE);
			p->c.in_buf = NULL;
			dimv3_print("%s:recycle dec vf;%d,t[%d]\n", __func__,
				p->index, p->type);
		} else {
			PR_INF("%s:p:%d,t[%d]\n", __func__, p->index, p->type);
		}

		queuev3_out(ch, di_buf->c.di_buf[0]);
		di_buf->c.di_buf[0]->c.sts |= EDI_ST_VFM_I;

		drop_frame(true,
			   (di_buf->c.di_buf_dup_p[0]->c.throw_flag) ||
			   (di_buf->c.di_buf_dup_p[1]->c.throw_flag) ||
			   (di_buf->c.di_buf_dup_p[2]->c.throw_flag),
			   di_buf, ch);

		frame_count++;

		dimv3_print("%s <i>: blend[%d]:\n", __func__,
			  di_buf->c.pd_config.global_mode);

	}
	ret = 1;

	return ret;
}

/***********************************************
 * split dim_process_post_vframe
 **********************************************/
static int dim_pst_vframe_a(unsigned int channel, struct di_buf_s *ready_di_buf)
{
	int ret = 0;
	#if 0
	ulong irq_flag2 = 0;
	int i = 0;

	int buffer_keep_count = 3;
	struct di_buf_s *di_buf = NULL;
	struct di_buf_s *p = NULL;
	int itmp;

	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;
	bool pw_en;

	/* get post free */
	di_lock_irqfiq_save(irq_flag2);
	di_buf = div3_que_out_to_di_buf(channel, QUE_POST_FREE);
	if (dimv3_check_di_buf(di_buf, 17, channel)) {
		di_unlock_irqfiq_restore(irq_flag2);
		return 0;
	}
	di_unlock_irqfiq_restore(irq_flag2);

	pw_en = (dimp_get(eDI_MP_post_wr_en) &&
		dimp_get(eDI_MP_post_wr_support)) ? true : false;

	/* set post_free's di_buf_dup_p[0/1/2] */
	i = 0;
	div3_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

	dimv3_print("%s\n", __func__);
	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(channel, tmpa[itmp]);
		dimv3_print("di:keep[%d]:t[%d]:idx[%d]\n",
			  i, tmpa[itmp], p->index);
		di_buf->c.di_buf_dup_p[i++] = p;

		if (i >= buffer_keep_count)
			break;
	}
	if (i < buffer_keep_count) {
		PR_ERR("%s:3\n", __func__);

		if (recovery_flag == 0)
			recovery_log_reason = 18;
		recovery_flag++;
		return 0;
	}

	/* set post_free's vfm by di_buf_dup_p[1]*/
	memcpy(di_buf->vframe,
	       di_buf->c.di_buf_dup_p[1]->vframe,
	       sizeof(vframe_t));
	di_buf->vframe->private_data = di_buf;
	if (di_buf->c.di_buf_dup_p[1]->c.post_proc_flag == 3) {
		/*i mode this will not run*/
		/* dummy, not for display */
		inc_post_ref_count(di_buf);
		di_buf->c.di_buf[0] = di_buf->c.di_buf_dup_p[0];
		di_buf->c.di_buf[1] = NULL;
		queuev3_out(channel, di_buf->c.di_buf[0]);
		di_lock_irqfiq_save(irq_flag2);
		queuev3_in(channel, di_buf, QUEUE_TMP);
		recycle_vframe_type_post(di_buf, channel);

		di_unlock_irqfiq_restore(irq_flag2);
		PR_INF("%s <dummy>: ", __func__);
	} else {
		if (di_buf->c.di_buf_dup_p[1]->c.post_proc_flag == 2) {
			di_buf->c.pd_config.global_mode = PULL_DOWN_BLEND_2;
			/* blend with di_buf->di_buf_dup_p[2] */
		} else {
			set_pulldown_mode(di_buf, channel);
		}
		di_buf->vframe->type = VIDTYPE_PROGRESSIVE		|
					VIDTYPE_VIU_422			|
					VIDTYPE_VIU_SINGLE_PLANE	|
					VIDTYPE_VIU_FIELD		|
					VIDTYPE_PRE_INTERLACE;

		di_buf->vframe->width = di_buf->c.di_buf_dup_p[1]->c.width_bk;

		if (di_buf->c.di_buf_dup_p[1]->c.new_format_flag) {
			/* if (di_buf->di_buf_dup_p[1]
			 * ->post_proc_flag == 2) {
			 */
			di_buf->vframe->early_process_fun = de_post_disable_fun;
		} else {
			di_buf->vframe->early_process_fun = do_nothing_fun;
		}

		if (di_buf->c.di_buf_dup_p[1]->type == VFRAME_TYPE_IN) {
			/* next will be bypass */
			di_buf->vframe->type = VIDTYPE_PROGRESSIVE	|
						VIDTYPE_VIU_422		|
						VIDTYPE_VIU_SINGLE_PLANE |
						VIDTYPE_VIU_FIELD	|
						VIDTYPE_PRE_INTERLACE;
			di_buf->vframe->height >>= 1;
			di_buf->vframe->canvas0Addr = di_buf->c.di_buf_dup_p[0]
						->nr_canvas_idx; /* top */
			di_buf->vframe->canvas1Addr =
				di_buf->c.di_buf_dup_p[0]
				->nr_canvas_idx;
			di_buf->vframe->process_fun = NULL;
			di_buf->c.process_fun_index = PROCESS_FUN_NULL;
		} else {
			/*for debug ?*/
			if (dimp_get(eDI_MP_debug_blend_mode) != -1)
				di_buf->c.pd_config.global_mode
				= dimp_get(eDI_MP_debug_blend_mode);

			di_buf->vframe->process_fun = (pw_en ?
					 NULL : dimv3_post_process);
			di_buf->c.process_fun_index = PROCESS_FUN_DI;
			inc_post_ref_count(di_buf);
		}
		/*ary:di_buf_di_buf*/
		di_buf->c.di_buf[0] = di_buf->c.di_buf_dup_p[0];
		di_buf->c.di_buf[1] = NULL;
		queuev3_out(channel, di_buf->c.di_buf[0]);

		drop_frame(true,
			   (di_buf->c.di_buf_dup_p[0]->c.throw_flag) ||
			   (di_buf->c.di_buf_dup_p[1]->c.throw_flag) ||
			   (di_buf->c.di_buf_dup_p[2]->c.throw_flag),
			   di_buf, channel);

		frame_count++;
#ifdef DI_BUFFER_DEBUG
		dimv3_print("%s <interlace>: ", __func__);
#endif
		if (!pw_en)
			pwv3_vf_notify_receiver(channel,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	}
	#endif
	PR_ERR("%s\n", __func__);
	ret = 1;

	return ret;
}

static int dim_pst_vframe_b(unsigned int channel, struct di_buf_s *ready_di_buf)
{
	int ret = 0;
	#if 0
	ulong irq_flag2 = 0;
	int i = 0;
//	int buffer_keep_count = 3;
	struct di_buf_s *di_buf = NULL;
	//struct di_buf_s *ready_di_buf;
	struct di_buf_s *p = NULL;
	int itmp;
	int ready_count = div3_que_list_count(channel, QUE_PRE_READY);
	bool check_drop = false;
	unsigned int tmpa[MAX_FIFO_SIZE]; /*new que*/
	unsigned int psize; /*new que*/

	/*p			*/
	/*bypass_all mode	*/
	/*??			*/
	/*bypass post mode ?	*/
	if (is_progressive(ready_di_buf->vframe)	||
	//    ready_di_buf->type == VFRAME_TYPE_IN	||
	    ready_di_buf->c.post_proc_flag < 0		||
	    dimp_get(eDI_MP_bypass_post_state)
	    ){
		int vframe_process_count = 1;

		if (dimp_get(eDI_MP_skip_top_bot) &&
		    (!is_progressive(ready_di_buf->vframe)))
			vframe_process_count = 2;

		if (ready_count >= vframe_process_count) {
			struct di_buf_s *di_buf_i;

			di_lock_irqfiq_save(irq_flag2);
			/*get post free*/
			di_buf = div3_que_out_to_di_buf(channel, QUE_POST_FREE);
			if (dimv3_check_di_buf(di_buf, 19, channel)) {
				di_unlock_irqfiq_restore(irq_flag2);
				return 0;
			}

			di_unlock_irqfiq_restore(irq_flag2);

			i = 0;

			div3_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);
			for (itmp = 0; itmp < psize; itmp++) {
				p = pwv3_qindex_2_buf(channel, tmpa[itmp]);
				di_buf->c.di_buf_dup_p[i++] = p;
				if (i >= vframe_process_count) {
					di_buf->c.di_buf_dup_p[i]	= NULL;
					di_buf->c.di_buf_dup_p[i + 1] = NULL;
					break;
				}
			}
			if (i < vframe_process_count) {
				PR_ERR("%s:6\n", __func__);
				if (recovery_flag == 0)
					recovery_log_reason = 22;

				recovery_flag++;
				return 0;
			}

			di_buf_i = di_buf->c.di_buf_dup_p[0];
			if (!is_progressive(ready_di_buf->vframe) &&
			    ((dimp_get(eDI_MP_skip_top_bot) == 1) ||
			    (dimp_get(eDI_MP_skip_top_bot) == 2))) {
			    /*ready is not p?*/
				unsigned int frame_type =
					di_buf->c.di_buf_dup_p[1]->
					vframe->type &
					VIDTYPE_TYPEMASK;
				if (dimp_get(eDI_MP_skip_top_bot) == 1) {
					di_buf_i = (frame_type ==
					VIDTYPE_INTERLACE_TOP)
					? di_buf->c.di_buf_dup_p[1]
					: di_buf->c.di_buf_dup_p[0];

				} else if (
					dimp_get(eDI_MP_skip_top_bot)
					== 2) {
					di_buf_i = (frame_type ==
					VIDTYPE_INTERLACE_BOTTOM)
					? di_buf->c.di_buf_dup_p[1]
					: di_buf->c.di_buf_dup_p[0];
				}
			}

			memcpy(di_buf->vframe, di_buf_i->vframe,
			       sizeof(vframe_t));

			di_buf->vframe->width = di_buf_i->c.width_bk;
			di_buf->vframe->private_data = di_buf;

			di_buf->vframe->early_process_fun = do_pre_only_fun;

			dimv3_print("%s:2\n", __func__);
			if (ready_di_buf->c.post_proc_flag == -2) {
				di_buf->vframe->type
					|= VIDTYPE_VIU_FIELD;
				di_buf->vframe->type
					&= ~(VIDTYPE_TYPEMASK);
				di_buf->vframe->process_fun
				= (dimp_get(eDI_MP_post_wr_en) &&
				   dimp_get(eDI_MP_post_wr_support)) ? NULL :
				dimv3_post_process;
				di_buf->c.process_fun_index = PROCESS_FUN_DI;
				di_buf->c.pd_config.global_mode = PULL_DOWN_EI;
			} else {
				di_buf->vframe->process_fun = NULL;
				di_buf->c.process_fun_index = PROCESS_FUN_NULL;
				di_buf->c.pd_config.global_mode =
					PULL_DOWN_NORMAL;
			}
			di_buf->c.di_buf[0] = ready_di_buf;
			di_buf->c.di_buf[1] = NULL;
			queuev3_out(channel, ready_di_buf);

			drop_frame(check_drop,
				   di_buf->c.di_buf[0]->c.throw_flag,
				   di_buf, channel);

			frame_count++;
#ifdef DI_BUFFER_DEBUG
			dimv3_print("%s <prog by frame>: ", __func__);
#endif
			ret = 1;
		#if 0
			pwv3_vf_notify_receiver(channel,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
		#endif
		}
	} else if (ready_count >= 2) {
		/*for progressive input,
		 * type 1: separate two fields,
		 * type 2: bypass post as frame
		 */
		unsigned char prog_tb_field_proc_type =
			(dimp_get(eDI_MP_prog_proc_config) >> 1) & 0x3;
		di_lock_irqfiq_save(irq_flag2);

		di_buf = div3_que_out_to_di_buf(channel, QUE_POST_FREE);
		if (dimv3_check_di_buf(di_buf, 20, channel)) {
			di_unlock_irqfiq_restore(irq_flag2);
			return 0;
		}

		di_unlock_irqfiq_restore(irq_flag2);

		i = 0;

		div3_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

		for (itmp = 0; itmp < psize; itmp++) {
			p = pwv3_qindex_2_buf(channel, tmpa[itmp]);
			di_buf->c.di_buf_dup_p[i++] = p;
			if (i >= 2) {
				di_buf->c.di_buf_dup_p[i] = NULL;
				break;
			}
		}
		if (i < 2) {
			PR_ERR("%s:Error6\n", __func__);

			if (recovery_flag == 0)
				recovery_log_reason = 21;

			recovery_flag++;
			return 0;
		}

		memcpy(di_buf->vframe,
		       di_buf->c.di_buf_dup_p[0]->vframe,
		       sizeof(vframe_t));
		di_buf->vframe->private_data = di_buf;

		/*separate one progressive frame
		 * as two interlace fields
		 */
		if (prog_tb_field_proc_type == 1) {
			/* do weave by di post */
			di_buf->vframe->type = VIDTYPE_PROGRESSIVE	|
						VIDTYPE_VIU_422		|
						VIDTYPE_VIU_SINGLE_PLANE |
						VIDTYPE_VIU_FIELD	|
						VIDTYPE_PRE_INTERLACE;
			if (di_buf->c.di_buf_dup_p[0]->c.new_format_flag)
				di_buf->vframe->early_process_fun =
					de_post_disable_fun;
			else
				di_buf->vframe->early_process_fun =
					do_nothing_fun;

			di_buf->c.pd_config.global_mode = PULL_DOWN_BUF1;
			di_buf->vframe->process_fun =
				(dimp_get(eDI_MP_post_wr_en) &&
				 dimp_get(eDI_MP_post_wr_support)) ? NULL :
				 dimv3_post_process;
			di_buf->c.process_fun_index = PROCESS_FUN_DI;
		} else if (prog_tb_field_proc_type == 0) {
			/* to do: need change for
			 * DI_USE_FIXED_CANVAS_IDX
			 */
			/* do weave by vpp */
			di_buf->vframe->type = VIDTYPE_PROGRESSIVE	|
						VIDTYPE_VIU_422		|
						VIDTYPE_VIU_SINGLE_PLANE;
			if ((di_buf->c.di_buf_dup_p[0]->c.new_format_flag) ||
				(Rd(DI_IF1_GEN_REG) & 1))
				di_buf->vframe->early_process_fun =
					de_post_disable_fun;
			else
				di_buf->vframe->early_process_fun =
					do_nothing_fun;
			di_buf->vframe->process_fun = NULL;
			di_buf->c.process_fun_index = PROCESS_FUN_NULL;
			di_buf->vframe->canvas0Addr =
				di_buf->c.di_buf_dup_p[0]->nr_canvas_idx;
			di_buf->vframe->canvas1Addr =
				di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
		} else {
			/* to do: need change for
			 * DI_USE_FIXED_CANVAS_IDX
			 */
			di_buf->vframe->type = VIDTYPE_PROGRESSIVE	|
						VIDTYPE_VIU_422		|
						VIDTYPE_VIU_SINGLE_PLANE |
						VIDTYPE_VIU_FIELD	|
						VIDTYPE_PRE_INTERLACE;
			di_buf->vframe->height >>= 1;

			di_buf->vframe->width
				= di_buf->c.di_buf_dup_p[0]->c.width_bk;
			if ((di_buf->c.di_buf_dup_p[0]->c.new_format_flag) ||
			    (Rd(DI_IF1_GEN_REG) & 1))
				di_buf->vframe->early_process_fun =
					de_post_disable_fun;
			else
				di_buf->vframe->early_process_fun =
					do_nothing_fun;
			if (prog_tb_field_proc_type == 2) {
				di_buf->vframe->canvas0Addr =
				di_buf->c.di_buf_dup_p[0]->nr_canvas_idx;
	/* top */
				di_buf->vframe->canvas1Addr =
				di_buf->c.di_buf_dup_p[0]->nr_canvas_idx;
			} else {
				/* top */
				di_buf->vframe->canvas0Addr =
				di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
				di_buf->vframe->canvas1Addr =
				di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
			}
		}

		di_buf->c.di_buf[0] = di_buf->c.di_buf_dup_p[0];
		queuev3_out(channel, di_buf->c.di_buf[0]);
		/*check if the field is error,then drop*/
		if ((di_buf->c.di_buf_dup_p[0]->vframe->type &
		     VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_BOTTOM) {
			di_buf->c.di_buf[1] = di_buf->c.di_buf_dup_p[1] = NULL;
			queuev3_in(channel, di_buf, QUEUE_TMP);
			recycle_vframe_type_post(di_buf, channel);
			pr_dbg("%s drop field %d.\n", __func__,
			       di_buf->c.di_buf_dup_p[0]->c.seq);
		} else {
			di_buf->c.di_buf[1] = di_buf->c.di_buf_dup_p[1];
			queuev3_out(channel, di_buf->c.di_buf[1]);

			drop_frame(dimp_get(eDI_MP_check_start_drop_prog),
				   (di_buf->c.di_buf_dup_p[0]->c.throw_flag) ||
				   (di_buf->c.di_buf_dup_p[1]->c.throw_flag),
				   di_buf, channel);
		}
		frame_count++;
#ifdef DI_BUFFER_DEBUG
		dimv3_print("%s <prog by field>: ", __func__);
#endif
		ret = 1;
#if 0
		pwv3_vf_notify_receiver(channel,
			VFRAME_EVENT_PROVIDER_VFRAME_READY,
			NULL);
#endif
	}
	#endif
	return ret;
}
/***********************************************
 * from dim_process_post_vframe_b
 **********************************************/
static int dim_pst_vframe_p_used2ibuf(unsigned int channel,
				      struct di_buf_s *ready_di_buf)
{
	ulong irq_flag2 = 0;
	int i = 0;
	int ret = 0;
//	int buffer_keep_count = 3;
	struct di_buf_s *di_buf = NULL;
	//struct di_buf_s *ready_di_buf;
	struct di_buf_s *p = NULL;
	int itmp;
	int ready_count = div3_que_list_count(channel, QUE_PRE_READY);
	bool check_drop = false;
	unsigned int tmpa[MAX_FIFO_SIZE]; /*new que*/
	unsigned int psize; /*new que*/
	struct di_buf_s *di_buf_i;

	/*p			*/
	/*bypass_all mode	*/
	/*??			*/
	/*bypass post mode ?	*/

	int vframe_process_count = 1;

	if (ready_count < vframe_process_count)
		return 0;

	di_lock_irqfiq_save(irq_flag2);
	/*get post free*/
	di_buf = div3_que_out_to_di_buf(channel, QUE_POST_FREE);
	if (dimv3_check_di_buf(di_buf, 19, channel)) {
		di_unlock_irqfiq_restore(irq_flag2);
		return 0;
	}

	di_unlock_irqfiq_restore(irq_flag2);

	i = 0;
	itmp = 0;
	dimv3_print("%s\n", __func__);
	div3_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);
	/*set post buf c.di_buf_dup_p[0] = ready*/
	#ifdef HIS_V3
	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(channel, tmpa[itmp]);
		di_buf->c.di_buf_dup_p[i++] = p;
		if (i >= vframe_process_count) {
			di_buf->c.di_buf_dup_p[i]	= NULL;
			di_buf->c.di_buf_dup_p[i + 1] = NULL;
			break;
		}
	}
	#else
	p = pwv3_qindex_2_buf(channel, tmpa[itmp]);
	di_buf->c.di_buf_dup_p[i++] = p;
	if (i >= vframe_process_count) {
		di_buf->c.di_buf_dup_p[i]	= NULL;
		di_buf->c.di_buf_dup_p[i + 1] = NULL;
		//break;
	}
	#endif
	if (i < vframe_process_count) {
		PR_ERR("%s:6\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 22;

		recovery_flag++;
		return 0;
	}

	/*di_buf_i is pre_ready*/
	di_buf_i = di_buf->c.di_buf_dup_p[0];

	#if 0	/*remove vframe 12-03*/
	/*pre_ready to post*/
	memcpy(di_buf->vframe, di_buf_i->vframe,
	       sizeof(vframe_t));

	di_buf->vframe->width = di_buf_i->c.width_bk;
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->early_process_fun = do_pre_only_fun;
	#endif
	dimv3_print("%s:2\n", __func__);
	if (ready_di_buf->c.post_proc_flag == -2) { /*?*/
		#if 0	/*remove vframe 12-03*/
		di_buf->vframe->type |= VIDTYPE_VIU_FIELD;
		di_buf->vframe->type &= ~(VIDTYPE_TYPEMASK);
		di_buf->vframe->process_fun
		= (dimp_get(eDI_MP_post_wr_en) &&
		   dimp_get(eDI_MP_post_wr_support)) ? NULL :
		dimv3_post_process;
		#endif
		di_buf->c.process_fun_index = PROCESS_FUN_DI;
		di_buf->c.pd_config.global_mode = PULL_DOWN_EI;
	} else {
		//di_buf->vframe->process_fun = NULL;/*remove vframe 12-03*/
		di_buf->c.process_fun_index = PROCESS_FUN_NULL;
		di_buf->c.pd_config.global_mode = PULL_DOWN_NORMAL;
	}
	di_buf->c.di_buf[0] = ready_di_buf;
	di_buf->c.di_buf[1] = NULL;
	di_buf->c.wmode		= ready_di_buf->c.wmode;
	di_buf->c.vmode		= ready_di_buf->c.vmode;
	di_buf->c.pdvfm		= ready_di_buf->c.pdvfm; /*2019-12-03*/
	queuev3_out(channel, ready_di_buf);
	ready_di_buf->c.sts |= EDI_ST_VFM_P_U2I;
	if (ready_di_buf->c.di_wr_linked_buf)
		ready_di_buf->c.di_wr_linked_buf->c.sts |= EDI_ST_VFM_P_U2I;

	drop_frame(check_drop, /*false*/
		   di_buf->c.di_buf[0]->c.throw_flag,
		   di_buf, channel);

	frame_count++;
#ifdef DI_BUFFER_DEBUG
	dimv3_print("%s <prog by frame>: ", __func__);
#endif
	ret = 1;

	return ret;
}

/***********************************************
 * from dim_process_post_vframe_b
 **********************************************/
static int dim_pst_vframe_p_as_i(unsigned int channel,
				 struct di_buf_s *ready_di_buf)
{
	ulong irq_flag2 = 0;
	int i = 0;
	int ret = 0;
//	int buffer_keep_count = 3;
	struct di_buf_s *di_buf = NULL;
	//struct di_buf_s *ready_di_buf;
	struct di_buf_s *p = NULL;
	int itmp;
	int ready_count = div3_que_list_count(channel, QUE_PRE_READY);
	bool check_drop = false;
	unsigned int tmpa[MAX_FIFO_SIZE]; /*new que*/
	unsigned int psize; /*new que*/
	unsigned char prog_tb_field_proc_type =
			(dimp_get(eDI_MP_prog_proc_config) >> 1) & 0x3;


	if (ready_count < 2)
		return 0;

	/*for progressive input,
	 * type 1: separate two fields,
	 * type 2: bypass post as frame
	 */

	di_lock_irqfiq_save(irq_flag2);

	/*di_buf: post free*/
	di_buf = div3_que_out_to_di_buf(channel, QUE_POST_FREE);
	if (dimv3_check_di_buf(di_buf, 20, channel)) {
		di_unlock_irqfiq_restore(irq_flag2);
		return 0;
	}

	di_unlock_irqfiq_restore(irq_flag2);

	dimv3_print("%s\n", __func__);
	i = 0;
	/*set post di_buf->c.di_buf_dup_p[0],[1] as pre ready*/
	div3_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);
	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(channel, tmpa[itmp]);
		di_buf->c.di_buf_dup_p[i++] = p;
		if (i >= 2) {
			di_buf->c.di_buf_dup_p[i] = NULL;
			break;
		}
	}
	if (i < 2) {
		PR_ERR("%s:Error6\n", __func__);

		if (recovery_flag == 0)
			recovery_log_reason = 21;

		recovery_flag++;
		return 0;
	}

	#if 0	/*remove vframe 12-03*/
	/*set post buf as top */
	memcpy(di_buf->vframe,
	       di_buf->c.di_buf_dup_p[0]->vframe,
	       sizeof(vframe_t));
	di_buf->vframe->private_data = di_buf;
	#endif
	/*separate one progressive frame
	 * as two interlace fields
	 */
	if (prog_tb_field_proc_type == 1) {
		/* do weave by di post */
		#if 0	/*remove vframe 12-03*/
		di_buf->vframe->type = VIDTYPE_PROGRESSIVE		|
					VIDTYPE_VIU_422			|
					VIDTYPE_VIU_SINGLE_PLANE	|
					VIDTYPE_VIU_FIELD		|
					VIDTYPE_PRE_INTERLACE;
		if (di_buf->c.di_buf_dup_p[0]->c.new_format_flag)
			di_buf->vframe->early_process_fun = de_post_disable_fun;
		else
			di_buf->vframe->early_process_fun = do_nothing_fun;
		#endif
		di_buf->c.pd_config.global_mode = PULL_DOWN_BUF1;
		#if 0	/*remove vframe 12-03*/
		di_buf->vframe->process_fun =
			(dimp_get(eDI_MP_post_wr_en) &&
			 dimp_get(eDI_MP_post_wr_support)) ? NULL :
			 dimv3_post_process;
		#endif
		di_buf->c.process_fun_index = PROCESS_FUN_DI;
		/*2019-12-03*/
		di_buf->c.pdvfm = di_buf->c.di_buf_dup_p[0]->c.pdvfm;
	} else if (prog_tb_field_proc_type == 0) {
		/* to do: need change for
		 * DI_USE_FIXED_CANVAS_IDX
		 */
		/* do weave by vpp */
		PR_ERR("%s:type?\n", __func__);
		#if 0
		di_buf->vframe->type = VIDTYPE_PROGRESSIVE	|
					VIDTYPE_VIU_422		|
					VIDTYPE_VIU_SINGLE_PLANE;
		if ((di_buf->c.di_buf_dup_p[0]->c.new_format_flag) ||
			(Rd(DI_IF1_GEN_REG) & 1))
			di_buf->vframe->early_process_fun =
				de_post_disable_fun;
		else
			di_buf->vframe->early_process_fun =
				do_nothing_fun;
		di_buf->vframe->process_fun = NULL;
		di_buf->c.process_fun_index = PROCESS_FUN_NULL;
		di_buf->vframe->canvas0Addr =
			di_buf->c.di_buf_dup_p[0]->nr_canvas_idx;
		di_buf->vframe->canvas1Addr =
			di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
		#endif
	} else {
		/* to do: need change for
		 * DI_USE_FIXED_CANVAS_IDX
		 */
		PR_ERR("%s:type2\n", __func__);
		#if 0
		di_buf->vframe->type = VIDTYPE_PROGRESSIVE	|
					VIDTYPE_VIU_422		|
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD	|
					VIDTYPE_PRE_INTERLACE;
		di_buf->vframe->height >>= 1;

		di_buf->vframe->width
			= di_buf->c.di_buf_dup_p[0]->c.width_bk;
		if ((di_buf->c.di_buf_dup_p[0]->c.new_format_flag) ||
		    (Rd(DI_IF1_GEN_REG) & 1))
			di_buf->vframe->early_process_fun =
				de_post_disable_fun;
		else
			di_buf->vframe->early_process_fun =
				do_nothing_fun;
		if (prog_tb_field_proc_type == 2) {
			di_buf->vframe->canvas0Addr =
			di_buf->c.di_buf_dup_p[0]->nr_canvas_idx;
/* top */
			di_buf->vframe->canvas1Addr =
			di_buf->c.di_buf_dup_p[0]->nr_canvas_idx;
		} else {
			/* top */
			di_buf->vframe->canvas0Addr =
			di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
			di_buf->vframe->canvas1Addr =
			di_buf->c.di_buf_dup_p[1]->nr_canvas_idx;
		}
		#endif
	}

	di_buf->c.di_buf[0]	= di_buf->c.di_buf_dup_p[0];
	di_buf->c.wmode		= ready_di_buf->c.wmode;
	di_buf->c.vmode		= di_buf->c.di_buf_dup_p[0]->c.vmode;
	queuev3_out(channel, di_buf->c.di_buf[0]);
	di_buf->c.di_buf[0]->c.sts |= EDI_ST_VFM_P_ASI_T;
	/*check if the field is error,then drop*/
	if (!(di_buf->c.di_buf_dup_p[0]->c.wmode.is_top)) {
		di_buf->c.di_buf[1] = di_buf->c.di_buf_dup_p[1] = NULL;
		queuev3_in(channel, di_buf, QUEUE_TMP);
		recycle_vframe_type_post(di_buf, channel);
		PR_INF("%s drop field %d.\n", __func__,
		       di_buf->c.di_buf_dup_p[0]->c.seq);
	} else {
		di_buf->c.di_buf[1] = di_buf->c.di_buf_dup_p[1];
		queuev3_out(channel, di_buf->c.di_buf[1]);
		di_buf->c.di_buf[1]->c.sts |= EDI_ST_VFM_P_ASI_B;
		drop_frame(check_drop/*dimp_get(eDI_MP_check_start_drop_prog)*/,
			   (di_buf->c.di_buf_dup_p[0]->c.throw_flag) ||
			   (di_buf->c.di_buf_dup_p[1]->c.throw_flag),
			   di_buf, channel);
	}
	frame_count++;
#ifdef DI_BUFFER_DEBUG
	dimv3_print("%s <prog by field>: ", __func__);
#endif
	ret = 1;

	return ret;
}

static int dim_pst_vframe_c_bypass_all(unsigned int ch,
				       struct di_buf_s *ready_di_buf)
{
	struct di_buf_s *di_buf = NULL;
	struct di_ch_s *pch;
	struct dim_inter_s *pintf;

	pch = get_chdata(ch);
	if (!pch) {
		PR_ERR("%s:no pch\n", __func__);
		return 0;
	}

	pintf = &pch->interf;

	/*post free*/
	di_buf = div3_que_out_to_di_buf(ch, QUE_POST_FREE);
	if (dimv3_check_di_buf(di_buf, 19, ch))
		return 0;

	di_buf->c.di_buf_dup_p[0] = ready_di_buf;
	//di_buf_i = di_buf->di_buf_dup_p[0];
	#if 0
	memcpy(di_buf->vframe, ready_di_buf->vframe,
	       sizeof(vframe_t));

	//di_buf->vframe->width = ready_di_buf->c.width_bk;
	di_buf->vframe->private_data = di_buf;

	if (ready_di_buf->c.new_format_flag) {
		pr_info("DI:ch[%d],%d disable post.\n",
			ch,
			__LINE__);
		di_buf->vframe->early_process_fun = de_post_disable_fun;
	} else {
		di_buf->vframe->early_process_fun = do_nothing_fun;
	}
	#endif
	dimv3_print("%s %p,%d,t[%d]\n", __func__, di_buf,
		  di_buf->index, di_buf->type);
	di_buf->vframe->process_fun = NULL;
	di_buf->c.process_fun_index = PROCESS_FUN_NULL; /*need*/
	di_buf->c.pd_config.global_mode = PULL_DOWN_NORMAL;/*no use*/
	di_buf->c.di_buf[0] = ready_di_buf;
	di_buf->c.di_buf[1] = NULL;
	di_buf->c.wmode = ready_di_buf->c.wmode;
	di_buf->c.pdvfm = ready_di_buf->c.pdvfm;
	/*input buf from pre_ready out*/
	queuev3_out(ch, ready_di_buf);


	/*post buf to doing*/
	if (dimp_get(eDI_MP_post_wr_en) &&
	    dimp_get(eDI_MP_post_wr_support)) {
		//PR_INF("%s:\n", __func__);
		//queue_in(ch, di_buf, QUEUE_POST_DOING);
		//di_que_in(ch, QUE_POST_DOING, di_buf);

		if (dimv3_tmode_is_localpost(ch))
			div3_que_in(ch, QUE_POST_DOING, di_buf);
		else
			div3_que_in(ch, QUE_POST_NOBUF, di_buf);

	}

	if (pintf->op_ins_2_doing)
		pintf->op_ins_2_doing(pch, 1, di_buf);
	dimv3_tr_ops.post_do(di_buf->c.vmode.omx_index);

	return 1;
}

/* */
int dimv3_pst_vframe_top(unsigned int ch)
{
	int ready_count = div3_que_list_count(ch, QUE_PRE_READY);
	struct di_buf_s *ready_di_buf;
	int buffer_keep_count = 3;
	struct di_buf_s *p[3] = {NULL};
	int itmp;
	unsigned int tmpa[MAX_FIFO_SIZE]; /*new que*/
	unsigned int psize; /*new que*/
//	int i = 0;
	int ret = 0;
	struct di_ch_s *pch;
	struct di_buffer *ins_buf;

	pch = get_chdata(ch);
	if (!pch)
		return 0;

	if (div3_que_is_empty(ch, QUE_POST_FREE))
		return 0;
	/*add : for now post buf only 3.*/
	//if (list_count(ch, QUEUE_POST_DOING) > 2)

	if (div3_que_list_count(ch, QUE_POST_DOING) > 2)
		return 0;

	if (ready_count == 0)
		return 0;

	ready_di_buf = div3_que_peek(ch, QUE_PRE_READY);
	if (!ready_di_buf) {
		pr_dbg("%s:Error1\n", __func__);

		if (recovery_flag == 0)
			recovery_log_reason = 16;

		recovery_flag++;
		return 0;
	}
	/*******************************************/
	if (ready_count >= buffer_keep_count	&&
	    ready_di_buf->c.wmode.is_i		&&
	    !ready_di_buf->c.wmode.is_bypass	&&
	    !ready_di_buf->c.wmode.need_bypass) {

		div3_que_list(ch, QUE_PRE_READY, &tmpa[0], &psize);
		for (itmp = 0; itmp < buffer_keep_count; itmp++)
			p[itmp] = pwv3_qindex_2_buf(ch, tmpa[itmp]);

		if (p[2]->c.wmode.is_bypass	||
		    p[1]->c.wmode.is_bypass	||
		    !p[1]->c.wmode.is_i) {
			/**/
			p[1]->c.wmode.is_bypass = 1;

			/*********************/
			if (p[1]->c.pdvfm && p[1]->c.pdvfm->vfm_in) {
				ins_buf =
				(struct di_buffer *)p[1]->c.pdvfm->vfm_in;
				ins_buf->flag |= DI_FLAG_BUF_BY_PASS;
				#if 0
				/*dec vf keep*/
				if (p[1]->c.in_buf) {
					queuev3_in(ch, p[1]->c.in_buf,
						 QUEUE_RECYCLE);
					p[1]->c.in_buf = NULL;
					dimv3_print("%s:recy dec vf;%d,t[%d]\n",
						  __func__,
						  p[1]->index, p[1]->type);
				} else {
					PR_INF("%s:p:%d,t[%d]\n", __func__,
					p[1]->index, p[1]->type);
				}
				#endif
				PR_INF("%s:%d, to bypass\n", __func__,
				       ins_buf->mng.index);
			} else {
				PR_ERR("%s bypass no input?\n", __func__);
			}
			//p[0]->c.wmode.is_bypass = 1;
			/*drop p[0] dummy*/
			queuev3_out(ch, p[0]);

			/*dec vf keep*/
			if (p[0]->c.in_buf) {
				queuev3_in(ch, p[0]->c.in_buf, QUEUE_RECYCLE);
				p[0]->c.in_buf = NULL;
				dimv3_print("%s:recydecvf;%d,t[%d]\n", __func__,
					    p[0]->index, p[0]->type);
			} else {
				PR_INF("%s:p:%d,t[%d]\n", __func__,
				       p[0]->index, p[0]->type);
			}
			queuev3_in(ch, p[0], QUEUE_RECYCLE);
			/*p1 to bypass*/
			return 1;
		}
	}
	#if 0
	/*******************************************/
	if ((ready_di_buf->c.post_proc_flag) &&
	    (ready_count >= buffer_keep_count)) {
		i = 0;

		div3_que_list(ch, QUE_PRE_READY, &tmpa[0], &psize);
		for (itmp = 0; itmp < psize; itmp++) { /*ary:for what?*/
			p[0] = pwv3_qindex_2_buf(ch, tmpa[itmp]);
			/* if(p->post_proc_flag == 0){ */
			if (p[0]->type == VFRAME_TYPE_IN) {
				ready_di_buf->c.post_proc_flag = -1;
				ready_di_buf->c.new_format_flag = 1;
			}
			i++;
			if (i > 2)
				break;
		}
	}
	#endif
	if (ready_di_buf->c.wmode.is_bypass) {
		dbg_dbg("%s:i[%d],%d\n", __func__,
			ready_di_buf->c.pdvfm->index,
			ready_di_buf->c.wmode.is_bypass);
		ret = dim_pst_vframe_c_bypass_all(ch, ready_di_buf);
	} else if (ready_di_buf->c.wmode.p_use_2i) {
		ret = dim_pst_vframe_p_used2ibuf(ch, ready_di_buf);
	} else if (ready_di_buf->c.wmode.p_as_i) {
		ret = dim_pst_vframe_p_as_i(ch, ready_di_buf);
	} else if (ready_di_buf->c.post_proc_flag > 0) {
		if (ready_count >= buffer_keep_count) {
			if (ready_di_buf->c.wmode.is_i)
				ret = dim_pst_vframe_i(ch, ready_di_buf);
			else
				ret = dim_pst_vframe_a(ch, ready_di_buf);
		}

	} else if (!ready_di_buf->c.post_proc_flag &&
		   ready_di_buf->type == VFRAME_TYPE_IN) {
		/*bypass mode*/
		ret = dim_pst_vframe_c_bypass_all(ch, ready_di_buf);
	} else {
		dimv3_print("vfm_top other\n");
		ret = dim_pst_vframe_b(ch, ready_di_buf);
	}

	return ret;
}

int dimv3_process_post_vframe(unsigned int channel)
{

	#if 0
/*
 * 1) get buf from post_free_list, config it according to buf
 * in pre_ready_list, send it to post_ready_list
 * (it will be send to post_free_list in di_vf_put())
 * 2) get buf from pre_ready_list, attach it to buf from post_free_list
 * (it will be send to recycle_list in di_vf_put() )
 */
	ulong irq_flag2 = 0;
	int i = 0;
	int ret = 0;
	int buffer_keep_count = 3;
	struct di_buf_s *di_buf = NULL;
	struct di_buf_s *ready_di_buf;
	struct di_buf_s *p = NULL;/* , *ptmp; */
	int itmp;
	/* new que int ready_count = list_count(channel, QUEUE_PRE_READY);*/
	int ready_count = div3_que_list_count(channel, QUE_PRE_READY);
	bool check_drop = false;
	unsigned int tmpa[MAX_FIFO_SIZE]; /*new que*/
	unsigned int psize; /*new que*/

#if 1
	if (div3_que_is_empty(channel, QUE_POST_FREE))
		return 0;
	/*add : for now post buf only 3.*/
	//if (list_count(channel, QUEUE_POST_DOING) > 2)
	if (div3_que_list_count(channel, QUE_POST_DOING) > 2)
		return 0;
#else
	/*for post write mode ,need reserved a post free buf;*/
	if (div3_que_list_count(channel, QUE_POST_FREE) < 2)
		return 0;
#endif
	if (ready_count == 0)
		return 0;

	ready_di_buf = div3_que_peek(channel, QUE_PRE_READY);
	if (!ready_di_buf || !ready_di_buf->vframe) {
		pr_dbg("%s:Error1\n", __func__);

		if (recovery_flag == 0)
			recovery_log_reason = 16;

		recovery_flag++;
		return 0;
	}
	dimv3_print("%s:1 ready_count[%d]:post_proc_flag[%d]\n", __func__,
		  ready_count, ready_di_buf->c.post_proc_flag);
	if ((ready_di_buf->c.post_proc_flag) &&
	    (ready_count >= buffer_keep_count)) {
		i = 0;

		div3_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);
		for (itmp = 0; itmp < psize; itmp++) {
			p = pwv3_qindex_2_buf(channel, tmpa[itmp]);
			/* if(p->post_proc_flag == 0){ */
			if (p->type == VFRAME_TYPE_IN) {
				ready_di_buf->c.post_proc_flag = -1;
				ready_di_buf->c.new_format_flag = 1;
			}
			i++;
			if (i > 2)
				break;
		}
	}
	if (ready_di_buf->c.post_proc_flag > 0) {
		if (ready_count >= buffer_keep_count) {
			di_lock_irqfiq_save(irq_flag2);

			di_buf = div3_que_out_to_di_buf(channel, QUE_POST_FREE);
			if (dimv3_check_di_buf(di_buf, 17, channel)) {
				di_unlock_irqfiq_restore(irq_flag2);
				return 0;
			}

			di_unlock_irqfiq_restore(irq_flag2);

			i = 0;

			div3_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

			for (itmp = 0; itmp < psize; itmp++) {
				p = pwv3_qindex_2_buf(channel, tmpa[itmp]);
				dimv3_print("di:keep[%d]:t[%d]:idx[%d]\n",
					  i, tmpa[itmp], p->index);
				di_buf->c.di_buf_dup_p[i++] = p;

				if (i >= buffer_keep_count)
					break;
			}
			if (i < buffer_keep_count) {
				PR_ERR("%s:3\n", __func__);

				if (recovery_flag == 0)
					recovery_log_reason = 18;
				recovery_flag++;
				return 0;
			}

			memcpy(di_buf->vframe,
			       di_buf->c.di_buf_dup_p[1]->vframe,
			       sizeof(vframe_t));
			di_buf->vframe->private_data = di_buf;
			if (di_buf->c.di_buf_dup_p[1]->c.post_proc_flag == 3) {
				/* dummy, not for display */
				inc_post_ref_count(di_buf);
				di_buf->c.di_buf[0] = di_buf->c.di_buf_dup_p[0];
				di_buf->c.di_buf[1] = NULL;
				queuev3_out(channel, di_buf->c.di_buf[0]);
				di_lock_irqfiq_save(irq_flag2);
				queuev3_in(channel, di_buf, QUEUE_TMP);
				recycle_vframe_type_post(di_buf, channel);

				di_unlock_irqfiq_restore(irq_flag2);
				dimv3_print("%s <dummy>: ", __func__);
#ifdef DI_BUFFER_DEBUG
				dimv3_print("%s <dummy>: ", __func__);
#endif
			} else {
				if (di_buf->c.di_buf_dup_p[1]->c.post_proc_flag
						== 2) {
					di_buf->c.pd_config.global_mode
						= PULL_DOWN_BLEND_2;
					/* blend with di_buf->di_buf_dup_p[2] */
				} else {
					set_pulldown_mode(di_buf, channel);
				}
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;

			di_buf->vframe->width
				= di_buf->c.di_buf_dup_p[1]->c.width_bk;

			if (di_buf->c.di_buf_dup_p[1]->c.new_format_flag) {
				/* if (di_buf->di_buf_dup_p[1]
				 * ->post_proc_flag == 2) {
				 */
				di_buf->vframe->early_process_fun =
						de_post_disable_fun;
			} else {
				di_buf->vframe->early_process_fun =
							do_nothing_fun;
			}

			if (di_buf->c.di_buf_dup_p[1]->type == VFRAME_TYPE_IN) {
				/* next will be bypass */
				di_buf->vframe->type
					= VIDTYPE_PROGRESSIVE |
					  VIDTYPE_VIU_422 |
					  VIDTYPE_VIU_SINGLE_PLANE |
						VIDTYPE_VIU_FIELD |
						VIDTYPE_PRE_INTERLACE;
				di_buf->vframe->height >>= 1;
				di_buf->vframe->canvas0Addr =
					di_buf->c.di_buf_dup_p[0]
					->nr_canvas_idx; /* top */
				di_buf->vframe->canvas1Addr =
					di_buf->c.di_buf_dup_p[0]
					->nr_canvas_idx;
				di_buf->vframe->process_fun =
					NULL;
				di_buf->c.process_fun_index = PROCESS_FUN_NULL;
			} else {
				/*for debug*/
				if (dimp_get(eDI_MP_debug_blend_mode) != -1)
					di_buf->c.pd_config.global_mode
					= dimp_get(eDI_MP_debug_blend_mode);

				di_buf->vframe->process_fun =
((dimp_get(eDI_MP_post_wr_en) && dimp_get(eDI_MP_post_wr_support)) ?
				NULL : dimv3_post_process);
				di_buf->c.process_fun_index = PROCESS_FUN_DI;
					inc_post_ref_count(di_buf);
				}
				di_buf->c.di_buf[0]	/*ary:di_buf_di_buf*/
					= di_buf->c.di_buf_dup_p[0];
				di_buf->c.di_buf[1] = NULL;
				queuev3_out(channel, di_buf->c.di_buf[0]);

				drop_frame(true,
				(di_buf->c.di_buf_dup_p[0]->c.throw_flag) ||
				(di_buf->c.di_buf_dup_p[1]->c.throw_flag) ||
				(di_buf->c.di_buf_dup_p[2]->c.throw_flag),
				di_buf, channel);

				frame_count++;
#ifdef DI_BUFFER_DEBUG
				dimv3_print("%s <interlace>: ", __func__);
#endif
				if (!(dimp_get(eDI_MP_post_wr_en) &&
				      dimp_get(eDI_MP_post_wr_support)))
					pwv3_vf_notify_receiver(channel,
VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
			}
			ret = 1;
		}
	} else {
		if (is_progressive(ready_di_buf->vframe) ||
		    ready_di_buf->type == VFRAME_TYPE_IN ||
		    ready_di_buf->c.post_proc_flag < 0 ||
		    dimp_get(eDI_MP_bypass_post_state)
		    ){
			int vframe_process_count = 1;
#ifdef DET3D
			int dual_vframe_flag = 0;

			if ((ppre->vframe_interleave_flag &&
			     ready_di_buf->c.left_right) ||
			    (dimp_get(eDI_MP_bypass_post) & 0x100)) {
				dual_vframe_flag = 1;
				vframe_process_count = 2;
			}
#endif
			if (dimp_get(eDI_MP_skip_top_bot) &&
			    (!is_progressive(ready_di_buf->vframe)))
				vframe_process_count = 2;

			if (ready_count >= vframe_process_count) {
				struct di_buf_s *di_buf_i;

				di_lock_irqfiq_save(irq_flag2);

		di_buf = div3_que_out_to_di_buf(channel, QUE_POST_FREE);
				if (dimv3_check_di_buf(di_buf, 19, channel)) {
					di_unlock_irqfiq_restore(irq_flag2);
					return 0;
				}

				di_unlock_irqfiq_restore(irq_flag2);

				i = 0;

		div3_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

				for (itmp = 0; itmp < psize; itmp++) {
					p = pwv3_qindex_2_buf(channel,
						tmpa[itmp]);
					di_buf->c.di_buf_dup_p[i++] = p;
					if (i >= vframe_process_count) {
						di_buf->c.di_buf_dup_p[i]
							= NULL;
						di_buf->c.di_buf_dup_p[i + 1]
							= NULL;
						break;
					}
				}
				if (i < vframe_process_count) {
					PR_ERR("%s:6\n", __func__);
					if (recovery_flag == 0)
						recovery_log_reason = 22;

					recovery_flag++;
					return 0;
				}

				di_buf_i = di_buf->c.di_buf_dup_p[0];
				if (!is_progressive(ready_di_buf->vframe) &&
				    ((dimp_get(eDI_MP_skip_top_bot) == 1) ||
				    (dimp_get(eDI_MP_skip_top_bot) == 2))) {
					unsigned int frame_type =
						di_buf->c.di_buf_dup_p[1]->
						vframe->type &
						VIDTYPE_TYPEMASK;
					if (dimp_get(eDI_MP_skip_top_bot)
						== 1) {
						di_buf_i = (frame_type ==
						VIDTYPE_INTERLACE_TOP)
						? di_buf->c.di_buf_dup_p[1]
						: di_buf->c.di_buf_dup_p[0];
					} else if (
						dimp_get(eDI_MP_skip_top_bot)
						== 2) {
						di_buf_i = (frame_type ==
						VIDTYPE_INTERLACE_BOTTOM)
						? di_buf->c.di_buf_dup_p[1]
						: di_buf->c.di_buf_dup_p[0];
					}
				}

				memcpy(di_buf->vframe, di_buf_i->vframe,
				       sizeof(vframe_t));

				di_buf->vframe->width = di_buf_i->c.width_bk;
				di_buf->vframe->private_data = di_buf;

				if (ready_di_buf->c.new_format_flag &&
				    (ready_di_buf->type == VFRAME_TYPE_IN)) {
					PR_INF("ch[%d],%d disable post.\n",
					       channel,
					       __LINE__);
					di_buf->vframe->early_process_fun
						= de_post_disable_fun;
				} else {
					if (ready_di_buf->type ==
						VFRAME_TYPE_IN)
						di_buf->vframe->
						early_process_fun
						 = do_nothing_fun;

					else
						di_buf->vframe->
						early_process_fun
						 = do_pre_only_fun;
				}
				dimv3_print("%s:2\n", __func__);
				if (ready_di_buf->c.post_proc_flag == -2) {
					di_buf->vframe->type
						|= VIDTYPE_VIU_FIELD;
					di_buf->vframe->type
						&= ~(VIDTYPE_TYPEMASK);
					di_buf->vframe->process_fun
= (dimp_get(eDI_MP_post_wr_en) && dimp_get(eDI_MP_post_wr_support)) ? NULL :
					dimv3_post_process;
					di_buf->c.process_fun_index =
						PROCESS_FUN_DI;
					di_buf->c.pd_config.global_mode
						= PULL_DOWN_EI;
				} else {
					di_buf->vframe->process_fun =
						NULL;
					di_buf->c.process_fun_index =
							PROCESS_FUN_NULL;
					di_buf->c.pd_config.global_mode =
						PULL_DOWN_NORMAL;
				}
				di_buf->c.di_buf[0] = ready_di_buf;
				di_buf->c.di_buf[1] = NULL;
				queuev3_out(channel, ready_di_buf);

#ifdef DET3D
				if (dual_vframe_flag) {
					di_buf->c.di_buf[1] =
						di_buf->c.di_buf_dup_p[1];
					queuev3_out(channel,
						    di_buf->c.di_buf[1]);
				}
#endif
				drop_frame(check_drop,
					di_buf->c.di_buf[0]->c.throw_flag,
					di_buf, channel);

				frame_count++;
#ifdef DI_BUFFER_DEBUG
				dimv3_print(
					"%s <prog by frame>: ",
					__func__);
#endif
				ret = 1;
				pwv3_vf_notify_receiver(channel,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
			}
		} else if (ready_count >= 2) {
			/*for progressive input,type
			 * 1:separate tow fields,type
			 * 2:bypass post as frame
			 */
			unsigned char prog_tb_field_proc_type =
				(dimp_get(eDI_MP_prog_proc_config) >> 1) & 0x3;
			di_lock_irqfiq_save(irq_flag2);

			di_buf = div3_que_out_to_di_buf(channel, QUE_POST_FREE);
			if (dimv3_check_di_buf(di_buf, 20, channel)) {
				di_unlock_irqfiq_restore(irq_flag2);
				return 0;
			}

			di_unlock_irqfiq_restore(irq_flag2);

			i = 0;

			div3_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

			for (itmp = 0; itmp < psize; itmp++) {
				p = pwv3_qindex_2_buf(channel, tmpa[itmp]);
				di_buf->c.di_buf_dup_p[i++] = p;
				if (i >= 2) {
					di_buf->c.di_buf_dup_p[i] = NULL;
					break;
				}
			}
			if (i < 2) {
				PR_ERR("%s:Error6\n", __func__);

				if (recovery_flag == 0)
					recovery_log_reason = 21;

				recovery_flag++;
				return 0;
			}

			memcpy(di_buf->vframe,
			       di_buf->c.di_buf_dup_p[0]->vframe,
			       sizeof(vframe_t));
			di_buf->vframe->private_data = di_buf;

			/*separate one progressive frame
			 * as two interlace fields
			 */
			if (prog_tb_field_proc_type == 1) {
				/* do weave by di post */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;
				if (
					di_buf->c.di_buf_dup_p[0]->
					c.new_format_flag)
					di_buf->vframe->
					early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->
					early_process_fun =
						do_nothing_fun;

				di_buf->c.pd_config.global_mode =
					PULL_DOWN_BUF1;
				di_buf->vframe->process_fun =
(dimp_get(eDI_MP_post_wr_en) && dimp_get(eDI_MP_post_wr_support)) ? NULL :
				dimv3_post_process;
				di_buf->c.process_fun_index = PROCESS_FUN_DI;
			} else if (prog_tb_field_proc_type == 0) {
				/* to do: need change for
				 * DI_USE_FIXED_CANVAS_IDX
				 */
				/* do weave by vpp */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE;
				if (
					(di_buf->c.di_buf_dup_p[0]->
					 c.new_format_flag) ||
					(Rd(DI_IF1_GEN_REG) & 1))
					di_buf->vframe->
					early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->
					early_process_fun =
						do_nothing_fun;
				di_buf->vframe->process_fun = NULL;
				di_buf->c.process_fun_index = PROCESS_FUN_NULL;
				di_buf->vframe->canvas0Addr =
					di_buf->c.di_buf_dup_p[0]->
					nr_canvas_idx;
				di_buf->vframe->canvas1Addr =
					di_buf->c.di_buf_dup_p[1]->
					nr_canvas_idx;
			} else {
				/* to do: need change for
				 * DI_USE_FIXED_CANVAS_IDX
				 */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;
				di_buf->vframe->height >>= 1;

				di_buf->vframe->width
					= di_buf->c.di_buf_dup_p[0]->c.width_bk;
				if (
					(di_buf->c.di_buf_dup_p[0]->
					 c.new_format_flag) ||
					(Rd(DI_IF1_GEN_REG) & 1))
					di_buf->vframe->
					early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->
					early_process_fun =
						do_nothing_fun;
				if (prog_tb_field_proc_type == 2) {
					di_buf->vframe->canvas0Addr =
						di_buf->c.di_buf_dup_p[0]
						->nr_canvas_idx;
/* top */
					di_buf->vframe->canvas1Addr =
						di_buf->c.di_buf_dup_p[0]
						->nr_canvas_idx;
				} else {
					di_buf->vframe->canvas0Addr =
						di_buf->c.di_buf_dup_p[1]
						->nr_canvas_idx; /* top */
					di_buf->vframe->canvas1Addr =
						di_buf->c.di_buf_dup_p[1]
						->nr_canvas_idx;
				}
			}

			di_buf->c.di_buf[0] = di_buf->c.di_buf_dup_p[0];
			queuev3_out(channel, di_buf->c.di_buf[0]);
			/*check if the field is error,then drop*/
			if (
				(di_buf->c.di_buf_dup_p[0]->vframe->type &
				 VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_BOTTOM) {
				di_buf->c.di_buf[1] =
					di_buf->c.di_buf_dup_p[1] = NULL;
				queuev3_in(channel, di_buf, QUEUE_TMP);
				recycle_vframe_type_post(di_buf, channel);
				pr_dbg("%s drop field %d.\n", __func__,
				       di_buf->c.di_buf_dup_p[0]->c.seq);
			} else {
				di_buf->c.di_buf[1] =
					di_buf->c.di_buf_dup_p[1];
				queuev3_out(channel, di_buf->c.di_buf[1]);

				drop_frame(
				dimp_get(eDI_MP_check_start_drop_prog),
				(di_buf->c.di_buf_dup_p[0]->c.throw_flag) ||
				(di_buf->c.di_buf_dup_p[1]->c.throw_flag),
				di_buf, channel);
			}
			frame_count++;
#ifdef DI_BUFFER_DEBUG
			dimv3_print("%s <prog by field>: ", __func__);
#endif
			ret = 1;
			pwv3_vf_notify_receiver(channel,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);
		}
	}

#ifdef DI_BUFFER_DEBUG
	if (di_buf) {
		dimv3_print("%s[%d](",
			  vframe_type_name[di_buf->type], di_buf->index);
		for (i = 0; i < 2; i++) {
			if (di_buf->c.di_buf[i])
				dimv3_print("%s[%d],",
				vframe_type_name[di_buf->c.di_buf[i]->type],
				di_buf->c.di_buf[i]->index);
		}
		dimv3_print(")(vframe type %x dur %d)",
			  di_buf->vframe->type, di_buf->vframe->duration);
		if (di_buf->c.di_buf_dup_p[1] &&
		    (di_buf->c.di_buf_dup_p[1]->c.post_proc_flag == 3))
			dimv3_print("=> recycle_list\n");
		else
			dimv3_print("=> post_ready_list\n");
	}
#endif
	return ret;
	#endif
	return 0;
}
#if 0
/*
 * di task
 */
void dim_unreg_process(unsigned int channel) /*ary no use*/
{
	unsigned long start_jiffes = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	PR_INF("%s unreg start %d.\n", __func__, get_reg_flag(channel));
	if (get_reg_flag(channel)) {
		start_jiffes = jiffies_64;
		di_vframe_unreg(channel);
		pr_dbg("%s vf unreg cost %u ms.\n", __func__,
			jiffies_to_msecs(jiffies_64 - start_jiffes));
		unreg_cnt++;
		if (unreg_cnt > 0x3fffffff)
			unreg_cnt = 0;
		pr_dbg("%s unreg stop %d.\n", __func__, get_reg_flag(channel));

	} else {
		ppre->force_unreg_req_flag = 0;
		ppre->disable_req_flag = 0;
		recovery_flag = 0;
	}
}
#endif
void div3_unreg_setting(void)
{
	unsigned int mirror_disable = get_blackout_policy();

	if (!get_hw_reg_flg()) {
		PR_ERR("%s:have setting?do nothing\n", __func__);
		return;
	}

	PR_INF("%s\n", __func__);
	/*set flg*/
	set_hw_reg_flg(false);

	dimhv3_enable_di_pre_mif(false, dimp_get(eDI_MP_mcpre_en));
	postv3_close_new();	/*2018-11-29*/
	dimhv3_afbc_reg_sw(false);
	dimhv3_hw_uninit();
	if (is_meson_txlx_cpu()	||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()	||
	    is_meson_sm1_cpu()) {
		dimv3_pre_gate_control(false, dimp_get(eDI_MP_mcpre_en));
		get_ops_nr()->nr_gate_control(false);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		dimv3_DI_Wr(DI_CLKG_CTRL, 0x80f60000);
		dimv3_DI_Wr(DI_PRE_CTRL, 0);
	} else {
		dimv3_DI_Wr(DI_CLKG_CTRL, 0xf60000);
	}
	/*ary add for switch to post wr, can't display*/
	//PR_INF("patch dimh_disable_post_deinterlace_2\n");
	dimhv3_disable_post_deinterlace_2();
	/* nr/blend0/ei0/mtn0 clock gate */

	dimv3_hw_disable(dimp_get(eDI_MP_mcpre_en));

	if (is_meson_txlx_cpu() ||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()	||
	    is_meson_sm1_cpu()) {
	#ifdef DIM_HIS
		dimhv3_enable_di_post_mif(GATE_OFF);
		dimv3_post_gate_control(false);
	#else
		hpstv3_power_ctr(false);
	#endif
		dimv3_top_gate_control(false, false);
	} else {
		dimv3_DI_Wr(DI_CLKG_CTRL, 0x80000000);
	}
	if (!is_meson_gxl_cpu()	&&
	    !is_meson_gxm_cpu()	&&
	    !is_meson_gxbb_cpu() &&
	    !is_meson_txlx_cpu())
		diextv3_clk_b_sw(false);
	PR_INF("%s disable di mirror image.\n", __func__);

#if 0
	if (mirror_disable) {
		/*no mirror:*/
		if (dimp_get(eDI_MP_post_wr_en) &&
		    dimp_get(eDI_MP_post_wr_support))
			dimv3_set_power_control(0);

	} else {
		/*have mirror:*/
	}
#else	/*0624?*/
	if ((dimp_get(eDI_MP_post_wr_en)	&&
	     dimp_get(eDI_MP_post_wr_support))	||
	     mirror_disable) {
		/*diwr_set_power_control(0);*/
		hpstv3_mem_pd_sw(0);
	}
	if (mirror_disable)
		hpstv3_vd1_sw(0);
#endif

	#if 0	/*tmp*/
	if (post_wr_en && post_wr_support)
		dimv3_set_power_control(0);
	#endif

	disp_frame_count = 0;/* debug only*/
}

void div3_unreg_variable(unsigned int channel)
{
	ulong irq_flag2 = 0;
	unsigned int mirror_disable = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	//struct di_dev_s *de_devp = get_dim_de_devp();

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
	ulong flags = 0;

	spin_lock_irqsave(&plistv3_lock, flags);
#endif
	PR_INF("%s:ch[%d]\n", __func__, channel);
	set_init_flag(channel, false);	/*init_flag = 0;*/
	dimv3_sumx_clear(channel);

	mirror_disable = get_blackout_policy();
	di_lock_irqfiq_save(irq_flag2);
	dimv3_print("%s: dimv3_uninit_buf\n", __func__);
	dimv3_uninit_buf(mirror_disable, channel);

	get_ops_mtn()->adpative_combing_exit();

	di_unlock_irqfiq_restore(irq_flag2);

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
	spin_unlock_irqrestore(&plistv3_lock, flags);
#endif
	dimhv3_patch_post_update_mc_sw(DI_MC_SW_REG, false);

	ppre->force_unreg_req_flag = 0;
	ppre->disable_req_flag = 0;
	recovery_flag = 0;
	ppre->cur_prog_flag = 0;

	if (cfgeq(mem_flg, eDI_MEM_M_cma)	||
	    cfgeq(mem_flg, eDI_MEM_M_codec_a)	||
	    cfgeq(mem_flg, eDI_MEM_M_codec_b))
		dipv3_wq_cma_run(channel, false);

	sum_g_clear(channel);
	sum_p_clear(channel);
	get_chdata(channel)->sum_pst_done = 0;
	di_bypass_state_set(channel, true); /*from unreg any use ?*/
	dbg_reg("%s:end\n", __func__);
}

void diextv3_clk_b_sw(bool on)
{
	if (on)
		extv3_ops.switch_vpu_clk_gate_vmod(VPU_VPU_CLKB,
				VPU_CLK_GATE_ON);
	else
		extv3_ops.switch_vpu_clk_gate_vmod(VPU_VPU_CLKB,
				VPU_CLK_GATE_OFF);
}

#if 0 /*no use*/
void dim_reg_process(unsigned int channel)
{
	/*get vout information first time*/
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (get_reg_flag(channel))
		return;
	di_vframe_reg(channel);

	ppre->bypass_flag = false;
	reg_cnt++;
	if (reg_cnt > 0x3fffffff)
		reg_cnt = 0;
	dimv3_print("########%s\n", __func__);
}
#endif


void dimv3_rdma_init(void)
{

}

void dimv3_rdma_exit(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	/* rdma handle */
	if (de_devp->rdma_handle > 0)
		rdma_unregister(de_devp->rdma_handle);
#endif
}

static void di_load_pq_table(void)
{
	struct di_pq_parm_s *pos = NULL, *tmp = NULL;
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	if (atomic_read(&de_devp->pq_flag) == 0 &&
	    (de_devp->flags & DI_LOAD_REG_FLAG)) {
		atomic_set(&de_devp->pq_flag, 1);
		list_for_each_entry_safe(pos, tmp,
					 &de_devp->pq_table_list, list) {
			dimhv3_load_regs(pos);
			list_del(&pos->list);
			di_pq_parm_destroy(pos);
		}
		de_devp->flags &= ~DI_LOAD_REG_FLAG;
		atomic_set(&de_devp->pq_flag, 0);
	}
}

static void di_pre_size_change(unsigned short width,
			       unsigned short height,
			       unsigned short vf_type,
			       unsigned int channel)
{
	unsigned int blkhsize = 0;
	int pps_w = 0, pps_h = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	/*pr_info("%s:\n", __func__);*/
	/*debug only:*/
	/*di_pause(channel, true);*/
	get_ops_nr()->nr_all_config(width, height, vf_type);
	#ifdef DET3D
	/*det3d_config*/
	get_ops_3d()->det3d_config(dimp_get(eDI_MP_det3d_en) ? 1 : 0);
	#endif
	if (dimp_get(eDI_MP_pulldown_enable)) {
		/*pulldown_init(width, height);*/
		get_ops_pd()->init(width, height);
		dimhv3_init_field_mode(height);

		if (is_meson_txl_cpu()	||
		    is_meson_txlx_cpu() ||
		    is_meson_gxlx_cpu() ||
		    is_meson_txhd_cpu() ||
		    is_meson_g12a_cpu() ||
		    is_meson_g12b_cpu() ||
		    is_meson_tl1_cpu()	||
		    is_meson_tm2_cpu()	||
		    is_meson_sm1_cpu())
			dimv3_film_mode_win_config(width, height);
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		dimhv3_combing_pd22_window_config(width, height);
	dimv3_RDMA_WR(DI_PRE_SIZE, (width - 1) |
		((height - 1) << 16));

	if (dimp_get(eDI_MP_mcpre_en)) {
		blkhsize = (width + 4) / 5;
		dimv3_RDMA_WR(MCDI_HV_SIZEIN, height
			| (width << 16));
		dimv3_RDMA_WR(MCDI_HV_BLKSIZEIN, (overturn ? 3 : 0) << 30
			| blkhsize << 16 | height);
		dimv3_RDMA_WR(MCDI_BLKTOTAL, blkhsize * height);
		if (is_meson_gxlx_cpu()) {
			dimv3_RDMA_WR(MCDI_PD_22_CHK_FLG_CNT, 0);
			dimv3_RDMA_WR(MCDI_FIELD_MV, 0);
		}
	}
	if (channel == 0 ||
	    (channel == 1 && !get_reg_flag(0)))
		di_load_pq_table();
	#ifdef OLD_PRE_GL
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		dimv3_RDMA_WR(DI_PRE_GL_CTRL, 0x80000005);
	#endif
	if (de_devp->nrds_enable)
		dimv3_nr_ds_init(width, height);
	if (de_devp->pps_enable	&& dimp_get(eDI_MP_pps_position)) {
		pps_w = ppre->cur_width;
		pps_h = ppre->cur_height >> 1;
		dimv3_pps_config(1, pps_w, pps_h,
			       dimp_get(eDI_MP_pps_dstw),
			       (dimp_get(eDI_MP_pps_dsth) >> 1));
	}

	if (is_meson_sm1_cpu() || is_meson_tm2_cpu()) {
		if (de_devp->h_sc_down_en) {
			pps_w = ppre->cur_width;
			dimv3_inp_hsc_setting(pps_w,
				dimp_get(eDI_MP_pre_hsc_down_width));
		} else {
			dimv3_inp_hsc_setting(ppre->cur_width,
					    ppre->cur_width);
		}
	}

	#if 0
	dimhv3_interrupt_ctrl(ppre->madi_enable,
			    det3d_en ? 1 : 0,
			    de_devp->nrds_enable,
			    post_wr_en,
			    ppre->mcdi_enable);
	#else
	/*dimh_int_ctr(0, 0, 0, 0, 0, 0);*/
	dimhv3_int_ctr(1, ppre->madi_enable,
		     dimp_get(eDI_MP_det3d_en) ? 1 : 0,
		     de_devp->nrds_enable,
		     dimp_get(eDI_MP_post_wr_en),
		     ppre->mcdi_enable);
	#endif
}
#if 0
static bool need_bypass(struct vframe_s *vf)
{
	if (vf->type & VIDTYPE_MVC)
		return true;

	if (vf->source_type == VFRAME_SOURCE_TYPE_PPMGR)
		return true;

	if (vf->type & VIDTYPE_VIU_444)
		return true;

	if (vf->type & VIDTYPE_PIC)
		return true;
#if 0
	if (vf->type & VIDTYPE_COMPRESS)
		return true;
#else
	/*support G12A and TXLX platform*/
	if (vf->type & VIDTYPE_COMPRESS) {
		if (!dimhv3_afbc_is_supported())
			return true;
		if ((vf->compHeight > (default_height + 8))	||
		    (vf->compWidth > default_width))
			return true;
	}
#endif
	if ((vf->width > default_width) ||
	    (vf->height > (default_height + 8)))
		return true;
#if 0
	if (vfv3_type_is_prog(vf->type)) {/*temp bypass p*/
		return true;
	}
#endif
	/*true bypass for 720p above*/
	if ((vf->flag & VFRAME_FLAG_GAME_MODE) &&
	    (vf->width > 720))
		return true;

	return false;
}
#endif
/*********************************
 *
 * setting register only call when
 * from di_reg_process_irq
 *********************************/
void div3_reg_setting(unsigned int channel, struct vframe_s *vframe,
		    struct dim_wmode_s	*pwmode)
{
	unsigned short nr_height = 0, first_field_type;
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	PR_INF("%s:ch[%d]:for first ch reg:\n", __func__, channel);

	if (get_hw_reg_flg()) {
		PR_ERR("%s:have setting?do nothing\n", __func__);
		return;
	}
	/*set flg*/
	set_hw_reg_flg(true);

	diextv3_clk_b_sw(true);

	dimv3_ddbg_mod_save(eDI_DBG_MOD_REGB, channel, 0);

	if (dimp_get(eDI_MP_post_wr_en)	&&
	    dimp_get(eDI_MP_post_wr_support))
		dimv3_set_power_control(1);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		/*if (!use_2_interlace_buff) {*/
		if (1) {
			dimv3_top_gate_control(true, true);
			/* dim_post_gate_control(true);*/
			/* freerun for reg configuration */
			/* dimh_enable_di_post_mif(GATE_AUTO);*/
			hpstv3_power_ctr(true);
		} else {
			dimv3_top_gate_control(true, false);
		}
		de_devp->flags |= DI_VPU_CLKB_SET;
		#if 1	/*set clkb to max */
		if (is_meson_g12a_cpu()	||
		    is_meson_g12b_cpu()	||
		    is_meson_tl1_cpu()	||
		    is_meson_tm2_cpu()	||
		    is_meson_sm1_cpu()
		    ) {
			#ifdef CLK_TREE_SUPPORT
			clk_set_rate(de_devp->vpu_clkb,
				     de_devp->clkb_max_rate);
			#endif
		}
		#endif
		dimhv3_enable_di_pre_mif(false, dimp_get(eDI_MP_mcpre_en));
		dimv3_pre_gate_control(true, dimp_get(eDI_MP_mcpre_en));
		dimv3_rst_protect(true);/*2019-01-22 by VLSI feng.wang*/
		dimv3_pre_nr_wr_done_sel(true);
		get_ops_nr()->nr_gate_control(true);
	} else {
		/* if mcdi enable DI_CLKG_CTRL should be 0xfef60000 */
		dimv3_DI_Wr(DI_CLKG_CTRL, 0xfef60001);
		/* nr/blend0/ei0/mtn0 clock gate */
	}
	/*--------------------------*/
	dimv3_init_setting_once();
	/*--------------------------*/
	/*div3_post_reset();*/ /*add by feijun 2018-11-19 */
	postv3_mif_sw(false);
	postv3_dbg_contr();
	/*--------------------------*/

	//nr_height = (vframe->height >> 1);/*temp*/
	nr_height = pwmode->src_h >> 1;
	/*--------------------------*/
	dimhv3_calc_lmv_init();
	first_field_type = (vframe->type & VIDTYPE_TYPEMASK);
	//di_pre_size_change(vframe->width, nr_height,
	//		   first_field_type, channel);
	di_pre_size_change(pwmode->src_w, nr_height,
			   first_field_type, channel);
	get_ops_nr()->cue_int(vframe);
	dimv3_ddbg_mod_save(eDI_DBG_MOD_REGE, channel, 0);

	/*--------------------------*/
	/*test*/
	dimhv3_int_ctr(0, 0, 0, 0, 0, 0);
}

/*********************************
 *
 * setting variable
 * from di_reg_process_irq
 *
 *********************************/
#if 0
void di_reg_variable(unsigned int channel,
		     struct dim_dvfm_s *pdvfm)
{
	ulong irq_flag2 = 0;
	#ifndef	RUN_DI_PROCESS_IN_IRQ
	ulong flags = 0;
	#endif
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	if ((prev3_run_flag != DI_RUN_FLAG_RUN) &&
	    (prev3_run_flag != DI_RUN_FLAG_STEP))
		return;
	if (prev3_run_flag == DI_RUN_FLAG_STEP)
		prev3_run_flag = DI_RUN_FLAG_STEP_DONE;

	dbg_reg("%s:\n", __func__);

	dimv3_print("%s:0x%p\n", __func__, pdvfm->vfm_in);
	if (!pdvfm) {
		PR_ERR("%s: nothing\n", __func__);
		return;
	}
	dimv3_tst_4k_reg_val();
	//dip_init_value_reg(channel);/*add 0404 for post*/
#ifdef DIM_DEBUG_QUE_ERR
		dim_dbg_que_int();
#endif
	dimv3_ddbg_mod_save(eDI_DBG_MOD_RVB, channel, 0);
	#if 0
	if (need_bypass(vframe)	||
	    ((dimp_get(eDI_MP_di_debug_flag) >> 20) & 0x1)) {
		if (!ppre->bypass_flag) {
			PR_INF("%ux%u-0x%x.\n",
			       vframe->width,
			       vframe->height,
			       vframe->type);
		}
		ppre->bypass_flag = true;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
		return;

	ppre->bypass_flag = false;
	}
	#else
	if (pdvfm->wmode.need_bypass) {
		if (!ppre->bypass_flag) {
			PR_INF("%ux%u-0x%x.\n",
			       pdvfm->in_inf.w,
			       pdvfm->in_inf.h,
			       pdvfm->in_inf.vtype);
		}
		ppre->bypass_flag = true;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
		return;

		ppre->bypass_flag = false;
	}
	#endif
	/* patch for vdin progressive input */
	#if 0
	if ((is_from_vdin(vframe)	&&
	    is_progressive(vframe))
	#else
	if ((pdvfm->wmode.is_vdin &&
	    !pdvfm->wmode.is_i)
	#endif
		#ifdef DET3D
		|| dimp_get(eDI_MP_det3d_en)
		#endif
		|| (dimp_get(eDI_MP_use_2_interlace_buff) & 0x2)
		) {
		dimp_set(eDI_MP_use_2_interlace_buff, 1);
	} else {
		dimp_set(eDI_MP_use_2_interlace_buff, 0);
	}
	de_devp->nrds_enable = dimp_get(eDI_MP_nrds_en);
	de_devp->pps_enable = dimp_get(eDI_MP_pps_en);
	/*di pre h scaling down: sm1 tm2*/
	de_devp->h_sc_down_en = di_mp_uit_get(eDI_MP_pre_hsc_down_en);

	if (dimp_get(eDI_MP_di_printk_flag) & 2)
		dimp_set(eDI_MP_di_printk_flag, 1);

	dimv3_print("%s: vframe come => di_init_buf\n", __func__);

	if (cfgeq(mem_flg, eDI_MEM_M_rev) && !de_devp->mem_flg)
		dimv3_rev_mem_check();

	/*(is_progressive(vframe) && (prog_proc_config & 0x10)) {*/
	if (0) {
#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
		spin_lock_irqsave(&plistv3_lock, flags);
#endif
		di_lock_irqfiq_save(irq_flag2);
		/*
		 * 10 bit mode need 1.5 times buffer size of
		 * 8 bit mode, init the buffer size as 10 bit
		 * mode size, to make sure can switch bit mode
		 * smoothly.
		 */
		di_init_buf(default_width, default_height, 1, channel);

		di_unlock_irqfiq_restore(irq_flag2);

#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
		spin_unlock_irqrestore(&plistv3_lock, flags);
#endif
	} else {
#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
		spin_lock_irqsave(&plistv3_lock, flags);
#endif
		di_lock_irqfiq_save(irq_flag2);
		/*
		 * 10 bit mode need 1.5 times buffer size of
		 * 8 bit mode, init the buffer size as 10 bit
		 * mode size, to make sure can switch bit mode
		 * smoothly.
		 */
		di_init_buf(default_width, default_height, 0, channel);

		di_unlock_irqfiq_restore(irq_flag2);

#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
		spin_unlock_irqrestore(&plistv3_lock, flags);
#endif
	}

	ppre->mtn_status =
		#if 0
		get_ops_mtn()->adpative_combing_config(vframe->width,
				(vframe->height >> 1),
				(vframe->source_type),
				is_progressive(vframe),
				vframe->sig_fmt);
		#else
		get_ops_mtn()->adpative_combing_config(pdvfm->in_inf.w,
				(pdvfm->in_inf.h >> 1),
				(pdvfm->in_inf.src_type),
				!pdvfm->wmode.is_i,
				pdvfm->in_inf.sig_fmt);
		#endif

	dimhv3_patch_post_update_mc_sw(DI_MC_SW_REG, true);
	div3_sum_reg_init(channel);

	set_init_flag(channel, true);/*init_flag = 1;*/

	dimv3_ddbg_mod_save(eDI_DBG_MOD_RVE, channel, 0);

}
#else

void di_reg_variable_needbypass(unsigned int channel,
				struct dim_dvfm_s *pdvfm)
{
//	ulong irq_flag2 = 0;
	//#ifndef	RUN_DI_PROCESS_IN_IRQ
//	ulong flags = 0;
	//#endif
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
//	struct di_dev_s *de_devp = get_dim_de_devp();

	dbg_reg("%s:\n", __func__);

	if (!pdvfm) {
		PR_ERR("%s: nothing\n", __func__);
		return;
	}
	dimv3_print("%s:0x%p\n", __func__, pdvfm->vfm_in);

	dimv3_tst_4k_reg_val();
	//dip_init_value_reg(channel);/*add 0404 for post*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_RVB, channel, 0);

	if (pdvfm->wmode.need_bypass) {
		if (!ppre->bypass_flag) {
			PR_INF("%ux%u-0x%x.\n",
			       pdvfm->in_inf.w,
			       pdvfm->in_inf.h,
			       pdvfm->wmode.vtype);
		}
		ppre->bypass_flag = true;
		dimhv3_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
		return;
	}

}

void di_reg_variable_normal(unsigned int channel,
			    struct dim_dvfm_s *pdvfm)
{
	ulong irq_flag2 = 0;
	#ifndef	RUN_DI_PROCESS_IN_IRQ
	ulong flags = 0;
	#endif
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	dbg_reg("%s:\n", __func__);

	dim_dbg_delay_mask_set(0);

	if (!pdvfm) {
		PR_ERR("%s: nothing\n", __func__);
		return;
	}
	dimv3_print("%s:0x%p\n", __func__, pdvfm->vfm_in);

	dimv3_tst_4k_reg_val();
	//dip_init_value_reg(channel);/*add 0404 for post*/
	dimv3_ddbg_mod_save(eDI_DBG_MOD_RVB, channel, 0);

	ppre->bypass_flag = false;
	/* patch for vdin progressive input */

	#if 0
	if ((pdvfm->wmode.is_vdin &&
	    !pdvfm->wmode.is_i)
		|| (dimp_get(eDI_MP_use_2_interlace_buff) & 0x2)
		) {
		dimp_set(eDI_MP_use_2_interlace_buff, 1);
	} else {
		dimp_set(eDI_MP_use_2_interlace_buff, 0);
	}
	#else
	if ((pdvfm->wmode.is_vdin && !pdvfm->wmode.is_i) ||
	    (cfgg(PMODE) == 2)) {
		dimp_set(eDI_MP_use_2_interlace_buff, 1);
	} else {
		dimp_set(eDI_MP_use_2_interlace_buff, 0);
	}
	#endif
	de_devp->nrds_enable = dimp_get(eDI_MP_nrds_en);
	de_devp->pps_enable = dimp_get(eDI_MP_pps_en);
	dbg_reg("pps_enable[%d]\n", de_devp->pps_enable);
	/*di pre h scaling down: sm1 tm2*/
	//de_devp->h_sc_down_en = dimp_get(eDI_MP_pre_hsc_down_en);
	//PR_INF("pps2[%d]\n",de_devp->h_sc_down_en);

	if (dimp_get(eDI_MP_di_printk_flag) & 2)
		dimp_set(eDI_MP_di_printk_flag, 1);

	dimv3_print("%s: vframe come => di_init_buf\n", __func__);

	if (cfgeq(mem_flg, eDI_MEM_M_rev) && !de_devp->mem_flg)
		dimv3_rev_mem_check();

	/*(is_progressive(vframe) && (prog_proc_config & 0x10)) {*/
	if (0) {

		spin_lock_irqsave(&plistv3_lock, flags);

		di_lock_irqfiq_save(irq_flag2);
		/*
		 * 10 bit mode need 1.5 times buffer size of
		 * 8 bit mode, init the buffer size as 10 bit
		 * mode size, to make sure can switch bit mode
		 * smoothly.
		 */
		di_init_buf(default_width, default_height, 1, channel);

		di_unlock_irqfiq_restore(irq_flag2);


		spin_unlock_irqrestore(&plistv3_lock, flags);

	} else {

		spin_lock_irqsave(&plistv3_lock, flags);

		di_lock_irqfiq_save(irq_flag2);
		/*
		 * 10 bit mode need 1.5 times buffer size of
		 * 8 bit mode, init the buffer size as 10 bit
		 * mode size, to make sure can switch bit mode
		 * smoothly.
		 */
		di_init_buf(default_width, default_height, 0, channel);

		di_unlock_irqfiq_restore(irq_flag2);


		spin_unlock_irqrestore(&plistv3_lock, flags);

	}

	ppre->mtn_status =
		#if 0
		get_ops_mtn()->adpative_combing_config(vframe->width,
				(vframe->height >> 1),
				(vframe->source_type),
				is_progressive(vframe),
				vframe->sig_fmt);
		#else
		get_ops_mtn()->adpative_combing_config(pdvfm->in_inf.w,
				(pdvfm->in_inf.h >> 1),
				(pdvfm->in_inf.src_type),
				!pdvfm->wmode.is_i,
				pdvfm->in_inf.sig_fmt);
		#endif

	dimhv3_patch_post_update_mc_sw(DI_MC_SW_REG, true);
	div3_sum_reg_init(channel);

	set_init_flag(channel, true);/*init_flag = 1;*/

	dimv3_ddbg_mod_save(eDI_DBG_MOD_RVE, channel, 0);

}

#endif
/*para 1 not use*/

/*
 * provider/receiver interface
 */

/*************************/
#if 0
int di_ori_event_unreg(unsigned int channel)
{
	//struct di_pre_stru_s *ppre = get_pre_stru(channel);
	#if 0
	pr_dbg("%s , is_bypass() %d bypass_all %d\n",
	       __func__,
	       dim_is_bypass(NULL, channel),
	       /*trick_mode,*/
	       div3_cfgx_get(channel, eDI_CFGX_BYPASS_ALL));
	#endif

	/*dbg_ev("%s:unreg\n", __func__);*/
	dipv3_even_unreg_val(channel);	/*new*/

	/*ppre->vdin_source = false; no use*/

	dipv3_event_unreg_chst(channel);

	//move to unreg di_bypass_state_set(channel, true);

	dbg_ev("ch[%d]unreg end\n", channel);
	return 0;
}
#endif
#if 0
int di_ori_event_reg(void *data, unsigned int channel)
{
	//struct di_pre_stru_s *ppre = get_pre_stru(channel);
	//struct di_post_stru_s *ppost = get_post_stru(channel);
	//struct di_dev_s *de_devp = get_dim_de_devp();

	dipv3_even_reg_init_val(channel);

	#if 0	/*no use*/
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		PR_ERR("reg event device hasn't resumed\n");
		return -1;
	}
	#endif
	if (get_reg_flag(channel)) {
		PR_ERR("no muti instance.\n");
		return -1;
	}
	//dbg_ev("reg:%s\n", provider_name);
	//move to variable set di_bypass_state_set(channel, false);

	dipv3_event_reg_chst(channel);

	#if 0 /*no use*/
	if (strncmp(provider_name, "vdin0", 4) == 0)
		ppre->vdin_source = true;
	else
		ppre->vdin_source = false;
	#endif
	/*ary: need use interface api*/
	/*receiver_name = vf_get_receiver_name(VFM_NAME);*/
	#if 0
	preceiver = vf_get_receiver(div3_rev_name[channel]);
	receiver_name = preceiver->name;
	#endif
	#if 0	/*ary clear run_early_proc_fun_flag*/
	if (receiver_name) {
		if (!strcmp(receiver_name, "amvideo")) {
			ppost->run_early_proc_fun_flag = 0;
			if (dimp_get(eDI_MP_post_wr_en)	&&
			    dimp_get(eDI_MP_post_wr_support))
				ppost->run_early_proc_fun_flag = 1;
		} else {
			ppost->run_early_proc_fun_flag = 1;
			PR_INF("set run_early_proc_fun_flag to 1\n");
		}
	} else {
		PR_INF("%s error receiver is null.\n", __func__);
	}
	#endif
	dbg_ev("ch[%d]:reg end\n", channel);
	return 0;
}
#endif

int div3_ori_event_qurey_vdin2nr(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	return ppre->vdin2nr;
}

int div3_ori_event_reset(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	void **pvframe_in = getv3_vframe_in(channel);
	int i;
	ulong flags;

	/*block*/
	di_blocking = 1;

	/*dbg_ev("%s: VFRAME_EVENT_PROVIDER_RESET\n", __func__);*/
	if (//dim_is_bypass(NULL, channel)	||
	    di_bypass_state_get(channel)	||
	    ppre->bypass_flag) {
		pwv3_vf_notify_receiver(channel,
				      VFRAME_EVENT_PROVIDER_RESET,
			NULL);
	}

	spin_lock_irqsave(&plistv3_lock, flags);
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (pvframe_in[i])
			pr_dbg("DI:clear vframe_in[%d]\n", i);

		pvframe_in[i] = NULL;
	}
	spin_unlock_irqrestore(&plistv3_lock, flags);
	di_blocking = 0;

	return 0;
}

int div3_ori_event_light_unreg(unsigned int channel)
{
	void **pvframe_in = getv3_vframe_in(channel);
	int i;
	ulong flags;

	di_blocking = 1;

	pr_dbg("%s: vf_notify_receiver ligth unreg\n", __func__);

	spin_lock_irqsave(&plistv3_lock, flags);
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (pvframe_in[i])
			pr_dbg("DI:clear vframe_in[%d]\n", i);

		pvframe_in[i] = NULL;
	}
	spin_unlock_irqrestore(&plistv3_lock, flags);
	di_blocking = 0;

	return 0;
}

int div3_ori_event_light_unreg_revframe(unsigned int channel)
{
	void **pvframe_in = getv3_vframe_in(channel);
	int i;
	ulong flags;
	struct dim_itf_ops_s	*pvfmops;
	struct di_ch_s *pch;
	unsigned char vf_put_flag = 0;

	PR_INF("%s:ch[%d]\n", __func__, channel);
/*
 * do not display garbage when 2d->3d or 3d->2d
 */
	pch = get_chdata(channel);
	pvfmops = &pch->interf.opsi;

	spin_lock_irqsave(&plistv3_lock, flags);
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (pvframe_in[i]) {
			//pw_vf_put(pvframe_in[i], channel);
			pvfmops->put(pvframe_in[i], pch);
			pr_dbg("DI:clear vframe_in[%d]\n", i);
			vf_put_flag = 1;
		}
		pvframe_in[i] = NULL;
	}
	//if (vf_put_flag)
	//	pw_vf_notify_provider(channel,
	//			      VFRAME_EVENT_RECEIVER_PUT, NULL);

	spin_unlock_irqrestore(&plistv3_lock, flags);

	return 0;
}

int div3_irq_ori_event_ready(unsigned int channel)
{
	return 0;
}

#if 0
int di_ori_event_ready(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (ppre->bypass_flag)
		pwv3_vf_notify_receiver(channel,
				      VFRAME_EVENT_PROVIDER_VFRAME_READY,
				      NULL);
	#if 0	/*hw timer*/
	if (dipv3_chst_get(channel) == eDI_TOP_STATE_REG_STEP1)
		taskv3_send_cmd(LCMD1(eCMD_READY, channel));
	else
		taskv3_send_ready();

	div3_irq_ori_event_ready(channel);
	#endif
	return 0;
}
#endif
#if 0	/*move to di_vframe.c*/
int div3_ori_event_qurey_state(unsigned int channel)
{
	/*int in_buf_num = 0;*/
	struct vframe_states states;

	if (recovery_flag)
		return RECEIVER_INACTIVE;

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
#endif
void  div3_ori_event_set_3D(int type, void *data, unsigned int channel)
{
#ifdef DET3D

	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (type == VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE) {
		int flag = (long)data;

		ppre->vframe_interleave_flag = flag;
	}

#endif
}

/*************************/

/*recycle the buffer for keeping buffer*/
static void recycle_keep_buffer(unsigned int channel)
{
	ulong irq_flag2 = 0;
	int i = 0;
	struct di_buf_s *keep_buf;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	keep_buf = ppost->keep_buf;
	if (!IS_ERR_OR_NULL(keep_buf)) {
		PR_ERR("%s:\n", __func__);
		if (keep_buf->type == VFRAME_TYPE_POST) {
			pr_dbg("%s recycle keep cur di_buf %d (",
			       __func__, keep_buf->index);
			di_lock_irqfiq_save(irq_flag2);
			for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
				if (keep_buf->c.di_buf_dup_p[i]) {
					queuev3_in(channel,
						 keep_buf->c.di_buf_dup_p[i],
						 QUEUE_RECYCLE);
					pr_dbg(" %d ",
					keep_buf->c.di_buf_dup_p[i]->index);
				}
			}
			div3_que_in(channel, QUE_POST_FREE, keep_buf);
			di_unlock_irqfiq_restore(irq_flag2);
			pr_dbg(")\n");
		}
		ppost->keep_buf = NULL;
	}
}

/************************************/
/************************************/
struct vframe_s *div3_vf_l_get(struct di_ch_s *pch)
{
	vframe_t *vframe_ret = NULL;
	struct di_buf_s *di_buf = NULL;
#ifdef DI_DEBUG_POST_BUF_FLOW
	struct di_buf_s *nr_buf = NULL;
#endif
	unsigned int ch = pch->ch_id;

	ulong irq_flag2 = 0;
	//struct di_post_stru_s *ppost = get_post_stru(channel);

	if (!get_init_flag(ch)	||
	    recovery_flag		||
	    di_blocking			||
	    !get_reg_flag(ch)	||
	    dump_state_flag) {
		dimv3_tr_ops.post_get2(1);
		return NULL;
	}

	/**************************/
	if (listv3_count(ch, QUEUE_DISPLAY) > DI_POST_GET_LIMIT) {
		dimv3_tr_ops.post_get2(2);
		return NULL;
	}
	/**************************/
	if (!div3_que_is_empty(ch, QUE_POST_READY)) {

		dimv3_log_buffer_state("ge_", ch);
		di_lock_irqfiq_save(irq_flag2);

		di_buf = div3_que_out_to_di_buf(ch, QUE_POST_READY);
		if (dimv3_check_di_buf(di_buf, 21, ch)) {
			di_unlock_irqfiq_restore(irq_flag2);
			return NULL;
		}
		/* add it into display_list */
		queuev3_in(ch, di_buf, QUEUE_DISPLAY);

		di_unlock_irqfiq_restore(irq_flag2);

		if (di_buf) {
			vframe_ret = di_buf->vframe;
#ifdef DI_DEBUG_POST_BUF_FLOW
	/*move to post_ready_buf_set*/
	nr_buf = di_buf->c.di_buf_dup_p[1];
	if (dimp_get(eDI_MP_post_wr_en)		&&
	    dimp_get(eDI_MP_post_wr_support)	&&
	    (di_buf->c.process_fun_index != PROCESS_FUN_NULL)) {
		#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
		vframe_ret->canvas0_config[0].phy_addr =
			di_buf->nr_adr;
		vframe_ret->canvas0_config[0].width =
			di_buf->canvas_width[NR_CANVAS],
		vframe_ret->canvas0_config[0].height =
			di_buf->canvas_height;
		vframe_ret->canvas0_config[0].block_mode = 0;
		vframe_ret->plane_num = 1;
		vframe_ret->canvas0Addr = -1;
		vframe_ret->canvas1Addr = -1;
		if (dimp_get(eDI_MP_show_nrwr)) {
			vframe_ret->canvas0_config[0].phy_addr =
				nr_buf->nr_adr;
			vframe_ret->canvas0_config[0].width =
				nr_buf->canvas_width[NR_CANVAS];
			vframe_ret->canvas0_config[0].height =
				nr_buf->canvas_height;
		}
		#else
		config_canvas_idx(di_buf, di_wr_idx, -1);
		vframe_ret->canvas0Addr = di_buf->nr_canvas_idx;
		vframe_ret->canvas1Addr = di_buf->nr_canvas_idx;
		if (dimp_get(eDI_MP_show_nrwr)) {
			config_canvas_idx(nr_buf,
					  di_wr_idx, -1);
			vframe_ret->canvas0Addr = di_wr_idx;
			vframe_ret->canvas1Addr = di_wr_idx;
		}
		#endif
		vframe_ret->early_process_fun = dimv3_do_post_wr_fun;
		vframe_ret->process_fun = NULL;
	}
#endif
			di_buf->c.seq = disp_frame_count;
			atomic_set(&di_buf->c.di_cnt, 1);
		}
		disp_frame_count++;

		dimv3_log_buffer_state("get", ch);
	}
	if (vframe_ret) {
		dimv3_print("%s:ch[%d];typ:%s;id[%d];omx[%d] %u ms\n", __func__,
			  ch,
			  vframe_type_name[di_buf->type],
			  di_buf->index,
			  vframe_ret->omx_index,
			  dim_get_timerms(vframe_ret->ready_jiffies64));
		didbgv3_vframe_out_save(vframe_ret);

		dimv3_tr_ops.post_get(vframe_ret->omx_index);
	} else {
		dimv3_tr_ops.post_get2(3);
	}

	#if 0	/*clear run_early_proc_fun_flag*/
	if (!dimp_get(eDI_MP_post_wr_en)	&&
	    ppost->run_early_proc_fun_flag	&&
	    vframe_ret) {
		if (vframe_ret->early_process_fun == do_pre_only_fun)
			vframe_ret->early_process_fun(vframe_ret->private_data,
						      vframe_ret);
	}
	#endif
	return vframe_ret;
}

void div3_vf_l_put(struct vframe_s *vf, struct di_ch_s *pch)
{
	struct di_buf_s *di_buf = NULL;
	ulong irq_flag2 = 0;
	unsigned int ch = pch->ch_id;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	struct di_post_stru_s *ppost = get_post_stru(ch);

	if (ppre->bypass_flag) {
		//pw_vf_put(vf, channel);
		//pw_vf_notify_provider(channel,
		//		      VFRAME_EVENT_RECEIVER_PUT, NULL);
		if (!IS_ERR_OR_NULL(ppost->keep_buf))
			recycle_keep_buffer(ch);
		return;
	}
/* struct di_buf_s *p = NULL; */
/* int itmp = 0; */
	if (!get_init_flag(ch)	||
	    recovery_flag		||
	    IS_ERR_OR_NULL(vf)) {
		PR_ERR("%s: 0x%p\n", __func__, vf);
		return;
	}
	if (di_blocking)
		return;
	dimv3_log_buffer_state("pu_", ch);
	di_buf = (struct di_buf_s *)vf->private_data;
	if (IS_ERR_OR_NULL(di_buf)) {
		//pw_vf_put(vf, channel);
		//pw_vf_notify_provider(channel,
		//		      VFRAME_EVENT_RECEIVER_PUT, NULL);
		pch->interf.opsi.put(vf, pch);
		PR_ERR("%s: get vframe %p without di buf\n",
		       __func__, vf);
		return;
	}

	di_lock_irqfiq_save(irq_flag2);
	if (di_buf->type == VFRAME_TYPE_POST) {
		//di_lock_irqfiq_save(irq_flag2);

		/*check if in QUE_POST_DISPLAY*/
		/*di_buf->queue_index = QUEUE_DISPLAY;*/
		if (isv3_in_queue(ch, di_buf, QUEUE_DISPLAY)) {
			di_buf->queue_index = -1;
			div3_que_in(ch, QUE_POST_BACK, di_buf);
			//di_unlock_irqfiq_restore(irq_flag2);
		} else {
			//di_unlock_irqfiq_restore(irq_flag2);
			PR_ERR("%s:not in display %d\n", __func__,
			       di_buf->index);
		}

	} else {
		//di_lock_irqfiq_save(irq_flag2);
		queuev3_in(ch, di_buf, QUEUE_RECYCLE);
		//di_unlock_irqfiq_restore(irq_flag2);
	}
	di_unlock_irqfiq_restore(irq_flag2);

	dimv3_print("%s: ch[%d];typ[%s];id[%d];omxp[%d]\n", __func__,
		  ch,
		  vframe_type_name[di_buf->type], di_buf->index,
		  di_buf->vframe->omx_index);

	//task_send_ready();	/*hw timer*/
}

#ifdef DBG2
void dimv3_recycle_post_back(unsigned int channel)
{
	struct di_buf_s *di_buf = NULL;
	unsigned int post_buf_index;
	struct di_buf_s *pbuf_post = get_buf_post(channel);
	ulong irq_flag2 = 0;

	struct di_post_stru_s *ppost = get_post_stru(channel);

	if (pwv3_queue_empty(channel, QUE_POST_BACK))
		return;

	while (pwv3_queue_out(channel, QUE_POST_BACK, &post_buf_index)) {
		/*pr_info("dp2:%d\n", post_buf_index);*/
		if (post_buf_index >= MAX_POST_BUF_NUM) {
			PR_ERR("%s:index is overlfow[%d]\n",
			       __func__, post_buf_index);
			break;
		}
		dimv3_print("di_back:%d\n", post_buf_index);
		di_lock_irqfiq_save(irq_flag2);	/**/

		di_buf = &pbuf_post[post_buf_index];
		if (!atomic_dec_and_test(&di_buf->c.di_cnt))
			PR_ERR("%s,di_cnt > 0\n", __func__);
		recycle_vframe_type_post(di_buf, channel);
		di_unlock_irqfiq_restore(irq_flag2);
	}
	/*back keep_buf:*/
	if (ppost->keep_buf_post) {
		PR_INF("ch[%d]:%s:%d\n",
		       channel,
		       __func__,
		       ppost->keep_buf_post->index);
		di_lock_irqfiq_save(irq_flag2);	/**/
		div3_que_in(channel, QUE_POST_FREE, ppost->keep_buf_post);
		di_unlock_irqfiq_restore(irq_flag2);
		ppost->keep_buf_post = NULL;
	}
}
#else
void dimv3_recycle_post_back(unsigned int channel)
{
	struct di_buf_s *di_buf = NULL;
	ulong irq_flag2 = 0;
	unsigned int i = 0;

	if (div3_que_is_empty(channel, QUE_POST_BACK))
		return;


	while (i < MAX_FIFO_SIZE) {
		i++;

		di_buf = div3_que_peek(channel, QUE_POST_BACK);
		/*pr_info("dp2:%d\n", post_buf_index);*/
		if (!di_buf)
			break;

		if (di_buf->type != VFRAME_TYPE_POST) {
			queuev3_out(channel, di_buf);
			PR_ERR("%s:type is not post\n", __func__);
			continue;
		}

		dimv3_print("di_back:%d\n", di_buf->index);
		di_lock_irqfiq_save(irq_flag2); /**/
		div3_que_out(channel, QUE_POST_BACK, di_buf);
		di_buf->queue_index = QUEUE_DISPLAY;
		/*queue_out(channel, di_buf);*/

		if (!atomic_dec_and_test(&di_buf->c.di_cnt))
			PR_ERR("%s,di_cnt > 0\n", __func__);

		recycle_vframe_type_post(di_buf, channel);
		//di_buf->c.invert_top_bot_flag = 0;
		//di_que_in(channel, QUE_POST_FREE, di_buf);

		di_unlock_irqfiq_restore(irq_flag2);
	}

	if (div3_cfg_top_get(EDI_CFG_KEEP_CLEAR_AUTO))
		taskv3_send_cmd(LCMD1(ECMD_RL_KEEP_ALL, channel));
		//dim_post_keep_release_all_2free(channel);

}

#endif
struct vframe_s *div3_vf_l_peek(struct di_ch_s *pch)
{
	struct vframe_s *vframe_ret = NULL;
	struct di_buf_s *di_buf = NULL;
//	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	unsigned int ch;

	ch = pch->ch_id;
	/*dim_print("%s:ch[%d]\n",__func__, channel);*/

	div3_sum_inc(ch, eDI_SUM_O_PEEK_CNT);
	#if 0
	if (ppre->bypass_flag) {
		dimv3_tr_ops.post_peek(0);
		return pw_vf_peek(channel);
	}
	#endif
	if (!get_init_flag(ch)	||
	    recovery_flag		||
	    di_blocking			||
	    !get_reg_flag(ch)	||
	    dump_state_flag) {
		dimv3_tr_ops.post_peek(1);
		return NULL;
	}

	/**************************/
	if (listv3_count(ch, QUEUE_DISPLAY) > DI_POST_GET_LIMIT) {
		dimv3_tr_ops.post_peek(2);
		return NULL;
	}
	/**************************/
	dimv3_log_buffer_state("pek", ch);

	{
		if (!div3_que_is_empty(ch, QUE_POST_READY)) {
			di_buf = div3_que_peek(ch, QUE_POST_READY);
			if (di_buf)
				vframe_ret = di_buf->vframe;
		}
	}
#ifdef DI_BUFFER_DEBUG
	if (vframe_ret)
		dimv3_print("%s: %s[%d]:%x\n", __func__,
			  vframe_type_name[di_buf->type],
			  di_buf->index, vframe_ret);
#endif
	if (vframe_ret)
		dimv3_tr_ops.post_peek(9);
	else
		dimv3_tr_ops.post_peek(4);
	return vframe_ret;
}

#if 0	/*move to di_vframe.c*/
int di_vf_l_states(struct vframe_states *states, unsigned int channel)
{
	struct di_mm_s *mm = dim_mm_get(channel);

	/*pr_info("%s: ch[%d]\n", __func__, channel);*/
	if (!states)
		return -1;
	states->vf_pool_size = mm->sts.num_local;
	states->buf_free_num = listv3_count(channel, QUEUE_LOCAL_FREE);

	states->buf_avail_num = div3_que_list_count(channel, QUE_POST_READY);
	states->buf_recycle_num = listv3_count(channel, QUEUE_RECYCLE);
	if (dimp_get(eDI_MP_di_dbg_mask) & 0x1) {
		di_pr_info("di-pre-ready-num:%d\n",
			   /*new que list_count(channel, QUEUE_PRE_READY));*/
			   div3_que_list_count(channel, QUE_PRE_READY));
		di_pr_info("di-display-num:%d\n",
			   listv3_count(channel, QUEUE_DISPLAY));
	}
	return 0;
}
#endif
/**********************************************/

/*****************************
 *	 di driver file_operations
 *
 ******************************/
#if 0 /*move to di_sys.c*/
static int di_open(struct inode *node, struct file *file)
{
	di_dev_t *di_in_devp;

/* Get the per-device structure that contains this cdev */
	di_in_devp = container_of(node->i_cdev, di_dev_t, cdev);
	file->private_data = di_in_devp;

	return 0;
}

static int di_release(struct inode *node, struct file *file)
{
/* di_dev_t *di_in_devp = file->private_data; */

/* Reset file pointer */

/* Release some other fields */
	file->private_data = NULL;
	return 0;
}

#endif	/*move to di_sys.c*/

static struct di_pq_parm_s *di_pq_parm_create(struct am_pq_parm_s *pq_parm_p)
{
	struct di_pq_parm_s *pq_ptr = NULL;
	struct am_reg_s *am_reg_p = NULL;
	size_t mem_size = 0;

	pq_ptr = vzalloc(sizeof(*pq_ptr));
	mem_size = sizeof(struct am_pq_parm_s);
	memcpy(&pq_ptr->pq_parm, pq_parm_p, mem_size);
	mem_size = sizeof(struct am_reg_s) * pq_parm_p->table_len;
	am_reg_p = vzalloc(mem_size);
	if (!am_reg_p) {
		vfree(pq_ptr);
		PR_ERR("alloc pq table memory errors\n");
		return NULL;
	}
	pq_ptr->regs = am_reg_p;

	return pq_ptr;
}

static void di_pq_parm_destroy(struct di_pq_parm_s *pq_ptr)
{
	if (!pq_ptr) {
		PR_ERR("%s pq parm pointer null.\n", __func__);
		return;
	}
	vfree(pq_ptr->regs);
	vfree(pq_ptr);
}

/*move from ioctrl*/
long dimv3_pq_load_io(unsigned long arg)
{
	long ret = 0, tab_flag = 0;
	di_dev_t *di_devp;
	void __user *argp = (void __user *)arg;
	size_t mm_size = 0;
	struct am_pq_parm_s tmp_pq_s = {0};
	struct di_pq_parm_s *di_pq_ptr = NULL;
	struct di_dev_s *de_devp = getv3_dim_de_devp();
//	unsigned int channel = 0;	/*fix to channel 0*/

	di_devp = de_devp;

	mm_size = sizeof(struct am_pq_parm_s);
	if (copy_from_user(&tmp_pq_s, argp, mm_size)) {
		PR_ERR("set pq parm errors\n");
		return -EFAULT;
	}
	if (tmp_pq_s.table_len >= TABLE_LEN_MAX) {
		PR_ERR("load 0x%x wrong pq table_len.\n",
		       tmp_pq_s.table_len);
		return -EFAULT;
	}
	tab_flag = TABLE_NAME_DI | TABLE_NAME_NR | TABLE_NAME_MCDI |
		TABLE_NAME_DEBLOCK | TABLE_NAME_DEMOSQUITO;
	if (tmp_pq_s.table_name & tab_flag) {
		PR_INF("load 0x%x pq table len %u %s.\n",
		       tmp_pq_s.table_name, tmp_pq_s.table_len,
		       get_reg_flag_all() ? "directly" : "later");
	} else {
		PR_ERR("load 0x%x wrong pq table.\n",
		       tmp_pq_s.table_name);
		return -EFAULT;
	}
	di_pq_ptr = di_pq_parm_create(&tmp_pq_s);
	if (!di_pq_ptr) {
		PR_ERR("allocat pq parm struct error.\n");
		return -EFAULT;
	}
	argp = (void __user *)tmp_pq_s.table_ptr;
	mm_size = tmp_pq_s.table_len * sizeof(struct am_reg_s);
	if (copy_from_user(di_pq_ptr->regs, argp, mm_size)) {
		PR_ERR("user copy pq table errors\n");
		return -EFAULT;
	}
	if (get_reg_flag_all()) {
		dimhv3_load_regs(di_pq_ptr);
		di_pq_parm_destroy(di_pq_ptr);

		return ret;
	}
	if (atomic_read(&de_devp->pq_flag) == 0) {
		atomic_set(&de_devp->pq_flag, 1);
		if (di_devp->flags & DI_LOAD_REG_FLAG) {
			struct di_pq_parm_s *pos = NULL, *tmp = NULL;

			list_for_each_entry_safe(pos, tmp,
				&di_devp->pq_table_list, list) {
				if (di_pq_ptr->pq_parm.table_name ==
				    pos->pq_parm.table_name) {
					PR_INF("remove 0x%x table.\n",
					       pos->pq_parm.table_name);
					list_del(&pos->list);
					di_pq_parm_destroy(pos);
				}
			}
		}
		list_add_tail(&di_pq_ptr->list,
			      &di_devp->pq_table_list);
		di_devp->flags |= DI_LOAD_REG_FLAG;
		atomic_set(&de_devp->pq_flag, 0);
	} else {
		PR_ERR("please retry table name 0x%x.\n",
		       di_pq_ptr->pq_parm.table_name);
		di_pq_parm_destroy(di_pq_ptr);
		ret = -EFAULT;
	}

	return ret;
}

#if 0 /*#ifdef CONFIG_AMLOGIC_MEDIA_RDMA*/
unsigned int dimv3_RDMA_RD_BITS(unsigned int adr, unsigned int start,
			      unsigned int len)
{
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	if (de_devp->rdma_handle && di_pre_rdma_enable)
		return rdma_read_reg(de_devp->rdma_handle, adr) &
		       (((1 << len) - 1) << start);
	else
		return Rd_reg_bits(adr, start, len);
}

unsigned int dimv3_RDMA_WR(unsigned int adr, unsigned int val)
{
	unsigned int channel = get_current_channel();
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	if (is_need_stop_reg(adr))
		return 0;

	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable) {
		if (ppre->field_count_for_cont < 1)
			dimv3_DI_Wr(adr, val);
		else
			rdma_write_reg(de_devp->rdma_handle, adr, val);
		return 0;
	}

	dimv3_DI_Wr(adr, val);
	return 1;
}

unsigned int dimv3_RDMA_RD(unsigned int adr)
{
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable)
		return rdma_read_reg(de_devp->rdma_handle, adr);
	else
		return Rd(adr);
}

unsigned int dimv3_RDMA_WR_BITS(unsigned int adr, unsigned int val,
			      unsigned int start, unsigned int len)
{
	unsigned int channel = get_current_channel();
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = getv3_dim_de_devp();

	if (is_need_stop_reg(adr))
		return 0;

	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable) {
		if (ppre->field_count_for_cont < 1)
			dimv3_DI_Wr_reg_bits(adr, val, start, len);
		else
			rdma_write_reg_bits(de_devp->rdma_handle,
					    adr, val, start, len);
		return 0;
	}
	dimv3_DI_Wr_reg_bits(adr, val, start, len);
	return 1;
}
#else
unsigned int dimv3_RDMA_RD_BITS(unsigned int adr, unsigned int start,
			      unsigned int len)
{
	return Rd_reg_bits(adr, start, len);
}

unsigned int dimv3_RDMA_WR(unsigned int adr, unsigned int val)
{
	dimv3_DI_Wr(adr, val);
	if (get_dbg_data()->delay_cnt)
		udelay(get_dbg_data()->delay_cnt);
	return 1;
}

unsigned int dimv3_RDMA_RD(unsigned int adr)
{
	return Rd(adr);
}

unsigned int dimv3_RDMA_WR_BITS(unsigned int adr, unsigned int val,
			      unsigned int start, unsigned int len)
{
	dimv3_DI_Wr_reg_bits(adr, val, start, len);
	if (get_dbg_data()->delay_cnt)
		udelay(get_dbg_data()->delay_cnt);
	return 1;
}
#endif

void dimv3_set_di_flag(void)
{
	if (is_meson_txl_cpu()	||
	    is_meson_txlx_cpu()	||
	    is_meson_gxlx_cpu() ||
	    is_meson_txhd_cpu() ||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu() ||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()	||
	    is_meson_sm1_cpu()) {
		dimp_set(eDI_MP_mcpre_en, 1);
		mc_mem_alloc = true;
		dimp_set(eDI_MP_pulldown_enable, 0);
		//di_pre_rdma_enable = false;
		/*
		 * txlx atsc 1080i ei only will cause flicker
		 * when full to small win in home screen
		 */

		dimp_set(eDI_MP_di_vscale_skip_enable,
			 (is_meson_txlx_cpu()	||
			 is_meson_txhd_cpu()) ? 12 : 4);
		/*use_2_interlace_buff = is_meson_gxlx_cpu()?0:1;*/
		dimp_set(eDI_MP_use_2_interlace_buff,
			 is_meson_gxlx_cpu() ? 0 : 1);
		if (is_meson_txl_cpu()	||
		    is_meson_txlx_cpu()	||
		    is_meson_gxlx_cpu() ||
		    is_meson_txhd_cpu() ||
		    is_meson_g12a_cpu() ||
		    is_meson_g12b_cpu() ||
		    is_meson_tl1_cpu()	||
		    is_meson_tm2_cpu()	||
		    is_meson_sm1_cpu()) {
			dimp_set(eDI_MP_full_422_pack, 1);
		}

		if (dimp_get(eDI_MP_nr10bit_support)) {
			dimp_set(eDI_MP_di_force_bit_mode, 10);
		} else {
			dimp_set(eDI_MP_di_force_bit_mode, 8);
			dimp_set(eDI_MP_full_422_pack, 0);
		}

		dimp_set(eDI_MP_post_hold_line,
			 (is_meson_g12a_cpu()	||
			 is_meson_g12b_cpu()	||
			 is_meson_tl1_cpu()	||
			 is_meson_tm2_cpu()	||
			 is_meson_sm1_cpu()) ? 10 : 17);
	} else {
		/*post_hold_line = 8;*/	/*2019-01-10: from VLSI feijun*/
		dimp_set(eDI_MP_post_hold_line, 8);
		dimp_set(eDI_MP_mcpre_en, 0);
		dimp_set(eDI_MP_pulldown_enable, 0);
		//di_pre_rdma_enable = false;
		dimp_set(eDI_MP_di_vscale_skip_enable, 4);
		dimp_set(eDI_MP_use_2_interlace_buff, 0);
		dimp_set(eDI_MP_di_force_bit_mode, 8);
	}
	/*if (is_meson_tl1_cpu() || is_meson_tm2_cpu())*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		dimp_set(eDI_MP_pulldown_enable, 1);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		intr_mode = 3;

	pldn_dly = 2;
	pldn_dly1 = 2;


	get_ops_mtn()->mtn_int_combing_glbmot();
}

#if 0	/*move to di_sys.c*/
static const struct reserved_mem_ops rmem_di_ops = {
	.device_init	= rmem_di_device_init,
	.device_release = rmem_di_device_release,
};

static int __init rmem_di_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_di_ops;
/* rmem->priv = cma; */

	di_pr_info(
	"DI reserved memory: created CMA memory pool at %pa, size %ld MiB\n",
		&rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}

RESERVEDMEM_OF_DECLARE(di, "amlogic, di-mem", rmem_di_setup);
#endif

void dimv3_get_vpu_clkb(struct device *dev, struct di_dev_s *pdev)
{
	int ret = 0;
	unsigned int tmp_clk[2] = {0, 0};
	struct clk *vpu_clk = NULL;
	struct clk *clkb_tmp_comp = NULL;

	vpu_clk = clk_get(dev, "vpu_mux");
	if (IS_ERR(vpu_clk))
		PR_ERR("%s: get clk vpu error.\n", __func__);
	else
		clk_prepare_enable(vpu_clk);

	ret = of_property_read_u32_array(dev->of_node, "clock-range",
					 tmp_clk, 2);
	if (ret) {
		pdev->clkb_min_rate = 250000000;
		pdev->clkb_max_rate = 500000000;
	} else {
		pdev->clkb_min_rate = tmp_clk[0] * 1000000;
		pdev->clkb_max_rate = tmp_clk[1] * 1000000;
	}
	PR_INF("vpu clkb <%lu, %lu>\n", pdev->clkb_min_rate,
	       pdev->clkb_max_rate);
	#ifdef CLK_TREE_SUPPORT
	pdev->vpu_clkb = clk_get(dev, "vpu_clkb_composite");
	if (is_meson_tl1_cpu()) {
		clkb_tmp_comp = clk_get(dev, "vpu_clkb_tmp_composite");
		if (IS_ERR(clkb_tmp_comp)) {
			PR_ERR("clkb_tmp_comp error\n");
		} else {
			/*ary: this make clk from 500 to 666?*/
			if (!IS_ERR(vpu_clk))
				clk_set_parent(clkb_tmp_comp, vpu_clk);
		}
	}

	if (IS_ERR(pdev->vpu_clkb)) {
		PR_ERR("%s: get vpu clkb gate error.\n", __func__);
	} else {
		clk_set_rate(pdev->vpu_clkb, pdev->clkb_min_rate);
		PR_INF("get clkb rate:%ld\n", clk_get_rate(pdev->vpu_clkb));
	}

	#endif
}

module_param_named(invert_top_bot, invert_top_bot, int, 0664);

#ifdef SUPPORT_START_FRAME_HOLD
module_param_named(start_frame_hold_count, start_frame_hold_count, int, 0664);
#endif

#ifdef DET3D

MODULE_PARM_DESC(det3d_mode, "\n det3d_mode\n");
module_param(det3d_mode, uint, 0664);
#endif

module_param_array(di_stop_reg_addr, uint, &num_di_stop_reg_addr,
		   0664);

module_param_named(overturn, overturn, bool, 0664);

#ifdef DEBUG_SUPPORT
#ifdef RUN_DI_PROCESS_IN_IRQ
module_param_named(input2pre, input2pre, uint, 0664);
module_param_named(input2pre_buf_miss_count, input2pre_buf_miss_count,
		   uint, 0664);
module_param_named(input2pre_proc_miss_count, input2pre_proc_miss_count,
		   uint, 0664);
module_param_named(input2pre_miss_policy, input2pre_miss_policy, uint, 0664);
module_param_named(input2pre_throw_count, input2pre_throw_count, uint, 0664);
#endif
#ifdef SUPPORT_MPEG_TO_VDIN
module_param_named(mpeg2vdin_en, mpeg2vdin_en, int, 0664);
module_param_named(mpeg2vdin_flag, mpeg2vdin_flag, int, 0664);
#endif
//module_param_named(di_pre_rdma_enable, di_pre_rdma_enable, uint, 0664);
module_param_named(pldn_dly, pldn_dly, uint, 0644);
module_param_named(pldn_dly1, pldn_dly1, uint, 0644);
module_param_named(di_reg_unreg_cnt, di_reg_unreg_cnt, int, 0664);
module_param_named(bypass_pre, bypass_pre, int, 0664);
module_param_named(frame_count, frame_count, int, 0664);
#endif

int dimv3_seq_file_module_para_di(struct seq_file *seq)
{
	seq_puts(seq, "di---------------\n");

#ifdef DET3D
	seq_printf(seq, "%-15s:%d\n", "det3d_frame_cnt", det3d_frame_cnt);
#endif

#ifdef SUPPORT_START_FRAME_HOLD
	seq_printf(seq, "%-15s:%d\n", "start_frame_hold_count",
		   start_frame_hold_count);
#endif
	seq_printf(seq, "%-15s:%ld\n", "same_field_top_count",
		   same_field_top_count);
	seq_printf(seq, "%-15s:%ld\n", "same_field_bot_count",
		   same_field_bot_count);

	seq_printf(seq, "%-15s:%d\n", "overturn", overturn);

#ifdef DEBUG_SUPPORT
#ifdef RUN_DI_PROCESS_IN_IRQ
	seq_printf(seq, "%-15s:%d\n", "input2pre", input2pre);
	seq_printf(seq, "%-15s:%d\n", "input2pre_buf_miss_count",
		   input2pre_buf_miss_count);
	seq_printf(seq, "%-15s:%d\n", "input2pre_proc_miss_count",
		   input2pre_proc_miss_count);
	seq_printf(seq, "%-15s:%d\n", "input2pre_miss_policy",
		   input2pre_miss_policy);
	seq_printf(seq, "%-15s:%d\n", "input2pre_throw_count",
		   input2pre_throw_count);
#endif
#ifdef SUPPORT_MPEG_TO_VDIN

	seq_printf(seq, "%-15s:%d\n", "mpeg2vdin_en", mpeg2vdin_en);
	seq_printf(seq, "%-15s:%d\n", "mpeg2vdin_flag", mpeg2vdin_flag);
#endif
//	seq_printf(seq, "%-15s:%d\n", "di_pre_rdma_enable",
//		   di_pre_rdma_enable);
	seq_printf(seq, "%-15s:%d\n", "pldn_dly", pldn_dly);
	seq_printf(seq, "%-15s:%d\n", "pldn_dly1", pldn_dly1);
	seq_printf(seq, "%-15s:%d\n", "di_reg_unreg_cnt", di_reg_unreg_cnt);
	seq_printf(seq, "%-15s:%d\n", "bypass_pre", bypass_pre);
	seq_printf(seq, "%-15s:%d\n", "frame_count", frame_count);
#endif
/******************************/

#ifdef DET3D
	seq_printf(seq, "%-15s:%d\n", "det3d_mode", det3d_mode);
#endif
	return 0;
}

#if 0 /*move to di_sys.c*/
MODULE_DESCRIPTION("AMLOGIC DEINTERLACE driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("4.0.0");
#endif
