/*
 * drivers/amlogic/media/enhancement/amdolby_vision/amdolby_vision.c
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include "../amvecm/arch/vpp_regs.h"
#include "../amvecm/arch/vpp_hdr_regs.h"
#include "../amvecm/arch/vpp_dolbyvision_regs.h"
#include "../amvecm/amcsc.h"
#include "../amvecm/reg_helper.h"
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include "amdolby_vision.h"

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "../../common/vfm/vfm.h"

DEFINE_SPINLOCK(dovi_lock);

static unsigned int dolby_vision_probe_ok;
static const struct dolby_vision_func_s *p_funcs_stb;

#define AMDOLBY_VISION_NAME               "amdolby_vision"
#define AMDOLBY_VISION_CLASS_NAME         "amdolby_vision"

#undef V2_4_3

struct amdolby_vision_dev_s {
	dev_t               devt;
	struct cdev         cdev;
	dev_t               devno;
	struct device       *dev;
	struct class        *clsp;
	wait_queue_head_t	dv_queue;
};
static struct amdolby_vision_dev_s amdolby_vision_dev;
struct dv_device_data_s dv_meson_dev;
static unsigned int dolby_vision_request_mode = 0xff;

static unsigned int dolby_vision_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
module_param(dolby_vision_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_mode, "\n dolby_vision_mode\n");

static unsigned int dolby_vision_target_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
module_param(dolby_vision_target_mode, uint, 0444);
MODULE_PARM_DESC(dolby_vision_target_mode, "\n dolby_vision_target_mode\n");

static unsigned int dolby_vision_profile = 0xff;
module_param(dolby_vision_profile, uint, 0664);
MODULE_PARM_DESC(dolby_vision_profile, "\n dolby_vision_profile\n");

static unsigned int dolby_vision_level = 0xff;
module_param(dolby_vision_level, uint, 0664);
MODULE_PARM_DESC(dolby_vision_level, "\n dolby_vision_level\n");

static unsigned int primary_debug;
module_param(primary_debug, uint, 0664);
MODULE_PARM_DESC(primary_debug, "\n primary_debug\n");

/* STB: if sink support DV, always output DV*/
/*		else always output SDR/HDR */
/* #define DOLBY_VISION_FOLLOW_SINK			0 */

/* STB: output DV only if source is DV*/
/*		and sink support DV*/
/*		else always output SDR/HDR */
/* #define DOLBY_VISION_FOLLOW_SOURCE		1 */

/* STB: always follow dolby_vision_mode */
/* #define DOLBY_VISION_FORCE_OUTPUT_MODE	2 */

static unsigned int dolby_vision_policy = DOLBY_VISION_FOLLOW_SOURCE;
module_param(dolby_vision_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_policy, "\n dolby_vision_policy\n");
static unsigned int last_dolby_vision_policy;

/* === HDR10 === */
/* bit0: follow sink 0: bypass hdr10 to vpp 1: process hdr10 by dolby core */
/* bit1: follow source 0: bypass hdr10 to vpp 1: process hdr10 by dolby core */

/* === HDR10+ === */
/* bit2: 0: bypass hdr10+ to vpp, 1: process hdr10+ as hdr10 by dolby core */

/* === HLG -- TV core 1.6 only === */
/* bit3: follow sink 0: bypass hlg to vpp, 1: process hlg by dolby core */
/* bit4: follow source 0: bypass hlg to vpp, 1: process hlg by dolby core */

/* === SDR === */
/* bit5: follow sink 0: bypass SDR to vpp, 1: process SDR by dolby core */
/* bit6: follow source 0: bypass SDR to vpp, 1: process SDR by dolby core */

#define HDR_BY_DV_F_SINK 0x1
#define HDR_BY_DV_F_SRC 0x2
#define HDRP_BY_DV 0x4
#define HLG_BY_DV_F_SINK 0x8
#define HLG_BY_DV_F_SRC 0x10
#define SDR_BY_DV_F_SINK 0x20
#define SDR_BY_DV_F_SRC 0x40

static unsigned int dolby_vision_hdr10_policy = (HDR_BY_DV_F_SRC | HLG_BY_DV_F_SRC | SDR_BY_DV_F_SRC);
module_param(dolby_vision_hdr10_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_hdr10_policy, "\n dolby_vision_hdr10_policy\n");
static unsigned int last_dolby_vision_hdr10_policy;

static bool dolby_vision_enable;
module_param(dolby_vision_enable, bool, 0664);
MODULE_PARM_DESC(dolby_vision_enable, "\n dolby_vision_enable\n");

static bool dolby_vision_efuse_bypass;
module_param(dolby_vision_efuse_bypass, bool, 0664);
MODULE_PARM_DESC(dolby_vision_efuse_bypass, "\n dolby_vision_efuse_bypass\n");
static bool efuse_mode;

/* core1: video priority 	-- linked to 1 	    has a lut	DOLBY 4K (AND 1080P EL added) */	
/* core2: graphic priority 	-- linked to 2A?    has a lut	OSD 1080P */

/* core3: no lut	-- looks to be dealing with metadata */

/* vpp unmuted when dv mute -- either dv mode of the amlogic video post processor mode ?*/

/* Core 1 (DV Core1A)  	-> EL 	1080p HEVC - If on then switch on Composer */				
/* Core 2 (DV Core1B) 	-> EL 	2160p HEVC */
/* Core 3				-> RPU 	*/

/* int composer_enable = bl_enable && el_enable && (dolby_vision_mask & 1) */
/* bool bypass_core1 = (!(dolby_vision_mask & 1)) */
/* if ((bl_enable && el_enable && (dolby_vision_mask & 1)))  then vd2 to core1 else vd2 to vpp */
/* if (dolby_vision_mask & 2) core2 enable --- on by default  111 * 010 > 010 */
/* if (dolby_vision_mask & 4) core3 enable --- on by default  111 * 100 > 100 */

/* things to test -> 1 (001) - No Image */
/* things to test -> 2 (010) - BL - Purple and Green */
/* things to test -> 3 (011) - BL EL - Brighter */
/* things to test -> 4 (100) - BL RPU - Purple and Green */
/* things to test -> 5 (101) - No Image */
/* things to test -> 6 (110) - BL RPU - Purple and Green  */
/* things to test -> 7 (111) - BL EL RPU */

static uint dolby_vision_mask = 7;
module_param(dolby_vision_mask, uint, 0664);
MODULE_PARM_DESC(dolby_vision_mask, "\n dolby_vision_mask\n");

#define BYPASS_PROCESS 0
#define SDR_PROCESS 1
#define HDR_PROCESS 2
#define DV_PROCESS 3

static uint dolby_vision_status;
module_param(dolby_vision_status, uint, 0664);
MODULE_PARM_DESC(dolby_vision_status, "\n dolby_vision_status\n");

/* delay before first frame toggle when core off->on */
static uint dolby_vision_wait_delay = 16;
module_param(dolby_vision_wait_delay, uint, 0664);
MODULE_PARM_DESC(dolby_vision_wait_delay, "\n dolby_vision_wait_delay\n");
static int dolby_vision_wait_count;

/* reset 1st fake frame (bit 0)*/
/*   and other fake frames (bit 1)*/
/*   and other toggle frames (bit 2) */
static uint dolby_vision_reset = (1 << 1) | (1 << 0);
module_param(dolby_vision_reset, uint, 0664);
MODULE_PARM_DESC(dolby_vision_reset, "\n dolby_vision_reset\n");

/* force run mode */
static uint dolby_vision_run_mode = 0xff; /* not force */
module_param(dolby_vision_run_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_run_mode, "\n dolby_vision_run_mode\n");

/* number of fake frame (run mode = 1) */
#define RUN_MODE_DELAY 2

static uint dolby_vision_run_mode_delay = RUN_MODE_DELAY;
module_param(dolby_vision_run_mode_delay, uint, 0664);
MODULE_PARM_DESC(dolby_vision_run_mode_delay, "\n dolby_vision_run_mode_delay\n");

/* reset control -- end << 8 | start */
static uint dolby_vision_reset_delay = (RUN_MODE_DELAY << 8) | RUN_MODE_DELAY;
module_param(dolby_vision_reset_delay, uint, 0664);
MODULE_PARM_DESC(dolby_vision_reset_delay, "\n dolby_vision_reset_delay\n");

static unsigned int dv_ll_output_mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
module_param(dv_ll_output_mode, uint, 0664);
MODULE_PARM_DESC(dv_ll_output_mode, "\n dv_ll_output_mode\n");

#define DOLBY_VISION_LL_DISABLE		0
#define DOLBY_VISION_LL_YUV422		1
#define DOLBY_VISION_LL_RGB444		2
static u32 dolby_vision_ll_policy = DOLBY_VISION_LL_DISABLE;
module_param(dolby_vision_ll_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_ll_policy, "\n dolby_vision_ll_policy\n");
static u32 last_dolby_vision_ll_policy = DOLBY_VISION_LL_DISABLE;

static unsigned int dolby_vision_src_format;
module_param(dolby_vision_src_format, uint, 0664);
MODULE_PARM_DESC(dolby_vision_src_format, "\n dolby_vision_src_format\n");

static unsigned int force_mel;
module_param(force_mel, uint, 0664);
MODULE_PARM_DESC(force_mel, "\n force_mel\n");

/*bit0: 0-> efuse, 1-> no efuse; */
/*bit1: 1-> ko loaded */
/*bit2: 1-> value updated */
static int support_info;
int get_dv_support_info(void)
{
	return support_info;
}
EXPORT_SYMBOL(get_dv_support_info);

char cur_crc[32] = "invalid";

static uint dolby_vision_on_count;
static bool dolby_vision_el_disable;

#define FLAG_FORCE_CVM				0x01		/* ?  - on by default */
#define FLAG_BYPASS_CVM				0x02		/* ? */		
#define FLAG_BYPASS_VPP				0x04		/* Bypass Video Post Processor - on by default */
#define FLAG_USE_SINK_MIN_MAX		0x08		/* Follow the lum of the HDMI for DV - content for HDR10 - probably should be on all the time */
#define FLAG_SINGLE_STEP			0x20		/* ? */
#define FLAG_CERTIFICAION			0x40		/* (IDK) Industrial Tool Kit Cerfitication - what are they cerfifying with this */
#define FLAG_CHANGE_SEQ_HEAD		0x80			/* ? */
#define FLAG_DISABLE_COMPOSER		0x100			/* For FEL disable the EL layer - just output BL ? */
#define FLAG_BYPASS_CSC				0x200		/* Bypass the Colour Space Converter in the Vidoe Processing Unit ? */
#define FLAG_CHECK_ES_PTS			0x400		/* ? */
#define FLAG_DISABE_CORE_SETTING	0x800			/* ? */
#define FLAG_DISABLE_DMA_UPDATE		0x1000			/* ? */
#define FLAG_DISABLE_DOVI_OUT		0x2000			/* ? */
#define FLAG_FORCE_DOVI_LL			0x4000		/* Force into LLDV */
#define FLAG_FORCE_RGB_OUTPUT		0x8000			/* Force RGB Output LLDV Specific ? */
#define FLAG_DOVI2HDR10_NOMAPPING	0x100000		/* ? */
#define FLAG_PRIORITY_GRAPHIC		0x200000		/* Force Graphic Priority */
#define FLAG_DISABLE_LOAD_VSVDB		0x400000		/* ? */
#define FLAG_DISABLE_CRC			0x800000	/* ? */
#define FLAG_MUTE					0x4000000	/* ? */
#define FLAG_FORCE_HDMI_PKT			0x8000000	/* ? */
#define FLAG_TOGGLE_FRAME			0x80000000	/* Swithcing between modes ? */

static unsigned int dolby_vision_flags = FLAG_BYPASS_VPP | FLAG_FORCE_CVM | FLAG_USE_SINK_MIN_MAX;
module_param(dolby_vision_flags, uint, 0664);
MODULE_PARM_DESC(dolby_vision_flags, "\n dolby_vision_flags\n");

/*
1	00001 	Core 1 (EL)  Reset
2	00010	Core 2 (BL)  Reset
4	00100	Core 3 (RPU) Reset
8	01000	Core 1 (EL)  Lut
16	10000	Core 2 (BL)  Lut	
31	11111	All Reset
*/	
	
/*bit0: reset core1 reg; bit1: reset core2 reg; bit2: reset core3 reg */
/*bit3: reset core1 lut; bit4: reset core2 lut */
static unsigned int force_update_reg;
module_param(force_update_reg, uint, 0664);
MODULE_PARM_DESC(force_update_reg, "\n force_update_reg\n");

#define DV_NAME_LEN_MAX 32

static unsigned int htotal_add = 0x140;
static unsigned int vtotal_add = 0x40;
static unsigned int vsize_add;
static unsigned int vwidth = 0x8;
static unsigned int hwidth = 0x8;
static unsigned int vpotch = 0x10;
static unsigned int hpotch = 0x8;
static unsigned int g_htotal_add = 0x40;
static unsigned int g_vtotal_add = 0x80;
static unsigned int g_vsize_add;
static unsigned int g_vwidth = 0x18;
static unsigned int g_hwidth = 0x10;
static unsigned int g_vpotch = 0x10;
static unsigned int g_hpotch = 0x10;

/*dma size:1877x2x64 bit = 30032 byte*/
static unsigned int dma_size = 30032;
static dma_addr_t dma_paddr;
static void *dma_vaddr;

#define CRC_BUFF_SIZE (256 * 1024)
static char *crc_output_buf;
static u32 crc_output_buff_off;
static u32 crc_count;
static u32 crc_bypass_count;
static u32 setting_update_count;
static s32 crc_read_delay;
static u32 core1_disp_hsize;
static u32 core1_disp_vsize;
static u32 vsync_count;
#define FLAG_VSYNC_CNT 10
#define MAX_TRANSITION_DELAY 5
#define MIN_TRANSITION_DELAY 2
#define MAX_CORE3_MD_SIZE 128 /*512byte*/

static bool is_osd_off;
static bool force_reset_core2;
static int core1_switch;
static int core3_switch;
static bool force_set_lut;
static bool ambient_update;
static struct ambient_cfg_s ambient_config_new;

/*core reg must be set at first time. bit0 is for core2, bit1 is for core3*/
static char *ko_info;

module_param(vtotal_add, uint, 0664);
MODULE_PARM_DESC(vtotal_add, "\n vtotal_add\n");

module_param(vpotch, uint, 0664);
MODULE_PARM_DESC(vpotch, "\n vpotch\n");

/* for core2 timing setup tuning */
/* g_vtotal_add << 24 | g_vsize_add << 16 */
/* | g_vwidth << 8 | g_vpotch */
static unsigned int g_vtiming;
module_param(g_vtiming, uint, 0664);
MODULE_PARM_DESC(g_vtiming, "\n vpotch\n");

static unsigned int dolby_vision_target_min = 50; /* min is expreseed in ten-thousandths of a nit, i.e. * 0.0001 to get nits - default: 0.005 nits*/
static unsigned int dolby_vision_target_max[3][3] = {
		{ 4000, 1000, 100 }, /* DOVI => DOVI/HDR/SDR */ /* all expresssed in nits */
		{ 1000, 1000, 100 }, /* HDR =>  DOVI/HDR/SDR */
		{ 600, 1000, 100 },  /* SDR =>  DOVI/HDR/SDR */
};

static unsigned int dolby_vision_default_max[3][3] = {
		{ 4000, 4000, 100 }, /* DOVI => DOVI/HDR/SDR */
		{ 1000, 1000, 100 }, /* HDR =>  DOVI/HDR/SDR */
		{ 600, 1000, 100 },  /* SDR =>  DOVI/HDR/SDR */
};

static unsigned int dolby_vision_graphic_min = 50; /* min is expreseed in ten-thousandths of a nit, i.e. * 0.0001 to get nits - default: 0.005 nits*/
static unsigned int dolby_vision_graphic_max; /* 100 - Why? */
static unsigned int old_dolby_vision_graphic_max;

module_param(dolby_vision_graphic_min, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphic_min, "\n dolby_vision_graphic_min\n");

module_param(dolby_vision_graphic_max, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphic_max, "\n dolby_vision_graphic_max\n");

static unsigned int dv_HDR10_graphics_max = 300;
static unsigned int dv_graphic_blend_test;
module_param(dv_graphic_blend_test, uint, 0664);
MODULE_PARM_DESC(dv_graphic_blend_test, "\n dv_graphic_blend_test\n");

static unsigned int dv_target_graphics_max[3][3] = {
		{ 300, 316, 380 }, /* DOVI => DOVI/HDR/SDR */
		{ 300, 316, 100 }, /* HDR =>  DOVI/HDR/SDR */
		{ 300, 316, 100 }, /* SDR =>  DOVI/HDR/SDR */
};
static unsigned int dv_target_graphics_LL_max[3][3] = {
		{ 300, 316, 100 }, /* DOVI => DOVI/HDR/SDR */
		{ 210, 316, 100 }, /* HDR =>  DOVI/HDR/SDR */
		{ 300, 316, 100 }, /* SDR =>  DOVI/HDR/SDR */
};

static unsigned int dolby_vision_force_hdmi_lum = 0;
module_param(dolby_vision_force_hdmi_lum, uint, 0664);
MODULE_PARM_DESC(dolby_vision_force_hdmi_lum, "\n dolby_vision_force_hdmi_lum\n");

static unsigned int dolby_vision_hdmi_lum_min = 50;
static unsigned int dolby_vision_hdmi_lum_max = 4000; 

module_param(dolby_vision_hdmi_lum_min, uint, 0664);
MODULE_PARM_DESC(dolby_vision_hdmi_lum_min, "\n dolby_vision_hdmi_lum_min\n");

module_param(dolby_vision_hdmi_lum_max, uint, 0664);
MODULE_PARM_DESC(dolby_vision_hdmi_lum_max, "\n dolby_vision_hdmi_lum_max\n");

/*these two parameters form OSD*/
static unsigned int osd_graphic_width = 3840;
static unsigned int osd_graphic_height = 2160;

module_param(osd_graphic_width, uint, 0664);
MODULE_PARM_DESC(osd_graphic_width, "\n osd_graphic_width\n");

module_param(osd_graphic_height, uint, 0664);
MODULE_PARM_DESC(osd_graphic_height, "\n osd_graphic_height\n");

static unsigned int new_osd_graphic_width = 3840;
static unsigned int new_osd_graphic_height = 2160;

static unsigned int dv_cert_graphic_width = 1920;
static unsigned int dv_cert_graphic_height = 1080;

module_param(dv_cert_graphic_width, uint, 0664);
MODULE_PARM_DESC(dv_cert_graphic_width, "\n dv_cert_graphic_width\n");

module_param(dv_cert_graphic_height, uint, 0664);
MODULE_PARM_DESC(dv_cert_graphic_height, "\n dv_cert_graphic_height\n");

static unsigned int enable_tunnel;

/* 0: video priority 1: graphic priority */
static unsigned int dolby_vision_graphics_priority;
module_param(dolby_vision_graphics_priority, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphics_priority, "\n dolby_vision_graphics_priority\n");

static unsigned int atsc_sei = 1;
module_param(atsc_sei, uint, 0664);
MODULE_PARM_DESC(atsc_sei, "\n atsc_sei\n");

static struct dv_atsc p_atsc_md;

static int content_fps = 24;
static int gd_rf_adjust;
static u32 last_vf_valid_crc;

#define MAX_DV_PICTUREMODES 40
struct pq_config *bin_to_cfg;

static struct dv_cfg_info_s cfg_info[MAX_DV_PICTUREMODES];

static s16 pq_center[MAX_DV_PICTUREMODES][4];
struct dv_pq_range_s pq_range[4];
u8 current_vsvdb[7];

int num_picture_mode;
static int default_pic_mode = 1;	/* bright(standard) mode as default */
static int cur_pic_mode;			/* current picture mode id */
static bool load_bin_config;
static bool need_update_cfg;

static const s16 EXTER_MIN_PQ = -256;
static const s16 EXTER_MAX_PQ = 256;
static const s16 INTER_MIN_PQ = -4096;
static const s16 INTER_MAX_PQ = 4095;

const char *pq_item_str[] = {"brightness",
							 "contrast",
							 "colorshift",
							 "saturation"};

const char *eotf_str[] = {"bt1886", "pq", "power"};

static u32 cur_debug_tprimary[3][2];
static int debug_tprimary;

/*0: set exter pq [-256,256]. 1:set inter pq [-4096,4095]*/
static int set_inter_pq;
static DEFINE_SPINLOCK(cfg_lock);

unsigned int debug_dolby;
module_param(debug_dolby, uint, 0664);
MODULE_PARM_DESC(debug_dolby, "\n debug_dolby\n");

static unsigned int debug_dolby_frame = 0xffff;
module_param(debug_dolby_frame, uint, 0664);
MODULE_PARM_DESC(debug_dolby_frame, "\n debug_dolby_frame\n");

#define pr_dolby_dbg(fmt, args...)\
	do {\
		if (debug_dolby)\
			pr_info("DOLBY: " fmt, ## args);\
	} while (0)

#define pr_dolby_error(fmt, args...)\
	pr_info("DOLBY ERROR: " fmt, ## args)

#define dump_enable \
	((debug_dolby_frame >= 0xffff) || \
	(debug_dolby_frame + 1 == frame_count))

#define single_step_enable \
	(((debug_dolby_frame >= 0xffff) || \
	((debug_dolby_frame + 1) == frame_count)) && \
	(debug_dolby & 0x80))

#define DV_CORE1_RECONFIG_CNT 2
#define DV_CORE2_RECONFIG_CNT 120

static bool dolby_vision_on;
static bool dolby_vision_core1_on;
static u32 dolby_vision_core1_on_cnt;
static bool dolby_vision_wait_on;
static u32 dolby_vision_core2_on_cnt;

module_param(dolby_vision_wait_on, bool, 0664);
MODULE_PARM_DESC(dolby_vision_wait_on, "\n dolby_vision_wait_on\n");
static bool dolby_vision_on_in_uboot;
static bool dolby_vision_wait_init;
static unsigned int frame_count;
static struct hdr10_parameter hdr10_param;
static struct hdr10_parameter last_hdr10_param;
static struct master_display_info_s hdr10_data;

static char *md_buf[2];
static char *comp_buf[2];
static char *drop_md_buf[2];
static char *drop_comp_buf[2];

#define VSEM_IF_BUF_SIZE 4096
static char *vsem_if_buf;/* buffer for vsem or vsif */
static char *vsem_md_buf;
#define VSEM_PKT_SIZE 31

static int current_id = 1;

static int backup_comp_size;
static int backup_md_size;

static struct dovi_setting_s dovi_setting;
static struct dovi_setting_s new_dovi_setting;

void *pq_config_fake;
static bool pq_config_set_flag;

static bool dovi_setting_video_flag;
static struct platform_device *dovi_pdev;
static bool vsvdb_config_set_flag;
static bool vfm_path_on;

#define CP_FLAG_CHANGE_TC       0x000010
#define CP_FLAG_CHANGE_TC2      0x000020
#define CP_FLAG_CONST_TC2       0x200000
#define CP_FLAG_CHANGE_ALL    0xffffffff

/* update all core */
static u32 stb_core_setting_update_flag = CP_FLAG_CHANGE_ALL;
static bool stb_core2_const_flag;

/* 256+(256*4+256*2)*4/8 64bit */
#define STB_DMA_TBL_SIZE (256+(256*4+256*2)*4/8)

static bool el_mode;
static bool mel_mode;
static bool osd_update;
static bool enable_fel;

static int force_disable_dv_backlight;
static bool dv_control_backlight_status;
static bool bypass_all_vpp_pq;
static int use_target_lum_from_cfg;

static bool is_mel = false;
module_param(is_mel, bool, 0444);
MODULE_PARM_DESC(is_mel, "\n is_mel\n");

/*0: not debug mode; 1:force bypass vpp pq; 2:force enable vpp pq*/
/*3: force do nothing*/
static u32 debug_bypass_vpp_pq;

static bool module_installed;

#define MAX_PARAM   8

static int is_graphics_output_off(void)
{
	u32 osd1 = READ_VPP_REG(OSD1_BLEND_SRC_CTRL);
	u32 osd2 = READ_VPP_REG(OSD2_BLEND_SRC_CTRL);

	pr_dolby_dbg("is graphics output off : osd1 [%x] osd2 [%x]\n", osd1, osd2);

	return !(osd1 & (0xf << 8)) && !(osd2 & (0xf << 8));
}

static u32 CORE1_BASE;
static u32 CORE2A_BASE;
static u32 CORE3_BASE;

static void dolby_vision_addr(void)
{
	CORE1_BASE = 0x3300;
	CORE2A_BASE = 0x3400;
	CORE3_BASE = 0x3600;
}

static u32 addr_map(u32 adr)
{
	if (adr & CORE1_OFFSET)
		adr = (adr & 0xffff) + CORE1_BASE;

	else if (adr & CORE2A_OFFSET)
		adr = (adr & 0xffff) + CORE2A_BASE;

	else if (adr & CORE3_OFFSET)
		adr = (adr & 0xffff) + CORE3_BASE;

	return adr;
}

static u32 VSYNC_RD_DV_REG(u32 adr)
{
	adr = addr_map(adr);
	return VSYNC_RD_MPEG_REG(adr);
}

static int VSYNC_WR_DV_REG(u32 adr, u32 val)
{
	adr = addr_map(adr);
	VSYNC_WR_MPEG_REG(adr, val);
	return 0;
}

static int VSYNC_WR_DV_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	adr = addr_map(adr);
	VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
	return 0;
}

static u32 READ_VPP_DV_REG(u32 adr)
{
	adr = addr_map(adr);
	return READ_VPP_REG(adr);
}

static int WRITE_VPP_DV_REG(u32 adr, const u32 val)
{
	adr = addr_map(adr);
	WRITE_VPP_REG(adr, val);
	return 0;
}

static unsigned int amdolby_vision_poll(struct file *file, poll_table *wait)
{
	struct amdolby_vision_dev_s *devp = file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &devp->dv_queue, wait);
	mask = (POLLIN | POLLRDNORM);

	return mask;
}

void dolby_vision_update_pq_config(char *pq_config_buf)
{
	memcpy((struct pq_config *)pq_config_fake, pq_config_buf, sizeof(struct pq_config));

	pr_info("update_pq_config[%zu] %x %x %x %x\n",
			sizeof(struct pq_config),
			pq_config_buf[1],
			pq_config_buf[2],
			pq_config_buf[3],
			pq_config_buf[4]);

	pq_config_set_flag = true;
}

void dolby_vision_update_vsvdb_config(char *vsvdb_buf, u32 tbl_size)
{
	if (tbl_size > sizeof(new_dovi_setting.vsvdb_tbl)) {
		pr_info("update_vsvdb_config tbl size overflow %d\n", tbl_size);
		return;
	}
	memset(&new_dovi_setting.vsvdb_tbl[0], 0, sizeof(new_dovi_setting.vsvdb_tbl));
	memcpy(&new_dovi_setting.vsvdb_tbl[0], vsvdb_buf, tbl_size);

	new_dovi_setting.vsvdb_len = tbl_size;
	new_dovi_setting.vsvdb_changed = 1;

	dolby_vision_set_toggle_flag(1);

	if (tbl_size >= 8)
		pr_info("update_vsvdb_config[%d] %x %x %x %x %x %x %x %x\n",
				tbl_size,
				vsvdb_buf[0],
				vsvdb_buf[1],
				vsvdb_buf[2],
				vsvdb_buf[3],
				vsvdb_buf[4],
				vsvdb_buf[5],
				vsvdb_buf[6],
				vsvdb_buf[7]);

	vsvdb_config_set_flag = true;
}

static bool skip_cvm_tbl[2][2][4][4] = {
		{ /* core1: video */
				{ /* video priority */
						{1, 1, 0, 0}, /* dv in */
						{1, 1, 0, 0}, /* hdr in */
						{0, 0, 1, 0}, /* sdr in */
						{0, 0, 0, 0}  /* only hdmi in */
				},
				{ /* graphic priority */
						{0, 0, 0, 0}, /* dv in */
						{0, 0, 0, 0}, /* hdr in */
						{0, 0, 1, 0}, /* sdr in */
						{0, 0, 0, 0}  /* only hdmi in */
				}
		},
		{ /* core2: graphic */
				{ /* video priority */
						{0, 0, 0, 0}, /* dv in */
						{0, 0, 0, 0}, /* hdr in */
						{0, 0, 1, 0}, /* sdr in */
						{0, 0, 0, 0}  /* only hdmi in */
				},
				{ /* graphic priority */
						{0, 0, 0, 0}, /* dv in */
						{0, 0, 0, 0}, /* hdr in */
						{0, 0, 1, 0}, /* sdr in */
						{0, 0, 0, 0}  /* only hdmi in */
				}
		}
};

static bool need_skip_cvm(unsigned int is_graphic)
{
	if (dolby_vision_flags & FLAG_CERTIFICAION)
		return false;

	if (dolby_vision_flags & FLAG_FORCE_CVM)
		return false;

	return skip_cvm_tbl[is_graphic]
	[dolby_vision_graphics_priority]
	[new_dovi_setting.src_format == FORMAT_INVALID ? FORMAT_SDR : new_dovi_setting.src_format]
	[new_dovi_setting.dovi_ll_enable ? FORMAT_DOVI_LL : new_dovi_setting.dst_format];
}

static void adjust_vpotch(void)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	int sync_duration_num = 60;

	if (vinfo) {
		
		if (vinfo->sync_duration_den)
			sync_duration_num = vinfo->sync_duration_num / vinfo->sync_duration_den;
		
		if (debug_dolby & 2)
			pr_dolby_dbg("vinfo %d %d %d %d %d %d\n",
						 vinfo->width,
						 vinfo->height,
						 vinfo->field_height,
						 vinfo->sync_duration_num,
						 vinfo->sync_duration_den,
						 sync_duration_num);
		
		if (vinfo->width < 1280 && vinfo->height < 720 && vinfo->field_height < 720)
			g_vpotch = 0x60;
		else if (vinfo->width == 1280 && vinfo->height == 720)
			g_vpotch = 0x38;
		else if (vinfo->width == 1280 && vinfo->height == 720 && vinfo->field_height < 720)
			g_vpotch = 0x60;
		else if (vinfo->width == 1920 && vinfo->height == 1080 && sync_duration_num < 30)
			g_vpotch = 0x60;
		else if (vinfo->width == 1920 && vinfo->height == 1080 && vinfo->field_height < 1080)
			g_vpotch = 0x60;
		else
			g_vpotch = 0x20;
			
		if (vinfo->width > 1920)
			htotal_add = 0xc0;
		else
			htotal_add = 0x140;
			
	} else {
		g_vpotch = 0x20;
	}
}

int dolby_vision_update_setting(void)
{
	u32 size = 0;
	int i;
	u64 *p;

	if (!p_funcs_stb)
		return -1;

	if (dolby_vision_flags & FLAG_DISABLE_DMA_UPDATE) {
		setting_update_count++;
		return -1;
	}

	if (!dma_vaddr)
		return -1;

	if (efuse_mode == 1) {
		setting_update_count++;
		return -1;
	}

	if (size && (debug_dolby & 0x800)) {
		p = (u64 *)dma_vaddr;
		pr_info("dma size = %d\n", STB_DMA_TBL_SIZE);
			
		for (i = 0; i < size / 8; i += 2)
			pr_info("%016llx, %016llx\n", p[i], p[i+1]);
	}

	setting_update_count = frame_count;
	return -1;
}
EXPORT_SYMBOL(dolby_vision_update_setting);

static int dolby_core1_set
		(u32 dm_count,
		 u32 comp_count,
		 u32 lut_count,
		 u32 *p_core1_dm_regs,
		 u32 *p_core1_comp_regs,
		 u32 *p_core1_lut,
		 int hsize,
		 int vsize,
		 int bl_enable,
		 int el_enable,
		 int el_41_mode,
		 int scramble_en,
		 bool dovi_src,
		 int lut_endian,
		 bool reset)
{
	u32 count;
	u32 bypass_flag = 0;
	int composer_enable = bl_enable && el_enable && (dolby_vision_mask & 1);
	int i;
	bool set_lut = false;
	u32 *last_dm = (u32 *)&dovi_setting.dm_reg1;
	u32 *last_comp = (u32 *)&dovi_setting.comp_reg;
	bool bypass_core1 = (!hsize || !vsize || !(dolby_vision_mask & 1));

	/* G12A: make sure the BL is enable for the very 1st frame*/
	/* Register: dolby_path_ctrl[0] = 0 to enable BL*/
	/*           dolby_path_ctrl[1] = 0 to enable EL*/
	/*           dolby_path_ctrl[2] = 0 to enable OSD*/

	if (frame_count == 1 && dolby_vision_core1_on == 0) {

		pr_dolby_dbg("((%s %d, register DOLBY_PATH_CTRL: %x))\n",  __func__, __LINE__, VSYNC_RD_DV_REG(DOLBY_PATH_CTRL));

		if ((VSYNC_RD_DV_REG(DOLBY_PATH_CTRL) & 0x1) != 0) {

			pr_dolby_dbg("BL is disable for 1st frame.Re-enable BL\n");
			VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 0, 0, 1);
			pr_dolby_dbg("((%s %d, enable_bl, DOLBY_PATH_CTRL: %x))\n", __func__, __LINE__, VSYNC_RD_DV_REG(DOLBY_PATH_CTRL));
		}

		if (el_enable) {
			if ((VSYNC_RD_DV_REG(DOLBY_PATH_CTRL) & 0x10) != 0) {

				pr_dolby_dbg("((%s %d enable el))\n", __func__, __LINE__);
				VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 0, 1, 1);
				pr_dolby_dbg("((%s %d, enable_el, DOLBY_PATH_CTRL: %x))\n", __func__, __LINE__, VSYNC_RD_DV_REG(DOLBY_PATH_CTRL));
			}
		}
	}

	if (dolby_vision_on && (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;

	if (force_update_reg & 8)
		set_lut = true;

	if (force_update_reg & 1)
		reset = true;

	if (dolby_vision_on_count == dolby_vision_run_mode_delay)
		reset = true;

	if ((!dolby_vision_on || reset) && bl_enable) {
		/* Reset Dolby Core 1A */
		VSYNC_WR_DV_REG(VIU_SW_RESET, 1 << 2);
		VSYNC_WR_DV_REG(VIU_SW_RESET, 0);
		reset = true;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (dolby_vision_core1_on &&
		dolby_vision_core1_on_cnt < DV_CORE1_RECONFIG_CNT) {
		reset = true;
		dolby_vision_core1_on_cnt++;
	}

	if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC)
		set_lut = true;

	if ((bl_enable && el_enable && (dolby_vision_mask & 1))) {
		/* vd2 to core1 */
		VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 0, 1, 1);
	} else {
		/* vd2 to vpp */
		VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 1, 1, 1);
	}

	if (bl_enable) {

		if (get_vpu_mem_pd_vmod(VPU_DOLBY1A) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_DOLBY1A) == VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_DOLBY1A);

		if (get_vpu_mem_pd_vmod(VPU_PRIME_DOLBY_RAM) == VPU_MEM_POWER_DOWN ||
			get_dv_mem_power_flag(VPU_PRIME_DOLBY_RAM) == VPU_MEM_POWER_DOWN)
			dv_mem_power_on(VPU_PRIME_DOLBY_RAM);

	}
	VSYNC_WR_DV_REG(DOLBY_CORE1_CLKGATE_CTRL, 0);

	VSYNC_WR_DV_REG(DOLBY_CORE1_SWAP_CTRL1, ((hsize + 0x80) << 16) | (vsize + 0x40));
	VSYNC_WR_DV_REG(DOLBY_CORE1_SWAP_CTRL3, (hwidth << 16) | vwidth);
	VSYNC_WR_DV_REG(DOLBY_CORE1_SWAP_CTRL4, (hpotch << 16) | vpotch);
	VSYNC_WR_DV_REG(DOLBY_CORE1_SWAP_CTRL2, (hsize << 16) | vsize);
	VSYNC_WR_DV_REG(DOLBY_CORE1_SWAP_CTRL5, 0xa);

	VSYNC_WR_DV_REG(DOLBY_CORE1_DMA_CTRL, 0x0);
	VSYNC_WR_DV_REG(DOLBY_CORE1_REG_START + 4, 4);
	VSYNC_WR_DV_REG(DOLBY_CORE1_REG_START + 2, 1);

	/* bypass composer to get 12bit when SDR and HDR source */
