/*
 * drivers/amlogic/amports/vh265.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/amlogic/tee.h>
#include "../../../stream_input/amports/amports_priv.h"
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "../utils/decoder_mmu_box.h"
#include "../utils/decoder_bmmu_box.h"
#include "../utils/config_parser.h"
#include "../utils/firmware.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include <trace/events/meson_atrace.h>

#define CONSTRAIN_MAX_BUF_NUM

#define SWAP_HEVC_UCODE
#define DETREFILL_ENABLE

#define AGAIN_HAS_THRESHOLD
/*#define TEST_NO_BUF*/
/*#define HEVC_PIC_STRUCT_SUPPORT*/
#define MULTI_INSTANCE_SUPPORT
#define USE_UNINIT_SEMA

			/* .buf_size = 0x100000*16,
			//4k2k , 0x100000 per buffer */
			/* 4096x2304 , 0x120000 per buffer */
#define MPRED_8K_MV_BUF_SIZE		(0x120000*4)
#define MPRED_4K_MV_BUF_SIZE		(0x120000)
#define MPRED_MV_BUF_SIZE		(0x40000)

#define MMU_COMPRESS_HEADER_SIZE  0x48000
#define MMU_COMPRESS_8K_HEADER_SIZE  (0x48000*4)

#define MAX_FRAME_4K_NUM 0x1200
#define MAX_FRAME_8K_NUM (0x1200*4)

//#define FRAME_MMU_MAP_SIZE  (MAX_FRAME_4K_NUM * 4)
#define H265_MMU_MAP_BUFFER       HEVC_ASSIST_SCRATCH_7

#define HEVC_ASSIST_MMU_MAP_ADDR                   0x3009

#define HEVC_CM_HEADER_START_ADDR                  0x3628
#define HEVC_SAO_MMU_VH1_ADDR                      0x363b
#define HEVC_SAO_MMU_VH0_ADDR                      0x363a
#define HEVC_SAO_MMU_STATUS                        0x3639

#define HEVC_DBLK_CFGB                             0x350b
#define HEVCD_MPP_DECOMP_AXIURG_CTL                0x34c7
#define SWAP_HEVC_OFFSET (3 * 0x1000)

#define MEM_NAME "codec_265"
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/utils/vdec_reg.h>

#include "../utils/vdec.h"
#include "../utils/amvdec.h"
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/codec_mm/configs.h>

#define SEND_LMEM_WITH_RPM
#define SUPPORT_10BIT
/* #define ERROR_HANDLE_DEBUG */

#ifndef STAT_KTHREAD
#define STAT_KTHREAD 0x40
#endif

#ifdef MULTI_INSTANCE_SUPPORT
#define MAX_DECODE_INSTANCE_NUM     9
#define MULTI_DRIVER_NAME "ammvdec_h265"
#endif
#define DRIVER_NAME "amvdec_h265"
#define MODULE_NAME "amvdec_h265"
#define DRIVER_HEADER_NAME "amvdec_h265_header"

#define PUT_INTERVAL        (HZ/100)
#define ERROR_SYSTEM_RESET_COUNT   200

#define PTS_NORMAL                0
#define PTS_NONE_REF_USE_DURATION 1

#define PTS_MODE_SWITCHING_THRESHOLD           3
#define PTS_MODE_SWITCHING_RECOVERY_THREASHOLD 3

#define DUR2PTS(x) ((x)*90/96)

#define MAX_SIZE_8K (8192 * 4608)
#define MAX_SIZE_4K (4096 * 2304)

#define IS_8K_SIZE(w, h)  (((w) * (h)) > MAX_SIZE_4K)
#define IS_4K_SIZE(w, h)  (((w) * (h)) > (1920*1088))

#define SEI_UserDataITU_T_T35	4

static struct semaphore h265_sema;

struct hevc_state_s;
static int hevc_print(struct hevc_state_s *hevc,
	int debug_flag, const char *fmt, ...);
static int hevc_print_cont(struct hevc_state_s *hevc,
	int debug_flag, const char *fmt, ...);
static int vh265_vf_states(struct vframe_states *states, void *);
static struct vframe_s *vh265_vf_peek(void *);
static struct vframe_s *vh265_vf_get(void *);
static void vh265_vf_put(struct vframe_s *, void *);
static int vh265_event_cb(int type, void *data, void *private_data);

static int vh265_stop(struct hevc_state_s *hevc);
#ifdef MULTI_INSTANCE_SUPPORT
static int vmh265_stop(struct hevc_state_s *hevc);
static s32 vh265_init(struct vdec_s *vdec);
static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask);
static void reset_process_time(struct hevc_state_s *hevc);
static void start_process_time(struct hevc_state_s *hevc);
static void restart_process_time(struct hevc_state_s *hevc);
static void timeout_process(struct hevc_state_s *hevc);
#else
static s32 vh265_init(struct hevc_state_s *hevc);
#endif
static void vh265_prot_init(struct hevc_state_s *hevc);
static int vh265_local_init(struct hevc_state_s *hevc);
static void vh265_check_timer_func(unsigned long arg);
static void config_decode_mode(struct hevc_state_s *hevc);

static const char vh265_dec_id[] = "vh265-dev";

#define PROVIDER_NAME   "decoder.h265"
#define MULTI_INSTANCE_PROVIDER_NAME    "vdec.h265"

static const struct vframe_operations_s vh265_vf_provider = {
	.peek = vh265_vf_peek,
	.get = vh265_vf_get,
	.put = vh265_vf_put,
	.event_cb = vh265_event_cb,
	.vf_states = vh265_vf_states,
};

static struct vframe_provider_s vh265_vf_prov;

static u32 bit_depth_luma;
static u32 bit_depth_chroma;
static u32 video_signal_type;

static int start_decode_buf_level = 0x8000;

static unsigned int decode_timeout_val = 200;

/*data_resend_policy:
	bit 0, stream base resend data when decoding buf empty
*/
static u32 data_resend_policy = 1;

#define VIDEO_SIGNAL_TYPE_AVAILABLE_MASK	0x20000000
/*
static const char * const video_format_names[] = {
	"component", "PAL", "NTSC", "SECAM",
	"MAC", "unspecified", "unspecified", "unspecified"
};

static const char * const color_primaries_names[] = {
	"unknown", "bt709", "undef", "unknown",
	"bt470m", "bt470bg", "smpte170m", "smpte240m",
	"film", "bt2020"
};

static const char * const transfer_characteristics_names[] = {
	"unknown", "bt709", "undef", "unknown",
	"bt470m", "bt470bg", "smpte170m", "smpte240m",
	"linear", "log100", "log316", "iec61966-2-4",
	"bt1361e", "iec61966-2-1", "bt2020-10", "bt2020-12",
	"smpte-st-2084", "smpte-st-428"
};

static const char * const matrix_coeffs_names[] = {
	"GBR", "bt709", "undef", "unknown",
	"fcc", "bt470bg", "smpte170m", "smpte240m",
	"YCgCo", "bt2020nc", "bt2020c"
};
*/
#ifdef SUPPORT_10BIT
#define HEVC_CM_BODY_START_ADDR                    0x3626
#define HEVC_CM_BODY_LENGTH                        0x3627
#define HEVC_CM_HEADER_LENGTH                      0x3629
#define HEVC_CM_HEADER_OFFSET                      0x362b
#define HEVC_SAO_CTRL9                             0x362d
#define LOSLESS_COMPRESS_MODE
/* DOUBLE_WRITE_MODE is enabled only when NV21 8 bit output is needed */
/* hevc->double_write_mode:
 *	0, no double write;
 *	1, 1:1 ratio;
 *	2, (1/4):(1/4) ratio;
 *	3, (1/4):(1/4) ratio, with both compressed frame included
 *	0x10, double write only
 */
static u32 double_write_mode;

/*#define DECOMP_HEADR_SURGENT*/

static u32 mem_map_mode; /* 0:linear 1:32x32 2:64x32 ; m8baby test1902 */
static u32 enable_mem_saving = 1;
static u32 workaround_enable;
static u32 force_w_h;
#endif
static u32 force_fps;
static u32 pts_unstable;
#define H265_DEBUG_BUFMGR                   0x01
#define H265_DEBUG_BUFMGR_MORE              0x02
#define H265_DEBUG_DETAIL                   0x04
#define H265_DEBUG_REG                      0x08
#define H265_DEBUG_MAN_SEARCH_NAL           0x10
#define H265_DEBUG_MAN_SKIP_NAL             0x20
#define H265_DEBUG_DISPLAY_CUR_FRAME        0x40
#define H265_DEBUG_FORCE_CLK                0x80
#define H265_DEBUG_SEND_PARAM_WITH_REG      0x100
#define H265_DEBUG_NO_DISPLAY               0x200
#define H265_DEBUG_DISCARD_NAL              0x400
#define H265_DEBUG_OUT_PTS                  0x800
#define H265_DEBUG_DUMP_PIC_LIST            0x1000
#define H265_DEBUG_PRINT_SEI		        0x2000
#define H265_DEBUG_PIC_STRUCT				0x4000
#define H265_DEBUG_HAS_AUX_IN_SLICE			0x8000
#define H265_DEBUG_DIS_LOC_ERROR_PROC       0x10000
#define H265_DEBUG_DIS_SYS_ERROR_PROC       0x20000
#define H265_NO_CHANG_DEBUG_FLAG_IN_CODE    0x40000
#define H265_DEBUG_TRIG_SLICE_SEGMENT_PROC  0x80000
#define H265_DEBUG_HW_RESET                 0x100000
#define H265_CFG_CANVAS_IN_DECODE           0x200000
#define H265_DEBUG_DV                       0x400000
#define H265_DEBUG_NO_EOS_SEARCH_DONE       0x800000
#define H265_DEBUG_NOT_USE_LAST_DISPBUF     0x1000000
#define H265_DEBUG_IGNORE_CONFORMANCE_WINDOW	0x2000000
#define H265_DEBUG_WAIT_DECODE_DONE_WHEN_STOP   0x4000000
#ifdef MULTI_INSTANCE_SUPPORT
#define IGNORE_PARAM_FROM_CONFIG		0x08000000
#define PRINT_FRAMEBASE_DATA            0x10000000
#define PRINT_FLAG_VDEC_STATUS             0x20000000
#define PRINT_FLAG_VDEC_DETAIL             0x40000000
#endif
#define BUF_POOL_SIZE	32
#define MAX_BUF_NUM 24
#define MAX_REF_PIC_NUM 24
#define MAX_REF_ACTIVE  16
#define DEFAULT_BUF_NUM 7

#ifdef MV_USE_FIXED_BUF
#define BMMU_MAX_BUFFERS (BUF_POOL_SIZE + 1)
#define VF_BUFFER_IDX(n)	(n)
#define BMMU_WORKSPACE_ID	(BUF_POOL_SIZE)
#else
#define BMMU_MAX_BUFFERS (BUF_POOL_SIZE + 1 + MAX_REF_PIC_NUM)
#define VF_BUFFER_IDX(n)	(n)
#define BMMU_WORKSPACE_ID	(BUF_POOL_SIZE)
#define MV_BUFFER_IDX(n) (BUF_POOL_SIZE + 1 + n)
#endif

#define HEVC_MV_INFO   0x310d
#define HEVC_QP_INFO   0x3137
#define HEVC_SKIP_INFO 0x3136

const u32 h265_version = 201602101;
static u32 debug_mask = 0xffffffff;
static u32 log_mask;
static u32 debug;
static u32 radr;
static u32 rval;
static u32 dbg_cmd;
static u32 dump_nal;
static u32 dbg_skip_decode_index;
static u32 endian = 0xff0;
#ifdef ERROR_HANDLE_DEBUG
static u32 dbg_nal_skip_flag;
		/* bit[0], skip vps; bit[1], skip sps; bit[2], skip pps */
static u32 dbg_nal_skip_count;
#endif
/*for debug*/
/*
	udebug_flag:
	bit 0, enable ucode print
	bit 1, enable ucode detail print
	bit [31:16] not 0, pos to dump lmem
		bit 2, pop bits to lmem
		bit [11:8], pre-pop bits for alignment (when bit 2 is 1)
*/
static u32 udebug_flag;
/*
	when udebug_flag[1:0] is not 0
	udebug_pause_pos not 0,
		pause position
*/
static u32 udebug_pause_pos;
/*
	when udebug_flag[1:0] is not 0
	and udebug_pause_pos is not 0,
		pause only when DEBUG_REG2 is equal to this val
*/
static u32 udebug_pause_val;

static u32 udebug_pause_decode_idx;

static u32 decode_pic_begin;
static uint slice_parse_begin;
static u32 step;
static bool is_reset;

#ifdef CONSTRAIN_MAX_BUF_NUM
static u32 run_ready_max_vf_only_num;
static u32 run_ready_display_q_num;
	/*0: not check
	  0xff: work_pic_num
	  */
static u32 run_ready_max_buf_num = 0xff;
#endif

static u32 default_buf_num = DEFAULT_BUF_NUM;
static u32 dynamic_buf_num_margin = DEFAULT_BUF_NUM;
static u32 buf_alloc_width;
static u32 buf_alloc_height;

static u32 max_buf_num = 16;
static u32 buf_alloc_size;
/*static u32 re_config_pic_flag;*/
/*
 *bit[0]: 0,
 *bit[1]: 0, always release cma buffer when stop
 *bit[1]: 1, never release cma buffer when stop
 *bit[0]: 1, when stop, release cma buffer if blackout is 1;
 *do not release cma buffer is blackout is not 1
 *
 *bit[2]: 0, when start decoding, check current displayed buffer
 *	 (only for buffer decoded by h265) if blackout is 0
 *	 1, do not check current displayed buffer
 *
 *bit[3]: 1, if blackout is not 1, do not release current
 *			displayed cma buffer always.
 */
/* set to 1 for fast play;
 *	set to 8 for other case of "keep last frame"
 */
static u32 buffer_mode = 1;

/* buffer_mode_dbg: debug only*/
static u32 buffer_mode_dbg = 0xffff0000;
/**/
/*
 *bit[1:0]PB_skip_mode: 0, start decoding at begin;
 *1, start decoding after first I;
 *2, only decode and display none error picture;
 *3, start decoding and display after IDR,etc
 *bit[31:16] PB_skip_count_after_decoding (decoding but not display),
 *only for mode 0 and 1.
 */
static u32 nal_skip_policy = 2;

/*
 *bit 0, 1: only display I picture;
 *bit 1, 1: only decode I picture;
 */
static u32 i_only_flag;

/*
bit 0, fast output first I picture
*/
static u32 fast_output_enable = 1;

static u32 frmbase_cont_bitlevel = 0x60;

/*
use_cma: 1, use both reserver memory and cma for buffers
2, only use cma for buffers
*/
static u32 use_cma = 2;

#define AUX_BUF_ALIGN(adr) ((adr + 0xf) & (~0xf))
static u32 prefix_aux_buf_size = (16 * 1024);
static u32 suffix_aux_buf_size;

static u32 max_decoding_time;
/*
 *error handling
 */
/*error_handle_policy:
 *bit 0: 0, auto skip error_skip_nal_count nals before error recovery;
 *1, skip error_skip_nal_count nals before error recovery;
 *bit 1 (valid only when bit0 == 1):
 *1, wait vps/sps/pps after error recovery;
 *bit 2 (valid only when bit0 == 0):
 *0, auto search after error recovery (hevc_recover() called);
 *1, manual search after error recovery
 *(change to auto search after get IDR: WRITE_VREG(NAL_SEARCH_CTL, 0x2))
 *
 *bit 4: 0, set error_mark after reset/recover
 *	1, do not set error_mark after reset/recover
 *bit 5: 0, check total lcu for every picture
 *	1, do not check total lcu
 *bit 6: 0, do not check head error
 *	1, check head error
 *
 */

static u32 error_handle_policy;
static u32 error_skip_nal_count = 6;
static u32 error_handle_threshold = 30;
static u32 error_handle_nal_skip_threshold = 10;
static u32 error_handle_system_threshold = 30;
static u32 interlace_enable = 1;
static u32 fr_hint_status;

	/*
	 *parser_sei_enable:
	 *  bit 0, sei;
	 *  bit 1, sei_suffix (fill aux buf)
	 *  bit 2, fill sei to aux buf (when bit 0 is 1)
	 *  bit 8, debug flag
	 */
static u32 parser_sei_enable;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static u32 parser_dolby_vision_enable = 1;
static u32 dolby_meta_with_el;
static u32 dolby_el_flush_th = 2;
#endif
/* this is only for h265 mmu enable */

static u32 mmu_enable = 1;
static u32 mmu_enable_force;
static u32 work_buf_size;
static unsigned int force_disp_pic_index;
static unsigned int disp_vframe_valve_level;
static int pre_decode_buf_level = 0x1000;

#ifdef MULTI_INSTANCE_SUPPORT
static unsigned int max_decode_instance_num
				= MAX_DECODE_INSTANCE_NUM;
static unsigned int decode_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int display_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int max_process_time[MAX_DECODE_INSTANCE_NUM];
static unsigned int max_get_frame_interval[MAX_DECODE_INSTANCE_NUM];
static unsigned int run_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int input_empty[MAX_DECODE_INSTANCE_NUM];
static unsigned int not_run_ready[MAX_DECODE_INSTANCE_NUM];
static unsigned int ref_frame_mark_flag[MAX_DECODE_INSTANCE_NUM] =
{1, 1, 1, 1, 1, 1, 1, 1, 1};

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static unsigned char get_idx(struct hevc_state_s *hevc);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static u32 dv_toggle_prov_name;

static u32 dv_debug;

static u32 force_bypass_dvenl;
#endif
#endif


#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
#define get_dbg_flag(hevc) ((debug_mask & (1 << hevc->index)) ? debug : 0)
#define get_dbg_flag2(hevc) ((debug_mask & (1 << get_idx(hevc))) ? debug : 0)
#define is_log_enable(hevc) ((log_mask & (1 << hevc->index)) ? 1 : 0)
#else
#define get_dbg_flag(hevc) debug
#define get_dbg_flag2(hevc) debug
#define is_log_enable(hevc) (log_mask ? 1 : 0)
#define get_valid_double_write_mode(hevc) double_write_mode
#define get_buf_alloc_width(hevc) buf_alloc_width
#define get_buf_alloc_height(hevc) buf_alloc_height
#define get_dynamic_buf_num_margin(hevc) dynamic_buf_num_margin
#endif
#define get_buffer_mode(hevc) buffer_mode


DEFINE_SPINLOCK(lock);
struct task_struct *h265_task = NULL;
#undef DEBUG_REG
#ifdef DEBUG_REG
void WRITE_VREG_DBG(unsigned adr, unsigned val)
{
	if (debug & H265_DEBUG_REG)
		pr_info("%s(%x, %x)\n", __func__, adr, val);
	WRITE_VREG(adr, val);
}

#undef WRITE_VREG
#define WRITE_VREG WRITE_VREG_DBG
#endif

static DEFINE_MUTEX(vh265_mutex);

static DEFINE_MUTEX(vh265_log_mutex);

static struct vdec_info *gvs;

/**************************************************
 *
 *h265 buffer management include
 *
 ***************************************************
 */
enum NalUnitType {
	NAL_UNIT_CODED_SLICE_TRAIL_N = 0,	/* 0 */
	NAL_UNIT_CODED_SLICE_TRAIL_R,	/* 1 */

	NAL_UNIT_CODED_SLICE_TSA_N,	/* 2 */
	/* Current name in the spec: TSA_R */
	NAL_UNIT_CODED_SLICE_TLA,	/* 3 */

	NAL_UNIT_CODED_SLICE_STSA_N,	/* 4 */
	NAL_UNIT_CODED_SLICE_STSA_R,	/* 5 */

	NAL_UNIT_CODED_SLICE_RADL_N,	/* 6 */
	/* Current name in the spec: RADL_R */
	NAL_UNIT_CODED_SLICE_DLP,	/* 7 */

	NAL_UNIT_CODED_SLICE_RASL_N,	/* 8 */
	/* Current name in the spec: RASL_R */
	NAL_UNIT_CODED_SLICE_TFD,	/* 9 */

	NAL_UNIT_RESERVED_10,
	NAL_UNIT_RESERVED_11,
	NAL_UNIT_RESERVED_12,
	NAL_UNIT_RESERVED_13,
	NAL_UNIT_RESERVED_14,
	NAL_UNIT_RESERVED_15,

	/* Current name in the spec: BLA_W_LP */
	NAL_UNIT_CODED_SLICE_BLA,	/* 16 */
	/* Current name in the spec: BLA_W_DLP */
	NAL_UNIT_CODED_SLICE_BLANT,	/* 17 */
	NAL_UNIT_CODED_SLICE_BLA_N_LP,	/* 18 */
	/* Current name in the spec: IDR_W_DLP */
	NAL_UNIT_CODED_SLICE_IDR,	/* 19 */
	NAL_UNIT_CODED_SLICE_IDR_N_LP,	/* 20 */
	NAL_UNIT_CODED_SLICE_CRA,	/* 21 */
	NAL_UNIT_RESERVED_22,
	NAL_UNIT_RESERVED_23,

	NAL_UNIT_RESERVED_24,
	NAL_UNIT_RESERVED_25,
	NAL_UNIT_RESERVED_26,
	NAL_UNIT_RESERVED_27,
	NAL_UNIT_RESERVED_28,
	NAL_UNIT_RESERVED_29,
	NAL_UNIT_RESERVED_30,
	NAL_UNIT_RESERVED_31,

	NAL_UNIT_VPS,		/* 32 */
	NAL_UNIT_SPS,		/* 33 */
	NAL_UNIT_PPS,		/* 34 */
	NAL_UNIT_ACCESS_UNIT_DELIMITER,	/* 35 */
	NAL_UNIT_EOS,		/* 36 */
	NAL_UNIT_EOB,		/* 37 */
	NAL_UNIT_FILLER_DATA,	/* 38 */
	NAL_UNIT_SEI,		/* 39 Prefix SEI */
	NAL_UNIT_SEI_SUFFIX,	/* 40 Suffix SEI */
	NAL_UNIT_RESERVED_41,
	NAL_UNIT_RESERVED_42,
	NAL_UNIT_RESERVED_43,
	NAL_UNIT_RESERVED_44,
	NAL_UNIT_RESERVED_45,
	NAL_UNIT_RESERVED_46,
	NAL_UNIT_RESERVED_47,
	NAL_UNIT_UNSPECIFIED_48,
	NAL_UNIT_UNSPECIFIED_49,
	NAL_UNIT_UNSPECIFIED_50,
	NAL_UNIT_UNSPECIFIED_51,
	NAL_UNIT_UNSPECIFIED_52,
	NAL_UNIT_UNSPECIFIED_53,
	NAL_UNIT_UNSPECIFIED_54,
	NAL_UNIT_UNSPECIFIED_55,
	NAL_UNIT_UNSPECIFIED_56,
	NAL_UNIT_UNSPECIFIED_57,
	NAL_UNIT_UNSPECIFIED_58,
	NAL_UNIT_UNSPECIFIED_59,
	NAL_UNIT_UNSPECIFIED_60,
	NAL_UNIT_UNSPECIFIED_61,
	NAL_UNIT_UNSPECIFIED_62,
	NAL_UNIT_UNSPECIFIED_63,
	NAL_UNIT_INVALID,
};

/* --------------------------------------------------- */
/* Amrisc Software Interrupt */
/* --------------------------------------------------- */
#define AMRISC_STREAM_EMPTY_REQ 0x01
#define AMRISC_PARSER_REQ       0x02
#define AMRISC_MAIN_REQ         0x04

/* --------------------------------------------------- */
/* HEVC_DEC_STATUS define */
/* --------------------------------------------------- */
#define HEVC_DEC_IDLE                        0x0
#define HEVC_NAL_UNIT_VPS                    0x1
#define HEVC_NAL_UNIT_SPS                    0x2
#define HEVC_NAL_UNIT_PPS                    0x3
#define HEVC_NAL_UNIT_CODED_SLICE_SEGMENT    0x4
#define HEVC_CODED_SLICE_SEGMENT_DAT         0x5
#define HEVC_SLICE_DECODING                  0x6
#define HEVC_NAL_UNIT_SEI                    0x7
#define HEVC_SLICE_SEGMENT_DONE              0x8
#define HEVC_NAL_SEARCH_DONE                 0x9
#define HEVC_DECPIC_DATA_DONE                0xa
#define HEVC_DECPIC_DATA_ERROR               0xb
#define HEVC_SEI_DAT                         0xc
#define HEVC_SEI_DAT_DONE                    0xd
#define HEVC_NAL_DECODE_DONE				0xe
#define HEVC_OVER_DECODE					0xf

#define HEVC_DATA_REQUEST           0x12

#define HEVC_DECODE_BUFEMPTY        0x20
#define HEVC_DECODE_TIMEOUT         0x21
#define HEVC_SEARCH_BUFEMPTY        0x22
#define HEVC_DECODE_OVER_SIZE       0x23
#define HEVC_DECODE_BUFEMPTY2       0x24
#define HEVC_FIND_NEXT_PIC_NAL				0x50
#define HEVC_FIND_NEXT_DVEL_NAL				0x51

#define HEVC_DUMP_LMEM				0x30

#define HEVC_4k2k_60HZ_NOT_SUPPORT	0x80
#define HEVC_DISCARD_NAL         0xf0
#define HEVC_ACTION_DEC_CONT     0xfd
#define HEVC_ACTION_ERROR        0xfe
#define HEVC_ACTION_DONE         0xff

/* --------------------------------------------------- */
/* Include "parser_cmd.h" */
/* --------------------------------------------------- */
#define PARSER_CMD_SKIP_CFG_0 0x0000090b

#define PARSER_CMD_SKIP_CFG_1 0x1b14140f

#define PARSER_CMD_SKIP_CFG_2 0x001b1910

#define PARSER_CMD_NUMBER 37

/**************************************************
 *
 *h265 buffer management
 *
 ***************************************************
 */
/* #define BUFFER_MGR_ONLY */
/* #define CONFIG_HEVC_CLK_FORCED_ON */
/* #define ENABLE_SWAP_TEST */
#define   MCRCC_ENABLE
#define INVALID_POC 0x80000000

#define HEVC_DEC_STATUS_REG       HEVC_ASSIST_SCRATCH_0
#define HEVC_RPM_BUFFER           HEVC_ASSIST_SCRATCH_1
#define HEVC_SHORT_TERM_RPS       HEVC_ASSIST_SCRATCH_2
#define HEVC_VPS_BUFFER           HEVC_ASSIST_SCRATCH_3
#define HEVC_SPS_BUFFER           HEVC_ASSIST_SCRATCH_4
#define HEVC_PPS_BUFFER           HEVC_ASSIST_SCRATCH_5
#define HEVC_SAO_UP               HEVC_ASSIST_SCRATCH_6
#define HEVC_STREAM_SWAP_BUFFER   HEVC_ASSIST_SCRATCH_7
#define HEVC_STREAM_SWAP_BUFFER2  HEVC_ASSIST_SCRATCH_8
#define HEVC_sao_mem_unit         HEVC_ASSIST_SCRATCH_9
#define HEVC_SAO_ABV              HEVC_ASSIST_SCRATCH_A
#define HEVC_sao_vb_size          HEVC_ASSIST_SCRATCH_B
#define HEVC_SAO_VB               HEVC_ASSIST_SCRATCH_C
#define HEVC_SCALELUT             HEVC_ASSIST_SCRATCH_D
#define HEVC_WAIT_FLAG            HEVC_ASSIST_SCRATCH_E
#define RPM_CMD_REG               HEVC_ASSIST_SCRATCH_F
#define LMEM_DUMP_ADR                 HEVC_ASSIST_SCRATCH_F
#ifdef ENABLE_SWAP_TEST
#define HEVC_STREAM_SWAP_TEST     HEVC_ASSIST_SCRATCH_L
#endif

/*#define HEVC_DECODE_PIC_BEGIN_REG HEVC_ASSIST_SCRATCH_M*/
/*#define HEVC_DECODE_PIC_NUM_REG   HEVC_ASSIST_SCRATCH_N*/
#define HEVC_DECODE_SIZE		HEVC_ASSIST_SCRATCH_N
	/*do not define ENABLE_SWAP_TEST*/
#define HEVC_AUX_ADR			HEVC_ASSIST_SCRATCH_L
#define HEVC_AUX_DATA_SIZE		HEVC_ASSIST_SCRATCH_M

#define DEBUG_REG1              HEVC_ASSIST_SCRATCH_G
#define DEBUG_REG2              HEVC_ASSIST_SCRATCH_H
/*
 *ucode parser/search control
 *bit 0:  0, header auto parse; 1, header manual parse
 *bit 1:  0, auto skip for noneseamless stream; 1, no skip
 *bit [3:2]: valid when bit1==0;
 *0, auto skip nal before first vps/sps/pps/idr;
 *1, auto skip nal before first vps/sps/pps
 *2, auto skip nal before first  vps/sps/pps,
 *	and not decode until the first I slice (with slice address of 0)
 *
 *3, auto skip before first I slice (nal_type >=16 && nal_type<=21)
 *bit [15:4] nal skip count (valid when bit0 == 1 (manual mode) )
 *bit [16]: for NAL_UNIT_EOS when bit0 is 0:
 *	0, send SEARCH_DONE to arm ;  1, do not send SEARCH_DONE to arm
 *bit [17]: for NAL_SEI when bit0 is 0:
 *	0, do not parse/fetch SEI in ucode;
 *	1, parse/fetch SEI in ucode
 *bit [18]: for NAL_SEI_SUFFIX when bit0 is 0:
 *	0, do not fetch NAL_SEI_SUFFIX to aux buf;
 *	1, fetch NAL_SEL_SUFFIX data to aux buf
 *bit [19]:
 *	0, parse NAL_SEI in ucode
 *	1, fetch NAL_SEI to aux buf
 *bit [20]: for DOLBY_VISION_META
 *	0, do not fetch DOLBY_VISION_META to aux buf
 *	1, fetch DOLBY_VISION_META to aux buf
 */
#define NAL_SEARCH_CTL            HEVC_ASSIST_SCRATCH_I
	/*read only*/
#define CUR_NAL_UNIT_TYPE       HEVC_ASSIST_SCRATCH_J
	/*
	[15 : 8] rps_set_id
	[7 : 0] start_decoding_flag
	*/
#define HEVC_DECODE_INFO       HEVC_ASSIST_SCRATCH_1
	/*set before start decoder*/
#define HEVC_DECODE_MODE		HEVC_ASSIST_SCRATCH_J
#define HEVC_DECODE_MODE2		HEVC_ASSIST_SCRATCH_H
#define DECODE_STOP_POS         HEVC_ASSIST_SCRATCH_K

#define DECODE_MODE_SINGLE					0x0
#define DECODE_MODE_MULTI_FRAMEBASE			0x1
#define DECODE_MODE_MULTI_STREAMBASE		0x2
#define DECODE_MODE_MULTI_DVBAL				0x3
#define DECODE_MODE_MULTI_DVENL				0x4

#define MAX_INT 0x7FFFFFFF

#define RPM_BEGIN                                              0x100
#define modification_list_cur                                  0x148
#define RPM_END                                                0x180

#define RPS_USED_BIT        14
/* MISC_FLAG0 */
#define PCM_LOOP_FILTER_DISABLED_FLAG_BIT       0
#define PCM_ENABLE_FLAG_BIT             1
#define LOOP_FILER_ACROSS_TILES_ENABLED_FLAG_BIT    2
#define PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT  3
#define DEBLOCKING_FILTER_OVERRIDE_ENABLED_FLAG_BIT 4
#define PPS_DEBLOCKING_FILTER_DISABLED_FLAG_BIT     5
#define DEBLOCKING_FILTER_OVERRIDE_FLAG_BIT     6
#define SLICE_DEBLOCKING_FILTER_DISABLED_FLAG_BIT   7
#define SLICE_SAO_LUMA_FLAG_BIT             8
#define SLICE_SAO_CHROMA_FLAG_BIT           9
#define SLICE_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT 10

union param_u {
	struct {
		unsigned short data[RPM_END - RPM_BEGIN];
	} l;
	struct {
		/* from ucode lmem, do not change this struct */
		unsigned short CUR_RPS[0x10];
		unsigned short num_ref_idx_l0_active;
		unsigned short num_ref_idx_l1_active;
		unsigned short slice_type;
		unsigned short slice_temporal_mvp_enable_flag;
		unsigned short dependent_slice_segment_flag;
		unsigned short slice_segment_address;
		unsigned short num_title_rows_minus1;
		unsigned short pic_width_in_luma_samples;
		unsigned short pic_height_in_luma_samples;
		unsigned short log2_min_coding_block_size_minus3;
		unsigned short log2_diff_max_min_coding_block_size;
		unsigned short log2_max_pic_order_cnt_lsb_minus4;
		unsigned short POClsb;
		unsigned short collocated_from_l0_flag;
		unsigned short collocated_ref_idx;
		unsigned short log2_parallel_merge_level;
		unsigned short five_minus_max_num_merge_cand;
		unsigned short sps_num_reorder_pics_0;
		unsigned short modification_flag;
		unsigned short tiles_enabled_flag;
		unsigned short num_tile_columns_minus1;
		unsigned short num_tile_rows_minus1;
		unsigned short tile_width[8];
		unsigned short tile_height[8];
		unsigned short misc_flag0;
		unsigned short pps_beta_offset_div2;
		unsigned short pps_tc_offset_div2;
		unsigned short slice_beta_offset_div2;
		unsigned short slice_tc_offset_div2;
		unsigned short pps_cb_qp_offset;
		unsigned short pps_cr_qp_offset;
		unsigned short first_slice_segment_in_pic_flag;
		unsigned short m_temporalId;
		unsigned short m_nalUnitType;

		unsigned short vui_num_units_in_tick_hi;
		unsigned short vui_num_units_in_tick_lo;
		unsigned short vui_time_scale_hi;
		unsigned short vui_time_scale_lo;
		unsigned short bit_depth;
		unsigned short profile_etc;
		unsigned short sei_frame_field_info;
		unsigned short video_signal_type;
		unsigned short modification_list[0x20];
		unsigned short conformance_window_flag;
		unsigned short conf_win_left_offset;
		unsigned short conf_win_right_offset;
		unsigned short conf_win_top_offset;
		unsigned short conf_win_bottom_offset;
		unsigned short chroma_format_idc;
		unsigned short color_description;
		unsigned short aspect_ratio_idc;
		unsigned short sar_width;
		unsigned short sar_height;
		unsigned short sps_max_dec_pic_buffering_minus1_0;
	} p;
};

#define RPM_BUF_SIZE (0x80*2)
/* non mmu mode lmem size : 0x400, mmu mode : 0x500*/
#define LMEM_BUF_SIZE (0x500 * 2)

struct buff_s {
	u32 buf_start;
	u32 buf_size;
	u32 buf_end;
};

struct BuffInfo_s {
	u32 max_width;
	u32 max_height;
	unsigned int start_adr;
	unsigned int end_adr;
	struct buff_s ipp;
	struct buff_s sao_abv;
	struct buff_s sao_vb;
	struct buff_s short_term_rps;
	struct buff_s vps;
	struct buff_s sps;
	struct buff_s pps;
	struct buff_s sao_up;
	struct buff_s swap_buf;
	struct buff_s swap_buf2;
	struct buff_s scalelut;
	struct buff_s dblk_para;
	struct buff_s dblk_data;
	struct buff_s dblk_data2;
	struct buff_s mmu_vbh;
	struct buff_s cm_header;
	struct buff_s mpred_above;
#ifdef MV_USE_FIXED_BUF
	struct buff_s mpred_mv;
#endif
	struct buff_s rpm;
	struct buff_s lmem;
};
#define WORK_BUF_SPEC_NUM 3
static struct BuffInfo_s amvh265_workbuff_spec[WORK_BUF_SPEC_NUM] = {
	{
		/* 8M bytes */
		.max_width = 1920,
		.max_height = 1088,
		.ipp = {
			/* IPP work space calculation :
			 *   4096 * (Y+CbCr+Flags) = 12k, round to 16k
			 */
			.buf_size = 0x4000,
		},
		.sao_abv = {
			.buf_size = 0x30000,
		},
		.sao_vb = {
			.buf_size = 0x30000,
		},
		.short_term_rps = {
			/* SHORT_TERM_RPS - Max 64 set, 16 entry every set,
			 *   total 64x16x2 = 2048 bytes (0x800)
			 */
			.buf_size = 0x800,
		},
		.vps = {
			/* VPS STORE AREA - Max 16 VPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.sps = {
			/* SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.pps = {
			/* PPS STORE AREA - Max 64 PPS, each has 0x80 bytes,
			 *   total 0x2000 bytes
			 */
			.buf_size = 0x2000,
		},
		.sao_up = {
			/* SAO UP STORE AREA - Max 640(10240/16) LCU,
			 *   each has 16 bytes total 0x2800 bytes
			 */
			.buf_size = 0x2800,
		},
		.swap_buf = {
			/* 256cyclex64bit = 2K bytes 0x800
			 *   (only 144 cycles valid)
			 */
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
			/* support up to 32 SCALELUT 1024x32 =
			 *   32Kbytes (0x8000)
			 */
			.buf_size = 0x8000,
		},
		.dblk_para = {
#ifdef SUPPORT_10BIT
			.buf_size = 0x40000,
#else
			/* DBLK -> Max 256(4096/16) LCU, each para
			 *512bytes(total:0x20000), data 1024bytes(total:0x40000)
			 */
			.buf_size = 0x20000,
#endif
		},
		.dblk_data = {
			.buf_size = 0x40000,
		},
		.dblk_data2 = {
			.buf_size = 0x40000,
		}, /*dblk data for adapter*/
		.mmu_vbh = {
			.buf_size = 0x5000, /*2*16*2304/4, 4K*/
		},
#if 0
		.cm_header = {/* 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)*/
			.buf_size = MMU_COMPRESS_HEADER_SIZE *
			(MAX_REF_PIC_NUM + 1),
		},
#endif
		.mpred_above = {
			.buf_size = 0x8000,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {/* 1080p, 0x40000 per buffer */
			.buf_size = 0x40000 * MAX_REF_PIC_NUM,
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		}
	},
	{
		.max_width = 4096,
		.max_height = 2048,
		.ipp = {
			/* IPP work space calculation :
			 *   4096 * (Y+CbCr+Flags) = 12k, round to 16k
			 */
			.buf_size = 0x4000,
		},
		.sao_abv = {
			.buf_size = 0x30000,
		},
		.sao_vb = {
			.buf_size = 0x30000,
		},
		.short_term_rps = {
			/* SHORT_TERM_RPS - Max 64 set, 16 entry every set,
			 *   total 64x16x2 = 2048 bytes (0x800)
			 */
			.buf_size = 0x800,
		},
		.vps = {
			/* VPS STORE AREA - Max 16 VPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.sps = {
			/* SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.pps = {
			/* PPS STORE AREA - Max 64 PPS, each has 0x80 bytes,
			 *   total 0x2000 bytes
			 */
			.buf_size = 0x2000,
		},
		.sao_up = {
			/* SAO UP STORE AREA - Max 640(10240/16) LCU,
			 *   each has 16 bytes total 0x2800 bytes
			 */
			.buf_size = 0x2800,
		},
		.swap_buf = {
			/* 256cyclex64bit = 2K bytes 0x800
			 *   (only 144 cycles valid)
			 */
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
			/* support up to 32 SCALELUT 1024x32 = 32Kbytes
			 *   (0x8000)
			 */
			.buf_size = 0x8000,
		},
		.dblk_para = {
			/* DBLK -> Max 256(4096/16) LCU, each para
			 *   512bytes(total:0x20000),
			 *   data 1024bytes(total:0x40000)
			 */
			.buf_size = 0x20000,
		},
		.dblk_data = {
			.buf_size = 0x80000,
		},
		.dblk_data2 = {
			.buf_size = 0x80000,
		}, /*dblk data for adapter*/
		.mmu_vbh = {
			.buf_size = 0x5000, /*2*16*2304/4, 4K*/
		},
#if 0
		.cm_header = {/*0x44000 = ((1088*2*1024*4)/32/4)*(32/8)*/
			.buf_size = MMU_COMPRESS_HEADER_SIZE *
			(MAX_REF_PIC_NUM + 1),
		},
#endif
		.mpred_above = {
			.buf_size = 0x8000,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
			/* .buf_size = 0x100000*16,
			//4k2k , 0x100000 per buffer */
			/* 4096x2304 , 0x120000 per buffer */
			.buf_size = MPRED_4K_MV_BUF_SIZE * MAX_REF_PIC_NUM,
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		}
	},

	{
		.max_width = 4096*2,
		.max_height = 2048*2,
		.ipp = {
			// IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k, round to 16k
			.buf_size = 0x4000*2,
		},
		.sao_abv = {
			.buf_size = 0x30000*2,
		},
		.sao_vb = {
			.buf_size = 0x30000*2,
		},
		.short_term_rps = {
			// SHORT_TERM_RPS - Max 64 set, 16 entry every set, total 64x16x2 = 2048 bytes (0x800)
			.buf_size = 0x800,
		},
		.vps = {
			// VPS STORE AREA - Max 16 VPS, each has 0x80 bytes, total 0x0800 bytes
			.buf_size = 0x800,
		},
		.sps = {
			// SPS STORE AREA - Max 16 SPS, each has 0x80 bytes, total 0x0800 bytes
			.buf_size = 0x800,
		},
		.pps = {
			// PPS STORE AREA - Max 64 PPS, each has 0x80 bytes, total 0x2000 bytes
			.buf_size = 0x2000,
		},
		.sao_up = {
			// SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes total 0x2800 bytes
			.buf_size = 0x2800*2,
		},
		.swap_buf = {
			// 256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
			// support up to 32 SCALELUT 1024x32 = 32Kbytes (0x8000)
			.buf_size = 0x8000*2,
		},
		.dblk_para  = {.buf_size = 0x40000*2, }, // dblk parameter
		.dblk_data  = {.buf_size = 0x80000*2, }, // dblk data for left/top
		.dblk_data2 = {.buf_size = 0x80000*2, }, // dblk data for adapter
		.mmu_vbh = {
			.buf_size = 0x5000*2, //2*16*2304/4, 4K
		},
#if 0
		.cm_header = {
			.buf_size = MMU_COMPRESS_8K_HEADER_SIZE *
				MAX_REF_PIC_NUM, 	// 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
		},
#endif
		.mpred_above = {
			.buf_size = 0x8000*2,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
			.buf_size = MPRED_8K_MV_BUF_SIZE * MAX_REF_PIC_NUM, //4k2k , 0x120000 per buffer
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		},
	}
};

static void init_buff_spec(struct hevc_state_s *hevc,
	struct BuffInfo_s *buf_spec)
{
	buf_spec->ipp.buf_start = buf_spec->start_adr;
	buf_spec->sao_abv.buf_start =
		buf_spec->ipp.buf_start + buf_spec->ipp.buf_size;

	buf_spec->sao_vb.buf_start =
		buf_spec->sao_abv.buf_start + buf_spec->sao_abv.buf_size;
	buf_spec->short_term_rps.buf_start =
		buf_spec->sao_vb.buf_start + buf_spec->sao_vb.buf_size;
	buf_spec->vps.buf_start =
		buf_spec->short_term_rps.buf_start +
		buf_spec->short_term_rps.buf_size;
	buf_spec->sps.buf_start =
		buf_spec->vps.buf_start + buf_spec->vps.buf_size;
	buf_spec->pps.buf_start =
		buf_spec->sps.buf_start + buf_spec->sps.buf_size;
	buf_spec->sao_up.buf_start =
		buf_spec->pps.buf_start + buf_spec->pps.buf_size;
	buf_spec->swap_buf.buf_start =
		buf_spec->sao_up.buf_start + buf_spec->sao_up.buf_size;
	buf_spec->swap_buf2.buf_start =
		buf_spec->swap_buf.buf_start + buf_spec->swap_buf.buf_size;
	buf_spec->scalelut.buf_start =
		buf_spec->swap_buf2.buf_start + buf_spec->swap_buf2.buf_size;
	buf_spec->dblk_para.buf_start =
		buf_spec->scalelut.buf_start + buf_spec->scalelut.buf_size;
	buf_spec->dblk_data.buf_start =
		buf_spec->dblk_para.buf_start + buf_spec->dblk_para.buf_size;
	buf_spec->dblk_data2.buf_start =
		buf_spec->dblk_data.buf_start + buf_spec->dblk_data.buf_size;
	buf_spec->mmu_vbh.buf_start  =
		buf_spec->dblk_data2.buf_start + buf_spec->dblk_data2.buf_size;
	buf_spec->mpred_above.buf_start =
		buf_spec->mmu_vbh.buf_start + buf_spec->mmu_vbh.buf_size;
#ifdef MV_USE_FIXED_BUF
	buf_spec->mpred_mv.buf_start =
		buf_spec->mpred_above.buf_start +
		buf_spec->mpred_above.buf_size;

	buf_spec->rpm.buf_start =
		buf_spec->mpred_mv.buf_start +
		buf_spec->mpred_mv.buf_size;
#else
	buf_spec->rpm.buf_start =
		buf_spec->mpred_above.buf_start +
		buf_spec->mpred_above.buf_size;
#endif
	buf_spec->lmem.buf_start =
		buf_spec->rpm.buf_start +
		buf_spec->rpm.buf_size;
	buf_spec->end_adr =
		buf_spec->lmem.buf_start +
		buf_spec->lmem.buf_size;

	if (hevc && get_dbg_flag2(hevc)) {
		hevc_print(hevc, 0,
				"%s workspace (%x %x) size = %x\n", __func__,
			   buf_spec->start_adr, buf_spec->end_adr,
			   buf_spec->end_adr - buf_spec->start_adr);

		hevc_print(hevc, 0,
			"ipp.buf_start             :%x\n",
			buf_spec->ipp.buf_start);
		hevc_print(hevc, 0,
			"sao_abv.buf_start          :%x\n",
			buf_spec->sao_abv.buf_start);
		hevc_print(hevc, 0,
			"sao_vb.buf_start          :%x\n",
			buf_spec->sao_vb.buf_start);
		hevc_print(hevc, 0,
			"short_term_rps.buf_start  :%x\n",
			buf_spec->short_term_rps.buf_start);
		hevc_print(hevc, 0,
			"vps.buf_start             :%x\n",
			buf_spec->vps.buf_start);
		hevc_print(hevc, 0,
			"sps.buf_start             :%x\n",
			buf_spec->sps.buf_start);
		hevc_print(hevc, 0,
			"pps.buf_start             :%x\n",
			buf_spec->pps.buf_start);
		hevc_print(hevc, 0,
			"sao_up.buf_start          :%x\n",
			buf_spec->sao_up.buf_start);
		hevc_print(hevc, 0,
			"swap_buf.buf_start        :%x\n",
			buf_spec->swap_buf.buf_start);
		hevc_print(hevc, 0,
			"swap_buf2.buf_start       :%x\n",
			buf_spec->swap_buf2.buf_start);
		hevc_print(hevc, 0,
			"scalelut.buf_start        :%x\n",
			buf_spec->scalelut.buf_start);
		hevc_print(hevc, 0,
			"dblk_para.buf_start       :%x\n",
			buf_spec->dblk_para.buf_start);
		hevc_print(hevc, 0,
			"dblk_data.buf_start       :%x\n",
			buf_spec->dblk_data.buf_start);
		hevc_print(hevc, 0,
			"dblk_data2.buf_start       :%x\n",
			buf_spec->dblk_data2.buf_start);
		hevc_print(hevc, 0,
			"mpred_above.buf_start     :%x\n",
			buf_spec->mpred_above.buf_start);
#ifdef MV_USE_FIXED_BUF
		hevc_print(hevc, 0,
			"mpred_mv.buf_start        :%x\n",
			  buf_spec->mpred_mv.buf_start);
#endif
		if ((get_dbg_flag2(hevc)
			&
			H265_DEBUG_SEND_PARAM_WITH_REG)
			== 0) {
			hevc_print(hevc, 0,
				"rpm.buf_start             :%x\n",
				   buf_spec->rpm.buf_start);
		}
	}

}

enum SliceType {
	B_SLICE,
	P_SLICE,
	I_SLICE
};

/*USE_BUF_BLOCK*/
struct BUF_s {
	unsigned long start_adr;
	unsigned int size;
	int used_flag;
} /*BUF_t */;

/* level 6, 6.1 maximum slice number is 800; other is 200 */
#define MAX_SLICE_NUM 800
struct PIC_s {
	int index;
	int scatter_alloc;
	int BUF_index;
	int mv_buf_index;
	int POC;
	int decode_idx;
	int slice_type;
	int RefNum_L0;
	int RefNum_L1;
	int num_reorder_pic;
	int stream_offset;
	unsigned char referenced;
	unsigned char output_mark;
	unsigned char recon_mark;
	unsigned char output_ready;
	unsigned char error_mark;
	/**/ int slice_idx;
	int m_aiRefPOCList0[MAX_SLICE_NUM][16];
	int m_aiRefPOCList1[MAX_SLICE_NUM][16];
	/*buffer */
	unsigned int header_adr;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	unsigned char dv_enhance_exist;
#endif
	char *aux_data_buf;
	int aux_data_size;
	unsigned long cma_alloc_addr;
	struct page *alloc_pages;
	unsigned int mpred_mv_wr_start_addr;
	unsigned int mc_y_adr;
	unsigned int mc_u_v_adr;
#ifdef SUPPORT_10BIT
	/*unsigned int comp_body_size;*/
	unsigned int dw_y_adr;
	unsigned int dw_u_v_adr;
#endif
	int mc_canvas_y;
	int mc_canvas_u_v;
	int width;
	int height;

	int y_canvas_index;
	int uv_canvas_index;
#ifdef MULTI_INSTANCE_SUPPORT
	struct canvas_config_s canvas_config[2];
#endif
#ifdef SUPPORT_10BIT
	int mem_saving_mode;
	u32 bit_depth_luma;
	u32 bit_depth_chroma;
#endif
#ifdef LOSLESS_COMPRESS_MODE
	unsigned int losless_comp_body_size;
#endif
	unsigned char pic_struct;
	int vf_ref;

	u32 pts;
	u64 pts64;

	u32 aspect_ratio_idc;
	u32 sar_width;
	u32 sar_height;
	u32 double_write_mode;
	u32 video_signal_type;
	unsigned short conformance_window_flag;
	unsigned short conf_win_left_offset;
	unsigned short conf_win_right_offset;
	unsigned short conf_win_top_offset;
	unsigned short conf_win_bottom_offset;
	unsigned short chroma_format_idc;

	/* picture qos infomation*/
	int max_qp;
	int avg_qp;
	int min_qp;
	int max_skip;
	int avg_skip;
	int min_skip;
	int max_mv;
	int min_mv;
	int avg_mv;
} /*PIC_t */;

#define MAX_TILE_COL_NUM    10
#define MAX_TILE_ROW_NUM   20
struct tile_s {
	int width;
	int height;
	int start_cu_x;
	int start_cu_y;

	unsigned int sao_vb_start_addr;
	unsigned int sao_abv_start_addr;
};

#define SEI_MASTER_DISPLAY_COLOR_MASK 0x00000001
#define SEI_CONTENT_LIGHT_LEVEL_MASK  0x00000002
#define SEI_HDR10PLUS_MASK			  0x00000004

#define VF_POOL_SIZE        32

#ifdef MULTI_INSTANCE_SUPPORT
#define DEC_RESULT_NONE             0
#define DEC_RESULT_DONE             1
#define DEC_RESULT_AGAIN            2
#define DEC_RESULT_CONFIG_PARAM     3
#define DEC_RESULT_ERROR            4
#define DEC_INIT_PICLIST			5
#define DEC_UNINIT_PICLIST			6
#define DEC_RESULT_GET_DATA         7
#define DEC_RESULT_GET_DATA_RETRY   8
#define DEC_RESULT_EOS              9
#define DEC_RESULT_FORCE_EXIT       10

static void vh265_work(struct work_struct *work);
static void vh265_notify_work(struct work_struct *work);

#endif

struct debug_log_s {
	struct list_head list;
	uint8_t data; /*will alloc more size*/
};

struct hevc_state_s {
#ifdef MULTI_INSTANCE_SUPPORT
	struct platform_device *platform_dev;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;
	struct vframe_chunk_s *chunk;
	int dec_result;
	struct work_struct work;
	struct work_struct notify_work;
	struct work_struct set_clk_work;
	/* timeout handle */
	unsigned long int start_process_time;
	unsigned int last_lcu_idx;
	unsigned int decode_timeout_count;
	unsigned int timeout_num;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	unsigned char switch_dvlayer_flag;
	unsigned char no_switch_dvlayer_count;
	unsigned char bypass_dvenl_enable;
	unsigned char bypass_dvenl;
#endif
	unsigned char start_parser_type;
	/*start_decoding_flag:
	vps/pps/sps/idr info from ucode*/
	unsigned char start_decoding_flag;
	unsigned char rps_set_id;
	unsigned char eos;
	int pic_decoded_lcu_idx;
	u8 over_decode;
	u8 empty_flag;
#endif
	struct vframe_s vframe_dummy;
	char *provider_name;
	int index;
	struct device *cma_dev;
	unsigned char m_ins_flag;
	unsigned char dolby_enhance_flag;
	unsigned long buf_start;
	u32 buf_size;
	u32 mv_buf_size;

	struct BuffInfo_s work_space_buf_store;
	struct BuffInfo_s *work_space_buf;

	u8 aux_data_dirty;
	u32 prefix_aux_size;
	u32 suffix_aux_size;
	void *aux_addr;
	void *rpm_addr;
	void *lmem_addr;
	dma_addr_t aux_phy_addr;
	dma_addr_t rpm_phy_addr;
	dma_addr_t lmem_phy_addr;

	unsigned int pic_list_init_flag;
	unsigned int use_cma_flag;

	unsigned short *rpm_ptr;
	unsigned short *lmem_ptr;
	unsigned short *debug_ptr;
	int debug_ptr_size;
	int pic_w;
	int pic_h;
	int lcu_x_num;
	int lcu_y_num;
	int lcu_total;
	int lcu_size;
	int lcu_size_log2;
	int lcu_x_num_pre;
	int lcu_y_num_pre;
	int first_pic_after_recover;

	int num_tile_col;
	int num_tile_row;
	int tile_enabled;
	int tile_x;
	int tile_y;
	int tile_y_x;
	int tile_start_lcu_x;
	int tile_start_lcu_y;
	int tile_width_lcu;
	int tile_height_lcu;

	int slice_type;
	unsigned int slice_addr;
	unsigned int slice_segment_addr;

	unsigned char interlace_flag;
	unsigned char curr_pic_struct;

	unsigned short sps_num_reorder_pics_0;
	unsigned short misc_flag0;
	int m_temporalId;
	int m_nalUnitType;
	int TMVPFlag;
	int isNextSliceSegment;
	int LDCFlag;
	int m_pocRandomAccess;
	int plevel;
	int MaxNumMergeCand;

	int new_pic;
	int new_tile;
	int curr_POC;
	int iPrevPOC;
#ifdef MULTI_INSTANCE_SUPPORT
	int decoded_poc;
	struct PIC_s *decoding_pic;
#endif
	int iPrevTid0POC;
	int list_no;
	int RefNum_L0;
	int RefNum_L1;
	int ColFromL0Flag;
	int LongTerm_Curr;
	int LongTerm_Col;
	int Col_POC;
	int LongTerm_Ref;
#ifdef MULTI_INSTANCE_SUPPORT
	int m_pocRandomAccess_bak;
	int curr_POC_bak;
	int iPrevPOC_bak;
	int iPrevTid0POC_bak;
	unsigned char start_parser_type_bak;
	unsigned char start_decoding_flag_bak;
	unsigned char rps_set_id_bak;
	int pic_decoded_lcu_idx_bak;
	int decode_idx_bak;
#endif
	struct PIC_s *cur_pic;
	struct PIC_s *col_pic;
	int skip_flag;
	int decode_idx;
	int slice_idx;
	unsigned char have_vps;
	unsigned char have_sps;
	unsigned char have_pps;
	unsigned char have_valid_start_slice;
	unsigned char wait_buf;
	unsigned char error_flag;
	unsigned int error_skip_nal_count;
	long used_4k_num;

	unsigned char
	ignore_bufmgr_error;	/* bit 0, for decoding;
			bit 1, for displaying
			bit 1 must be set if bit 0 is 1*/
	int PB_skip_mode;
	int PB_skip_count_after_decoding;
#ifdef SUPPORT_10BIT
	int mem_saving_mode;
#endif
#ifdef LOSLESS_COMPRESS_MODE
	unsigned int losless_comp_body_size;
#endif
	int pts_mode;
	int last_lookup_pts;
	int last_pts;
	u64 last_lookup_pts_us64;
	u64 last_pts_us64;
	u32 shift_byte_count_lo;
	u32 shift_byte_count_hi;
	int pts_mode_switching_count;
	int pts_mode_recovery_count;

	int pic_num;

	/**/
	union param_u param;

	struct tile_s m_tile[MAX_TILE_ROW_NUM][MAX_TILE_COL_NUM];

	struct timer_list timer;
	struct BUF_s m_BUF[BUF_POOL_SIZE];
	struct BUF_s m_mv_BUF[MAX_REF_PIC_NUM];
	struct PIC_s *m_PIC[MAX_REF_PIC_NUM];

	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(pending_q, struct vframe_s *, VF_POOL_SIZE);
	struct vframe_s vfpool[VF_POOL_SIZE];

	u32 stat;
	u32 frame_width;
	u32 frame_height;
	u32 frame_dur;
	u32 frame_ar;
	u32 bit_depth_luma;
	u32 bit_depth_chroma;
	u32 video_signal_type;
	u32 video_signal_type_debug;
	u32 saved_resolution;
	bool get_frame_dur;
	u32 error_watchdog_count;
	u32 error_skip_nal_wt_cnt;
	u32 error_system_watchdog_count;

#ifdef DEBUG_PTS
	unsigned long pts_missed;
	unsigned long pts_hit;
#endif
	struct dec_sysinfo vh265_amstream_dec_info;
	unsigned char init_flag;
	unsigned char first_sc_checked;
	unsigned char uninit_list;
	u32 start_decoding_time;

	int show_frame_num;
#ifdef USE_UNINIT_SEMA
	struct semaphore h265_uninit_done_sema;
#endif
	int fatal_error;


	u32 sei_present_flag;
	void *frame_mmu_map_addr;
	dma_addr_t frame_mmu_map_phy_addr;
	unsigned int mmu_mc_buf_start;
	unsigned int mmu_mc_buf_end;
	unsigned int mmu_mc_start_4k_adr;
	void *mmu_box;
	void *bmmu_box;
	int mmu_enable;

	unsigned int dec_status;

	/* data for SEI_MASTER_DISPLAY_COLOR */
	unsigned int primaries[3][2];
	unsigned int white_point[2];
	unsigned int luminance[2];
	/* data for SEI_CONTENT_LIGHT_LEVEL */
	unsigned int content_light_level[2];

	struct PIC_s *pre_top_pic;
	struct PIC_s *pre_bot_pic;

#ifdef MULTI_INSTANCE_SUPPORT
	int double_write_mode;
	int dynamic_buf_num_margin;
	int start_action;
	int save_buffer_mode;
#endif
	u32 i_only;
	struct list_head log_list;
	u32 ucode_pause_pos;
	u32 start_shift_bytes;

	u32 vf_pre_count;
	u32 vf_get_count;
	u32 vf_put_count;
#ifdef SWAP_HEVC_UCODE
	dma_addr_t mc_dma_handle;
	void *mc_cpu_addr;
	int swap_size;
	ulong swap_addr;
#endif
#ifdef DETREFILL_ENABLE
	dma_addr_t detbuf_adr;
	u16 *detbuf_adr_virt;
	u8 delrefill_check;
#endif
	u8 head_error_flag;
	int valve_count;
	struct firmware_s *fw;
	int max_pic_w;
	int max_pic_h;
#ifdef AGAIN_HAS_THRESHOLD
	u8 next_again_flag;
	u32 pre_parser_wr_ptr;
#endif
	u32 ratio_control;
	u32 first_pic_flag;
	u32 decode_size;
	struct mutex chunks_mutex;
	int need_cache_size;
	u64 sc_start_time;
	u32 skip_first_nal;
	bool is_swap;
	bool is_4k;

	int frameinfo_enable;
	struct vframe_qos_s vframe_qos;
} /*hevc_stru_t */;

#ifdef AGAIN_HAS_THRESHOLD
u32 again_threshold = 0x40;
#endif
#ifdef SEND_LMEM_WITH_RPM
#define get_lmem_params(hevc, ladr) \
	hevc->lmem_ptr[ladr - (ladr & 0x3) + 3 - (ladr & 0x3)]


static int get_frame_mmu_map_size(void)
{
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
		return (MAX_FRAME_8K_NUM * 4);

	return (MAX_FRAME_4K_NUM * 4);
}

static int is_oversize(int w, int h)
{
	int max = (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)?
		MAX_SIZE_8K : MAX_SIZE_4K;

	if (w < 0 || h < 0)
		return true;

	if (h != 0 && (w > max / h))
		return true;

	return false;
}

void check_head_error(struct hevc_state_s *hevc)
{
#define pcm_enabled_flag                                  0x040
#define pcm_sample_bit_depth_luma                         0x041
#define pcm_sample_bit_depth_chroma                       0x042
	hevc->head_error_flag = 0;
	if ((error_handle_policy & 0x40) == 0)
		return;
	if (get_lmem_params(hevc, pcm_enabled_flag)) {
		uint16_t pcm_depth_luma = get_lmem_params(
			hevc, pcm_sample_bit_depth_luma);
		uint16_t pcm_sample_chroma = get_lmem_params(
			hevc, pcm_sample_bit_depth_chroma);
		if (pcm_depth_luma >
			hevc->bit_depth_luma ||
			pcm_sample_chroma >
			hevc->bit_depth_chroma) {
			hevc_print(hevc, 0,
			"error, pcm bit depth %d, %d is greater than normal bit depth %d, %d\n",
			pcm_depth_luma,
			pcm_sample_chroma,
			hevc->bit_depth_luma,
			hevc->bit_depth_chroma);
			hevc->head_error_flag = 1;
		}
	}
}
#endif

#ifdef SUPPORT_10BIT
/* Losless compression body buffer size 4K per 64x32 (jt) */
static  int  compute_losless_comp_body_size(struct hevc_state_s *hevc,
	int width, int height, int mem_saving_mode)
{
	int width_x64;
	int     height_x32;
	int     bsize;

	width_x64 = width + 63;
	width_x64 >>= 6;

	height_x32 = height + 31;
	height_x32 >>= 5;
	if (mem_saving_mode == 1 && hevc->mmu_enable)
		bsize = 3200 * width_x64 * height_x32;
	else if (mem_saving_mode == 1)
		bsize = 3072 * width_x64 * height_x32;
	else
		bsize = 4096 * width_x64 * height_x32;

	return  bsize;
}

/* Losless compression header buffer size 32bytes per 128x64 (jt) */
static  int  compute_losless_comp_header_size(int width, int height)
{
	int     width_x128;
	int     height_x64;
	int     hsize;

	width_x128 = width + 127;
	width_x128 >>= 7;

	height_x64 = height + 63;
	height_x64 >>= 6;

	hsize = 32*width_x128*height_x64;

	return  hsize;
}
#endif

static int add_log(struct hevc_state_s *hevc,
	const char *fmt, ...)
{
#define HEVC_LOG_BUF		196
	struct debug_log_s *log_item;
	unsigned char buf[HEVC_LOG_BUF];
	int len = 0;
	va_list args;
	mutex_lock(&vh265_log_mutex);
	va_start(args, fmt);
	len = sprintf(buf, "<%ld>   <%05d> ",
		jiffies, hevc->decode_idx);
	len += vsnprintf(buf + len,
		HEVC_LOG_BUF - len, fmt, args);
	va_end(args);
	log_item = kmalloc(
		sizeof(struct debug_log_s) + len,
		GFP_KERNEL);
	if (log_item) {
		INIT_LIST_HEAD(&log_item->list);
		strcpy(&log_item->data, buf);
		list_add_tail(&log_item->list,
			&hevc->log_list);
	}
	mutex_unlock(&vh265_log_mutex);
	return 0;
}

static void dump_log(struct hevc_state_s *hevc)
{
	int i = 0;
	struct debug_log_s *log_item, *tmp;
	mutex_lock(&vh265_log_mutex);
	list_for_each_entry_safe(log_item, tmp, &hevc->log_list, list) {
		hevc_print(hevc, 0,
			"[LOG%04d]%s\n",
			i++,
			&log_item->data);
		list_del(&log_item->list);
		kfree(log_item);
	}
	mutex_unlock(&vh265_log_mutex);
}

static unsigned char is_skip_decoding(struct hevc_state_s *hevc,
		struct PIC_s *pic)
{
	if (pic->error_mark
		&& ((hevc->ignore_bufmgr_error & 0x1) == 0))
		return 1;
	return 0;
}

static int get_pic_poc(struct hevc_state_s *hevc,
		unsigned int idx)
{
	if (idx != 0xff
		&& idx < MAX_REF_PIC_NUM
		&& hevc->m_PIC[idx])
		return hevc->m_PIC[idx]->POC;
	return INVALID_POC;
}

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static int get_valid_double_write_mode(struct hevc_state_s *hevc)
{
	return (hevc->m_ins_flag &&
		((double_write_mode & 0x80000000) == 0)) ?
		hevc->double_write_mode :
		(double_write_mode & 0x7fffffff);
}

static int get_dynamic_buf_num_margin(struct hevc_state_s *hevc)
{
	return (hevc->m_ins_flag &&
		((dynamic_buf_num_margin & 0x80000000) == 0)) ?
		hevc->dynamic_buf_num_margin :
		(dynamic_buf_num_margin & 0x7fffffff);
}
#endif

static int get_double_write_mode(struct hevc_state_s *hevc)
{
	u32 valid_dw_mode = get_valid_double_write_mode(hevc);
	u32 dw = hevc->double_write_mode;
	if (valid_dw_mode == 0x100) {
		int w = hevc->pic_w;
		int h = hevc->pic_h;
		if (w > 1920 && h > 1088)
			dw = 0x4; /*1:2*/
		else
			dw = 0x1; /*1:1*/

	} else
		dw = valid_dw_mode;
	return dw;
}

static int get_double_write_ratio(struct hevc_state_s *hevc,
	int dw_mode)
{
	int ratio = 1;
	if ((dw_mode == 2) ||
			(dw_mode == 3))
		ratio = 4;
	else if (dw_mode == 4)
		ratio = 2;
	return ratio;
}
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static unsigned char get_idx(struct hevc_state_s *hevc)
{
	return hevc->index;
}
#endif

#undef pr_info
#define pr_info printk
static int hevc_print(struct hevc_state_s *hevc,
	int flag, const char *fmt, ...)
{
#define HEVC_PRINT_BUF		256
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
	if (hevc == NULL ||
		(flag == 0) ||
		((debug_mask &
		(1 << hevc->index))
		&& (debug & flag))) {
		va_list args;

		va_start(args, fmt);
		if (hevc)
			len = sprintf(buf, "[%d]", hevc->index);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		pr_debug("%s", buf);
		va_end(args);
	}
	return 0;
}

static int hevc_print_cont(struct hevc_state_s *hevc,
	int flag, const char *fmt, ...)
{
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (hevc == NULL ||
		(flag == 0) ||
		((debug_mask &
		(1 << hevc->index))
		&& (debug & flag))) {
#endif
		va_list args;

		va_start(args, fmt);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	}
#endif
	return 0;
}

static void put_mv_buf(struct hevc_state_s *hevc,
	struct PIC_s *pic);

static void update_vf_memhandle(struct hevc_state_s *hevc,
	struct vframe_s *vf, struct PIC_s *pic);

static void set_canvas(struct hevc_state_s *hevc, struct PIC_s *pic);

static void release_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic);
static void release_pic_mmu_buf(struct hevc_state_s *hevc, struct PIC_s *pic);

#ifdef MULTI_INSTANCE_SUPPORT
static void backup_decode_state(struct hevc_state_s *hevc)
{
	hevc->m_pocRandomAccess_bak = hevc->m_pocRandomAccess;
	hevc->curr_POC_bak = hevc->curr_POC;
	hevc->iPrevPOC_bak = hevc->iPrevPOC;
	hevc->iPrevTid0POC_bak = hevc->iPrevTid0POC;
	hevc->start_parser_type_bak = hevc->start_parser_type;
	hevc->start_decoding_flag_bak = hevc->start_decoding_flag;
	hevc->rps_set_id_bak = hevc->rps_set_id;
	hevc->pic_decoded_lcu_idx_bak = hevc->pic_decoded_lcu_idx;
	hevc->decode_idx_bak = hevc->decode_idx;

}

static void restore_decode_state(struct hevc_state_s *hevc)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);
	if (!vdec_has_more_input(vdec)) {
		hevc->pic_decoded_lcu_idx =
			READ_VREG(HEVC_PARSER_LCU_START)
			& 0xffffff;
		return;
	}
	hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
		"%s: discard pic index 0x%x\n",
		__func__, hevc->decoding_pic ?
		hevc->decoding_pic->index : 0xff);
	if (hevc->decoding_pic) {
		hevc->decoding_pic->error_mark = 0;
		hevc->decoding_pic->output_ready = 0;
		hevc->decoding_pic->output_mark = 0;
		hevc->decoding_pic->referenced = 0;
		hevc->decoding_pic->POC = INVALID_POC;
		put_mv_buf(hevc, hevc->decoding_pic);
		release_pic_mmu_buf(hevc, hevc->decoding_pic);
		release_aux_data(hevc, hevc->decoding_pic);
		hevc->decoding_pic = NULL;
	}
	hevc->decode_idx = hevc->decode_idx_bak;
	hevc->m_pocRandomAccess = hevc->m_pocRandomAccess_bak;
	hevc->curr_POC = hevc->curr_POC_bak;
	hevc->iPrevPOC = hevc->iPrevPOC_bak;
	hevc->iPrevTid0POC = hevc->iPrevTid0POC_bak;
	hevc->start_parser_type = hevc->start_parser_type_bak;
	hevc->start_decoding_flag = hevc->start_decoding_flag_bak;
	hevc->rps_set_id = hevc->rps_set_id_bak;
	hevc->pic_decoded_lcu_idx = hevc->pic_decoded_lcu_idx_bak;

	if (hevc->pic_list_init_flag == 1)
		hevc->pic_list_init_flag = 0;
	/*if (hevc->decode_idx == 0)
		hevc->start_decoding_flag = 0;*/

	hevc->slice_idx = 0;
	hevc->used_4k_num = -1;
}
#endif

static void hevc_init_stru(struct hevc_state_s *hevc,
		struct BuffInfo_s *buf_spec_i)
{
	int i;
	INIT_LIST_HEAD(&hevc->log_list);
	hevc->work_space_buf = buf_spec_i;
	hevc->prefix_aux_size = 0;
	hevc->suffix_aux_size = 0;
	hevc->aux_addr = NULL;
	hevc->rpm_addr = NULL;
	hevc->lmem_addr = NULL;

	hevc->curr_POC = INVALID_POC;

	hevc->pic_list_init_flag = 0;
	hevc->use_cma_flag = 0;
	hevc->decode_idx = 0;
	hevc->slice_idx = 0;
	hevc->new_pic = 0;
	hevc->new_tile = 0;
	hevc->iPrevPOC = 0;
	hevc->list_no = 0;
	/* int m_uiMaxCUWidth = 1<<7; */
	/* int m_uiMaxCUHeight = 1<<7; */
	hevc->m_pocRandomAccess = MAX_INT;
	hevc->tile_enabled = 0;
	hevc->tile_x = 0;
	hevc->tile_y = 0;
	hevc->iPrevTid0POC = 0;
	hevc->slice_addr = 0;
	hevc->slice_segment_addr = 0;
	hevc->skip_flag = 0;
	hevc->misc_flag0 = 0;

	hevc->cur_pic = NULL;
	hevc->col_pic = NULL;
	hevc->wait_buf = 0;
	hevc->error_flag = 0;
	hevc->head_error_flag = 0;
	hevc->error_skip_nal_count = 0;
	hevc->have_vps = 0;
	hevc->have_sps = 0;
	hevc->have_pps = 0;
	hevc->have_valid_start_slice = 0;

	hevc->pts_mode = PTS_NORMAL;
	hevc->last_pts = 0;
	hevc->last_lookup_pts = 0;
	hevc->last_pts_us64 = 0;
	hevc->last_lookup_pts_us64 = 0;
	hevc->pts_mode_switching_count = 0;
	hevc->pts_mode_recovery_count = 0;

	hevc->PB_skip_mode = nal_skip_policy & 0x3;
	hevc->PB_skip_count_after_decoding = (nal_skip_policy >> 16) & 0xffff;
	if (hevc->PB_skip_mode == 0)
		hevc->ignore_bufmgr_error = 0x1;
	else
		hevc->ignore_bufmgr_error = 0x0;

	for (i = 0; i < MAX_REF_PIC_NUM; i++)
		hevc->m_PIC[i] = NULL;
	hevc->pic_num = 0;
	hevc->lcu_x_num_pre = 0;
	hevc->lcu_y_num_pre = 0;
	hevc->first_pic_after_recover = 0;

	hevc->pre_top_pic = NULL;
	hevc->pre_bot_pic = NULL;

	hevc->sei_present_flag = 0;
	hevc->valve_count = 0;
	hevc->first_pic_flag = 0;
#ifdef MULTI_INSTANCE_SUPPORT
	hevc->decoded_poc = INVALID_POC;
	hevc->start_process_time = 0;
	hevc->last_lcu_idx = 0;
	hevc->decode_timeout_count = 0;
	hevc->timeout_num = 0;
	hevc->eos = 0;
	hevc->pic_decoded_lcu_idx = -1;
	hevc->over_decode = 0;
	hevc->used_4k_num = -1;
	hevc->start_decoding_flag = 0;
	hevc->rps_set_id = 0;
	backup_decode_state(hevc);
#endif
#ifdef DETREFILL_ENABLE
	hevc->detbuf_adr = 0;
	hevc->detbuf_adr_virt = NULL;
#endif
}

static int prepare_display_buf(struct hevc_state_s *hevc, struct PIC_s *pic);
static int H265_alloc_mmu(struct hevc_state_s *hevc,
			struct PIC_s *new_pic,	unsigned short bit_depth,
			unsigned int *mmu_index_adr);

#ifdef DETREFILL_ENABLE
#define DETREFILL_BUF_SIZE (4 * 0x4000)
#define HEVC_SAO_DBG_MODE0                         0x361e
#define HEVC_SAO_DBG_MODE1                         0x361f
#define HEVC_SAO_CTRL10                            0x362e
#define HEVC_SAO_CTRL11                            0x362f
static int init_detrefill_buf(struct hevc_state_s *hevc)
{
	if (hevc->detbuf_adr_virt)
		return 0;

	hevc->detbuf_adr_virt =
		(void *)dma_alloc_coherent(amports_get_dma_device(),
			DETREFILL_BUF_SIZE, &hevc->detbuf_adr,
			GFP_KERNEL);

	if (hevc->detbuf_adr_virt == NULL) {
		pr_err("%s: failed to alloc ETREFILL_BUF\n", __func__);
		return -1;
	}
	return 0;
}

static void uninit_detrefill_buf(struct hevc_state_s *hevc)
{
	if (hevc->detbuf_adr_virt) {
		dma_free_coherent(amports_get_dma_device(),
			DETREFILL_BUF_SIZE, hevc->detbuf_adr_virt,
			hevc->detbuf_adr);

		hevc->detbuf_adr_virt = NULL;
		hevc->detbuf_adr = 0;
	}
}

/*
 * convert uncompressed frame buffer data from/to ddr
 */
static void convUnc8x4blk(uint16_t* blk8x4Luma,
	uint16_t* blk8x4Cb, uint16_t* blk8x4Cr, uint16_t* cmBodyBuf, int32_t direction)
{
	if (direction == 0) {
		blk8x4Luma[3 + 0 * 8] = ((cmBodyBuf[0] >> 0)) & 0x3ff;
		blk8x4Luma[3 + 1 * 8] = ((cmBodyBuf[1] << 6)
			| (cmBodyBuf[0] >> 10)) & 0x3ff;
		blk8x4Luma[3 + 2 * 8] = ((cmBodyBuf[1] >> 4)) & 0x3ff;
		blk8x4Luma[3 + 3 * 8] = ((cmBodyBuf[2] << 2)
			| (cmBodyBuf[1] >> 14)) & 0x3ff;
		blk8x4Luma[7 + 0 * 8] = ((cmBodyBuf[3] << 8)
			| (cmBodyBuf[2] >> 8)) & 0x3ff;
		blk8x4Luma[7 + 1 * 8] = ((cmBodyBuf[3] >> 2)) & 0x3ff;
		blk8x4Luma[7 + 2 * 8] = ((cmBodyBuf[4] << 4)
			| (cmBodyBuf[3] >> 12)) & 0x3ff;
		blk8x4Luma[7 + 3 * 8] = ((cmBodyBuf[4] >> 6)) & 0x3ff;
		blk8x4Cb  [0 + 0 * 4] = ((cmBodyBuf[5] >> 0)) & 0x3ff;
		blk8x4Cr  [0 + 0 * 4] = ((cmBodyBuf[6]	<< 6)
			| (cmBodyBuf[5] >> 10)) & 0x3ff;
		blk8x4Cb  [0 + 1 * 4] = ((cmBodyBuf[6] >> 4)) & 0x3ff;
		blk8x4Cr  [0 + 1 * 4] = ((cmBodyBuf[7] << 2)
			| (cmBodyBuf[6] >> 14)) & 0x3ff;

		blk8x4Luma[0 + 0 * 8] = ((cmBodyBuf[0 + 8] >> 0)) & 0x3ff;
		blk8x4Luma[1 + 0 * 8] = ((cmBodyBuf[1 + 8] << 6) |
			(cmBodyBuf[0 + 8] >> 10)) & 0x3ff;
		blk8x4Luma[2 + 0 * 8] = ((cmBodyBuf[1 + 8] >> 4)) & 0x3ff;
		blk8x4Luma[0 + 1 * 8] = ((cmBodyBuf[2 + 8] << 2) |
			(cmBodyBuf[1 + 8] >> 14)) & 0x3ff;
		blk8x4Luma[1 + 1 * 8] = ((cmBodyBuf[3 + 8] << 8) |
			(cmBodyBuf[2 + 8] >> 8)) & 0x3ff;
		blk8x4Luma[2 + 1 * 8] = ((cmBodyBuf[3 + 8] >> 2)) & 0x3ff;
		blk8x4Luma[0 + 2 * 8] = ((cmBodyBuf[4 + 8] << 4) |
			(cmBodyBuf[3 + 8] >> 12)) & 0x3ff;
		blk8x4Luma[1 + 2 * 8] = ((cmBodyBuf[4 + 8] >> 6)) & 0x3ff;
		blk8x4Luma[2 + 2 * 8] = ((cmBodyBuf[5 + 8] >> 0)) & 0x3ff;
		blk8x4Luma[0 + 3 * 8] = ((cmBodyBuf[6 + 8] << 6) |
			(cmBodyBuf[5 + 8] >> 10)) & 0x3ff;
		blk8x4Luma[1 + 3 * 8] = ((cmBodyBuf[6 + 8] >> 4)) & 0x3ff;
		blk8x4Luma[2 + 3 * 8] = ((cmBodyBuf[7 + 8] << 2) |
			(cmBodyBuf[6 + 8] >> 14)) & 0x3ff;

		blk8x4Luma[4 + 0 * 8] = ((cmBodyBuf[0 + 16] >> 0)) & 0x3ff;
		blk8x4Luma[5 + 0 * 8] = ((cmBodyBuf[1 + 16] << 6) |
			(cmBodyBuf[0 + 16] >> 10)) & 0x3ff;
		blk8x4Luma[6 + 0 * 8] = ((cmBodyBuf[1 + 16] >> 4)) & 0x3ff;
		blk8x4Luma[4 + 1 * 8] = ((cmBodyBuf[2 + 16] << 2) |
			(cmBodyBuf[1 + 16] >> 14)) & 0x3ff;
		blk8x4Luma[5 + 1 * 8] = ((cmBodyBuf[3 + 16] << 8) |
			(cmBodyBuf[2 + 16] >> 8)) & 0x3ff;
		blk8x4Luma[6 + 1 * 8] = ((cmBodyBuf[3 + 16] >> 2)) & 0x3ff;
		blk8x4Luma[4 + 2 * 8] = ((cmBodyBuf[4 + 16] << 4) |
			(cmBodyBuf[3 + 16] >> 12)) & 0x3ff;
		blk8x4Luma[5 + 2 * 8] = ((cmBodyBuf[4 + 16] >> 6)) & 0x3ff;
		blk8x4Luma[6 + 2 * 8] = ((cmBodyBuf[5 + 16] >> 0)) & 0x3ff;
		blk8x4Luma[4 + 3 * 8] = ((cmBodyBuf[6 + 16] << 6) |
			(cmBodyBuf[5 + 16] >> 10)) & 0x3ff;
		blk8x4Luma[5 + 3 * 8] = ((cmBodyBuf[6 + 16] >> 4)) & 0x3ff;
		blk8x4Luma[6 + 3 * 8] = ((cmBodyBuf[7 + 16] << 2) |
			(cmBodyBuf[6 + 16] >> 14)) & 0x3ff;

		blk8x4Cb[1 + 0 * 4] = ((cmBodyBuf[0 + 24] >> 0)) & 0x3ff;
		blk8x4Cr[1 + 0 * 4] = ((cmBodyBuf[1 + 24] << 6) |
			(cmBodyBuf[0 + 24] >> 10)) & 0x3ff;
		blk8x4Cb[2 + 0 * 4] = ((cmBodyBuf[1 + 24] >> 4)) & 0x3ff;
		blk8x4Cr[2 + 0 * 4] = ((cmBodyBuf[2 + 24] << 2) |
			(cmBodyBuf[1 + 24] >> 14)) & 0x3ff;
		blk8x4Cb[3 + 0 * 4] = ((cmBodyBuf[3 + 24] << 8) |
			(cmBodyBuf[2 + 24] >> 8)) & 0x3ff;
		blk8x4Cr[3 + 0 * 4] = ((cmBodyBuf[3 + 24] >> 2)) & 0x3ff;
		blk8x4Cb[1 + 1 * 4] = ((cmBodyBuf[4 + 24] << 4) |
			(cmBodyBuf[3 + 24] >> 12)) & 0x3ff;
		blk8x4Cr[1 + 1 * 4] = ((cmBodyBuf[4 + 24] >> 6)) & 0x3ff;
		blk8x4Cb[2 + 1 * 4] = ((cmBodyBuf[5 + 24] >> 0)) & 0x3ff;
		blk8x4Cr[2 + 1 * 4] = ((cmBodyBuf[6 + 24] << 6) |
			(cmBodyBuf[5 + 24] >> 10)) & 0x3ff;
		blk8x4Cb[3 + 1 * 4] = ((cmBodyBuf[6 + 24] >> 4)) & 0x3ff;
		blk8x4Cr[3 + 1 * 4] = ((cmBodyBuf[7 + 24] << 2) |
			(cmBodyBuf[6 + 24] >> 14)) & 0x3ff;
	} else {
		cmBodyBuf[0 + 8 * 0] = (blk8x4Luma[3 + 1 * 8] << 10) |
			blk8x4Luma[3 + 0 * 8];
		cmBodyBuf[1 + 8 * 0] = (blk8x4Luma[3 + 3 * 8] << 14) |
			(blk8x4Luma[3 + 2 * 8] << 4) | (blk8x4Luma[3 + 1 * 8] >> 6);
		cmBodyBuf[2 + 8 * 0] = (blk8x4Luma[7 + 0 * 8] << 8) |
			(blk8x4Luma[3 + 3 * 8] >> 2);
		cmBodyBuf[3 + 8 * 0] = (blk8x4Luma[7 + 2 * 8] << 12) |
			(blk8x4Luma[7 + 1 * 8] << 2) | (blk8x4Luma[7 + 0 * 8] >>8);
		cmBodyBuf[4 + 8 * 0] = (blk8x4Luma[7 + 3 * 8] << 6) |
			(blk8x4Luma[7 + 2 * 8] >>4);
		cmBodyBuf[5 + 8 * 0] = (blk8x4Cr[0 + 0 * 4] << 10) |
			blk8x4Cb[0 + 0 * 4];
		cmBodyBuf[6 + 8 * 0] = (blk8x4Cr[0 + 1 * 4] << 14) |
			(blk8x4Cb[0 + 1 * 4] << 4)   | (blk8x4Cr[0 + 0 * 4] >> 6);
		cmBodyBuf[7 + 8 * 0] = (0<< 8) | (blk8x4Cr[0 + 1 * 4] >> 2);

		cmBodyBuf[0 + 8 * 1] = (blk8x4Luma[1 + 0 * 8] << 10) |
			blk8x4Luma[0 + 0 * 8];
		cmBodyBuf[1 + 8 * 1] = (blk8x4Luma[0 + 1 * 8] << 14) |
			(blk8x4Luma[2 + 0 * 8] << 4) | (blk8x4Luma[1 + 0 * 8] >> 6);
		cmBodyBuf[2 + 8 * 1] = (blk8x4Luma[1 + 1 * 8] << 8) |
			(blk8x4Luma[0 + 1 * 8] >> 2);
		cmBodyBuf[3 + 8 * 1] = (blk8x4Luma[0 + 2 * 8] << 12) |
			(blk8x4Luma[2 + 1 * 8] << 2) | (blk8x4Luma[1 + 1 * 8] >>8);
		cmBodyBuf[4 + 8 * 1] = (blk8x4Luma[1 + 2 * 8] << 6) |
			(blk8x4Luma[0 + 2 * 8] >>4);
		cmBodyBuf[5 + 8 * 1] = (blk8x4Luma[0 + 3 * 8] << 10) |
			blk8x4Luma[2 + 2 * 8];
		cmBodyBuf[6 + 8 * 1] = (blk8x4Luma[2 + 3 * 8] << 14) |
			(blk8x4Luma[1 + 3 * 8] << 4) | (blk8x4Luma[0 + 3 * 8] >> 6);
		cmBodyBuf[7 + 8 * 1] = (0<< 8) | (blk8x4Luma[2 + 3 * 8] >> 2);

		cmBodyBuf[0 + 8 * 2] = (blk8x4Luma[5 + 0 * 8] << 10) |
			blk8x4Luma[4 + 0 * 8];
		cmBodyBuf[1 + 8 * 2] = (blk8x4Luma[4 + 1 * 8] << 14) |
			(blk8x4Luma[6 + 0 * 8] << 4) | (blk8x4Luma[5 + 0 * 8] >> 6);
		cmBodyBuf[2 + 8 * 2] = (blk8x4Luma[5 + 1 * 8] << 8) |
			(blk8x4Luma[4 + 1 * 8] >> 2);
		cmBodyBuf[3 + 8 * 2] = (blk8x4Luma[4 + 2 * 8] << 12) |
			(blk8x4Luma[6 + 1 * 8] << 2) | (blk8x4Luma[5 + 1 * 8] >>8);
		cmBodyBuf[4 + 8 * 2] = (blk8x4Luma[5 + 2 * 8] << 6) |
			(blk8x4Luma[4 + 2 * 8] >>4);
		cmBodyBuf[5 + 8 * 2] = (blk8x4Luma[4 + 3 * 8] << 10) |
			blk8x4Luma[6 + 2 * 8];
		cmBodyBuf[6 + 8 * 2] = (blk8x4Luma[6 + 3 * 8] << 14) |
			(blk8x4Luma[5 + 3 * 8] << 4) | (blk8x4Luma[4 + 3 * 8] >> 6);
		cmBodyBuf[7 + 8 * 2] = (0<< 8) | (blk8x4Luma[6 + 3 * 8] >> 2);

		cmBodyBuf[0 + 8 * 3] = (blk8x4Cr[1 + 0 * 4] << 10) |
			blk8x4Cb[1 + 0 * 4];
		cmBodyBuf[1 + 8 * 3] = (blk8x4Cr[2 + 0 * 4] << 14) |
			(blk8x4Cb[2 + 0 * 4] << 4) | (blk8x4Cr[1 + 0 * 4] >> 6);
		cmBodyBuf[2 + 8 * 3] = (blk8x4Cb[3 + 0 * 4] << 8) |
			(blk8x4Cr[2 + 0 * 4] >> 2);
		cmBodyBuf[3 + 8 * 3] = (blk8x4Cb[1 + 1 * 4] << 12) |
			(blk8x4Cr[3 + 0 * 4] << 2) | (blk8x4Cb[3 + 0 * 4] >>8);
		cmBodyBuf[4 + 8 * 3] = (blk8x4Cr[1 + 1 * 4] << 6) |
			(blk8x4Cb[1 + 1 * 4] >>4);
		cmBodyBuf[5 + 8 * 3] = (blk8x4Cr[2 + 1 * 4] << 10) |
			blk8x4Cb[2 + 1 * 4];
		cmBodyBuf[6 + 8 * 3] = (blk8x4Cr[3 + 1 * 4] << 14) |
			(blk8x4Cb[3 + 1 * 4] << 4) | (blk8x4Cr[2 + 1 * 4] >> 6);
		cmBodyBuf[7 + 8 * 3] = (0 << 8) | (blk8x4Cr[3 + 1 * 4] >> 2);
	}
}

static void corrRefillWithAmrisc (
	struct hevc_state_s *hevc,
	uint32_t  cmHeaderBaseAddr,
	uint32_t  picWidth,
	uint32_t  ctuPosition)
{
	int32_t i;
	uint16_t ctux = (ctuPosition>>16) & 0xffff;
	uint16_t ctuy = (ctuPosition>> 0) & 0xffff;
	int32_t aboveCtuAvailable = (ctuy) ? 1 : 0;

	uint16_t cmBodyBuf[32 * 18];

	uint32_t pic_width_x64_pre = picWidth + 0x3f;
	uint32_t pic_width_x64 = pic_width_x64_pre >> 6;
	uint32_t stride64x64 = pic_width_x64 * 128;
	uint32_t addr_offset64x64_abv = stride64x64 *
		(aboveCtuAvailable ? ctuy - 1 : ctuy) + 128 * ctux;
	uint32_t addr_offset64x64_cur = stride64x64*ctuy + 128 * ctux;
	uint32_t cmHeaderAddrAbv = cmHeaderBaseAddr + addr_offset64x64_abv;
	uint32_t cmHeaderAddrCur = cmHeaderBaseAddr + addr_offset64x64_cur;
	unsigned int tmpData32;

	uint16_t blkBuf0Y[32];
	uint16_t blkBuf0Cb[8];
	uint16_t blkBuf0Cr[8];
	uint16_t blkBuf1Y[32];
	uint16_t blkBuf1Cb[8];
	uint16_t blkBuf1Cr[8];
	int32_t  blkBufCnt = 0;

	int32_t blkIdx;

	WRITE_VREG(HEVC_SAO_CTRL10, cmHeaderAddrAbv);
	WRITE_VREG(HEVC_SAO_CTRL11, cmHeaderAddrCur);
	WRITE_VREG(HEVC_SAO_DBG_MODE0, hevc->detbuf_adr);
	WRITE_VREG(HEVC_SAO_DBG_MODE1, 2);

	for (i = 0; i < 32 * 18; i++)
		cmBodyBuf[i] = 0;

	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s, %d\n", __func__, __LINE__);
	do {
		tmpData32 = READ_VREG(HEVC_SAO_DBG_MODE1);
	} while (tmpData32);
	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s, %d\n", __func__, __LINE__);

	hevc_print(hevc, H265_DEBUG_DETAIL,
		"cmBodyBuf from detbuf:\n");
	for (i = 0; i < 32 * 18; i++) {
		cmBodyBuf[i] = hevc->detbuf_adr_virt[i];
		if (get_dbg_flag(hevc) &
				H265_DEBUG_DETAIL) {
			if ((i & 0xf) == 0)
				hevc_print_cont(hevc, 0, "\n");
			hevc_print_cont(hevc, 0, "%02x ", cmBodyBuf[i]);
		}
	}
	hevc_print_cont(hevc, H265_DEBUG_DETAIL, "\n");

	for (i = 0; i < 32; i++)
		blkBuf0Y[i] = 0;
	for (i = 0; i < 8; i++)
		blkBuf0Cb[i] = 0;
	for (i = 0; i < 8; i++)
		blkBuf0Cr[i] = 0;
	for (i = 0; i < 32; i++)
		blkBuf1Y[i] = 0;
	for (i = 0; i < 8; i++)
		blkBuf1Cb[i] = 0;
	for (i = 0; i < 8; i++)
		blkBuf1Cr[i] = 0;

	for (blkIdx = 0; blkIdx < 18; blkIdx++) {
		int32_t   inAboveCtu = (blkIdx<2) ? 1 : 0;
		int32_t   restoreEnable = (blkIdx>0) ? 1 : 0;
		uint16_t* blkY = (blkBufCnt==0) ? blkBuf0Y : blkBuf1Y ;
		uint16_t* blkCb = (blkBufCnt==0) ? blkBuf0Cb : blkBuf1Cb;
		uint16_t* blkCr = (blkBufCnt==0) ? blkBuf0Cr : blkBuf1Cr;
		uint16_t* cmBodyBufNow = cmBodyBuf + (blkIdx * 32);

		if (!aboveCtuAvailable && inAboveCtu)
			continue;

		/* detRefillBuf --> 8x4block*/
		convUnc8x4blk(blkY, blkCb, blkCr, cmBodyBufNow, 0);

		if (restoreEnable) {
			blkY[3 + 0 * 8] = blkY[2 + 0 * 8] + 2;
			blkY[4 + 0 * 8] = blkY[1 + 0 * 8] + 3;
			blkY[5 + 0 * 8] = blkY[0 + 0 * 8] + 1;
			blkY[6 + 0 * 8] = blkY[0 + 0 * 8] + 2;
			blkY[7 + 0 * 8] = blkY[1 + 0 * 8] + 2;
			blkY[3 + 1 * 8] = blkY[2 + 1 * 8] + 1;
			blkY[4 + 1 * 8] = blkY[1 + 1 * 8] + 2;
			blkY[5 + 1 * 8] = blkY[0 + 1 * 8] + 2;
			blkY[6 + 1 * 8] = blkY[0 + 1 * 8] + 2;
			blkY[7 + 1 * 8] = blkY[1 + 1 * 8] + 3;
			blkY[3 + 2 * 8] = blkY[2 + 2 * 8] + 3;
			blkY[4 + 2 * 8] = blkY[1 + 2 * 8] + 1;
			blkY[5 + 2 * 8] = blkY[0 + 2 * 8] + 3;
			blkY[6 + 2 * 8] = blkY[0 + 2 * 8] + 3;
			blkY[7 + 2 * 8] = blkY[1 + 2 * 8] + 3;
			blkY[3 + 3 * 8] = blkY[2 + 3 * 8] + 0;
			blkY[4 + 3 * 8] = blkY[1 + 3 * 8] + 0;
			blkY[5 + 3 * 8] = blkY[0 + 3 * 8] + 1;
			blkY[6 + 3 * 8] = blkY[0 + 3 * 8] + 2;
			blkY[7 + 3 * 8] = blkY[1 + 3 * 8] + 1;
			blkCb[1 + 0 * 4] = blkCb[0 + 0 * 4];
			blkCb[2 + 0 * 4] = blkCb[0 + 0 * 4];
			blkCb[3 + 0 * 4] = blkCb[0 + 0 * 4];
			blkCb[1 + 1 * 4] = blkCb[0 + 1 * 4];
			blkCb[2 + 1 * 4] = blkCb[0 + 1 * 4];
			blkCb[3 + 1 * 4] = blkCb[0 + 1 * 4];
			blkCr[1 + 0 * 4] = blkCr[0 + 0 * 4];
			blkCr[2 + 0 * 4] = blkCr[0 + 0 * 4];
			blkCr[3 + 0 * 4] = blkCr[0 + 0 * 4];
			blkCr[1 + 1 * 4] = blkCr[0 + 1 * 4];
			blkCr[2 + 1 * 4] = blkCr[0 + 1 * 4];
			blkCr[3 + 1 * 4] = blkCr[0 + 1 * 4];

			/*Store data back to DDR*/
			convUnc8x4blk(blkY, blkCb, blkCr, cmBodyBufNow, 1);
		}

		blkBufCnt = (blkBufCnt==1) ? 0 : blkBufCnt + 1;
	}

	hevc_print(hevc, H265_DEBUG_DETAIL,
		"cmBodyBuf to detbuf:\n");
	for (i = 0; i < 32 * 18; i++) {
		hevc->detbuf_adr_virt[i] = cmBodyBuf[i];
		if (get_dbg_flag(hevc) &
				H265_DEBUG_DETAIL) {
			if ((i & 0xf) == 0)
				hevc_print_cont(hevc, 0, "\n");
			hevc_print_cont(hevc, 0, "%02x ", cmBodyBuf[i]);
		}
	}
	hevc_print_cont(hevc, H265_DEBUG_DETAIL, "\n");

	WRITE_VREG(HEVC_SAO_DBG_MODE1, 3);
	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s, %d\n", __func__, __LINE__);
	do {
		tmpData32 = READ_VREG(HEVC_SAO_DBG_MODE1);
	} while (tmpData32);
	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s, %d\n", __func__, __LINE__);
}

static void delrefill(struct hevc_state_s *hevc)
{
	/*
	 * corrRefill
	 */
	/*HEVC_SAO_DBG_MODE0: picGlobalVariable
	[31:30]error number
	[29:20]error2([9:7]tilex[6:0]ctuy)
	[19:10]error1 [9:0]error0*/
	uint32_t detResult = READ_VREG(HEVC_ASSIST_SCRATCH_3);
	uint32_t errorIdx;
	uint32_t errorNum = (detResult>>30);

	if (detResult) {
		hevc_print(hevc, H265_DEBUG_BUFMGR,
			"[corrRefillWithAmrisc] detResult=%08x\n", detResult);
		for (errorIdx = 0; errorIdx < errorNum; errorIdx++) {
			uint32_t errorPos = errorIdx * 10;
			uint32_t errorResult = (detResult >> errorPos) & 0x3ff;
			uint32_t tilex = (errorResult >> 7) - 1;
			uint16_t ctux = hevc->m_tile[0][tilex].start_cu_x
				+ hevc->m_tile[0][tilex].width - 1;
			uint16_t ctuy = (uint16_t)(errorResult & 0x7f);
			uint32_t ctuPosition = (ctux<< 16) + ctuy;
			hevc_print(hevc, H265_DEBUG_BUFMGR,
				"Idx:%d tilex:%d ctu(%d(0x%x), %d(0x%x))\n",
				errorIdx,tilex,ctux,ctux, ctuy,ctuy);
			corrRefillWithAmrisc(
				hevc,
				(uint32_t)hevc->cur_pic->header_adr,
				hevc->pic_w,
				ctuPosition);
		}

		WRITE_VREG(HEVC_ASSIST_SCRATCH_3, 0); /*clear status*/
		WRITE_VREG(HEVC_SAO_DBG_MODE0, 0);
		WRITE_VREG(HEVC_SAO_DBG_MODE1, 1);
	}
}
#endif

static void get_rpm_param(union param_u *params)
{
	int i;
	unsigned int data32;

	for (i = 0; i < 128; i++) {
		do {
			data32 = READ_VREG(RPM_CMD_REG);
			/* hevc_print(hevc, 0, "%x\n", data32); */
		} while ((data32 & 0x10000) == 0);
		params->l.data[i] = data32 & 0xffff;
		/* hevc_print(hevc, 0, "%x\n", data32); */
		WRITE_VREG(RPM_CMD_REG, 0);
	}
}

static struct PIC_s *get_pic_by_POC(struct hevc_state_s *hevc, int POC)
{
	int i;
	struct PIC_s *pic;
	struct PIC_s *ret_pic = NULL;
	if (POC == INVALID_POC)
		return NULL;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1 ||
			pic->BUF_index == -1)
			continue;
		if (pic->POC == POC) {
			if (ret_pic == NULL)
				ret_pic = pic;
			else {
				if (pic->decode_idx > ret_pic->decode_idx)
					ret_pic = pic;
			}
		}
	}
	return ret_pic;
}

static struct PIC_s *get_ref_pic_by_POC(struct hevc_state_s *hevc, int POC)
{
	int i;
	struct PIC_s *pic;
	struct PIC_s *ret_pic = NULL;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1 ||
			pic->BUF_index == -1)
			continue;
		if ((pic->POC == POC) && (pic->referenced)) {
			if (ret_pic == NULL)
				ret_pic = pic;
			else {
				if (pic->decode_idx > ret_pic->decode_idx)
					ret_pic = pic;
			}
		}
	}

	if (ret_pic == NULL) {
		if (get_dbg_flag(hevc)) {
			hevc_print(hevc, 0,
				"Wrong, POC of %d is not in referenced list\n",
				   POC);
		}
		ret_pic = get_pic_by_POC(hevc, POC);
	}
	return ret_pic;
}

static unsigned int log2i(unsigned int val)
{
	unsigned int ret = -1;

	while (val != 0) {
		val >>= 1;
		ret++;
	}
	return ret;
}

static int init_buf_spec(struct hevc_state_s *hevc);
static void uninit_mmu_buffers(struct hevc_state_s *hevc)
{
	if (hevc->mmu_box)
		decoder_mmu_box_free(hevc->mmu_box);
	hevc->mmu_box = NULL;

	if (hevc->bmmu_box)
		decoder_bmmu_box_free(hevc->bmmu_box);
	hevc->bmmu_box = NULL;
}
static int init_mmu_buffers(struct hevc_state_s *hevc)
{
	int tvp_flag = vdec_secure(hw_to_vdec(hevc)) ?
		CODEC_MM_FLAGS_TVP : 0;
	int buf_size = 64;
	if ((hevc->max_pic_w * hevc->max_pic_h) > 0 &&
		(hevc->max_pic_w * hevc->max_pic_h) <= 1920*1088) {
		buf_size = 24;
	}
	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0, "%s max_w %d max_h %d\n",
			__func__, hevc->max_pic_w, hevc->max_pic_h);
	}
	hevc->need_cache_size = buf_size * SZ_1M;
	hevc->sc_start_time = get_jiffies_64();
	if (hevc->mmu_enable
		&& ((get_double_write_mode(hevc) & 0x10) == 0)) {
		hevc->mmu_box = decoder_mmu_box_alloc_box(DRIVER_NAME,
			hevc->index,
			MAX_REF_PIC_NUM,
			buf_size * SZ_1M,
			tvp_flag
			);
		if (!hevc->mmu_box) {
			pr_err("h265 alloc mmu box failed!!\n");
			return -1;
		}
	}

	hevc->bmmu_box = decoder_bmmu_box_alloc_box(DRIVER_NAME,
			hevc->index,
			BMMU_MAX_BUFFERS,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER |
			tvp_flag);
	if (!hevc->bmmu_box) {
		if (hevc->mmu_box)
			decoder_mmu_box_free(hevc->mmu_box);
		hevc->mmu_box = NULL;
		pr_err("h265 alloc mmu box failed!!\n");
		return -1;
	}
	return 0;
}

struct buf_stru_s
{
	int lcu_total;
	int mc_buffer_size_h;
	int mc_buffer_size_u_v_h;
};

#ifndef MV_USE_FIXED_BUF
static void dealloc_mv_bufs(struct hevc_state_s *hevc)
{
	int i;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		if (hevc->m_mv_BUF[i].start_adr) {
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print(hevc, 0,
				"dealloc mv buf(%d) adr 0x%p size 0x%x used_flag %d\n",
				i, hevc->m_mv_BUF[i].start_adr,
				hevc->m_mv_BUF[i].size,
				hevc->m_mv_BUF[i].used_flag);
			decoder_bmmu_box_free_idx(
				hevc->bmmu_box,
				MV_BUFFER_IDX(i));
			hevc->m_mv_BUF[i].start_adr = 0;
			hevc->m_mv_BUF[i].size = 0;
			hevc->m_mv_BUF[i].used_flag = 0;
		}
	}
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		if (hevc->m_PIC[i] != NULL)
			hevc->m_PIC[i]->mv_buf_index = -1;
	}

}

static int alloc_mv_buf(struct hevc_state_s *hevc, int i)
{
	int ret = 0;
	/*get_cma_alloc_ref();*/ /*DEBUG_TMP*/
	if (decoder_bmmu_box_alloc_buf_phy
		(hevc->bmmu_box,
		MV_BUFFER_IDX(i), hevc->mv_buf_size,
		DRIVER_NAME,
		&hevc->m_mv_BUF[i].start_adr) < 0) {
		hevc->m_mv_BUF[i].start_adr = 0;
		ret = -1;
	} else {
		hevc->m_mv_BUF[i].size = hevc->mv_buf_size;
		hevc->m_mv_BUF[i].used_flag = 0;
		ret = 0;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
			"MV Buffer %d: start_adr %p size %x\n",
			i,
			(void *)hevc->m_mv_BUF[i].start_adr,
			hevc->m_mv_BUF[i].size);
		}
	}
	/*put_cma_alloc_ref();*/ /*DEBUG_TMP*/
	return ret;
}
#endif

static int get_mv_buf(struct hevc_state_s *hevc, struct PIC_s *pic)
{
#ifdef MV_USE_FIXED_BUF
	if (pic && pic->index >= 0) {
		if (IS_8K_SIZE(pic->width, pic->height)) {
			pic->mpred_mv_wr_start_addr =
				hevc->work_space_buf->mpred_mv.buf_start
				+ (pic->index * MPRED_8K_MV_BUF_SIZE);
		} else {
			pic->mpred_mv_wr_start_addr =
				hevc->work_space_buf->mpred_mv.buf_start
				+ (pic->index * MPRED_4K_MV_BUF_SIZE);
		}
	}
	return 0;
#else
	int i;
	int ret = -1;
	int new_size;
	if (IS_8K_SIZE(pic->width, pic->height))
		new_size = MPRED_8K_MV_BUF_SIZE + 0x10000;
	else if (IS_4K_SIZE(pic->width, pic->height))
		new_size = MPRED_4K_MV_BUF_SIZE + 0x10000; /*0x120000*/
	else
		new_size = MPRED_MV_BUF_SIZE + 0x10000;
	if (new_size != hevc->mv_buf_size) {
		dealloc_mv_bufs(hevc);
		hevc->mv_buf_size = new_size;
	}
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		if (hevc->m_mv_BUF[i].start_adr &&
			hevc->m_mv_BUF[i].used_flag == 0) {
			hevc->m_mv_BUF[i].used_flag = 1;
			ret = i;
			break;
		}
	}
	if (ret < 0) {
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			if (hevc->m_mv_BUF[i].start_adr == 0) {
				if (alloc_mv_buf(hevc, i) >= 0) {
					hevc->m_mv_BUF[i].used_flag = 1;
					ret = i;
				}
				break;
			}
		}
	}

	if (ret >= 0) {
		pic->mv_buf_index = ret;
		pic->mpred_mv_wr_start_addr =
			(hevc->m_mv_BUF[ret].start_adr + 0xffff) &
			(~0xffff);
		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s => %d (0x%x) size 0x%x\n",
		__func__, ret,
		pic->mpred_mv_wr_start_addr,
		hevc->m_mv_BUF[ret].size);

	} else {
		hevc_print(hevc, 0,
		"%s: Error, mv buf is not enough\n",
		__func__);
	}
	return ret;

#endif
}

static void put_mv_buf(struct hevc_state_s *hevc,
	struct PIC_s *pic)
{
#ifndef MV_USE_FIXED_BUF
	int i = pic->mv_buf_index;
	if (i < 0 || i >= MAX_REF_PIC_NUM) {
		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s: index %d beyond range\n",
		__func__, i);
		return;
	}
	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
	"%s(%d): used_flag(%d)\n",
	__func__, i,
	hevc->m_mv_BUF[i].used_flag);

	if (hevc->m_mv_BUF[i].start_adr &&
		hevc->m_mv_BUF[i].used_flag)
		hevc->m_mv_BUF[i].used_flag = 0;
	pic->mv_buf_index = -1;
#endif
}

static int cal_current_buf_size(struct hevc_state_s *hevc,
	struct buf_stru_s *buf_stru)
{

	int buf_size;
	int pic_width = hevc->pic_w;
	int pic_height = hevc->pic_h;
	int lcu_size = hevc->lcu_size;
	int pic_width_lcu = (pic_width % lcu_size) ? pic_width / lcu_size +
				 1 : pic_width / lcu_size;
	int pic_height_lcu = (pic_height % lcu_size) ? pic_height / lcu_size +
				 1 : pic_height / lcu_size;
	/*SUPPORT_10BIT*/
	int losless_comp_header_size = compute_losless_comp_header_size
		(pic_width, pic_height);
		/*always alloc buf for 10bit*/
	int losless_comp_body_size = compute_losless_comp_body_size
		(hevc, pic_width, pic_height, 0);
	int mc_buffer_size = losless_comp_header_size
		+ losless_comp_body_size;
	int mc_buffer_size_h = (mc_buffer_size + 0xffff) >> 16;
	int mc_buffer_size_u_v_h = 0;

	int dw_mode = get_double_write_mode(hevc);

	if (hevc->mmu_enable) {
		if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
			(IS_8K_SIZE(hevc->pic_w, hevc->pic_h)))
			buf_size = ((MMU_COMPRESS_8K_HEADER_SIZE + 0xffff) >> 16)
				<< 16;
		else
			buf_size = ((MMU_COMPRESS_HEADER_SIZE + 0xffff) >> 16)
				<< 16;
	} else
		buf_size = 0;

	if (dw_mode) {
		int pic_width_dw = pic_width /
			get_double_write_ratio(hevc, dw_mode);
		int pic_height_dw = pic_height /
			get_double_write_ratio(hevc, dw_mode);

		int pic_width_lcu_dw = (pic_width_dw % lcu_size) ?
			pic_width_dw / lcu_size + 1 :
			pic_width_dw / lcu_size;
		int pic_height_lcu_dw = (pic_height_dw % lcu_size) ?
			pic_height_dw / lcu_size + 1 :
			pic_height_dw / lcu_size;
		int lcu_total_dw = pic_width_lcu_dw * pic_height_lcu_dw;

		int mc_buffer_size_u_v = lcu_total_dw * lcu_size * lcu_size / 2;
		mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16;
			/*64k alignment*/
		buf_size += ((mc_buffer_size_u_v_h << 16) * 3);
	}

	if ((!hevc->mmu_enable) &&
		((dw_mode & 0x10) == 0)) {
		/* use compress mode without mmu,
		need buf for compress decoding*/
		buf_size += (mc_buffer_size_h << 16);
	}

	/*in case start adr is not 64k alignment*/
	if (buf_size > 0)
		buf_size += 0x10000;

	if (buf_stru) {
		buf_stru->lcu_total = pic_width_lcu * pic_height_lcu;
		buf_stru->mc_buffer_size_h = mc_buffer_size_h;
		buf_stru->mc_buffer_size_u_v_h = mc_buffer_size_u_v_h;
	}
	return buf_size;
}

static int alloc_buf(struct hevc_state_s *hevc)
{
	int i;
	int ret = -1;
	int buf_size = cal_current_buf_size(hevc, NULL);

	for (i = 0; i < BUF_POOL_SIZE; i++) {
		if (hevc->m_BUF[i].start_adr == 0)
			break;
	}
	if (i < BUF_POOL_SIZE) {
		if (buf_size > 0) {
			/*get_cma_alloc_ref();*/ /*DEBUG_TMP*/
			/*alloc compress header first*/

			if (decoder_bmmu_box_alloc_buf_phy
				(hevc->bmmu_box,
				VF_BUFFER_IDX(i), buf_size,
				DRIVER_NAME,
				&hevc->m_BUF[i].start_adr) < 0)
				hevc->m_BUF[i].start_adr = 0;
			else {
				hevc->m_BUF[i].size = buf_size;
				hevc->m_BUF[i].used_flag = 0;
				ret = 0;
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print(hevc, 0,
					"Buffer %d: start_adr %p size %x\n",
					i,
					(void *)hevc->m_BUF[i].start_adr,
					hevc->m_BUF[i].size);
				}
			}
			/*put_cma_alloc_ref();*/ /*DEBUG_TMP*/
		} else
			ret = 0;
	}
	if (ret >= 0) {
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
			"alloc buf(%d) for %d/%d size 0x%x) => %p\n",
			i, hevc->pic_w, hevc->pic_h,
			buf_size,
			hevc->m_BUF[i].start_adr);
		}
	} else {
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
			"alloc buf(%d) for %d/%d size 0x%x) => Fail!!!\n",
			i, hevc->pic_w, hevc->pic_h,
			buf_size);
		}
	}
	return ret;
}

static void set_buf_unused(struct hevc_state_s *hevc, int i)
{
	if (i >= 0 && i < BUF_POOL_SIZE)
		hevc->m_BUF[i].used_flag = 0;
}

static void dealloc_unused_buf(struct hevc_state_s *hevc)
{
	int i;
	for (i = 0; i < BUF_POOL_SIZE; i++) {
		if (hevc->m_BUF[i].start_adr &&
			hevc->m_BUF[i].used_flag == 0) {
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
				hevc_print(hevc, 0,
					"dealloc buf(%d) adr 0x%p size 0x%x\n",
					i, hevc->m_BUF[i].start_adr,
					hevc->m_BUF[i].size);
			}
			decoder_bmmu_box_free_idx(
				hevc->bmmu_box,
				VF_BUFFER_IDX(i));
			hevc->m_BUF[i].start_adr = 0;
			hevc->m_BUF[i].size = 0;
		}
	}

}

static void dealloc_pic_buf(struct hevc_state_s *hevc,
	struct PIC_s *pic)
{
	int i = pic->BUF_index;
	pic->BUF_index = -1;
	if (i >= 0 &&
		i < BUF_POOL_SIZE &&
		hevc->m_BUF[i].start_adr) {
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"dealloc buf(%d) adr 0x%p size 0x%x\n",
				i, hevc->m_BUF[i].start_adr,
				hevc->m_BUF[i].size);
		}
		decoder_bmmu_box_free_idx(
			hevc->bmmu_box,
			VF_BUFFER_IDX(i));
		hevc->m_BUF[i].used_flag = 0;
		hevc->m_BUF[i].start_adr = 0;
		hevc->m_BUF[i].size = 0;
	}
}

static int get_work_pic_num(struct hevc_state_s *hevc)
{
	int used_buf_num = 0;
	int sps_pic_buf_diff = 0;

	if (get_dynamic_buf_num_margin(hevc) > 0) {
		if ((!hevc->sps_num_reorder_pics_0) &&
			(hevc->param.p.sps_max_dec_pic_buffering_minus1_0)) {
			/* the range of sps_num_reorder_pics_0 is in
			  [0, sps_max_dec_pic_buffering_minus1_0] */
			used_buf_num = get_dynamic_buf_num_margin(hevc) +
				hevc->param.p.sps_max_dec_pic_buffering_minus1_0;
		} else
			used_buf_num = hevc->sps_num_reorder_pics_0
				+ get_dynamic_buf_num_margin(hevc);

		sps_pic_buf_diff = hevc->param.p.sps_max_dec_pic_buffering_minus1_0
					- hevc->sps_num_reorder_pics_0;

#ifdef MULTI_INSTANCE_SUPPORT
		/*
		need one more for multi instance, as
		apply_ref_pic_set() has no chanch to run to
		to clear referenced flag in some case
		*/
		if (hevc->m_ins_flag)
			used_buf_num++;
#endif
	} else
		used_buf_num = max_buf_num;

	if (hevc->save_buffer_mode)
			hevc_print(hevc, 0,
				"save buf _mode : dynamic_buf_num_margin %d ----> %d \n",
				dynamic_buf_num_margin,  hevc->dynamic_buf_num_margin);

	if (sps_pic_buf_diff >= 4)
	{
		used_buf_num += 1;
	}

	if (used_buf_num > MAX_BUF_NUM)
		used_buf_num = MAX_BUF_NUM;
	return used_buf_num;
}

static int get_alloc_pic_count(struct hevc_state_s *hevc)
{
	int alloc_pic_count = 0;
	int i;
	struct PIC_s *pic;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic && pic->index >= 0)
			alloc_pic_count++;
	}
	return alloc_pic_count;
}

static int config_pic(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	int ret = -1;
	int i;
	/*int lcu_size_log2 = hevc->lcu_size_log2;
	int MV_MEM_UNIT=lcu_size_log2==
		6 ? 0x100 : lcu_size_log2==5 ? 0x40 : 0x10;*/
	/*int MV_MEM_UNIT = lcu_size_log2 == 6 ? 0x200 : lcu_size_log2 ==
					 5 ? 0x80 : 0x20;
	int mpred_mv_end = hevc->work_space_buf->mpred_mv.buf_start +
				 hevc->work_space_buf->mpred_mv.buf_size;*/
	unsigned int y_adr = 0;
	struct buf_stru_s buf_stru;
	int buf_size = cal_current_buf_size(hevc, &buf_stru);
	int dw_mode = get_double_write_mode(hevc);

	for (i = 0; i < BUF_POOL_SIZE; i++) {
		if (hevc->m_BUF[i].start_adr != 0 &&
			hevc->m_BUF[i].used_flag == 0 &&
			buf_size <= hevc->m_BUF[i].size) {
			hevc->m_BUF[i].used_flag = 1;
			break;
		}
	}
	if (i >= BUF_POOL_SIZE)
		return -1;

	if (hevc->mmu_enable) {
		pic->header_adr = hevc->m_BUF[i].start_adr;
		if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
			(IS_8K_SIZE(hevc->pic_w, hevc->pic_h)))
			y_adr = hevc->m_BUF[i].start_adr +
				MMU_COMPRESS_8K_HEADER_SIZE;
		else
			y_adr = hevc->m_BUF[i].start_adr +
				MMU_COMPRESS_HEADER_SIZE;
	} else
		y_adr = hevc->m_BUF[i].start_adr;

	y_adr = ((y_adr + 0xffff) >> 16) << 16; /*64k alignment*/
	pic->POC = INVALID_POC;
	/*ensure get_pic_by_POC()
	not get the buffer not decoded*/
	pic->BUF_index = i;

	if ((!hevc->mmu_enable) &&
		((dw_mode & 0x10) == 0)
		) {
		pic->mc_y_adr = y_adr;
		y_adr += (buf_stru.mc_buffer_size_h << 16);
	}
	pic->mc_canvas_y = pic->index;
	pic->mc_canvas_u_v = pic->index;
	if (dw_mode & 0x10) {
		pic->mc_y_adr = y_adr;
		pic->mc_u_v_adr = y_adr +
			((buf_stru.mc_buffer_size_u_v_h << 16) << 1);

		pic->mc_canvas_y = (pic->index << 1);
		pic->mc_canvas_u_v = (pic->index << 1) + 1;

		pic->dw_y_adr = pic->mc_y_adr;
		pic->dw_u_v_adr = pic->mc_u_v_adr;
	} else if (dw_mode) {
		pic->dw_y_adr = y_adr;
		pic->dw_u_v_adr = pic->dw_y_adr +
		((buf_stru.mc_buffer_size_u_v_h << 16) << 1);
	}


	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
		"%s index %d BUF_index %d mc_y_adr %x\n",
		 __func__, pic->index,
		 pic->BUF_index, pic->mc_y_adr);
		if (hevc->mmu_enable &&
			dw_mode)
			hevc_print(hevc, 0,
			"mmu double write  adr %ld\n",
			 pic->cma_alloc_addr);


	}
	ret = 0;

	return ret;
}

static void init_pic_list(struct hevc_state_s *hevc)
{
	int i;
	int init_buf_num = get_work_pic_num(hevc);
	int dw_mode = get_double_write_mode(hevc);
	struct vdec_s *vdec = hw_to_vdec(hevc);
	/*alloc decoder buf*/
	for (i = 0; i < init_buf_num; i++) {
		if (alloc_buf(hevc) < 0) {
			if (i <= 8) {
				/*if alloced (i+1)>=9
				don't send errors.*/
				hevc->fatal_error |=
				DECODER_FATAL_ERROR_NO_MEM;
			}
			break;
		}
	}

	for (i = 0; i < init_buf_num; i++) {
		struct PIC_s *pic =
			vmalloc(sizeof(struct PIC_s));
		if (pic == NULL) {
			hevc_print(hevc, 0,
				"%s: alloc pic %d fail!!!\n",
				__func__, i);
			break;
		}
		memset(pic, 0, sizeof(struct PIC_s));
		hevc->m_PIC[i] = pic;
		pic->index = i;
		pic->BUF_index = -1;
		pic->mv_buf_index = -1;
		if (vdec->parallel_dec == 1) {
			pic->y_canvas_index = -1;
			pic->uv_canvas_index = -1;
		}
		if (config_pic(hevc, pic) < 0) {
			if (get_dbg_flag(hevc))
				hevc_print(hevc, 0,
					"Config_pic %d fail\n", pic->index);
			pic->index = -1;
			i++;
			break;
		}
		pic->width = hevc->pic_w;
		pic->height = hevc->pic_h;
		pic->double_write_mode = dw_mode;
		if (pic->double_write_mode)
			set_canvas(hevc, pic);
	}

	for (; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic =
			vmalloc(sizeof(struct PIC_s));
		if (pic == NULL) {
			hevc_print(hevc, 0,
				"%s: alloc pic %d fail!!!\n",
				__func__, i);
			break;
		}
		memset(pic, 0, sizeof(struct PIC_s));
		hevc->m_PIC[i] = pic;
		pic->index = -1;
		pic->BUF_index = -1;
		if (vdec->parallel_dec == 1) {
			pic->y_canvas_index = -1;
			pic->uv_canvas_index = -1;
		}
	}

}

static void uninit_pic_list(struct hevc_state_s *hevc)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);
	int i;
#ifndef MV_USE_FIXED_BUF
	dealloc_mv_bufs(hevc);
#endif
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic = hevc->m_PIC[i];

		if (pic) {
			if (vdec->parallel_dec == 1) {
				vdec->free_canvas_ex(pic->y_canvas_index, vdec->id);
				vdec->free_canvas_ex(pic->uv_canvas_index, vdec->id);
			}
			release_aux_data(hevc, pic);
			vfree(pic);
			hevc->m_PIC[i] = NULL;
		}
	}
}

#ifdef LOSLESS_COMPRESS_MODE
static void init_decode_head_hw(struct hevc_state_s *hevc)
{

	struct BuffInfo_s *buf_spec = hevc->work_space_buf;
	unsigned int data32;

	int losless_comp_header_size =
		compute_losless_comp_header_size(hevc->pic_w,
			 hevc->pic_h);
	int losless_comp_body_size = compute_losless_comp_body_size(hevc,
		hevc->pic_w, hevc->pic_h, hevc->mem_saving_mode);

	hevc->losless_comp_body_size = losless_comp_body_size;


	if (hevc->mmu_enable) {
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0x1 << 4));
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0x0);
	} else {
	if (hevc->mem_saving_mode == 1)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,
			(1 << 3) | ((workaround_enable & 2) ? 1 : 0));
	else
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,
			((workaround_enable & 2) ? 1 : 0));
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, (losless_comp_body_size >> 5));
	/*
	 *WRITE_VREG(HEVCD_MPP_DECOMP_CTL3,(0xff<<20) | (0xff<<10) | 0xff);
	 *	//8-bit mode
	 */
	}
	WRITE_VREG(HEVC_CM_BODY_LENGTH, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH, losless_comp_header_size);

	if (hevc->mmu_enable) {
		WRITE_VREG(HEVC_SAO_MMU_VH0_ADDR, buf_spec->mmu_vbh.buf_start);
		WRITE_VREG(HEVC_SAO_MMU_VH1_ADDR,
			buf_spec->mmu_vbh.buf_start +
			buf_spec->mmu_vbh.buf_size/2);
		data32 = READ_VREG(HEVC_SAO_CTRL9);
		data32 |= 0x1;
		WRITE_VREG(HEVC_SAO_CTRL9, data32);

		/* use HEVC_CM_HEADER_START_ADDR */
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 |= (1<<10);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}

	if (!hevc->m_ins_flag)
		hevc_print(hevc, 0,
			"%s: (%d, %d) body_size 0x%x header_size 0x%x\n",
			__func__, hevc->pic_w, hevc->pic_h,
			losless_comp_body_size, losless_comp_header_size);

}
#endif
#define HEVCD_MPP_ANC2AXI_TBL_DATA                 0x3464

static void init_pic_list_hw(struct hevc_state_s *hevc)
{
	int i;
	int cur_pic_num = MAX_REF_PIC_NUM;
	int dw_mode = get_double_write_mode(hevc);
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL)
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR,
			(0x1 << 1) | (0x1 << 2));
	else
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x0);

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		if (hevc->m_PIC[i] == NULL ||
			hevc->m_PIC[i]->index == -1) {
			cur_pic_num = i;
			break;
		}
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) {
			if (hevc->mmu_enable && ((dw_mode & 0x10) == 0))
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
					hevc->m_PIC[i]->header_adr>>5);
			else
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
					hevc->m_PIC[i]->mc_y_adr >> 5);
		} else
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
				hevc->m_PIC[i]->mc_y_adr |
				(hevc->m_PIC[i]->mc_canvas_y << 8) | 0x1);
		if (dw_mode & 0x10) {
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) {
					WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
					hevc->m_PIC[i]->mc_u_v_adr >> 5);
				}
			else
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
					hevc->m_PIC[i]->mc_u_v_adr |
					(hevc->m_PIC[i]->mc_canvas_u_v << 8)
					| 0x1);
		}
	}
	if (cur_pic_num == 0)
		return;
	for (; i < MAX_REF_PIC_NUM; i++) {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) {
			if (hevc->mmu_enable  && ((dw_mode & 0x10) == 0))
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				hevc->m_PIC[cur_pic_num-1]->header_adr>>5);
			else
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				hevc->m_PIC[cur_pic_num-1]->mc_y_adr >> 5);
#ifndef LOSLESS_COMPRESS_MODE
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				hevc->m_PIC[cur_pic_num-1]->mc_u_v_adr >> 5);
#endif
		} else {
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
				hevc->m_PIC[cur_pic_num-1]->mc_y_adr|
				(hevc->m_PIC[cur_pic_num-1]->mc_canvas_y<<8)
				| 0x1);
#ifndef LOSLESS_COMPRESS_MODE
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
				hevc->m_PIC[cur_pic_num-1]->mc_u_v_adr|
				(hevc->m_PIC[cur_pic_num-1]->mc_canvas_u_v<<8)
				| 0x1);
#endif
		}
	}

	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);

	/* Zero out canvas registers in IPP -- avoid simulation X */
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			   (0 << 8) | (0 << 1) | 1);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);

#ifdef LOSLESS_COMPRESS_MODE
	if ((dw_mode & 0x10) == 0)
		init_decode_head_hw(hevc);
#endif

}


static void dump_pic_list(struct hevc_state_s *hevc)
{
	int i;
	struct PIC_s *pic;

	hevc_print(hevc, 0,
		"pic_list_init_flag is %d\r\n", hevc->pic_list_init_flag);
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		hevc_print_cont(hevc, 0,
		"index %d buf_idx %d mv_idx %d decode_idx:%d,	POC:%d,	referenced:%d,	",
		 pic->index, pic->BUF_index,
#ifndef MV_USE_FIXED_BUF
		pic->mv_buf_index,
#else
		 -1,
#endif
		 pic->decode_idx, pic->POC, pic->referenced);
		hevc_print_cont(hevc, 0,
			"num_reorder_pic:%d, output_mark:%d, w/h %d,%d",
				pic->num_reorder_pic, pic->output_mark,
				pic->width, pic->height);
		hevc_print_cont(hevc, 0,
			"output_ready:%d, mv_wr_start %x vf_ref %d\n",
				pic->output_ready, pic->mpred_mv_wr_start_addr,
				pic->vf_ref);
	}
}

static void clear_referenced_flag(struct hevc_state_s *hevc)
{
	int i;
	struct PIC_s *pic;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		if (pic->referenced) {
			pic->referenced = 0;
			put_mv_buf(hevc, pic);
		}
	}
}

static void clear_poc_flag(struct hevc_state_s *hevc)
{
	int i;
	struct PIC_s *pic;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		pic->POC = INVALID_POC;
	}
}

static struct PIC_s *output_pic(struct hevc_state_s *hevc,
		unsigned char flush_flag)
{
	int num_pic_not_yet_display = 0;
	int i;
	struct PIC_s *pic;
	struct PIC_s *pic_display = NULL;

	if (hevc->i_only & 0x4) {
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic == NULL ||
				(pic->index == -1) ||
				(pic->BUF_index == -1) ||
				(pic->POC == INVALID_POC))
				continue;
			if (pic->output_mark) {
				if (pic_display) {
					if (pic->decode_idx <
						pic_display->decode_idx)
						pic_display = pic;

				} else
					pic_display = pic;

			}
		}
		if (pic_display) {
			pic_display->output_mark = 0;
			pic_display->recon_mark = 0;
			pic_display->output_ready = 1;
			pic_display->referenced = 0;
			put_mv_buf(hevc, pic_display);
		}
	} else {
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic == NULL ||
				(pic->index == -1) ||
				(pic->BUF_index == -1) ||
				(pic->POC == INVALID_POC))
				continue;
			if (pic->output_mark)
				num_pic_not_yet_display++;
			if (pic->slice_type == 2 &&
				hevc->vf_pre_count == 0 &&
				fast_output_enable & 0x1) {
				/*fast output for first I picture*/
				pic->num_reorder_pic = 0;
				hevc_print(hevc, 0, "VH265: output first frame\n");
			}
		}

		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic == NULL ||
				(pic->index == -1) ||
				(pic->BUF_index == -1) ||
				(pic->POC == INVALID_POC))
				continue;
			if (pic->output_mark) {
				if (pic_display) {
					if (pic->POC < pic_display->POC)
						pic_display = pic;
					else if ((pic->POC == pic_display->POC)
						&& (pic->decode_idx <
							pic_display->
							decode_idx))
								pic_display
								= pic;
				} else
					pic_display = pic;
			}
		}
		if (pic_display) {
			if ((num_pic_not_yet_display >
				pic_display->num_reorder_pic)
				|| flush_flag) {
				pic_display->output_mark = 0;
				pic_display->recon_mark = 0;
				pic_display->output_ready = 1;
			} else if (num_pic_not_yet_display >=
				(MAX_REF_PIC_NUM - 1)) {
				pic_display->output_mark = 0;
				pic_display->recon_mark = 0;
				pic_display->output_ready = 1;
				hevc_print(hevc, 0,
					"Warning, num_reorder_pic %d is byeond buf num\n",
					pic_display->num_reorder_pic);
			} else
				pic_display = NULL;
		}
	}
	if (pic_display && (hevc->vf_pre_count == 1) && (hevc->first_pic_flag == 1)) {
		pic_display = NULL;
		hevc->first_pic_flag = 0;
	}
	return pic_display;
}

static int config_mc_buffer(struct hevc_state_s *hevc, struct PIC_s *cur_pic)
{
	int i;
	struct PIC_s *pic;

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"config_mc_buffer entered .....\n");
	if (cur_pic->slice_type != 2) {	/* P and B pic */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				   (0 << 8) | (0 << 1) | 1);
		for (i = 0; i < cur_pic->RefNum_L0; i++) {
			pic =
				get_ref_pic_by_POC(hevc,
						cur_pic->
						m_aiRefPOCList0[cur_pic->
						slice_idx][i]);
			if (pic) {
				if ((pic->width != hevc->pic_w) ||
					(pic->height != hevc->pic_h)) {
					hevc_print(hevc, 0,
						"%s: Wrong reference pic (poc %d) width/height %d/%d\n",
						__func__, pic->POC,
						pic->width, pic->height);
					cur_pic->error_mark = 1;
				}
				if (pic->error_mark && (ref_frame_mark_flag[hevc->index]))
					cur_pic->error_mark = 1;
				WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
						(pic->mc_canvas_u_v << 16)
						| (pic->mc_canvas_u_v
							<< 8) |
						   pic->mc_canvas_y);
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print_cont(hevc, 0,
					"refid %x mc_canvas_u_v %x",
					 i, pic->mc_canvas_u_v);
					hevc_print_cont(hevc, 0,
						" mc_canvas_y %x\n",
					 pic->mc_canvas_y);
				}
			} else
				cur_pic->error_mark = 1;

			if (pic == NULL || pic->error_mark) {
				hevc_print(hevc, 0,
				"Error %s, %dth poc (%d) %s",
				 __func__, i,
				 cur_pic->m_aiRefPOCList0[cur_pic->
				 slice_idx][i],
				 pic ? "has error" :
				 "not in list0");
			}
		}
	}
	if (cur_pic->slice_type == 0) {	/* B pic */
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print(hevc, 0,
				"config_mc_buffer RefNum_L1\n");
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				   (16 << 8) | (0 << 1) | 1);

		for (i = 0; i < cur_pic->RefNum_L1; i++) {
			pic =
				get_ref_pic_by_POC(hevc,
						cur_pic->
						m_aiRefPOCList1[cur_pic->
						slice_idx][i]);
			if (pic) {
				if ((pic->width != hevc->pic_w) ||
					(pic->height != hevc->pic_h)) {
					hevc_print(hevc, 0,
						"%s: Wrong reference pic (poc %d) width/height %d/%d\n",
						__func__, pic->POC,
						pic->width, pic->height);
					cur_pic->error_mark = 1;
				}

				if (pic->error_mark && (ref_frame_mark_flag[hevc->index]))
					cur_pic->error_mark = 1;
				WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
						   (pic->mc_canvas_u_v << 16)
						   | (pic->mc_canvas_u_v
								   << 8) |
						   pic->mc_canvas_y);
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print_cont(hevc, 0,
					"refid %x mc_canvas_u_v %x",
					 i, pic->mc_canvas_u_v);
					hevc_print_cont(hevc, 0,
						" mc_canvas_y %x\n",
					 pic->mc_canvas_y);
				}
			} else
				cur_pic->error_mark = 1;

			if (pic == NULL || pic->error_mark) {
				hevc_print(hevc, 0,
				"Error %s, %dth poc (%d) %s",
				 __func__, i,
				 cur_pic->m_aiRefPOCList1[cur_pic->
				 slice_idx][i],
				 pic ? "has error" :
				 "not in list1");
			}
		}
	}
	return 0;
}

static void apply_ref_pic_set(struct hevc_state_s *hevc, int cur_poc,
							  union param_u *params)
{
	int ii, i;
	int poc_tmp;
	struct PIC_s *pic;
	unsigned char is_referenced;
	/* hevc_print(hevc, 0,
	"%s cur_poc %d\n", __func__, cur_poc); */

	for (ii = 0; ii < MAX_REF_PIC_NUM; ii++) {
		pic = hevc->m_PIC[ii];
		if (pic == NULL ||
			pic->index == -1 ||
			pic->BUF_index == -1
			)
			continue;

		if ((pic->referenced == 0 || pic->POC == cur_poc))
			continue;
		is_referenced = 0;
		for (i = 0; i < 16; i++) {
			int delt;

			if (params->p.CUR_RPS[i] & 0x8000)
				break;
			delt =
				params->p.CUR_RPS[i] &
				((1 << (RPS_USED_BIT - 1)) - 1);
			if (params->p.CUR_RPS[i] & (1 << (RPS_USED_BIT - 1))) {
				poc_tmp =
					cur_poc - ((1 << (RPS_USED_BIT - 1)) -
							   delt);
			} else
				poc_tmp = cur_poc + delt;
			if (poc_tmp == pic->POC) {
				is_referenced = 1;
				/* hevc_print(hevc, 0, "i is %d\n", i); */
				break;
			}
		}
		if (is_referenced == 0) {
			pic->referenced = 0;
			put_mv_buf(hevc, pic);
			/* hevc_print(hevc, 0,
			"set poc %d reference to 0\n", pic->POC); */
		}
	}

}

static void set_ref_pic_list(struct hevc_state_s *hevc, union param_u *params)
{
	struct PIC_s *pic = hevc->cur_pic;
	int i, rIdx;
	int num_neg = 0;
	int num_pos = 0;
	int total_num;
	int num_ref_idx_l0_active =
		(params->p.num_ref_idx_l0_active >
		 MAX_REF_ACTIVE) ? MAX_REF_ACTIVE :
		params->p.num_ref_idx_l0_active;
	int num_ref_idx_l1_active =
		(params->p.num_ref_idx_l1_active >
		 MAX_REF_ACTIVE) ? MAX_REF_ACTIVE :
		params->p.num_ref_idx_l1_active;

	int RefPicSetStCurr0[16];
	int RefPicSetStCurr1[16];

	for (i = 0; i < 16; i++) {
		RefPicSetStCurr0[i] = 0;
		RefPicSetStCurr1[i] = 0;
		pic->m_aiRefPOCList0[pic->slice_idx][i] = 0;
		pic->m_aiRefPOCList1[pic->slice_idx][i] = 0;
	}
	for (i = 0; i < 16; i++) {
		if (params->p.CUR_RPS[i] & 0x8000)
			break;
		if ((params->p.CUR_RPS[i] >> RPS_USED_BIT) & 1) {
			int delt =
				params->p.CUR_RPS[i] &
				((1 << (RPS_USED_BIT - 1)) - 1);

			if ((params->p.CUR_RPS[i] >> (RPS_USED_BIT - 1)) & 1) {
				RefPicSetStCurr0[num_neg] =
					pic->POC - ((1 << (RPS_USED_BIT - 1)) -
								delt);
				/* hevc_print(hevc, 0,
				 *	"RefPicSetStCurr0 %x %x %x\n",
				 *   RefPicSetStCurr0[num_neg], pic->POC,
				 *   (0x800-(params[i]&0x7ff)));
				 */
				num_neg++;
			} else {
				RefPicSetStCurr1[num_pos] = pic->POC + delt;
				/* hevc_print(hevc, 0,
				 *	"RefPicSetStCurr1 %d\n",
				 *   RefPicSetStCurr1[num_pos]);
				 */
				num_pos++;
			}
		}
	}
	total_num = num_neg + num_pos;
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
		"%s: curpoc %d slice_type %d, total %d ",
		 __func__, pic->POC, params->p.slice_type, total_num);
		hevc_print_cont(hevc, 0,
			"num_neg %d num_list0 %d num_list1 %d\n",
		 num_neg, num_ref_idx_l0_active, num_ref_idx_l1_active);
	}

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
		"HEVC Stream buf start ");
		hevc_print_cont(hevc, 0,
		"%x end %x wr %x rd %x lev %x ctl %x intctl %x\n",
		 READ_VREG(HEVC_STREAM_START_ADDR),
		 READ_VREG(HEVC_STREAM_END_ADDR),
		 READ_VREG(HEVC_STREAM_WR_PTR),
		 READ_VREG(HEVC_STREAM_RD_PTR),
		 READ_VREG(HEVC_STREAM_LEVEL),
		 READ_VREG(HEVC_STREAM_FIFO_CTL),
		 READ_VREG(HEVC_PARSER_INT_CONTROL));
	}

	if (total_num > 0) {
		if (params->p.modification_flag & 0x1) {
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print(hevc, 0, "ref0 POC (modification):");
			for (rIdx = 0; rIdx < num_ref_idx_l0_active; rIdx++) {
				int cIdx = params->p.modification_list[rIdx];

				pic->m_aiRefPOCList0[pic->slice_idx][rIdx] =
					cIdx >=
					num_neg ? RefPicSetStCurr1[cIdx -
					num_neg] :
					RefPicSetStCurr0[cIdx];
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print_cont(hevc, 0, "%d ",
						   pic->m_aiRefPOCList0[pic->
						   slice_idx]
						   [rIdx]);
				}
			}
		} else {
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print(hevc, 0, "ref0 POC:");
			for (rIdx = 0; rIdx < num_ref_idx_l0_active; rIdx++) {
				int cIdx = rIdx % total_num;

				pic->m_aiRefPOCList0[pic->slice_idx][rIdx] =
					cIdx >=
					num_neg ? RefPicSetStCurr1[cIdx -
					num_neg] :
					RefPicSetStCurr0[cIdx];
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print_cont(hevc, 0, "%d ",
						   pic->m_aiRefPOCList0[pic->
						   slice_idx]
						   [rIdx]);
				}
			}
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print_cont(hevc, 0, "\n");
		if (params->p.slice_type == B_SLICE) {
			if (params->p.modification_flag & 0x2) {
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
					hevc_print(hevc, 0,
						"ref1 POC (modification):");
				for (rIdx = 0; rIdx < num_ref_idx_l1_active;
					 rIdx++) {
					int cIdx;

					if (params->p.modification_flag & 0x1) {
						cIdx =
							params->p.
							modification_list
							[num_ref_idx_l0_active +
							 rIdx];
					} else {
						cIdx =
							params->p.
							modification_list[rIdx];
					}
					pic->m_aiRefPOCList1[pic->
						slice_idx][rIdx] =
						cIdx >=
						num_pos ?
						RefPicSetStCurr0[cIdx -	num_pos]
						: RefPicSetStCurr1[cIdx];
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print_cont(hevc, 0, "%d ",
							   pic->
							   m_aiRefPOCList1[pic->
							   slice_idx]
							   [rIdx]);
					}
				}
			} else {
				if (get_dbg_flag(hevc) &
					H265_DEBUG_BUFMGR)
					hevc_print(hevc, 0, "ref1 POC:");
				for (rIdx = 0; rIdx < num_ref_idx_l1_active;
					 rIdx++) {
					int cIdx = rIdx % total_num;

					pic->m_aiRefPOCList1[pic->
						slice_idx][rIdx] =
						cIdx >=
						num_pos ?
						RefPicSetStCurr0[cIdx -
						num_pos]
						: RefPicSetStCurr1[cIdx];
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print_cont(hevc, 0, "%d ",
							   pic->
							   m_aiRefPOCList1[pic->
							   slice_idx]
							   [rIdx]);
					}
				}
			}
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print_cont(hevc, 0, "\n");
		}
	}
	/*set m_PIC */
	pic->slice_type = (params->p.slice_type == I_SLICE) ? 2 :
		(params->p.slice_type == P_SLICE) ? 1 :
		(params->p.slice_type == B_SLICE) ? 0 : 3;
	pic->RefNum_L0 = num_ref_idx_l0_active;
	pic->RefNum_L1 = num_ref_idx_l1_active;
}

static void update_tile_info(struct hevc_state_s *hevc, int pic_width_cu,
		int pic_height_cu, int sao_mem_unit,
		union param_u *params)
{
	int i, j;
	int start_cu_x, start_cu_y;
	int sao_vb_size = (sao_mem_unit + (2 << 4)) * pic_height_cu;
	int sao_abv_size = sao_mem_unit * pic_width_cu;
#ifdef DETREFILL_ENABLE
	if (hevc->is_swap && get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		int tmpRefillLcuSize = 1 <<
			(params->p.log2_min_coding_block_size_minus3 +
			3 + params->p.log2_diff_max_min_coding_block_size);
		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
			"%x, %x, %x, %x\n",
			params->p.slice_segment_address,
			params->p.bit_depth,
			params->p.tiles_enabled_flag,
			tmpRefillLcuSize);
		if (params->p.slice_segment_address == 0 &&
			params->p.bit_depth != 0 &&
			(params->p.tiles_enabled_flag & 1) &&
			tmpRefillLcuSize == 64)
			hevc->delrefill_check = 1;
		else
			hevc->delrefill_check = 0;
	}
#endif

	hevc->tile_enabled = params->p.tiles_enabled_flag & 1;
	if (params->p.tiles_enabled_flag & 1) {
		hevc->num_tile_col = params->p.num_tile_columns_minus1 + 1;
		hevc->num_tile_row = params->p.num_tile_rows_minus1 + 1;

		if (hevc->num_tile_row > MAX_TILE_ROW_NUM
			|| hevc->num_tile_row <= 0) {
			hevc->num_tile_row = 1;
			hevc_print(hevc, 0,
				"%s: num_tile_rows_minus1 (%d) error!!\n",
				   __func__, params->p.num_tile_rows_minus1);
		}
		if (hevc->num_tile_col > MAX_TILE_COL_NUM
			|| hevc->num_tile_col <= 0) {
			hevc->num_tile_col = 1;
			hevc_print(hevc, 0,
				"%s: num_tile_columns_minus1 (%d) error!!\n",
				   __func__, params->p.num_tile_columns_minus1);
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
			"%s pic_w_cu %d pic_h_cu %d tile_enabled ",
			 __func__, pic_width_cu, pic_height_cu);
			hevc_print_cont(hevc, 0,
				"num_tile_col %d num_tile_row %d:\n",
			 hevc->num_tile_col, hevc->num_tile_row);
		}

		if (params->p.tiles_enabled_flag & 2) {	/* uniform flag */
			int w = pic_width_cu / hevc->num_tile_col;
			int h = pic_height_cu / hevc->num_tile_row;

			start_cu_y = 0;
			for (i = 0; i < hevc->num_tile_row; i++) {
				start_cu_x = 0;
				for (j = 0; j < hevc->num_tile_col; j++) {
					if (j == (hevc->num_tile_col - 1)) {
						hevc->m_tile[i][j].width =
							pic_width_cu -
							start_cu_x;
					} else
						hevc->m_tile[i][j].width = w;
					if (i == (hevc->num_tile_row - 1)) {
						hevc->m_tile[i][j].height =
							pic_height_cu -
							start_cu_y;
					} else
						hevc->m_tile[i][j].height = h;
					hevc->m_tile[i][j].start_cu_x
					    = start_cu_x;
					hevc->m_tile[i][j].start_cu_y
					    = start_cu_y;
					hevc->m_tile[i][j].sao_vb_start_addr =
						hevc->work_space_buf->sao_vb.
						buf_start + j * sao_vb_size;
					hevc->m_tile[i][j].sao_abv_start_addr =
						hevc->work_space_buf->sao_abv.
						buf_start + i * sao_abv_size;
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print_cont(hevc, 0,
						"{y=%d, x=%d w %d h %d ",
						 i, j, hevc->m_tile[i][j].width,
						 hevc->m_tile[i][j].height);
						hevc_print_cont(hevc, 0,
						"start_x %d start_y %d ",
						 hevc->m_tile[i][j].start_cu_x,
						 hevc->m_tile[i][j].start_cu_y);
						hevc_print_cont(hevc, 0,
						"sao_vb_start 0x%x ",
						 hevc->m_tile[i][j].
						 sao_vb_start_addr);
						hevc_print_cont(hevc, 0,
						"sao_abv_start 0x%x}\n",
						 hevc->m_tile[i][j].
						 sao_abv_start_addr);
					}
					start_cu_x += hevc->m_tile[i][j].width;

				}
				start_cu_y += hevc->m_tile[i][0].height;
			}
		} else {
			start_cu_y = 0;
			for (i = 0; i < hevc->num_tile_row; i++) {
				start_cu_x = 0;
				for (j = 0; j < hevc->num_tile_col; j++) {
					if (j == (hevc->num_tile_col - 1)) {
						hevc->m_tile[i][j].width =
							pic_width_cu -
							start_cu_x;
					} else {
						hevc->m_tile[i][j].width =
							params->p.tile_width[j];
					}
					if (i == (hevc->num_tile_row - 1)) {
						hevc->m_tile[i][j].height =
							pic_height_cu -
							start_cu_y;
					} else {
						hevc->m_tile[i][j].height =
							params->
							p.tile_height[i];
					}
					hevc->m_tile[i][j].start_cu_x
					    = start_cu_x;
					hevc->m_tile[i][j].start_cu_y
					    = start_cu_y;
					hevc->m_tile[i][j].sao_vb_start_addr =
						hevc->work_space_buf->sao_vb.
						buf_start + j * sao_vb_size;
					hevc->m_tile[i][j].sao_abv_start_addr =
						hevc->work_space_buf->sao_abv.
						buf_start + i * sao_abv_size;
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print_cont(hevc, 0,
						"{y=%d, x=%d w %d h %d ",
						 i, j, hevc->m_tile[i][j].width,
						 hevc->m_tile[i][j].height);
						hevc_print_cont(hevc, 0,
						"start_x %d start_y %d ",
						 hevc->m_tile[i][j].start_cu_x,
						 hevc->m_tile[i][j].start_cu_y);
						hevc_print_cont(hevc, 0,
						"sao_vb_start 0x%x ",
						 hevc->m_tile[i][j].
						 sao_vb_start_addr);
						hevc_print_cont(hevc, 0,
						"sao_abv_start 0x%x}\n",
						 hevc->m_tile[i][j].
						 sao_abv_start_addr);

					}
					start_cu_x += hevc->m_tile[i][j].width;
				}
				start_cu_y += hevc->m_tile[i][0].height;
			}
		}
	} else {
		hevc->num_tile_col = 1;
		hevc->num_tile_row = 1;
		hevc->m_tile[0][0].width = pic_width_cu;
		hevc->m_tile[0][0].height = pic_height_cu;
		hevc->m_tile[0][0].start_cu_x = 0;
		hevc->m_tile[0][0].start_cu_y = 0;
		hevc->m_tile[0][0].sao_vb_start_addr =
			hevc->work_space_buf->sao_vb.buf_start;
		hevc->m_tile[0][0].sao_abv_start_addr =
			hevc->work_space_buf->sao_abv.buf_start;
	}
}

static int get_tile_index(struct hevc_state_s *hevc, int cu_adr,
						  int pic_width_lcu)
{
	int cu_x;
	int cu_y;
	int tile_x = 0;
	int tile_y = 0;
	int i;

	if (pic_width_lcu == 0) {
		if (get_dbg_flag(hevc)) {
			hevc_print(hevc, 0,
			"%s Error, pic_width_lcu is 0, pic_w %d, pic_h %d\n",
			 __func__, hevc->pic_w, hevc->pic_h);
		}
		return -1;
	}
	cu_x = cu_adr % pic_width_lcu;
	cu_y = cu_adr / pic_width_lcu;
	if (hevc->tile_enabled) {
		for (i = 0; i < hevc->num_tile_col; i++) {
			if (cu_x >= hevc->m_tile[0][i].start_cu_x)
				tile_x = i;
			else
				break;
		}
		for (i = 0; i < hevc->num_tile_row; i++) {
			if (cu_y >= hevc->m_tile[i][0].start_cu_y)
				tile_y = i;
			else
				break;
		}
	}
	return (tile_x) | (tile_y << 8);
}

static void print_scratch_error(int error_num)
{
#if 0
	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0,
		" ERROR : HEVC_ASSIST_SCRATCH_TEST Error : %d\n",
			   error_num);
	}
#endif
}

static void hevc_config_work_space_hw(struct hevc_state_s *hevc)
{
	struct BuffInfo_s *buf_spec = hevc->work_space_buf;

	if (get_dbg_flag(hevc))
		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
			"%s %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			__func__,
			buf_spec->ipp.buf_start,
			buf_spec->start_adr,
			buf_spec->short_term_rps.buf_start,
			buf_spec->vps.buf_start,
			buf_spec->sps.buf_start,
			buf_spec->pps.buf_start,
			buf_spec->sao_up.buf_start,
			buf_spec->swap_buf.buf_start,
			buf_spec->swap_buf2.buf_start,
			buf_spec->scalelut.buf_start,
			buf_spec->dblk_para.buf_start,
			buf_spec->dblk_data.buf_start,
			buf_spec->dblk_data2.buf_start);
	WRITE_VREG(HEVCD_IPP_LINEBUFF_BASE, buf_spec->ipp.buf_start);
	if ((get_dbg_flag(hevc) & H265_DEBUG_SEND_PARAM_WITH_REG) == 0)
		WRITE_VREG(HEVC_RPM_BUFFER, (u32)hevc->rpm_phy_addr);
	WRITE_VREG(HEVC_SHORT_TERM_RPS, buf_spec->short_term_rps.buf_start);
	WRITE_VREG(HEVC_VPS_BUFFER, buf_spec->vps.buf_start);
	WRITE_VREG(HEVC_SPS_BUFFER, buf_spec->sps.buf_start);
	WRITE_VREG(HEVC_PPS_BUFFER, buf_spec->pps.buf_start);
	WRITE_VREG(HEVC_SAO_UP, buf_spec->sao_up.buf_start);
	if (hevc->mmu_enable) {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
			WRITE_VREG(HEVC_ASSIST_MMU_MAP_ADDR, hevc->frame_mmu_map_phy_addr);
			hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
				"write HEVC_ASSIST_MMU_MAP_ADDR\n");
		} else
			WRITE_VREG(H265_MMU_MAP_BUFFER, hevc->frame_mmu_map_phy_addr);
	} /*else
		WRITE_VREG(HEVC_STREAM_SWAP_BUFFER,
				buf_spec->swap_buf.buf_start);
	WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, buf_spec->swap_buf2.buf_start);*/
	WRITE_VREG(HEVC_SCALELUT, buf_spec->scalelut.buf_start);
	/* cfg_p_addr */
	WRITE_VREG(HEVC_DBLK_CFG4, buf_spec->dblk_para.buf_start);
	/* cfg_d_addr */
	WRITE_VREG(HEVC_DBLK_CFG5, buf_spec->dblk_data.buf_start);

	WRITE_VREG(HEVC_DBLK_CFGE, buf_spec->dblk_data2.buf_start);

	WRITE_VREG(LMEM_DUMP_ADR, (u32)hevc->lmem_phy_addr);
}

static void parser_cmd_write(void)
{
	u32 i;
	const unsigned short parser_cmd[PARSER_CMD_NUMBER] = {
		0x0401, 0x8401, 0x0800, 0x0402, 0x9002, 0x1423,
		0x8CC3, 0x1423, 0x8804, 0x9825, 0x0800, 0x04FE,
		0x8406, 0x8411, 0x1800, 0x8408, 0x8409, 0x8C2A,
		0x9C2B, 0x1C00, 0x840F, 0x8407, 0x8000, 0x8408,
		0x2000, 0xA800, 0x8410, 0x04DE, 0x840C, 0x840D,
		0xAC00, 0xA000, 0x08C0, 0x08E0, 0xA40E, 0xFC00,
		0x7C00
	};
	for (i = 0; i < PARSER_CMD_NUMBER; i++)
		WRITE_VREG(HEVC_PARSER_CMD_WRITE, parser_cmd[i]);
}

static void hevc_init_decoder_hw(struct hevc_state_s *hevc,
	int decode_pic_begin, int decode_pic_num)
{
	unsigned int data32;
	int i;
#if 0
	if (get_cpu_major_id() >= MESON_CPU_MAJOR_ID_G12A) {
		/* Set MCR fetch priorities*/
		data32 = 0x1 | (0x1 << 2) | (0x1 <<3) |
			(24 << 4) | (32 << 11) | (24 << 18) | (32 << 25);
		WRITE_VREG(HEVCD_MPP_DECOMP_AXIURG_CTL, data32);
	}
#endif
#if 1
	/* m8baby test1902 */
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"%s\n", __func__);
	data32 = READ_VREG(HEVC_PARSER_VERSION);
	if (data32 != 0x00010001) {
		print_scratch_error(25);
		return;
	}
	WRITE_VREG(HEVC_PARSER_VERSION, 0x5a5a55aa);
	data32 = READ_VREG(HEVC_PARSER_VERSION);
	if (data32 != 0x5a5a55aa) {
		print_scratch_error(26);
		return;
	}
#if 0
	/* test Parser Reset */
	/* reset iqit to start mem init again */
	WRITE_VREG(DOS_SW_RESET3, (1 << 14) |
			   (1 << 3)	/* reset_whole parser */
			  );
	WRITE_VREG(DOS_SW_RESET3, 0);	/* clear reset_whole parser */
	data32 = READ_VREG(HEVC_PARSER_VERSION);
	if (data32 != 0x00010001)
		hevc_print(hevc, 0,
		"Test Parser Fatal Error\n");
#endif
	/* reset iqit to start mem init again */
	WRITE_VREG(DOS_SW_RESET3, (1 << 14)
			  );
	CLEAR_VREG_MASK(HEVC_CABAC_CONTROL, 1);
	CLEAR_VREG_MASK(HEVC_PARSER_CORE_CONTROL, 1);

#endif
	if (!hevc->m_ins_flag) {
		data32 = READ_VREG(HEVC_STREAM_CONTROL);
		data32 = data32 | (1 << 0);      /* stream_fetch_enable */
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A)
			data32 |= (0xf << 25); /*arwlen_axi_max*/
		WRITE_VREG(HEVC_STREAM_CONTROL, data32);
	}
	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x00000100) {
		print_scratch_error(29);
		return;
	}
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x00000300) {
		print_scratch_error(30);
		return;
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x12345678);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x9abcdef0);
	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x12345678) {
		print_scratch_error(31);
		return;
	}
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x9abcdef0) {
		print_scratch_error(32);
		return;
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x00000100);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x00000300);

	data32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
	data32 &= 0x03ffffff;
	data32 = data32 | (3 << 29) | (2 << 26) | (1 << 24)
			 |	/* stream_buffer_empty_int_amrisc_enable */
			 (1 << 22) |	/* stream_fifo_empty_int_amrisc_enable*/
			 (1 << 7) |	/* dec_done_int_cpu_enable */
			 (1 << 4) |	/* startcode_found_int_cpu_enable */
			 (0 << 3) |	/* startcode_found_int_amrisc_enable */
			 (1 << 0)	/* parser_int_enable */
			 ;
	WRITE_VREG(HEVC_PARSER_INT_CONTROL, data32);

	data32 = READ_VREG(HEVC_SHIFT_STATUS);
	data32 = data32 | (1 << 1) |	/* emulation_check_on */
			 (1 << 0)		/* startcode_check_on */
			 ;
	WRITE_VREG(HEVC_SHIFT_STATUS, data32);

	WRITE_VREG(HEVC_SHIFT_CONTROL, (3 << 6) |/* sft_valid_wr_position */
			   (2 << 4) |	/* emulate_code_length_sub_1 */
			   (2 << 1) |	/* start_code_length_sub_1 */
			   (1 << 0)	/* stream_shift_enable */
			  );

	WRITE_VREG(HEVC_CABAC_CONTROL, (1 << 0)	/* cabac_enable */
			  );
	/* hevc_parser_core_clk_en */
	WRITE_VREG(HEVC_PARSER_CORE_CONTROL, (1 << 0)
			  );

	WRITE_VREG(HEVC_DEC_STATUS_REG, 0);

	/* Initial IQIT_SCALELUT memory -- just to avoid X in simulation */
	WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0);	/* cfg_p_addr */
	for (i = 0; i < 1024; i++)
		WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, 0);

#ifdef ENABLE_SWAP_TEST
	WRITE_VREG(HEVC_STREAM_SWAP_TEST, 100);
#endif

	/*WRITE_VREG(HEVC_DECODE_PIC_BEGIN_REG, 0);*/
	/*WRITE_VREG(HEVC_DECODE_PIC_NUM_REG, 0xffffffff);*/
	WRITE_VREG(HEVC_DECODE_SIZE, 0);
	/*WRITE_VREG(HEVC_DECODE_COUNT, 0);*/
	/* Send parser_cmd */
	WRITE_VREG(HEVC_PARSER_CMD_WRITE, (1 << 16) | (0 << 0));

	parser_cmd_write();

	WRITE_VREG(HEVC_PARSER_CMD_SKIP_0, PARSER_CMD_SKIP_CFG_0);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_1, PARSER_CMD_SKIP_CFG_1);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_2, PARSER_CMD_SKIP_CFG_2);

	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
			   /* (1 << 8) | // sao_sw_pred_enable */
			   (1 << 5) |	/* parser_sao_if_en */
			   (1 << 2) |	/* parser_mpred_if_en */
			   (1 << 0)	/* parser_scaler_if_en */
			  );

	/* Changed to Start MPRED in microcode */
	/*
	 *   hevc_print(hevc, 0, "[test.c] Start MPRED\n");
	 *   WRITE_VREG(HEVC_MPRED_INT_STATUS,
	 *   (1<<31)
	 *   );
	 */

	WRITE_VREG(HEVCD_IPP_TOP_CNTL, (0 << 1) |	/* enable ipp */
			   (1 << 0)	/* software reset ipp and mpp */
			  );
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, (1 << 1) |	/* enable ipp */
			   (0 << 0)	/* software reset ipp and mpp */
			  );

	if (get_double_write_mode(hevc) & 0x10)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,
			0x1 << 31  /*/Enable NV21 reference read mode for MC*/
			);

}

static void decoder_hw_reset(void)
{
	int i;
	unsigned int data32;
	/* reset iqit to start mem init again */
	WRITE_VREG(DOS_SW_RESET3, (1 << 14)
			  );
	CLEAR_VREG_MASK(HEVC_CABAC_CONTROL, 1);
	CLEAR_VREG_MASK(HEVC_PARSER_CORE_CONTROL, 1);

	data32 = READ_VREG(HEVC_STREAM_CONTROL);
	data32 = data32 | (1 << 0)	/* stream_fetch_enable */
			 ;
	WRITE_VREG(HEVC_STREAM_CONTROL, data32);

	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x00000100) {
		print_scratch_error(29);
		return;
	}
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x00000300) {
		print_scratch_error(30);
		return;
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x12345678);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x9abcdef0);
	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x12345678) {
		print_scratch_error(31);
		return;
	}
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x9abcdef0) {
		print_scratch_error(32);
		return;
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x00000100);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x00000300);

	data32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
	data32 &= 0x03ffffff;
	data32 = data32 | (3 << 29) | (2 << 26) | (1 << 24)
			 |	/* stream_buffer_empty_int_amrisc_enable */
			 (1 << 22) |	/*stream_fifo_empty_int_amrisc_enable */
			 (1 << 7) |	/* dec_done_int_cpu_enable */
			 (1 << 4) |	/* startcode_found_int_cpu_enable */
			 (0 << 3) |	/* startcode_found_int_amrisc_enable */
			 (1 << 0)	/* parser_int_enable */
			 ;
	WRITE_VREG(HEVC_PARSER_INT_CONTROL, data32);

	data32 = READ_VREG(HEVC_SHIFT_STATUS);
	data32 = data32 | (1 << 1) |	/* emulation_check_on */
			 (1 << 0)		/* startcode_check_on */
			 ;
	WRITE_VREG(HEVC_SHIFT_STATUS, data32);

	WRITE_VREG(HEVC_SHIFT_CONTROL, (3 << 6) |/* sft_valid_wr_position */
			   (2 << 4) |	/* emulate_code_length_sub_1 */
			   (2 << 1) |	/* start_code_length_sub_1 */
			   (1 << 0)	/* stream_shift_enable */
			  );

	WRITE_VREG(HEVC_CABAC_CONTROL, (1 << 0)	/* cabac_enable */
			  );
	/* hevc_parser_core_clk_en */
	WRITE_VREG(HEVC_PARSER_CORE_CONTROL, (1 << 0)
			  );

	/* Initial IQIT_SCALELUT memory -- just to avoid X in simulation */
	WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0);	/* cfg_p_addr */
	for (i = 0; i < 1024; i++)
		WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, 0);

	/* Send parser_cmd */
	WRITE_VREG(HEVC_PARSER_CMD_WRITE, (1 << 16) | (0 << 0));

	parser_cmd_write();

	WRITE_VREG(HEVC_PARSER_CMD_SKIP_0, PARSER_CMD_SKIP_CFG_0);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_1, PARSER_CMD_SKIP_CFG_1);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_2, PARSER_CMD_SKIP_CFG_2);

	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
			   /* (1 << 8) | // sao_sw_pred_enable */
			   (1 << 5) |	/* parser_sao_if_en */
			   (1 << 2) |	/* parser_mpred_if_en */
			   (1 << 0)	/* parser_scaler_if_en */
			  );

	WRITE_VREG(HEVCD_IPP_TOP_CNTL, (0 << 1) |	/* enable ipp */
			   (1 << 0)	/* software reset ipp and mpp */
			  );
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, (1 << 1) |	/* enable ipp */
			   (0 << 0)	/* software reset ipp and mpp */
			  );
}

#ifdef CONFIG_HEVC_CLK_FORCED_ON
static void config_hevc_clk_forced_on(void)
{
	unsigned int rdata32;
	/* IQIT */
	rdata32 = READ_VREG(HEVC_IQIT_CLK_RST_CTRL);
	WRITE_VREG(HEVC_IQIT_CLK_RST_CTRL, rdata32 | (0x1 << 2));

	/* DBLK */
	rdata32 = READ_VREG(HEVC_DBLK_CFG0);
	WRITE_VREG(HEVC_DBLK_CFG0, rdata32 | (0x1 << 2));

	/* SAO */
	rdata32 = READ_VREG(HEVC_SAO_CTRL1);
	WRITE_VREG(HEVC_SAO_CTRL1, rdata32 | (0x1 << 2));

	/* MPRED */
	rdata32 = READ_VREG(HEVC_MPRED_CTRL1);
	WRITE_VREG(HEVC_MPRED_CTRL1, rdata32 | (0x1 << 24));

	/* PARSER */
	rdata32 = READ_VREG(HEVC_STREAM_CONTROL);
	WRITE_VREG(HEVC_STREAM_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_SHIFT_CONTROL);
	WRITE_VREG(HEVC_SHIFT_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_CABAC_CONTROL);
	WRITE_VREG(HEVC_CABAC_CONTROL, rdata32 | (0x1 << 13));
	rdata32 = READ_VREG(HEVC_PARSER_CORE_CONTROL);
	WRITE_VREG(HEVC_PARSER_CORE_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
	WRITE_VREG(HEVC_PARSER_INT_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_PARSER_IF_CONTROL);
	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
			   rdata32 | (0x3 << 5) | (0x3 << 2) | (0x3 << 0));

	/* IPP */
	rdata32 = READ_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG);
	WRITE_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG, rdata32 | 0xffffffff);

	/* MCRCC */
	rdata32 = READ_VREG(HEVCD_MCRCC_CTL1);
	WRITE_VREG(HEVCD_MCRCC_CTL1, rdata32 | (0x1 << 3));
}
#endif

#ifdef MCRCC_ENABLE
static void config_mcrcc_axi_hw(struct hevc_state_s *hevc, int slice_type)
{
	unsigned int rdata32;
	unsigned int rdata32_2;
	int l0_cnt = 0;
	int l1_cnt = 0x7fff;

	if (get_double_write_mode(hevc) & 0x10) {
		l0_cnt = hevc->cur_pic->RefNum_L0;
		l1_cnt = hevc->cur_pic->RefNum_L1;
	}

	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2);	/* reset mcrcc */

	if (slice_type == 2) {	/* I-PIC */
		/* remove reset -- disables clock */
		WRITE_VREG(HEVCD_MCRCC_CTL1, 0x0);
		return;
	}

	if (slice_type == 0) {	/* B-PIC */
		/* Programme canvas0 */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				   (0 << 8) | (0 << 1) | 0);
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		/* Programme canvas1 */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				   (16 << 8) | (1 << 1) | 0);
		rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32_2 = rdata32_2 & 0xffff;
		rdata32_2 = rdata32_2 | (rdata32_2 << 16);
		if (rdata32 == rdata32_2 && l1_cnt > 1) {
			rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
			rdata32_2 = rdata32_2 & 0xffff;
			rdata32_2 = rdata32_2 | (rdata32_2 << 16);
		}
		WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32_2);
	} else {		/* P-PIC */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				   (0 << 8) | (1 << 1) | 0);
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		if (l0_cnt == 1) {
			WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32);
		} else {
			/* Programme canvas1 */
			rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
			rdata32 = rdata32 & 0xffff;
			rdata32 = rdata32 | (rdata32 << 16);
			WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32);
		}
	}
	/* enable mcrcc progressive-mode */
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0);
}
#endif

static void config_title_hw(struct hevc_state_s *hevc, int sao_vb_size,
							int sao_mem_unit)
{
	WRITE_VREG(HEVC_sao_mem_unit, sao_mem_unit);
	WRITE_VREG(HEVC_SAO_ABV, hevc->work_space_buf->sao_abv.buf_start);
	WRITE_VREG(HEVC_sao_vb_size, sao_vb_size);
	WRITE_VREG(HEVC_SAO_VB, hevc->work_space_buf->sao_vb.buf_start);
}

static u32 init_aux_size;
static int aux_data_is_avaible(struct hevc_state_s *hevc)
{
	u32 reg_val;

	reg_val = READ_VREG(HEVC_AUX_DATA_SIZE);
	if (reg_val != 0 && reg_val != init_aux_size)
		return 1;
	else
		return 0;
}

static void config_aux_buf(struct hevc_state_s *hevc)
{
	WRITE_VREG(HEVC_AUX_ADR, hevc->aux_phy_addr);
	init_aux_size = ((hevc->prefix_aux_size >> 4) << 16) |
		(hevc->suffix_aux_size >> 4);
	WRITE_VREG(HEVC_AUX_DATA_SIZE, init_aux_size);
}

static void config_mpred_hw(struct hevc_state_s *hevc)
{
	int i;
	unsigned int data32;
	struct PIC_s *cur_pic = hevc->cur_pic;
	struct PIC_s *col_pic = hevc->col_pic;
	int AMVP_MAX_NUM_CANDS_MEM = 3;
	int AMVP_MAX_NUM_CANDS = 2;
	int NUM_CHROMA_MODE = 5;
	int DM_CHROMA_IDX = 36;
	int above_ptr_ctrl = 0;
	int buffer_linear = 1;
	int cu_size_log2 = 3;

	int mpred_mv_rd_start_addr;
	int mpred_curr_lcu_x;
	int mpred_curr_lcu_y;
	int mpred_above_buf_start;
	int mpred_mv_rd_ptr;
	int mpred_mv_rd_ptr_p1;
	int mpred_mv_rd_end_addr;
	int MV_MEM_UNIT;
	int mpred_mv_wr_ptr;
	int *ref_poc_L0, *ref_poc_L1;

	int above_en;
	int mv_wr_en;
	int mv_rd_en;
	int col_isIntra;

	if (hevc->slice_type != 2) {
		above_en = 1;
		mv_wr_en = 1;
		mv_rd_en = 1;
		col_isIntra = 0;
	} else {
		above_en = 1;
		mv_wr_en = 1;
		mv_rd_en = 0;
		col_isIntra = 0;
	}

	mpred_mv_rd_start_addr = col_pic->mpred_mv_wr_start_addr;
	data32 = READ_VREG(HEVC_MPRED_CURR_LCU);
	mpred_curr_lcu_x = data32 & 0xffff;
	mpred_curr_lcu_y = (data32 >> 16) & 0xffff;

	MV_MEM_UNIT =
		hevc->lcu_size_log2 == 6 ? 0x200 : hevc->lcu_size_log2 ==
		5 ? 0x80 : 0x20;
	mpred_mv_rd_ptr =
		mpred_mv_rd_start_addr + (hevc->slice_addr * MV_MEM_UNIT);

	mpred_mv_rd_ptr_p1 = mpred_mv_rd_ptr + MV_MEM_UNIT;
	mpred_mv_rd_end_addr =
		mpred_mv_rd_start_addr +
		((hevc->lcu_x_num * hevc->lcu_y_num) * MV_MEM_UNIT);

	mpred_above_buf_start = hevc->work_space_buf->mpred_above.buf_start;

	mpred_mv_wr_ptr =
		cur_pic->mpred_mv_wr_start_addr +
		(hevc->slice_addr * MV_MEM_UNIT);

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
			"cur pic index %d  col pic index %d\n", cur_pic->index,
			col_pic->index);
	}

	WRITE_VREG(HEVC_MPRED_MV_WR_START_ADDR,
			   cur_pic->mpred_mv_wr_start_addr);
	WRITE_VREG(HEVC_MPRED_MV_RD_START_ADDR, mpred_mv_rd_start_addr);

	data32 = ((hevc->lcu_x_num - hevc->tile_width_lcu) * MV_MEM_UNIT);
	WRITE_VREG(HEVC_MPRED_MV_WR_ROW_JUMP, data32);
	WRITE_VREG(HEVC_MPRED_MV_RD_ROW_JUMP, data32);

	data32 = READ_VREG(HEVC_MPRED_CTRL0);
	data32 = (hevc->slice_type |
			  hevc->new_pic << 2 |
			  hevc->new_tile << 3 |
			  hevc->isNextSliceSegment << 4 |
			  hevc->TMVPFlag << 5 |
			  hevc->LDCFlag << 6 |
			  hevc->ColFromL0Flag << 7 |
			  above_ptr_ctrl << 8 |
			  above_en << 9 |
			  mv_wr_en << 10 |
			  mv_rd_en << 11 |
			  col_isIntra << 12 |
			  buffer_linear << 13 |
			  hevc->LongTerm_Curr << 14 |
			  hevc->LongTerm_Col << 15 |
			  hevc->lcu_size_log2 << 16 |
			  cu_size_log2 << 20 | hevc->plevel << 24);
	WRITE_VREG(HEVC_MPRED_CTRL0, data32);

	data32 = READ_VREG(HEVC_MPRED_CTRL1);
	data32 = (
#if 0
			/* no set in m8baby test1902 */
			/* Don't override clk_forced_on , */
			(data32 & (0x1 << 24)) |
#endif
				 hevc->MaxNumMergeCand |
				 AMVP_MAX_NUM_CANDS << 4 |
				 AMVP_MAX_NUM_CANDS_MEM << 8 |
				 NUM_CHROMA_MODE << 12 | DM_CHROMA_IDX << 16);
	WRITE_VREG(HEVC_MPRED_CTRL1, data32);

	data32 = (hevc->pic_w | hevc->pic_h << 16);
	WRITE_VREG(HEVC_MPRED_PIC_SIZE, data32);

	data32 = ((hevc->lcu_x_num - 1) | (hevc->lcu_y_num - 1) << 16);
	WRITE_VREG(HEVC_MPRED_PIC_SIZE_LCU, data32);

	data32 = (hevc->tile_start_lcu_x | hevc->tile_start_lcu_y << 16);
	WRITE_VREG(HEVC_MPRED_TILE_START, data32);

	data32 = (hevc->tile_width_lcu | hevc->tile_height_lcu << 16);
	WRITE_VREG(HEVC_MPRED_TILE_SIZE_LCU, data32);

	data32 = (hevc->RefNum_L0 | hevc->RefNum_L1 << 8 | 0
			  /* col_RefNum_L0<<16| */
			  /* col_RefNum_L1<<24 */
			 );
	WRITE_VREG(HEVC_MPRED_REF_NUM, data32);

	data32 = (hevc->LongTerm_Ref);
	WRITE_VREG(HEVC_MPRED_LT_REF, data32);

	data32 = 0;
	for (i = 0; i < hevc->RefNum_L0; i++)
		data32 = data32 | (1 << i);
	WRITE_VREG(HEVC_MPRED_REF_EN_L0, data32);

	data32 = 0;
	for (i = 0; i < hevc->RefNum_L1; i++)
		data32 = data32 | (1 << i);
	WRITE_VREG(HEVC_MPRED_REF_EN_L1, data32);

	WRITE_VREG(HEVC_MPRED_CUR_POC, hevc->curr_POC);
	WRITE_VREG(HEVC_MPRED_COL_POC, hevc->Col_POC);

	/* below MPRED Ref_POC_xx_Lx registers must follow Ref_POC_xx_L0 ->
	 *   Ref_POC_xx_L1 in pair write order!!!
	 */
	ref_poc_L0 = &(cur_pic->m_aiRefPOCList0[cur_pic->slice_idx][0]);
	ref_poc_L1 = &(cur_pic->m_aiRefPOCList1[cur_pic->slice_idx][0]);

	WRITE_VREG(HEVC_MPRED_L0_REF00_POC, ref_poc_L0[0]);
	WRITE_VREG(HEVC_MPRED_L1_REF00_POC, ref_poc_L1[0]);

	WRITE_VREG(HEVC_MPRED_L0_REF01_POC, ref_poc_L0[1]);
	WRITE_VREG(HEVC_MPRED_L1_REF01_POC, ref_poc_L1[1]);

	WRITE_VREG(HEVC_MPRED_L0_REF02_POC, ref_poc_L0[2]);
	WRITE_VREG(HEVC_MPRED_L1_REF02_POC, ref_poc_L1[2]);

	WRITE_VREG(HEVC_MPRED_L0_REF03_POC, ref_poc_L0[3]);
	WRITE_VREG(HEVC_MPRED_L1_REF03_POC, ref_poc_L1[3]);

	WRITE_VREG(HEVC_MPRED_L0_REF04_POC, ref_poc_L0[4]);
	WRITE_VREG(HEVC_MPRED_L1_REF04_POC, ref_poc_L1[4]);

	WRITE_VREG(HEVC_MPRED_L0_REF05_POC, ref_poc_L0[5]);
	WRITE_VREG(HEVC_MPRED_L1_REF05_POC, ref_poc_L1[5]);

	WRITE_VREG(HEVC_MPRED_L0_REF06_POC, ref_poc_L0[6]);
	WRITE_VREG(HEVC_MPRED_L1_REF06_POC, ref_poc_L1[6]);

	WRITE_VREG(HEVC_MPRED_L0_REF07_POC, ref_poc_L0[7]);
	WRITE_VREG(HEVC_MPRED_L1_REF07_POC, ref_poc_L1[7]);

	WRITE_VREG(HEVC_MPRED_L0_REF08_POC, ref_poc_L0[8]);
	WRITE_VREG(HEVC_MPRED_L1_REF08_POC, ref_poc_L1[8]);

	WRITE_VREG(HEVC_MPRED_L0_REF09_POC, ref_poc_L0[9]);
	WRITE_VREG(HEVC_MPRED_L1_REF09_POC, ref_poc_L1[9]);

	WRITE_VREG(HEVC_MPRED_L0_REF10_POC, ref_poc_L0[10]);
	WRITE_VREG(HEVC_MPRED_L1_REF10_POC, ref_poc_L1[10]);

	WRITE_VREG(HEVC_MPRED_L0_REF11_POC, ref_poc_L0[11]);
	WRITE_VREG(HEVC_MPRED_L1_REF11_POC, ref_poc_L1[11]);

	WRITE_VREG(HEVC_MPRED_L0_REF12_POC, ref_poc_L0[12]);
	WRITE_VREG(HEVC_MPRED_L1_REF12_POC, ref_poc_L1[12]);

	WRITE_VREG(HEVC_MPRED_L0_REF13_POC, ref_poc_L0[13]);
	WRITE_VREG(HEVC_MPRED_L1_REF13_POC, ref_poc_L1[13]);

	WRITE_VREG(HEVC_MPRED_L0_REF14_POC, ref_poc_L0[14]);
	WRITE_VREG(HEVC_MPRED_L1_REF14_POC, ref_poc_L1[14]);

	WRITE_VREG(HEVC_MPRED_L0_REF15_POC, ref_poc_L0[15]);
	WRITE_VREG(HEVC_MPRED_L1_REF15_POC, ref_poc_L1[15]);

	if (hevc->new_pic) {
		WRITE_VREG(HEVC_MPRED_ABV_START_ADDR, mpred_above_buf_start);
		WRITE_VREG(HEVC_MPRED_MV_WPTR, mpred_mv_wr_ptr);
		/* WRITE_VREG(HEVC_MPRED_MV_RPTR,mpred_mv_rd_ptr); */
		WRITE_VREG(HEVC_MPRED_MV_RPTR, mpred_mv_rd_start_addr);
	} else if (!hevc->isNextSliceSegment) {
		/* WRITE_VREG(HEVC_MPRED_MV_RPTR,mpred_mv_rd_ptr_p1); */
		WRITE_VREG(HEVC_MPRED_MV_RPTR, mpred_mv_rd_ptr);
	}

	WRITE_VREG(HEVC_MPRED_MV_RD_END_ADDR, mpred_mv_rd_end_addr);
}

static void config_sao_hw(struct hevc_state_s *hevc, union param_u *params)
{
	unsigned int data32, data32_2;
	int misc_flag0 = hevc->misc_flag0;
	int slice_deblocking_filter_disabled_flag = 0;

	int mc_buffer_size_u_v =
		hevc->lcu_total * hevc->lcu_size * hevc->lcu_size / 2;
	int mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16;
	struct PIC_s *cur_pic = hevc->cur_pic;

	data32 = READ_VREG(HEVC_SAO_CTRL0);
	data32 &= (~0xf);
	data32 |= hevc->lcu_size_log2;
	WRITE_VREG(HEVC_SAO_CTRL0, data32);

	data32 = (hevc->pic_w | hevc->pic_h << 16);
	WRITE_VREG(HEVC_SAO_PIC_SIZE, data32);

	data32 = ((hevc->lcu_x_num - 1) | (hevc->lcu_y_num - 1) << 16);
	WRITE_VREG(HEVC_SAO_PIC_SIZE_LCU, data32);

	if (hevc->new_pic)
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, 0xffffffff);
#ifdef LOSLESS_COMPRESS_MODE
/*SUPPORT_10BIT*/
	if ((get_double_write_mode(hevc) & 0x10) == 0) {
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 &= (~(0xff << 16));

		if (get_double_write_mode(hevc) == 2 ||
			get_double_write_mode(hevc) == 3)
			data32 |= (0xff<<16);
		else if (get_double_write_mode(hevc) == 4)
			data32 |= (0x33<<16);

		if (hevc->mem_saving_mode == 1)
			data32 |= (1 << 9);
		else
			data32 &= ~(1 << 9);
		if (workaround_enable & 1)
			data32 |= (1 << 7);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}
	data32 = cur_pic->mc_y_adr;
	if (get_double_write_mode(hevc))
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, cur_pic->dw_y_adr);

	if ((get_double_write_mode(hevc) & 0x10) == 0)
		WRITE_VREG(HEVC_CM_BODY_START_ADDR, data32);

	if (hevc->mmu_enable)
		WRITE_VREG(HEVC_CM_HEADER_START_ADDR, cur_pic->header_adr);
#else
	data32 = cur_pic->mc_y_adr;
	WRITE_VREG(HEVC_SAO_Y_START_ADDR, data32);
#endif
	data32 = (mc_buffer_size_u_v_h << 16) << 1;
	WRITE_VREG(HEVC_SAO_Y_LENGTH, data32);

#ifdef LOSLESS_COMPRESS_MODE
/*SUPPORT_10BIT*/
	if (get_double_write_mode(hevc))
		WRITE_VREG(HEVC_SAO_C_START_ADDR, cur_pic->dw_u_v_adr);
#else
	data32 = cur_pic->mc_u_v_adr;
	WRITE_VREG(HEVC_SAO_C_START_ADDR, data32);
#endif
	data32 = (mc_buffer_size_u_v_h << 16);
	WRITE_VREG(HEVC_SAO_C_LENGTH, data32);

#ifdef LOSLESS_COMPRESS_MODE
/*SUPPORT_10BIT*/
	if (get_double_write_mode(hevc)) {
		WRITE_VREG(HEVC_SAO_Y_WPTR, cur_pic->dw_y_adr);
		WRITE_VREG(HEVC_SAO_C_WPTR, cur_pic->dw_u_v_adr);
	}
#else
	/* multi tile to do... */
	data32 = cur_pic->mc_y_adr;
	WRITE_VREG(HEVC_SAO_Y_WPTR, data32);

	data32 = cur_pic->mc_u_v_adr;
	WRITE_VREG(HEVC_SAO_C_WPTR, data32);
#endif
	/* DBLK CONFIG HERE */
	if (hevc->new_pic) {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
				data32 = (0xff << 8) | (0x0  << 0);
			else
				data32 = (0x57 << 8) |  /* 1st/2nd write both enable*/
					(0x0  << 0);   /* h265 video format*/

			if (hevc->pic_w >= 1280)
				data32 |= (0x1 << 4); /*dblk pipeline mode=1 for performance*/
			data32 &= (~0x300); /*[8]:first write enable (compress)  [9]:double write enable (uncompress)*/
			if (get_double_write_mode(hevc) == 0)
				data32 |= (0x1 << 8); /*enable first write*/
			else if (get_double_write_mode(hevc) == 0x10)
				data32 |= (0x1 << 9); /*double write only*/
			else
				data32 |= ((0x1 << 8)  |(0x1 << 9));

			WRITE_VREG(HEVC_DBLK_CFGB, data32);
			hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
				"[DBLK DEBUG] HEVC1 CFGB : 0x%x\n", data32);
		}
		data32 = (hevc->pic_w | hevc->pic_h << 16);
		WRITE_VREG(HEVC_DBLK_CFG2, data32);

		if ((misc_flag0 >> PCM_ENABLE_FLAG_BIT) & 0x1) {
			data32 =
				((misc_flag0 >>
				  PCM_LOOP_FILTER_DISABLED_FLAG_BIT) &
				 0x1) << 3;
		} else
			data32 = 0;
		data32 |=
			(((params->p.pps_cb_qp_offset & 0x1f) << 4) |
			 ((params->p.pps_cr_qp_offset
			   & 0x1f) <<
			  9));
		data32 |=
			(hevc->lcu_size ==
			 64) ? 0 : ((hevc->lcu_size == 32) ? 1 : 2);

		WRITE_VREG(HEVC_DBLK_CFG1, data32);

		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
			/*if (debug & 0x80) {*/
				data32 = 1 << 28; /* Debug only: sts1 chooses dblk_main*/
				WRITE_VREG(HEVC_DBLK_STS1 + 4, data32); /* 0x3510 */
				hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
					"[DBLK DEBUG] HEVC1 STS1 : 0x%x\n",
					data32);
			/*}*/
		}
	}
#if 0
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	data32 |= (mem_map_mode <<
			12);

/*  [13:12] axi_aformat,
 *			       0-Linear, 1-32x32, 2-64x32
 */
	WRITE_VREG(HEVC_SAO_CTRL1, data32);

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	data32 |= (mem_map_mode <<
			   4);

/*  [5:4]    -- address_format
 *				00:linear 01:32x32 10:64x32
 */
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#else
	/* m8baby test1902 */
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	data32 |= (mem_map_mode <<
			   12);

/*  [13:12] axi_aformat, 0-Linear,
 *				   1-32x32, 2-64x32
 */
	data32 &= (~0xff0);
	/* data32 |= 0x670;  // Big-Endian per 64-bit */
	data32 |= endian;	/* Big-Endian per 64-bit */
	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_G12A) {
		data32 &= (~0x3); /*[1]:dw_disable [0]:cm_disable*/
		if (get_double_write_mode(hevc) == 0)
			data32 |= 0x2; /*disable double write*/
		else if (get_double_write_mode(hevc) & 0x10)
			data32 |= 0x1; /*disable cm*/
	} else {
			unsigned int data;
			data = (0x57 << 8) |  /* 1st/2nd write both enable*/
				(0x0  << 0);   /* h265 video format*/
			if (hevc->pic_w >= 1280)
				data |= (0x1 << 4); /*dblk pipeline mode=1 for performance*/
			data &= (~0x300); /*[8]:first write enable (compress)  [9]:double write enable (uncompress)*/
			if (get_double_write_mode(hevc) == 0)
				data |= (0x1 << 8); /*enable first write*/
			else if (get_double_write_mode(hevc) & 0x10)
				data |= (0x1 << 9); /*double write only*/
			else
				data |= ((0x1 << 8)  |(0x1 << 9));

			WRITE_VREG(HEVC_DBLK_CFGB, data);
			hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
				"[DBLK DEBUG] HEVC1 CFGB : 0x%x\n", data);
	}

	WRITE_VREG(HEVC_SAO_CTRL1, data32);
	if (get_double_write_mode(hevc) & 0x10) {
		/* [23:22] dw_v1_ctrl
		 *[21:20] dw_v0_ctrl
		 *[19:18] dw_h1_ctrl
		 *[17:16] dw_h0_ctrl
		 */
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		/*set them all 0 for H265_NV21 (no down-scale)*/
		data32 &= ~(0xff << 16);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	/* [5:4]    -- address_format 00:linear 01:32x32 10:64x32 */
	data32 |= (mem_map_mode <<
			   4);
	data32 &= (~0xF);
	data32 |= 0xf;  /* valid only when double write only */
		/*data32 |= 0x8;*/		/* Big-Endian per 64-bit */
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#endif
	data32 = 0;
	data32_2 = READ_VREG(HEVC_SAO_CTRL0);
	data32_2 &= (~0x300);
	/* slice_deblocking_filter_disabled_flag = 0;
	 *	ucode has handle it , so read it from ucode directly
	 */
	if (hevc->tile_enabled) {
		data32 |=
			((misc_flag0 >>
			  LOOP_FILER_ACROSS_TILES_ENABLED_FLAG_BIT) &
			 0x1) << 0;
		data32_2 |=
			((misc_flag0 >>
			  LOOP_FILER_ACROSS_TILES_ENABLED_FLAG_BIT) &
			 0x1) << 8;
	}
	slice_deblocking_filter_disabled_flag = (misc_flag0 >>
			SLICE_DEBLOCKING_FILTER_DISABLED_FLAG_BIT) &
		0x1;	/* ucode has handle it,so read it from ucode directly */
	if ((misc_flag0 & (1 << DEBLOCKING_FILTER_OVERRIDE_ENABLED_FLAG_BIT))
		&& (misc_flag0 & (1 << DEBLOCKING_FILTER_OVERRIDE_FLAG_BIT))) {
		/* slice_deblocking_filter_disabled_flag =
		 * (misc_flag0>>SLICE_DEBLOCKING_FILTER_DISABLED_FLAG_BIT)&0x1;
		 * //ucode has handle it , so read it from ucode directly
		 */
		data32 |= slice_deblocking_filter_disabled_flag << 2;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print_cont(hevc, 0,
			"(1,%x)", data32);
		if (!slice_deblocking_filter_disabled_flag) {
			data32 |= (params->p.slice_beta_offset_div2 & 0xf) << 3;
			data32 |= (params->p.slice_tc_offset_div2 & 0xf) << 7;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print_cont(hevc, 0,
				"(2,%x)", data32);
		}
	} else {
		data32 |=
			((misc_flag0 >>
			  PPS_DEBLOCKING_FILTER_DISABLED_FLAG_BIT) &
			 0x1) << 2;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print_cont(hevc, 0,
			"(3,%x)", data32);
		if (((misc_flag0 >> PPS_DEBLOCKING_FILTER_DISABLED_FLAG_BIT) &
			 0x1) == 0) {
			data32 |= (params->p.pps_beta_offset_div2 & 0xf) << 3;
			data32 |= (params->p.pps_tc_offset_div2 & 0xf) << 7;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print_cont(hevc, 0,
				"(4,%x)", data32);
		}
	}
	if ((misc_flag0 & (1 << PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT))
		&& ((misc_flag0 & (1 << SLICE_SAO_LUMA_FLAG_BIT))
			|| (misc_flag0 & (1 << SLICE_SAO_CHROMA_FLAG_BIT))
			|| (!slice_deblocking_filter_disabled_flag))) {
		data32 |=
			((misc_flag0 >>
			  SLICE_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			 & 0x1)	<< 1;
		data32_2 |=
			((misc_flag0 >>
			  SLICE_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			& 0x1) << 9;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print_cont(hevc, 0,
			"(5,%x)\n", data32);
	} else {
		data32 |=
			((misc_flag0 >>
			  PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			 & 0x1) << 1;
		data32_2 |=
			((misc_flag0 >>
			  PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			 & 0x1) << 9;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print_cont(hevc, 0,
			"(6,%x)\n", data32);
	}
	WRITE_VREG(HEVC_DBLK_CFG9, data32);
	WRITE_VREG(HEVC_SAO_CTRL0, data32_2);
}

#ifdef TEST_NO_BUF
static unsigned char test_flag = 1;
#endif

static void pic_list_process(struct hevc_state_s *hevc)
{
	int work_pic_num = get_work_pic_num(hevc);
	int alloc_pic_count = 0;
	int i;
	struct PIC_s *pic;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		alloc_pic_count++;
		if (pic->output_mark == 0 && pic->referenced == 0
			&& pic->output_ready == 0
			&& (pic->width != hevc->pic_w ||
				pic->height != hevc->pic_h)
			) {
			set_buf_unused(hevc, pic->BUF_index);
			pic->BUF_index = -1;
			if (alloc_pic_count > work_pic_num) {
				pic->width = 0;
				pic->height = 0;
				pic->index = -1;
			} else {
				pic->width = hevc->pic_w;
				pic->height = hevc->pic_h;
			}
		}
	}
	if (alloc_pic_count < work_pic_num) {
		int new_count = alloc_pic_count;
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic && pic->index == -1) {
				pic->index = i;
				pic->BUF_index = -1;
				pic->width = hevc->pic_w;
				pic->height = hevc->pic_h;
				new_count++;
				if (new_count >=
					work_pic_num)
					break;
			}
		}

	}
	dealloc_unused_buf(hevc);
	if (get_alloc_pic_count(hevc)
		!= alloc_pic_count) {
		hevc_print_cont(hevc, 0,
		"%s: work_pic_num is %d, Change alloc_pic_count from %d to %d\n",
		__func__,
		work_pic_num,
		alloc_pic_count,
		get_alloc_pic_count(hevc));
	}
}

static void recycle_mmu_bufs(struct hevc_state_s *hevc)
{
	int i;
	struct PIC_s *pic;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;

		if (pic->output_mark == 0 && pic->referenced == 0
			&& pic->output_ready == 0
			&& pic->scatter_alloc
			)
			release_pic_mmu_buf(hevc, pic);
	}

}

static struct PIC_s *get_new_pic(struct hevc_state_s *hevc,
		union param_u *rpm_param)
{
	struct PIC_s *new_pic = NULL;
	struct PIC_s *pic;
	int i;
	int ret;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;

		if (pic->output_mark == 0 && pic->referenced == 0
			&& pic->output_ready == 0
			&& pic->width == hevc->pic_w
			&& pic->height == hevc->pic_h
			) {
			if (new_pic) {
				if (new_pic->POC != INVALID_POC) {
					if (pic->POC == INVALID_POC ||
						pic->POC < new_pic->POC)
						new_pic = pic;
				}
			} else
				new_pic = pic;
		}
	}

	if (new_pic == NULL)
		return NULL;

	if (new_pic->BUF_index < 0) {
		if (alloc_buf(hevc) < 0)
			return NULL;
		else {
			if (config_pic(hevc, new_pic) < 0) {
				dealloc_pic_buf(hevc, new_pic);
				return NULL;
			}
		}
		new_pic->width = hevc->pic_w;
		new_pic->height = hevc->pic_h;
		set_canvas(hevc, new_pic);

		init_pic_list_hw(hevc);
	}

	if (new_pic) {
		new_pic->double_write_mode =
			get_double_write_mode(hevc);
		if (new_pic->double_write_mode)
			set_canvas(hevc, new_pic);

#ifdef TEST_NO_BUF
		if (test_flag) {
			test_flag = 0;
			return NULL;
		} else
			test_flag = 1;
#endif
		if (get_mv_buf(hevc, new_pic) < 0)
			return NULL;

		if (hevc->mmu_enable) {
			ret = H265_alloc_mmu(hevc, new_pic,
				rpm_param->p.bit_depth,
				hevc->frame_mmu_map_addr);
			if (ret != 0) {
				put_mv_buf(hevc, new_pic);
				hevc_print(hevc, 0,
				"can't alloc need mmu1,idx %d ret =%d\n",
				new_pic->decode_idx,
				ret);
				return NULL;
			}
		}
		new_pic->referenced = 1;
		new_pic->decode_idx = hevc->decode_idx;
		new_pic->slice_idx = 0;
		new_pic->referenced = 1;
		new_pic->output_mark = 0;
		new_pic->recon_mark = 0;
		new_pic->error_mark = 0;
		/* new_pic->output_ready = 0; */
		new_pic->num_reorder_pic = rpm_param->p.sps_num_reorder_pics_0;
		new_pic->losless_comp_body_size = hevc->losless_comp_body_size;
		new_pic->POC = hevc->curr_POC;
		new_pic->pic_struct = hevc->curr_pic_struct;
		if (new_pic->aux_data_buf)
			release_aux_data(hevc, new_pic);
		new_pic->mem_saving_mode =
			hevc->mem_saving_mode;
		new_pic->bit_depth_luma =
			hevc->bit_depth_luma;
		new_pic->bit_depth_chroma =
			hevc->bit_depth_chroma;
		new_pic->video_signal_type =
			hevc->video_signal_type;

		new_pic->conformance_window_flag =
			hevc->param.p.conformance_window_flag;
		new_pic->conf_win_left_offset =
			hevc->param.p.conf_win_left_offset;
		new_pic->conf_win_right_offset =
			hevc->param.p.conf_win_right_offset;
		new_pic->conf_win_top_offset =
			hevc->param.p.conf_win_top_offset;
		new_pic->conf_win_bottom_offset =
			hevc->param.p.conf_win_bottom_offset;
		new_pic->chroma_format_idc =
				hevc->param.p.chroma_format_idc;

		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
			"%s: index %d, buf_idx %d, decode_idx %d, POC %d\n",
			__func__, new_pic->index,
			new_pic->BUF_index, new_pic->decode_idx,
			new_pic->POC);

	}

	return new_pic;
}

static int get_display_pic_num(struct hevc_state_s *hevc)
{
	int i;
	struct PIC_s *pic;
	int num = 0;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL ||
			pic->index == -1)
			continue;

		if (pic->output_ready == 1)
			num++;
	}
	return num;
}

static void flush_output(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	struct PIC_s *pic_display;

	if (pic) {
		/*PB skip control */
		if (pic->error_mark == 0 && hevc->PB_skip_mode == 1) {
			/* start decoding after first I */
			hevc->ignore_bufmgr_error |= 0x1;
		}
		if (hevc->ignore_bufmgr_error & 1) {
			if (hevc->PB_skip_count_after_decoding > 0)
				hevc->PB_skip_count_after_decoding--;
			else {
				/* start displaying */
				hevc->ignore_bufmgr_error |= 0x2;
			}
		}
		/**/
		if (pic->POC != INVALID_POC) {
			pic->output_mark = 1;
			pic->recon_mark = 1;
		}
		pic->recon_mark = 1;
	}
	do {
		pic_display = output_pic(hevc, 1);

		if (pic_display) {
			pic_display->referenced = 0;
			put_mv_buf(hevc, pic_display);
			if ((pic_display->error_mark
				 && ((hevc->ignore_bufmgr_error & 0x2) == 0))
				|| (get_dbg_flag(hevc) &
					H265_DEBUG_DISPLAY_CUR_FRAME)
				|| (get_dbg_flag(hevc) &
					H265_DEBUG_NO_DISPLAY)) {
				pic_display->output_ready = 0;
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print(hevc, 0,
					"[BM] Display: POC %d, ",
					 pic_display->POC);
					hevc_print_cont(hevc, 0,
					"decoding index %d ==> ",
					 pic_display->decode_idx);
					hevc_print_cont(hevc, 0,
					"Debug mode or error, recycle it\n");
				}
			} else {
				if (hevc->i_only & 0x1
					&& pic_display->slice_type != 2) {
					pic_display->output_ready = 0;
				} else {
					prepare_display_buf(hevc, pic_display);
					if (get_dbg_flag(hevc)
						& H265_DEBUG_BUFMGR) {
						hevc_print(hevc, 0,
						"[BM] flush Display: POC %d, ",
						 pic_display->POC);
						hevc_print_cont(hevc, 0,
						"decoding index %d\n",
						 pic_display->decode_idx);
					}
				}
			}
		}
	} while (pic_display);
	clear_referenced_flag(hevc);
}

/*
* dv_meta_flag: 1, dolby meta only; 2, not include dolby meta
*/
static void set_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic, unsigned char suffix_flag,
	unsigned char dv_meta_flag)
{
	int i;
	unsigned short *aux_adr;
	unsigned int size_reg_val =
		READ_VREG(HEVC_AUX_DATA_SIZE);
	unsigned int aux_count = 0;
	int aux_size = 0;
	if (pic == NULL || 0 == aux_data_is_avaible(hevc))
		return;

	if (hevc->aux_data_dirty ||
		hevc->m_ins_flag == 0) {
		dma_sync_single_for_cpu(
		amports_get_dma_device(),
		hevc->aux_phy_addr,
		hevc->prefix_aux_size + hevc->suffix_aux_size,
		DMA_FROM_DEVICE);

		hevc->aux_data_dirty = 0;
	}

	if (suffix_flag) {
		aux_adr = (unsigned short *)
			(hevc->aux_addr +
			hevc->prefix_aux_size);
		aux_count =
		((size_reg_val & 0xffff) << 4)
			>> 1;
		aux_size =
			hevc->suffix_aux_size;
	} else {
		aux_adr =
		(unsigned short *)hevc->aux_addr;
		aux_count =
		((size_reg_val >> 16) << 4)
			>> 1;
		aux_size =
			hevc->prefix_aux_size;
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE) {
		hevc_print(hevc, 0,
			"%s:pic 0x%p old size %d count %d,suf %d dv_flag %d\r\n",
			__func__, pic, pic->aux_data_size,
			aux_count, suffix_flag, dv_meta_flag);
	}
	if (aux_size > 0 && aux_count > 0) {
		int heads_size = 0;
		int new_size;
		char *new_buf;

		for (i = 0; i < aux_count; i++) {
			unsigned char tag = aux_adr[i] >> 8;
			if (tag != 0 && tag != 0xff) {
				if (dv_meta_flag == 0)
					heads_size += 8;
				else if (dv_meta_flag == 1 && tag == 0x1)
					heads_size += 8;
				else if (dv_meta_flag == 2 && tag != 0x1)
					heads_size += 8;
			}
		}
		new_size = pic->aux_data_size + aux_count + heads_size;
		new_buf = vmalloc(new_size);
		if (new_buf) {
			unsigned char valid_tag = 0;
			unsigned char *h =
				new_buf +
				pic->aux_data_size;
			unsigned char *p = h + 8;
			int len = 0;
			int padding_len = 0;
			memcpy(new_buf, pic->aux_data_buf,  pic->aux_data_size);
			if (pic->aux_data_buf)
				vfree(pic->aux_data_buf);
			pic->aux_data_buf = new_buf;
			for (i = 0; i < aux_count; i += 4) {
				int ii;
				unsigned char tag = aux_adr[i + 3] >> 8;
				if (tag != 0 && tag != 0xff) {
					if (dv_meta_flag == 0)
						valid_tag = 1;
					else if (dv_meta_flag == 1
						&& tag == 0x1)
						valid_tag = 1;
					else if (dv_meta_flag == 2
						&& tag != 0x1)
						valid_tag = 1;
					else
						valid_tag = 0;
					if (valid_tag && len > 0) {
						pic->aux_data_size +=
						(len + 8);
						h[0] = (len >> 24)
						& 0xff;
						h[1] = (len >> 16)
						& 0xff;
						h[2] = (len >> 8)
						& 0xff;
						h[3] = (len >> 0)
						& 0xff;
						h[6] =
						(padding_len >> 8)
						& 0xff;
						h[7] = (padding_len)
						& 0xff;
						h += (len + 8);
						p += 8;
						len = 0;
						padding_len = 0;
					}
					if (valid_tag) {
						h[4] = tag;
						h[5] = 0;
						h[6] = 0;
						h[7] = 0;
					}
				}
				if (valid_tag) {
					for (ii = 0; ii < 4; ii++) {
						unsigned short aa =
							aux_adr[i + 3
							- ii];
						*p = aa & 0xff;
						p++;
						len++;
						/*if ((aa >> 8) == 0xff)
							padding_len++;*/
					}
				}
			}
			if (len > 0) {
				pic->aux_data_size += (len + 8);
				h[0] = (len >> 24) & 0xff;
				h[1] = (len >> 16) & 0xff;
				h[2] = (len >> 8) & 0xff;
				h[3] = (len >> 0) & 0xff;
				h[6] = (padding_len >> 8) & 0xff;
				h[7] = (padding_len) & 0xff;
			}
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE) {
				hevc_print(hevc, 0,
					"aux: (size %d) suffix_flag %d\n",
					pic->aux_data_size, suffix_flag);
				for (i = 0; i < pic->aux_data_size; i++) {
					hevc_print_cont(hevc, 0,
						"%02x ", pic->aux_data_buf[i]);
					if (((i + 1) & 0xf) == 0)
						hevc_print_cont(hevc, 0, "\n");
				}
				hevc_print_cont(hevc, 0, "\n");
			}

		} else {
			hevc_print(hevc, 0, "new buf alloc failed\n");
			if (pic->aux_data_buf)
				vfree(pic->aux_data_buf);
			pic->aux_data_buf = NULL;
			pic->aux_data_size = 0;
		}
	}

}

static void release_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic)
{
	if (pic->aux_data_buf)
		vfree(pic->aux_data_buf);
	pic->aux_data_buf = NULL;
	pic->aux_data_size = 0;
}

static inline void hevc_pre_pic(struct hevc_state_s *hevc,
			struct PIC_s *pic)
{

	/* prev pic */
	/*if (hevc->curr_POC != 0) {*/
	int decoded_poc = hevc->iPrevPOC;
#ifdef MULTI_INSTANCE_SUPPORT
	if (hevc->m_ins_flag) {
		decoded_poc = hevc->decoded_poc;
		hevc->decoded_poc = INVALID_POC;
	}
#endif
	if (hevc->m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR
			&& hevc->m_nalUnitType !=
			NAL_UNIT_CODED_SLICE_IDR_N_LP) {
		struct PIC_s *pic_display;

		pic = get_pic_by_POC(hevc, decoded_poc);
		if (pic && (pic->POC != INVALID_POC)) {
			/*PB skip control */
			if (pic->error_mark == 0
					&& hevc->PB_skip_mode == 1) {
				/* start decoding after
				 *   first I
				 */
				hevc->ignore_bufmgr_error |= 0x1;
			}
			if (hevc->ignore_bufmgr_error & 1) {
				if (hevc->PB_skip_count_after_decoding > 0) {
					hevc->PB_skip_count_after_decoding--;
				} else {
					/* start displaying */
					hevc->ignore_bufmgr_error |= 0x2;
				}
			}
			if (hevc->mmu_enable
				&& ((hevc->double_write_mode & 0x10) == 0)) {
				if (!hevc->m_ins_flag) {
					hevc->used_4k_num =
					READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;

					if ((!is_skip_decoding(hevc, pic)) &&
						(hevc->used_4k_num >= 0) &&
						(hevc->cur_pic->scatter_alloc
						== 1)) {
						hevc_print(hevc,
						H265_DEBUG_BUFMGR_MORE,
						"%s pic index %d scatter_alloc %d page_start %d\n",
						"decoder_mmu_box_free_idx_tail",
						hevc->cur_pic->index,
						hevc->cur_pic->scatter_alloc,
						hevc->used_4k_num);
						decoder_mmu_box_free_idx_tail(
						hevc->mmu_box,
						hevc->cur_pic->index,
						hevc->used_4k_num);
						hevc->cur_pic->scatter_alloc
							= 2;
					}
					hevc->used_4k_num = -1;
				}
			}

			pic->output_mark = 1;
			pic->recon_mark = 1;
		}
		do {
			pic_display = output_pic(hevc, 0);

			if (pic_display) {
				if ((pic_display->error_mark &&
					((hevc->ignore_bufmgr_error &
							  0x2) == 0))
					|| (get_dbg_flag(hevc) &
						H265_DEBUG_DISPLAY_CUR_FRAME)
					|| (get_dbg_flag(hevc) &
						H265_DEBUG_NO_DISPLAY)) {
					pic_display->output_ready = 0;
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print(hevc, 0,
						"[BM] Display: POC %d, ",
							 pic_display->POC);
						hevc_print_cont(hevc, 0,
						"decoding index %d ==> ",
							 pic_display->
							 decode_idx);
						hevc_print_cont(hevc, 0,
						"Debug or err,recycle it\n");
					}
				} else {
					if (hevc->i_only & 0x1
						&& pic_display->
						slice_type != 2) {
						pic_display->output_ready = 0;
					} else {
						prepare_display_buf
							(hevc,
							 pic_display);
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print(hevc, 0,
						"[BM] Display: POC %d, ",
							 pic_display->POC);
							hevc_print_cont(hevc, 0,
							"decoding index %d\n",
							 pic_display->
							 decode_idx);
						}
					}
				}
			}
		} while (pic_display);
	} else {
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
			"[BM] current pic is IDR, ");
			hevc_print(hevc, 0,
			"clear referenced flag of all buffers\n");
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			dump_pic_list(hevc);
		pic = get_pic_by_POC(hevc, decoded_poc);
		flush_output(hevc, pic);
	}

}

static void check_pic_decoded_error_pre(struct hevc_state_s *hevc,
	int decoded_lcu)
{
	int current_lcu_idx = decoded_lcu;
	if (decoded_lcu < 0)
		return;

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
			"cur lcu idx = %d, (total %d)\n",
			current_lcu_idx, hevc->lcu_total);
	}
	if ((error_handle_policy & 0x20) == 0 && hevc->cur_pic != NULL) {
		if (hevc->first_pic_after_recover) {
			if (current_lcu_idx !=
			 ((hevc->lcu_x_num_pre*hevc->lcu_y_num_pre) - 1))
				hevc->cur_pic->error_mark = 1;
		} else {
			if (hevc->lcu_x_num_pre != 0
			 && hevc->lcu_y_num_pre != 0
			 && current_lcu_idx != 0
			 && current_lcu_idx <
			 ((hevc->lcu_x_num_pre*hevc->lcu_y_num_pre) - 1))
				hevc->cur_pic->error_mark = 1;
		}
		if (hevc->cur_pic->error_mark) {
			hevc_print(hevc, 0,
				"cur lcu idx = %d, (total %d), set error_mark\n",
				current_lcu_idx,
				hevc->lcu_x_num_pre*hevc->lcu_y_num_pre);
			if (is_log_enable(hevc))
				add_log(hevc,
					"cur lcu idx = %d, (total %d), set error_mark",
					current_lcu_idx,
					hevc->lcu_x_num_pre *
					    hevc->lcu_y_num_pre);

		}

	}
	if (hevc->cur_pic && hevc->head_error_flag) {
		hevc->cur_pic->error_mark = 1;
		hevc_print(hevc, 0,
			"head has error, set error_mark\n");
	}

	if ((error_handle_policy & 0x80) == 0) {
		if (hevc->over_decode && hevc->cur_pic) {
			hevc_print(hevc, 0,
				"over decode, set error_mark\n");
			hevc->cur_pic->error_mark = 1;
		}
	}

	hevc->lcu_x_num_pre = hevc->lcu_x_num;
	hevc->lcu_y_num_pre = hevc->lcu_y_num;
}

static void check_pic_decoded_error(struct hevc_state_s *hevc,
	int decoded_lcu)
{
	int current_lcu_idx = decoded_lcu;
	if (decoded_lcu < 0)
		return;

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
			"cur lcu idx = %d, (total %d)\n",
			current_lcu_idx, hevc->lcu_total);
	}
	if ((error_handle_policy & 0x20) == 0 && hevc->cur_pic != NULL) {
		if (hevc->lcu_x_num != 0
		 && hevc->lcu_y_num != 0
		 && current_lcu_idx != 0
		 && current_lcu_idx <
		 ((hevc->lcu_x_num*hevc->lcu_y_num) - 1))
			hevc->cur_pic->error_mark = 1;
		if (hevc->cur_pic->error_mark) {
			hevc_print(hevc, 0,
				"cur lcu idx = %d, (total %d), set error_mark\n",
				current_lcu_idx,
				hevc->lcu_x_num*hevc->lcu_y_num);
			if (is_log_enable(hevc))
				add_log(hevc,
					"cur lcu idx = %d, (total %d), set error_mark",
					current_lcu_idx,
					hevc->lcu_x_num *
					    hevc->lcu_y_num);

		}

	}
	if (hevc->cur_pic && hevc->head_error_flag) {
		hevc->cur_pic->error_mark = 1;
		hevc_print(hevc, 0,
			"head has error, set error_mark\n");
	}

	if ((error_handle_policy & 0x80) == 0) {
		if (hevc->over_decode && hevc->cur_pic) {
			hevc_print(hevc, 0,
				"over decode, set error_mark\n");
			hevc->cur_pic->error_mark = 1;
		}
	}
}

/* only when we decoded one field or one frame,
we can call this function to get qos info*/
static void get_picture_qos_info(struct hevc_state_s *hevc)
{
	struct PIC_s *picture = hevc->cur_pic;

/*
#define DEBUG_QOS
*/

	if (!hevc->cur_pic)
		return;

	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_G12A) {
		unsigned char a[3];
		unsigned char i, j, t;
		unsigned long  data;

		data = READ_VREG(HEVC_MV_INFO);
		if (picture->slice_type == I_SLICE)
			data = 0;
		a[0] = data & 0xff;
		a[1] = (data >> 8) & 0xff;
		a[2] = (data >> 16) & 0xff;

		for (i = 0; i < 3; i++)
			for (j = i+1; j < 3; j++) {
				if (a[j] < a[i]) {
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				} else if (a[j] == a[i]) {
					a[i]++;
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				}
			}
		picture->max_mv = a[2];
		picture->avg_mv = a[1];
		picture->min_mv = a[0];
#ifdef DEBUG_QOS
		hevc_print(hevc, 0, "mv data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);
#endif

		data = READ_VREG(HEVC_QP_INFO);
		a[0] = data & 0x1f;
		a[1] = (data >> 8) & 0x3f;
		a[2] = (data >> 16) & 0x7f;

		for (i = 0; i < 3; i++)
			for (j = i+1; j < 3; j++) {
				if (a[j] < a[i]) {
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				} else if (a[j] == a[i]) {
					a[i]++;
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				}
			}
		picture->max_qp = a[2];
		picture->avg_qp = a[1];
		picture->min_qp = a[0];
#ifdef DEBUG_QOS
		hevc_print(hevc, 0, "qp data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);
#endif

		data = READ_VREG(HEVC_SKIP_INFO);
		a[0] = data & 0x1f;
		a[1] = (data >> 8) & 0x3f;
		a[2] = (data >> 16) & 0x7f;

		for (i = 0; i < 3; i++)
			for (j = i+1; j < 3; j++) {
				if (a[j] < a[i]) {
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				} else if (a[j] == a[i]) {
					a[i]++;
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				}
			}
		picture->max_skip = a[2];
		picture->avg_skip = a[1];
		picture->min_skip = a[0];

#ifdef DEBUG_QOS
		hevc_print(hevc, 0,
			"skip data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);
#endif
	} else {
		uint32_t blk88_y_count;
		uint32_t blk88_c_count;
		uint32_t blk22_mv_count;
		uint32_t rdata32;
		int32_t mv_hi;
		int32_t mv_lo;
		uint32_t rdata32_l;
		uint32_t mvx_L0_hi;
		uint32_t mvy_L0_hi;
		uint32_t mvx_L1_hi;
		uint32_t mvy_L1_hi;
		int64_t value;
		uint64_t temp_value;
#ifdef DEBUG_QOS
		int pic_number = picture->POC;
#endif

		picture->max_mv = 0;
		picture->avg_mv = 0;
		picture->min_mv = 0;

		picture->max_skip = 0;
		picture->avg_skip = 0;
		picture->min_skip = 0;

		picture->max_qp = 0;
		picture->avg_qp = 0;
		picture->min_qp = 0;



#ifdef DEBUG_QOS
		hevc_print(hevc, 0, "slice_type:%d, poc:%d\n",
			picture->slice_type,
			picture->POC);
#endif
		/* set rd_idx to 0 */
	    WRITE_VREG(HEVC_PIC_QUALITY_CTRL, 0);

	    blk88_y_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk88_y_count == 0) {
#ifdef DEBUG_QOS
			hevc_print(hevc, 0,
				"[Picture %d Quality] NO Data yet.\n",
				pic_number);
#endif
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* qp_y_sum */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] Y QP AVG : %d (%d/%d)\n",
			pic_number, rdata32/blk88_y_count,
			rdata32, blk88_y_count);
#endif
		picture->avg_qp = rdata32/blk88_y_count;
		/* intra_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] Y intra rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_y_count,
			'%', rdata32);
#endif
		/* skipped_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] Y skipped rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_y_count,
			'%', rdata32);
#endif
		picture->avg_skip = rdata32*100/blk88_y_count;
		/* coeff_non_zero_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] Y ZERO_Coeff rate : %d%c (%d)\n",
			pic_number, (100 - rdata32*100/(blk88_y_count*1)),
			'%', rdata32);
#endif
		/* blk66_c_count */
	    blk88_c_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk88_c_count == 0) {
#ifdef DEBUG_QOS
			hevc_print(hevc, 0,
				"[Picture %d Quality] NO Data yet.\n",
				pic_number);
#endif
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* qp_c_sum */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] C QP AVG : %d (%d/%d)\n",
			pic_number, rdata32/blk88_c_count,
			rdata32, blk88_c_count);
#endif
		/* intra_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] C intra rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_c_count,
			'%', rdata32);
#endif
		/* skipped_cu_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] C skipped rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_c_count,
			'%', rdata32);
#endif
		/* coeff_non_zero_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] C ZERO_Coeff rate : %d%c (%d)\n",
			pic_number, (100 - rdata32*100/(blk88_c_count*1)),
			'%', rdata32);
#endif

		/* 1'h0, qp_c_max[6:0], 1'h0, qp_c_min[6:0],
		1'h0, qp_y_max[6:0], 1'h0, qp_y_min[6:0] */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] Y QP min : %d\n",
			pic_number, (rdata32>>0)&0xff);
#endif
		picture->min_qp = (rdata32>>0)&0xff;

#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] Y QP max : %d\n",
			pic_number, (rdata32>>8)&0xff);
#endif
		picture->max_qp = (rdata32>>8)&0xff;

#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] C QP min : %d\n",
			pic_number, (rdata32>>16)&0xff);
	    hevc_print(hevc, 0, "[Picture %d Quality] C QP max : %d\n",
			pic_number, (rdata32>>24)&0xff);
#endif

		/* blk22_mv_count */
	    blk22_mv_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk22_mv_count == 0) {
#ifdef DEBUG_QOS
			hevc_print(hevc, 0,
				"[Picture %d Quality] NO MV Data yet.\n",
				pic_number);
#endif
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* mvy_L1_count[39:32], mvx_L1_count[39:32],
		mvy_L0_count[39:32], mvx_L0_count[39:32] */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    /* should all be 0x00 or 0xff */
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] MV AVG High Bits: 0x%X\n",
			pic_number, rdata32);
#endif
	    mvx_L0_hi = ((rdata32>>0)&0xff);
	    mvy_L0_hi = ((rdata32>>8)&0xff);
	    mvx_L1_hi = ((rdata32>>16)&0xff);
	    mvy_L1_hi = ((rdata32>>24)&0xff);

		/* mvx_L0_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvx_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvx_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
		 value = div_s64(value, blk22_mv_count);
#ifdef DEBUG_QOS
		hevc_print(hevc, 0,
			"[Picture %d Quality] MVX_L0 AVG : %d (%lld/%d)\n",
			pic_number, (int)value,
			value, blk22_mv_count);
#endif
		picture->avg_mv = value;

		/* mvy_L0_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvy_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvy_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] MVY_L0 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);
#endif

		/* mvx_L1_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvx_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvx_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] MVX_L1 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);
#endif

		/* mvy_L1_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvy_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvy_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] MVY_L1 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);
#endif

		/* {mvx_L0_max, mvx_L0_min} // format : {sign, abs[14:0]}  */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32>>16)&0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] MVX_L0 MAX : %d\n",
			pic_number, mv_hi);
#endif
		picture->max_mv = mv_hi;

	    mv_lo = (rdata32>>0)&0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] MVX_L0 MIN : %d\n",
			pic_number, mv_lo);
#endif
		picture->min_mv = mv_lo;

		/* {mvy_L0_max, mvy_L0_min} */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32>>16)&0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] MVY_L0 MAX : %d\n",
			pic_number, mv_hi);
#endif

	    mv_lo = (rdata32>>0)&0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] MVY_L0 MIN : %d\n",
			pic_number, mv_lo);
#endif

		/* {mvx_L1_max, mvx_L1_min} */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32>>16)&0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] MVX_L1 MAX : %d\n",
			pic_number, mv_hi);
#endif

	    mv_lo = (rdata32>>0)&0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] MVX_L1 MIN : %d\n",
			pic_number, mv_lo);
#endif

		/* {mvy_L1_max, mvy_L1_min} */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32>>16)&0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] MVY_L1 MAX : %d\n",
			pic_number, mv_hi);
#endif
	    mv_lo = (rdata32>>0)&0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0, "[Picture %d Quality] MVY_L1 MIN : %d\n",
			pic_number, mv_lo);
#endif

	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_CTRL);
#ifdef DEBUG_QOS
	    hevc_print(hevc, 0,
			"[Picture %d Quality] After Read : VDEC_PIC_QUALITY_CTRL : 0x%x\n",
			pic_number, rdata32);
#endif
		/* reset all counts */
	    WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
	}
}

static int hevc_slice_segment_header_process(struct hevc_state_s *hevc,
		union param_u *rpm_param,
		int decode_pic_begin)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif
	int i;
	int lcu_x_num_div;
	int lcu_y_num_div;
	int Col_ref;
	int dbg_skip_flag = 0;

	if (hevc->wait_buf == 0) {
		hevc->sps_num_reorder_pics_0 =
			rpm_param->p.sps_num_reorder_pics_0;
		hevc->m_temporalId = rpm_param->p.m_temporalId;
		hevc->m_nalUnitType = rpm_param->p.m_nalUnitType;
		hevc->interlace_flag =
			(rpm_param->p.profile_etc >> 2) & 0x1;
		hevc->curr_pic_struct =
			(rpm_param->p.sei_frame_field_info >> 3) & 0xf;

		if (interlace_enable == 0 || hevc->m_ins_flag)
			hevc->interlace_flag = 0;
		if (interlace_enable & 0x100)
			hevc->interlace_flag = interlace_enable & 0x1;
		if (hevc->interlace_flag == 0)
			hevc->curr_pic_struct = 0;
		/* if(hevc->m_nalUnitType == NAL_UNIT_EOS){ */
		/*
		 *hevc->m_pocRandomAccess = MAX_INT;
		 *	//add to fix RAP_B_Bossen_1
		 */
		/* } */
		hevc->misc_flag0 = rpm_param->p.misc_flag0;
		if (rpm_param->p.first_slice_segment_in_pic_flag == 0) {
			hevc->slice_segment_addr =
				rpm_param->p.slice_segment_address;
			if (!rpm_param->p.dependent_slice_segment_flag)
				hevc->slice_addr = hevc->slice_segment_addr;
		} else {
			hevc->slice_segment_addr = 0;
			hevc->slice_addr = 0;
		}

		hevc->iPrevPOC = hevc->curr_POC;
		hevc->slice_type = (rpm_param->p.slice_type == I_SLICE) ? 2 :
			(rpm_param->p.slice_type == P_SLICE) ? 1 :
			(rpm_param->p.slice_type == B_SLICE) ? 0 : 3;
		/* hevc->curr_predFlag_L0=(hevc->slice_type==2) ? 0:1; */
		/* hevc->curr_predFlag_L1=(hevc->slice_type==0) ? 1:0; */
		hevc->TMVPFlag = rpm_param->p.slice_temporal_mvp_enable_flag;
		hevc->isNextSliceSegment =
			rpm_param->p.dependent_slice_segment_flag ? 1 : 0;
		if (hevc->pic_w != rpm_param->p.pic_width_in_luma_samples
			|| hevc->pic_h !=
			rpm_param->p.pic_height_in_luma_samples) {
			hevc_print(hevc, 0,
				"Pic Width/Height Change (%d,%d)=>(%d,%d), interlace %d\n",
				   hevc->pic_w, hevc->pic_h,
				   rpm_param->p.pic_width_in_luma_samples,
				   rpm_param->p.pic_height_in_luma_samples,
				   hevc->interlace_flag);

			hevc->pic_w = rpm_param->p.pic_width_in_luma_samples;
			hevc->pic_h = rpm_param->p.pic_height_in_luma_samples;
			hevc->frame_width = hevc->pic_w;
			hevc->frame_height = hevc->pic_h;
#ifdef LOSLESS_COMPRESS_MODE
			if (/*re_config_pic_flag == 0 &&*/
				(get_double_write_mode(hevc) & 0x10) == 0)
				init_decode_head_hw(hevc);
#endif
		}

		if (is_oversize(hevc->pic_w, hevc->pic_h)) {
			hevc_print(hevc, 0, "over size : %u x %u.\n",
				hevc->pic_w, hevc->pic_h);
			if ((!hevc->m_ins_flag) &&
				((debug &
				H265_NO_CHANG_DEBUG_FLAG_IN_CODE) == 0))
				debug |= (H265_DEBUG_DIS_LOC_ERROR_PROC |
				H265_DEBUG_DIS_SYS_ERROR_PROC);
			hevc->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
			return 3;
		}
		if (hevc->bit_depth_chroma > 10 ||
			hevc->bit_depth_luma > 10) {
			hevc_print(hevc, 0, "unsupport bitdepth : %u,%u\n",
				hevc->bit_depth_chroma,
				hevc->bit_depth_luma);
			if (!hevc->m_ins_flag)
				debug |= (H265_DEBUG_DIS_LOC_ERROR_PROC |
				H265_DEBUG_DIS_SYS_ERROR_PROC);
			hevc->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
			return 4;
		}

		/* it will cause divide 0 error */
		if (hevc->pic_w == 0 || hevc->pic_h == 0) {
			if (get_dbg_flag(hevc)) {
				hevc_print(hevc, 0,
					"Fatal Error, pic_w = %d, pic_h = %d\n",
					   hevc->pic_w, hevc->pic_h);
			}
			return 3;
		}
		pic_list_process(hevc);

		hevc->lcu_size =
			1 << (rpm_param->p.log2_min_coding_block_size_minus3 +
					3 + rpm_param->
					p.log2_diff_max_min_coding_block_size);
		if (hevc->lcu_size == 0) {
			hevc_print(hevc, 0,
				"Error, lcu_size = 0 (%d,%d)\n",
				   rpm_param->p.
				   log2_min_coding_block_size_minus3,
				   rpm_param->p.
				   log2_diff_max_min_coding_block_size);
			return 3;
		}
		hevc->lcu_size_log2 = log2i(hevc->lcu_size);
		lcu_x_num_div = (hevc->pic_w / hevc->lcu_size);
		lcu_y_num_div = (hevc->pic_h / hevc->lcu_size);
		hevc->lcu_x_num =
			((hevc->pic_w % hevc->lcu_size) ==
			 0) ? lcu_x_num_div : lcu_x_num_div + 1;
		hevc->lcu_y_num =
			((hevc->pic_h % hevc->lcu_size) ==
			 0) ? lcu_y_num_div : lcu_y_num_div + 1;
		hevc->lcu_total = hevc->lcu_x_num * hevc->lcu_y_num;

		if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR
			|| hevc->m_nalUnitType ==
			NAL_UNIT_CODED_SLICE_IDR_N_LP) {
			hevc->curr_POC = 0;
			if ((hevc->m_temporalId - 1) == 0)
				hevc->iPrevTid0POC = hevc->curr_POC;
		} else {
			int iMaxPOClsb =
				1 << (rpm_param->p.
				log2_max_pic_order_cnt_lsb_minus4 + 4);
			int iPrevPOClsb;
			int iPrevPOCmsb;
			int iPOCmsb;
			int iPOClsb = rpm_param->p.POClsb;

			if (iMaxPOClsb == 0) {
				hevc_print(hevc, 0,
					"error iMaxPOClsb is 0\n");
				return 3;
			}

			iPrevPOClsb = hevc->iPrevTid0POC % iMaxPOClsb;
			iPrevPOCmsb = hevc->iPrevTid0POC - iPrevPOClsb;

			if ((iPOClsb < iPrevPOClsb)
				&& ((iPrevPOClsb - iPOClsb) >=
					(iMaxPOClsb / 2)))
				iPOCmsb = iPrevPOCmsb + iMaxPOClsb;
			else if ((iPOClsb > iPrevPOClsb)
					 && ((iPOClsb - iPrevPOClsb) >
						 (iMaxPOClsb / 2)))
				iPOCmsb = iPrevPOCmsb - iMaxPOClsb;
			else
				iPOCmsb = iPrevPOCmsb;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
				hevc_print(hevc, 0,
				"iPrePOC%d iMaxPOClsb%d iPOCmsb%d iPOClsb%d\n",
				 hevc->iPrevTid0POC, iMaxPOClsb, iPOCmsb,
				 iPOClsb);
			}
			if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA
				|| hevc->m_nalUnitType ==
				NAL_UNIT_CODED_SLICE_BLANT
				|| hevc->m_nalUnitType ==
				NAL_UNIT_CODED_SLICE_BLA_N_LP) {
				/* For BLA picture types, POCmsb is set to 0. */
				iPOCmsb = 0;
			}
			hevc->curr_POC = (iPOCmsb + iPOClsb);
			if ((hevc->m_temporalId - 1) == 0)
				hevc->iPrevTid0POC = hevc->curr_POC;
			else {
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print(hevc, 0,
						"m_temporalID is %d\n",
						   hevc->m_temporalId);
				}
			}
		}
		hevc->RefNum_L0 =
			(rpm_param->p.num_ref_idx_l0_active >
			 MAX_REF_ACTIVE) ? MAX_REF_ACTIVE : rpm_param->p.
			num_ref_idx_l0_active;
		hevc->RefNum_L1 =
			(rpm_param->p.num_ref_idx_l1_active >
			 MAX_REF_ACTIVE) ? MAX_REF_ACTIVE : rpm_param->p.
			num_ref_idx_l1_active;

		/* if(curr_POC==0x10) dump_lmem(); */

		/* skip RASL pictures after CRA/BLA pictures */
		if (hevc->m_pocRandomAccess == MAX_INT) {/* first picture */
			if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA ||
				hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA
				|| hevc->m_nalUnitType ==
				NAL_UNIT_CODED_SLICE_BLANT
				|| hevc->m_nalUnitType ==
				NAL_UNIT_CODED_SLICE_BLA_N_LP)
				hevc->m_pocRandomAccess = hevc->curr_POC;
			else
				hevc->m_pocRandomAccess = -MAX_INT;
		} else if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA
				   || hevc->m_nalUnitType ==
				   NAL_UNIT_CODED_SLICE_BLANT
				   || hevc->m_nalUnitType ==
				   NAL_UNIT_CODED_SLICE_BLA_N_LP)
			hevc->m_pocRandomAccess = hevc->curr_POC;
		else if ((hevc->curr_POC < hevc->m_pocRandomAccess) &&
				(nal_skip_policy >= 3) &&
				 (hevc->m_nalUnitType ==
				  NAL_UNIT_CODED_SLICE_RASL_N ||
				  hevc->m_nalUnitType ==
				  NAL_UNIT_CODED_SLICE_TFD)) {	/* skip */
			if (get_dbg_flag(hevc)) {
				hevc_print(hevc, 0,
				"RASL picture with POC %d < %d ",
				 hevc->curr_POC, hevc->m_pocRandomAccess);
				hevc_print(hevc, 0,
					"RandomAccess point POC), skip it\n");
			}
			return 1;
			}

		WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG) | 0x2);
		hevc->skip_flag = 0;
		/**/
		/* if((iPrevPOC != curr_POC)){ */
		if (rpm_param->p.slice_segment_address == 0) {
			struct PIC_s *pic;

			hevc->new_pic = 1;
#ifdef MULTI_INSTANCE_SUPPORT
			if (!hevc->m_ins_flag)
#endif
				check_pic_decoded_error_pre(hevc,
					READ_VREG(HEVC_PARSER_LCU_START)
					& 0xffffff);
			/**/ if (use_cma == 0) {
				if (hevc->pic_list_init_flag == 0) {
					init_pic_list(hevc);
					init_pic_list_hw(hevc);
					init_buf_spec(hevc);
					hevc->pic_list_init_flag = 3;
				}
			}
			if (!hevc->m_ins_flag) {
				if (hevc->cur_pic)
					get_picture_qos_info(hevc);
			}
			hevc->first_pic_after_recover = 0;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE)
				dump_pic_list(hevc);
			/* prev pic */
			hevc_pre_pic(hevc, pic);
			/*
			 *update referenced of old pictures
			 *(cur_pic->referenced is 1 and not updated)
			 */
			apply_ref_pic_set(hevc, hevc->curr_POC,
							  rpm_param);

			if (hevc->mmu_enable)
				recycle_mmu_bufs(hevc);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (vdec->master) {
				struct hevc_state_s *hevc_ba =
				(struct hevc_state_s *)
					vdec->master->private;
				if (hevc_ba->cur_pic != NULL) {
					hevc_ba->cur_pic->dv_enhance_exist = 1;
					hevc_print(hevc, H265_DEBUG_DV,
					"To decode el (poc %d) => set bl (poc %d) dv_enhance_exist flag\n",
					hevc->curr_POC, hevc_ba->cur_pic->POC);
				}
			}
			if (vdec->master == NULL &&
				vdec->slave == NULL)
				set_aux_data(hevc,
					hevc->cur_pic, 1, 0); /*suffix*/
			if (hevc->bypass_dvenl && !dolby_meta_with_el)
				set_aux_data(hevc,
					hevc->cur_pic, 0, 1); /*dv meta only*/
#else
			set_aux_data(hevc, hevc->cur_pic, 1, 0);
#endif
			/* new pic */
			hevc->cur_pic = get_new_pic(hevc, rpm_param);
			if (hevc->cur_pic == NULL) {
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
					dump_pic_list(hevc);
				hevc->wait_buf = 1;
				return -1;
			}
#ifdef MULTI_INSTANCE_SUPPORT
			hevc->decoding_pic = hevc->cur_pic;
			if (!hevc->m_ins_flag)
				hevc->over_decode = 0;
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			hevc->cur_pic->dv_enhance_exist = 0;
			if (vdec->slave)
				hevc_print(hevc, H265_DEBUG_DV,
				"Clear bl (poc %d) dv_enhance_exist flag\n",
				hevc->curr_POC);
			if (vdec->master == NULL &&
				vdec->slave == NULL)
				set_aux_data(hevc,
					hevc->cur_pic, 0, 0); /*prefix*/

			if (hevc->bypass_dvenl && !dolby_meta_with_el)
				set_aux_data(hevc,
					hevc->cur_pic, 0, 2); /*pre sei only*/
#else
			set_aux_data(hevc, hevc->cur_pic, 0, 0);
#endif
			if (get_dbg_flag(hevc) & H265_DEBUG_DISPLAY_CUR_FRAME) {
				hevc->cur_pic->output_ready = 1;
				hevc->cur_pic->stream_offset =
					READ_VREG(HEVC_SHIFT_BYTE_COUNT);
				prepare_display_buf(hevc, hevc->cur_pic);
				hevc->wait_buf = 2;
				return -1;
			}
		} else {
			if (get_dbg_flag(hevc) & H265_DEBUG_HAS_AUX_IN_SLICE) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				if (vdec->master == NULL &&
					vdec->slave == NULL) {
					set_aux_data(hevc, hevc->cur_pic, 1, 0);
					set_aux_data(hevc, hevc->cur_pic, 0, 0);
				}
#else
				set_aux_data(hevc, hevc->cur_pic, 1, 0);
				set_aux_data(hevc, hevc->cur_pic, 0, 0);
#endif
			}
			if (hevc->pic_list_init_flag != 3
				|| hevc->cur_pic == NULL) {
				/* make it dec from the first slice segment */
				return 3;
			}
			hevc->cur_pic->slice_idx++;
			hevc->new_pic = 0;
		}
	} else {
	if (hevc->wait_buf == 1) {
			pic_list_process(hevc);
			hevc->cur_pic = get_new_pic(hevc, rpm_param);
			if (hevc->cur_pic == NULL)
				return -1;

			if (!hevc->m_ins_flag)
				hevc->over_decode = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			hevc->cur_pic->dv_enhance_exist = 0;
			if (vdec->master == NULL &&
				vdec->slave == NULL)
				set_aux_data(hevc, hevc->cur_pic, 0, 0);
#else
			set_aux_data(hevc, hevc->cur_pic, 0, 0);
#endif
			hevc->wait_buf = 0;
		} else if (hevc->wait_buf ==
				   2) {
			if (get_display_pic_num(hevc) >
				1)
				return -1;
			hevc->wait_buf = 0;
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE)
			dump_pic_list(hevc);
	}

	if (hevc->new_pic) {
#if 1
		/*SUPPORT_10BIT*/
		int sao_mem_unit =
				(hevc->lcu_size == 16 ? 9 :
						hevc->lcu_size ==
						32 ? 14 : 24) << 4;
#else
		int sao_mem_unit = ((hevc->lcu_size / 8) * 2 + 4) << 4;
#endif
		int pic_height_cu =
			(hevc->pic_h + hevc->lcu_size - 1) / hevc->lcu_size;
		int pic_width_cu =
			(hevc->pic_w + hevc->lcu_size - 1) / hevc->lcu_size;
		int sao_vb_size = (sao_mem_unit + (2 << 4)) * pic_height_cu;

		/* int sao_abv_size = sao_mem_unit*pic_width_cu; */
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"==>%s dec idx %d, struct %d interlace %d pic idx %d\n",
				__func__,
				hevc->decode_idx,
				hevc->curr_pic_struct,
				hevc->interlace_flag,
				hevc->cur_pic->index);
		}
		if (dbg_skip_decode_index != 0 &&
			hevc->decode_idx == dbg_skip_decode_index)
			dbg_skip_flag = 1;

		hevc->decode_idx++;
		update_tile_info(hevc, pic_width_cu, pic_height_cu,
						 sao_mem_unit, rpm_param);

		config_title_hw(hevc, sao_vb_size, sao_mem_unit);
	}

	if (hevc->iPrevPOC != hevc->curr_POC) {
		hevc->new_tile = 1;
		hevc->tile_x = 0;
		hevc->tile_y = 0;
		hevc->tile_y_x = 0;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"new_tile (new_pic) tile_x=%d, tile_y=%d\n",
				   hevc->tile_x, hevc->tile_y);
		}
	} else if (hevc->tile_enabled) {
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"slice_segment_address is %d\n",
				   rpm_param->p.slice_segment_address);
		}
		hevc->tile_y_x =
			get_tile_index(hevc, rpm_param->p.slice_segment_address,
						   (hevc->pic_w +
						    hevc->lcu_size -
							1) / hevc->lcu_size);
		if ((hevc->tile_y_x != (hevc->tile_x | (hevc->tile_y << 8)))
			&& (hevc->tile_y_x != -1)) {
			hevc->new_tile = 1;
			hevc->tile_x = hevc->tile_y_x & 0xff;
			hevc->tile_y = (hevc->tile_y_x >> 8) & 0xff;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
				hevc_print(hevc, 0,
				"new_tile seg adr %d tile_x=%d, tile_y=%d\n",
				 rpm_param->p.slice_segment_address,
				 hevc->tile_x, hevc->tile_y);
			}
		} else
			hevc->new_tile = 0;
	} else
		hevc->new_tile = 0;

	if ((hevc->tile_x > (MAX_TILE_COL_NUM - 1))
	|| (hevc->tile_y > (MAX_TILE_ROW_NUM - 1)))
		hevc->new_tile = 0;

	if (hevc->new_tile) {
		hevc->tile_start_lcu_x =
			hevc->m_tile[hevc->tile_y][hevc->tile_x].start_cu_x;
		hevc->tile_start_lcu_y =
			hevc->m_tile[hevc->tile_y][hevc->tile_x].start_cu_y;
		hevc->tile_width_lcu =
		    hevc->m_tile[hevc->tile_y][hevc->tile_x].width;
		hevc->tile_height_lcu =
			hevc->m_tile[hevc->tile_y][hevc->tile_x].height;
	}

	set_ref_pic_list(hevc, rpm_param);

	Col_ref = rpm_param->p.collocated_ref_idx;

	hevc->LDCFlag = 0;
	if (rpm_param->p.slice_type != I_SLICE) {
		hevc->LDCFlag = 1;
		for (i = 0; (i < hevc->RefNum_L0) && hevc->LDCFlag; i++) {
			if (hevc->cur_pic->
				m_aiRefPOCList0[hevc->cur_pic->slice_idx][i] >
				hevc->curr_POC)
				hevc->LDCFlag = 0;
		}
		if (rpm_param->p.slice_type == B_SLICE) {
			for (i = 0; (i < hevc->RefNum_L1)
					&& hevc->LDCFlag; i++) {
				if (hevc->cur_pic->
					m_aiRefPOCList1[hevc->cur_pic->
					slice_idx][i] >
					hevc->curr_POC)
					hevc->LDCFlag = 0;
			}
		}
	}

	hevc->ColFromL0Flag = rpm_param->p.collocated_from_l0_flag;

	hevc->plevel =
		rpm_param->p.log2_parallel_merge_level;
	hevc->MaxNumMergeCand = 5 - rpm_param->p.five_minus_max_num_merge_cand;

	hevc->LongTerm_Curr = 0;	/* to do ... */
	hevc->LongTerm_Col = 0;	/* to do ... */

	hevc->list_no = 0;
	if (rpm_param->p.slice_type == B_SLICE)
		hevc->list_no = 1 - hevc->ColFromL0Flag;
	if (hevc->list_no == 0) {
		if (Col_ref < hevc->RefNum_L0) {
			hevc->Col_POC =
				hevc->cur_pic->m_aiRefPOCList0[hevc->cur_pic->
				slice_idx][Col_ref];
		} else
			hevc->Col_POC = INVALID_POC;
	} else {
		if (Col_ref < hevc->RefNum_L1) {
			hevc->Col_POC =
				hevc->cur_pic->m_aiRefPOCList1[hevc->cur_pic->
				slice_idx][Col_ref];
		} else
			hevc->Col_POC = INVALID_POC;
	}

	hevc->LongTerm_Ref = 0;	/* to do ... */

	if (hevc->slice_type != 2) {
		/* if(hevc->i_only==1){ */
		/* return 0xf; */
		/* } */

		if (hevc->Col_POC != INVALID_POC) {
			hevc->col_pic = get_ref_pic_by_POC(hevc, hevc->Col_POC);
			if (hevc->col_pic == NULL) {
				hevc->cur_pic->error_mark = 1;
				if (get_dbg_flag(hevc)) {
					hevc_print(hevc, 0,
					"WRONG,fail to get the pic Col_POC\n");
				}
				if (is_log_enable(hevc))
					add_log(hevc,
					"WRONG,fail to get the pic Col_POC");
			} else if (hevc->col_pic->error_mark) {
				hevc->cur_pic->error_mark = 1;
				if (get_dbg_flag(hevc)) {
					hevc_print(hevc, 0,
					"WRONG, Col_POC error_mark is 1\n");
				}
				if (is_log_enable(hevc))
					add_log(hevc,
					"WRONG, Col_POC error_mark is 1");
			} else {
				if ((hevc->col_pic->width
					!= hevc->pic_w) ||
					(hevc->col_pic->height
					!= hevc->pic_h)) {
					hevc_print(hevc, 0,
						"Wrong reference pic (poc %d) width/height %d/%d\n",
						hevc->col_pic->POC,
						hevc->col_pic->width,
						hevc->col_pic->height);
					hevc->cur_pic->error_mark = 1;
				}

			}

			if (hevc->cur_pic->error_mark
				&& ((hevc->ignore_bufmgr_error & 0x1) == 0)) {
#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
				/*count info*/
				vdec_count_info(gvs, hevc->cur_pic->error_mark,
					hevc->cur_pic->stream_offset);
#endif
			}

			if (is_skip_decoding(hevc,
				hevc->cur_pic)) {
				return 2;
			}
		} else
			hevc->col_pic = hevc->cur_pic;
	}			/*  */
	if (hevc->col_pic == NULL)
		hevc->col_pic = hevc->cur_pic;
#ifdef BUFFER_MGR_ONLY
	return 0xf;
#else
	if ((decode_pic_begin > 0 && hevc->decode_idx <= decode_pic_begin)
		|| (dbg_skip_flag))
		return 0xf;
#endif

	config_mc_buffer(hevc, hevc->cur_pic);

	if (is_skip_decoding(hevc,
			hevc->cur_pic)) {
		if (get_dbg_flag(hevc))
			hevc_print(hevc, 0,
			"Discard this picture index %d\n",
					hevc->cur_pic->index);
#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
		/*count info*/
		vdec_count_info(gvs, hevc->cur_pic->error_mark,
			hevc->cur_pic->stream_offset);
#endif
		return 2;
	}
#ifdef MCRCC_ENABLE
	config_mcrcc_axi_hw(hevc, hevc->cur_pic->slice_type);
#endif
	config_mpred_hw(hevc);

	config_sao_hw(hevc, rpm_param);

	if ((hevc->slice_type != 2) && (hevc->i_only & 0x2))
		return 0xf;

	return 0;
}



static int H265_alloc_mmu(struct hevc_state_s *hevc, struct PIC_s *new_pic,
		unsigned short bit_depth, unsigned int *mmu_index_adr) {
	int cur_buf_idx = new_pic->index;
	int bit_depth_10 = (bit_depth != 0x00);
	int picture_size;
	int cur_mmu_4k_number;
	int ret, max_frame_num;
	picture_size = compute_losless_comp_body_size(hevc, new_pic->width,
				new_pic->height, !bit_depth_10);
	cur_mmu_4k_number = ((picture_size+(1<<12)-1) >> 12);
	if (hevc->double_write_mode & 0x10)
		return 0;
	/*hevc_print(hevc, 0,
	"alloc_mmu cur_idx : %d picture_size : %d mmu_4k_number : %d\r\n",
	cur_buf_idx, picture_size, cur_mmu_4k_number);*/
	if (new_pic->scatter_alloc) {
		decoder_mmu_box_free_idx(hevc->mmu_box, new_pic->index);
		new_pic->scatter_alloc = 0;
	}
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
		max_frame_num = MAX_FRAME_8K_NUM;
	else
		max_frame_num = MAX_FRAME_4K_NUM;
	if (cur_mmu_4k_number > max_frame_num) {
		hevc_print(hevc, 0, "over max !! 0x%x width %d height %d\n",
			cur_mmu_4k_number,
			new_pic->width,
			new_pic->height);
		return -1;
	}
	ret = decoder_mmu_box_alloc_idx(
	  hevc->mmu_box,
	  cur_buf_idx,
	  cur_mmu_4k_number,
	  mmu_index_adr);
	if (ret == 0)
		new_pic->scatter_alloc = 1;
	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
	"%s pic index %d page count(%d) ret =%d\n",
	__func__, cur_buf_idx,
	cur_mmu_4k_number,
	ret);
	return ret;
}


static void release_pic_mmu_buf(struct hevc_state_s *hevc,
	struct PIC_s *pic)
{
	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
	"%s pic index %d scatter_alloc %d\n",
	__func__, pic->index,
	pic->scatter_alloc);

	if (hevc->mmu_enable
		&& ((hevc->double_write_mode & 0x10) == 0)
		&& pic->scatter_alloc)
		decoder_mmu_box_free_idx(hevc->mmu_box, pic->index);
	pic->scatter_alloc = 0;
}

/*
 *************************************************
 *
 *h265 buffer management end
 *
 **************************************************
 */
static struct hevc_state_s *gHevc;

static void hevc_local_uninit(struct hevc_state_s *hevc)
{
	hevc->rpm_ptr = NULL;
	hevc->lmem_ptr = NULL;

#ifdef SWAP_HEVC_UCODE
	if (hevc->is_swap && get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		if (hevc->mc_cpu_addr != NULL) {
			dma_free_coherent(amports_get_dma_device(),
				hevc->swap_size, hevc->mc_cpu_addr,
				hevc->mc_dma_handle);
				hevc->mc_cpu_addr = NULL;
		}

	}
#endif
#ifdef DETREFILL_ENABLE
	if (hevc->is_swap && get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM)
		uninit_detrefill_buf(hevc);
#endif
	if (hevc->aux_addr) {
		dma_unmap_single(amports_get_dma_device(),
			hevc->aux_phy_addr,
			hevc->prefix_aux_size + hevc->suffix_aux_size,
			DMA_FROM_DEVICE);
		kfree(hevc->aux_addr);
		hevc->aux_addr = NULL;
	}
	if (hevc->rpm_addr) {
		dma_unmap_single(amports_get_dma_device(),
			hevc->rpm_phy_addr, RPM_BUF_SIZE, DMA_FROM_DEVICE);
		kfree(hevc->rpm_addr);
		hevc->rpm_addr = NULL;
	}
	if (hevc->lmem_addr) {
		dma_unmap_single(amports_get_dma_device(),
			hevc->lmem_phy_addr, LMEM_BUF_SIZE, DMA_FROM_DEVICE);
		kfree(hevc->lmem_addr);
		hevc->lmem_addr = NULL;
	}

	if (hevc->mmu_enable && hevc->frame_mmu_map_addr) {
		if (hevc->frame_mmu_map_phy_addr)
			dma_free_coherent(amports_get_dma_device(),
				get_frame_mmu_map_size(), hevc->frame_mmu_map_addr,
					hevc->frame_mmu_map_phy_addr);

		hevc->frame_mmu_map_addr = NULL;
	}

	kfree(gvs);
	gvs = NULL;
}

static int hevc_local_init(struct hevc_state_s *hevc)
{
	int ret = -1;
	struct BuffInfo_s *cur_buf_info = NULL;

	memset(&hevc->param, 0, sizeof(union param_u));

	cur_buf_info = &hevc->work_space_buf_store;

	if (vdec_is_support_4k()) {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
			memcpy(cur_buf_info, &amvh265_workbuff_spec[2],	/* 4k */
			sizeof(struct BuffInfo_s));
		else
			memcpy(cur_buf_info, &amvh265_workbuff_spec[1],	/* 4k */
			sizeof(struct BuffInfo_s));
	} else
		memcpy(cur_buf_info, &amvh265_workbuff_spec[0],	/* 1080p */
		sizeof(struct BuffInfo_s));

	cur_buf_info->start_adr = hevc->buf_start;
	init_buff_spec(hevc, cur_buf_info);

	hevc_init_stru(hevc, cur_buf_info);

	hevc->bit_depth_luma = 8;
	hevc->bit_depth_chroma = 8;
	hevc->video_signal_type = 0;
	hevc->video_signal_type_debug = 0;
	bit_depth_luma = hevc->bit_depth_luma;
	bit_depth_chroma = hevc->bit_depth_chroma;
	video_signal_type = hevc->video_signal_type;

	if ((get_dbg_flag(hevc) & H265_DEBUG_SEND_PARAM_WITH_REG) == 0) {
		hevc->rpm_addr = kmalloc(RPM_BUF_SIZE, GFP_KERNEL);
		if (hevc->rpm_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}

		hevc->rpm_phy_addr = dma_map_single(amports_get_dma_device(),
			hevc->rpm_addr, RPM_BUF_SIZE, DMA_FROM_DEVICE);
		if (dma_mapping_error(amports_get_dma_device(),
			hevc->rpm_phy_addr)) {
			pr_err("%s: failed to map rpm buffer\n", __func__);
			kfree(hevc->rpm_addr);
			hevc->rpm_addr = NULL;
			return -1;
		}

		hevc->rpm_ptr = hevc->rpm_addr;
	}

	if (prefix_aux_buf_size > 0 ||
		suffix_aux_buf_size > 0) {
		u32 aux_buf_size;

		hevc->prefix_aux_size = AUX_BUF_ALIGN(prefix_aux_buf_size);
		hevc->suffix_aux_size = AUX_BUF_ALIGN(suffix_aux_buf_size);
		aux_buf_size = hevc->prefix_aux_size + hevc->suffix_aux_size;
		hevc->aux_addr = kmalloc(aux_buf_size, GFP_KERNEL);
		if (hevc->aux_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}

		hevc->aux_phy_addr = dma_map_single(amports_get_dma_device(),
			hevc->aux_addr, aux_buf_size, DMA_FROM_DEVICE);
		if (dma_mapping_error(amports_get_dma_device(),
			hevc->aux_phy_addr)) {
			pr_err("%s: failed to map rpm buffer\n", __func__);
			kfree(hevc->aux_addr);
			hevc->aux_addr = NULL;
			return -1;
		}
	}

	hevc->lmem_addr = kmalloc(LMEM_BUF_SIZE, GFP_KERNEL);
	if (hevc->lmem_addr == NULL) {
		pr_err("%s: failed to alloc lmem buffer\n", __func__);
		return -1;
	}
	hevc->lmem_phy_addr = dma_map_single(amports_get_dma_device(),
		hevc->lmem_addr, LMEM_BUF_SIZE, DMA_FROM_DEVICE);
	if (dma_mapping_error(amports_get_dma_device(),
		hevc->lmem_phy_addr)) {
		pr_err("%s: failed to map lmem buffer\n", __func__);
		kfree(hevc->lmem_addr);
		hevc->lmem_addr = NULL;
		return -1;
	}
	hevc->lmem_ptr = hevc->lmem_addr;

	if (hevc->mmu_enable) {
		hevc->frame_mmu_map_addr =
				dma_alloc_coherent(amports_get_dma_device(),
				get_frame_mmu_map_size(),
				&hevc->frame_mmu_map_phy_addr, GFP_KERNEL);
		if (hevc->frame_mmu_map_addr == NULL) {
			pr_err("%s: failed to alloc count_buffer\n", __func__);
			return -1;
		}
		memset(hevc->frame_mmu_map_addr, 0, get_frame_mmu_map_size());
	}
	ret = 0;
	return ret;
}

/*
 *******************************************
 *  Mailbox command
 *******************************************
 */
#define CMD_FINISHED               0
#define CMD_ALLOC_VIEW             1
#define CMD_FRAME_DISPLAY          3
#define CMD_DEBUG                  10


#define DECODE_BUFFER_NUM_MAX    32
#define DISPLAY_BUFFER_NUM       6

#define video_domain_addr(adr) (adr&0x7fffffff)
#define DECODER_WORK_SPACE_SIZE 0x800000

#define spec2canvas(x)  \
	(((x)->uv_canvas_index << 16) | \
	 ((x)->uv_canvas_index << 8)  | \
	 ((x)->y_canvas_index << 0))


static void set_canvas(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);
	int canvas_w = ALIGN(pic->width, 64)/4;
	int canvas_h = ALIGN(pic->height, 32)/4;
	int blkmode = mem_map_mode;

	/*CANVAS_BLKMODE_64X32*/
#ifdef SUPPORT_10BIT
	if	(pic->double_write_mode) {
		canvas_w = pic->width /
			get_double_write_ratio(hevc, pic->double_write_mode);
		canvas_h = pic->height /
			get_double_write_ratio(hevc, pic->double_write_mode);

		if (mem_map_mode == 0)
			canvas_w = ALIGN(canvas_w, 32);
		else
			canvas_w = ALIGN(canvas_w, 64);
		canvas_h = ALIGN(canvas_h, 32);

		if (vdec->parallel_dec == 1) {
			if (pic->y_canvas_index == -1)
				pic->y_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
			if (pic->uv_canvas_index == -1)
				pic->uv_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
		} else {
			pic->y_canvas_index = 128 + pic->index * 2;
			pic->uv_canvas_index = 128 + pic->index * 2 + 1;
		}

		canvas_config_ex(pic->y_canvas_index,
			pic->dw_y_adr, canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0x7);
		canvas_config_ex(pic->uv_canvas_index, pic->dw_u_v_adr,
			canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0x7);
#ifdef MULTI_INSTANCE_SUPPORT
		pic->canvas_config[0].phy_addr =
				pic->dw_y_adr;
		pic->canvas_config[0].width =
				canvas_w;
		pic->canvas_config[0].height =
				canvas_h;
		pic->canvas_config[0].block_mode =
				blkmode;
		pic->canvas_config[0].endian = 7;

		pic->canvas_config[1].phy_addr =
				pic->dw_u_v_adr;
		pic->canvas_config[1].width =
				canvas_w;
		pic->canvas_config[1].height =
				canvas_h;
		pic->canvas_config[1].block_mode =
				blkmode;
		pic->canvas_config[1].endian = 7;
#endif
	} else {
		if (!hevc->mmu_enable) {
			/* to change after 10bit VPU is ready ... */
			if (vdec->parallel_dec == 1) {
				if (pic->y_canvas_index == -1)
					pic->y_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
				pic->uv_canvas_index = pic->y_canvas_index;
			} else {
				pic->y_canvas_index = 128 + pic->index;
				pic->uv_canvas_index = 128 + pic->index;
			}

			canvas_config_ex(pic->y_canvas_index,
				pic->mc_y_adr, canvas_w, canvas_h,
				CANVAS_ADDR_NOWRAP, blkmode, 0x7);
			canvas_config_ex(pic->uv_canvas_index, pic->mc_u_v_adr,
				canvas_w, canvas_h,
				CANVAS_ADDR_NOWRAP, blkmode, 0x7);
		}
	}
#else
	if (vdec->parallel_dec == 1) {
		if (pic->y_canvas_index == -1)
			pic->y_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
		if (pic->uv_canvas_index == -1)
			pic->uv_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
	} else {
		pic->y_canvas_index = 128 + pic->index * 2;
		pic->uv_canvas_index = 128 + pic->index * 2 + 1;
	}


	canvas_config_ex(pic->y_canvas_index, pic->mc_y_adr, canvas_w, canvas_h,
		CANVAS_ADDR_NOWRAP, blkmode, 0x7);
	canvas_config_ex(pic->uv_canvas_index, pic->mc_u_v_adr,
		canvas_w, canvas_h,
		CANVAS_ADDR_NOWRAP, blkmode, 0x7);
#endif
}

static int init_buf_spec(struct hevc_state_s *hevc)
{
	int pic_width = hevc->pic_w;
	int pic_height = hevc->pic_h;

	/* hevc_print(hevc, 0,
	 *"%s1: %d %d\n", __func__, hevc->pic_w, hevc->pic_h);
	 */
	hevc_print(hevc, 0,
		"%s2 %d %d\n", __func__, pic_width, pic_height);
	/* pic_width = hevc->pic_w; */
	/* pic_height = hevc->pic_h; */

	if (hevc->frame_width == 0 || hevc->frame_height == 0) {
		hevc->frame_width = pic_width;
		hevc->frame_height = pic_height;

	}

	return 0;
}

static int parse_sei(struct hevc_state_s *hevc,
	struct PIC_s *pic, char *sei_buf, uint32_t size)
{
	char *p = sei_buf;
	char *p_sei;
	uint16_t header;
	uint8_t nal_unit_type;
	uint8_t payload_type, payload_size;
	int i, j;

	if (size < 2)
		return 0;
	header = *p++;
	header <<= 8;
	header += *p++;
	nal_unit_type = header >> 9;
	if ((nal_unit_type != NAL_UNIT_SEI)
	&& (nal_unit_type != NAL_UNIT_SEI_SUFFIX))
		return 0;
	while (p+2 <= sei_buf+size) {
		payload_type = *p++;
		payload_size = *p++;
		if (p+payload_size <= sei_buf+size) {
			switch (payload_type) {
			case SEI_PicTiming:
				p_sei = p;
				hevc->curr_pic_struct = (*p_sei >> 4)&0x0f;
				pic->pic_struct = hevc->curr_pic_struct;
				if (get_dbg_flag(hevc) &
					H265_DEBUG_PIC_STRUCT) {
					hevc_print(hevc, 0,
					"parse result pic_struct = %d\n",
					hevc->curr_pic_struct);
				}
				break;
			case SEI_UserDataITU_T_T35:
				p_sei = p;
				if (p_sei[0] == 0xB5
					&& p_sei[1] == 0x00
					&& p_sei[2] == 0x3C
					&& p_sei[3] == 0x00
					&& p_sei[4] == 0x01
					&& p_sei[5] == 0x04)
					hevc->sei_present_flag |= SEI_HDR10PLUS_MASK;

				break;
			case SEI_MasteringDisplayColorVolume:
				/*hevc_print(hevc, 0,
					"sei type: primary display color volume %d, size %d\n",
					payload_type,
					payload_size);*/
				/* master_display_colour */
				p_sei = p;
				for (i = 0; i < 3; i++) {
					for (j = 0; j < 2; j++) {
						hevc->primaries[i][j]
							= (*p_sei<<8)
							| *(p_sei+1);
						p_sei += 2;
					}
				}
				for (i = 0; i < 2; i++) {
					hevc->white_point[i]
						= (*p_sei<<8)
						| *(p_sei+1);
					p_sei += 2;
				}
				for (i = 0; i < 2; i++) {
					hevc->luminance[i]
						= (*p_sei<<24)
						| (*(p_sei+1)<<16)
						| (*(p_sei+2)<<8)
						| *(p_sei+3);
					p_sei += 4;
				}
				hevc->sei_present_flag |=
					SEI_MASTER_DISPLAY_COLOR_MASK;
				/*for (i = 0; i < 3; i++)
					for (j = 0; j < 2; j++)
						hevc_print(hevc, 0,
						"\tprimaries[%1d][%1d] = %04x\n",
						i, j,
						hevc->primaries[i][j]);
				hevc_print(hevc, 0,
					"\twhite_point = (%04x, %04x)\n",
					hevc->white_point[0],
					hevc->white_point[1]);
				hevc_print(hevc, 0,
					"\tmax,min luminance = %08x, %08x\n",
					hevc->luminance[0],
					hevc->luminance[1]);*/
				break;
			case SEI_ContentLightLevel:
				if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
					hevc_print(hevc, 0,
						"sei type: max content light level %d, size %d\n",
					payload_type, payload_size);
				/* content_light_level */
				p_sei = p;
				hevc->content_light_level[0]
					= (*p_sei<<8) | *(p_sei+1);
				p_sei += 2;
				hevc->content_light_level[1]
					= (*p_sei<<8) | *(p_sei+1);
				p_sei += 2;
				hevc->sei_present_flag |=
					SEI_CONTENT_LIGHT_LEVEL_MASK;
				if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
					hevc_print(hevc, 0,
						"\tmax cll = %04x, max_pa_cll = %04x\n",
					hevc->content_light_level[0],
					hevc->content_light_level[1]);
				break;
			default:
				break;
			}
		}
		p += payload_size;
	}
	return 0;
}

static unsigned calc_ar(unsigned idc, unsigned sar_w, unsigned sar_h,
			unsigned w, unsigned h)
{
	unsigned ar;

	if (idc	== 255) {
		ar = div_u64(256ULL * sar_h * h,
				sar_w * w);
	} else {
		switch (idc) {
		case 1:
			ar = 0x100 * h / w;
			break;
		case 2:
			ar = 0x100 * h * 11 / (w * 12);
			break;
		case 3:
			ar = 0x100 * h * 11 / (w * 10);
			break;
		case 4:
			ar = 0x100 * h * 11 / (w * 16);
			break;
		case 5:
			ar = 0x100 * h * 33 / (w * 40);
			break;
		case 6:
			ar = 0x100 * h * 11 / (w * 24);
			break;
		case 7:
			ar = 0x100 * h * 11 / (w * 20);
			break;
		case 8:
			ar = 0x100 * h * 11 / (w * 32);
			break;
		case 9:
			ar = 0x100 * h * 33 / (w * 80);
			break;
		case 10:
			ar = 0x100 * h * 11 / (w * 18);
			break;
		case 11:
			ar = 0x100 * h * 11 / (w * 15);
			break;
		case 12:
			ar = 0x100 * h * 33 / (w * 64);
			break;
		case 13:
			ar = 0x100 * h * 99 / (w * 160);
			break;
		case 14:
			ar = 0x100 * h * 3 / (w * 4);
			break;
		case 15:
			ar = 0x100 * h * 2 / (w * 3);
			break;
		case 16:
			ar = 0x100 * h * 1 / (w * 2);
			break;
		default:
			ar = h * 0x100 / w;
			break;
		}
	}

	return ar;
}

static void set_frame_info(struct hevc_state_s *hevc, struct vframe_s *vf,
			struct PIC_s *pic)
{
	unsigned int ar;
	int i, j;
	char *p;
	unsigned size = 0;
	unsigned type = 0;
	struct vframe_master_display_colour_s *vf_dp
		= &vf->prop.master_display_colour;

	vf->width = pic->width /
		get_double_write_ratio(hevc, pic->double_write_mode);
	vf->height = pic->height /
		get_double_write_ratio(hevc, pic->double_write_mode);

	vf->duration = hevc->frame_dur;
	vf->duration_pulldown = 0;
	vf->flag = 0;

	ar = min_t(u32, hevc->frame_ar, DISP_RATIO_ASPECT_RATIO_MAX);
	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);


	if (((pic->aspect_ratio_idc == 255) &&
		pic->sar_width &&
		pic->sar_height) ||
		((pic->aspect_ratio_idc != 255) &&
		(pic->width))) {
		ar = min_t(u32,
			calc_ar(pic->aspect_ratio_idc,
			pic->sar_width,
			pic->sar_height,
			pic->width,
			pic->height),
			DISP_RATIO_ASPECT_RATIO_MAX);
		vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);
	}
	hevc->ratio_control = vf->ratio_control;
	if (pic->aux_data_buf
		&& pic->aux_data_size) {
		/* parser sei */
		p = pic->aux_data_buf;
		while (p < pic->aux_data_buf
			+ pic->aux_data_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (type == 0x02000000) {
				/* hevc_print(hevc, 0,
				"sei(%d)\n", size); */
				parse_sei(hevc, pic, p, size);
			}
			p += size;
		}
	}
	if (hevc->video_signal_type & VIDEO_SIGNAL_TYPE_AVAILABLE_MASK) {
		vf->signal_type = pic->video_signal_type;
		if (hevc->sei_present_flag & SEI_HDR10PLUS_MASK) {
			u32 data;
			data = vf->signal_type;
			data = data & 0xFFFF00FF;
			data = data | (0x30<<8);
			vf->signal_type = data;
		}
	}
	else
		vf->signal_type = 0;
	hevc->video_signal_type_debug = vf->signal_type;

	/* master_display_colour */
	if (hevc->sei_present_flag & SEI_MASTER_DISPLAY_COLOR_MASK) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++)
				vf_dp->primaries[i][j] = hevc->primaries[i][j];
		for (i = 0; i < 2; i++) {
			vf_dp->white_point[i] = hevc->white_point[i];
			vf_dp->luminance[i]
				= hevc->luminance[i];
		}
		vf_dp->present_flag = 1;
	} else
		vf_dp->present_flag = 0;

	/* content_light_level */
	if (hevc->sei_present_flag & SEI_CONTENT_LIGHT_LEVEL_MASK) {
		vf_dp->content_light_level.max_content
			= hevc->content_light_level[0];
		vf_dp->content_light_level.max_pic_average
			= hevc->content_light_level[1];
		vf_dp->content_light_level.present_flag = 1;
	} else
		vf_dp->content_light_level.present_flag = 0;
}

static int vh265_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif

	spin_lock_irqsave(&lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hevc->newframe_q);
	states->buf_avail_num = kfifo_len(&hevc->display_q);

	if (step == 2)
		states->buf_avail_num = 0;
	spin_unlock_irqrestore(&lock, flags);
	return 0;
}

static struct vframe_s *vh265_vf_peek(void *op_arg)
{
	struct vframe_s *vf[2] = {0, 0};
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif

	if (step == 2)
		return NULL;

	if (force_disp_pic_index & 0x100) {
		if (force_disp_pic_index & 0x200)
			return NULL;
		return &hevc->vframe_dummy;
	}


	if (kfifo_out_peek(&hevc->display_q, (void *)&vf, 2)) {
		if (vf[1]) {
			vf[0]->next_vf_pts_valid = true;
			vf[0]->next_vf_pts = vf[1]->pts;
		} else
			vf[0]->next_vf_pts_valid = false;
		return vf[0];
	}

	return NULL;
}

static struct vframe_s *vh265_vf_get(void *op_arg)
{
	struct vframe_s *vf;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif

	if (step == 2)
		return NULL;
	else if (step == 1)
		step = 2;

#if 0
	if (force_disp_pic_index & 0x100) {
		int buffer_index = force_disp_pic_index & 0xff;
		struct PIC_s *pic = NULL;
		if (buffer_index >= 0
			&& buffer_index < MAX_REF_PIC_NUM)
			pic = hevc->m_PIC[buffer_index];
		if (pic == NULL)
			return NULL;
		if (force_disp_pic_index & 0x200)
			return NULL;

		vf = &hevc->vframe_dummy;
		if (get_double_write_mode(hevc)) {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
			if (hevc->m_ins_flag) {
				vf->canvas0Addr = vf->canvas1Addr = -1;
				vf->plane_num = 2;
				vf->canvas0_config[0] =
					pic->canvas_config[0];
				vf->canvas0_config[1] =
					pic->canvas_config[1];

				vf->canvas1_config[0] =
					pic->canvas_config[0];
				vf->canvas1_config[1] =
					pic->canvas_config[1];
			} else {
				vf->canvas0Addr = vf->canvas1Addr
				= spec2canvas(pic);
			}
		} else {
			vf->canvas0Addr = vf->canvas1Addr = 0;
			vf->type = VIDTYPE_COMPRESS | VIDTYPE_VIU_FIELD;
			if (hevc->mmu_enable)
				vf->type |= VIDTYPE_SCATTER;
		}
		vf->compWidth = pic->width;
		vf->compHeight = pic->height;
		update_vf_memhandle(hevc, vf, pic);
		switch (hevc->bit_depth_luma) {
		case 9:
			vf->bitdepth = BITDEPTH_Y9 | BITDEPTH_U9 | BITDEPTH_V9;
			break;
		case 10:
			vf->bitdepth = BITDEPTH_Y10 | BITDEPTH_U10
				| BITDEPTH_V10;
			break;
		default:
			vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
			break;
		}
		if ((vf->type & VIDTYPE_COMPRESS) == 0)
			vf->bitdepth =
				BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		if (hevc->mem_saving_mode == 1)
			vf->bitdepth |= BITDEPTH_SAVING_MODE;
		vf->duration_pulldown = 0;
		vf->pts = 0;
		vf->pts_us64 = 0;
		set_frame_info(hevc, vf);

		vf->width = pic->width /
			get_double_write_ratio(hevc, pic->double_write_mode);
		vf->height = pic->height /
			get_double_write_ratio(hevc, pic->double_write_mode);

		force_disp_pic_index |= 0x200;
		return vf;
	}
#endif

	if (kfifo_get(&hevc->display_q, &vf)) {
		struct vframe_s *next_vf;
		if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
			hevc_print(hevc, 0,
				"%s(vf 0x%p type %d index 0x%x poc %d/%d) pts(%d,%d) dur %d\n",
				__func__, vf, vf->type, vf->index,
				get_pic_poc(hevc, vf->index & 0xff),
				get_pic_poc(hevc, (vf->index >> 8) & 0xff),
				vf->pts, vf->pts_us64,
				vf->duration);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (get_dbg_flag(hevc) & H265_DEBUG_DV) {
			struct PIC_s *pic = hevc->m_PIC[vf->index & 0xff];
			if (pic->aux_data_buf && pic->aux_data_size > 0) {
				int i;
				struct PIC_s *pic =
					hevc->m_PIC[vf->index & 0xff];
				hevc_print(hevc, 0,
					"pic 0x%p aux size %d:\n",
					pic, pic->aux_data_size);
				for (i = 0; i < pic->aux_data_size; i++) {
					hevc_print_cont(hevc, 0,
						"%02x ", pic->aux_data_buf[i]);
					if (((i + 1) & 0xf) == 0)
						hevc_print_cont(hevc, 0, "\n");
				}
				hevc_print_cont(hevc, 0, "\n");
			}
		}
#endif
		hevc->show_frame_num++;
		hevc->vf_get_count++;

		if (kfifo_peek(&hevc->display_q, &next_vf)) {
			vf->next_vf_pts_valid = true;
			vf->next_vf_pts = next_vf->pts;
		} else
			vf->next_vf_pts_valid = false;

		return vf;
	}

	return NULL;
}

static void vh265_vf_put(struct vframe_s *vf, void *op_arg)
{
	unsigned long flags;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif
	unsigned char index_top = vf->index & 0xff;
	unsigned char index_bot = (vf->index >> 8) & 0xff;
	if (vf == (&hevc->vframe_dummy))
		return;
	if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
		hevc_print(hevc, 0,
			"%s(type %d index 0x%x)\n",
			__func__, vf->type, vf->index);
	hevc->vf_put_count++;
	kfifo_put(&hevc->newframe_q, (const struct vframe_s *)vf);
	spin_lock_irqsave(&lock, flags);

	if (index_top != 0xff
		&& index_top < MAX_REF_PIC_NUM
		&& hevc->m_PIC[index_top]) {
		if (hevc->m_PIC[index_top]->vf_ref > 0) {
			hevc->m_PIC[index_top]->vf_ref--;

			if (hevc->m_PIC[index_top]->vf_ref == 0) {
				hevc->m_PIC[index_top]->output_ready = 0;

				if (hevc->wait_buf != 0)
					WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG,
						0x1);
			}
		}
	}

	if (index_bot != 0xff
		&& index_bot < MAX_REF_PIC_NUM
		&& hevc->m_PIC[index_bot]) {
		if (hevc->m_PIC[index_bot]->vf_ref > 0) {
			hevc->m_PIC[index_bot]->vf_ref--;

			if (hevc->m_PIC[index_bot]->vf_ref == 0) {
				hevc->m_PIC[index_bot]->output_ready = 0;
				if (hevc->wait_buf != 0)
					WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG,
						0x1);
			}
		}
	}
	spin_unlock_irqrestore(&lock, flags);
}

static int vh265_event_cb(int type, void *data, void *op_arg)
{
	unsigned long flags;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif
	if (type & VFRAME_EVENT_RECEIVER_RESET) {
#if 0
		amhevc_stop();
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vf_light_unreg_provider(&vh265_vf_prov);
#endif
		spin_lock_irqsave(&hevc->lock, flags);
		vh265_local_init();
		vh265_prot_init();
		spin_unlock_irqrestore(&hevc->lock, flags);
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vf_reg_provider(&vh265_vf_prov);
#endif
		amhevc_start();
#endif
	} else if (type & VFRAME_EVENT_RECEIVER_GET_AUX_DATA) {
		struct provider_aux_req_s *req =
			(struct provider_aux_req_s *)data;
		unsigned char index;

		spin_lock_irqsave(&lock, flags);
		index = req->vf->index & 0xff;
		req->aux_buf = NULL;
		req->aux_size = 0;
		if (req->bot_flag)
			index = (req->vf->index >> 8) & 0xff;
		if (index != 0xff
			&& index < MAX_REF_PIC_NUM
			&& hevc->m_PIC[index]) {
			req->aux_buf = hevc->m_PIC[index]->aux_data_buf;
			req->aux_size = hevc->m_PIC[index]->aux_data_size;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (hevc->bypass_dvenl && !dolby_meta_with_el)
				req->dv_enhance_exist = false;
			else
				req->dv_enhance_exist =
					hevc->m_PIC[index]->dv_enhance_exist;
			hevc_print(hevc, H265_DEBUG_DV,
			"query dv_enhance_exist for pic (vf 0x%p, poc %d index %d) flag => %d, aux sizd 0x%x\n",
			req->vf,
			hevc->m_PIC[index]->POC, index,
			req->dv_enhance_exist, req->aux_size);
#else
			req->dv_enhance_exist = 0;
#endif
		}
		spin_unlock_irqrestore(&lock, flags);

		if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
			hevc_print(hevc, 0,
			"%s(type 0x%x vf index 0x%x)=>size 0x%x\n",
			__func__, type, index, req->aux_size);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	} else if (type & VFRAME_EVENT_RECEIVER_DOLBY_BYPASS_EL) {
		if ((force_bypass_dvenl & 0x80000000) == 0) {
			hevc_print(hevc, 0,
			"%s: VFRAME_EVENT_RECEIVER_DOLBY_BYPASS_EL\n",
			__func__);
			hevc->bypass_dvenl_enable = 1;
		}

#endif
	}
	return 0;
}

#ifdef HEVC_PIC_STRUCT_SUPPORT
static int process_pending_vframe(struct hevc_state_s *hevc,
	struct PIC_s *pair_pic, unsigned char pair_frame_top_flag)
{
	struct vframe_s *vf;

	if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
		hevc_print(hevc, 0,
			"%s: pair_pic index 0x%x %s\n",
			__func__, pair_pic->index,
			pair_frame_top_flag ?
			"top" : "bot");

	if (kfifo_len(&hevc->pending_q) > 1) {
		/* do not pending more than 1 frame */
		if (kfifo_get(&hevc->pending_q, &vf) == 0) {
			hevc_print(hevc, 0,
			"fatal error, no available buffer slot.");
			return -1;
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
			hevc_print(hevc, 0,
			"%s warning(1), vf=>display_q: (index 0x%x)\n",
				__func__, vf->index);
		hevc->vf_pre_count++;
		kfifo_put(&hevc->display_q, (const struct vframe_s *)vf);
		ATRACE_COUNTER(MODULE_NAME, vf->pts);
	}

	if (kfifo_peek(&hevc->pending_q, &vf)) {
		if (pair_pic == NULL || pair_pic->vf_ref <= 0) {
			/*
			 *if pair_pic is recycled (pair_pic->vf_ref <= 0),
			 *do not use it
			 */
			if (kfifo_get(&hevc->pending_q, &vf) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
				"%s warning(2), vf=>display_q: (index 0x%x)\n",
				__func__, vf->index);
			if (vf) {
				hevc->vf_pre_count++;
				kfifo_put(&hevc->display_q,
				(const struct vframe_s *)vf);
				ATRACE_COUNTER(MODULE_NAME, vf->pts);
			}
		} else if ((!pair_frame_top_flag) &&
			(((vf->index >> 8) & 0xff) == 0xff)) {
			if (kfifo_get(&hevc->pending_q, &vf) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			if (vf) {
				vf->type = VIDTYPE_PROGRESSIVE
				| VIDTYPE_VIU_NV21;
				vf->index &= 0xff;
				vf->index |= (pair_pic->index << 8);
				vf->canvas1Addr = spec2canvas(pair_pic);
				pair_pic->vf_ref++;
				kfifo_put(&hevc->display_q,
				(const struct vframe_s *)vf);
				ATRACE_COUNTER(MODULE_NAME, vf->pts);
				hevc->vf_pre_count++;
				if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
					hevc_print(hevc, 0,
					"%s vf => display_q: (index 0x%x)\n",
					__func__, vf->index);
			}
		} else if (pair_frame_top_flag &&
			((vf->index & 0xff) == 0xff)) {
			if (kfifo_get(&hevc->pending_q, &vf) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			if (vf) {
				vf->type = VIDTYPE_PROGRESSIVE
				| VIDTYPE_VIU_NV21;
				vf->index &= 0xff00;
				vf->index |= pair_pic->index;
				vf->canvas0Addr = spec2canvas(pair_pic);
				pair_pic->vf_ref++;
				kfifo_put(&hevc->display_q,
				(const struct vframe_s *)vf);
				ATRACE_COUNTER(MODULE_NAME, vf->pts);
				hevc->vf_pre_count++;
				if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
					hevc_print(hevc, 0,
					"%s vf => display_q: (index 0x%x)\n",
					__func__, vf->index);
			}
		}
	}
	return 0;
}
#endif
static void update_vf_memhandle(struct hevc_state_s *hevc,
	struct vframe_s *vf, struct PIC_s *pic)
{
	if (pic->index < 0) {
		vf->mem_handle = NULL;
		vf->mem_head_handle = NULL;
	} else if (vf->type & VIDTYPE_SCATTER) {
		vf->mem_handle =
			decoder_mmu_box_get_mem_handle(
				hevc->mmu_box, pic->index);
		vf->mem_head_handle =
			decoder_bmmu_box_get_mem_handle(
				hevc->bmmu_box, VF_BUFFER_IDX(pic->BUF_index));
	} else {
		vf->mem_handle =
			decoder_bmmu_box_get_mem_handle(
				hevc->bmmu_box, VF_BUFFER_IDX(pic->BUF_index));
		vf->mem_head_handle = NULL;
		/*vf->mem_head_handle =
			decoder_bmmu_box_get_mem_handle(
				hevc->bmmu_box, VF_BUFFER_IDX(BUF_index));*/
	}
	return;
}

static void fill_frame_info(struct hevc_state_s *hevc,
	struct PIC_s *pic, unsigned int framesize, unsigned int pts)
{
	struct vframe_qos_s *vframe_qos = &hevc->vframe_qos;
	if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR)
		vframe_qos->type = 4;
	else if (pic->slice_type == I_SLICE)
		vframe_qos->type = 1;
	else if (pic->slice_type == P_SLICE)
		vframe_qos->type = 2;
	else if (pic->slice_type == B_SLICE)
		vframe_qos->type = 3;
/*
#define SHOW_QOS_INFO
*/
	vframe_qos->size = framesize;
	vframe_qos->pts = pts;
#ifdef SHOW_QOS_INFO
	hevc_print(hevc, 0, "slice:%d, poc:%d\n", pic->slice_type, pic->POC);
#endif


	vframe_qos->max_mv = pic->max_mv;
	vframe_qos->avg_mv = pic->avg_mv;
	vframe_qos->min_mv = pic->min_mv;
#ifdef SHOW_QOS_INFO
	hevc_print(hevc, 0, "mv: max:%d,  avg:%d, min:%d\n",
			vframe_qos->max_mv,
			vframe_qos->avg_mv,
			vframe_qos->min_mv);
#endif

	vframe_qos->max_qp = pic->max_qp;
	vframe_qos->avg_qp = pic->avg_qp;
	vframe_qos->min_qp = pic->min_qp;
#ifdef SHOW_QOS_INFO
	hevc_print(hevc, 0, "qp: max:%d,  avg:%d, min:%d\n",
			vframe_qos->max_qp,
			vframe_qos->avg_qp,
			vframe_qos->min_qp);
#endif

	vframe_qos->max_skip = pic->max_skip;
	vframe_qos->avg_skip = pic->avg_skip;
	vframe_qos->min_skip = pic->min_skip;
#ifdef SHOW_QOS_INFO
	hevc_print(hevc, 0, "skip: max:%d,	avg:%d, min:%d\n",
			vframe_qos->max_skip,
			vframe_qos->avg_skip,
			vframe_qos->min_skip);
#endif

	vframe_qos->num++;

	if (hevc->frameinfo_enable)
		vdec_fill_frame_info(vframe_qos, 1);
}

static int prepare_display_buf(struct hevc_state_s *hevc, struct PIC_s *pic)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif
	struct vframe_s *vf = NULL;
	int stream_offset = pic->stream_offset;
	unsigned short slice_type = pic->slice_type;
	u32 frame_size;

	if (force_disp_pic_index & 0x100) {
		/*recycle directly*/
		pic->output_ready = 0;
		return -1;
	}
	if (kfifo_get(&hevc->newframe_q, &vf) == 0) {
		hevc_print(hevc, 0,
			"fatal error, no available buffer slot.");
		return -1;
	}
	display_frame_count[hevc->index]++;
	if (vf) {
		/*hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s: pic index 0x%x\n",
			__func__, pic->index);*/

#ifdef MULTI_INSTANCE_SUPPORT
		if (vdec_frame_based(hw_to_vdec(hevc))) {
			vf->pts = pic->pts;
			vf->pts_us64 = pic->pts64;
		}
		/* if (pts_lookup_offset(PTS_TYPE_VIDEO,
		   stream_offset, &vf->pts, 0) != 0) { */
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		else if (vdec->master == NULL) {
#else
		else {
#endif
#endif
		hevc_print(hevc, H265_DEBUG_OUT_PTS,
			"call pts_lookup_offset_us64(0x%x)\n",
			stream_offset);
		if (pts_lookup_offset_us64
			(PTS_TYPE_VIDEO, stream_offset, &vf->pts,
			&frame_size, 0,
			 &vf->pts_us64) != 0) {
#ifdef DEBUG_PTS
			hevc->pts_missed++;
#endif
			vf->pts = 0;
			vf->pts_us64 = 0;
		}
#ifdef DEBUG_PTS
		else
			hevc->pts_hit++;
#endif
#ifdef MULTI_INSTANCE_SUPPORT
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		} else {
			vf->pts = 0;
			vf->pts_us64 = 0;
		}
#else
		}
#endif
#endif
		if (pts_unstable && (hevc->frame_dur > 0))
			hevc->pts_mode = PTS_NONE_REF_USE_DURATION;

		fill_frame_info(hevc, pic, frame_size, vf->pts);

		if ((hevc->pts_mode == PTS_NORMAL) && (vf->pts != 0)
			&& hevc->get_frame_dur) {
			int pts_diff = (int)vf->pts - hevc->last_lookup_pts;

			if (pts_diff < 0) {
				hevc->pts_mode_switching_count++;
				hevc->pts_mode_recovery_count = 0;

				if (hevc->pts_mode_switching_count >=
					PTS_MODE_SWITCHING_THRESHOLD) {
					hevc->pts_mode =
						PTS_NONE_REF_USE_DURATION;
					hevc_print(hevc, 0,
					"HEVC: switch to n_d mode.\n");
				}

			} else {
				int p = PTS_MODE_SWITCHING_RECOVERY_THREASHOLD;

				hevc->pts_mode_recovery_count++;
				if (hevc->pts_mode_recovery_count > p) {
					hevc->pts_mode_switching_count = 0;
					hevc->pts_mode_recovery_count = 0;
				}
			}
		}

		if (vf->pts != 0)
			hevc->last_lookup_pts = vf->pts;

		if ((hevc->pts_mode == PTS_NONE_REF_USE_DURATION)
			&& (slice_type != 2))
			vf->pts = hevc->last_pts + DUR2PTS(hevc->frame_dur);
		hevc->last_pts = vf->pts;

		if (vf->pts_us64 != 0)
			hevc->last_lookup_pts_us64 = vf->pts_us64;

		if ((hevc->pts_mode == PTS_NONE_REF_USE_DURATION)
			&& (slice_type != 2)) {
			vf->pts_us64 =
				hevc->last_pts_us64 +
				(DUR2PTS(hevc->frame_dur) * 100 / 9);
		}
		hevc->last_pts_us64 = vf->pts_us64;
		if ((get_dbg_flag(hevc) & H265_DEBUG_OUT_PTS) != 0) {
			hevc_print(hevc, 0,
			"H265 dec out pts: vf->pts=%d, vf->pts_us64 = %lld\n",
			 vf->pts, vf->pts_us64);
		}

		/*
		 *vf->index:
		 *(1) vf->type is VIDTYPE_PROGRESSIVE
		 *	and vf->canvas0Addr !=  vf->canvas1Addr,
		 *	vf->index[7:0] is the index of top pic
		 *	vf->index[15:8] is the index of bot pic
		 *(2) other cases,
		 *	only vf->index[7:0] is used
		 *	vf->index[15:8] == 0xff
		 */
		vf->index = 0xff00 | pic->index;
#if 1
/*SUPPORT_10BIT*/
		if (pic->double_write_mode & 0x10) {
			/* double write only */
			vf->compBodyAddr = 0;
			vf->compHeadAddr = 0;
		} else {

		if (hevc->mmu_enable) {
			vf->compBodyAddr = 0;
			vf->compHeadAddr = pic->header_adr;
		} else {
			vf->compBodyAddr = pic->mc_y_adr; /*body adr*/
			vf->compHeadAddr = pic->mc_y_adr +
						pic->losless_comp_body_size;
			vf->mem_head_handle = NULL;
		}

					/*head adr*/
			vf->canvas0Addr = vf->canvas1Addr = 0;
		}
		if (pic->double_write_mode) {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
			vf->type |= VIDTYPE_VIU_NV21;
			if ((pic->double_write_mode == 3) &&
				(!(IS_8K_SIZE(pic->width, pic->height)))) {
				vf->type |= VIDTYPE_COMPRESS;
				if (hevc->mmu_enable)
					vf->type |= VIDTYPE_SCATTER;
			}
#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag &&
				(get_dbg_flag(hevc)
				& H265_CFG_CANVAS_IN_DECODE) == 0) {
					vf->canvas0Addr = vf->canvas1Addr = -1;
					vf->plane_num = 2;
					vf->canvas0_config[0] =
						pic->canvas_config[0];
					vf->canvas0_config[1] =
						pic->canvas_config[1];

					vf->canvas1_config[0] =
						pic->canvas_config[0];
					vf->canvas1_config[1] =
						pic->canvas_config[1];

			} else
#endif
				vf->canvas0Addr = vf->canvas1Addr
				= spec2canvas(pic);
		} else {
			vf->canvas0Addr = vf->canvas1Addr = 0;
			vf->type = VIDTYPE_COMPRESS | VIDTYPE_VIU_FIELD;
			if (hevc->mmu_enable)
				vf->type |= VIDTYPE_SCATTER;
		}
		vf->compWidth = pic->width;
		vf->compHeight = pic->height;
		update_vf_memhandle(hevc, vf, pic);
		switch (pic->bit_depth_luma) {
		case 9:
			vf->bitdepth = BITDEPTH_Y9;
			break;
		case 10:
			vf->bitdepth = BITDEPTH_Y10;
			break;
		default:
			vf->bitdepth = BITDEPTH_Y8;
			break;
		}
		switch (pic->bit_depth_chroma) {
		case 9:
			vf->bitdepth |= (BITDEPTH_U9 | BITDEPTH_V9);
			break;
		case 10:
			vf->bitdepth |= (BITDEPTH_U10 | BITDEPTH_V10);
			break;
		default:
			vf->bitdepth |= (BITDEPTH_U8 | BITDEPTH_V8);
			break;
		}
		if ((vf->type & VIDTYPE_COMPRESS) == 0)
			vf->bitdepth =
				BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		if (pic->mem_saving_mode == 1)
			vf->bitdepth |= BITDEPTH_SAVING_MODE;
#else
		vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
		vf->type |= VIDTYPE_VIU_NV21;
		vf->canvas0Addr = vf->canvas1Addr = spec2canvas(pic);
#endif
		set_frame_info(hevc, vf, pic);
		/* if((vf->width!=pic->width)||(vf->height!=pic->height)) */
		/* hevc_print(hevc, 0,
			"aaa: %d/%d, %d/%d\n",
		   vf->width,vf->height, pic->width, pic->height); */
		vf->width = pic->width;
		vf->height = pic->height;

		if (force_w_h != 0) {
			vf->width = (force_w_h >> 16) & 0xffff;
			vf->height = force_w_h & 0xffff;
		}
		if (force_fps & 0x100) {
			u32 rate = force_fps & 0xff;

			if (rate)
				vf->duration = 96000/rate;
			else
				vf->duration = 0;
		}
		if (force_fps & 0x200) {
			vf->pts = 0;
			vf->pts_us64 = 0;
		}
		/*
		 *	!!! to do ...
		 *	need move below code to get_new_pic(),
		 *	hevc->xxx can only be used by current decoded pic
		 */
		if (pic->conformance_window_flag &&
			(get_dbg_flag(hevc) &
				H265_DEBUG_IGNORE_CONFORMANCE_WINDOW) == 0) {
			unsigned int SubWidthC, SubHeightC;

			switch (pic->chroma_format_idc) {
			case 1:
				SubWidthC = 2;
				SubHeightC = 2;
				break;
			case 2:
				SubWidthC = 2;
				SubHeightC = 1;
				break;
			default:
				SubWidthC = 1;
				SubHeightC = 1;
				break;
			}
			 vf->width -= SubWidthC *
				(pic->conf_win_left_offset +
				pic->conf_win_right_offset);
			 vf->height -= SubHeightC *
				(pic->conf_win_top_offset +
				pic->conf_win_bottom_offset);

			 vf->compWidth -= SubWidthC *
				(pic->conf_win_left_offset +
				pic->conf_win_right_offset);
			 vf->compHeight -= SubHeightC *
				(pic->conf_win_top_offset +
				pic->conf_win_bottom_offset);

			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print(hevc, 0,
					"conformance_window %d, %d, %d, %d, %d => cropped width %d, height %d com_w %d com_h %d\n",
					pic->chroma_format_idc,
					pic->conf_win_left_offset,
					pic->conf_win_right_offset,
					pic->conf_win_top_offset,
					pic->conf_win_bottom_offset,
					vf->width, vf->height, vf->compWidth, vf->compHeight);
		}

		vf->width = vf->width /
			get_double_write_ratio(hevc, pic->double_write_mode);
		vf->height = vf->height /
			get_double_write_ratio(hevc, pic->double_write_mode);
#ifdef HEVC_PIC_STRUCT_SUPPORT
		if (pic->pic_struct == 3 || pic->pic_struct == 4) {
			struct vframe_s *vf2;

			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);

			if (kfifo_get(&hevc->newframe_q, &vf2) == 0) {
				hevc_print(hevc, 0,
					"fatal error, no available buffer slot.");
				return -1;
			}
			pic->vf_ref = 2;
			vf->duration = vf->duration>>1;
			memcpy(vf2, vf, sizeof(struct vframe_s));

			if (pic->pic_struct == 3) {
				vf->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21;
				vf2->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21;
			} else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21;
				vf2->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21;
			}
			hevc->vf_pre_count++;
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
			ATRACE_COUNTER(MODULE_NAME, vf->pts);
			hevc->vf_pre_count++;
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf2);
			ATRACE_COUNTER(MODULE_NAME, vf2->pts);
		} else if (pic->pic_struct == 5
			|| pic->pic_struct == 6) {
			struct vframe_s *vf2, *vf3;

			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);

			if (kfifo_get(&hevc->newframe_q, &vf2) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			if (kfifo_get(&hevc->newframe_q, &vf3) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			pic->vf_ref = 3;
			vf->duration = vf->duration/3;
			memcpy(vf2, vf, sizeof(struct vframe_s));
			memcpy(vf3, vf, sizeof(struct vframe_s));

			if (pic->pic_struct == 5) {
				vf->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21;
				vf2->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21;
				vf3->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21;
			} else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21;
				vf2->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21;
				vf3->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21;
			}
			hevc->vf_pre_count++;
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
			ATRACE_COUNTER(MODULE_NAME, vf->pts);
			hevc->vf_pre_count++;
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf2);
			ATRACE_COUNTER(MODULE_NAME, vf2->pts);
			hevc->vf_pre_count++;
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf3);
			ATRACE_COUNTER(MODULE_NAME, vf3->pts);

		} else if (pic->pic_struct == 9
			|| pic->pic_struct == 10) {
			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);

			pic->vf_ref = 1;
			/* process previous pending vf*/
			process_pending_vframe(hevc,
			pic, (pic->pic_struct == 9));

			/* process current vf */
			kfifo_put(&hevc->pending_q,
			(const struct vframe_s *)vf);
			vf->height <<= 1;
			if (pic->pic_struct == 9) {
				vf->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
				process_pending_vframe(hevc,
				hevc->pre_bot_pic, 0);
			} else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM |
				VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
				vf->index = (pic->index << 8) | 0xff;
				process_pending_vframe(hevc,
				hevc->pre_top_pic, 1);
			}

			/**/
			if (pic->pic_struct == 9)
				hevc->pre_top_pic = pic;
			else
				hevc->pre_bot_pic = pic;

		} else if (pic->pic_struct == 11
		    || pic->pic_struct == 12) {
			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
				"pic_struct = %d index 0x%x\n",
				pic->pic_struct,
				pic->index);
			pic->vf_ref = 1;
			/* process previous pending vf*/
			process_pending_vframe(hevc, pic,
			(pic->pic_struct == 11));

			/* put current into pending q */
			vf->height <<= 1;
			if (pic->pic_struct == 11)
				vf->type = VIDTYPE_INTERLACE_TOP |
				VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
			else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM |
				VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
				vf->index = (pic->index << 8) | 0xff;
			}
			kfifo_put(&hevc->pending_q,
			(const struct vframe_s *)vf);

			/**/
			if (pic->pic_struct == 11)
				hevc->pre_top_pic = pic;
			else
				hevc->pre_bot_pic = pic;

		} else {
			pic->vf_ref = 1;

			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
				"pic_struct = %d index 0x%x\n",
				pic->pic_struct,
				pic->index);

			switch (pic->pic_struct) {
			case 7:
				vf->duration <<= 1;
				break;
			case 8:
				vf->duration = vf->duration * 3;
				break;
			case 1:
				vf->height <<= 1;
				vf->type = VIDTYPE_INTERLACE_TOP |
				VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
				process_pending_vframe(hevc, pic, 1);
				hevc->pre_top_pic = pic;
				break;
			case 2:
				vf->height <<= 1;
				vf->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21
				| VIDTYPE_VIU_FIELD;
				process_pending_vframe(hevc, pic, 0);
				hevc->pre_bot_pic = pic;
				break;
			}
			hevc->vf_pre_count++;
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
			ATRACE_COUNTER(MODULE_NAME, vf->pts);
		}
#else
		vf->type_original = vf->type;
		pic->vf_ref = 1;
		hevc->vf_pre_count++;
		decoder_do_frame_check(hw_to_vdec(hevc), vf);
		kfifo_put(&hevc->display_q, (const struct vframe_s *)vf);
		ATRACE_COUNTER(MODULE_NAME, vf->pts);

		if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
			hevc_print(hevc, 0,
				"%s(type %d index 0x%x poc %d/%d) pts(%d,%d) dur %d\n",
				__func__, vf->type, vf->index,
				get_pic_poc(hevc, vf->index & 0xff),
				get_pic_poc(hevc, (vf->index >> 8) & 0xff),
				vf->pts, vf->pts_us64,
				vf->duration);
#endif
#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
		/*count info*/
		vdec_count_info(gvs, 0, stream_offset);
#endif
		hw_to_vdec(hevc)->vdec_fps_detec(hw_to_vdec(hevc)->id);
		vf_notify_receiver(hevc->provider_name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	}

	return 0;
}

static void process_nal_sei(struct hevc_state_s *hevc,
	int payload_type, int payload_size)
{
	unsigned short data;

	if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
		hevc_print(hevc, 0,
			"\tsei message: payload_type = 0x%02x, payload_size = 0x%02x\n",
		payload_type, payload_size);

	if (payload_type == 137) {
		int i, j;
		/* MASTERING_DISPLAY_COLOUR_VOLUME */
		if (payload_size >= 24) {
			if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
				hevc_print(hevc, 0,
					"\tsei MASTERING_DISPLAY_COLOUR_VOLUME available\n");
			for (i = 0; i < 3; i++) {
				for (j = 0; j < 2; j++) {
					data =
					(READ_HREG(HEVC_SHIFTED_DATA) >> 16);
					hevc->primaries[i][j] = data;
					WRITE_HREG(HEVC_SHIFT_COMMAND,
					(1<<7)|16);
					if (get_dbg_flag(hevc) &
						H265_DEBUG_PRINT_SEI)
						hevc_print(hevc, 0,
							"\t\tprimaries[%1d][%1d] = %04x\n",
						i, j, hevc->primaries[i][j]);
				}
			}
			for (i = 0; i < 2; i++) {
				data = (READ_HREG(HEVC_SHIFTED_DATA) >> 16);
				hevc->white_point[i] = data;
				WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|16);
				if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
					hevc_print(hevc, 0,
						"\t\twhite_point[%1d] = %04x\n",
					i, hevc->white_point[i]);
			}
			for (i = 0; i < 2; i++) {
				data = (READ_HREG(HEVC_SHIFTED_DATA) >> 16);
				hevc->luminance[i] = data << 16;
				WRITE_HREG(HEVC_SHIFT_COMMAND,
				(1<<7)|16);
				data =
				(READ_HREG(HEVC_SHIFTED_DATA) >> 16);
				hevc->luminance[i] |= data;
				WRITE_HREG(HEVC_SHIFT_COMMAND,
				(1<<7)|16);
				if (get_dbg_flag(hevc) &
					H265_DEBUG_PRINT_SEI)
					hevc_print(hevc, 0,
						"\t\tluminance[%1d] = %08x\n",
					i, hevc->luminance[i]);
			}
			hevc->sei_present_flag |= SEI_MASTER_DISPLAY_COLOR_MASK;
		}
		payload_size -= 24;
		while (payload_size > 0) {
			data = (READ_HREG(HEVC_SHIFTED_DATA) >> 24);
			payload_size--;
			WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|8);
			hevc_print(hevc, 0, "\t\tskip byte %02x\n", data);
		}
	}
}

static int hevc_recover(struct hevc_state_s *hevc)
{
	int ret = -1;
	u32 rem;
	u64 shift_byte_count64;
	unsigned int hevc_shift_byte_count;
	unsigned int hevc_stream_start_addr;
	unsigned int hevc_stream_end_addr;
	unsigned int hevc_stream_rd_ptr;
	unsigned int hevc_stream_wr_ptr;
	unsigned int hevc_stream_control;
	unsigned int hevc_stream_fifo_ctl;
	unsigned int hevc_stream_buf_size;

	mutex_lock(&vh265_mutex);
#if 0
	for (i = 0; i < (hevc->debug_ptr_size / 2); i += 4) {
		int ii;

		for (ii = 0; ii < 4; ii++)
			hevc_print(hevc, 0,
			"%04x ", hevc->debug_ptr[i + 3 - ii]);
		if (((i + ii) & 0xf) == 0)
			hevc_print(hevc, 0, "\n");
	}
#endif
#define ES_VID_MAN_RD_PTR            (1<<0)
	if (!hevc->init_flag) {
		hevc_print(hevc, 0, "h265 has stopped, recover return!\n");
		mutex_unlock(&vh265_mutex);
		return ret;
	}
	amhevc_stop();
	msleep(20);
	ret = 0;
	/* reset */
	WRITE_PARSER_REG(PARSER_VIDEO_RP, READ_VREG(HEVC_STREAM_RD_PTR));
	SET_PARSER_REG_MASK(PARSER_ES_CONTROL, ES_VID_MAN_RD_PTR);

	hevc_stream_start_addr = READ_VREG(HEVC_STREAM_START_ADDR);
	hevc_stream_end_addr = READ_VREG(HEVC_STREAM_END_ADDR);
	hevc_stream_rd_ptr = READ_VREG(HEVC_STREAM_RD_PTR);
	hevc_stream_wr_ptr = READ_VREG(HEVC_STREAM_WR_PTR);
	hevc_stream_control = READ_VREG(HEVC_STREAM_CONTROL);
	hevc_stream_fifo_ctl = READ_VREG(HEVC_STREAM_FIFO_CTL);
	hevc_stream_buf_size = hevc_stream_end_addr - hevc_stream_start_addr;

	/* HEVC streaming buffer will reset and restart
	 *   from current hevc_stream_rd_ptr position
	 */
	/* calculate HEVC_SHIFT_BYTE_COUNT value with the new position. */
	hevc_shift_byte_count = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
	if ((hevc->shift_byte_count_lo & (1 << 31))
		&& ((hevc_shift_byte_count & (1 << 31)) == 0))
		hevc->shift_byte_count_hi++;

	hevc->shift_byte_count_lo = hevc_shift_byte_count;
	shift_byte_count64 = ((u64)(hevc->shift_byte_count_hi) << 32) |
				hevc->shift_byte_count_lo;
	div_u64_rem(shift_byte_count64, hevc_stream_buf_size, &rem);
	shift_byte_count64 -= rem;
	shift_byte_count64 += hevc_stream_rd_ptr - hevc_stream_start_addr;

	if (rem > (hevc_stream_rd_ptr - hevc_stream_start_addr))
		shift_byte_count64 += hevc_stream_buf_size;

	hevc->shift_byte_count_lo = (u32)shift_byte_count64;
	hevc->shift_byte_count_hi = (u32)(shift_byte_count64 >> 32);

	WRITE_VREG(DOS_SW_RESET3,
			   /* (1<<2)| */
			   (1 << 3) | (1 << 4) | (1 << 8) |
			   (1 << 11) | (1 << 12) | (1 << 14)
			   | (1 << 15) | (1 << 17) | (1 << 18) | (1 << 19));
	WRITE_VREG(DOS_SW_RESET3, 0);

	WRITE_VREG(HEVC_STREAM_START_ADDR, hevc_stream_start_addr);
	WRITE_VREG(HEVC_STREAM_END_ADDR, hevc_stream_end_addr);
	WRITE_VREG(HEVC_STREAM_RD_PTR, hevc_stream_rd_ptr);
	WRITE_VREG(HEVC_STREAM_WR_PTR, hevc_stream_wr_ptr);
	WRITE_VREG(HEVC_STREAM_CONTROL, hevc_stream_control);
	WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, hevc->shift_byte_count_lo);
	WRITE_VREG(HEVC_STREAM_FIFO_CTL, hevc_stream_fifo_ctl);

	hevc_config_work_space_hw(hevc);
	decoder_hw_reset();

	hevc->have_vps = 0;
	hevc->have_sps = 0;
	hevc->have_pps = 0;

	hevc->have_valid_start_slice = 0;

	if (get_double_write_mode(hevc) & 0x10)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,
			0x1 << 31  /*/Enable NV21 reference read mode for MC*/
			);

	WRITE_VREG(HEVC_WAIT_FLAG, 1);
	/* clear mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_CLR_REG, 1);
	/* enable mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 1);
	/* disable PSCALE for hardware sharing */
	WRITE_VREG(HEVC_PSCALE_CTRL, 0);

	CLEAR_PARSER_REG_MASK(PARSER_ES_CONTROL, ES_VID_MAN_RD_PTR);

	WRITE_VREG(DEBUG_REG1, 0x0);

	if ((error_handle_policy & 1) == 0) {
		if ((error_handle_policy & 4) == 0) {
			/* ucode auto mode, and do not check vps/sps/pps/idr */
			WRITE_VREG(NAL_SEARCH_CTL,
					   0xc);
		} else {
			WRITE_VREG(NAL_SEARCH_CTL, 0x1);/* manual parser NAL */
		}
	} else {
		WRITE_VREG(NAL_SEARCH_CTL, 0x1);/* manual parser NAL */
	}

	if (get_dbg_flag(hevc) & H265_DEBUG_NO_EOS_SEARCH_DONE)
		WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x10000);
	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL)
		| ((parser_sei_enable & 0x7) << 17));
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL) |
		((parser_dolby_vision_enable & 0x1) << 20));
#endif
	config_decode_mode(hevc);
	WRITE_VREG(DECODE_STOP_POS, udebug_flag);

	/* if (amhevc_loadmc(vh265_mc) < 0) { */
	/* amhevc_disable(); */
	/* return -EBUSY; */
	/* } */
#if 0
	for (i = 0; i < (hevc->debug_ptr_size / 2); i += 4) {
		int ii;

		for (ii = 0; ii < 4; ii++) {
			/* hevc->debug_ptr[i+3-ii]=ttt++; */
			hevc_print(hevc, 0,
			"%04x ", hevc->debug_ptr[i + 3 - ii]);
		}
		if (((i + ii) & 0xf) == 0)
			hevc_print(hevc, 0, "\n");
	}
#endif
	init_pic_list_hw(hevc);

	hevc_print(hevc, 0, "%s HEVC_SHIFT_BYTE_COUNT=0x%x\n", __func__,
		   READ_VREG(HEVC_SHIFT_BYTE_COUNT));

#ifdef SWAP_HEVC_UCODE
	if (!tee_enabled() && hevc->is_swap &&
		get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->mc_dma_handle);
		/*pr_info("write swap buffer %x\n", (u32)(hevc->mc_dma_handle));*/
	}
#endif
	amhevc_start();

	/* skip, search next start code */
	WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG) & (~0x2));
	hevc->skip_flag = 1;
#ifdef ERROR_HANDLE_DEBUG
	if (dbg_nal_skip_count & 0x20000) {
		dbg_nal_skip_count &= ~0x20000;
		mutex_unlock(&vh265_mutex);
		return ret;
	}
#endif
	WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
	/* Interrupt Amrisc to excute */
	WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);
#ifdef MULTI_INSTANCE_SUPPORT
	if (!hevc->m_ins_flag)
#endif
		hevc->first_pic_after_recover = 1;
	mutex_unlock(&vh265_mutex);
	return ret;
}

static void dump_aux_buf(struct hevc_state_s *hevc)
{
	int i;
	unsigned short *aux_adr =
		(unsigned short *)
		hevc->aux_addr;
	unsigned int aux_size =
		(READ_VREG(HEVC_AUX_DATA_SIZE)
		>> 16) << 4;

	if (hevc->prefix_aux_size > 0) {
		hevc_print(hevc, 0,
			"prefix aux: (size %d)\n",
			aux_size);
		for (i = 0; i <
		(aux_size >> 1); i++) {
			hevc_print_cont(hevc, 0,
				"%04x ",
				*(aux_adr + i));
			if (((i + 1) & 0xf)
				== 0)
				hevc_print_cont(hevc,
				0, "\n");
		}
	}
	if (hevc->suffix_aux_size > 0) {
		aux_adr = (unsigned short *)
			(hevc->aux_addr +
			hevc->prefix_aux_size);
		aux_size =
		(READ_VREG(HEVC_AUX_DATA_SIZE) & 0xffff)
			<< 4;
		hevc_print(hevc, 0,
			"suffix aux: (size %d)\n",
			aux_size);
		for (i = 0; i <
		(aux_size >> 1); i++) {
			hevc_print_cont(hevc, 0,
				"%04x ", *(aux_adr + i));
			if (((i + 1) & 0xf) == 0)
				hevc_print_cont(hevc, 0, "\n");
		}
	}
}

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static void dolby_get_meta(struct hevc_state_s *hevc)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (get_dbg_flag(hevc) &
		H265_DEBUG_BUFMGR_MORE)
		dump_aux_buf(hevc);
	if (vdec->dolby_meta_with_el || vdec->slave) {
		set_aux_data(hevc,
		hevc->cur_pic, 0, 0);
	} else if (vdec->master) {
		struct hevc_state_s *hevc_ba =
		(struct hevc_state_s *)
			vdec->master->private;
		/*do not use hevc_ba*/
		set_aux_data(hevc,
		hevc_ba->cur_pic,
			0, 1);
		set_aux_data(hevc,
		hevc->cur_pic, 0, 2);
	}
}
#endif

static void read_decode_info(struct hevc_state_s *hevc)
{
	uint32_t decode_info =
		READ_HREG(HEVC_DECODE_INFO);
	hevc->start_decoding_flag |=
		(decode_info & 0xff);
	hevc->rps_set_id = (decode_info >> 8) & 0xff;
}

static irqreturn_t vh265_isr_thread_fn(int irq, void *data)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *) data;
	unsigned int dec_status = hevc->dec_status;
	int i, ret;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif
	if (hevc->eos)
		return IRQ_HANDLED;
	if (
#ifdef MULTI_INSTANCE_SUPPORT
		(!hevc->m_ins_flag) &&
#endif
		hevc->error_flag == 1) {
		if ((error_handle_policy & 0x10) == 0) {
			if (hevc->cur_pic) {
				int current_lcu_idx =
					READ_VREG(HEVC_PARSER_LCU_START)
					& 0xffffff;
				if (current_lcu_idx <
					((hevc->lcu_x_num*hevc->lcu_y_num)-1))
					hevc->cur_pic->error_mark = 1;

			}
		}
		if ((error_handle_policy & 1) == 0) {
			hevc->error_skip_nal_count = 1;
			/* manual search nal, skip  error_skip_nal_count
			 *   of nal and trigger the HEVC_NAL_SEARCH_DONE irq
			 */
			WRITE_VREG(NAL_SEARCH_CTL,
					   (error_skip_nal_count << 4) | 0x1);
		} else {
			hevc->error_skip_nal_count = error_skip_nal_count;
			WRITE_VREG(NAL_SEARCH_CTL, 0x1);/* manual parser NAL */
		}
		if ((get_dbg_flag(hevc) & H265_DEBUG_NO_EOS_SEARCH_DONE)
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			|| vdec->master
			|| vdec->slave
#endif
			) {
			WRITE_VREG(NAL_SEARCH_CTL,
					   READ_VREG(NAL_SEARCH_CTL) | 0x10000);
		}
		WRITE_VREG(NAL_SEARCH_CTL,
			READ_VREG(NAL_SEARCH_CTL)
			| ((parser_sei_enable & 0x7) << 17));
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		WRITE_VREG(NAL_SEARCH_CTL,
			READ_VREG(NAL_SEARCH_CTL) |
			((parser_dolby_vision_enable & 0x1) << 20));
#endif
		config_decode_mode(hevc);
		/* search new nal */
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
		/* Interrupt Amrisc to excute */
		WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);

		/* hevc_print(hevc, 0,
		 *"%s: error handle\n", __func__);
		 */
		hevc->error_flag = 2;
		return IRQ_HANDLED;
	} else if (
#ifdef MULTI_INSTANCE_SUPPORT
		(!hevc->m_ins_flag) &&
#endif
		hevc->error_flag == 3) {
		hevc_print(hevc, 0, "error_flag=3, hevc_recover\n");
		hevc_recover(hevc);
		hevc->error_flag = 0;

		if ((error_handle_policy & 0x10) == 0) {
			if (hevc->cur_pic) {
				int current_lcu_idx =
					READ_VREG(HEVC_PARSER_LCU_START)
					& 0xffffff;
				if (current_lcu_idx <
					((hevc->lcu_x_num*hevc->lcu_y_num)-1))
					hevc->cur_pic->error_mark = 1;

			}
		}
		if ((error_handle_policy & 1) == 0) {
			/* need skip some data when
			 *   error_flag of 3 is triggered,
			 */
			/* to avoid hevc_recover() being called
			 *   for many times at the same bitstream position
			 */
			hevc->error_skip_nal_count = 1;
			/* manual search nal, skip  error_skip_nal_count
			 *   of nal and trigger the HEVC_NAL_SEARCH_DONE irq
			 */
			WRITE_VREG(NAL_SEARCH_CTL,
					   (error_skip_nal_count << 4) | 0x1);
		}

		if ((error_handle_policy & 0x2) == 0) {
			hevc->have_vps = 1;
			hevc->have_sps = 1;
			hevc->have_pps = 1;
		}
		return IRQ_HANDLED;
	}
	if (!hevc->m_ins_flag) {
		i = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
		if ((hevc->shift_byte_count_lo & (1 << 31))
			&& ((i & (1 << 31)) == 0))
			hevc->shift_byte_count_hi++;
		hevc->shift_byte_count_lo = i;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	mutex_lock(&hevc->chunks_mutex);
	if ((dec_status == HEVC_DECPIC_DATA_DONE ||
		dec_status == HEVC_FIND_NEXT_PIC_NAL ||
		dec_status == HEVC_FIND_NEXT_DVEL_NAL)
		&& (hevc->chunk)) {
		hevc->cur_pic->pts = hevc->chunk->pts;
		hevc->cur_pic->pts64 = hevc->chunk->pts64;
	}
	mutex_unlock(&hevc->chunks_mutex);

	if (dec_status == HEVC_DECODE_BUFEMPTY ||
		dec_status == HEVC_DECODE_BUFEMPTY2) {
		if (hevc->m_ins_flag) {
			read_decode_info(hevc);
			if (vdec_frame_based(hw_to_vdec(hevc))) {
				hevc->empty_flag = 1;
				goto pic_done;
			} else {
				if (
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
					vdec->master ||
					vdec->slave ||
#endif
					(data_resend_policy & 0x1)) {
					hevc->dec_result = DEC_RESULT_AGAIN;
					amhevc_stop();
					restore_decode_state(hevc);
				} else
					hevc->dec_result = DEC_RESULT_GET_DATA;
			}
			reset_process_time(hevc);
			vdec_schedule_work(&hevc->work);
		}
		return IRQ_HANDLED;
	} else if ((dec_status == HEVC_SEARCH_BUFEMPTY) ||
		(dec_status == HEVC_NAL_DECODE_DONE)
		) {
		if (hevc->m_ins_flag) {
			read_decode_info(hevc);
			if (vdec_frame_based(hw_to_vdec(hevc))) {
				/*hevc->dec_result = DEC_RESULT_GET_DATA;*/
				hevc->empty_flag = 1;
				goto pic_done;
			} else {
				hevc->dec_result = DEC_RESULT_AGAIN;
				amhevc_stop();
				restore_decode_state(hevc);
			}

			reset_process_time(hevc);
			vdec_schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
	} else if (dec_status == HEVC_DECPIC_DATA_DONE) {
		if (hevc->m_ins_flag) {
			struct PIC_s *pic;
			struct PIC_s *pic_display;
			int decoded_poc;
#ifdef DETREFILL_ENABLE
			if (hevc->is_swap &&
				get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
				if (hevc->detbuf_adr_virt && hevc->delrefill_check
					&& READ_VREG(HEVC_SAO_DBG_MODE0))
					hevc->delrefill_check = 2;
			}
#endif
			hevc->empty_flag = 0;
pic_done:
			if (input_frame_based(hw_to_vdec(hevc)) &&
				frmbase_cont_bitlevel != 0 &&
				(hevc->decode_size > READ_VREG(HEVC_SHIFT_BYTE_COUNT)) &&
				(hevc->decode_size - (READ_VREG(HEVC_SHIFT_BYTE_COUNT))
				 >	frmbase_cont_bitlevel)) {
				/*handle the case: multi pictures in one packet*/
				hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s  has more data index= %d, size=0x%x shiftcnt=0x%x)\n",
				__func__,
				hevc->decode_idx, hevc->decode_size,
				READ_VREG(HEVC_SHIFT_BYTE_COUNT));
				WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
				start_process_time(hevc);
				return IRQ_HANDLED;
			}

			read_decode_info(hevc);
			get_picture_qos_info(hevc);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			hevc->start_parser_type = 0;
			hevc->switch_dvlayer_flag = 0;
#endif
			hevc->decoded_poc = hevc->curr_POC;
			hevc->decoding_pic = NULL;
			hevc->dec_result = DEC_RESULT_DONE;
#ifdef DETREFILL_ENABLE
			if (hevc->is_swap &&
				get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM)
				if (hevc->delrefill_check != 2)
#endif

			amhevc_stop();

			reset_process_time(hevc);

			if (hevc->vf_pre_count == 0) {
				decoded_poc = hevc->curr_POC;
				pic = get_pic_by_POC(hevc, decoded_poc);
				if (pic && (pic->POC != INVALID_POC)) {
					/*PB skip control */
					if (pic->error_mark == 0
							&& hevc->PB_skip_mode == 1) {
						/* start decoding after
						 *   first I
						 */
						hevc->ignore_bufmgr_error |= 0x1;
					}
					if (hevc->ignore_bufmgr_error & 1) {
						if (hevc->PB_skip_count_after_decoding > 0) {
							hevc->PB_skip_count_after_decoding--;
						} else {
							/* start displaying */
							hevc->ignore_bufmgr_error |= 0x2;
						}
					}
					if (hevc->mmu_enable
							&& ((hevc->double_write_mode & 0x10) == 0)) {
						if (!hevc->m_ins_flag) {
							hevc->used_4k_num =
							READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;

							if ((!is_skip_decoding(hevc, pic)) &&
								(hevc->used_4k_num >= 0) &&
								(hevc->cur_pic->scatter_alloc
								== 1)) {
								hevc_print(hevc,
								H265_DEBUG_BUFMGR_MORE,
								"%s pic index %d scatter_alloc %d page_start %d\n",
								"decoder_mmu_box_free_idx_tail",
								hevc->cur_pic->index,
								hevc->cur_pic->scatter_alloc,
								hevc->used_4k_num);
								decoder_mmu_box_free_idx_tail(
								hevc->mmu_box,
								hevc->cur_pic->index,
								hevc->used_4k_num);
								hevc->cur_pic->scatter_alloc
									= 2;
							}
							hevc->used_4k_num = -1;
						}
					}

					pic->output_mark = 1;
					pic->recon_mark = 1;
				}

				pic_display = output_pic(hevc, 1);

				if (pic_display) {
					if ((pic_display->error_mark &&
						((hevc->ignore_bufmgr_error &
								  0x2) == 0))
						|| (get_dbg_flag(hevc) &
							H265_DEBUG_DISPLAY_CUR_FRAME)
						|| (get_dbg_flag(hevc) &
							H265_DEBUG_NO_DISPLAY)) {
						pic_display->output_ready = 0;
						if (get_dbg_flag(hevc) &
							H265_DEBUG_BUFMGR) {
							hevc_print(hevc, 0,
							"[BM] Display: POC %d, ",
								 pic_display->POC);
							hevc_print_cont(hevc, 0,
							"decoding index %d ==> ",
								 pic_display->
								 decode_idx);
							hevc_print_cont(hevc, 0,
							"Debug or err,recycle it\n");
						}
					} else {
						if (pic_display->
						slice_type != 2) {
						pic_display->output_ready = 0;
						} else {
							prepare_display_buf
								(hevc,
								 pic_display);
							hevc->first_pic_flag = 1;
						}
					}
				}
			}

			vdec_schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	} else if (dec_status == HEVC_FIND_NEXT_PIC_NAL ||
		dec_status == HEVC_FIND_NEXT_DVEL_NAL) {
		if (hevc->m_ins_flag) {
			unsigned char next_parser_type =
					READ_HREG(CUR_NAL_UNIT_TYPE) & 0xff;
			read_decode_info(hevc);

			if (vdec->slave &&
				dec_status == HEVC_FIND_NEXT_DVEL_NAL) {
				/*cur is base, found enhance*/
				struct hevc_state_s *hevc_el =
				(struct hevc_state_s *)
					vdec->slave->private;
				hevc->switch_dvlayer_flag = 1;
				hevc->no_switch_dvlayer_count = 0;
				hevc_el->start_parser_type =
					next_parser_type;
				hevc_print(hevc, H265_DEBUG_DV,
					"switch (poc %d) to el\n",
					hevc->cur_pic ?
					hevc->cur_pic->POC :
					INVALID_POC);
			} else if (vdec->master &&
				dec_status == HEVC_FIND_NEXT_PIC_NAL) {
				/*cur is enhance, found base*/
				struct hevc_state_s *hevc_ba =
				(struct hevc_state_s *)
					vdec->master->private;
				hevc->switch_dvlayer_flag = 1;
				hevc->no_switch_dvlayer_count = 0;
				hevc_ba->start_parser_type =
					next_parser_type;
				hevc_print(hevc, H265_DEBUG_DV,
					"switch (poc %d) to bl\n",
					hevc->cur_pic ?
					hevc->cur_pic->POC :
					INVALID_POC);
			} else {
				hevc->switch_dvlayer_flag = 0;
				hevc->start_parser_type =
					next_parser_type;
				hevc->no_switch_dvlayer_count++;
				hevc_print(hevc, H265_DEBUG_DV,
					"%s: no_switch_dvlayer_count = %d\n",
					vdec->master ? "el" : "bl",
					hevc->no_switch_dvlayer_count);
				if (vdec->slave &&
					dolby_el_flush_th != 0 &&
					hevc->no_switch_dvlayer_count >
					dolby_el_flush_th) {
					struct hevc_state_s *hevc_el =
					(struct hevc_state_s *)
					vdec->slave->private;
					struct PIC_s *el_pic;
					check_pic_decoded_error(hevc_el,
					hevc_el->pic_decoded_lcu_idx);
					el_pic = get_pic_by_POC(hevc_el,
						hevc_el->curr_POC);
					hevc_el->curr_POC = INVALID_POC;
					hevc_el->m_pocRandomAccess = MAX_INT;
					flush_output(hevc_el, el_pic);
					hevc_el->decoded_poc = INVALID_POC; /*
					already call flush_output*/
					hevc_el->decoding_pic = NULL;
					hevc->no_switch_dvlayer_count = 0;
					if (get_dbg_flag(hevc) & H265_DEBUG_DV)
						hevc_print(hevc, 0,
						"no el anymore, flush_output el\n");
				}
			}
			hevc->decoded_poc = hevc->curr_POC;
			hevc->decoding_pic = NULL;
			hevc->dec_result = DEC_RESULT_DONE;
			amhevc_stop();
			reset_process_time(hevc);
			if (aux_data_is_avaible(hevc))
				dolby_get_meta(hevc);

			vdec_schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
#endif
	}

#endif

	if (dec_status == HEVC_SEI_DAT) {
		if (!hevc->m_ins_flag) {
			int payload_type =
				READ_HREG(CUR_NAL_UNIT_TYPE) & 0xffff;
			int payload_size =
				(READ_HREG(CUR_NAL_UNIT_TYPE) >> 16) & 0xffff;
				process_nal_sei(hevc,
					payload_type, payload_size);
		}
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_SEI_DAT_DONE);
	} else if (dec_status == HEVC_NAL_SEARCH_DONE) {
		int naltype = READ_HREG(CUR_NAL_UNIT_TYPE);
		int parse_type = HEVC_DISCARD_NAL;

		hevc->error_watchdog_count = 0;
		hevc->error_skip_nal_wt_cnt = 0;
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag)
			reset_process_time(hevc);
#endif
		if (slice_parse_begin > 0 &&
			get_dbg_flag(hevc) & H265_DEBUG_DISCARD_NAL) {
			hevc_print(hevc, 0,
				"nal type %d, discard %d\n", naltype,
				slice_parse_begin);
			if (naltype <= NAL_UNIT_CODED_SLICE_CRA)
				slice_parse_begin--;
		}
		if (naltype == NAL_UNIT_EOS) {
			struct PIC_s *pic;

			hevc_print(hevc, 0, "get NAL_UNIT_EOS, flush output\n");
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if ((vdec->master || vdec->slave) &&
				aux_data_is_avaible(hevc)) {
				if (hevc->decoding_pic)
					dolby_get_meta(hevc);
			}
#endif
			check_pic_decoded_error(hevc,
				hevc->pic_decoded_lcu_idx);
			pic = get_pic_by_POC(hevc, hevc->curr_POC);
			hevc->curr_POC = INVALID_POC;
			/* add to fix RAP_B_Bossen_1 */
			hevc->m_pocRandomAccess = MAX_INT;
			flush_output(hevc, pic);
			clear_poc_flag(hevc);
			WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_DISCARD_NAL);
			/* Interrupt Amrisc to excute */
			WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);
#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag) {
				hevc->decoded_poc = INVALID_POC; /*
					already call flush_output*/
				hevc->decoding_pic = NULL;
				hevc->dec_result = DEC_RESULT_DONE;
				amhevc_stop();

				vdec_schedule_work(&hevc->work);
			}
#endif
			return IRQ_HANDLED;
		}

		if (
#ifdef MULTI_INSTANCE_SUPPORT
			(!hevc->m_ins_flag) &&
#endif
			hevc->error_skip_nal_count > 0) {
			hevc_print(hevc, 0,
				"nal type %d, discard %d\n", naltype,
				hevc->error_skip_nal_count);
			hevc->error_skip_nal_count--;
			if (hevc->error_skip_nal_count == 0) {
				hevc_recover(hevc);
				hevc->error_flag = 0;
				if ((error_handle_policy & 0x2) == 0) {
					hevc->have_vps = 1;
					hevc->have_sps = 1;
					hevc->have_pps = 1;
				}
				return IRQ_HANDLED;
			}
		} else if (naltype == NAL_UNIT_VPS) {
				parse_type = HEVC_NAL_UNIT_VPS;
				hevc->have_vps = 1;
#ifdef ERROR_HANDLE_DEBUG
				if (dbg_nal_skip_flag & 1)
					parse_type = HEVC_DISCARD_NAL;
#endif
		} else if (hevc->have_vps) {
			if (naltype == NAL_UNIT_SPS) {
				parse_type = HEVC_NAL_UNIT_SPS;
				hevc->have_sps = 1;
#ifdef ERROR_HANDLE_DEBUG
				if (dbg_nal_skip_flag & 2)
					parse_type = HEVC_DISCARD_NAL;
#endif
			} else if (naltype == NAL_UNIT_PPS) {
				parse_type = HEVC_NAL_UNIT_PPS;
				hevc->have_pps = 1;
#ifdef ERROR_HANDLE_DEBUG
				if (dbg_nal_skip_flag & 4)
					parse_type = HEVC_DISCARD_NAL;
#endif
			} else if (hevc->have_sps && hevc->have_pps) {
				int seg = HEVC_NAL_UNIT_CODED_SLICE_SEGMENT;

				if ((naltype == NAL_UNIT_CODED_SLICE_IDR) ||
					(naltype ==
					NAL_UNIT_CODED_SLICE_IDR_N_LP)
					|| (naltype ==
						NAL_UNIT_CODED_SLICE_CRA)
					|| (naltype ==
						NAL_UNIT_CODED_SLICE_BLA)
					|| (naltype ==
						NAL_UNIT_CODED_SLICE_BLANT)
					|| (naltype ==
						NAL_UNIT_CODED_SLICE_BLA_N_LP)
				) {
					if (slice_parse_begin > 0) {
						hevc_print(hevc, 0,
						"discard %d, for debugging\n",
						 slice_parse_begin);
						slice_parse_begin--;
					} else {
						parse_type = seg;
					}
					hevc->have_valid_start_slice = 1;
				} else if (naltype <=
						NAL_UNIT_CODED_SLICE_CRA
						&& (hevc->have_valid_start_slice
						|| (hevc->PB_skip_mode != 3))) {
					if (slice_parse_begin > 0) {
						hevc_print(hevc, 0,
						"discard %d, dd\n",
						slice_parse_begin);
						slice_parse_begin--;
					} else
						parse_type = seg;

				}
			}
		}
		if (hevc->have_vps && hevc->have_sps && hevc->have_pps
			&& hevc->have_valid_start_slice &&
			hevc->error_flag == 0) {
			if ((get_dbg_flag(hevc) &
				H265_DEBUG_MAN_SEARCH_NAL) == 0
				/* && (!hevc->m_ins_flag)*/) {
				/* auot parser NAL; do not check
				 *vps/sps/pps/idr
				 */
				WRITE_VREG(NAL_SEARCH_CTL, 0x2);
			}

			if ((get_dbg_flag(hevc) &
				H265_DEBUG_NO_EOS_SEARCH_DONE)
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				|| vdec->master
				|| vdec->slave
#endif
				) {
				WRITE_VREG(NAL_SEARCH_CTL,
						READ_VREG(NAL_SEARCH_CTL) |
						0x10000);
			}
			WRITE_VREG(NAL_SEARCH_CTL,
				READ_VREG(NAL_SEARCH_CTL)
				| ((parser_sei_enable & 0x7) << 17));
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			WRITE_VREG(NAL_SEARCH_CTL,
				READ_VREG(NAL_SEARCH_CTL) |
				((parser_dolby_vision_enable & 0x1) << 20));
#endif
			config_decode_mode(hevc);
		}

		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"naltype = %d  parse_type %d\n %d %d %d %d\n",
				naltype, parse_type, hevc->have_vps,
				hevc->have_sps, hevc->have_pps,
				hevc->have_valid_start_slice);
		}

		WRITE_VREG(HEVC_DEC_STATUS_REG, parse_type);
		/* Interrupt Amrisc to excute */
		WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag)
			start_process_time(hevc);
#endif
	} else if (dec_status == HEVC_SLICE_SEGMENT_DONE) {
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag) {
			reset_process_time(hevc);
			read_decode_info(hevc);

		}
#endif
		if (hevc->start_decoding_time > 0) {
			u32 process_time = 1000*
				(jiffies - hevc->start_decoding_time)/HZ;
			if (process_time > max_decoding_time)
				max_decoding_time = process_time;
		}

		hevc->error_watchdog_count = 0;
		if (hevc->pic_list_init_flag == 2) {
			hevc->pic_list_init_flag = 3;
			hevc_print(hevc, 0, "set pic_list_init_flag to 3\n");
		} else if (hevc->wait_buf == 0) {
			u32 vui_time_scale;
			u32 vui_num_units_in_tick;
			unsigned char reconfig_flag = 0;

			if (get_dbg_flag(hevc) & H265_DEBUG_SEND_PARAM_WITH_REG)
				get_rpm_param(&hevc->param);
			else {
				dma_sync_single_for_cpu(
				amports_get_dma_device(),
				hevc->rpm_phy_addr,
				RPM_BUF_SIZE,
				DMA_FROM_DEVICE);

				for (i = 0; i < (RPM_END - RPM_BEGIN); i += 4) {
					int ii;

					for (ii = 0; ii < 4; ii++) {
						hevc->param.l.data[i + ii] =
							hevc->rpm_ptr[i + 3
							- ii];
					}
				}
#ifdef SEND_LMEM_WITH_RPM
				dma_sync_single_for_cpu(
					amports_get_dma_device(),
					hevc->lmem_phy_addr,
					LMEM_BUF_SIZE,
					DMA_FROM_DEVICE);
				check_head_error(hevc);
#endif
			}
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE) {
				hevc_print(hevc, 0,
					"rpm_param: (%d)\n", hevc->slice_idx);
				hevc->slice_idx++;
				for (i = 0; i < (RPM_END - RPM_BEGIN); i++) {
					hevc_print_cont(hevc, 0,
						"%04x ", hevc->param.l.data[i]);
					if (((i + 1) & 0xf) == 0)
						hevc_print_cont(hevc, 0, "\n");
				}

				hevc_print(hevc, 0,
					"vui_timing_info: %x, %x, %x, %x\n",
					hevc->param.p.vui_num_units_in_tick_hi,
					hevc->param.p.vui_num_units_in_tick_lo,
					hevc->param.p.vui_time_scale_hi,
					hevc->param.p.vui_time_scale_lo);
			}
			if (
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				vdec->master == NULL &&
				vdec->slave == NULL &&
#endif
				aux_data_is_avaible(hevc)
				) {
				dma_sync_single_for_cpu(
				amports_get_dma_device(),
				hevc->aux_phy_addr,
				hevc->prefix_aux_size + hevc->suffix_aux_size,
				DMA_FROM_DEVICE);
				if (get_dbg_flag(hevc) &
					H265_DEBUG_BUFMGR_MORE)
					dump_aux_buf(hevc);
			}

			vui_time_scale =
				(u32)(hevc->param.p.vui_time_scale_hi << 16) |
				hevc->param.p.vui_time_scale_lo;
			vui_num_units_in_tick =
				(u32)(hevc->param.
				p.vui_num_units_in_tick_hi << 16) |
				hevc->param.
				p.vui_num_units_in_tick_lo;
			if (hevc->bit_depth_luma !=
				((hevc->param.p.bit_depth & 0xf) + 8)) {
				reconfig_flag = 1;
				hevc_print(hevc, 0, "Bit depth luma = %d\n",
					(hevc->param.p.bit_depth & 0xf) + 8);
			}
			if (hevc->bit_depth_chroma !=
				(((hevc->param.p.bit_depth >> 4) & 0xf) + 8)) {
				reconfig_flag = 1;
				hevc_print(hevc, 0, "Bit depth chroma = %d\n",
					((hevc->param.p.bit_depth >> 4) &
					0xf) + 8);
			}
			hevc->bit_depth_luma =
				(hevc->param.p.bit_depth & 0xf) + 8;
			hevc->bit_depth_chroma =
				((hevc->param.p.bit_depth >> 4) & 0xf) + 8;
			bit_depth_luma = hevc->bit_depth_luma;
			bit_depth_chroma = hevc->bit_depth_chroma;
#ifdef SUPPORT_10BIT
			if (hevc->bit_depth_luma == 8 &&
				hevc->bit_depth_chroma == 8 &&
				enable_mem_saving)
				hevc->mem_saving_mode = 1;
			else
				hevc->mem_saving_mode = 0;
#endif
			if (reconfig_flag &&
				(get_double_write_mode(hevc) & 0x10) == 0)
				init_decode_head_hw(hevc);

			if ((vui_time_scale != 0)
				&& (vui_num_units_in_tick != 0)) {
				hevc->frame_dur =
					div_u64(96000ULL *
						vui_num_units_in_tick,
						vui_time_scale);
					if (hevc->get_frame_dur != true)
						schedule_work(
						&hevc->notify_work);

				hevc->get_frame_dur = true;
#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
				gvs->frame_dur = hevc->frame_dur;
#endif
			}

			if (hevc->video_signal_type !=
				((hevc->param.p.video_signal_type << 16)
				| hevc->param.p.color_description)) {
				u32 v = hevc->param.p.video_signal_type;
				u32 c = hevc->param.p.color_description;
#if 0
			if (v & 0x2000) {
				hevc_print(hevc, 0,
				"video_signal_type present:\n");
				hevc_print(hevc, 0, " %s %s\n",
				video_format_names[(v >> 10) & 7],
					((v >> 9) & 1) ?
					"full_range" : "limited");
				if (v & 0x100) {
					hevc_print(hevc, 0,
					" color_description present:\n");
					hevc_print(hevc, 0,
					"  color_primarie = %s\n",
					color_primaries_names
					[v & 0xff]);
					hevc_print(hevc, 0,
					"  transfer_characteristic = %s\n",
					transfer_characteristics_names
					[(c >> 8) & 0xff]);
					hevc_print(hevc, 0,
					"  matrix_coefficient = %s\n",
					matrix_coeffs_names[c & 0xff]);
				}
			}
#endif
		hevc->video_signal_type = (v << 16) | c;
		video_signal_type = hevc->video_signal_type;
	}

	if (use_cma &&
		(hevc->param.p.slice_segment_address == 0)
		&& (hevc->pic_list_init_flag == 0)) {
		int log = hevc->param.p.log2_min_coding_block_size_minus3;
		int log_s = hevc->param.p.log2_diff_max_min_coding_block_size;

		hevc->pic_w = hevc->param.p.pic_width_in_luma_samples;
		hevc->pic_h = hevc->param.p.pic_height_in_luma_samples;
		hevc->lcu_size = 1 << (log + 3 + log_s);
		hevc->lcu_size_log2 = log2i(hevc->lcu_size);
		if (hevc->pic_w == 0 || hevc->pic_h == 0
						|| hevc->lcu_size == 0
						|| is_oversize(hevc->pic_w, hevc->pic_h)
						||  (!hevc->skip_first_nal &&
						(hevc->pic_h == 96) && (hevc->pic_w  == 160))) {
			/* skip search next start code */
			WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG)
						& (~0x2));
			if  ( !hevc->skip_first_nal &&
				(hevc->pic_h == 96) && (hevc->pic_w  == 160))
				hevc->skip_first_nal = 1;
			hevc->skip_flag = 1;
			WRITE_VREG(HEVC_DEC_STATUS_REG,	HEVC_ACTION_DONE);
			/* Interrupt Amrisc to excute */
			WRITE_VREG(HEVC_MCPU_INTR_REQ,	AMRISC_MAIN_REQ);
#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag)
				start_process_time(hevc);
#endif
		} else {
			hevc->sps_num_reorder_pics_0 =
			hevc->param.p.sps_num_reorder_pics_0;
			hevc->pic_list_init_flag = 1;
#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag) {
				vdec_schedule_work(&hevc->work);
			} else
#endif
				up(&h265_sema);
			hevc_print(hevc, 0, "set pic_list_init_flag 1\n");
		}
		return IRQ_HANDLED;
	}

}
	ret =
		hevc_slice_segment_header_process(hevc,
			&hevc->param, decode_pic_begin);
	if (ret < 0) {
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag) {
			hevc->wait_buf = 0;
			hevc->dec_result = DEC_RESULT_AGAIN;
			amhevc_stop();
			restore_decode_state(hevc);
			reset_process_time(hevc);
			vdec_schedule_work(&hevc->work);
			return IRQ_HANDLED;
		}
#else
		;
#endif
	} else if (ret == 0) {
		if ((hevc->new_pic) && (hevc->cur_pic)) {
			hevc->cur_pic->stream_offset =
			READ_VREG(HEVC_SHIFT_BYTE_COUNT);
			hevc_print(hevc, H265_DEBUG_OUT_PTS,
				"read stream_offset = 0x%x\n",
				hevc->cur_pic->stream_offset);
			hevc->cur_pic->aspect_ratio_idc =
				hevc->param.p.aspect_ratio_idc;
			hevc->cur_pic->sar_width =
				hevc->param.p.sar_width;
			hevc->cur_pic->sar_height =
				hevc->param.p.sar_height;
		}

		WRITE_VREG(HEVC_DEC_STATUS_REG,
			HEVC_CODED_SLICE_SEGMENT_DAT);
		/* Interrupt Amrisc to excute */
		WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);

		hevc->start_decoding_time = jiffies;
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag)
			start_process_time(hevc);
#endif
#if 1
		/*to do..., copy aux data to hevc->cur_pic*/
#endif
#ifdef MULTI_INSTANCE_SUPPORT
	} else if (hevc->m_ins_flag) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s, bufmgr ret %d skip, DEC_RESULT_DONE\n",
			__func__, ret);
		hevc->decoded_poc = INVALID_POC;
		hevc->decoding_pic = NULL;
		hevc->dec_result = DEC_RESULT_DONE;
		amhevc_stop();
		reset_process_time(hevc);
		vdec_schedule_work(&hevc->work);
#endif
	} else {
		/* skip, search next start code */
#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
		gvs->drop_frame_count++;
#endif
		WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG) & (~0x2));
			hevc->skip_flag = 1;
		WRITE_VREG(HEVC_DEC_STATUS_REG,	HEVC_ACTION_DONE);
		/* Interrupt Amrisc to excute */
		WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);
	}

	} else if (dec_status == HEVC_DECODE_OVER_SIZE) {
		hevc_print(hevc, 0 , "hevc  decode oversize !!\n");
#ifdef MULTI_INSTANCE_SUPPORT
		if (!hevc->m_ins_flag)
			debug |= (H265_DEBUG_DIS_LOC_ERROR_PROC |
				H265_DEBUG_DIS_SYS_ERROR_PROC);
#endif
		hevc->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
	}
	return IRQ_HANDLED;
}

static void wait_hevc_search_done(struct hevc_state_s *hevc)
{
	int count = 0;
	WRITE_VREG(HEVC_SHIFT_STATUS, 0);
	while (READ_VREG(HEVC_STREAM_CONTROL) & 0x2) {
		msleep(20);
		count++;
		if (count > 100) {
			hevc_print(hevc, 0, "%s timeout\n", __func__);
			break;
		}
	}
}
static irqreturn_t vh265_isr(int irq, void *data)
{
	int i, temp;
	unsigned int dec_status;
	struct hevc_state_s *hevc = (struct hevc_state_s *)data;
	u32 debug_tag;
	dec_status = READ_VREG(HEVC_DEC_STATUS_REG);
	if (hevc->init_flag == 0)
		return IRQ_HANDLED;
	hevc->dec_status = dec_status;
	if (is_log_enable(hevc))
		add_log(hevc,
			"isr: status = 0x%x dec info 0x%x lcu 0x%x shiftbyte 0x%x shiftstatus 0x%x",
			dec_status, READ_HREG(HEVC_DECODE_INFO),
			READ_VREG(HEVC_MPRED_CURR_LCU),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_STATUS));

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"265 isr dec status = 0x%x dec info 0x%x shiftbyte 0x%x shiftstatus 0x%x\n",
			dec_status, READ_HREG(HEVC_DECODE_INFO),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_STATUS));

	debug_tag = READ_HREG(DEBUG_REG1);
	if (debug_tag & 0x10000) {
		dma_sync_single_for_cpu(
			amports_get_dma_device(),
			hevc->lmem_phy_addr,
			LMEM_BUF_SIZE,
			DMA_FROM_DEVICE);

		hevc_print(hevc, 0,
			"LMEM<tag %x>:\n", READ_HREG(DEBUG_REG1));

		if (hevc->mmu_enable)
			temp = 0x500;
		else
			temp = 0x400;
		for (i = 0; i < temp; i += 4) {
			int ii;
			if ((i & 0xf) == 0)
				hevc_print_cont(hevc, 0, "%03x: ", i);
			for (ii = 0; ii < 4; ii++) {
				hevc_print_cont(hevc, 0, "%04x ",
					   hevc->lmem_ptr[i + 3 - ii]);
			}
			if (((i + ii) & 0xf) == 0)
				hevc_print_cont(hevc, 0, "\n");
		}

		if (((udebug_pause_pos & 0xffff)
			== (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx == hevc->decode_idx) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_HREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			hevc->ucode_pause_pos = udebug_pause_pos;
		}
		else if (debug_tag & 0x20000)
			hevc->ucode_pause_pos = 0xffffffff;
		if (hevc->ucode_pause_pos)
			reset_process_time(hevc);
		else
			WRITE_HREG(DEBUG_REG1, 0);
	} else if (debug_tag != 0) {
		hevc_print(hevc, 0,
			"dbg%x: %x l/w/r %x %x %x\n", READ_HREG(DEBUG_REG1),
			   READ_HREG(DEBUG_REG2),
				READ_VREG(HEVC_STREAM_LEVEL),
				READ_VREG(HEVC_STREAM_WR_PTR),
				READ_VREG(HEVC_STREAM_RD_PTR));
		if (((udebug_pause_pos & 0xffff)
			== (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx == hevc->decode_idx) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_HREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			hevc->ucode_pause_pos = udebug_pause_pos;
		}
		if (hevc->ucode_pause_pos)
			reset_process_time(hevc);
		else
			WRITE_HREG(DEBUG_REG1, 0);
		return IRQ_HANDLED;
	}


	if (hevc->pic_list_init_flag == 1)
		return IRQ_HANDLED;

	if (!hevc->m_ins_flag) {
		if (dec_status == HEVC_OVER_DECODE) {
			hevc->over_decode = 1;
			hevc_print(hevc, 0,
				"isr: over decode\n"),
				WRITE_VREG(HEVC_DEC_STATUS_REG, 0);
			return IRQ_HANDLED;
		}
	}

	return IRQ_WAKE_THREAD;

}

static void vh265_set_clk(struct work_struct *work)
{
	struct hevc_state_s *hevc = container_of(work,
		struct hevc_state_s, work);

		int fps = 96000 / hevc->frame_dur;

		if (hevc_source_changed(VFORMAT_HEVC,
			hevc->frame_width, hevc->frame_height, fps) > 0)
			hevc->saved_resolution = hevc->frame_width *
			hevc->frame_height * fps;
}

static void vh265_check_timer_func(unsigned long arg)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)arg;
	struct timer_list *timer = &hevc->timer;
	unsigned char empty_flag;
	unsigned int buf_level;

	enum receviver_start_e state = RECEIVER_INACTIVE;

	if (hevc->init_flag == 0) {
		if (hevc->stat & STAT_TIMER_ARM) {
			mod_timer(&hevc->timer, jiffies + PUT_INTERVAL);
		}
		return;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	if (hevc->m_ins_flag &&
		(get_dbg_flag(hevc) &
		H265_DEBUG_WAIT_DECODE_DONE_WHEN_STOP) == 0 &&
		hw_to_vdec(hevc)->next_status ==
		VDEC_STATUS_DISCONNECTED) {
		hevc->dec_result = DEC_RESULT_FORCE_EXIT;
		vdec_schedule_work(&hevc->work);
		hevc_print(hevc,
			0, "vdec requested to be disconnected\n");
		return;
	}

	if (hevc->m_ins_flag) {
		if ((input_frame_based(hw_to_vdec(hevc)) ||
			(READ_VREG(HEVC_STREAM_LEVEL) > 0xb0)) &&
			((get_dbg_flag(hevc) &
			H265_DEBUG_DIS_LOC_ERROR_PROC) == 0) &&
			(decode_timeout_val > 0) &&
			(hevc->start_process_time > 0) &&
			((1000 * (jiffies - hevc->start_process_time) / HZ)
				> decode_timeout_val)
		) {
			u32 dec_status = READ_VREG(HEVC_DEC_STATUS_REG);
			int current_lcu_idx =
				READ_VREG(HEVC_PARSER_LCU_START)&0xffffff;
			if (dec_status == HEVC_CODED_SLICE_SEGMENT_DAT) {
				if (hevc->last_lcu_idx == current_lcu_idx) {
					if (hevc->decode_timeout_count > 0)
						hevc->decode_timeout_count--;
					if (hevc->decode_timeout_count == 0)
						timeout_process(hevc);
				} else
					restart_process_time(hevc);
				hevc->last_lcu_idx = current_lcu_idx;
			} else {
				hevc->pic_decoded_lcu_idx = current_lcu_idx;
				timeout_process(hevc);
			}
		}
	} else {
#endif
	if (hevc->m_ins_flag == 0 &&
		vf_get_receiver(hevc->provider_name)) {
		state =
			vf_notify_receiver(hevc->provider_name,
				VFRAME_EVENT_PROVIDER_QUREY_STATE,
				NULL);
		if ((state == RECEIVER_STATE_NULL)
			|| (state == RECEIVER_STATE_NONE))
			state = RECEIVER_INACTIVE;
	} else
		state = RECEIVER_INACTIVE;

	empty_flag = (READ_VREG(HEVC_PARSER_INT_STATUS) >> 6) & 0x1;
	/* error watchdog */
	if (hevc->m_ins_flag == 0 &&
		(empty_flag == 0)
		&& (hevc->pic_list_init_flag == 0
			|| hevc->pic_list_init_flag
			== 3)) {
		/* decoder has input */
		if ((get_dbg_flag(hevc) &
			H265_DEBUG_DIS_LOC_ERROR_PROC) == 0) {

			buf_level = READ_VREG(HEVC_STREAM_LEVEL);
			/* receiver has no buffer to recycle */
			if ((state == RECEIVER_INACTIVE) &&
				(kfifo_is_empty(&hevc->display_q) &&
				 buf_level > 0x200)
			   ) {
				if (hevc->error_flag == 0) {
					hevc->error_watchdog_count++;
					if (hevc->error_watchdog_count ==
						error_handle_threshold) {
						hevc_print(hevc, 0,
						"H265 dec err local reset.\n");
						hevc->error_flag = 1;
						hevc->error_watchdog_count = 0;
						hevc->error_skip_nal_wt_cnt = 0;
						hevc->
						error_system_watchdog_count++;
						WRITE_VREG
						(HEVC_ASSIST_MBOX0_IRQ_REG,
						 0x1);
					}
				} else if (hevc->error_flag == 2) {
					int th =
						error_handle_nal_skip_threshold;
					hevc->error_skip_nal_wt_cnt++;
					if (hevc->error_skip_nal_wt_cnt
					== th) {
						hevc->error_flag = 3;
						hevc->error_watchdog_count = 0;
						hevc->
						error_skip_nal_wt_cnt =	0;
						WRITE_VREG
						(HEVC_ASSIST_MBOX0_IRQ_REG,
						 0x1);
					}
				}
			}
		}

		if ((get_dbg_flag(hevc)
			& H265_DEBUG_DIS_SYS_ERROR_PROC) == 0)
			/* receiver has no buffer to recycle */
			if ((state == RECEIVER_INACTIVE) &&
				(kfifo_is_empty(&hevc->display_q))
			   ) {	/* no buffer to recycle */
				if ((get_dbg_flag(hevc) &
					H265_DEBUG_DIS_LOC_ERROR_PROC) !=
					0)
					hevc->error_system_watchdog_count++;
				if (hevc->error_system_watchdog_count ==
					error_handle_system_threshold) {
					/* and it lasts for a while */
					hevc_print(hevc, 0,
					"H265 dec fatal error watchdog.\n");
					hevc->
					error_system_watchdog_count = 0;
					hevc->fatal_error |= DECODER_FATAL_ERROR_UNKNOWN;
				}
			}
	} else {
		hevc->error_watchdog_count = 0;
		hevc->error_system_watchdog_count = 0;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	}
#endif
	if ((hevc->ucode_pause_pos != 0) &&
		(hevc->ucode_pause_pos != 0xffffffff) &&
		udebug_pause_pos != hevc->ucode_pause_pos) {
		hevc->ucode_pause_pos = 0;
		WRITE_HREG(DEBUG_REG1, 0);
	}

	if (get_dbg_flag(hevc) & H265_DEBUG_DUMP_PIC_LIST) {
		dump_pic_list(hevc);
		debug &= ~H265_DEBUG_DUMP_PIC_LIST;
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_TRIG_SLICE_SEGMENT_PROC) {
		WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
		debug &= ~H265_DEBUG_TRIG_SLICE_SEGMENT_PROC;
	}
#ifdef TEST_NO_BUF
	if (hevc->wait_buf)
		WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
#endif
	if (get_dbg_flag(hevc) & H265_DEBUG_HW_RESET) {
		hevc->error_skip_nal_count = error_skip_nal_count;
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

		debug &= ~H265_DEBUG_HW_RESET;
	}

#ifdef ERROR_HANDLE_DEBUG
	if ((dbg_nal_skip_count > 0) && ((dbg_nal_skip_count & 0x10000) != 0)) {
		hevc->error_skip_nal_count = dbg_nal_skip_count & 0xffff;
		dbg_nal_skip_count &= ~0x10000;
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
	}
#endif

	if (radr != 0) {
		if (rval != 0) {
			WRITE_VREG(radr, rval);
			hevc_print(hevc, 0,
				"WRITE_VREG(%x,%x)\n", radr, rval);
		} else
			hevc_print(hevc, 0,
				"READ_VREG(%x)=%x\n", radr, READ_VREG(radr));
		rval = 0;
		radr = 0;
	}
	if (dbg_cmd != 0) {
		if (dbg_cmd == 1) {
			u32 disp_laddr;

			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB &&
				get_double_write_mode(hevc) == 0) {
				disp_laddr =
					READ_VCBUS_REG(AFBC_BODY_BADDR) << 4;
			} else {
				struct canvas_s cur_canvas;

				canvas_read((READ_VCBUS_REG(VD1_IF0_CANVAS0)
					& 0xff), &cur_canvas);
				disp_laddr = cur_canvas.addr;
			}
			hevc_print(hevc, 0,
				"current displayed buffer address %x\r\n",
				disp_laddr);
		}
		dbg_cmd = 0;
	}
	/*don't changed at start.*/
	if (hevc->m_ins_flag == 0 &&
		hevc->get_frame_dur && hevc->show_frame_num > 60 &&
		hevc->frame_dur > 0 && hevc->saved_resolution !=
		hevc->frame_width * hevc->frame_height *
			(96000 / hevc->frame_dur))
		schedule_work(&hevc->set_clk_work);

	mod_timer(timer, jiffies + PUT_INTERVAL);
}

static int h265_task_handle(void *data)
{
	int ret = 0;
	struct hevc_state_s *hevc = (struct hevc_state_s *)data;

	set_user_nice(current, -10);
	while (1) {
		if (use_cma == 0) {
			hevc_print(hevc, 0,
			"ERROR: use_cma can not be changed dynamically\n");
		}
		ret = down_interruptible(&h265_sema);
		if ((hevc->init_flag != 0) && (hevc->pic_list_init_flag == 1)) {
			init_pic_list(hevc);
			init_pic_list_hw(hevc);
			init_buf_spec(hevc);
			hevc->pic_list_init_flag = 2;
			hevc_print(hevc, 0, "set pic_list_init_flag to 2\n");

			WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);

		}

		if (hevc->uninit_list) {
			/*USE_BUF_BLOCK*/
			uninit_pic_list(hevc);
			hevc_print(hevc, 0, "uninit list\n");
			hevc->uninit_list = 0;
#ifdef USE_UNINIT_SEMA
			if (use_cma) {
				up(&hevc->h265_uninit_done_sema);
				while (!kthread_should_stop())
					msleep(1);
				break;
			}
#endif
		}
	}

	return 0;
}

void vh265_free_cmabuf(void)
{
	struct hevc_state_s *hevc = gHevc;

	mutex_lock(&vh265_mutex);

	if (hevc->init_flag) {
		mutex_unlock(&vh265_mutex);
		return;
	}

	mutex_unlock(&vh265_mutex);
}

#ifdef MULTI_INSTANCE_SUPPORT
int vh265_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
#else
int vh265_dec_status(struct vdec_info *vstatus)
#endif
{
#ifdef MULTI_INSTANCE_SUPPORT
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = gHevc;
#endif
	if (!hevc)
		return -1;

	vstatus->frame_width = hevc->frame_width;
	vstatus->frame_height = hevc->frame_height;
	if (hevc->frame_dur != 0)
		vstatus->frame_rate = 96000 / hevc->frame_dur;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = 0;
	vstatus->status = hevc->stat | hevc->fatal_error;
#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	vstatus->bit_rate = gvs->bit_rate;
	vstatus->frame_dur = hevc->frame_dur;
	if (gvs) {
		vstatus->bit_rate = gvs->bit_rate;
		vstatus->frame_data = gvs->frame_data;
		vstatus->total_data = gvs->total_data;
		vstatus->frame_count = gvs->frame_count;
		vstatus->error_frame_count = gvs->error_frame_count;
		vstatus->drop_frame_count = gvs->drop_frame_count;
		vstatus->total_data = gvs->total_data;
		vstatus->samp_cnt = gvs->samp_cnt;
		vstatus->offset = gvs->offset;
	}
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s", DRIVER_NAME);
#endif
	vstatus->ratio_control = hevc->ratio_control;
	return 0;
}

int vh265_set_isreset(struct vdec_s *vdec, int isreset)
{
	is_reset = isreset;
	return 0;
}

static int vh265_vdec_info_init(void)
{
	gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	return 0;
}

#if 0
static void H265_DECODE_INIT(void)
{
	/* enable hevc clocks */
	WRITE_VREG(DOS_GCLK_EN3, 0xffffffff);
	/* *************************************************************** */
	/* Power ON HEVC */
	/* *************************************************************** */
	/* Powerup HEVC */
	WRITE_VREG(P_AO_RTI_GEN_PWR_SLEEP0,
			READ_VREG(P_AO_RTI_GEN_PWR_SLEEP0) & (~(0x3 << 6)));
	WRITE_VREG(DOS_MEM_PD_HEVC, 0x0);
	WRITE_VREG(DOS_SW_RESET3, READ_VREG(DOS_SW_RESET3) | (0x3ffff << 2));
	WRITE_VREG(DOS_SW_RESET3, READ_VREG(DOS_SW_RESET3) & (~(0x3ffff << 2)));
	/* remove isolations */
	WRITE_VREG(AO_RTI_GEN_PWR_ISO0,
			READ_VREG(AO_RTI_GEN_PWR_ISO0) & (~(0x3 << 10)));

}
#endif

static void config_decode_mode(struct hevc_state_s *hevc)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif
	unsigned decode_mode;
	if (!hevc->m_ins_flag)
		decode_mode = DECODE_MODE_SINGLE;
	else if (vdec_frame_based(hw_to_vdec(hevc)))
		decode_mode =
			DECODE_MODE_MULTI_FRAMEBASE;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	else if (vdec->slave) {
		if (force_bypass_dvenl & 0x80000000)
			hevc->bypass_dvenl = force_bypass_dvenl & 0x1;
		else
			hevc->bypass_dvenl = hevc->bypass_dvenl_enable;
		if (dolby_meta_with_el && hevc->bypass_dvenl) {
			hevc->bypass_dvenl = 0;
			hevc_print(hevc, 0,
				"NOT support bypass_dvenl when meta_with_el\n");
		}
		if (hevc->bypass_dvenl)
			decode_mode =
				(hevc->start_parser_type << 8)
				| DECODE_MODE_MULTI_STREAMBASE;
		else
			decode_mode =
				(hevc->start_parser_type << 8)
				| DECODE_MODE_MULTI_DVBAL;
	} else if (vdec->master)
		decode_mode =
			(hevc->start_parser_type << 8)
			| DECODE_MODE_MULTI_DVENL;
#endif
	else
		decode_mode =
			DECODE_MODE_MULTI_STREAMBASE;

	if (hevc->m_ins_flag)
		decode_mode |=
			(hevc->start_decoding_flag << 16);
	/* set MBX0 interrupt flag */
	decode_mode |= (0x80 << 24);
	WRITE_VREG(HEVC_DECODE_MODE, decode_mode);
	WRITE_VREG(HEVC_DECODE_MODE2,
		hevc->rps_set_id);
}

static void vh265_prot_init(struct hevc_state_s *hevc)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif
	/* H265_DECODE_INIT(); */

	hevc_config_work_space_hw(hevc);

	hevc_init_decoder_hw(hevc, 0, 0xffffffff);

	WRITE_VREG(HEVC_WAIT_FLAG, 1);

	/* WRITE_VREG(P_HEVC_MPSR, 1); */

	/* clear mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 1);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(HEVC_PSCALE_CTRL, 0);

	WRITE_VREG(DEBUG_REG1, 0x0 | (dump_nal << 8));

	if ((get_dbg_flag(hevc) &
		(H265_DEBUG_MAN_SKIP_NAL |
		H265_DEBUG_MAN_SEARCH_NAL))
		/*||hevc->m_ins_flag*/
		) {
		WRITE_VREG(NAL_SEARCH_CTL, 0x1);	/* manual parser NAL */
	} else {
		/* check vps/sps/pps/i-slice in ucode */
		unsigned ctl_val = 0x8;
		if (hevc->PB_skip_mode == 0)
			ctl_val = 0x4;	/* check vps/sps/pps only in ucode */
		else if (hevc->PB_skip_mode == 3)
			ctl_val = 0x0;	/* check vps/sps/pps/idr in ucode */
		WRITE_VREG(NAL_SEARCH_CTL, ctl_val);
	}
	if ((get_dbg_flag(hevc) & H265_DEBUG_NO_EOS_SEARCH_DONE)
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		|| vdec->master
		|| vdec->slave
#endif
		)
		WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x10000);

	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL)
		| ((parser_sei_enable & 0x7) << 17));
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL) |
		((parser_dolby_vision_enable & 0x1) << 20));
#endif
	WRITE_VREG(DECODE_STOP_POS, udebug_flag);

	config_decode_mode(hevc);
	config_aux_buf(hevc);
#ifdef SWAP_HEVC_UCODE
	if (!tee_enabled() && hevc->is_swap &&
		get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->mc_dma_handle);
		/*pr_info("write swap buffer %x\n", (u32)(hevc->mc_dma_handle));*/
	}
#endif
#ifdef DETREFILL_ENABLE
	if (hevc->is_swap &&
		get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		WRITE_VREG(HEVC_SAO_DBG_MODE0, 0);
		WRITE_VREG(HEVC_SAO_DBG_MODE1, 0);
	}
#endif
}

static int vh265_local_init(struct hevc_state_s *hevc)
{
	int i;
	int ret = -1;

#ifdef DEBUG_PTS
	hevc->pts_missed = 0;
	hevc->pts_hit = 0;
#endif

	hevc->saved_resolution = 0;
	hevc->get_frame_dur = false;
	hevc->frame_width = hevc->vh265_amstream_dec_info.width;
	hevc->frame_height = hevc->vh265_amstream_dec_info.height;
	if (is_oversize(hevc->frame_width, hevc->frame_height)) {
		pr_info("over size : %u x %u.\n",
			hevc->frame_width, hevc->frame_height);
		hevc->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
		return ret;
	}

	if (hevc->max_pic_w && hevc->max_pic_h) {
		hevc->is_4k = !(hevc->max_pic_w && hevc->max_pic_h) ||
			((hevc->max_pic_w * hevc->max_pic_h) >
			1920 * 1088) ? true : false;
	} else {
		hevc->is_4k = !(hevc->frame_width && hevc->frame_height) ||
			((hevc->frame_width * hevc->frame_height) >
			1920 * 1088) ? true : false;
	}

	hevc->frame_dur =
		(hevc->vh265_amstream_dec_info.rate ==
		 0) ? 3600 : hevc->vh265_amstream_dec_info.rate;
#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	gvs->frame_dur = hevc->frame_dur;
#endif
	if (hevc->frame_width && hevc->frame_height)
		hevc->frame_ar = hevc->frame_height * 0x100 / hevc->frame_width;

	if (i_only_flag)
		hevc->i_only = i_only_flag & 0xff;
	else if ((unsigned long) hevc->vh265_amstream_dec_info.param
		& 0x08)
		hevc->i_only = 0x7;
	else
		hevc->i_only = 0x0;
	hevc->error_watchdog_count = 0;
	hevc->sei_present_flag = 0;
	pts_unstable = ((unsigned long)hevc->vh265_amstream_dec_info.param
		& 0x40) >> 6;
	hevc_print(hevc, 0,
		"h265:pts_unstable=%d\n", pts_unstable);
/*
 *TODO:FOR VERSION
 */
	hevc_print(hevc, 0,
		"h265: ver (%d,%d) decinfo: %dx%d rate=%d\n", h265_version,
		   0, hevc->frame_width, hevc->frame_height, hevc->frame_dur);

	if (hevc->frame_dur == 0)
		hevc->frame_dur = 96000 / 24;

	INIT_KFIFO(hevc->display_q);
	INIT_KFIFO(hevc->newframe_q);


	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hevc->vfpool[i];

		hevc->vfpool[i].index = -1;
		kfifo_put(&hevc->newframe_q, vf);
	}


	ret = hevc_local_init(hevc);

	return ret;
}
#ifdef MULTI_INSTANCE_SUPPORT
static s32 vh265_init(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
static s32 vh265_init(struct hevc_state_s *hevc)
{

#endif
	int ret, size = -1;
	int fw_size = 0x1000 * 16;
	struct firmware_s *fw = NULL;

	init_timer(&hevc->timer);

	hevc->stat |= STAT_TIMER_INIT;
	if (vh265_local_init(hevc) < 0)
		return -EBUSY;

	mutex_init(&hevc->chunks_mutex);
	INIT_WORK(&hevc->notify_work, vh265_notify_work);
	INIT_WORK(&hevc->set_clk_work, vh265_set_clk);

	fw = vmalloc(sizeof(struct firmware_s) + fw_size);
	if (IS_ERR_OR_NULL(fw))
		return -ENOMEM;

	if (hevc->mmu_enable)
		if (get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_GXM)
			size = get_firmware_data(VIDEO_DEC_HEVC_MMU, fw->data);
		else {
			if (!hevc->is_4k) {
				/* if an older version of the fw was loaded, */
				/* needs try to load noswap fw because the */
				/* old fw package dose not contain the swap fw.*/
				size = get_firmware_data(
					VIDEO_DEC_HEVC_MMU_SWAP, fw->data);
				if (size < 0)
					size = get_firmware_data(
						VIDEO_DEC_HEVC_MMU, fw->data);
				else if (size)
					hevc->is_swap = true;
			} else
				size = get_firmware_data(VIDEO_DEC_HEVC_MMU,
					fw->data);
		}
	else
		size = get_firmware_data(VIDEO_DEC_HEVC, fw->data);

	if (size < 0) {
		pr_err("get firmware fail.\n");
		vfree(fw);
		return -1;
	}

	fw->len = size;

#ifdef SWAP_HEVC_UCODE
	if (!tee_enabled() && hevc->is_swap &&
		get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		if (hevc->mmu_enable) {
			hevc->swap_size = (4 * (4 * SZ_1K)); /*max 4 swap code, each 0x400*/
			hevc->mc_cpu_addr =
				dma_alloc_coherent(amports_get_dma_device(),
					hevc->swap_size,
					&hevc->mc_dma_handle, GFP_KERNEL);
			if (!hevc->mc_cpu_addr) {
				amhevc_disable();
				pr_info("vh265 mmu swap ucode loaded fail.\n");
				return -ENOMEM;
			}

			memcpy((u8 *) hevc->mc_cpu_addr, fw->data + SWAP_HEVC_OFFSET,
				hevc->swap_size);

			hevc_print(hevc, 0,
				"vh265 mmu ucode swap loaded %x\n",
				hevc->mc_dma_handle);
		}
	}
#endif

#ifdef MULTI_INSTANCE_SUPPORT
	if (hevc->m_ins_flag) {
		hevc->timer.data = (ulong) hevc;
		hevc->timer.function = vh265_check_timer_func;
		hevc->timer.expires = jiffies + PUT_INTERVAL;

#ifdef USE_UNINIT_SEMA
		sema_init(&hevc->h265_uninit_done_sema, 0);
#endif

		/*add_timer(&hevc->timer);
		 *hevc->stat |= STAT_TIMER_ARM;
		 */

		INIT_WORK(&hevc->work, vh265_work);

		hevc->fw = fw;

		return 0;
	}
#endif
	hevc_enable_DMC(hw_to_vdec(hevc));
	amhevc_enable();

	if (hevc->mmu_enable)
		if (get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_GXM)
			ret = amhevc_loadmc_ex(VFORMAT_HEVC, "h265_mmu", fw->data);
		else {
			if (!hevc->is_4k) {
				/* if an older version of the fw was loaded, */
				/* needs try to load noswap fw because the */
				/* old fw package dose not contain the swap fw. */
				ret = amhevc_loadmc_ex(VFORMAT_HEVC,
					"hevc_mmu_swap", fw->data);
				if (ret < 0)
					ret = amhevc_loadmc_ex(VFORMAT_HEVC,
						"h265_mmu", fw->data);
				else
					hevc->is_swap = true;
			} else
				ret = amhevc_loadmc_ex(VFORMAT_HEVC,
					"h265_mmu", fw->data);
		}
	else
		ret = amhevc_loadmc_ex(VFORMAT_HEVC, NULL, fw->data);

	if (ret < 0) {
		amhevc_disable();
		vfree(fw);
		pr_err("H265: the %s fw loading failed, err: %x\n",
			tee_enabled() ? "TEE" : "local", ret);
		return -EBUSY;
	}

	vfree(fw);

	hevc->stat |= STAT_MC_LOAD;

#ifdef DETREFILL_ENABLE
	if (hevc->is_swap &&
		get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM)
		init_detrefill_buf(hevc);
#endif
	/* enable AMRISC side protocol */
	vh265_prot_init(hevc);

	if (vdec_request_threaded_irq(VDEC_IRQ_0, vh265_isr,
				vh265_isr_thread_fn,
				IRQF_ONESHOT,/*run thread on this irq disabled*/
				"vh265-irq", (void *)hevc)) {
		hevc_print(hevc, 0, "vh265 irq register error.\n");
		amhevc_disable();
		return -ENOENT;
	}

	hevc->stat |= STAT_ISR_REG;
	hevc->provider_name = PROVIDER_NAME;

#ifdef MULTI_INSTANCE_SUPPORT
	vf_provider_init(&vh265_vf_prov, hevc->provider_name,
				&vh265_vf_provider, vdec);
	vf_reg_provider(&vh265_vf_prov);
	vf_notify_receiver(hevc->provider_name, VFRAME_EVENT_PROVIDER_START,
				NULL);
	if (hevc->frame_dur != 0) {
		if (!is_reset) {
			vf_notify_receiver(hevc->provider_name,
					VFRAME_EVENT_PROVIDER_FR_HINT,
					(void *)
					((unsigned long)hevc->frame_dur));
			fr_hint_status = VDEC_HINTED;
		}
	} else
		fr_hint_status = VDEC_NEED_HINT;
#else
	vf_provider_init(&vh265_vf_prov, PROVIDER_NAME, &vh265_vf_provider,
					 hevc);
	vf_reg_provider(&vh265_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
	if (hevc->frame_dur != 0) {
		vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_FR_HINT,
				(void *)
				((unsigned long)hevc->frame_dur));
		fr_hint_status = VDEC_HINTED;
	} else
		fr_hint_status = VDEC_NEED_HINT;
#endif
	hevc->stat |= STAT_VF_HOOK;

	hevc->timer.data = (ulong) hevc;
	hevc->timer.function = vh265_check_timer_func;
	hevc->timer.expires = jiffies + PUT_INTERVAL;

	add_timer(&hevc->timer);

	hevc->stat |= STAT_TIMER_ARM;

	if (use_cma) {
#ifdef USE_UNINIT_SEMA
		sema_init(&hevc->h265_uninit_done_sema, 0);
#endif
		if (h265_task == NULL) {
			sema_init(&h265_sema, 1);
			h265_task =
				kthread_run(h265_task_handle, hevc,
						"kthread_h265");
		}
	}
	/* hevc->stat |= STAT_KTHREAD; */
#if 0
	if (get_dbg_flag(hevc) & H265_DEBUG_FORCE_CLK) {
		hevc_print(hevc, 0, "%s force clk\n", __func__);
		WRITE_VREG(HEVC_IQIT_CLK_RST_CTRL,
				   READ_VREG(HEVC_IQIT_CLK_RST_CTRL) |
				   ((1 << 2) | (1 << 1)));
		WRITE_VREG(HEVC_DBLK_CFG0,
				READ_VREG(HEVC_DBLK_CFG0) | ((1 << 2) |
					(1 << 1) | 0x3fff0000));/* 2,29:16 */
		WRITE_VREG(HEVC_SAO_CTRL1, READ_VREG(HEVC_SAO_CTRL1) |
				(1 << 2));	/* 2 */
		WRITE_VREG(HEVC_MPRED_CTRL1, READ_VREG(HEVC_MPRED_CTRL1) |
				(1 << 24));	/* 24 */
		WRITE_VREG(HEVC_STREAM_CONTROL,
				READ_VREG(HEVC_STREAM_CONTROL) |
				(1 << 15));	/* 15 */
		WRITE_VREG(HEVC_CABAC_CONTROL, READ_VREG(HEVC_CABAC_CONTROL) |
				(1 << 13));	/* 13 */
		WRITE_VREG(HEVC_PARSER_CORE_CONTROL,
				READ_VREG(HEVC_PARSER_CORE_CONTROL) |
				(1 << 15));	/* 15 */
		WRITE_VREG(HEVC_PARSER_INT_CONTROL,
				READ_VREG(HEVC_PARSER_INT_CONTROL) |
				(1 << 15));	/* 15 */
		WRITE_VREG(HEVC_PARSER_IF_CONTROL,
				READ_VREG(HEVC_PARSER_IF_CONTROL) | ((1 << 6) |
					(1 << 3) | (1 << 1)));	/* 6, 3, 1 */
		WRITE_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG, 0xffffffff);	/* 31:0 */
		WRITE_VREG(HEVCD_MCRCC_CTL1, READ_VREG(HEVCD_MCRCC_CTL1) |
				(1 << 3));	/* 3 */
	}
#endif
#ifdef SWAP_HEVC_UCODE
	if (!tee_enabled() && hevc->is_swap &&
		get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->mc_dma_handle);
		/*pr_info("write swap buffer %x\n", (u32)(hevc->mc_dma_handle));*/
	}
#endif

#ifndef MULTI_INSTANCE_SUPPORT
	set_vdec_func(&vh265_dec_status);
#endif
	amhevc_start();
	hevc->stat |= STAT_VDEC_RUN;
	hevc->init_flag = 1;
	error_handle_threshold = 30;
	/* pr_info("%d, vh265_init, RP=0x%x\n",
	 *   __LINE__, READ_VREG(HEVC_STREAM_RD_PTR));
	 */

	return 0;
}

static int vh265_stop(struct hevc_state_s *hevc)
{
	if (get_dbg_flag(hevc) &
		H265_DEBUG_WAIT_DECODE_DONE_WHEN_STOP) {
		int wait_timeout_count = 0;

		while (READ_VREG(HEVC_DEC_STATUS_REG) ==
			   HEVC_CODED_SLICE_SEGMENT_DAT &&
				wait_timeout_count < 10){
			wait_timeout_count++;
			msleep(20);
		}
	}
	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}

	if (hevc->stat & STAT_ISR_REG) {
#ifdef MULTI_INSTANCE_SUPPORT
		if (!hevc->m_ins_flag)
#endif
			WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 0);
		vdec_free_irq(VDEC_IRQ_0, (void *)hevc);
		hevc->stat &= ~STAT_ISR_REG;
	}

	hevc->stat &= ~STAT_TIMER_INIT;
	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}

	if (hevc->stat & STAT_VF_HOOK) {
		if (fr_hint_status == VDEC_HINTED) {
			vf_notify_receiver(hevc->provider_name,
					VFRAME_EVENT_PROVIDER_FR_END_HINT,
					NULL);
		}
		fr_hint_status = VDEC_NO_NEED_HINT;
		vf_unreg_provider(&vh265_vf_prov);
		hevc->stat &= ~STAT_VF_HOOK;
	}

	hevc_local_uninit(hevc);

	if (use_cma) {
		hevc->uninit_list = 1;
		up(&h265_sema);
#ifdef USE_UNINIT_SEMA
		down(&hevc->h265_uninit_done_sema);
		if (!IS_ERR(h265_task)) {
			kthread_stop(h265_task);
			h265_task = NULL;
		}
#else
		while (hevc->uninit_list)	/* wait uninit complete */
			msleep(20);
#endif

	}
	hevc->init_flag = 0;
	hevc->first_sc_checked = 0;
	cancel_work_sync(&hevc->notify_work);
	cancel_work_sync(&hevc->set_clk_work);
	uninit_mmu_buffers(hevc);
	amhevc_disable();

	kfree(gvs);
	gvs = NULL;

	return 0;
}

#ifdef MULTI_INSTANCE_SUPPORT
static void reset_process_time(struct hevc_state_s *hevc)
{
	if (hevc->start_process_time) {
		unsigned int process_time =
			1000 * (jiffies - hevc->start_process_time) / HZ;
		hevc->start_process_time = 0;
		if (process_time > max_process_time[hevc->index])
			max_process_time[hevc->index] = process_time;
	}
}

static void start_process_time(struct hevc_state_s *hevc)
{
	hevc->start_process_time = jiffies;
	hevc->decode_timeout_count = 2;
	hevc->last_lcu_idx = 0;
}

static void restart_process_time(struct hevc_state_s *hevc)
{
	hevc->start_process_time = jiffies;
	hevc->decode_timeout_count = 2;
}

static void timeout_process(struct hevc_state_s *hevc)
{
	hevc->timeout_num++;
	amhevc_stop();
	read_decode_info(hevc);

	hevc_print(hevc,
		0, "%s decoder timeout\n", __func__);
	check_pic_decoded_error(hevc,
				hevc->pic_decoded_lcu_idx);
	hevc->decoded_poc = hevc->curr_POC;
	hevc->decoding_pic = NULL;
	hevc->dec_result = DEC_RESULT_DONE;
	reset_process_time(hevc);
	vdec_schedule_work(&hevc->work);
}

#ifdef CONSTRAIN_MAX_BUF_NUM
static int get_vf_ref_only_buf_count(struct hevc_state_s *hevc)
{
	struct PIC_s *pic;
	int i;
	int count = 0;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		if (pic->output_mark == 0 && pic->referenced == 0
			&& pic->output_ready == 1)
			count++;
	}

	return count;
}

static int get_used_buf_count(struct hevc_state_s *hevc)
{
	struct PIC_s *pic;
	int i;
	int count = 0;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		if (pic->output_mark != 0 || pic->referenced != 0
			|| pic->output_ready != 0)
			count++;
	}

	return count;
}
#endif


static unsigned char is_new_pic_available(struct hevc_state_s *hevc)
{
	struct PIC_s *new_pic = NULL;
	struct PIC_s *pic;
	/* recycle un-used pic */
	int i;
	/*return 1 if pic_list is not initialized yet*/
	if (hevc->pic_list_init_flag != 3)
		return 1;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		if (pic->output_mark == 0 && pic->referenced == 0
			&& pic->output_ready == 0
			) {
			if (new_pic) {
				if (pic->POC < new_pic->POC)
					new_pic = pic;
			} else
				new_pic = pic;
		}
	}
	return (new_pic != NULL) ? 1 : 0;
}

static int vmh265_stop(struct hevc_state_s *hevc)
{
	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}
	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}
	if (hevc->stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_0, (void *)hevc);
		hevc->stat &= ~STAT_ISR_REG;
	}

	if (hevc->stat & STAT_VF_HOOK) {
		if (fr_hint_status == VDEC_HINTED)
			vf_notify_receiver(hevc->provider_name,
					VFRAME_EVENT_PROVIDER_FR_END_HINT,
					NULL);
		fr_hint_status = VDEC_NO_NEED_HINT;
		vf_unreg_provider(&vh265_vf_prov);
		hevc->stat &= ~STAT_VF_HOOK;
	}

	hevc_local_uninit(hevc);

	if (use_cma) {
		hevc->uninit_list = 1;
		reset_process_time(hevc);
		vdec_schedule_work(&hevc->work);
#ifdef USE_UNINIT_SEMA
		if (hevc->init_flag) {
			down(&hevc->h265_uninit_done_sema);
		}
#else
		while (hevc->uninit_list)	/* wait uninit complete */
			msleep(20);
#endif
	}
	hevc->init_flag = 0;
	hevc->first_sc_checked = 0;
	cancel_work_sync(&hevc->work);
	cancel_work_sync(&hevc->notify_work);
	cancel_work_sync(&hevc->set_clk_work);
	uninit_mmu_buffers(hevc);

	vfree(hevc->fw);
	hevc->fw = NULL;

	dump_log(hevc);
	return 0;
}

static unsigned char get_data_check_sum
	(struct hevc_state_s *hevc, int size)
{
	int jj;
	int sum = 0;
	u8 *data = NULL;

	if (!hevc->chunk->block->is_mapped)
		data = codec_mm_vmap(hevc->chunk->block->start +
			hevc->chunk->offset, size);
	else
		data = ((u8 *)hevc->chunk->block->start_virt) +
			hevc->chunk->offset;

	for (jj = 0; jj < size; jj++)
		sum += data[jj];

	if (!hevc->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);
	return sum;
}

static void vh265_notify_work(struct work_struct *work)
{
	struct hevc_state_s *hevc =
						container_of(work,
						struct hevc_state_s,
						notify_work);
	struct vdec_s *vdec = hw_to_vdec(hevc);
#ifdef MULTI_INSTANCE_SUPPORT
	if (vdec->fr_hint_state == VDEC_NEED_HINT) {
		vf_notify_receiver(hevc->provider_name,
					VFRAME_EVENT_PROVIDER_FR_HINT,
					(void *)
					((unsigned long)hevc->frame_dur));
		vdec->fr_hint_state = VDEC_HINTED;
	} else if (fr_hint_status == VDEC_NEED_HINT) {
		vf_notify_receiver(hevc->provider_name,
					VFRAME_EVENT_PROVIDER_FR_HINT,
					(void *)
					((unsigned long)hevc->frame_dur));
		fr_hint_status = VDEC_HINTED;
	}
#else
	if (fr_hint_status == VDEC_NEED_HINT)
		vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_FR_HINT,
					(void *)
					((unsigned long)hevc->frame_dur));
		fr_hint_status = VDEC_HINTED;
	}
#endif

	return;
}

static void vh265_work(struct work_struct *work)
{
	struct hevc_state_s *hevc = container_of(work,
		struct hevc_state_s, work);
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (hevc->uninit_list) {
		/*USE_BUF_BLOCK*/
		uninit_pic_list(hevc);
		hevc_print(hevc, 0, "uninit list\n");
		hevc->uninit_list = 0;
#ifdef USE_UNINIT_SEMA
		up(&hevc->h265_uninit_done_sema);
#endif
		return;
	}

	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	if (hevc->pic_list_init_flag == 1
		&& (hevc->dec_result != DEC_RESULT_FORCE_EXIT)) {
		hevc->pic_list_init_flag = 2;
		init_pic_list(hevc);
		init_pic_list_hw(hevc);
		init_buf_spec(hevc);
		hevc_print(hevc, 0,
			"set pic_list_init_flag to 2\n");

		WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
		return;
	}

	hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
		"%s dec_result %d %x %x %x\n",
		__func__,
		hevc->dec_result,
		READ_VREG(HEVC_STREAM_LEVEL),
		READ_VREG(HEVC_STREAM_WR_PTR),
		READ_VREG(HEVC_STREAM_RD_PTR));

	if (((hevc->dec_result == DEC_RESULT_GET_DATA) ||
		(hevc->dec_result == DEC_RESULT_GET_DATA_RETRY))
		&& (hw_to_vdec(hevc)->next_status !=
		VDEC_STATUS_DISCONNECTED)) {
		if (!vdec_has_more_input(vdec)) {
			hevc->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hevc->work);
			return;
		}
		if (!input_frame_based(vdec)) {
			int r = vdec_sync_input(vdec);
			if (r >= 0x200) {
				WRITE_VREG(HEVC_DECODE_SIZE,
					READ_VREG(HEVC_DECODE_SIZE) + r);

				hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
					"%s DEC_RESULT_GET_DATA %x %x %x mpc %x size 0x%x\n",
					__func__,
					READ_VREG(HEVC_STREAM_LEVEL),
					READ_VREG(HEVC_STREAM_WR_PTR),
					READ_VREG(HEVC_STREAM_RD_PTR),
					READ_VREG(HEVC_MPC_E), r);

				start_process_time(hevc);
				if (READ_VREG(HEVC_DEC_STATUS_REG)
				 == HEVC_DECODE_BUFEMPTY2)
					WRITE_VREG(HEVC_DEC_STATUS_REG,
						HEVC_ACTION_DONE);
				else
					WRITE_VREG(HEVC_DEC_STATUS_REG,
						HEVC_ACTION_DEC_CONT);
			} else {
				hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;
				vdec_schedule_work(&hevc->work);
			}
			return;
		}

		/*below for frame_base*/
		if (hevc->dec_result == DEC_RESULT_GET_DATA) {
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_GET_DATA %x %x %x mpc %x\n",
				__func__,
				READ_VREG(HEVC_STREAM_LEVEL),
				READ_VREG(HEVC_STREAM_WR_PTR),
				READ_VREG(HEVC_STREAM_RD_PTR),
				READ_VREG(HEVC_MPC_E));
			mutex_lock(&hevc->chunks_mutex);
			vdec_vframe_dirty(vdec, hevc->chunk);
			hevc->chunk = NULL;
			mutex_unlock(&hevc->chunks_mutex);
			vdec_clean_input(vdec);
		}

		/*if (is_new_pic_available(hevc)) {*/
		if (run_ready(vdec, VDEC_HEVC)) {
			int r;
			int decode_size;
			r = vdec_prepare_input(vdec, &hevc->chunk);
			if (r < 0) {
				hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;

				hevc_print(hevc,
					PRINT_FLAG_VDEC_DETAIL,
					"amvdec_vh265: Insufficient data\n");

				vdec_schedule_work(&hevc->work);
				return;
			}
			hevc->dec_result = DEC_RESULT_NONE;
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s: chunk size 0x%x sum 0x%x mpc %x\n",
				__func__, r,
				(get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS) ?
				get_data_check_sum(hevc, r) : 0,
				READ_VREG(HEVC_MPC_E));

			if (get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA) {
				int jj;
				u8 *data = NULL;

				if (!hevc->chunk->block->is_mapped)
					data = codec_mm_vmap(
						hevc->chunk->block->start +
						hevc->chunk->offset, r);
				else
					data = ((u8 *)
						hevc->chunk->block->start_virt)
						+ hevc->chunk->offset;

				for (jj = 0; jj < r; jj++) {
					if ((jj & 0xf) == 0)
						hevc_print(hevc,
						PRINT_FRAMEBASE_DATA,
							"%06x:", jj);
					hevc_print_cont(hevc,
					PRINT_FRAMEBASE_DATA,
						"%02x ", data[jj]);
					if (((jj + 1) & 0xf) == 0)
						hevc_print_cont(hevc,
						PRINT_FRAMEBASE_DATA,
							"\n");
				}

				if (!hevc->chunk->block->is_mapped)
					codec_mm_unmap_phyaddr(data);
			}

			decode_size = hevc->chunk->size +
				(hevc->chunk->offset & (VDEC_FIFO_ALIGN - 1));
			WRITE_VREG(HEVC_DECODE_SIZE,
				READ_VREG(HEVC_DECODE_SIZE) + decode_size);

			vdec_enable_input(vdec);

			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s: mpc %x\n",
				__func__, READ_VREG(HEVC_MPC_E));

			start_process_time(hevc);
			WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
		} else{
			hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;

			/*hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
			 *	"amvdec_vh265: Insufficient data\n");
			 */

			vdec_schedule_work(&hevc->work);
		}
		return;
	} else if (hevc->dec_result == DEC_RESULT_DONE) {
		/* if (!hevc->ctx_valid)
			hevc->ctx_valid = 1; */
		decode_frame_count[hevc->index]++;
#ifdef DETREFILL_ENABLE
	if (hevc->is_swap &&
		get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		if (hevc->delrefill_check == 2) {
			delrefill(hevc);
			amhevc_stop();
		}
	}
#endif
		if (hevc->mmu_enable && ((hevc->double_write_mode & 0x10) == 0)) {
			hevc->used_4k_num =
				READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;
			if (hevc->used_4k_num >= 0 &&
				hevc->cur_pic &&
				hevc->cur_pic->scatter_alloc
				== 1) {
				hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
				"%s pic index %d scatter_alloc %d page_start %d\n",
				"decoder_mmu_box_free_idx_tail",
				hevc->cur_pic->index,
				hevc->cur_pic->scatter_alloc,
				hevc->used_4k_num);
				decoder_mmu_box_free_idx_tail(
				hevc->mmu_box,
				hevc->cur_pic->index,
				hevc->used_4k_num);
				hevc->cur_pic->scatter_alloc = 2;
			}
		}
		hevc->pic_decoded_lcu_idx =
			READ_VREG(HEVC_PARSER_LCU_START)
			& 0xffffff;

		if (vdec->master == NULL && vdec->slave == NULL &&
			hevc->empty_flag == 0) {
			hevc->over_decode =
				(READ_VREG(HEVC_SHIFT_STATUS) >> 15) & 0x1;
			if (hevc->over_decode)
				hevc_print(hevc, 0,
					"!!!Over decode\n");
		}

		if (is_log_enable(hevc))
			add_log(hevc,
				"%s dec_result %d lcu %d used_mmu %d shiftbyte 0x%x decbytes 0x%x",
				__func__,
				hevc->dec_result,
				hevc->pic_decoded_lcu_idx,
				hevc->used_4k_num,
				READ_VREG(HEVC_SHIFT_BYTE_COUNT),
				READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
				hevc->start_shift_bytes
				);

		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s dec_result %d (%x %x %x) lcu %d used_mmu %d shiftbyte 0x%x decbytes 0x%x\n",
			__func__,
			hevc->dec_result,
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
			hevc->pic_decoded_lcu_idx,
			hevc->used_4k_num,
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
			hevc->start_shift_bytes
			);

		hevc->used_4k_num = -1;

		check_pic_decoded_error(hevc,
			hevc->pic_decoded_lcu_idx);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#if 1
		if (vdec->slave) {
			if (dv_debug & 0x1)
				vdec_set_flag(vdec->slave,
					VDEC_FLAG_SELF_INPUT_CONTEXT);
			else
				vdec_set_flag(vdec->slave,
					VDEC_FLAG_OTHER_INPUT_CONTEXT);
		}
#else
		if (vdec->slave) {
			if (no_interleaved_el_slice)
				vdec_set_flag(vdec->slave,
				VDEC_FLAG_INPUT_KEEP_CONTEXT);
				/* this will move real HW pointer for input */
			else
				vdec_set_flag(vdec->slave, 0);
				/* this will not move real HW pointer
				 *and SL layer decoding
				 *will start from same stream position
				 *as current BL decoder
				 */
		}
#endif
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		hevc->shift_byte_count_lo
			= READ_VREG(HEVC_SHIFT_BYTE_COUNT);
		if (vdec->slave) {
			/*cur is base, found enhance*/
			struct hevc_state_s *hevc_el =
			(struct hevc_state_s *)
				vdec->slave->private;
			if (hevc_el)
				hevc_el->shift_byte_count_lo =
				hevc->shift_byte_count_lo;
		} else if (vdec->master) {
			/*cur is enhance, found base*/
			struct hevc_state_s *hevc_ba =
			(struct hevc_state_s *)
				vdec->master->private;
			if (hevc_ba)
				hevc_ba->shift_byte_count_lo =
				hevc->shift_byte_count_lo;
		}
#endif
		mutex_lock(&hevc->chunks_mutex);
		vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
		hevc->chunk = NULL;
		mutex_unlock(&hevc->chunks_mutex);
	} else if (hevc->dec_result == DEC_RESULT_AGAIN) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		if (!vdec_has_more_input(vdec)) {
			hevc->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hevc->work);
			return;
		}
#ifdef AGAIN_HAS_THRESHOLD
		hevc->next_again_flag = 1;
#endif
	} else if (hevc->dec_result == DEC_RESULT_EOS) {
		struct PIC_s *pic;
		hevc->eos = 1;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if ((vdec->master || vdec->slave) &&
			aux_data_is_avaible(hevc))
			dolby_get_meta(hevc);
#endif
		check_pic_decoded_error(hevc,
			hevc->pic_decoded_lcu_idx);
		pic = get_pic_by_POC(hevc, hevc->curr_POC);
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s: end of stream, last dec poc %d => 0x%pf\n",
			__func__, hevc->curr_POC, pic);
		flush_output(hevc, pic);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		hevc->shift_byte_count_lo
			= READ_VREG(HEVC_SHIFT_BYTE_COUNT);
		if (vdec->slave) {
			/*cur is base, found enhance*/
			struct hevc_state_s *hevc_el =
			(struct hevc_state_s *)
				vdec->slave->private;
			if (hevc_el)
				hevc_el->shift_byte_count_lo =
				hevc->shift_byte_count_lo;
		} else if (vdec->master) {
			/*cur is enhance, found base*/
			struct hevc_state_s *hevc_ba =
			(struct hevc_state_s *)
				vdec->master->private;
			if (hevc_ba)
				hevc_ba->shift_byte_count_lo =
				hevc->shift_byte_count_lo;
		}
#endif
		mutex_lock(&hevc->chunks_mutex);
		vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
		hevc->chunk = NULL;
		mutex_unlock(&hevc->chunks_mutex);
	} else if (hevc->dec_result == DEC_RESULT_FORCE_EXIT) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s: force exit\n",
			__func__);
		if (hevc->stat & STAT_VDEC_RUN) {
			amhevc_stop();
			hevc->stat &= ~STAT_VDEC_RUN;
		}
		if (hevc->stat & STAT_ISR_REG) {
				WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 0);
			vdec_free_irq(VDEC_IRQ_0, (void *)hevc);
			hevc->stat &= ~STAT_ISR_REG;
		}
		hevc_print(hevc, 0, "%s: force exit end\n",
			__func__);
	}

	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}

	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}

	wait_hevc_search_done(hevc);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (hevc->switch_dvlayer_flag) {
		if (vdec->slave)
			vdec_set_next_sched(vdec, vdec->slave);
		else if (vdec->master)
			vdec_set_next_sched(vdec, vdec->master);
	} else if (vdec->slave || vdec->master)
		vdec_set_next_sched(vdec, vdec);
#endif

	/* mark itself has all HW resource released and input released */
	if (vdec->parallel_dec == 1)
		vdec_core_finish_run(vdec, CORE_MASK_HEVC);
	else
		vdec_core_finish_run(vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	if (hevc->vdec_cb)
		hevc->vdec_cb(hw_to_vdec(hevc), hevc->vdec_cb_arg);
}

static int vh265_hw_ctx_restore(struct hevc_state_s *hevc)
{
	/* new to do ... */
	vh265_prot_init(hevc);
	return 0;
}
static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	int tvp = vdec_secure(hw_to_vdec(hevc)) ?
		CODEC_MM_FLAGS_TVP : 0;
	bool ret = 0;
	if (step == 0x12)
		return 0;
	else if (step == 0x11)
		step = 0x12;

	if (hevc->eos)
		return 0;
	if (!hevc->first_sc_checked && hevc->mmu_enable) {
		int size = decoder_mmu_box_sc_check(hevc->mmu_box, tvp);
		hevc->first_sc_checked =1;
		hevc_print(hevc, 0,
			"vh265 cached=%d  need_size=%d speed= %d ms\n",
			size, (hevc->need_cache_size >> PAGE_SHIFT),
			(int)(get_jiffies_64() - hevc->sc_start_time) * 1000/HZ);
	}
	if (vdec_stream_based(vdec) && (hevc->init_flag == 0)
			&& pre_decode_buf_level != 0) {
			u32 rp, wp, level;

			rp = READ_PARSER_REG(PARSER_VIDEO_RP);
			wp = READ_PARSER_REG(PARSER_VIDEO_WP);
			if (wp < rp)
				level = vdec->input.size + wp - rp;
			else
				level = wp - rp;

			if (level < pre_decode_buf_level)
				return 0;
	}

#ifdef AGAIN_HAS_THRESHOLD
	if (hevc->next_again_flag &&
		(!vdec_frame_based(vdec))) {
		u32 parser_wr_ptr =
			READ_PARSER_REG(PARSER_VIDEO_WP);
		if (parser_wr_ptr >= hevc->pre_parser_wr_ptr &&
			(parser_wr_ptr - hevc->pre_parser_wr_ptr) <
			again_threshold) {
			int r = vdec_sync_input(vdec);
			hevc_print(hevc,
			PRINT_FLAG_VDEC_DETAIL, "%s buf lelvel:%x\n",  __func__, r);
			return 0;
		}
	}
#endif

	if (disp_vframe_valve_level &&
		kfifo_len(&hevc->display_q) >=
		disp_vframe_valve_level) {
		hevc->valve_count--;
		if (hevc->valve_count <= 0)
			hevc->valve_count = 2;
		else
			return 0;
	}

	ret = is_new_pic_available(hevc);
	if (!ret) {
		hevc_print(hevc,
		PRINT_FLAG_VDEC_DETAIL, "%s=>%d\r\n",
		__func__, ret);
	}

#ifdef CONSTRAIN_MAX_BUF_NUM
	if (hevc->pic_list_init_flag == 3) {
		if (run_ready_max_vf_only_num > 0 &&
			get_vf_ref_only_buf_count(hevc) >=
			run_ready_max_vf_only_num
			)
			ret = 0;
		if (run_ready_display_q_num > 0 &&
			kfifo_len(&hevc->display_q) >=
			run_ready_display_q_num)
			ret = 0;

		/*avoid more buffers consumed when
		switching resolution*/
		if (run_ready_max_buf_num == 0xff &&
			get_used_buf_count(hevc) >=
			get_work_pic_num(hevc))
			ret = 0;
		else if (run_ready_max_buf_num &&
			get_used_buf_count(hevc) >=
			run_ready_max_buf_num)
			ret = 0;
	}
#endif

	if (ret)
		not_run_ready[hevc->index] = 0;
	else
		not_run_ready[hevc->index]++;
	if (vdec->parallel_dec == 1)
		return ret ? (CORE_MASK_HEVC) : 0;
	else
		return ret ? (CORE_MASK_VDEC_1 | CORE_MASK_HEVC) : 0;
}

static void run(struct vdec_s *vdec, unsigned long mask,
	void (*callback)(struct vdec_s *, void *), void *arg)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	int r, loadr = 0;
	unsigned char check_sum = 0;

	run_count[hevc->index]++;
	hevc->vdec_cb_arg = arg;
	hevc->vdec_cb = callback;
	hevc->aux_data_dirty = 1;
	hevc_reset_core(vdec);

#ifdef AGAIN_HAS_THRESHOLD
	hevc->pre_parser_wr_ptr =
		READ_PARSER_REG(PARSER_VIDEO_WP);
	hevc->next_again_flag = 0;
#endif
	r = vdec_prepare_input(vdec, &hevc->chunk);
	if (r < 0) {
		input_empty[hevc->index]++;
		hevc->dec_result = DEC_RESULT_AGAIN;
		hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
			"ammvdec_vh265: Insufficient data\n");

		vdec_schedule_work(&hevc->work);
		return;
	}
	input_empty[hevc->index] = 0;
	hevc->dec_result = DEC_RESULT_NONE;
	if (vdec_frame_based(vdec) &&
		((get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
		|| is_log_enable(hevc)))
		check_sum = get_data_check_sum(hevc, r);

	if (is_log_enable(hevc))
		add_log(hevc,
			"%s: size 0x%x sum 0x%x shiftbyte 0x%x",
			__func__, r,
			check_sum,
			READ_VREG(HEVC_SHIFT_BYTE_COUNT)
			);
	hevc->start_shift_bytes = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
	hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
		"%s: size 0x%x sum 0x%x (%x %x %x %x %x) byte count %x\n",
		__func__, r,
		check_sum,
		READ_VREG(HEVC_STREAM_LEVEL),
		READ_VREG(HEVC_STREAM_WR_PTR),
		READ_VREG(HEVC_STREAM_RD_PTR),
		READ_PARSER_REG(PARSER_VIDEO_RP),
		READ_PARSER_REG(PARSER_VIDEO_WP),
		hevc->start_shift_bytes
		);
	if ((get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA) &&
		input_frame_based(vdec)) {
		int jj;
		u8 *data = NULL;

		if (!hevc->chunk->block->is_mapped)
			data = codec_mm_vmap(hevc->chunk->block->start +
				hevc->chunk->offset, r);
		else
			data = ((u8 *)hevc->chunk->block->start_virt)
				+ hevc->chunk->offset;

		for (jj = 0; jj < r; jj++) {
			if ((jj & 0xf) == 0)
				hevc_print(hevc, PRINT_FRAMEBASE_DATA,
					"%06x:", jj);
			hevc_print_cont(hevc, PRINT_FRAMEBASE_DATA,
				"%02x ", data[jj]);
			if (((jj + 1) & 0xf) == 0)
				hevc_print_cont(hevc, PRINT_FRAMEBASE_DATA,
					"\n");
		}

		if (!hevc->chunk->block->is_mapped)
			codec_mm_unmap_phyaddr(data);
	}
	if (vdec->mc_loaded) {
		/*firmware have load before,
		  and not changes to another.
		  ignore reload.
		*/
		if (tee_enabled() && hevc->is_swap &&
			get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM)
			WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->swap_addr);
	} else {
		if (hevc->mmu_enable)
			if (get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_GXM)
				loadr = amhevc_vdec_loadmc_ex(VFORMAT_HEVC, vdec,
						"h265_mmu", hevc->fw->data);
			else {
				if (!hevc->is_4k) {
					/* if an older version of the fw was loaded, */
					/* needs try to load noswap fw because the */
					/* old fw package dose not contain the swap fw.*/
					loadr = amhevc_vdec_loadmc_ex(
						VFORMAT_HEVC, vdec,
						"hevc_mmu_swap",
						hevc->fw->data);
					if (loadr < 0)
						loadr = amhevc_vdec_loadmc_ex(
							VFORMAT_HEVC, vdec,
							"h265_mmu",
							hevc->fw->data);
					else
						hevc->is_swap = true;
				} else
					loadr = amhevc_vdec_loadmc_ex(
						VFORMAT_HEVC, vdec,
						"h265_mmu", hevc->fw->data);
			}
		else
			loadr = amhevc_vdec_loadmc_ex(VFORMAT_HEVC, vdec,
					NULL, hevc->fw->data);

		if (loadr < 0) {
			amhevc_disable();
			hevc_print(hevc, 0, "H265: the %s fw loading failed, err: %x\n",
				tee_enabled() ? "TEE" : "local", loadr);
			hevc->dec_result = DEC_RESULT_FORCE_EXIT;
			vdec_schedule_work(&hevc->work);
			return;
		}

		if (tee_enabled() && hevc->is_swap &&
			get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM)
			hevc->swap_addr = READ_VREG(HEVC_STREAM_SWAP_BUFFER2);
#ifdef DETREFILL_ENABLE
		if (hevc->is_swap &&
			get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM)
			init_detrefill_buf(hevc);
#endif
		vdec->mc_loaded = 1;
		vdec->mc_type = VFORMAT_HEVC;
	}
	if (vh265_hw_ctx_restore(hevc) < 0) {
		vdec_schedule_work(&hevc->work);
		return;
	}
	vdec_enable_input(vdec);

	WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

	if (vdec_frame_based(vdec)) {
		WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, 0);
		r = hevc->chunk->size +
			(hevc->chunk->offset & (VDEC_FIFO_ALIGN - 1));
		hevc->decode_size = r;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	else {
		if (vdec->master || vdec->slave)
			WRITE_VREG(HEVC_SHIFT_BYTE_COUNT,
				hevc->shift_byte_count_lo);
	}
#endif
	WRITE_VREG(HEVC_DECODE_SIZE, r);
	/*WRITE_VREG(HEVC_DECODE_COUNT, hevc->decode_idx);*/
	hevc->init_flag = 1;

	if (hevc->pic_list_init_flag == 3)
		init_pic_list_hw(hevc);

	backup_decode_state(hevc);

	start_process_time(hevc);
	mod_timer(&hevc->timer, jiffies);
	hevc->stat |= STAT_TIMER_ARM;
	hevc->stat |= STAT_ISR_REG;
	amhevc_start();
	hevc->stat |= STAT_VDEC_RUN;
}

static void reset(struct vdec_s *vdec)
{

	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	hevc_print(hevc,
		PRINT_FLAG_VDEC_DETAIL, "%s\r\n", __func__);

}

static irqreturn_t vh265_irq_cb(struct vdec_s *vdec, int irq)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	return vh265_isr(0, hevc);
}

static irqreturn_t vh265_threaded_irq_cb(struct vdec_s *vdec, int irq)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	return vh265_isr_thread_fn(0, hevc);
}
#endif

static int amvdec_h265_probe(struct platform_device *pdev)
{
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
#else
	struct vdec_dev_reg_s *pdata =
		(struct vdec_dev_reg_s *)pdev->dev.platform_data;
#endif
	int ret;
	struct hevc_state_s *hevc;

	hevc = vmalloc(sizeof(struct hevc_state_s));
	if (hevc == NULL) {
		hevc_print(hevc, 0, "%s vmalloc hevc failed\r\n", __func__);
		return -ENOMEM;
	}
	gHevc = hevc;
	if ((debug & H265_NO_CHANG_DEBUG_FLAG_IN_CODE) == 0)
			debug &= (~(H265_DEBUG_DIS_LOC_ERROR_PROC |
					H265_DEBUG_DIS_SYS_ERROR_PROC));
	memset(hevc, 0, sizeof(struct hevc_state_s));
	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0, "%s\r\n", __func__);
	mutex_lock(&vh265_mutex);

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXTVBB) &&
		(parser_sei_enable & 0x100) == 0)
		parser_sei_enable = 7; /*old 1*/
	hevc->m_ins_flag = 0;
	hevc->init_flag = 0;
	hevc->first_sc_checked = 0;
	hevc->uninit_list = 0;
	hevc->fatal_error = 0;
	hevc->show_frame_num = 0;
	hevc->frameinfo_enable = 1;
#ifdef MULTI_INSTANCE_SUPPORT
	hevc->platform_dev = pdev;
	platform_set_drvdata(pdev, pdata);
#endif

	if (pdata == NULL) {
		hevc_print(hevc, 0,
			"\namvdec_h265 memory resource undefined.\n");
		vfree(hevc);
		mutex_unlock(&vh265_mutex);
		return -EFAULT;
	}
	if (mmu_enable_force == 0) {
		if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_GXL
			|| double_write_mode == 0x10)
			hevc->mmu_enable = 0;
		else
			hevc->mmu_enable = 1;
	}
	if (init_mmu_buffers(hevc)) {
		hevc_print(hevc, 0,
			"\n 265 mmu init failed!\n");
		vfree(hevc);
		mutex_unlock(&vh265_mutex);
		return -EFAULT;
	}

	ret = decoder_bmmu_box_alloc_buf_phy(hevc->bmmu_box, BMMU_WORKSPACE_ID,
			work_buf_size, DRIVER_NAME, &hevc->buf_start);
	if (ret < 0) {
		uninit_mmu_buffers(hevc);
		vfree(hevc);
		mutex_unlock(&vh265_mutex);
		return ret;
	}
	hevc->buf_size = work_buf_size;

	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0,
			"===H.265 decoder mem resource 0x%lx size 0x%x\n",
			hevc->buf_start, hevc->buf_size);
	}

	if (pdata->sys_info)
		hevc->vh265_amstream_dec_info = *pdata->sys_info;
	else {
		hevc->vh265_amstream_dec_info.width = 0;
		hevc->vh265_amstream_dec_info.height = 0;
		hevc->vh265_amstream_dec_info.rate = 30;
	}
#ifndef MULTI_INSTANCE_SUPPORT
	if (pdata->flag & DEC_FLAG_HEVC_WORKAROUND) {
		workaround_enable |= 3;
		hevc_print(hevc, 0,
			"amvdec_h265 HEVC_WORKAROUND flag set.\n");
	} else
		workaround_enable &= ~3;
#endif
	hevc->cma_dev = pdata->cma_dev;
	vh265_vdec_info_init();

#ifdef MULTI_INSTANCE_SUPPORT
	pdata->private = hevc;
	pdata->dec_status = vh265_dec_status;
	pdata->set_isreset = vh265_set_isreset;
	is_reset = 0;
	if (vh265_init(pdata) < 0) {
#else
	if (vh265_init(hevc) < 0) {
#endif
		hevc_print(hevc, 0,
			"\namvdec_h265 init failed.\n");
		hevc_local_uninit(hevc);
		uninit_mmu_buffers(hevc);
		vfree(hevc);
		pdata->dec_status = NULL;
		mutex_unlock(&vh265_mutex);
		return -ENODEV;
	}
	/*set the max clk for smooth playing...*/
	hevc_source_changed(VFORMAT_HEVC,
			3840, 2160, 60);
	mutex_unlock(&vh265_mutex);

	return 0;
}

static int amvdec_h265_remove(struct platform_device *pdev)
{
	struct hevc_state_s *hevc = gHevc;

	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0, "%s\r\n", __func__);

	mutex_lock(&vh265_mutex);

	vh265_stop(hevc);

	hevc_source_changed(VFORMAT_HEVC, 0, 0, 0);


#ifdef DEBUG_PTS
	hevc_print(hevc, 0,
		"pts missed %ld, pts hit %ld, duration %d\n",
		hevc->pts_missed, hevc->pts_hit, hevc->frame_dur);
#endif

	vfree(hevc);
	hevc = NULL;
	gHevc = NULL;

	mutex_unlock(&vh265_mutex);

	return 0;
}
/****************************************/

static struct platform_driver amvdec_h265_driver = {
	.probe = amvdec_h265_probe,
	.remove = amvdec_h265_remove,
#ifdef CONFIG_PM
	.suspend = amhevc_suspend,
	.resume = amhevc_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

#ifdef MULTI_INSTANCE_SUPPORT
static void vh265_dump_state(struct vdec_s *vdec)
{
	int i;
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	hevc_print(hevc, 0,
		"====== %s\n", __func__);

	hevc_print(hevc, 0,
		"width/height (%d/%d), reorder_pic_num %d buf count(bufspec size) %d, video_signal_type 0x%x, is_swap %d\n",
		hevc->frame_width,
		hevc->frame_height,
		hevc->sps_num_reorder_pics_0,
		get_work_pic_num(hevc),
		hevc->video_signal_type_debug,
		hevc->is_swap
		);

	hevc_print(hevc, 0,
		"is_framebase(%d), eos %d, dec_result 0x%x dec_frm %d disp_frm %d run %d not_run_ready %d input_empty %d\n",
		input_frame_based(vdec),
		hevc->eos,
		hevc->dec_result,
		decode_frame_count[hevc->index],
		display_frame_count[hevc->index],
		run_count[hevc->index],
		not_run_ready[hevc->index],
		input_empty[hevc->index]
		);

	if (vf_get_receiver(vdec->vf_provider_name)) {
		enum receviver_start_e state =
		vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_QUREY_STATE,
			NULL);
		hevc_print(hevc, 0,
			"\nreceiver(%s) state %d\n",
			vdec->vf_provider_name,
			state);
	}

	hevc_print(hevc, 0,
	"%s, newq(%d/%d), dispq(%d/%d), vf prepare/get/put (%d/%d/%d), pic_list_init_flag(%d), is_new_pic_available(%d)\n",
	__func__,
	kfifo_len(&hevc->newframe_q),
	VF_POOL_SIZE,
	kfifo_len(&hevc->display_q),
	VF_POOL_SIZE,
	hevc->vf_pre_count,
	hevc->vf_get_count,
	hevc->vf_put_count,
	hevc->pic_list_init_flag,
	is_new_pic_available(hevc)
	);

	dump_pic_list(hevc);

	for (i = 0; i < BUF_POOL_SIZE; i++) {
		hevc_print(hevc, 0,
			"Buf(%d) start_adr 0x%x size 0x%x used %d\n",
			i,
			hevc->m_BUF[i].start_adr,
			hevc->m_BUF[i].size,
			hevc->m_BUF[i].used_flag);
	}

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		hevc_print(hevc, 0,
			"mv_Buf(%d) start_adr 0x%x size 0x%x used %d\n",
			i,
			hevc->m_mv_BUF[i].start_adr,
			hevc->m_mv_BUF[i].size,
			hevc->m_mv_BUF[i].used_flag);
	}

	hevc_print(hevc, 0,
		"HEVC_DEC_STATUS_REG=0x%x\n",
		READ_VREG(HEVC_DEC_STATUS_REG));
	hevc_print(hevc, 0,
		"HEVC_MPC_E=0x%x\n",
		READ_VREG(HEVC_MPC_E));
	hevc_print(hevc, 0,
		"HEVC_DECODE_MODE=0x%x\n",
		READ_VREG(HEVC_DECODE_MODE));
	hevc_print(hevc, 0,
		"HEVC_DECODE_MODE2=0x%x\n",
		READ_VREG(HEVC_DECODE_MODE2));
	hevc_print(hevc, 0,
		"NAL_SEARCH_CTL=0x%x\n",
		READ_VREG(NAL_SEARCH_CTL));
	hevc_print(hevc, 0,
		"HEVC_PARSER_LCU_START=0x%x\n",
		READ_VREG(HEVC_PARSER_LCU_START));
	hevc_print(hevc, 0,
		"HEVC_DECODE_SIZE=0x%x\n",
		READ_VREG(HEVC_DECODE_SIZE));
	hevc_print(hevc, 0,
		"HEVC_SHIFT_BYTE_COUNT=0x%x\n",
		READ_VREG(HEVC_SHIFT_BYTE_COUNT));
	hevc_print(hevc, 0,
		"HEVC_STREAM_START_ADDR=0x%x\n",
		READ_VREG(HEVC_STREAM_START_ADDR));
	hevc_print(hevc, 0,
		"HEVC_STREAM_END_ADDR=0x%x\n",
		READ_VREG(HEVC_STREAM_END_ADDR));
	hevc_print(hevc, 0,
		"HEVC_STREAM_LEVEL=0x%x\n",
		READ_VREG(HEVC_STREAM_LEVEL));
	hevc_print(hevc, 0,
		"HEVC_STREAM_WR_PTR=0x%x\n",
		READ_VREG(HEVC_STREAM_WR_PTR));
	hevc_print(hevc, 0,
		"HEVC_STREAM_RD_PTR=0x%x\n",
		READ_VREG(HEVC_STREAM_RD_PTR));
	hevc_print(hevc, 0,
		"PARSER_VIDEO_RP=0x%x\n",
		READ_PARSER_REG(PARSER_VIDEO_RP));
	hevc_print(hevc, 0,
		"PARSER_VIDEO_WP=0x%x\n",
		READ_PARSER_REG(PARSER_VIDEO_WP));

	if (input_frame_based(vdec) &&
		(get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA)
		) {
		int jj;
		if (hevc->chunk && hevc->chunk->block &&
			hevc->chunk->size > 0) {
			u8 *data = NULL;
			if (!hevc->chunk->block->is_mapped)
				data = codec_mm_vmap(hevc->chunk->block->start +
					hevc->chunk->offset, hevc->chunk->size);
			else
				data = ((u8 *)hevc->chunk->block->start_virt)
					+ hevc->chunk->offset;
			hevc_print(hevc, 0,
				"frame data size 0x%x\n",
				hevc->chunk->size);
			for (jj = 0; jj < hevc->chunk->size; jj++) {
				if ((jj & 0xf) == 0)
					hevc_print(hevc,
					PRINT_FRAMEBASE_DATA,
						"%06x:", jj);
				hevc_print_cont(hevc,
				PRINT_FRAMEBASE_DATA,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					hevc_print_cont(hevc,
					PRINT_FRAMEBASE_DATA,
						"\n");
			}

			if (!hevc->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
	}

}


static int ammvdec_h265_probe(struct platform_device *pdev)
{

	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct hevc_state_s *hevc = NULL;
	int ret;
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	int config_val;
#endif
	if (pdata == NULL) {
		pr_info("\nammvdec_h265 memory resource undefined.\n");
		return -EFAULT;
	}

	/* hevc = (struct hevc_state_s *)devm_kzalloc(&pdev->dev,
		sizeof(struct hevc_state_s), GFP_KERNEL); */
	hevc = vmalloc(sizeof(struct hevc_state_s));
	memset(hevc, 0, sizeof(struct hevc_state_s));
	if (hevc == NULL) {
		pr_info("\nammvdec_h265 device data allocation failed\n");
		return -ENOMEM;
	}
	pdata->private = hevc;
	pdata->dec_status = vh265_dec_status;
	/* pdata->set_trickmode = set_trickmode; */
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vh265_irq_cb;
	pdata->threaded_irq_handler = vh265_threaded_irq_cb;
	pdata->dump_state = vh265_dump_state;

	hevc->index = pdev->id;
	hevc->m_ins_flag = 1;

	if (pdata->use_vfm_path) {
		snprintf(pdata->vf_provider_name,
		VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
		hevc->frameinfo_enable = 1;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	else if (vdec_dual(pdata)) {
		struct hevc_state_s *hevc_pair = NULL;

		if (dv_toggle_prov_name) /*debug purpose*/
			snprintf(pdata->vf_provider_name,
			VDEC_PROVIDER_NAME_SIZE,
				(pdata->master) ? VFM_DEC_DVBL_PROVIDER_NAME :
				VFM_DEC_DVEL_PROVIDER_NAME);
		else
			snprintf(pdata->vf_provider_name,
			VDEC_PROVIDER_NAME_SIZE,
				(pdata->master) ? VFM_DEC_DVEL_PROVIDER_NAME :
				VFM_DEC_DVBL_PROVIDER_NAME);
		hevc->dolby_enhance_flag = pdata->master ? 1 : 0;
		if (pdata->master)
			hevc_pair = (struct hevc_state_s *)
				pdata->master->private;
		else if (pdata->slave)
			hevc_pair = (struct hevc_state_s *)
				pdata->slave->private;
		if (hevc_pair)
			hevc->shift_byte_count_lo =
			hevc_pair->shift_byte_count_lo;
	}
#endif
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			MULTI_INSTANCE_PROVIDER_NAME ".%02x", pdev->id & 0xff);

	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vh265_vf_provider, pdata);

	hevc->provider_name = pdata->vf_provider_name;
	platform_set_drvdata(pdev, pdata);

	hevc->platform_dev = pdev;

	if (((get_dbg_flag(hevc) & IGNORE_PARAM_FROM_CONFIG) == 0) &&
			pdata->config && pdata->config_len) {
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
		/*use ptr config for doubel_write_mode, etc*/
		hevc_print(hevc, 0, "pdata->config=%s\n", pdata->config);

		if (get_config_int(pdata->config, "hevc_double_write_mode",
				&config_val) == 0)
			hevc->double_write_mode = config_val;
		else
			hevc->double_write_mode = double_write_mode;

		if (get_config_int(pdata->config, "save_buffer_mode",
				&config_val) == 0)
			hevc->save_buffer_mode = config_val;
		else
			hevc->save_buffer_mode = 0;

		/*use ptr config for max_pic_w, etc*/
		if (get_config_int(pdata->config, "hevc_buf_width",
				&config_val) == 0) {
				hevc->max_pic_w = config_val;
		}
		if (get_config_int(pdata->config, "hevc_buf_height",
				&config_val) == 0) {
				hevc->max_pic_h = config_val;
		}

#endif
	} else {
		if (pdata->sys_info)
			hevc->vh265_amstream_dec_info = *pdata->sys_info;
		else {
			hevc->vh265_amstream_dec_info.width = 0;
			hevc->vh265_amstream_dec_info.height = 0;
			hevc->vh265_amstream_dec_info.rate = 30;
		}
		hevc->double_write_mode = double_write_mode;
	}
	if (hevc->save_buffer_mode && dynamic_buf_num_margin > 2)
		hevc->dynamic_buf_num_margin = dynamic_buf_num_margin -2;
	else
		hevc->dynamic_buf_num_margin = dynamic_buf_num_margin;

	if (mmu_enable_force == 0) {
		if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_GXL)
			hevc->mmu_enable = 0;
		else
			hevc->mmu_enable = 1;
	}

	if (init_mmu_buffers(hevc) < 0) {
		hevc_print(hevc, 0,
			"\n 265 mmu init failed!\n");
		mutex_unlock(&vh265_mutex);
		/* devm_kfree(&pdev->dev, (void *)hevc);*/
		if (hevc)
			vfree((void *)hevc);
		pdata->dec_status = NULL;
		return -EFAULT;
	}
#if 0
	hevc->buf_start = pdata->mem_start;
	hevc->buf_size = pdata->mem_end - pdata->mem_start + 1;
#else

	ret = decoder_bmmu_box_alloc_buf_phy(hevc->bmmu_box,
			BMMU_WORKSPACE_ID, work_buf_size,
			DRIVER_NAME, &hevc->buf_start);
	if (ret < 0) {
		uninit_mmu_buffers(hevc);
		/* devm_kfree(&pdev->dev, (void *)hevc); */
		if (hevc)
			vfree((void *)hevc);
		pdata->dec_status = NULL;
		mutex_unlock(&vh265_mutex);
		return ret;
	}
	hevc->buf_size = work_buf_size;
#endif
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXTVBB) &&
		(parser_sei_enable & 0x100) == 0)
		parser_sei_enable = 7;
	hevc->init_flag = 0;
	hevc->first_sc_checked = 0;
	hevc->uninit_list = 0;
	hevc->fatal_error = 0;
	hevc->show_frame_num = 0;

	/*
	 *hevc->mc_buf_spec.buf_end = pdata->mem_end + 1;
	 *for (i = 0; i < WORK_BUF_SPEC_NUM; i++)
	 *	amvh265_workbuff_spec[i].start_adr = pdata->mem_start;
	 */
	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0,
			"===H.265 decoder mem resource 0x%lx size 0x%x\n",
			   hevc->buf_start, hevc->buf_size);
	}

	hevc_print(hevc, 0,
		"dynamic_buf_num_margin=%d\n",
		hevc->dynamic_buf_num_margin);
	hevc_print(hevc, 0,
		"double_write_mode=%d\n",
		hevc->double_write_mode);

	hevc->cma_dev = pdata->cma_dev;

	if (vh265_init(pdata) < 0) {
		hevc_print(hevc, 0,
			"\namvdec_h265 init failed.\n");
		hevc_local_uninit(hevc);
		uninit_mmu_buffers(hevc);
		/* devm_kfree(&pdev->dev, (void *)hevc); */
		if (hevc)
			vfree((void *)hevc);
		pdata->dec_status = NULL;
		return -ENODEV;
	}

	vdec_set_prepare_level(pdata, start_decode_buf_level);

	/*set the max clk for smooth playing...*/
	hevc_source_changed(VFORMAT_HEVC,
			3840, 2160, 60);
	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_HEVC);
	else
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
					| CORE_MASK_COMBINE);

	return 0;
}

static int ammvdec_h265_remove(struct platform_device *pdev)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (hevc == NULL)
		return 0;

	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0, "%s\r\n", __func__);

	vmh265_stop(hevc);

	/* vdec_source_changed(VFORMAT_H264, 0, 0, 0); */
	if (vdec->parallel_dec == 1)
		vdec_core_release(hw_to_vdec(hevc), CORE_MASK_HEVC);
	else
		vdec_core_release(hw_to_vdec(hevc), CORE_MASK_HEVC);

	vdec_set_status(hw_to_vdec(hevc), VDEC_STATUS_DISCONNECTED);

	vfree((void *)hevc);
	return 0;
}

static struct platform_driver ammvdec_h265_driver = {
	.probe = ammvdec_h265_probe,
	.remove = ammvdec_h265_remove,
#ifdef CONFIG_PM
	.suspend = amhevc_suspend,
	.resume = amhevc_resume,
#endif
	.driver = {
		.name = MULTI_DRIVER_NAME,
	}
};
#endif

static struct codec_profile_t amvdec_h265_profile = {
	.name = "hevc",
	.profile = ""
};
static struct mconfig h265_configs[] = {
	MC_PU32("use_cma", &use_cma),
	MC_PU32("bit_depth_luma", &bit_depth_luma),
	MC_PU32("bit_depth_chroma", &bit_depth_chroma),
	MC_PU32("video_signal_type", &video_signal_type),
#ifdef ERROR_HANDLE_DEBUG
	MC_PU32("dbg_nal_skip_flag", &dbg_nal_skip_flag),
	MC_PU32("dbg_nal_skip_count", &dbg_nal_skip_count),
#endif
	MC_PU32("radr", &radr),
	MC_PU32("rval", &rval),
	MC_PU32("dbg_cmd", &dbg_cmd),
	MC_PU32("dbg_skip_decode_index", &dbg_skip_decode_index),
	MC_PU32("endian", &endian),
	MC_PU32("step", &step),
	MC_PU32("udebug_flag", &udebug_flag),
	MC_PU32("decode_pic_begin", &decode_pic_begin),
	MC_PU32("slice_parse_begin", &slice_parse_begin),
	MC_PU32("nal_skip_policy", &nal_skip_policy),
	MC_PU32("i_only_flag", &i_only_flag),
	MC_PU32("error_handle_policy", &error_handle_policy),
	MC_PU32("error_handle_threshold", &error_handle_threshold),
	MC_PU32("error_handle_nal_skip_threshold",
		&error_handle_nal_skip_threshold),
	MC_PU32("error_handle_system_threshold",
		&error_handle_system_threshold),
	MC_PU32("error_skip_nal_count", &error_skip_nal_count),
	MC_PU32("debug", &debug),
	MC_PU32("debug_mask", &debug_mask),
	MC_PU32("buffer_mode", &buffer_mode),
	MC_PU32("double_write_mode", &double_write_mode),
	MC_PU32("buf_alloc_width", &buf_alloc_width),
	MC_PU32("buf_alloc_height", &buf_alloc_height),
	MC_PU32("dynamic_buf_num_margin", &dynamic_buf_num_margin),
	MC_PU32("max_buf_num", &max_buf_num),
	MC_PU32("buf_alloc_size", &buf_alloc_size),
	MC_PU32("buffer_mode_dbg", &buffer_mode_dbg),
	MC_PU32("mem_map_mode", &mem_map_mode),
	MC_PU32("enable_mem_saving", &enable_mem_saving),
	MC_PU32("force_w_h", &force_w_h),
	MC_PU32("force_fps", &force_fps),
	MC_PU32("max_decoding_time", &max_decoding_time),
	MC_PU32("prefix_aux_buf_size", &prefix_aux_buf_size),
	MC_PU32("suffix_aux_buf_size", &suffix_aux_buf_size),
	MC_PU32("interlace_enable", &interlace_enable),
	MC_PU32("pts_unstable", &pts_unstable),
	MC_PU32("parser_sei_enable", &parser_sei_enable),
	MC_PU32("start_decode_buf_level", &start_decode_buf_level),
	MC_PU32("decode_timeout_val", &decode_timeout_val),
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	MC_PU32("parser_dolby_vision_enable", &parser_dolby_vision_enable),
	MC_PU32("dv_toggle_prov_name", &dv_toggle_prov_name),
	MC_PU32("dv_debug", &dv_debug),
#endif
};
static struct mconfig_node decoder_265_node;

static int __init amvdec_h265_driver_init_module(void)
{
	struct BuffInfo_s *p_buf_info;

	if (vdec_is_support_4k()) {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
			p_buf_info = &amvh265_workbuff_spec[2];
		else
			p_buf_info = &amvh265_workbuff_spec[1];
	} else
		p_buf_info = &amvh265_workbuff_spec[0];

	init_buff_spec(NULL, p_buf_info);
	work_buf_size =
		(p_buf_info->end_adr - p_buf_info->start_adr
		 + 0xffff) & (~0xffff);

	pr_debug("amvdec_h265 module init\n");
	error_handle_policy = 0;

#ifdef ERROR_HANDLE_DEBUG
	dbg_nal_skip_flag = 0;
	dbg_nal_skip_count = 0;
#endif
	udebug_flag = 0;
	decode_pic_begin = 0;
	slice_parse_begin = 0;
	step = 0;
	buf_alloc_size = 0;

#ifdef MULTI_INSTANCE_SUPPORT
	if (platform_driver_register(&ammvdec_h265_driver))
		pr_err("failed to register ammvdec_h265 driver\n");

#endif
	if (platform_driver_register(&amvdec_h265_driver)) {
		pr_err("failed to register amvdec_h265 driver\n");
		return -ENODEV;
	}
#if 1/*MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8*/
	if (!has_hevc_vdec()) {
		/* not support hevc */
		amvdec_h265_profile.name = "hevc_unsupport";
	}
	if (vdec_is_support_4k()) {
		if (is_meson_m8m2_cpu()) {
			/* m8m2 support 4k */
			amvdec_h265_profile.profile = "4k";
		} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) {
			amvdec_h265_profile.profile =
				"8k, 8bit, 10bit, dwrite, compressed";
		}else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB) {
			amvdec_h265_profile.profile =
				"4k, 8bit, 10bit, dwrite, compressed";
		} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_MG9TV)
			amvdec_h265_profile.profile = "4k";
	}
#endif
	if (codec_mm_get_total_size() < 80 * SZ_1M) {
		pr_info("amvdec_h265 default mmu enabled.\n");
		mmu_enable = 1;
	}

	vcodec_profile_register(&amvdec_h265_profile);
	INIT_REG_NODE_CONFIGS("media.decoder", &decoder_265_node,
		"h265", h265_configs, CONFIG_FOR_RW);
	return 0;
}

static void __exit amvdec_h265_driver_remove_module(void)
{
	pr_debug("amvdec_h265 module remove.\n");

#ifdef MULTI_INSTANCE_SUPPORT
	platform_driver_unregister(&ammvdec_h265_driver);
#endif
	platform_driver_unregister(&amvdec_h265_driver);
}

/****************************************/
/*
 *module_param(stat, uint, 0664);
 *MODULE_PARM_DESC(stat, "\n amvdec_h265 stat\n");
 */
module_param(use_cma, uint, 0664);
MODULE_PARM_DESC(use_cma, "\n amvdec_h265 use_cma\n");

module_param(bit_depth_luma, uint, 0664);
MODULE_PARM_DESC(bit_depth_luma, "\n amvdec_h265 bit_depth_luma\n");

module_param(bit_depth_chroma, uint, 0664);
MODULE_PARM_DESC(bit_depth_chroma, "\n amvdec_h265 bit_depth_chroma\n");

module_param(video_signal_type, uint, 0664);
MODULE_PARM_DESC(video_signal_type, "\n amvdec_h265 video_signal_type\n");

#ifdef ERROR_HANDLE_DEBUG
module_param(dbg_nal_skip_flag, uint, 0664);
MODULE_PARM_DESC(dbg_nal_skip_flag, "\n amvdec_h265 dbg_nal_skip_flag\n");

module_param(dbg_nal_skip_count, uint, 0664);
MODULE_PARM_DESC(dbg_nal_skip_count, "\n amvdec_h265 dbg_nal_skip_count\n");
#endif

module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\n radr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\n rval\n");

module_param(dbg_cmd, uint, 0664);
MODULE_PARM_DESC(dbg_cmd, "\n dbg_cmd\n");

module_param(dump_nal, uint, 0664);
MODULE_PARM_DESC(dump_nal, "\n dump_nal\n");

module_param(dbg_skip_decode_index, uint, 0664);
MODULE_PARM_DESC(dbg_skip_decode_index, "\n dbg_skip_decode_index\n");

module_param(endian, uint, 0664);
MODULE_PARM_DESC(endian, "\n rval\n");

module_param(step, uint, 0664);
MODULE_PARM_DESC(step, "\n amvdec_h265 step\n");

module_param(decode_pic_begin, uint, 0664);
MODULE_PARM_DESC(decode_pic_begin, "\n amvdec_h265 decode_pic_begin\n");

module_param(slice_parse_begin, uint, 0664);
MODULE_PARM_DESC(slice_parse_begin, "\n amvdec_h265 slice_parse_begin\n");

module_param(nal_skip_policy, uint, 0664);
MODULE_PARM_DESC(nal_skip_policy, "\n amvdec_h265 nal_skip_policy\n");

module_param(i_only_flag, uint, 0664);
MODULE_PARM_DESC(i_only_flag, "\n amvdec_h265 i_only_flag\n");

module_param(fast_output_enable, uint, 0664);
MODULE_PARM_DESC(fast_output_enable, "\n amvdec_h265 fast_output_enable\n");

module_param(error_handle_policy, uint, 0664);
MODULE_PARM_DESC(error_handle_policy, "\n amvdec_h265 error_handle_policy\n");

module_param(error_handle_threshold, uint, 0664);
MODULE_PARM_DESC(error_handle_threshold,
		"\n amvdec_h265 error_handle_threshold\n");

module_param(error_handle_nal_skip_threshold, uint, 0664);
MODULE_PARM_DESC(error_handle_nal_skip_threshold,
		"\n amvdec_h265 error_handle_nal_skip_threshold\n");

module_param(error_handle_system_threshold, uint, 0664);
MODULE_PARM_DESC(error_handle_system_threshold,
		"\n amvdec_h265 error_handle_system_threshold\n");

module_param(error_skip_nal_count, uint, 0664);
MODULE_PARM_DESC(error_skip_nal_count,
				 "\n amvdec_h265 error_skip_nal_count\n");

module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n amvdec_h265 debug\n");

module_param(debug_mask, uint, 0664);
MODULE_PARM_DESC(debug_mask, "\n amvdec_h265 debug mask\n");

module_param(log_mask, uint, 0664);
MODULE_PARM_DESC(log_mask, "\n amvdec_h265 log_mask\n");

module_param(buffer_mode, uint, 0664);
MODULE_PARM_DESC(buffer_mode, "\n buffer_mode\n");

module_param(double_write_mode, uint, 0664);
MODULE_PARM_DESC(double_write_mode, "\n double_write_mode\n");

module_param(buf_alloc_width, uint, 0664);
MODULE_PARM_DESC(buf_alloc_width, "\n buf_alloc_width\n");

module_param(buf_alloc_height, uint, 0664);
MODULE_PARM_DESC(buf_alloc_height, "\n buf_alloc_height\n");

module_param(dynamic_buf_num_margin, uint, 0664);
MODULE_PARM_DESC(dynamic_buf_num_margin, "\n dynamic_buf_num_margin\n");

module_param(default_buf_num, uint, 0444);
MODULE_PARM_DESC(default_buf_num, "\n default_buf_num\n");

module_param(max_buf_num, uint, 0664);
MODULE_PARM_DESC(max_buf_num, "\n max_buf_num\n");

module_param(buf_alloc_size, uint, 0664);
MODULE_PARM_DESC(buf_alloc_size, "\n buf_alloc_size\n");

#ifdef CONSTRAIN_MAX_BUF_NUM
module_param(run_ready_max_vf_only_num, uint, 0664);
MODULE_PARM_DESC(run_ready_max_vf_only_num, "\n run_ready_max_vf_only_num\n");

module_param(run_ready_display_q_num, uint, 0664);
MODULE_PARM_DESC(run_ready_display_q_num, "\n run_ready_display_q_num\n");

module_param(run_ready_max_buf_num, uint, 0664);
MODULE_PARM_DESC(run_ready_max_buf_num, "\n run_ready_max_buf_num\n");
#endif

#if 0
module_param(re_config_pic_flag, uint, 0664);
MODULE_PARM_DESC(re_config_pic_flag, "\n re_config_pic_flag\n");
#endif

module_param(buffer_mode_dbg, uint, 0664);
MODULE_PARM_DESC(buffer_mode_dbg, "\n buffer_mode_dbg\n");

module_param(mem_map_mode, uint, 0664);
MODULE_PARM_DESC(mem_map_mode, "\n mem_map_mode\n");

module_param(enable_mem_saving, uint, 0664);
MODULE_PARM_DESC(enable_mem_saving, "\n enable_mem_saving\n");

module_param(force_w_h, uint, 0664);
MODULE_PARM_DESC(force_w_h, "\n force_w_h\n");

module_param(force_fps, uint, 0664);
MODULE_PARM_DESC(force_fps, "\n force_fps\n");

module_param(max_decoding_time, uint, 0664);
MODULE_PARM_DESC(max_decoding_time, "\n max_decoding_time\n");

module_param(prefix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(prefix_aux_buf_size, "\n prefix_aux_buf_size\n");

module_param(suffix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(suffix_aux_buf_size, "\n suffix_aux_buf_size\n");

module_param(interlace_enable, uint, 0664);
MODULE_PARM_DESC(interlace_enable, "\n interlace_enable\n");
module_param(pts_unstable, uint, 0664);
MODULE_PARM_DESC(pts_unstable, "\n amvdec_h265 pts_unstable\n");
module_param(parser_sei_enable, uint, 0664);
MODULE_PARM_DESC(parser_sei_enable, "\n parser_sei_enable\n");

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
module_param(parser_dolby_vision_enable, uint, 0664);
MODULE_PARM_DESC(parser_dolby_vision_enable,
	"\n parser_dolby_vision_enable\n");

module_param(dolby_meta_with_el, uint, 0664);
MODULE_PARM_DESC(dolby_meta_with_el,
	"\n dolby_meta_with_el\n");

module_param(dolby_el_flush_th, uint, 0664);
MODULE_PARM_DESC(dolby_el_flush_th,
	"\n dolby_el_flush_th\n");
#endif
module_param(mmu_enable, uint, 0664);
MODULE_PARM_DESC(mmu_enable, "\n mmu_enable\n");

module_param(mmu_enable_force, uint, 0664);
MODULE_PARM_DESC(mmu_enable_force, "\n mmu_enable_force\n");

#ifdef MULTI_INSTANCE_SUPPORT
module_param(start_decode_buf_level, int, 0664);
MODULE_PARM_DESC(start_decode_buf_level,
		"\n h265 start_decode_buf_level\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val,
	"\n h265 decode_timeout_val\n");

module_param(data_resend_policy, uint, 0664);
MODULE_PARM_DESC(data_resend_policy,
	"\n h265 data_resend_policy\n");

module_param_array(decode_frame_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(display_frame_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(max_process_time, uint,
	&max_decode_instance_num, 0664);

module_param_array(max_get_frame_interval,
	uint, &max_decode_instance_num, 0664);

module_param_array(run_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(input_empty, uint,
	&max_decode_instance_num, 0664);

module_param_array(not_run_ready, uint,
	&max_decode_instance_num, 0664);

module_param_array(ref_frame_mark_flag, uint,
	&max_decode_instance_num, 0664);

#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
module_param(dv_toggle_prov_name, uint, 0664);
MODULE_PARM_DESC(dv_toggle_prov_name, "\n dv_toggle_prov_name\n");

module_param(dv_debug, uint, 0664);
MODULE_PARM_DESC(dv_debug, "\n dv_debug\n");

module_param(force_bypass_dvenl, uint, 0664);
MODULE_PARM_DESC(force_bypass_dvenl, "\n force_bypass_dvenl\n");
#endif

#ifdef AGAIN_HAS_THRESHOLD
module_param(again_threshold, uint, 0664);
MODULE_PARM_DESC(again_threshold, "\n again_threshold\n");
#endif

module_param(force_disp_pic_index, int, 0664);
MODULE_PARM_DESC(force_disp_pic_index,
	"\n amvdec_h265 force_disp_pic_index\n");

module_param(frmbase_cont_bitlevel, uint, 0664);
MODULE_PARM_DESC(frmbase_cont_bitlevel,	"\n frmbase_cont_bitlevel\n");

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n amvdec_h265 udebug_flag\n");

module_param(udebug_pause_pos, uint, 0664);
MODULE_PARM_DESC(udebug_pause_pos, "\n udebug_pause_pos\n");

module_param(udebug_pause_val, uint, 0664);
MODULE_PARM_DESC(udebug_pause_val, "\n udebug_pause_val\n");

module_param(pre_decode_buf_level, int, 0664);
MODULE_PARM_DESC(pre_decode_buf_level, "\n ammvdec_h264 pre_decode_buf_level\n");

module_param(udebug_pause_decode_idx, uint, 0664);
MODULE_PARM_DESC(udebug_pause_decode_idx, "\n udebug_pause_decode_idx\n");

module_param(disp_vframe_valve_level, uint, 0664);
MODULE_PARM_DESC(disp_vframe_valve_level, "\n disp_vframe_valve_level\n");

module_init(amvdec_h265_driver_init_module);
module_exit(amvdec_h265_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC h265 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <tim.yao@amlogic.com>");
