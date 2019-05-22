/*
 * drivers/amlogic/media/frame_provider/decoder/h264/vh264.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>

#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "../../../stream_input/amports/amports_priv.h"
#include <linux/amlogic/media/canvas/canvas.h>

#include "../utils/vdec.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include "../utils/amvdec.h"
#include "vh264.h"
#include "../../../stream_input/parser/streambuf.h"
#include <linux/delay.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include "../utils/decoder_mmu_box.h"
#include "../utils/decoder_bmmu_box.h"
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include "../utils/firmware.h"
#include <linux/amlogic/tee.h>

#define DRIVER_NAME "amvdec_h264"
#define MODULE_NAME "amvdec_h264"
#define MEM_NAME "codec_264"
#define HANDLE_H264_IRQ

#if 0
/* currently, only iptv supports this function*/
#define SUPPORT_BAD_MACRO_BLOCK_REDUNDANCY
#endif

/* #define DEBUG_PTS */
#if 0 /* MESON_CPU_TYPE <= MESON_CPU_TYPE_MESON6TV */
#define DROP_B_FRAME_FOR_1080P_50_60FPS
#endif
#define RATE_MEASURE_NUM 8
#define RATE_CORRECTION_THRESHOLD 5
#define RATE_24_FPS  4004	/* 23.97 */
#define RATE_25_FPS  3840	/* 25 */
#define DUR2PTS(x) ((x)*90/96)
#define PTS2DUR(x) ((x)*96/90)
#define DUR2PTS_REM(x) (x*90 - DUR2PTS(x)*96)
#define FIX_FRAME_RATE_CHECK_IDRFRAME_NUM 2
#define VDEC_CLOCK_ADJUST_FRAME 50

static inline bool close_to(int a, int b, int m)
{
	return (abs(a - b) < m) ? true : false;
}

static DEFINE_MUTEX(vh264_mutex);
#define DEF_BUF_START_ADDR            0x1000000
#define V_BUF_ADDR_OFFSET_NEW         (0x1ee000)
#define V_BUF_ADDR_OFFSET             (0x13e000)

#define PIC_SINGLE_FRAME        0
#define PIC_TOP_BOT_TOP         1
#define PIC_BOT_TOP_BOT         2
#define PIC_DOUBLE_FRAME        3
#define PIC_TRIPLE_FRAME        4
#define PIC_TOP_BOT              5
#define PIC_BOT_TOP              6
#define PIC_INVALID              7

#define EXTEND_SAR                      0xff

#define VF_POOL_SIZE        64
#define VF_BUF_NUM          24
#define WORKSPACE_BUF_NUM	2
#define PUT_INTERVAL        (HZ/100)
#define NO_DISP_WD_COUNT    (3 * HZ / PUT_INTERVAL)

#define SWITCHING_STATE_OFF       0
#define SWITCHING_STATE_ON_CMD3   1
#define SWITCHING_STATE_ON_CMD1   2
#define SWITCHING_STATE_ON_CMD1_PENDING   3


#define DEC_CONTROL_FLAG_FORCE_2997_1080P_INTERLACE 0x0001
#define DEC_CONTROL_FLAG_FORCE_2500_576P_INTERLACE  0x0002
#define DEC_CONTROL_FLAG_DISABLE_FAST_POC              0x0004

#define INCPTR(p) ptr_atomic_wrap_inc(&p)

#define SLICE_TYPE_I 2
#define SLICE_TYPE_P 5
#define SLICE_TYPE_B 6

struct buffer_spec_s {
	unsigned int y_addr;
	unsigned int u_addr;
	unsigned int v_addr;

	int y_canvas_index;
	int u_canvas_index;
	int v_canvas_index;

	unsigned int y_canvas_width;
	unsigned int u_canvas_width;
	unsigned int v_canvas_width;

	unsigned int y_canvas_height;
	unsigned int u_canvas_height;
	unsigned int v_canvas_height;

	unsigned long phy_addr;
	int alloc_count;
};

#define spec2canvas(x)  \
	(((x)->v_canvas_index << 16) | \
	 ((x)->u_canvas_index << 8)  | \
	 ((x)->y_canvas_index << 0))

static struct vframe_s *vh264_vf_peek(void *);
static struct vframe_s *vh264_vf_get(void *);
static void vh264_vf_put(struct vframe_s *, void *);
static int vh264_vf_states(struct vframe_states *states, void *);
static int vh264_event_cb(int type, void *data, void *private_data);

static void vh264_prot_init(void);
static int vh264_local_init(void);
static void vh264_put_timer_func(unsigned long arg);
static void stream_switching_done(void);

static const char vh264_dec_id[] = "vh264-dev";

#define PROVIDER_NAME   "decoder.h264"

static const struct vframe_operations_s vh264_vf_provider_ops = {
	.peek = vh264_vf_peek,
	.get = vh264_vf_get,
	.put = vh264_vf_put,
	.event_cb = vh264_event_cb,
	.vf_states = vh264_vf_states,
};

static struct vframe_provider_s vh264_vf_prov;
/*TODO irq*/
#if 1
static u32 frame_width, frame_height, frame_dur, frame_prog, frame_packing_type,
	   last_duration;
static u32 saved_resolution;
static u32 last_mb_width, last_mb_height;
#else
static u32 frame_buffer_size;
static u32 frame_width, frame_height, frame_dur, frame_prog, last_duration;
static u32 last_mb_width, last_mb_height;
static u32 frame_packing_type;
#endif
static DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(recycle_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(delay_display_q, struct vframe_s *, VF_POOL_SIZE);

static struct vframe_s vfpool[VF_POOL_SIZE];
static s32 vfbuf_use[VF_BUF_NUM];
static struct buffer_spec_s buffer_spec[VF_BUF_NUM];
static struct buffer_spec_s fense_buffer_spec[2];
/* disp buf + keep buf+ fense buf + workspace  */

#define MAX_BLK_BUFFERS (VF_BUF_NUM + 2 + WORKSPACE_BUF_NUM)
#define VF_BUFFER_IDX(n) (WORKSPACE_BUF_NUM  + n)
#define FENSE_BUFFER_IDX(n) (WORKSPACE_BUF_NUM + VF_BUF_NUM + n)

#define USER_DATA_RUND_SIZE		(USER_DATA_SIZE + 4096)
static struct vframe_s fense_vf[2];

static struct timer_list recycle_timer;
static u32 stat;
static s32 buf_offset;
static u32 pts_outside;
static u32 sync_outside;
static u32 dec_control;
static u32 vh264_ratio;
static u32 vh264_rotation;
static u32 use_idr_framerate;
static u32 high_bandwidth;

static u32 seq_info;
static u32 timing_info_present_flag;
static u32 fixed_frame_rate_flag;
static u32 fixed_frame_rate_check_count;
static u32 aspect_ratio_info;
static u32 num_units_in_tick;
static u32 time_scale;
static u32 h264_ar;
static u32 decoder_debug_flag;
static u32 dpb_size_adj = 6;
static u32 fr_hint_status;

#ifdef DROP_B_FRAME_FOR_1080P_50_60FPS
static u32 last_interlaced;
#endif
static bool is_4k;
static unsigned char h264_first_pts_ready;
static bool h264_first_valid_pts_ready;
static u32 h264pts1, h264pts2;
static u32 h264_pts_count, duration_from_pts_done, duration_on_correcting;
static u32 vh264_error_count;
static u32 vh264_no_disp_count;
static u32 fatal_error_flag;
static u32 fatal_error_reset;
static u32 max_refer_buf = 1;
static u32 decoder_force_reset;
static unsigned int no_idr_error_count;
static unsigned int no_idr_error_max = 60;
#ifdef SUPPORT_BAD_MACRO_BLOCK_REDUNDANCY
/* 0~128*/
static u32 bad_block_scale;
#endif

static unsigned int enable_switch_fense = 0;
#define EN_SWITCH_FENCE() (enable_switch_fense && !is_4k)
#if 0
static u32 vh264_no_disp_wd_count;
#endif
static u32 vh264_running;
static s32 vh264_stream_switching_state;
static s32 vh264_eos;
static struct vframe_s *p_last_vf;
static s32 iponly_early_mode;
static void *mm_blk_handle;
static int tvp_flag;
static bool is_reset;

/*TODO irq*/
#if 1
static u32 last_pts, last_pts_remainder;
#else
static u32 last_pts;
#endif
static bool check_pts_discontinue;
static u32 wait_buffer_counter;
static u32 video_signal_from_vui;

static uint error_recovery_mode = 1;
static uint error_recovery_mode_in = 3;
static uint error_recovery_mode_use = 3;

static uint mb_total = 0, mb_width = 0, mb_height;
static uint saved_idc_level;
#define UCODE_IP_ONLY 2
#define UCODE_IP_ONLY_PARAM 1
static uint ucode_type;

#ifdef DEBUG_PTS
static unsigned long pts_missed, pts_hit;
#endif
static uint debugfirmware;

static atomic_t vh264_active = ATOMIC_INIT(0);
static int vh264_reset;
static struct work_struct error_wd_work;
static struct work_struct stream_switching_work;
static struct work_struct set_parameter_work;
static struct work_struct notify_work;
static struct work_struct set_clk_work;
static struct work_struct userdata_push_work;



static struct dec_sysinfo vh264_amstream_dec_info;
static dma_addr_t mc_dma_handle;
static void *mc_cpu_addr;
static u32 first_offset;
static u32 first_pts;
static u64 first_pts64;
static bool first_pts_cached;
static void *sei_data_buffer;
static dma_addr_t sei_data_buffer_phys;
static int clk_adj_frame_count;

#define MC_OFFSET_HEADER    0x0000
#define MC_OFFSET_DATA      0x1000
#define MC_OFFSET_MMCO      0x2000
#define MC_OFFSET_LIST      0x3000
#define MC_OFFSET_SLICE     0x4000

#define MC_TOTAL_SIZE       (20*SZ_1K)
#define MC_SWAP_SIZE        (4*SZ_1K)

#define MODE_ERROR 0
#define MODE_FULL  1

static DEFINE_SPINLOCK(lock);
static DEFINE_SPINLOCK(prepare_lock);
static DEFINE_SPINLOCK(recycle_lock);

static bool block_display_q;
static int vh264_stop(int mode);
static s32 vh264_init(void);

#define DFS_HIGH_THEASHOLD 3

static bool pts_discontinue;

static struct ge2d_context_s *ge2d_videoh264_context;

static struct vdec_info *gvs;

static int ge2d_videoh264task_init(void)
{
	if (ge2d_videoh264_context == NULL)
		ge2d_videoh264_context = create_ge2d_work_queue();

	if (ge2d_videoh264_context == NULL) {
		pr_info("create_ge2d_work_queue video task failed\n");
		return -1;
	}
	return 0;
}

static int ge2d_videoh264task_release(void)
{
	if (ge2d_videoh264_context) {
		destroy_ge2d_work_queue(ge2d_videoh264_context);
		ge2d_videoh264_context = NULL;
	}
	return 0;
}

static int ge2d_canvas_dup(struct canvas_s *srcy, struct canvas_s *srcu,
		struct canvas_s *des, int format, u32 srcindex,
		u32 desindex)
{

	struct config_para_ex_s ge2d_config;
	/* pr_info("[%s]h264 ADDR srcy[0x%lx] srcu[0x%lx] des[0x%lx]\n",
	 *	   __func__, srcy->addr, srcu->addr, des->addr);
	 */
	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));

	ge2d_config.alu_const_color = 0;
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;

	ge2d_config.src_planes[0].addr = srcy->addr;
	ge2d_config.src_planes[0].w = srcy->width;
	ge2d_config.src_planes[0].h = srcy->height;

	ge2d_config.src_planes[1].addr = srcu->addr;
	ge2d_config.src_planes[1].w = srcu->width;
	ge2d_config.src_planes[1].h = srcu->height;

	ge2d_config.dst_planes[0].addr = des->addr;
	ge2d_config.dst_planes[0].w = des->width;
	ge2d_config.dst_planes[0].h = des->height;

	ge2d_config.src_para.canvas_index = srcindex;
	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = format;
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.color = 0;
	ge2d_config.src_para.top = 0;
	ge2d_config.src_para.left = 0;
	ge2d_config.src_para.width = srcy->width;
	ge2d_config.src_para.height = srcy->height;

	ge2d_config.dst_para.canvas_index = desindex;
	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.format = format;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.width = srcy->width;
	ge2d_config.dst_para.height = srcy->height;

	if (ge2d_context_config_ex(ge2d_videoh264_context, &ge2d_config) < 0) {
		pr_info("ge2d_context_config_ex failed\n");
		return -1;
	}

	stretchblt_noalpha(ge2d_videoh264_context, 0, 0, srcy->width,
			srcy->height, 0, 0, srcy->width, srcy->height);

	return 0;
}

static inline int fifo_level(void)
{
	return VF_POOL_SIZE - kfifo_len(&newframe_q);
}