#ifdef OLD_VERSION
	if (!dovi_src)
		bypass_flag |= 1 << 0;
#endif
	if (dolby_vision_flags & FLAG_BYPASS_CSC)
		bypass_flag |= 1 << 1;
	if (dolby_vision_flags & FLAG_BYPASS_CVM)
		bypass_flag |= 1 << 2;
	if (need_skip_cvm(0))
		bypass_flag |= 1 << 2;
	if (el_41_mode)
		bypass_flag |= 1 << 3;

	VSYNC_WR_DV_REG(DOLBY_CORE1_REG_START + 1, 0x70 | bypass_flag); /* bypass CVM and/or CSC */
	VSYNC_WR_DV_REG(DOLBY_CORE1_REG_START + 1, 0x70 | bypass_flag); /* for delay */

	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;

	for (i = 0; i < count; i++)
		if (reset || (p_core1_dm_regs[i] != last_dm[i])) {
			VSYNC_WR_DV_REG(DOLBY_CORE1_REG_START + 6 + i, p_core1_dm_regs[i]);
		}

	if (comp_count == 0)
		count = 173;
	else
		count = comp_count;

	for (i = 0; i < count; i++)
		if (reset || p_core1_comp_regs[i] != last_comp[i]) {
			VSYNC_WR_DV_REG(DOLBY_CORE1_REG_START + 6 + 44 + i, p_core1_comp_regs[i]);
		}

	/* metadata program done */
	VSYNC_WR_DV_REG(DOLBY_CORE1_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;

	if (count && (set_lut || reset)) {

		VSYNC_WR_DV_REG(DOLBY_CORE1_DMA_CTRL, 0x1401);

		if (lut_endian)
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_DV_REG(DOLBY_CORE1_DMA_PORT, p_core1_lut[i+3]);
				VSYNC_WR_DV_REG(DOLBY_CORE1_DMA_PORT, p_core1_lut[i+2]);
				VSYNC_WR_DV_REG(DOLBY_CORE1_DMA_PORT, p_core1_lut[i+1]);
				VSYNC_WR_DV_REG(DOLBY_CORE1_DMA_PORT, p_core1_lut[i]);
			}
		else {
			for (i = 0; i < count; i++)
				VSYNC_WR_DV_REG(DOLBY_CORE1_DMA_PORT, p_core1_lut[i]);
		}

	}

	if (dolby_vision_on_count < dolby_vision_run_mode_delay) {

		VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0, (0x200 << 10) | 0x200);
		VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1, (0x200 << 10) | 0x200);
		VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 1, 0, 1);

	} else {
		if (dolby_vision_on_count > dolby_vision_run_mode_delay) {

			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0, (0x3ff << 20) | (0x3ff << 10) | 0x3ff);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1, 0);

		}
		if (dolby_vision_core1_on && !bypass_core1) {

			VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 0, 0, 1);

		} else if (dolby_vision_core1_on && bypass_core1) {

			VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 1, 0, 1);
		}
	}

	VSYNC_WR_DV_REG(DOLBY_CORE1_SWAP_CTRL0,
					(el_41_mode ? (0x3 << 4) : (0x0 << 4)) |
					bl_enable | composer_enable << 1 | el_41_mode << 2);

	return 0;
}

static int dolby_core2_set
		(u32 dm_count,
		 u32 lut_count,
		 u32 *p_core2_dm_regs,
		 u32 *p_core2_lut,
		 int hsize,
		 int vsize,
		 int dolby_enable,
		 int lut_endian)
{
	u32 count;
	int i;
	bool set_lut = false;
	bool reset = false;
	u32 *last_dm = (u32 *)&dovi_setting.dm_reg2;
	u32 bypass_flag = 0;

	if (dolby_vision_on && (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (!dolby_vision_on || force_reset_core2) {
		force_reset_core2 = false;
		reset = true;
	}

	if (dolby_vision_on && dolby_vision_core2_on_cnt < DV_CORE2_RECONFIG_CNT) {
		reset = true;
		dolby_vision_core2_on_cnt++;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		reset = true;

	if (dolby_vision_on_count == 0)
		reset = true;

	if (force_update_reg & 0x10)
		set_lut = true;

	if (force_update_reg & 2)
		reset = true;

	if (get_vpu_mem_pd_vmod(VPU_DOLBY2) == VPU_MEM_POWER_DOWN || get_dv_mem_power_flag(VPU_DOLBY2) == VPU_MEM_POWER_DOWN)
		dv_mem_power_on(VPU_DOLBY2);

	VSYNC_WR_DV_REG(DOLBY_CORE2A_CLKGATE_CTRL, 0);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL0, 0);

	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL1,
					((hsize + g_htotal_add) << 16) | (vsize
													  + ((g_vtiming & 0xff000000) ?
														 ((g_vtiming >> 24) & 0xff) : g_vtotal_add)
													  + ((g_vtiming & 0xff0000) ?
														 ((g_vtiming >> 16) & 0xff) : g_vsize_add)));

	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL2,
					(hsize << 16) | (vsize
									 + ((g_vtiming & 0xff0000) ?
										((g_vtiming >> 16) & 0xff) : g_vsize_add)));

	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL3,
					(g_hwidth << 16) | ((g_vtiming & 0xff00) ?
										((g_vtiming >> 8) & 0xff) : g_vwidth));

	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL4,
					(g_hpotch << 16) | ((g_vtiming & 0xff) ?
										(g_vtiming & 0xff) : g_vpotch));

	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL5,  0xa8000000);

	VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_CTRL, 0x0);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 2, 1);

	if (need_skip_cvm(1))
		bypass_flag |= 1 << 0;

	VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 2, 1);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 1, 2 | bypass_flag);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 1, 2 | bypass_flag);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_CTRL, 0);
	VSYNC_WR_DV_REG(DOLBY_CORE2A_CTRL, 0);

	if (dm_count == 0)
		count = 24;
	else
		count = dm_count;

	for (i = 0; i < count; i++) {
		if (reset || p_core2_dm_regs[i] != last_dm[i]) {
			VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 6 + i, p_core2_dm_regs[i]);
			set_lut = true;
		}
	}

	if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC2)
		set_lut = true;

	else if (stb_core_setting_update_flag & CP_FLAG_CONST_TC2)
		set_lut = false;

	if (debug_dolby & 2)
		pr_dolby_dbg("core2a g_potch %x %x, reset %d, set_lut %d, flag %x\n",
					 g_hpotch, g_vpotch, reset, set_lut,
					 stb_core_setting_update_flag);

	/* core2 metadata program done */
	VSYNC_WR_DV_REG(DOLBY_CORE2A_REG_START + 3, 1);

	if (lut_count == 0)
		count = 256 * 5;
	else
		count = lut_count;

	if (count && (set_lut || reset || force_set_lut)) {

		VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_CTRL, 0x1401);

		if (lut_endian) {
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_PORT, p_core2_lut[i + 3]);
				VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_PORT, p_core2_lut[i + 2]);
				VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_PORT, p_core2_lut[i + 1]);
				VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_PORT, p_core2_lut[i]);
			}
		} else {
			for (i = 0; i < count; i++) {
				VSYNC_WR_DV_REG(DOLBY_CORE2A_DMA_PORT, p_core2_lut[i]);
			}
		}

	}
	force_set_lut = false;

	/* enable core2 */
	VSYNC_WR_DV_REG(DOLBY_CORE2A_SWAP_CTRL0, dolby_enable << 0);
	return 0;
}

bool is_core3_mute_reg(int index)
{
	return (index == 12) || /* ipt_scale for ipt*/
		   (index >= 16 && index <= 17) || /* rgb2yuv scale for yuv */
		   (index >= 5 && index <= 9); /* lms2rgb coeff for rgb */
}

static int dolby_core3_set
		(u32 dm_count,
		 u32 md_count,
		 u32 *p_core3_dm_regs,
		 u32 *p_core3_md_regs,
		 int hsize,
		 int vsize,
		 int dolby_enable,
		 int scramble_en,
		 u8 pps_state)
{
	u32 count;
	int i;
	int vsize_hold = 0x10;
	u32 diag_mode = 0;
	u32 cur_dv_mode = dolby_vision_mode;
	u32 *last_dm = (u32 *)&dovi_setting.dm_reg3;
	bool reset = false;
	u32 diag_enable = 0;
	bool reset_post_table = false;

	if (new_dovi_setting.diagnostic_enable || new_dovi_setting.dovi_ll_enable)
		diag_enable = 1;

	if (dolby_vision_on && (dolby_vision_flags & FLAG_DISABE_CORE_SETTING))
		return 0;

	if (!dolby_vision_on || (dolby_vision_flags & FLAG_CERTIFICAION))
		reset = true;

	if (force_update_reg & 4)
		reset = true;

	if (dolby_vision_on_count == 0)
		reset = true;

	if ((cur_dv_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL ||
		 cur_dv_mode == DOLBY_VISION_OUTPUT_MODE_IPT) && diag_enable) {
		cur_dv_mode = dv_ll_output_mode & 0xff;

		if (dolby_vision_ll_policy == DOLBY_VISION_LL_YUV422)
			diag_mode = 0x20;
		else
			diag_mode = 3;

	}

	if (dolby_vision_on &&
		(last_dolby_vision_ll_policy != dolby_vision_ll_policy ||
		 new_dovi_setting.mode_changed ||
		 new_dovi_setting.vsvdb_changed ||
		 pps_state)) {

		last_dolby_vision_ll_policy = dolby_vision_ll_policy;
		new_dovi_setting.vsvdb_changed = 0;
		new_dovi_setting.mode_changed = 0;

		if (new_dovi_setting.dovi_ll_enable && new_dovi_setting.diagnostic_enable == 0) {
			VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 3, 6, 2); /* post matrix */
			VSYNC_WR_DV_REG_BITS(VPP_MATRIX_CTRL, 1, 0, 1); /* post matrix */
		} else {
			VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 0, 6, 2); /* post matrix */
			VSYNC_WR_DV_REG_BITS(VPP_MATRIX_CTRL, 0, 0, 1); /* post matrix */
		}

		reset_post_table = true;
	}

	/* flush post matrix table when ll mode on and setting changed */
	if (new_dovi_setting.dovi_ll_enable &&
		new_dovi_setting.diagnostic_enable == 0 &&
		dolby_vision_on &&
		(reset_post_table || reset || memcmp(&p_core3_dm_regs[18], &last_dm[18], 32)))
		enable_rgb_to_yuv_matrix_for_dvll(1, &p_core3_dm_regs[18], 12);

	if (get_vpu_mem_pd_vmod(VPU_DOLBY_CORE3) == VPU_MEM_POWER_DOWN ||
		get_dv_mem_power_flag(VPU_DOLBY_CORE3) == VPU_MEM_POWER_DOWN)
		dv_mem_power_on(VPU_DOLBY_CORE3);


	VSYNC_WR_DV_REG(DOLBY_CORE3_CLKGATE_CTRL, 0);
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL1, ((hsize + htotal_add) << 16) | (vsize + vtotal_add + vsize_add + vsize_hold * 2));
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL2, (hsize << 16) | (vsize + vsize_add));
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL3, (0x80 << 16) | vsize_hold);
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL4, (0x04 << 16) | vsize_hold);
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL5, 0x0000);
			
	if (cur_dv_mode != DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)
		VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL6, 0);
	else
		VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL6, 0x10000000);  /* swap UV */

	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 5, 7);
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 4, 4);
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 4, 2);
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 2, 1);

	/* Control Register, address 0x04 2:0 RW */
	/* Output_operating mode */
	/*   00- IPT 12 bit 444 bypass Dolby Vision output */
	/*   01- IPT 12 bit tunnelled over RGB 8 bit 444, dolby vision output */
	/*   02- HDR10 output, RGB 10 bit 444 PQ */
	/*   03- Deep color SDR, RGB 10 bit 444 Gamma */
	/*   04- SDR, RGB 8 bit 444 Gamma */

	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 1, cur_dv_mode);
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 1, cur_dv_mode);
	
	/* for delay */
	if (dm_count == 0)
		count = 26;
	else
		count = dm_count;

	for (i = 0; i < count; i++) {
			
		if (reset || p_core3_dm_regs[i] != last_dm[i] || is_core3_mute_reg(i)) {
			
			if ((dolby_vision_flags & FLAG_MUTE) && is_core3_mute_reg(i))
				VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 0x6 + i, 0);
			else
				VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 0x6 + i, p_core3_dm_regs[i]);
			
		}
	}
	/* from addr 0x18 */

	if (scramble_en) {
		if (md_count > 204) {
			pr_dolby_error("core3 metadata size %d > 204 !\n", md_count);
		} else {
			count = md_count;
			for (i = 0; i < count; i++) {
#ifdef FORCE_HDMI_META
				if (i == 20 &&
				    p_core3_md_regs[i] == 0x5140a3e)
					VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 0x24 + i,
						 (p_core3_md_regs[i] & 0xffffff00) | 0x80);
				else
#endif
				VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 0x24 + i, p_core3_md_regs[i]);
			}
			for (; i < (MAX_CORE3_MD_SIZE + 1); i++)
				VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 0x24 + i, 0);
		}
	}

	/* from addr 0x90 */
	/* core3 metadata program done */
	VSYNC_WR_DV_REG(DOLBY_CORE3_REG_START + 3, 1);

	VSYNC_WR_DV_REG(DOLBY_CORE3_DIAG_CTRL, diag_mode);

	if ((dolby_vision_flags & FLAG_CERTIFICAION) &&
		!(dolby_vision_flags & FLAG_DISABLE_CRC))
		VSYNC_WR_DV_REG(DOLBY_CORE3_CRC_CTRL, 1);

	/* enable core3 */
	VSYNC_WR_DV_REG(DOLBY_CORE3_SWAP_CTRL0, (dolby_enable << 0));
	return 0;
}

/* note: This is used externally */
void update_graphic_width_height(unsigned int width,
								 unsigned int height)
{
	new_osd_graphic_width = width;
	new_osd_graphic_height = height;
	pr_dolby_dbg("osd update, height and width [%d] [%d]\n", new_osd_graphic_width, new_osd_graphic_height);
}

/* note: This is used externally */
void update_graphic_status(void)
{
	osd_update = true;
	pr_dolby_dbg("osd update, need toggle\n");
}

static int is_graphic_changed(void)
{
	int ret = 0;

	if (is_graphics_output_off()) {
		if (!is_osd_off) {
			pr_dolby_dbg("osd off\n");
			is_osd_off = true;
			ret |= 1;
		}
	} else if (is_osd_off) {
		/* force reset core2 when osd off->on */
		force_reset_core2 = true;
		pr_dolby_dbg("osd on\n");
		is_osd_off = false;
		ret |= 2;
	}

	/*
	if (osd_graphic_width != new_osd_graphic_width ||
		osd_graphic_height != new_osd_graphic_height) {
		if (debug_dolby & 0x2)
			pr_dolby_dbg("osd changed %d %d-%d %d\n",
						 osd_graphic_width,
						 osd_graphic_height,
						 new_osd_graphic_width,
						 new_osd_graphic_height);

		 TODO: g12/tm2/sc2/t7 osd pps is after dolby core2, but 
		 sometimes osd do crop,should monitor osd size change
		if (!is_osd_off) {
			osd_graphic_width = new_osd_graphic_width;
			osd_graphic_height = new_osd_graphic_height;
			ret |= 2;
		}
	}
	*/

	if (old_dolby_vision_graphic_max !=
		dolby_vision_graphic_max) {
		if (debug_dolby & 0x2)
			pr_dolby_dbg("graphic max changed %d-%d\n", old_dolby_vision_graphic_max, dolby_vision_graphic_max);
		if (!is_osd_off) {
			old_dolby_vision_graphic_max = dolby_vision_graphic_max;
			ret |= 2;
			force_set_lut = true;
		}
	}

	return ret;
}

static int cur_mute_type;
static char mute_type_str[4][4] = {
		"NON",
		"YUV",
		"RGB",
		"IPT"
};

int get_mute_type(void)
{
	if (dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444)
		return MUTE_TYPE_RGB;
	else if ((dolby_vision_ll_policy == DOLBY_VISION_LL_YUV422) ||
			 (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_SDR8) ||
			 (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_SDR10) ||
			 (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_HDR10))
		return MUTE_TYPE_YUV;
	else if ((dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT) ||
			 (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL))
		return MUTE_TYPE_IPT;
	else
		return MUTE_TYPE_NONE;
}

static void apply_stb_core_settings
		(int enable, unsigned int mask,
		 bool reset, u32 frame_size, u8 pps_state)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	u32 h_size = (frame_size >> 16) & 0xffff;
	u32 v_size = frame_size & 0xffff;
	u32 core1_dm_count = 27;
	u32 graphics_w = osd_graphic_width;
	u32 graphics_h = osd_graphic_height;
	u32 update_bk = stb_core_setting_update_flag;
	static u32 update_flag_more;
	int mute_type;

	if (h_size == 0xffff)
		h_size = 0;
	if (v_size == 0xffff)
		v_size = 0;

	if (stb_core_setting_update_flag != update_flag_more &&
		(debug_dolby & 2))
		pr_dolby_dbg
		("%s update setting again %x->%x\n",
		 __func__, stb_core_setting_update_flag,
		 update_flag_more);

	stb_core_setting_update_flag |= update_flag_more;

	if ((dolby_vision_flags & FLAG_CERTIFICAION)) {
		graphics_w = dv_cert_graphic_width;
		graphics_h = dv_cert_graphic_height;
	}

	adjust_vpotch();
	if (mask & 1) { /* Core 1 (EL) */

		dolby_core1_set
				(core1_dm_count, 173, 256 * 5,
				 (u32 *)&new_dovi_setting.dm_reg1,
				 (u32 *)&new_dovi_setting.comp_reg,
				 (u32 *)&new_dovi_setting.dm_lut1,
				 h_size,
				 v_size,
				 enable, 								/* BL enable */
				 enable && new_dovi_setting.el_flag,	/* EL enable */
				 new_dovi_setting.el_halfsize_flag,
				 dolby_vision_mode ==
				 DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL,
				 new_dovi_setting.src_format == FORMAT_DOVI,
				 1,
				 reset);

	}

	if (mask & 2) { /* Core 2 (BL) */

		if (stb_core_setting_update_flag != CP_FLAG_CHANGE_ALL) {
			
			/* when CP_FLAG_CONST_TC2 is set, */
			/* set the stb_core_setting_update_flag */
			/* until only meeting the CP_FLAG_CONST_TC2 */
		
			if (stb_core_setting_update_flag & CP_FLAG_CONST_TC2)
				stb_core2_const_flag = true;
			
			else if (stb_core_setting_update_flag & CP_FLAG_CHANGE_TC2)
				stb_core2_const_flag = false;
		}

		/* revert the core2 lut as last corret one when const case */
		if (stb_core2_const_flag)
			memcpy(&new_dovi_setting.dm_lut2, &dovi_setting.dm_lut2, sizeof(struct dm_lut_ipcore));

		dolby_core2_set(
				24, 256 * 5,
				(u32 *)&new_dovi_setting.dm_reg2,
				(u32 *)&new_dovi_setting.dm_lut2,
				graphics_w, 
				graphics_h, 
				1, 1);
	}

	if (mask & 4) { 
		v_size = vinfo->height;
		if ((vinfo->width == 720 &&
			 vinfo->height == 480 &&
			 vinfo->height != vinfo->field_height) ||
			(vinfo->width == 720 &&
			 vinfo->height == 576 &&
			 vinfo->height != vinfo->field_height) ||
			(vinfo->width == 1920 &&
			 vinfo->height == 1080 &&
			 vinfo->height != vinfo->field_height) ||
			(vinfo->width == 1920 &&
			 vinfo->height == 1080 &&
			 vinfo->height != vinfo->field_height &&
			 vinfo->sync_duration_num
			 / vinfo->sync_duration_den == 50))
			v_size = v_size / 2;
		mute_type = get_mute_type();

		if ((get_video_mute() == VIDEO_MUTE_ON_DV) && (!(dolby_vision_flags & FLAG_MUTE) || cur_mute_type != mute_type)) {

			pr_dolby_dbg("mute %s\n", mute_type_str[mute_type]);

			/* unmute vpp and mute by core3 */
			VSYNC_WR_MPEG_REG(VPP_CLIP_MISC0, (0x3ff << 20) | (0x3ff << 10) | 0x3ff);
			VSYNC_WR_MPEG_REG(VPP_CLIP_MISC1, (0x0 << 20) | (0x0 << 10) | 0x0);

			cur_mute_type = mute_type;
			dolby_vision_flags |= FLAG_MUTE;

		} else if ((get_video_mute() == VIDEO_MUTE_OFF) && (dolby_vision_flags & FLAG_MUTE)) {

			/* vpp unmuted when dv mute */
			/* clean flag to unmute core3 here*/

			pr_dolby_dbg("unmute %s\n", mute_type_str[cur_mute_type]);

			cur_mute_type = MUTE_TYPE_NONE;
			dolby_vision_flags &= ~FLAG_MUTE;
		}

		dolby_core3_set(26, 
						new_dovi_setting.md_reg3.size,
						(u32 *)&new_dovi_setting.dm_reg3,
				 		new_dovi_setting.md_reg3.raw_metadata,
				 		vinfo->width, v_size,
				 		1,
				 		dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL,
				 		pps_state);
	}
	stb_core_setting_update_flag = 0;
	update_flag_more = update_bk;
}

static bool is_video_effect_bypass;

static void video_effect_bypass(int bypass)
{

	if (debug_bypass_vpp_pq == 1) {
		if ((dolby_vision_flags & FLAG_CERTIFICAION) ||
			bypass_all_vpp_pq)
			dv_pq_ctl(DV_PQ_CERT);
		else
			dv_pq_ctl(DV_PQ_BYPASS);
		return;
	} else if (debug_bypass_vpp_pq == 2) {
		dv_pq_ctl(DV_PQ_REC);
		return;
	} else if (debug_bypass_vpp_pq == 3) {
		return;
	}

	if (bypass) {
		
		if (!is_video_effect_bypass) {
			is_video_effect_bypass = true;
		}

		if ((dolby_vision_flags & FLAG_CERTIFICAION) || bypass_all_vpp_pq)
			dv_pq_ctl(DV_PQ_CERT);
		else
			dv_pq_ctl(DV_PQ_BYPASS);

	} else if (is_video_effect_bypass) {

		dv_pq_ctl(DV_PQ_REC);

		is_video_effect_bypass = false;
	}
}

static u32 viu_misc_ctrl_backup;
static u32 vpp_matrix_backup;
static u32 vpp_dummy1_backup;
void enable_dolby_vision(int enable)
{

	if (enable) {
		if (!dolby_vision_on) {

			dolby_vision_wait_on = true;

			viu_misc_ctrl_backup = VSYNC_RD_DV_REG(VIU_MISC_CTRL1);
			vpp_matrix_backup = VSYNC_RD_DV_REG(VPP_MATRIX_CTRL);
			vpp_dummy1_backup = VSYNC_RD_DV_REG(VPP_DUMMY_DATA1);

			hdr_osd_off();
			hdr_vd1_off();

			set_hdr_module_status(VD1_PATH, HDR_MODULE_BYPASS);

			if (dolby_vision_mask & 4) /* Core 3 (RPU) */
				VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 1, 3, 1);   /* core3 enable */
			else
				VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 0, 3, 1);   /* bypass core3 */
			
			if (dolby_vision_mask & 2) /* Core 2 (VPU) */
				VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 0, 2, 1);	 /* core2 enable */
			else
				VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 1, 2, 1);  /* core2 bypass */

			hdr_vd1_off();
			
			if ((dolby_vision_mask & 1) && dovi_setting_video_flag) {
				VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 0, 0, 1); /* core1 on */
				dolby_vision_core1_on = true;
			} else {
				VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 1, 0, 1); /* core1 off */
				dv_mem_power_off(VPU_DOLBY1A);
				dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
				VSYNC_WR_DV_REG(DOLBY_CORE1_CLKGATE_CTRL, 0x55555455);
				dolby_vision_core1_on = false;
			}

			if (dolby_vision_flags & FLAG_CERTIFICAION) {
				/* bypass dither/PPS/SR/CM*/
				/*   bypass EO/OE*/
				/*   bypass vadj2/mtx/gainoff */
				VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 7, 0, 3);
				/* bypass all video effect */
				video_effect_bypass(1);
				/* 12 bit unsigned to sign*/
				/*   before vadj1 */
				/* 12 bit sign to unsign*/
				/*   before post blend */
				VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA0, 0x08000800);
				/* 12->10 before vadj2*/
				/*   10->12 after gainoff */
				VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA1, 0x20002000);
			} else {
				/* bypass all video effect */
				if (dolby_vision_flags &FLAG_BYPASS_VPP)
					video_effect_bypass(1);
				/* 12->10 before vadj1*/
				/*   10->12 before post blend */
				VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA0, 0x20002000);
				/* 12->10 before vadj2*/
				/*   10->12 after gainoff */
				VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA1, 0x20002000);
			}

			VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, 0);
			VSYNC_WR_DV_REG(VPP_DUMMY_DATA1, 0x80200);

			if ((dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL ||
				 dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT) &&
				dovi_setting.diagnostic_enable == 0 &&
				dovi_setting.dovi_ll_enable) {

				u32 *reg = (u32 *)&dovi_setting.dm_reg3;

				/* input u12 -0x800 to s12 */
				VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA1, 0x8000800);

				/* bypass vadj */
				VSYNC_WR_DV_REG(VPP_VADJ_CTRL, 0);

				/* bypass gainoff */
				VSYNC_WR_DV_REG(VPP_GAINOFF_CTRL0, 0);

				/* enable wm tp vks*/
				/* bypass gainoff to vks */
				VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 1, 1, 2);

				enable_rgb_to_yuv_matrix_for_dvll
						(1, &reg[18],
						 (dv_ll_output_mode >> 8)
						 & 0xff);
			} else {
				enable_rgb_to_yuv_matrix_for_dvll
						(0, NULL, 12);
			}
			last_dolby_vision_ll_policy = dolby_vision_ll_policy;
			
			pr_dolby_dbg("Dolby Vision G12a turn on%s\n", dolby_vision_core1_on ? ", core1 on" : "");
			
			if (!dolby_vision_core1_on)
				frame_count = 0;

			dolby_vision_core1_on_cnt = 0;
		} else {

			if (!dolby_vision_core1_on && (dolby_vision_mask & 1) && dovi_setting_video_flag) {

				VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 0, 0, 2); /* enable core1 */

				hdr_vd1_off();

				dolby_vision_core1_on = true;
				dolby_vision_core1_on_cnt = 0;
				pr_dolby_dbg("Dolby Vision core1 turn on\n");
			
			} else if (dolby_vision_core1_on && (!(dolby_vision_mask & 1) || !dovi_setting_video_flag)) {

				/* core1a */
				VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL, 3, 0, 2);
				dv_mem_power_off(VPU_DOLBY1A);
				dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
				VSYNC_WR_DV_REG(DOLBY_CORE1_CLKGATE_CTRL, 0x55555455);

				dolby_vision_core1_on = false;
				dolby_vision_core1_on_cnt = 0;
				frame_count = 0;
				last_vf_valid_crc = 0;
				pr_dolby_dbg("Dolby Vision core1 turn off\n");
			}
		}
		dolby_vision_on = true;
		dolby_vision_wait_on = false;
		dolby_vision_wait_init = false;
		dolby_vision_wait_count = 0;
		vsync_count = 0;
	} else {

		/* Disabling DoVi if it is on - then switch everything off */
		if (dolby_vision_on) {

			VSYNC_WR_DV_REG_BITS(DOLBY_PATH_CTRL,
								 (1 << 2) |	/* core2 bypass */
								 (1 << 1) |	/* vd2 connect to vpp */
								 (1 << 0),	/* core1 bl bypass */
								 0, 3);
			
			VSYNC_WR_DV_REG_BITS(VPP_DOLBY_CTRL, 0, 3, 1);   /* core3 disable */
			
			/* core1a */
			VSYNC_WR_DV_REG(DOLBY_CORE1_CLKGATE_CTRL, 0x55555455);
			dv_mem_power_off(VPU_DOLBY1A);
			dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
			
			/* core2 */
			VSYNC_WR_DV_REG(DOLBY_CORE2A_CLKGATE_CTRL, 0x55555555);
			dv_mem_power_off(VPU_DOLBY2);
			
			/* core3 */
			VSYNC_WR_DV_REG(DOLBY_CORE3_CLKGATE_CTRL, 0x55555555);
			dv_mem_power_off(VPU_DOLBY_CORE3);

			if (p_funcs_stb) /* destroy ctx */
				p_funcs_stb->control_path
						(FORMAT_INVALID, 0,
						 comp_buf[current_id], 0,
						 md_buf[current_id], 0,
						 0, 0, 0, SIGNAL_RANGE_SMPTE,
						 0, 0, 0, 0,
						 0,
						 &hdr10_param,
						 &new_dovi_setting);
			last_dolby_vision_ll_policy =
					DOLBY_VISION_LL_DISABLE;
			stb_core_setting_update_flag =
					CP_FLAG_CHANGE_ALL;
			stb_core2_const_flag = false;
			memset(&dovi_setting, 0, sizeof(dovi_setting));
			dovi_setting.src_format = FORMAT_SDR;
			pr_dolby_dbg("Dolby Vision G12a turn off\n");

			VSYNC_WR_DV_REG(VIU_SW_RESET, 3 << 9);
			VSYNC_WR_DV_REG(VIU_SW_RESET, 0);


			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC0, (0x3ff << 20) | (0x3ff << 10) | 0x3ff);
			VSYNC_WR_DV_REG(VPP_VD1_CLIP_MISC1, 0);
			video_effect_bypass(0);

			/* always vd2 to vpp and bypass core 1 (EL) */
			viu_misc_ctrl_backup |= (VSYNC_RD_DV_REG(VIU_MISC_CTRL1) & 2);

			VSYNC_WR_DV_REG(VIU_MISC_CTRL1, viu_misc_ctrl_backup | (3 << 16));
			VSYNC_WR_DV_REG(VPP_MATRIX_CTRL, vpp_matrix_backup);
			VSYNC_WR_DV_REG(VPP_DUMMY_DATA1, vpp_dummy1_backup);
		}

		frame_count = 0;
		last_vf_valid_crc = 0;
		core1_disp_hsize = 0;
		core1_disp_vsize = 0;
		dolby_vision_on = false;
		force_reset_core2 = true;
		dolby_vision_core2_on_cnt = 0;
		dolby_vision_on_in_uboot = false;
		dolby_vision_core1_on = false;
		dolby_vision_core1_on_cnt = 0;
		dolby_vision_wait_on = false;
		dolby_vision_wait_init = false;
		dolby_vision_wait_count = 0;
		dolby_vision_status = BYPASS_PROCESS;
		dolby_vision_target_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
		dolby_vision_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
		dolby_vision_src_format = 0;
		dolby_vision_on_count = 0;
		cur_csc_type[VD1_PATH] = VPP_MATRIX_NULL;
		/* clean mute flag for next time dv on */
		dolby_vision_flags &= ~FLAG_MUTE;

		hdr_osd_off();
		hdr_vd1_off();

	}
}
EXPORT_SYMBOL(enable_dolby_vision);

/*  dolby vision enhanced layer receiver*/

#define DVEL_RECV_NAME "dvel"

static inline void dvel_vf_put(struct vframe_s *vf)
{
	struct vframe_provider_s *vfp = vf_get_provider(DVEL_RECV_NAME);
	int event = 0;

	if (vfp) {
		vf_put(vf, DVEL_RECV_NAME);
		event |= VFRAME_EVENT_RECEIVER_PUT;
		vf_notify_provider(DVEL_RECV_NAME, event, NULL);
	}
}

static inline struct vframe_s *dvel_vf_peek(void)
{
	return vf_peek(DVEL_RECV_NAME);
}

static inline struct vframe_s *dvel_vf_get(void)
{
	int event = 0;
	struct vframe_s *vf = vf_get(DVEL_RECV_NAME);

	if (vf) {
		event |= VFRAME_EVENT_RECEIVER_GET;
		vf_notify_provider(DVEL_RECV_NAME, event, NULL);
	}
	return vf;
}

static struct vframe_s *dv_vf[16][2];
static void *metadata_parser;
static bool metadata_parser_reset_flag;
static char meta_buf[1024];
void dv_vf_light_unreg_provider(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&dovi_lock, flags);
	if (vfm_path_on) {

		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0]) {
				if (dv_vf[i][1])
					dvel_vf_put(dv_vf[i][1]);
				dv_vf[i][1] = NULL;
			}
			dv_vf[i][0] = NULL;
		}

		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		memset(&last_hdr10_param, 0, sizeof(last_hdr10_param));
		frame_count = 0;
		setting_update_count = 0;
		crc_count = 0;
		crc_bypass_count = 0;
		dolby_vision_el_disable = 0;
	}
	vfm_path_on = false;
	spin_unlock_irqrestore(&dovi_lock, flags);
}
EXPORT_SYMBOL(dv_vf_light_unreg_provider);

