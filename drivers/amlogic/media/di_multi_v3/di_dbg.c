/*
 * drivers/amlogic/media/di_multi_v3/di_dbg.c
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
#include <linux/debugfs.h>

#include "di_data.h"
#include "di_dbg.h"

#include "di_reg_tab.h"
#include "deinterlace.h"
#include "deinterlace_dbg.h"
#include "deinterlace_hw.h"
#include "di_data_l.h"
#include "di_que.h"
#include "di_task.h"
#include "di_prc.h"
#include "di_pre.h"
#include "di_post.h"
#include "di_vfm_test.h"
/********************************
 *trace:
 *******************************/
#define CREATE_TRACE_POINTS
#include "dim_trace.h"

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE dim_trace
#include <trace/define_trace.h>

/**********************/
/* data get */
static struct dentry **dich_get_dbgroot(unsigned int ch)
{
	return &get_datal()->ch_data[ch].dbg_rootx;
}

static struct dentry **dich_get_dbgroot_top(void)
{
	return &get_datal()->dbg_root_top;
}

static int *di_get_plane(void)
{
	return &get_datal()->plane[0];
}

static struct vframe_s *di_get_dbg_vframe_in(unsigned int ch)
{
	return &get_datal()->ch_data[ch].dbg_data.vfm_input;
}

static struct vframe_s **di_get_dbg_vframe_out(unsigned int ch)
{
	return &get_datal()->ch_data[ch].dbg_data.pfm_out;
}

/********************************
 *timer:
 *******************************/

u64 curv3_to_msecs(void)
{
	u64 cur = sched_clock();

	do_div(cur, NSEC_PER_MSEC);
	return cur;
}

u64 curv3_to_usecs(void)/*2019*/
{
	u64 cur = sched_clock();

	do_div(cur, NSEC_PER_USEC);
	return cur;
}

/********************************
 *trace:
 *******************************/

static void trace_pre(unsigned int index, unsigned long ctime)
{
	trace_dimv3_pre("PRE-IRQ-0", index, ctime);
}

static void trace_post(unsigned int index, unsigned long ctime)
{
	trace_dimv3_post("POST-IRQ-1", index, ctime);
}