void spec_set_canvas(struct buffer_spec_s *spec,
		unsigned int width, unsigned int height)
{
	canvas_config(spec->y_canvas_index,
			spec->y_addr,
			width, height,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

	canvas_config(spec->u_canvas_index,
				  spec->u_addr,
				  width, height / 2,
				  CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
	return;
}

static void vh264_notify_work(struct work_struct *work)
{
	pr_info("frame duration changed %d\n", frame_dur);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_FR_HINT,
		(void *)((unsigned long)frame_dur));

	return;
}

static void prepare_display_q(void)
{
	unsigned long flags;
	int count;

	spin_lock_irqsave(&prepare_lock, flags);

	if (block_display_q) {
		spin_unlock_irqrestore(&prepare_lock, flags);
		return;
	}

	spin_unlock_irqrestore(&prepare_lock, flags);

	count  = (int)VF_POOL_SIZE -
		kfifo_len(&delay_display_q) -
		kfifo_len(&display_q) -
		kfifo_len(&recycle_q) -
		kfifo_len(&newframe_q);

	if ((vh264_stream_switching_state != SWITCHING_STATE_OFF)
		|| !EN_SWITCH_FENCE())
		count = 0;
	else
		count = (count < 2) ? 0 : 2;

	while (kfifo_len(&delay_display_q) > count) {
		struct vframe_s *vf;

		if (kfifo_get(&delay_display_q, &vf)) {
			kfifo_put(&display_q,
				(const struct vframe_s *)vf);
			vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		}
	}
}

static struct vframe_s *vh264_vf_peek(void *op_arg)
{
	struct vframe_s *vf;

	if (kfifo_peek(&display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vh264_vf_get(void *op_arg)
{
	struct vframe_s *vf;

	if (kfifo_get(&display_q, &vf))
		return vf;

	return NULL;
}

static void vh264_vf_put(struct vframe_s *vf, void *op_arg)
{
	unsigned long flags;

	spin_lock_irqsave(&recycle_lock, flags);

	if ((vf != &fense_vf[0]) && (vf != &fense_vf[1]))
		kfifo_put(&recycle_q, (const struct vframe_s *)vf);

	spin_unlock_irqrestore(&recycle_lock, flags);
}

static int vh264_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_RESET) {
		unsigned long flags;

		amvdec_stop();
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vf_light_unreg_provider(&vh264_vf_prov);
#endif
		spin_lock_irqsave(&lock, flags);
		vh264_local_init();
		vh264_prot_init();
		spin_unlock_irqrestore(&lock, flags);
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vf_reg_provider(&vh264_vf_prov);
#endif
		amvdec_start();
	}
	return 0;
}

static int vh264_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&newframe_q);
	states->buf_avail_num = kfifo_len(&display_q) +
				kfifo_len(&delay_display_q);
	states->buf_recycle_num = kfifo_len(&recycle_q);

	spin_unlock_irqrestore(&lock, flags);

	return 0;
}

#if 0
static tvin_trans_fmt_t convert_3d_format(u32 type)
{
	const tvin_trans_fmt_t conv_tab[] = {
		0,		/* checkerboard */
		0,		/* column alternation */
		TVIN_TFMT_3D_LA,	/* row alternation */
		TVIN_TFMT_3D_LRH_OLER,	/* side by side */
		TVIN_TFMT_3D_FA	/* top bottom */
	};

	return (type <= 4) ? conv_tab[type] : 0;
}
#endif


#ifdef DEBUG_CC_USER_DATA
static int vbi_to_ascii(int c)
{
	if (c < 0)
		return '?';

	c &= 0x7F;

	if (c < 0x20 || c >= 0x7F)
		return '.';

	return c;
}

static void dump_cc_ascii(const uint8_t *buf, int poc)
{
	int cc_flag;
	int cc_count;
	int i;
	int szAscii[32];
	int index = 0;

	cc_flag = buf[1] & 0x40;
	if (!cc_flag) {
		pr_info("### cc_flag is invalid\n");
		return;
	}
	cc_count = buf[1] & 0x1f;

	for (i = 0; i < cc_count; ++i) {
		unsigned int b0;
		unsigned int cc_valid;
		unsigned int cc_type;
		unsigned char cc_data1;
		unsigned char cc_data2;

		b0 = buf[3 + i * 3];
		cc_valid = b0 & 4;
		cc_type = b0 & 3;
		cc_data1 = buf[4 + i * 3];
		cc_data2 = buf[5 + i * 3];


		if (cc_type == 0) {
			/* NTSC pair, Line 21 */
			szAscii[index++] = vbi_to_ascii(cc_data1);
			szAscii[index++] = vbi_to_ascii(cc_data2);
			if ((!cc_valid) || (i >= 3))
				break;
		}
	}
	switch (index) {
	case 8:
		pr_info("push poc:%d : %c %c %c %c %c %c %c %c\n",
			poc,
			szAscii[0], szAscii[1], szAscii[2], szAscii[3],
			szAscii[4], szAscii[5], szAscii[6], szAscii[7]);
		break;
	case 7:
		pr_info("push poc:%d : %c %c %c %c %c %c %c\n",
			poc,
			szAscii[0], szAscii[1], szAscii[2], szAscii[3],
			szAscii[4], szAscii[5], szAscii[6]);
		break;
	case 6:
		pr_info("push poc:%d : %c %c %c %c %c %c\n", poc,
			szAscii[0], szAscii[1], szAscii[2], szAscii[3],
			szAscii[4], szAscii[5]);
		break;
	case 5:
		pr_info("push poc:%d : %c %c %c %c %c\n", poc,
			szAscii[0], szAscii[1], szAscii[2], szAscii[3],
			szAscii[4]);
		break;
	case 4:
		pr_info("push poc:%d : %c %c %c %c\n", poc,
			szAscii[0], szAscii[1], szAscii[2], szAscii[3]);
		break;
	case 3:
		pr_info("push poc:%d : %c %c %c\n", poc,
			szAscii[0], szAscii[1], szAscii[2]);
		break;
	case 2:
		pr_info("push poc:%d : %c %c\n", poc,
			szAscii[0], szAscii[1]);
		break;
	case 1:
		pr_info("push poc:%d : %c\n", poc, szAscii[0]);
		break;
	default:
		pr_info("push poc:%d and no CC data: index = %d\n",
			poc, index);
		break;
	}
}

/*
#define DUMP_USER_DATA_HEX
*/

#ifdef DUMP_USER_DATA_HEX
static void print_data(unsigned char *pdata, int len)
{
	int nLeft;

	nLeft = len;

	while (nLeft >= 8) {
		pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
			pdata[0], pdata[1], pdata[2], pdata[3],
			pdata[4], pdata[5], pdata[6], pdata[7]);
		nLeft -= 8;
		pdata += 8;
	}
}
#endif

static void aml_swap_data(uint8_t *user_data, int ud_size)
{
	int swap_blocks, i, j, k, m;
	unsigned char c_temp;

	/* swap byte order */
	swap_blocks = ud_size / 8;
	for (i = 0; i < swap_blocks; i++) {
		j = i * 8;
		k = j + 7;
		for (m = 0; m < 4; m++) {
			c_temp = user_data[j];
			user_data[j++] = user_data[k];
			user_data[k--] = c_temp;
		}
	}
}

static void dump_data(unsigned int user_data_wp,
						unsigned int user_data_length,
						int poc)
{
	unsigned char *pdata;
	int user_data_len;
	int wp_start;
	int nLeft;
	unsigned char szBuf[256];
	int nOffset;

	dma_sync_single_for_cpu(amports_get_dma_device(),
			sei_data_buffer_phys, USER_DATA_SIZE,
			DMA_FROM_DEVICE);

	if (user_data_length & 0x07)
		user_data_len = (user_data_length + 8) & 0xFFFFFFF8;
	else
		user_data_len = user_data_length;

	if (user_data_wp >= user_data_len) {
		wp_start = user_data_wp - user_data_len;

		pdata = (unsigned char *)sei_data_buffer;
		pdata += wp_start;
		nLeft = user_data_len;

		memset(szBuf, 0, 256);
		memcpy(szBuf, pdata, user_data_len);
	} else {
		wp_start = user_data_wp +
			USER_DATA_SIZE - user_data_len;

		pdata = (unsigned char *)sei_data_buffer;
		pdata += wp_start;
		nLeft = USER_DATA_SIZE - wp_start;

		memset(szBuf, 0, 256);
		memcpy(szBuf, pdata, nLeft);
		nOffset = nLeft;

		pdata = (unsigned char *)sei_data_buffer;
		nLeft = user_data_wp;
		memcpy(szBuf+nOffset, pdata, nLeft);
	}

	aml_swap_data(szBuf, user_data_len);
#ifdef DUMP_USER_DATA_HEX
	print_data(szBuf, user_data_len);
#endif
	dump_cc_ascii(szBuf+7, poc);
}
#endif


static void userdata_push_do_work(struct work_struct *work)
{
	unsigned int sei_itu35_flags;
	unsigned int sei_itu35_wp;
	unsigned int sei_itu35_data_length;
	struct userdata_poc_info_t user_data_poc;

	sei_itu35_flags = READ_VREG(AV_SCRATCH_J);
	sei_itu35_wp = (sei_itu35_flags >> 16) & 0xffff;
	sei_itu35_data_length = sei_itu35_flags & 0x7fff;

#if 0
	pr_info("pocinfo 0x%x, top poc %d, wp 0x%x, length %d\n",
				   READ_VREG(AV_SCRATCH_L),
				   READ_VREG(AV_SCRATCH_M),
				   sei_itu35_wp, sei_itu35_data_length);
#endif
	user_data_poc.poc_info = READ_VREG(AV_SCRATCH_L);
	user_data_poc.poc_number = READ_VREG(AV_SCRATCH_M);
#ifdef DEBUG_CC_USER_DATA
	dump_data(sei_itu35_wp, sei_itu35_data_length,
		user_data_poc.poc_number);
#endif
	WRITE_VREG(AV_SCRATCH_J, 0);
	wakeup_userdata_poll(user_data_poc, sei_itu35_wp,
					(unsigned long)sei_data_buffer,
					USER_DATA_SIZE, sei_itu35_data_length);
}


static void set_frame_info(struct vframe_s *vf)
{
	vf->width = frame_width;
	vf->height = frame_height;
	vf->duration = frame_dur;
	vf->ratio_control =
		(min(h264_ar, (u32) DISP_RATIO_ASPECT_RATIO_MAX)) <<
		DISP_RATIO_ASPECT_RATIO_BIT;
	vf->orientation = vh264_rotation;
	vf->flag = 0;

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
	vf->trans_fmt = 0;
	if ((vf->trans_fmt == TVIN_TFMT_3D_LRF) ||
		(vf->trans_fmt == TVIN_TFMT_3D_LA)) {
		vf->left_eye.start_x = 0;
		vf->left_eye.start_y = 0;
		vf->left_eye.width = frame_width / 2;
		vf->left_eye.height = frame_height;

		vf->right_eye.start_x = 0;
		vf->right_eye.start_y = 0;
		vf->right_eye.width = frame_width / 2;
		vf->right_eye.height = frame_height;
	} else if ((vf->trans_fmt == TVIN_TFMT_3D_LRH_OLER) ||
			   (vf->trans_fmt == TVIN_TFMT_3D_TB)) {
		vf->left_eye.start_x = 0;
		vf->left_eye.start_y = 0;
		vf->left_eye.width = frame_width / 2;
		vf->left_eye.height = frame_height;

		vf->right_eye.start_x = 0;
		vf->right_eye.start_y = 0;
		vf->right_eye.width = frame_width / 2;
		vf->right_eye.height = frame_height;
	}
#endif

}

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
static void vh264_ppmgr_reset(void)
{
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_RESET, NULL);

	vh264_local_init();

	pr_info("vh264dec: vf_ppmgr_reset\n");
}
#endif