void dv_vf_light_reg_provider(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&dovi_lock, flags);
	if (!vfm_path_on) {
		for (i = 0; i < 16; i++) {
			dv_vf[i][0] = NULL;
			dv_vf[i][1] = NULL;
		}
		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		memset(&last_hdr10_param, 0, sizeof(last_hdr10_param));
		frame_count = 0;
		setting_update_count = 0;
		crc_count = 0;
		crc_bypass_count = 0;
		dolby_vision_el_disable = 0;
	}
	vfm_path_on = true;
	spin_unlock_irqrestore(&dovi_lock, flags);
}
EXPORT_SYMBOL(dv_vf_light_reg_provider);

static int dvel_receiver_event_fun(int type, void *data, void *arg)
{
	char *provider_name = (char *)data;
	int i;
	unsigned long flags;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		pr_info("%s, provider %s unregistered\n",
				__func__, provider_name);
		spin_lock_irqsave(&dovi_lock, flags);
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0]) {
				if (dv_vf[i][1])
					dvel_vf_put(dv_vf[i][1]);
				dv_vf[i][1] = NULL;
			}
			dv_vf[i][0] = NULL;
		}

		spin_unlock_irqrestore(&dovi_lock, flags);
		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		frame_count = 0;
		setting_update_count = 0;
		crc_count = 0;
		crc_bypass_count = 0;
		dolby_vision_el_disable = false;
		return -1;
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		return RECEIVER_ACTIVE;
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		pr_info("%s, provider %s registered\n",
				__func__, provider_name);
		spin_lock_irqsave(&dovi_lock, flags);
		for (i = 0; i < 16; i++)
			dv_vf[i][0] = dv_vf[i][1] = NULL;
		spin_unlock_irqrestore(&dovi_lock, flags);
		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		frame_count = 0;
		setting_update_count = 0;
		crc_count = 0;
		crc_bypass_count = 0;
		dolby_vision_el_disable = false;
	}
	return 0;
}

static const struct vframe_receiver_op_s dvel_vf_receiver = {
		.event_cb = dvel_receiver_event_fun
};

static struct vframe_receiver_s dvel_vf_recv;

void dolby_vision_init_receiver(void *pdev)
{
	ulong alloc_size;
	int i;

	pr_info("%s(%s)\n", __func__, DVEL_RECV_NAME);
	vf_receiver_init(&dvel_vf_recv, DVEL_RECV_NAME, &dvel_vf_receiver, &dvel_vf_recv);
	vf_reg_receiver(&dvel_vf_recv);
	pr_info("%s: %s\n", __func__, dvel_vf_recv.name);
	dovi_pdev = (struct platform_device *)pdev;
	alloc_size = dma_size;
	alloc_size = (alloc_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	dma_vaddr = dma_alloc_coherent(&dovi_pdev->dev, alloc_size, &dma_paddr, GFP_KERNEL);
	for (i = 0; i < 2; i++) {
		md_buf[i] = vmalloc(MD_BUF_SIZE);
		if (md_buf[i])
			memset(md_buf[i], 0, MD_BUF_SIZE);
		comp_buf[i] = vmalloc(COMP_BUF_SIZE);
		if (comp_buf[i])
			memset(comp_buf[i], 0, COMP_BUF_SIZE);
		drop_md_buf[i] = vmalloc(MD_BUF_SIZE);
		if (drop_md_buf[i])
			memset(drop_md_buf[i], 0, MD_BUF_SIZE);
		drop_comp_buf[i] = vmalloc(COMP_BUF_SIZE);
		if (drop_comp_buf[i])
			memset(drop_comp_buf[i], 0, COMP_BUF_SIZE);
	}
	vsem_if_buf = vmalloc(VSEM_IF_BUF_SIZE);
	if (vsem_if_buf)
		memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
	vsem_md_buf = vmalloc(VSEM_IF_BUF_SIZE);
	if (vsem_md_buf)
		memset(vsem_md_buf, 0, VSEM_IF_BUF_SIZE);
}

void dolby_vision_clear_buf(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&dovi_lock, flags);
	for (i = 0; i < 16; i++) {
		dv_vf[i][0] = NULL;
		dv_vf[i][1] = NULL;
	}
	spin_unlock_irqrestore(&dovi_lock, flags);
}
EXPORT_SYMBOL(dolby_vision_clear_buf);

#define MAX_FILENAME_LENGTH 64
static const char comp_file[] = "%s_comp.%04d.reg";
static const char dm_reg_core1_file[] = "%s_dm_core1.%04d.reg";
static const char dm_reg_core2_file[] = "%s_dm_core2.%04d.reg";
static const char dm_reg_core3_file[] = "%s_dm_core3.%04d.reg";
static const char dm_lut_core1_file[] = "%s_dm_core1.%04d.lut";
static const char dm_lut_core2_file[] = "%s_dm_core2.%04d.lut";

static void dump_struct(void *structure,
						int struct_length,
						const char file_string[],
						int frame_nr)
{
	char fn[MAX_FILENAME_LENGTH];
	struct file *fp;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();

	if (frame_nr == 0)
		return;

	set_fs(KERNEL_DS);
	snprintf(fn, MAX_FILENAME_LENGTH, file_string, "/data/tmp/tmp", frame_nr - 1);
	fp = filp_open(fn, O_RDWR | O_CREAT, 0666);

	if (!fp) {
		pr_info("Error open file for writing NULL\n");
	} else {
		vfs_write(fp, structure, struct_length, &pos);
		vfs_fsync(fp, 0);
		filp_close(fp, NULL);
	}

	set_fs(old_fs);
}

void dolby_vision_dump_struct(void)
{
	dump_struct(&dovi_setting.dm_reg1, sizeof(dovi_setting.dm_reg1), dm_reg_core1_file, frame_count);

	if (dovi_setting.el_flag)
		dump_struct(&dovi_setting.comp_reg, sizeof(dovi_setting.comp_reg), comp_file, frame_count);

	if (!is_graphics_output_off())
		dump_struct(&dovi_setting.dm_reg2, sizeof(dovi_setting.dm_reg2), dm_reg_core2_file, frame_count);

	dump_struct(&dovi_setting.dm_reg3, sizeof(dovi_setting.dm_reg3), dm_reg_core3_file, frame_count);

	dump_struct(&dovi_setting.dm_lut1, sizeof(dovi_setting.dm_lut1), dm_lut_core1_file, frame_count);

	if (!is_graphics_output_off())
		dump_struct(&dovi_setting.dm_lut2, sizeof(dovi_setting.dm_lut2),dm_lut_core2_file, frame_count);

	pr_dolby_dbg("setting for frame %d dumped\n", frame_count);
}
EXPORT_SYMBOL(dolby_vision_dump_struct);

static void dump_setting
		(struct dovi_setting_s *setting,
		 int frame_cnt, int debug_flag)
{
	int i;
	u32 *p;

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("core1\n");
		p = (u32 *)&setting->dm_reg1;
		for (i = 0; i < 27; i++)
			pr_info("%08x\n", p[i]);
		pr_info("\ncomposer\n");
		p = (u32 *)&setting->comp_reg;
		for (i = 0; i < 173; i++)
			pr_info("%08x\n", p[i]);
	}

	if ((debug_flag & 0x20) && dump_enable) {
		pr_info("\ncore1lut\n");
		p = (u32 *)&setting->dm_lut1.tm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.tm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.sm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.sm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.g_2_l;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x10) && dump_enable && !is_graphics_output_off()) {
		pr_info("core2\n");
		p = (u32 *)&setting->dm_reg2;
		for (i = 0; i < 24; i++)
			pr_info("%08x\n", p[i]);
		pr_info("core2 swap\n");
		for (i = DOLBY_CORE2A_CLKGATE_CTRL;
			 i <= DOLBY_CORE2A_DMA_PORT; i++)
			pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
		pr_info("core2 real reg\n");
		for (i = DOLBY_CORE2A_REG_START;
			 i <= DOLBY_CORE2A_REG_START + 5; i++)
			pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
	}

	if ((debug_flag & 0x20) && dump_enable && !is_graphics_output_off()) {
		pr_info("\ncore2lut\n");
		p = (u32 *)&setting->dm_lut2.tm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.tm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.sm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.sm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.g_2_l;
		for (i = 0; i < 64; i++)
			pr_info
					("%08x, %08x, %08x, %08x\n",
					 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("core3\n");
		p = (u32 *)&setting->dm_reg3;
		for (i = 0; i < 26; i++)
			pr_info("%08x\n", p[i]);
		pr_info("core3 swap\n");
		for (i = DOLBY_CORE3_CLKGATE_CTRL;
			 i <= DOLBY_CORE3_OUTPUT_CSC_CRC; i++)
			pr_info("[0x%4x] = 0x%x\n", i, READ_VPP_DV_REG(i));
		pr_info("core3 real reg\n");
		for (i = DOLBY_CORE3_REG_START;
			 i <= DOLBY_CORE3_REG_START + 67; i++)
			pr_info("[0x%4x] = 0x%x\n", i, READ_VPP_DV_REG(i));
	}

	if ((debug_flag & 0x40) && dump_enable &&
		dolby_vision_mode <= DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
		pr_info("\ncore3_meta %d\n", setting->md_reg3.size);
		p = setting->md_reg3.raw_metadata;
		for (i = 0; i < setting->md_reg3.size; i++)
			pr_info("%08x\n", p[i]);
		pr_info("\n");
	}
}

void dolby_vision_dump_setting(int debug_flag)
{
	pr_dolby_dbg("\n====== setting for frame %d ======\n", frame_count);
	dump_setting(&new_dovi_setting, frame_count, debug_flag);
	pr_dolby_dbg("=== setting for frame %d dumped ===\n\n", frame_count);
}
EXPORT_SYMBOL(dolby_vision_dump_setting);

static int sink_support_dolby_vision(const struct vinfo_s *vinfo)
{
	if (dolby_vision_flags & FLAG_DISABLE_DOVI_OUT)
		return 0;
	return (sink_hdr_support(vinfo) & DV_SUPPORT) >> DV_SUPPORT_SHF;
}

static int sink_support_hdr(const struct vinfo_s *vinfo)
{
	return sink_hdr_support(vinfo) & HDR_SUPPORT;
}

static int sink_support_hdr10_plus(const struct vinfo_s *vinfo)
{
	return sink_hdr_support(vinfo) & HDRP_SUPPORT;
}

static int current_hdr_cap = -1; /* should set when probe */
static int current_sink_available;

static int is_policy_changed(void)
{
	int ret = 0;

	if (last_dolby_vision_policy != dolby_vision_policy) {
		/* handle policy change */
		pr_dolby_dbg("policy changed %d->%d\n",
					 last_dolby_vision_policy,
					 dolby_vision_policy);
		last_dolby_vision_policy = dolby_vision_policy;
		ret |= 1;
	}
	if (last_dolby_vision_ll_policy != dolby_vision_ll_policy) {
		/* handle ll policy change when dolby on */
		if (dolby_vision_on) {
			pr_dolby_dbg("ll policy changed %d->%d\n",
						 last_dolby_vision_ll_policy,
						 dolby_vision_ll_policy);
			last_dolby_vision_ll_policy = dolby_vision_ll_policy;
			ret |= 2;
		}
	}
	if (last_dolby_vision_hdr10_policy != dolby_vision_hdr10_policy) {
		/* handle policy change */
		pr_dolby_dbg("hdr10 policy changed %d->%d\n",
					 last_dolby_vision_hdr10_policy,
					 dolby_vision_hdr10_policy);
		last_dolby_vision_hdr10_policy = dolby_vision_hdr10_policy;
		ret |= 4;
	}
	return ret;
}

static bool vf_is_hdr10_plus(struct vframe_s *vf);
static bool vf_is_hdr10(struct vframe_s *vf);
static bool vf_is_hlg(struct vframe_s *vf);
static bool is_mvc_frame(struct vframe_s *vf);
static const char *input_str[8] = {
		"NONE",
		"HDR",
		"HDR+",
		"DOVI",
		"PRIME",
		"HLG",
		"SDR",
		"MVC"
};

static void update_src_format
		(enum signal_format_enum src_format, struct vframe_s *vf)
{
	enum signal_format_enum cur_format = dolby_vision_src_format;

	if (src_format == FORMAT_DOVI ||
		src_format == FORMAT_DOVI_LL) {
		dolby_vision_src_format = 3;
	} else {
		if (vf) {
			if (vf_is_hdr10_plus(vf))
				dolby_vision_src_format = 2;
			else if (vf_is_hdr10(vf))
				dolby_vision_src_format = 1;
			else if (vf_is_hlg(vf))
				dolby_vision_src_format = 5;
			else if (is_mvc_frame(vf))
				dolby_vision_src_format = 7;
			else
				dolby_vision_src_format = 6;
		}
	}
	if (cur_format != dolby_vision_src_format) {
		pr_dolby_dbg
		("update src fmt: %s => %s, signal_type 0x%x, src fmt %d\n",
		 input_str[cur_format],
		 input_str[dolby_vision_src_format],
		 vf ? vf->signal_type : 0,
		 src_format);
		cur_format = dolby_vision_src_format;
	}
}

int get_dolby_vision_src_format(void)
{
	return dolby_vision_src_format;
}
EXPORT_SYMBOL(get_dolby_vision_src_format);

static enum signal_format_enum get_cur_src_format(void)
{
	int cur_format = dolby_vision_src_format;
	enum signal_format_enum ret = FORMAT_SDR;

	switch (cur_format) {
		case 1: /* HDR10 */
			ret = FORMAT_HDR10;
			break;
		case 2: /* HDR10+ */
			ret = FORMAT_HDR10PLUS;
			break;
		case 3: /* DOVI */
			ret = FORMAT_DOVI;
			break;
		case 5: /* HLG */
			ret = FORMAT_HLG;
			break;
		case 6: /* SDR */
			ret = FORMAT_SDR;
			break;
		case 7: /* MVC */
			ret = FORMAT_MVC;
			break;
		case 8: /* CUVA */
			ret = FORMAT_CUVA;
			break;
		default:
			break;
	}
	return ret;
}