#define DI_TRACE_LIMIT		10000
static void trace_pre_get(unsigned int index)
{
	u64 ustime;

	if (div3_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = curv3_to_usecs();
	trace_dimv3_pre_getxx("PRE-GET-01", index, ustime);
}

static void trace_pre_set(unsigned int index)
{
	u64 ustime;

	if (div3_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = curv3_to_usecs();
	trace_dimv3_pre_setxx("PRE-SET-01", index, ustime);
}

static void trace_pre_ready(unsigned int index)
{
	u64 ustime;

	if (div3_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = curv3_to_usecs();
	trace_dimv3_pre_ready("PRE-READY2", index, ustime);
}

static void trace_post_ready(unsigned int index)
{
	u64 ustime;

	if (div3_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = curv3_to_usecs();
	trace_dimv3_pst_ready("PST-READY3", index, ustime);
}

static void trace_post_get(unsigned int index)
{
	u64 ustime;

	if (div3_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = curv3_to_usecs();
	trace_dimv3_pst_getxx("PST-GET-04", index, ustime);
}

static void trace_post_get2(unsigned int index)
{
	u64 ustime;

	if (div3_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = curv3_to_usecs();
	trace_dimv3_pst_get2x("PST-GET-0a", index, ustime);
}

static void trace_post_set(unsigned int index)
{
	u64 ustime;

	if (div3_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = curv3_to_usecs();
	trace_dimv3_pst_setxx("PST-SET-05", index, ustime);
}

static void trace_post_irq(unsigned int index)
{
	u64 ustime;

	if (div3_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = curv3_to_usecs();
	trace_dimv3_pst_irxxx("PST-IRQ-06", index, ustime);
}

static void trace_post_doing(unsigned int index)
{
	u64 ustime;

	if (div3_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = curv3_to_usecs();
	trace_dimv3_pst_doing("PST-DOI-07", index, ustime);
}

static void trace_post_peek(unsigned int index)
{
	u64 ustime;

	if (div3_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = curv3_to_usecs();
	trace_dimv3_pst_peekx("PST-PEEK-8", index, ustime);
}

const struct dim_tr_ops_s dimv3_tr_ops = {
	.pre = trace_pre,
	.post = trace_post,

	.pre_get = trace_pre_get,
	.pre_set = trace_pre_set,
	.pre_ready = trace_pre_ready,
	.post_ready = trace_post_ready,
	.post_get = trace_post_get,
	.post_get2 = trace_post_get2,

	.post_set = trace_post_set,
	.post_ir = trace_post_irq,
	.post_do = trace_post_doing,
	.post_peek = trace_post_peek,
};

static unsigned int seq_get_channel(struct seq_file *s)
{
	int *pCh;

	pCh = (int *)s->private;
	return *pCh;
}

/********************************
 *debug register:
 *******************************/
/* also see enum eDI_DBG_MOD */
const char * const dbgv3_mode_name[] = {
	"REGB",
	"REGE",
	"UNREGB",
	"UNREGE",
	"PRE_SETB",
	"PRE_SETE",
	"PRE_DONEB",
	"PRE_DONEE",
	"PST_SETB",
	"PST_SETE",
	"PST_IRQB",
	"PST_IRQE",
	"PST_DB",
	"PST_DE",
	"PST_CH_CHG",
	"PST_TOUT",
	"RVB",
	"RVE",
	"PST_RESIZE"
};

const char *ddbgv3_get_mod_name(unsigned int mod)
{
	if (mod >= eDI_DBG_MOD_END)
		return "nothing!";

	return dbgv3_mode_name[mod];
}

void ddbgv3_reg_save(unsigned int addr, unsigned int val,
		   unsigned int st, unsigned int bw)
{
	struct di_dbg_reg dbg_reg;
	struct di_dbg_reg_log *plog = get_dbg_reg_log();
	unsigned int pos;

	if (!plog->en_reg)
		return;
	if (plog->en_notoverwrite && plog->overflow)
		return;

	pos = plog->pos;

	dbg_reg.addr = addr;
	dbg_reg.val = val;
	dbg_reg.st_bit = st;
	dbg_reg.b_w = bw;
	dbg_reg.res = 0;

	plog->log[pos].reg = dbg_reg;
	pos++;
	if (pos >= (K_DI_SIZE_REG_LOG - 1)) {
		if (plog->en_notoverwrite) {
			plog->overflow = 1;

		} else {
			pos = 0;
			plog->overflow = 1;
		}
	}
	plog->wsize++;
	plog->pos = pos;
}

/* mod also see: enum eDI_DBG_MOD */
void dimv3_ddbg_mod_save(unsigned int mod, unsigned int ch, unsigned int cnt)
{
	struct di_dbg_mod dbg_mod;
	struct di_dbg_reg_log *plog = get_dbg_reg_log();
	unsigned int pos;
#if 1
/*--------------------------*/
	if (ch)
		hv3_dbg_reg_set(mod | 0x80000000);
	else
		hv3_dbg_reg_set(mod);
/*--------------------------*/
#endif
	if (!plog->en_mod)
		return;
	if (plog->en_notoverwrite && plog->overflow)
		return;
	pos = plog->pos;

	dbg_mod.lable = K_DI_LAB_MOD;
	dbg_mod.ch = ch;
	dbg_mod.mod = mod;
	dbg_mod.cnt = cnt;
	dbg_mod.res = 0;

	plog->log[pos].mod = dbg_mod;
	pos++;
	if (pos >= (K_DI_SIZE_REG_LOG - 1)) {
		if (plog->en_notoverwrite) {
			plog->overflow = 1;

		} else {
			pos = 0;
			plog->overflow = 1;
		}
	}
	plog->wsize++;
	plog->pos = pos;
}

#if 0
void ddbgv3_sw(bool on)
{
	struct di_dbg_reg_log *plog = get_dbg_reg_log();

	plog->en = on;
}
#else

void ddbgv3_sw(enum eDI_LOG_TYPE mode, bool on)
{
	struct di_dbg_reg_log *plog = get_dbg_reg_log();

	switch (mode) {
	case eDI_LOG_TYPE_ALL:
		plog->en_all = on;
		break;
	case eDI_LOG_TYPE_REG:
		plog->en_reg = on;
		break;
	case eDI_LOG_TYPE_MOD:
		plog->en_mod = on;
		break;
	default:
		PR_WARN("%s:mode overlow:%d\n", __func__, mode);
		break;
	}
}
#endif
void ddbgv3_reg_clear(void)
{
	struct di_dbg_reg_log *plog = get_dbg_reg_log();

	memset(plog, 0, sizeof(struct di_dbg_reg_log));
	plog->en_notoverwrite = 1;
}

static int ddbg_log_reg_show(struct seq_file *seq, void *v)
{
	struct di_dbg_reg_log *plog = get_dbg_reg_log();
	unsigned int pos;
	int i;

	if (plog->overflow)
		pos = K_DI_SIZE_REG_LOG;
	else
		pos = plog->pos;

	seq_printf(seq, "%s:pos=%d,overflow=%d, size=%d\n",
		   __func__, plog->pos, plog->overflow, plog->wsize);

	for (i = 0; i < pos; i++) {
		if (plog->log[i].mod.lable == K_DI_LAB_MOD) {
			seq_printf(seq, "%d,ch[%d]:cnt[%d]:%s\n",
				   i,
				   plog->log[i].mod.ch,
				   plog->log[i].mod.cnt,
				   ddbgv3_get_mod_name(plog->log[i].mod.mod));
			continue;
		}

		seq_printf(seq, "\t0x%x,0x%x,%d,%d\n",
			   plog->log[i].reg.addr,
			   plog->log[i].reg.val,
			   plog->log[i].reg.st_bit,
			   plog->log[i].reg.b_w);
	}

	return 0;
}

static ssize_t ddbg_log_reg_store(struct file *file, const char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	unsigned int item, val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	switch (ret) {
	case 2:
		if (item == 0 && val == 0) {
			/*help info:*/
			pr_info("help:\n");
			/*all:*/
			pr_info("\t1 0 : log all disable\n");
			pr_info("\t1 1 : log all enable\n");
			/*reg:*/
			pr_info("\t2 0 : log reg disable\n");
			pr_info("\t2 1 : log reg enable\n");
			/*mod:*/
			pr_info("\t3 0 : log mod disable\n");
			pr_info("\t3 1 : log mod enable\n");
			/*clean:*/
			pr_info("\t4 0 : clear\n");
			break;
		}
		switch (item) {
		case 1:
		case 2:
		case 3:
			pr_info("ddbgv3_sw: %d\n", val);
			ddbgv3_sw(item, val);
			break;
		case 4:
			pr_info("ddbgv3_reg_clear\n");
			ddbgv3_reg_clear();
			break;
		};

		break;
	default:
		pr_info("err:please enter: 0 x for help\n");
		return -EINVAL;
	}

	return count;
}

/**********************/
int seq_file_vframe(struct seq_file *seq, void *v, struct vframe_s *pVfm)
{
	struct canvas_config_s *pcvs;

	pcvs = &pVfm->canvas0_config[0];
	if (!pVfm) {
		seq_puts(seq, "war: dump vframe NULL\n");
		return 0;
	}
	seq_printf(seq, "%-15s:0x%p\n", "addr", pVfm);
	seq_printf(seq, "%-15s:%d\n", "index", pVfm->index);
	seq_printf(seq, "%-15s:%d\n", "index_disp", pVfm->index_disp);
	seq_printf(seq, "%-15s:%d\n", "omx_index", pVfm->omx_index);
	seq_printf(seq, "%-15s:0x%x\n", "type", pVfm->type);
	seq_printf(seq, "%-15s:0x%x\n", "type_backup", pVfm->type_backup);
	seq_printf(seq, "%-15s:0x%x\n", "type_original", pVfm->type_original);
	seq_printf(seq, "%-15s:%d\n", "blend_mode", pVfm->blend_mode);
	seq_printf(seq, "%-15s:%d\n", "duration",  pVfm->duration);
	seq_printf(seq, "%-15s:%d\n", "duration_pull", pVfm->duration_pulldown);
	seq_printf(seq, "%-15s:%d\n", "pts", pVfm->pts);

	seq_printf(seq, "%-15s:%lld\n", "pts_us64", pVfm->pts_us64);
	seq_printf(seq, "%-15s:%d\n", "next_vf_pts_valid",
		   pVfm->next_vf_pts_valid);
	seq_printf(seq, "%-15s:%d\n", "next_vf_pts", pVfm->next_vf_pts);
	seq_printf(seq, "%-15s:%d\n", "disp_pts", pVfm->disp_pts);
	seq_printf(seq, "%-15s:%lld\n", "disp_pts_us64", pVfm->disp_pts_us64);
	seq_printf(seq, "%-15s:%lld\n", "timestamp", pVfm->timestamp);
	seq_printf(seq, "%-15s:%d\n", "flag", pVfm->flag);
	seq_printf(seq, "%-15s:0x%x\n", "canvas0Addr", pVfm->canvas0Addr);
	seq_printf(seq, "%-15s:0x%x\n", "canvas1Addr", pVfm->canvas1Addr);
	seq_printf(seq, "%-15s:0x%x\n", "compHeadAddr", pVfm->compHeadAddr);
	seq_printf(seq, "%-15s:0x%x\n", "compBodyAddr", pVfm->compBodyAddr);
	seq_printf(seq, "%-15s:%d\n", "plane_num", pVfm->plane_num);

	seq_printf(seq, "%-15s:%d\n", "bufWidth", pVfm->bufWidth);
	seq_printf(seq, "%-15s:%d\n", "width", pVfm->width);
	seq_printf(seq, "%-15s:%d\n", "height", pVfm->height);
	seq_printf(seq, "%-15s:%d\n", "compWidth", pVfm->compWidth);
	seq_printf(seq, "%-15s:%d\n", "compHeight", pVfm->compHeight);
	seq_printf(seq, "%-15s:%d\n", "ratio_control", pVfm->ratio_control);
	seq_printf(seq, "%-15s:%d\n", "bitdepth", pVfm->bitdepth);
	seq_printf(seq, "%-15s:%d\n", "signal_type", pVfm->signal_type);

	/*
	 *	   bit 29: present_flag
	 *	   bit 28-26: video_format
	 *	   "component", "PAL", "NTSC", "SECAM",
	 *	   "MAC", "unspecified"
	 *	   bit 25: range "limited", "full_range"
	 *	   bit 24: color_description_present_flag
	 *	   bit 23-16: color_primaries
	 *	   "unknown", "bt709", "undef", "bt601",
	 *	   "bt470m", "bt470bg", "smpte170m", "smpte240m",
	 *	   "film", "bt2020"
	 *	   bit 15-8: transfer_characteristic
	 *	   "unknown", "bt709", "undef", "bt601",
	 *	   "bt470m", "bt470bg", "smpte170m", "smpte240m",
	 *	   "linear", "log100", "log316", "iec61966-2-4",
	 *	   "bt1361e", "iec61966-2-1", "bt2020-10", "bt2020-12",
	 *	   "smpte-st-2084", "smpte-st-428"
	 *	   bit 7-0: matrix_coefficient
	 *	   "GBR", "bt709", "undef", "bt601",
	 *	   "fcc", "bt470bg", "smpte170m", "smpte240m",
	 *	   "YCgCo", "bt2020nc", "bt2020c"
	 */
	seq_printf(seq, "%-15s:0x%x\n", "orientation", pVfm->orientation);
	seq_printf(seq, "%-15s:0x%x\n", "video_angle", pVfm->video_angle);
	seq_printf(seq, "%-15s:0x%x\n", "source_type", pVfm->source_type);

	seq_printf(seq, "%-15s:0x%x\n", "phase", pVfm->phase);
	seq_printf(seq, "%-15s:0x%x\n", "source_mode", pVfm->source_mode);
	seq_printf(seq, "%-15s:0x%x\n", "sig_fmt", pVfm->sig_fmt);
	seq_printf(seq, "%-15s:0x%x\n", "trans_fmt", pVfm->trans_fmt);

	seq_printf(seq, "%-15s:0x%x\n", "mode_3d_enable",
		   pVfm->mode_3d_enable);

	seq_printf(seq, "%-15s:0x%p\n", "early_process_fun",
		   pVfm->early_process_fun);
	seq_printf(seq, "%-15s:0x%p\n", "process_fun",
		   pVfm->early_process_fun);
	seq_printf(seq, "%-15s:0x%p\n", "private_data",
		   pVfm->early_process_fun);

#if 1
	/* vframe properties */

#endif

	/* pixel aspect ratio */
	seq_printf(seq, "%-15s:%d\n", "pixel_ratio", pVfm->pixel_ratio);

	/* ready from decode on  jiffies_64 */
	seq_printf(seq, "%-15s:%d\n", "use_cnt", atomic_read(&pVfm->use_cnt));
	seq_printf(seq, "%-15s:%d\n", "frame_dirty", pVfm->frame_dirty);
	/*
	 *prog_proc_config:
	 *1: process p from decoder as filed
	 *0: process p from decoder as frame
	 */
	seq_printf(seq, "%-15s:0x%x\n", "prog_proc_config",
		   pVfm->prog_proc_config);
		/* used for indicate current video is motion or static */
	seq_printf(seq, "%-15s:%d\n", "combing_cur_lev",
		   pVfm->combing_cur_lev);
	/*canvas:*/
	seq_printf(seq, "%-15s:0x%x\n", "phy_addr",
		   pcvs->phy_addr);
	seq_printf(seq, "%-15s:%d\n", "width",
		   pcvs->width);
	seq_printf(seq, "%-15s:%d\n", "height",
		   pcvs->height);
	seq_printf(seq, "%-15s:%d\n", "block_mode",
		   pcvs->block_mode);

	return 0;
}

/**********************/
/* debug input vframe */
/**********************/
void didbgv3_vframe_in_copy(unsigned int ch, struct vframe_s *pvfm)
{
	struct vframe_s *pvfm_t;

	if (!div3_cfgx_get(ch, eDI_DBG_CFGX_IDX_VFM_IN))
		return;

	pvfm_t = di_get_dbg_vframe_in(ch);

	memcpy(pvfm_t, pvfm, sizeof(struct vframe_s));
}

static int seq_file_vframe_in_show(struct seq_file *seq, void *v)
{
	unsigned int ch;

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	if (!div3_cfgx_get(ch, eDI_DBG_CFGX_IDX_VFM_IN)) {
		seq_puts(seq, "war: cfg[eDI_DBG_CFGX_IDX_VFM_IN] disable\n");
		return 0;
	}

	seq_file_vframe(seq, v, di_get_dbg_vframe_in(ch));

	return 0;
}

/***********************/
/* debug output vframe */
/***********************/
void didbgv3_vframe_out_save(struct vframe_s *pvfm)
{
	unsigned int ch;
	struct vframe_s **pvfm_t;

	ch = DI_SUB_ID_S0;
	if (!div3_cfgx_get(ch, eDI_DBG_CFGX_IDX_VFM_OT))
		return;

	pvfm_t = di_get_dbg_vframe_out(ch);
	*pvfm_t = pvfm;
}

static int seq_file_vframe_out_show(struct seq_file *seq, void *v)
{
	unsigned int ch;

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	if (!div3_cfgx_get(ch, eDI_DBG_CFGX_IDX_VFM_OT)) {
		seq_puts(seq, "war: cfg[eDI_DBG_CFGX_IDX_VFM_OT] disable\n");
		return 0;
	}

	seq_file_vframe(seq, v, *di_get_dbg_vframe_out(ch));

	return 0;
}
/**********************************************************
 * debug display vframe
 *********************************************************/
static int vframe_pstready_show(struct seq_file *seq, void *v)
{
	unsigned int ch;

	int itmp;
	struct di_buf_s *p = NULL;
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;
	char *splt = "\t---------------------------";

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	/********************************/
	/* post_ready_list		*/
	/********************************/
	div3_que_list(ch, QUE_POST_READY, &tmpa[0], &psize);
	seq_printf(seq, "post_ready_list: curr(%d)\n", psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]);
		seq_file_vframe(seq, v, p->vframe);
		seq_printf(seq, "%s\n", splt);
	}

	return 0;
}

/**********************************************************
 * debug display vframe
 *********************************************************/
static int vframe_preready_show(struct seq_file *seq, void *v)
{
	unsigned int ch;

	int itmp;
	struct di_buf_s *p = NULL;
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;
	char *splt = "\t---------------------------";

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	/********************************/
	/* post_ready_list		*/
	/********************************/
	div3_que_list(ch, QUE_PRE_READY, &tmpa[0], &psize);
	seq_printf(seq, "pre_ready_list: curr(%d)\n", psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]);
		seq_file_vframe(seq, v, p->vframe);
		seq_printf(seq, "%s\n", splt);
	}

	return 0;
}

/**********************/
/* debug vframe type  */
/**********************/
const struct di_vframe_type_info div3_vtype_info[] = {
	{"interlace", VIDTYPE_INTERLACE, NULL},
	{"bottom", VIDTYPE_INTERLACE_BOTTOM, NULL},

	{"interllace first", VIDTYPE_INTERLACE_FIRST, NULL},
	{"mvc", VIDTYPE_MVC, NULL},
	{"no video en", VIDTYPE_NO_VIDEO_ENABLE, NULL},
	{"v422", VIDTYPE_VIU_422, NULL},
	{"field", VIDTYPE_VIU_FIELD, NULL},

	{"single plane", VIDTYPE_VIU_SINGLE_PLANE, NULL},
	{"v444", VIDTYPE_VIU_444, NULL},
	{"nv21", VIDTYPE_VIU_NV21, NULL},
	{"vscale disable", VIDTYPE_VSCALE_DISABLE, NULL},
	{"cvs toggle", VIDTYPE_CANVAS_TOGGLE, NULL},
	{"pre interlace", VIDTYPE_PRE_INTERLACE, NULL},
	{"high run", VIDTYPE_HIGHRUN, NULL},
	{"compress", VIDTYPE_COMPRESS, NULL},
	{"pic", VIDTYPE_PIC, NULL},
	{"scatter", VIDTYPE_SCATTER, NULL},
	{"vd2", VIDTYPE_VD2, NULL},
	{"compress loss", VIDTYPE_COMPRESS_LOSS, NULL},
	{"comb", VIDTYPE_COMB_MODE, NULL},
	{"tb detect", TB_DETECT_MASK, NULL},

	/*finish*/
	{NULL, TABLE_FLG_END, NULL}
};

static void didbg_vtype_set(unsigned int type)
{
	get_datal()->dbg_data.vframe_type = type;
}

static unsigned int didbg_vtype_get(void)
{
	return get_datal()->dbg_data.vframe_type;
}

static int seqv3_file_vtype_show(struct seq_file *seq, void *v)
{
	unsigned int vtype;
	int i;
	unsigned int mask;

	i = 0;
	vtype = didbg_vtype_get();

	seq_printf(seq, "%s:vtype[0x%x]\n", __func__, vtype);

	while (div3_vtype_info[i].name) {
		mask = div3_vtype_info[i].mask;

		if ((vtype & mask) == mask) {
			seq_printf(seq, "\t%-15s:y\n", div3_vtype_info[i].name);
		} else {
			if (div3_vtype_info[i].other) {
				seq_printf(seq, "\t%-15s:yes\n",
					   div3_vtype_info[i].other);
			} else {
				seq_printf(seq, "\t%-15s:no\n",
					   div3_vtype_info[i].name);
			}
		}
		i++;
	}

	return 0;
}

ssize_t seqv3_file_vtype_store(struct file *file, const char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	char buf[20];
	int ret;

	unsigned int vtype;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;
	/*reg, bit, width, val*/
	#if 1
	ret = kstrtouint(buf, 0, &vtype);
	if (ret) {
		pr_info("war:please enter vtype\n");
		return 0;
	}
	pr_info("save type:0x%x", vtype);
	didbg_vtype_set(vtype);
	#else

	ret = sscanfxxx(buf, "%x", &vtype);

	/*--------------------------*/

	switch (ret) {
	case 1:
		pr_info("save type:0x%x", vtype);
		didbg_vtype_set(vtype);
		break;
	default:
		pr_info("war:please enter vtype\n");
		break;
	}
	#endif
	return count;
}

/**************************************
 *
 * show vframe current
 *
 **************************************/
static int seq_file_curr_vframe_show(struct seq_file *seq, void *v)
{
	unsigned int ch;
	struct di_buf_s *p = NULL;
	struct vframe_s *pvfm;
	char *splt = "---------------------------";
	char *splt2 = "-------------";
	int itmp;
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	seq_printf(seq, "%s\n", splt);

	/********************************/
	/* post_doing_list		*/
	/********************************/
	seq_puts(seq, "vfm for: post_doing_list:\n");
	//queue_for_each_entry(p, ch, QUEUE_POST_DOING, list) {
	div3_que_list(ch, QUE_POST_DOING, &tmpa[0], &psize);
	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]);
		pvfm = p->vframe;
		seq_file_vframe(seq, v, pvfm);
		seq_printf(seq, "%s\n", splt2);
	}
	seq_printf(seq, "%s\n", splt);

	/********************************/
	/* pre_ready_list		*/
	/********************************/
	seq_puts(seq, "pre_ready_list:\n");
	div3_que_list(ch, QUE_PRE_READY, &tmpa[0], &psize);
	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]);

		pvfm = p->vframe;
		seq_file_vframe(seq, v, pvfm);
		seq_printf(seq, "%s\n", splt2);
	}
	seq_printf(seq, "%s\n", splt);

	/********************************/
	/* post_ready_list		*/
	/********************************/
	div3_que_list(ch, QUE_POST_READY, &tmpa[0], &psize);
	seq_printf(seq, "post_ready_list: curr(%d)\n", psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]);
		pvfm = p->vframe;
		seq_file_vframe(seq, v, pvfm);
		seq_printf(seq, "%s\n", splt2);
	}
	seq_printf(seq, "%s\n", splt);

	/********************************/
	/* display_list			*/
	/********************************/
	seq_puts(seq, "display_list:\n");
	queue_for_each_entry(p, ch, QUEUE_DISPLAY, list) {
		pvfm = p->vframe;
		seq_file_vframe(seq, v, pvfm);
		seq_printf(seq, "%s\n", splt2);
	}
	seq_printf(seq, "%s\n", splt);

	return 0;
}