static int get_max_dpb_size(int level_idc, int mb_width, int mb_height)
{
	int size, r;

	switch (level_idc) {
	case 10:
		r = 1485;
		break;
	case 11:
		r = 3375;
		break;
	case 12:
	case 13:
	case 20:
		r = 8910;
		break;
	case 21:
		r = 17820;
		break;
	case 22:
	case 30:
		r = 30375;
		break;
	case 31:
		r = 67500;
		break;
	case 32:
		r = 76800;
		break;
	case 40:
	case 41:
	case 42:
		r = 122880;
		break;
	case 50:
		r = 414000;
		break;
	case 51:
	case 52:
		r = 691200;
		break;
	default:
		return 0;
		}
		size = (mb_width * mb_height +
				(mb_width * mb_height / 2)) * 256 * 10;
		r = (r * 1024 + size-1) / size;
		r = min(r, 16);
		/*pr_info("max_dpb %d size:%d\n", r, size);*/
		return r;
}
static void vh264_set_params(struct work_struct *work)
{
	int aspect_ratio_info_present_flag, aspect_ratio_idc;
	int max_dpb_size, actual_dpb_size, max_reference_size;
	int i, mb_mv_byte, ret;
	unsigned long addr;
	unsigned int post_canvas, buf_size;
	unsigned int frame_mbs_only_flag;
	unsigned int chroma_format_idc, chroma444, video_signal;
	unsigned int crop_infor, crop_bottom, crop_right, level_idc;
	if (!atomic_read(&vh264_active))
		return;
	mutex_lock(&vh264_mutex);
	if (vh264_stream_switching_state == SWITCHING_STATE_ON_CMD1)
		vh264_stream_switching_state = SWITCHING_STATE_ON_CMD1_PENDING;
	post_canvas = get_post_canvas();
	clk_adj_frame_count = 0;
	/* set to max decoder clock rate at the beginning */
	vdec_source_changed(VFORMAT_H264, 3840, 2160, 60);
	timing_info_present_flag = 0;
	mb_width = READ_VREG(AV_SCRATCH_1);
	seq_info = READ_VREG(AV_SCRATCH_2);
	aspect_ratio_info = READ_VREG(AV_SCRATCH_3);
	num_units_in_tick = READ_VREG(AV_SCRATCH_4);
	time_scale = READ_VREG(AV_SCRATCH_5);
	level_idc = READ_VREG(AV_SCRATCH_A);
	if (level_idc > 0)
		saved_idc_level = level_idc;
	else if (saved_idc_level > 0)
		level_idc = saved_idc_level;
	video_signal = READ_VREG(AV_SCRATCH_H);
	video_signal_from_vui =
				((video_signal & 0xffff) << 8) |
				((video_signal & 0xff0000) >> 16) |
				((video_signal & 0x3f000000));
/*
 *	pr_info("video_signal_type_present_flag 0x%x\n",
 *				(video_signal_from_vui >> 29) & 1);
 *	pr_info("video_format  0x%x\n",
 *				(video_signal_from_vui >> 26) & 7);
 *	pr_info("video_full_range_flag  0x%x\n",
 *				(video_signal_from_vui >> 25) & 1);
 *	pr_info("color_description_present_flag  0x%x\n",
 *				(video_signal_from_vui >> 24) & 1);
 *	pr_info("color_primaries	0x%x\n",
 *				(video_signal_from_vui >> 16) & 0xff);
 *	pr_info("transfer_characteristic	0x%x\n",
 *				(video_signal_from_vui >> 8) & 0xff);
 *	pr_info("matrix_coefficient	0x%x\n",
 *				video_signal_from_vui  & 0xff);
 */

	mb_total = (mb_width >> 8) & 0xffff;
	max_reference_size = (mb_width >> 24) & 0x7f;
	mb_mv_byte = (mb_width & 0x80000000) ? 24 : 96;
	if (ucode_type == UCODE_IP_ONLY_PARAM)
		mb_mv_byte = 96;
	mb_width = mb_width & 0xff;
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
		if (!mb_width && mb_total)
			mb_width = 256;
	}
	mb_height = mb_total / mb_width;
	last_duration = 0;
	/* AV_SCRATCH_2
	 *  bit 15: frame_mbs_only_flag
	 *  bit 13-14: chroma_format_idc
	 */
	frame_mbs_only_flag = (seq_info >> 15) & 0x01;
	chroma_format_idc = (seq_info >> 13) & 0x03;
	chroma444 = (chroma_format_idc == 3) ? 1 : 0;

	/* @AV_SCRATCH_6.31-16 =  (left  << 8 | right ) << 1
	 *   @AV_SCRATCH_6.15-0   =  (top << 8  | bottom ) <<
	 * (2 - frame_mbs_only_flag)
	 */
	crop_infor = READ_VREG(AV_SCRATCH_6);
	crop_bottom = (crop_infor & 0xff) >> (2 - frame_mbs_only_flag);
	crop_right = ((crop_infor >> 16) & 0xff) >> (2 - frame_mbs_only_flag);

	/* if width or height from outside is not equal to mb, then use mb */
	/* add: for seeking stream with other resolution */
	if ((last_mb_width && (last_mb_width != mb_width))
		|| (mb_width != ((frame_width + 15) >> 4)))
		frame_width = 0;
	if ((last_mb_height && (last_mb_height != mb_height))
		|| (mb_height != ((frame_height + 15) >> 4)))
		frame_height = 0;
	last_mb_width = mb_width;
	last_mb_height = mb_height;

	if ((frame_width == 0) || (frame_height == 0) || crop_infor) {
		frame_width = mb_width << 4;
		frame_height = mb_height << 4;
		if (frame_mbs_only_flag) {
			frame_height =
				frame_height - (2 >> chroma444) *
				min(crop_bottom,
					(unsigned int)((8 << chroma444) - 1));
			frame_width =
				frame_width - (2 >> chroma444) * min(crop_right,
						(unsigned
						 int)((8 << chroma444) - 1));
		} else {
			frame_height =
				frame_height - (4 >> chroma444) *
				min(crop_bottom,
					(unsigned int)((8 << chroma444)
							  - 1));
			frame_width =
				frame_width - (4 >> chroma444) * min(crop_right,
						(unsigned
						 int)((8 <<
							   chroma444)
							  - 1));
		}
#if 0
		pr_info
		("frame_mbs_only_flag %d, crop_bottom %d,  frame_height %d, ",
		 frame_mbs_only_flag, crop_bottom, frame_height);
		pr_info
		("mb_height %d,crop_right %d, frame_width %d, mb_width %d\n",
		 mb_height, crop_right, frame_width, mb_width);
#endif
		if (frame_height == 1088)
			frame_height = 1080;
	}

	mb_width = (mb_width + 3) & 0xfffffffc;
	mb_height = (mb_height + 3) & 0xfffffffc;
	mb_total = mb_width * mb_height;

	 /*max_reference_size <= max_dpb_size <= actual_dpb_size*/
	 is_4k = (mb_total > 8160) ? true:false;


	max_dpb_size = get_max_dpb_size(level_idc, mb_width, mb_height);
	if (max_dpb_size < max_reference_size)
		max_dpb_size = max_reference_size;
	if (max_dpb_size > 15
		&& get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB
		&& (codec_mm_get_total_size() < 80 * SZ_1M)) {
				actual_dpb_size
				= max_reference_size + dpb_size_adj;
			if (actual_dpb_size > VF_BUF_NUM)
				actual_dpb_size = VF_BUF_NUM;
	} else {
		actual_dpb_size = max_dpb_size + dpb_size_adj;
		actual_dpb_size = min(actual_dpb_size, VF_BUF_NUM);
	}
	max_reference_size++;
	pr_info("actual_dpb_size %d max_dpb_size %d max_ref %d\n",
				actual_dpb_size, max_dpb_size,
				max_reference_size);
	buf_size = mb_total * mb_mv_byte * max_reference_size;

	ret = decoder_bmmu_box_alloc_buf_phy(mm_blk_handle, 1,
		buf_size, DRIVER_NAME, &addr);

	if (ret < 0) {
		fatal_error_flag =
			DECODER_FATAL_ERROR_NO_MEM;
		vh264_running = 0;
		mutex_unlock(&vh264_mutex);
		return;
	}

	WRITE_VREG(AV_SCRATCH_1, addr);
	WRITE_VREG(AV_SCRATCH_3, post_canvas);
	WRITE_VREG(AV_SCRATCH_4, addr + buf_size);

	if (!(READ_VREG(AV_SCRATCH_F) & 0x1)) {
		for (i = 0; i < actual_dpb_size; i++) {
#ifdef DOUBLE_WRITE
				int page_count =
				PAGE_ALIGN((mb_total << 8) + (mb_total
						<< 7) +	(mb_total << 6) +
						(mb_total << 5)) / PAGE_SIZE;
#else
				int page_count =
					PAGE_ALIGN((mb_total << 8) +
						(mb_total << 7)) / PAGE_SIZE;
#endif

			ret = decoder_bmmu_box_alloc_buf_phy(mm_blk_handle,
				VF_BUFFER_IDX(i),
				page_count << PAGE_SHIFT,
				DRIVER_NAME, &buffer_spec[i].phy_addr);

			if (ret < 0) {
				buffer_spec[i].alloc_count = 0;
				fatal_error_flag =
				DECODER_FATAL_ERROR_NO_MEM;
				vh264_running = 0;
				mutex_unlock(&vh264_mutex);
				return;
			}

			addr = buffer_spec[i].phy_addr;
			buffer_spec[i].alloc_count = page_count;

			if (i <= 21) {
				buffer_spec[i].y_addr = addr;
				addr += mb_total << 8;
				buffer_spec[i].u_addr = addr;
				buffer_spec[i].v_addr = addr;
				addr += mb_total << 7;
				vfbuf_use[i] = 0;

				buffer_spec[i].y_canvas_index = 128 + i * 2;
				buffer_spec[i].u_canvas_index = 128 + i * 2 + 1;
				buffer_spec[i].v_canvas_index = 128 + i * 2 + 1;

				buffer_spec[i].y_canvas_width = mb_width << 4;
				buffer_spec[i].y_canvas_height = mb_height << 4;
				buffer_spec[i].u_canvas_width = mb_width << 4;
				buffer_spec[i].u_canvas_height = mb_height << 4;
				buffer_spec[i].v_canvas_width = mb_width << 4;
				buffer_spec[i].v_canvas_height = mb_height << 4;

				canvas_config(128 + i * 2,
						buffer_spec[i].y_addr,
						mb_width << 4, mb_height << 4,
						CANVAS_ADDR_NOWRAP,
						CANVAS_BLKMODE_32X32);
				canvas_config(128 + i * 2 + 1,
						buffer_spec[i].u_addr,
						mb_width << 4, mb_height << 3,
						CANVAS_ADDR_NOWRAP,
						CANVAS_BLKMODE_32X32);
				WRITE_VREG(ANC0_CANVAS_ADDR + i,
						spec2canvas(&buffer_spec[i]));
				} else {
				buffer_spec[i].y_canvas_index =
					2 * (i - 21) + 4;
				buffer_spec[i].y_addr = addr;
				addr += mb_total << 8;
				buffer_spec[i].u_canvas_index =
					2 * (i - 21) + 5;
				buffer_spec[i].v_canvas_index =
					2 * (i - 21) + 5;
				buffer_spec[i].u_addr = addr;
				addr += mb_total << 7;
				vfbuf_use[i] = 0;

				buffer_spec[i].y_canvas_width = mb_width << 4;
				buffer_spec[i].y_canvas_height = mb_height << 4;
				buffer_spec[i].u_canvas_width = mb_width << 4;
				buffer_spec[i].u_canvas_height = mb_height << 4;
				buffer_spec[i].v_canvas_width = mb_width << 4;
				buffer_spec[i].v_canvas_height = mb_height << 4;

				spec_set_canvas(&buffer_spec[i]
					, mb_width << 4, mb_height << 4);
				WRITE_VREG(ANC0_CANVAS_ADDR + i
					, spec2canvas(&buffer_spec[i]));
			}
		}
		} else {
			fatal_error_flag =
					DECODER_FATAL_ERROR_NO_MEM;
			vh264_running = 0;
			mutex_unlock(&vh264_mutex);
			pr_err("never be here!!\n");
			return;
		}

	timing_info_present_flag = seq_info & 0x2;
	fixed_frame_rate_flag = 0;
	aspect_ratio_info_present_flag = seq_info & 0x1;
	aspect_ratio_idc = (seq_info >> 16) & 0xff;

	if (timing_info_present_flag) {
		fixed_frame_rate_flag = seq_info & 0x40;

		if (((num_units_in_tick * 120) >= time_scale
			 && ((!sync_outside) || (!frame_dur))) &&
				num_units_in_tick
			&& time_scale) {
			if (use_idr_framerate || !frame_dur
				|| !duration_from_pts_done || vh264_running) {
				u32 frame_dur_es =
					div_u64(96000ULL * 2 *
							num_units_in_tick,
							time_scale);

				/* hack to avoid use ES frame duration
				 *   when it's half of the rate from
				 *  system info
				 */
				/* sometimes the encoder is given a wrong
				 * frame rate but the system side information
				 *is more reliable
				 */
				if ((frame_dur * 2) != frame_dur_es) {
					frame_dur = frame_dur_es;
					if (fr_hint_status == VDEC_NEED_HINT) {
						schedule_work(&notify_work);
						fr_hint_status = VDEC_HINTED;
					}
				}
			}
		}
	} else
		pr_info("H.264: timing_info not present\n");

	if (aspect_ratio_info_present_flag) {
		if (aspect_ratio_idc == EXTEND_SAR) {
			h264_ar =
				div_u64(256ULL * (aspect_ratio_info >> 16) *
						frame_height,
						(aspect_ratio_info & 0xffff) *
						frame_width);
		} else {
			/* pr_info("v264dec: aspect_ratio_idc = %d\n",
			 *  aspect_ratio_idc);
			 */

			switch (aspect_ratio_idc) {
			case 1:
				h264_ar = 0x100 * frame_height / frame_width;
				break;
			case 2:
				h264_ar = 0x100 * frame_height * 11 /
					(frame_width * 12);
				break;
			case 3:
				h264_ar = 0x100 * frame_height * 11 /
					(frame_width * 10);
				break;
			case 4:
				h264_ar = 0x100 * frame_height * 11 /
					(frame_width * 16);
				break;
			case 5:
				h264_ar = 0x100 * frame_height * 33 /
					(frame_width * 40);
				break;
			case 6:
				h264_ar = 0x100 * frame_height * 11 /
					(frame_width * 24);
				break;
			case 7:
				h264_ar = 0x100 * frame_height * 11 /
					(frame_width * 20);
				break;
			case 8:
				h264_ar = 0x100 * frame_height * 11 /
					(frame_width * 32);
				break;
			case 9:
				h264_ar = 0x100 * frame_height * 33 /
					(frame_width * 80);
				break;
			case 10:
				h264_ar = 0x100 * frame_height * 11 /
					(frame_width * 18);
				break;
			case 11:
				h264_ar = 0x100 * frame_height * 11 /
					(frame_width * 15);
				break;
			case 12:
				h264_ar = 0x100 * frame_height * 33 /
					(frame_width * 64);
				break;
			case 13:
				h264_ar = 0x100 * frame_height * 99 /
					(frame_width * 160);
				break;
			case 14:
				h264_ar = 0x100 * frame_height * 3 /
					(frame_width * 4);
				break;
			case 15:
				h264_ar = 0x100 * frame_height * 2 /
					(frame_width * 3);
				break;
			case 16:
				h264_ar = 0x100 * frame_height * 1 /
					(frame_width * 2);
				break;
			default:
				if (vh264_ratio >> 16) {
					h264_ar = (frame_height *
							(vh264_ratio & 0xffff) *
							0x100 +
							((vh264_ratio >> 16) *
							 frame_width / 2)) /
						((vh264_ratio >> 16) *
						 frame_width);
				} else {
					h264_ar = frame_height * 0x100 /
						frame_width;
				}
				break;
			}
		}
	} else {
		pr_info("v264dec: aspect_ratio not available from source\n");
		if (vh264_ratio >> 16) {
			/* high 16 bit is width, low 16 bit is height */
			h264_ar =
				((vh264_ratio & 0xffff) * frame_height * 0x100 +
				 (vh264_ratio >> 16) * frame_width / 2) /
				((vh264_ratio >> 16) * frame_width);
		} else
			h264_ar = frame_height * 0x100 / frame_width;
	}

	WRITE_VREG(AV_SCRATCH_0,
			(max_reference_size << 24) | (actual_dpb_size << 16) |
			(max_dpb_size << 8));
	if (vh264_stream_switching_state != SWITCHING_STATE_OFF) {
		vh264_stream_switching_state = SWITCHING_STATE_OFF;
		pr_info("Leaving switching mode.\n");
	}
	mutex_unlock(&vh264_mutex);
}