static int dolby_vision_policy_process(int *mode, enum signal_format_enum src_format)
{
	
	const struct vinfo_s *vinfo;
	int mode_change = 0;

	if (!dolby_vision_enable || !p_funcs_stb)
		return mode_change;

	vinfo = get_current_vinfo();
	if (src_format == FORMAT_MVC) {
		if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) {
			if (debug_dolby & 2)
				pr_dolby_dbg("mvc, dovi output -> BYPASS\n");
			*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		} else {
			mode_change = 0;
		}
		return mode_change;
	}
	if (src_format == FORMAT_CUVA) {
		if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) {
			if (debug_dolby & 2)
				pr_dolby_dbg("cuva, dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
			*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		} else {
			mode_change = 0;
		}
		return mode_change;
	}

	if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SINK) {
		/* bypass dv_mode with efuse */
		if (efuse_mode == 1 && !dolby_vision_efuse_bypass)  {
			if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				mode_change = 0;
			}
			return mode_change;
		}
		if (src_format == FORMAT_HLG || (src_format == FORMAT_HDR10PLUS && !(dolby_vision_hdr10_policy & HDRP_BY_DV))) {
			if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				if (debug_dolby & 2)
					pr_dolby_dbg("hlg/hdr+, dovi output -> BYPASS\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
		} else if (cur_csc_type[VD1_PATH] != 0xffff && (get_hdr_module_status(VD1_PATH) == HDR_MODULE_ON)) {
			
			/*if vpp is playing hlg/hdr10+*/
			/*dolby need bypass at this time*/
			
			if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				if (debug_dolby & 2)
					pr_dolby_dbg("src=%d, hdr module on, dovi output->BYPASS\n", src_format);
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
			return mode_change;
			
		} else if (vinfo && sink_support_dolby_vision(vinfo)) {
			
			/* TV support DOVI, All -> DOVI */
			if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
				pr_dolby_dbg("src=%d, dovi output -> DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL\n", src_format);
				*mode = DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL;
				mode_change = 1;
			}
		} else if (vinfo && sink_support_hdr(vinfo)) {
			/* TV support HDR, All -> HDR */
			if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_HDR10) {
				pr_dolby_dbg("src=%d, dovi output -> DOLBY_VISION_OUTPUT_MODE_HDR10\n", src_format);
				*mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
				mode_change = 1;
			}
		} else {
			/* TV not support DOVI and HDR */
			if (src_format == FORMAT_DOVI || src_format == FORMAT_DOVI_LL) {
				/* DOVI to SDR */
				if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (src_format == FORMAT_HDR10) {
				if (dolby_vision_hdr10_policy & HDR_BY_DV_F_SINK) {
					if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_SDR8) {
						/* HDR10 to SDR */
						pr_dolby_dbg("hdr, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
						*mode =
								DOLBY_VISION_OUTPUT_MODE_SDR8;
						mode_change = 1;
					}
				} else if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					/* HDR bypass */
					pr_dolby_dbg("hdr, dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			} else if (is_meson_g12b_cpu() || is_meson_g12a_cpu()) {
				/* dv cores keep on if in sdr mode */
				if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_SDR8) {
					/* SDR to SDR */
					pr_dolby_dbg("sdr, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				/* HDR/SDR bypass */
				pr_dolby_dbg("sdr, dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
		}
	} else if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE) {
		
		/* bypass dv_mode with efuse */
		
		if (efuse_mode == 1 && !dolby_vision_efuse_bypass) {
			if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				mode_change = 0;
			}
			return mode_change;
		}
		if ((cur_csc_type[VD1_PATH] != 0xffff) &&
			(get_hdr_module_status(VD1_PATH) == HDR_MODULE_ON) &&
			(!((src_format == FORMAT_DOVI) ||
			   (src_format == FORMAT_DOVI_LL)))) {
			/* bypass dolby incase VPP is not in sdr mode */
			if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				pr_dolby_dbg("hdr module on, dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
			return mode_change;
		} else if ((src_format == FORMAT_DOVI) || (src_format == FORMAT_DOVI_LL)) {
			/* DOVI source */
			if (vinfo && sink_support_dolby_vision(vinfo)) {
				/* TV support DOVI, DOVI -> DOVI */
				if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
					pr_dolby_dbg("dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL;
					mode_change = 1;
				}
			} else if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, DOVI -> HDR */
				if (dolby_vision_mode !=
					DOLBY_VISION_OUTPUT_MODE_HDR10) {
					pr_dolby_dbg("dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_HDR10\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				/* TV not support DOVI and HDR, DOVI -> SDR */
				if (dolby_vision_mode !=
					DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if ((src_format == FORMAT_HDR10) && (dolby_vision_hdr10_policy & HDR_BY_DV_F_SRC)) {
			if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, HDR -> HDR */
				if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_HDR10) {
					pr_dolby_dbg("hdr10, dovi output -> DOLBY_VISION_OUTPUT_MODE_HDR10\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_SDR8) {
					/* HDR10 to SDR */
					pr_dolby_dbg("hdr10, dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if (dolby_vision_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) {
			/* HDR/SDR bypass */
			pr_dolby_dbg("sdr, dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
			*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		}
	} else if (dolby_vision_policy == DOLBY_VISION_FORCE_OUTPUT_MODE) {
		if (dolby_vision_mode != *mode) {
			pr_dolby_dbg("src=%d, dovi output mode change %d -> %d\n", src_format, dolby_vision_mode, *mode);
			mode_change = 1;
		}
	}
	return mode_change;
}

static char dv_provider[32] = "dvbldec";

void dolby_vision_set_provider(char *prov_name)
{
	if (prov_name && strlen(prov_name) < 32) {
		if (strcmp(dv_provider, prov_name)) {
			strcpy(dv_provider, prov_name);
			pr_dolby_dbg("provider changed to %s\n", dv_provider);
		}
	}
}
EXPORT_SYMBOL(dolby_vision_set_provider);

/* 0: no dv, 1: dv std, 2: dv ll */
int is_dovi_frame(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	char *p;
	unsigned int size = 0;
	unsigned int type = 0;
	enum vframe_signal_fmt_e fmt;

	if (!vf)
		return 0;

	fmt = get_vframe_src_fmt(vf);
	if (fmt == VFRAME_SIGNAL_FMT_DOVI)
		return true;

	if (fmt != VFRAME_SIGNAL_FMT_INVALID)
		return false;

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	req.low_latency = 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		if (!strcmp(dv_provider, "dvbldec"))
			vf_notify_provider_by_name
					(dv_provider,
					 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
					 (void *)&req);
		if (req.dv_enhance_exist)
			return 1;
		if (!req.aux_buf || !req.aux_size)
			return 0;
		p = req.aux_buf;
		while (p < req.aux_buf + req.aux_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (type == DV_SEI ||
				(type & 0xffff0000) == DV_AV1_SEI)
				return 1;
			p += size;
		}
	}
	return 0;
}
EXPORT_SYMBOL(is_dovi_frame);

bool is_dovi_dual_layer_frame(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	enum vframe_signal_fmt_e fmt;

	if (!vf)
		return false;

	fmt = get_vframe_src_fmt(vf);
	/* valid src_fmt = DOVI or invalid src_fmt will check dual layer */
	/* otherwise, it certainly is a non-dv vframe */
	if (fmt != VFRAME_SIGNAL_FMT_DOVI &&
		fmt != VFRAME_SIGNAL_FMT_INVALID)
		return false;

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		if (!strcmp(dv_provider, "dvbldec"))
			vf_notify_provider_by_name(dv_provider,
									   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
									   (void *)&req);
		if (req.dv_enhance_exist)
			return true;
	}
	return false;
}
EXPORT_SYMBOL(is_dovi_dual_layer_frame);


#define signal_color_primaries ((vf->signal_type >> 16) & 0xff)
#define signal_transfer_characteristic ((vf->signal_type >> 8) & 0xff)

static bool vf_is_hlg(struct vframe_s *vf)
{
	if ((signal_transfer_characteristic == 14 ||
		 signal_transfer_characteristic == 18) &&
		signal_color_primaries == 9)
		return true;
	return false;
}

static bool is_hlg_frame(struct vframe_s *vf)
{
	if (!vf)
		return false;
	if (((get_dolby_vision_hdr_policy() & 2) == 0) &&
		(signal_transfer_characteristic == 14 ||
		 signal_transfer_characteristic == 18) &&
		signal_color_primaries == 9)
		return true;

	return false;
}

static bool vf_is_hdr10_plus(struct vframe_s *vf)
{
	if (signal_transfer_characteristic == 0x30 &&
		(signal_color_primaries == 9 ||
		 signal_color_primaries == 2))
		return true;
	return false;
}

static bool is_cuva_frame(struct vframe_s *vf)
{
	if ((vf->signal_type >> 31) & 1)
		return true;
	return false;
}

static bool is_hdr10plus_frame(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (!vf)
		return false;
	if (!(dolby_vision_hdr10_policy & HDRP_BY_DV)) {
		/* report hdr10 for the content hdr10+ and
		 * sink is hdr10+ case
		 */
		if (signal_transfer_characteristic == 0x30 &&
			(sink_support_hdr10_plus(vinfo)) &&
			(signal_color_primaries == 9 ||
			 signal_color_primaries == 2))
			return true;
	}
	return false;
}

static bool vf_is_hdr10(struct vframe_s *vf)
{
	if (signal_transfer_characteristic == 16 &&
		(signal_color_primaries == 9 ||
		 signal_color_primaries == 2))
		return true;
	return false;
}

static bool is_hdr10_frame(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (!vf)
		return false;
	if ((signal_transfer_characteristic == 16 ||
		 /* report as hdr10 for the content hdr10+ and
		  * sink not support hdr10+ or use DV to handle
		  * hdr10+ as hdr10
		  */
		 (signal_transfer_characteristic == 0x30 &&
		  ((!sink_support_hdr10_plus(vinfo)) ||
		   (dolby_vision_hdr10_policy & HDRP_BY_DV)))) &&
		(signal_color_primaries == 9 ||
		 signal_color_primaries == 2))
		return true;
	return false;
}

static bool is_mvc_frame(struct vframe_s *vf)
{
	if (!vf)
		return false;
	if (vf->type & VIDTYPE_MVC)
		return true;
	return false;
}

int dolby_vision_check_mvc(struct vframe_s *vf)
{
	int mode;

	if (is_mvc_frame(vf) && dolby_vision_on) {
		/* mvc source, but dovi enabled, need bypass dv */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(&mode, FORMAT_MVC)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			dolby_vision_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_mvc);

int dolby_vision_check_hlg(struct vframe_s *vf)
{
	int mode;

	if (!is_dovi_frame(vf) && is_hlg_frame(vf) && !dolby_vision_on) {
		/* hlg source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(&mode, FORMAT_HLG)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			dolby_vision_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_hlg);

int dolby_vision_check_hdr10plus(struct vframe_s *vf)
{
	int mode;

	if (!is_dovi_frame(vf) && is_hdr10plus_frame(vf) && !dolby_vision_on) {
		/* hdr10+ source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(&mode, FORMAT_HDR10PLUS)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			dolby_vision_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_hdr10plus);

int dolby_vision_check_cuva(struct vframe_s *vf)
{
	int mode;

	if (is_cuva_frame(vf) && !dolby_vision_on) {
		/* dovi source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(&mode, FORMAT_CUVA)) {
			if ((mode != DOLBY_VISION_OUTPUT_MODE_BYPASS) &&
				(dolby_vision_mode ==
				 DOLBY_VISION_OUTPUT_MODE_BYPASS))
				dolby_vision_wait_on = true;
			dolby_vision_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_cuva);

int dolby_vision_check_hdr10(struct vframe_s *vf)
{
	int mode;

	if (!is_dovi_frame(vf) && is_hdr10_frame(vf) && !dolby_vision_on) {
		/* hdr10 source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(&mode, FORMAT_HDR10)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			dolby_vision_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_hdr10);

void dolby_vision_vf_put(struct vframe_s *vf)
{
	int i;

	if (vf)
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0] == vf) {
				if (dv_vf[i][1]) {
					if (debug_dolby & 2)
						pr_dolby_dbg
						("put bl(%p-%lld) with el(%p-%lld)\n",
						 vf, vf->pts_us64,
						 dv_vf[i][1],
						 dv_vf[i][1]->pts_us64);
					dvel_vf_put(dv_vf[i][1]);
				} else if (debug_dolby & 2) {
					pr_dolby_dbg("--- put bl(%p-%lld) ---\n",
								 vf, vf->pts_us64);
				}
				dv_vf[i][0] = NULL;
				dv_vf[i][1] = NULL;
			}
		}
}
EXPORT_SYMBOL(dolby_vision_vf_put);

struct vframe_s *dolby_vision_vf_peek_el(struct vframe_s *vf)
{
	int i;

	if (dolby_vision_flags && (p_funcs_stb)) {
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0] == vf) {
				if (dv_vf[i][1] &&
					dolby_vision_status == BYPASS_PROCESS &&
					!is_dolby_vision_on())
					dv_vf[i][1]->type |= VIDTYPE_VD2;
				return dv_vf[i][1];
			}
		}
	}
	return NULL;
}
EXPORT_SYMBOL(dolby_vision_vf_peek_el);

static void dolby_vision_vf_add(struct vframe_s *vf, struct vframe_s *el_vf)
{
	int i;

	for (i = 0; i < 16; i++) {
		if (!dv_vf[i][0]) {
			dv_vf[i][0] = vf;
			dv_vf[i][1] = el_vf;
			break;
		}
	}
}

static int dolby_vision_vf_check(struct vframe_s *vf)
{
	int i;

	for (i = 0; i < 16; i++) {
		if (dv_vf[i][0] == vf) {
			if (debug_dolby & 2) {
				if (dv_vf[i][1])
					pr_dolby_dbg("=== bl(%p-%lld) with el(%p-%lld) toggled ===\n",
								 vf,
								 vf->pts_us64,
								 dv_vf[i][1],
								 dv_vf[i][1]->pts_us64);
				else
					pr_dolby_dbg("=== bl(%p-%lld) toggled ===\n",
								 vf,
								 vf->pts_us64);
			}
			return 0;
		}
	}
	return 1;
}

/*return 0: parse ok; 1,2,3: parse err */
int parse_sei_and_meta_ext(struct vframe_s *vf,
						   char *aux_buf,
						   int aux_size,
						   int *total_comp_size,
						   int *total_md_size,
						   void *fmt,
						   int *ret_flags,
						   char *md_buf,
						   char *comp_buf)
{
	int i;
	char *p;
	unsigned int size = 0;
	unsigned int type = 0;
	int md_size = 0;
	int comp_size = 0;
	int parser_ready = 0;
	int ret = 2;
	unsigned long flags;
	bool parser_overflow = false;
	int rpu_ret = 0;
	int reset_flag = 0;
	unsigned int rpu_size = 0;
	enum signal_format_enum *src_format = (enum signal_format_enum *)fmt;
	static int parse_process_count;
	int dump_size = 100;
	static u32 last_play_id;

	if (!aux_buf || aux_size == 0 || !fmt || !md_buf || !comp_buf ||
		!total_comp_size || !total_md_size || !ret_flags)
		return 1;

	parse_process_count++;
	if (parse_process_count > 1) {
		pr_err("parser not support multi instance\n");
		ret = 1;
		goto parse_err;
	}

	/* release metadata_parser when new playing */
	if (vf && vf->src_fmt.play_id != last_play_id) {
		if (metadata_parser) {

			if (p_funcs_stb)
				p_funcs_stb->metadata_parser_release();

			metadata_parser = NULL;

			pr_dolby_dbg("new play, release parser\n");

			dolby_vision_clear_buf();
		}
		last_play_id = vf->src_fmt.play_id;
		if (debug_dolby & 2)
			pr_dolby_dbg("update play id=%d:\n", last_play_id);
	}

	p = aux_buf;
	while (p < aux_buf + aux_size - 8) {
		size = *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		type = *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		if (debug_dolby & 4)
			pr_dolby_dbg("metadata type=%08x, size=%d:\n",
						 type, size);
		if (size == 0 || size > aux_size) {
			pr_dolby_dbg("invalid aux size %d\n", size);
			ret = 1;
			goto parse_err;
		}
		if (type == DV_SEI || /* hevc t35 sei */
			(type & 0xffff0000) == DV_AV1_SEI) { /* av1 t35 obu */
			*total_comp_size = 0;
			*total_md_size = 0;

			if ((type & 0xffff0000) == DV_AV1_SEI &&
				p[0] == 0xb5 &&
				p[1] == 0x00 &&
				p[2] == 0x3b &&
				p[3] == 0x00 &&
				p[4] == 0x00 &&
				p[5] == 0x08 &&
				p[6] == 0x00 &&
				p[7] == 0x37 &&
				p[8] == 0xcd &&
				p[9] == 0x08) {
				/* AV1 dv meta in obu */
				*src_format = FORMAT_DOVI;
				meta_buf[0] = 0;
				meta_buf[1] = 0;
				meta_buf[2] = 0;
				meta_buf[3] = 0x01;
				meta_buf[4] = 0x19;
				if (p[11] & 0x10) {
					rpu_size = 0x100;
					rpu_size |= (p[11] & 0x0f) << 4;
					rpu_size |= (p[12] >> 4) & 0x0f;
					if (p[12] & 0x08) {
						pr_dolby_error
						("rpu exceed 512 bytes\n");
						break;
					}
					for (i = 0; i < rpu_size; i++) {
						meta_buf[5 + i] =
								(p[12 + i] & 0x07) << 5;
						meta_buf[5 + i] |=
								(p[13 + i] >> 3) & 0x1f;
					}
					rpu_size += 5;
				} else {
					rpu_size = (p[10] & 0x1f) << 3;
					rpu_size |= (p[11] >> 5) & 0x07;
					for (i = 0; i < rpu_size; i++) {
						meta_buf[5 + i] =
								(p[11 + i] & 0x0f) << 4;
						meta_buf[5 + i] |=
								(p[12 + i] >> 4) & 0x0f;
					}
					rpu_size += 5;
				}
			} else {
				/* HEVC dv meta in sei */
				*src_format = FORMAT_DOVI;
				if (size > (sizeof(meta_buf) - 3))
					size = (sizeof(meta_buf) - 3);
				meta_buf[0] = 0;
				meta_buf[1] = 0;
				meta_buf[2] = 0;
				memcpy(&meta_buf[3], p + 1, size - 1);
				rpu_size = size + 2;
			}
			if ((debug_dolby & 4) && dump_enable) {
				pr_dolby_dbg("metadata(%d):\n", rpu_size);
				for (i = 0; i < size; i += 16)
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
							meta_buf[i],
							meta_buf[i + 1],
							meta_buf[i + 2],
							meta_buf[i + 3],
							meta_buf[i + 4],
							meta_buf[i + 5],
							meta_buf[i + 6],
							meta_buf[i + 7],
							meta_buf[i + 8],
							meta_buf[i + 9],
							meta_buf[i + 10],
							meta_buf[i + 11],
							meta_buf[i + 12],
							meta_buf[i + 13],
							meta_buf[i + 14],
							meta_buf[i + 15]);
			}

			if (!p_funcs_stb) {
				ret = 1;
				goto parse_err;
			}

			/* prepare metadata parser */
			spin_lock_irqsave(&dovi_lock, flags);
			parser_ready = 0;
			if (!metadata_parser) {

				if (p_funcs_stb) {
					metadata_parser =
							p_funcs_stb->metadata_parser_init
									(dolby_vision_flags
									 & FLAG_CHANGE_SEQ_HEAD
									 ? 1 : 0);
					p_funcs_stb->metadata_parser_reset(1);
				} else {
					pr_dolby_dbg("p_funcs_stb is null\n");
				}

				if (metadata_parser) {
					parser_ready = 1;
					if (debug_dolby & 1)
						pr_dolby_dbg("metadata parser init OK\n");
				}
			} else {

				if (p_funcs_stb->metadata_parser_reset
						(metadata_parser_reset_flag) == 0)
					metadata_parser_reset_flag = 0;

				parser_ready = 1;
			}
			if (!parser_ready) {
				spin_unlock_irqrestore(&dovi_lock, flags);
				pr_dolby_error
				("meta(%d), pts(%lld) -> parser init fail\n",
				 size, vf->pts_us64);
				*total_comp_size = backup_comp_size;
				*total_md_size = backup_md_size;
				ret = 2;
				goto parse_err;
			}

			md_size = 0;
			comp_size = 0;

			rpu_ret =
					p_funcs_stb->metadata_parser_process
							(meta_buf, rpu_size,
							 comp_buf + *total_comp_size,
							 &comp_size,
							 md_buf + *total_md_size,
							 &md_size,
							 true);

			if (rpu_ret < 0) {
				pr_dolby_error
				("meta(%d), pts(%lld) -> parser process fail\n",
				 rpu_size, vf->pts_us64);
				if (vf)
					pr_dolby_error
					("meta(%d), pts(%lld) -> metadata parser process fail\n",
					 rpu_size, vf->pts_us64);
				ret = 3;
			} else {
				if (*total_comp_size + comp_size
					< COMP_BUF_SIZE)
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< MD_BUF_SIZE)
					*total_md_size += md_size;
				else
					parser_overflow = true;
				if (rpu_ret == 1)
					*ret_flags = 1;
				ret = 0;
			}
			spin_unlock_irqrestore(&dovi_lock, flags);
			if (parser_overflow) {
				ret = 2;
				break;
			}
			/*dolby type just appears once in metadata
			 *after parsing dolby type,breaking the
			 *circulation directly
			 */
			break;
		}
		p += size;
	}

	/*continue to check atsc/dvb dv - US Terestial Broadcast Standard? */
	if (atsc_sei && *src_format != FORMAT_DOVI) {
		struct dv_vui_parameters vui_param;
		static u32 len_2086_sei;
		u32 len_2094_sei = 0;
		static u8 payload_2086_sei[MAX_LEN_2086_SEI];
		u8 payload_2094_sei[MAX_LEN_2094_SEI];
		unsigned char nal_type;
		unsigned char sei_payload_type = 0;
		unsigned char sei_payload_size = 0;

		if (vf) {
			vui_param.video_fmt_i = (vf->signal_type >> 26) & 7;
			vui_param.video_fullrange_b = (vf->signal_type >> 25) & 1;
			vui_param.color_description_b = (vf->signal_type >> 24) & 1;
			vui_param.color_primaries_i = (vf->signal_type >> 16) & 0xff;
			vui_param.trans_characteristic_i =
					(vf->signal_type >> 8) & 0xff;
			vui_param.matrix_coeff_i = (vf->signal_type) & 0xff;
			if (debug_dolby & 2)
				pr_dolby_dbg("vui_param %d, %d, %d, %d, %d, %d\n",
							 vui_param.video_fmt_i,
							 vui_param.video_fullrange_b,
							 vui_param.color_description_b,
							 vui_param.color_primaries_i,
							 vui_param.trans_characteristic_i,
							 vui_param.matrix_coeff_i);
		}

		p = aux_buf;

		if ((debug_dolby & 0x200) && dump_enable) {
			pr_dolby_dbg("aux_buf(%d):\n", aux_size);
			for (i = 0; i < aux_size; i += 8)
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
						p[i],
						p[i + 1],
						p[i + 2],
						p[i + 3],
						p[i + 4],
						p[i + 5],
						p[i + 6],
						p[i + 7]);
		}
		while (p < aux_buf + aux_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (debug_dolby & 2)
				pr_dolby_dbg("type: 0x%x\n", type);

			/*4 byte size + 4 byte type*/
			/*1 byte nal_type + 1 byte (layer_id+temporal_id)*/
			/*1 byte payload type + 1 byte size + payload data*/
			if (type == 0x02000000) {
				nal_type = ((*p) & 0x7E) >> 1; /*nal unit type*/
				if (debug_dolby & 2)
					pr_dolby_dbg("nal_type: %d\n",
								 nal_type);

				if (nal_type == PREFIX_SEI_NUT_NAL ||
					nal_type == SUFFIX_SEI_NUT_NAL) {
					sei_payload_type = *(p + 2);
					sei_payload_size = *(p + 3);
					if (debug_dolby & 2)
						pr_dolby_dbg("type %d, size %d\n",
									 sei_payload_type,
									 sei_payload_size);
					if (sei_payload_type ==
						SEI_TYPE_MASTERING_DISP_COLOUR_VOLUME) {
						len_2086_sei =
								sei_payload_size;
						memcpy(payload_2086_sei, p + 4,
							   len_2086_sei);
					} else if (sei_payload_type ==
							   SEI_TYPE_USERDATA_REGISTERED_ITUT_T35) {
						len_2094_sei =
								sei_payload_size;
						memcpy(payload_2094_sei, p + 4,
							   len_2094_sei);
					}
					if (len_2086_sei > 0 &&
						len_2094_sei > 0)
						break;
				}
			}
			p += size;
		}
		if (len_2094_sei > 0) {
			/* source is VS10 */
			*total_comp_size = 0;
			*total_md_size = 0;
			*src_format = FORMAT_DOVI;
			if (vf)
				p_atsc_md.vui_param = vui_param;
			p_atsc_md.len_2086_sei = len_2086_sei;
			memcpy(p_atsc_md.sei_2086, payload_2086_sei,
				   len_2086_sei);
			p_atsc_md.len_2094_sei = len_2094_sei;
			memcpy(p_atsc_md.sei_2094, payload_2094_sei,
				   len_2094_sei);
			size = sizeof(struct dv_atsc);
			if (size > sizeof(meta_buf))
				size = sizeof(meta_buf);
			memcpy(meta_buf, (unsigned char *)(&p_atsc_md), size);
			if ((debug_dolby & 4) && dump_enable) {
				pr_dolby_dbg("metadata(%d):\n", size);
				for (i = 0; i < size; i += 8)
					pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
							meta_buf[i],
							meta_buf[i + 1],
							meta_buf[i + 2],
							meta_buf[i + 3],
							meta_buf[i + 4],
							meta_buf[i + 5],
							meta_buf[i + 6],
							meta_buf[i + 7]);
			}

			if (!p_funcs_stb) {
				ret = 1;
				goto parse_err;
			}

			/* prepare metadata parser */
			spin_lock_irqsave(&dovi_lock, flags);
			parser_ready = 0;
			reset_flag = 2; /*flag: bit0 flag, bit1 0->dv, 1->atsc*/

			if (!metadata_parser) {

				metadata_parser =
						p_funcs_stb->metadata_parser_init
								(dolby_vision_flags
								 & FLAG_CHANGE_SEQ_HEAD
								 ? 1 : 0);
				p_funcs_stb->metadata_parser_reset
						(reset_flag | 1);

				if (metadata_parser) {
					parser_ready = 1;
					if (debug_dolby & 1)
						pr_dolby_dbg("metadata parser init OK\n");
				}
			} else {

				if (p_funcs_stb->metadata_parser_reset
						(reset_flag |
						 metadata_parser_reset_flag
						) == 0)
					metadata_parser_reset_flag = 0;

				parser_ready = 1;
			}
			if (!parser_ready) {
				spin_unlock_irqrestore(&dovi_lock, flags);
				pr_dolby_error
				("meta(%d), pts(%lld) -> parser init fail\n",
				 size, vf->pts_us64);
				if (vf)
					pr_dolby_error
					("meta(%d), pts(%lld) -> metadata parser init fail\n",
					 size, vf->pts_us64);
				*total_comp_size = backup_comp_size;
				*total_md_size = backup_md_size;
				ret = 2;
				goto parse_err;
			}

			md_size = 0;
			comp_size = 0;

			rpu_ret =
					p_funcs_stb->metadata_parser_process
							(meta_buf, size,
							 comp_buf + *total_comp_size,
							 &comp_size,
							 md_buf + *total_md_size,
							 &md_size,
							 true);

			if (rpu_ret < 0) {
				pr_dolby_error
				("meta(%d), pts(%lld)->parser process fail\n",
				 size, vf->pts_us64);
				if (vf)
					pr_dolby_error
					("meta(%d), pts(%lld) -> metadata parser process fail\n",
					 size, vf->pts_us64);
				ret = 3;
			} else {
				if (*total_comp_size + comp_size
					< COMP_BUF_SIZE)
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< MD_BUF_SIZE)
					*total_md_size += md_size;
				else
					parser_overflow = true;
				if (rpu_ret == 1)
					*ret_flags = 1;
				ret = 0;
			}
			spin_unlock_irqrestore(&dovi_lock, flags);
			if (parser_overflow)
				ret = 2;
		} else {
			len_2086_sei = 0;
		}
	}

	if (*total_md_size) {
		if ((debug_dolby & 1) && vf)
			pr_dolby_dbg
			("meta(%d), pts(%lld) -> md(%d), comp(%d)\n",
			 size, vf->pts_us64,
			 *total_md_size, *total_comp_size);
		if ((debug_dolby & 4) && dump_enable)  {
			pr_dolby_dbg("parsed md(%d):\n", *total_md_size);
			for (i = 0; i < *total_md_size + 7; i += 8) {
				pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						md_buf[i],
						md_buf[i + 1],
						md_buf[i + 2],
						md_buf[i + 3],
						md_buf[i + 4],
						md_buf[i + 5],
						md_buf[i + 6],
						md_buf[i + 7]);
			}
			pr_dolby_dbg("parsed comp(%d):\n", *total_comp_size);
			if (*total_comp_size < dump_size)
				dump_size = *total_comp_size;
			for (i = 0; i < dump_size + 7; i += 8)
				pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						comp_buf[i],
						comp_buf[i + 1],
						comp_buf[i + 2],
						comp_buf[i + 3],
						comp_buf[i + 4],
						comp_buf[i + 5],
						comp_buf[i + 6],
						comp_buf[i + 7]);
		}
	}
	parse_err:
	parse_process_count--;
	return ret;
}
EXPORT_SYMBOL(parse_sei_and_meta_ext);

static int parse_sei_and_meta
		(struct vframe_s *vf,
		 struct provider_aux_req_s *req,
		 int *total_comp_size,
		 int *total_md_size,
		 enum signal_format_enum *src_format,
		 int *ret_flags, bool drop_flag)
{
	int ret = 2;
	char *p_md_buf;
	char *p_comp_buf;
	int next_id = current_id ^ 1;

	if (!req->aux_buf || req->aux_size == 0)
		return 1;

	if (drop_flag) {
		p_md_buf = drop_md_buf[next_id];
		p_comp_buf = drop_comp_buf[next_id];
	} else {
		p_md_buf = md_buf[next_id];
		p_comp_buf = comp_buf[next_id];
	}
	ret = parse_sei_and_meta_ext(vf,
								 req->aux_buf,
								 req->aux_size,
								 total_comp_size,
								 total_md_size,
								 src_format,
								 ret_flags,
								 p_md_buf,
								 p_comp_buf);

	if (ret == 0) {
		current_id = next_id;
		backup_comp_size = *total_comp_size;
		backup_md_size = *total_md_size;
	}
	if (ret == 3) {
		*total_comp_size = backup_comp_size;
		*total_md_size = backup_md_size;
	}
	return ret;
}

#define INORM	50000
static u32 bt2020_primaries[3][2] = {
		{0.17 * INORM + 0.5, 0.797 * INORM + 0.5},	/* G */
		{0.131 * INORM + 0.5, 0.046 * INORM + 0.5},	/* B */
		{0.708 * INORM + 0.5, 0.292 * INORM + 0.5},	/* R */
};

static u32 p3_primaries[3][2] = {
		{0.265 * INORM + 0.5, 0.69 * INORM + 0.5},	/* G */
		{0.15 * INORM + 0.5, 0.06 * INORM + 0.5},	/* B */
		{0.68 * INORM + 0.5, 0.32 * INORM + 0.5},	/* R */
};

static u32 bt2020_white_point[2] = {
		0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

static u32 p3_white_point[2] = {
		0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

void prepare_hdr10_param(struct vframe_master_display_colour_s *p_mdc,
						 struct hdr10_parameter *p_hdr10_param)
{
	struct vframe_content_light_level_s *p_cll =
			&p_mdc->content_light_level;
	u8 flag = 0;
	u32 max_lum = 1000 * 10000;
	u32 min_lum = 50;
	int primaries_type = 0;

	if (get_primary_policy() == PRIMARIES_NATIVE ||
		primary_debug == 1 ||
		(dolby_vision_flags & FLAG_CERTIFICAION) ||
		!strcasecmp(cfg_info[cur_pic_mode].pic_mode_name,
					"hdr10_dark")) {
		p_hdr10_param->min_display_mastering_lum =
				min_lum;
		p_hdr10_param->max_display_mastering_lum =
				max_lum;
		p_hdr10_param->r_x = bt2020_primaries[2][0];
		p_hdr10_param->r_y = bt2020_primaries[2][1];
		p_hdr10_param->g_x = bt2020_primaries[0][0];
		p_hdr10_param->g_y = bt2020_primaries[0][1];
		p_hdr10_param->b_x = bt2020_primaries[1][0];
		p_hdr10_param->b_y = bt2020_primaries[1][1];
		p_hdr10_param->w_x = bt2020_white_point[0];
		p_hdr10_param->w_y = bt2020_white_point[1];
		p_hdr10_param->max_content_light_level = 0;
		p_hdr10_param->max_frame_avg_light_level = 0;
		return;
	} else if (get_primary_policy() == PRIMARIES_AUTO ||
			   primary_debug == 2) {
		p_hdr10_param->min_display_mastering_lum = min_lum;
		p_hdr10_param->max_display_mastering_lum = max_lum;
		p_hdr10_param->r_x = p3_primaries[2][0];
		p_hdr10_param->r_y = p3_primaries[2][1];
		p_hdr10_param->g_x = p3_primaries[0][0];
		p_hdr10_param->g_y = p3_primaries[0][1];
		p_hdr10_param->b_x = p3_primaries[1][0];
		p_hdr10_param->b_y = p3_primaries[1][1];
		p_hdr10_param->w_x = p3_white_point[0];
		p_hdr10_param->w_y = p3_white_point[1];
		p_hdr10_param->max_content_light_level = 0;
		p_hdr10_param->max_frame_avg_light_level = 0;
		return;
	}

	primaries_type = get_primaries_type(p_mdc);
	if (primaries_type == 2) {
		/* GBR -> RGB as dolby will swap back to GBR
		 * in send_hdmi_pkt
		 */
		if (p_hdr10_param->max_display_mastering_lum !=
			p_mdc->luminance[0] ||
			p_hdr10_param->min_display_mastering_lum !=
			p_mdc->luminance[1] ||
			p_hdr10_param->r_x != p_mdc->primaries[2][0] ||
			p_hdr10_param->r_y != p_mdc->primaries[2][1] ||
			p_hdr10_param->g_x != p_mdc->primaries[0][0] ||
			p_hdr10_param->g_y != p_mdc->primaries[0][1] ||
			p_hdr10_param->b_x != p_mdc->primaries[1][0] ||
			p_hdr10_param->b_y != p_mdc->primaries[1][1] ||
			p_hdr10_param->w_x != p_mdc->white_point[0] ||
			p_hdr10_param->w_y != p_mdc->white_point[1]) {
			flag |= 1;
			p_hdr10_param->max_display_mastering_lum =
					p_mdc->luminance[0];
			p_hdr10_param->min_display_mastering_lum =
					p_mdc->luminance[1];
			p_hdr10_param->r_x = p_mdc->primaries[2][0];
			p_hdr10_param->r_y = p_mdc->primaries[2][1];
			p_hdr10_param->g_x = p_mdc->primaries[0][0];
			p_hdr10_param->g_y = p_mdc->primaries[0][1];
			p_hdr10_param->b_x = p_mdc->primaries[1][0];
			p_hdr10_param->b_y = p_mdc->primaries[1][1];
			p_hdr10_param->w_x = p_mdc->white_point[0];
			p_hdr10_param->w_y = p_mdc->white_point[1];
		}
	} else if (primaries_type == 1) {
		/* RGB -> RGB and dolby will swap to send as GBR
		 * in send_hdmi_pkt
		 */
		if (p_hdr10_param->max_display_mastering_lum !=
			p_mdc->luminance[0] ||
			p_hdr10_param->min_display_mastering_lum !=
			p_mdc->luminance[1] ||
			p_hdr10_param->r_x != p_mdc->primaries[0][0] ||
			p_hdr10_param->r_y != p_mdc->primaries[0][1] ||
			p_hdr10_param->g_x != p_mdc->primaries[1][0] ||
			p_hdr10_param->g_y != p_mdc->primaries[1][1] ||
			p_hdr10_param->b_x != p_mdc->primaries[2][0] ||
			p_hdr10_param->b_y != p_mdc->primaries[2][1] ||
			p_hdr10_param->w_x != p_mdc->white_point[0] ||
			p_hdr10_param->w_y != p_mdc->white_point[1]) {
			flag |= 1;
			p_hdr10_param->max_display_mastering_lum =
					p_mdc->luminance[0];
			p_hdr10_param->min_display_mastering_lum =
					p_mdc->luminance[1];
			p_hdr10_param->r_x = p_mdc->primaries[0][0];
			p_hdr10_param->r_y = p_mdc->primaries[0][1];
			p_hdr10_param->g_x = p_mdc->primaries[1][0];
			p_hdr10_param->g_y = p_mdc->primaries[1][1];
			p_hdr10_param->b_x = p_mdc->primaries[2][0];
			p_hdr10_param->b_y = p_mdc->primaries[2][1];
			p_hdr10_param->w_x = p_mdc->white_point[0];
			p_hdr10_param->w_y = p_mdc->white_point[1];
		}
	} else {
		/* GBR -> RGB as dolby will swap back to GBR
		 * in send_hdmi_pkt
		 */
		if (p_hdr10_param->min_display_mastering_lum !=
			min_lum ||
			p_hdr10_param->max_display_mastering_lum !=
			max_lum ||
			p_hdr10_param->r_x != bt2020_primaries[2][0] ||
			p_hdr10_param->r_y != bt2020_primaries[2][1] ||
			p_hdr10_param->g_x != bt2020_primaries[0][0] ||
			p_hdr10_param->g_y != bt2020_primaries[0][1] ||
			p_hdr10_param->b_x != bt2020_primaries[1][0] ||
			p_hdr10_param->b_y != bt2020_primaries[1][1] ||
			p_hdr10_param->w_x != bt2020_white_point[0] ||
			p_hdr10_param->w_y != bt2020_white_point[1]) {
			flag |= 2;
			p_hdr10_param->min_display_mastering_lum =
					min_lum;
			p_hdr10_param->max_display_mastering_lum =
					max_lum;
			p_hdr10_param->r_x = bt2020_primaries[2][0];
			p_hdr10_param->r_y = bt2020_primaries[2][1];
			p_hdr10_param->g_x = bt2020_primaries[0][0];
			p_hdr10_param->g_y = bt2020_primaries[0][1];
			p_hdr10_param->b_x = bt2020_primaries[1][0];
			p_hdr10_param->b_y = bt2020_primaries[1][1];
			p_hdr10_param->w_x = bt2020_white_point[0];
			p_hdr10_param->w_y = bt2020_white_point[1];
		}
	}

	if (p_cll->present_flag) {
		if (p_hdr10_param->max_content_light_level
			!= p_cll->max_content ||
			p_hdr10_param->max_frame_avg_light_level
			!= p_cll->max_pic_average)
			flag |= 4;
		if (flag & 4) {
			p_hdr10_param->max_content_light_level =
					p_cll->max_content;
			p_hdr10_param->max_frame_avg_light_level =
					p_cll->max_pic_average;
		}
	} else {
		if (p_hdr10_param->max_content_light_level != 0 ||
			p_hdr10_param->max_frame_avg_light_level != 0) {
			p_hdr10_param->max_content_light_level = 0;
			p_hdr10_param->max_frame_avg_light_level = 0;
			flag |= 8;
		}
	}

	if (flag && (debug_dolby & 1)) {
		pr_dolby_dbg("HDR10: present %d, %d, %d, %d\n", p_mdc->present_flag, p_cll->max_content, flag, primaries_type);
		pr_dolby_dbg("\tR = %04x, %04x\n", p_hdr10_param->r_x, p_hdr10_param->r_y);
		pr_dolby_dbg("\tG = %04x, %04x\n", p_hdr10_param->g_x, p_hdr10_param->g_y);
		pr_dolby_dbg("\tB = %04x, %04x\n", p_hdr10_param->b_x, p_hdr10_param->b_y);
		pr_dolby_dbg("\tW = %04x, %04x\n", p_hdr10_param->w_x, p_hdr10_param->w_y);
		pr_dolby_dbg("\tMax = %d\n", p_hdr10_param->max_display_mastering_lum);
		pr_dolby_dbg("\tMin = %d\n", p_hdr10_param->min_display_mastering_lum);
		pr_dolby_dbg("\tMCLL = %d\n", p_hdr10_param->max_content_light_level);
		pr_dolby_dbg("\tMPALL = %d\n\n", p_hdr10_param->max_frame_avg_light_level);
	}
}

static int prepare_vsif_pkt
		(struct dv_vsif_para *vsif,
		 struct dovi_setting_s *setting,
		 const struct vinfo_s *vinfo,
		 enum signal_format_enum src_format)
{
	if (!vsif || !vinfo || !setting || !vinfo->vout_device || !vinfo->vout_device->dv_info)
		return -1;
	
	vsif->vers.ver2.low_latency = setting->dovi_ll_enable;
	
	if (src_format == FORMAT_DOVI || src_format == FORMAT_DOVI_LL)
		vsif->vers.ver2.dobly_vision_signal = 1;/*0b0001*/
	else if (src_format == FORMAT_HDR10)
		vsif->vers.ver2.dobly_vision_signal = 3;/*0b0011*/
	else if (src_format == FORMAT_HLG)
		vsif->vers.ver2.dobly_vision_signal = 7;/*0b0111*/
	else if (src_format == FORMAT_SDR || src_format == FORMAT_SDR_2020)
		vsif->vers.ver2.dobly_vision_signal = 5;/*0b0101*/
	
	if ((debug_dolby & 2))
		pr_dolby_dbg("src %d, dobly_vision_signal %d\n", src_format, vsif->vers.ver2.dobly_vision_signal);

	if (vinfo->vout_device->dv_info &&
		vinfo->vout_device->dv_info->sup_backlight_control &&
		(setting->ext_md.avail_level_mask & EXT_MD_LEVEL_2)) {
		vsif->vers.ver2.backlt_ctrl_MD_present = 1;
		vsif->vers.ver2.eff_tmax_PQ_hi =
				setting->ext_md.level_2.target_max_pq_h & 0xf;
		vsif->vers.ver2.eff_tmax_PQ_low =
				setting->ext_md.level_2.target_max_pq_l;
	} else {
		vsif->vers.ver2.backlt_ctrl_MD_present = 0;
		vsif->vers.ver2.eff_tmax_PQ_hi = 0;
		vsif->vers.ver2.eff_tmax_PQ_low = 0;
	}

	if (setting->dovi_ll_enable &&
		(setting->ext_md.avail_level_mask
		 & EXT_MD_LEVEL_255)) {
		vsif->vers.ver2.auxiliary_MD_present = 1;
		vsif->vers.ver2.auxiliary_runmode =
				setting->ext_md.level_255.run_mode;
		vsif->vers.ver2.auxiliary_runversion =
				setting->ext_md.level_255.run_version;
		vsif->vers.ver2.auxiliary_debug0 =
				setting->ext_md.level_255.dm_debug_0;
	} else {
		vsif->vers.ver2.auxiliary_MD_present = 0;
		vsif->vers.ver2.auxiliary_runmode = 0;
		vsif->vers.ver2.auxiliary_runversion = 0;
		vsif->vers.ver2.auxiliary_debug0 = 0;
	}
	return 0;
}

unsigned char vsif_emp[32];
static int prepare_emp_vsif_pkt
		(unsigned char *vsif_PB,
		 struct dovi_setting_s *setting,
		 const struct vinfo_s *vinfo)
{
	if (!vinfo || !setting || !vinfo->vout_device || !vinfo->vout_device->dv_info)
		return -1;

	memset(vsif_PB, 0, 32);

	/* low_latency */
	if (setting->dovi_ll_enable)
		vsif_PB[0] = 1 << 0;

	/* dovi_signal_type */
	vsif_PB[0] |= 1 << 1;

	/* source_dm_version */
	vsif_PB[0] |= 3 << 5;

	if (vinfo->vout_device->dv_info->sup_backlight_control &&
		(setting->ext_md.avail_level_mask & EXT_MD_LEVEL_2)) {
		vsif_PB[1] |= 1 << 7;
		vsif_PB[1] |= setting->ext_md.level_2.target_max_pq_h & 0xf;
		vsif_PB[2] = setting->ext_md.level_2.target_max_pq_l;
	}

	if (setting->dovi_ll_enable && (setting->ext_md.avail_level_mask & EXT_MD_LEVEL_255)) {
		vsif_PB[1] |= 1 << 6;
		vsif_PB[3] = setting->ext_md.level_255.run_mode;
		vsif_PB[4] = setting->ext_md.level_255.run_version;
		vsif_PB[5] = setting->ext_md.level_255.dm_debug_0;
	}
	return 0;
}

static int notify_vd_signal_to_amvideo(struct vd_signal_info_s *vd_signal)
{
	static int pre_signal = -1;

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	if (pre_signal != vd_signal->signal_type) {
		vd_signal->vd1_signal_type = SIGNAL_DOVI;
		vd_signal->vd2_signal_type = SIGNAL_DOVI;
		amvideo_notifier_call_chain(AMVIDEO_UPDATE_SIGNAL_MODE, (void *)vd_signal);
	}
#endif

	pre_signal = vd_signal->signal_type;
	return 0;
}

/* #define HDMI_SEND_ALL_PKT */
static u32 last_dst_format = FORMAT_SDR;
static bool send_hdmi_pkt
		(enum signal_format_enum src_format,
		 enum signal_format_enum dst_format,
		 const struct vinfo_s *vinfo, struct vframe_s *vf)
{
	struct hdr10_infoframe *p_hdr;
	int i;
	bool flag = false;
	static int sdr_transition_delay;
	struct vd_signal_info_s vd_signal;

	if ((debug_dolby & 2))
		pr_dolby_dbg("[%s]src_format %d, dst %d, last %d:\n",
					 __func__, src_format, dst_format, last_dst_format);

	if (dst_format == FORMAT_HDR10) {
		sdr_transition_delay = 0;
		p_hdr = &dovi_setting.hdr_info;
		hdr10_data.features =
				(1 << 29)	/* video available */
				| (5 << 26)	/* unspecified */
				| (0 << 25)	/* limit */
				| (1 << 24)	/* color available */
				| (9 << 16)	/* bt2020 */
				| (0x10 << 8)	/* bt2020-10 */
				| (10 << 0);/* bt2020c */
		if (src_format != FORMAT_HDR10PLUS) {
			/* keep as r,g,b when src not HDR+ */
			if (hdr10_data.primaries[0][0] !=
				((p_hdr->primaries_x_0_msb << 8)
				 | p_hdr->primaries_x_0_lsb))
				flag = true;
			hdr10_data.primaries[0][0] =
					(p_hdr->primaries_x_0_msb << 8)
					| p_hdr->primaries_x_0_lsb;

			if (hdr10_data.primaries[0][1] !=
				((p_hdr->primaries_y_0_msb << 8)
				 | p_hdr->primaries_y_0_lsb))
				flag = true;
			hdr10_data.primaries[0][1] =
					(p_hdr->primaries_y_0_msb << 8)
					| p_hdr->primaries_y_0_lsb;

			if (hdr10_data.primaries[1][0] !=
				((p_hdr->primaries_x_1_msb << 8)
				 | p_hdr->primaries_x_1_lsb))
				flag = true;
			hdr10_data.primaries[1][0] =
					(p_hdr->primaries_x_1_msb << 8)
					| p_hdr->primaries_x_1_lsb;

			if (hdr10_data.primaries[1][1] !=
				((p_hdr->primaries_y_1_msb << 8)
				 | p_hdr->primaries_y_1_lsb))
				flag = true;
			hdr10_data.primaries[1][1] =
					(p_hdr->primaries_y_1_msb << 8)
					| p_hdr->primaries_y_1_lsb;
			if (hdr10_data.primaries[2][0] !=
				((p_hdr->primaries_x_2_msb << 8)
				 | p_hdr->primaries_x_2_lsb))
				flag = true;
			hdr10_data.primaries[2][0] =
					(p_hdr->primaries_x_2_msb << 8)
					| p_hdr->primaries_x_2_lsb;

			if (hdr10_data.primaries[2][1] !=
				((p_hdr->primaries_y_2_msb << 8)
				 | p_hdr->primaries_y_2_lsb))
				flag = true;
			hdr10_data.primaries[2][1] =
					(p_hdr->primaries_y_2_msb << 8)
					| p_hdr->primaries_y_2_lsb;
		} else {
			/* need invert to g,b,r */
			if (hdr10_data.primaries[0][0] !=
				((p_hdr->primaries_x_1_msb << 8)
				 | p_hdr->primaries_x_1_lsb))
				flag = true;
			hdr10_data.primaries[0][0] =
					(p_hdr->primaries_x_1_msb << 8)
					| p_hdr->primaries_x_1_lsb;

			if (hdr10_data.primaries[0][1] !=
				((p_hdr->primaries_y_1_msb << 8)
				 | p_hdr->primaries_y_1_lsb))
				flag = true;
			hdr10_data.primaries[0][1] =
					(p_hdr->primaries_y_1_msb << 8)
					| p_hdr->primaries_y_1_lsb;

			if (hdr10_data.primaries[1][0] !=
				((p_hdr->primaries_x_2_msb << 8)
				 | p_hdr->primaries_x_2_lsb))
				flag = true;
			hdr10_data.primaries[1][0] =
					(p_hdr->primaries_x_2_msb << 8)
					| p_hdr->primaries_x_2_lsb;

			if (hdr10_data.primaries[1][1] !=
				((p_hdr->primaries_y_2_msb << 8)
				 | p_hdr->primaries_y_2_lsb))
				flag = true;
			hdr10_data.primaries[1][1] =
					(p_hdr->primaries_y_2_msb << 8)
					| p_hdr->primaries_y_2_lsb;
			if (hdr10_data.primaries[2][0] !=
				((p_hdr->primaries_x_0_msb << 8)
				 | p_hdr->primaries_x_0_lsb))
				flag = true;
			hdr10_data.primaries[2][0] =
					(p_hdr->primaries_x_0_msb << 8)
					| p_hdr->primaries_x_0_lsb;

			if (hdr10_data.primaries[2][1] !=
				((p_hdr->primaries_y_0_msb << 8)
				 | p_hdr->primaries_y_0_lsb))
				flag = true;
			hdr10_data.primaries[2][1] =
					(p_hdr->primaries_y_0_msb << 8)
					| p_hdr->primaries_y_0_lsb;
		}

		if (hdr10_data.white_point[0] !=
			((p_hdr->white_point_x_msb << 8)
			 | p_hdr->white_point_x_lsb))
			flag = true;
		hdr10_data.white_point[0] =
				(p_hdr->white_point_x_msb << 8)
				| p_hdr->white_point_x_lsb;

		if (hdr10_data.white_point[1] !=
			((p_hdr->white_point_y_msb << 8)
			 | p_hdr->white_point_y_lsb))
			flag = true;
		hdr10_data.white_point[1] =
				(p_hdr->white_point_y_msb << 8)
				| p_hdr->white_point_y_lsb;

		if (hdr10_data.luminance[0] !=
			((p_hdr->max_display_mastering_lum_msb << 8)
			 | p_hdr->max_display_mastering_lum_lsb))
			flag = true;
		hdr10_data.luminance[0] =
				(p_hdr->max_display_mastering_lum_msb << 8)
				| p_hdr->max_display_mastering_lum_lsb;

		if (hdr10_data.luminance[1] !=
			((p_hdr->min_display_mastering_lum_msb << 8)
			 | p_hdr->min_display_mastering_lum_lsb))
			flag = true;
		hdr10_data.luminance[1] =
				(p_hdr->min_display_mastering_lum_msb << 8)
				| p_hdr->min_display_mastering_lum_lsb;

		if (hdr10_data.max_content !=
			((p_hdr->max_content_light_level_msb << 8)
			 | p_hdr->max_content_light_level_lsb))
			flag = true;
		hdr10_data.max_content =
				(p_hdr->max_content_light_level_msb << 8)
				| p_hdr->max_content_light_level_lsb;

		if (hdr10_data.max_frame_average !=
			((p_hdr->max_frame_avg_light_level_msb << 8)
			 | p_hdr->max_frame_avg_light_level_lsb))
			flag = true;
		hdr10_data.max_frame_average =
				(p_hdr->max_frame_avg_light_level_msb << 8)
				| p_hdr->max_frame_avg_light_level_lsb;

		if (vinfo && vinfo->vout_device &&
			vinfo->vout_device->fresh_tx_hdr_pkt)
			vinfo->vout_device->fresh_tx_hdr_pkt(&hdr10_data);

#ifdef HDMI_SEND_ALL_PKT
		if (vinfo && vinfo->vout_device &&
			vinfo->vout_device->fresh_tx_vsif_pkt) {

			vinfo->vout_device->fresh_tx_vsif_pkt
				(0, 0, NULL, true);		}
#endif

		if (last_dst_format != FORMAT_HDR10 ||
			(dolby_vision_flags & FLAG_FORCE_HDMI_PKT))
			pr_dolby_dbg("send hdmi pkt: HDR10\n");

		last_dst_format = dst_format;
		vd_signal.signal_type = SIGNAL_HDR10;
		notify_vd_signal_to_amvideo(&vd_signal);
		if (flag && debug_dolby & 8) {
			pr_dolby_dbg("Info frame for hdr10 changed:\n");
			for (i = 0; i < 3; i++)
				pr_dolby_dbg("\tprimaries[%1d] = %04x, %04x\n",
							 i,
							 hdr10_data.primaries[i][0],
							 hdr10_data.primaries[i][1]);
			pr_dolby_dbg("\twhite_point = %04x, %04x\n",
						 hdr10_data.white_point[0],
						 hdr10_data.white_point[1]);
			pr_dolby_dbg("\tMax = %d\n",
						 hdr10_data.luminance[0]);
			pr_dolby_dbg("\tMin = %d\n",
						 hdr10_data.luminance[1]);
			pr_dolby_dbg("\tMCLL = %d\n",
						 hdr10_data.max_content);
			pr_dolby_dbg("\tMPALL = %d\n\n",
						 hdr10_data.max_frame_average);
		}
	} else if (dst_format == FORMAT_DOVI) {
		struct dv_vsif_para vsif;

		sdr_transition_delay = 0;
		memset(&vsif, 0, sizeof(vsif));
		if (vinfo) {
			prepare_vsif_pkt(&vsif, &dovi_setting, vinfo, src_format);
			prepare_emp_vsif_pkt(vsif_emp, &dovi_setting, vinfo);
		}

#ifdef HDMI_SEND_ALL_PKT
		hdr10_data.features =
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/* color available */
			| (1 << 16)	/* bt709 */
			| (1 << 8)	/* bt709 */
			| (1 << 0);	/* bt709 */

		for (i = 0; i < 3; i++) {
			hdr10_data.primaries[i][0] = 0;
			hdr10_data.primaries[i][1] = 0;
		}

		hdr10_data.white_point[0] = 0;
		hdr10_data.white_point[1] = 0;
		hdr10_data.luminance[0] = 0;
		hdr10_data.luminance[1] = 0;
		hdr10_data.max_content = 0;
		hdr10_data.max_frame_average = 0;

		if (vinfo && vinfo->vout_device &&
		    vinfo->vout_device->fresh_tx_hdr_pkt)
			vinfo->vout_device->fresh_tx_hdr_pkt(&hdr10_data);
#endif

		if (vinfo && vinfo->vout_device &&
			vinfo->vout_device->fresh_tx_vsif_pkt) {

			if (dovi_setting.dovi_ll_enable) {

				vinfo->vout_device->fresh_tx_vsif_pkt(
						EOTF_T_LL_MODE,
						dovi_setting.diagnostic_enable
						? RGB_10_12BIT : YUV422_BIT12,
						&vsif, false);

			} else {

				vinfo->vout_device->fresh_tx_vsif_pkt(
						EOTF_T_DOLBYVISION,
						dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL
						? RGB_8BIT : YUV422_BIT12,
						&vsif, false);
			}

		}
		if (last_dst_format != FORMAT_DOVI ||
			(dolby_vision_flags & FLAG_FORCE_HDMI_PKT))
			pr_dolby_dbg("send hdmi pkt: %s\n", dovi_setting.dovi_ll_enable ? "LL" : "DV");

		last_dst_format = dst_format;
		vd_signal.signal_type = SIGNAL_DOVI;
		notify_vd_signal_to_amvideo(&vd_signal);

	} else if (last_dst_format != dst_format) {

		if (last_dst_format == FORMAT_HDR10) {
			sdr_transition_delay = 0;
			if (!(vf && (is_hlg_frame(vf) ||
						 is_hdr10plus_frame(vf)))) {
				hdr10_data.features =
						(1 << 29)	/* video available */
						| (5 << 26)	/* unspecified */
						| (0 << 25)	/* limit */
						| (1 << 24)	/* color available */
						| (1 << 16)	/* bt709 */
						| (1 << 8)	/* bt709 */
						| (1 << 0);	/* bt709 */
				for (i = 0; i < 3; i++) {
					hdr10_data.primaries[i][0] = 0;
					hdr10_data.primaries[i][1] = 0;
				}
				hdr10_data.white_point[0] = 0;
				hdr10_data.white_point[1] = 0;
				hdr10_data.luminance[0] = 0;
				hdr10_data.luminance[1] = 0;
				hdr10_data.max_content = 0;
				hdr10_data.max_frame_average = 0;
				if (vinfo && vinfo->vout_device &&
					vinfo->vout_device->fresh_tx_hdr_pkt) {
					vinfo->vout_device->fresh_tx_hdr_pkt
							(&hdr10_data);
					last_dst_format = dst_format;
				}
			}
		} else if (last_dst_format == FORMAT_DOVI) {
			if (vinfo && vinfo->vout_device &&
				vinfo->vout_device->fresh_tx_vsif_pkt) {
				if (vf && (is_hlg_frame(vf) ||
						   is_hdr10plus_frame(vf) ||
						   is_cuva_frame(vf))) {
					/* HLG/HDR10+/cuva case: first
					 * switch to SDR
					 * immediately.
					 */
					pr_dolby_dbg
					("send pkt: HDR10+/HLG: signal SDR first\n");

					vinfo->vout_device->fresh_tx_vsif_pkt
							(0, 0, NULL, true);

					last_dst_format = dst_format;
					sdr_transition_delay = 0;
				} else if (sdr_transition_delay >=
						   MAX_TRANSITION_DELAY) {
					pr_dolby_dbg
					("send pkt: VSIF disabled, signal SDR\n");

					vinfo->vout_device->fresh_tx_vsif_pkt
							(0, 0, NULL, true);

					last_dst_format = dst_format;
					sdr_transition_delay = 0;
				} else {
					if (sdr_transition_delay == 0) {
						pr_dolby_dbg("send pkt: disable Dovi/H14b VSIF\n");

						vinfo->vout_device->fresh_tx_vsif_pkt
								(0, 0, NULL, false);

					}
					sdr_transition_delay++;
				}
			}
		}
		vd_signal.signal_type = SIGNAL_SDR;
		notify_vd_signal_to_amvideo(&vd_signal);
	}
	if (dolby_vision_flags & FLAG_FORCE_HDMI_PKT)
		dolby_vision_flags &= ~FLAG_FORCE_HDMI_PKT;
	return flag;
}

static void send_hdmi_pkt_ahead
		(enum signal_format_enum dst_format,
		 const struct vinfo_s *vinfo)
{
	bool dovi_ll_enable = false;
	bool diagnostic_enable = false;

	if ((dolby_vision_flags & FLAG_FORCE_DOVI_LL) ||
		dolby_vision_ll_policy == DOLBY_VISION_LL_YUV422 ||
		dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444) {
		dovi_ll_enable = true;
		if (dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444)
			diagnostic_enable = true;
	}

	if (dst_format == FORMAT_DOVI) {
		struct dv_vsif_para vsif;

		memset(&vsif, 0, sizeof(vsif));
		vsif.vers.ver2.low_latency = dovi_ll_enable;
		vsif.vers.ver2.dobly_vision_signal = 1;
		vsif.vers.ver2.backlt_ctrl_MD_present = 0;
		vsif.vers.ver2.eff_tmax_PQ_hi = 0;
		vsif.vers.ver2.eff_tmax_PQ_low = 0;
		vsif.vers.ver2.auxiliary_MD_present = 0;
		vsif.vers.ver2.auxiliary_runmode = 0;
		vsif.vers.ver2.auxiliary_runversion = 0;
		vsif.vers.ver2.auxiliary_debug0 = 0;

		if (vinfo && vinfo->vout_device &&
			vinfo->vout_device->fresh_tx_vsif_pkt) {
			if (dovi_ll_enable)
				vinfo->vout_device->fresh_tx_vsif_pkt
						(EOTF_T_DV_AHEAD,
						 diagnostic_enable
						 ? RGB_10_12BIT : YUV422_BIT12,
						 &vsif, false);
			else
				vinfo->vout_device->fresh_tx_vsif_pkt
						(EOTF_T_DV_AHEAD,
						 dolby_vision_target_mode ==
						 dovi_ll_enable
						 ? YUV422_BIT12 : RGB_8BIT, &vsif,
						 false);
		}
		pr_dolby_dbg("send_hdmi_pkt ahead: %s\n",
					 dovi_ll_enable ? "LL" : "DV");
	}
}

static u32 null_vf_cnt;
static bool video_off_handled;
static int is_video_output_off(struct vframe_s *vf)
{
	if ((READ_VPP_DV_REG(VPP_MISC) & (1 << 10)) == 0) {

		/*Not reset frame0/1 clipping*/
		/*when core off to avoid green garbage*/

		if (!vf)
			null_vf_cnt++;
		else
			null_vf_cnt = 0;

		if (null_vf_cnt > dolby_vision_wait_delay + 1) {
			null_vf_cnt = 0;
			return 1;
		}

		if (video_off_handled)
			return 1;

	} else {
		video_off_handled = 0;
	}
	return 0;
}

bool is_dv_standard_es(int dvel, int mflag, int width)
{
	if (dolby_vision_profile == 4 &&
		dvel == 1 && mflag == 0 &&
		width >= 3840)
		return false;
	else
		return true;
}

static u32 last_total_md_size;
static u32 last_total_comp_size;
/* toggle mode: 0: not toggle; 1: toggle frame; 2: use keep frame */
int dolby_vision_parse_metadata(struct vframe_s *vf,
								u8 toggle_mode,
								bool bypass_release,
								bool drop_flag)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	struct vframe_s *el_vf;
	struct provider_aux_req_s req;
	struct provider_aux_req_s el_req;
	int flag;
	enum signal_format_enum src_format = FORMAT_SDR;
	enum signal_format_enum check_format;
	enum signal_format_enum dst_format;
	enum signal_format_enum cur_src_format;
	enum signal_format_enum cur_dst_format;
	int total_md_size = 0;
	int total_comp_size = 0;
	bool el_flag = 0;
	bool el_halfsize_flag = 1;
	u32 w = 0xffff;
	u32 h = 0xffff;
	int meta_flag_bl = 1;
	int meta_flag_el = 1;
	int src_bdp = 12;
	bool video_frame = false;
	int i;
	struct vframe_master_display_colour_s *p_mdc;
	unsigned int current_mode = dolby_vision_mode;
	u32 target_lumin_max = 0;
	enum input_mode_enum input_mode = IN_MODE_OTT;
	enum priority_mode_enum pri_mode = V_PRIORITY;
	u32 graphic_min = 50; /* 0.0001 */
	u32 graphic_max = 100; /* 1 */
	int ret_flags = 0;
	static int bypass_frame = -1;
	static int last_current_format;
	int ret = -1;
	bool mel_flag = false;
	unsigned long time_use = 0;
	struct timeval start;
	struct timeval end;
	int dump_size = 100;
	char *dvel_provider = NULL;

	memset(&req, 0, (sizeof(struct provider_aux_req_s)));
	memset(&el_req, 0, (sizeof(struct provider_aux_req_s)));

	if (!dolby_vision_enable || !module_installed)
		return -1;

	if (vf) {
		video_frame = true;
		w = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		h = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
	}

	if (vf && (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)) {
		enum vframe_signal_fmt_e fmt;

		input_mode = IN_MODE_OTT;
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;

		/* check source format */
		fmt = get_vframe_src_fmt(vf);
		if ((fmt == VFRAME_SIGNAL_FMT_DOVI ||
			 fmt == VFRAME_SIGNAL_FMT_INVALID) &&
			!vf->discard_dv_data) {
			vf_notify_provider_by_name
					(dv_provider,
					 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
					 (void *)&req);
		}
		/* use callback aux date first, if invalid, use sei_ptr */
		if ((!req.aux_buf || !req.aux_size) &&
			fmt == VFRAME_SIGNAL_FMT_DOVI) {
			u32 sei_size = 0;
			char *sei;

			if (debug_dolby & 1)
				pr_dolby_dbg("no aux %p %x, el %d from %s, use sei_ptr\n",
							 req.aux_buf,
							 req.aux_size,
							 req.dv_enhance_exist,
							 dv_provider);

			sei = (char *)get_sei_from_src_fmt(vf, &sei_size);
			
			if (sei && sei_size) {
				req.aux_buf = sei;
				req.aux_size = sei_size;
				req.dv_enhance_exist =
						vf->src_fmt.dual_layer;
			}

		}
		if (debug_dolby & 1)
			pr_dolby_dbg("%s get vf %p(%d), fmt %d, aux %p %x, el %d\n",
						 dv_provider, vf, vf->discard_dv_data, fmt,
						 req.aux_buf,
						 req.aux_size,
						 req.dv_enhance_exist);
		/* parse meta in base layer */
		if (toggle_mode != 2) {
			ret = get_md_from_src_fmt(vf);
			if (ret == 1) { /*parse succeeded*/
				meta_flag_bl = 0;
				src_format = FORMAT_DOVI;
				memcpy(md_buf[current_id],
					   vf->src_fmt.md_buf,
					   vf->src_fmt.md_size);
				memcpy(comp_buf[current_id],
					   vf->src_fmt.comp_buf,
					   vf->src_fmt.comp_size);
				total_md_size =  vf->src_fmt.md_size;
				total_comp_size =  vf->src_fmt.comp_size;
				ret_flags = vf->src_fmt.parse_ret_flags;
				if ((debug_dolby & 4) && dump_enable) {
					pr_dolby_dbg("get md_buf %p, size(%d):\n",
								 vf->src_fmt.md_buf,
								 vf->src_fmt.md_size);
					for (i = 0; i < total_md_size; i += 8)
						pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
								md_buf[current_id][i],
								md_buf[current_id][i + 1],
								md_buf[current_id][i + 2],
								md_buf[current_id][i + 3],
								md_buf[current_id][i + 4],
								md_buf[current_id][i + 5],
								md_buf[current_id][i + 6],
								md_buf[current_id][i + 7]);
					pr_dolby_dbg("comp(%d):\n",
								 vf->src_fmt.comp_size);
					if (vf->src_fmt.comp_size < dump_size)
						dump_size =
								vf->src_fmt.comp_size;
					for (i = 0; i < dump_size; i += 8)
						pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
								comp_buf[current_id][i],
								comp_buf[current_id][i + 1],
								comp_buf[current_id][i + 2],
								comp_buf[current_id][i + 3],
								comp_buf[current_id][i + 4],
								comp_buf[current_id][i + 5],
								comp_buf[current_id][i + 6],
								comp_buf[current_id][i + 7]);
				}
			} else {  /*no parse or parse failed*/
				meta_flag_bl =
						parse_sei_and_meta
								(vf, &req,
								 &total_comp_size,
								 &total_md_size,
								 &src_format,
								 &ret_flags, drop_flag);
			}
			if (force_mel)
				ret_flags = 1;

			if (ret_flags && req.dv_enhance_exist) {
				if (!strcmp(dv_provider, "dvbldec"))
					vf_notify_provider_by_name
							(dv_provider,
							 VFRAME_EVENT_RECEIVER_DOLBY_BYPASS_EL,
							 (void *)&req);
				pr_dolby_dbg("bypass mel\n");
			}
			if (ret_flags == 1)
				mel_flag = is_mel = true;
			if (!is_dv_standard_es(req.dv_enhance_exist,
								   ret_flags, w)) {
				src_format = FORMAT_SDR;
				/* dovi_setting.src_format = src_format; */
				total_comp_size = 0;
				total_md_size = 0;
				src_bdp = 10;
				bypass_release = true;
			}

		} else {
			src_format = dovi_setting.src_format;
		}

		if (src_format != FORMAT_DOVI && is_hdr10_frame(vf)) {
			src_format = FORMAT_HDR10;
			/* prepare parameter from SEI for hdr10 */
			p_mdc =	&vf->prop.master_display_colour;
			prepare_hdr10_param(p_mdc, &hdr10_param);
			/* for 962x with v1.4 or stb with v2.3 may use 12 bit */
			src_bdp = 10;
			req.dv_enhance_exist = 0;
		}

		if (src_format != FORMAT_DOVI && is_hlg_frame(vf))
			src_format = FORMAT_HLG;

		if (src_format != FORMAT_DOVI && is_hdr10plus_frame(vf))
			src_format = FORMAT_HDR10PLUS;

		if (src_format != FORMAT_DOVI && is_mvc_frame(vf))
			src_format = FORMAT_MVC;

		if ((src_format != FORMAT_CUVA) && is_cuva_frame(vf))
			src_format = FORMAT_CUVA;

		if (src_format == FORMAT_SDR && !req.dv_enhance_exist)
			src_bdp = 10;

		if (((debug_dolby & 1) || frame_count == 0) && toggle_mode == 1)
			pr_info("DV:[%d,%lld,%d,%s,%d,%d]\n",
					 frame_count, vf->pts_us64, src_bdp,
					 (src_format == FORMAT_HDR10) ? "HDR10" :
					 (src_format == FORMAT_DOVI ? "DOVI" :
					  (src_format == FORMAT_HLG ? "HLG" :
					   (src_format == FORMAT_HDR10PLUS ? "HDR10+" :
						(src_format == FORMAT_CUVA ? "CUVA" :
						 (req.dv_enhance_exist ? "DOVI (el meta)" : "SDR"))))),
					 req.aux_size, req.dv_enhance_exist);
		
		if (src_format != FORMAT_DOVI && !req.dv_enhance_exist)
			memset(&req, 0, sizeof(req));

		/* check dvel decoder is active, if active, should */
		/* get/put el data, otherwise, dvbl is stuck */
		dvel_provider = vf_get_provider_name(DVEL_RECV_NAME);
		if (req.dv_enhance_exist && toggle_mode == 1 &&
			dvel_provider && !strcmp(dvel_provider, "dveldec")) {
			el_vf = dvel_vf_get();
			if (el_vf && (el_vf->pts_us64 == vf->pts_us64 ||
						  !(dolby_vision_flags &
							FLAG_CHECK_ES_PTS))) {
				if (debug_dolby & 2)
					pr_dolby_dbg("+++ get bl(%p-%lld) with el(%p-%lld) +++\n",
								 vf, vf->pts_us64,
								 el_vf, el_vf->pts_us64);
				if (meta_flag_bl) {
					int el_md_size = 0;
					int el_comp_size = 0;

					el_req.vf = el_vf;
					el_req.bot_flag = 0;
					el_req.aux_buf = NULL;
					el_req.aux_size = 0;
					if (!strcmp(dv_provider, "dvbldec"))
						vf_notify_provider_by_name
								("dveldec",
								 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
								 (void *)&el_req);
					if (el_req.aux_buf &&
						el_req.aux_size) {
						meta_flag_el =
								parse_sei_and_meta
										(el_vf, &el_req,
										 &el_comp_size,
										 &el_md_size,
										 &src_format,
										 &ret_flags, drop_flag);
					}
					if (!meta_flag_el) {
						total_comp_size = el_comp_size;
						total_md_size = el_md_size;
						src_bdp = 12;
					}
					/* force set format as DOVI*/
					/*	when meta data error */
					if (meta_flag_el &&
						el_req.aux_buf &&
						el_req.aux_size)
						src_format = FORMAT_DOVI;
					if (debug_dolby & 2)
						pr_dolby_dbg
						("el mode:src_fmt:%d,meta_flag_el:%d\n",
						 src_format,
						 meta_flag_el);
					if (meta_flag_el && frame_count == 0)
						pr_info("el mode:parser err,aux %p,size:%d\n",
								 el_req.aux_buf,
								 el_req.aux_size);
				}
				dolby_vision_vf_add(vf, el_vf);
				el_flag = 1;
				if (vf->width == el_vf->width)
					el_halfsize_flag = 0;
			} else {
				if (!el_vf)
					pr_dolby_error("bl(%p-%lld) not found el\n", vf, vf->pts_us64);
				else
					pr_dolby_error("bl(%p-%lld) not found el(%p-%lld)\n", 
						vf, vf->pts_us64,
						el_vf, el_vf->pts_us64);
			}
		} else if (toggle_mode == 1) {
			if (debug_dolby & 2)
				pr_dolby_dbg("+++ get bl(%p-%lld) +++\n",
							 vf, vf->pts_us64);
			dolby_vision_vf_add(vf, NULL);
		}

		if (req.dv_enhance_exist)
			el_flag = 1;

		if (toggle_mode != 2) {
			if (!drop_flag) {
				last_total_md_size = total_md_size;
				last_total_comp_size = total_comp_size;
			}
		} else if (meta_flag_bl && meta_flag_el) {
			total_md_size = last_total_md_size;
			total_comp_size = last_total_comp_size;

			el_flag = dovi_setting.el_flag;

			mel_flag = mel_mode;

			if (debug_dolby & 2)
				pr_dolby_dbg("update el_flag %d, melFlag %d\n",
							 el_flag, mel_flag);

			meta_flag_bl = 0;
		}

		if (el_flag && !mel_flag &&
			((dolby_vision_flags & FLAG_CERTIFICAION) == 0) && !enable_fel) {
			el_flag = 0;
			dolby_vision_el_disable = true;
		}
		if (src_format != FORMAT_DOVI) {
			el_flag = 0;
			mel_flag = 0;
		}
		if (src_format == FORMAT_DOVI &&
			meta_flag_bl && meta_flag_el) {
			/* dovi frame no meta or meta error */
			/* use old setting for this frame   */
			pr_dolby_dbg("no meta or meta err!\n");
			return -1;
		}
	}

	if (src_format == FORMAT_DOVI && meta_flag_bl && meta_flag_el) {
		/* dovi frame no meta or meta error */
		/* use old setting for this frame   */
		pr_dolby_dbg("no meta or meta err!\n");
		return -1;
	}

	/* if not DOVI, release metadata_parser */
	if (vf && src_format != FORMAT_DOVI && metadata_parser && !bypass_release) {

		if (p_funcs_stb)
			p_funcs_stb->metadata_parser_release();

		metadata_parser = NULL;
		pr_dolby_dbg("parser release\n");
	}

	if (drop_flag) {
		pr_dolby_dbg("drop frame_count %d\n", frame_count);
		return 1;
	}

	check_format = src_format;
	if (vf) {
		update_src_format(check_format, vf);
		last_current_format = check_format;
	}

	if (dolby_vision_request_mode != 0xff) {
		dolby_vision_mode = dolby_vision_request_mode;
		dolby_vision_request_mode = 0xff;
	}
	
	current_mode = dolby_vision_mode;
	
	if (dolby_vision_policy_process(&current_mode, check_format)) {

		if (!dolby_vision_wait_init)
			dolby_vision_set_toggle_flag(1);

		pr_info("[%s]output change from %d to %d\n", __func__, dolby_vision_mode, current_mode);

		dolby_vision_target_mode = current_mode;
		dolby_vision_mode = current_mode;
		new_dovi_setting.mode_changed = 1;

		pr_dolby_dbg("[%s]output change from %d to %d(%d, %p, %d)\n",
					 __func__, dolby_vision_mode, current_mode,
					 toggle_mode, vf, src_format);

	} else {
		dolby_vision_target_mode = dolby_vision_mode;
	}

	if (get_hdr_module_status(VD1_PATH) != HDR_MODULE_ON &&
		check_format == FORMAT_SDR) {
		/* insert 2 SDR frames before send DOVI */
		if ((dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL ||
			 dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT) &&
			(last_current_format == FORMAT_HLG ||
			 last_current_format == FORMAT_HDR10PLUS)) {
			bypass_frame = 0;
			pr_dolby_dbg
			("[%s] source transition from %d to %d\n",
			 __func__, last_current_format, check_format);
		}
		last_current_format = check_format;
	}

	if (bypass_frame >= 0 && bypass_frame < MIN_TRANSITION_DELAY) {
		dolby_vision_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
		bypass_frame++;
	} else {
		bypass_frame = -1;
	}

	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		new_dovi_setting.mode_changed = 0;
		dolby_vision_wait_on = false;
		if (get_hdr_module_status(VD1_PATH) == HDR_MODULE_BYPASS)
			return 1;
		return -1;
	}

	if (!p_funcs_stb)
		return -1;

	/* check dst format */
	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL ||
		dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT)
		dst_format = FORMAT_DOVI;
	else if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_HDR10)
		dst_format = FORMAT_HDR10;
	else
		dst_format = FORMAT_SDR;

	/* STB core */
	/* check target luminance */
	/* TODO: Add in overrides for video luminance */
	graphic_min = dolby_vision_graphic_min;
	if (dolby_vision_graphic_max != 0) {
		graphic_max = dolby_vision_graphic_max;
	} else {
		if ((dolby_vision_flags & FLAG_FORCE_DOVI_LL) ||
			dolby_vision_ll_policy >= DOLBY_VISION_LL_YUV422) {
			graphic_max = dv_target_graphics_LL_max[src_format][dst_format];
		} else {
			graphic_max = dv_target_graphics_max[src_format][dst_format];
		}
		if (dv_graphic_blend_test && dst_format == FORMAT_HDR10)
			graphic_max = dv_HDR10_graphics_max;
	}

	if (dolby_vision_flags & FLAG_USE_SINK_MIN_MAX) {
		if (vinfo->vout_device->dv_info->ieeeoui == 0x00d046) { /* 0x00D046 is Dolby's IEEE OUI */
			if (vinfo->vout_device->dv_info->ver == 0) {
				/* need lookup PQ table ... */
			} else if (vinfo->vout_device->dv_info->ver == 1) {

				if (vinfo->vout_device->dv_info->tmaxLUM) {

					if (dolby_vision_force_hdmi_lum) {

						graphic_max = target_lumin_max = dolby_vision_hdmi_lum_max;
						graphic_min = dolby_vision_target_min = dolby_vision_hdmi_lum_min;

					} else {

						/* Target max luminance = 100+50*CV */
						graphic_max =
						target_lumin_max = (vinfo->vout_device->dv_info->tmaxLUM * 50 + 100);

						/* Target min luminance = (CV/127)^2 */
						graphic_min =
						dolby_vision_target_min = (vinfo->vout_device-> dv_info->tminLUM ^ 2)
												* 10000 / (127 * 127);
					}
				}
			}
		} else if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
			if (vinfo->hdr_info.lumi_max) {
				
				/* Luminance value = 50 * (2 ^ (CV/32)) */
				graphic_max =
				target_lumin_max = 50 * (2 ^ (vinfo->hdr_info.lumi_max >> 5));
				
				/* Desired Content Min Luminance =*/
				/*	Desired Content Max Luminance*/
				/*	* (CV/255) * (CV/255) / 100	*/
				graphic_min =
				dolby_vision_target_min =
						target_lumin_max * 10000
						* vinfo->hdr_info.lumi_min
						* vinfo->hdr_info.lumi_min
						/ (255 * 255 * 100);
			}
		}
		if (target_lumin_max) {
			dolby_vision_target_max[0][0] =
			dolby_vision_target_max[0][1] =
			dolby_vision_target_max[1][0] =
			dolby_vision_target_max[1][1] =
			dolby_vision_target_max[2][0] =
			dolby_vision_target_max[2][1] =
					target_lumin_max;
		} else {
			memcpy(dolby_vision_target_max,
				   dolby_vision_default_max,
				   sizeof(dolby_vision_target_max));
		}
	}

	if (new_dovi_setting.video_width && new_dovi_setting.video_height) {
		/* Toggle multiple frames in one vsync case: */
		/* new_dovi_setting.video_width will be available, but not be applied */
		/* So use new_dovi_setting as reference instead of dovi_setting. */
		/* To avoid unnecessary reset control_path. */
		cur_src_format = new_dovi_setting.src_format;
		cur_dst_format = new_dovi_setting.dst_format;
	} else {
		cur_src_format = dovi_setting.src_format;
		cur_dst_format = dovi_setting.dst_format;
	}

	if (src_format != cur_src_format || dst_format != cur_dst_format) {

		pr_dolby_dbg("reset: src:%d-%d, dst:%d-%d, frame_count:%d, flags:0x%x\n",
					 cur_src_format, src_format,
					 cur_dst_format, dst_format,
					 frame_count, dolby_vision_flags);

		p_funcs_stb->control_path
				(FORMAT_INVALID, 0,
				 comp_buf[current_id], 0,
				 md_buf[current_id], 0,
				 0, 0, 0, SIGNAL_RANGE_SMPTE,
				 0, 0, 0, 0,
				 0,
				 &hdr10_param,
				 &new_dovi_setting);
	}
	if (!vsvdb_config_set_flag) {
		memset(&new_dovi_setting.vsvdb_tbl[0],
			   0, sizeof(new_dovi_setting.vsvdb_tbl));
		new_dovi_setting.vsvdb_len = 0;
		new_dovi_setting.vsvdb_changed = 1;
		vsvdb_config_set_flag = true;
	}
	if ((dolby_vision_flags & FLAG_DISABLE_LOAD_VSVDB) == 0) {
		/* check if vsvdb is changed */
		if (vinfo &&  vinfo->vout_device &&
			vinfo->vout_device->dv_info &&
			vinfo->vout_device->dv_info->ieeeoui == 0x00d046 &&
			vinfo->vout_device->dv_info->block_flag == CORRECT) {
			if (new_dovi_setting.vsvdb_len
				!= vinfo->vout_device->dv_info->length + 1)
				new_dovi_setting.vsvdb_changed = 1;
			else if (memcmp
					(&new_dovi_setting.vsvdb_tbl[0],
					 &vinfo->vout_device->dv_info->rawdata[0],
					 vinfo->vout_device->dv_info->length + 1))
				new_dovi_setting.vsvdb_changed = 1;
			memset(&new_dovi_setting.vsvdb_tbl[0],
				   0, sizeof(new_dovi_setting.vsvdb_tbl));
			memcpy(&new_dovi_setting.vsvdb_tbl[0],
				   &vinfo->vout_device->dv_info->rawdata[0],
				   vinfo->vout_device->dv_info->length + 1);
			new_dovi_setting.vsvdb_len =
					vinfo->vout_device->dv_info->length + 1;
			if (new_dovi_setting.vsvdb_changed &&
				new_dovi_setting.vsvdb_len) {
				int k = 0;

				pr_dolby_dbg("new vsvdb[%d]:\n",
							 new_dovi_setting.vsvdb_len);
				pr_dolby_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dolby_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dolby_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dolby_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
			}
		} else {
			if (new_dovi_setting.vsvdb_len)
				new_dovi_setting.vsvdb_changed = 1;
			memset(&new_dovi_setting.vsvdb_tbl[0],
				   0, sizeof(new_dovi_setting.vsvdb_tbl));
			new_dovi_setting.vsvdb_len = 0;
		}

	}

	/* check video/graphics priority on the fly */
	/* cert: some graphic test also need video pri 5223,5243,5253,5263 */
	if (dolby_vision_flags & FLAG_CERTIFICAION) {
		dolby_vision_graphics_priority = 0;
	} else {		
		if (get_video_enabled())
			dolby_vision_graphics_priority = 0;
		else
			dolby_vision_graphics_priority = 1;
	}

	if (dolby_vision_graphics_priority || (dolby_vision_flags & FLAG_PRIORITY_GRAPHIC))
		pri_mode = G_PRIORITY;
	else
		pri_mode = V_PRIORITY;

	if (dst_format == FORMAT_DOVI) {
		if ((dolby_vision_flags & FLAG_FORCE_DOVI_LL) ||
			dolby_vision_ll_policy >= DOLBY_VISION_LL_YUV422)
			new_dovi_setting.use_ll_flag = 1;
		else
			new_dovi_setting.use_ll_flag = 0;
		if (dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444 ||
			(dolby_vision_flags & FLAG_FORCE_RGB_OUTPUT))
			new_dovi_setting.ll_rgb_desired = 1;
		else
			new_dovi_setting.ll_rgb_desired = 0;
	} else {
		new_dovi_setting.use_ll_flag = 0;
		new_dovi_setting.ll_rgb_desired = 0;
	}
	if (dst_format == FORMAT_HDR10 &&
		(dolby_vision_flags & FLAG_DOVI2HDR10_NOMAPPING))
		new_dovi_setting.dovi2hdr10_nomapping = 1;
	else
		new_dovi_setting.dovi2hdr10_nomapping = 0;

	/* always use rgb setting */
	new_dovi_setting.g_bitdepth = 8;
	new_dovi_setting.g_format = G_SDR_RGB;

	new_dovi_setting.diagnostic_enable = 0;
	new_dovi_setting.diagnostic_mux_select = 0;
	new_dovi_setting.dovi_ll_enable = 0;
	if (vinfo) {
		new_dovi_setting.vout_width = vinfo->width;
		new_dovi_setting.vout_height = vinfo->height;
	} else {
		new_dovi_setting.vout_width = 0;
		new_dovi_setting.vout_height = 0;
	}
	memset(&new_dovi_setting.ext_md, 0, sizeof(struct ext_md_s));
	new_dovi_setting.video_width = w << 16;
	new_dovi_setting.video_height = h << 16;

	if (debug_dolby & 0x400)
		do_gettimeofday(&start);

	flag = p_funcs_stb->control_path
			(src_format, dst_format,
			 comp_buf[current_id],
			 (src_format == FORMAT_DOVI) ? total_comp_size : 0,
			 md_buf[current_id],
			 (src_format == FORMAT_DOVI) ? total_md_size : 0,
			 pri_mode,
			 src_bdp, 0, SIGNAL_RANGE_SMPTE, /* bit/chroma/range */
			 graphic_min,
			 graphic_max * 10000,	/* Value in nits, processing input is in 10-thousanths of nits */
			 dolby_vision_target_min,
			 dolby_vision_target_max[src_format][dst_format] * 10000,  /* Value in nits, processing input is in 10-thousanths of nits */
			 (!el_flag && !mel_flag) || (dolby_vision_flags & FLAG_DISABLE_COMPOSER),
			 &hdr10_param,
			 &new_dovi_setting);

	if (debug_dolby & 0x400) {
		
		do_gettimeofday(&end);
		time_use = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
		pr_info("controlpath time: %5ld us\n", time_use);
	}
	
	if (flag >= 0) {
		stb_core_setting_update_flag |= flag;
		if ((dolby_vision_flags & FLAG_FORCE_DOVI_LL) &&
			dst_format == FORMAT_DOVI)
			new_dovi_setting.dovi_ll_enable = 1;
		if ((dolby_vision_flags & FLAG_FORCE_RGB_OUTPUT) &&
			dst_format == FORMAT_DOVI) {
			new_dovi_setting.dovi_ll_enable = 1;
			new_dovi_setting.diagnostic_enable = 1;
			new_dovi_setting.diagnostic_mux_select = 1;
		}

		if (debug_dolby & 2)
			pr_dolby_dbg("ll_enable=%d,diagnostic=%d,ll_policy=%d\n",
						 new_dovi_setting.dovi_ll_enable,
						 new_dovi_setting.diagnostic_enable,
						 dolby_vision_ll_policy);

		new_dovi_setting.src_format = src_format;
		new_dovi_setting.dst_format = dst_format;
		new_dovi_setting.el_flag = el_flag;
		new_dovi_setting.el_halfsize_flag = el_halfsize_flag;
		new_dovi_setting.video_width = w;
		new_dovi_setting.video_height = h;

		dovi_setting_video_flag = video_frame;

		if (debug_dolby & 1) {

			if (is_video_output_off(vf))
				pr_dolby_dbg("setting %d->%d(T:%d-%d), osd:%dx%d\n",
							 src_format, dst_format,
							 dolby_vision_target_min,
							 dolby_vision_target_max[src_format][dst_format],
							 osd_graphic_width,
							 osd_graphic_height);

			if (el_flag) {

				pr_dolby_dbg("v %d: %dx%d %d->%d(T:%d-%d), g %d: %dx%d %d->%d,%s\n",
							 dovi_setting_video_flag,
							 w == 0xffff ? 0 : w,
							 h == 0xffff ? 0 : h,
							 src_format, dst_format,
							 dolby_vision_target_min,
							 dolby_vision_target_max[src_format][dst_format] * 10000, /* Value in nits, processing input is in 10-thousanths of nits */
							 pri_mode == G_PRIORITY,
							 osd_graphic_width,
							 osd_graphic_height,
							 graphic_min,
							 graphic_max * 10000, /* Value in nits, processing input is in 10-thousanths of nits */
							 pri_mode == V_PRIORITY ? "vpr" : "gpr");

				pr_dolby_dbg("flag=%x, md=%d, comp=%d, frame:%d\n",
							 flag, total_md_size, total_comp_size,
							 frame_count);

			} else {

				pr_dolby_dbg("v %d: %dx%d %d->%d(T:%d-%d), g %d: %dx%d %d->%d, %s, flag=%x, md=%d, frame:%d\n",
							 dovi_setting_video_flag,
							 w == 0xffff ? 0 : w,
							 h == 0xffff ? 0 : h,
							 src_format, dst_format,
							 dolby_vision_target_min,
							 dolby_vision_target_max[src_format][dst_format] * 10000, /* Value in nits, processing input is in 10-thousanths of nits */
							 pri_mode == G_PRIORITY,
							 osd_graphic_width,
							 osd_graphic_height,
							 graphic_min,
							 graphic_max * 10000, /* Value in nits, processing input is in 10-thousanths of nits */
							 pri_mode == V_PRIORITY ? "vpr" : "gpr",
							 flag,
							 total_md_size, frame_count);

			}
		}
		dump_setting(&new_dovi_setting, frame_count, debug_dolby);
		el_mode = el_flag;
		mel_mode = mel_flag;
		return 0; /* setting updated */
	}
	if (flag < 0) {
		pr_dolby_dbg("video %d:%dx%d setting %d->%d(T:%d-%d): pri_mode=%d, no_el=%d, md=%d, frame:%d\n",
					 dovi_setting_video_flag,
					 w == 0xffff ? 0 : w,
					 h == 0xffff ? 0 : h,
					 src_format, dst_format,
					 dolby_vision_target_min,
					 dolby_vision_target_max
					 [src_format][dst_format],
					 pri_mode,
					 (!el_flag && !mel_flag),
					 total_md_size, frame_count);
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		pr_dolby_error("control_path(%d, %d) failed %d\n",
					   src_format, dst_format, flag);
		if ((debug_dolby & 0x2000) && dump_enable &&
			total_md_size > 0) {
			pr_dolby_dbg("control_path failed, md(%d):\n",
						 total_md_size);
			for (i = 0; i < total_md_size + 7; i += 8)
				pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						md_buf[current_id][i],
						md_buf[current_id][i + 1],
						md_buf[current_id][i + 2],
						md_buf[current_id][i + 3],
						md_buf[current_id][i + 4],
						md_buf[current_id][i + 5],
						md_buf[current_id][i + 6],
						md_buf[current_id][i + 7]);
		}
	}
	return -1; /* do nothing for this frame */
}
EXPORT_SYMBOL(dolby_vision_parse_metadata);