/**************************************
 *
 * summmary variable
 *
 **************************************/
static int seq_file_sum_show(struct seq_file *seq, void *v)
{
	unsigned int ch;
	char *sname;
	unsigned int val;
	unsigned int tsize;
	int i;

	ch = seq_get_channel(seq);

	tsize = div3_sum_get_tab_size();/*ARRAY_SIZE(di_sum_tab);*/

	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);
	for (i = 0; i < tsize; i++) {
		if (!div3_sum_check(ch, i))
			continue;
		div3_sum_get_info(ch, i, &sname, &val);
		seq_printf(seq, "\t%-2d:%-15s:%d\n", i, sname, val);
	}

	seq_printf(seq, "%s:finish\n", __func__);

	return 0;
}

/********************************/
/* state */
/********************************/

static int seq_file_state_show(struct seq_file *seq, void *v)
{
	unsigned int ch;

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	dimv3_state_show(seq, v, ch);

	seq_printf(seq, "%s:%s\n", "hw_timer", dimv3_htr_get_stsname());
	return 0;
}

static int seq_file_mif_show(struct seq_file *seq, void *v)
{
	unsigned int ch;

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	dimv3_dump_mif_size_state_show(seq, v, ch);

	return 0;
}

/********************************/
/* qued				*/
/********************************/
static int dim_qued_show(struct seq_file *s, void *what)
{
	int i;
	int *pCh;
	struct di_data_l_s *pdt = get_datal();
	struct di_ch_s *pchd;

	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;
	char *splt = "---------------------------";

	pCh = (int *)s->private;

	pchd = get_chdata(*pCh);
	if (!pdt || !pchd) {
		seq_printf(s, "%s warn:1 do nothing\n", __func__);
		return 0;
	}

	/********************************/
	/* free_list			*/
	/********************************/
	qued_ops.list(pchd, QUED_T_FREE, &tmpa[0], &psize);
	seq_printf(s, "free_list (max %d) (crr %d):\n",
		   DIM_K_VFM_NUM, psize);
	for (i = 0; i < psize; i++)
		seq_printf(s, "\t%2d,\n", tmpa[i]);

	seq_printf(s, "%s\n", splt);
	/********************************/
	/* IN				*/
	/********************************/
	qued_ops.list(pchd, QUED_T_IN, &tmpa[0], &psize);
	seq_printf(s, "in_list (crr %d):\n", psize);
	for (i = 0; i < psize; i++)
		seq_printf(s, "\t%2d,\n", tmpa[i]);

	seq_printf(s, "%s\n", splt);
	/********************************/
	/* PRE				*/
	/********************************/
	qued_ops.list(pchd, QUED_T_PRE, &tmpa[0], &psize);
	seq_printf(s, "pre list (crr %d):\n", psize);
	for (i = 0; i < psize; i++)
		seq_printf(s, "\t%2d,\n", tmpa[i]);

	seq_printf(s, "%s\n", splt);
	/********************************/
	/* recycle				*/
	/********************************/
	qued_ops.list(pchd, QUED_T_RECYCL, &tmpa[0], &psize);
	seq_printf(s, "recycle (crr %d):\n", psize);
	for (i = 0; i < psize; i++)
		seq_printf(s, "\t%2d,\n", tmpa[i]);

	seq_printf(s, "%s\n", splt);

	/********************************/
	/* QUED_T_IS_IN				*/
	/********************************/
	qued_ops.list(pchd, QUED_T_IS_IN, &tmpa[0], &psize);
	seq_printf(s, "is in (crr %d):\n", psize);
	for (i = 0; i < psize; i++)
		seq_printf(s, "\t%2d,\n", tmpa[i]);

	seq_printf(s, "%s\n", splt);

	/********************************/
	/* QUED_T_IS_FREE				*/
	/********************************/
	qued_ops.list(pchd, QUED_T_IS_FREE, &tmpa[0], &psize);
	seq_printf(s, "IS_FREE (crr %d):\n", psize);
	for (i = 0; i < psize; i++)
		seq_printf(s, "\t%2d,\n", tmpa[i]);

	seq_printf(s, "%s\n", splt);

	/********************************/
	/* QUED_T_IS_PST_FREE				*/
	/********************************/
	qued_ops.list(pchd, QUED_T_IS_PST_FREE, &tmpa[0], &psize);
	seq_printf(s, "IS_PST_FREE (crr %d):\n", psize);
	for (i = 0; i < psize; i++)
		seq_printf(s, "\t%2d,\n", tmpa[i]);

	seq_printf(s, "%s\n", splt);
	/********************************/
	/* QUED_T_IS_PST_DOBEF				*/
	/********************************/
	qued_ops.list(pchd, QUED_T_IS_PST_DOBEF, &tmpa[0], &psize);
	seq_printf(s, "IS_PST_DOBEF (crr %d):\n", psize);
	for (i = 0; i < psize; i++)
		seq_printf(s, "\t%2d,\n", tmpa[i]);

	seq_printf(s, "%s\n", splt);

	return 0;
}