static unsigned int pts_inc_by_duration(
		unsigned int *new_pts, unsigned int *new_pts_rem)
{
	unsigned int r, rem;

	r = last_pts + DUR2PTS(frame_dur);
	rem = last_pts_remainder + DUR2PTS_REM(frame_dur);

	if (rem >= 96) {
		r++;
		rem -= 96;
	}

	if (new_pts)
		*new_pts = r;
	if (new_pts_rem)
		*new_pts_rem = rem;

	return r;
}
static inline bool vh264_isr_parser(struct vframe_s *vf,
		unsigned int  pts_valid, unsigned int buffer_index,
		unsigned int pts)
{
	unsigned int pts_duration = 0;

	if (h264_first_pts_ready == 0) {
		if (pts_valid == 0) {
			vfbuf_use[buffer_index]++;
			vf->index = buffer_index;
			kfifo_put(&recycle_q,
					(const struct vframe_s *)vf);
			return false;
		}

		h264pts1 = pts;
		h264_pts_count = 0;
		h264_first_pts_ready = 1;
	} else {
		if (pts < h264pts1) {
			if (h264_pts_count > 24) {
				pr_info("invalid h264pts1, reset\n");
				h264pts1 = pts;
				h264_pts_count = 0;
			}
		}
		if (pts_valid && (pts > h264pts1) && (h264_pts_count > 24)
				&& (duration_from_pts_done == 0)) {
			unsigned int
				old_duration = frame_dur;
			h264pts2 = pts;

			pts_duration = (h264pts2 - h264pts1) * 16 /
				(h264_pts_count * 15);

			if ((pts_duration != frame_dur)
					&& (!pts_outside)) {
				if (use_idr_framerate) {
					bool pts_c_24 = close_to(pts_duration,
						RATE_24_FPS,
						RATE_CORRECTION_THRESHOLD);
					bool frm_c_25 = close_to(frame_dur,
						RATE_25_FPS,
						RATE_CORRECTION_THRESHOLD);
					bool pts_c_25 = close_to(pts_duration,
						  RATE_25_FPS,
						  RATE_CORRECTION_THRESHOLD);
					bool frm_c_24 = close_to(frame_dur,
						  RATE_24_FPS,
						  RATE_CORRECTION_THRESHOLD);
					if ((pts_c_24 && frm_c_25)
						|| (pts_c_25 && frm_c_24)) {
						pr_info
						("H.264:Correct frame dur ");
						pr_info
						(" from %d to duration based ",
							 frame_dur);
						pr_info
						("on PTS %d ---\n",
							 pts_duration);
						frame_dur = pts_duration;
						duration_from_pts_done = 1;
					} else if (((frame_dur < 96000 / 240)
						&& (pts_duration > 96000 / 240))
						|| (!duration_on_correcting &&
						!frm_c_25 && !frm_c_24)) {
						/* fft: if the frame rate is
						 *  not regular, use the
						 * calculate rate insteadof.
						 */
						pr_info
						("H.264:Correct frame dur ");
						pr_info
						(" from %d to duration based ",
							 frame_dur);
						pr_info
						("on PTS %d ---\n",
							 pts_duration);
						frame_dur = pts_duration;
						duration_on_correcting = 1;
					}
				} else {
					if (close_to(pts_duration,
							frame_dur, 2000)) {
						frame_dur = pts_duration;
						pr_info
						("used calculate frame rate,");
						pr_info("on duration =%d\n",
							 frame_dur);
					} else {
						pr_info
						("don't use calculate frame ");
						pr_info
						("rate pts_duration =%d\n",
							 pts_duration);
					}
				}
			}

			if (duration_from_pts_done == 0) {
				if (close_to
						(pts_duration,
						 old_duration,
						 RATE_CORRECTION_THRESHOLD)) {
					pr_info
					("finished correct frame dur");
					pr_info
					(" new=%d,old_duration=%d,cnt=%d\n",
						 pts_duration,
						 old_duration,
						 h264_pts_count);
					duration_from_pts_done = 1;
				} else {	/*not the same,redo it. */
					if (!close_to(pts_duration,
						 old_duration, 1000) &&
						!close_to(pts_duration,
						frame_dur, 1000) &&
						close_to(pts_duration,
						last_duration, 200)) {
						/* yangle: frame_dur must
						 *  wrong,recover it.
						 */
						frame_dur = pts_duration;
					}

					pr_info
					("restart correct frame duration ");
					pr_info
					("new=%d,old_duration=%d,cnt=%d\n",
						 pts_duration,
						 old_duration,
						 h264_pts_count);
					h264pts1 = h264pts2;
					h264_pts_count = 0;
					duration_from_pts_done = 0;
			}
		}
		    last_duration = pts_duration;
		}
	}
	return true;
}
#ifdef HANDLE_H264_IRQ
static irqreturn_t vh264_isr(int irq, void *dev_id)
#else
static void vh264_isr(void)
#endif
{
	unsigned int buffer_index;
	struct vframe_s *vf;
	unsigned int cpu_cmd;
	unsigned int pts, pts_lookup_save, pts_valid_save, pts_valid = 0;
	unsigned int pts_us64_valid = 0;
	u64 pts_us64;
	bool force_interlaced_frame = false;
	unsigned int sei_itu35_flags;
	static const unsigned int idr_num =
		FIX_FRAME_RATE_CHECK_IDRFRAME_NUM;
	static const unsigned int flg_1080_itl =
		DEC_CONTROL_FLAG_FORCE_2997_1080P_INTERLACE;
	static const unsigned int flg_576_itl =
		DEC_CONTROL_FLAG_FORCE_2500_576P_INTERLACE;

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	if (0 == (stat & STAT_VDEC_RUN)) {
		pr_info("decoder is not running\n");
#ifdef HANDLE_H264_IRQ
		return IRQ_HANDLED;
#else
		return;
#endif
	}

	cpu_cmd = READ_VREG(AV_SCRATCH_0);

#ifdef DROP_B_FRAME_FOR_1080P_50_60FPS
	if ((frame_dur < 2004) &&
		(frame_width >= 1400) &&
		(frame_height >= 1000) && (last_interlaced == 0))
		SET_VREG_MASK(AV_SCRATCH_F, 0x8);
#endif
	if ((decoder_force_reset == 1)
			|| ((error_recovery_mode != 1)
			&& (no_idr_error_count >= no_idr_error_max)
			&& (ucode_type != UCODE_IP_ONLY_PARAM))) {
		vh264_running = 0;
		pr_info("force reset decoder  %d!!!\n", no_idr_error_count);
		schedule_work(&error_wd_work);
		decoder_force_reset = 0;
		no_idr_error_count = 0;
	} else if ((cpu_cmd & 0xff) == 1) {
		if (unlikely
			(vh264_running
			 && (kfifo_len(&newframe_q) != VF_POOL_SIZE))) {
			/* a cmd 1 sent during decoding w/o getting a cmd 3. */
			/* should not happen but the original code has such
			 *  case, do the same process
			 */
			if ((READ_VREG(AV_SCRATCH_1) & 0xff)
				== 1) {/*invalid mb_width*/
				vh264_running = 0;
				fatal_error_flag = DECODER_FATAL_ERROR_UNKNOWN;
			/* this is fatal error, need restart */
				pr_info("cmd 1 fatal error happened\n");
				schedule_work(&error_wd_work);
			} else {
			vh264_stream_switching_state = SWITCHING_STATE_ON_CMD1;
			pr_info("Enter switching mode cmd1.\n");
			schedule_work(&stream_switching_work);
			}
			return IRQ_HANDLED;
		}
		pr_info("Enter set parameter cmd1.\n");
		schedule_work(&set_parameter_work);
		return IRQ_HANDLED;
	} else if ((cpu_cmd & 0xff) == 2) {
		int frame_mb_only, pic_struct_present, pic_struct, prog_frame,
			poc_sel, idr_flag, eos, error;
		int i, status, num_frame, b_offset;
		int current_error_count, slice_type;

		vh264_running = 1;
		vh264_no_disp_count = 0;
		num_frame = (cpu_cmd >> 8) & 0xff;
		frame_mb_only = seq_info & 0x8000;
		pic_struct_present = seq_info & 0x10;

		current_error_count = READ_VREG(AV_SCRATCH_D);
		if (vh264_error_count != current_error_count) {
			/* pr_info("decoder error happened, count %d\n",
			 *   current_error_count);
			 */
			vh264_error_count = current_error_count;
		}

		for (i = 0; (i < num_frame) && (!vh264_eos); i++) {
			status = READ_VREG(AV_SCRATCH_1 + i);
			buffer_index = status & 0x1f;
			error = status & 0x200;
			slice_type = (READ_VREG(AV_SCRATCH_H) >> (i * 4)) & 0xf;

			if ((error_recovery_mode_use & 2) && error)
				check_pts_discontinue = true;
			if (ucode_type == UCODE_IP_ONLY_PARAM
				&& iponly_early_mode)
				continue;
			if ((p_last_vf != NULL)
				&& (p_last_vf->index == buffer_index))
				continue;

			if (buffer_index >= VF_BUF_NUM)
				continue;

			pic_struct = (status >> 5) & 0x7;
			prog_frame = status & 0x100;
			poc_sel = status & 0x200;
			idr_flag = status & 0x400;
			frame_packing_type = (status >> 12) & 0x7;
			eos = (status >> 15) & 1;

			if (eos)
				vh264_eos = 1;

			b_offset = (status >> 16) & 0xffff;

			if (error)
				no_idr_error_count++;
			if (idr_flag ||
				(!error && (slice_type != SLICE_TYPE_I)))
				no_idr_error_count = 0;

			if (decoder_debug_flag) {
				pr_info
				("slice_type %x idr %x error %x count %d",
					slice_type, idr_flag, error,
					no_idr_error_count);
				pr_info(" prog %x pic_struct %x offset %x\n",
				prog_frame, pic_struct,	b_offset);
			}
#ifdef DROP_B_FRAME_FOR_1080P_50_60FPS
			last_interlaced = prog_frame ? 0 : 1;
#endif
			if (kfifo_get(&newframe_q, &vf) == 0) {
				pr_info
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}

			if (clk_adj_frame_count < VDEC_CLOCK_ADJUST_FRAME)
				clk_adj_frame_count++;

			set_frame_info(vf);

			switch (i) {
			case 0:
				b_offset |=
					(READ_VREG(AV_SCRATCH_A) & 0xffff)
					<< 16;
				break;
			case 1:
				b_offset |=
					READ_VREG(AV_SCRATCH_A) & 0xffff0000;
				break;
			case 2:
				b_offset |=
					(READ_VREG(AV_SCRATCH_B) & 0xffff)
					<< 16;
				break;
			case 3:
				b_offset |=
					READ_VREG(AV_SCRATCH_B) & 0xffff0000;
				break;
			case 4:
				b_offset |=
					(READ_VREG(AV_SCRATCH_C) & 0xffff)
					<< 16;
				break;
			case 5:
				b_offset |=
					READ_VREG(AV_SCRATCH_C) & 0xffff0000;
				break;
			default:
				break;
			}

			if (error)
				gvs->drop_frame_count++;

			/* add 64bit pts us ; */
			if (unlikely
				((b_offset == first_offset)
				 && (first_pts_cached))) {
				pts = first_pts;
				pts_us64 = first_pts64;
				first_pts_cached = false;
				pts_valid = 1;
				pts_us64_valid = 1;
#ifdef DEBUG_PTS
				pts_hit++;
#endif
			} else if (pts_lookup_offset_us64
					   (PTS_TYPE_VIDEO, b_offset, &pts, 0,
						&pts_us64) == 0) {
				pts_valid = 1;
				pts_us64_valid = 1;
#ifdef DEBUG_PTS
				pts_hit++;
#endif
			} else {
				pts_valid = 0;
				pts_us64_valid = 0;
#ifdef DEBUG_PTS
				pts_missed++;
#endif
			}

			/* on second IDR frame,check the diff between pts
			 *  compute from duration and pts from lookup ,
			 * if large than frame_dur,we think it is uncorrect.
			 */
			pts_lookup_save = pts;
			pts_valid_save = pts_valid;
			if (fixed_frame_rate_flag
				&& (fixed_frame_rate_check_count <=
					idr_num)) {
				if (idr_flag && pts_valid) {
					fixed_frame_rate_check_count++;
					/* pr_info("diff:%d\n",
					 *   last_pts - pts_lookup_save);
					 */
					if ((fixed_frame_rate_check_count ==
						idr_num) &&
						(abs(pts - (last_pts +
							 DUR2PTS(frame_dur))) >
							 DUR2PTS(frame_dur))) {
						fixed_frame_rate_flag = 0;
						pr_info("pts sync mode play\n");
					}

					if (fixed_frame_rate_flag
						&& (fixed_frame_rate_check_count
							> idr_num)) {
						pr_info
						("fix_frame_rate mode play\n");
					}
				}
			}

			if (READ_VREG(AV_SCRATCH_F) & 2) {
				/* for I only mode, ignore the PTS information
				 *   and only uses frame duration for each I
				 *  frame decoded
				 */
				if (p_last_vf)
					pts_valid = 0;
				/* also skip frame duration calculation
				 *  based on PTS
				 */
				duration_from_pts_done = 1;
				/* and add a default duration for 1/30 second
				 *  if there is no valid frame
				 * duration available
				 */
				if (frame_dur == 0)
					frame_dur = 96000 / 30;
			}

			if (sync_outside == 0) {
				if (!vh264_isr_parser(vf,
						pts_valid, buffer_index, pts))
					continue;

				h264_pts_count++;
			} else {
				/* non-idr or non-I frame will set pts_valid */
				if (!idr_flag && !(slice_type == SLICE_TYPE_I))
					pts_valid = 0;
			}

			if (pts_valid && !pts_discontinue) {
				pts_discontinue =
					(abs(last_pts - pts) >=
					 tsync_vpts_discontinuity_margin());
			}
			/* if use_idr_framerate or fixed frame rate, only
			 *  use PTS for IDR frames except for pts discontinue
			 */
			if (timing_info_present_flag &&
				frame_dur &&
				(use_idr_framerate ||
				 (fixed_frame_rate_flag != 0))
				&& pts_valid && h264_first_valid_pts_ready
				&& (!pts_discontinue)) {
				pts_valid =
					(slice_type == SLICE_TYPE_I) ? 1 : 0;
			}

			if (!h264_first_valid_pts_ready && pts_valid) {
				h264_first_valid_pts_ready = true;
				last_pts = pts - DUR2PTS(frame_dur);
				last_pts_remainder = 0;
			}
			/* calculate PTS of next frame and smooth
			 *  PTS for fixed rate source
			 */
			if (pts_valid) {
				if ((fixed_frame_rate_flag) &&
					(!pts_discontinue) &&
					(abs(pts_inc_by_duration(NULL, NULL)
					     - pts)
					 < DUR2PTS(frame_dur))) {
					pts = pts_inc_by_duration(&pts,
							&last_pts_remainder);
				} else
					last_pts_remainder = 0;

			} else {
				if (fixed_frame_rate_flag && !pts_discontinue &&
				(fixed_frame_rate_check_count > idr_num) &&
				pts_valid_save && (sync_outside == 0) &&
				(abs(pts_inc_by_duration(NULL, NULL) - pts)
				 > DUR2PTS(frame_dur))) {
					duration_from_pts_done = 0;
					pr_info("recalc frame_dur\n");
				} else
					pts = pts_inc_by_duration(&pts,
						&last_pts_remainder);
				pts_valid = 1;
			}

			if ((dec_control &
				 flg_1080_itl)
				&& (frame_width == 1920)
				&& (frame_height >= 1080)
				&& (vf->duration == 3203))
				force_interlaced_frame = true;
			else if ((dec_control &
					  flg_576_itl)
					 && (frame_width == 720)
					 && (frame_height == 576)
					 && (vf->duration == 3840))
				force_interlaced_frame = true;

			/* for frames with PTS, check if there is PTS
			 *  discontinue based on previous frames
			 * (including error frames),
			 * force no VPTS discontinue reporting if we saw
			 *errors earlier but only once.
			 */

			/*count info*/
			gvs->frame_dur = frame_dur;
			vdec_count_info(gvs, error, b_offset);

			if ((pts_valid) && (check_pts_discontinue)
					&& (!error)) {
				if (pts_discontinue) {
					vf->flag = 0;
					check_pts_discontinue = false;
				} else if ((pts - last_pts) < 90000) {
					vf->flag = VFRAME_FLAG_NO_DISCONTINUE;
					check_pts_discontinue = false;
				}
			}

			last_pts = pts;

			if (fixed_frame_rate_flag
				&& (fixed_frame_rate_check_count <=
					idr_num)
				&& (sync_outside == 0)
				&& pts_valid_save)
				pts = pts_lookup_save;

			if (pic_struct_present) {
				if ((pic_struct == PIC_TOP_BOT)
					|| (pic_struct == PIC_BOT_TOP))
					prog_frame = 0;
			}

			if ((!force_interlaced_frame)
				&& (prog_frame
					|| (pic_struct_present
						&& pic_struct
						<= PIC_TRIPLE_FRAME))) {
				if (pic_struct_present) {
					if (pic_struct == PIC_TOP_BOT_TOP
						|| pic_struct
						== PIC_BOT_TOP_BOT) {
						vf->duration +=
							vf->duration >> 1;
					} else if (pic_struct ==
							   PIC_DOUBLE_FRAME)
						vf->duration += vf->duration;
					else if (pic_struct ==
							 PIC_TRIPLE_FRAME) {
						vf->duration +=
							vf->duration << 1;
					}
				}

				last_pts =
					last_pts + DUR2PTS(vf->duration -
							frame_dur);

				vf->index = buffer_index;
				vf->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_VIU_NV21;
				vf->duration_pulldown = 0;
				vf->signal_type = video_signal_from_vui;
				vf->index = buffer_index;
				vf->pts = (pts_valid) ? pts : 0;
				if (pts_us64_valid == 1)
					vf->pts_us64 = pts_us64;
				else
				vf->pts_us64 = div64_u64(((u64)vf->pts)*100, 9);
				vf->canvas0Addr = vf->canvas1Addr =
					spec2canvas(&buffer_spec[buffer_index]);
				vf->type_original = vf->type;
				vfbuf_use[buffer_index]++;
				vf->mem_handle =
					decoder_bmmu_box_get_mem_handle(
						mm_blk_handle,
						VF_BUFFER_IDX(buffer_index));
				if ((error_recovery_mode_use & 2) && error) {
					kfifo_put(&recycle_q,
						(const struct vframe_s *)vf);
				} else {
					p_last_vf = vf;
					pts_discontinue = false;
					kfifo_put(&delay_display_q,
						  (const struct vframe_s *)vf);
				}
			} else {
				if (pic_struct_present
					&& pic_struct == PIC_TOP_BOT)
					vf->type = VIDTYPE_INTERLACE_TOP;
				else if (pic_struct_present
						 && pic_struct == PIC_BOT_TOP)
					vf->type = VIDTYPE_INTERLACE_BOTTOM;
				else {
					vf->type =
						poc_sel ?
						VIDTYPE_INTERLACE_BOTTOM :
						VIDTYPE_INTERLACE_TOP;
				}
				vf->type |= VIDTYPE_VIU_NV21;
				vf->type |= VIDTYPE_INTERLACE_FIRST;

				high_bandwidth |=
				((codec_mm_get_total_size() < 80 * SZ_1M)
				& ((READ_VREG(AV_SCRATCH_N) & 0xf) == 3)
				& ((frame_width * frame_height) >= 1920*1080));
				if (high_bandwidth)
					vf->flag |= VFRAME_FLAG_HIGH_BANDWIDTH;

				vf->duration >>= 1;
				vf->duration_pulldown = 0;
				vf->signal_type = video_signal_from_vui;
				vf->index = buffer_index;
				vf->pts = (pts_valid) ? pts : 0;
				if (pts_us64_valid == 1)
					vf->pts_us64 = pts_us64;
				else
				vf->pts_us64 = div64_u64(((u64)vf->pts)*100, 9);
				vf->canvas0Addr = vf->canvas1Addr =
					spec2canvas(&buffer_spec[buffer_index]);
				vf->type_original = vf->type;
				vfbuf_use[buffer_index]++;
				vf->ready_jiffies64 = jiffies_64;
				vf->mem_handle =
					decoder_bmmu_box_get_mem_handle(
						mm_blk_handle,
						VF_BUFFER_IDX(buffer_index));
				if ((error_recovery_mode_use & 2) && error) {
					kfifo_put(&recycle_q,
						(const struct vframe_s *)vf);
					continue;
				} else {
					pts_discontinue = false;
					kfifo_put(&delay_display_q,
						(const struct vframe_s *)vf);
				}

				if (READ_VREG(AV_SCRATCH_F) & 2)
					continue;

				if (kfifo_get(&newframe_q, &vf) == 0) {
					pr_info
					("fatal error, no avail buffer slot.");
					return IRQ_HANDLED;
				}

				set_frame_info(vf);

				if (pic_struct_present
					&& pic_struct == PIC_TOP_BOT)
					vf->type = VIDTYPE_INTERLACE_BOTTOM;
				else if (pic_struct_present
						 && pic_struct == PIC_BOT_TOP)
					vf->type = VIDTYPE_INTERLACE_TOP;
				else {
					vf->type =
						poc_sel ?
						VIDTYPE_INTERLACE_TOP :
						VIDTYPE_INTERLACE_BOTTOM;
				}

				vf->type |= VIDTYPE_VIU_NV21;
				vf->duration >>= 1;
				vf->duration_pulldown = 0;
				vf->signal_type = video_signal_from_vui;
				vf->index = buffer_index;
				vf->pts = 0;
				vf->pts_us64 = 0;
				vf->canvas0Addr = vf->canvas1Addr =
					spec2canvas(&buffer_spec[buffer_index]);
				vf->type_original = vf->type;
				vfbuf_use[buffer_index]++;
				if (high_bandwidth)
					vf->flag |= VFRAME_FLAG_HIGH_BANDWIDTH;

				p_last_vf = vf;
				vf->ready_jiffies64 = jiffies_64;
				vf->mem_handle =
					decoder_bmmu_box_get_mem_handle(
						mm_blk_handle,
						VF_BUFFER_IDX(buffer_index));
				kfifo_put(&delay_display_q,
						(const struct vframe_s *)vf);
			}
		}

		WRITE_VREG(AV_SCRATCH_0, 0);
	} else if ((cpu_cmd & 0xff) == 3) {
		vh264_running = 1;
		vh264_stream_switching_state = SWITCHING_STATE_ON_CMD3;

		pr_info("Enter switching mode cmd3.\n");
		schedule_work(&stream_switching_work);

	} else if ((cpu_cmd & 0xff) == 4) {
		vh264_running = 1;
		/* reserved for slice group */
		WRITE_VREG(AV_SCRATCH_0, 0);
	} else if ((cpu_cmd & 0xff) == 5) {
		vh264_running = 1;
		/* reserved for slice group */
		WRITE_VREG(AV_SCRATCH_0, 0);
	} else if ((cpu_cmd & 0xff) == 6) {
		vh264_running = 0;
		fatal_error_flag = DECODER_FATAL_ERROR_UNKNOWN;
		/* this is fatal error, need restart */
		pr_info("fatal error happend\n");
		amvdec_stop();
		if (!fatal_error_reset)
			schedule_work(&error_wd_work);
	} else if ((cpu_cmd & 0xff) == 7) {
		vh264_running = 0;
		frame_width = (READ_VREG(AV_SCRATCH_1) + 1) * 16;
		pr_info("Over decoder supported size, width = %d\n",
			   frame_width);
		fatal_error_flag = DECODER_FATAL_ERROR_SIZE_OVERFLOW;
	} else if ((cpu_cmd & 0xff) == 8) {
		vh264_running = 0;
		frame_height = (READ_VREG(AV_SCRATCH_1) + 1) * 16;
		pr_info("Over decoder supported size, height = %d\n",
			   frame_height);
		fatal_error_flag = DECODER_FATAL_ERROR_SIZE_OVERFLOW;
	} else if ((cpu_cmd & 0xff) == 9) {
		first_offset = READ_VREG(AV_SCRATCH_1);
		if (pts_lookup_offset_us64
			(PTS_TYPE_VIDEO, first_offset, &first_pts, 0,
			 &first_pts64) == 0)
			first_pts_cached = true;
		WRITE_VREG(AV_SCRATCH_0, 0);
	} else if ((cpu_cmd & 0xff) == 0xa) {
		int b_offset;

		b_offset = READ_VREG(AV_SCRATCH_2);
		buffer_index = READ_VREG(AV_SCRATCH_1);
		/*pr_info("iponly output %d  b_offset %x\n",
		 *	buffer_index,b_offset);
		 */
		if (kfifo_get(&newframe_q, &vf) == 0) {
			WRITE_VREG(AV_SCRATCH_0, 0);
			pr_info
			("fatal error, no available buffer slot.");
			return IRQ_HANDLED;
		}
		if (pts_lookup_offset_us64 (PTS_TYPE_VIDEO, b_offset,
				&pts, 0, &pts_us64) != 0)
			vf->pts_us64 = vf->pts = 0;
		else {
			vf->pts_us64 = pts_us64;
			vf->pts = pts;
		}
		set_frame_info(vf);
		vf->type = VIDTYPE_PROGRESSIVE |
				VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
		vf->duration_pulldown = 0;
		vf->signal_type = video_signal_from_vui;
		vf->index = buffer_index;
		vf->canvas0Addr = vf->canvas1Addr =
				spec2canvas(&buffer_spec[buffer_index]);
		vf->type_original = vf->type;
		vf->mem_handle = decoder_bmmu_box_get_mem_handle(
			mm_blk_handle,
					VF_BUFFER_IDX(buffer_index));
		vfbuf_use[buffer_index]++;
		p_last_vf = vf;
		pts_discontinue = false;
		iponly_early_mode = 1;
		kfifo_put(&delay_display_q,
			(const struct vframe_s *)vf);
		WRITE_VREG(AV_SCRATCH_0, 0);
	}

	sei_itu35_flags = READ_VREG(AV_SCRATCH_J);
	if (sei_itu35_flags & (1 << 15)) {	/* data ready */
		schedule_work(&userdata_push_work);
	}
#ifdef HANDLE_H264_IRQ
	return IRQ_HANDLED;
#else
	return;
#endif
}