/*dual_layer && parse_ret_flags =1 =>mel*/
/*dual_layer && parse_ret_flags !=1 =>fel*/
bool vf_is_fel(struct vframe_s *vf)
{
	enum vframe_signal_fmt_e fmt;
	bool fel = false;

	if (!vf)
		return false;

	fmt = get_vframe_src_fmt(vf);

	if (fmt == VFRAME_SIGNAL_FMT_DOVI) {
		if (debug_dolby & 0x1)
			pr_dolby_dbg("dual layer %d, parse ret flags %d\n",
						 vf->src_fmt.dual_layer,
						 vf->src_fmt.parse_ret_flags);
		if (vf->src_fmt.dual_layer && vf->src_fmt.parse_ret_flags != 1)
			fel = true;
	}
	return fel;
}

/* 0: no el; >0: with el */
/* 1: need wait el vf    */
/* 2: no match el found  */
/* 3: found match el     */
int dolby_vision_wait_metadata(struct vframe_s *vf)
{
	struct vframe_s *el_vf;
	int ret = 0;
	unsigned int mode = dolby_vision_mode;
	enum signal_format_enum check_format;
	const struct vinfo_s *vinfo = get_current_vinfo();
	bool vd1_on = false;

	if (single_step_enable) {
		if (dolby_vision_flags & FLAG_SINGLE_STEP)
			/* wait fake el for "step" */
			return 1;

		dolby_vision_flags |= FLAG_SINGLE_STEP;
	}

	if (dolby_vision_flags & FLAG_CERTIFICAION) {
		bool ott_mode = true;

		if (debug_dolby & 0x1000)
			pr_dolby_dbg("setting_update_count %d, crc_count %d, flag %x\n",
						 setting_update_count, crc_count,
						 dolby_vision_flags);

		if (setting_update_count > crc_count &&
			!(dolby_vision_flags & FLAG_DISABLE_CRC)) {
			if (ott_mode)
				return 1;
		}
	}

	if (is_dovi_dual_layer_frame(vf)) {

		el_vf = dvel_vf_peek();

		while (el_vf) {

			if (debug_dolby & 2)
				pr_dolby_dbg("=== peek bl(%p-%lld) with el(%p-%lld) ===\n",
							 vf, vf->pts_us64,
							 el_vf, el_vf->pts_us64);

			if (el_vf->pts_us64 == vf->pts_us64 ||
				!(dolby_vision_flags & FLAG_CHECK_ES_PTS)) {

				/* found el */
				ret = 3;
				break;

			} else if (el_vf->pts_us64 < vf->pts_us64) {

				if (debug_dolby & 2)
					pr_dolby_dbg("bl(%p-%lld) => skip el pts(%p-%lld)\n",
								 vf, vf->pts_us64,
								 el_vf, el_vf->pts_us64);

				el_vf = dvel_vf_get();
				dvel_vf_put(el_vf);

				vf_notify_provider(DVEL_RECV_NAME,
								   VFRAME_EVENT_RECEIVER_PUT, NULL);

				if (debug_dolby & 2)
					pr_dolby_dbg("=== get & put el(%p-%lld) ===\n", el_vf, el_vf->pts_us64);

				/* skip old el and peek new */
				el_vf = dvel_vf_peek();
			} else {
				/* no el found */
				ret = 2;
				break;
			}
		}
		/* need wait el */
		if (!el_vf) {
			if (debug_dolby & 2)
				pr_dolby_dbg("=== bl wait el(%p-%lld) ===\n", vf, vf->pts_us64);
			ret = 1;
		}
	}
	if (ret == 1)
		return ret;

	if (!dolby_vision_wait_init && !dolby_vision_core1_on) {

		ret = is_dovi_frame(vf);

		if (ret) {
			check_format = FORMAT_DOVI;
			ret = 0;
		} else if (is_hdr10_frame(vf)) {
			check_format = FORMAT_HDR10;
		} else if (is_hlg_frame(vf)) {
			check_format = FORMAT_HLG;
		} else if (is_hdr10plus_frame(vf)) {
			check_format = FORMAT_HDR10PLUS;
		} else if (is_mvc_frame(vf)) {
			check_format = FORMAT_MVC;
		} else if (is_cuva_frame(vf)) {
			check_format = FORMAT_CUVA;
		} else {
			check_format = FORMAT_SDR;
		}

		if (vf)
			update_src_format(check_format, vf);

		if (dolby_vision_policy_process(&mode, check_format)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
				dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {

				dolby_vision_wait_init = true;
				dolby_vision_target_mode = mode;
				dolby_vision_wait_on = true;

				/*dv off->on, delay vfream*/
				if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE &&
					dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS &&
					mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL &&
					dolby_vision_wait_delay > 0 && !vf_is_fel(vf))
				{
					dolby_vision_wait_count = dolby_vision_wait_delay;
				} else {
					dolby_vision_wait_count = 0;
				}

				pr_dolby_dbg("dolby_vision_need_wait src=%d mode=%d\n",
							 check_format, mode);
			}
		}
		/*chip after g12 not used bit VPP_MISC 9:11*/
		if (READ_VPP_DV_REG(VPP_MISC) & (1 << 10))
			vd1_on = true;

		/* don't use run mode when sdr -> dv and vd1 not disable */
		if (dolby_vision_wait_init && vd1_on)
			dolby_vision_on_count = dolby_vision_run_mode_delay + 1;
	}

	if (dolby_vision_wait_init && dolby_vision_wait_count > 0) {

		if (debug_dolby & 8)
			pr_dolby_dbg("delay wait %d\n", dolby_vision_wait_count);

		if (!get_disable_video_flag(VD1_PATH)) {

			/*update only after app enable video display,*/
			/* to distinguish play start and netflix exit*/
			send_hdmi_pkt_ahead(FORMAT_DOVI, vinfo);
			dolby_vision_wait_count--;

		} else {

			/*exit netflix, still process vf after video disable,*/
			/*wait init will be on, need reset wait init */
			dolby_vision_wait_init = false;
			dolby_vision_wait_count = 0;

			if (debug_dolby & 8)
				pr_dolby_dbg("clear dolby_vision_wait_on\n");
		}
		ret = 1;
	} else if (dolby_vision_core1_on && (dolby_vision_on_count <= dolby_vision_run_mode_delay))
		ret = 1;

	if (debug_dolby & 8)
		pr_dolby_dbg("dv wait return %d\n", ret);

	return ret;
}