/********************************/
#define DEFINE_SEQ_SHOW_ONLY(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

/*--------------------------*/
#if 1
/*note: this define can't used for x*/
#define DEFINE_SEQ_SHOW_STORE(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.write = __name ## _store,	\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}
#endif
/*--------------------------*/
#define DEFINE_SHOW_STORE(__name) \
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = simple_open,	\
	.read = __name ## _show,		\
	.write = __name ## _store,	\
}

/*--------------------------*/
#define DEFINE_STORE_ONLY(__name) \
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = simple_open,	\
	.read = NULL,		\
	.write = __name ## _store,	\
}

/**********************/

static int rcfgx_show(struct seq_file *s, void *what)
{
	int i;
	int *pCh;

	pCh = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *pCh);

	for (i = eDI_CFGX_BEGIN; i < eDI_DBG_CFGX_END; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n", i,
			   div3_cfgx_get_name(i),
			   div3_cfgx_get(*pCh, i));
	}

	return 0;
}

/*************************************************************/
static ssize_t wcfgx_store(struct file *file, const char __user *userbuf,
			   size_t count, loff_t *ppos)
{
	unsigned int item, val;
	char buf[80];
	int ret;
	int *pCh;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pCh = (int *)file->private_data;
	pr_info("%s:ch[%d]\n", __func__, *pCh);

	switch (ret) {
	case 2:
		if ((item <= eDI_CFGX_BEGIN)	||
		    (item >= eDI_DBG_CFGX_END)) {
			pr_info("war:cfg_item is overflow[%d,%d]:%d\n",
				eDI_CFGX_BEGIN,
				eDI_DBG_CFGX_END,
				item);
			break;
		}
		if (val > 1)
			pr_info("war:cfg value[%d] is not bool\n", val);

		pr_info("change cfg:%s\n", div3_cfgx_get_name(item));
		pr_info("\t%d -> %d\n", div3_cfgx_get(*pCh, item), val);
		div3_cfgx_set(*pCh, item, val);
		break;
	default:
		pr_info("err:please enter: cfg_item, value(bool)\n");
		return -EINVAL;
	}

	return count;
}