static void vh264_set_clk(struct work_struct *work)
{
	/*pr_info
	("vh264_set_clk: clk_adj_frame_count=%d saved_resolution=%d frame_dur=%d ucode_type=%d \n",
	 clk_adj_frame_count, saved_resolution, frame_dur, ucode_type);*/
	if (ucode_type != UCODE_IP_ONLY_PARAM &&
		(clk_adj_frame_count >= VDEC_CLOCK_ADJUST_FRAME) &&
		frame_dur > 0 && saved_resolution !=
		frame_width * frame_height * (96000 / frame_dur)) {
		int fps = 96000 / frame_dur;

		if (frame_dur < 10) /*dur is too small ,think it errors fps*/
			fps = 60;
		saved_resolution = frame_width * frame_height * fps;
		vdec_source_changed(VFORMAT_H264,
			frame_width, frame_height, fps);
	}
}

static void vh264_put_timer_func(unsigned long arg)
{
	struct timer_list *timer = (struct timer_list *)arg;
	unsigned int wait_buffer_status;
	unsigned int wait_i_pass_frames;
	unsigned int reg_val;

	enum receviver_start_e state = RECEIVER_INACTIVE;

	if (vh264_reset) {
		pr_info("operation forbidden in timer !\n");
		goto exit;
	}

	prepare_display_q();

	if (vf_get_receiver(PROVIDER_NAME)) {
		state =
			vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_QUREY_STATE,
					NULL);
		if ((state == RECEIVER_STATE_NULL)
			|| (state == RECEIVER_STATE_NONE)) {
			/* receiver has no event_cb or receiver's
			 *  event_cb does not process this event
			 */
			state = RECEIVER_INACTIVE;
		}
	} else
		state = RECEIVER_INACTIVE;