int dolby_vision_update_metadata(struct vframe_s *vf, bool drop_flag)
{
	int ret = -1;

	if (!dolby_vision_enable)
		return -1;

	/*clear dv_vf when render first frame */
	if (vf && vf->omx_index == 0)
		dolby_vision_clear_buf();

	if (vf && dolby_vision_vf_check(vf)) {
		ret = dolby_vision_parse_metadata(vf, 1, false, drop_flag);
		frame_count++;
	}

	return ret;
}
EXPORT_SYMBOL(dolby_vision_update_metadata);

/* to re-init the src format after video off -> on case */
int dolby_vision_update_src_format(struct vframe_s *vf, u8 toggle_mode)
{
	unsigned int mode = dolby_vision_mode;
	enum signal_format_enum check_format;
	int ret = 0;

	if (!dolby_vision_enable || !vf)
		return -1;

	/* src_format is valid, need not re-init */
	if (dolby_vision_src_format != 0)
		return 0;

	/* vf is not in the dv queue, new frame case */
	if (dolby_vision_vf_check(vf))
		return 0;

	ret = is_dovi_frame(vf);
	if (ret) {
		check_format = FORMAT_DOVI;
	} else if (is_hdr10_frame(vf)) {
		check_format = FORMAT_HDR10;
	} else if (is_hlg_frame(vf)) {
		check_format = FORMAT_HLG;
	} else if (is_hdr10plus_frame(vf)) {
		check_format = FORMAT_HDR10PLUS;
	} else if (is_mvc_frame(vf)) {
		check_format = FORMAT_MVC;
	} else if (is_cuva_frame(vf)) {
		check_format = FORMAT_CUVA;
	} else {
		check_format = FORMAT_SDR;
	}
	if (vf)
		update_src_format(check_format, vf);

	if (!dolby_vision_wait_init &&
		!dolby_vision_core1_on &&
		dolby_vision_src_format != 0) {
		if (dolby_vision_policy_process
				(&mode, check_format)) {
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				dolby_vision_wait_init = true;
				dolby_vision_target_mode = mode;
				dolby_vision_wait_on = true;
				pr_dolby_dbg
				("dolby_vision_need_wait src=%d mode=%d\n",
				 check_format, mode);
			}
		}
		/* don't use run mode when sdr -> dv and vd1 not disable */
		if (dolby_vision_wait_init &&
			(READ_VPP_DV_REG(VPP_MISC) & (1 << 10)))
			dolby_vision_on_count =
					dolby_vision_run_mode_delay + 1;
	}
	pr_dolby_dbg
	("%s done vf:%p, src=%d, toggle mode:%d\n",
	 __func__, vf, dolby_vision_src_format, toggle_mode);
	return 1;
}
EXPORT_SYMBOL(dolby_vision_update_src_format);

static void update_dolby_vision_status(enum signal_format_enum src_format)
{
	if ((src_format == FORMAT_DOVI || src_format == FORMAT_DOVI_LL) &&
		dolby_vision_status != DV_PROCESS) {

		pr_dolby_dbg("Dolby Vision mode changed to DV_PROCESS %d\n", src_format);
		dolby_vision_status = DV_PROCESS;

	} else if (src_format == FORMAT_HDR10 && dolby_vision_status != HDR_PROCESS) {

		pr_dolby_dbg("Dolby Vision mode changed to HDR_PROCESS %d\n", src_format);
		dolby_vision_status = HDR_PROCESS;

	} else if (src_format == FORMAT_SDR && dolby_vision_status != SDR_PROCESS) {

		pr_dolby_dbg("Dolby Vision mode changed to SDR_PROCESS %d\n", src_format);
		dolby_vision_status = SDR_PROCESS;
	}
}

static u8 last_pps_state;
static void bypass_pps_path(u8 pps_state)
{
	if (pps_state && last_pps_state != pps_state) {
		pr_dolby_dbg("pps_state %d => %d\n", last_pps_state, pps_state);
		last_pps_state = pps_state;
	}
}

/* toggle mode: 0: not toggle; 1: toggle frame; 2: use keep frame */
/* pps_state 0: no change, 1: pps enable, 2: pps disable */
int dolby_vision_process(struct vframe_s *vf,
						 u32 display_size,
						 u8 toggle_mode, u8 pps_state)
{
	int src_chroma_format = 0;
	u32 h_size = (display_size >> 16) & 0xffff;
	u32 v_size = display_size & 0xffff;
	const struct vinfo_s *vinfo = get_current_vinfo();
	bool reset_flag = false;
	bool force_set = false;
	static int sdr_delay;
	unsigned int mode = dolby_vision_mode;
	static bool video_turn_off = true;
	static bool video_on[VD_PATH_MAX];
	int video_status = 0;
	int graphic_status = 0;
	int policy_changed = 0;
	int sink_changed = 0;
	int format_changed = 0;
	u8 core_mask = 0x7;
	bool reverse_changed = false;
	static u8 last_toggle_mode;
	struct vout_device_s *p_vout = NULL;

	if (dolby_vision_flags & FLAG_CERTIFICAION) {
		if (vf) {
			h_size = (vf->type & VIDTYPE_COMPRESS) ? vf->compWidth : vf->width;
			v_size = (vf->type & VIDTYPE_COMPRESS) ? vf->compHeight : vf->height;
		} else {
			h_size = 0;
			v_size = 0;
		}
		dolby_vision_on_count = 1 + dolby_vision_run_mode_delay;
	}

	if (dolby_vision_flags & FLAG_TOGGLE_FRAME)	{

		h_size = (display_size >> 16) & 0xffff;
		v_size = display_size & 0xffff;

		/* stb control path case */
		if (new_dovi_setting.video_width & 0xffff && new_dovi_setting.video_height & 0xffff) {
			
			if (new_dovi_setting.video_width != h_size || new_dovi_setting.video_height != v_size) {
				
				if (debug_dolby & 8)
					pr_dolby_dbg("stb update disp size %d %d->%d %d\n",
					 new_dovi_setting.video_width,
					 new_dovi_setting.video_height,
					 h_size, v_size);
			}
			if (h_size && v_size) {
				new_dovi_setting.video_width = h_size;
				new_dovi_setting.video_height = v_size;
			} else {
				new_dovi_setting.video_width = 0xffff;
				new_dovi_setting.video_height = 0xffff;
			}

		} else if (core1_disp_hsize != h_size || core1_disp_vsize != v_size) {
			if (debug_dolby & 8)
				pr_dolby_dbg("stb update display size %d %d->%d %d\n",
						core1_disp_hsize,
						core1_disp_vsize,
						h_size, v_size);
			if (h_size && v_size) {
				new_dovi_setting.video_width = h_size;
				new_dovi_setting.video_height = v_size;
			} else {
				new_dovi_setting.video_width = 0xffff;
				new_dovi_setting.video_height = 0xffff;
			}
		}

		if ((!vf || toggle_mode != 1) && !sdr_delay) {
			/* log to monitor if has dv toggles not needed */
			/* !sdr_delay: except in transition from DV to SDR */
			pr_dolby_dbg("NULL/RPT frame %p, hdr module %s, video %s\n",
						 vf,
						 get_hdr_module_status(VD1_PATH)
						 == HDR_MODULE_ON ? "on" : "off",
						 get_video_enabled() ? "on" : "off");
		}
	}

	last_toggle_mode = toggle_mode;
	if (debug_dolby & 0x1000)
		pr_dolby_dbg("setting_update_count %d, crc_count %d\n",
					 setting_update_count, crc_count);

	if ((dolby_vision_flags & FLAG_CERTIFICAION) &&
		!(dolby_vision_flags & FLAG_DISABLE_CRC) &&
		setting_update_count > crc_count &&
		is_dolby_vision_on()) {

		if (setting_update_count == 1 &&
			crc_read_delay == 1) {
			/* work around to enable crc for frame 0 */
			VSYNC_WR_DV_REG(DOLBY_CORE3_CRC_CTRL, 1);
			crc_read_delay++;
		}
	}

	video_status = is_video_turn_on(video_on, VD1_PATH);
	if (video_status == -1) {
		video_turn_off = true;
		pr_dolby_dbg("VD1 video off, video_status -1\n");
	} else if (video_status == 1) {
		pr_dolby_dbg("VD1 video on, video_status 1\n");
		video_turn_off = false;
	}

	if (dolby_vision_mode != dolby_vision_target_mode)
		format_changed = 1;

	graphic_status = is_graphic_changed();

	/* monitor policy changes */
	policy_changed = is_policy_changed();
	if (policy_changed || format_changed ||
		(graphic_status & 2) || osd_update) {
		dolby_vision_set_toggle_flag(1);
		if (osd_update)
			osd_update = false;
	}

	if (!is_dolby_vision_on())
		dolby_vision_flags &= ~FLAG_FORCE_HDMI_PKT;

	sink_changed = (is_sink_cap_changed(vinfo,
										&current_hdr_cap, &current_sink_available) & 2) ? 1 : 0;

	if (sink_changed || policy_changed || format_changed ||
		(video_status == 1 && !(dolby_vision_flags & FLAG_CERTIFICAION)) ||
		(graphic_status & 2) ||
		(dolby_vision_flags & FLAG_FORCE_HDMI_PKT) ||
		need_update_cfg || reverse_changed) {
		if (debug_dolby & 1)
			pr_dolby_dbg("sink %s,cap 0x%x,video %s,osd %s,vf %p,toggle %d\n",
						 current_sink_available ? "on" : "off",
						 current_hdr_cap,
						 video_turn_off ? "off" : "on",
						 is_graphics_output_off() ? "off" : "on",
						 vf, toggle_mode);
		/* do not toggle a new el vf */
		if (toggle_mode == 1)
			toggle_mode = 0;
		if (vf &&
			!dolby_vision_parse_metadata
					(vf, toggle_mode, false, false)) {
			h_size = (display_size >> 16) & 0xffff;
			v_size = display_size & 0xffff;

			new_dovi_setting.video_width = h_size;
			new_dovi_setting.video_height = v_size;

			dolby_vision_set_toggle_flag(1);
		}
		need_update_cfg = false;
	}

	if (debug_dolby & 8)
		pr_dolby_dbg("vf %p, turn_off %d, video_status %d, toggle %d, flag %x\n",
					 vf, video_turn_off, video_status,
					 toggle_mode, dolby_vision_flags);

	if ((!vf && video_turn_off) ||
		(video_status == -1)) {
		if (dolby_vision_policy_process(&mode, FORMAT_SDR)) {
			pr_dolby_dbg("Fake SDR, mode->%d\n", mode);
			if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE &&
				mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				dolby_vision_target_mode =
						DOLBY_VISION_OUTPUT_MODE_BYPASS;
				dolby_vision_mode =
						DOLBY_VISION_OUTPUT_MODE_BYPASS;
				dolby_vision_set_toggle_flag(0);
				dolby_vision_wait_on = false;
				dolby_vision_wait_init = false;
			} else {
				dolby_vision_set_toggle_flag(1);
			}
		}
		if ((dolby_vision_flags & FLAG_TOGGLE_FRAME) ||
			((video_status == -1) && dolby_vision_core1_on)) {
			pr_dolby_dbg("update when video off\n");
			dolby_vision_parse_metadata(NULL, 1, false, false);
			dolby_vision_set_toggle_flag(1);
		}
		if (!vf && video_turn_off &&
			!dolby_vision_core1_on &&
			dolby_vision_src_format != 0) {
			pr_dolby_dbg("update src_fmt when video off\n");
			dolby_vision_src_format = 0;
		}
	}

	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {

		if (vinfo && vinfo->vout_device &&
			!vinfo->vout_device->dv_info &&
			vsync_count < FLAG_VSYNC_CNT) {
			vsync_count++;
			return 0;
		}

		if (dolby_vision_status != BYPASS_PROCESS) {
			if (vinfo) {
				if (vf && is_hdr10plus_frame(vf)) {
					/* disable dolby immediately */
					pr_dolby_dbg("Dolby bypass: HDR10+: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_HDR10PLUS,
								  FORMAT_SDR, vinfo, vf);
					enable_dolby_vision(0);
				} else if (vf && is_hlg_frame(vf)) {
					/* disable dolby immediately */
					pr_dolby_dbg("Dolby bypass: HLG: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_HLG,
								  FORMAT_SDR, vinfo, vf);
					enable_dolby_vision(0);
				} else if (vf && is_cuva_frame(vf)) {
					/* disable dolby immediately */
					pr_dolby_dbg("Dolby bypass: cuva: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_CUVA,
								  FORMAT_SDR, vinfo, vf);
					enable_dolby_vision(0);
				} else if (last_dst_format != FORMAT_DOVI) {
					/* disable dolby immediately:
					 * non-dovi alwyas hdr to adaptive
					 */
					pr_dolby_dbg("Dolby bypass: Switched %d to SDR\n",
								 last_dst_format);
					send_hdmi_pkt(dolby_vision_src_format,
								  FORMAT_SDR, vinfo, vf);
					enable_dolby_vision(0);
				} else {
					/* disable dolby after sdr delay:
					 * dovi alwyas hdr to adaptive or dovi
					 * playback exit in adaptive mode on
					 * a dovi tv
					 */
					if (sdr_delay == 0) {
						pr_dolby_dbg("Dolby bypass: Start - Switched to SDR\n");
						dolby_vision_set_toggle_flag(1);
					}
					if ((get_video_mute() == VIDEO_MUTE_ON_DV && !(dolby_vision_flags & FLAG_MUTE)) || 
						(get_video_mute() == VIDEO_MUTE_OFF && dolby_vision_flags & FLAG_MUTE))
						
						/* core 3 (RPU) only */
						apply_stb_core_settings
								(dovi_setting_video_flag,
								 dolby_vision_mask & 0x4,
								 0,
								 (dovi_setting.video_width << 16) | dovi_setting.video_height, pps_state);
					
					send_hdmi_pkt(dolby_vision_src_format,
								  FORMAT_SDR, vinfo, vf);
					if (sdr_delay >= MAX_TRANSITION_DELAY) {
						pr_dolby_dbg("Dolby bypass: Done - Switched to SDR\n");
						enable_dolby_vision(0);
						sdr_delay = 0;
					} else {
						sdr_delay++;
					}
				}
			} else {
				enable_dolby_vision(0);
			}
		}
		if (sdr_delay == 0)
			dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		return 0;
	} else if (sdr_delay != 0) {
		/* in case mode change to a mode requiring dolby block */
		sdr_delay = 0;
	}

	if ((dolby_vision_flags & FLAG_CERTIFICAION) ||
		(dolby_vision_flags & FLAG_BYPASS_VPP))
		video_effect_bypass(1);

	if (!p_funcs_stb) {
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		return 0;
	}

	if ((debug_dolby & 2) && force_set &&
		!(dolby_vision_flags & FLAG_CERTIFICAION))
		pr_dolby_dbg
		("core1 size changed--old: %d x %d, new: %d x %d\n",
		 core1_disp_hsize, core1_disp_vsize,
		 h_size, v_size);
	if (dolby_vision_core1_on &&
		dolby_vision_core1_on_cnt < DV_CORE1_RECONFIG_CNT &&
		!(dolby_vision_flags & FLAG_TOGGLE_FRAME)) {
		dolby_vision_set_toggle_flag(1);
		if (!(dolby_vision_flags & FLAG_CERTIFICAION))
			pr_dolby_dbg
			("Need update core1 setting first %d times\n",
			 dolby_vision_core1_on_cnt);
	}
	if (dolby_vision_on && !dolby_vision_core1_on &&
		dolby_vision_core2_on_cnt &&
		dolby_vision_core2_on_cnt < DV_CORE2_RECONFIG_CNT &&
		!(dolby_vision_flags & FLAG_TOGGLE_FRAME) &&
		!(dolby_vision_flags & FLAG_CERTIFICAION)) {
		force_set_lut = true;
		dolby_vision_set_toggle_flag(1);
		if (debug_dolby & 2)
			pr_dolby_dbg("Need update core2 first %d times\n",
						 dolby_vision_core2_on_cnt);
	}
	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {

		if (!(dolby_vision_flags & FLAG_CERTIFICAION))
			reset_flag =
					(dolby_vision_reset & 1) &&
					(!dolby_vision_core1_on) &&
					(dolby_vision_on_count == 0);

		if (((new_dovi_setting.video_width & 0xffff) && (new_dovi_setting.video_height & 0xffff)) || force_set_lut) {

			if (new_dovi_setting.video_width == 0xffff)
				new_dovi_setting.video_width = 0;
			
			if (new_dovi_setting.video_height == 0xffff)
				new_dovi_setting.video_height = 0;
			
			if (vf && (vf->type & VIDTYPE_VIU_422))
				src_chroma_format = 2;
			else if (vf)
				src_chroma_format = 1;

			if (force_set && !(dolby_vision_flags & FLAG_CERTIFICAION))
				reset_flag = true;
			
			apply_stb_core_settings
					(dovi_setting_video_flag,
					 dolby_vision_mask & core_mask,
					 reset_flag,
					 (new_dovi_setting.video_width << 16) | new_dovi_setting.video_height, pps_state);
			
			memcpy(&dovi_setting, &new_dovi_setting, sizeof(dovi_setting));
			
			if (core1_disp_hsize != dovi_setting.video_width ||
				core1_disp_vsize != dovi_setting.video_height)
				
				if (core1_disp_hsize &&
					core1_disp_vsize)
					pr_dolby_dbg
					("frame size %d %d->%d %d\n",
					 core1_disp_hsize,
					 core1_disp_vsize,
					 dovi_setting.video_width,
					 dovi_setting.video_height);
			new_dovi_setting.video_width =
			new_dovi_setting.video_height = 0;
			
			if (!dovi_setting.video_width || !dovi_setting.video_height)
				dovi_setting_video_flag = false;
			
			if (dovi_setting_video_flag && dolby_vision_on_count == 0)
				pr_dolby_dbg("first frame reset %d\n", reset_flag);
			
			/* clr hdr+ pkt when enable dv */
			if (!dolby_vision_on &&
				vinfo && vinfo->vout_device) {
				p_vout = vinfo->vout_device;
				if (p_vout->fresh_tx_hdr10plus_pkt)
					p_vout->fresh_tx_hdr10plus_pkt
							(0, NULL);
			}

			enable_dolby_vision(1);
			bypass_pps_path(pps_state);
			core1_disp_hsize = dovi_setting.video_width;
			core1_disp_vsize = dovi_setting.video_height;

			/* send HDMI packet according to dst_format */
			if (vinfo)
				send_hdmi_pkt(dovi_setting.src_format, dovi_setting.dst_format, vinfo, vf);

			update_dolby_vision_status(dovi_setting.src_format);

		} else {

			if ((get_video_mute() == VIDEO_MUTE_ON_DV && !(dolby_vision_flags & FLAG_MUTE)) ||
				(get_video_mute() == VIDEO_MUTE_OFF && dolby_vision_flags & FLAG_MUTE) ||
				last_dolby_vision_ll_policy != dolby_vision_ll_policy)
				
				/* core 3 (RPU) only */
				apply_stb_core_settings
						(dovi_setting_video_flag,
						 dolby_vision_mask & 0x4,
						 reset_flag,
						 (dovi_setting.video_width << 16) | dovi_setting.video_height, pps_state);

			/* force send hdmi pkt */
			if (dolby_vision_flags & FLAG_FORCE_HDMI_PKT) {
				if (vinfo)
					send_hdmi_pkt(dovi_setting.src_format, dovi_setting.dst_format, vinfo, vf);
			}
		}

		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;

	} else if (dolby_vision_core1_on && !(dolby_vision_flags & FLAG_CERTIFICAION)) {

		bool reset_flag =
				(dolby_vision_reset & 2) &&
				(dolby_vision_on_count <= (dolby_vision_reset_delay >> 8)) &&
				(dolby_vision_on_count >= (dolby_vision_reset_delay & 0xff));

		if ((dolby_vision_on_count <= dolby_vision_run_mode_delay) || force_set) {
			
			if (force_set)
				reset_flag = true;
			
			apply_stb_core_settings
					(true, 						/* always enable */							
					 dolby_vision_mask & 0x1,	/* core 1 (EL) only */
					 reset_flag,
					 (core1_disp_hsize << 16) | core1_disp_vsize, pps_state);
			
			if (dolby_vision_on_count < dolby_vision_run_mode_delay)
				pr_dolby_dbg("fake frame (%d %d) %d reset %d\n",
							 core1_disp_hsize,
							 core1_disp_vsize,
							 dolby_vision_on_count,
							 reset_flag);
		}
	}

	if (dolby_vision_core1_on) {
		if (dolby_vision_on_count <= dolby_vision_run_mode_delay + 1)
			dolby_vision_on_count++;
	} else {
		dolby_vision_on_count = 0;
	}

	return 0;
}
EXPORT_SYMBOL(dolby_vision_process);

/* when dolby on in uboot, other module cannot get dolby status
 * in time through dolby_vision_on due to dolby_vision_on
 * is set in register_dv_functions
 * Add dolby_vision_on_in_uboot condition for this case.
 */
bool is_dolby_vision_on(void)
{
	return dolby_vision_on || dolby_vision_wait_on || dolby_vision_on_in_uboot;
}
EXPORT_SYMBOL(is_dolby_vision_on);

bool is_dolby_vision_video_on(void)
{
	return dolby_vision_core1_on;
}
EXPORT_SYMBOL(is_dolby_vision_video_on);

bool for_dolby_vision_certification(void)
{
	return is_dolby_vision_on() &&
		   dolby_vision_flags & FLAG_CERTIFICAION;
}
EXPORT_SYMBOL(for_dolby_vision_certification);

bool for_dolby_vision_video_effect(void)
{
	return is_dolby_vision_on() &&
		   dolby_vision_flags & FLAG_BYPASS_VPP;
}
EXPORT_SYMBOL(for_dolby_vision_video_effect);

void dolby_vision_set_toggle_flag(int flag)
{
	if (flag) {
		dolby_vision_flags |= FLAG_TOGGLE_FRAME;
		if (flag & 2)
			dolby_vision_flags |= FLAG_FORCE_HDMI_PKT;
	} else {
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
	}
}
EXPORT_SYMBOL(dolby_vision_set_toggle_flag);

bool is_dv_control_backlight(void)
{
	return dv_control_backlight_status;
}
EXPORT_SYMBOL(is_dv_control_backlight);

void set_dolby_vision_mode(int mode)
{
	if (dolby_vision_enable
		&& (dolby_vision_request_mode == 0xff)) {
		if (dolby_vision_policy_process(
				&mode, get_cur_src_format())) {
			dolby_vision_set_toggle_flag(1);
			if (mode != DOLBY_VISION_OUTPUT_MODE_BYPASS &&
				dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS)
				dolby_vision_wait_on = true;
			pr_info("DOVI output change from %d to %d\n",
					dolby_vision_mode, mode);
			dolby_vision_target_mode = mode;
			dolby_vision_mode = mode;
		}
	}
}
EXPORT_SYMBOL(set_dolby_vision_mode);

int get_dolby_vision_mode(void)
{
	return dolby_vision_mode;
}
EXPORT_SYMBOL(get_dolby_vision_mode);

int get_dolby_vision_target_mode(void)
{
	return dolby_vision_target_mode;
}
EXPORT_SYMBOL(get_dolby_vision_target_mode);

bool is_dolby_vision_enable(void)
{
	return dolby_vision_enable;
}
EXPORT_SYMBOL(is_dolby_vision_enable);

bool is_dolby_vision_stb_mode(void)
{
	return true;
}
EXPORT_SYMBOL(is_dolby_vision_stb_mode);

bool is_dolby_vision_el_disable(void)
{
	return dolby_vision_el_disable;
}
EXPORT_SYMBOL(is_dolby_vision_el_disable);

void set_dolby_vision_policy(int policy)
{
	dolby_vision_policy = policy;
}
EXPORT_SYMBOL(set_dolby_vision_policy);

int get_dolby_vision_policy(void)
{
	return dolby_vision_policy;
}
EXPORT_SYMBOL(get_dolby_vision_policy);

/* bit 0 for HDR10: 1=by dv, 0-by vpp */
/* bit 1 for HLG: 1=by dv, 0-by vpp */
int get_dolby_vision_hdr_policy(void)
{
	int ret = 0;

	if (!is_dolby_vision_enable())
		return 0;
	if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE) {
		/* policy == FOLLOW_SRC, check hdr/hlg policy */
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SRC) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SRC) ? 2 : 0;
		ret |= (dolby_vision_hdr10_policy & SDR_BY_DV_F_SRC) ? 0x40 : 0;
	} else if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SINK) {
		/* policy == FOLLOW_SINK, check hdr/hlg policy */
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SINK) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SINK) ? 2 : 0;
		ret |= (dolby_vision_hdr10_policy & SDR_BY_DV_F_SINK) ? 0x20 : 0;
	} else {
		/* policy == FORCE, check hdr/hlg policy */
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SRC) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SRC) ? 2 : 0;
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SINK) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SINK) ? 2 : 0;
		ret |= (dolby_vision_hdr10_policy & SDR_BY_DV_F_SINK) ? 0x20 : 0;
		ret |= (dolby_vision_hdr10_policy & SDR_BY_DV_F_SRC) ? 0x40 : 0;
	}
	return ret;
}
EXPORT_SYMBOL(get_dolby_vision_hdr_policy);

void dolby_vision_update_backlight(void)
{
}
EXPORT_SYMBOL(dolby_vision_update_backlight);

static void parse_param_amdolby_vision(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
		if (n >= MAX_PARAM)
			break;
	}
}

static inline s16 clamps(s16 value, s16 v_min, s16 v_max)
{
if (value > v_max)
return v_max;
if (value < v_min)
return v_min;
return value;
}