/***********************************************
 *
 ***********************************************/
ssize_t keepv3_buf_clear_store(struct file *file, const char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	char buf[20];
	int ret;

	unsigned int ch;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;
	/*reg, bit, width, val*/

	ret = kstrtouint(buf, 0, &ch);
	if (ret) {
		pr_info("war:please enter ch\n");
		return 0;
	}
	pr_info("ch:%d", ch);

	if (ch >= DI_CHANNEL_NUB) {
		PR_ERR("%s:ch is overflow %d\n", __func__, ch);
		return 0;
	}

	dimv3_dbg_release_keep_all(ch);

	return count;
}

/***************************************************************
 *
 * cfg top
 *
 ***************************************************************/
static int cfgt_help_show(struct seq_file *s, void *what)
{
	seq_puts(s, "cat list\n");
	seq_printf(s, "\t%-10s:%s\n", "cfg_ai", "all cfg infor");
	seq_printf(s, "\t%-10s:%s\n", "cfg_av", "all cfg val");
	seq_printf(s, "\t%-10s:%s\n", "cfg_one", "sel val or infor");
	seq_printf(s, "\t%-10s:%s\n", "cfg_sel", "sel infor");
	seq_puts(s, "echo list\n");

	seq_printf(s, "\t%-10s:%s\n", "val > cfgw_one",
		   "change cfg that have sel");
	seq_printf(s, "\t%-10s:%s\n", "index val > cfgw_index",
		   "change cfg by index");
	seq_printf(s, "\t%-10s:%s\n", "mode sel(0/1) index > cfgw_sel",
		   "change sel");

	return 0;
}

/*cfg_ai*/
static int cfgt_itme_all_show(struct seq_file *s, void *what)
{
	div3_cfgt_show_item_all(s);
	return 0;
}

/*cfg_av*/
static int cfgt_val_all_show(struct seq_file *s, void *what)
{
	div3_cfgt_show_val_all(s);
	return 0;
}

/*cfg_one*/
static int cfgt_one_show(struct seq_file *s, void *what)
{
	if (get_datal()->cfg_dbg_mode)
		div3_cfgt_show_item_sel(s);
	else
		div3_cfgt_show_val_sel(s);
	return 0;
}

/*cfg_sel*/
static int cfgt_sel_show(struct seq_file *s, void *what)
{
	unsigned int i;

	i = get_datal()->cfg_sel;
	seq_printf(s, "mode[%d]:index[%d]\n",
		   get_datal()->cfg_dbg_mode, i);

	if (!div3_cfg_top_check(i))
		return 0;
	seq_printf(s, "%s\n", div3_cfg_top_get_name(i));

	return 0;
}

static ssize_t cfgt_sel_store(struct file *file, const char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	unsigned int sel, index;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &sel, &index);

	switch (ret) {
	case 2:
		div3_cfgt_set_sel(sel, index);
		break;
	default:
		pr_info("err:please enter: cfg_item, index\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t cfgtw_id_store(struct file *file, const char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	unsigned int index, val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &index, &val);

	switch (ret) {
	case 2:
		if (!div3_cfg_top_check(index))
			break;
		pr_info("%s:%d->%d\n",
			div3_cfg_top_get_name(index),
			div3_cfg_top_get(index),
			val);
		div3_cfg_top_set(index, val);
		break;
	default:
		pr_info("err:please enter: cfg_item, index\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t cfgt_one_store(struct file *file, const char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	unsigned int index, val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		pr_info("war:please enter val\n");
		return 0;
	}
	index = get_datal()->cfg_sel;
	if (!div3_cfg_top_check(index))
		return count;

	pr_info("%s:%d->%d\n",
		div3_cfg_top_get_name(index),
		div3_cfg_top_get(index),
		val);
	div3_cfg_top_set(index, val);

	return count;
}

/*************************************************************/

/***************************************************************
 * parameter show and store for top : DI
 **************************************************************/
static int mpr_di_show(struct seq_file *s, void *what)
{
	int i;

	seq_printf(s, "%s:\n", __func__);

	for (i = eDI_MP_SUB_DI_B; i < eDI_MP_SUB_DI_E; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n",
			   i - eDI_MP_SUB_DI_B,
			   div3_mp_uit_get_name(i),
			   dimp_get(i));
	}

	return 0;
}

static ssize_t mpw_di_store(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	unsigned int item, rid;
	char buf[80];
	int ret, val;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pr_info("%s:\n", __func__);

	switch (ret) {
	case 2:
		/*check []*/
		if (item >= (eDI_MP_SUB_DI_E - eDI_MP_SUB_DI_B)) {
			PR_WARN("index is overflow[%d,%d]:%d\n",
				0,
				eDI_MP_SUB_DI_E - eDI_MP_SUB_DI_B,
				item);
			break;
		}
		rid = item + eDI_MP_SUB_DI_B;
		pr_info("change mp :%s\n",
			div3_mp_uit_get_name(rid));
		pr_info("\t%d -> %d\n", dimp_get(rid), val);
		dimp_set(rid, val);
		break;
	default:
		PR_ERR("please enter: id, value(int)\n");
		return -EINVAL;
	}

	return count;
}

/***************************************************************
 * parameter show and store for top : nr
 **************************************************************/
static int mpr_nr_show(struct seq_file *s, void *what)
{
	int i;

	seq_printf(s, "%s:\n", __func__);

	for (i = eDI_MP_SUB_NR_B; i < eDI_MP_SUB_NR_E; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n",
			   i - eDI_MP_SUB_NR_B,
			   div3_mp_uit_get_name(i),
			   dimp_get(i));
	}

	return 0;
}

static ssize_t mpw_nr_store(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	unsigned int item, rid;
	char buf[80];
	int ret, val;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pr_info("%s:\n", __func__);

	switch (ret) {
	case 2:
		/*check []*/
		if (item >= (eDI_MP_SUB_NR_E - eDI_MP_SUB_NR_B)) {
			PR_WARN("index is overflow[%d,%d]:%d\n",
				0,
				eDI_MP_SUB_NR_E - eDI_MP_SUB_NR_B,
				item);
			break;
		}
		rid = item + eDI_MP_SUB_NR_B;
		pr_info("change mp:%s\n",
			div3_mp_uit_get_name(rid));
		pr_info("\t%d -> %d\n", dimp_get(rid), val);
		dimp_set(rid, val);
		break;
	default:
		PR_ERR("please enter: id, value(int)\n");
		return -EINVAL;
	}

	return count;
}

/***************************************************************
 * parameter show and store for top : pulldown
 **************************************************************/
static int mpr_pd_show(struct seq_file *s, void *what)
{
	int i;

	seq_printf(s, "%s:\n", __func__);

	for (i = eDI_MP_SUB_PD_B; i < eDI_MP_SUB_PD_E; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n",
			   i - eDI_MP_SUB_PD_B,
			   div3_mp_uit_get_name(i),
			   dimp_get(i));
	}

	return 0;
}