#ifndef HANDLE_H264_IRQ
	vh264_isr();
#endif

	if (vh264_stream_switching_state != SWITCHING_STATE_OFF)
		wait_buffer_counter = 0;
	else {
		reg_val = READ_VREG(AV_SCRATCH_9);
		wait_buffer_status = reg_val & (1 << 31);
		wait_i_pass_frames = reg_val & 0xff;
		if (wait_buffer_status) {
			if (kfifo_is_empty(&display_q) &&
				kfifo_is_empty(&delay_display_q) &&
				kfifo_is_empty(&recycle_q) &&
				(state == RECEIVER_INACTIVE)) {
				pr_info("$$$$decoder is waiting for buffer\n");
				if (++wait_buffer_counter > 4) {
					amvdec_stop();
					schedule_work(&error_wd_work);
				}
			} else
				wait_buffer_counter = 0;
		} else if (wait_i_pass_frames > 1000) {
			pr_info("i passed frames > 1000\n");
			amvdec_stop();
			schedule_work(&error_wd_work);
		}
	}

#if 0
	if (!wait_buffer_status) {
		if (vh264_no_disp_count++ > NO_DISP_WD_COUNT) {
			pr_info("$$$decoder did not send frame out\n");
			amvdec_stop();
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
			vh264_ppmgr_reset();
#else
			vf_light_unreg_provider(PROVIDER_NAME);
			vh264_local_init();
			vf_reg_provider(vh264_vf_prov);
#endif
			vh264_prot_init();
			amvdec_start();

			vh264_no_disp_count = 0;
			vh264_no_disp_wd_count++;
		}
	}
#endif

	while (!kfifo_is_empty(&recycle_q) &&
		   ((READ_VREG(AV_SCRATCH_7) == 0)
			|| (READ_VREG(AV_SCRATCH_8) == 0))
		   && (vh264_stream_switching_state == SWITCHING_STATE_OFF)) {
		struct vframe_s *vf;

		if (kfifo_get(&recycle_q, &vf)) {
			if ((vf->index >= 0) && (vf->index < VF_BUF_NUM)) {
				if (--vfbuf_use[vf->index] == 0) {
					if (READ_VREG(AV_SCRATCH_7) == 0) {
						WRITE_VREG(AV_SCRATCH_7,
								vf->index + 1);
					} else {
						WRITE_VREG(AV_SCRATCH_8,
								vf->index + 1);
					}
				}

				vf->index = VF_BUF_NUM;
				kfifo_put(&newframe_q,
						(const struct vframe_s *)vf);
			}
		}
	}

	if (vh264_stream_switching_state != SWITCHING_STATE_OFF) {
		while (!kfifo_is_empty(&recycle_q)) {
			struct vframe_s *vf;

			if (kfifo_get(&recycle_q, &vf)) {
				if ((vf->index >= 0 &&
					(vf->index < VF_BUF_NUM))) {
					vf->index = VF_BUF_NUM;
					kfifo_put(&newframe_q,
						(const struct vframe_s *)vf);
				}
			}
		}

		WRITE_VREG(AV_SCRATCH_7, 0);
		WRITE_VREG(AV_SCRATCH_8, 0);

		if (kfifo_len(&newframe_q) == VF_POOL_SIZE)
			stream_switching_done();
	}

	schedule_work(&set_clk_work);

exit:
	timer->expires = jiffies + PUT_INTERVAL;

	add_timer(timer);
}

int vh264_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	vstatus->frame_width = frame_width;
	vstatus->frame_height = frame_height;
	if (frame_dur != 0)
		vstatus->frame_rate = 96000 / frame_dur;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = READ_VREG(AV_SCRATCH_D);
	vstatus->status = stat;
	if (fatal_error_reset)
		vstatus->status |= fatal_error_flag;
	vstatus->bit_rate = gvs->bit_rate;
	vstatus->frame_dur = frame_dur;
	vstatus->frame_data = gvs->frame_data;
	vstatus->total_data = gvs->total_data;
	vstatus->frame_count = gvs->frame_count;
	vstatus->error_frame_count = gvs->error_frame_count;
	vstatus->drop_frame_count = gvs->drop_frame_count;
	vstatus->total_data = gvs->total_data;
	vstatus->samp_cnt = gvs->samp_cnt;
	vstatus->offset = gvs->offset;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s", DRIVER_NAME);

	return 0;
}

static int vh264_vdec_info_init(void)
{
	gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	return 0;
}

int vh264_set_trickmode(struct vdec_s *vdec, unsigned long trickmode)
{
	if (trickmode == TRICKMODE_I) {
		WRITE_VREG(AV_SCRATCH_F,
				   (READ_VREG(AV_SCRATCH_F) & 0xfffffffc) | 2);
		trickmode_i = 1;
	} else if (trickmode == TRICKMODE_NONE) {
		WRITE_VREG(AV_SCRATCH_F, READ_VREG(AV_SCRATCH_F) & 0xfffffffc);
		trickmode_i = 0;
	}

	return 0;
}

int vh264_set_isreset(struct vdec_s *vdec, int isreset)
{
	is_reset = isreset;
	return 0;
}

static void vh264_prot_init(void)
{

	while (READ_VREG(DCAC_DMA_CTRL) & 0x8000)
		;
	while (READ_VREG(LMEM_DMA_CTRL) & 0x8000)
		;		/* reg address is 0x350 */

#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
	WRITE_VREG(DOS_SW_RESET0, 0);

	READ_VREG(DOS_SW_RESET0);
	READ_VREG(DOS_SW_RESET0);
	READ_VREG(DOS_SW_RESET0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
	WRITE_VREG(DOS_SW_RESET0, 0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 9) | (1 << 8));
	WRITE_VREG(DOS_SW_RESET0, 0);

	READ_VREG(DOS_SW_RESET0);
	READ_VREG(DOS_SW_RESET0);
	READ_VREG(DOS_SW_RESET0);

#else
	WRITE_RESET_REG(RESET0_REGISTER,
				   RESET_IQIDCT | RESET_MC | RESET_VLD_PART);
	READ_RESET_REG(RESET0_REGISTER);
	WRITE_RESET_REG(RESET0_REGISTER,
				   RESET_IQIDCT | RESET_MC | RESET_VLD_PART);

	WRITE_RESET_REG(RESET2_REGISTER, RESET_PIC_DC | RESET_DBLK);
#endif

	WRITE_VREG(POWER_CTL_VLD,
			   READ_VREG(POWER_CTL_VLD) |
			   (0 << 10) | (1 << 9) | (1 << 6));

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);

	WRITE_VREG(AV_SCRATCH_0, 0);
	WRITE_VREG(AV_SCRATCH_1, buf_offset);
	if (!tee_enabled())
		WRITE_VREG(AV_SCRATCH_G, mc_dma_handle);
	WRITE_VREG(AV_SCRATCH_7, 0);
	WRITE_VREG(AV_SCRATCH_8, 0);
	WRITE_VREG(AV_SCRATCH_9, 0);
	WRITE_VREG(AV_SCRATCH_N, 0);

#ifdef SUPPORT_BAD_MACRO_BLOCK_REDUNDANCY
	if (bad_block_scale > 128)
		bad_block_scale = 128;
	WRITE_VREG(AV_SCRATCH_A, bad_block_scale);
#endif

	error_recovery_mode_use =
		(error_recovery_mode !=
		 0) ? error_recovery_mode : error_recovery_mode_in;
	WRITE_VREG(AV_SCRATCH_F,
			   (READ_VREG(AV_SCRATCH_F) & 0xffffffc3) |
			   (READ_VREG(AV_SCRATCH_F) & 0xffffff43) |
			   ((error_recovery_mode_use & 0x1) << 4));
	if (dec_control & DEC_CONTROL_FLAG_DISABLE_FAST_POC)
		SET_VREG_MASK(AV_SCRATCH_F, 1 << 7);
	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);

	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
	if (ucode_type == UCODE_IP_ONLY_PARAM)
		SET_VREG_MASK(AV_SCRATCH_F, 1 << 6);
	else
		CLEAR_VREG_MASK(AV_SCRATCH_F, 1 << 6);

	WRITE_VREG(AV_SCRATCH_I, (u32)(sei_data_buffer_phys - buf_offset));
	WRITE_VREG(AV_SCRATCH_J, 0);
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) && !is_meson_mtvd_cpu()) {
		/* pr_info("vh264 meson8 prot init\n"); */
		WRITE_VREG(MDEC_PIC_DC_THRESH, 0x404038aa);
	}
	/* #endif */
}