/*to do*/
static void update_vsvdb_to_rx(void)
{
	u8 *p = cfg_info[cur_pic_mode].vsvdb;

	if (memcmp(&current_vsvdb[0], p, sizeof(current_vsvdb))) {
		memcpy(&current_vsvdb[0], p, sizeof(current_vsvdb));
		pr_info("%s: %d %d %d %d %d %d %d\n",
				__func__, p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
	}
}

static void update_cp_cfg(void)
{
	unsigned long lockflags;
	struct target_config *tdc;

	if (cur_pic_mode >= num_picture_mode || num_picture_mode == 0 ||
		!bin_to_cfg) {
		pr_info("%s, invalid para %d/%d, bin_to_cfg %p",
				__func__, cur_pic_mode, num_picture_mode, bin_to_cfg);
		return;
	}

	memcpy(pq_config_fake,
		   &bin_to_cfg[cur_pic_mode],
		   sizeof(struct pq_config));
	tdc = &(((struct pq_config *)pq_config_fake)->tdc);
	tdc->d_brightness = cfg_info[cur_pic_mode].brightness;
	tdc->d_contrast = cfg_info[cur_pic_mode].contrast;
	tdc->d_color_shift = cfg_info[cur_pic_mode].colorshift;
	tdc->d_saturation = cfg_info[cur_pic_mode].saturation;

	if (debug_tprimary) {
		tdc->t_primaries[0] = cur_debug_tprimary[0][0]; /*rx*/
		tdc->t_primaries[1] = cur_debug_tprimary[0][1]; /*ry*/
		tdc->t_primaries[2] = cur_debug_tprimary[1][0]; /*gx*/
		tdc->t_primaries[3] = cur_debug_tprimary[1][1]; /*gy*/
		tdc->t_primaries[4] = cur_debug_tprimary[2][0]; /*bx*/
		tdc->t_primaries[5] = cur_debug_tprimary[2][1]; /*by*/
	}

	spin_lock_irqsave(&cfg_lock, lockflags);
	need_update_cfg = true;
	spin_unlock_irqrestore(&cfg_lock, lockflags);
}

/*0: reset picture mode and reset pq for all picture mode*/
/*1: reset pq for all picture mode*/
/*2: reset pq for current picture mode */
static void restore_dv_pq_setting(enum pq_reset_e pq_reset)
{
	int mode = 0;

	if (pq_reset == RESET_ALL)
		cur_pic_mode = default_pic_mode;

	for (mode = 0; mode < num_picture_mode; mode++) {
		if (pq_reset == RESET_PQ_FOR_CUR && mode != cur_pic_mode)
			continue;
		cfg_info[mode].brightness =
				bin_to_cfg[mode].tdc.d_brightness;
		cfg_info[mode].contrast =
				bin_to_cfg[mode].tdc.d_contrast;
		cfg_info[mode].colorshift =
				bin_to_cfg[mode].tdc.d_color_shift;
		cfg_info[mode].saturation =
				bin_to_cfg[mode].tdc.d_saturation;
		memcpy(cfg_info[mode].vsvdb,
			   bin_to_cfg[mode].tdc.vsvdb,
			   sizeof(cfg_info[mode].vsvdb));
	}
	cur_debug_tprimary[0][0] =
			bin_to_cfg[0].tdc.t_primaries[0];
	cur_debug_tprimary[0][1] =
			bin_to_cfg[0].tdc.t_primaries[1];
	cur_debug_tprimary[1][0] =
			bin_to_cfg[0].tdc.t_primaries[2];
	cur_debug_tprimary[1][1] =
			bin_to_cfg[0].tdc.t_primaries[3];
	cur_debug_tprimary[2][0] =
			bin_to_cfg[0].tdc.t_primaries[4];
	cur_debug_tprimary[2][1] =
			bin_to_cfg[0].tdc.t_primaries[5];

	update_cp_cfg();
	if (debug_dolby & 0x200)
		pr_info("reset pq %d\n", pq_reset);
}

#define MAX_READ_SIZE 256

/*internal range -> external range*/
static s16 map_pq_inter_to_exter
		(enum pq_item_e pq_item,
		 s16 inter_value)
{
	s16 inter_pq_min;
	s16 inter_pq_max;
	s16 exter_pq_min;
	s16 exter_pq_max;
	s16 exter_value;
	s16 inter_range;
	s16 exter_range;
	s16 tmp;
	s16 left_range = pq_range[pq_item].left;
	s16 right_range = pq_range[pq_item].right;
	s16 center = pq_center[cur_pic_mode][pq_item];

	if (inter_value >= center) {
		exter_pq_min = 0;
		exter_pq_max = EXTER_MAX_PQ;
		inter_pq_min = center;
		inter_pq_max = right_range;
	} else {
		exter_pq_min = EXTER_MIN_PQ;
		exter_pq_max = 0;
		inter_pq_min = left_range;
		inter_pq_max = center;
	}
	inter_pq_min = clamps(inter_pq_min, INTER_MIN_PQ, INTER_MAX_PQ);
	inter_pq_max = clamps(inter_pq_max, INTER_MIN_PQ, INTER_MAX_PQ);
	tmp = clamps(inter_value, inter_pq_min, inter_pq_max);

	inter_range = inter_pq_max - inter_pq_min;
	exter_range = exter_pq_max - exter_pq_min;

	if (inter_range == 0) {
		inter_range = INTER_MAX_PQ;
		pr_info("invalid inter_range, set to INTER_MAX_PQ\n");
	}

	exter_value = exter_pq_min +
				  (tmp - inter_pq_min) * exter_range / inter_range;

	if (debug_dolby & 0x200)
		pr_info("%s: %s [%d]->[%d]\n",
				__func__, pq_item_str[pq_item],
				inter_value, exter_value);
	return exter_value;
}

/*external range -> internal range*/
static s16 map_pq_exter_to_inter
		(enum pq_item_e pq_item,
		 s16 exter_value)
{
	s16 inter_pq_min;
	s16 inter_pq_max;
	s16 exter_pq_min;
	s16 exter_pq_max;
	s16 inter_value;
	s16 inter_range;
	s16 exter_range;
	s16 tmp;
	s16 left_range = pq_range[pq_item].left;
	s16 right_range = pq_range[pq_item].right;
	s16 center = pq_center[cur_pic_mode][pq_item];

	if (exter_value >= 0) {
		exter_pq_min = 0;
		exter_pq_max = EXTER_MAX_PQ;
		inter_pq_min = center;
		inter_pq_max = right_range;
	} else {
		exter_pq_min = EXTER_MIN_PQ;
		exter_pq_max = 0;
		inter_pq_min = left_range;
		inter_pq_max = center;
	}

	inter_pq_min = clamps(inter_pq_min, INTER_MIN_PQ, INTER_MAX_PQ);
	inter_pq_max = clamps(inter_pq_max, INTER_MIN_PQ, INTER_MAX_PQ);
	tmp = clamps(exter_value, exter_pq_min, exter_pq_max);

	inter_range = inter_pq_max - inter_pq_min;
	exter_range = exter_pq_max - exter_pq_min;

	inter_value = inter_pq_min +
				  (tmp - exter_pq_min) * inter_range / exter_range;

	if (debug_dolby & 0x200)
		pr_info("%s: %s [%d]->[%d]\n",
				__func__, pq_item_str[pq_item],
				exter_value, inter_value);
	return inter_value;
}

static bool is_valid_pic_mode(int mode)
{
	if (mode < num_picture_mode && mode >= 0)
		return true;
	else
		return false;
}

static bool is_valid_pq_exter_value(s16 exter_value)
{
	if (exter_value <= EXTER_MAX_PQ && exter_value >= EXTER_MIN_PQ)
		return true;

	pr_info("pq %d is out of range[%d, %d]\n",
			exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
	return false;
}

static bool is_valid_pq_inter_value(s16 exter_value)
{
	if (exter_value <= INTER_MAX_PQ && exter_value >= INTER_MIN_PQ)
		return true;

	pr_info("pq %d is out of range[%d, %d]\n",
			exter_value, INTER_MIN_PQ, INTER_MAX_PQ);
	return false;
}

static void set_pic_mode(int mode)
{
	if (cur_pic_mode != mode) {
		cur_pic_mode = mode;
		update_cp_cfg();
		update_vsvdb_to_rx();
	}
}

static int get_pic_mode_num(void)
{
	return num_picture_mode;
}

static int get_pic_mode(void)
{
	return cur_pic_mode;
}

static char *get_pic_mode_name(int mode)
{
	if (!is_valid_pic_mode(mode))
		return "errmode";

	return cfg_info[mode].pic_mode_name;
}

static s16 get_single_pq_value(int mode, enum pq_item_e item)
{
	s16 exter_value = 0;
	s16 inter_value = 0;

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return exter_value;
	}
	switch (item) {
		case PQ_BRIGHTNESS:
			inter_value = cfg_info[mode].brightness;
			break;
		case PQ_CONTRAST:
			inter_value = cfg_info[mode].contrast;
			break;
		case PQ_COLORSHIFT:
			inter_value = cfg_info[mode].colorshift;
			break;
		case PQ_SATURATION:
			inter_value = cfg_info[mode].saturation;
			break;
		default:
			pr_err("err pq_item %d\n", item);
			break;
	}

	exter_value = map_pq_inter_to_exter(item, inter_value);

	if (debug_dolby & 0x200)
		pr_info("%s: mode:%d, item:%s, [inter: %d, exter: %d]\n",
				__func__, mode, pq_item_str[item],
				inter_value, exter_value);
	return exter_value;
}

static struct dv_full_pq_info_s get_full_pq_value(int mode)
{
	s16 exter_value = 0;
	struct dv_full_pq_info_s full_pq_info = {mode, 0, 0, 0, 0};

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return full_pq_info;
	}

	exter_value = map_pq_inter_to_exter(PQ_BRIGHTNESS,
										cfg_info[mode].brightness);
	full_pq_info.brightness = exter_value;

	exter_value = map_pq_inter_to_exter(PQ_CONTRAST,
										cfg_info[mode].contrast);
	full_pq_info.contrast = exter_value;

	exter_value = map_pq_inter_to_exter(PQ_COLORSHIFT,
										cfg_info[mode].colorshift);
	full_pq_info.colorshift = exter_value;

	exter_value = map_pq_inter_to_exter(PQ_SATURATION,
										cfg_info[mode].saturation);
	full_pq_info.saturation = exter_value;

	if (debug_dolby & 0x200) {
		pr_info("----------%s: mode:%d-----------\n", __func__, mode);
		pr_info("%s: value[inter: %d, exter: %d]\n",
				pq_item_str[PQ_BRIGHTNESS],
				cfg_info[mode].brightness,
				full_pq_info.brightness);
		pr_info("%s: value[inter: %d, exter: %d]\n",
				pq_item_str[PQ_CONTRAST],
				cfg_info[mode].contrast,
				full_pq_info.contrast);
		pr_info("%s: value[inter: %d, exter: %d]\n",
				pq_item_str[PQ_COLORSHIFT],
				cfg_info[mode].colorshift,
				full_pq_info.colorshift);
		pr_info("%s: value[inter: %d, exter: %d]\n",
				pq_item_str[PQ_SATURATION],
				cfg_info[mode].saturation,
				full_pq_info.saturation);
	}

	return full_pq_info;
}

static void set_single_pq_value(int mode, enum pq_item_e item, s16 value)
{
	s16 inter_value = 0;
	s16 exter_value = value;
	bool pq_changed = false;

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return;
	}
	if (!set_inter_pq) {
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
					clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps %s to %d\n",
					pq_item_str[item], exter_value);
		}
		inter_value = map_pq_exter_to_inter(item, exter_value);
		if (debug_dolby & 0x200) {
			pr_info("%s: mode:%d, item:%s, [inter:%d, exter:%d]\n",
					__func__, mode, pq_item_str[item],
					inter_value, exter_value);
		}
	} else {
		inter_value = value;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
					clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps %s to %d\n",
					pq_item_str[item], inter_value);
		}
		if (debug_dolby & 0x200) {
			exter_value = map_pq_inter_to_exter(item, inter_value);
			pr_info("%s: mode:%d, item:%s, [inter:%d, exter:%d]\n",
					__func__, mode, pq_item_str[item],
					inter_value, exter_value);
		}
	}
	switch (item) {
		case PQ_BRIGHTNESS:
			if (cfg_info[cur_pic_mode].brightness != inter_value) {
				cfg_info[cur_pic_mode].brightness = inter_value;
				pq_changed = true;
			}
			break;
		case PQ_CONTRAST:
			if (cfg_info[cur_pic_mode].contrast != inter_value) {
				cfg_info[cur_pic_mode].contrast = inter_value;
				pq_changed = true;
			}
			break;
		case PQ_COLORSHIFT:
			if (cfg_info[cur_pic_mode].colorshift != inter_value) {
				cfg_info[cur_pic_mode].colorshift = inter_value;
				pq_changed = true;
			}
			break;
		case PQ_SATURATION:
			if (cfg_info[cur_pic_mode].saturation != inter_value) {
				cfg_info[cur_pic_mode].saturation = inter_value;
				pq_changed = true;
			}
			break;
		default:
			pr_err("err pq_item %d\n", item);
			break;
	}
	if (pq_changed)
		update_cp_cfg();
}

static void set_full_pq_value(struct dv_full_pq_info_s full_pq_info)
{
	s16 inter_value = 0;
	s16 exter_value = 0;
	int mode = full_pq_info.pic_mode_id;

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return;
	}
	/*------------set brightness----------*/
	if (!set_inter_pq) {
		exter_value = full_pq_info.brightness;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
					clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps brightness from %d to %d\n",
					full_pq_info.brightness, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_BRIGHTNESS, exter_value);
	} else {
		inter_value = full_pq_info.brightness;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
					clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps brightness from %d to %d\n",
					full_pq_info.brightness, inter_value);
		}
	}
	cfg_info[cur_pic_mode].brightness = inter_value;

	/*-------------set contrast-----------*/
	if (!set_inter_pq) {
		exter_value = full_pq_info.contrast;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
					clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps contrast from %d to %d\n",
					full_pq_info.contrast, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_CONTRAST, exter_value);
	} else {
		inter_value = full_pq_info.contrast;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
					clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps contrast from %d to %d\n",
					full_pq_info.contrast, inter_value);
		}
	}
	cfg_info[cur_pic_mode].contrast = inter_value;

	/*-------------set colorshift-----------*/
	if (!set_inter_pq) {
		exter_value = full_pq_info.colorshift;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
					clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps colorshift from %d to %d\n",
					full_pq_info.colorshift, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_COLORSHIFT, exter_value);
	} else {
		inter_value = full_pq_info.colorshift;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
					clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps colorshift from %d to %d\n",
					full_pq_info.colorshift, inter_value);
		}
	}
	cfg_info[cur_pic_mode].colorshift = inter_value;

	/*-------------set colorshift-----------*/
	if (!set_inter_pq) {
		exter_value = full_pq_info.saturation;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
					clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps saturation from %d to %d\n",
					full_pq_info.saturation, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_SATURATION, exter_value);
	} else {
		inter_value = full_pq_info.saturation;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
					clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps saturation from %d to %d\n",
					full_pq_info.saturation, inter_value);
		}
	}
	cfg_info[cur_pic_mode].saturation = inter_value;

	update_cp_cfg();

	if (debug_dolby & 0x200) {
		pr_info("----------%s: mode:%d----------\n", __func__, mode);
		exter_value = map_pq_inter_to_exter(PQ_BRIGHTNESS,
											cfg_info[mode].brightness);
		pr_info("%s: [inter:%d, exter:%d]\n",
				pq_item_str[PQ_BRIGHTNESS],
				cfg_info[mode].brightness,
				exter_value);

		exter_value = map_pq_inter_to_exter(PQ_CONTRAST,
											cfg_info[mode].contrast);
		pr_info("%s: [inter:%d, exter:%d]\n",
				pq_item_str[PQ_CONTRAST],
				cfg_info[mode].contrast,
				exter_value);

		exter_value = map_pq_inter_to_exter(PQ_COLORSHIFT,
											cfg_info[mode].colorshift);
		pr_info("%s: [inter:%d, exter:%d]\n",
				pq_item_str[PQ_COLORSHIFT],
				cfg_info[mode].colorshift,
				exter_value);

		exter_value = map_pq_inter_to_exter(PQ_SATURATION,
											cfg_info[mode].saturation);
		pr_info("%s: [inter:%d, exter:%d]\n",
				pq_item_str[PQ_SATURATION],
				cfg_info[mode].saturation,
				exter_value);
	}
}

static ssize_t amdolby_vision_set_inter_pq_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	return snprintf(buf, 40, "%d\n",
					set_inter_pq);
}

static ssize_t amdolby_vision_set_inter_pq_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)

{
	size_t r;

	pr_info("cmd: %s\n", buf);
	r = kstrtoint(buf, 0, &set_inter_pq);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t amdolby_vision_load_cfg_status_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	return snprintf(buf, 40, "%d\n",
					load_bin_config);
}

static ssize_t amdolby_vision_load_cfg_status_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;
	int value = 0;

	pr_info("%s: cmd: %s\n", __func__, buf);
	r = kstrtoint(buf, 0, &value);
	if (r != 0)
		return -EINVAL;
	load_bin_config = value;
	if (load_bin_config)
		update_cp_cfg();
	else
		pq_config_set_flag = false;
	return count;
}

static ssize_t  amdolby_vision_pq_info_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	int pos = 0;
	int exter_value = 0;
	int inter_value = 0;
	int i = 0;
	const char *pq_usage_str = {
			"\n"
			"------------------------- set pq usage ------------------------\n"
			"echo picture_mode value > /sys/class/amdolby_vision/dv_pq_info;\n"
			"echo brightness value   > /sys/class/amdolby_vision/dv_pq_info;\n"
			"echo contrast value     > /sys/class/amdolby_vision/dv_pq_info;\n"
			"echo colorshift value   > /sys/class/amdolby_vision/dv_pq_info;\n"
			"echo saturation value   > /sys/class/amdolby_vision/dv_pq_info;\n"
			"echo all v1 v2 v3 v4 v5 > /sys/class/amdolby_vision/dv_pq_info;\n"
			"echo reset value        > /sys/class/amdolby_vision/dv_pq_info;\n"
			"\n"
			"picture_mode range: [0, num_picture_mode-1]\n"
			"brightness   range: [-256, 256]\n"
			"contrast     range: [-256, 256]\n"
			"colorshift   range: [-256, 256]\n"
			"saturation   range: [-256, 256]\n"
			"reset            0: reset pict mode/all pq for all pict mode]\n"
			"                 1: reset pq for all picture mode]\n"
			"                 2: reset pq for current picture mode]\n"
			"---------------------------------------------------------------\n"
	};

	if (!load_bin_config) {
		pr_info("no load_bin_config\n");
		return 0;
	}

	pos += sprintf(buf + pos,
				   "\n==== show current picture mode info ====\n");

	pos += sprintf(buf + pos,
				   "num_picture_mode:          [%d]\n", num_picture_mode);
	for (i = 0; i < num_picture_mode; i++) {
		pos += sprintf(buf + pos,
					   "picture_mode   %d:          [%s]\n",
					   cfg_info[i].id, cfg_info[i].pic_mode_name);
	}
	pos += sprintf(buf + pos, "\n\n");
	pos += sprintf(buf + pos,
				   "current picture mode:      [%d]\n", cur_pic_mode);

	pos += sprintf(buf + pos,
				   "current picture mode name: [%s]\n",
				   cfg_info[cur_pic_mode].pic_mode_name);

	inter_value = cfg_info[cur_pic_mode].brightness;
	exter_value = map_pq_inter_to_exter(PQ_BRIGHTNESS, inter_value);
	pos += sprintf(buf + pos,
				   "current brightness:        [inter:%d][exter:%d]\n",
				   inter_value, exter_value);

	inter_value = cfg_info[cur_pic_mode].contrast;
	exter_value = map_pq_inter_to_exter(PQ_CONTRAST, inter_value);
	pos += sprintf(buf + pos,
				   "current contrast:          [inter:%d][exter:%d]\n",
				   inter_value, exter_value);

	inter_value = cfg_info[cur_pic_mode].colorshift;
	exter_value = map_pq_inter_to_exter(PQ_COLORSHIFT, inter_value);
	pos += sprintf(buf + pos,
				   "current colorshift:        [inter:%d][exter:%d]\n",
				   inter_value, exter_value);

	inter_value = cfg_info[cur_pic_mode].saturation;
	exter_value = map_pq_inter_to_exter(PQ_SATURATION, inter_value);
	pos += sprintf(buf + pos,
				   "current saturation:        [inter:%d][exter:%d]\n",
				   inter_value, exter_value);
	pos += sprintf(buf + pos, "========================================\n");

	pos += sprintf(buf + pos, "%s\n", pq_usage_str);

	return pos;
}

static ssize_t amdolby_vision_pq_info_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig;
	char *parm[MAX_PARAM] = {NULL};
	struct dv_full_pq_info_s full_pq_info;
	enum pq_reset_e pq_reset;
	int val = 0;
	int val1 = 0;
	int val2 = 0;
	int val3 = 0;
	int val4 = 0;
	int val5 = 0;
	int item = -1;

	if (!load_bin_config)
		return count;
	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);

	pr_info("%s: cmd: %s\n", __func__, buf_orig);
	parse_param_amdolby_vision(buf_orig, (char **)&parm);
	if (!parm[0] || !parm[1])
		goto ERR;

	if (!strcmp(parm[0], "picture_mode")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;
		if (is_valid_pic_mode(val))
			set_pic_mode(val);
		else
			pr_info("err picture_mode\n");
	} else if (!strcmp(parm[0], "brightness")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;

		item = PQ_BRIGHTNESS;
	} else if (!strcmp(parm[0], "contrast")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;

		item = PQ_CONTRAST;
	} else if (!strcmp(parm[0], "colorshift")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;

		item = PQ_COLORSHIFT;
	} else if (!strcmp(parm[0], "saturation")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;
		item = PQ_SATURATION;
	} else if (!strcmp(parm[0], "all")) {
		if (kstrtoint(parm[1], 10, &val1) != 0 ||
			kstrtoint(parm[2], 10, &val2) != 0 ||
			kstrtoint(parm[3], 10, &val3) != 0 ||
			kstrtoint(parm[4], 10, &val4) != 0 ||
			kstrtoint(parm[5], 10, &val5) != 0)
			goto ERR;
		full_pq_info.pic_mode_id = val1;
		full_pq_info.brightness = val2;
		full_pq_info.contrast = val3;
		full_pq_info.colorshift = val4;
		full_pq_info.saturation = val5;
		set_full_pq_value(full_pq_info);
	} else if (!strcmp(parm[0], "reset")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;
		pq_reset = val;
		restore_dv_pq_setting(pq_reset);
	} else {
		pr_info("unsupport cmd\n");
	}
	if (item != -1)
		set_single_pq_value(cur_pic_mode, item, val);
	ERR:
	kfree(buf_orig);
	return count;
}

static ssize_t amdolby_vision_bin_config_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	int mode = 0;
	struct pq_config *pq_config = NULL;
	struct target_config *config = NULL;

	if (!load_bin_config) {
		pr_info("no load_bin_config\n");
		return 0;
	}

	pr_info("There are %d picture modes\n", num_picture_mode);
	for (mode = 0; mode < num_picture_mode; mode++) {
		pr_info("================ Picture Mode: %s =================\n",
				cfg_info[mode].pic_mode_name);
		pq_config = &bin_to_cfg[mode];
		config = &pq_config->tdc;
		pr_info("gamma:                %d\n",
				config->gamma);
		pr_info("eotf:                 %d-%s\n",
				config->eotf,
				eotf_str[config->eotf]);
		pr_info("range_spec:           %d\n",
				config->range_spec);
		pr_info("max_pq:               %d\n",
				config->max_pq);
		pr_info("min_pq:               %d\n",
				config->min_pq);
		pr_info("max_pq_dm3:           %d\n",
				config->max_pq_dm3);
		pr_info("min_lin:              %d\n",
				config->min_lin);
		pr_info("max_lin:              %d\n",
				config->max_lin);
		pr_info("max_lin_dm3:          %d\n",
				config->max_lin_dm3);
		pr_info("tPrimaries:           %d,%d,%d,%d,%d,%d,%d,%d\n",
				config->t_primaries[0],
				config->t_primaries[1],
				config->t_primaries[2],
				config->t_primaries[3],
				config->t_primaries[4],
				config->t_primaries[5],
				config->t_primaries[6],
				config->t_primaries[7]);
		pr_info("m_sweight:             %d\n",
				config->m_sweight);
		pr_info("trim_slope_bias:       %d\n",
				config->trim_slope_bias);
		pr_info("trim_offset_bias:      %d\n",
				config->trim_offset_bias);
		pr_info("trim_power_bias:       %d\n",
				config->trim_power_bias);
		pr_info("ms_weight_bias:        %d\n",
				config->ms_weight_bias);
		pr_info("chroma_weight_bias:    %d\n",
				config->chroma_weight_bias);
		pr_info("saturation_gain_bias:  %d\n",
				config->saturation_gain_bias);
		pr_info("d_brightness:          %d\n",
				config->d_brightness);
		pr_info("d_contrast:            %d\n",
				config->d_contrast);
		pr_info("d_color_shift:         %d\n",
				config->d_color_shift);
		pr_info("d_saturation:          %d\n",
				config->d_saturation);
		pr_info("d_backlight:           %d\n",
				config->d_backlight);
		pr_info("dbg_exec_paramsprint:  %d\n",
				config->dbg_exec_params_print_period);
		pr_info("dbg_dm_md_print_period:%d\n",
				config->dbg_dm_md_print_period);
		pr_info("dbg_dmcfg_print_period:%d\n",
				config->dbg_dm_cfg_print_period);
		pr_info("\n");
		pr_info("------ Global Dimming configuration ------\n");
		pr_info("gd_enable:            %d\n",
				config->gd_config.gd_enable);
		pr_info("gd_wmin:              %d\n",
				config->gd_config.gd_wmin);
		pr_info("gd_wmax:              %d\n",
				config->gd_config.gd_wmax);
		pr_info("gd_wmm:               %d\n",
				config->gd_config.gd_wmm);
		pr_info("gd_wdyn_rng_sqrt:     %d\n",
				config->gd_config.gd_wdyn_rng_sqrt);
		pr_info("gd_weight_mean:       %d\n",
				config->gd_config.gd_weight_mean);
		pr_info("gd_weight_std:        %d\n",
				config->gd_config.gd_weight_std);
		pr_info("gd_delay_ms_hdmi:     %d\n",
				config->gd_config.gd_delay_msec_hdmi);
		pr_info("gd_rgb2yuv_ext:       %d\n",
				config->gd_config.gd_rgb2yuv_ext);
		pr_info("gd_wdyn_rng_sqrt:     %d\n",
				config->gd_config.gd_wdyn_rng_sqrt);
		pr_info("gd_m33_rgb2yuv:       %d,%d,%d\n",
				config->gd_config.gd_m33_rgb2yuv[0][0],
				config->gd_config.gd_m33_rgb2yuv[0][1],
				config->gd_config.gd_m33_rgb2yuv[0][2]);
		pr_info("                      %d,%d,%d\n",
				config->gd_config.gd_m33_rgb2yuv[1][0],
				config->gd_config.gd_m33_rgb2yuv[1][1],
				config->gd_config.gd_m33_rgb2yuv[1][2]);
		pr_info("                      %d,%d,%d\n",
				config->gd_config.gd_m33_rgb2yuv[2][0],
				config->gd_config.gd_m33_rgb2yuv[2][1],
				config->gd_config.gd_m33_rgb2yuv[2][2]);
		pr_info("gd_m33_rgb2yuv_scale2p:%d\n",
				config->gd_config.gd_m33_rgb2yuv_scale2p);
		pr_info("gd_rgb2yuv_off_ext:    %d\n",
				config->gd_config.gd_rgb2yuv_off_ext);
		pr_info("gd_rgb2yuv_off:        %d,%d,%d\n",
				config->gd_config.gd_rgb2yuv_off[0],
				config->gd_config.gd_rgb2yuv_off[1],
				config->gd_config.gd_rgb2yuv_off[2]);
		pr_info("gd_up_bound:           %d\n",
				config->gd_config.gd_up_bound);
		pr_info("gd_low_bound:          %d\n",
				config->gd_config.gd_low_bound);
		pr_info("last_max_pq:           %d\n",
				config->gd_config.last_max_pq);
		pr_info("gd_wmin_pq:            %d\n",
				config->gd_config.gd_wmin_pq);
		pr_info("gd_wmax_pq:            %d\n",
				config->gd_config.gd_wmax_pq);
		pr_info("gd_wm_pq:              %d\n",
				config->gd_config.gd_wm_pq);
		pr_info("gd_trigger_period:     %d\n",
				config->gd_config.gd_trigger_period);
		pr_info("gd_trigger_lin_thresh: %d\n",
				config->gd_config.gd_trigger_lin_thresh);
		pr_info("gdDelayMilliSec_ott:   %d\n",
				config->gd_config.gd_delay_msec_ott);
		pr_info("gd_rise_weight:        %d\n",
				config->gd_config.gd_rise_weight);
		pr_info("gd_fall_weight:        %d\n",
				config->gd_config.gd_fall_weight);
		pr_info("gd_delay_msec_ll:      %d\n",
				config->gd_config.gd_delay_msec_ll);
		pr_info("gd_contrast:           %d\n",
				config->gd_config.gd_contrast);
		pr_info("\n");

		pr_info("------ Adaptive boost configuration ------\n");
		pr_info("ab_enable:             %d\n",
				config->ab_config.ab_enable);
		pr_info("ab_highest_tmax:       %d\n",
				config->ab_config.ab_highest_tmax);
		pr_info("ab_lowest_tmax:        %d\n",
				config->ab_config.ab_lowest_tmax);
		pr_info("ab_rise_weight:        %d\n",
				config->ab_config.ab_rise_weight);
		pr_info("ab_fall_weight:        %d\n",
				config->ab_config.ab_fall_weight);
		pr_info("ab_delay_msec_hdmi:    %d\n",
				config->ab_config.ab_delay_msec_hdmi);
		pr_info("ab_delay_msec_ott:     %d\n",
				config->ab_config.ab_delay_msec_ott);
		pr_info("ab_delay_msec_ll:      %d\n",
				config->ab_config.ab_delay_msec_ll);
		pr_info("\n");

		pr_info("------- Ambient light configuration ------\n");
		pr_info("ambient:            %d\n",
				config->ambient_config.ambient);
		pr_info("t_front_lux:        %d\n",
				config->ambient_config.t_front_lux);
		pr_info("t_front_lux_scale:  %d\n",
				config->ambient_config.t_front_lux_scale);
		pr_info("t_rear_lum:         %d\n",
				config->ambient_config.t_rear_lum);
		pr_info("t_rear_lum_scale:   %d\n",
				config->ambient_config.t_rear_lum_scale);
		pr_info("t_whitexy:          %d, %d\n",
				config->ambient_config.t_whitexy[0],
				config->ambient_config.t_whitexy[1]);
		pr_info("t_surround_reflecti:%d\n",
				config->ambient_config.t_surround_reflection);
		pr_info("t_screen_reflection:%d\n",
				config->ambient_config.t_screen_reflection);
		pr_info("al_delay:           %d\n",
				config->ambient_config.al_delay);
		pr_info("al_rise:            %d\n",
				config->ambient_config.al_rise);
		pr_info("al_fall:            %d\n",
				config->ambient_config.al_fall);

		pr_info("vsvdb:              %x,%x,%x,%x,%x,%x,%x\n",
				config->vsvdb[0],
				config->vsvdb[1],
				config->vsvdb[2],
				config->vsvdb[3],
				config->vsvdb[4],
				config->vsvdb[5],
				config->vsvdb[6]);
		pr_info("\n");
		pr_info("---------------- ocsc_config --------------\n");
		pr_info("lms2rgb_mat:        %d,%d,%d\n",
				config->ocsc_config.lms2rgb_mat[0][0],
				config->ocsc_config.lms2rgb_mat[0][1],
				config->ocsc_config.lms2rgb_mat[0][2]);
		pr_info("                    %d,%d,%d\n",
				config->ocsc_config.lms2rgb_mat[1][0],
				config->ocsc_config.lms2rgb_mat[1][1],
				config->ocsc_config.lms2rgb_mat[1][2]);
		pr_info("                    %d,%d,%d\n",
				config->ocsc_config.lms2rgb_mat[2][0],
				config->ocsc_config.lms2rgb_mat[2][1],
				config->ocsc_config.lms2rgb_mat[2][2]);
		pr_info("lms2rgb_mat_scale:  %d\n",
				config->ocsc_config.lms2rgb_mat_scale);
		pr_info("\n");

		pr_info("dm31_avail:         %d\n",
				config->dm31_avail);
		pr_info("bright_preservation:%d\n",
				config->bright_preservation);
		pr_info("total_viewing_modes:%d\n",
				config->total_viewing_modes_num);
		pr_info("viewing_mode_valid: %d\n",
				config->viewing_mode_valid);
		pr_info("\n");
	}
	return 0;
}

int register_dv_functions(const struct dolby_vision_func_s *func)
{
	int ret = -1;
	unsigned int reg_value;
	const struct vinfo_s *vinfo = get_current_vinfo();
	unsigned int ko_info_len = 0;

	if (dolby_vision_probe_ok == 0) {
		pr_info("error:(%s) dv probe fail cannot register\n", __func__);
		return -ENOMEM;
	}

	/*when dv ko load into kernel, this flag will be disabled
	 *otherwise it will effect hdr module
	 */
	if (dolby_vision_on_in_uboot) {
		if (is_vinfo_available(vinfo))
			is_sink_cap_changed(vinfo,
								&current_hdr_cap,
								&current_sink_available);
		else
			pr_info("sink not available\n");
		dolby_vision_on = true;
		dolby_vision_wait_on = false;
		dolby_vision_wait_init = false;
		dolby_vision_on_in_uboot = 0;
	}

	if (!p_funcs_stb && func) {
		if (func->control_path && !p_funcs_stb) {
			pr_info("*** register_dv_stb_functions.***\n");
			if (!ko_info) {
				ko_info_len = strlen(func->version_info);
				ko_info = vmalloc(ko_info_len + 1);
				if (ko_info) {
					strncpy(ko_info, func->version_info,
							ko_info_len);
					ko_info[ko_info_len] = '\0';
				}
			}
			p_funcs_stb = func;

			dolby_vision_hdr10_policy |= SDR_BY_DV_F_SINK;
			dolby_vision_hdr10_policy |= HDR_BY_DV_F_SINK;
			last_dolby_vision_hdr10_policy = dolby_vision_hdr10_policy;
			if (ko_info)
				pr_info("hdr10_policy %d, ko_info %s\n",
						dolby_vision_hdr10_policy, ko_info);

		} else {
			return ret;
		}
		ret = 0;
		/* get efuse flag*/

		reg_value = READ_VPP_DV_REG(DOLBY_CORE1_REG_START + 1);
		if ((reg_value & 0x100) == 0)
			efuse_mode = 0;
		else
			efuse_mode = 1;

		pr_info("efuse_mode=%d reg_value = 0x%x\n",
				efuse_mode, reg_value);

		support_info = efuse_mode ? 0 : 1;/*bit0=1 => no efuse*/
		support_info = support_info | (1 << 1); /*bit1=1 => ko loaded*/
		support_info = support_info | (1 << 2); /*bit2=1 => updated*/

		pr_info("dv capability %d\n", support_info);

		dovi_setting.src_format = FORMAT_SDR;
		new_dovi_setting.src_format = FORMAT_SDR;

		/*stb core doesn't need run mode*/
		dolby_vision_run_mode_delay = 0;

		adjust_vpotch();

	}
	module_installed = true;
	return ret;
}
EXPORT_SYMBOL(register_dv_functions);