static ssize_t mpw_pd_store(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	unsigned int item, rid;
	char buf[80];
	int ret, val;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pr_info("%s:\n", __func__);

	switch (ret) {
	case 2:
		/*check []*/
		if (item >= (eDI_MP_SUB_PD_E - eDI_MP_SUB_PD_B)) {
			PR_WARN("index is overflow[%d,%d]:%d\n",
				0,
				eDI_MP_SUB_PD_E - eDI_MP_SUB_PD_B,
				item);
			break;
		}
		rid = item + eDI_MP_SUB_PD_B;
		pr_info("change mp:%s\n",
			div3_mp_uit_get_name(rid));
		pr_info("\t%d -> %d\n", dimp_get(rid), val);
		dimp_set(rid, val);
		break;
	default:
		PR_ERR("please enter: id, value(int)\n");
		return -EINVAL;
	}

	return count;
}

/***************************************************************
 * parameter show and store for top : mtn
 **************************************************************/
static int mpr_mtn_show(struct seq_file *s, void *what)
{
	int i;

	seq_printf(s, "%s:\n", __func__);

	for (i = eDI_MP_SUB_MTN_B; i < eDI_MP_SUB_MTN_E; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n",
			   i - eDI_MP_SUB_MTN_B,
			   div3_mp_uit_get_name(i),
			   dimp_get(i));
	}

	return 0;
}

static ssize_t mpw_mtn_store(struct file *file, const char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	unsigned int item, rid;
	char buf[80];
	int ret, val;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pr_info("%s:\n", __func__);

	switch (ret) {
	case 2:
		/*check []*/
		if (item >= (eDI_MP_SUB_MTN_E - eDI_MP_SUB_MTN_B)) {
			PR_WARN("index is overflow[%d,%d]:%d\n",
				0,
				eDI_MP_SUB_MTN_E - eDI_MP_SUB_MTN_B,
				item);
			break;
		}
		rid = item + eDI_MP_SUB_MTN_B;
		pr_info("change mp:%s\n",
			div3_mp_uit_get_name(rid));
		pr_info("\t%d -> %d\n", dimp_get(rid), val);
		dimp_set(rid, val);
		break;
	default:
		PR_ERR("please enter: id, value(int)\n");
		return -EINVAL;
	}

	return count;
}

/**********************/
static int mpxr_show(struct seq_file *s, void *what)
{
	int i;
	int *pCh;

	pCh = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *pCh);

	for (i = eDI_MP_UIX_BEGIN; i < eDI_MP_UIX_END; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n", i,
			   div3_mp_uix_get_name(i),
			   div3_mp_uix_get(*pCh, i));
	}

	return 0;
}

/*************************************************************/
static ssize_t mpxw_store(struct file *file, const char __user *userbuf,
			  size_t count, loff_t *ppos)
{
	unsigned int item, val;
	char buf[80];
	int ret;
	int *pCh;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pCh = (int *)file->private_data;
	pr_info("%s:ch[%d]\n", __func__, *pCh);

	switch (ret) {
	case 2:
		if ((item <= eDI_MP_UIX_BEGIN)	||
		    (item >= eDI_MP_UIX_END)) {
			PR_WARN("mpxw is overflow[%d,%d]:%d\n",
				eDI_MP_UIX_BEGIN,
				eDI_MP_UIX_END,
				item);
			break;
		}

		pr_info("change mp ch[%d]:%s\n", *pCh,
			div3_mp_uix_get_name(item));
		pr_info("\t%d -> %d\n", div3_mp_uix_get(*pCh, item), val);
		div3_mp_uix_set(*pCh, item, val);
		break;
	default:
		PR_ERR("please enter: mpxw, value(unsigned int)\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t buf_cnt_store(struct file *file, const char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	int w, h, pflg, mc;
	char buf[80];
	int ret, cnt_flg;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i %i %i", &w, &h, &pflg, &mc);

	pr_info("%s:\n", __func__);
	cnt_flg = 0;
	switch (ret) {
	case 2:
		cnt_flg = 1;
		pflg = 0;
		mc = 1;
		break;
	case 3:
		cnt_flg = 1;
		mc = 1;
		break;
	case 4:
		cnt_flg = 1;
		break;
	default:
		PR_ERR("please enter: w, h, pflg, mc\n");
		return -EINVAL;
	}

	if (cnt_flg)
		div3_cnt_buf(w, h, pflg, mc, 1, 1);
	return count;
}

/**********************/
void dbgv3_f_post_disable(unsigned int para)
{
	dimhv3_disable_post_deinterlace_2();
}

void dbgv3_f_trig_task(unsigned int para)
{
	taskv3_send_ready();
}

void dbg_f_trig_eos(unsigned int para)
{
#ifdef TST_NEW_INS_INTERFACE
	if (para)
		tst_new_trig_eos(true);
	else
		tst_new_trig_eos(false);
#endif
}

const struct di_dbg_func_s div3_func_tab[] = {
	{eDI_DBG_F_00, dbgv3_f_post_disable,
		"dimhv3_disable_post_deinterlace_2", "no para"},
	{eDI_DBG_F_01, dbgv3_f_trig_task,
		"trig task", "no para"},
	{eDI_DBG_F_02, dprev3_dbg_f_trig,
		"trig pre flow debug", "bit[4]:ddebug on/off;bi[3:0]:cnt"},
	{eDI_DBG_F_03, dpstv3_dbg_f_trig,
		"trig post flow debug", "bit[4]:ddebug on/off;bi[3:0]:cnt"},
	{eDI_DBG_F_04, hpstv3_dbg_power_ctr_trig,
		"trig post power", "1: on; 0: off"},

	{eDI_DBG_F_05, hpstv3_dbg_mem_pd_trig,
		"trig post mem pd", "no para"},
	{eDI_DBG_F_06, hpstv3_dbg_trig_gate,
		"trig post gate off/on", "no para"},
	{eDI_DBG_F_07, hpstv3_dbg_trig_mif,
		"trig post mif off/free", "no para"},
	{eDI_DBG_F_08, dbg_f_trig_eos,
		"trig EOS for new interface", "1:on;0:off"}
};

static ssize_t wfunc_store(struct file *file, const char __user *userbuf,
			   size_t count, loff_t *ppos)
{
	unsigned int findex, para;
	char buf[20];
	int ret;

	int i;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %x", &findex, &para);

	switch (ret) {
	case 2:
		pr_info("func:%d,para=0x%x\n", findex, para);
		for (i = 0; i < ARRAY_SIZE(div3_func_tab); i++) {
			if (i == findex && div3_func_tab[i].index == findex) {
				if (div3_func_tab[i].func)
					div3_func_tab[i].func(para);
				pr_info("func:%s finish\n",
					div3_func_tab[i].name);
				break;
			}
		}
		break;
	default:
		pr_info("warn: please enter function index and para\n");
		return -EINVAL;
	}

	return count;
}

static int rfunc_show(struct seq_file *seq, void *v)
{
	int i;

	seq_puts(seq, "debug function list:\n");

	for (i = 0; i < ARRAY_SIZE(div3_func_tab); i++) {
		if (div3_func_tab[i].index != i)
			seq_printf(seq, "warn: index(%d->%d) is not map\n",
				   i, div3_func_tab[i].index);
		seq_printf(seq, "index[%d]:%s:%s\n",
			   div3_func_tab[i].index,
			   div3_func_tab[i].name,
			   div3_func_tab[i].info);
	}
	return 0;
}
/************************************************************
 *
 ************************************************************/
static void dbg_wmode(struct seq_file *s, struct dim_wmode_s *pwmode)
{


	seq_printf(s, "\t%20s:%d\n", "is_afbc", pwmode->is_afbc);
	seq_printf(s, "\t%20s:%d\n", "is_vdin", pwmode->is_vdin);
	seq_printf(s, "\t%20s:%d\n", "is_i", pwmode->is_i);
	seq_printf(s, "\t%20s:%d\n", "need_bypass", pwmode->need_bypass);
	seq_printf(s, "\t%20s:%d\n", "is_bypass", pwmode->is_bypass);
	seq_printf(s, "\t%20s:%d\n", "pre_bypass", pwmode->pre_bypass);
	seq_printf(s, "\t%20s:%d\n", "post_bypass", pwmode->post_bypass);
	seq_printf(s, "\t%20s:%d\n", "flg_keep", pwmode->flg_keep);
	seq_printf(s, "\t%20s:%d\n", "trick_mode", pwmode->trick_mode);
	seq_printf(s, "\t%20s:%d\n", "prog_proc_config",
		   pwmode->prog_proc_config);
	seq_printf(s, "\t%20s:%d\n", "is_invert_tp", pwmode->is_invert_tp);
	seq_printf(s, "\t%20s:%d\n", "p_as_i", pwmode->p_as_i);
	seq_printf(s, "\t%20s:%d\n", "p_use_2i", pwmode->p_use_2i);
	seq_printf(s, "\t%20s:%d\n", "is_top", pwmode->is_top);
	seq_printf(s, "\t%20s:%d\n", "is_angle", pwmode->is_angle);

	seq_printf(s, "\t%20s:0x%x\n", "vtype", pwmode->vtype);
	seq_printf(s, "\t%20s:%d\n", "src_h", pwmode->src_h);
	seq_printf(s, "\t%20s:%d\n", "src_w", pwmode->src_w);
	seq_printf(s, "\t%20s:%d\n", "tgt_h", pwmode->tgt_h);
	seq_printf(s, "\t%20s:%d\n", "tgt_w", pwmode->tgt_w);
	seq_printf(s, "\t%20s:%d\n", "o_h", pwmode->o_h);
	seq_printf(s, "\t%20s:%d\n", "o_w", pwmode->o_w);
	seq_printf(s, "\t%20s:%d\n", "seq", pwmode->seq);
	seq_printf(s, "\t%20s:%d\n", "seq_pre", pwmode->seq_pre);


}

static void dbg_vmode(struct seq_file *s, struct dim_vmode_s *pvmode)
{
	char *splt = "---------------------------";

	seq_printf(s, "%s\n", splt);
	seq_printf(s, "\t%20s:0x%x\n", "vtype", pvmode->vtype);
	seq_printf(s, "\t%20s:%d\n", "h", pvmode->h);
	seq_printf(s, "\t%20s:%d\n", "w", pvmode->w);
	seq_printf(s, "\t%20s:0x%x\n", "canvas0Addr", pvmode->canvas0Addr);
	seq_printf(s, "\t%20s:0x%x\n", "bitdepth", pvmode->bitdepth);
	seq_printf(s, "\t%20s:0x%x\n", "bit_mode", pvmode->bit_mode);
	seq_printf(s, "\t%20s:0x%x\n", "omx_index", pvmode->omx_index);
}

static int wmode_pstready_show(struct seq_file *seq, void *v)
{
	unsigned int ch;
	int itmp;
	struct di_buf_s *p = NULL;
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;
	char *splt = "\t---------------------------";

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	/********************************/
	/* post_ready_list		*/
	/********************************/
	div3_que_list(ch, QUE_POST_READY, &tmpa[0], &psize); /*new que*/
	seq_printf(seq, "post_ready_list: curr(%d)\n", psize);

	for (itmp = 0; itmp < psize; itmp++) {			/*new que*/
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]); /*new que*/
		seq_printf(seq, "\t type[%d] id[%d]\n", p->type, p->index);
		dbg_wmode(seq, &p->c.wmode);
		dbg_vmode(seq, &p->c.vmode);
		seq_printf(seq, "%s\n", splt);
	}

	return 0;
}