static int vh264_local_init(void)
{
	int i, ret;
	u32 size;
	unsigned long buf_start;
	vh264_ratio = vh264_amstream_dec_info.ratio;
	/* vh264_ratio = 0x100; */

	vh264_rotation = (((unsigned long) vh264_amstream_dec_info.param)
				>> 16) & 0xffff;

	frame_prog = 0;
	frame_width = vh264_amstream_dec_info.width;
	frame_height = vh264_amstream_dec_info.height;
	frame_dur = vh264_amstream_dec_info.rate;
	pts_outside = ((unsigned long) vh264_amstream_dec_info.param) & 0x01;
	sync_outside = ((unsigned long) vh264_amstream_dec_info.param & 0x02)
			>> 1;
	use_idr_framerate = ((unsigned long) vh264_amstream_dec_info.param
				& 0x04) >> 2;
	max_refer_buf = !(((unsigned long) vh264_amstream_dec_info.param
				& 0x10) >> 4);
	if (!vh264_reset) {
		if (mm_blk_handle) {
			decoder_bmmu_box_free(mm_blk_handle);
			mm_blk_handle = NULL;
		}

		mm_blk_handle = decoder_bmmu_box_alloc_box(
			DRIVER_NAME,
			0,
			MAX_BLK_BUFFERS,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER |
			tvp_flag);
	}
	pr_info
	("H264 sysinfo: %dx%d duration=%d, pts_outside=%d \n",
	 frame_width, frame_height, frame_dur, pts_outside);
	pr_debug("sync_outside=%d, use_idr_framerate=%d\n",
	 sync_outside, use_idr_framerate);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB)
		size = V_BUF_ADDR_OFFSET_NEW;
	else
		size = V_BUF_ADDR_OFFSET;

	ret = decoder_bmmu_box_alloc_buf_phy(mm_blk_handle, 0,
		size, DRIVER_NAME, &buf_start);
	if (ret < 0)
		return ret;

	buf_offset = buf_start - DEF_BUF_START_ADDR;

	if ((unsigned long) vh264_amstream_dec_info.param & 0x08)
		ucode_type = UCODE_IP_ONLY_PARAM;
	else
		ucode_type = 0;

	if ((unsigned long) vh264_amstream_dec_info.param & 0x20)
		error_recovery_mode_in = 1;
	else
		error_recovery_mode_in = 3;

	if (!vh264_running) {
		last_mb_width = 0;
		last_mb_height = 0;
	}

	for (i = 0; i < VF_BUF_NUM; i++)
		vfbuf_use[i] = 0;

	INIT_KFIFO(display_q);
	INIT_KFIFO(delay_display_q);
	INIT_KFIFO(recycle_q);
	INIT_KFIFO(newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &vfpool[i];

		vfpool[i].index = VF_BUF_NUM;
		vfpool[i].bufWidth = 1920;
		kfifo_put(&newframe_q, vf);
	}

#ifdef DROP_B_FRAME_FOR_1080P_50_60FPS
	last_interlaced = 1;
#endif
	h264_first_pts_ready = 0;
	h264_first_valid_pts_ready = false;
	h264pts1 = 0;
	h264pts2 = 0;
	h264_pts_count = 0;
	duration_from_pts_done = 0;
	vh264_error_count = READ_VREG(AV_SCRATCH_D);

	p_last_vf = NULL;
	check_pts_discontinue = false;
	last_pts = 0;
	wait_buffer_counter = 0;
	vh264_no_disp_count = 0;
	fatal_error_flag = 0;
	high_bandwidth = 0;
	vh264_stream_switching_state = SWITCHING_STATE_OFF;
#ifdef DEBUG_PTS
	pts_missed = 0;
	pts_hit = 0;
#endif
	pts_discontinue = false;
	no_idr_error_count = 0;

	reset_userdata_fifo(1);

	if (enable_switch_fense) {
		for (i = 0; i < ARRAY_SIZE(fense_buffer_spec); i++) {
			struct buffer_spec_s *s = &fense_buffer_spec[i];
			s->alloc_count = 3 * SZ_1M / PAGE_SIZE;
			ret = decoder_bmmu_box_alloc_buf_phy(mm_blk_handle,
				FENSE_BUFFER_IDX(i),
				3 * SZ_1M, DRIVER_NAME, &s->phy_addr);

			if (ret < 0) {
				fatal_error_flag =
				DECODER_FATAL_ERROR_NO_MEM;
				vh264_running = 0;
				return ret;
			}
			s->y_canvas_index = 2 * i;
			s->u_canvas_index = 2 * i + 1;
			s->v_canvas_index = 2 * i + 1;
		}
	}
	return 0;
}

static s32 vh264_init(void)
{
	int ret = 0;
	int trickmode_fffb = 0;
	int firmwareloaded = 0;

	/* pr_info("\nvh264_init\n"); */
	init_timer(&recycle_timer);

	stat |= STAT_TIMER_INIT;

	vh264_running = 0;/* init here to reset last_mb_width&last_mb_height */
	vh264_eos = 0;
	duration_on_correcting = 0;
	first_pts = 0;
	first_pts64 = 0;
	first_offset = 0;
	first_pts_cached = false;
	fixed_frame_rate_check_count = 0;
	fr_hint_status = VDEC_NO_NEED_HINT;
	saved_resolution = 0;
	iponly_early_mode = 0;
	saved_idc_level = 0;

	/*init vdec status*/
	ret = vh264_vdec_info_init();
	if (0 != ret)
		return -ret;

	ret = vh264_local_init();
	if (ret < 0)
		return ret;
	query_video_status(0, &trickmode_fffb);

	amvdec_enable();
	if (!firmwareloaded && tee_enabled()) {
		if (tee_load_video_fw((u32)VIDEO_DEC_H264, 0) != 0) {
			amvdec_disable();
			return -1;
		}
	} else {
	/* -- ucode loading (amrisc and swap code) */
	mc_cpu_addr =
		dma_alloc_coherent(amports_get_dma_device(), MC_TOTAL_SIZE,
				&mc_dma_handle, GFP_KERNEL);
	if (!mc_cpu_addr) {
		amvdec_disable();
		del_timer_sync(&recycle_timer);
		pr_err("vh264_init: Can not allocate mc memory.\n");
		return -ENOMEM;
	}

	pr_debug("264 ucode swap area: phyaddr %p, cpu vaddr %p\n",
		(void *)mc_dma_handle, mc_cpu_addr);
	if (debugfirmware) {
		int r0, r1, r2, r3, r4, r5;
		char firmwarename[32];

		pr_debug("start load debug %d firmware ...\n", debugfirmware);

		snprintf(firmwarename, 32, "%s%d", "vh264_mc", debugfirmware);
		r0 = amvdec_loadmc_ex(VFORMAT_H264, firmwarename, NULL);

#define DEBUGGET_FW(t, name, buf, size, ret)\
		do {\
			snprintf(firmwarename, 32, "%s%d", name,\
				debugfirmware);\
			ret = get_decoder_firmware_data(t,\
				firmwarename, buf, size);\
		} while (0)
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_HEADER, vh264_header_mc,
		 *MC_SWAP_SIZE);
		 */
		DEBUGGET_FW(VFORMAT_H264, "vh264_header_mc",
			(u8 *) mc_cpu_addr + MC_OFFSET_HEADER,
			MC_SWAP_SIZE, r1);

		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_DATA, vh264_data_mc,
		 *MC_SWAP_SIZE);
		 */
		DEBUGGET_FW(VFORMAT_H264, "vh264_data_mc",
			(u8 *) mc_cpu_addr + MC_OFFSET_DATA, MC_SWAP_SIZE, r2);
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_MMCO, vh264_mmco_mc,
		 *MC_SWAP_SIZE);
		 */
		DEBUGGET_FW(VFORMAT_H264, "vh264_mmco_mc",
			(u8 *) mc_cpu_addr + MC_OFFSET_MMCO, MC_SWAP_SIZE, r3);
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_LIST, vh264_list_mc,
		 *MC_SWAP_SIZE);
		 */
		DEBUGGET_FW(VFORMAT_H264, "vh264_list_mc",
			(u8 *) mc_cpu_addr + MC_OFFSET_LIST, MC_SWAP_SIZE, r4);
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_SLICE, vh264_slice_mc,
		 *MC_SWAP_SIZE);
		 */
		DEBUGGET_FW(VFORMAT_H264, "vh264_slice_mc",
			(u8 *) mc_cpu_addr + MC_OFFSET_SLICE, MC_SWAP_SIZE, r5);

		if (r0 < 0 || r1 < 0 || r2 < 0 || r3 < 0 || r4 < 0 || r5 < 0) {
			pr_err("264 load debugfirmware err %d,%d,%d,%d,%d,%d\n",
			r0, r1, r2, r3, r4, r5);
			amvdec_disable();
			if (mc_cpu_addr) {
				dma_free_coherent(amports_get_dma_device(),
					MC_TOTAL_SIZE, mc_cpu_addr,
					mc_dma_handle);
				mc_cpu_addr = NULL;
			}
			return -EBUSY;
		}
		firmwareloaded = 1;
	} else {
		int ret = -1, size = -1;
		char *buf = vmalloc(0x1000 * 16);

		if (IS_ERR_OR_NULL(buf))
			return -ENOMEM;

		size = get_firmware_data(VIDEO_DEC_H264, buf);
		if (size < 0) {
			pr_err("get firmware fail.");
			vfree(buf);
			return -1;
		}

		ret = amvdec_loadmc_ex(VFORMAT_H264, NULL, buf);
		memcpy((u8 *) mc_cpu_addr + MC_OFFSET_HEADER,
			buf + 0x4000, MC_SWAP_SIZE);
		memcpy((u8 *) mc_cpu_addr + MC_OFFSET_DATA,
			buf + 0x2000, MC_SWAP_SIZE);
		memcpy((u8 *) mc_cpu_addr + MC_OFFSET_MMCO,
			buf + 0x6000, MC_SWAP_SIZE);
		memcpy((u8 *) mc_cpu_addr + MC_OFFSET_LIST,
			buf + 0x3000, MC_SWAP_SIZE);
		memcpy((u8 *) mc_cpu_addr + MC_OFFSET_SLICE,
			buf + 0x5000, MC_SWAP_SIZE);

		vfree(buf);

		if (ret < 0) {
			pr_err("h264 load orignal firmware error %d.\n", ret);
			amvdec_disable();
			if (mc_cpu_addr) {
				dma_free_coherent(amports_get_dma_device(),
					MC_TOTAL_SIZE, mc_cpu_addr,
					mc_dma_handle);
				mc_cpu_addr = NULL;
			}
			return -EBUSY;
		}
	}
	}

	stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	vh264_prot_init();

#ifdef HANDLE_H264_IRQ
	/*TODO irq */

	if (vdec_request_irq(VDEC_IRQ_1, vh264_isr,
			"vh264-irq", (void *)vh264_dec_id)) {
		pr_err("vh264 irq register error.\n");
		amvdec_disable();
		return -ENOENT;
	}
#endif

	stat |= STAT_ISR_REG;

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	vf_provider_init(&vh264_vf_prov, PROVIDER_NAME, &vh264_vf_provider_ops,
					 NULL);
	vf_reg_provider(&vh264_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
#else
	vf_provider_init(&vh264_vf_prov, PROVIDER_NAME, &vh264_vf_provider_ops,
					 NULL);
	vf_reg_provider(&vh264_vf_prov);
#endif

	if (frame_dur != 0) {
		if (!is_reset)
			vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_FR_HINT,
					(void *)((unsigned long)frame_dur));
		fr_hint_status = VDEC_HINTED;
	} else
		fr_hint_status = VDEC_NEED_HINT;

	stat |= STAT_VF_HOOK;

	recycle_timer.data = (ulong) &recycle_timer;
	recycle_timer.function = vh264_put_timer_func;
	recycle_timer.expires = jiffies + PUT_INTERVAL;

	add_timer(&recycle_timer);

	stat |= STAT_TIMER_ARM;

	vh264_stream_switching_state = SWITCHING_STATE_OFF;

	stat |= STAT_VDEC_RUN;
	wmb();			/* Ensure fetchbuf  contents visible */

	/* -- start decoder */
	amvdec_start();

	init_userdata_fifo();

	return 0;
}

static int vh264_stop(int mode)
{


	if (stat & STAT_VDEC_RUN) {
		amvdec_stop();
		stat &= ~STAT_VDEC_RUN;
	}

	if (stat & STAT_ISR_REG) {
		WRITE_VREG(ASSIST_MBOX1_MASK, 0);
		/*TODO irq */

		vdec_free_irq(VDEC_IRQ_1, (void *)vh264_dec_id);

		stat &= ~STAT_ISR_REG;
	}

	if (stat & STAT_TIMER_ARM) {
		del_timer_sync(&recycle_timer);
		stat &= ~STAT_TIMER_ARM;
	}

	if (stat & STAT_VF_HOOK) {
		if (mode == MODE_FULL) {
			if (fr_hint_status == VDEC_HINTED && !is_reset)
				vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_FR_END_HINT,
					NULL);
			fr_hint_status = VDEC_NO_NEED_HINT;
		}

		vf_unreg_provider(&vh264_vf_prov);
		stat &= ~STAT_VF_HOOK;
	}

	if (stat & STAT_MC_LOAD) {
		if (mc_cpu_addr != NULL) {
			dma_free_coherent(amports_get_dma_device(),
					MC_TOTAL_SIZE, mc_cpu_addr,
					mc_dma_handle);
			mc_cpu_addr = NULL;
		}
	}
	if (sei_data_buffer != NULL) {
		dma_free_coherent(
			amports_get_dma_device(),
			USER_DATA_RUND_SIZE,
			sei_data_buffer,
			sei_data_buffer_phys);
		sei_data_buffer = NULL;
		sei_data_buffer_phys = 0;
	}
	amvdec_disable();
	if (mm_blk_handle) {
		decoder_bmmu_box_free(mm_blk_handle);
		mm_blk_handle = NULL;
	}
	memset(&fense_buffer_spec, 0, sizeof(fense_buffer_spec));
	memset(&buffer_spec, 0, sizeof(buffer_spec));
	return 0;
}