int unregister_dv_functions(void)
{
	int ret = -1;

	module_installed = false;

	if (p_funcs_stb) {
		pr_info("*** %s ***\n", __func__);
		if (pq_config_fake) {
			vfree(pq_config_fake);
			pq_config_fake = NULL;
		}
		if (bin_to_cfg) {
			vfree(bin_to_cfg);
			bin_to_cfg = NULL;
		}
		if (ko_info) {
			vfree(ko_info);
			ko_info = NULL;
		}
		p_funcs_stb = NULL;
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(unregister_dv_functions);

void tv_dolby_vision_crc_clear(int flag)
{
	crc_output_buff_off = 0;
	crc_count = 0;
	crc_bypass_count = 0;
	setting_update_count = 0;
	if (!crc_output_buf)
		crc_output_buf = vmalloc(CRC_BUFF_SIZE);
	pr_info("clear crc_output_buf\n");
	if (crc_output_buf)
		memset(crc_output_buf, 0, CRC_BUFF_SIZE);
	strcpy(cur_crc, "invalid");
}

char *tv_dolby_vision_get_crc(u32 *len)
{
	if (!crc_output_buf || !len || crc_output_buff_off == 0)
		return NULL;
	*len = crc_output_buff_off;
	return crc_output_buf;
}

void tv_dolby_vision_insert_crc(bool print)
{
	char str[64];
	int len;
	bool crc_enable;
	u32 crc;

	if (dolby_vision_flags & FLAG_DISABLE_CRC) {
		crc_bypass_count++;
		crc_count++;
		return;
	}

	crc_enable = true;
	crc = READ_VPP_DV_REG(0x36fd);

	if (debug_dolby & 0x1000)
		pr_dolby_dbg("crc_enable %d, crc %x\n", crc_enable, crc);

	if (crc == 0 || !crc_enable || !crc_output_buf) {
		crc_bypass_count++;
		crc_count++;
		return;
	}

	if (crc_count < crc_bypass_count)
		crc_bypass_count = crc_count;

	memset(str, 0, sizeof(str));
	snprintf(str, 64, "crc(%d) = 0x%08x", crc_count - crc_bypass_count, crc);
	len = strlen(str);
	str[len] = 0xa;
	len++;
	memcpy(&crc_output_buf[crc_output_buff_off], &str[0], len);
	crc_output_buff_off += len;

	pr_info("%s\n", str);
	crc_count++;
}

void tv_dolby_vision_dma_table_modify(u32 tbl_id, u64 value)
{
	u64 *tbl = NULL;

	if (!dma_vaddr || tbl_id >= 3754) {
		pr_info("No dma table %p to write or id %d overflow\n",
				dma_vaddr, tbl_id);
		return;
	}
	tbl = (u64 *)dma_vaddr;
	pr_info("dma_vaddr:%p, modify table[%d]=0x%llx -> 0x%llx\n",
			dma_vaddr, tbl_id, tbl[tbl_id], value);
	tbl[tbl_id] = value;
}

void tv_dolby_vision_efuse_info(void)
{
	pr_info("\n p_funcs is NULL\n");
	pr_info("efuse_mode:%d\n", efuse_mode);
}

void tv_dolby_vision_el_info(void)
{
	pr_info("el_mode:%d\n", el_mode);
}

static int amdolby_vision_open(struct inode *inode, struct file *file)
{
	struct amdolby_vision_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct amdolby_vision_dev_s, cdev);
	file->private_data = devp;
	return 0;

}

static char *pq_config_buf;
static u32 pq_config_level;
static ssize_t amdolby_vision_write
		(struct file *file,
		 const char __user *buf,
		 size_t len,
		loff_t *off)
{
	int max_len, w_len;

	if (!pq_config_buf) {
		pq_config_buf = vmalloc(108 * 1024);
		pq_config_level = 0;
		if (!pq_config_buf)
			return -ENOSPC;
	}
	max_len = sizeof(struct pq_config) - pq_config_level;
	w_len = len < max_len ? len : max_len;

	pr_info("write len %d, w_len %d, level %d\n", (int)len, w_len, pq_config_level);
	if (copy_from_user(pq_config_buf + pq_config_level, buf, w_len))
		return -EFAULT;

	pq_config_level += w_len;
	if (pq_config_level == sizeof(struct pq_config)) {
		dolby_vision_update_pq_config(pq_config_buf);
		pq_config_level = 0;
	}

	if (len <= 0x1f) {
		dolby_vision_update_vsvdb_config(pq_config_buf, len);
		pq_config_level = 0;
	}
	return len;
}

static ssize_t amdolby_vision_read
		(struct file *file, char __user *buf,
size_t count, loff_t *ppos)
{
char *out;
u32 data_size = 0, res, ret_val = -1;

if (!is_dolby_vision_enable())
return ret_val;
out = tv_dolby_vision_get_crc(&data_size);
if (data_size > CRC_BUFF_SIZE) {
pr_err("crc_output_buff_off is out of bound\n");
tv_dolby_vision_crc_clear(0);
return ret_val;
}

if (out && data_size > 0) {
res = copy_to_user((void *)buf,
				   (void *)out,
				   data_size);
ret_val = data_size - res;
pr_info
("%s crc size %d, res: %d, ret: %d\n",
__func__, data_size, res, ret_val);
tv_dolby_vision_crc_clear(0);
}
return ret_val;
}

static int amdolby_vision_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long amdolby_vision_ioctl(struct file *file,
								 unsigned int cmd, unsigned long arg)
{
#define MAX_BYTES (256)
	int ret = 0;
	int mode_num = 0;
	int mode_id = 0;
	s16 pq_value = 0;
	enum pq_item_e pq_item;
	enum pq_reset_e pq_reset;

	struct pic_mode_info_s pic_info;
	struct dv_pq_info_s pq_info;
	struct dv_full_pq_info_s pq_full_info;
	struct dv_config_file_s config_file;
	void __user *argp = (void __user *)arg;

	if (debug_dolby & 0x200)
		pr_info("[DV]: %s: cmd_nr = 0x%x\n",
				__func__, _IOC_NR(cmd));

	if (!module_installed) {
		pr_info("[DV] module not install\n");
		return ret;
	}

	if (!load_bin_config && cmd != DV_IOC_SET_DV_CONFIG_FILE) {
		pr_info("[DV] no config file, pq ioctl disable!\n");
		return ret;
	}
	switch (cmd) {
		case DV_IOC_GET_DV_PIC_MODE_NUM:
			mode_num = get_pic_mode_num();
			put_user(mode_num, (u32 __user *)argp);
			break;
		case DV_IOC_GET_DV_PIC_MODE_NAME:
			if (copy_from_user(&pic_info, argp,
							   sizeof(struct pic_mode_info_s)) == 0) {
				mode_id = pic_info.pic_mode_id;
				strcpy(pic_info.name, get_pic_mode_name(mode_id));
				if (debug_dolby & 0x200)
					pr_info("[DV]: get mode %d, name %s\n",
							pic_info.pic_mode_id, pic_info.name);
				if (copy_to_user(argp,
								 &pic_info,
								 sizeof(struct pic_mode_info_s)))
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}
			break;
		case DV_IOC_GET_DV_PIC_MODE_ID:
			mode_id = get_pic_mode();
			put_user(mode_id, (u32 __user *)argp);
			break;
		case DV_IOC_SET_DV_PIC_MODE_ID:
			if (copy_from_user(&mode_id, argp, sizeof(s32)) == 0) {
				if (debug_dolby & 0x200)
					pr_info("[DV]: set mode %d\n", mode_id);
				set_pic_mode(mode_id);
			} else {
				ret = -EFAULT;
			}
			break;
		case DV_IOC_GET_DV_SINGLE_PQ_VALUE:
			if (copy_from_user(&pq_info, argp,
							   sizeof(struct dv_pq_info_s)) == 0) {
				mode_id = pq_info.pic_mode_id;
				pq_item = pq_info.item;
				pq_info.value = get_single_pq_value(mode_id, pq_item);

				if (debug_dolby & 0x200)
					pr_info("[DV]: get mode %d, pq %s, value %d\n",
							mode_id,
							pq_item_str[pq_item],
							pq_info.value);

				if (copy_to_user(argp,
								 &pq_info,
								 sizeof(struct dv_pq_info_s)))
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}
			break;
		case DV_IOC_GET_DV_FULL_PQ_VALUE:
			if (copy_from_user(&pq_full_info, argp,
							   sizeof(struct dv_full_pq_info_s)) == 0) {
				mode_id = pq_full_info.pic_mode_id;
				pq_full_info = get_full_pq_value(mode_id);

				if (copy_to_user(argp,
								 &pq_full_info,
								 sizeof(struct dv_full_pq_info_s)))
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}
			break;
		case DV_IOC_SET_DV_SINGLE_PQ_VALUE:
			if (copy_from_user(&pq_info, argp,
							   sizeof(struct dv_pq_info_s)) == 0) {
				mode_id = pq_info.pic_mode_id;
				pq_item = pq_info.item;
				pq_value = pq_info.value;
				set_single_pq_value(mode_id, pq_item, pq_value);

				if (debug_dolby & 0x200)
					pr_info("[DV]: set mode %d, pq %s, value %d\n",
							mode_id,
							pq_item_str[pq_item],
							pq_value);

			} else {
				ret = -EFAULT;
			}
			break;
		case DV_IOC_SET_DV_FULL_PQ_VALUE:
			if (copy_from_user(&pq_full_info, argp,
							   sizeof(struct dv_full_pq_info_s)) == 0) {
				set_full_pq_value(pq_full_info);
			} else {
				ret = -EFAULT;
			}
			break;
		case DV_IOC_SET_DV_PQ_RESET:
			if (copy_from_user(&pq_reset, argp,
							   sizeof(enum pq_reset_e)) == 0) {
				restore_dv_pq_setting(pq_reset);
				if (debug_dolby & 0x200)
					pr_info("[DV]: reset mode %d\n", pq_reset);
			} else {
				ret = -EFAULT;
			}
			break;
		case DV_IOC_SET_DV_CONFIG_FILE:
			if (copy_from_user(&config_file, argp,
							   sizeof(struct dv_config_file_s)) == 0) {

				if (debug_dolby & 0x200)
					pr_info("[DV]: config_file %s, %s\n",
							config_file.bin_name,
							config_file.cfg_name);
			} else {
				ret = -EFAULT;
			}
			break;
		case DV_IOC_CONFIG_DV_BL:
			if (copy_from_user(&force_disable_dv_backlight, argp, sizeof(s32)) == 0) {
				if (debug_dolby & 0x200)
					pr_info("[DV]: disable dv bl %d\n", force_disable_dv_backlight);

			} else {
				ret = -EFAULT;
			}
			break;
		case DV_IOC_SET_DV_AMBIENT:
			if (copy_from_user(&ambient_config_new, argp,
							   sizeof(struct ambient_cfg_s)) == 0) {
				ambient_update = true;
			} else {
				ret = -EFAULT;
			}
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long amdolby_vision_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = amdolby_vision_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations amdolby_vision_fops = {
		.owner   = THIS_MODULE,
		.open    = amdolby_vision_open,
		.write   = amdolby_vision_write,
		.read = amdolby_vision_read,
		.release = amdolby_vision_release,
		.unlocked_ioctl   = amdolby_vision_ioctl,
#ifdef CONFIG_COMPAT
		.compat_ioctl = amdolby_vision_compat_ioctl,
#endif
		.poll = amdolby_vision_poll,
};

static ssize_t amdolby_vision_ko_info_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	if (ko_info)
		return sprintf(buf, "%s", ko_info);
	else
		return 0;
}

static const char *amdolby_vision_debug_usage_str = {
		"Usage:\n"
		"echo dolby_crc 0/1 > /sys/class/amdolby_vision/debug; dolby_crc insert or clr\n"
		"echo dolby_dma index(D) value(H) > /sys/class/amdolby_vision/debug; dolby dma table modify\n"
		"echo dv_efuse > /sys/class/amdolby_vision/debug; get dv efuse info\n"
		"echo dv_el > /sys/class/amdolby_vision/debug; get dv enhanced layer info\n"
		"echo force_support_emp 1/0 > /sys/class/amdolby_vision/debug; send emp\n"
		"echo set_backlight_delay 0 > /sys/class/amdolby_vision/debug; set backlight no delay\n"
		"echo set_backlight_delay 1 > /sys/class/amdolby_vision/debug; set backlight delay one vysnc\n"
		"echo enable_vpu_probe 1 > /sys/class/amdolby_vision/debug; enable vpu probe\n"
		"echo debug_bypass_vpp_pq 0 > /sys/class/amdolby_vision/debug; not debug mode\n"
		"echo debug_bypass_vpp_pq 1 > /sys/class/amdolby_vision/debug; force disable vpp pq\n"
		"echo debug_bypass_vpp_pq 2 > /sys/class/amdolby_vision/debug; force enable vpp pq\n"
		"echo bypass_all_vpp_pq 1 > /sys/class/amdolby_vision/debug; force bypass vpp pq in cert mode\n"
		"echo enable_vpu_probe 1 > /sys/class/amdolby_vision/debug; enable vpu probe\n"
		"echo ko_info > /sys/class/amdolby_vision/debug; query ko info\n"

};

static ssize_t  amdolby_vision_debug_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",  amdolby_vision_debug_usage_str);
}

static ssize_t amdolby_vision_debug_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};
	long val = 0;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amdolby_vision(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "dolby_crc")) {

		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;

		if (val == 1)
			tv_dolby_vision_crc_clear(val);
		else
			tv_dolby_vision_insert_crc(true);

	} else if (!strcmp(parm[0], "dolby_dma")) {
		long tbl_id;
		long value;

		if (kstrtoul(parm[1], 10, &tbl_id) < 0)
			return -EINVAL;
		if (kstrtoul(parm[2], 16, &value) < 0)
			return -EINVAL;
		tv_dolby_vision_dma_table_modify((u32)tbl_id, (u64)value);
	} else if (!strcmp(parm[0], "dv_efuse")) {
		tv_dolby_vision_efuse_info();
	} else if (!strcmp(parm[0], "dv_el")) {
		tv_dolby_vision_el_info();
	} else if (!strcmp(parm[0], "enable_tunnel")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		enable_tunnel = val;
		pr_info("enable_tunnel %d\n", enable_tunnel);

	} else if (!strcmp(parm[0], "debug_bypass_vpp_pq")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		debug_bypass_vpp_pq = val;
		pr_info("set debug_bypass_vpp_pq %d\n",
				debug_bypass_vpp_pq);
	} else if (!strcmp(parm[0], "force_cert_bypass_vpp_pq")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 0)
			bypass_all_vpp_pq = 0;
		else
			bypass_all_vpp_pq = 1;
		pr_info("set bypass_all_vpp_pq %d\n",
				bypass_all_vpp_pq);
	} else if (!strcmp(parm[0], "enable_fel")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		enable_fel = val;
		is_mel = false;
		pr_info("enable_fel %d\n", enable_fel);
	} else if (!strcmp(parm[0], "ko_info")) {
		if (ko_info)
			pr_info("ko info: %s\n", ko_info);
	} else {
		pr_info("unsupport cmd\n");
	}

	kfree(buf_orig);
	return count;
}

static ssize_t	amdolby_vision_primary_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 100, "debug %d, tprimary %d %d %d %d %d %d\n",
					debug_tprimary,
					cur_debug_tprimary[0][0], cur_debug_tprimary[0][1],
					cur_debug_tprimary[1][0], cur_debug_tprimary[1][1],
					cur_debug_tprimary[2][0], cur_debug_tprimary[2][1]);
}

static ssize_t amdolby_vision_primary_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amdolby_vision(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "primary_r")) {
		if (kstrtoint(parm[1], 10, &cur_debug_tprimary[0][0]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoint(parm[2], 10, &cur_debug_tprimary[0][1]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	} else if (!strcmp(parm[0], "primary_g")) {
		if (kstrtoint(parm[1], 10, &cur_debug_tprimary[1][0]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoint(parm[2], 10, &cur_debug_tprimary[1][1]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	} else if (!strcmp(parm[0], "primary_b")) {
		if (kstrtoint(parm[1], 10, &cur_debug_tprimary[2][0]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoint(parm[2], 10, &cur_debug_tprimary[2][1]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	} else if (!strcmp(parm[0], "debug_tprimary")) {
		if (kstrtoint(parm[1], 10, &debug_tprimary) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	}
	kfree(buf_orig);
	update_cp_cfg();
	return count;
}

static ssize_t	amdolby_vision_config_file_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	const char *str =
			"echo bin cfg > /sys/class/amdolby_vision/config_file";
	return sprintf(buf, "%s\n", str);
}

static ssize_t amdolby_vision_config_file_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amdolby_vision(buf_orig, (char **)&parm);
	if (!parm[0] || !parm[1]) {
		pr_info("missing parameter... param1:bin param2:cfg\n");
		kfree(buf_orig);
		return -EINVAL;
	}
	pr_info("parm[0]: %s, parm[1]: %s\n", parm[0], parm[1]);

	kfree(buf_orig);
	return count;
}

static ssize_t	amdolby_vision_content_fps_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "content_fps: %d\n", content_fps);
}

static ssize_t amdolby_vision_content_fps_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &content_fps);
	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t	amdolby_vision_gd_rf_adjust_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "gd_rf_adjust: %d\n", gd_rf_adjust);
}

static ssize_t amdolby_vision_gd_rf_adjust_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &gd_rf_adjust);
	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t	amdolby_vision_force_disable_bl_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "force disable dv bl: %d\n", force_disable_dv_backlight);
}

static ssize_t amdolby_vision_force_disable_bl_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &force_disable_dv_backlight);
	if (r != 0)
		return -EINVAL;

	pr_info("update force_disable_dv_backlight to %d\n", force_disable_dv_backlight);

	return count;
}

static ssize_t	amdolby_vision_use_cfg_target_lum_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "use_target_lum_from_cfg: %d\n",
				   use_target_lum_from_cfg);
}

static ssize_t amdolby_vision_use_cfg_target_lum_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &use_target_lum_from_cfg);
	if (r != 0)
		return -EINVAL;

	pr_info("update use_target_lum_from_cfg to %d\n",
			use_target_lum_from_cfg);

	return count;
}

/* supported mode: IPT_TUNNEL/HDR10/SDR10 */
static const int dv_mode_table[6] = {
		5, /*DOLBY_VISION_OUTPUT_MODE_BYPASS*/
		0, /*DOLBY_VISION_OUTPUT_MODE_IPT*/
		1, /*DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL*/
		2, /*DOLBY_VISION_OUTPUT_MODE_HDR10*/
		3, /*DOLBY_VISION_OUTPUT_MODE_SDR10*/
		4, /*DOLBY_VISION_OUTPUT_MODE_SDR8*/
};

static const char dv_mode_str[6][12] = {
		"IPT",
		"IPT_TUNNEL",
		"HDR10",
		"SDR10",
		"SDR8",
		"BYPASS"
};

static bool is_recovery_mode(void)
{
	struct file *filp1 = NULL;
	struct file *filp2 = NULL;
	bool  file1_exist = false;
	bool  file2_exist = false;
	const char *recovery_file1 = "/system/bin/recovery";
	const char *recovery_file2 = "/sbin/recovery";

	filp1 = filp_open(recovery_file1, O_RDONLY, 0444);
	if (IS_ERR(filp1)) {
		filp2 = filp_open(recovery_file2, O_RDONLY, 0444);
		if (IS_ERR(filp2)) {
			pr_info("[%s] no file: |%s|%s|\n", __func__, recovery_file1, recovery_file2);
		} else {
			file2_exist = true;
			filp_close(filp2, NULL);
		}
	} else {
		file1_exist = true;
		filp_close(filp1, NULL);
	}
	if (file1_exist || file2_exist)
		return true;
	else
		return false;
}

unsigned int dolby_vision_check_enable(void)
{
	int uboot_dv_mode = 0;
	int uboot_dv_source_led_yuv = 0;
	int uboot_dv_source_led_rgb = 0;
	int uboot_dv_sink_led = 0;
	const struct vinfo_s *vinfo = get_current_vinfo();

	if ((READ_VPP_DV_REG(DOLBY_CORE3_DIAG_CTRL) & 0xff) == 0x20) {
		
		/*LL YUV422 mode*/
		uboot_dv_mode = dv_mode_table[2];
		uboot_dv_source_led_yuv = 1;
	
	} else if ((READ_VPP_DV_REG(DOLBY_CORE3_DIAG_CTRL) & 0xff) == 0x3) {
	
		/*LL RGB444 mode*/
		uboot_dv_mode = dv_mode_table[2];
		uboot_dv_source_led_rgb = 1;
	
	} else {
		if (READ_VPP_DV_REG(DOLBY_CORE3_REG_START + 1) == 2) {
			
			/*HDR10 mode*/
			uboot_dv_mode = dv_mode_table[3];

		} else if (READ_VPP_DV_REG(DOLBY_CORE3_REG_START + 1) == 4) {

			/*SDR mode*/
			uboot_dv_mode = dv_mode_table[5];

		} else {

			/*STANDARD RGB444 mode*/
			uboot_dv_mode = dv_mode_table[2];
			uboot_dv_sink_led = 1;
		}
	}

	/*check if dovi enable in uboot*/

	if (dolby_vision_on_in_uboot) {

		if (is_recovery_mode()) {/*recovery mode*/

			dolby_vision_on = true;
			if (uboot_dv_source_led_yuv || uboot_dv_sink_led) {

#ifdef CONFIG_AMLOGIC_HDMITX
				if (uboot_dv_source_led_yuv)
					setup_attr("422,12bit");
				else
					setup_attr("444,8bit");
#endif

			}

			if (vinfo && vinfo->vout_device && vinfo->vout_device->fresh_tx_vsif_pkt) {
				vinfo->vout_device->fresh_tx_vsif_pkt(0, 0, NULL, true);
			}
			
			enable_dolby_vision(0);
			dolby_vision_on_in_uboot = 0;
			
		} else {

			dolby_vision_enable = 1;

			if (uboot_dv_mode == dv_mode_table[2] && uboot_dv_source_led_yuv == 1) {

				/*LL YUV422 mode*/
				/*set_dolby_vision_mode(dv_mode);*/
				dolby_vision_mode = uboot_dv_mode;
				dolby_vision_status = DV_PROCESS;
				dolby_vision_ll_policy = DOLBY_VISION_LL_YUV422;
				last_dst_format = FORMAT_DOVI;
				pr_info("dovi enable in uboot and mode is LL 422\n");

			} else if ((uboot_dv_mode == dv_mode_table[2]) && (uboot_dv_source_led_rgb == 1)) {

				/*LL RGB444 mode*/
				/*set_dolby_vision_mode(dv_mode);*/				
				dolby_vision_mode = uboot_dv_mode;
				dolby_vision_status = DV_PROCESS;
				dolby_vision_ll_policy = DOLBY_VISION_LL_RGB444;
				last_dst_format = FORMAT_DOVI;
				pr_info("dovi enable in uboot and mode is LL RGB\n");
				
			} else {
				
				if (uboot_dv_mode == dv_mode_table[3]) {
					/*HDR10 mode*/
					dolby_vision_hdr10_policy |= HDR_BY_DV_F_SINK;
					dolby_vision_mode = uboot_dv_mode;
					dolby_vision_status = HDR_PROCESS;
					pr_info("dovi enable in uboot and mode is HDR10\n");
					last_dst_format = FORMAT_HDR10;
				} else if (uboot_dv_mode == dv_mode_table[5]) {
					/*SDR mode*/
					dolby_vision_mode = uboot_dv_mode;
					dolby_vision_status = SDR_PROCESS;
					pr_info("dovi enable in uboot and mode is SDR\n");
					last_dst_format = FORMAT_SDR;
				} else {
					/*STANDARD RGB444 mode*/
					dolby_vision_mode = uboot_dv_mode;
					dolby_vision_status = DV_PROCESS;
					dolby_vision_ll_policy = DOLBY_VISION_LL_DISABLE;
					last_dst_format = FORMAT_DOVI;
					pr_info("dovi enable in uboot and mode is DV ST\n");
				}
			}
			dolby_vision_target_mode = dolby_vision_mode;
		}
	} else {
		/* core1a */
		dv_mem_power_off(VPU_DOLBY1A);
		dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
		VSYNC_WR_DV_REG(DOLBY_CORE1_CLKGATE_CTRL, 0x55555455);
		/* core2 */
		dv_mem_power_off(VPU_DOLBY2);
		VSYNC_WR_DV_REG(DOLBY_CORE2A_CLKGATE_CTRL, 0x55555555);
		/* core3 */
		dv_mem_power_off(VPU_DOLBY_CORE3);
		VSYNC_WR_DV_REG(DOLBY_CORE3_CLKGATE_CTRL, 0x55555555);
		pr_info("g12 dovi disable in uboot\n");
	}

	return 0;
}

static ssize_t amdolby_vision_dv_mode_show(struct class *cla,
										   struct class_attribute *attr, char *buf)
{
	pr_info("usage: echo mode > /sys/class/amdolby_vision/dv_mode\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_BYPASS		0\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_IPT			1\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL	2\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_HDR10		3\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_SDR10		4\n");
	pr_info("\tDOLBY_VISION_OUTPUT_MODE_SDR8		5\n");
	if (is_dolby_vision_enable())
		pr_info("current dv_mode = %s\n",
				dv_mode_str[get_dolby_vision_mode()]);
	else
		pr_info("current dv_mode = off\n");
	return 0;
}

static ssize_t amdolby_vision_dv_mode_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r != 0)
		return -EINVAL;

	if (val >= 0 && val < 6)
		set_dolby_vision_mode(dv_mode_table[val]);
	else if (val & 0x200)
		dolby_vision_dump_struct();
	else if (val & 0x70)
		dolby_vision_dump_setting(val);
	return count;
}

static void parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

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
}

static ssize_t amdolby_vision_reg_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int reg_addr, reg_val;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "rv")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_addr = val;
		reg_val = READ_VPP_DV_REG(reg_addr);
		pr_info("reg[0x%04x]=0x%08x\n", reg_addr, reg_val);
	} else if (!strcmp(parm[0], "wv")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_val = val;
		WRITE_VPP_DV_REG(reg_addr, reg_val);
	}
	kfree(buf_orig);
	buf_orig =	NULL;
	return count;

}

static ssize_t amdolby_vision_core1_switch_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 40, "%d\n",
					core1_switch);
}

static ssize_t amdolby_vision_core1_switch_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &core1_switch);
	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t amdolby_vision_core3_switch_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 40, "%d\n",
					core3_switch);
}

static ssize_t amdolby_vision_dv_support_info_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	pr_dolby_dbg("show dv capability %d\n", support_info);
	return snprintf(buf, 40, "%d\n",
					support_info);
}

static ssize_t amdolby_vision_core3_switch_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &core3_switch);
	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t  amdolby_vision_crc_show(struct class *cla,
										struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", cur_crc);
}

static ssize_t dv_video_on_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len, "%d\n",
				   is_dolby_vision_video_on());
	return len;
}

static struct class_attribute amdolby_vision_class_attrs[] = {
		__ATTR(ko_info, 0444, amdolby_vision_ko_info_show, NULL),
		__ATTR(debug, 0644, amdolby_vision_debug_show, amdolby_vision_debug_store),
		__ATTR(dv_mode, 0644, amdolby_vision_dv_mode_show, amdolby_vision_dv_mode_store),
		__ATTR(dv_reg, 0220, NULL, amdolby_vision_reg_store),
		__ATTR(core1_switch, 0644, amdolby_vision_core1_switch_show, amdolby_vision_core1_switch_store),
		__ATTR(core3_switch, 0644, amdolby_vision_core3_switch_show, amdolby_vision_core3_switch_store),
		__ATTR(dv_bin_config, 0644, amdolby_vision_bin_config_show, NULL),
		__ATTR(dv_pq_info, 0644, amdolby_vision_pq_info_show, amdolby_vision_pq_info_store),
		__ATTR(dv_set_inter_pq, 0644, amdolby_vision_set_inter_pq_show, amdolby_vision_set_inter_pq_store),
		__ATTR(dv_load_cfg_status, 0644, amdolby_vision_load_cfg_status_show, amdolby_vision_load_cfg_status_store),
		__ATTR(support_info, 0444, amdolby_vision_dv_support_info_show, NULL),
		__ATTR(tprimary, 0644, amdolby_vision_primary_show, amdolby_vision_primary_store),
		__ATTR(config_file, 0644, amdolby_vision_config_file_show, amdolby_vision_config_file_store),
		__ATTR(content_fps, 0644, amdolby_vision_content_fps_show, amdolby_vision_content_fps_store),
		__ATTR(gd_rf_adjust, 0644, amdolby_vision_gd_rf_adjust_show, amdolby_vision_gd_rf_adjust_store),
		__ATTR(cur_crc, 0644, amdolby_vision_crc_show, NULL),
		__ATTR(force_disable_dv_backlight, 0644, amdolby_vision_force_disable_bl_show, amdolby_vision_force_disable_bl_store),
		__ATTR(use_target_lum_from_cfg, 0644, amdolby_vision_use_cfg_target_lum_show, amdolby_vision_use_cfg_target_lum_store),
		__ATTR(dv_video_on, 0444, dv_video_on_show, NULL),
		__ATTR_NULL
};

static struct dv_device_data_s dolby_vision_g12 = {
		.cpu_id = _CPU_MAJOR_ID_G12,
};

static const struct of_device_id amlogic_dolby_vision_match[] = {
		{
				.compatible = "amlogic, dolby_vision_g12a",
				.data = &dolby_vision_g12,
		},
		{
				.compatible = "amlogic, dolby_vision_g12b",
				.data = &dolby_vision_g12,
		},
		{
				.compatible = "amlogic, dolby_vision_sm1",
				.data = &dolby_vision_g12,
		},
		{},
};

static int amdolby_vision_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct amdolby_vision_dev_s *devp = &amdolby_vision_dev;

	pr_info("\n amdolby_vision probe start & ver: %s\n", DRIVER_VER);
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct dv_device_data_s *dv_meson;
		struct device_node *of_node = pdev->dev.of_node;

		match = of_match_node(amlogic_dolby_vision_match, of_node);
		if (match) {
			dv_meson = (struct dv_device_data_s *)match->data;
			if (dv_meson) {
				memcpy(&dv_meson_dev, dv_meson,
					   sizeof(struct dv_device_data_s));
			} else {
				pr_err("%s data NOT match\n", __func__);
				dolby_vision_probe_ok = 0;
				return -ENODEV;
			}
		} else {
			pr_err("%s NOT match\n", __func__);
			dolby_vision_probe_ok = 0;
			return -ENODEV;
		}

	}
	pr_info("\n cpu_id=%d\n", dv_meson_dev.cpu_id);
	memset(devp, 0, (sizeof(struct amdolby_vision_dev_s)));

	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMDOLBY_VISION_NAME);
	if (ret < 0)
		goto fail_alloc_region;
	devp->clsp = class_create(THIS_MODULE,
							  AMDOLBY_VISION_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}
	for (i = 0;  amdolby_vision_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp,
							  &amdolby_vision_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
	cdev_init(&devp->cdev, &amdolby_vision_fops);
	devp->cdev.owner = THIS_MODULE;
	
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno, NULL, AMDOLBY_VISION_NAME);

	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;

	}
	dolby_vision_addr();
	dolby_vision_init_receiver(pdev);
	init_waitqueue_head(&devp->dv_queue);
	pr_info("%s: ok\n", __func__);
	dolby_vision_check_enable();
	dolby_vision_probe_ok = 1;
	return 0;

	fail_create_device:
	pr_info("[amdolby_vision.] : amdolby_vision device create error.\n");
	cdev_del(&devp->cdev);
	fail_add_cdev:
	pr_info("[amdolby_vision.] : amdolby_vision add device error.\n");
	fail_class_create_file:
	pr_info("[amdolby_vision.] : amdolby_vision class create file error.\n");
	for (i = 0; amdolby_vision_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->clsp,
						  &amdolby_vision_class_attrs[i]);
	}
	class_destroy(devp->clsp);
	fail_create_class:
	pr_info("[amdolby_vision.] : amdolby_vision class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
	fail_alloc_region:
	pr_info("[amdolby_vision.] : amdolby_vision alloc error.\n");
	pr_info("[amdolby_vision.] : amdolby_vision_init.\n");
	dolby_vision_probe_ok = 0;

	return ret;
}

static int __exit amdolby_vision_remove(struct platform_device *pdev)
{
	struct amdolby_vision_dev_s *devp = &amdolby_vision_dev;
	int i;

	if (pq_config_buf) {
		vfree(pq_config_buf);
		pq_config_buf = NULL;
	}
	for (i = 0; i < 2; i++) {
		if (md_buf[i]) {
			vfree(md_buf[i]);
			md_buf[i] = NULL;
		}
		if (comp_buf[i]) {
			vfree(comp_buf[i]);
			comp_buf[i] = NULL;
		}
		if (drop_md_buf[i]) {
			vfree(drop_md_buf[i]);
			drop_md_buf[i] = NULL;
		}
		if (drop_comp_buf[i]) {
			vfree(drop_comp_buf[i]);
			drop_comp_buf[i] = NULL;
		}
	}
	if (vsem_if_buf) {
		vfree(vsem_if_buf);
		vsem_if_buf = NULL;
	}
	if (vsem_md_buf) {
		vfree(vsem_md_buf);
		vsem_md_buf = NULL;
	}
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
	pr_info("[ amdolby_vision.] :  amdolby_vision_exit.\n");
	return 0;
}

static struct platform_driver aml_amdolby_vision_driver = {
		.driver = {
				.owner = THIS_MODULE,
				.name = "aml_amdolby_vision_driver",
				.of_match_table = amlogic_dolby_vision_match,
		},
		.probe = amdolby_vision_probe,
		.remove = __exit_p(amdolby_vision_remove),
};

static int __init get_dolby_uboot_status(char *str)
{
	char uboot_dolby_status[DV_NAME_LEN_MAX] = {0};

	snprintf(uboot_dolby_status, DV_NAME_LEN_MAX, "%s", str);
	pr_info("get_dolby_on: %s\n", uboot_dolby_status);

	if (!strcmp(uboot_dolby_status, "1")) {
		dolby_vision_on_in_uboot = 1;
		dolby_vision_enable = 1;
	}
	return 0;
}

__setup("dolby_vision_on=", get_dolby_uboot_status);

static int __init amdolby_vision_init(void)
{
	pr_info("%s:module init\n", __func__);

	if (platform_driver_register(&aml_amdolby_vision_driver)) {
		pr_err("failed to register amdolby_vision module\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit amdolby_vision_exit(void)
{
	pr_info("%s:module exit\n", __func__);
	platform_driver_unregister(&aml_amdolby_vision_driver);
}

module_init(amdolby_vision_init);
module_exit(amdolby_vision_exit);

MODULE_DESCRIPTION("AMLOGIC amdolby_vision driver");
MODULE_LICENSE("GPL");