static int wmode_preready_show(struct seq_file *seq, void *v)
{
	unsigned int ch;
	int itmp;
	struct di_buf_s *p = NULL;
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;
	char *splt = "\t---------------------------";

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	/********************************/
	/* pre_ready_list		*/
	/********************************/
	seq_puts(seq, "pre_ready_list:\n");
	div3_que_list(ch, QUE_PRE_READY, &tmpa[0], &psize);
	for (itmp = 0; itmp < psize; itmp++) {
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]);
		seq_printf(seq, "\t type[%d] id[%d]\n", p->type, p->index);
		dbg_wmode(seq, &p->c.wmode);
		dbg_vmode(seq, &p->c.vmode);
		seq_printf(seq, "%s\n", splt);
	}

	return 0;
}

/************************************************************
 *
 ************************************************************/
static void dbg_mcinfo_pre(struct seq_file *s, struct mcinfo_pre_s *pmcinfo)
{
	int i;

	struct mcinfo_pre_s {
		unsigned int highvertfrqflg;
		unsigned int motionparadoxflg;
		unsigned int regs[26];/* reg 0x2fb0~0x2fc9 */
	};
	seq_printf(s, "\t%20s:%d\n",
		   "highvertfrqflg",
		   pmcinfo->highvertfrqflg);
	seq_printf(s, "\t%20s:%d\n",
		   "motionparadoxflg",
		   pmcinfo->motionparadoxflg);

	for (i = 0; i < 26; i++) {
		seq_printf(s, "\treg[%d]:%d\n",
			   i,
			   pmcinfo->regs[i]);
	}
}

static int mcinfo_pre_pstready_show(struct seq_file *seq, void *v)
{
	unsigned int ch;
	int itmp;
	struct di_buf_s *p = NULL;
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;
	char *splt = "\t---------------------------";

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	/********************************/
	/* post_ready_list		*/
	/********************************/
	div3_que_list(ch, QUE_POST_READY, &tmpa[0], &psize); /*new que*/
	seq_printf(seq, "post_ready_list: curr(%d)\n", psize);

	for (itmp = 0; itmp < psize; itmp++) {			/*new que*/
		p = pwv3_qindex_2_buf(ch, tmpa[itmp]); /*new que*/
		seq_printf(seq, "\t type[%d] id[%d]\n", p->type, p->index);
		dbg_mcinfo_pre(seq, &p->c.curr_field_mcinfo);
		seq_printf(seq, "%s\n", splt);
	}

	return 0;
}