static void error_do_work(struct work_struct *work)
{
	mutex_lock(&vh264_mutex);

	/*
	 * we need to lock vh264_stop/vh264_init.
	 * because we will call amvdec_h264_remove on this step;
	 * then we may call more than once on
	 * free_irq/deltimer/..and some other.
	 */
	if (atomic_read(&vh264_active)) {
		amvdec_stop();
		do {
			msleep(20);
		} while (vh264_stream_switching_state != SWITCHING_STATE_OFF);
		vh264_reset  = 1;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vh264_ppmgr_reset();
#else
		vf_light_unreg_provider(&vh264_vf_prov);

		vh264_local_init();

		vf_reg_provider(&vh264_vf_prov);
#endif
		msleep(30);
		vh264_prot_init();

		amvdec_start();
		vh264_reset  = 0;
	}

	mutex_unlock(&vh264_mutex);
}

static void stream_switching_done(void)
{
	int state = vh264_stream_switching_state;

	WRITE_VREG(AV_SCRATCH_7, 0);
	WRITE_VREG(AV_SCRATCH_8, 0);
	WRITE_VREG(AV_SCRATCH_9, 0);

	if (state == SWITCHING_STATE_ON_CMD1) {
		pr_info("Enter set parameter cmd1 switching_state %x.\n",
					vh264_stream_switching_state);
		schedule_work(&set_parameter_work);
		return;
	} else if (state == SWITCHING_STATE_ON_CMD1_PENDING)
		return;

	vh264_stream_switching_state = SWITCHING_STATE_OFF;

	wmb();			/* Ensure fetchbuf  contents visible */

	if (state == SWITCHING_STATE_ON_CMD3)
		WRITE_VREG(AV_SCRATCH_0, 0);

	pr_info("Leaving switching mode.\n");
}

/* construt a new frame as a copy of last frame so frame receiver can
 * release all buffer resources to decoder.
 */
static void stream_switching_do(struct work_struct *work)
{
	int mb_total_num, mb_width_num, mb_height_num, i = 0;
	struct vframe_s *vf = NULL;
	u32 y_index, u_index, src_index, des_index, y_desindex, u_desindex;
	struct canvas_s csy, csu, cyd;
	unsigned long flags;
	bool delay = true;

	if (!atomic_read(&vh264_active))
		return;

	if (vh264_stream_switching_state == SWITCHING_STATE_OFF)
		return;

	spin_lock_irqsave(&prepare_lock, flags);

	block_display_q = true;

	spin_unlock_irqrestore(&prepare_lock, flags);

	mb_total_num = mb_total;
	mb_width_num = mb_width;
	mb_height_num = mb_height;

	while (is_4k || kfifo_len(&delay_display_q) > 2) {
		if (kfifo_get(&delay_display_q, &vf)) {
			kfifo_put(&display_q,
				(const struct vframe_s *)vf);
			vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		} else
			break;
	}

	if (!kfifo_get(&delay_display_q, &vf)) {
		vf = p_last_vf;
		delay = false;
	}

	while (vf) {
		int buffer_index;

		buffer_index = vf->index & 0xff;

		/* construct a clone of the frame from last frame */

#if 0

		pr_info("src yaddr[0x%x] index[%d] width[%d] heigth[%d]\n",
			buffer_spec[buffer_index].y_addr,
			buffer_spec[buffer_index].y_canvas_index,
			buffer_spec[buffer_index].y_canvas_width,
			buffer_spec[buffer_index].y_canvas_height);

		pr_info("src uaddr[0x%x] index[%d] width[%d] heigth[%d]\n",
			buffer_spec[buffer_index].u_addr,
			buffer_spec[buffer_index].u_canvas_index,
			buffer_spec[buffer_index].u_canvas_width,
			buffer_spec[buffer_index].u_canvas_height);
#endif
		if (EN_SWITCH_FENCE()) {
			y_index = buffer_spec[buffer_index].y_canvas_index;
			u_index = buffer_spec[buffer_index].u_canvas_index;

			canvas_read(y_index, &csy);
			canvas_read(u_index, &csu);

			canvas_config(fense_buffer_spec[i].y_canvas_index,
				fense_buffer_spec[i].phy_addr,
				mb_width_num << 4, mb_height_num << 4,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(fense_buffer_spec[i].u_canvas_index,
				fense_buffer_spec[i].phy_addr +
				(mb_total_num << 8),
				mb_width_num << 4, mb_height_num << 3,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);

			y_desindex = fense_buffer_spec[i].y_canvas_index;
			u_desindex = fense_buffer_spec[i].u_canvas_index;

			canvas_read(y_desindex, &cyd);

			src_index = ((y_index & 0xff) |
				((u_index << 8) & 0x0000ff00));
			des_index = ((y_desindex & 0xff) |
				((u_desindex << 8) & 0x0000ff00));

			ge2d_canvas_dup(&csy, &csu, &cyd,
				GE2D_FORMAT_M24_NV21,
				src_index,
				des_index);
		}
		vf->mem_handle = decoder_bmmu_box_get_mem_handle(
			mm_blk_handle,
			FENSE_BUFFER_IDX(i));
		fense_vf[i] = *vf;
		fense_vf[i].index = -1;

		if (EN_SWITCH_FENCE())
			fense_vf[i].canvas0Addr =
				spec2canvas(&fense_buffer_spec[i]);
		else
			fense_vf[i].flag |= VFRAME_FLAG_SWITCHING_FENSE;

		/* send clone to receiver */
		kfifo_put(&display_q,
			(const struct vframe_s *)&fense_vf[i]);

		/* early recycle frames for last session */
		if (delay)
			vh264_vf_put(vf, NULL);

		vf_notify_receiver(PROVIDER_NAME,
			VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);

		i++;

		if (!kfifo_get(&delay_display_q, &vf))
			break;
	}

	block_display_q = false;

	pr_info("Switching fense frame post\n");
}

static int amvdec_h264_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;

	mutex_lock(&vh264_mutex);

	if (pdata == NULL) {
		pr_info("\namvdec_h264 memory resource undefined.\n");
		mutex_unlock(&vh264_mutex);
		return -EFAULT;
	}
	tvp_flag = vdec_secure(pdata) ? CODEC_MM_FLAGS_TVP : 0;
	if (pdata->sys_info)
		vh264_amstream_dec_info = *pdata->sys_info;
	if (sei_data_buffer == NULL) {
		sei_data_buffer =
			dma_alloc_coherent(amports_get_dma_device(),
				USER_DATA_RUND_SIZE,
				&sei_data_buffer_phys, GFP_KERNEL);
		if (!sei_data_buffer) {
			pr_info("%s: Can not allocate sei_data_buffer\n",
				   __func__);
			mutex_unlock(&vh264_mutex);
			return -ENOMEM;
		}
		/* pr_info("buffer 0x%x, phys 0x%x, remap 0x%x\n",
		 *   sei_data_buffer, sei_data_buffer_phys,
		 * (u32)sei_data_buffer_remap);
		 */
	}
	pdata->dec_status = vh264_dec_status;
	pdata->set_trickmode = vh264_set_trickmode;
	pdata->set_isreset = vh264_set_isreset;
	is_reset = 0;

	if (vh264_init() < 0) {
		pr_info("\namvdec_h264 init failed.\n");
		kfree(gvs);
		gvs = NULL;
		mutex_unlock(&vh264_mutex);
		return -ENODEV;
	}

	INIT_WORK(&error_wd_work, error_do_work);
	INIT_WORK(&stream_switching_work, stream_switching_do);
	INIT_WORK(&set_parameter_work, vh264_set_params);
	INIT_WORK(&notify_work, vh264_notify_work);
	INIT_WORK(&set_clk_work, vh264_set_clk);
	INIT_WORK(&userdata_push_work, userdata_push_do_work);

	atomic_set(&vh264_active, 1);

	mutex_unlock(&vh264_mutex);

	return 0;
}

static int amvdec_h264_remove(struct platform_device *pdev)
{
	atomic_set(&vh264_active, 0);
	cancel_work_sync(&set_parameter_work);
	cancel_work_sync(&error_wd_work);
	cancel_work_sync(&stream_switching_work);
	cancel_work_sync(&notify_work);
	cancel_work_sync(&set_clk_work);
	cancel_work_sync(&userdata_push_work);

	mutex_lock(&vh264_mutex);
	vh264_stop(MODE_FULL);
	vdec_source_changed(VFORMAT_H264, 0, 0, 0);
	atomic_set(&vh264_active, 0);
#ifdef DEBUG_PTS
	pr_info
	("pts missed %ld, pts hit %ld, pts_outside %d, duration %d, ",
	 pts_missed, pts_hit, pts_outside, frame_dur);
	pr_info("sync_outside %d, use_idr_framerate %d\n",
			sync_outside, use_idr_framerate);
#endif
	kfree(gvs);
	gvs = NULL;
	mutex_unlock(&vh264_mutex);
	return 0;
}

/****************************************/

static struct platform_driver amvdec_h264_driver = {
	.probe = amvdec_h264_probe,
	.remove = amvdec_h264_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static struct codec_profile_t amvdec_h264_profile = {
	.name = "h264",
	.profile = ""
};


static struct mconfig h264_configs[] = {
	MC_PU32("stat", &stat),
	MC_PU32("error_recovery_mode", &error_recovery_mode),
	MC_PU32("sync_outside", &sync_outside),
	MC_PU32("dec_control", &dec_control),
	MC_PU32("fatal_error_reset", &fatal_error_reset),
	MC_PU32("max_refer_buf", &max_refer_buf),
	MC_PU32("ucode_type", &ucode_type),
	MC_PU32("debugfirmware", &debugfirmware),
	MC_PU32("fixed_frame_rate_flag", &fixed_frame_rate_flag),
	MC_PU32("decoder_debug_flag", &decoder_debug_flag),
	MC_PU32("dpb_size_adj", &dpb_size_adj),
	MC_PU32("decoder_force_reset", &decoder_force_reset),
	MC_PU32("no_idr_error_max", &no_idr_error_max),
	MC_PU32("enable_switch_fense", &enable_switch_fense),
};
static struct mconfig_node h264_node;


static int __init amvdec_h264_driver_init_module(void)
{
	pr_debug("amvdec_h264 module init\n");

	ge2d_videoh264task_init();

	if (platform_driver_register(&amvdec_h264_driver)) {
		pr_err("failed to register amvdec_h264 driver\n");
		return -ENODEV;
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB
		&& (codec_mm_get_total_size() > 80 * SZ_1M)) {
		amvdec_h264_profile.profile = "4k";
	}
	vcodec_profile_register(&amvdec_h264_profile);
	INIT_REG_NODE_CONFIGS("media.decoder", &h264_node,
		"h264", h264_configs, CONFIG_FOR_RW);
	return 0;
}

static void __exit amvdec_h264_driver_remove_module(void)
{
	pr_debug("amvdec_h264 module remove.\n");

	platform_driver_unregister(&amvdec_h264_driver);

	ge2d_videoh264task_release();
}

/****************************************/

module_param(stat, uint, 0664);
MODULE_PARM_DESC(stat, "\n amvdec_h264 stat\n");
module_param(error_recovery_mode, uint, 0664);
MODULE_PARM_DESC(error_recovery_mode, "\n amvdec_h264 error_recovery_mode\n");
module_param(sync_outside, uint, 0664);
MODULE_PARM_DESC(sync_outside, "\n amvdec_h264 sync_outside\n");
module_param(dec_control, uint, 0664);
MODULE_PARM_DESC(dec_control, "\n amvdec_h264 decoder control\n");
module_param(fatal_error_reset, uint, 0664);
MODULE_PARM_DESC(fatal_error_reset,
		"\n amvdec_h264 decoder reset when fatal error happens\n");
module_param(max_refer_buf, uint, 0664);
MODULE_PARM_DESC(max_refer_buf,
		"\n amvdec_h264 dec buffering or not for reference frame\n");
module_param(ucode_type, uint, 0664);
MODULE_PARM_DESC(ucode_type,
		"\n amvdec_h264 dec buffering or not for reference frame\n");
module_param(debugfirmware, uint, 0664);
MODULE_PARM_DESC(debugfirmware, "\n amvdec_h264 debug load firmware\n");
module_param(fixed_frame_rate_flag, uint, 0664);
MODULE_PARM_DESC(fixed_frame_rate_flag,
				 "\n amvdec_h264 fixed_frame_rate_flag\n");
module_param(decoder_debug_flag, uint, 0664);
MODULE_PARM_DESC(decoder_debug_flag,
				 "\n amvdec_h264 decoder_debug_flag\n");

module_param(dpb_size_adj, uint, 0664);
MODULE_PARM_DESC(dpb_size_adj,
				 "\n amvdec_h264 dpb_size_adj\n");


module_param(decoder_force_reset, uint, 0664);
MODULE_PARM_DESC(decoder_force_reset,
		"\n amvdec_h264 decoder force reset\n");
module_param(no_idr_error_max, uint, 0664);
MODULE_PARM_DESC(no_idr_error_max,
		"\n print no_idr_error_max\n");
module_param(enable_switch_fense, uint, 0664);
MODULE_PARM_DESC(enable_switch_fense,
		"\n enable switch fense\n");

#ifdef SUPPORT_BAD_MACRO_BLOCK_REDUNDANCY
module_param(bad_block_scale, uint, 0664);
MODULE_PARM_DESC(bad_block_scale,
				"\n print bad_block_scale\n");
#endif

module_init(amvdec_h264_driver_init_module);
module_exit(amvdec_h264_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC H264 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chen Zhang <chen.zhang@amlogic.com>");
