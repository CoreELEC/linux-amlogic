/* * Amlogic M2
 * frame buffer driver-----------Deinterlace
 * Author: Rain Zhang <rain.zhang@amlogic.com>
 * Copyright (C) 2010 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/canvas/canvas_mgr.h>
#include <linux/amlogic/tvin/tvin_v4l2.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/vpu.h>
#ifdef CONFIG_AML_RDMA
#include <linux/amlogic/rdma/rdma_mgr.h>
#endif
#include "register.h"
#include "deinterlace.h"
#include "deinterlace_pd.h"
#include "../amports/video.h"
#ifdef CONFIG_VSYNC_RDMA
#include "../amports/rdma.h"
#endif
#if defined(NEW_DI_TV)
#define ENABLE_SPIN_LOCK_ALWAYS
#endif
#ifdef DET3D
#include "detect3d.h"
#endif
#ifdef NEW_DI_V4
#include "dnr.h"
#endif

#define RUN_DI_PROCESS_IN_IRQ

#ifdef ENABLE_SPIN_LOCK_ALWAYS
static DEFINE_SPINLOCK(di_lock2);
#define di_lock_irqfiq_save(irq_flag, fiq_flag) \
	do { fiq_flag = 0; flags = flags; \
	     spin_lock_irqsave(&di_lock2, irq_flag); \
	} while (0)

#define di_unlock_irqfiq_restore(irq_flag, fiq_flag) \
	spin_unlock_irqrestore(&di_lock2, irq_flag);

#else
#define di_lock_irqfiq_save(irq_flag, fiq_flag) \
	do { flags = irq_flag; \
	     irq_flag = fiq_flag; \
	} while (0)
#define di_unlock_irqfiq_restore(irq_flag, fiq_flag) \
	do { flags = irq_flag; \
	     irq_flag = fiq_flag; \
	} while (0)
#endif

int mpeg2vdin_flag = 0;
module_param(mpeg2vdin_flag, int, 0664);
MODULE_PARM_DESC(mpeg2vdin_flag, "mpeg2vdin_en");

int mpeg2vdin_en = 0;
module_param(mpeg2vdin_en, int, 0664);
MODULE_PARM_DESC(mpeg2vdin_en, "mpeg2vdin_en");

static int wr_print_flag;
module_param(wr_print_flag, int, 0664);
MODULE_PARM_DESC(wr_print_flag, "wr_print_flag");

static int queue_print_flag = -1;
module_param(queue_print_flag, int, 0664);
MODULE_PARM_DESC(queue_print_flag, "queue_print_flag");

static int rdma_en;
module_param(rdma_en, int, 0664);
MODULE_PARM_DESC(rdma_en, "rdma_en");

static int di_reg_unreg_cnt = 10;
module_param(di_reg_unreg_cnt, int, 0664);
MODULE_PARM_DESC(di_reg_unreg_cnt, "di_reg_unreg_cnt");

static bool overturn;
module_param(overturn, bool, 0664);
MODULE_PARM_DESC(overturn, "overturn /disable reverse");

static bool check_start_drop_prog;
module_param(check_start_drop_prog, bool, 0664);
MODULE_PARM_DESC(check_start_drop_prog,
	"enable/disable progress start drop function");
bool mcpre_en = true;
module_param(mcpre_en, bool, 0664);
MODULE_PARM_DESC(mcpre_en, "enable/disable me in pre");

#ifdef NEW_DI_V4
static bool dnr_en = 1;
module_param(dnr_en, bool, 0664);
MODULE_PARM_DESC(dnr_en, "enable/disable dnr in pre");


#endif
static unsigned int di_pre_rdma_enable;

static bool full_422_pack;
static bool tff_bff_enable;
/* destory unnecessary frames befor display */
static int hold_video;
#define CHECK_VDIN_BUF_ERROR

#define DEVICE_NAME "deinterlace"
#define CLASS_NAME      "deinterlace"

char *vf_get_receiver_name(const char *provider_name);

static DEFINE_SPINLOCK(plist_lock);

static di_dev_t *de_devp;
static dev_t di_devno;
static struct class *di_clsp;

#define INIT_FLAG_NOT_LOAD 0x80
static const char version_s[] = "2016-12-18a";
static unsigned char boot_init_flag;
static int receiver_is_amvideo = 1;

static unsigned char new_keep_last_frame_enable;
static int bypass_state = 1;
static int bypass_prog = 1;
static int bypass_hd_prog;
static int bypass_4K;
static int bypass_interlace_output;
#ifdef CONFIG_AM_DEINTERLACE_SD_ONLY
static int bypass_hd = 1;
#else
static int bypass_hd;
#endif
static int bypass_superd = 1;
static int bypass_all;
/*1:enable bypass pre,ei only;
 * 2:debug force bypass pre,ei for post
 */
static int bypass_pre;
static int bypass_trick_mode = 1;
static int bypass_1080p;
static int bypass_3d = 1;
static int invert_top_bot;
static int skip_top_bot;/*1or2: may affect atv when bypass di*/
static char interlace_output_flag;
static int bypass_get_buf_threshold = 4;
static int bypass_hdmi_get_buf_threshold = 3;

static int post_hold_line = 17;/* for m8 1080i/50 output */
static int force_update_post_reg = 0x10;
/*
 * bit[4]
 * 1 call process_fun every vsync;
 * 0 call process_fun when toggle frame (early_process_fun is called)
 * bit[3:0] set force_update_post_reg = 1: only update post reg in
 * vsync for one time
 * set force_update_post_reg = 2: always update post reg in vsync
 */
static int bypass_post;
bool post_wr_en;
unsigned int post_wr_surpport;
static int bypass_post_state;
static int bypass_dynamic;
static int bypass_dynamic_flag;
static int vdin_source_flag;
static int update_post_reg_count = 1;
static int timeout_miss_policy;
/* 0, use di_wr_buf still;
 * 1, call pre_de_done_buf_clear to clear di_wr_buf;
 * 2, do nothing*/

static int post_ready_empty_count;

static int force_width;
static int force_height;
/* add avoid vframe put/get error */
static int di_blocking;
/*
 * bit[2]: enable bypass all when skip
 * bit[1:0]: enable bypass post when skip
 */
static int di_vscale_skip_enable;

/* 0: not surpport nr10bit, 1: surpport nr10bit */
static unsigned int nr10bit_surpport;

#ifdef RUN_DI_PROCESS_IN_IRQ
/*
 * di_process() run in irq,
 * di_reg_process(), di_unreg_process() run in kernel thread
 * di_reg_process_irq(), di_unreg_process_irq() run in irq
 * di_vf_put(), di_vf_peek(), di_vf_get() run in irq
 * di_receiver_event_fun() run in task or irq
 */
/*
 * important:
 * to set input2pre, VFRAME_EVENT_PROVIDER_VFRAME_READY of
 * vdin should be sent in irq
 */
#ifdef NEW_DI_V1
#undef CHECK_DI_DONE
#else
#define CHECK_DI_DONE
#endif

#ifdef NEW_DI_TV
static int input2pre = 1;
/*false:process progress by field;
 * true: process progress by frame with 2 interlace buffer
 */
#else
static int input2pre;
#endif
static int use_2_interlace_buff;
static int input2pre_buf_miss_count;
static int input2pre_proc_miss_count;
static int input2pre_throw_count;
static int static_pic_threshold = 10;
#ifdef NEW_DI_V1
static int input2pre_miss_policy;
/* 0, do not force pre_de_busy to 0, use di_wr_buf after de_irq happen;
 * 1, force pre_de_busy to 0 and call pre_de_done_buf_clear to clear di_wr_buf
 */
#else
static int input2pre_miss_policy;
/* 0, do not force pre_de_busy to 0, use di_wr_buf after de_irq happen;
 * 1, force pre_de_busy to 0 and call pre_de_done_buf_clear to clear di_wr_buf
 */
#endif
#else
#ifdef NEW_DI_TV
static int use_2_interlace_buff = 1;
/*false:process progress by field;
 * bit0: process progress by frame with 2 interlace buffer
 * bit1: temp add debug for 3d process FA,1:bit0 force to 1;
 */
#else
static int use_2_interlace_buff;
#endif
#endif
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
static int prog_proc_config = (1 << 5) | (1 << 1) | 1;
/*
 * for source include both progressive and interlace pictures,
 * always use post_di module for blending
 */
#define is_handle_prog_frame_as_interlace(vframe) \
	(((prog_proc_config & 0x30) == 0x20) && \
	 ((vframe->type & VIDTYPE_VIU_422) == 0))

static int pulldown_detect = 0x10;
static int skip_wrong_field = 1;
static int frame_count;
static int provider_vframe_level;
static int disp_frame_count;
static int start_frame_drop_count = 2;
/* static int start_frame_hold_count = 0; */

static int force_trig_cnt;
static int di_process_cnt;
static int video_peek_cnt;
static unsigned long reg_unreg_timeout_cnt;
int di_vscale_skip_count = 0;
int di_vscale_skip_count_real = 0;
static int vpp_3d_mode;
#ifdef DET3D
bool det3d_en = false;
static unsigned int det3d_mode;
static void set3d_view(enum tvin_trans_fmt trans_fmt, struct vframe_s *vf);
#endif

static int force_duration_0;

static int use_reg_cfg = 1;

static uint init_flag;/*flag for di buferr*/
static uint mem_flag;/*flag for mem alloc*/
static unsigned int reg_flag;/*flag for vframe reg/unreg*/
static unsigned int unreg_cnt;/*cnt for vframe unreg*/
static unsigned int reg_cnt;/*cnt for vframe reg*/
static unsigned char active_flag;
static unsigned char recovery_flag;
static unsigned int force_recovery = 1;
static unsigned int force_recovery_count;
static unsigned int recovery_log_reason;
static unsigned int recovery_log_queue_idx;
static struct di_buf_s *recovery_log_di_buf;

#define VFM_NAME "deinterlace"

static long same_field_top_count;
static long same_field_bot_count;
static long same_w_r_canvas_count;
static long pulldown_count;
static long pulldown_buffer_mode = 1;
/* bit 0:
 * 0, keep 3 buffers in pre_ready_list for checking;
 * 1, keep 4 buffers in pre_ready_list for checking;
 */
static long pulldown_win_mode = 0x11111;
static int same_field_source_flag_th = 60;
int nr_hfilt_en = 0;

static int pre_hold_line = 10;
static int pre_urgent = 1;
static int post_urgent = 1;

/*pre process speed debug */
static int pre_process_time;
static int pre_process_time_force;
/**/
#define USED_LOCAL_BUF_MAX 3
static int used_local_buf_index[USED_LOCAL_BUF_MAX];
static int used_post_buf_index = -1;

static int di_receiver_event_fun(int type, void *data, void *arg);
static void di_uninit_buf(void);
static unsigned char is_bypass(vframe_t *vf_in);
static void log_buffer_state(unsigned char *tag);
/* static void put_get_disp_buf(void); */

static const
struct vframe_receiver_op_s di_vf_receiver = {
	.event_cb	= di_receiver_event_fun
};

static struct vframe_receiver_s di_vf_recv;

static vframe_t *di_vf_peek(void *arg);
static vframe_t *di_vf_get(void *arg);
static void di_vf_put(vframe_t *vf, void *arg);
static int di_event_cb(int type, void *data, void *private_data);
static int di_vf_states(struct vframe_states *states, void *arg);
static void di_process(void);
static void di_reg_process(void);
static void di_reg_process_irq(void);
static void di_unreg_process(void);
static void di_unreg_process_irq(void);

static const
struct vframe_operations_s deinterlace_vf_provider = {
	.peek		= di_vf_peek,
	.get		= di_vf_get,
	.put		= di_vf_put,
	.event_cb	= di_event_cb,
	.vf_states	= di_vf_states,
};

static struct vframe_provider_s di_vf_prov;

int di_sema_init_flag = 0;
static struct semaphore di_sema;
#ifdef USE_HRTIMER
static struct tasklet_struct di_pre_tasklet;
#endif
void trigger_pre_di_process(char idx)
{
	if (di_sema_init_flag == 0)
		return;

	if (idx != 'p')
		log_buffer_state((idx == 'i') ? "irq" : ((idx == 'p') ?
			"put" : ((idx == 'r') ? "rdy" : "oth")));

#if (defined RUN_DI_PROCESS_IN_IRQ)
#ifdef USE_HRTIMER
	/* tasklet_hi_schedule(&di_pre_tasklet); */
	tasklet_schedule(&di_pre_tasklet);
	de_devp->jiffy = jiffies_64;
#else
	aml_write_cbus(ISA_TIMERC, 1);
#endif
	/* trigger di_reg_process and di_unreg_process */
	if ((idx != 'p') && (idx != 'i'))
		up(&di_sema);
#endif
}

static unsigned int di_printk_flag;
#define DI_PRE_INTERVAL         (HZ / 100)


static void set_output_mode_info(void)
{
	const struct vinfo_s *info;

	info = get_current_vinfo();

	if (info) {
		if ((strncmp(info->name, "480cvbs", 7) == 0) ||
		    (strncmp(info->name, "576cvbs", 7) == 0) ||
		    (strncmp(info->name, "480i", 4) == 0) ||
		    (strncmp(info->name, "576i", 4) == 0) ||
		    (strncmp(info->name, "1080i", 5) == 0))
			interlace_output_flag = 1;
		else
			interlace_output_flag = 0;
	}
}

static int
display_notify_callback_v(struct notifier_block *block,
			  unsigned long cmd, void *para)
{
	if (cmd != VOUT_EVENT_MODE_CHANGE)
		return 0;

	set_output_mode_info();
	return 0;
}

static struct
notifier_block display_mode_notifier_nb_v = {
	.notifier_call	= display_notify_callback_v,
};

/************For Write register**********************/
static unsigned int di_stop_reg_flag;
static unsigned int num_di_stop_reg_addr = 4;
static unsigned int di_stop_reg_addr[4] = {0};

unsigned int is_need_stop_reg(unsigned int addr)
{
	int idx = 0;

	if (di_stop_reg_flag) {
		for (idx = 0; idx < num_di_stop_reg_addr; idx++) {
			if (addr == di_stop_reg_addr[idx]) {
				pr_dbg("stop write addr: %x\n", addr);
				return 1;
			}
		}
	}

	return 0;
}

void DI_Wr(unsigned int addr, unsigned int val)
{
	if (is_need_stop_reg(addr))
		return;

	Wr(addr, val);
	return;
}

void DI_Wr_reg_bits(unsigned int adr, unsigned int val,
		unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(adr))
		return;

	Wr_reg_bits(adr, val, start, len);
	return;
}

void DI_VSYNC_WR_MPEG_REG(unsigned int addr, unsigned int val)
{
	if (is_need_stop_reg(addr))
		return;

	VSYNC_WR_MPEG_REG(addr, val);
	return;
}

void DI_VSYNC_WR_MPEG_REG_BITS(unsigned int addr, unsigned int val,
	unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(addr))
		return;

	VSYNC_WR_MPEG_REG_BITS(addr, val, start, len);
	return;
}

/**********************************/

/*****************************
 *	 di attr management :
 *	 enable
 *	 mode
 *	 reg
 ******************************/
/*config attr*/
static ssize_t
show_config(struct device *dev,
	    struct device_attribute *attr, char *buf)
{
	int pos = 0;

	return pos;
}

static ssize_t
store_config(struct device *dev, struct device_attribute *attr, const char *buf,
	     size_t count);

static void dump_state(void);
static void dump_di_buf(struct di_buf_s *di_buf);
static void dump_pool(int index);
static void dump_vframe(vframe_t *vf);
static void force_source_change(void);

#define DI_RUN_FLAG_RUN                         0
#define DI_RUN_FLAG_PAUSE                       1
#define DI_RUN_FLAG_STEP                        2
#define DI_RUN_FLAG_STEP_DONE           3

static int run_flag = DI_RUN_FLAG_RUN;
static int pre_run_flag = DI_RUN_FLAG_RUN;
static int dump_state_flag;

static ssize_t
store_dbg(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count)
{
	if (strncmp(buf, "buf", 3) == 0) {
		struct di_buf_s *di_buf_tmp = 0;
		if (kstrtoul(buf + 3, 16, (unsigned long *)&di_buf_tmp))
			return count;
		dump_di_buf(di_buf_tmp);
	} else if (strncmp(buf, "vframe", 6) == 0) {
		vframe_t *vf = 0;
		if (kstrtoul(buf + 6, 16, (unsigned long *)&vf))
			return count;
		dump_vframe(vf);
	} else if (strncmp(buf, "pool", 4) == 0) {
		unsigned long idx = 0;
		if (kstrtoul(buf + 4, 10, &idx))
			return count;
		dump_pool(idx);
	} else if (strncmp(buf, "state", 4) == 0) {
		dump_state();
	} else if (strncmp(buf, "bypass_prog", 11) == 0) {
		force_source_change();
		if (buf[11] == '1')
			bypass_prog = 1;
		else
			bypass_prog = 0;
	} else if (strncmp(buf, "prog_proc_config", 16) == 0) {
		if (buf[16] == '1')
			prog_proc_config = 1;
		else
			prog_proc_config = 0;
	} else if (strncmp(buf, "init_flag", 9) == 0) {
		if (buf[9] == '1')
			init_flag = 1;
		else
			init_flag = 0;
	} else if (strncmp(buf, "run", 3) == 0) {
		/* timestamp_pcrscr_enable(1); */
		run_flag = DI_RUN_FLAG_RUN;
	} else if (strncmp(buf, "pause", 5) == 0) {
		/* timestamp_pcrscr_enable(0); */
		run_flag = DI_RUN_FLAG_PAUSE;
	} else if (strncmp(buf, "step", 4) == 0) {
		run_flag = DI_RUN_FLAG_STEP;
	} else if (strncmp(buf, "prun", 4) == 0) {
		pre_run_flag = DI_RUN_FLAG_RUN;
	} else if (strncmp(buf, "ppause", 6) == 0) {
		pre_run_flag = DI_RUN_FLAG_PAUSE;
	} else if (strncmp(buf, "pstep", 5) == 0) {
		pre_run_flag = DI_RUN_FLAG_STEP;
	} else if (strncmp(buf, "dumpreg", 7) == 0) {
		unsigned int i = 0;
		pr_info("----dump di reg----\n");
		for (i = 0; i < 255; i++) {
			if (i == 0x45)
				pr_info("----nr reg----");
			if (i == 0x80)
				pr_info("----3d reg----");
			if (i == 0x9e)
				pr_info("---nr reg done---");
			if (i == 0x9c)
				pr_info("---3d reg done---");
			pr_info("[0x%x][0x%x]=0x%x\n",
				0xd0100000 + ((0x1700 + i) << 2),
				0x1700 + i, Rd(0x1700 + i));
		}
		pr_info("----dump mcdi reg----\n");
		for (i = 0; i < 201; i++)
			pr_info("[0x%x][0x%x]=0x%x\n",
				0xd0100000 + ((0x2f00 + i) << 2),
				0x2f00 + i, Rd(0x2f00 + i));
		pr_info("----dump pulldown reg----\n");
		for (i = 0; i < 26; i++)
			pr_info("[0x%x][0x%x]=0x%x\n",
				0xd0100000 + ((0x2fd0 + i) << 2),
				0x2fd0 + i, Rd(0x2fd0 + i));
		pr_info("----dump bit mode reg----\n");
		for (i = 0; i < 4; i++)
			pr_info("[0x%x][0x%x]=0x%x\n",
				0xd0100000 + ((0x20a7 + i) << 2),
				0x20a7 + i, Rd(0x20a7 + i));
		pr_info("[0x%x][0x%x]=0x%x\n",
			0xd0100000 + (0x2022 << 2),
			0x2022, Rd(0x2022));
		pr_info("[0x%x][0x%x]=0x%x\n",
			0xd0100000 + (0x17c1 << 2),
			0x17c1, Rd(0x17c1));
		pr_info("[0x%x][0x%x]=0x%x\n",
			0xd0100000 + (0x17c2 << 2),
			0x17c2, Rd(0x17c2));
		pr_info("[0x%x][0x%x]=0x%x\n",
			0xd0100000 + (0x1aa7 << 2),
			0x1aa7, Rd(0x1aa7));
		pr_info("----dump dnr reg----\n");
		for (i = 0; i < 29; i++)
			pr_info("[0x%x][0x%x]=0x%x\n",
				0xd0100000 + ((0x2d00 + i) << 2),
				0x2d00 + i, Rd(0x2d00 + i));
		pr_info("----dump if0 reg----\n");
		for (i = 0; i < 26; i++)
			pr_info("[0x%x][0x%x]=0x%x\n",
				0xd0100000 + ((0x1a60 + i) << 2),
				0x1a50 + i, Rd(0x1a50 + i));
		pr_info("----dump if2 reg----\n");
		for (i = 0; i < 29; i++)
			pr_info("[0x%x][0x%x]=0x%x\n",
				0xd0100000 + ((0x2010 + i) << 2),
				0x2010 + i, Rd(0x2010 + i));
		pr_info("----dump reg done----\n");
	} else if (strncmp(buf, "robust_test", 11) == 0) {
		recovery_flag = 1;
	} else if (strncmp(buf, "recycle_buf", 11) == 0) {
		recycle_keep_buffer();
	} else {
		pr_info("DI no support cmd!!!\n");
	}

	return count;
}
#ifdef NEW_DI_V1
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
unsigned int di_debug_flag = 0x10;/* enable rdma even di bypassed */
static unsigned char *di_log_buf;
static unsigned int di_log_wr_pos;
static unsigned int di_log_rd_pos;
static unsigned int di_log_buf_size;
static unsigned int di_printk_flag;
unsigned int di_log_flag = 0;
unsigned int buf_state_log_threshold = 16;
unsigned int buf_state_log_start = 0;
/*  set to 1 by condition of "post_ready count < buf_state_log_threshold",
 * reset to 0 by set buf_state_log_threshold as 0 */

static DEFINE_SPINLOCK(di_print_lock);

#define PRINT_TEMP_BUF_SIZE 128

int di_print_buf(char *buf, int len)
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
#if 0
#define di_print printk
#else
int di_print(const char *fmt, ...)
{
	va_list args;
	int avail = PRINT_TEMP_BUF_SIZE;
	char buf[PRINT_TEMP_BUF_SIZE];
	int pos, len = 0;

	if (di_printk_flag & 1) {
		if (di_log_flag & DI_LOG_PRECISE_TIMESTAMP)
			pr_dbg("%u:", aml_read_cbus(ISA_TIMERE));
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
		return 0;
	}

	if (di_log_buf_size == 0)
		return 0;

/* len += snprintf(buf+len, avail-len, "%d:",log_seq++); */
	if (di_log_flag & DI_LOG_TIMESTAMP)
		len += snprintf(buf + len, avail - len, "%u:",
			jiffies_to_msecs(jiffies_64));
	else if (di_log_flag & DI_LOG_PRECISE_TIMESTAMP)
		len += snprintf(buf + len, avail - len, "%u:",
			aml_read_cbus(ISA_TIMERE));

	va_start(args, fmt);
	len += vsnprintf(buf + len, avail - len, fmt, args);
	va_end(args);

	if ((avail - len) <= 0)
		buf[PRINT_TEMP_BUF_SIZE - 1] = '\0';

	pos = di_print_buf(buf, len);
/* pr_dbg("di_print:%d %d\n", di_log_wr_pos, di_log_rd_pos); */
	return pos;
}
#endif

static ssize_t read_log(char *buf)
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

static ssize_t
show_log(struct device *dev, struct device_attribute *attr, char *buf)
{
	return read_log(buf);
}

static ssize_t
store_log(struct device *dev,
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
		di_printk_flag = tmp;
	} else {
		di_print("%s", buf);
	}
	return 16;
}

struct di_param_s {
	char *name;
	uint *param;
	int (*proc_fun)(void);
};
#define di_param_t struct di_param_s

di_param_t di_params[] = {
	{"di_mtn_1_ctrl1", &di_mtn_1_ctrl1, NULL},
	{"win0_pd_frame_diff_chg_th", &(win_pd_th[0].frame_diff_chg_th), NULL},
	{ "win0_pd_frame_diff_num_chg_th",
	  &(win_pd_th[0].frame_diff_num_chg_th), NULL },
	{"win0_pd_field_diff_chg_th", &(win_pd_th[0].field_diff_chg_th), NULL},
	{ "win0_pd_field_diff_num_chg_th",
	  &(win_pd_th[0].field_diff_num_chg_th), NULL },
	{ "win0_pd_frame_diff_skew_th",
	  &(win_pd_th[0].frame_diff_skew_th), NULL },
	{ "win0_pd_frame_diff_num_skew_th",
	  &(win_pd_th[0].frame_diff_num_skew_th), NULL },
	{ "win0_pd_field_diff_num_th",
	  &(win_pd_th[0].field_diff_num_th), NULL },
	{"win1_pd_frame_diff_chg_th", &(win_pd_th[1].frame_diff_chg_th), NULL},
	{ "win1_pd_frame_diff_num_chg_th",
	  &(win_pd_th[1].frame_diff_num_chg_th), NULL },
	{"win1_pd_field_diff_chg_th", &(win_pd_th[1].field_diff_chg_th), NULL},
	{ "win1_pd_field_diff_num_chg_th",
	  &(win_pd_th[1].field_diff_num_chg_th), NULL },
	{ "win1_pd_frame_diff_skew_th",
	  &(win_pd_th[1].frame_diff_skew_th), NULL },
	{ "win1_pd_frame_diff_num_skew_th",
	  &(win_pd_th[1].frame_diff_num_skew_th), NULL },
	{"win1_pd_field_diff_num_th", &(win_pd_th[1].field_diff_num_th), NULL},

	{"win2_pd_frame_diff_chg_th", &(win_pd_th[2].frame_diff_chg_th), NULL},
	{ "win2_pd_frame_diff_num_chg_th",
	  &(win_pd_th[2].frame_diff_num_chg_th), NULL },
	{"win2_pd_field_diff_chg_th", &(win_pd_th[2].field_diff_chg_th), NULL},
	{ "win2_pd_field_diff_num_chg_th",
	  &(win_pd_th[2].field_diff_num_chg_th), NULL },
	{ "win2_pd_frame_diff_skew_th",
	  &(win_pd_th[2].frame_diff_skew_th), NULL },
	{ "win2_pd_frame_diff_num_skew_th",
	  &(win_pd_th[2].frame_diff_num_skew_th), NULL },
	{"win2_pd_field_diff_num_th", &(win_pd_th[2].field_diff_num_th), NULL},

	{"win3_pd_frame_diff_chg_th", &(win_pd_th[3].frame_diff_chg_th), NULL},
	{ "win3_pd_frame_diff_num_chg_th",
	  &(win_pd_th[3].frame_diff_num_chg_th), NULL },
	{"win3_pd_field_diff_chg_th", &(win_pd_th[3].field_diff_chg_th), NULL},
	{ "win3_pd_field_diff_num_chg_th",
	  &(win_pd_th[3].field_diff_num_chg_th), NULL },
	{ "win3_pd_frame_diff_skew_th",
	  &(win_pd_th[3].frame_diff_skew_th), NULL },
	{ "win3_pd_frame_diff_num_skew_th",
	  &(win_pd_th[3].frame_diff_num_skew_th), NULL },
	{ "win3_pd_field_diff_num_th",
	  &(win_pd_th[3].field_diff_num_th), NULL },

	{"win4_pd_frame_diff_chg_th", &(win_pd_th[4].frame_diff_chg_th), NULL},
	{ "win4_pd_frame_diff_num_chg_th",
	  &(win_pd_th[4].frame_diff_num_chg_th), NULL },
	{ "win4_pd_field_diff_chg_th",
	  &(win_pd_th[4].field_diff_chg_th), NULL },
	{ "win4_pd_field_diff_num_chg_th",
	  &(win_pd_th[4].field_diff_num_chg_th), NULL },
	{ "win4_pd_frame_diff_skew_th",
	  &(win_pd_th[4].frame_diff_skew_th), NULL },
	{ "win4_pd_frame_diff_num_skew_th",
	  &(win_pd_th[4].frame_diff_num_skew_th), NULL },
	{ "",				    NULL },
};

static ssize_t
show_parameters(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i = 0;
	int len = 0;

	for (i = 0; di_params[i].param; i++)
		len += sprintf(buf + len, "%s=%08x\n", di_params[i].name,
			*(di_params[i].param));
	return len;
}

static char tmpbuf[128];
static ssize_t
store_parameters(struct device *dev,
		 struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int i = 0, j;
	unsigned long tmp = 0;

	while ((buf[i]) && (buf[i] != '=') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;
	/* pr_dbg("%s\n", tmpbuf); */
	if (strcmp("reset", tmpbuf) == 0) {
		/* reset_di_para(); */
		di_print("reset param\n");
	} else {
		for (j = 0; di_params[j].param; j++) {
			if (strcmp(di_params[j].name, tmpbuf) == 0) {
				if (kstrtoul(buf + i + 1, 16, &tmp))
					return count;
				*(di_params[j].param) = tmp;
				if (di_params[j].proc_fun)
					di_params[j].proc_fun();

				di_print("set %s=%lx\n",
					di_params[j].name, tmp);
				break;
			}
		}
	}
	return count;
}

struct di_status_s {
	char *name;
	uint *status;
};
#define di_status_t struct di_status_s
di_status_t di_status[] = {
	{ "active", &init_flag			 },
	{ "",	    NULL			 }
};

static ssize_t
show_status(struct device *dev,
	    struct device_attribute *attr,
	    char *buf)
{
	int i = 0;
	int len = 0;

	di_print("%s\n", __func__);
	for (i = 0; di_status[i].status; i++)
		len += sprintf(buf + len, "%s=%08x\n", di_status[i].name,
			*(di_status[i].status));
	return len;
}

static ssize_t
show_vframe_status(struct device *dev,
		   struct device_attribute *attr,
		   char *buf)
{
	int ret = 0;
	struct vframe_states states;
	struct vframe_provider_s *vfp;

	vfp = vf_get_provider(VFM_NAME);
	if (vfp && vfp->ops && vfp->ops->vf_states)
		ret = vfp->ops->vf_states(&states, vfp->op_arg);

	if (ret == 0) {
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

	return ret;
}

static void parse_cmd_params(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[2] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	strcat(delim1, delim2);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

struct pd_param_s {
	char *name;
	int *addr;
};
struct FlmDectRes dectres;
struct sFlmSftPar pd_param;
static struct pd_param_s pd_params[] = {
	{ "sFrmDifAvgRat",
	  &(pd_param.sFrmDifAvgRat)},
	{ "sFrmDifLgTDif",
	  &(pd_param.sFrmDifLgTDif) },
	{ "sF32StpWgt01",
	  &(pd_param.sF32StpWgt01) },
	{ "sF32StpWgt02",
	  &(pd_param.sF32StpWgt02) },
	{ "sF32DifLgRat",
	  &(pd_param.sF32DifLgRat) },
	{ "sFlm2MinAlpha",
	  &(pd_param.sFlm2MinAlpha) },
	{ "sFlm2MinBelta",
	  &(pd_param.sFlm2MinBelta) },
	{ "sFlm20ftAlpha",
	  &(pd_param.sFlm20ftAlpha) },
	{ "sFlm2LgDifThd",
	  &(pd_param.sFlm2LgDifThd) },
	{ "sFlm2LgFlgThd",
	  &(pd_param.sFlm2LgFlgThd) },
	{ "sF32Dif01A1",
	  &(pd_param.sF32Dif01A1)   },
	{ "sF32Dif01T1",
	  &(pd_param.sF32Dif01T1)   },
	{ "sF32Dif01A2",
	  &(pd_param.sF32Dif01A2)   },
	{ "sF32Dif01T2",
	  &(pd_param.sF32Dif01T2)   },
	{ "rCmbRwMinCt0",
	  &(pd_param.rCmbRwMinCt0)  },
	{ "rCmbRwMinCt1",
	  &(pd_param.rCmbRwMinCt1)  },
	{ "mPstDlyPre",
	  &(pd_param.mPstDlyPre)    },
	{ "mNxtDlySft",
	  &(pd_param.mNxtDlySft)    },
	{ "sF32Dif02M0",
	  &(pd_param.sF32Dif02M0)   },        /* mpeg-4096, cvbs-8192 */
	{ "sF32Dif02M1",
	  &(pd_param.sF32Dif02M1)   },        /* mpeg-4096, cvbs-8192 */
	{ "",		  NULL          }
};

static ssize_t pd_param_store(struct device *dev,
			      struct device_attribute *attr, const char *buff,
			      size_t count)
{
	int i = 0;
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;

	buf_orig = kstrdup(buff, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));
	for (i = 0; pd_params[i].addr; i++) {
		if (!strcmp(parm[0], pd_params[i].name)) {
			rc = kstrtoint(parm[1], 10, &value);
			*(pd_params[i].addr) = value;
			pr_dbg("%s=%d.\n", pd_params[i].name, value);
		}
	}

	return count;
}

static ssize_t pd_param_show(struct device *dev,
			     struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;
	int i = 0;

	for (i = 0; pd_params[i].addr; i++)
		len += sprintf(buff + len, "%s=%d.\n",
			pd_params[i].name, *pd_params[i].addr);

	len += sprintf(buff + len, "\npulldown detection result:\n");
	len += sprintf(buff + len, "rCmb32Spcl=%u.\n", dectres.rCmb32Spcl);
	len += sprintf(buff + len, "rPstCYWnd0 s=%u.\n", dectres.rPstCYWnd0[0]);
	len += sprintf(buff + len, "rPstCYWnd0 e=%u.\n", dectres.rPstCYWnd0[1]);
	len += sprintf(buff + len, "rPstCYWnd0 b=%u.\n", dectres.rPstCYWnd0[2]);

	len += sprintf(buff + len, "rPstCYWnd1 s=%u.\n", dectres.rPstCYWnd1[0]);
	len += sprintf(buff + len, "rPstCYWnd1 e=%u.\n", dectres.rPstCYWnd1[1]);
	len += sprintf(buff + len, "rPstCYWnd1 b=%u.\n", dectres.rPstCYWnd1[2]);

	len += sprintf(buff + len, "rPstCYWnd2 s=%u.\n", dectres.rPstCYWnd2[0]);
	len += sprintf(buff + len, "rPstCYWnd2 e=%u.\n", dectres.rPstCYWnd2[1]);
	len += sprintf(buff + len, "rPstCYWnd2 b=%u.\n", dectres.rPstCYWnd2[2]);

	len += sprintf(buff + len, "rPstCYWnd3 s=%u.\n", dectres.rPstCYWnd3[0]);
	len += sprintf(buff + len, "rPstCYWnd3 e=%u.\n", dectres.rPstCYWnd3[1]);
	len += sprintf(buff + len, "rPstCYWnd3 b=%u.\n", dectres.rPstCYWnd3[2]);

	len += sprintf(buff + len, "rPstCYWnd4 s=%u.\n", dectres.rPstCYWnd4[0]);
	len += sprintf(buff + len, "rPstCYWnd4 e=%u.\n", dectres.rPstCYWnd4[1]);
	len += sprintf(buff + len, "rPstCYWnd4 b=%u.\n", dectres.rPstCYWnd4[2]);

	len += sprintf(buff + len, "rFlmPstGCm=%u.\n", dectres.rFlmPstGCm);
	len += sprintf(buff + len, "rFlmSltPre=%u.\n", dectres.rFlmSltPre);
	len += sprintf(buff + len, "rFlmPstMod=%d.\n", dectres.rFlmPstMod);
	len += sprintf(buff + len, "rF22Flag=%d.\n", dectres.rF22Flag);
	return len;
}

static ssize_t show_tvp_region(struct device *dev,
			     struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;

	len = sprintf(buff, "segment DI:%lx - %lx (size:0x%x)\n",
			de_devp->mem_start,
			de_devp->mem_start + de_devp->mem_size - 1,
			de_devp->mem_size);
	return len;
}


static DEVICE_ATTR(pd_param, 0664, pd_param_show, pd_param_store);

static ssize_t store_dump_mem(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t len);
static DEVICE_ATTR(config, 0664, show_config, store_config);
static DEVICE_ATTR(parameters, 0664, show_parameters, store_parameters);
static DEVICE_ATTR(debug, 0222, NULL, store_dbg);
static DEVICE_ATTR(dump_pic, 0222, NULL, store_dump_mem);
static DEVICE_ATTR(log, 0664, show_log, store_log);
static DEVICE_ATTR(status, 0444, show_status, NULL);
static DEVICE_ATTR(provider_vframe_status, 0444, show_vframe_status, NULL);
static DEVICE_ATTR(tvp_region, 0444, show_tvp_region, NULL);
/***************************
* di buffer management
***************************/
#define MAX_IN_BUF_NUM            20
#define MAX_LOCAL_BUF_NUM         12
#define MAX_POST_BUF_NUM          16

#define VFRAME_TYPE_IN                  1
#define VFRAME_TYPE_LOCAL               2
#define VFRAME_TYPE_POST                3
#define VFRAME_TYPE_NUM                 3

static char *vframe_type_name[] = {
	"", "di_buf_in", "di_buf_loc", "di_buf_post"
};

#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
static unsigned int default_width = 720;
static unsigned int default_height = 576;
#else
static unsigned int default_width = 1920;
static unsigned int default_height = 1080;
#endif
static int local_buf_num;

/*
 * progressive frame process type config:
 * 0, process by field;
 * 1, process by frame (only valid for vdin source whose
 * width/height does not change)
 */
static vframe_t *vframe_in[MAX_IN_BUF_NUM];
static vframe_t vframe_in_dup[MAX_IN_BUF_NUM];
static vframe_t vframe_local[MAX_LOCAL_BUF_NUM * 2];
static vframe_t vframe_post[MAX_POST_BUF_NUM];
static struct di_buf_s *cur_post_ready_di_buf;

static struct di_buf_s di_buf_local[MAX_LOCAL_BUF_NUM * 2];
static struct di_buf_s di_buf_in[MAX_IN_BUF_NUM];
static struct di_buf_s di_buf_post[MAX_POST_BUF_NUM];

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
#define QUEUE_LOCAL_FREE           0
#define QUEUE_IN_FREE              1
#define QUEUE_PRE_READY            2
#define QUEUE_POST_FREE            3
#define QUEUE_POST_READY           4
#define QUEUE_RECYCLE              5
#define QUEUE_DISPLAY              6
#define QUEUE_TMP                  7
#define QUEUE_POST_DOING           8
#define QUEUE_NUM                  9

#ifdef USE_LIST
static struct
list_head local_free_list_head = LIST_HEAD_INIT(local_free_list_head);
/* vframe is local_vframe */
static struct
list_head in_free_list_head = LIST_HEAD_INIT(in_free_list_head);
/* vframe is NULL */
static struct
list_head pre_ready_list_head = LIST_HEAD_INIT(pre_ready_list_head);
/* vframe is either local_vframe or in_vframe */
static struct
list_head recycle_list_head = LIST_HEAD_INIT(recycle_list_head);
/* vframe is either local_vframe or in_vframe */
static struct
list_head post_free_list_head = LIST_HEAD_INIT(post_free_list_head);
/* vframe is post_vframe */
static struct
list_head post_ready_list_head = LIST_HEAD_INIT(post_ready_list_head);
/* vframe is post_vframe */
static struct
list_head display_list_head = LIST_HEAD_INIT(display_list_head);
/* vframe sent out for displaying */
static struct list_head *list_head_array[QUEUE_NUM];

#define get_di_buf_head(queue_idx) \
	list_first_entry(list_head_array[queue_idx], struct di_buf_s, list)

static void queue_init(int local_buffer_num)
{
	list_head_array[QUEUE_LOCAL_FREE] = &local_free_list_head;
	list_head_array[QUEUE_IN_FREE] = &in_free_list_head;
	list_head_array[QUEUE_PRE_READY] = &pre_ready_list_head;
	list_head_array[QUEUE_POST_FREE] = &post_free_list_head;
	list_head_array[QUEUE_POST_READY] = &post_ready_list_head;
	list_head_array[QUEUE_RECYCLE] = &recycle_list_head;
	list_head_array[QUEUE_DISPLAY] = &display_list_head;
	list_head_array[QUEUE_TMP] = &post_ready_list_head;
}

static bool queue_empty(int queue_idx)
{
	return list_empty(list_head_array[queue_idx]);
}

static void queue_out(struct di_buf_s *di_buf)
{
	list_del(&(di_buf->list));
}

static void queue_in(struct di_buf_s *di_buf, int queue_idx)
{
	list_add_tail(&(di_buf->list), list_head_array[queue_idx]);
}

static int list_count(int queue_idx)
{
	int count = 0;
	struct di_buf_s *p = NULL, *ptmp;

	list_for_each_entry_safe(p, ptmp, list_head_array[queue_idx], list) {
		count++;
	}
	return count;
}

#define queue_for_each_entry(di_buf, ptm, queue_idx, list)      \
	list_for_each_entry_safe(di_buf, ptmp, list_head_array[queue_idx], list)

#else
#define MAX_QUEUE_POOL_SIZE   256
struct queue_s {
	int		num;
	int		in_idx;
	int		out_idx;
	int		type; /* 0, first in first out;
			       * 1, general;2, fix position for di buf*/
	unsigned int	pool[MAX_QUEUE_POOL_SIZE];
};
#define queue_t struct queue_s
static queue_t queue[QUEUE_NUM];

struct di_buf_pool_s {
	struct di_buf_s *di_buf_ptr;
	unsigned int	size;
} di_buf_pool[VFRAME_TYPE_NUM];

#define queue_for_each_entry(di_buf, ptm, queue_idx, list)      \
	for (itmp = 0; ((di_buf = get_di_buf(queue_idx, &itmp)) != NULL); )

static void queue_init(int local_buffer_num)
{
	int i, j;

	for (i = 0; i < QUEUE_NUM; i++) {
		queue_t *q = &queue[i];
		for (j = 0; j < MAX_QUEUE_POOL_SIZE; j++)
			q->pool[j] = 0;

		q->in_idx = 0;
		q->out_idx = 0;
		q->num = 0;
		q->type = 0;
		if ((i == QUEUE_RECYCLE) ||
			(i == QUEUE_DISPLAY) ||
			(i == QUEUE_TMP)     ||
			(i == QUEUE_POST_DOING))
			q->type = 1;

		if ((i == QUEUE_LOCAL_FREE) && use_2_interlace_buff)
			q->type = 2;
	}
	if (local_buffer_num > 0) {
		di_buf_pool[VFRAME_TYPE_IN - 1].di_buf_ptr = &di_buf_in[0];
		di_buf_pool[VFRAME_TYPE_IN - 1].size = MAX_IN_BUF_NUM;

		di_buf_pool[VFRAME_TYPE_LOCAL-1].di_buf_ptr = &di_buf_local[0];
		di_buf_pool[VFRAME_TYPE_LOCAL - 1].size = local_buffer_num;

		di_buf_pool[VFRAME_TYPE_POST - 1].di_buf_ptr = &di_buf_post[0];
		di_buf_pool[VFRAME_TYPE_POST - 1].size = MAX_POST_BUF_NUM;
	}
}

static struct di_buf_s *get_di_buf(int queue_idx, int *start_pos)
{
	queue_t *q = &(queue[queue_idx]);
	int idx = 0;
	unsigned int pool_idx, di_buf_idx;
	struct di_buf_s *di_buf = NULL;
	int start_pos_init = *start_pos;

	if (di_log_flag & DI_LOG_QUEUE)
		di_print("%s:<%d:%d,%d,%d> %d\n", __func__, queue_idx,
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
			if (di_buf_idx < di_buf_pool[pool_idx].size)
				di_buf =
					&(di_buf_pool[pool_idx].di_buf_ptr[
						  di_buf_idx]);
		}
	}

	if ((di_buf) && ((((pool_idx + 1) << 8) | di_buf_idx)
			 != ((di_buf->type << 8) | (di_buf->index)))) {
		pr_dbg("%s: Error (%x,%x)\n", __func__,
			(((pool_idx + 1) << 8) | di_buf_idx),
			((di_buf->type << 8) | (di_buf->index)));
		if (recovery_flag == 0) {
			recovery_log_reason = 1;
			recovery_log_queue_idx = (start_pos_init<<8)|queue_idx;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
		di_buf = NULL;
	}

	if (di_log_flag & DI_LOG_QUEUE) {
		if (di_buf)
			di_print("%s: %x(%d,%d)\n", __func__, di_buf,
				pool_idx, di_buf_idx);
		else
			di_print("%s: %x\n", __func__, di_buf);
	}

	return di_buf;
}


static struct di_buf_s *get_di_buf_head(int queue_idx)
{
	queue_t *q = &(queue[queue_idx]);
	int idx;
	unsigned int pool_idx, di_buf_idx;
	struct di_buf_s *di_buf = NULL;

	if (di_log_flag & DI_LOG_QUEUE)
		di_print("%s:<%d:%d,%d,%d>\n", __func__, queue_idx,
			q->num, q->in_idx, q->out_idx);

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
				if (di_buf_idx < di_buf_pool[pool_idx].size)
					di_buf = &(di_buf_pool[pool_idx].
						di_buf_ptr[di_buf_idx]);
			}
		}
	}

	if ((di_buf) && ((((pool_idx + 1) << 8) | di_buf_idx) !=
			 ((di_buf->type << 8) | (di_buf->index)))) {

		pr_dbg("%s: Error (%x,%x)\n", __func__,
			(((pool_idx + 1) << 8) | di_buf_idx),
			((di_buf->type << 8) | (di_buf->index)));

		if (recovery_flag == 0) {
			recovery_log_reason = 2;
			recovery_log_queue_idx = queue_idx;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
		di_buf = NULL;
	}

	if (di_log_flag & DI_LOG_QUEUE) {
		if (di_buf)
			di_print("%s: %x(%d,%d)\n", __func__, di_buf,
				pool_idx, di_buf_idx);
		else
			di_print("%s: %x\n", __func__, di_buf);
	}

	return di_buf;
}

static void queue_out(struct di_buf_s *di_buf)
{
	int i;
	queue_t *q;

	if (di_buf == NULL) {

		pr_dbg("%s:Error\n", __func__);

		if (recovery_flag == 0)
			recovery_log_reason = 3;

		recovery_flag++;
		return;
	}
	if (di_buf->queue_index >= 0 && di_buf->queue_index < QUEUE_NUM) {
		q = &(queue[di_buf->queue_index]);

		if (di_log_flag & DI_LOG_QUEUE)
			di_print("%s:<%d:%d,%d,%d> %x\n", __func__,
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

					pr_dbg(
						"%s: Error (%d, %x,%x)\n",
						__func__,
						di_buf->queue_index,
						q->pool[q->out_idx],
						((di_buf->type << 8) |
						(di_buf->index)));

					if (recovery_flag == 0) {
						recovery_log_reason = 4;
						recovery_log_queue_idx =
							di_buf->queue_index;
						recovery_log_di_buf = di_buf;
					}
					recovery_flag++;
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

					pr_dbg("%s: Error\n", __func__);

					if (recovery_flag == 0) {
						recovery_log_reason = 5;
						recovery_log_queue_idx =
							di_buf->queue_index;
						recovery_log_di_buf = di_buf;
					}
					recovery_flag++;
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

					pr_dbg("%s: Error\n", __func__);

					if (recovery_flag == 0) {
						recovery_log_reason = 5;
						recovery_log_queue_idx =
							di_buf->queue_index;
						recovery_log_di_buf = di_buf;
					}
					recovery_flag++;
				}
			}
		}
	} else {

		pr_dbg("%s: Error, queue_index %d is not right\n",
			__func__, di_buf->queue_index);

		if (recovery_flag == 0) {
			recovery_log_reason = 6;
			recovery_log_queue_idx = 0;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
	}

	if (di_log_flag & DI_LOG_QUEUE)
		di_print("%s done\n", __func__);

}

static void queue_in(struct di_buf_s *di_buf, int queue_idx)
{
	queue_t *q = NULL;

	if (di_buf == NULL) {
		pr_dbg("%s:Error\n", __func__);
		if (recovery_flag == 0) {
			recovery_log_reason = 7;
			recovery_log_queue_idx = queue_idx;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
		return;
	}
	if (di_buf->queue_index != -1) {
		pr_dbg("%s:%s[%d] Error, queue_index(%d) is not -1\n",
			__func__, vframe_type_name[di_buf->type],
			di_buf->index, di_buf->queue_index);
		if (recovery_flag == 0) {
			recovery_log_reason = 8;
			recovery_log_queue_idx = queue_idx;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
		return;
	}
	q = &(queue[queue_idx]);
	if (di_log_flag & DI_LOG_QUEUE)
		di_print("%s:<%d:%d,%d,%d> %x\n", __func__, queue_idx,
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
				q->pool[i] = (di_buf->type<<8)|(di_buf->index);
				di_buf->queue_index = queue_idx;
				q->num++;
				break;
			}
		}
		if (i == MAX_QUEUE_POOL_SIZE) {
			pr_dbg("%s: Error\n", __func__);
			if (recovery_flag == 0) {
				recovery_log_reason = 9;
				recovery_log_queue_idx = queue_idx;
			}
			recovery_flag++;
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
			if (recovery_flag == 0) {
				recovery_log_reason = 9;
				recovery_log_queue_idx = queue_idx;
			}
			recovery_flag++;
		}
	}

	if (di_log_flag & DI_LOG_QUEUE)
		di_print("%s done\n", __func__);
}

static int list_count(int queue_idx)
{
	return queue[queue_idx].num;
}

static bool queue_empty(int queue_idx)
{
	bool ret = (queue[queue_idx].num == 0);

	return ret;
}
#endif

static bool is_in_queue(struct di_buf_s *di_buf, int queue_idx)
{
	bool ret = 0;
	struct di_buf_s *p = NULL;
	int itmp;
	unsigned int overflow_cnt;
	overflow_cnt = 0;
	if ((di_buf == NULL) || (queue_idx < 0) || (queue_idx >= QUEUE_NUM)) {
		ret = 0;
		di_print("%s: not in queue:%d!!!\n", __func__, queue_idx);
		return ret;
	}
	queue_for_each_entry(p, ptmp, queue_idx, list) {
		if (p == di_buf) {
			ret = 1;
			break;
		}
		if (overflow_cnt++ > MAX_QUEUE_POOL_SIZE) {
			ret = 0;
			di_print("%s: overflow_cnt!!!\n", __func__);
			break;
		}
	}
	return ret;
}

struct di_pre_stru_s {
/* pre input */
	struct DI_MIF_s	di_inp_mif;
	struct DI_MIF_s	di_mem_mif;
	struct DI_MIF_s	di_chan2_mif;
	struct di_buf_s *di_inp_buf;
	struct di_buf_s *di_post_inp_buf;
	struct di_buf_s *di_inp_buf_next;
	struct di_buf_s *di_mem_buf_dup_p;
	struct di_buf_s *di_chan2_buf_dup_p;
/* pre output */
	struct DI_SIM_MIF_s	di_nrwr_mif;
	struct DI_SIM_MIF_s	di_mtnwr_mif;
	struct di_buf_s *di_wr_buf;
	struct di_buf_s *di_post_wr_buf;
#ifdef NEW_DI_V1
	struct DI_SIM_MIF_s	di_contp2rd_mif;
	struct DI_SIM_MIF_s	di_contprd_mif;
	struct DI_SIM_MIF_s	di_contwr_mif;
	int		field_count_for_cont;
/*
 * 0 (f0,null,f0)->nr0,
 * 1 (f1,nr0,f1)->nr1_cnt,
 * 2 (f2,nr1_cnt,nr0)->nr2_cnt
 * 3 (f3,nr2_cnt,nr1_cnt)->nr3_cnt
 */
#endif
	struct DI_MC_MIF_s		di_mcinford_mif;
	struct DI_MC_MIF_s		di_mcvecwr_mif;
	struct DI_MC_MIF_s		di_mcinfowr_mif;
/* pre state */
	int	in_seq;
	int	recycle_seq;
	int	pre_ready_seq;

	int	pre_de_busy;            /* 1 if pre_de is not done */
	int	pre_de_busy_timer_count;
	int	pre_de_process_done;    /* flag when irq done */
	int	pre_de_clear_flag;
	/* flag is set when VFRAME_EVENT_PROVIDER_UNREG*/
	int	unreg_req_flag;
	int	unreg_req_flag_irq;
	int	unreg_req_flag_cnt;
	int	reg_req_flag;
	int	reg_req_flag_irq;
	int	reg_req_flag_cnt;
	int	force_unreg_req_flag;
	int	disable_req_flag;
	/* current source info */
	int	cur_width;
	int	cur_height;
	int	cur_inp_type;
	int	cur_source_type;
	int	cur_sig_fmt;
	unsigned int orientation;
	int	cur_prog_flag; /* 1 for progressive source */
/* valid only when prog_proc_type is 0, for
 * progressive source: top field 1, bot field 0 */
	int	source_change_flag;

	unsigned char prog_proc_type;
/* set by prog_proc_config when source is vdin,0:use 2 i
 * serial buffer,1:use 1 p buffer,3:use 2 i paralleling buffer*/
	unsigned char buf_alloc_mode;
/* alloc di buf as p or i;0: alloc buf as i;
 * 1: alloc buf as p;*/
	unsigned char enable_mtnwr;
	unsigned char enable_pulldown_check;

	int	same_field_source_flag;
	int	left_right;/*1,left eye; 0,right eye in field alternative*/
/*input2pre*/
	int	bypass_start_count;
/* need discard some vframe when input2pre => bypass */
	int	vdin2nr;
	enum tvin_trans_fmt	source_trans_fmt;
	enum tvin_trans_fmt	det3d_trans_fmt;
	unsigned int det_lr;
	unsigned int det_tp;
	unsigned int det_la;
	unsigned int det_null;
#ifdef DET3D
	int	vframe_interleave_flag;
#endif
/**/
	int	pre_de_irq_timeout_count;
	int	pre_throw_flag;
	int	bad_frame_throw_count;
/*for static pic*/
	int	static_frame_count;
	bool force_interlace;
	bool bypass_pre;
	bool invert_flag;
	int nr_size;
	int count_size;
	int mcinfo_size;
	int mv_size;
	int mtn_size;
	int cma_alloc_req;
	int cma_alloc_done;
	int cma_release_req;
};
static struct di_pre_stru_s di_pre_stru;

static void dump_di_pre_stru(void)
{
	pr_info("di_pre_stru:\n");
	pr_info("di_mem_buf_dup_p	   = 0x%p\n",
		di_pre_stru.di_mem_buf_dup_p);
	pr_info("di_chan2_buf_dup_p	   = 0x%p\n",
		di_pre_stru.di_chan2_buf_dup_p);
	pr_info("in_seq				   = %d\n",
		di_pre_stru.in_seq);
	pr_info("recycle_seq		   = %d\n",
		di_pre_stru.recycle_seq);
	pr_info("pre_ready_seq		   = %d\n",
		di_pre_stru.pre_ready_seq);
	pr_info("pre_de_busy		   = %d\n",
		di_pre_stru.pre_de_busy);
	pr_info("pre_de_busy_timer_count= %d\n",
		di_pre_stru.pre_de_busy_timer_count);
	pr_info("pre_de_process_done   = %d\n",
		di_pre_stru.pre_de_process_done);
	pr_info("pre_de_irq_timeout_count=%d\n",
		di_pre_stru.pre_de_irq_timeout_count);
	pr_info("unreg_req_flag		   = %d\n",
		di_pre_stru.unreg_req_flag);
	pr_info("unreg_req_flag_irq		   = %d\n",
		di_pre_stru.unreg_req_flag_irq);
	pr_info("reg_req_flag		   = %d\n",
		di_pre_stru.reg_req_flag);
	pr_info("reg_req_flag_irq		   = %d\n",
		di_pre_stru.reg_req_flag_irq);
	pr_info("cur_width			   = %d\n",
		di_pre_stru.cur_width);
	pr_info("cur_height			   = %d\n",
		di_pre_stru.cur_height);
	pr_info("cur_inp_type		   = 0x%x\n",
		di_pre_stru.cur_inp_type);
	pr_info("cur_source_type	   = %d\n",
		di_pre_stru.cur_source_type);
	pr_info("cur_prog_flag		   = %d\n",
		di_pre_stru.cur_prog_flag);
	pr_info("source_change_flag	   = %d\n",
		di_pre_stru.source_change_flag);
	pr_info("prog_proc_type		   = %d\n",
		di_pre_stru.prog_proc_type);
	pr_info("enable_mtnwr		   = %d\n",
		di_pre_stru.enable_mtnwr);
	pr_info("enable_pulldown_check	= %d\n",
		di_pre_stru.enable_pulldown_check);
	pr_info("same_field_source_flag = %d\n",
		di_pre_stru.same_field_source_flag);
#ifdef DET3D
	pr_info("vframe_interleave_flag = %d\n",
		di_pre_stru.vframe_interleave_flag);
#endif
	pr_info("left_right		   = %d\n",
		di_pre_stru.left_right);
	pr_info("force_interlace   = %s\n",
		di_pre_stru.force_interlace ? "true" : "false");
	pr_info("vdin2nr		   = %d\n",
		di_pre_stru.vdin2nr);
	pr_info("bypass_pre		   = %s\n",
		di_pre_stru.bypass_pre ? "true" : "false");
	pr_info("invert_flag	   = %s\n",
		di_pre_stru.invert_flag ? "true" : "false");
}

struct di_post_stru_s {
	struct DI_MIF_s	di_buf0_mif;
	struct DI_MIF_s	di_buf1_mif;
	struct DI_MIF_s	di_buf2_mif;
	struct DI_SIM_MIF_s di_diwr_mif;
	struct DI_SIM_MIF_s	di_mtnprd_mif;
	struct DI_MC_MIF_s	di_mcvecrd_mif;
	struct di_buf_s *cur_post_buf;
	int		update_post_reg_flag;
	int		post_process_fun_index;
	int		run_early_proc_fun_flag;
	int		cur_disp_index;
	int		canvas_id;
	int		next_canvas_id;
	bool		toggle_flag;
	bool		vscale_skip_flag;
	uint		start_pts;
	u64 start_pts_us64;
	int		buf_type;
	int de_post_process_done;
	int post_de_busy;
	int di_post_num;
	unsigned int di_post_process_cnt;
	unsigned int check_recycle_buf_cnt;
};
static struct di_post_stru_s di_post_stru;
static void dump_di_post_stru(void)
{
	di_pr_info("\ndi_post_stru:\n");
	di_pr_info("run_early_proc_fun_flag	= %d\n",
		di_post_stru.run_early_proc_fun_flag);
	di_pr_info("cur_disp_index = %d\n",
		di_post_stru.cur_disp_index);
	di_pr_info("post_de_busy			= %d\n",
		di_post_stru.post_de_busy);
	di_pr_info("de_post_process_done	= %d\n",
		di_post_stru.de_post_process_done);
	di_pr_info("cur_post_buf			= 0x%p\n,",
		di_post_stru.cur_post_buf);
}

#ifdef NEW_DI_V1
static ssize_t
store_dump_mem(struct device *dev, struct device_attribute *attr,
	       const char *buf, size_t len)
{
	unsigned int n = 0, canvas_w = 0, canvas_h = 0;
	unsigned long nr_size = 0;
	struct di_buf_s *di_buf;
	char *buf_orig, *ps, *token;
	char *parm[3] = { NULL };
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buff = NULL;
	mm_segment_t old_fs;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, " \n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (0 == strcmp(parm[0], "capture"))
		di_buf = di_pre_stru.di_mem_buf_dup_p;
	else if (0 == strcmp(parm[0], "capture_post"))
		di_buf = di_post_stru.cur_post_buf;
	else {
		pr_err("wrong dump cmd\n");
		return len;
	}
	if (parm[1] != NULL) {
		if (unlikely(di_buf == NULL))
			return len;
	}
	canvas_w = di_buf->canvas_width[NR_CANVAS];
	canvas_h = di_buf->canvas_height;
	nr_size = canvas_w * canvas_h * 2;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
/* pr_dbg("dump path =%s\n",dump_path); */
		filp = filp_open(parm[1], O_RDWR | O_CREAT, 0666);
		if (IS_ERR(filp)) {
			pr_err("create %s error.\n", parm[1]);
			return len;
		}
		dump_state_flag = 1;
		if (de_devp->flags && DI_MAP_FLAG)
			buff = phys_to_virt(di_buf->nr_adr);
		else
			buff = ioremap(di_buf->nr_adr, nr_size);
		if (buff == NULL)
			pr_err("%s: ioremap error.\n", __func__);
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
		if (!(de_devp->flags & DI_MAP_FLAG))
			iounmap(buff);
		dump_state_flag = 0;
		filp_close(filp, NULL);
		set_fs(old_fs);
		pr_info("write buffer %d  to %s.\n", di_buf->seq, parm[1]);
	return len;
}
#endif

#define is_from_vdin(vframe) (vframe->type & VIDTYPE_VIU_422)
static void recycle_vframe_type_pre(struct di_buf_s *di_buf);
static void recycle_vframe_type_post(struct di_buf_s *di_buf);
#ifdef DI_BUFFER_DEBUG
static void
recycle_vframe_type_post_print(struct di_buf_s *di_buf,
				const char *func,
				const int line);
#endif

reg_cfg_t *reg_cfg_head = NULL;

#ifdef NEW_DI_V1
/* new pre and post di setting */
reg_cfg_t di_default_pre = {
	NULL,
	((1 << VFRAME_SOURCE_TYPE_OTHERS) |
	 (1 << VFRAME_SOURCE_TYPE_TUNER) |
	 (1 << VFRAME_SOURCE_TYPE_CVBS) |
	 (1 << VFRAME_SOURCE_TYPE_COMP) |
	 (1 << VFRAME_SOURCE_TYPE_HDMI)
	),
	0,
	0,
	{
		(
			(TVIN_SIG_FMT_COMP_480P_60HZ_D000 << 16) |
			TVIN_SIG_FMT_CVBS_SECAM),
		0
	},
	{
		{DI_EI_CTRL3,  0x0000013, 0, 27},
		{DI_EI_CTRL4, 0x151b3084, 0, 31},
		{DI_EI_CTRL5, 0x5273204f, 0, 31},
		{DI_EI_CTRL6, 0x50232815, 0, 31},
		{DI_EI_CTRL7, 0x2fb56650, 0, 31},
		{DI_EI_CTRL8, 0x230019a4, 0, 31},
		{DI_EI_CTRL9, 0x7cb9bb33, 0, 31},
/* #define DI_EI_CTRL10 */
		{0x1793, 0x0842c6a9, 0, 31},
/* #define DI_EI_CTRL11 */
		{0x179e, 0x486ab07a, 0, 31},
/* #define DI_EI_CTRL12 */
		{0x179f, 0xdb0c2503, 0, 32},
/* #define DI_EI_CTRL13 */
		{0x17a8, 0x0f021414 , 0, 31},
		{ 0 },
	}
};
reg_cfg_t di_default_post = {
	NULL,
	((1 << VFRAME_SOURCE_TYPE_OTHERS) |
	 (1 << VFRAME_SOURCE_TYPE_TUNER) |
	 (1 << VFRAME_SOURCE_TYPE_CVBS) |
	 (1 << VFRAME_SOURCE_TYPE_COMP) |
	 (1 << VFRAME_SOURCE_TYPE_HDMI)
	),
	1,
	2,
	{
		(
			(TVIN_SIG_FMT_COMP_480P_60HZ_D000 << 16) |
			TVIN_SIG_FMT_CVBS_SECAM),
		0
	},
	{
		{DI_MTN_1_CTRL1,		 0, 30, 1},
		{DI_MTN_1_CTRL1, 0x0202015,  0, 27},
		{DI_MTN_1_CTRL2, 0x141a2062, 0, 31},
		{DI_MTN_1_CTRL3, 0x1520050a, 0, 31},
		{DI_MTN_1_CTRL4, 0x08800840, 0, 31},
		{DI_MTN_1_CTRL5, 0x74200d0d, 0, 31},
/* #define DI_MTN_1_CTRL6 */
		{DI_MTN_1_CTRL6, 0x0d5a1520, 0, 31},
/* #define DI_MTN_1_CTRL7 */
		{DI_MTN_1_CTRL7, 0x0a0a0201, 0, 31},
/* #define DI_MTN_1_CTRL8 */
		{DI_MTN_1_CTRL8, 0x1a1a2662, 0, 31},
/* #define DI_MTN_1_CTRL9 */
		{DI_MTN_1_CTRL9, 0x0d200302, 0, 31},
/* #define DI_MTN_1_CTRL10 */
		{DI_MTN_1_CTRL10, 0x02020606, 0, 31},
/* #define DI_MTN_1_CTRL11 */
		{DI_MTN_1_CTRL11, 0x05080304, 0, 31},
/* #define DI_MTN_1_CTRL12 */
		{DI_MTN_1_CTRL12, 0x40020a04, 0, 31},
		{ 0 },
	}
};

void di_add_reg_cfg(reg_cfg_t *reg_cfg)
{
	reg_cfg->next = reg_cfg_head;
	reg_cfg_head = reg_cfg;
}

static void di_apply_reg_cfg(unsigned char pre_post_type)
{
	reg_cfg_t *reg_cfg = reg_cfg_head;
	int ii;

	if (!use_reg_cfg)
		return;
	while (reg_cfg) {
		unsigned char set_flag = 0;
		if ((pre_post_type == reg_cfg->pre_post_type) &&
			((1 << di_pre_stru.cur_source_type) &
			reg_cfg->source_types_enable)) {
			if (di_pre_stru.cur_source_type ==
			VFRAME_SOURCE_TYPE_OTHERS &&
			(2 != reg_cfg->dtv_defintion_type)) {
				/* if:dtv stand defintion
				 * else if:high defintion */
				if (di_pre_stru.cur_height < 720 &&
				reg_cfg->dtv_defintion_type)
					set_flag = 1;
				else if (di_pre_stru.cur_height >= 720
				&& (!reg_cfg->dtv_defintion_type))
					set_flag = 1;
			} else {
				for (ii = 0; ii < FMT_MAX_NUM; ii++) {
					if (reg_cfg->
					sig_fmt_range[ii] == 0)
						break;
					else if (
					(di_pre_stru.cur_sig_fmt >=
					((reg_cfg->sig_fmt_range[ii]
					>> 16) & 0xffff))
					&& (di_pre_stru.cur_sig_fmt <=
					(reg_cfg->sig_fmt_range[ii] &
					0xffff))) {
						set_flag = 1;
						break;
					}
				}
			}
		}
		if (set_flag) {
			for (ii = 0; ii < REG_SET_MAX_NUM; ii++) {
				if (0 == reg_cfg->reg_set[ii].adr)
					break;
				if (pre_post_type) {
					DI_VSYNC_WR_MPEG_REG_BITS(
					reg_cfg->reg_set[ii].adr,
					reg_cfg->reg_set[ii].val,
					reg_cfg->reg_set[ii].start,
					reg_cfg->reg_set[ii].len);
				} else {
					RDMA_WR_BITS(
					reg_cfg->reg_set[ii].adr,
					reg_cfg->reg_set[ii].val,
					reg_cfg->reg_set[ii].start,
					reg_cfg->reg_set[ii].len);
				}
			}
			break;
		}
		reg_cfg = reg_cfg->next;
	}
}
#endif


static void dis2_di(void)
{
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;

	init_flag = 0;
	di_lock_irqfiq_save(irq_flag2, fiq_flag);
/* vf_unreg_provider(&di_vf_prov); */
	vf_light_unreg_provider(&di_vf_prov);
	di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
	reg_flag = 0;
	spin_lock_irqsave(&plist_lock, flags);
	di_lock_irqfiq_save(irq_flag2, fiq_flag);
	if (di_pre_stru.di_inp_buf) {
		if (vframe_in[di_pre_stru.di_inp_buf->index]) {
			vf_put(
				vframe_in[di_pre_stru.di_inp_buf->index],
				VFM_NAME);
			vframe_in[di_pre_stru.di_inp_buf->index] = NULL;
			vf_notify_provider(
				VFM_NAME, VFRAME_EVENT_RECEIVER_PUT, NULL);
		}
/* list_add_tail(
 * &(di_pre_stru.di_inp_buf->list), &in_free_list_head); */
		di_pre_stru.di_inp_buf->invert_top_bot_flag = 0;
		queue_in(di_pre_stru.di_inp_buf, QUEUE_IN_FREE);
		di_pre_stru.di_inp_buf = NULL;
	}
	di_uninit_buf();
	di_set_power_control(0, 0);
	if (get_blackout_policy()) {
		di_set_power_control(1, 0);
		DI_Wr(DI_CLKG_CTRL, 0x2);
	}

	if (post_wr_en && post_wr_surpport) {
		diwr_set_power_control(0);
		#ifdef CONFIG_VSYNC_RDMA
		enable_rdma(1);
		#endif
	}

	di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
	spin_unlock_irqrestore(&plist_lock, flags);
}

static ssize_t
store_config(struct device *dev,
	     struct device_attribute *attr,
	     const char *buf, size_t count)
{
	char *parm[2] = { NULL }, *buf_orig;
	int rc = 0;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));

	if (strncmp(buf, "disable", 7) == 0) {
		di_print("%s: disable\n", __func__);
		if (init_flag && mem_flag) {
			di_pre_stru.disable_req_flag = 1;
			provider_vframe_level = 0;
			bypass_dynamic_flag = 0;
			post_ready_empty_count = 0;
			trigger_pre_di_process(TRIGGER_PRE_BY_DEBUG_DISABLE);
			while (di_pre_stru.disable_req_flag)
				usleep_range(1000, 1001);
		}
	} else if (strncmp(buf, "dis2", 4) == 0) {
		dis2_di();
	} else if (strcmp(parm[0], "hold_video") == 0) {
		pr_info("%s(%s %s)\n", __func__, parm[0], parm[1]);
		rc = kstrtoint(parm[1], 10, &hold_video);
	}
	return count;
}

static unsigned char is_progressive(vframe_t *vframe)
{
	unsigned char ret = 0;

	ret = ((vframe->type & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE);
	return ret;
}

static void force_source_change(void)
{
	di_pre_stru.cur_inp_type = 0;
}

static unsigned char is_source_change(vframe_t *vframe)
{
#define VFRAME_FORMAT_MASK      \
	(VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_444 | \
	 VIDTYPE_MVC)
	if ((di_pre_stru.cur_width != vframe->width) ||
	(di_pre_stru.cur_height != vframe->height) ||
	(((di_pre_stru.cur_inp_type & VFRAME_FORMAT_MASK) !=
	(vframe->type & VFRAME_FORMAT_MASK)) &&
	(!is_handle_prog_frame_as_interlace(vframe))) ||
	(di_pre_stru.cur_source_type != vframe->source_type)) {
		/* video format changed */
		return 1;
	} else if (
	((di_pre_stru.cur_prog_flag != is_progressive(vframe)) &&
	(!is_handle_prog_frame_as_interlace(vframe))) ||
	((di_pre_stru.cur_inp_type & VIDTYPE_VIU_FIELD) !=
	(vframe->type & VIDTYPE_VIU_FIELD))
	) {
		/* just scan mode changed */
		if (!di_pre_stru.force_interlace)
			pr_dbg("DI I<->P.\n");
		return 2;
	}
	return 0;
}
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
static int trick_mode;
static unsigned char is_bypass(vframe_t *vf_in)
{
	unsigned int vtype = 0;
	int ret = 0;
	if (di_debug_flag & 0x10000) /* for debugging */
		return (di_debug_flag >> 17) & 0x1;

	if (bypass_interlace_output && interlace_output_flag)
		return 1;

	if (bypass_all)
		return 1;
	if (di_pre_stru.cur_prog_flag &&
	    ((bypass_prog) ||
	     (di_pre_stru.cur_width > 1920) || (di_pre_stru.cur_height > 1080)
	     || (di_pre_stru.cur_inp_type & VIDTYPE_VIU_444))
	    )
		return 1;
	if (di_pre_stru.cur_prog_flag &&
	    (di_pre_stru.cur_width == 1920) && (di_pre_stru.cur_height == 1080)
	    && (bypass_1080p)
	    )
		return 1;

	if (di_pre_stru.cur_prog_flag &&
	    (di_pre_stru.cur_width > 720) && (di_pre_stru.cur_height > 576)
	    && (bypass_hd_prog)
	    )
		return 1;

	if (bypass_hd &&
	    ((di_pre_stru.cur_width > 720) || (di_pre_stru.cur_height > 576))
	    )
		return 1;
	if (bypass_superd &&
	    ((di_pre_stru.cur_width > 1920) && (di_pre_stru.cur_height > 1080))
	    )
		return 1;
	if ((di_pre_stru.cur_width < 16) || (di_pre_stru.cur_height < 16))
		return 1;

	if (di_pre_stru.cur_inp_type & VIDTYPE_MVC)
		return 1;

	if (di_pre_stru.cur_source_type == VFRAME_SOURCE_TYPE_PPMGR)
		return 1;

	if (((bypass_trick_mode) &&
	     (new_keep_last_frame_enable == 0)) || (bypass_trick_mode & 0x2)) {
		int trick_mode_fffb = 0;
		int trick_mode_i = 0;
		if (bypass_trick_mode&0x1)
			query_video_status(0, &trick_mode_fffb);
		if (bypass_trick_mode&0x2)
			query_video_status(1, &trick_mode_i);
		trick_mode = trick_mode_fffb | (trick_mode_i << 1);
		if (trick_mode)
			return 1;
	}

	if (bypass_3d &&
	    (di_pre_stru.source_trans_fmt != 0))
		return 1;

/*prot is conflict with di post*/
	if (vf_in && vf_in->video_angle)
		return 1;
	if (vf_in && (vf_in->type & VIDTYPE_PIC))
		return 1;

	if (vf_in && (vf_in->type & VIDTYPE_COMPRESS))
		return 1;
	if ((di_vscale_skip_enable & 0x4) && vf_in) {
		/*backup vtype,set type as progressive*/
		vtype = vf_in->type;
		vf_in->type &= (~VIDTYPE_TYPEMASK);
		vf_in->type |= VIDTYPE_VIU_422;
		ret = get_current_vscale_skip_count(vf_in);
		di_vscale_skip_count = (ret&0xff);
		vpp_3d_mode = ((ret>>8)&0xff);
		vf_in->type = vtype;
		if (di_vscale_skip_count > 0 ||
			(vpp_3d_mode
			#ifdef DET3D
			&& (!det3d_en)
			#endif
			)
			)
			return 1;
	}

	return 0;
}

static unsigned char is_bypass_post(void)
{
	if (di_debug_flag & 0x40000) /* for debugging */
		return (di_debug_flag >> 19) & 0x1;

	/*prot is conflict with di post*/
	if (di_pre_stru.orientation)
		return 1;
	if ((bypass_post) || (bypass_dynamic_flag & 1))
		return 1;


#ifdef DET3D
	if (di_pre_stru.vframe_interleave_flag != 0)
		return 1;

#endif
	return 0;
}

#ifdef RUN_DI_PROCESS_IN_IRQ
static unsigned char is_input2pre(void)
{
	if (input2pre
#ifdef NEW_DI_V3
	    && di_pre_stru.cur_prog_flag
#endif
	    && vdin_source_flag
	    && (bypass_state == 0))
		return 1;

	return 0;
}
#endif

#ifdef DI_USE_FIXED_CANVAS_IDX
static int di_post_idx[2][6];
static int di_pre_idx[2][10];
static int di_get_canvas(void)
{
	int pre_num = 7, post_num = 6, i = 0;

	if (mcpre_en) {
		/* mem/chan2/nr/mtn/contrd/contrd2/
		 * contw/mcinfrd/mcinfow/mcvecw */
		pre_num = 10;
		/* buf0/buf1/buf2/mtnp/mcvec */
		post_num = 6;
	}
	if (canvas_pool_alloc_canvas_table("di_pre",
	&di_pre_idx[0][0], pre_num, CANVAS_MAP_TYPE_1)) {
		pr_dbg("%s allocate di pre canvas error.\n", __func__);
		return 1;
	}
	if (di_pre_rdma_enable) {
		if (canvas_pool_alloc_canvas_table("di_pre",
		&di_pre_idx[1][0], pre_num, CANVAS_MAP_TYPE_1)) {
			pr_dbg("%s allocate di pre canvas error.\n", __func__);
			return 1;
		}
	} else {
		for (i = 0; i < pre_num; i++)
			di_pre_idx[1][i] = di_pre_idx[0][i];
	}
	if (canvas_pool_alloc_canvas_table("di_post",
		    &di_post_idx[0][0], post_num, CANVAS_MAP_TYPE_1)) {
		pr_dbg("%s allocate di post canvas error.\n", __func__);
		return 1;
	}

#ifdef CONFIG_VSYNC_RDMA
	if (canvas_pool_alloc_canvas_table("di_post",
		    &di_post_idx[1][0], post_num, CANVAS_MAP_TYPE_1)) {
		pr_dbg("%s allocate di post canvas error.\n", __func__);
		return 1;
	}
#else
	for (i = 0; i < post_num; i++)
		di_post_idx[1][i] = di_post_idx[0][i];
#endif
	return 0;
}
static void config_canvas_idx(struct di_buf_s *di_buf, int nr_canvas_idx,
	int mtn_canvas_idx)
{
	unsigned int height = 0;
	if (!di_buf)
		return;
	if (di_buf->canvas_config_flag == 1) {
		if (nr_canvas_idx >= 0) {
			/* linked two interlace buffer should double height*/
			if (di_buf->di_wr_linked_buf)
				height = (di_buf->canvas_height << 1);
			else
				height =  di_buf->canvas_height;
			di_buf->nr_canvas_idx = nr_canvas_idx;
			canvas_config(nr_canvas_idx, di_buf->nr_adr,
				di_buf->canvas_width[NR_CANVAS], height, 0, 0);
		}
	} else if (di_buf->canvas_config_flag == 2) {
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
	}
	if (nr_canvas_idx >= 0) {
		di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
		di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
	}
}

#ifdef NEW_DI_V1
static void config_cnt_canvas_idx(struct di_buf_s *di_buf,
	unsigned int cnt_canvas_idx)
{

	if (!di_buf)
		return;

	di_buf->cnt_canvas_idx = cnt_canvas_idx;
	canvas_config(
		cnt_canvas_idx, di_buf->cnt_adr,
		di_buf->canvas_width[MTN_CANVAS],
		di_buf->canvas_height, 0, 0);
}
#endif

static void config_mcinfo_canvas_idx(struct di_buf_s *di_buf,
	int mcinfo_canvas_idx)
{

	if (!di_buf)
		return;

	di_buf->mcinfo_canvas_idx = mcinfo_canvas_idx;
	canvas_config(
		mcinfo_canvas_idx, di_buf->mcinfo_adr,
		di_buf->canvas_height, 2, 0, 0);
}
static void config_mcvec_canvas_idx(struct di_buf_s *di_buf,
	int mcvec_canvas_idx)
{

	if (!di_buf)
		return;

	di_buf->mcvec_canvas_idx = mcvec_canvas_idx;
	canvas_config(
		mcvec_canvas_idx, di_buf->mcvec_adr,
		di_buf->canvas_width[MV_CANVAS],
		di_buf->canvas_height, 0, 0);
}


#else

static void config_canvas(struct di_buf_s *di_buf)
{
	unsigned int height = 0;
	if (!di_buf)
		return;

	if (di_buf->canvas_config_flag == 1) {
			/* linked two interlace buffer should double height*/
			if (di_buf->di_wr_linked_buf)
				height = (di_buf->canvas_height << 1);
			else
				height =  di_buf->canvas_height;
		canvas_config(di_buf->nr_canvas_idx, di_buf->nr_adr,
			di_buf->canvas_width[NR_CANVAS], height, 0, 0);
		di_buf->canvas_config_flag = 0;
	} else if (di_buf->canvas_config_flag == 2) {
		canvas_config(di_buf->nr_canvas_idx, di_buf->nr_adr,
			di_buf->canvas_width[MV_CANVAS],
			di_buf->canvas_height, 0, 0);
		canvas_config(di_buf->mtn_canvas_idx, di_buf->mtn_adr,
			di_buf->canvas_width[MTN_CANVAS],
			di_buf->canvas_height, 0, 0);
		di_buf->canvas_config_flag = 0;
	}
}

#endif
#ifdef CONFIG_CMA
void di_cma_alloc(void)
{
	unsigned int i, start_time, end_time, delta_time;
	if (bypass_4K == 1) {
		pr_dbg("%s:di don't need alloc mem for 4k input\n", __func__);
		return;
	}
	start_time = jiffies_to_msecs(jiffies);
	for (i = 0; (i < local_buf_num); i++) {
		struct di_buf_s *di_buf = &(di_buf_local[i]);
		int ii = USED_LOCAL_BUF_MAX;
		if ((used_post_buf_index != -1) &&
		    (new_keep_last_frame_enable)) {
			for (ii = 0; ii < USED_LOCAL_BUF_MAX; ii++) {
				if (i == used_local_buf_index[ii]) {
					pr_dbg("%s skip %d,cma_alloc=%d\n",
						__func__, i,
						de_devp->cma_alloc[i]);
					break;
				}
			}
		}
		if ((ii >= USED_LOCAL_BUF_MAX) &&
			(de_devp->cma_alloc[i] == 0)) {
			de_devp->pages[i] =
				dma_alloc_from_contiguous(&(de_devp->pdev->dev),
				de_devp->buffer_size >> PAGE_SHIFT, 0);
			if (de_devp->pages[i]) {
				de_devp->mem_start =
					page_to_phys(de_devp->pages[i]);
				de_devp->cma_alloc[i] = 1;
				pr_dbg("DI CMA  allocate addr:0x%x[%d] ok.\n",
					(unsigned int)de_devp->mem_start, i);
			} else {
				pr_dbg("DI CMA  allocate %d fail.\n", i);
			}
			if (di_pre_stru.buf_alloc_mode) {
				di_buf->nr_adr = de_devp->mem_start;
			} else {
				di_buf->nr_adr = de_devp->mem_start;
				di_buf->mtn_adr = de_devp->mem_start +
					di_pre_stru.nr_size;
#ifdef NEW_DI_V1
				di_buf->cnt_adr = de_devp->mem_start +
					di_pre_stru.nr_size +
					di_pre_stru.mtn_size;
#endif
				if (mcpre_en) {
					di_buf->mcvec_adr = de_devp->mem_start +
						di_pre_stru.nr_size +
						di_pre_stru.mtn_size +
						di_pre_stru.count_size;
					di_buf->mcinfo_adr =
						de_devp->mem_start +
						di_pre_stru.nr_size +
						di_pre_stru.mtn_size +
						di_pre_stru.count_size +
						di_pre_stru.mv_size;
				}
			}
		}
	}
	end_time = jiffies_to_msecs(jiffies);
	delta_time = end_time - start_time;
	pr_dbg("%s:alloc use %d ms(%d~%d)\n", __func__, delta_time,
		start_time, end_time);
}
void di_cma_release(void)
{
	unsigned int i, start_time, end_time, delta_time;
	start_time = jiffies_to_msecs(jiffies);
	for (i = 0; (i < local_buf_num); i++) {
		int ii = USED_LOCAL_BUF_MAX;
		if ((used_post_buf_index != -1) &&
		    (new_keep_last_frame_enable)) {
			for (ii = 0; ii < USED_LOCAL_BUF_MAX; ii++) {
				if (i == used_local_buf_index[ii]) {
					pr_dbg("%s skip %d,cma_alloc=%d\n",
						__func__, i,
						de_devp->cma_alloc[i]);
					break;
				}
			}
		}
		if ((ii >= USED_LOCAL_BUF_MAX) &&
			(de_devp->cma_alloc[i] == 1)) {
			if (dma_release_from_contiguous(&(de_devp->pdev->dev),
				de_devp->pages[i],
				de_devp->buffer_size >> PAGE_SHIFT)) {
				de_devp->cma_alloc[i] = 0;
				pr_dbg("DI CMA  release %d ok.\n", i);
			} else {
				pr_dbg("DI CMA  release %d fail.\n", i);
			}
		}
	}
	end_time = jiffies_to_msecs(jiffies);
	delta_time = end_time - start_time;
	pr_dbg("%s:release use %d ms(%d~%d)\n", __func__, delta_time,
		start_time, end_time);
}
#endif
static int di_init_buf(int width, int height, unsigned char prog_flag)
{
	int i;
	int canvas_height = height + 8;

	unsigned int di_buf_size = 0, di_post_buf_size = 0, mtn_size = 0;
	unsigned int nr_size = 0, count_size = 0, mv_size = 0, mc_size = 0;
	unsigned int nr_width = width, mtn_width = width, mv_width = width;
	unsigned int nr_canvas_width = width, mtn_canvas_width = width;
	unsigned int mv_canvas_width = width;
	unsigned long di_post_mem = 0;
	frame_count = 0;
	disp_frame_count = 0;
	cur_post_ready_di_buf = NULL;
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (vframe_in[i]) {
			vf_put(vframe_in[i], VFM_NAME);
			vf_notify_provider(
				VFM_NAME, VFRAME_EVENT_RECEIVER_PUT, NULL);
			vframe_in[i] = NULL;
		}
	}
	memset(&di_pre_stru, 0, sizeof(di_pre_stru));
	if (nr10bit_surpport) {
		if (full_422_pack)
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
	nr_canvas_width = roundup(nr_canvas_width, 32);
	mtn_canvas_width = roundup(mtn_canvas_width, 32);
	mv_canvas_width = roundup(mv_canvas_width, 32);
	nr_width = nr_canvas_width >> 1;
	mtn_width = mtn_canvas_width << 1;
	mv_width = (mv_canvas_width * 5) >> 1;

	if (prog_flag) {
		di_pre_stru.prog_proc_type = 1;
		di_pre_stru.buf_alloc_mode = 1;
		di_buf_size = nr_width * canvas_height * 2;
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);
		local_buf_num = de_devp->mem_size / di_buf_size;
		if (local_buf_num > (2 * MAX_LOCAL_BUF_NUM))
			local_buf_num = 2 * MAX_LOCAL_BUF_NUM;

		if (local_buf_num >= 6)
			new_keep_last_frame_enable = 1;
		else
			new_keep_last_frame_enable = 0;
	} else {
		di_pre_stru.prog_proc_type = 0;
		di_pre_stru.buf_alloc_mode = 0;
		/*nr_size(bits)=w*active_h*8*2(yuv422)
		* mtn(bits)=w*active_h*4
		* cont(bits)=w*active_h*4 mv(bits)=w*active_h/5*16
		* mcinfo(bits)=active_h*16*/
		nr_size = (nr_width * canvas_height)*8*2/16;
		mtn_size = (mtn_width * canvas_height)*4/16;
		count_size = (mtn_width * canvas_height)*4/16;
		mv_size = (mv_width * canvas_height)/5;
		mc_size = canvas_height;
		if (mcpre_en) {
			di_buf_size = nr_size + mtn_size + count_size +
				mv_size + mc_size;
		} else {
#ifdef NEW_DI_V1
			di_buf_size = nr_size + mtn_size + count_size;
#else
			di_buf_size = nr_size + mtn_size;
#endif
		}
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);
		local_buf_num = de_devp->mem_size / di_buf_size;

		if (post_wr_en && post_wr_surpport) {
			local_buf_num = (de_devp->mem_size +
				(nr_width*canvas_height<<2)) /
				(di_buf_size + (nr_width*canvas_height<<1));
		}

		if (local_buf_num > MAX_LOCAL_BUF_NUM)
			local_buf_num = MAX_LOCAL_BUF_NUM;

		if (local_buf_num >= 8)
			new_keep_last_frame_enable = 1;
		else
			new_keep_last_frame_enable = 0;
	}
	de_devp->buffer_size = di_buf_size;
	di_pre_stru.nr_size = nr_size;
	di_pre_stru.count_size = count_size;
	di_pre_stru.mtn_size = mtn_size;
	di_pre_stru.mv_size = mv_size;
	di_pre_stru.mcinfo_size = mc_size;
	same_w_r_canvas_count = 0;
	same_field_top_count = 0;
	same_field_bot_count = 0;

	queue_init(local_buf_num);
	for (i = 0; i < local_buf_num; i++) {
		struct di_buf_s *di_buf = &(di_buf_local[i]);
		int ii = USED_LOCAL_BUF_MAX;
		if ((used_post_buf_index != -1) &&
		    (new_keep_last_frame_enable)) {
			for (ii = 0; ii < USED_LOCAL_BUF_MAX; ii++) {
				if (i == used_local_buf_index[ii]) {
					di_print("%s skip %d\n", __func__, i);
					break;
				}
			}
		}

		if (ii >= USED_LOCAL_BUF_MAX) {
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->type = VFRAME_TYPE_LOCAL;
			di_buf->pre_ref_count = 0;
			di_buf->post_ref_count = 0;
			di_buf->canvas_width[NR_CANVAS] = nr_canvas_width;
			di_buf->canvas_width[MTN_CANVAS] = mtn_canvas_width;
			di_buf->canvas_width[MV_CANVAS] = mv_canvas_width;
			if (prog_flag) {
				di_buf->canvas_height = canvas_height;
				di_buf->nr_adr = de_devp->mem_start +
					di_buf_size * i;
				di_buf->canvas_config_flag = 1;
			} else {
				di_buf->canvas_height = (canvas_height>>1);
				di_buf->nr_adr = de_devp->mem_start +
					di_buf_size * i;
				di_buf->mtn_adr = de_devp->mem_start +
					di_buf_size * i +
					nr_size;
#ifdef NEW_DI_V1
				di_buf->cnt_adr = de_devp->mem_start +
					di_buf_size * i +
					nr_size + mtn_size;
#endif

				if (mcpre_en) {
					di_buf->mcvec_adr = de_devp->mem_start +
						di_buf_size * i +
						nr_size + mtn_size + count_size;
					di_buf->mcinfo_adr =
						de_devp->mem_start +
						di_buf_size * i + nr_size +
						mtn_size + count_size + mv_size;
				}
				di_buf->canvas_config_flag = 2;
			}
			di_buf->index = i;
			di_buf->vframe = &(vframe_local[i]);
			di_buf->vframe->private_data = di_buf;
			di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
			di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			queue_in(di_buf, QUEUE_LOCAL_FREE);
		}
	}
#ifdef CONFIG_CMA
	if (de_devp->flag_cma == 1) {
		pr_dbg("%s:cma alloc req time: %d ms\n",
			__func__, jiffies_to_msecs(jiffies));
		di_pre_stru.cma_alloc_req = 1;
		up(&di_sema);
	}
#endif
	if (post_wr_en && post_wr_surpport) {
		di_post_mem = de_devp->mem_start + di_buf_size*local_buf_num;
		di_post_buf_size = width * canvas_height*2;
		/* pre buffer must 2 more than post buffer */
		di_post_stru.di_post_num = local_buf_num - 2;
		pr_info("DI: di post buffer size %u byte.\n", di_post_buf_size);
	} else {
		di_post_stru.di_post_num = MAX_POST_BUF_NUM;
		di_post_buf_size = 0;
	}
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		struct di_buf_s *di_buf = &(di_buf_in[i]);
		if (di_buf) {
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->type = VFRAME_TYPE_IN;
			di_buf->pre_ref_count = 0;
			di_buf->post_ref_count = 0;
			di_buf->vframe = &(vframe_in_dup[i]);
			di_buf->vframe->private_data = di_buf;
			di_buf->index = i;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			queue_in(di_buf, QUEUE_IN_FREE);
		}
	}

	for (i = 0; i < di_post_stru.di_post_num; i++) {
		struct di_buf_s *di_buf = &(di_buf_post[i]);
		if (di_buf) {
			if (i != used_post_buf_index) {
				memset(di_buf, 0, sizeof(struct di_buf_s));
				di_buf->type = VFRAME_TYPE_POST;
				di_buf->index = i;
				di_buf->vframe = &(vframe_post[i]);
				di_buf->vframe->private_data = di_buf;
				di_buf->queue_index = -1;
				di_buf->invert_top_bot_flag = 0;
				if (post_wr_en && post_wr_surpport) {
					di_buf->canvas_width[NR_CANVAS] =
						(nr_width << 1);
					di_buf->canvas_height = canvas_height;
					di_buf->canvas_config_flag = 1;
					di_buf->nr_adr = di_post_mem +
						di_post_buf_size*i;
				}
				queue_in(di_buf, QUEUE_POST_FREE);
			}
		}
	}
	return 0;
}

static void di_uninit_buf(void)
{
	struct di_buf_s *p = NULL;/* , *ptmp; */
	int i, ii = 0;
	int itmp;

	/* vframe_t* cur_vf = get_cur_dispbuf(); */
	if (!queue_empty(QUEUE_DISPLAY)) {
		for (i = 0; i < USED_LOCAL_BUF_MAX; i++)
			used_local_buf_index[i] = -1;

		used_post_buf_index = -1;
	}

	queue_for_each_entry(p, ptmp, QUEUE_DISPLAY, list) {
	if (p->di_buf[0]->type != VFRAME_TYPE_IN &&
		(p->process_fun_index != PROCESS_FUN_NULL) &&
		(ii < USED_LOCAL_BUF_MAX) &&
		(p->index == di_post_stru.cur_disp_index)) {
		used_post_buf_index = p->index;
		for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
			if (p->di_buf_dup_p[i] == NULL)
				continue;
			used_local_buf_index[ii] =
				p->di_buf_dup_p[i]->index;
			/* prepare for recycle
			 * the keep buffer*/
			p->di_buf_dup_p[i]->pre_ref_count = 0;
			p->di_buf_dup_p[i]->post_ref_count = 0;
			if ((p->di_buf_dup_p[i]->queue_index >= 0) &&
			(p->di_buf_dup_p[i]->queue_index < QUEUE_NUM)) {
				if (is_in_queue(p->di_buf_dup_p[i],
				p->di_buf_dup_p[i]->queue_index))
					queue_out(p->di_buf_dup_p[i]);
			}
			ii++;
			if (p->di_buf_dup_p[i]->di_wr_linked_buf)
				used_local_buf_index[ii] =
				p->di_buf_dup_p[i]->di_wr_linked_buf->index;
		}
		queue_out(p);
		break;
	}
	}
	if (used_post_buf_index != -1) {
		pr_info("%s keep cur di_buf %d (%d %d %d)\n",
			__func__, used_post_buf_index, used_local_buf_index[0],
			used_local_buf_index[1], used_local_buf_index[2]);
	}
#ifdef USE_LIST
	list_for_each_entry_safe(p, ptmp, &local_free_list_head, list) {
		list_del(&p->list);
	}
	list_for_each_entry_safe(p, ptmp, &in_free_list_head, list) {
		list_del(&p->list);
	}
	list_for_each_entry_safe(p, ptmp, &pre_ready_list_head, list) {
		list_del(&p->list);
	}
	list_for_each_entry_safe(p, ptmp, &recycle_list_head, list) {
		list_del(&p->list);
	}
	list_for_each_entry_safe(p, ptmp, &post_free_list_head, list) {
		list_del(&p->list);
	}
	list_for_each_entry_safe(p, ptmp, &post_ready_list_head, list) {
		list_del(&p->list);
	}
	list_for_each_entry_safe(p, ptmp, &display_list_head, list) {
		list_del(&p->list);
	}
#else
	queue_init(0);
#endif
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (vframe_in[i]) {
			vf_put(vframe_in[i], VFM_NAME);
			vf_notify_provider(
				VFM_NAME, VFRAME_EVENT_RECEIVER_PUT, NULL);
			vframe_in[i] = NULL;
		}
	}
#ifdef CONFIG_CMA
	if (de_devp->flag_cma == 1) {
		pr_dbg("%s:cma release req time: %d ms\n",
			__func__, jiffies_to_msecs(jiffies));
		di_pre_stru.cma_release_req = 1;
		up(&di_sema);
	}
#endif
	di_pre_stru.pre_de_process_done = 0;
	di_pre_stru.pre_de_busy = 0;
	if (post_wr_en && post_wr_surpport) {
		di_post_stru.cur_post_buf = NULL;
		di_post_stru.post_de_busy = 0;
		di_post_stru.de_post_process_done = 0;
	}
}

static void log_buffer_state(unsigned char *tag)
{
	if (di_log_flag & DI_LOG_BUFFER_STATE) {
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
		ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;
		di_lock_irqfiq_save(irq_flag2, fiq_flag);
		in_free = list_count(QUEUE_IN_FREE);
		local_free = list_count(QUEUE_LOCAL_FREE);
		pre_ready = list_count(QUEUE_PRE_READY);
		post_free = list_count(QUEUE_POST_FREE);
		post_ready = list_count(QUEUE_POST_READY);
		queue_for_each_entry(p, ptmp, QUEUE_POST_READY, list) {
			if (p->di_buf[0])
				post_ready_ext++;

			if (p->di_buf[1])
				post_ready_ext++;
		}
		queue_for_each_entry(p, ptmp, QUEUE_DISPLAY, list) {
			display++;
			if (p->di_buf[0])
				display_ext++;

			if (p->di_buf[1])
				display_ext++;
		}
		recycle = list_count(QUEUE_RECYCLE);

		if (di_pre_stru.di_inp_buf)
			di_inp++;
		if (di_pre_stru.di_wr_buf)
			di_wr++;

		if (buf_state_log_threshold == 0)
			buf_state_log_start = 0;
		else if (post_ready < buf_state_log_threshold)
			buf_state_log_start = 1;

		if (buf_state_log_start) {
			di_print(
		"[%s]i %d, i_f %d/%d, l_f %d/%d, pre_r %d, post_f %d/%d,",
				tag,
				provider_vframe_level,
				in_free, MAX_IN_BUF_NUM,
				local_free, local_buf_num,
				pre_ready,
				post_free, MAX_POST_BUF_NUM);
			di_print(
		"post_r (%d:%d), disp (%d:%d),rec %d, di_i %d, di_w %d\n",
				post_ready, post_ready_ext,
				display, display_ext,
				recycle,
				di_inp, di_wr
				);
		}
		di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
	}
}

static void dump_di_buf(struct di_buf_s *di_buf)
{
	pr_info("di_buf %p vframe %p:\n", di_buf, di_buf->vframe);
	pr_info("index %d, post_proc_flag %d, new_format_flag %d, type %d,",
		di_buf->index, di_buf->post_proc_flag,
		di_buf->new_format_flag, di_buf->type);
	pr_info("seq %d, pre_ref_count %d,post_ref_count %d, queue_index %d,",
		di_buf->seq, di_buf->pre_ref_count, di_buf->post_ref_count,
		di_buf->queue_index);
	pr_info("pulldown_mode %d process_fun_index %d\n",
		di_buf->pulldown_mode, di_buf->process_fun_index);
	pr_info("di_buf: %p, %p, di_buf_dup_p: %p, %p, %p, %p, %p\n",
		di_buf->di_buf[0], di_buf->di_buf[1], di_buf->di_buf_dup_p[0],
		di_buf->di_buf_dup_p[1], di_buf->di_buf_dup_p[2],
		di_buf->di_buf_dup_p[3], di_buf->di_buf_dup_p[4]);
	pr_info(
	"nr_adr 0x%lx, nr_canvas_idx 0x%x, mtn_adr 0x%lx, mtn_canvas_idx 0x%x",
		di_buf->nr_adr, di_buf->nr_canvas_idx, di_buf->mtn_adr,
		di_buf->mtn_canvas_idx);
#ifdef NEW_DI_V1
	pr_info("cnt_adr 0x%lx, cnt_canvas_idx 0x%x\n",
		di_buf->cnt_adr, di_buf->cnt_canvas_idx);
#endif
	pr_info("di_cnt %d, priveated %u.\n",
			atomic_read(&di_buf->di_cnt), di_buf->privated);
}

static void dump_pool(int index)
{
	int j;
	queue_t *q = &queue[index];

	pr_info("queue[%d]: in_idx %d, out_idx %d, num %d, type %d\n",
		index, q->in_idx, q->out_idx, q->num, q->type);
	for (j = 0; j < MAX_QUEUE_POOL_SIZE; j++) {
		pr_info("0x%x ", q->pool[j]);
		if (((j + 1) % 16) == 0)
			pr_debug("\n");
	}
	pr_info("\n");
}

static void dump_vframe(vframe_t *vf)
{
	pr_info("vframe %p:\n", vf);
	pr_info("index %d, type 0x%x, type_backup 0x%x, blend_mode %d bitdepth %d\n",
		vf->index, vf->type, vf->type_backup,
		vf->blend_mode, (vf->bitdepth&BITDEPTH_Y10)?10:8);
	pr_info("duration %d, duration_pulldown %d, pts %d, flag 0x%x\n",
		vf->duration, vf->duration_pulldown, vf->pts, vf->flag);
	pr_info("canvas0Addr 0x%x, canvas1Addr 0x%x, bufWidth %d\n",
		vf->canvas0Addr, vf->canvas1Addr, vf->bufWidth);
	pr_info("width %d, height %d, ratio_control 0x%x, orientation 0x%x\n",
		vf->width, vf->height, vf->ratio_control, vf->orientation);
	pr_info("source_type %d, phase %d, soruce_mode %d, sig_fmt %d\n",
		vf->source_type, vf->phase, vf->source_mode, vf->sig_fmt);
	pr_info(
		"trans_fmt 0x%x, lefteye(%d %d %d %d), righteye(%d %d %d %d)\n",
		vf->trans_fmt, vf->left_eye.start_x, vf->left_eye.start_y,
		vf->left_eye.width, vf->left_eye.height,
		vf->right_eye.start_x, vf->right_eye.start_y,
		vf->right_eye.width, vf->right_eye.height);
	pr_info("mode_3d_enable %d",
		vf->mode_3d_enable);
	pr_info("early_process_fun 0x%p, process_fun 0x%p, private_data %p\n",
		vf->early_process_fun,
		vf->process_fun, vf->private_data);
	pr_info("pixel_ratio %d list %p\n",
		vf->pixel_ratio, &vf->list);
}

static void print_di_buf(struct di_buf_s *di_buf, int format)
{
	if (!di_buf)
		return;
	if (format == 1) {
		pr_info(
		"\t+index %d, 0x%p, type %d, vframetype 0x%x, trans_fmt %u,bitdepath %d\n",
			di_buf->index,
			di_buf,
			di_buf->type,
			di_buf->vframe->type,
			di_buf->vframe->trans_fmt,
			di_buf->vframe->bitdepth);
		if (di_buf->di_wr_linked_buf) {
			pr_info("\tlinked  +index %d, 0x%p, type %d\n",
				di_buf->di_wr_linked_buf->index,
				di_buf->di_wr_linked_buf,
				di_buf->di_wr_linked_buf->type);
		}
	} else if (format == 2) {
		pr_info("index %d, 0x%p(vframe 0x%p), type %d\n",
			di_buf->index, di_buf,
			di_buf->vframe, di_buf->type);
		pr_info("vframetype 0x%x, trans_fmt %u,duration %d pts %d,bitdepth %d\n",
			di_buf->vframe->type,
			di_buf->vframe->trans_fmt,
			di_buf->vframe->duration,
			di_buf->vframe->pts,
			di_buf->vframe->bitdepth);
		if (di_buf->di_wr_linked_buf) {
			pr_info("linked index %d, 0x%p, type %d\n",
				di_buf->di_wr_linked_buf->index,
				di_buf->di_wr_linked_buf,
				di_buf->di_wr_linked_buf->type);
		}
	}
}

static void dump_state(void)
{
	struct di_buf_s *p = NULL;/* , *ptmp; */
	int itmp;
	int i;

	dump_state_flag = 1;
	pr_info("version %s, provider vframe level %d,",
		version_s, provider_vframe_level);
	pr_info("init_flag %d, is_bypass %d, receiver_is_amvideo %d\n",
		init_flag, is_bypass(NULL), receiver_is_amvideo);
	pr_info("recovery_flag = %d, recovery_log_reason=%d, di_blocking=%d",
		recovery_flag, recovery_log_reason, di_blocking);
	pr_info("recovery_log_queue_idx=%d, recovery_log_di_buf=0x%p\n",
		recovery_log_queue_idx, recovery_log_di_buf);
	pr_info("buffer_size=%d,mem_flag=%d\n", de_devp->buffer_size, mem_flag);
	pr_info("new_keep_last_frame_enable %d,", new_keep_last_frame_enable);
	pr_info("used_post_buf_index %d(0x%p),", used_post_buf_index,
		(used_post_buf_index ==
		 -1) ? NULL : &(di_buf_post[used_post_buf_index]));
	pr_info("used_local_buf_index:\n");
	for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
		int tmp = used_local_buf_index[i];
		pr_info("%d(0x%p) ", tmp,
			(tmp == -1) ? NULL :  &(di_buf_local[tmp]));
	}

	pr_info("\nin_free_list (max %d):\n", MAX_IN_BUF_NUM);
	queue_for_each_entry(p, ptmp, QUEUE_IN_FREE, list) {
		pr_info("index %2d, 0x%p, type %d\n",
			p->index, p, p->type);
	}
	pr_info("local_free_list (max %d):\n", local_buf_num);
	queue_for_each_entry(p, ptmp, QUEUE_LOCAL_FREE, list) {
		pr_info("index %2d, 0x%p, type %d\n", p->index, p, p->type);
	}

	pr_info("post_doing_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_POST_DOING, list) {
		print_di_buf(p, 2);
	}
	pr_info("pre_ready_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_PRE_READY, list) {
		print_di_buf(p, 2);
	}
	pr_info("post_free_list (max %d):\n", di_post_stru.di_post_num);
	queue_for_each_entry(p, ptmp, QUEUE_POST_FREE, list) {
		pr_info("index %2d, 0x%p, type %d, vframetype 0x%x\n",
			p->index, p, p->type, p->vframe->type);
	}
	pr_info("post_ready_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_POST_READY, list) {
		print_di_buf(p, 2);
		print_di_buf(p->di_buf[0], 1);
		print_di_buf(p->di_buf[1], 1);
	}
	pr_info("display_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_DISPLAY, list) {
		print_di_buf(p, 2);
		print_di_buf(p->di_buf[0], 1);
		print_di_buf(p->di_buf[1], 1);
	}
	pr_info("recycle_list:\n");
	queue_for_each_entry(p, ptmp, QUEUE_RECYCLE, list) {
		pr_info(
"index %d, 0x%p, type %d, vframetype 0x%x pre_ref_count %d post_ref_count %d\n",
			p->index, p, p->type,
			p->vframe->type,
			p->pre_ref_count,
			p->post_ref_count);
		if (p->di_wr_linked_buf) {
			pr_info(
	"linked index %2d, 0x%p, type %d pre_ref_count %d post_ref_count %d\n",
				p->di_wr_linked_buf->index,
				p->di_wr_linked_buf,
				p->di_wr_linked_buf->type,
				p->di_wr_linked_buf->pre_ref_count,
				p->di_wr_linked_buf->post_ref_count);
		}
	}
	if (di_pre_stru.di_inp_buf) {
		pr_info("di_inp_buf:index %d, 0x%p, type %d\n",
			di_pre_stru.di_inp_buf->index,
			di_pre_stru.di_inp_buf,
			di_pre_stru.di_inp_buf->type);
	} else {
		pr_info("di_inp_buf: NULL\n");
	}
	if (di_pre_stru.di_wr_buf) {
		pr_info("di_wr_buf:index %d, 0x%p, type %d\n",
			di_pre_stru.di_wr_buf->index,
			di_pre_stru.di_wr_buf,
			di_pre_stru.di_wr_buf->type);
	} else {
		pr_info("di_wr_buf: NULL\n");
	}
	dump_di_pre_stru();
	dump_di_post_stru();
	pr_info("vframe_in[]:");

	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		pr_info("0x%p ", vframe_in[i]);

	pr_info("\n");
	pr_info("vf_peek()=>0x%p,di_process_cnt = %d\n",
		vf_peek(VFM_NAME), di_process_cnt);
	pr_info("video_peek_cnt = %d,force_trig_cnt = %d\n",
		video_peek_cnt, force_trig_cnt);
	pr_info("reg_unreg_timerout = %lu\n", reg_unreg_timeout_cnt);
	dump_state_flag = 0;
}

static unsigned char check_di_buf(struct di_buf_s *di_buf, int reason)
{
	int error = 0;

	if (di_buf == NULL) {
		pr_dbg("%s: Error %d, di_buf is NULL\n", __func__, reason);
		return 1;
	} else {
		if (di_buf->type == VFRAME_TYPE_IN) {
			if (di_buf->vframe != &vframe_in_dup[di_buf->index])
				error = 1;
		} else if (di_buf->type == VFRAME_TYPE_LOCAL) {
			if (di_buf->vframe !=
			    &vframe_local[di_buf->index])
				error = 1;
		} else if (di_buf->type == VFRAME_TYPE_POST) {
			if (di_buf->vframe != &vframe_post[di_buf->index])
				error = 1;
		} else {
			error = 1;
		}

		if (error) {
			pr_dbg(
				"%s: Error %d, di_buf wrong\n", __func__,
				reason);
			if (recovery_flag == 0)
				recovery_log_reason = reason;
			recovery_flag++;
			dump_di_buf(di_buf);
			return 1;
		}
	}
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
		di_mcinford_mif->size_x = di_buf->vframe->height / 4 - 1;
		di_mcinford_mif->size_y = 1;
		di_mcinford_mif->canvas_num = di_buf->mcinfo_canvas_idx;
	}
}
static void
config_di_pre_mc_mif(struct DI_MC_MIF_s *di_mcinfo_mif,
		     struct DI_MC_MIF_s *di_mcvec_mif,
		     struct di_buf_s *di_buf)
{
	if (di_buf) {
		di_mcinfo_mif->size_x = di_buf->vframe->height / 4 - 1;
		di_mcinfo_mif->size_y = 1;
		di_mcinfo_mif->canvas_num = di_buf->mcinfo_canvas_idx;

		di_mcvec_mif->size_x = (di_buf->vframe->width + 4) / 5 - 1;
		di_mcvec_mif->size_y = di_buf->vframe->height / 2 - 1;
		di_mcvec_mif->canvas_num = di_buf->mcvec_canvas_idx;
	}
}
#ifdef NEW_DI_V1
static void config_di_cnt_mif(struct DI_SIM_MIF_s *di_cnt_mif,
	struct di_buf_s *di_buf)
{
	if (di_buf) {
		di_cnt_mif->start_x = 0;
		di_cnt_mif->end_x = di_buf->vframe->width - 1;
		di_cnt_mif->start_y = 0;
		di_cnt_mif->end_y = di_buf->vframe->height / 2 - 1;
		di_cnt_mif->canvas_num = di_buf->cnt_canvas_idx;
	}
}
#endif

static void
config_di_wr_mif(struct DI_SIM_MIF_s *di_nrwr_mif,
		 struct DI_SIM_MIF_s *di_mtnwr_mif,
		 struct di_buf_s *di_buf,
		 vframe_t *in_vframe)
{
	di_nrwr_mif->canvas_num = di_buf->nr_canvas_idx;
	di_nrwr_mif->start_x = 0;
	di_nrwr_mif->end_x = in_vframe->width - 1;
	di_nrwr_mif->start_y = 0;
	if (di_buf->vframe->bitdepth & BITDEPTH_Y10)
		di_nrwr_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
	else
		di_nrwr_mif->bit_mode = 0;

	if (di_pre_stru.prog_proc_type == 0)
		di_nrwr_mif->end_y = in_vframe->height / 2 - 1;
	else
		di_nrwr_mif->end_y = in_vframe->height - 1;
	if (di_pre_stru.prog_proc_type == 0) {
		di_mtnwr_mif->start_x = 0;
		di_mtnwr_mif->end_x = in_vframe->width - 1;
		di_mtnwr_mif->start_y = 0;
		di_mtnwr_mif->end_y = in_vframe->height / 2 - 1;
		di_mtnwr_mif->canvas_num = di_buf->mtn_canvas_idx;
	}
}
static bool force_prog;
module_param_named(force_prog, force_prog, bool, 0664);
static void config_di_mif(struct DI_MIF_s *di_mif, struct di_buf_s *di_buf)
{
	if (di_buf == NULL)
		return;
	di_mif->canvas0_addr0 =
		di_buf->vframe->canvas0Addr & 0xff;
	di_mif->canvas0_addr1 =
		(di_buf->vframe->canvas0Addr >> 8) & 0xff;
	di_mif->canvas0_addr2 =
		(di_buf->vframe->canvas0Addr >> 16) & 0xff;

	if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:2;
		else if (di_buf->vframe->type & VIDTYPE_VIU_422)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
	} else {
		di_mif->bit_mode = 0;
	}
	if (di_buf->vframe->type & VIDTYPE_VIU_422) {
		/* from vdin or local vframe */
		if ((!is_progressive(di_buf->vframe))
		    || (di_pre_stru.prog_proc_type)
		    ) {
			di_mif->video_mode = 0;
			di_mif->set_separate_en = 0;
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0;
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			if (di_pre_stru.prog_proc_type)
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
		    (di_pre_stru.prog_proc_type)) {
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
			di_mif->src_prog = force_prog?1:0;
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
						- (di_mif->src_prog?1:2);
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
					(di_mif->src_prog?0:1);
				di_mif->chroma_y_end0 =
					di_buf->vframe->height / 2 - 1;
			}
		}
	}
}

static void pre_de_process(void)
{
	int chan2_field_num = 1;
	int canvases_idex = di_pre_stru.field_count_for_cont % 2;

#ifdef NEW_DI_V1
	int cont_rd = 1;
#endif
	unsigned int blkhsize = 0;
	di_print("%s: start\n", __func__);
	di_pre_stru.pre_de_busy = 1;
	di_pre_stru.pre_de_busy_timer_count = 0;

	config_di_mif(&di_pre_stru.di_inp_mif, di_pre_stru.di_inp_buf);
	/* pr_dbg("set_separate_en=%d vframe->type %d\n",
	 * di_pre_stru.di_inp_mif.set_separate_en,
	 * di_pre_stru.di_inp_buf->vframe->type); */
#ifdef DI_USE_FIXED_CANVAS_IDX
	if ((di_pre_stru.di_mem_buf_dup_p != NULL &&
	     di_pre_stru.di_mem_buf_dup_p != di_pre_stru.di_inp_buf)) {
		config_canvas_idx(di_pre_stru.di_mem_buf_dup_p,
			di_pre_idx[canvases_idex][0], -1);
#ifdef NEW_DI_V1
		config_cnt_canvas_idx(di_pre_stru.di_mem_buf_dup_p,
			di_pre_idx[canvases_idex][1]);
#endif
	}
	if (di_pre_stru.di_chan2_buf_dup_p != NULL) {
		config_canvas_idx(di_pre_stru.di_chan2_buf_dup_p,
			di_pre_idx[canvases_idex][2], -1);
#ifdef NEW_DI_V1
		config_cnt_canvas_idx(di_pre_stru.di_chan2_buf_dup_p,
			di_pre_idx[canvases_idex][3]);
#endif
	}
	config_canvas_idx(di_pre_stru.di_wr_buf,
		di_pre_idx[canvases_idex][4], di_pre_idx[canvases_idex][5]);
#ifdef NEW_DI_V1
	config_cnt_canvas_idx(di_pre_stru.di_wr_buf,
		di_pre_idx[canvases_idex][6]);
#endif
	if (mcpre_en) {
		if (di_pre_stru.di_chan2_buf_dup_p != NULL)
			config_mcinfo_canvas_idx(di_pre_stru.di_chan2_buf_dup_p,
				di_pre_idx[canvases_idex][7]);
		config_mcinfo_canvas_idx(di_pre_stru.di_wr_buf,
			di_pre_idx[canvases_idex][8]);
		config_mcvec_canvas_idx(di_pre_stru.di_wr_buf,
			di_pre_idx[canvases_idex][9]);
	}
#endif
	config_di_mif(&di_pre_stru.di_mem_mif, di_pre_stru.di_mem_buf_dup_p);
	config_di_mif(&di_pre_stru.di_chan2_mif,
		di_pre_stru.di_chan2_buf_dup_p);
	config_di_wr_mif(&di_pre_stru.di_nrwr_mif, &di_pre_stru.di_mtnwr_mif,
		di_pre_stru.di_wr_buf, di_pre_stru.di_inp_buf->vframe);

#ifdef NEW_DI_V1
	config_di_cnt_mif(&di_pre_stru.di_contp2rd_mif,
		di_pre_stru.di_mem_buf_dup_p);
	config_di_cnt_mif(&di_pre_stru.di_contprd_mif,
		di_pre_stru.di_chan2_buf_dup_p);
	config_di_cnt_mif(&di_pre_stru.di_contwr_mif, di_pre_stru.di_wr_buf);
#endif
	if (mcpre_en) {
		config_di_mcinford_mif(&di_pre_stru.di_mcinford_mif,
			di_pre_stru.di_chan2_buf_dup_p);
		config_di_pre_mc_mif(&di_pre_stru.di_mcinfowr_mif,
			&di_pre_stru.di_mcvecwr_mif, di_pre_stru.di_wr_buf);
	}

	if ((di_pre_stru.di_chan2_buf_dup_p) &&
	    ((di_pre_stru.di_chan2_buf_dup_p->vframe->type & VIDTYPE_TYPEMASK)
	     == VIDTYPE_INTERLACE_TOP))
		chan2_field_num = 0;

	RDMA_WR(DI_PRE_SIZE, di_pre_stru.di_nrwr_mif.end_x |
		(di_pre_stru.di_nrwr_mif.end_y << 16));
	if (mcpre_en) {
		blkhsize = (di_pre_stru.di_nrwr_mif.end_x + 4) / 5;
		RDMA_WR(MCDI_HV_SIZEIN, (di_pre_stru.di_nrwr_mif.end_y + 1)
			| ((di_pre_stru.di_nrwr_mif.end_x + 1) << 16));
		RDMA_WR(MCDI_HV_BLKSIZEIN, (overturn ? 3 : 0) << 30
			| blkhsize << 16
			| (di_pre_stru.di_nrwr_mif.end_y + 1));
		RDMA_WR(MCDI_BLKTOTAL,
			blkhsize * (di_pre_stru.di_nrwr_mif.end_y + 1));
	}
	/* set interrupt mask for pre module. */
	/* we need to only leave one mask open
	   to prevent multiple entry for de_irq */
	RDMA_WR(DI_INTR_CTRL,
		/* mask nrwr interrupt. */
		((di_pre_stru.enable_mtnwr ? 1 : 0) << 16) |
		/* mtnwr interrupt. */
		((di_pre_stru.enable_mtnwr ? 0 : 1) << 17) |
		/* mask diwr intrpt. */
		(((post_wr_en && post_wr_surpport)?0:1) << 18) |
		(1 << 19) | /* mask hist check interrupt. */
		(1 << 20) | /* mask cont interrupt. */
		(1 << 21) | /* mask medi interrupt. */
		(1 << 22) | /* mask vecwr interrupt. */
		(1 << 23) | /* mask infwr interrupt. */
		#ifdef DET3D
		((det3d_en ? 0 : 1) << 24) | /* mask det3d interrupt. */
		#else
		(1 << 24) | /* mask det3d interrupt. */
		#endif
		((post_wr_en && post_wr_surpport)?0xb:0xf));
		/* clean all pending interrupt bits. */

	enable_di_pre_aml(&di_pre_stru.di_inp_mif,      /* di_inp */
		&di_pre_stru.di_mem_mif,                /* di_mem */
		&di_pre_stru.di_chan2_mif,              /* chan2 */
		&di_pre_stru.di_nrwr_mif,               /* nrwrite */
		&di_pre_stru.di_mtnwr_mif,              /* mtn write */
#ifdef NEW_DI_V1
		&di_pre_stru.di_contp2rd_mif,
		&di_pre_stru.di_contprd_mif,
		&di_pre_stru.di_contwr_mif,
#endif
		1,                              /* nr enable */
		di_pre_stru.enable_mtnwr,       /* mtn enable */
		di_pre_stru.enable_pulldown_check,
/* pd32 check_en */
		di_pre_stru.enable_pulldown_check,
/* pd22 check_en */
		0, /* hist check_en */
		chan2_field_num,
/* field num for chan2. 1 bottom, 0 top. */
		di_pre_stru.vdin2nr,    /* pre vdin link. */
		pre_hold_line,          /* hold line. */
		pre_urgent
		);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		enable_afbc_input(di_pre_stru.di_inp_buf->vframe);

	if (mcpre_en) {
		if (!di_pre_stru.cur_prog_flag && mcpre_en)
			enable_mc_di_pre(&di_pre_stru.di_mcinford_mif,
				&di_pre_stru.di_mcinfowr_mif,
				&di_pre_stru.di_mcvecwr_mif, pre_urgent);
	}
	if (di_pre_stru.cur_prog_flag == 1 || di_pre_stru.enable_mtnwr == 0) {
		di_mtn_1_ctrl1 &= (~(1 << 31)); /* disable contwr */
		di_mtn_1_ctrl1 &= (~(1 << 29)); /* disable txt */
		cont_rd = 0;
		if (mcpre_en)
			RDMA_WR(DI_ARB_CTRL,
				(RDMA_RD(DI_ARB_CTRL) & (~0x303)) | 0xF0F);
		else
			RDMA_WR(DI_ARB_CTRL,
				RDMA_RD(DI_ARB_CTRL) | 1 << 9 | 1 << 8
				| 1 << 1 | 1 <<	0);
	} else {
		if (mcpre_en)
			RDMA_WR(DI_ARB_CTRL,
				RDMA_RD(DI_ARB_CTRL) | 1 << 9 | 1 << 8
				| 1 << 1 | 1 << 0 | 0xF0F);
		else
			RDMA_WR(DI_ARB_CTRL,
				RDMA_RD(DI_ARB_CTRL) | 1 << 9 | 1 << 8
				| 1 << 1 | 1 <<	0);

		di_mtn_1_ctrl1 |= (1 << 31); /* enable contwr */
		RDMA_WR(DI_PRE_CTRL, RDMA_RD(DI_PRE_CTRL) | (1 << 1));
		/* mtn must enable for mtn1 enable */
		di_mtn_1_ctrl1 &= (~(1 << 29));/* disable txt */
		cont_rd = 1;
	}
	if (di_pre_stru.field_count_for_cont >= 3) {
		di_mtn_1_ctrl1 |= 1 << 29;/* enable txt */
		RDMA_WR(DI_PRE_CTRL,
			RDMA_RD(DI_PRE_CTRL) | (cont_rd << 25));
#ifdef NEW_DI_V4
		if ((!dnr_en) && (Rd(DNR_CTRL) != 0) && dnr_reg_update)
			RDMA_WR(DNR_CTRL, 0);
#endif
		if (mcpre_en) {
			if ((di_pre_stru.cur_prog_flag == 0) &&
				(di_pre_stru.enable_mtnwr == 1)
			   )
				RDMA_WR(DI_MTN_CTRL1,
					(mcpre_en ? 0x3000 : 0) |
					RDMA_RD(DI_MTN_CTRL1));
			else
				RDMA_WR(DI_MTN_CTRL1,
					(0xffffcfff & RDMA_RD(DI_MTN_CTRL1)));
			/* enable me(mc di) */
			if (di_pre_stru.field_count_for_cont == 4) {
				di_mtn_1_ctrl1 &= (~(1 << 30));
				/* enable contp2rd and contprd */
				RDMA_WR(MCDI_MOTINEN, 1 << 1 | 1);
			}
			if (di_pre_stru.field_count_for_cont == 5)
				RDMA_WR(MCDI_CTRL_MODE, 0x1bfff7ff);
				/* disalbe reflinfo */
		} else {
			di_mtn_1_ctrl1 &= (~(1 << 30));
		}
		/* enable contp2rd and contprd */
	} else {
		enable_film_mode_check(
			di_pre_stru.di_inp_buf->vframe->width,
			di_pre_stru.di_inp_buf->vframe->height / 2,
			di_pre_stru.di_inp_buf->vframe->source_type);
		if (mcpre_en) {
			/* txtdet_en mode */
			RDMA_WR_BITS(MCDI_CTRL_MODE, 0, 1, 1);
			RDMA_WR_BITS(MCDI_CTRL_MODE, 1, 9, 1);
			RDMA_WR_BITS(MCDI_CTRL_MODE, 1, 16, 1);
			RDMA_WR_BITS(MCDI_CTRL_MODE, 0, 28, 1);
			RDMA_WR(MCDI_MOTINEN, 0);
			RDMA_WR(DI_MTN_CTRL1,
				(0xffffcfff & RDMA_RD(DI_MTN_CTRL1)));
			/* disable me(mc di) */
		}
#ifdef NEW_DI_V4
		RDMA_WR(DNR_CTRL, 0);
#endif
	}
	di_pre_stru.field_count_for_cont++;

	RDMA_WR(DI_MTN_1_CTRL1, di_mtn_1_ctrl1);
	di_apply_reg_cfg(0);
#ifdef SUPPORT_MPEG_TO_VDIN
	if (mpeg2vdin_flag) {
		struct vdin_arg_s vdin_arg;
		struct vdin_v4l2_ops_s *vdin_ops = get_vdin_v4l2_ops();
		vdin_arg.cmd = VDIN_CMD_FORCE_GO_FIELD;
		if (vdin_ops->tvin_vdin_func)
			vdin_ops->tvin_vdin_func(0, &vdin_arg);
	}
#endif

	/* enable pre mif*/
	enable_di_pre_mif(1);
	di_print("DI:buf[%d] irq start.\n", di_pre_stru.di_inp_buf->seq);
#ifdef CONFIG_AML_RDMA
	if (di_pre_rdma_enable & 0x2)
		rdma_config(de_devp->rdma_handle, RDMA_TRIGGER_MANUAL);
	else if (di_pre_rdma_enable & 1)
		rdma_config(de_devp->rdma_handle, RDMA_DEINT_IRQ);
#endif
}

static void pre_de_done_buf_clear(void)
{
	struct di_buf_s *wr_buf = NULL;

	if (di_pre_stru.di_wr_buf) {
		wr_buf = di_pre_stru.di_wr_buf;
		if ((di_pre_stru.prog_proc_type == 2) &&
		    wr_buf->di_wr_linked_buf) {
			wr_buf->di_wr_linked_buf->pre_ref_count = 0;
			wr_buf->di_wr_linked_buf->post_ref_count = 0;
			queue_in(wr_buf->di_wr_linked_buf, QUEUE_RECYCLE);
			wr_buf->di_wr_linked_buf = NULL;
		}
		wr_buf->pre_ref_count = 0;
		wr_buf->post_ref_count = 0;
		queue_in(wr_buf, QUEUE_RECYCLE);
		di_pre_stru.di_wr_buf = NULL;
	}
	if (di_pre_stru.di_inp_buf) {
		if (di_pre_stru.di_mem_buf_dup_p == di_pre_stru.di_inp_buf)
			di_pre_stru.di_mem_buf_dup_p = NULL;

		queue_in(di_pre_stru.di_inp_buf, QUEUE_RECYCLE);
		di_pre_stru.di_inp_buf = NULL;
	}
}

static void top_bot_config(struct di_buf_s *di_buf)
{
	vframe_t *vframe = di_buf->vframe;

	if (((invert_top_bot & 0x1) != 0) && (!is_progressive(vframe))) {
		if (di_buf->invert_top_bot_flag == 0) {
			if (
				(vframe->type & VIDTYPE_TYPEMASK) ==
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

struct FlmModReg_t flmreg;
unsigned int pulldown_enable = 1;
module_param(pulldown_enable, uint, 0644);
MODULE_PARM_DESC(pulldown_enable, "/n pulldonw mode result./n");

static unsigned int pldn_mod;
module_param(pldn_mod, uint, 0644);
MODULE_PARM_DESC(pldn_mod, "/n pulldonw mode result./n");

static unsigned int pldn_cmb0 = 1;
module_param(pldn_cmb0, uint, 0644);
MODULE_PARM_DESC(pldn_cmb0, "/n pulldonw combing result./n");

static unsigned int pldn_cmb1;
module_param(pldn_cmb1, uint, 0644);
MODULE_PARM_DESC(pldn_cmb1, "/n pulldonw combing result./n");

static unsigned int pldn_calc_en = 1;
module_param(pldn_calc_en, uint, 0644);
MODULE_PARM_DESC(pldn_calc_en, "/n pulldonw calculation enable./n");

#define MAX_NUM_DI_REG 32
#define GXTVBB_REG_START 12
static unsigned int combing_setting_registers[MAX_NUM_DI_REG] = {
	DI_MTN_1_CTRL1,
	DI_MTN_1_CTRL2,
	DI_MTN_1_CTRL3,
	DI_MTN_1_CTRL4,
	DI_MTN_1_CTRL5,
	DI_MTN_1_CTRL6,
	DI_MTN_1_CTRL7,
	DI_MTN_1_CTRL8,
	DI_MTN_1_CTRL9,
	DI_MTN_1_CTRL10,
	DI_MTN_1_CTRL11,
	DI_MTN_1_CTRL12,
	/* below reg are gxtvbb only, offset defined in GXTVBB_REG_START */
	MCDI_REL_DET_PD22_CHK,
	MCDI_PD22_CHK_THD,
	MCDI_PD22_CHK_THD_RT,
	NR2_MATNR_DEGHOST,
	0
};

static unsigned int combing_setting_masks[MAX_NUM_DI_REG] = {
	0x0fffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0x0003ff1f,
	0x01ff3fff,
	0x07ffff1f,
	0x000001ff,
	0
};

static unsigned int combing_pure_still_setting[MAX_NUM_DI_REG] = {
	0x00141410,
	0x1A1A3A62,
	0x15200A0A,
	0x01800880,
	0x74200D0D,
	0x0D5A1520,
	0x0A800480,
	0x1A1A2662,
	0x0D200302,
	0x02020202,
	0x06090708,
	0x40020A04,
	0x0001FF0C,
	0x00400204,
	0x00016404,
	0x00000199
};

static unsigned int combing_bias_static_setting[MAX_NUM_DI_REG] = {
	0x00141410,
	0x1A1A3A62,
	0x15200A0A,
	0x01800880,
	0x74200D0D,
	0x0D5A1520,
	0x0A800480,
	0x1A1A2662,
	0x0D200302,
	0x02020202,
	0x06090708,
	0x40020A04,
	0x0001FF0C,
	0x00400204,
	0x00016404,
	0x00000166
};


static unsigned int combing_normal_setting[MAX_NUM_DI_REG] = {
	0x00202015,
	0x1A1A3A62,
	0x15200a0a,
	0x01000880,
	0x74200D0D,
	0x0D5A1520,
	0x0A0A0201,
	0x1A1A2662,
	0x0D200302,
	0x02020606,
	0x05080304,
	0x40020a04,
	0x0001FF0C,
	0x00400204,
	0x00016404,
	0x00000153
};

static unsigned int combing_bias_motion_setting[MAX_NUM_DI_REG] = {
	0x00202015,
	0x1A1A3A62,
	0x15200101,
	0x01200440,
	0x74200D0D,
	0x0D5A1520,
	0x0A0A0201,
	0x1A1A2662,
	0x0D200302,
	0x02020606,
	0x05080304,
	0x40020a04,
	0x0001ff0c, /* 0x0001FF12 */
	0x00400204, /* 0x00200204 */
	0x00016404, /* 0x00012002 */
	0x00000142
};

static unsigned int combing_very_motion_setting[MAX_NUM_DI_REG] = {
	0x00202015,
	0x1A1A3A62,
	0x15200101,
	0x01200440,
	0x74200D0D,
	0x0D5A1520,
	0x0A0A0201,
	0x1A1A2662,
	0x0D200302,
	0x02020606,
	0x05080304,
	0x40020a04,  /* 0x60000404,*/
	0x0001ff0c, /* 0x0001FF12 */
	0x00400204, /* 0x00200204 */
	0x00016404, /* 0x00012002 */
	0x00000131
};
/*special for resolution test file*/
static unsigned int combing_resolution_setting[MAX_NUM_DI_REG] = {
	0x00202015,
	0x141a3a62,
	0x15200a0a,
	0x01800880,
	0x74200d0d,
	0x0d5a1520,
	0x0a800480,
	0x1a1a2662,
	0x0d200302,
	0x01010101,
	0x06090708,
	0x40020a04,
	0x0001ff0c,
	0x00400204,
	0x00016404,
	0x00000131
};

static unsigned int (*combing_setting_values[6])[MAX_NUM_DI_REG] = {
	&combing_pure_still_setting,
	&combing_bias_static_setting,
	&combing_normal_setting,
	&combing_bias_motion_setting,
	&combing_very_motion_setting,
	&combing_resolution_setting
};

/* decide the levels based on glb_mot[0:4]
 * or with patches like Hist, smith trig logic
 * level:    0->1  1->2   2->3   3->4
 * from low motion to high motion level
 * take 720x480 as example */
static unsigned int combing_glb_mot_thr_LH[4] = {1440, 2880, 4760, 9520};

/* >> 4 */
static unsigned int combing_glbmot_radprat[4] = {32, 48, 64, 80};
static unsigned int num_glbmot_radprat = 4;
module_param_array(combing_glbmot_radprat, uint, &num_glbmot_radprat, 0664);

/* level:    0<-1  1<-2   2<-3   3<-4
 * from high motion to low motion level */
static unsigned int combing_glb_mot_thr_HL[4] = {720, 1440, 2880, 5760};

static bool combing_fix_en = true;
module_param(combing_fix_en, bool, 0664);
static unsigned int num_glb_mot_thr_LH = 4;
module_param_array(combing_glb_mot_thr_LH, uint, &num_glb_mot_thr_LH, 0664);
static unsigned int num_glb_mot_thr_HL = 4;
module_param_array(combing_glb_mot_thr_HL, uint, &num_glb_mot_thr_HL, 0664);
static unsigned int num_combing_setting_registers = MAX_NUM_DI_REG;
module_param_array(combing_setting_registers, uint,
	&num_combing_setting_registers, 0664);
static unsigned int num_combing_setting_masks = MAX_NUM_DI_REG;
module_param_array(combing_setting_masks, uint,
	&num_combing_setting_masks, 0664);
static unsigned int num_combing_pure_still_setting = MAX_NUM_DI_REG;
module_param_array(combing_pure_still_setting, uint,
	&num_combing_pure_still_setting, 0664);
static unsigned int num_combing_bias_static_setting = MAX_NUM_DI_REG;
module_param_array(combing_bias_static_setting, uint,
	&num_combing_bias_static_setting, 0664);
static unsigned int num_combing_normal_setting = MAX_NUM_DI_REG;
module_param_array(combing_normal_setting, uint, &num_combing_normal_setting,
	0664);
static unsigned int num_combing_bias_motion_setting = MAX_NUM_DI_REG;
module_param_array(combing_bias_motion_setting, uint,
	&num_combing_bias_motion_setting, 0664);
static unsigned int num_combing_very_motion_setting = MAX_NUM_DI_REG;
module_param_array(combing_very_motion_setting, uint,
	&num_combing_very_motion_setting, 0664);
/* 0: pure still; 1: bias static; 2: normal; 3: bias motion, 4: very motion */
static int cur_lev = 2;
module_param_named(combing_cur_lev, cur_lev, int, 0444);
static int force_lev = 0xff;
module_param_named(combing_force_lev, force_lev, int, 0664);
static int dejaggy_flag = -1;
module_param_named(combing_dejaggy_flag, dejaggy_flag, int, 0664);
static int dejaggy_enable = 1;
module_param_named(combing_dejaggy_enable, dejaggy_enable, int, 0664);
static uint num_dejaggy_setting = 5;
/* 0:off 1:1-14-1 2:1-6-1 3:3-10-3 4:100% */
/* current setting dejaggy always on when interlace source */
static int combing_dejaggy_setting[5] = {1, 1, 1, 2, 3};
module_param_array(combing_dejaggy_setting, uint,
	&num_dejaggy_setting, 0664);
#ifdef CONFIG_AM_ATVDEMOD
static int atv_snr_val = 30;
module_param_named(atv_snr_val, atv_snr_val, int, 0664);
static int atv_snr_cnt;
module_param_named(atv_snr_cnt, atv_snr_cnt, int, 0664);
static int atv_snr_cnt_limit = 30;
module_param_named(atv_snr_cnt_limit, atv_snr_cnt_limit, int, 0664);
#endif
static int last_lev = -1;
static int glb_mot[5] = {0, 0, 0, 0, 0};
static int still_field_count;
static int dejaggy_4p = true;
module_param_named(dejaggy_4p, dejaggy_4p, int, 0664);
UINT32 field_count = 0;

static int cmb_adpset_cnt;
module_param(cmb_adpset_cnt, int, 0664);
static void combing_threshold_config(unsigned int  width)
{
	int i = 0;
	/* init combing threshold */
	/*
	for (i = 0; i < 4; i++) {
		combing_glb_mot_thr_LH[i] = (width<<1)*(i+1);
		combing_glb_mot_thr_HL[i] = width<<i;
	}
	combing_glb_mot_thr_LH[3] = width*13;
	*/

	for (i = 0; i < 4; i++) {
		combing_glb_mot_thr_LH[i] =
			((width * combing_glbmot_radprat[i] + 8) >> 4);
		combing_glb_mot_thr_HL[i] =
			combing_glb_mot_thr_LH[i] - width;
	}
}

unsigned int adp_set_level(unsigned int diff, unsigned int field_diff_num)
{
	unsigned int rst = 0;
	char tlog[] = "LHM";
	if (diff <= combing_glb_mot_thr_LH[0])
		rst = 0;
	else if (diff >= combing_glb_mot_thr_LH[3])
		rst = 1;
	else
		rst = 2;

	if (cmb_adpset_cnt > 0) {
		pr_info("\field-num=%04d frame-num=%04d lvl=%c\n",
			field_diff_num, diff, tlog[rst]);
		cmb_adpset_cnt--;
	}

	return rst;
}

unsigned int adp_set_mtn_ctrl3(unsigned int diff, unsigned int dlvel)
{
	int istp = 0;
	int idats = 0;
	int idatm = 0;
	int idatr = 0;
	unsigned int rst = 0;
	if (dlvel == 0)
		rst = combing_pure_still_setting[2];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[2];
	else {
		rst = 0x1520;
		istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
			(combing_glb_mot_thr_LH[3] -
			combing_glb_mot_thr_LH[0] + 1);

		idats = (combing_pure_still_setting[2] >> 8) & 0xff;
		idatm = (combing_very_motion_setting[2] >> 8) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_pure_still_setting[2]) & 0xff;
		idatm = (combing_very_motion_setting[2]) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);
	}
/*
	if (cmb_adpset_cnt > 0)
		pr_info("mtn_ctrl3=%8x\n", rst); */

	return rst;
}

int di_debug_new_en = 1;
module_param(di_debug_new_en, int, 0644);
MODULE_PARM_DESC(di_debug_new_en, "di_debug_new_en");

int cmb_num_rat_ctl4 = 64; /* 0~255 */
module_param(cmb_num_rat_ctl4, int, 0644);
MODULE_PARM_DESC(cmb_num_rat_ctl4, "cmb_num_rat_ctl4");

int cmb_rat_ctl4_minthd = 64;
module_param(cmb_rat_ctl4_minthd, int, 0644);
MODULE_PARM_DESC(cmb_rat_ctl4_minthd, "cmb_rat_ctl4_minthd");

int tTCNm = 0; /* combing rows */
unsigned int adp_set_mtn_ctrl4(unsigned int diff, unsigned int dlvel)
{
	int hHeight = di_pre_stru.di_nrwr_mif.end_y;
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;
	if (dlvel == 0)
		rst = combing_pure_still_setting[3];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[3];
	else {
			rst = 1;
			istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
				(combing_glb_mot_thr_LH[3] -
				combing_glb_mot_thr_LH[0] + 1);

			idats = (combing_pure_still_setting[3] >> 16) & 0xff;
			idatm = (combing_very_motion_setting[3] >> 16) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			if (di_debug_new_en)
				idatr = idatr >> 1;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[3] >> 8) & 0xff;
			idatm = (combing_very_motion_setting[3] >> 8) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[3]) & 0xff;
			idatm = (combing_very_motion_setting[3]) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst << 8) | (idatr & 0xff);
	}

	if (di_debug_new_en == 1) {
		istp = ((cmb_num_rat_ctl4 * hHeight + 128) >> 8);
		if (cmb_adpset_cnt > 0)
			pr_info("mtn_ctrl4=%8x %03d (%03d)\n",
				rst, istp, tTCNm);
	if (tTCNm > istp) {
		istp = 64 * (hHeight - tTCNm) / (hHeight - istp + 1);
		if (istp < 4)
			istp = 4;

		idatm = 1;
		idats = (rst >> 16) & 0xff;
		idatr = ((idats * istp + 32) >> 6);
				idatr = idatr >> 1; /*color*/
		if (idatr < (cmb_rat_ctl4_minthd >> 1))
			idatr = (cmb_rat_ctl4_minthd >> 1);
		idatm = (idatm<<8) | (idatr & 0xff);

		idats = (rst >> 8) & 0xff;
		idatr = ((idats * istp + 32) >> 6);
		if (idatr < 4)
			idatr = 4;
		idatm = (idatm<<8) | (idatr & 0xff);

		idats = rst & 0xff;
		idatr = ((idats * istp + 32) >> 6);
		if (idatr < cmb_rat_ctl4_minthd)
			idatr = cmb_rat_ctl4_minthd;
		idatm = (idatm<<8) | (idatr & 0xff);

		rst = idatm;

		if (cmb_adpset_cnt > 0)
			pr_info("%03d (%03d)=%8x\n",
				tTCNm, hHeight, rst);
	}
	}
	return rst;
}

unsigned int adp_set_mtn_ctrl7(unsigned int diff, unsigned int dlvel)
{
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;
	if (dlvel == 0)
		rst = combing_pure_still_setting[6];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[6];
	else {
			rst = 10;
			istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
				(combing_glb_mot_thr_LH[3] -
				combing_glb_mot_thr_LH[0] + 1);

			idats = (combing_pure_still_setting[6] >> 16) & 0xff;
			idatm = (combing_very_motion_setting[6] >> 16) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[6] >> 8) & 0xff;
			idatm = (combing_very_motion_setting[6] >> 8) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[6]) & 0xff;
			idatm = (combing_very_motion_setting[6]) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);
	}
	/*
	if (cmb_adpset_cnt > 0) {
		pr_info("mtn_ctrl7=%8x\n", rst);
	}*/
	return rst;
}
static unsigned int small_local_mtn = 70;
module_param(small_local_mtn, uint, 0644);
MODULE_PARM_DESC(small_local_mtn, "small_local_mtn");

unsigned int adp_set_mtn_ctrl10(unsigned int diff, unsigned int dlvel)
{
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;

	if (frame_diff_avg < small_local_mtn)
		rst = combing_very_motion_setting[9];
	else if (dlvel == 0)
		rst = combing_pure_still_setting[9];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[9];
	else {
			istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
				(combing_glb_mot_thr_LH[3] -
				combing_glb_mot_thr_LH[0] + 1);

			idats = (combing_very_motion_setting[9] >> 24) & 0xff;
			idatm = (combing_pure_still_setting[9] >> 24) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_very_motion_setting[9] >> 16) & 0xff;
			idatm = (combing_pure_still_setting[9] >> 16) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_very_motion_setting[9] >> 8) & 0xff;
			idatm = (combing_pure_still_setting[9] >> 8) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_very_motion_setting[9]) & 0xff;
			idatm = (combing_pure_still_setting[9]) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);
	}

	if (cmb_adpset_cnt > 0) {
		pr_info("mtn_ctr10=0x%08x (frame_dif_avg=%03d)\n",
			rst, frame_diff_avg);
	}
	return rst;
}

unsigned int adp_set_mtn_ctrl11(unsigned int diff, unsigned int dlvel)
{
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;
	if (dlvel == 0)
		rst = combing_pure_still_setting[10];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[10];
	else {
			istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
				(combing_glb_mot_thr_LH[3] -
				combing_glb_mot_thr_LH[0] + 1);

			idats = (combing_pure_still_setting[10] >> 24) & 0xff;
			idatm = (combing_very_motion_setting[10] >> 24) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[10] >> 16) & 0xff;
			idatm = (combing_very_motion_setting[10] >> 16) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[10] >> 8) & 0xff;
			idatm = (combing_very_motion_setting[10] >> 8) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[10]) & 0xff;
			idatm = (combing_very_motion_setting[10]) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);
	}
	/*
	if (cmb_adpset_cnt > 0) {
		pr_info("mtn_ctr11=%8x\n", rst);
	}*/
	return rst;
}

void set_combing_regs(int lvl)
{
	int i;
	unsigned int ndat = 0;
	for (i = 0; i < MAX_NUM_DI_REG; i++) {
		if ((combing_setting_registers[i] == 0)
		|| (combing_setting_masks[i] == 0))
			break;
		if (combing_setting_registers[i] == DI_MTN_1_CTRL1)
			di_mtn_1_ctrl1 =
				(di_mtn_1_ctrl1 &
				~combing_setting_masks[i]) |
				((*combing_setting_values[lvl])[0] &
				combing_setting_masks[i]);
		if (di_force_bit_mode != 10 &&
			combing_setting_registers[i] == NR2_MATNR_DEGHOST)
			break;
		else if (i < GXTVBB_REG_START) {
			/* TODO: need change to check if
			register only in GCTVBB */
			ndat = (*combing_setting_values[lvl])[i];
			DI_Wr(combing_setting_registers[i], ndat);
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
			DI_Wr(combing_setting_registers[i],
				((*combing_setting_values[lvl])[i] &
				combing_setting_masks[i]) |
				(Rd(
					combing_setting_registers[i])
					& ~combing_setting_masks[i]));
	}
}

static int like_pulldown22_flag;

int di_debug_readreg = 0;
module_param(di_debug_readreg, int, 0644);
MODULE_PARM_DESC(di_debug_readreg, "di_debug_readreg");

static unsigned int field_diff_rate;

static void adaptive_combing_fixing(
	pulldown_detect_info_t *field_pd_info,
	int frame_type, int wWidth)
{
	unsigned int glb_mot_avg2;
	unsigned int glb_mot_avg3;
	unsigned int glb_mot_avg5;

	unsigned int diff = 0;
	unsigned int wt_dat = 0;
	unsigned int dlvl = 0;
	static unsigned int pre_dat[5];
	bool prt_flg = (cmb_adpset_cnt > 0);
	unsigned int i = 0;

	static unsigned int pre_num;
	unsigned int crt_num = field_pd_info->field_diff_num;
	unsigned int drat = 0;
	if (pre_num > crt_num)
		diff = pre_num - crt_num;
	else
		diff = crt_num - pre_num;

	if (diff >= wWidth)
		field_diff_rate = 0;
	else {
		drat = (diff << 8) / (wWidth + 1);
		if (drat > 255)
			field_diff_rate = 0;
		else
			field_diff_rate = 256 - drat;
	}
	pre_num = crt_num;

	if (di_debug_readreg > 1) {
		for (i = 0; i < 12; i++) {
			wt_dat = Rd(combing_setting_registers[i]);
			pr_info("mtn_ctrl%02d = 0x%08x\n",
				i+1, wt_dat);
		}
		pr_info("\n");
		di_debug_readreg--;
	}

	if (!combing_fix_en)
		return;

	glb_mot[4] = glb_mot[3];
	glb_mot[3] = glb_mot[2];
	glb_mot[2] = glb_mot[1];
	glb_mot[1] = glb_mot[0];
	glb_mot[0] = field_pd_info->frame_diff_num;
	glb_mot_avg5 =
		(glb_mot[0] + glb_mot[1] + glb_mot[2] + glb_mot[3] +
		 glb_mot[4]) / 5;
	glb_mot_avg3 = (glb_mot[0] + glb_mot[1] + glb_mot[2]) / 3;
	glb_mot_avg2 = (glb_mot[0] + glb_mot[1]) / 2;

	if (glb_mot[0] > combing_glb_mot_thr_LH[0])
		still_field_count = 0;
	else
	if (still_field_count < 16)
		still_field_count++;
	if (glb_mot_avg3 > combing_glb_mot_thr_LH[min(cur_lev, 3)]) {
		if (cur_lev < 4)
			cur_lev++;
	} else {
		if (glb_mot_avg5 <
		    combing_glb_mot_thr_HL[max(cur_lev - 1, 0)]) {
			if (cur_lev <= 1 && still_field_count > 5)
				cur_lev = 0;
			else
				cur_lev = max(cur_lev - 1, 1);
		}
	}
	if ((force_lev >= 0) & (force_lev < 6))
		cur_lev = force_lev;
	if (cur_lev != last_lev) {
		set_combing_regs(cur_lev);
		if (pr_pd & 0x400)
			pr_dbg("\t%5d: from %d to %d: di_mtn_1_ctrl1 = %08x\n",
				field_count, last_lev, cur_lev, di_mtn_1_ctrl1);

		last_lev = cur_lev;
	}

	if ((force_lev > 5) && (di_debug_new_en == 1) &&
		(glb_mot[1] != glb_mot[0])) {
		dlvl = adp_set_level(glb_mot[0], field_pd_info->field_diff_num);
		diff = glb_mot[0];
		pre_dat[0] = Rd(DI_MTN_1_CTRL3);
		wt_dat = adp_set_mtn_ctrl3(diff, dlvl);
		if (pre_dat[0] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL3, wt_dat);
			pre_dat[0] = wt_dat;
			if (prt_flg)
				pr_info("set mtn03 0x%08x.\n", wt_dat);
		}

		pre_dat[1] = Rd(DI_MTN_1_CTRL4);
		wt_dat = adp_set_mtn_ctrl4(diff, dlvl);
		if (pre_dat[1] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL4, wt_dat);
			if (prt_flg)
				pr_info("set mtn04 %08x -> %08x.\n",
				pre_dat[1], wt_dat);
			pre_dat[1] = wt_dat;
		}

		pre_dat[2] = Rd(DI_MTN_1_CTRL7);
		wt_dat = adp_set_mtn_ctrl7(diff, dlvl);
		if (pre_dat[2] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL7, wt_dat);
			pre_dat[2] = wt_dat;
			if (prt_flg)
				pr_info("set mtn07 0x%08x.\n", wt_dat);
		}

		pre_dat[3] = Rd(DI_MTN_1_CTRL10);
		wt_dat = adp_set_mtn_ctrl10(diff, dlvl);
		if (pre_dat[3] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL10, wt_dat);
			pre_dat[3] = wt_dat;
			if (prt_flg)
				pr_info("set mtn10 0x%08x.\n", wt_dat);
		}

		pre_dat[4] = Rd(DI_MTN_1_CTRL11);
		wt_dat = adp_set_mtn_ctrl11(diff, dlvl);
		if (pre_dat[4] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL11, wt_dat);
			pre_dat[4] = wt_dat;
			if (prt_flg)
				pr_info("set mtn11 0x%08x.\n\n", wt_dat);
		}
	}

	if (is_meson_gxtvbb_cpu() && dejaggy_enable) {
		/* only enable dejaggy for interlace */
		if ((frame_type & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE &&
			!dejaggy_4p) {
			if (dejaggy_flag != -1) {
				dejaggy_flag = -1;
				DI_Wr_reg_bits(SRSHARP0_SHARP_DEJ1_MISC,
					0, 3, 1);
			}
		} else {
			if ((dejaggy_flag == -1)
			|| ((Rd(SRSHARP0_SHARP_SR2_CTRL) & (1 << 24)) == 0)) {
				/* enable dejaggy module */
				DI_Wr_reg_bits(SRSHARP0_SHARP_SR2_CTRL,
					1, 24, 1);
				/* first time set default */
				DI_Wr_reg_bits(SRSHARP0_SHARP_DEJ2_PRC,
					0xff, 24, 8);
				DI_Wr(SRSHARP0_SHARP_DEJ1_PRC,
					(0xff<<24)|(0xd1<<16)|(0xe<<8)|0x31);
				DI_Wr(
					SRSHARP0_SHARP_DEJ2_MISC, 0x30);
				DI_Wr(
					SRSHARP0_SHARP_DEJ1_MISC, 0x02f4);
				dejaggy_flag = 0;
			}
			if (dejaggy_enable) {
				/* dejaggy alpha according to motion level */
				dejaggy_flag =
					combing_dejaggy_setting[cur_lev];
				/* TODO: check like_pulldown22_flag and ATV
				noise_level */
				#ifdef CONFIG_AM_ATVDEMOD
				if ((aml_atvdemod_get_snr_ex() < atv_snr_val)
					&& (di_pre_stru.cur_source_type ==
					VFRAME_SOURCE_TYPE_TUNER)) {
					if (atv_snr_cnt++ > atv_snr_cnt_limit)
						dejaggy_flag += 3;
				} else if (atv_snr_cnt)
					atv_snr_cnt = 0;
				#endif
				if (like_pulldown22_flag && (cur_lev > 2))
					dejaggy_flag += 1;
				/* overwrite dejaggy alpha */
				if (dejaggy_enable >= 2)
					dejaggy_flag = dejaggy_enable;
				if (dejaggy_flag > 4)
					dejaggy_flag = 4;
				if (dejaggy_flag)
					DI_Wr_reg_bits(
						SRSHARP0_SHARP_DEJ1_MISC,
						(1<<3)|dejaggy_flag, 0, 4);
				else
					DI_Wr_reg_bits(
						SRSHARP0_SHARP_DEJ1_MISC,
						0, 3, 1);
			} else
				dejaggy_flag = 0;
		}
	} else if (is_meson_gxtvbb_cpu()) {
		dejaggy_flag = -1;
		DI_Wr_reg_bits(SRSHARP0_SHARP_DEJ1_MISC, 0, 3, 1);
	}
}

static unsigned int flm22_sure_num = 100;

/*
static unsigned int flmxx_sure_num = 50;
module_param(flmxx_sure_num, uint, 0644);
MODULE_PARM_DESC(flmxx_sure_num, "ture film-xx/n");
*/

static unsigned int flm22_sure_smnum = 70;
static unsigned int flm22_ratio = 100;
/* 79 for iptv test pd22 ts */
module_param_named(flm22_ratio, flm22_ratio, uint, 0644);
/* static unsigned int flmxx_sure_num[7]
 = {50, 50, 50, 50, 50, 50, 50}; */
static unsigned int flmxx_sure_num[7] = {20, 20, 20, 20, 20, 20, 20};
static unsigned int flmxx_snum_adr = 7;
module_param_array(flmxx_sure_num, uint, &flmxx_snum_adr, 0664);

static unsigned int flm22_glbpxlnum_rat = 4; /* 4/256 = 64 */

static unsigned int flm22_glbpxl_maxrow = 16; /* 16/256 = 16 */
module_param(flm22_glbpxl_maxrow, uint, 0644);
MODULE_PARM_DESC(flm22_glbpxl_maxrow, "flm22_glbpxl_maxrow/n");

static unsigned int flm22_glbpxl_minrow = 3; /* 4/256 = 64 */
module_param(flm22_glbpxl_minrow, uint, 0644);
MODULE_PARM_DESC(flm22_glbpxl_minrow, "flm22_glbpxl_minrow/n");

static unsigned int cmb_3point_rnum;
module_param(cmb_3point_rnum, uint, 0644);
MODULE_PARM_DESC(cmb_3point_rnum, "cmb_3point_rnum/n");

static unsigned int cmb_3point_rrat = 32;
module_param(cmb_3point_rrat, uint, 0644);
MODULE_PARM_DESC(cmb_3point_rrat, "cmb_3point_rrat/n");

static void pre_de_done_buf_config(void)
{
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;
	bool dynamic_flag = false;
	int hHeight = di_pre_stru.di_nrwr_mif.end_y;
	int wWidth  = di_pre_stru.di_nrwr_mif.end_x;

	bool flm32 = false;
	bool flm22 = false;
	bool flmxx = false;
	int tb_chk_ret = 0;

	unsigned int glb_frame_mot_num = 0;
	unsigned int glb_field_mot_num = 0;
	unsigned int mot_row = 0;
	unsigned int mot_max = 0;
	unsigned int flm22_surenum = flm22_sure_num;
	unsigned int ntmp = 0;
	if (di_pre_stru.di_wr_buf) {
		if (di_pre_stru.pre_throw_flag > 0) {
			di_pre_stru.di_wr_buf->throw_flag = 1;
			di_pre_stru.pre_throw_flag--;
		} else {
			di_pre_stru.di_wr_buf->throw_flag = 0;
		}
#ifdef DET3D
		if (di_pre_stru.di_wr_buf->vframe->trans_fmt == 0 &&
			di_pre_stru.det3d_trans_fmt != 0 && det3d_en) {
				di_pre_stru.di_wr_buf->vframe->trans_fmt =
				di_pre_stru.det3d_trans_fmt;
				set3d_view(di_pre_stru.det3d_trans_fmt,
					di_pre_stru.di_wr_buf->vframe);
		}
#endif
		if (!di_pre_rdma_enable)
			di_pre_stru.di_post_wr_buf = di_pre_stru.di_wr_buf;

		if (di_pre_stru.cur_source_type == VFRAME_SOURCE_TYPE_OTHERS &&
				tff_bff_enable) {
			tb_chk_ret = tff_bff_check((di_pre_stru.cur_height>>1),
					di_pre_stru.cur_width);
			di_pre_stru.di_post_wr_buf->privated &= (~0x3);
			di_pre_stru.di_post_wr_buf->privated |=	tb_chk_ret;
		}
		if (di_pre_stru.di_post_wr_buf) {
			dynamic_flag = read_pulldown_info(
				&(di_pre_stru.di_post_wr_buf->field_pd_info),
				&(di_pre_stru.di_post_wr_buf->win_pd_info[0]));
			di_pre_stru.static_frame_count =
				dynamic_flag ? 0 : (di_pre_stru.
						    static_frame_count + 1);
			if (di_pre_stru.static_frame_count >
			    static_pic_threshold) {
				di_pre_stru.static_frame_count =
					static_pic_threshold;
				di_pre_stru.di_post_wr_buf->pulldown_mode =
					PULL_DOWN_BLEND_0;
			} else {
				di_pre_stru.di_post_wr_buf->pulldown_mode =
					PULL_DOWN_NORMAL;
			}
			adaptive_combing_fixing(
				&(di_pre_stru.di_post_wr_buf->field_pd_info),
				di_pre_stru.cur_inp_type,
				wWidth + 1);
		}

		tTCNm = 0;
		if (!di_pre_stru.cur_prog_flag) {
			/* always read and print data */
			read_new_pulldown_info(&flmreg);
			/* read_new_pulldown_info(&flmreg); */
			if ((pldn_calc_en == 1) && pulldown_enable) {
				glb_frame_mot_num = di_pre_stru.di_post_wr_buf->
						field_pd_info.frame_diff_num;
				glb_field_mot_num = di_pre_stru.di_post_wr_buf->
						field_pd_info.field_diff_num;
				dectres.rF22Flag = FlmVOFSftTop(
					&(dectres.rCmb32Spcl),
					dectres.rPstCYWnd0,
					dectres.rPstCYWnd1,
					dectres.rPstCYWnd2,
					dectres.rPstCYWnd3,
					dectres.rPstCYWnd4,
					&(dectres.rFlmPstGCm),
					&(dectres.rFlmSltPre),
					&(dectres.rFlmPstMod),
					flmreg.rROFldDif01,
					flmreg.rROFrmDif02,
					flmreg.rROCmbInf,
					glb_frame_mot_num,
					glb_field_mot_num,
					&tTCNm,
					&pd_param,
					hHeight + 1,
					wWidth + 1,
					overturn);

				if (hHeight >= 289) /*full hd */
					tTCNm = tTCNm << 1;
				if (tTCNm > hHeight)
					tTCNm = hHeight;

				prt_flg = ((pr_pd >> 1) & 0x1);
				if (prt_flg) {
					sprintf(debug_str, "#Pst-Dbg:\n");
					sprintf(debug_str + strlen(debug_str),
					"Mod=%d, Pre=%d, GCmb=%d, Lvl2=%d\n",
					dectres.rFlmPstMod,
					dectres.rFlmSltPre,
					dectres.rFlmPstGCm,
					dectres.rF22Flag);

					sprintf(debug_str + strlen(debug_str),
					"N%03d: nd[%d~%d], [%d~%d], [%d~%d], [%d~%d]\n",
					tTCNm,
					dectres.rPstCYWnd0[0],
					dectres.rPstCYWnd0[1],
					dectres.rPstCYWnd1[0],
					dectres.rPstCYWnd1[1],
					dectres.rPstCYWnd2[0],
					dectres.rPstCYWnd2[1],
					dectres.rPstCYWnd3[0],
					dectres.rPstCYWnd3[1]);

					pr_info("%s", debug_str);
				}
			}

			if (pulldown_enable && di_pre_stru.di_post_wr_buf) {
				/* refresh, default setting */
				di_pre_stru.di_post_wr_buf->pulldown_mode =
					PULL_DOWN_NORMAL;
				di_pre_stru.di_post_wr_buf->reg0_s = 0;
				di_pre_stru.di_post_wr_buf->reg0_e = 0;
				di_pre_stru.di_post_wr_buf->reg0_bmode = 0;

				di_pre_stru.di_post_wr_buf->reg1_s = 0;
				di_pre_stru.di_post_wr_buf->reg1_e = 0;
				di_pre_stru.di_post_wr_buf->reg1_bmode = 0;

				di_pre_stru.di_post_wr_buf->reg2_s = 0;
				di_pre_stru.di_post_wr_buf->reg2_e = 0;
				di_pre_stru.di_post_wr_buf->reg2_bmode = 0;

				di_pre_stru.di_post_wr_buf->reg3_s = 0;
				di_pre_stru.di_post_wr_buf->reg3_e = 0;
				di_pre_stru.di_post_wr_buf->reg3_bmode = 0;
			}
			if (dectres.rFlmPstMod == 1)
				like_pulldown22_flag = dectres.rF22Flag;
			else
				like_pulldown22_flag = 0;

			if (di_debug_new_en) {
				if ((pr_pd >> 1) & 0x1)
					pr_info("fld_dif_rat=%d\n",
					field_diff_rate);

				if ((dectres.rF22Flag >=
					(cmb_3point_rnum + field_diff_rate)) &&
					(tTCNm >
					(hHeight * cmb_3point_rrat >> 8))) {
					if ((pr_pd >> 1) & 0x1)
						pr_info("coeff-3-point enabled\n");
				}
			}
			if (pulldown_enable == 1 && dectres.rFlmPstMod != 0
				&& di_pre_stru.di_post_wr_buf) {
				flm32 = (dectres.rFlmPstMod == 2 &&
					dectres.rFlmPstGCm == 0);

				ntmp = (glb_frame_mot_num + glb_field_mot_num) /
					(wWidth + 1);
				if (flm22_sure_num > ntmp + flm22_sure_smnum)
					flm22_surenum = flm22_sure_num - ntmp;
				else
					flm22_surenum = flm22_sure_smnum;

				if (cmb_adpset_cnt > 0)
					pr_info("flm22_surenum = %2d\n",
						flm22_surenum);
				if (di_debug_new_en &&
					(dectres.rFlmPstMod == 1)) {
					mot_row = glb_frame_mot_num *
					flm22_glbpxlnum_rat / (wWidth + 1);
					mot_max = (flm22_glbpxl_maxrow *
						hHeight + 128) >> 8;
					if ((pr_pd >> 1) & 0x1)
						pr_info("dejaggies level=%3d - (%02d - %02d)\n",
							dectres.rF22Flag,
							mot_max, mot_row);

				if (mot_row < mot_max) {
					if (dectres.rF22Flag >
						(mot_max - mot_row))
							dectres.rF22Flag -=
							(mot_max - mot_row);
					else
							dectres.rF22Flag = 0;

				if (mot_row <=
							flm22_glbpxl_minrow)
							dectres.rFlmPstMod = 0;
					}
				}

				flm22 = (dectres.rFlmPstMod == 1  &&
					dectres.rF22Flag >= flm22_surenum);
				if (dectres.rFlmPstMod >= 4)
					flmxx = (dectres.rF22Flag >=
					flmxx_sure_num[dectres.rFlmPstMod - 4]);
				else
					flmxx = 0;

				/* 2-2 force */
				if ((pldn_mod == 0) &&
					(flm32 || flm22 || flmxx)) {
					if (dectres.rFlmSltPre == 1)
						di_pre_stru.di_post_wr_buf
						->pulldown_mode =
							PULL_DOWN_BLEND_0;
					else {
						di_pre_stru.di_post_wr_buf
						->pulldown_mode =
							PULL_DOWN_BLEND_2;
						}
				} else if (pldn_mod == 1) {
					if (dectres.rFlmSltPre == 1)
						di_pre_stru.di_post_wr_buf
						->pulldown_mode =
							PULL_DOWN_BLEND_0;
					else
						di_pre_stru.di_post_wr_buf
						->pulldown_mode =
							PULL_DOWN_BLEND_2;
				} else {
					di_pre_stru.di_post_wr_buf->
					pulldown_mode =
						PULL_DOWN_NORMAL;
				}

				if (flm32 && (pldn_cmb0 == 1)) {
					di_pre_stru.di_post_wr_buf->reg0_s =
						dectres.rPstCYWnd0[0];
					di_pre_stru.di_post_wr_buf->reg0_e =
						dectres.rPstCYWnd0[1];

					di_pre_stru.di_post_wr_buf->reg1_s =
						dectres.rPstCYWnd1[0];
					di_pre_stru.di_post_wr_buf->reg1_e =
						dectres.rPstCYWnd1[1];

					di_pre_stru.di_post_wr_buf->reg2_s =
						dectres.rPstCYWnd2[0];
					di_pre_stru.di_post_wr_buf->reg2_e =
						dectres.rPstCYWnd2[1];

					di_pre_stru.di_post_wr_buf->reg3_s =
						dectres.rPstCYWnd3[0];
					di_pre_stru.di_post_wr_buf->reg3_e =
						dectres.rPstCYWnd3[1];

					di_pre_stru.di_post_wr_buf->reg0_bmode =
						dectres.rPstCYWnd0[2];
					di_pre_stru.di_post_wr_buf->reg1_bmode =
						dectres.rPstCYWnd1[2];
					di_pre_stru.di_post_wr_buf->reg2_bmode =
						dectres.rPstCYWnd2[2];
					di_pre_stru.di_post_wr_buf->reg3_bmode =
						dectres.rPstCYWnd3[2];
				} else if (dectres.rF22Flag > 1 &&
					dectres.rFlmPstMod == 1 &&
					pldn_cmb0 == 1) {
					/* SRSHARP0_SHARP_SR2_CTRL */
					/* SRSHARP0_SHARP_DEJ2_MISC */
					/* SRSHARP0_SHARP_DEJ1_MISC */

					if ((pr_pd >> 1) & 0x1)
						pr_info("dejaggies level= %3d\n",
						dectres.rF22Flag);
				} else if (dectres.rFlmPstGCm == 0
					&& pldn_cmb0 > 1
					&& pldn_cmb0 <= 5) {
					di_pre_stru.di_post_wr_buf->reg0_s =
						dectres.rPstCYWnd0[0];
					di_pre_stru.di_post_wr_buf->reg0_e =
						dectres.rPstCYWnd0[1];

					di_pre_stru.di_post_wr_buf->reg1_s =
						dectres.rPstCYWnd1[0];
					di_pre_stru.di_post_wr_buf->reg1_e =
						dectres.rPstCYWnd1[1];

					di_pre_stru.di_post_wr_buf->reg2_s =
						dectres.rPstCYWnd2[0];
					di_pre_stru.di_post_wr_buf->reg2_e =
						dectres.rPstCYWnd2[1];

					di_pre_stru.di_post_wr_buf->reg3_s =
						dectres.rPstCYWnd3[0];
					di_pre_stru.di_post_wr_buf->reg3_e =
						dectres.rPstCYWnd3[1];

					/* 1-->only film-mode
					 * 2-->windows-->mtn
					 * 3-->windows-->detected
					 * 4-->windows-->di */
					if (pldn_cmb0 == 2) { /* 1-->normal */
						di_pre_stru.di_post_wr_buf->
							reg0_bmode =
							dectres.rPstCYWnd0[2];
						di_pre_stru.di_post_wr_buf->
							reg1_bmode =
							dectres.rPstCYWnd1[2];
						di_pre_stru.di_post_wr_buf->
							reg2_bmode =
							dectres.rPstCYWnd2[2];
						di_pre_stru.di_post_wr_buf->
							reg3_bmode =
							dectres.rPstCYWnd3[2];
					} else if (pldn_cmb0 == 3) {
						di_pre_stru.di_post_wr_buf->
							reg0_bmode = 3;
						di_pre_stru.di_post_wr_buf->
							reg1_bmode = 3;
						di_pre_stru.di_post_wr_buf->
							reg2_bmode = 3;
						di_pre_stru.di_post_wr_buf->
							reg3_bmode = 3;
					} else if (pldn_cmb0 == 4) {
						di_pre_stru.di_post_wr_buf->
							reg0_bmode = 2;
						di_pre_stru.di_post_wr_buf->
							reg1_bmode = 2;
						di_pre_stru.di_post_wr_buf->
							reg2_bmode = 2;
						di_pre_stru.di_post_wr_buf->
							reg3_bmode = 2;
					} else if (pldn_cmb0 == 5) {
						di_pre_stru.
						di_post_wr_buf->reg3_s = 0;
						di_pre_stru.
						di_post_wr_buf->reg3_e = 60;
						di_pre_stru.
						di_post_wr_buf->reg3_bmode = 0;
					}
				}
				/* else pldn_cmb0==0 (Nothing) */
				if ((1 == dectres.rFlmPstGCm) && (pldn_cmb1 > 0)
				    && (pldn_cmb1 <= 5)) {
					di_pre_stru.di_post_wr_buf->reg0_s =
						dectres.rPstCYWnd0[0];
					di_pre_stru.di_post_wr_buf->reg0_e =
						dectres.rPstCYWnd0[1];

					di_pre_stru.di_post_wr_buf->reg1_s =
						dectres.rPstCYWnd1[0];
					di_pre_stru.di_post_wr_buf->reg1_e =
						dectres.rPstCYWnd1[1];

					di_pre_stru.di_post_wr_buf->reg2_s =
						dectres.rPstCYWnd2[0];
					di_pre_stru.di_post_wr_buf->reg2_e =
						dectres.rPstCYWnd2[1];

					di_pre_stru.di_post_wr_buf->reg3_s =
						dectres.rPstCYWnd3[0];
					di_pre_stru.di_post_wr_buf->reg3_e =
						dectres.rPstCYWnd3[1];

					if (pldn_cmb1 == 1) { /* 1-->normal */
					di_pre_stru.di_post_wr_buf->reg0_bmode =
							dectres.rPstCYWnd0[2];
					di_pre_stru.di_post_wr_buf->reg1_bmode =
							dectres.rPstCYWnd1[2];
					di_pre_stru.di_post_wr_buf->reg2_bmode =
							dectres.rPstCYWnd2[2];
					di_pre_stru.di_post_wr_buf->reg3_bmode =
							dectres.rPstCYWnd3[2];
					} else if (pldn_cmb1 == 2) {
						di_pre_stru.
						di_post_wr_buf->reg0_bmode = 3;
						di_pre_stru.
						di_post_wr_buf->reg1_bmode = 3;
						di_pre_stru.
						di_post_wr_buf->reg2_bmode = 3;
						di_pre_stru.
						di_post_wr_buf->reg3_bmode = 3;
					} else if (pldn_cmb1 == 3) {
						di_pre_stru.
						di_post_wr_buf->reg0_bmode = 2;
						di_pre_stru.
						di_post_wr_buf->reg1_bmode = 2;
						di_pre_stru.
						di_post_wr_buf->reg2_bmode = 2;
						di_pre_stru.
						di_post_wr_buf->reg3_bmode = 2;
					} else if (pldn_cmb1 == 4) {
						di_pre_stru.
						di_post_wr_buf->reg2_s = 202;
						di_pre_stru.
						di_post_wr_buf->reg2_e = 222;
						di_pre_stru.
						di_post_wr_buf->reg2_bmode = 0;
					}
				}
			} else if ((pldn_cmb0 == 6) && (pldn_cmb1 == 6)) {
				di_pre_stru.di_post_wr_buf->reg1_s = 60;
				di_pre_stru.di_post_wr_buf->reg1_e = 180;
				di_pre_stru.di_post_wr_buf->reg1_bmode = 0;
			}
		}
		field_count++;
		if (field_count == 0x7fffffff)
			field_count = 3;

		if (di_pre_stru.cur_prog_flag) {
			if (di_pre_stru.prog_proc_type == 0) {
				/* di_mem_buf_dup->vfrme
				 * is either local vframe,
				 * or bot field of vframe from in_list */
				di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
				di_pre_stru.di_mem_buf_dup_p
					= di_pre_stru.di_chan2_buf_dup_p;
				di_pre_stru.di_chan2_buf_dup_p
					= di_pre_stru.di_wr_buf;
#ifdef DI_BUFFER_DEBUG
				di_print("%s:set di_mem to di_chan2,",
					__func__);
				di_print("%s:set di_chan2 to di_wr_buf\n",
					__func__);
#endif
			} else {
				di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
				/*recycle the progress throw buffer*/
				if (di_pre_stru.di_wr_buf->throw_flag) {
					di_pre_stru.di_wr_buf->
						pre_ref_count = 0;
					di_pre_stru.di_mem_buf_dup_p = NULL;
#ifdef DI_BUFFER_DEBUG
				di_print(
				"%s set throw %s[%d] pre_ref_count to 0.\n",
				__func__,
				vframe_type_name[di_pre_stru.di_wr_buf->type],
				di_pre_stru.di_wr_buf->index);
#endif
				} else {
					di_pre_stru.di_mem_buf_dup_p
						= di_pre_stru.di_wr_buf;
				}
#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s: set di_mem_buf_dup_p to di_wr_buf\n",
					__func__);
#endif
			}

			di_pre_stru.di_wr_buf->seq
				= di_pre_stru.pre_ready_seq++;
			di_pre_stru.di_wr_buf->post_ref_count = 0;
			di_pre_stru.di_wr_buf->left_right
				= di_pre_stru.left_right;
			if (di_pre_stru.source_change_flag) {
				di_pre_stru.di_wr_buf->new_format_flag = 1;
				di_pre_stru.source_change_flag = 0;
			} else {
				di_pre_stru.di_wr_buf->new_format_flag = 0;
			}
			if (bypass_state == 1) {
				di_pre_stru.di_wr_buf->new_format_flag = 1;
				bypass_state = 0;
#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s:bypass_state->0, is_bypass() %d\n",
					__func__, is_bypass(NULL));
				di_print(
					"trick_mode %d bypass_all %d\n",
					trick_mode, bypass_all);
#endif
			}
			if (di_pre_stru.di_post_wr_buf)
				queue_in(di_pre_stru.di_post_wr_buf,
					QUEUE_PRE_READY);
#ifdef DI_BUFFER_DEBUG
			di_print(
				"%s: %s[%d] => pre_ready_list\n", __func__,
				vframe_type_name[di_pre_stru.di_wr_buf->type],
				di_pre_stru.di_wr_buf->index);
#endif
			if (di_pre_stru.di_wr_buf) {
				if (di_pre_rdma_enable)
					di_pre_stru.di_post_wr_buf =
				di_pre_stru.di_wr_buf;
				else
					di_pre_stru.di_post_wr_buf = NULL;
				di_pre_stru.di_wr_buf = NULL;
			}
		} else {
			di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
			di_pre_stru.di_mem_buf_dup_p = NULL;
			if (di_pre_stru.di_chan2_buf_dup_p) {
				di_pre_stru.di_mem_buf_dup_p =
					di_pre_stru.di_chan2_buf_dup_p;
#ifdef DI_BUFFER_DEBUG
				di_print(
				"%s: di_mem_buf_dup_p = di_chan2_buf_dup_p\n",
				__func__);
#endif
			}
			di_pre_stru.di_chan2_buf_dup_p = di_pre_stru.di_wr_buf;

			if (di_pre_stru.di_wr_buf->post_proc_flag == 2) {
				/* add dummy buf, will not be displayed */
				if (!queue_empty(QUEUE_LOCAL_FREE)) {
					struct di_buf_s *di_buf_tmp;
					di_buf_tmp =
					get_di_buf_head(QUEUE_LOCAL_FREE);
					if (di_buf_tmp) {
						queue_out(di_buf_tmp);
						di_buf_tmp->pre_ref_count = 0;
						di_buf_tmp->post_ref_count = 0;
						di_buf_tmp->post_proc_flag = 3;
						di_buf_tmp->new_format_flag = 0;
						queue_in(
							di_buf_tmp,
							QUEUE_PRE_READY);
					}
#ifdef DI_BUFFER_DEBUG
					di_print(
					"%s: dummy %s[%d] => pre_ready_list\n",
					__func__,
					vframe_type_name[di_buf_tmp->type],
					di_buf_tmp->index);
#endif
				}
			}
			di_pre_stru.di_wr_buf->seq =
				di_pre_stru.pre_ready_seq++;
			di_pre_stru.di_wr_buf->left_right =
				di_pre_stru.left_right;
			di_pre_stru.di_wr_buf->post_ref_count = 0;
			if (di_pre_stru.source_change_flag) {
				di_pre_stru.di_wr_buf->new_format_flag = 1;
				di_pre_stru.source_change_flag = 0;
			} else {
				di_pre_stru.di_wr_buf->new_format_flag = 0;
			}
			if (bypass_state == 1) {
				di_pre_stru.di_wr_buf->new_format_flag = 1;
				bypass_state = 0;

#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s:bypass_state->0, is_bypass() %d\n",
					__func__, is_bypass(NULL));
				di_print(
					"trick_mode %d bypass_all %d\n",
					trick_mode, bypass_all);
#endif
			}

			if (di_pre_stru.di_post_wr_buf)
				queue_in(di_pre_stru.di_post_wr_buf,
					QUEUE_PRE_READY);

			di_print("%s: %s[%d] => pre_ready_list\n", __func__,
				vframe_type_name[di_pre_stru.di_wr_buf->type],
				di_pre_stru.di_wr_buf->index);

			if (di_pre_stru.di_wr_buf) {
				if (di_pre_rdma_enable)
					di_pre_stru.di_post_wr_buf =
				di_pre_stru.di_wr_buf;
				else
					di_pre_stru.di_post_wr_buf = NULL;
				di_pre_stru.di_wr_buf = NULL;
			}
		}
	}

	if (di_pre_stru.di_post_inp_buf && di_pre_rdma_enable) {
#ifdef DI_BUFFER_DEBUG
		di_print("%s: %s[%d] => recycle_list\n", __func__,
			vframe_type_name[di_pre_stru.di_post_inp_buf->type],
			di_pre_stru.di_post_inp_buf->index);
#endif
		di_lock_irqfiq_save(irq_flag2, fiq_flag);
		queue_in(di_pre_stru.di_post_inp_buf, QUEUE_RECYCLE);
		di_pre_stru.di_post_inp_buf = NULL;
		di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
	}
	if (di_pre_stru.di_inp_buf) {
		if (!di_pre_rdma_enable) {
#ifdef DI_BUFFER_DEBUG
			di_print("%s: %s[%d] => recycle_list\n", __func__,
			vframe_type_name[di_pre_stru.di_inp_buf->type],
			di_pre_stru.di_inp_buf->index);
#endif
			di_lock_irqfiq_save(irq_flag2, fiq_flag);
			queue_in(di_pre_stru.di_inp_buf, QUEUE_RECYCLE);
			di_pre_stru.di_inp_buf = NULL;
			di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
		} else {
			di_pre_stru.di_post_inp_buf = di_pre_stru.di_inp_buf;
			di_pre_stru.di_inp_buf = NULL;
		}
	}
}

#if defined(NEW_DI_TV)
/* add for di Reg re-init */
static enum
vframe_source_type_e vframe_source_type = VFRAME_SOURCE_TYPE_OTHERS;
static void di_set_para_by_tvinfo(vframe_t *vframe)
{
	if (vframe->source_type == vframe_source_type)
		return;
	pr_dbg("%s: tvinfo change, reset di Reg\n", __func__);
	vframe_source_type = vframe->source_type;
	/* add for smooth skin */
	if (vframe_source_type != VFRAME_SOURCE_TYPE_OTHERS)
		nr_hfilt_en = 1;
	else
		nr_hfilt_en = 0;

	/* if input is pal and ntsc */
	if (vframe_source_type == VFRAME_SOURCE_TYPE_TUNER) {
		/*input is tuner*/
		RDMA_WR(DI_EI_CTRL1, 0x5ac00f80);
		RDMA_WR(DI_EI_CTRL2, 0x0aff0aff);
		RDMA_WR(DI_BLEND_CTRL, 0x19f00019);
		pr_dbg(
			"%s: tvinfo change, reset di Reg in tuner source\n",
			__func__);
	}

	/* DI_Wr(DI_EI_CTRL0, ei_ctrl0); */
	/* DI_Wr(DI_EI_CTRL1, ei_ctrl1); */
	/* DI_Wr(DI_EI_CTRL2, ei_ctrl2); */
}
#endif
static void recycle_vframe_type_pre(struct di_buf_s *di_buf)
{
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;

	di_lock_irqfiq_save(irq_flag2, fiq_flag);

	queue_in(di_buf, QUEUE_RECYCLE);

	di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
}
/*
 * it depend on local buffer queue type is 2
 */
static int peek_free_linked_buf(void)
{
	struct di_buf_s *p = NULL;
	int itmp, p_index = -2;

	if (list_count(QUEUE_LOCAL_FREE) < 2)
		return -1;

	queue_for_each_entry(p, ptmp, QUEUE_LOCAL_FREE, list) {
		if (abs(p->index - p_index) == 1)
			return min(p->index, p_index);
		else
			p_index = p->index;
	}
	return -1;
}
/*
 * it depend on local buffer queue type is 2
 */
static struct di_buf_s *get_free_linked_buf(int idx)
{
	struct di_buf_s *di_buf = NULL, *di_buf_linked = NULL;
	int pool_idx = 0, di_buf_idx = 0;

	queue_t *q = &(queue[QUEUE_LOCAL_FREE]);

	if (list_count(QUEUE_LOCAL_FREE) < 2)
		return NULL;
	if (q->pool[idx] != 0 && q->pool[idx + 1] != 0) {
		pool_idx = ((q->pool[idx] >> 8) & 0xff) - 1;
		di_buf_idx = q->pool[idx] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (di_buf_idx < di_buf_pool[pool_idx].size) {
				di_buf = &(di_buf_pool[pool_idx].
					di_buf_ptr[di_buf_idx]);
				queue_out(di_buf);
			}
		}
		pool_idx = ((q->pool[idx + 1] >> 8) & 0xff) - 1;
		di_buf_idx = q->pool[idx + 1] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (di_buf_idx < di_buf_pool[pool_idx].size) {
				di_buf_linked =	&(di_buf_pool[pool_idx].
					di_buf_ptr[di_buf_idx]);
				queue_out(di_buf_linked);
			}
		}
		di_buf->di_wr_linked_buf = di_buf_linked;
	}
	return di_buf;
}

static unsigned char pre_de_buf_config(void)
{
	struct di_buf_s *di_buf = NULL;
	vframe_t *vframe;
	int i, di_linked_buf_idx = -1;
	unsigned char change_type = 0;

	if (di_blocking)
		return 0;
	if ((list_count(QUEUE_IN_FREE) < 2 && (!di_pre_stru.di_inp_buf_next)) ||
	    (queue_empty(QUEUE_LOCAL_FREE)))
		return 0;

	if (is_bypass(NULL)) {
		/* some provider has problem if receiver
		 * get all buffers of provider */
		int in_buf_num = 0;
		int bypass_buf_threshold = bypass_get_buf_threshold;
		if ((di_pre_stru.cur_inp_type & VIDTYPE_VIU_444) &&
			(di_pre_stru.cur_source_type ==
			VFRAME_SOURCE_TYPE_HDMI))
			bypass_buf_threshold = bypass_hdmi_get_buf_threshold;
		cur_lev = 0;
		for (i = 0; i < MAX_IN_BUF_NUM; i++)
			if (vframe_in[i] != NULL)
				in_buf_num++;
		if (in_buf_num > bypass_buf_threshold
#ifdef DET3D
		    && (di_pre_stru.vframe_interleave_flag == 0)
#endif
		    )
			return 0;
	} else if (di_pre_stru.prog_proc_type == 2) {
		di_linked_buf_idx = peek_free_linked_buf();
		if (di_linked_buf_idx == -1 && used_post_buf_index != -1) {
			recycle_keep_buffer();
			pr_info("%s: recycle keep buffer for peek null linked buf\n",
				__func__);
			return 0;
		}
	}
	if (di_pre_stru.di_inp_buf_next) {
		di_pre_stru.di_inp_buf = di_pre_stru.di_inp_buf_next;
		di_pre_stru.di_inp_buf_next = NULL;
#ifdef DI_BUFFER_DEBUG
		di_print("%s: di_inp_buf_next %s[%d] => di_inp_buf\n",
			__func__,
			vframe_type_name[di_pre_stru.di_inp_buf->type],
			di_pre_stru.di_inp_buf->index);
#endif
		if (di_pre_stru.di_mem_buf_dup_p == NULL) {/* use n */
			di_pre_stru.di_mem_buf_dup_p = di_pre_stru.di_inp_buf;
#ifdef DI_BUFFER_DEBUG
			di_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
				__func__);
#endif
		}
	} else {
		/* check if source change */
#ifdef CHECK_VDIN_BUF_ERROR
#define WR_CANVAS_BIT                                   0
#define WR_CANVAS_WID                                   8

		vframe = vf_peek(VFM_NAME);

		if (vframe && is_from_vdin(vframe)) {
			if (vframe->canvas0Addr ==
			    Rd_reg_bits((VDIN_WR_CTRL + 0), WR_CANVAS_BIT,
				    WR_CANVAS_WID))
				same_w_r_canvas_count++;
#ifdef RUN_DI_PROCESS_IN_IRQ
			di_pre_stru.vdin2nr = is_input2pre();
#endif
		}
#endif

		vframe = vf_get(VFM_NAME);

		if (vframe == NULL)
			return 0;
		else {
			di_print("DI: get vframe[0x%p] from frontend %u ms.\n",
			vframe,
jiffies_to_msecs(jiffies_64 - vframe->ready_jiffies64));
		}
		vframe->prog_proc_config = (prog_proc_config&0x20) >> 5;

		if (vframe->width > 10000 || vframe->height > 10000 ||
		    hold_video || di_pre_stru.bad_frame_throw_count > 0) {
			if (vframe->width > 10000 || vframe->height > 10000)
				di_pre_stru.bad_frame_throw_count = 10;
			di_pre_stru.bad_frame_throw_count--;
			vf_put(vframe, VFM_NAME);
			vf_notify_provider(
				VFM_NAME, VFRAME_EVENT_RECEIVER_PUT, NULL);
			return 0;
		}
		if (
			((vframe->type & VIDTYPE_TYPEMASK) !=
			 VIDTYPE_PROGRESSIVE) &&
			(vframe->width == 1920) && (vframe->height == 1088))
			force_height = 1080;
		else
			force_height = 0;
		if ((vframe->source_type == VFRAME_SOURCE_TYPE_OTHERS) &&
			(vframe->width % 2 == 1)) {
			force_width = vframe->width - 1;
			if (force_width != (vframe->width - 1))
				pr_info("DI: force source width %u to even num %d.\n",
					vframe->width, force_width);
		} else {
			force_width = 0;
		}
		di_pre_stru.source_trans_fmt = vframe->trans_fmt;
		di_pre_stru.left_right = di_pre_stru.left_right ? 0 : 1;

		di_pre_stru.invert_flag =
			(vframe->type & TB_DETECT_MASK) ? true : false;
		vframe->type &= ~TB_DETECT_MASK;

		if ((((invert_top_bot & 0x2) != 0) ||
				di_pre_stru.invert_flag) &&
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

		if (force_width)
			vframe->width = force_width;

		if (force_height)
			vframe->height = force_height;

		/* backup frame motion info */
		vframe->combing_cur_lev = cur_lev;

		di_print("%s: vf_get => 0x%p\n", __func__, vframe);

		provider_vframe_level--;
		di_buf = get_di_buf_head(QUEUE_IN_FREE);

		if (check_di_buf(di_buf, 10))
			return 0;

		if (di_log_flag & DI_LOG_VFRAME)
			dump_vframe(vframe);

#ifdef SUPPORT_MPEG_TO_VDIN
		if (
			(!is_from_vdin(vframe)) &&
			(vframe->sig_fmt == TVIN_SIG_FMT_NULL) &&
			mpeg2vdin_flag) {
			struct vdin_arg_s vdin_arg;
			struct vdin_v4l2_ops_s *vdin_ops = get_vdin_v4l2_ops();
			vdin_arg.cmd = VDIN_CMD_GET_HISTGRAM;
			vdin_arg.private = (unsigned int)vframe;
			if (vdin_ops->tvin_vdin_func)
				vdin_ops->tvin_vdin_func(0, &vdin_arg);
		}
#endif
		memcpy(di_buf->vframe, vframe, sizeof(vframe_t));

		di_buf->vframe->private_data = di_buf;
		vframe_in[di_buf->index] = vframe;
		di_buf->seq = di_pre_stru.in_seq;
		di_pre_stru.in_seq++;
		queue_out(di_buf);
		change_type = is_source_change(vframe);
		/* source change, when i mix p,force p as i*/
		if (change_type == 1 || (change_type == 2 &&
					 di_pre_stru.cur_prog_flag == 1)) {
			if (di_pre_stru.di_mem_buf_dup_p) {
				/*avoid only 2 i field then p field*/
				if (
					(di_pre_stru.cur_prog_flag == 0) &&
					use_2_interlace_buff)
					di_pre_stru.di_mem_buf_dup_p->
					post_proc_flag = -1;
				di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
				di_pre_stru.di_mem_buf_dup_p = NULL;
			}
			if (di_pre_stru.di_chan2_buf_dup_p) {
				/*avoid only 1 i field then p field*/
				if (
					(di_pre_stru.cur_prog_flag == 0) &&
					use_2_interlace_buff)
					di_pre_stru.di_chan2_buf_dup_p->
					post_proc_flag = -1;
				di_pre_stru.di_chan2_buf_dup_p->pre_ref_count =
					0;
				di_pre_stru.di_chan2_buf_dup_p = NULL;
			}
			/* force recycle keep buffer when switch source */
			if (used_post_buf_index != -1) {
				if (di_buf_post[used_post_buf_index].vframe
					->source_type !=
					di_buf->vframe->source_type) {
					recycle_keep_buffer();
					pr_info("%s: source type changed!!!\n",
						__func__);
				}
			}
			pr_info(
			"%s: source change: 0x%x/%d/%d/%d=>0x%x/%d/%d/%d\n",
				__func__,
				di_pre_stru.cur_inp_type,
				di_pre_stru.cur_width,
				di_pre_stru.cur_height,
				di_pre_stru.cur_source_type,
				di_buf->vframe->type,
				di_buf->vframe->width,
				di_buf->vframe->height,
				di_buf->vframe->source_type);
			di_pre_stru.cur_width = di_buf->vframe->width;
			di_pre_stru.cur_height = di_buf->vframe->height;
			di_pre_stru.cur_prog_flag =
				is_progressive(di_buf->vframe);
			if (di_pre_stru.cur_prog_flag) {
				if ((use_2_interlace_buff) &&
				    !(prog_proc_config & 0x10))
					di_pre_stru.prog_proc_type = 2;
				else
					di_pre_stru.prog_proc_type
						= prog_proc_config & 0x10;
			} else
				di_pre_stru.prog_proc_type = 0;
			di_pre_stru.cur_inp_type = di_buf->vframe->type;
			di_pre_stru.cur_source_type =
				di_buf->vframe->source_type;
			di_pre_stru.cur_sig_fmt = di_buf->vframe->sig_fmt;
			di_pre_stru.orientation = di_buf->vframe->video_angle;
			di_pre_stru.source_change_flag = 1;
			di_pre_stru.same_field_source_flag = 0;
#if defined(NEW_DI_TV)
			di_set_para_by_tvinfo(vframe);
#endif
#ifdef SUPPORT_MPEG_TO_VDIN
			if ((!is_from_vdin(vframe)) &&
			    (vframe->sig_fmt == TVIN_SIG_FMT_NULL) &&
			    (mpeg2vdin_en)) {
				struct vdin_arg_s vdin_arg;
				struct vdin_v4l2_ops_s *vdin_ops =
					get_vdin_v4l2_ops();
				vdin_arg.cmd = VDIN_CMD_MPEGIN_START;
				vdin_arg.h_active = di_pre_stru.cur_width;
				vdin_arg.v_active = di_pre_stru.cur_height;
				if (vdin_ops->tvin_vdin_func)
					vdin_ops->tvin_vdin_func(0, &vdin_arg);
				mpeg2vdin_flag = 1;
			}
#endif
#ifdef NEW_DI_V1
			di_pre_stru.field_count_for_cont = 0;
#endif
		} else if (di_pre_stru.cur_prog_flag == 0) {
			/* check if top/bot interleaved */
			if (change_type == 2)
				/* source is i interleaves p fields */
				di_pre_stru.force_interlace = true;
			if ((di_pre_stru.cur_inp_type &
			VIDTYPE_TYPEMASK) == (di_buf->vframe->type &
			VIDTYPE_TYPEMASK)) {
#ifdef CHECK_VDIN_BUF_ERROR
				if ((di_buf->vframe->type &
				VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP)
					same_field_top_count++;
				else
					same_field_bot_count++;
#endif
				if (di_pre_stru.same_field_source_flag <
				same_field_source_flag_th) {
					/* some source's filed
					 * is top or bot always */
					di_pre_stru.same_field_source_flag++;

					if (skip_wrong_field &&
					is_from_vdin(di_buf->vframe)) {
						recycle_vframe_type_pre(
							di_buf);
						return 0;
					}
				}

			} else
				di_pre_stru.same_field_source_flag = 0;
			di_pre_stru.cur_inp_type = di_buf->vframe->type;
		} else {
			di_pre_stru.same_field_source_flag = 0;
			di_pre_stru.cur_inp_type = di_buf->vframe->type;
		}

		if (is_bypass(di_buf->vframe)) {
			/* bypass progressive */
			di_buf->seq = di_pre_stru.pre_ready_seq++;
			di_buf->post_ref_count = 0;
			cur_lev = 0;
			if (di_pre_stru.source_change_flag) {
				di_buf->new_format_flag = 1;
				di_pre_stru.source_change_flag = 0;
			} else {
				di_buf->new_format_flag = 0;
			}

			if (bypass_state == 0) {
				if (di_pre_stru.di_mem_buf_dup_p) {
					di_pre_stru.di_mem_buf_dup_p->
					pre_ref_count = 0;
					di_pre_stru.di_mem_buf_dup_p = NULL;
				}
				if (di_pre_stru.di_chan2_buf_dup_p) {
					di_pre_stru.di_chan2_buf_dup_p->
					pre_ref_count = 0;
					di_pre_stru.di_chan2_buf_dup_p = NULL;
				}

				if (di_pre_stru.di_wr_buf) {
					di_pre_stru.di_wr_buf->pre_ref_count =
						0;
					di_pre_stru.di_wr_buf->post_ref_count =
						0;
					recycle_vframe_type_pre(
						di_pre_stru.di_wr_buf);
#ifdef DI_BUFFER_DEBUG
					di_print(
						"%s: %s[%d] => recycle_list\n",
						__func__,
						vframe_type_name[di_pre_stru.
							di_wr_buf->type],
						di_pre_stru.di_wr_buf->index);
#endif
					di_pre_stru.di_wr_buf = NULL;
				}

				di_buf->new_format_flag = 1;
				bypass_state = 1;
#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s:bypass_state = 1, is_bypass() %d\n",
					__func__, is_bypass(NULL));
				di_print(
					"trick_mode %d bypass_all %d\n",
					trick_mode, bypass_all);
#endif
			}

			top_bot_config(di_buf);
			queue_in(di_buf, QUEUE_PRE_READY);
			/*if previous isn't bypass post_wr_buf not recycled */
			if (di_pre_stru.di_post_wr_buf && di_pre_rdma_enable) {
				queue_in(
					di_pre_stru.di_post_inp_buf,
					QUEUE_RECYCLE);
				di_pre_stru.di_post_inp_buf = NULL;
			}

			if (
				(bypass_pre & 0x2) &&
				!di_pre_stru.cur_prog_flag)
				di_buf->post_proc_flag = -2;
			else
				di_buf->post_proc_flag = 0;
#ifdef DI_BUFFER_DEBUG
			di_print(
				"%s: %s[%d] => pre_ready_list\n", __func__,
				vframe_type_name[di_buf->type], di_buf->index);
#endif
			return 0;
		} else if (is_progressive(di_buf->vframe)) {
			if (
				is_handle_prog_frame_as_interlace(vframe) &&
				(is_progressive(vframe))) {
				struct di_buf_s *di_buf_tmp = NULL;
				vframe_in[di_buf->index] = NULL;
				di_buf->vframe->type &=
					(~VIDTYPE_TYPEMASK);
				di_buf->vframe->type |=
					VIDTYPE_INTERLACE_TOP;
				di_buf->post_proc_flag = 0;

				di_buf_tmp =
					get_di_buf_head(QUEUE_IN_FREE);
				if (check_di_buf(di_buf_tmp, 10)) {
					recycle_vframe_type_pre(di_buf);
					pr_err("DI:no free in_buffer for progressive skip.\n");
					return 0;
				}
				di_buf_tmp->vframe->private_data
					= di_buf_tmp;
				di_buf_tmp->seq = di_pre_stru.in_seq;
				di_pre_stru.in_seq++;
				queue_out(di_buf_tmp);
				vframe_in[di_buf_tmp->index] = vframe;
				memcpy(
					di_buf_tmp->vframe, vframe,
					sizeof(vframe_t));
				di_pre_stru.di_inp_buf_next
					= di_buf_tmp;
				di_buf_tmp->vframe->type
					&= (~VIDTYPE_TYPEMASK);
				di_buf_tmp->vframe->type
					|= VIDTYPE_INTERLACE_BOTTOM;
				di_buf_tmp->post_proc_flag = 0;

				di_pre_stru.di_inp_buf = di_buf;
#ifdef DI_BUFFER_DEBUG
				di_print(
			"%s: %s[%d] => di_inp_buf; %s[%d] => di_inp_buf_next\n",
					__func__,
					vframe_type_name[di_buf->type],
					di_buf->index,
					vframe_type_name[di_buf_tmp->type],
					di_buf_tmp->index);
#endif
				if (di_pre_stru.di_mem_buf_dup_p == NULL) {
					di_pre_stru.di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
					di_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
						__func__);
#endif
				}
			} else {
				di_buf->post_proc_flag = 0;
				if ((prog_proc_config & 0x40) ||
				    di_pre_stru.force_interlace)
					di_buf->post_proc_flag = 1;
				di_pre_stru.di_inp_buf = di_buf;
#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s: %s[%d] => di_inp_buf\n",
					__func__,
					vframe_type_name[di_buf->type],
					di_buf->index);
#endif
				if (
					di_pre_stru.di_mem_buf_dup_p == NULL) {
					/* use n */
					di_pre_stru.di_mem_buf_dup_p
						= di_buf;
#ifdef DI_BUFFER_DEBUG
					di_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
						__func__);
#endif
				}
			}
		} else {
#ifdef NEW_DI_V1
			if (
				di_pre_stru.di_chan2_buf_dup_p == NULL) {
				di_pre_stru.field_count_for_cont = 0;
				di_mtn_1_ctrl1 |= (1 << 30);
				/* ignore contp2rd and contprd */
			}
#endif
			di_buf->post_proc_flag = 1;
			di_pre_stru.di_inp_buf = di_buf;
			di_print("%s: %s[%d] => di_inp_buf\n", __func__,
				vframe_type_name[di_buf->type], di_buf->index);

			if (di_pre_stru.di_mem_buf_dup_p == NULL) {/* use n */
				di_pre_stru.di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
				di_print(
				"%s: set di_mem_buf_dup_p to be di_inp_buf\n",
					__func__);
#endif
			}
		}
	}

	/* di_wr_buf */
	if (di_pre_stru.prog_proc_type == 2) {
		di_linked_buf_idx = peek_free_linked_buf();
		if (di_linked_buf_idx != -1)
			di_buf = get_free_linked_buf(di_linked_buf_idx);
		else
			di_buf = NULL;
		if (di_buf == NULL) {
			/* recycle_vframe_type_pre(di_pre_stru.di_inp_buf); */
			/*save for next process*/
			recycle_keep_buffer();
			di_pre_stru.di_inp_buf_next = di_pre_stru.di_inp_buf;
			return 0;
		}
		di_buf->post_proc_flag = 0;
		di_buf->di_wr_linked_buf->pre_ref_count = 0;
		di_buf->di_wr_linked_buf->post_ref_count = 0;
		di_buf->canvas_config_flag = 1;
	} else {
		di_buf = get_di_buf_head(QUEUE_LOCAL_FREE);
		if (check_di_buf(di_buf, 11)) {
			/* recycle_keep_buffer();
			pr_dbg("%s:recycle keep buffer\n", __func__);*/
			recycle_vframe_type_pre(di_pre_stru.di_inp_buf);
			return 0;
		}
		queue_out(di_buf);
		if (di_pre_stru.prog_proc_type & 0x10)
			di_buf->canvas_config_flag = 1;
		else
			di_buf->canvas_config_flag = 2;
		di_buf->di_wr_linked_buf = NULL;
	}

	di_pre_stru.di_wr_buf = di_buf;
	di_pre_stru.di_wr_buf->pre_ref_count = 1;

#ifdef DI_BUFFER_DEBUG
	di_print("%s: %s[%d] => di_wr_buf\n", __func__,
		vframe_type_name[di_buf->type], di_buf->index);
	if (di_buf->di_wr_linked_buf)
		di_print("%s: linked %s[%d] => di_wr_buf\n", __func__,
			vframe_type_name[di_buf->di_wr_linked_buf->type],
			di_buf->di_wr_linked_buf->index);
#endif

	memcpy(di_buf->vframe,
		di_pre_stru.di_inp_buf->vframe, sizeof(vframe_t));
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
	di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
	/* set vframe bit info */
	di_buf->vframe->bitdepth &= ~(BITDEPTH_YMASK);
	di_buf->vframe->bitdepth &= ~(FULL_PACK_422_MODE);
	if (di_force_bit_mode == 10) {
		di_buf->vframe->bitdepth |= (BITDEPTH_Y10);
		if (full_422_pack)
			di_buf->vframe->bitdepth |= (FULL_PACK_422_MODE);
	} else
		di_buf->vframe->bitdepth |= (BITDEPTH_Y8);

	if (di_pre_stru.prog_proc_type) {
		di_buf->vframe->type = VIDTYPE_PROGRESSIVE |
				       VIDTYPE_VIU_422 |
				       VIDTYPE_VIU_SINGLE_PLANE |
				       VIDTYPE_VIU_FIELD;
		if (di_pre_stru.cur_inp_type & VIDTYPE_PRE_INTERLACE)
			di_buf->vframe->type |= VIDTYPE_PRE_INTERLACE;
	} else {
		if (
			((di_pre_stru.di_inp_buf->vframe->type &
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
	}

/* */
	if (is_bypass_post()) {
		if (bypass_post_state == 0)
			di_pre_stru.source_change_flag = 1;

		bypass_post_state = 1;
	} else {
		if (bypass_post_state)
			di_pre_stru.source_change_flag = 1;

		bypass_post_state = 0;
	}

/* if(is_progressive(di_pre_stru.di_inp_buf->vframe)){ */
	if (di_pre_stru.di_inp_buf->post_proc_flag == 0) {
		di_pre_stru.enable_mtnwr = 0;
		di_pre_stru.enable_pulldown_check = 0;
		di_buf->post_proc_flag = 0;
	} else if (bypass_post_state) {
		di_pre_stru.enable_mtnwr = 0;
		di_pre_stru.enable_pulldown_check = 0;
		di_buf->post_proc_flag = 0;
	} else {
		if (di_pre_stru.di_chan2_buf_dup_p == NULL) {
			di_pre_stru.enable_mtnwr = 0;
			di_pre_stru.enable_pulldown_check = 0;
			di_buf->post_proc_flag = 2;
		} else {
			di_pre_stru.enable_mtnwr = 1;
			di_buf->post_proc_flag = 1;
			di_pre_stru.enable_pulldown_check =
				di_pre_stru.cur_prog_flag ? 0 : 1;
		}
	}

#ifndef USE_LIST
	if ((di_pre_stru.di_mem_buf_dup_p == di_pre_stru.di_wr_buf) ||
	    (di_pre_stru.di_chan2_buf_dup_p == di_pre_stru.di_wr_buf)) {
		pr_dbg("+++++++++++++++++++++++\n");
		if (recovery_flag == 0)
			recovery_log_reason = 12;

		recovery_flag++;
		return 0;
	}
#endif
	return 1;
}

static int check_recycle_buf(void)
{
	struct di_buf_s *di_buf = NULL;/* , *ptmp; */
	int itmp;
	int ret = 0;

	if (di_blocking)
		return ret;
	queue_for_each_entry(di_buf, ptmp, QUEUE_RECYCLE, list) {
		if ((di_buf->pre_ref_count == 0) &&
		    (di_buf->post_ref_count == 0)) {
			if (di_buf->type == VFRAME_TYPE_IN) {
				queue_out(di_buf);
				if (vframe_in[di_buf->index]) {
					vf_put(
						vframe_in[di_buf->index],
						VFM_NAME);
					vf_notify_provider(VFM_NAME,
						VFRAME_EVENT_RECEIVER_PUT,
						NULL);
#ifdef DI_BUFFER_DEBUG
					di_print(
						"%s: vf_put(%d) %x\n", __func__,
						di_pre_stru.recycle_seq,
						vframe_in[di_buf->index]);
#endif
					vframe_in[di_buf->index] = NULL;
				}
				di_buf->invert_top_bot_flag = 0;
				queue_in(di_buf, QUEUE_IN_FREE);
				di_pre_stru.recycle_seq++;
				ret |= 1;
			} else {
				queue_out(di_buf);
				di_buf->invert_top_bot_flag = 0;
				queue_in(di_buf, QUEUE_LOCAL_FREE);
				if (di_buf->di_wr_linked_buf) {
					queue_in(
						di_buf->di_wr_linked_buf,
						QUEUE_LOCAL_FREE);
#ifdef DI_BUFFER_DEBUG
					di_print(
					"%s: linked %s[%d]=>recycle_list\n",
						__func__,
						vframe_type_name[
						di_buf->di_wr_linked_buf->type],
						di_buf->di_wr_linked_buf->index
					);
#endif
					di_buf->di_wr_linked_buf = NULL;
				}
				ret |= 2;
			}
#ifdef DI_BUFFER_DEBUG
			di_print("%s: recycle %s[%d]\n", __func__,
				vframe_type_name[di_buf->type], di_buf->index);
#endif
		}
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
static void det3d_irq(void)
{
	unsigned int data32 = 0, likely_val = 0;
	unsigned long frame_sum = 0;
	if (!det3d_en)
		return;

	data32 = det3d_fmt_detect();
	switch (data32) {
	case TVIN_TFMT_3D_DET_LR:
	case TVIN_TFMT_3D_LRH_OLOR:
		di_pre_stru.det_lr++;
		break;
	case TVIN_TFMT_3D_DET_TB:
	case TVIN_TFMT_3D_TB:
		di_pre_stru.det_tp++;
		break;
	case TVIN_TFMT_3D_DET_INTERLACE:
		di_pre_stru.det_la++;
		break;
	default:
		di_pre_stru.det_null++;
		break;
	}

	if (det3d_mode != data32) {
		det3d_mode = data32;
		di_print("[det3d..]new 3d fmt: %d\n", det3d_mode);
	}
	if (frame_count > 20) {
		frame_sum = di_pre_stru.det_lr + di_pre_stru.det_tp
					+ di_pre_stru.det_la
					+ di_pre_stru.det_null;
		if ((frame_count%det3d_frame_cnt) || (frame_sum > UINT_MAX))
			return;
		likely_val = max3(di_pre_stru.det_lr,
						di_pre_stru.det_tp,
						di_pre_stru.det_la);
		if (di_pre_stru.det_null >= likely_val)
			det3d_mode = 0;
		else if (likely_val == di_pre_stru.det_lr)
			det3d_mode = TVIN_TFMT_3D_LRH_OLOR;
		else if (likely_val == di_pre_stru.det_tp)
			det3d_mode = TVIN_TFMT_3D_TB;
		else
			det3d_mode = TVIN_TFMT_3D_DET_INTERLACE;
		di_pre_stru.det3d_trans_fmt = det3d_mode;
	} else {
		di_pre_stru.det3d_trans_fmt = 0;
	}
}
#endif

static bool calc_mcinfo_en = 1;
module_param(calc_mcinfo_en, bool, 0664);
MODULE_PARM_DESC(calc_mcinfo_en, "/n get mcinfo for post /n");

static unsigned int colcfd_thr = 128;
module_param(colcfd_thr, uint, 0664);
MODULE_PARM_DESC(colcfd_thr, "/n threshold for cfd/n");

unsigned int ro_mcdi_col_cfd[26];
static void get_mcinfo_from_reg_in_irq(void)
{
	unsigned int i = 0, ncolcrefsum = 0, blkcount = 0, *reg = NULL;

/*get info for current field process by post*/
	di_pre_stru.di_wr_buf->curr_field_mcinfo.highvertfrqflg =
		(Rd(MCDI_RO_HIGH_VERT_FRQ_FLG) & 0x1);
/* post:MCDI_MC_REL_GAIN_OFFST_0 */
	di_pre_stru.di_wr_buf->curr_field_mcinfo.motionparadoxflg =
		(Rd(MCDI_RO_MOTION_PARADOX_FLG) & 0x1);
/* post:MCDI_MC_REL_GAIN_OFFST_0 */
	reg = di_pre_stru.di_wr_buf->curr_field_mcinfo.regs;
	for (i = 0; i < 26; i++) {
		ro_mcdi_col_cfd[i] = Rd(0x2fb0 + i);
		di_pre_stru.di_wr_buf->curr_field_mcinfo.regs[i] = 0;
		if (!calc_mcinfo_en)
			*(reg + i) = ro_mcdi_col_cfd[i];
	}
	if (calc_mcinfo_en) {
		blkcount = (di_pre_stru.cur_width + 4) / 5;
		for (i = 0; i < blkcount; i++) {
			ncolcrefsum +=
				((ro_mcdi_col_cfd[i / 32] >> (i % 32)) & 0x1);
			if (
				((ncolcrefsum + (blkcount >> 1)) << 8) /
				blkcount > colcfd_thr)
				for (i = 0; i < blkcount; i++)
					*(reg + i / 32) += (1 << (i % 32));
		}
	}
}

static unsigned int bit_reverse(unsigned int val)
{
	unsigned int i = 0, res = 0;
	for (i = 0; i < 16; i++) {
		res |= (((val&(1<<i))>>i)<<(31-i));
		res |= (((val&(1<<(31-i)))<<i)>>(31-i));
	}
	return res;
}

static void set_post_mcinfo(struct mcinfo_pre_s *curr_field_mcinfo)
{
	unsigned int i = 0, value = 0;

	DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_REL_GAIN_OFFST_0,
		curr_field_mcinfo->highvertfrqflg, 24, 1);
	DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_REL_GAIN_OFFST_0,
		curr_field_mcinfo->motionparadoxflg, 25, 1);
	for (i = 0; i < 26; i++) {
		if (overturn)
			value = bit_reverse(curr_field_mcinfo->regs[i]);
		else
			value = curr_field_mcinfo->regs[i];
		DI_VSYNC_WR_MPEG_REG(0x2f78 + i, value);
	}
}

static irqreturn_t de_irq(int irq, void *dev_instance)
{
#ifndef CHECK_DI_DONE
	unsigned int data32 = Rd(DI_INTR_CTRL);
	unsigned int mask32 = (data32 >> 16) & 0x1ff;
	int flag = 0;

	if (di_pre_stru.pre_de_busy) {
		/* only one inetrrupr mask should be enable */
		if ((data32 & 2) && !(mask32 & 2)) {
			di_print("== MTNWR ==\n");
			flag = 1;
		} else if ((data32 & 1) && !(mask32 & 1)) {
			di_print("== NRWR ==\n");
			flag = 1;
		} else {
			di_print("== DI IRQ 0x%x ==\n", data32);
			flag = 0;
		}
	}

#endif

#ifdef DET3D
	if (det3d_en) {
		if ((data32 & 0x100) && !(mask32 & 0x100) && flag) {
			DI_Wr(DI_INTR_CTRL, data32);
			det3d_irq();
		} else {
			goto end;
		}
	} else {
		DI_Wr(DI_INTR_CTRL, data32);
	}
#else
	DI_Wr(DI_INTR_CTRL, data32);
#endif
	if ((post_wr_en && post_wr_surpport) && (data32&0x4)) {
		di_post_stru.de_post_process_done = 1;
		di_post_stru.post_de_busy = 0;
		if (!(data32 & 0x2)) {
			DI_Wr(DI_INTR_CTRL, data32);
			goto end;
		}
	}
	if (pre_process_time_force)
		return IRQ_HANDLED;

	if (di_pre_stru.pre_de_busy == 0) {
		di_print("%s: wrong enter %x\n", __func__, Rd(DI_INTR_CTRL));
		return IRQ_HANDLED;
	}

	if (flag) {
		if (mcpre_en)
			get_mcinfo_from_reg_in_irq();
#ifdef NEW_DI_V4
		if (dnr_en)
			run_dnr_in_irq(
				di_pre_stru.di_nrwr_mif.end_x + 1,
				di_pre_stru.di_nrwr_mif.end_y + 1);
		else
			if ((Rd(DNR_CTRL) != 0) && dnr_reg_update)
				DI_Wr(DNR_CTRL, 0);
#endif
		/* disable mif */
		enable_di_pre_mif(0);
		di_pre_stru.pre_de_process_done = 1;
		di_pre_stru.pre_de_busy = 0;

end:
		if (init_flag && mem_flag)
			/* pr_dbg("%s:up di sema\n", __func__); */
			trigger_pre_di_process(TRIGGER_PRE_BY_DE_IRQ);
	}

	di_print("DI:buf[%d] irq end.\n", di_pre_stru.di_inp_buf->seq);

	return IRQ_HANDLED;
}

/*
 * di post process
 */
static void inc_post_ref_count(struct di_buf_s *di_buf)
{
/* int post_blend_mode; */

	if (di_buf == NULL) {
		pr_dbg("%s: Error\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 13;

		recovery_flag++;
	}

	if (di_buf->di_buf_dup_p[1])
		di_buf->di_buf_dup_p[1]->post_ref_count++;

	if (di_buf->di_buf_dup_p[0])
		di_buf->di_buf_dup_p[0]->post_ref_count++;

	if (di_buf->di_buf_dup_p[2])
		di_buf->di_buf_dup_p[2]->post_ref_count++;
}

static void dec_post_ref_count(struct di_buf_s *di_buf)
{
	if (di_buf == NULL) {
		pr_dbg("%s: Error\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 14;

		recovery_flag++;
	}
	if (di_buf->pulldown_mode == PULL_DOWN_BUF1)
		return;
	if (di_buf->di_buf_dup_p[1])
		di_buf->di_buf_dup_p[1]->post_ref_count--;

	if (di_buf->di_buf_dup_p[0] &&
	    di_buf->di_buf_dup_p[0]->post_proc_flag != -2)
		di_buf->di_buf_dup_p[0]->post_ref_count--;

	if (di_buf->di_buf_dup_p[2])
		di_buf->di_buf_dup_p[2]->post_ref_count--;
}

static void vscale_skip_disable_post(struct di_buf_s *di_buf, vframe_t *disp_vf)
{
	struct di_buf_s *di_buf_i = NULL;
	int canvas_height = di_buf->di_buf[0]->canvas_height;

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
		if ((di_buf->di_buf[0]->post_proc_flag > 0)
		    && (di_buf->di_buf_dup_p[1]))
			di_buf_i = di_buf->di_buf_dup_p[1];
		else
			di_buf_i = di_buf->di_buf[0];
	}
	disp_vf->type = di_buf_i->vframe->type;
	/* pr_dbg("%s (%x %x) (%x %x)\n", __func__,
	 * disp_vf, disp_vf->type, di_buf_i->vframe,
	 * di_buf_i->vframe->type); */
	disp_vf->width = di_buf_i->vframe->width;
	disp_vf->height = di_buf_i->vframe->height;
	disp_vf->duration = di_buf_i->vframe->duration;
	disp_vf->pts = di_buf_i->vframe->pts;
	disp_vf->pts_us64 = di_buf_i->vframe->pts_us64;
	disp_vf->flag = di_buf_i->vframe->flag;
	disp_vf->canvas0Addr = di_post_idx[di_post_stru.canvas_id][0];
	disp_vf->canvas1Addr = di_post_idx[di_post_stru.canvas_id][0];
	canvas_config(
		di_post_idx[di_post_stru.canvas_id][0],
		di_buf_i->nr_adr, di_buf_i->canvas_width[NR_CANVAS],
		canvas_height, 0, 0);
	disable_post_deinterlace_2();
	di_post_stru.vscale_skip_flag = true;
}
static void process_vscale_skip(struct di_buf_s *di_buf, vframe_t *disp_vf)
{
			int ret = 0, vpp_3d_mode = 0;
	if ((di_buf->di_buf[0] != NULL) && (di_vscale_skip_enable & 0x5) &&
	    (di_buf->process_fun_index != PROCESS_FUN_NULL)) {
		ret = get_current_vscale_skip_count(disp_vf);
		di_vscale_skip_count = (ret & 0xff);
		vpp_3d_mode = ((ret >> 8) & 0xff);
		if (((di_vscale_skip_count > 0) &&
		     (di_vscale_skip_enable & 0x5))
		    || (di_vscale_skip_enable >> 16) ||
		    (bypass_dynamic_flag & 0x2)) {
			if ((di_vscale_skip_enable & 0x4) && !vpp_3d_mode) {
				if (di_buf->di_buf_dup_p[1] &&
				    di_buf->pulldown_mode != PULL_DOWN_BUF1)
					di_buf->pulldown_mode = PULL_DOWN_EI;
			} else {
				vscale_skip_disable_post(di_buf, disp_vf);
			}
		}
	}
}

static int de_post_disable_fun(void *arg, vframe_t *disp_vf)
{
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;

	di_post_stru.vscale_skip_flag = false;
	di_post_stru.toggle_flag = true;

	process_vscale_skip(di_buf, disp_vf);
/* for atv static image flickering */
	if (di_buf->process_fun_index == PROCESS_FUN_NULL)
		disable_post_deinterlace_2();

	return 1;
/* called for new_format_flag, make
*  video set video_property_changed */
}

static int do_nothing_fun(void *arg, vframe_t *disp_vf)
{
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;

	di_post_stru.vscale_skip_flag = false;
	di_post_stru.toggle_flag = true;

	process_vscale_skip(di_buf, disp_vf);

	if (di_buf->process_fun_index == PROCESS_FUN_NULL) {
		if (Rd(DI_IF1_GEN_REG) & 0x1 || Rd(DI_POST_CTRL) & 0xf)
			disable_post_deinterlace_2();
	/*if(di_buf->pulldown_mode == PULL_DOWN_EI && Rd(DI_IF1_GEN_REG)&0x1)
	 * DI_VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG, 0x3 << 30);*/
	}
	return 0;
}

static int do_pre_only_fun(void *arg, vframe_t *disp_vf)
{
	di_post_stru.vscale_skip_flag = false;
	di_post_stru.toggle_flag = true;

#ifdef DI_USE_FIXED_CANVAS_IDX
	if (arg) {
		struct di_buf_s *di_buf = (struct di_buf_s *)arg;
		vframe_t *vf = di_buf->vframe;
		int width, canvas_height;
		if ((vf == NULL) || (di_buf->di_buf[0] == NULL)) {
			di_print("error:%s,NULL point!!\n", __func__);
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
#ifdef CONFIG_VSYNC_RDMA
		if ((is_vsync_rdma_enable() &&
		     ((force_update_post_reg & 0x40) == 0)) ||
		    (force_update_post_reg & 0x20)) {
			di_post_stru.canvas_id = di_post_stru.next_canvas_id;
		} else {
			di_post_stru.canvas_id = 0;
			di_post_stru.next_canvas_id = 1;
		}
#endif

		canvas_config(
			di_post_idx[di_post_stru.canvas_id][0],
			di_buf->di_buf[0]->nr_adr,
			di_buf->di_buf[0]->canvas_width[NR_CANVAS],
			canvas_height, 0, 0);

		vf->canvas0Addr =
			di_post_idx[di_post_stru.canvas_id][0];
		vf->canvas1Addr =
			di_post_idx[di_post_stru.canvas_id][0];
#ifdef DET3D
		if (di_pre_stru.vframe_interleave_flag && di_buf->di_buf[1]) {
			canvas_config(
				di_post_idx[di_post_stru.canvas_id][1],
				di_buf->di_buf[1]->nr_adr,
				di_buf->di_buf[1]->canvas_width[NR_CANVAS],
				canvas_height, 0, 0);
			vf->canvas1Addr =
				di_post_idx[di_post_stru.canvas_id][1];
			vf->duration <<= 1;
		}
#endif
		di_post_stru.next_canvas_id = di_post_stru.canvas_id ? 0 : 1;

		if (di_buf->process_fun_index == PROCESS_FUN_NULL) {
			if (Rd(DI_IF1_GEN_REG) & 0x1 || Rd(DI_POST_CTRL) & 0xf)
				disable_post_deinterlace_2();
		}

	}
#endif

	return 0;
}

static void config_fftffb_mode(struct di_buf_s *di_buf,
	unsigned int *post_field_type, char fftffb_mode)
{
	unsigned char tmp_idx = 0;
	switch (fftffb_mode) {
	case 1:
		*post_field_type =
		(di_buf->di_buf_dup_p[0]->vframe->type &
		 VIDTYPE_TYPEMASK)
		== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		di_post_stru.di_buf0_mif.canvas0_addr0 =
		di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
		di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		di_post_stru.di_buf2_mif.canvas0_addr0 =
		di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		di_post_stru.di_mtnprd_mif.canvas_num =
		di_buf->di_buf_dup_p[1]->mtn_canvas_idx;
		break;
	case 2:
		*post_field_type =
		(di_buf->di_buf_dup_p[2]->vframe->type &
		 VIDTYPE_TYPEMASK)
		== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		di_post_stru.di_buf0_mif.canvas0_addr0 =
		di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
		di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		di_post_stru.di_buf2_mif.canvas0_addr0 =
		di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		di_post_stru.di_mtnprd_mif.canvas_num =
		di_buf->di_buf_dup_p[1]->mtn_canvas_idx;
		break;
	default:
		*post_field_type =
		(di_buf->di_buf_dup_p[1]->vframe->type &
		 VIDTYPE_TYPEMASK)
		== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		di_post_stru.di_buf0_mif.canvas0_addr0 =
		di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
		di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		di_post_stru.di_buf2_mif.canvas0_addr0 =
		di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		di_post_stru.di_mtnprd_mif.canvas_num =
		di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		break;
	}

	if (is_meson_txl_cpu() && overturn) {
		/* swap if1&if2 mean negation of mv for normal di*/
		tmp_idx = di_post_stru.di_buf1_mif.canvas0_addr0;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
		di_post_stru.di_buf2_mif.canvas0_addr0;
		di_post_stru.di_buf2_mif.canvas0_addr0 = tmp_idx;
	}

}

static void get_vscale_skip_count(unsigned par)
{
	di_vscale_skip_count_real = (par >> 24) & 0xff;
}

static void recalc_pd_wnd(struct di_buf_s *di_buf, unsigned short offset)
{
	if (di_buf->reg0_s > offset)
		di_buf->reg0_s -= offset;
	else
		di_buf->reg0_s = 0;
	if (di_buf->reg0_e > offset)
		di_buf->reg0_e -= offset;
	else
		di_buf->reg0_e = 0;

	if (di_buf->reg1_s > offset)
		di_buf->reg1_s -= offset;
	else
		di_buf->reg1_s = 0;
	if (di_buf->reg1_e > offset)
		di_buf->reg1_e -= offset;
	else
		di_buf->reg1_e = 0;

	if (di_buf->reg2_s > offset)
		di_buf->reg2_s -= offset;
	else
		di_buf->reg2_s = 0;
	if (di_buf->reg2_e > offset)
		di_buf->reg2_e -= offset;
	else
		di_buf->reg2_e = 0;

	if (di_buf->reg3_s > offset)
		di_buf->reg3_s -= offset;
	else
		di_buf->reg3_s = 0;
	if (di_buf->reg3_e > offset)
		di_buf->reg3_e -= offset;
	else
		di_buf->reg3_e = 0;
}
#define get_vpp_reg_update_flag(par) ((par >> 16) & 0x1)

static unsigned int pldn_dly = 1;
static unsigned int tbbtff_dly;

static unsigned int pldn_wnd_flsh = 1;
module_param(pldn_wnd_flsh, uint, 0644);
MODULE_PARM_DESC(pldn_wnd_flsh, "/n reflesh the window./n");

static unsigned int pldn_pst_set;
module_param(pldn_pst_set, uint, 0644);
MODULE_PARM_DESC(pldn_pst_set, "/n pulldown post setting./n");

static unsigned int post_blend;
module_param(post_blend, uint, 0664);
MODULE_PARM_DESC(post_blend, "/n show blend mode/n");
static unsigned int post_ei;
module_param(post_ei, uint, 0664);
MODULE_PARM_DESC(post_ei, "/n show blend mode/n");
static int
de_post_process(void *arg, unsigned zoom_start_x_lines,
		unsigned zoom_end_x_lines, unsigned zoom_start_y_lines,
		unsigned zoom_end_y_lines, vframe_t *disp_vf)
{
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;
	struct di_buf_s *di_pldn_buf = di_buf->di_buf_dup_p[pldn_dly];
	unsigned int di_width, di_height, di_start_x, di_end_x;
	unsigned int di_start_y, di_end_y, hold_line = post_hold_line;
	unsigned int post_blend_en = 0, post_blend_mode = 0,
		     blend_mtn_en = 0, ei_en = 0, post_field_num = 0;
	int di_vpp_en, di_ddr_en;
	unsigned char mc_pre_flag = 0;

	if ((di_get_power_control(1) == 0) || di_post_stru.vscale_skip_flag)
		return 0;
	if (is_in_queue(di_buf, QUEUE_POST_FREE)) {
		pr_info("%s post_buf[%d] is in post free list.\n",
				__func__, di_buf->index);
		return 0;
	}
	get_vscale_skip_count(zoom_start_x_lines);

	if ((!di_post_stru.toggle_flag) &&
	    ((force_update_post_reg & 0x10) == 0))
		return 0;

	if ((di_buf == NULL) || (di_buf->di_buf_dup_p[0] == NULL))
		return 0;

	if (di_post_stru.toggle_flag && di_buf->di_buf_dup_p[1])
		top_bot_config(di_buf->di_buf_dup_p[1]);

	di_post_stru.toggle_flag = false;

	di_post_stru.cur_disp_index = di_buf->index;

	if ((di_post_stru.post_process_fun_index != 1) ||
	    ((force_update_post_reg & 0xf) != 0)) {
		force_update_post_reg &= ~0x1;
		di_post_stru.post_process_fun_index = 1;
		di_post_stru.update_post_reg_flag = update_post_reg_count;
	}

	if (get_vpp_reg_update_flag(zoom_start_x_lines))
		di_post_stru.update_post_reg_flag = update_post_reg_count;
/* pr_dbg("%s set update_post_reg_flag to %d\n", __func__,
 * di_post_stru.update_post_reg_flag); */

	zoom_start_x_lines = zoom_start_x_lines & 0xffff;
	zoom_end_x_lines = zoom_end_x_lines & 0xffff;
	zoom_start_y_lines = zoom_start_y_lines & 0xffff;
	zoom_end_y_lines = zoom_end_y_lines & 0xffff;

	if (((init_flag == 0) || (mem_flag == 0)) &&
		(new_keep_last_frame_enable == 0))
		return 0;

	di_start_x = zoom_start_x_lines;
	di_end_x = zoom_end_x_lines;
	di_width = di_end_x - di_start_x + 1;
	di_start_y = zoom_start_y_lines;
	di_end_y = zoom_end_y_lines;
	di_height = di_end_y - di_start_y + 1;
	di_height = di_height / (di_vscale_skip_count_real + 1);
	/* make sure the height is even number */
	if (di_height%2)
		di_height++;
/* pr_dbg("height = (%d %d %d %d %d)\n",
*  di_buf->vframe->height, zoom_start_x_lines, zoom_end_x_lines,
*  zoom_start_y_lines, zoom_end_y_lines); */

	if (Rd(DI_POST_SIZE) != ((di_width - 1) | ((di_height - 1) << 16)) ||
	    di_post_stru.buf_type != di_buf->di_buf_dup_p[0]->type ||
	    (di_post_stru.di_buf0_mif.luma_x_start0 != di_start_x) ||
	    (di_post_stru.di_buf0_mif.luma_y_start0 != di_start_y / 2)) {
		di_post_stru.buf_type = di_buf->di_buf_dup_p[0]->type;

		initial_di_post_2(di_width, di_height, hold_line);

		if ((di_buf->di_buf_dup_p[0]->vframe == NULL) ||
			(di_buf->vframe == NULL))
			return 0;
		/* bit mode config */
		if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
			if (di_buf->vframe->type & VIDTYPE_VIU_444) {
				di_post_stru.di_buf0_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:2;
				di_post_stru.di_buf1_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:2;
				di_post_stru.di_buf2_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:2;
			} else {
				di_post_stru.di_buf0_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
				di_post_stru.di_buf1_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
				di_post_stru.di_buf2_mif.bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE)?3:1;
			}
		} else {
			di_post_stru.di_buf0_mif.bit_mode = 0;
			di_post_stru.di_buf1_mif.bit_mode = 0;
			di_post_stru.di_buf2_mif.bit_mode = 0;
		}
		if (di_buf->vframe->type & VIDTYPE_VIU_444) {
			di_post_stru.di_buf0_mif.video_mode = 1;
			di_post_stru.di_buf1_mif.video_mode = 1;
			di_post_stru.di_buf2_mif.video_mode = 1;
		} else {
			di_post_stru.di_buf0_mif.video_mode = 0;
			di_post_stru.di_buf1_mif.video_mode = 0;
			di_post_stru.di_buf2_mif.video_mode = 0;
		}
		if (di_post_stru.buf_type == VFRAME_TYPE_IN &&
		    !(di_buf->di_buf_dup_p[0]->vframe->type &
		      VIDTYPE_VIU_FIELD)) {
			if (di_buf->vframe->type & VIDTYPE_VIU_NV21) {
				di_post_stru.di_buf0_mif.set_separate_en = 1;
				di_post_stru.di_buf1_mif.set_separate_en = 1;
				di_post_stru.di_buf2_mif.set_separate_en = 1;
			} else {
				di_post_stru.di_buf0_mif.set_separate_en = 0;
				di_post_stru.di_buf1_mif.set_separate_en = 0;
				di_post_stru.di_buf2_mif.set_separate_en = 0;
			}
			di_post_stru.di_buf0_mif.luma_y_start0 = di_start_y;
			di_post_stru.di_buf0_mif.luma_y_end0 = di_end_y;
		} else { /* from vdin or local vframe process by di pre */
			di_post_stru.di_buf0_mif.set_separate_en = 0;
			di_post_stru.di_buf0_mif.luma_y_start0 =
				di_start_y >> 1;
			di_post_stru.di_buf0_mif.luma_y_end0 = di_end_y >> 1;
			di_post_stru.di_buf1_mif.set_separate_en = 0;
			di_post_stru.di_buf1_mif.luma_y_start0 =
				di_start_y >> 1;
			di_post_stru.di_buf1_mif.luma_y_end0 = di_end_y >> 1;
			di_post_stru.di_buf2_mif.set_separate_en = 0;
			di_post_stru.di_buf2_mif.luma_y_end0 = di_end_y >> 1;
			di_post_stru.di_buf2_mif.luma_y_start0 =
				di_start_y >> 1;
		}
		di_post_stru.di_buf0_mif.luma_x_start0 = di_start_x;
		di_post_stru.di_buf0_mif.luma_x_end0 = di_end_x;
		di_post_stru.di_buf1_mif.luma_x_start0 = di_start_x;
		di_post_stru.di_buf1_mif.luma_x_end0 = di_end_x;
		di_post_stru.di_buf2_mif.luma_x_start0 = di_start_x;
		di_post_stru.di_buf2_mif.luma_x_end0 = di_end_x;

		if ((post_wr_en && post_wr_surpport)) {
			di_post_stru.di_diwr_mif.start_x = di_start_x;
			di_post_stru.di_diwr_mif.end_x   = di_end_x;
			di_post_stru.di_diwr_mif.start_y = di_start_y;
			di_post_stru.di_diwr_mif.end_y   = di_end_y;
		}

		di_post_stru.di_mtnprd_mif.start_x = di_start_x;
		di_post_stru.di_mtnprd_mif.end_x = di_end_x;
		di_post_stru.di_mtnprd_mif.start_y = di_start_y >> 1;
		di_post_stru.di_mtnprd_mif.end_y = di_end_y >> 1;
		if (mcpre_en) {
			di_post_stru.di_mcvecrd_mif.start_x = di_start_x / 5;
			di_post_stru.di_mcvecrd_mif.vecrd_offset =
				((di_start_x % 5) ? (5 - di_start_x % 5) : 0);
			di_post_stru.di_mcvecrd_mif.start_y =
				(di_start_y >> 1);
			di_post_stru.di_mcvecrd_mif.size_x =
				(di_end_x + 1 + 4) / 5 - 1 - di_start_x / 5;
			di_post_stru.di_mcvecrd_mif.size_y =
				(di_height >> 1);
		}
		di_post_stru.update_post_reg_flag = update_post_reg_count;
		/* if height decrease, mtn will not enough */
		if (di_buf->pulldown_mode != PULL_DOWN_BUF1)
			di_buf->pulldown_mode = PULL_DOWN_EI;
	}

#ifdef DI_USE_FIXED_CANVAS_IDX
#ifdef CONFIG_VSYNC_RDMA
	if ((is_vsync_rdma_enable() &&
		((force_update_post_reg & 0x40) == 0)) ||
		(force_update_post_reg & 0x20)) {
		di_post_stru.canvas_id = di_post_stru.next_canvas_id;
	} else {
		di_post_stru.canvas_id = 0;
		di_post_stru.next_canvas_id = 1;
	}
#endif
	post_blend = di_buf->pulldown_mode;
	switch (di_buf->pulldown_mode) {
	case PULL_DOWN_BLEND_0:
	case PULL_DOWN_NORMAL:
		config_canvas_idx(
			di_buf->di_buf_dup_p[1],
			di_post_idx[di_post_stru.canvas_id][0], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[2], -1,
			di_post_idx[di_post_stru.canvas_id][2]);
		config_canvas_idx(
			di_buf->di_buf_dup_p[0],
			di_post_idx[di_post_stru.canvas_id][1], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[2],
			di_post_idx[di_post_stru.canvas_id][3], -1);
		if (mcpre_en)
			config_mcvec_canvas_idx(
				di_buf->di_buf_dup_p[2],
				di_post_idx[di_post_stru.canvas_id][4]);

		/* for post_wr_en */
		if ((post_wr_en && post_wr_surpport) && !mcpre_en)
			config_canvas_idx(
			di_buf, di_post_idx[di_post_stru.canvas_id][4], -1);
		break;
	case PULL_DOWN_BLEND_2:
		config_canvas_idx(
			di_buf->di_buf_dup_p[0],
			di_post_idx[di_post_stru.canvas_id][3], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[1],
			di_post_idx[di_post_stru.canvas_id][0], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[2], -1,
			di_post_idx[di_post_stru.canvas_id][2]);
		config_canvas_idx(
			di_buf->di_buf_dup_p[2],
			di_post_idx[di_post_stru.canvas_id][1], -1);
		if (mcpre_en)
			config_mcvec_canvas_idx(
				di_buf->di_buf_dup_p[2],
				di_post_idx[di_post_stru.canvas_id][4]);
		if ((post_wr_en && post_wr_surpport) && !mcpre_en)
			config_canvas_idx(
			di_buf, di_post_idx[di_post_stru.canvas_id][4], -1);
		break;
	case PULL_DOWN_MTN:
		config_canvas_idx(
			di_buf->di_buf_dup_p[1],
			di_post_idx[di_post_stru.canvas_id][0], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[2], -1,
			di_post_idx[di_post_stru.canvas_id][2]);
		config_canvas_idx(
			di_buf->di_buf_dup_p[0],
			di_post_idx[di_post_stru.canvas_id][1], -1);
		if ((post_wr_en && post_wr_surpport))
			config_canvas_idx(
di_buf, di_post_idx[di_post_stru.canvas_id][4], -1);
		break;
	case PULL_DOWN_BUF1:/* wave with buf1 */
		config_canvas_idx(
			di_buf->di_buf_dup_p[1],
			di_post_idx[di_post_stru.canvas_id][0], -1);
		config_canvas_idx(
			di_buf->di_buf_dup_p[1], -1,
			di_post_idx[di_post_stru.canvas_id][2]);
		config_canvas_idx(
			di_buf->di_buf_dup_p[0],
			di_post_idx[di_post_stru.canvas_id][1], -1);
		if ((post_wr_en && post_wr_surpport))
			config_canvas_idx(
di_buf, di_post_idx[di_post_stru.canvas_id][4], -1);
		break;
	case PULL_DOWN_EI:
		if (di_buf->di_buf_dup_p[1])
			config_canvas_idx(
				di_buf->di_buf_dup_p[1],
				di_post_idx[di_post_stru.canvas_id][0], -1);
		if ((post_wr_en && post_wr_surpport))
			config_canvas_idx(
di_buf, di_post_idx[di_post_stru.canvas_id][4], -1);
		break;
	default:
		break;
	}
	di_post_stru.next_canvas_id = di_post_stru.canvas_id ? 0 : 1;
#endif
	if (di_buf->di_buf_dup_p[1] == NULL)
		return 0;
	if ((di_buf->di_buf_dup_p[1]->vframe == NULL) ||
		di_buf->di_buf_dup_p[0]->vframe == NULL)
		return 0;

	if (is_meson_txl_cpu() && overturn && di_buf->di_buf_dup_p[2]) {
		if (post_blend == PULL_DOWN_BLEND_2)
			post_blend = PULL_DOWN_BLEND_0;
		else if (post_blend == PULL_DOWN_BLEND_0)
			post_blend = PULL_DOWN_BLEND_2;
	}

	switch (post_blend) {
	case PULL_DOWN_BLEND_0:
	case PULL_DOWN_NORMAL:
		config_fftffb_mode(di_buf, &post_field_num,
			(di_buf->di_buf_dup_p[tbbtff_dly]->privated&0x3));
		if (di_buf->pulldown_mode == PULL_DOWN_NORMAL) {
			post_blend_mode = 3;
			mc_pre_flag = is_meson_txl_cpu()?1:(overturn?0:1);
		} else {
			post_blend_mode = 1;
			mc_pre_flag = is_meson_txl_cpu()?0:(overturn?0:1);
		}
		if (mcpre_en) {
			di_post_stru.di_mcvecrd_mif.canvas_num =
				di_buf->di_buf_dup_p[2]->mcvec_canvas_idx;
		}
		blend_mtn_en = 1;
		post_ei = ei_en = 1;
		post_blend_en = 1;
		break;
	case PULL_DOWN_BLEND_2:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		di_post_stru.di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		di_post_stru.di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		di_post_stru.di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		if (is_meson_txl_cpu() && overturn) {
			di_post_stru.di_buf1_mif.canvas0_addr0 =
			di_post_stru.di_buf2_mif.canvas0_addr0;
		}
		if (mcpre_en) {
			di_post_stru.di_mcvecrd_mif.canvas_num =
				di_buf->di_buf_dup_p[2]->mcvec_canvas_idx;
			mc_pre_flag = is_meson_txl_cpu()?0:(overturn?1:0);
			if (!overturn)
				di_post_stru.di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		}
		if ((post_wr_en && post_wr_surpport))
			di_post_stru.di_diwr_mif.canvas_num =
				di_post_idx[di_post_stru.canvas_id][4];

		post_blend_mode = 1;
		blend_mtn_en = 1;
		post_ei = ei_en = 1;
		post_blend_en = 1;
		break;
	case PULL_DOWN_MTN:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		di_post_stru.di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		di_post_stru.di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		if ((post_wr_en && post_wr_surpport))
			di_post_stru.di_diwr_mif.canvas_num =
				di_post_idx[di_post_stru.canvas_id][4];
		post_blend_mode = 0;
		blend_mtn_en = 1;
		post_ei = ei_en = 1;
		post_blend_en = 1;
		break;
	case PULL_DOWN_BUF1:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		di_post_stru.di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		di_post_stru.di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[1]->mtn_canvas_idx;
		di_post_stru.di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		if ((post_wr_en && post_wr_surpport))
			di_post_stru.di_diwr_mif.canvas_num =
				di_post_idx[di_post_stru.canvas_id][4];
		post_blend_mode = 1;
		blend_mtn_en = 0;
		post_ei = ei_en = 0;
		post_blend_en = 0;
		break;
	case PULL_DOWN_EI:
		if (di_buf->di_buf_dup_p[1]) {
			di_post_stru.di_buf0_mif.canvas0_addr0 =
				di_buf->di_buf_dup_p[1]->nr_canvas_idx;
			post_field_num =
				(di_buf->di_buf_dup_p[1]->vframe->type &
				 VIDTYPE_TYPEMASK)
				== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		} else {
			post_field_num =
				(di_buf->di_buf_dup_p[0]->vframe->type &
				 VIDTYPE_TYPEMASK)
				== VIDTYPE_INTERLACE_TOP ? 0 : 1;
			di_post_stru.di_buf0_mif.src_field_mode
				= post_field_num;
		}
		if ((post_wr_en && post_wr_surpport))
			di_post_stru.di_diwr_mif.canvas_num =
				di_post_idx[di_post_stru.canvas_id][4];
		post_blend_mode = 2;
		blend_mtn_en = 0;
		post_ei = ei_en = 1;
		post_blend_en = 0;
		break;
	default:
		break;
	}

	if ((post_wr_en && post_wr_surpport)) {
		di_vpp_en = 0;
		di_ddr_en = 1;
	} else {
		di_vpp_en = 1;
		di_ddr_en = 0;
	}

	/* if post size < MIN_POST_WIDTH, force ei */
	if ((di_width < MIN_BLEND_WIDTH) &&
		(di_buf->pulldown_mode == PULL_DOWN_BLEND_0 ||
		di_buf->pulldown_mode == PULL_DOWN_BLEND_2 ||
		di_buf->pulldown_mode == PULL_DOWN_NORMAL
		)) {
		post_blend_mode = 1;
		blend_mtn_en = 0;
		post_ei = ei_en = 0;
		post_blend_en = 0;
	}

	if (mcpre_en)
		di_post_stru.di_mcvecrd_mif.blend_en = post_blend_en;

	if ((di_post_stru.update_post_reg_flag) &&
	    ((force_update_post_reg & 0x80) == 0)) {
		enable_di_post_2(
			&di_post_stru.di_buf0_mif,
			&di_post_stru.di_buf1_mif,
			&di_post_stru.di_buf2_mif,
			(di_ddr_en ?
				(&di_post_stru.di_diwr_mif):NULL),
			&di_post_stru.di_mtnprd_mif,
			ei_en,                  /* ei enable */
			post_blend_en,          /* blend enable */
			blend_mtn_en,           /* blend mtn enable */
			post_blend_mode,        /* blend mode. */
			di_vpp_en,		/* di_vpp_en. */
			di_ddr_en,		/* di_ddr_en. */
			post_field_num,         /* 1 bottom generate top */
			hold_line,
			post_urgent
			);
		if (mcpre_en)
			enable_mc_di_post(
				&di_post_stru.di_mcvecrd_mif, post_urgent,
				overturn);
		else if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu())
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 2);
	} else {
		di_post_switch_buffer(
			&di_post_stru.di_buf0_mif,
			&di_post_stru.di_buf1_mif,
			&di_post_stru.di_buf2_mif,
(di_ddr_en ? (&di_post_stru.di_diwr_mif):NULL),
			&di_post_stru.di_mtnprd_mif,
			&di_post_stru.di_mcvecrd_mif,
			ei_en,                  /* ei enable */
			post_blend_en,          /* blend enable */
			blend_mtn_en,           /* blend mtn enable */
			post_blend_mode,        /* blend mode. */
			di_vpp_en,	/* di_vpp_en. */
			di_ddr_en,	/* di_ddr_en. */
			post_field_num,         /* 1 bottom generate top */
			hold_line,
			post_urgent
			);
	}

#ifdef NEW_DI_V1
	if (di_post_stru.update_post_reg_flag && (!combing_fix_en)) {
		di_apply_reg_cfg(1);
		last_lev = -1;
	}

#endif
	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu())
		di_post_read_reverse_irq(overturn, mc_pre_flag);
	if (mcpre_en) {
		if (di_buf->di_buf_dup_p[2])
			set_post_mcinfo(&di_buf->di_buf_dup_p[2]
				->curr_field_mcinfo);
	} else if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu())
			DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 2);


/* set pull down region (f(t-1) */

	if (di_pldn_buf && pulldown_enable &&
		!di_pre_stru.cur_prog_flag) {
		unsigned short offset = (di_start_y>>1);
		if (overturn)
			offset = ((di_buf->vframe->height - di_end_y)>>1);
		recalc_pd_wnd(di_pldn_buf, offset);
		if (pldn_wnd_flsh == 1) {
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG0_Y,
				di_pldn_buf->reg0_s, 17, 12);
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG0_Y,
				di_pldn_buf->reg0_e, 1, 12);

			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG1_Y,
				di_pldn_buf->reg1_s, 17, 12);
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG1_Y,
				di_pldn_buf->reg1_e, 1, 12);

			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG2_Y,
				di_pldn_buf->reg2_s, 17, 12);
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG2_Y,
				di_pldn_buf->reg2_e, 1, 12);

			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG3_Y,
				di_pldn_buf->reg3_s, 17, 12);
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_REG3_Y,
				di_pldn_buf->reg3_e, 1, 12);

/* region0 */
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
				(di_pldn_buf->reg0_e > di_pldn_buf->reg0_s)
				? 1 : 0, 16, 1);
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
				di_pldn_buf->reg0_bmode, 8, 2);

			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
				(di_pldn_buf->reg1_e > di_pldn_buf->reg1_s)
				? 1 : 0, 17, 1);
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
				di_pldn_buf->reg1_bmode, 10, 2);

			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
				(di_pldn_buf->reg2_e > di_pldn_buf->reg2_s)
				? 1 : 0, 18, 1);
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
				di_pldn_buf->reg2_bmode, 12, 2);

			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
				(di_pldn_buf->reg3_e > di_pldn_buf->reg3_s)
				? 1 : 0, 19, 1);
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL,
				di_pldn_buf->reg3_bmode, 14, 2);
		} else if (pldn_wnd_flsh == 2) {
			DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG0_Y, 479);
			DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG1_Y, 160);
			DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG2_Y, 320 << 16 | 160);
			DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG3_Y, 479 << 16 | 320);
			RDMA_WR(DI_BLEND_CTRL, 0x81f00019); /* normal */
		}

		if (pldn_pst_set == 1) {
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL, 1, 17, 1);
/* weaver */
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL, 1, 10, 2);
			DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG1_Y, 120);
		} else if (pldn_pst_set == 2) {
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL, 1, 18, 1);
/* bob */
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL, 2, 12, 2);
			DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG2_Y, 120);
		} else if (pldn_pst_set == 3) {
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL, 1, 19, 1);
/* bob mtn */
			DI_VSYNC_WR_MPEG_REG_BITS(DI_BLEND_CTRL, 0, 14, 2);
			DI_VSYNC_WR_MPEG_REG(DI_BLEND_REG3_Y, 120);
		} else if (pldn_pst_set == 5) {
			RDMA_WR(DI_BLEND_CTRL, 0x81f00019); /* normal */
		}
	}

	if (di_post_stru.update_post_reg_flag > 0)
		di_post_stru.update_post_reg_flag--;
	return 0;
}


static void post_de_done_buf_config(void)
{
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;
	struct di_buf_s *di_buf = NULL;
	if (di_post_stru.cur_post_buf == NULL)
		return;

	di_lock_irqfiq_save(irq_flag2, fiq_flag);
	queue_out(di_post_stru.cur_post_buf);
	di_buf = di_post_stru.cur_post_buf;
	queue_in(di_post_stru.cur_post_buf, QUEUE_POST_READY);
	di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
	vf_notify_receiver(VFM_NAME, VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	di_post_stru.cur_post_buf = NULL;

}

static void di_post_process(void)
{
	struct di_buf_s *di_buf = NULL;
	vframe_t *vf_p = NULL;


	if (!queue_empty(QUEUE_POST_DOING) && !di_post_stru.post_de_busy) {
		di_buf = get_di_buf_head(QUEUE_POST_DOING);
		if (check_di_buf(di_buf, 20))
			return;
		vf_p = di_buf->vframe;
		if (di_post_stru.run_early_proc_fun_flag) {
			if (vf_p->early_process_fun)
				vf_p->early_process_fun(
					vf_p->private_data, vf_p);
		}
		if (di_buf->process_fun_index)
			de_post_process(
				di_buf, 0, vf_p->width-1,
				0, vf_p->height-1, vf_p);
		di_post_stru.post_de_busy = 1;
		di_post_stru.cur_post_buf = di_buf;
	}
}

int pd_detect_rst;

static void recycle_vframe_type_post(struct di_buf_s *di_buf)
{
	int i;

	if (di_buf == NULL) {
		pr_dbg("%s:Error\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 15;

		recovery_flag++;
		return;
	}
	if (di_buf->process_fun_index == PROCESS_FUN_DI)
		dec_post_ref_count(di_buf);

	for (i = 0; i < 2; i++) {
		if (di_buf->di_buf[i])
			queue_in(di_buf->di_buf[i], QUEUE_RECYCLE);
	}
	queue_out(di_buf); /* remove it from display_list_head */
	di_buf->invert_top_bot_flag = 0;
	queue_in(di_buf, QUEUE_POST_FREE);
}

#ifdef DI_BUFFER_DEBUG
static void
recycle_vframe_type_post_print(struct di_buf_s *di_buf,
			       const char *func,
			       const int	line)
{
	int i;

	di_print("%s:%d ", func, line);
	for (i = 0; i < 2; i++) {
		if (di_buf->di_buf[i])
			di_print("%s[%d]<%d>=>recycle_list; ",
				vframe_type_name[di_buf->di_buf[i]->type],
				di_buf->di_buf[i]->index, i);
	}
	di_print("%s[%d] =>post_free_list\n",
		vframe_type_name[di_buf->type], di_buf->index);
}
#endif

void check_pulldown_mode(
	int pulldown_type, int pulldown_mode2,
	int *win_pd_type,  struct di_buf_s *di_buf)
{
	int ii;
	unsigned mode, fdiff_num, dup1_wp_fdiff,
		 dup1_wc_fdiff, dup1_wn_fdiff, dup2_wp_fdiff,
		 dup2_wc_fdiff, dup2_wn_fdiff;
	if ((pulldown_win_mode & 0xfffff) != 0) {
		for (ii = 0; ii < 5; ii++) {
			mode = (pulldown_win_mode >> (ii * 4)) & 0xf;
			if (mode == 1) {
				if (di_buf->di_buf_dup_p[1]->
					pulldown_mode == 0) {
					fdiff_num =
						di_buf->di_buf_dup_p[1]->
						win_pd_info[ii].field_diff_num;
					if ((fdiff_num * win_pd_th[ii].
					field_diff_num_th)
					>= pd_win_prop[ii].pixels_num)
						break;
				} else {
					fdiff_num =
						di_buf->di_buf_dup_p[2]->
						win_pd_info[ii].field_diff_num;
					if ((fdiff_num * win_pd_th[ii].
					field_diff_num_th)
					>= pd_win_prop[ii].pixels_num)
						break;
				}
				dup1_wn_fdiff =
					di_buf->di_buf_dup_p[1]->
					win_pd_info[ii + 1].field_diff_num;
				dup1_wc_fdiff =
					di_buf->di_buf_dup_p[1]->
					win_pd_info[ii].field_diff_num;
				dup1_wp_fdiff =
					di_buf->di_buf_dup_p[1]->
					win_pd_info[ii - 1].field_diff_num;
				dup2_wc_fdiff =
					di_buf->di_buf_dup_p[2]->
					win_pd_info[ii].field_diff_num;
				dup2_wn_fdiff =
					di_buf->di_buf_dup_p[2]->
					win_pd_info[ii + 1].field_diff_num;
				dup2_wp_fdiff =
					di_buf->di_buf_dup_p[2]->
					win_pd_info[ii - 1].field_diff_num;
				if ((ii != 0) && (ii != 5) && (ii != 4) &&
				(pulldown_mode2 == 1) &&
				(((dup2_wn_fdiff * 100) < dup1_wn_fdiff) &&
				((dup2_wp_fdiff * 100) < dup1_wp_fdiff) &&
				((dup2_wc_fdiff * 100) >= dup1_wc_fdiff))) {
					di_print(
						"out %x %06x %06x\n",
						ii,
						di_buf->di_buf_dup_p[2]->
						win_pd_info[ii].field_diff_num,
						di_buf->di_buf_dup_p[1]->
						win_pd_info[ii].field_diff_num);
					pd_detect_rst = 0;
					break;
				}
				if ((ii != 0) && (ii != 5) && (ii != 4) &&
				(pulldown_mode2 == 0) &&
				((dup1_wn_fdiff * 100) < dup1_wc_fdiff) &&
				((dup1_wp_fdiff * 100) < dup1_wc_fdiff)) {
					pd_detect_rst = 0;
					break;
				}
			} else if (mode == 2) {
				if ((pulldown_type == 0) &&
				(di_buf->di_buf_dup_p[1]->
				win_pd_info[ii].field_diff_num_pattern
				!= di_buf->di_buf_dup_p[1]->
				field_pd_info.field_diff_num_pattern))
					break;
				if ((pulldown_type == 1) &&
				(di_buf->di_buf_dup_p[1]->
				win_pd_info[ii].frame_diff_num_pattern
				!= di_buf->di_buf_dup_p[1]->
				field_pd_info.frame_diff_num_pattern))
					break;
			} else if (mode == 3) {
				if ((di_buf->di_buf_dup_p[1]->win_pd_mode[ii]
				!= di_buf->di_buf_dup_p[1]->pulldown_mode)
				|| (pulldown_type != win_pd_type[ii]))
					break;
			}
		}
		if (ii < 5)
			di_buf->pulldown_mode = -1;
	}
}

static int pulldown_process(struct di_buf_s *di_buf, int buffer_count)
{
	int pulldown_type = -1; /* 0, 2:2; 1, m:n */
	int win_pd_type[5] = { -1, -1, -1, -1, -1 };
	int ii;
	int pulldown_mode2;
	int pulldown_mode_ret;

	insert_pd_his(&di_buf->di_buf_dup_p[1]->field_pd_info);
	if (buffer_count == 3) {
		/* 3 buffers */
		cal_pd_parameters(&di_buf->di_buf_dup_p[1]->field_pd_info,
			&di_buf->di_buf_dup_p[0]->field_pd_info,
			&di_buf->di_buf_dup_p[2]->field_pd_info);
		/* cal parameters of di_buf_dup_p[1] */
		pattern_check_pre_2(0, &di_buf->di_buf_dup_p[2]->field_pd_info,
			&di_buf->di_buf_dup_p[1]->field_pd_info,
			&di_buf->di_buf_dup_p[0]->field_pd_info,
			(int *)&di_buf->di_buf_dup_p[1]->pulldown_mode,
			NULL, &pulldown_type, &field_pd_th);

		for (ii = 0; ii < MAX_WIN_NUM; ii++) {
			cal_pd_parameters(
				&di_buf->di_buf_dup_p[1]->win_pd_info[ii],
				&di_buf->di_buf_dup_p[0]->win_pd_info[ii],
				&di_buf->di_buf_dup_p[2]->win_pd_info[ii]);
/* cal parameters of di_buf_dup_p[1] */
			pattern_check_pre_2(ii + 1,
				&di_buf->di_buf_dup_p[2]->win_pd_info[ii],
				&di_buf->di_buf_dup_p[1]->win_pd_info[ii],
				&di_buf->di_buf_dup_p[0]->win_pd_info[ii],
				&(di_buf->di_buf_dup_p[1]->win_pd_mode[ii]),
				NULL, &(win_pd_type[ii]), &win_pd_th[ii]
				);
		}
	}
	pulldown_mode_ret = pulldown_mode2 = detect_pd32();

	if (di_log_flag & DI_LOG_PULLDOWN) {
		struct di_buf_s *dp = di_buf->di_buf_dup_p[1];
		di_print(
"%02d (%x%x%x) %08x %06x %08x %06x %02x %02x %02x %02x %02x %02x %02x %02x ",
			dp->seq % 100,
			dp->pulldown_mode < 0 ? 0xf : dp->pulldown_mode,
			pulldown_type < 0 ? 0xf : pulldown_type,
			pulldown_mode2 < 0 ? 0xf : pulldown_mode2,
			dp->field_pd_info.frame_diff,
			dp->field_pd_info.frame_diff_num,
			dp->field_pd_info.field_diff,
			dp->field_pd_info.field_diff_num,
			dp->field_pd_info.frame_diff_by_pre,
			dp->field_pd_info.frame_diff_num_by_pre,
			dp->field_pd_info.field_diff_by_pre,
			dp->field_pd_info.field_diff_num_by_pre,
			dp->field_pd_info.field_diff_by_next,
			dp->field_pd_info.field_diff_num_by_next,
			dp->field_pd_info.frame_diff_skew_ratio,
			dp->field_pd_info.frame_diff_num_skew_ratio);
		for (ii = 0; ii < MAX_WIN_NUM; ii++) {
			di_print(
	"(%x,%x) %08x %06x %08x %06x %02x %02x %02x %02x %02x %02x %02x %02x ",
				(dp->win_pd_mode[ii] < 0) ?
					0xf : dp->win_pd_mode[ii],
				win_pd_type[ii] < 0 ? 0xf : win_pd_type[ii],
				dp->win_pd_info[ii].frame_diff,
				dp->win_pd_info[ii].frame_diff_num,
				dp->win_pd_info[ii].field_diff,
				dp->win_pd_info[ii].field_diff_num,
				dp->win_pd_info[ii].frame_diff_by_pre,
				dp->win_pd_info[ii].frame_diff_num_by_pre,
				dp->win_pd_info[ii].field_diff_by_pre,
				dp->win_pd_info[ii].field_diff_num_by_pre,
				dp->win_pd_info[ii].field_diff_by_next,
				dp->win_pd_info[ii].field_diff_num_by_next,
				dp->win_pd_info[ii].frame_diff_skew_ratio,
				dp->win_pd_info[ii].frame_diff_num_skew_ratio);
		}
		di_print("\n");
	}

	di_buf->pulldown_mode = -1;
	if (pulldown_detect) {
		if (pulldown_detect & 0x1)
			di_buf->pulldown_mode =
				di_buf->di_buf_dup_p[1]->pulldown_mode;
		/* used by de_post_process */
		if (pulldown_detect & 0x10) {
			if ((pulldown_mode2 >= 0) && (pd_detect_rst > 15))
				di_buf->pulldown_mode = pulldown_mode2;
		}
		if (pd_detect_rst <= 32)
			pd_detect_rst++;

		check_pulldown_mode(pulldown_type, pulldown_mode2, win_pd_type,
			di_buf);
		if (di_buf->pulldown_mode != -1)
			pulldown_count++;

#if defined(NEW_DI_TV)
		if (di_buf->vframe->source_type == VFRAME_SOURCE_TYPE_TUNER)
			di_buf->pulldown_mode = -1;
		/* pr_dbg("2:2 ignore\n"); */

#endif
	}
	return pulldown_mode_ret;
}


static int pulldown_mode;
static int debug_blend_mode = -1;

static unsigned int pldn_dly1 = 1;

static unsigned int pldn_pst_wver = 5;
module_param(pldn_pst_wver, uint, 0644);
MODULE_PARM_DESC(pldn_pst_wver, "/n pulldonw post-weaver for test./n");

int set_pulldown_mode(int buffer_keep_count, struct di_buf_s *di_buf)
{
	int pulldown_mode_hise = 0;
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		if (pulldown_enable && !di_pre_stru.cur_prog_flag)
			di_buf->pulldown_mode =
				di_buf->di_buf_dup_p[pldn_dly1]->pulldown_mode;
		else
			di_buf->pulldown_mode =	PULL_DOWN_NORMAL;
		if (pldn_pst_wver != 5) {
			if (pldn_pst_wver > 5)
				pldn_pst_wver = 5;
			di_buf->pulldown_mode = pldn_pst_wver;
		}
	} else {
		if (pulldown_mode & 1) {
			pulldown_mode_hise =
				pulldown_process(di_buf, buffer_keep_count);
			if (di_buf->pulldown_mode == -1)
				di_buf->pulldown_mode = PULL_DOWN_NORMAL;
			else if (di_buf->pulldown_mode == 0)
				di_buf->pulldown_mode = PULL_DOWN_BLEND_0;
			else if (di_buf->pulldown_mode == 1)
				di_buf->pulldown_mode = PULL_DOWN_BLEND_2;
		} else {
			di_buf->pulldown_mode =
				di_buf->di_buf_dup_p[1]->pulldown_mode;
		}
	}
	return pulldown_mode_hise;
}

void drop_frame(int check_drop, int throw_flag, struct di_buf_s *di_buf)
{
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;
	int i = 0, drop_flag = 0;
	di_lock_irqfiq_save(irq_flag2, fiq_flag);
	if ((frame_count == 0) && check_drop)
		di_post_stru.start_pts = di_buf->vframe->pts;
		di_post_stru.start_pts_us64 = di_buf->vframe->pts_us64;
	if ((check_drop && (frame_count < start_frame_drop_count))
	|| throw_flag) {
		drop_flag = 1;
	} else {
		if (check_drop && (frame_count == start_frame_drop_count)) {
			if ((di_post_stru.start_pts)
			&& (di_buf->vframe->pts == 0))
				di_buf->vframe->pts = di_post_stru.start_pts;
                                di_buf->vframe->pts_us64 = di_post_stru.start_pts_us64;
			di_post_stru.start_pts = 0;
		}
		for (i = 0; i < 3; i++) {
			if (di_buf->di_buf_dup_p[i]) {
				if (di_buf->di_buf_dup_p[i]->vframe->bitdepth !=
					di_buf->vframe->bitdepth) {
					pr_info("%s buf[%d] not match bit mode\n",
						__func__, i);
					drop_flag = 1;
					break;
				}
			}
		}
	}
	if (drop_flag) {
		queue_in(di_buf, QUEUE_TMP);
		recycle_vframe_type_post(di_buf);
#ifdef DI_BUFFER_DEBUG
		recycle_vframe_type_post_print(
				di_buf, __func__,
				__LINE__);
#endif
	} else {
		if ((post_wr_en && post_wr_surpport))
			queue_in(di_buf, QUEUE_POST_DOING);
		else
			queue_in(di_buf, QUEUE_POST_READY);
		di_print("DI:%s[%d] => post ready %u ms.\n",
		vframe_type_name[di_buf->type], di_buf->index,
		jiffies_to_msecs(jiffies_64 - di_buf->vframe->ready_jiffies64));


	}
	di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
}

static int process_post_vframe(void)
{
/*
 * 1) get buf from post_free_list, config it according to buf
 * in pre_ready_list, send it to post_ready_list
 * (it will be send to post_free_list in di_vf_put())
 * 2) get buf from pre_ready_list, attach it to buf from post_free_list
 * (it will be send to recycle_list in di_vf_put() )
 */
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;
	int i = 0;
	int pulldown_mode_hise = 0;
	int ret = 0;
	int buffer_keep_count = 3;
	struct di_buf_s *di_buf = NULL;
	struct di_buf_s *ready_di_buf;
	struct di_buf_s *p = NULL;/* , *ptmp; */
	int itmp;
	int ready_count = list_count(QUEUE_PRE_READY);
	bool check_drop = false;

	if (queue_empty(QUEUE_POST_FREE))
		return 0;

	if (ready_count == 0)
		return 0;

	ready_di_buf = get_di_buf_head(QUEUE_PRE_READY);
	if ((ready_di_buf == NULL) || (ready_di_buf->vframe == NULL)) {

		pr_dbg("%s:Error1\n", __func__);

		if (recovery_flag == 0)
			recovery_log_reason = 16;

		recovery_flag++;
		return 0;
	}

	if ((ready_di_buf->post_proc_flag) &&
	    (ready_count >= buffer_keep_count)) {
		i = 0;
		queue_for_each_entry(p, ptmp, QUEUE_PRE_READY, list) {
			/* if(p->post_proc_flag == 0){ */
			if (p->type == VFRAME_TYPE_IN) {
				ready_di_buf->post_proc_flag = -1;
				ready_di_buf->new_format_flag = 1;
			}
			i++;
			if (i > 2)
				break;
		}
	}
	if (ready_di_buf->post_proc_flag > 0) {
		if (ready_count >= buffer_keep_count) {
			di_lock_irqfiq_save(irq_flag2, fiq_flag);
			di_buf = get_di_buf_head(QUEUE_POST_FREE);
			if (check_di_buf(di_buf, 17))
				return 0;

			queue_out(di_buf);
			di_unlock_irqfiq_restore(irq_flag2, fiq_flag);

			i = 0;
			queue_for_each_entry(
				p, ptmp, QUEUE_PRE_READY, list) {
				di_buf->di_buf_dup_p[i++] = p;
				if (i >= buffer_keep_count)
					break;
			}
			if (i < buffer_keep_count) {

				pr_dbg("%s:Error3\n", __func__);

				if (recovery_flag == 0)
					recovery_log_reason = 18;
				recovery_flag++;
				return 0;
			}

			memcpy(di_buf->vframe,
				di_buf->di_buf_dup_p[1]->vframe,
				sizeof(vframe_t));
			di_buf->vframe->private_data = di_buf;
			if (di_buf->di_buf_dup_p[1]->post_proc_flag == 3) {
				/* dummy, not for display */
				inc_post_ref_count(di_buf);
				di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
				di_buf->di_buf[1] = NULL;
				queue_out(di_buf->di_buf[0]);
				di_lock_irqfiq_save(irq_flag2, fiq_flag);
				queue_in(di_buf, QUEUE_TMP);
				recycle_vframe_type_post(di_buf);

				di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
#ifdef DI_BUFFER_DEBUG
				di_print("%s <dummy>: ", __func__);
#endif
			} else {
				if (di_buf->di_buf_dup_p[1]->
				post_proc_flag == 2){
					reset_pulldown_state();
					di_buf->pulldown_mode
						= PULL_DOWN_BLEND_2;
					/* blend with di_buf->di_buf_dup_p[2] */
				} else {
					pulldown_mode_hise = set_pulldown_mode(
						buffer_keep_count,
						di_buf);
				}
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD;
				if (
					di_buf->di_buf_dup_p[1]->
					new_format_flag) {
					/* if (di_buf->di_buf_dup_p[1]
					 * ->post_proc_flag == 2) { */
					di_buf->vframe->
					early_process_fun
						= de_post_disable_fun;
				} else {
					di_buf->vframe->
					early_process_fun
						= do_nothing_fun;
				}

				if (di_buf->di_buf_dup_p[1]->type
				    == VFRAME_TYPE_IN) {
					/* next will be bypass */
					di_buf->vframe->type
						= VIDTYPE_PROGRESSIVE |
						  VIDTYPE_VIU_422 |
						  VIDTYPE_VIU_SINGLE_PLANE
						  | VIDTYPE_VIU_FIELD;
					di_buf->vframe->height >>= 1;
					di_buf->vframe->canvas0Addr =
						di_buf->di_buf_dup_p[0]
						->nr_canvas_idx; /* top */
					di_buf->vframe->canvas1Addr =
						di_buf->di_buf_dup_p[0]
						->nr_canvas_idx;
					di_buf->vframe->process_fun =
						NULL;
					di_buf->process_fun_index =
						PROCESS_FUN_NULL;
				} else {
					/*for debug*/
					if (debug_blend_mode != -1)
						di_buf->pulldown_mode =
							debug_blend_mode;

					di_buf->vframe->process_fun =
((post_wr_en && post_wr_surpport) ? NULL : de_post_process);
					di_buf->process_fun_index =
						PROCESS_FUN_DI;
					inc_post_ref_count(di_buf);
				}
				di_buf->di_buf[0]
					= di_buf->di_buf_dup_p[0];
				di_buf->di_buf[1] = NULL;
				queue_out(di_buf->di_buf[0]);

				drop_frame(true,
					(di_buf->di_buf_dup_p[0]->throw_flag) ||
					(di_buf->di_buf_dup_p[1]->throw_flag) ||
					(di_buf->di_buf_dup_p[2]->throw_flag),
					di_buf);

				frame_count++;
#ifdef DI_BUFFER_DEBUG
				di_print("%s <interlace>: ", __func__);
#endif
				if (!(post_wr_en && post_wr_surpport))
					vf_notify_receiver(VFM_NAME,
VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
			}
			ret = 1;
		}
	} else {
		if (is_progressive(ready_di_buf->vframe) ||
		    ready_di_buf->type == VFRAME_TYPE_IN ||
		    ready_di_buf->post_proc_flag < 0 ||
		    bypass_post_state
		    ){
			int vframe_process_count = 1;
#ifdef DET3D
			int dual_vframe_flag = 0;
			if ((di_pre_stru.vframe_interleave_flag &&
			     ready_di_buf->left_right) ||
			    (bypass_post & 0x100)) {
				dual_vframe_flag = 1;
				vframe_process_count = 2;
			}
#endif
			if (skip_top_bot &&
			    (!is_progressive(ready_di_buf->vframe)))
				vframe_process_count = 2;

			if (ready_count >= vframe_process_count) {
				struct di_buf_s *di_buf_i;
				di_lock_irqfiq_save(irq_flag2, fiq_flag);
				di_buf = get_di_buf_head(QUEUE_POST_FREE);
				if (check_di_buf(di_buf, 19))
					return 0;

				queue_out(di_buf);
				di_unlock_irqfiq_restore(irq_flag2, fiq_flag);

				i = 0;
				queue_for_each_entry(
					p, ptmp, QUEUE_PRE_READY, list){
					di_buf->di_buf_dup_p[i++] = p;
					if (i >= vframe_process_count) {
						di_buf->di_buf_dup_p[i] = NULL;
						di_buf->di_buf_dup_p[i+1] =
							NULL;
						break;
					}
				}
				if (i < vframe_process_count) {
					pr_dbg("%s:Error6\n", __func__);
					if (recovery_flag == 0)
						recovery_log_reason = 22;

					recovery_flag++;
					return 0;
				}

				di_buf_i = di_buf->di_buf_dup_p[0];
				if (!is_progressive(ready_di_buf->vframe)
					&& ((skip_top_bot == 1)
					|| (skip_top_bot == 2))) {
					unsigned frame_type =
						di_buf->di_buf_dup_p[1]->
						vframe->type & VIDTYPE_TYPEMASK;
					if (skip_top_bot == 1) {
						di_buf_i = (frame_type ==
						VIDTYPE_INTERLACE_TOP)
						? di_buf->di_buf_dup_p[1]
						: di_buf->di_buf_dup_p[0];
					} else if (skip_top_bot == 2) {
						di_buf_i = (frame_type ==
						VIDTYPE_INTERLACE_BOTTOM)
						? di_buf->di_buf_dup_p[1]
						: di_buf->di_buf_dup_p[0];
					}
				}

				memcpy(di_buf->vframe,
					di_buf_i->vframe,
					sizeof(vframe_t));
				di_buf->vframe->private_data = di_buf;

				if (ready_di_buf->new_format_flag &&
				(ready_di_buf->type == VFRAME_TYPE_IN)) {
					pr_info("DI:%d disable post.\n",
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
				if (ready_di_buf->post_proc_flag == -2) {
					di_buf->vframe->type
						|= VIDTYPE_VIU_FIELD;
					di_buf->vframe->type
						&= ~(VIDTYPE_TYPEMASK);
					di_buf->vframe->process_fun
= (post_wr_en && post_wr_surpport)?NULL:de_post_process;
					di_buf->process_fun_index
						= PROCESS_FUN_DI;
					di_buf->pulldown_mode
						= PULL_DOWN_EI;
				} else {
					di_buf->vframe->process_fun =
						NULL;
					di_buf->process_fun_index =
						PROCESS_FUN_NULL;
					di_buf->pulldown_mode =
						PULL_DOWN_NORMAL;
				}
				di_buf->di_buf[0] = ready_di_buf;
				di_buf->di_buf[1] = NULL;
				queue_out(ready_di_buf);

#ifdef DET3D
				if (dual_vframe_flag) {
					di_buf->di_buf[1] =
						di_buf->di_buf_dup_p[1];
					queue_out(di_buf->di_buf[1]);
				}
#endif
				if ((check_start_drop_prog &&
				     is_progressive(ready_di_buf->vframe))
				    || !is_progressive(ready_di_buf->vframe))
					check_drop = true;
				drop_frame(check_drop,
					di_buf->di_buf[0]->throw_flag, di_buf);

				frame_count++;
#ifdef DI_BUFFER_DEBUG
				di_print(
					"%s <prog by frame>: ",
					__func__);
#endif
				ret = 1;
				vf_notify_receiver(VFM_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
			}
		} else if (ready_count >= 2) {
			/*for progressive input,type
			 * 1:separate tow fields,type
			 * 2:bypass post as frame*/
			unsigned char prog_tb_field_proc_type =
				(prog_proc_config >> 1) & 0x3;
			di_lock_irqfiq_save(irq_flag2, fiq_flag);
			di_buf = get_di_buf_head(QUEUE_POST_FREE);

			if (check_di_buf(di_buf, 20))
				return 0;

			queue_out(di_buf);
			di_unlock_irqfiq_restore(irq_flag2, fiq_flag);

			i = 0;
			queue_for_each_entry(
				p, ptmp, QUEUE_PRE_READY, list) {
				di_buf->di_buf_dup_p[i++] = p;
				if (i >= 2) {
					di_buf->di_buf_dup_p[i] = NULL;
					break;
				}
			}
			if (i < 2) {
				pr_dbg("%s:Error6\n", __func__);

				if (recovery_flag == 0)
					recovery_log_reason = 21;

				recovery_flag++;
				return 0;
			}

			memcpy(di_buf->vframe,
				di_buf->di_buf_dup_p[0]->vframe,
				sizeof(vframe_t));
			di_buf->vframe->private_data = di_buf;

			/*separate one progressive frame
			 * as two interlace fields */
			if (prog_tb_field_proc_type == 1) {
				/* do weave by di post */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD;
				if (
					di_buf->di_buf_dup_p[0]->
					new_format_flag)
					di_buf->vframe->
					early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->
					early_process_fun =
						do_nothing_fun;

				di_buf->pulldown_mode = PULL_DOWN_BUF1;
				di_buf->vframe->process_fun =
(post_wr_en && post_wr_surpport)?NULL:de_post_process;
				di_buf->process_fun_index =
					PROCESS_FUN_DI;
			} else if (prog_tb_field_proc_type == 0) {
				/* to do: need change for
				 * DI_USE_FIXED_CANVAS_IDX */
				/* do weave by vpp */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE;
				if (
					(di_buf->di_buf_dup_p[0]->
					 new_format_flag) ||
					(Rd(DI_IF1_GEN_REG) & 1))
					di_buf->vframe->
					early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->
					early_process_fun =
						do_nothing_fun;
				di_buf->vframe->process_fun = NULL;
				di_buf->process_fun_index =
					PROCESS_FUN_NULL;
				di_buf->vframe->canvas0Addr =
					di_buf->di_buf_dup_p[0]->
					nr_canvas_idx;
				di_buf->vframe->canvas1Addr =
					di_buf->di_buf_dup_p[1]->
					nr_canvas_idx;
			} else {
				/* to do: need change for
				 * DI_USE_FIXED_CANVAS_IDX */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD;
				di_buf->vframe->height >>= 1;
				if (
					(di_buf->di_buf_dup_p[0]->
					 new_format_flag) ||
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
						di_buf->di_buf_dup_p[0]
						->nr_canvas_idx;
/* top */
					di_buf->vframe->canvas1Addr =
						di_buf->di_buf_dup_p[0]
						->nr_canvas_idx;
				} else {
					di_buf->vframe->canvas0Addr =
						di_buf->di_buf_dup_p[1]
						->nr_canvas_idx; /* top */
					di_buf->vframe->canvas1Addr =
						di_buf->di_buf_dup_p[1]
						->nr_canvas_idx;
				}
			}

			di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
			queue_out(di_buf->di_buf[0]);
			/*check if the field is error,then drop*/
			if (
				(di_buf->di_buf_dup_p[0]->vframe->type &
				 VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_BOTTOM) {
				di_buf->di_buf[1] =
					di_buf->di_buf_dup_p[1] = NULL;
				di_lock_irqfiq_save(irq_flag2,
					fiq_flag);
				queue_in(di_buf, QUEUE_TMP);
				recycle_vframe_type_post(di_buf);
				pr_dbg("%s drop field %d.\n", __func__,
					di_buf->di_buf_dup_p[0]->seq);
			} else {
				di_buf->di_buf[1] =
					di_buf->di_buf_dup_p[1];
				queue_out(di_buf->di_buf[1]);

				drop_frame(check_start_drop_prog,
					(di_buf->di_buf_dup_p[0]->throw_flag) ||
					(di_buf->di_buf_dup_p[1]->throw_flag),
					di_buf);
			}
			frame_count++;
#ifdef DI_BUFFER_DEBUG
			di_print("%s <prog by field>: ", __func__);
#endif
			ret = 1;
			vf_notify_receiver(VFM_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);
		}
	}

#ifdef DI_BUFFER_DEBUG
	if (di_buf) {
		di_print("%s[%d](",
			vframe_type_name[di_buf->type], di_buf->index);
		for (i = 0; i < 2; i++) {
			if (di_buf->di_buf[i])
				di_print("%s[%d],",
				 vframe_type_name[di_buf->di_buf[i]->type],
				 di_buf->di_buf[i]->index);
		}
		di_print(")(vframe type %x dur %d)",
			di_buf->vframe->type, di_buf->vframe->duration);
		if (di_buf->di_buf_dup_p[1] &&
		    (di_buf->di_buf_dup_p[1]->post_proc_flag == 3))
			di_print("=> recycle_list\n");
		else
			di_print("=> post_ready_list\n");
	}
#endif
	return ret;
}

/*
 * di task
 */
static void di_unreg_process(void)
{
	unsigned long start_jiffes = 0;
	if (reg_flag) {
		field_count = 0;
		pr_dbg("%s unreg start %d.\n", __func__, reg_flag);
		start_jiffes = jiffies_64;
		vf_unreg_provider(&di_vf_prov);
		pr_dbg("%s vf unreg cost %u ms.\n", __func__,
			jiffies_to_msecs(jiffies_64 - start_jiffes));
		reg_flag = 0;
		unreg_cnt++;
		if (unreg_cnt > 0x3fffffff)
			unreg_cnt = 0;
		pr_dbg("%s unreg stop %d.\n", __func__, reg_flag);
		di_pre_stru.pre_de_busy = 0;
		di_pre_stru.unreg_req_flag_irq = 1;
		trigger_pre_di_process(TRIGGER_PRE_BY_UNREG);
	} else {
		di_pre_stru.force_unreg_req_flag = 0;
		di_pre_stru.disable_req_flag = 0;
		recovery_flag = 0;
		di_pre_stru.unreg_req_flag = 0;
		trigger_pre_di_process(TRIGGER_PRE_BY_UNREG);
	}
}

static void di_unreg_process_irq(void)
{
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;
#if (defined ENABLE_SPIN_LOCK_ALWAYS)
	spin_lock_irqsave(&plist_lock, flags);
#endif
	di_lock_irqfiq_save(irq_flag2, fiq_flag);
	di_print("%s: di_uninit_buf\n", __func__);
	di_uninit_buf();
	init_flag = 0;
#ifdef CONFIG_AML_RDMA
/* stop rdma */
	rdma_clear(de_devp->rdma_handle);
#endif
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		if (dejaggy_enable) {
			dejaggy_flag = -1;
			DI_Wr_reg_bits(SRSHARP0_SHARP_DEJ1_MISC, 0, 3, 1);
		}
	di_set_power_control(0, 0);
#ifndef NEW_DI_V3
	DI_Wr(DI_CLKG_CTRL, 0xff0000);
/* di enable nr clock gate */
#else
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		enable_di_pre_mif(0);
		DI_Wr(DI_CLKG_CTRL, 0x80f60000);
	} else
		DI_Wr(DI_CLKG_CTRL, 0xf60000);
/* nr/blend0/ei0/mtn0 clock gate */
#endif
	if (get_blackout_policy()) {
		di_set_power_control(1, 0);
		di_hw_disable();
		DI_Wr(DI_CLKG_CTRL, 0x80000000);
		if (!is_meson_gxl_cpu() && !is_meson_gxm_cpu() &&
			!is_meson_gxbb_cpu())
			switch_vpu_clk_gate_vmod(VPU_VPU_CLKB,
				VPU_CLK_GATE_OFF);
	}
	if ((post_wr_en && post_wr_surpport)) {
		diwr_set_power_control(0);
		#ifdef CONFIG_VSYNC_RDMA
		enable_rdma(1);
		#endif
	}
	di_unlock_irqfiq_restore(irq_flag2, fiq_flag);

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
	spin_unlock_irqrestore(&plist_lock, flags);
#endif

	di_pre_stru.force_unreg_req_flag = 0;
	di_pre_stru.disable_req_flag = 0;
	recovery_flag = 0;
#ifdef NEW_DI_V3
	di_pre_stru.cur_prog_flag = 0;
#endif
	di_pre_stru.unreg_req_flag = 0;
	di_pre_stru.unreg_req_flag_irq = 0;
}

static void di_reg_process(void)
{
/*get vout information first time*/
	if (reg_flag == 1)
		return;
	set_output_mode_info();
	vf_provider_init(&di_vf_prov, VFM_NAME, &deinterlace_vf_provider, NULL);
	vf_reg_provider(&di_vf_prov);
	vf_notify_receiver(VFM_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
	reg_flag = 1;
	reg_cnt++;
	if (reg_cnt > 0x3fffffff)
		reg_cnt = 0;
	di_print("########%s\n", __func__);
}
#ifdef CONFIG_AML_RDMA
/* di pre rdma operation */
static void di_rdma_irq(void *arg)
{
	struct di_dev_s *di_devp = (struct di_dev_s *)arg;

	if (!di_devp || (di_devp->rdma_handle <= 0)) {
		pr_err("%s rdma handle %d error.\n", __func__,
			di_devp->rdma_handle);
		return;
	}
	if (di_printk_flag)
		pr_dbg("%s...%d.\n", __func__,
			di_pre_stru.field_count_for_cont);
	return;
}

static struct rdma_op_s di_rdma_op = {
	di_rdma_irq,
	NULL
};
#endif
static int nr_level;

static void di_reg_process_irq(void)
{
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;
	vframe_t *vframe;

	if ((pre_run_flag != DI_RUN_FLAG_RUN) &&
	    (pre_run_flag != DI_RUN_FLAG_STEP))
		return;
	if (pre_run_flag == DI_RUN_FLAG_STEP)
		pre_run_flag = DI_RUN_FLAG_STEP_DONE;

	/*di_pre_stru.reg_req_flag = 1;*/
	/*trigger_pre_di_process(TRIGGER_PRE_BY_TIMERC);*/

	vframe = vf_peek(VFM_NAME);

	if (vframe) {
		#ifdef CONFIG_CMA
		if ((vframe->width > default_width) ||
			(vframe->height > (default_height + 8)))
			bypass_4K = 1;
		else
			bypass_4K = 0;
		#endif
		/* patch for vdin progressive input */
		if (((vframe->type & VIDTYPE_VIU_422) &&
		    ((vframe->type & VIDTYPE_PROGRESSIVE) == 0))
			#ifdef DET3D
			|| det3d_en
			#endif
			|| (use_2_interlace_buff & 0x2)
			)
			use_2_interlace_buff = 1;
		else
			use_2_interlace_buff = 0;
		switch_vpu_clk_gate_vmod(VPU_VPU_CLKB, VPU_CLK_GATE_ON);
		di_set_power_control(0, 1);
		di_set_power_control(1, 1);
		if ((post_wr_en && post_wr_surpport)) {
			diwr_set_power_control(1);
			#ifdef CONFIG_VSYNC_RDMA
			enable_rdma(rdma_en);
			#endif
		}
#ifndef NEW_DI_V3
		DI_Wr(DI_CLKG_CTRL, 0xfeff0000);
		/* di enable nr clock gate */
#else
		/* if mcdi enable DI_CLKG_CTRL should be 0xfef60000 */
		DI_Wr(DI_CLKG_CTRL, 0xfef60001);
		/* nr/blend0/ei0/mtn0 clock gate */
#endif
		/* add for di Reg re-init */
#ifdef NEW_DI_TV
		di_set_para_by_tvinfo(vframe);
#endif
		if (di_printk_flag & 2)
			di_printk_flag = 1;

		di_print("%s: vframe come => di_init_buf\n", __func__);

		if (is_progressive(vframe) && (prog_proc_config & 0x10)) {
#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_lock_irqsave(&plist_lock, flags);
#endif
			di_lock_irqfiq_save(irq_flag2, fiq_flag);
			 /*
			 * 10 bit mode need 1.5 times buffer size of
			 * 8 bit mode, init the buffer size as 10 bit
			 * mode size, to make sure can switch bit mode
			 * smoothly.
			 */
			di_init_buf(default_width, default_height, 1);

			di_unlock_irqfiq_restore(irq_flag2, fiq_flag);

#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_unlock_irqrestore(&plist_lock, flags);
#endif
		} else {
#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_lock_irqsave(&plist_lock, flags);
#endif
			di_lock_irqfiq_save(irq_flag2, fiq_flag);
			/*
			 * 10 bit mode need 1.5 times buffer size of
			 * 8 bit mode, init the buffer size as 10 bit
			 * mode size, to make sure can switch bit mode
			 * smoothly.
			 */
			di_init_buf(default_width, default_height, 0);

			di_unlock_irqfiq_restore(irq_flag2, fiq_flag);

#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_unlock_irqrestore(&plist_lock, flags);
#endif
		}
		di_nr_level_config(nr_level);
		reset_pulldown_state();
		if (pulldown_enable) {
			FlmVOFSftInt(&pd_param);
			flm22_sure_num = (vframe->height * 100)/480;
			flm22_sure_smnum = (flm22_sure_num * flm22_ratio)/100;
		}
		combing_threshold_config(vframe->width);
		if (is_meson_txl_cpu()) {
			combing_pd22_window_config(vframe->width,
				(vframe->height>>1));
			tbff_init();
		}
		init_flag = 1;
		di_pre_stru.reg_req_flag_irq = 1;
		last_lev = -1;
	}
}



static void dynamic_bypass_process(void)
{
	if ((disp_frame_count > 0) && (vdin_source_flag == 0)) {
		int ready_count = list_count(QUEUE_POST_READY);
		if (bypass_dynamic_flag == 0) {
			if (ready_count == 0) {
				if (post_ready_empty_count < 10) {
					post_ready_empty_count++;
				} else {
					bypass_dynamic_flag =
						bypass_dynamic;
					post_ready_empty_count = 30;
				}
			} else
				post_ready_empty_count = 0;
		} else {
			if (ready_count >= 4) {
				post_ready_empty_count--;
				if (post_ready_empty_count <= 0)
					bypass_dynamic_flag = 0;
			} else
				post_ready_empty_count = 60;
		}
	}
}


static void di_process(void)
{
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;

	di_process_cnt++;

	if (init_flag && mem_flag && (recovery_flag == 0) &&
		(dump_state_flag == 0)) {
		if (bypass_dynamic != 0)
			dynamic_bypass_process();
#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
		spin_lock_irqsave(&plist_lock, flags);
#endif
		if (di_pre_stru.pre_de_busy == 0) {
			if (di_pre_stru.pre_de_process_done) {
#if 0/*def CHECK_DI_DONE*/
				/* also for NEW_DI ? 7/15/2013 */
				unsigned int data32 = Rd(DI_INTR_CTRL);
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
				 * DI_INTR_CTRL[16] and DI_INTR_CTRL[17] are 0*/
				if (
					((data32 & 0x1) &&
					 ((di_pre_stru.enable_mtnwr == 0)
					  || (data32 &
					      0x2))) ||
					(di_pre_stru.pre_de_clear_flag == 2)) {
					RDMA_WR(DI_INTR_CTRL, data32);
#endif
				pre_process_time =
					di_pre_stru.pre_de_busy_timer_count;
				pre_de_done_buf_config();

				di_pre_stru.pre_de_process_done = 0;
				di_pre_stru.pre_de_clear_flag = 0;
#ifdef CHECK_DI_DONE
			}
#endif
			} else if (di_pre_stru.pre_de_clear_flag == 1) {
				di_lock_irqfiq_save(
					irq_flag2, fiq_flag);
				pre_de_done_buf_clear();
				di_unlock_irqfiq_restore(
					irq_flag2, fiq_flag);
				di_pre_stru.pre_de_process_done = 0;
				di_pre_stru.pre_de_clear_flag = 0;
			}
		}

		di_lock_irqfiq_save(irq_flag2, fiq_flag);
		di_post_stru.check_recycle_buf_cnt = 0;
		while (check_recycle_buf() & 1) {
			if (di_post_stru.check_recycle_buf_cnt++ >
				MAX_IN_BUF_NUM) {
				di_pr_info("%s: check_recycle_buf time out!!\n",
					__func__);
				break;
			}
		}
		di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
		if ((di_pre_stru.pre_de_busy == 0) &&
		    (di_pre_stru.pre_de_process_done == 0)) {
			if ((pre_run_flag == DI_RUN_FLAG_RUN) ||
			    (pre_run_flag == DI_RUN_FLAG_STEP)) {
				if (pre_run_flag == DI_RUN_FLAG_STEP)
					pre_run_flag = DI_RUN_FLAG_STEP_DONE;
				if (pre_de_buf_config())
					pre_de_process();
			}
		}
		di_post_stru.di_post_process_cnt = 0;
		while (process_post_vframe()) {
			if (di_post_stru.di_post_process_cnt++ >
				MAX_POST_BUF_NUM) {
				di_pr_info("%s: process_post_vframe time out!!\n",
					__func__);
				break;
			}
		}
		if ((post_wr_en && post_wr_surpport)) {
			if (di_post_stru.post_de_busy == 0 &&
			di_post_stru.de_post_process_done) {
				post_de_done_buf_config();
				di_post_stru.de_post_process_done = 0;
			}
			di_post_process();
		}

#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
		spin_unlock_irqrestore(&plist_lock, flags);
#endif
	}
}
static unsigned int nr_done_check_cnt = 5;
module_param_named(nr_done_check_cnt, nr_done_check_cnt, uint, 0644);
static void di_pre_trigger_work(struct di_pre_stru_s *pre_stru_p)
{

	if (pre_stru_p->pre_de_busy && init_flag) {
		pre_stru_p->pre_de_busy_timer_count++;
		if (pre_stru_p->pre_de_busy_timer_count >= nr_done_check_cnt) {
			enable_di_pre_mif(0);
			pre_stru_p->pre_de_busy_timer_count = 0;
			pre_stru_p->pre_de_irq_timeout_count++;
			if (timeout_miss_policy == 0) {
				pre_stru_p->pre_de_process_done = 1;
				pre_stru_p->pre_de_busy = 0;
				pre_stru_p->pre_de_clear_flag = 2;
			} else if (timeout_miss_policy == 1) {
				pre_stru_p->pre_de_clear_flag = 1;
				pre_stru_p->pre_de_busy = 0;
			} /* else if (timeout_miss_policy == 2) {
			   * }*/
			pr_info("***** DI ****** wait %d pre_de_irq timeout\n",
				pre_stru_p->field_count_for_cont);
		}
	} else {
		pre_stru_p->pre_de_busy_timer_count = 0;
	}

	/* if(force_trig){ */
	force_trig_cnt++;
	trigger_pre_di_process(TRIGGER_PRE_BY_TIMER);
	/* } */

	if (force_recovery) {
		if (recovery_flag || (force_recovery & 0x2)) {
			force_recovery_count++;
			if (init_flag && mem_flag) {
				pr_dbg("====== DI force recovery =========\n");
				force_recovery &= (~0x2);
				dis2_di();
				recovery_flag = 0;
			}
		}
	}
}

static int di_task_handle(void *data)
{
	int ret = 0;

	while (1) {
		ret = down_interruptible(&di_sema);
		if (active_flag) {
			if ((di_pre_stru.unreg_req_flag ||
				di_pre_stru.force_unreg_req_flag ||
				di_pre_stru.disable_req_flag) &&
				(di_pre_stru.pre_de_busy == 0)) {
				di_unreg_process();
			}
			if (di_pre_stru.reg_req_flag_irq ||
				di_pre_stru.reg_req_flag) {
				di_reg_process();
				di_pre_stru.reg_req_flag = 0;
				di_pre_stru.reg_req_flag_irq = 0;
			}
			#ifdef CONFIG_CMA
			if (di_pre_stru.cma_alloc_req) {
				di_cma_alloc();
				di_pre_stru.cma_alloc_req = 0;
				di_pre_stru.cma_alloc_done = 1;
				mem_flag = 1;
			}
			if (di_pre_stru.cma_release_req) {
				di_cma_release();
				di_pre_stru.cma_release_req = 0;
				di_pre_stru.cma_alloc_done = 0;
				mem_flag = 0;
			}
			#endif
		}
	}

	return 0;
}

static void di_pre_process_irq(struct di_pre_stru_s *pre_stru_p)
{
	int i;
	if (active_flag) {
		if (pre_stru_p->unreg_req_flag_irq)
			di_unreg_process_irq();
		if (init_flag == 0 && pre_stru_p->reg_req_flag_irq == 0)
			di_reg_process_irq();
	}

	for (i = 0; i < 2; i++) {
		if (active_flag)
			di_process();
	}
	log_buffer_state("pro");
}
#ifdef USE_HRTIMER
static struct hrtimer di_pre_hrtimer;
static void pre_tasklet(unsigned long arg)
{
	if (jiffies_to_msecs(jiffies_64 - de_devp->jiffy) > 10)
		pr_err("DI: tasklet schedule over 10ms.\n");
	di_pre_process_irq((struct di_pre_stru_s *)arg);
}

static enum hrtimer_restart di_pre_hrtimer_func(struct hrtimer *timer)
{
	di_pre_trigger_work(&di_pre_stru);
	hrtimer_forward_now(&di_pre_hrtimer, ms_to_ktime(10));
	return HRTIMER_RESTART;
}
#else
static struct timer_list di_pre_timer;
static struct work_struct di_pre_work;
static void di_pre_timer_cb(unsigned long arg)
{
	struct timer_list *timer = (struct timer_list *)arg;
	schedule_work(&di_pre_work);
	timer->expires = jiffies + DI_PRE_INTERVAL;
	add_timer(timer);
}

static void di_per_work_func(struct work_struct *work)
{
	di_pre_trigger_work(&di_pre_stru);
}

static irqreturn_t timer_irq(int irq, void *dev_instance)
{
	di_pre_process_irq(&di_pre_stru);
	return IRQ_HANDLED;
}

#endif
/*
 * provider/receiver interface
 */

/* unsigned int vf_keep_current(void);*/
static int di_receiver_event_fun(int type, void *data, void *arg)
{
	int i;
	ulong flags;

	if (type == VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR) {
		return di_pre_stru.vdin2nr;
	} else if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		pr_dbg("%s , is_bypass() %d trick_mode %d bypass_all %d\n",
			__func__, is_bypass(NULL), trick_mode, bypass_all);
		pr_info("%s: vf_notify_receiver unreg\n", __func__);

		di_pre_stru.unreg_req_flag = 1;
		provider_vframe_level = 0;
		bypass_dynamic_flag = 0;
		post_ready_empty_count = 0;
		vdin_source_flag = 0;
		trigger_pre_di_process(TRIGGER_PRE_BY_PROVERDER_UNREG);
		di_pre_stru.unreg_req_flag_cnt = 0;
		while (di_pre_stru.unreg_req_flag) {
			usleep_range(10000, 10001);
			if (di_pre_stru.unreg_req_flag_cnt++ >
				di_reg_unreg_cnt) {
				reg_unreg_timeout_cnt++;
				pr_dbg("%s:unreg_reg_flag timeout!!!\n",
					__func__);
				break;
			}
		}
#ifdef SUPPORT_MPEG_TO_VDIN
		if (mpeg2vdin_flag) {
			struct vdin_arg_s vdin_arg;
			struct vdin_v4l2_ops_s *vdin_ops = get_vdin_v4l2_ops();
			vdin_arg.cmd = VDIN_CMD_MPEGIN_STOP;
			if (vdin_ops->tvin_vdin_func)
				vdin_ops->tvin_vdin_func(0, &vdin_arg);

			mpeg2vdin_flag = 0;
		}
#endif
		bypass_state = 1;
#ifdef RUN_DI_PROCESS_IN_IRQ
		if (vdin_source_flag)
			DI_Wr_reg_bits(VDIN_WR_CTRL, 0x3, 24, 3);

#endif
	} else if (type == VFRAME_EVENT_PROVIDER_RESET) {
		di_blocking = 1;

		pr_dbg("%s: VFRAME_EVENT_PROVIDER_RESET\n", __func__);

		goto light_unreg;
	} else if (type == VFRAME_EVENT_PROVIDER_LIGHT_UNREG) {
		di_blocking = 1;

		pr_dbg("%s: vf_notify_receiver ligth unreg\n", __func__);

light_unreg:
		provider_vframe_level = 0;
		bypass_dynamic_flag = 0;
		post_ready_empty_count = 0;

		spin_lock_irqsave(&plist_lock, flags);
		for (i = 0; i < MAX_IN_BUF_NUM; i++) {

			if (vframe_in[i])
				pr_dbg("DI:clear vframe_in[%d]\n", i);

			vframe_in[i] = NULL;
		}
		spin_unlock_irqrestore(&plist_lock, flags);
		di_blocking = 0;
	} else if (type == VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME) {
		unsigned char vf_put_flag = 0;

		pr_dbg(
			"%s:VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME\n",
			__func__);

		provider_vframe_level = 0;
		bypass_dynamic_flag = 0;
		post_ready_empty_count = 0;

/* DisableVideoLayer();
 * do not display garbage when 2d->3d or 3d->2d */
		spin_lock_irqsave(&plist_lock, flags);
		for (i = 0; i < MAX_IN_BUF_NUM; i++) {
			if (vframe_in[i]) {
				vf_put(vframe_in[i], VFM_NAME);
				pr_dbg("DI:clear vframe_in[%d]\n", i);
				vf_put_flag = 1;
			}
			vframe_in[i] = NULL;
		}
		if (vf_put_flag)
			vf_notify_provider(VFM_NAME,
				VFRAME_EVENT_RECEIVER_PUT, NULL);

		spin_unlock_irqrestore(&plist_lock, flags);
	} else if (type == VFRAME_EVENT_PROVIDER_VFRAME_READY) {
		provider_vframe_level++;
		trigger_pre_di_process(TRIGGER_PRE_BY_VFRAME_READY);

#ifdef RUN_DI_PROCESS_IN_IRQ
#define INPUT2PRE_2_BYPASS_SKIP_COUNT   4
		if (active_flag && vdin_source_flag) {
			if (is_bypass(NULL)) {
				if (di_pre_stru.pre_de_busy == 0) {
					DI_Wr_reg_bits(VDIN_WR_CTRL,
						0x3, 24, 3);
					di_pre_stru.vdin2nr = 0;
				}
				if (di_pre_stru.bypass_start_count <
				    INPUT2PRE_2_BYPASS_SKIP_COUNT) {
					vframe_t *vframe_tmp = vf_get(VFM_NAME);
					if (vframe_tmp != NULL) {
						vf_put(vframe_tmp, VFM_NAME);
						vf_notify_provider(VFM_NAME,
						VFRAME_EVENT_RECEIVER_PUT,
						NULL);
					}
					di_pre_stru.bypass_start_count++;
				}
			} else if (is_input2pre()) {
				di_pre_stru.bypass_start_count = 0;
				if ((di_pre_stru.pre_de_busy != 0) &&
				    (input2pre_miss_policy == 1 &&
				     frame_count < 30)) {
					di_pre_stru.pre_de_clear_flag = 1;
					di_pre_stru.pre_de_busy = 0;
					input2pre_buf_miss_count++;
				}

				if (di_pre_stru.pre_de_busy == 0) {
					DI_Wr_reg_bits(VDIN_WR_CTRL,
						0x5, 24, 3);
					di_pre_stru.vdin2nr = 1;
					di_process();
					log_buffer_state("pr_");
					if (di_pre_stru.pre_de_busy == 0)
						input2pre_proc_miss_count++;
				} else {
					vframe_t *vframe_tmp = vf_get(VFM_NAME);
					if (vframe_tmp != NULL) {
						vf_put(vframe_tmp, VFM_NAME);
						vf_notify_provider(VFM_NAME,
						VFRAME_EVENT_RECEIVER_PUT,
						NULL);
					}
					input2pre_buf_miss_count++;
					if ((di_pre_stru.cur_width > 720 &&
					     di_pre_stru.cur_height > 576) ||
					    (input2pre_throw_count & 0x10000))
						di_pre_stru.pre_throw_flag =
							input2pre_throw_count &
							0xffff;
				}
			} else {
				if (di_pre_stru.pre_de_busy == 0) {
					DI_Wr_reg_bits(VDIN_WR_CTRL,
						0x3, 24, 3);
					di_pre_stru.vdin2nr = 0;
				}
				di_pre_stru.bypass_start_count =
					INPUT2PRE_2_BYPASS_SKIP_COUNT;
			}
		}
#endif
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		/*int in_buf_num = 0;*/
		struct vframe_states states;
		if (recovery_flag)
			return RECEIVER_INACTIVE;
#if 1/*fix for ucode reset method be break by di.20151230*/
		di_vf_states(&states, NULL);
		if (states.buf_avail_num > 0) {
			return RECEIVER_ACTIVE;
		} else {
			if (vf_notify_receiver(
				VFM_NAME,
				VFRAME_EVENT_PROVIDER_QUREY_STATE,
				NULL)
			== RECEIVER_ACTIVE)
				return RECEIVER_ACTIVE;
			return RECEIVER_INACTIVE;
		}
#else
		for (i = 0; i < MAX_IN_BUF_NUM; i++) {
			if (vframe_in[i] != NULL)
				in_buf_num++;
		if (bypass_state == 1) {
			if (in_buf_num > 1)
				return RECEIVER_ACTIVE;
			else
				return RECEIVER_INACTIVE;
		} else {
			if (in_buf_num > 0)
				return RECEIVER_ACTIVE;
			else
				return RECEIVER_INACTIVE;
		}
#endif
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		char *provider_name = (char *)data;
		char *receiver_name = NULL;
		bypass_state = 0;
		di_pre_stru.reg_req_flag = 1;
		pr_dbg("%s: vframe provider reg\n", __func__);
		trigger_pre_di_process(TRIGGER_PRE_BY_PROVERDER_REG);
		di_pre_stru.reg_req_flag_cnt = 0;
		while (di_pre_stru.reg_req_flag) {
			usleep_range(10000, 10001);
			if (di_pre_stru.reg_req_flag_cnt++ > di_reg_unreg_cnt) {
				reg_unreg_timeout_cnt++;
				pr_dbg("%s:reg_req_flag timeout!!!\n",
					__func__);
				break;
			}
		}
#ifndef USE_HRTIMER
		aml_cbus_update_bits(ISA_TIMER_MUX, 1 << 14, 0 << 14);
		aml_cbus_update_bits(ISA_TIMER_MUX, 3 << 4, 0 << 4);
		aml_cbus_update_bits(ISA_TIMER_MUX, 1 << 18, 1 << 18);
		aml_write_cbus(ISA_TIMERC, 1);
#endif
		if (strncmp(provider_name, "vdin", 4) == 0) {
			vdin_source_flag = 1;
		} else {
			vdin_source_flag = 0;
			pre_urgent = 0;
		}
		receiver_name = vf_get_receiver_name(VFM_NAME);
		if (receiver_name) {
			if (!strcmp(receiver_name, "amvideo")) {
				di_post_stru.run_early_proc_fun_flag = 0;
				receiver_is_amvideo = 1;
		/* pr_info("set run_early_proc_fun_flag to 1\n"); */
			} else {
				di_post_stru.run_early_proc_fun_flag = 1;
				receiver_is_amvideo = 0;
		 pr_info("set run_early_proc_fun_flag to 1\n");
			}
		} else {
			pr_info("%s error receiver is null.\n", __func__);
		}
	}
#ifdef DET3D
	else if (type == VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE) {
		int flag = (long)data;
		di_pre_stru.vframe_interleave_flag = flag;
	}
#endif
	else if (type == VFRAME_EVENT_PROVIDER_FR_HINT) {
		vf_notify_receiver(VFM_NAME,
			VFRAME_EVENT_PROVIDER_FR_HINT, data);
	} else if (type == VFRAME_EVENT_PROVIDER_FR_END_HINT) {
		vf_notify_receiver(VFM_NAME,
			VFRAME_EVENT_PROVIDER_FR_END_HINT, data);
	}

	return 0;
}

static void fast_process(void)
{
	int i;
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;
	unsigned int bypass_buf_threshold = bypass_get_buf_threshold;

	if ((di_pre_stru.cur_inp_type & VIDTYPE_VIU_444) &&
		(di_pre_stru.cur_source_type == VFRAME_SOURCE_TYPE_HDMI))
		bypass_buf_threshold = bypass_hdmi_get_buf_threshold;
	if (active_flag && is_bypass(NULL) && (bypass_buf_threshold <= 1) &&
		init_flag && mem_flag && (recovery_flag == 0) &&
		(dump_state_flag == 0)) {
		if (vf_peek(VFM_NAME) == NULL)
			return;

		for (i = 0; i < 2; i++) {
			spin_lock_irqsave(&plist_lock, flags);
			if (di_pre_stru.pre_de_process_done) {
				pre_de_done_buf_config();
				di_pre_stru.pre_de_process_done = 0;
			}

			di_lock_irqfiq_save(irq_flag2, fiq_flag);
			di_post_stru.check_recycle_buf_cnt = 0;
			while (check_recycle_buf() & 1) {
				if (di_post_stru.check_recycle_buf_cnt++ >
					MAX_IN_BUF_NUM) {
					di_pr_info("%s: check_recycle_buf time out!!\n",
						__func__);
					break;
				}
			}
			di_unlock_irqfiq_restore(irq_flag2, fiq_flag);

			if ((di_pre_stru.pre_de_busy == 0) &&
			    (di_pre_stru.pre_de_process_done == 0)) {
				if ((pre_run_flag == DI_RUN_FLAG_RUN) ||
				    (pre_run_flag == DI_RUN_FLAG_STEP)) {
					if (pre_run_flag == DI_RUN_FLAG_STEP)
						pre_run_flag =
							DI_RUN_FLAG_STEP_DONE;
					if (pre_de_buf_config())
						pre_de_process();
				}
			}
			di_post_stru.di_post_process_cnt = 0;
			while (process_post_vframe()) {
				if (di_post_stru.di_post_process_cnt++ >
					MAX_POST_BUF_NUM) {
					di_pr_info("%s: process_post_vframe time out!!\n",
						__func__);
					break;
				}
			}

			spin_unlock_irqrestore(&plist_lock, flags);
		}
	}
}

static vframe_t *di_vf_peek(void *arg)
{
	vframe_t *vframe_ret = NULL;
	struct di_buf_s *di_buf = NULL;

	video_peek_cnt++;
	if ((init_flag == 0) || (mem_flag == 0) || recovery_flag ||
		di_blocking || di_pre_stru.unreg_req_flag || dump_state_flag)
		return NULL;
	if ((run_flag == DI_RUN_FLAG_PAUSE) ||
	    (run_flag == DI_RUN_FLAG_STEP_DONE))
		return NULL;

	log_buffer_state("pek");

	fast_process();
#ifdef SUPPORT_START_FRAME_HOLD
	if ((disp_frame_count == 0) && (is_bypass(NULL) == 0)) {
		int ready_count = list_count(QUEUE_POST_READY);
		if (ready_count > start_frame_hold_count) {
			di_buf = get_di_buf_head(QUEUE_POST_READY);
			if (di_buf)
				vframe_ret = di_buf->vframe;
		}
	} else
#endif
	{
		if (!queue_empty(QUEUE_POST_READY)) {
			di_buf = get_di_buf_head(QUEUE_POST_READY);
			if (di_buf)
				vframe_ret = di_buf->vframe;
		}
	}
#ifdef DI_BUFFER_DEBUG
	if (vframe_ret)
		di_print("%s: %s[%d]:%x\n", __func__,
			vframe_type_name[di_buf->type],
			di_buf->index, vframe_ret);
#endif
	if (force_duration_0) {
		if (vframe_ret)
			vframe_ret->duration = 0;
	}
	return vframe_ret;
}
/*recycle the buffer for keeping buffer*/
void recycle_keep_buffer(void)
{
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;
	int i = 0;

	if ((used_post_buf_index != -1) && (new_keep_last_frame_enable)) {
		if (di_buf_post[used_post_buf_index].type == VFRAME_TYPE_POST) {
			pr_dbg("%s recycle keep cur di_buf %d (",
				__func__, used_post_buf_index);
			di_lock_irqfiq_save(irq_flag2, fiq_flag);
			for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
				if (
					di_buf_post[used_post_buf_index].
					di_buf_dup_p[i]) {
					queue_in(
					di_buf_post[used_post_buf_index].
					di_buf_dup_p[i],
					QUEUE_RECYCLE);
					pr_dbg(" %d ",
						di_buf_post[used_post_buf_index]
						.di_buf_dup_p[i]->index);
				}
			}
			queue_in(&di_buf_post[used_post_buf_index],
				QUEUE_POST_FREE);
			di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
			pr_dbg(")\n");
		}
		used_post_buf_index = -1;
	}
}
static vframe_t *di_vf_get(void *arg)
{
	vframe_t *vframe_ret = NULL;
	struct di_buf_s *di_buf = NULL;
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;

	if ((init_flag == 0) || (mem_flag == 0) || recovery_flag ||
		di_blocking || di_pre_stru.unreg_req_flag || dump_state_flag)
		return NULL;

	if ((run_flag == DI_RUN_FLAG_PAUSE) ||
	    (run_flag == DI_RUN_FLAG_STEP_DONE))
		return NULL;

#ifdef SUPPORT_START_FRAME_HOLD
	if ((disp_frame_count == 0) && (is_bypass(NULL) == 0)) {
		int ready_count = list_count(QUEUE_POST_READY);
		if (ready_count > start_frame_hold_count)
			goto get_vframe;
	} else
#endif
	if (!queue_empty(QUEUE_POST_READY)) {
#ifdef SUPPORT_START_FRAME_HOLD
get_vframe:
#endif
		log_buffer_state("ge_");
		di_lock_irqfiq_save(irq_flag2, fiq_flag);

		di_buf = get_di_buf_head(QUEUE_POST_READY);
		queue_out(di_buf);
		queue_in(di_buf, QUEUE_DISPLAY); /* add it into display_list */

		di_unlock_irqfiq_restore(irq_flag2, fiq_flag);

		if (di_buf) {
			vframe_ret = di_buf->vframe;

			if ((post_wr_en && post_wr_surpport) &&
			(di_buf->process_fun_index != PROCESS_FUN_NULL)) {
				config_canvas_idx(di_buf,
				di_post_idx[di_post_stru.canvas_id][5], -1);
				vframe_ret->canvas0Addr = di_buf->nr_canvas_idx;
				vframe_ret->canvas1Addr = di_buf->nr_canvas_idx;
				vframe_ret->early_process_fun = NULL;
				vframe_ret->process_fun = NULL;
			}
		}
		disp_frame_count++;
		if (run_flag == DI_RUN_FLAG_STEP)
			run_flag = DI_RUN_FLAG_STEP_DONE;

		log_buffer_state("get");
	}
	if (vframe_ret) {
		di_print("%s: %s[%d]:%x %u ms\n", __func__,
		vframe_type_name[di_buf->type], di_buf->index, vframe_ret,
		jiffies_to_msecs(jiffies_64 - vframe_ret->ready_jiffies64));
	}

	if (force_duration_0) {
		if (vframe_ret)
			vframe_ret->duration = 0;
	}

	if (!post_wr_en && di_post_stru.run_early_proc_fun_flag && vframe_ret) {
		if (vframe_ret->early_process_fun == do_pre_only_fun)
			vframe_ret->early_process_fun(
				vframe_ret->private_data, vframe_ret);
	}
	/*if (vframe_ret)
		recycle_keep_buffer();*/
	atomic_set(&di_buf->di_cnt, 1);
	return vframe_ret;
}

static void di_vf_put(vframe_t *vf, void *arg)
{
	struct di_buf_s *di_buf = (struct di_buf_s *)vf->private_data;
	ulong flags = 0, fiq_flag = 0, irq_flag2 = 0;

/* struct di_buf_s *p = NULL; */
/* int itmp = 0; */
	if ((init_flag == 0) || (mem_flag == 0) || recovery_flag) {
		di_print("%s: 0x%p\n", __func__, vf);
		return;
	}
	if (di_blocking)
		return;
	log_buffer_state("pu_");
	if (used_post_buf_index != -1) {
			recycle_keep_buffer();
	}
	if (di_buf->type == VFRAME_TYPE_POST) {
		di_lock_irqfiq_save(irq_flag2, fiq_flag);

		if (is_in_queue(di_buf, QUEUE_DISPLAY)) {
			if (!atomic_dec_and_test(&di_buf->di_cnt))
				di_print("%s,di_cnt > 0\n", __func__);
			recycle_vframe_type_post(di_buf);
	} else {
			di_print("%s: %s[%d] not in display list\n", __func__,
			vframe_type_name[di_buf->type], di_buf->index);
	}
		di_unlock_irqfiq_restore(irq_flag2, fiq_flag);
#ifdef DI_BUFFER_DEBUG
		recycle_vframe_type_post_print(di_buf, __func__, __LINE__);
#endif
	} else {
		di_lock_irqfiq_save(irq_flag2, fiq_flag);
		queue_in(di_buf, QUEUE_RECYCLE);
		di_unlock_irqfiq_restore(irq_flag2, fiq_flag);

		di_print("%s: %s[%d] =>recycle_list\n", __func__,
			vframe_type_name[di_buf->type], di_buf->index);
	}

	trigger_pre_di_process(TRIGGER_PRE_BY_PUT);
}

static int di_event_cb(int type, void *data, void *private_data)
{
	if (type == VFRAME_EVENT_RECEIVER_FORCE_UNREG) {
		pr_info("%s: RECEIVER_FORCE_UNREG return\n",
			__func__);
		return 0;
		di_pre_stru.force_unreg_req_flag = 1;
		provider_vframe_level = 0;
		bypass_dynamic_flag = 0;
		post_ready_empty_count = 0;

		trigger_pre_di_process(TRIGGER_PRE_BY_FORCE_UNREG);
		di_pre_stru.unreg_req_flag_cnt = 0;
		while (di_pre_stru.force_unreg_req_flag) {
			usleep_range(1000, 1001);
			di_pr_info("%s:unreg_req_flag_cnt:%d!!!\n",
				__func__, di_pre_stru.unreg_req_flag_cnt);
			if (di_pre_stru.unreg_req_flag_cnt++ >
				di_reg_unreg_cnt) {
				di_pre_stru.unreg_req_flag_cnt = 0;
				di_pr_info("%s:unreg_reg_flag timeout!!!\n",
					__func__);
				break;
			}
		}
	}
	return 0;
}

static int di_vf_states(struct vframe_states *states, void *arg)
{
	if (!states)
		return -1;
	states->vf_pool_size = local_buf_num;
	states->buf_free_num = list_count(QUEUE_LOCAL_FREE);
	states->buf_avail_num = list_count(QUEUE_POST_READY);
	states->buf_recycle_num = list_count(QUEUE_RECYCLE);
	return 0;
}

/*****************************
 *	 di driver file_operations
 *
 ******************************/
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
/* ... */
	return 0;
}

static const struct file_operations di_fops = {
	.owner		= THIS_MODULE,
	.open		= di_open,
	.release	= di_release,
/* .ioctl	 = di_ioctl, */
};

static ssize_t
show_frame_format(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	int ret = 0;

	if (init_flag && mem_flag)
		ret += sprintf(buf + ret, "%s\n",
			di_pre_stru.cur_prog_flag
			? "progressive" : "interlace");

	else
		ret += sprintf(buf + ret, "%s\n", "null");

	return ret;
}
static DEVICE_ATTR(frame_format, 0444, show_frame_format, NULL);

static int rmem_di_device_init(struct reserved_mem *rmem, struct device *dev)
{
	di_dev_t *di_devp = dev_get_drvdata(dev);

	if (di_devp) {
		di_devp->mem_start = rmem->base;
		di_devp->mem_size = rmem->size;
		if (!of_get_flat_dt_prop(rmem->fdt_node, "no-map", NULL))
			di_devp->flags |= DI_MAP_FLAG;
	pr_dbg("di reveser memory 0x%lx, size %uMB.\n",
			di_devp->mem_start, (di_devp->mem_size >> 20));
		return 0;
	}
/* pr_dbg("di reveser memory 0x%x, size %u B.\n",
 * rmem->base, rmem->size); */
	return 1;
}

static void rmem_di_device_release(struct reserved_mem *rmem,
				   struct device *dev)
{
	di_dev_t *di_devp = dev_get_drvdata(dev);

	if (di_devp) {
		di_devp->mem_start = 0;
		di_devp->mem_size = 0;
	}
}
#ifdef CONFIG_AML_RDMA
unsigned int RDMA_RD_BITS(unsigned int adr, unsigned int start,
			  unsigned int len)
{
	if (de_devp->rdma_handle)
		return rdma_read_reg(de_devp->rdma_handle, adr) &
		       (((1 << len) - 1) << start);
	else
		return Rd_reg_bits(adr, start, len);
}

unsigned int RDMA_WR(unsigned int adr, unsigned int val)
{
	if (is_need_stop_reg(adr))
		return 0;

	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable) {
		if (di_pre_stru.field_count_for_cont < 1)
			DI_Wr(adr, val);
		else
			rdma_write_reg(de_devp->rdma_handle, adr, val);
		return 0;
	} else {
		DI_Wr(adr, val);
		return 1;
	}
}

unsigned int RDMA_RD(unsigned int adr)
{
	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable)
		return rdma_read_reg(de_devp->rdma_handle, adr);
	else
		return Rd(adr);
}

unsigned int RDMA_WR_BITS(unsigned int adr, unsigned int val,
			  unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(adr))
		return 0;

	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable) {
		if (di_pre_stru.field_count_for_cont < 1)
			DI_Wr_reg_bits(adr, val, start, len);
		else
			rdma_write_reg_bits(de_devp->rdma_handle,
				adr, val, start, len);
		return 0;
	} else {
		DI_Wr_reg_bits(adr, val, start, len);
		return 1;
	}
}
#else
unsigned int RDMA_RD_BITS(unsigned int adr, unsigned int start,
			  unsigned int len)
{
	return Rd_reg_bits(adr, start, len);
}
unsigned int RDMA_WR(unsigned int adr, unsigned int val)
{
	DI_Wr(adr, val);
	return 1;
}

unsigned int RDMA_RD(unsigned int adr)
{
	return Rd(adr);
}

unsigned int RDMA_WR_BITS(unsigned int adr, unsigned int val,
			  unsigned int start, unsigned int len)
{
	DI_Wr_reg_bits(adr, val, start, len);
	return 1;
}
#endif

static void set_di_flag(void)
{
	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu()) {
		mcpre_en = true;
		pulldown_mode = 1;
		pulldown_enable = 1;
		di_pre_rdma_enable = false;
		di_vscale_skip_enable = 4;
		use_2_interlace_buff = 1;
		pre_hold_line = 12;
		if (nr10bit_surpport)
			di_force_bit_mode = 10;
		else
			di_force_bit_mode = 8;
		if (is_meson_txl_cpu()) {
			full_422_pack = true;
			tff_bff_enable = false;
			dejaggy_enable = 0;
		}
	} else {
		mcpre_en = false;
		pulldown_mode = 0;
		pulldown_enable = 0;
		di_pre_rdma_enable = false;
		di_vscale_skip_enable = 4;
		use_2_interlace_buff = 0;
		di_force_bit_mode = 8;
	}

	if (di_pre_rdma_enable) {
		pldn_dly = 1;
		pldn_dly1 = 1;
		tbbtff_dly = 1;
	} else {
		pldn_dly = 2;
		pldn_dly1 = 2;
		tbbtff_dly = 0;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXBB))
		dnr_dm_en = true;

	return;
}

static const struct reserved_mem_ops rmem_di_ops = {
	.device_init	= rmem_di_device_init,
	.device_release = rmem_di_device_release,
};

static int __init rmem_di_setup(struct reserved_mem *rmem)
{
/*
 * struct cma *cma;
 * int err;
 * pr_dbg("%s setup.\n",__func__);
 * err = cma_init_reserved_mem(rmem->base, rmem->size, 0, &cma);
 * if (err) {
 * pr_err("Reserved memory: unable to setup CMA region\n");
 * return err;
 * }
 */
	rmem->ops = &rmem_di_ops;
/* rmem->priv = cma; */

	di_pr_info(
	"DI reserved memory: created CMA memory pool at %pa, size %ld MiB\n",
		&rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}
RESERVEDMEM_OF_DECLARE(di, "amlogic, di-mem", rmem_di_setup);
static int di_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;/* , offset = 0, size = 0; */
/* struct resource *res_irq = NULL; */
	int buf_num_avail = 0;
	struct di_dev_s *di_devp = NULL;

/* const void *name = NULL; */
	di_pr_info("di_probe\n");
	di_devp = kmalloc(sizeof(struct di_dev_s), GFP_KERNEL);
	if (!di_devp) {
		pr_err("%s fail to allocate memory.\n", __func__);
		goto fail_kmalloc_dev;
	}
	de_devp = di_devp;
	memset(di_devp, 0, sizeof(struct di_dev_s));
	cdev_init(&(di_devp->cdev), &di_fops);
	di_devp->cdev.owner = THIS_MODULE;
	cdev_add(&(di_devp->cdev), di_devno, DI_COUNT);
	di_devp->devt = MKDEV(MAJOR(di_devno), 0);
	di_devp->dev = device_create(di_clsp, &pdev->dev,
		di_devp->devt, di_devp, "di%d", 0);

	if (di_devp->dev == NULL) {
		pr_error("device_create create error\n");
		ret = -EEXIST;
		return ret;
	}
	dev_set_drvdata(di_devp->dev, di_devp);
	platform_set_drvdata(pdev, di_devp);
	of_reserved_mem_device_init(&pdev->dev);
	ret = of_property_read_u32(pdev->dev.of_node,
		"flag_cma", &(di_devp->flag_cma));
	if (ret)
		pr_err("DI-%s: get flag_cma error.\n", __func__);
	else
		pr_info("DI-%s: flag_cma: %d\n", __func__, di_devp->flag_cma);
	if (di_devp->flag_cma == 1) {
#ifdef CONFIG_CMA
		di_devp->pdev = pdev;
		di_devp->mem_size = dma_get_cma_size_int_byte(&pdev->dev);
		pr_info("DI: CMA size 0x%x.\n", di_devp->mem_size);
#endif
		mem_flag = 0;
	} else {
		mem_flag = 1;
	}
	di_devp->di_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	di_devp->timerc_irq = irq_of_parse_and_map(pdev->dev.of_node, 1);
	pr_info("di_irq:%d,timerc_irq:%d\n",
		di_devp->di_irq, di_devp->timerc_irq);
#ifdef CONFIG_AML_RDMA
/* rdma handle */
	di_rdma_op.arg = di_devp;
	di_devp->rdma_handle = rdma_register(&di_rdma_op,
		di_devp, RDMA_TABLE_SIZE);
#endif
	di_pr_info("%s allocate rdma channel %d.\n", __func__,
		di_devp->rdma_handle);

	ret = of_property_read_u32(pdev->dev.of_node,
		"buffer-size", &(di_devp->buffer_size));
	if (ret)
		pr_err("DI-%s: get buffer size error.\n", __func__);

	ret = of_property_read_u32(pdev->dev.of_node,
		"hw-version", &(di_devp->hw_version));
	if (ret)
		pr_err("DI-%s: get hw version error.\n", __func__);
	di_pr_info("DI hw version %u.\n", di_devp->hw_version);

	vout_register_client(&display_mode_notifier_nb_v);

	/* set flag to indicate that post_wr is surpportted */
	ret = of_property_read_u32(pdev->dev.of_node,
				"post-wr-surpport",
				&(di_devp->post_wr_surpport));
	if (ret)
		post_wr_surpport = 0;
	else
		post_wr_surpport = di_devp->post_wr_surpport;

	ret = of_property_read_u32(pdev->dev.of_node,
		"nr10bit-surpport",
		&(di_devp->nr10bit_surpport));
	if (ret)
		nr10bit_surpport = 0;
	else
		nr10bit_surpport = di_devp->nr10bit_surpport;

	memset(&di_post_stru, 0, sizeof(di_post_stru));
	di_post_stru.next_canvas_id = 1;
#ifdef DI_USE_FIXED_CANVAS_IDX
	if (di_get_canvas()) {
		pr_dbg("DI get canvas error.\n");
		ret = -EEXIST;
		return ret;
	}
#endif
/* call di_add_reg_cfg() */
#ifdef NEW_DI_V1
	di_add_reg_cfg(&di_default_pre);
	di_add_reg_cfg(&di_default_post);
#endif

	device_create_file(di_devp->dev, &dev_attr_config);
	device_create_file(di_devp->dev, &dev_attr_debug);
	device_create_file(di_devp->dev, &dev_attr_dump_pic);
	device_create_file(di_devp->dev, &dev_attr_log);
	device_create_file(di_devp->dev, &dev_attr_parameters);
	device_create_file(di_devp->dev, &dev_attr_status);
	device_create_file(di_devp->dev, &dev_attr_provider_vframe_status);
	device_create_file(di_devp->dev, &dev_attr_frame_format);
	device_create_file(di_devp->dev, &dev_attr_pd_param);
	device_create_file(di_devp->dev, &dev_attr_tvp_region);
#ifdef NEW_DI_V4
	dnr_init(di_devp->dev);
#endif
/* get resource as memory,irq...*/
/* mem = &memobj; */
	for (i = 0; i < USED_LOCAL_BUF_MAX; i++)
		used_local_buf_index[i] = -1;

	used_post_buf_index = -1;
	init_flag = 0;
	reg_flag = 0;
	field_count = 0;

/* set start_frame_hold_count base on buffer size */
	di_devp->buf_num_avail = di_devp->mem_size / di_devp->buffer_size;
	if (di_devp->buf_num_avail > MAX_LOCAL_BUF_NUM)
		di_devp->buf_num_avail = MAX_LOCAL_BUF_NUM;

	buf_num_avail = di_devp->buf_num_avail;
/**/

	vf_receiver_init(&di_vf_recv, VFM_NAME, &di_vf_receiver, NULL);
	vf_reg_receiver(&di_vf_recv);
	active_flag = 1;
/* data32 = (*P_A9_0_IRQ_IN1_INTR_STAT_CLR); */
	ret = request_irq(di_devp->di_irq, &de_irq, IRQF_SHARED,
		"deinterlace", (void *)"deinterlace");

	sema_init(&di_sema, 1);
	di_sema_init_flag = 1;

	init_pd_para();
	di_hw_init();

	if (pulldown_enable)
		FlmVOFSftInt(&pd_param);

	set_di_flag();

/* Disable MCDI when code does not surpport MCDI */
	if (!mcpre_en)
		DI_VSYNC_WR_MPEG_REG_BITS(MCDI_MC_CRTL, 0, 0, 1);
#ifdef USE_HRTIMER
	tasklet_init(&di_pre_tasklet, pre_tasklet,
		(unsigned long)(&di_pre_stru));
	tasklet_disable(&di_pre_tasklet);
	hrtimer_init(&di_pre_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	di_pre_hrtimer.function = di_pre_hrtimer_func;
	hrtimer_start(&di_pre_hrtimer, ms_to_ktime(10), HRTIMER_MODE_REL);
	tasklet_enable(&di_pre_tasklet);
#else
	/* timer */
	INIT_WORK(&di_pre_work, di_per_work_func);
	init_timer(&di_pre_timer);
	di_pre_timer.data = (ulong) &di_pre_timer;
	di_pre_timer.function = di_pre_timer_cb;
	di_pre_timer.expires = jiffies + DI_PRE_INTERVAL;
	add_timer(&di_pre_timer);
	aml_cbus_update_bits(ISA_TIMER_MUX, 1 << 14, 0 << 14);
	aml_cbus_update_bits(ISA_TIMER_MUX, 3 << 4, 0 << 4);
	aml_cbus_update_bits(ISA_TIMER_MUX, 1 << 18, 1 << 18);

	ret = request_irq(di_devp->timerc_irq, &timer_irq,
		IRQF_SHARED, "timerC",
		(void *)"timerC");
#endif
	di_devp->task = kthread_run(di_task_handle, di_devp, "kthread_di");
	if (IS_ERR(di_devp->task))
		pr_err("%s create kthread error.\n", __func__);
	di_set_power_control(0, 0);
	di_set_power_control(1, 0);
fail_kmalloc_dev:
	return ret;
}

static int di_remove(struct platform_device *pdev)
{
	struct di_dev_s *di_devp = NULL;

	di_devp = platform_get_drvdata(pdev);

	di_hw_uninit();
	di_devp->di_event = 0xff;
	kthread_stop(di_devp->task);
#ifdef USE_HRTIMER
	hrtimer_cancel(&di_pre_hrtimer);
	tasklet_disable(&di_pre_tasklet);
	tasklet_kill(&di_pre_tasklet);
#else
	del_timer(&di_pre_timer);
#endif
#ifdef CONFIG_AML_RDMA
/* rdma handle */
	if (di_devp->rdma_handle > 0)
		rdma_unregister(di_devp->rdma_handle);
#endif

	vf_unreg_provider(&di_vf_prov);
	vf_unreg_receiver(&di_vf_recv);

	di_uninit_buf();
	di_set_power_control(0, 0);
	di_set_power_control(1, 0);
/* Remove the cdev */
	device_remove_file(di_devp->dev, &dev_attr_config);
	device_remove_file(di_devp->dev, &dev_attr_debug);
	device_remove_file(di_devp->dev, &dev_attr_log);
	device_remove_file(di_devp->dev, &dev_attr_dump_pic);
	device_remove_file(di_devp->dev, &dev_attr_parameters);
	device_remove_file(di_devp->dev, &dev_attr_status);

	cdev_del(&di_devp->cdev);

	device_destroy(di_clsp, di_devno);
	kfree(di_devp);
/* free drvdata */
	dev_set_drvdata(&pdev->dev, NULL);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int save_init_flag;
static int save_mem_flag;
static int di_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifndef USE_HRTIMER
	aml_cbus_update_bits(ISA_TIMER_MUX, 1 << 18, 0 << 18);
#endif

/* fix suspend/resume crash problem */
	save_init_flag = init_flag;
	save_mem_flag = mem_flag;
	init_flag = 0;
	mem_flag = 0;
	field_count = 0;
	if (di_pre_stru.di_inp_buf) {
		if (vframe_in[di_pre_stru.di_inp_buf->index]) {
			vf_put(vframe_in[di_pre_stru.di_inp_buf->index],
				VFM_NAME);
			vframe_in[di_pre_stru.di_inp_buf->index] = NULL;
			vf_notify_provider(VFM_NAME,
				VFRAME_EVENT_RECEIVER_PUT, NULL);
		}
	}


	di_set_power_control(0, 0);
	di_set_power_control(1, 0);
	di_pr_info("di: di_suspend\n");
	return 0;
}

static int di_resume(struct platform_device *pdev)
{
	init_flag = save_init_flag;
	mem_flag = save_mem_flag;
	if (init_flag && mem_flag) {
		di_set_power_control(0, 1);
		di_set_power_control(1, 1);
	}
#ifndef USE_HRTIMER
	aml_cbus_update_bits(ISA_TIMER_MUX, 1 << 14, 0 << 14);
	aml_cbus_update_bits(ISA_TIMER_MUX, 3 << 4, 0 << 4);
	aml_cbus_update_bits(ISA_TIMER_MUX, 1 << 18, 1 << 18);
	aml_write_cbus(ISA_TIMERC, 1);
#endif
	di_pr_info("di_hdmirx: resume module\n");
	return 0;
}
#endif

/* #ifdef CONFIG_USE_OF */
static const struct of_device_id amlogic_deinterlace_dt_match[] = {
	{ .compatible = "amlogic, deinterlace", },
	{},
};
/* #else */
/* #define amlogic_deinterlace_dt_match NULL */
/* #endif */

static struct platform_driver di_driver = {
	.probe			= di_probe,
	.remove			= di_remove,
#ifdef CONFIG_PM
	.suspend		= di_suspend,
	.resume			= di_resume,
#endif
	.driver			= {
		.name		= DEVICE_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = amlogic_deinterlace_dt_match,
	}
};

static int __init di_module_init(void)
{
	int ret = 0;

	di_pr_info("%s ok.\n", __func__);
	if (boot_init_flag & INIT_FLAG_NOT_LOAD)
		return 0;

	ret = alloc_chrdev_region(&di_devno, 0, DI_COUNT, DEVICE_NAME);
	if (ret < 0) {
		pr_err("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	di_pr_info("%s: major %d\n", __func__, MAJOR(di_devno));
	di_clsp = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(di_clsp)) {
		ret = PTR_ERR(di_clsp);
		pr_err("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}

	ret = platform_driver_register(&di_driver);
	if (ret != 0) {
		pr_err("%s: failed to register driver\n", __func__);
		goto fail_pdrv_register;
	}
	return 0;
fail_pdrv_register:
	class_destroy(di_clsp);
fail_class_create:
	unregister_chrdev_region(di_devno, DI_COUNT);
fail_alloc_cdev_region:
	return ret;
}




static void __exit di_module_exit(void)
{
	class_destroy(di_clsp);
	unregister_chrdev_region(di_devno, DI_COUNT);
	platform_driver_unregister(&di_driver);
	return;
}

MODULE_PARM_DESC(bypass_hd, "\n bypass_hd\n");
module_param(bypass_hd, int, 0664);

MODULE_PARM_DESC(bypass_hd_prog, "\n bypass_hd_prog\n");
module_param(bypass_hd_prog, int, 0664);

MODULE_PARM_DESC(bypass_interlace_output, "\n bypass_interlace_output\n");
module_param(bypass_interlace_output, int, 0664);

MODULE_PARM_DESC(bypass_prog, "\n bypass_prog\n");
module_param(bypass_prog, int, 0664);

MODULE_PARM_DESC(bypass_superd, "\n bypass_superd\n");
module_param(bypass_superd, int, 0664);

MODULE_PARM_DESC(bypass_all, "\n bypass_all\n");
module_param(bypass_all, int, 0664);

MODULE_PARM_DESC(bypass_pre, "\n bypass_all\n");
module_param(bypass_pre, int, 0664);
MODULE_PARM_DESC(bypass_1080p, "\n bypass_1080p\n");
module_param(bypass_1080p, int, 0664);

MODULE_PARM_DESC(bypass_3d, "\n bypass_3d\n");
module_param(bypass_3d, int, 0664);

MODULE_PARM_DESC(bypass_dynamic, "\n bypass_dynamic\n");
module_param(bypass_dynamic, int, 0664);

MODULE_PARM_DESC(bypass_trick_mode, "\n bypass_trick_mode\n");
module_param(bypass_trick_mode, int, 0664);

MODULE_PARM_DESC(invert_top_bot, "\n invert_top_bot\n");
module_param(invert_top_bot, int, 0664);

MODULE_PARM_DESC(skip_top_bot, "\n skip_top_bot\n");
module_param(skip_top_bot, int, 0664);

MODULE_PARM_DESC(force_width, "\n force_width\n");
module_param(force_width, int, 0664);

MODULE_PARM_DESC(force_height, "\n force_height\n");
module_param(force_height, int, 0664);

MODULE_PARM_DESC(bypass_get_buf_threshold, "\n bypass_get_buf_threshold\n");
module_param(bypass_get_buf_threshold, uint, 0664);

MODULE_PARM_DESC(bypass_hdmi_get_buf_threshold, "\n bypass_hdmi_get_buf_threshold\n");
module_param(bypass_hdmi_get_buf_threshold, uint, 0664);

MODULE_PARM_DESC(pulldown_detect, "\n pulldown_detect\n");
module_param(pulldown_detect, int, 0664);

MODULE_PARM_DESC(prog_proc_config, "\n prog_proc_config\n");
module_param(prog_proc_config, int, 0664);

MODULE_PARM_DESC(skip_wrong_field, "\n skip_wrong_field\n");
module_param(skip_wrong_field, int, 0664);

MODULE_PARM_DESC(frame_count, "\n frame_count\n");
module_param(frame_count, int, 0664);

MODULE_PARM_DESC(start_frame_drop_count, "\n start_frame_drop_count\n");
module_param(start_frame_drop_count, int, 0664);

#ifdef SUPPORT_START_FRAME_HOLD
MODULE_PARM_DESC(start_frame_hold_count, "\n start_frame_hold_count\n");
module_param(start_frame_hold_count, int, 0664);
#endif

module_init(di_module_init);
module_exit(di_module_exit);

MODULE_PARM_DESC(same_w_r_canvas_count,
	"\n canvas of write and read are same\n");
module_param(same_w_r_canvas_count, long, 0664);

MODULE_PARM_DESC(same_field_top_count, "\n same top field\n");
module_param(same_field_top_count, long, 0664);

MODULE_PARM_DESC(same_field_bot_count, "\n same bottom field\n");
module_param(same_field_bot_count, long, 0664);

MODULE_PARM_DESC(same_field_source_flag_th, "\n same_field_source_flag_th\n");
module_param(same_field_source_flag_th, int, 0664);

MODULE_PARM_DESC(pulldown_count, "\n pulldown_count\n");
module_param(pulldown_count, long, 0664);

MODULE_PARM_DESC(pulldown_buffer_mode, "\n pulldown_buffer_mode\n");
module_param(pulldown_buffer_mode, long, 0664);

MODULE_PARM_DESC(pulldown_win_mode, "\n pulldown_win_mode\n");
module_param(pulldown_win_mode, long, 0664);

MODULE_PARM_DESC(di_log_flag, "\n di log flag\n");
module_param(di_log_flag, int, 0664);

MODULE_PARM_DESC(di_debug_flag, "\n di debug flag\n");
module_param(di_debug_flag, int, 0664);

MODULE_PARM_DESC(buf_state_log_threshold, "\n buf_state_log_threshold\n");
module_param(buf_state_log_threshold, int, 0664);

MODULE_PARM_DESC(bypass_state, "\n bypass_state\n");
module_param(bypass_state, uint, 0664);

MODULE_PARM_DESC(di_vscale_skip_enable, "\n di_vscale_skip_enable\n");
module_param(di_vscale_skip_enable, uint, 0664);

MODULE_PARM_DESC(di_vscale_skip_count, "\n di_vscale_skip_count\n");
module_param(di_vscale_skip_count, int, 0664);

MODULE_PARM_DESC(di_vscale_skip_count_real, "\n di_vscale_skip_count_real\n");
module_param(di_vscale_skip_count_real, int, 0664);

module_param_named(vpp_3d_mode, vpp_3d_mode, int, 0664);
#ifdef DET3D
MODULE_PARM_DESC(det3d_en, "\n det3d_enable\n");
module_param(det3d_en, bool, 0664);
MODULE_PARM_DESC(det3d_mode, "\n det3d_mode\n");
module_param(det3d_mode, uint, 0664);
#endif

MODULE_PARM_DESC(pre_hold_line, "\n pre_hold_line\n");
module_param(pre_hold_line, uint, 0664);

MODULE_PARM_DESC(pre_urgent, "\n pre_urgent\n");
module_param(pre_urgent, uint, 0664);

MODULE_PARM_DESC(post_hold_line, "\n post_hold_line\n");
module_param(post_hold_line, uint, 0664);

MODULE_PARM_DESC(post_urgent, "\n post_urgent\n");
module_param(post_urgent, uint, 0664);

MODULE_PARM_DESC(force_duration_0, "\n force_duration_0\n");
module_param(force_duration_0, uint, 0664);

MODULE_PARM_DESC(use_reg_cfg, "\n use_reg_cfg\n");
module_param(use_reg_cfg, uint, 0664);

MODULE_PARM_DESC(di_printk_flag, "\n di_printk_flag\n");
module_param(di_printk_flag, uint, 0664);

MODULE_PARM_DESC(force_recovery, "\n force_recovery\n");
module_param(force_recovery, uint, 0664);

MODULE_PARM_DESC(force_recovery_count, "\n force_recovery_count\n");
module_param(force_recovery_count, uint, 0664);

MODULE_PARM_DESC(pre_process_time_force, "\n pre_process_time_force\n");
module_param(pre_process_time_force, uint, 0664);

MODULE_PARM_DESC(pre_process_time, "\n pre_process_time\n");
module_param(pre_process_time, uint, 0664);

#ifdef RUN_DI_PROCESS_IN_IRQ
MODULE_PARM_DESC(input2pre, "\n input2pre\n");
module_param(input2pre, uint, 0664);

MODULE_PARM_DESC(input2pre_buf_miss_count, "\n input2pre_buf_miss_count\n");
module_param(input2pre_buf_miss_count, uint, 0664);

MODULE_PARM_DESC(input2pre_proc_miss_count, "\n input2pre_proc_miss_count\n");
module_param(input2pre_proc_miss_count, uint, 0664);

MODULE_PARM_DESC(input2pre_miss_policy, "\n input2pre_miss_policy\n");
module_param(input2pre_miss_policy, uint, 0664);

MODULE_PARM_DESC(input2pre_throw_count, "\n input2pre_throw_count\n");
module_param(input2pre_throw_count, uint, 0664);
#endif

MODULE_PARM_DESC(timeout_miss_policy, "\n timeout_miss_policy\n");
module_param(timeout_miss_policy, uint, 0664);

MODULE_PARM_DESC(bypass_post, "\n bypass_post\n");
module_param(bypass_post, uint, 0664);

MODULE_PARM_DESC(post_wr_en, "\n post write enable\n");
module_param(post_wr_en, bool, 0664);

MODULE_PARM_DESC(post_wr_surpport, "\n post write enable\n");
module_param(post_wr_surpport, uint, 0664);

MODULE_PARM_DESC(bypass_post_state, "\n bypass_post_state\n");
module_param(bypass_post_state, uint, 0664);

MODULE_PARM_DESC(force_update_post_reg, "\n force_update_post_reg\n");
module_param(force_update_post_reg, uint, 0664);

MODULE_PARM_DESC(update_post_reg_count, "\n update_post_reg_count\n");
module_param(update_post_reg_count, uint, 0664);

module_param(use_2_interlace_buff, int, 0664);
MODULE_PARM_DESC(use_2_interlace_buff,
	"/n debug for progress interlace mixed source /n");

module_param(pulldown_mode, int, 0664);
MODULE_PARM_DESC(pulldown_mode, "\n option for pulldown\n");

module_param(reg_cnt, uint, 0664);
MODULE_PARM_DESC(reg_cnt, "\n cnt for reg\n");

module_param(unreg_cnt, uint, 0664);
MODULE_PARM_DESC(unreg_cnt, "\n cnt for unreg\n");

module_param(debug_blend_mode, int, 0664);
MODULE_PARM_DESC(debug_blend_mode, "\n force post blend mode\n");

module_param(nr10bit_surpport, uint, 0664);
MODULE_PARM_DESC(nr10bit_surpport, "\n nr10bit surpport flag\n");

module_param(di_stop_reg_flag, uint, 0664);
MODULE_PARM_DESC(di_stop_reg_flag, "\n di_stop_reg_flag\n");

module_param_array(di_stop_reg_addr, uint, &num_di_stop_reg_addr,
	0664);

module_param(static_pic_threshold, int, 0664);
MODULE_PARM_DESC(static_pic_threshold, "/n threshold for static pic /n");
MODULE_DESCRIPTION("AMLOGIC DEINTERLACE driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");


static char *next_token_ex(char *seperator, char *buf, unsigned size,
			   unsigned offset, unsigned *token_len,
			   unsigned *token_offset)
{       /* besides characters defined in seperator, '\"' are used as
	 * seperator; and any characters in '\"' will not act as seperator */
	char *pToken = NULL;
	char last_seperator = 0;
	char trans_char_flag = 0;

	if (buf) {
		for (; offset < size; offset++) {
			int ii = 0;
			char ch;
			if (buf[offset] == '\\') {
				trans_char_flag = 1;
				continue;
			}
			while (((ch = seperator[ii]) != buf[offset]) && (ch))
				ii++;
			if (ch) {
				if (!pToken) {
					continue;
				} else {
					if (last_seperator != '"') {
						*token_len = (unsigned)
						(buf + offset - pToken);
						*token_offset = offset;
						return pToken;
					}
				}
			} else if (!pToken) {
				if (trans_char_flag && (buf[offset] == '"'))
					last_seperator = buf[offset];
				pToken = &buf[offset];
			} else if (
				(trans_char_flag && (buf[offset] == '"')) &&
				(last_seperator == '"')) {
				*token_len =
					(unsigned)(buf + offset - pToken - 2);
				*token_offset = offset + 1;
				return pToken + 1;
			}
			trans_char_flag = 0;
		}
		if (pToken) {
			*token_len = (unsigned)(buf + offset - pToken);
			*token_offset = offset;
		}
	}
	return pToken;
}

static int __init di_boot_para_setup(char *s)
{
	char separator[] = { ' ', ',', ';', 0x0 };
	char *token;
	unsigned token_len, token_offset, offset = 0;
	int size = strlen(s);

	do {
		token = next_token_ex(separator, s, size,
			offset, &token_len, &token_offset);
		if (token) {
			if ((token_len == 3) &&
			    (strncmp(token, "off", token_len) == 0))
				boot_init_flag |= INIT_FLAG_NOT_LOAD;
		}
		offset = token_offset;
	} while (token);
	return 0;
}

__setup("di=", di_boot_para_setup);


vframe_t *get_di_inp_vframe(void)
{
	vframe_t *vframe = NULL;

	if (di_pre_stru.di_inp_buf)
		vframe = di_pre_stru.di_inp_buf->vframe;
	return vframe;
}

module_param_named(full_422_pack, full_422_pack, bool, 0644);
#ifdef DEBUG_SUPPORT
module_param_named(di_pre_rdma_enable, di_pre_rdma_enable, uint, 0664);
module_param_named(pldn_dly, pldn_dly, uint, 0644);
module_param_named(tbbtff_dly, tbbtff_dly, uint, 0644);

module_param(pldn_dly1, uint, 0644);
MODULE_PARM_DESC(pldn_dly1, "/n pulldonw field delay result./n");

module_param(flm22_sure_num, uint, 0644);
MODULE_PARM_DESC(flm22_sure_num, "ture film-22/n");

module_param(flm22_glbpxlnum_rat, uint, 0644);
MODULE_PARM_DESC(flm22_glbpxlnum_rat, "flm22_glbpxlnum_rat/n");
#endif