/**********************/
static ssize_t reg_show(struct file *file, char __user *userbuf,
			size_t count, loff_t *ppos)
{
	char buf[80];

	ssize_t len;
	int *pInt;

	pInt = (int *)file->private_data;
	pr_info("pInt=0x%p,val=%d\n", pInt, *pInt);

	len = snprintf(buf, sizeof(buf), "%s\n",
		       __func__);

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t reg_store(struct file *file, const char __user *userbuf,
			 size_t count, loff_t *ppos)
{
	unsigned int reg, val;
	char buf[80];
	int ret;
	int *pInt;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%x %i", &reg, &val);

	switch (ret) {
	case 1:
		pr_info("reg:0x%x\n", reg);

		pInt = (int *)file->private_data;
		pr_info("pInt=0x%p,val=%d\n", pInt, *pInt);
		break;
	case 2:
		pr_info("reg:0x%x,val=%d\n", reg, val);
		break;
	default:
		return -EINVAL;
	}

	return count;
}
/**********************/
#ifndef TST_NEW_INS_INTERFACE
int vfmtst_vfmo_show(struct seq_file *s, void *what)
{
	seq_printf(s, "%s:no new interface\n", __func__);
	return 0;
}
int vfmtst_vfmi_show(struct seq_file *s, void *what)
{
	seq_printf(s, "%s:no new interface\n", __func__);
	return 0;

}
int vfmtst_bufo_show(struct seq_file *s, void *what)
{
	seq_printf(s, "%s:no new interface\n", __func__);
	return 0;
}
int vfmtst_que_show(struct seq_file *s, void *what)
{
	seq_printf(s, "%s:no new interface\n", __func__);
		return 0;

}

#endif
/**********************/
DEFINE_SEQ_SHOW_ONLY(dimv3_reg_cue_int);
/**********************/
DEFINE_SEQ_SHOW_ONLY(rcfgx);
DEFINE_SEQ_SHOW_ONLY(seq_file_vframe_in);
DEFINE_SEQ_SHOW_ONLY(seq_file_vframe_out);
DEFINE_SEQ_SHOW_ONLY(seq_file_state);
DEFINE_SEQ_SHOW_ONLY(seq_file_mif);
DEFINE_SEQ_SHOW_ONLY(seq_file_sum);

DEFINE_SEQ_SHOW_ONLY(regv3_con);
DEFINE_SEQ_SHOW_ONLY(rfunc);
DEFINE_SEQ_SHOW_ONLY(mpxr);
DEFINE_SEQ_SHOW_ONLY(mpr_di);
DEFINE_SEQ_SHOW_ONLY(mpr_nr);
DEFINE_SEQ_SHOW_ONLY(mpr_pd);
DEFINE_SEQ_SHOW_ONLY(mpr_mtn);
DEFINE_SEQ_SHOW_ONLY(dim_qued);

/*cfg top show only*/
DEFINE_SEQ_SHOW_ONLY(cfgt_help);
DEFINE_SEQ_SHOW_ONLY(cfgt_itme_all);
DEFINE_SEQ_SHOW_ONLY(cfgt_val_all);

/*cfg top store only*/
DEFINE_STORE_ONLY(cfgtw_id);

/*cfg top show/store*/
DEFINE_SEQ_SHOW_STORE(cfgt_one);
DEFINE_SEQ_SHOW_STORE(cfgt_sel);

DEFINE_SEQ_SHOW_ONLY(seq_file_curr_vframe);

DEFINE_STORE_ONLY(wcfgx);
DEFINE_STORE_ONLY(wfunc);
DEFINE_STORE_ONLY(mpxw);
DEFINE_STORE_ONLY(mpw_di);
DEFINE_STORE_ONLY(mpw_nr);
DEFINE_STORE_ONLY(mpw_pd);
DEFINE_STORE_ONLY(mpw_mtn);
DEFINE_STORE_ONLY(buf_cnt);
DEFINE_STORE_ONLY(keepv3_buf_clear);

DEFINE_SHOW_STORE(reg);

DEFINE_SEQ_SHOW_STORE(seqv3_file_vtype);
DEFINE_SEQ_SHOW_STORE(ddbg_log_reg);

/*wmode:*/
DEFINE_SEQ_SHOW_ONLY(wmode_pstready);
DEFINE_SEQ_SHOW_ONLY(wmode_preready);

/**/
DEFINE_SEQ_SHOW_ONLY(mcinfo_pre_pstready);

/*vframe*/
DEFINE_SEQ_SHOW_ONLY(vframe_pstready);
DEFINE_SEQ_SHOW_ONLY(vframe_preready);

DEFINE_SEQ_SHOW_ONLY(vfmtst_que);
DEFINE_SEQ_SHOW_ONLY(vfmtst_vfmo);
DEFINE_SEQ_SHOW_ONLY(vfmtst_vfmi);
DEFINE_SEQ_SHOW_ONLY(vfmtst_bufo);

/**********************/

struct di_dbgfs_files_t {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

static const struct di_dbgfs_files_t di_debugfs_files_top[] = {
	{"vtype", S_IFREG | 0644, &seqv3_file_vtype_fops},
	{"reg_log", S_IFREG | 0644, &ddbg_log_reg_fops},
	{"regctr", S_IFREG | 0644, &regv3_con_fops},
	{"rfunc", S_IFREG | 0644, &rfunc_fops},
	{"wfunc", S_IFREG | 0644, &wfunc_fops},
	{"reg_cue", S_IFREG | 0644, &dimv3_reg_cue_int_fops},
	/*module parameter*/
	{"mr_di", S_IFREG | 0644, &mpr_di_fops},
	{"mw_di", S_IFREG | 0644, &mpw_di_fops},
	{"mr_nr", S_IFREG | 0644, &mpr_nr_fops},
	{"mw_nr", S_IFREG | 0644, &mpw_nr_fops},
	{"mr_pd", S_IFREG | 0644, &mpr_pd_fops},
	{"mw_pd", S_IFREG | 0644, &mpw_pd_fops},
	{"mr_mtn", S_IFREG | 0644, &mpr_mtn_fops},
	{"mw_mtn", S_IFREG | 0644, &mpw_mtn_fops},
	{"buf_cnt", S_IFREG | 0644, &buf_cnt_fops},
	{"keep_clear", S_IFREG | 0644, &keepv3_buf_clear_fops},
	/*cfg*/
	{"cfghelp", S_IFREG | 0644, &cfgt_help_fops},
	{"cfgr_ai", S_IFREG | 0644, &cfgt_itme_all_fops},
	{"cfgr_av", S_IFREG | 0644, &cfgt_val_all_fops},
	{"cfgw_id", S_IFREG | 0644, &cfgtw_id_fops},
	{"cfg_one", S_IFREG | 0644, &cfgt_one_fops},
	{"cfg_sel", S_IFREG | 0644, &cfgt_sel_fops},
	{"tstvfm_que", S_IFREG | 0644, &vfmtst_que_fops},
	{"tstvfm_vfmo", S_IFREG | 0644, &vfmtst_vfmo_fops},
	{"tstvfm_vfmi", S_IFREG | 0644, &vfmtst_vfmi_fops},
	{"tstvfm_bufo", S_IFREG | 0644, &vfmtst_bufo_fops}
};

static const struct di_dbgfs_files_t di_debugfs_files[] = {
	{"rcfgx", S_IFREG | 0644, &rcfgx_fops},
	{"wcfgx", S_IFREG | 0644, &wcfgx_fops},
	{"rvfm_in", S_IFREG | 0644, &seq_file_vframe_in_fops},
	{"rvfm_out", S_IFREG | 0644, &seq_file_vframe_out_fops},
	{"vfm_pstrdy", S_IFREG | 0644, &vframe_pstready_fops},
	{"vfm_prerdy", S_IFREG | 0644, &vframe_preready_fops},
	{"state", S_IFREG | 0644, &seq_file_state_fops},
	{"dumpmif", S_IFREG | 0644, &seq_file_mif_fops},
	{"test_reg", S_IFREG | 0644, &reg_fops},
	{"sum", S_IFREG | 0644, &seq_file_sum_fops},
	{"mpxr", S_IFREG | 0644, &mpxr_fops},
	{"mpxw", S_IFREG | 0644, &mpxw_fops},
	{"vfmc", S_IFREG | 0644, &seq_file_curr_vframe_fops},
	{"qued", S_IFREG | 0644, &dim_qued_fops},
	{"wmode_pstrdy", S_IFREG | 0644, &wmode_pstready_fops},
	{"wmode_prerdy", S_IFREG | 0644, &wmode_preready_fops},
	{"mcinfopre_pstrdy", S_IFREG | 0644, &mcinfo_pre_pstready_fops}
};

void didbgv3_fs_init(void)
{
	int i, j;
	char name[5];
	/*char buf[3];*/

	struct dentry **root_ent;

	struct dentry *ent;
	int *pPlane = di_get_plane();

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
#if 0
		strcpy(name, "di");
		sprintf(buf, "%01d", i);
		strncat(name, buf, sizeof(buf) - 1);
#endif
		snprintf(name, sizeof(name), "di%01d", i);
		root_ent = dich_get_dbgroot(i);
		if (IS_ERR_OR_NULL(root_ent))
			continue;
		*root_ent = debugfs_create_dir(name, NULL);
		if (IS_ERR_OR_NULL(*root_ent))
			continue;
		*(pPlane + i) = i;
		/*printk("plane 0x%p\n", &plane_ch[i]);*/
		for (j = 0; j < ARRAY_SIZE(di_debugfs_files); j++) {
			ent = debugfs_create_file(di_debugfs_files[j].name,
						  di_debugfs_files[j].mode,
						  *root_ent, (pPlane + i),
						  di_debugfs_files[j].fops);
			if (!ent)
				PR_ERR("debugfs create failed\n");
		}
	}
	/*top*/
	root_ent = dich_get_dbgroot_top();
	if (IS_ERR_OR_NULL(root_ent))
		return;
	*root_ent = debugfs_create_dir("di_top", NULL);
	if (IS_ERR_OR_NULL(*root_ent))
		return;
	for (i = 0; i < ARRAY_SIZE(di_debugfs_files_top); i++) {
		ent = debugfs_create_file(di_debugfs_files_top[i].name,
					  di_debugfs_files_top[i].mode,
					  *root_ent, NULL,
					  di_debugfs_files_top[i].fops);
		if (!ent)
			PR_ERR("debugfs top [%d]create failed\n", i);
	}
}

void didbgv3_fs_exit(void)
{
	struct dentry **root_ent;
	int i;

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		root_ent = dich_get_dbgroot(i);
		debugfs_remove_recursive(*root_ent);
	}

	/*top*/
	root_ent = dich_get_dbgroot_top();
	debugfs_remove_recursive(*root_ent);

	pr_info("%s:finish\n", __func__);
}

/*-----------------------*/

