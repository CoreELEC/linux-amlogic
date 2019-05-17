/*
 * include/linux/amlogic/media/amvecm/amvecm.h
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

#ifndef AMVECM_H
#define AMVECM_H

#include "linux/amlogic/media/amvecm/ve.h"
#include "linux/amlogic/media/amvecm/cm.h"
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/cpu_version.h>
#include <drm/drmP.h>


/* struct ve_dnlp_s          video_ve_dnlp; */

#define FLAG_RSV31              (1 << 31)
#define FLAG_VADJ1_COLOR        (1 << 30)
#define FLAG_VE_DNLP            (1 << 29)
#define FLAG_VE_NEW_DNLP        (1 << 28)
#define FLAG_VE_LC_CURV         (1 << 27)
#define FLAG_RSV26              (1 << 26)
#define FLAG_3D_BLACK_DIS       (1 << 25)
#define FLAG_3D_BLACK_EN        (1 << 24)
#define FLAG_3D_SYNC_DIS        (1 << 23)
#define FLAG_3D_SYNC_EN         (1 << 22)
#define FLAG_VLOCK_DIS          (1 << 21)
#define FLAG_VLOCK_EN          (1 << 20)
#define FLAG_VE_DNLP_EN         (1 << 19)
#define FLAG_VE_DNLP_DIS        (1 << 18)
#define FLAG_VADJ1_CON			(1 << 17)
#define FLAG_VADJ1_BRI			(1 << 16)
#define FLAG_GAMMA_TABLE_EN     (1 << 15)
#define FLAG_GAMMA_TABLE_DIS    (1 << 14)
#define FLAG_GAMMA_TABLE_R      (1 << 13)
#define FLAG_GAMMA_TABLE_G      (1 << 12)
#define FLAG_GAMMA_TABLE_B      (1 << 11)
#define FLAG_RGB_OGO            (1 << 10)
#define FLAG_RSV9               (1 <<  9)
#define FLAG_MATRIX_UPDATE      (1 <<  8)
#define FLAG_BRI_CON            (1 <<  7)
#define FLAG_LVDS_FREQ_SW       (1 <<  6)
#define FLAG_REG_MAP5           (1 <<  5)
#define FLAG_REG_MAP4           (1 <<  4)
#define FLAG_REG_MAP3           (1 <<  3)
#define FLAG_REG_MAP2           (1 <<  2)
#define FLAG_REG_MAP1           (1 <<  1)
#define FLAG_REG_MAP0           (1 <<  0)

#define VPP_VADJ2_BLMINUS_EN        (1 << 3)
#define VPP_VADJ2_EN                (1 << 2)
#define VPP_VADJ1_BLMINUS_EN        (1 << 1)
#define VPP_VADJ1_EN                (1 << 0)

#define VPP_DEMO_DNLP_DIS           (1 << 3)
#define VPP_DEMO_DNLP_EN            (1 << 2)
#define VPP_DEMO_CM_DIS             (1 << 1)
#define VPP_DEMO_CM_EN              (1 << 0)

/*PQ USER LATCH*/
#define PQ_USER_SR1_DERECTION_DIS  (1 << 19)
#define PQ_USER_SR1_DERECTION_EN   (1 << 18)
#define PQ_USER_SR0_DERECTION_DIS  (1 << 17)
#define PQ_USER_SR0_DERECTION_EN   (1 << 16)
#define PQ_USER_SR1_DEJAGGY_DIS    (1 << 15)
#define PQ_USER_SR1_DEJAGGY_EN     (1 << 14)
#define PQ_USER_SR0_DEJAGGY_DIS    (1 << 13)
#define PQ_USER_SR0_DEJAGGY_EN     (1 << 12)
#define PQ_USER_SR1_DERING_DIS     (1 << 11)
#define PQ_USER_SR1_DERING_EN      (1 << 10)
#define PQ_USER_SR0_DERING_DIS     (1 << 9)
#define PQ_USER_SR0_DERING_EN      (1 << 8)
#define PQ_USER_SR1_PK_DIS         (1 << 7)
#define PQ_USER_SR1_PK_EN          (1 << 6)
#define PQ_USER_SR0_PK_DIS         (1 << 5)
#define PQ_USER_SR0_PK_EN          (1 << 4)
#define PQ_USER_BLK_SLOPE          (1 << 3)
#define PQ_USER_BLK_START          (1 << 2)
#define PQ_USER_BLK_DIS            (1 << 1)
#define PQ_USER_BLK_EN             (1 << 0)

/*white balance latch*/
#define MTX_BYPASS_RGB_OGO			(1 << 0)
#define MTX_RGB2YUVL_RGB_OGO		(1 << 1)

#define SDR_SOURCE    (1 << 0)
#define HDR10_SOURCE  (1 << 1)
#define HLG_SOURCE    (1 << 2)

enum cm_hist_e {
	CM_HUE_HIST = 0,
	CM_SAT_HIST,
	CM_MAX_HIST
};

enum pq_table_name_e {
	TABLE_NAME_SHARPNESS0 = 0x1,/*in vpp*/
	TABLE_NAME_SHARPNESS1 = 0x2,/*in vpp*/
	TABLE_NAME_DNLP = 0x4,		/*in vpp*/
	TABLE_NAME_CM = 0x8,		/*in vpp*/
	TABLE_NAME_BLK_BLUE_EXT = 0x10,/*in vpp*/
	TABLE_NAME_BRIGHTNESS = 0x20,/*in vpp*/
	TABLE_NAME_CONTRAST = 0x40,	/*in vpp*/
	TABLE_NAME_SATURATION_HUE = 0x80,/*in vpp*/
	TABLE_NAME_CVD2 = 0x100,		/*in tvafe*/
	TABLE_NAME_DI = 0x200,		/*in di*/
	TABLE_NAME_NR = 0x400,		/*in di*/
	TABLE_NAME_MCDI = 0x800,	/*in di*/
	TABLE_NAME_DEBLOCK = 0x1000,	/*in di*/
	TABLE_NAME_DEMOSQUITO = 0x2000,/*in di*/
	TABLE_NAME_WB = 0X4000,		/*in vpp*/
	TABLE_NAME_GAMMA = 0X8000,	/*in vpp*/
	TABLE_NAME_XVYCC = 0x10000,	/*in vpp*/
	TABLE_NAME_HDR = 0x20000,	/*in vpp*/
	TABLE_NAME_DOLBY_VISION = 0x40000,/*in vpp*/
	TABLE_NAME_OVERSCAN = 0x80000,
	TABLE_NAME_RESERVED1 = 0x100000,
	TABLE_NAME_RESERVED2 = 0x200000,
	TABLE_NAME_RESERVED3 = 0x400000,
	TABLE_NAME_RESERVED4 = 0x800000,
	TABLE_NAME_MAX,
};

#define _VE_CM  'C'

#define AMVECM_IOC_G_HIST_AVG   _IOW(_VE_CM, 0x22, struct ve_hist_s)
#define AMVECM_IOC_VE_DNLP_EN   _IO(_VE_CM, 0x23)
#define AMVECM_IOC_VE_DNLP_DIS  _IO(_VE_CM, 0x24)
#define AMVECM_IOC_VE_NEW_DNLP  _IOW(_VE_CM, 0x25, struct ve_dnlp_curve_param_s)
#define AMVECM_IOC_G_HIST_BIN   _IOW(_VE_CM, 0x26, struct vpp_hist_param_s)
#define AMVECM_IOC_G_HDR_METADATA _IOW(_VE_CM, 0x27, struct hdr_metadata_info_s)
/*vpp get color primary*/
#define AMVECM_IOC_G_COLOR_PRI _IOR(_VE_CM, 0x28, enum color_primary_e)

/* VPP.CM IOCTL command list */
#define AMVECM_IOC_LOAD_REG  _IOW(_VE_CM, 0x30, struct am_regs_s)

/* VPP.GAMMA IOCTL command list */
#define AMVECM_IOC_GAMMA_TABLE_EN  _IO(_VE_CM, 0x40)
#define AMVECM_IOC_GAMMA_TABLE_DIS _IO(_VE_CM, 0x41)
#define AMVECM_IOC_GAMMA_TABLE_R _IOW(_VE_CM, 0x42, struct tcon_gamma_table_s)
#define AMVECM_IOC_GAMMA_TABLE_G _IOW(_VE_CM, 0x43, struct tcon_gamma_table_s)
#define AMVECM_IOC_GAMMA_TABLE_B _IOW(_VE_CM, 0x44, struct tcon_gamma_table_s)
#define AMVECM_IOC_S_RGB_OGO   _IOW(_VE_CM, 0x45, struct tcon_rgb_ogo_s)
#define AMVECM_IOC_G_RGB_OGO  _IOR(_VE_CM, 0x46, struct tcon_rgb_ogo_s)

/*VPP.VLOCK IOCTL command list*/
#define AMVECM_IOC_VLOCK_EN  _IO(_VE_CM, 0x47)
#define AMVECM_IOC_VLOCK_DIS _IO(_VE_CM, 0x48)

/*VPP.3D-SYNC IOCTL command list*/
#define AMVECM_IOC_3D_SYNC_EN  _IO(_VE_CM, 0x49)
#define AMVECM_IOC_3D_SYNC_DIS _IO(_VE_CM, 0x50)

struct ve_pq_load_s {
	enum pq_table_name_e param_id;
	unsigned int length;
	union {
	void *param_ptr;
	long long param_ptr_len;
	};
	union {
	void *reserved;
	long long reserved_len;
	};
};

struct ve_pq_table_s {
	unsigned int src_timing;
	unsigned int value1;
	unsigned int value2;
	unsigned int reserved1;
	unsigned int reserved2;
};

#define AMVECM_IOC_SET_OVERSCAN  _IOW(_VE_CM, 0x52, struct ve_pq_load_s)
enum dnlp_state_e {
	DNLP_OFF = 0,
	DNLP_ON,
};
/*DNLP IOCTL command list*/
#define AMVECM_IOC_G_DNLP_STATE _IOR(_VE_CM, 0x53, enum dnlp_state_e)
#define AMVECM_IOC_S_DNLP_STATE _IOW(_VE_CM, 0x54, enum dnlp_state_e)
enum pc_mode_e {
	PCMODE_OFF = 0,
	PCMODE_ON,
};
/*PCMODE IOCTL command list*/
#define AMVECM_IOC_G_PQMODE _IOR(_VE_CM, 0x55, enum pc_mode_e)
#define AMVECM_IOC_S_PQMODE _IOW(_VE_CM, 0x56, enum pc_mode_e)

/*CUR_CSCTYPE IOCTL command list*/
#define AMVECM_IOC_G_CSCTYPE _IOR(_VE_CM, 0x57, enum vpp_matrix_csc_e)
#define AMVECM_IOC_S_CSCTYPE _IOW(_VE_CM, 0x58, enum vpp_matrix_csc_e)

/*PIC_MODE IOCTL command list*/
#define AMVECM_IOC_G_PIC_MODE _IOR(_VE_CM, 0x59, struct am_vdj_mode_s)
#define AMVECM_IOC_S_PIC_MODE _IOW(_VE_CM, 0x60, struct am_vdj_mode_s)


/*HDR TYPE command list*/
#define AMVECM_IOC_G_HDR_TYPE _IOR(_VE_CM, 0x61, enum hdr_type_e)


/*Local contrast command list*/
#define AMVECM_IOC_S_LC_CURVE _IOW(_VE_CM, 0x62, struct ve_lc_curve_parm_s)


struct am_vdj_mode_s {
	int flag;
	int brightness;
	int brightness2;
	int saturation_hue;
	int saturation_hue_post;
	int contrast;
	int contrast2;
};

enum color_primary_e {
	VPP_COLOR_PRI_NULL = 0,
	VPP_COLOR_PRI_BT601,
	VPP_COLOR_PRI_BT709,
	VPP_COLOR_PRI_BT2020,
	VPP_COLOR_PRI_MAX,
};

enum vpp_matrix_csc_e {
	VPP_MATRIX_NULL = 0,
	VPP_MATRIX_RGB_YUV601 = 0x1,
	VPP_MATRIX_RGB_YUV601F = 0x2,
	VPP_MATRIX_RGB_YUV709 = 0x3,
	VPP_MATRIX_RGB_YUV709F = 0x4,
	VPP_MATRIX_YUV601_RGB = 0x10,
	VPP_MATRIX_YUV601_YUV601F = 0x11,
	VPP_MATRIX_YUV601_YUV709 = 0x12,
	VPP_MATRIX_YUV601_YUV709F = 0x13,
	VPP_MATRIX_YUV601F_RGB = 0x14,
	VPP_MATRIX_YUV601F_YUV601 = 0x15,
	VPP_MATRIX_YUV601F_YUV709 = 0x16,
	VPP_MATRIX_YUV601F_YUV709F = 0x17,
	VPP_MATRIX_YUV709_RGB = 0x20,
	VPP_MATRIX_YUV709_YUV601 = 0x21,
	VPP_MATRIX_YUV709_YUV601F = 0x22,
	VPP_MATRIX_YUV709_YUV709F = 0x23,
	VPP_MATRIX_YUV709F_RGB = 0x24,
	VPP_MATRIX_YUV709F_YUV601 = 0x25,
	VPP_MATRIX_YUV709F_YUV709 = 0x26,
	VPP_MATRIX_YUV601L_YUV709L = 0x27,
	VPP_MATRIX_YUV709L_YUV601L = 0x28,
	VPP_MATRIX_YUV709F_YUV601F = 0x29,
	VPP_MATRIX_BT2020YUV_BT2020RGB = 0x40,
	VPP_MATRIX_BT2020RGB_709RGB,
	VPP_MATRIX_BT2020RGB_CUSRGB,
	VPP_MATRIX_BT2020YUV_BT2020RGB_DYNAMIC = 0x50,
	VPP_MATRIX_DEFAULT_CSCTYPE = 0xffff,
};

enum hdr_type_e {
	HDRTYPE_NONE = 0,
	HDRTYPE_SDR = 0x1,
	HDRTYPE_HDR10 = 0x2,
	HDRTYPE_HLG = 0x4,
	HDRTYPE_MAX,
};

enum vpp_transfer_characteristic_e {
	VPP_ST_NULL = 0,
	VPP_ST709 = 0x1,
	VPP_ST2084 = 0x2,
	VPP_ST2094_40 = 0x4,
};

enum ve_source_input_e {
	SOURCE_INVALID = -1,
	SOURCE_TV = 0,
	SOURCE_AV1,
	SOURCE_AV2,
	SOURCE_YPBPR1,
	SOURCE_YPBPR2,
	SOURCE_HDMI1,
	SOURCE_HDMI2,
	SOURCE_HDMI3,
	SOURCE_HDMI4,
	SOURCE_VGA,
	SOURCE_MPEG,
	SOURCE_DTV,
	SOURCE_SVIDEO,
	SOURCE_IPTV,
	SOURCE_DUMMY,
	SOURCE_SPDIF,
	SOURCE_ADTV,
	SOURCE_MAX,
};

/*pq_timing:
 *SD/HD/FHD/UHD for DTV/MEPG,
 *NTST_M/NTST_443/PAL_I/PAL_M/PAL_60/PAL_CN/SECAM/NTST_50 for AV/ATV
 */
enum ve_pq_timing_e {
	TIMING_SD = 0,
	TIMING_HD,
	TIMING_FHD,
	TIMING_UHD,
	TIMING_NTST_M,
	TIMING_NTST_443,
	TIMING_PAL_I,
	TIMING_PAL_M,
	TIMING_PAL_60,
	TIMING_PAL_CN,
	TIMING_SECAM,
	TIMING_NTSC_50,
	TIMING_MAX,
};

enum vlock_hw_ver_e {
	/*gxtvbb*/
	vlock_hw_org,
	/*
	 *txl
	 *txlx
	 */
	vlock_hw_ver1,
	/* tl1 later
	 * fix bug:i problem
	 * fix bug:affect ss function
	 * add: phase lock
	 */
	vlock_hw_ver2,
};

struct vecm_match_data_s {
	u32 vlk_support;
	u32 vlk_new_fsm;
	enum vlock_hw_ver_e vlk_hwver;
	u32 vlk_phlock_en;
};

/*overscan:
 *length 0~31bit :number of crop;
 *src_timing: bit31: on: load/save all crop
			  bit31: off: load one according to timing*
			  bit30: AFD_enable: 1 -> on; 0 -> off*
			  screen mode: bit24~bit29*
			  source: bit16~bit23 -> source*
			  timing: bit0~bit15 -> sd/hd/fhd/uhd*
 *value1: 0~15bit hs   16~31bit he*
 *value2: 0~15bit vs   16~31bit ve*
 */
struct ve_pq_overscan_s {
	unsigned int load_flag;
	unsigned int afd_enable;
	unsigned int screen_mode;
	enum ve_source_input_e source;
	enum ve_pq_timing_e timing;
	unsigned int hs;
	unsigned int he;
	unsigned int vs;
	unsigned int ve;
};

extern struct ve_pq_overscan_s overscan_table[TIMING_MAX];

#define _DI_	'D'

struct am_pq_parm_s {
	unsigned int table_name;
	unsigned int table_len;
	union {
	void *table_ptr;
	long long l_table;
	};
	union {
	void *reserved;
	long long l_reserved;
	};
};

#define AMDI_IOC_SET_PQ_PARM  _IOW(_DI_, 0x51, struct am_pq_parm_s)

/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8) */
/* #define WRITE_VPP_REG(x,val)*/
/* WRITE_VCBUS_REG(x,val) */
/* #define WRITE_VPP_REG_BITS(x,val,start,length)*/
/* WRITE_VCBUS_REG_BITS(x,val,start,length) */
/* #define READ_VPP_REG(x)*/
/* READ_VCBUS_REG(x) */
/* #define READ_VPP_REG_BITS(x,start,length)*/
/* READ_VCBUS_REG_BITS(x,start,length) */
/* #else */
/* #define WRITE_VPP_REG(x,val)*/
/* WRITE_CBUS_REG(x,val) */
/* #define WRITE_VPP_REG_BITS(x,val,start,length)*/
/* WRITE_CBUS_REG_BITS(x,val,start,length) */
/* #define READ_VPP_REG(x)*/
/* READ_CBUS_REG(x) */
/* #define READ_VPP_REG_BITS(x,start,length)*/
/* READ_CBUS_REG_BITS(x,start,length) */
/* #endif */

static inline void WRITE_VPP_REG(uint32_t reg,
		const uint32_t value)
{
	aml_write_vcbus(reg, value);
}

static inline uint32_t READ_VPP_REG(uint32_t reg)
{
	return aml_read_vcbus(reg);
}

static inline void WRITE_VPP_REG_BITS(uint32_t reg,
		const uint32_t value,
		const uint32_t start,
		const uint32_t len)
{
	WRITE_VPP_REG(reg, ((READ_VPP_REG(reg) &
		~(((1L << (len)) - 1) << (start))) |
		(((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t READ_VPP_REG_BITS(uint32_t reg,
				    const uint32_t start,
				    const uint32_t len)
{
	uint32_t val;

	val = ((READ_VPP_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

extern signed int vd1_brightness, vd1_contrast;
extern bool gamma_en;

extern unsigned int atv_source_flg;

extern enum hdr_type_e hdr_source_type;

#define CSC_FLAG_TOGGLE_FRAME	1
#define CSC_FLAG_CHECK_OUTPUT	2

extern int amvecm_on_vs(
	struct vframe_s *display_vf,
	struct vframe_s *toggle_vf,
	int flags,
	unsigned int sps_h_en,
	unsigned int sps_v_en);
extern void refresh_on_vs(struct vframe_s *vf);
extern void pc_mode_process(void);
extern void pq_user_latch_process(void);
extern void vlock_process(struct vframe_s *vf);

/* master_display_info for display device */
struct hdr_metadata_info_s {
	u32 primaries[3][2];		/* normalized 50000 in G,B,R order */
	u32 white_point[2];		/* normalized 50000 */
	u32 luminance[2];		/* max/min lumin, normalized 10000 */
};

extern void vpp_vd_adj1_saturation_hue(signed int sat_val,
	signed int hue_val, struct vframe_s *vf);
extern void amvecm_sharpness_enable(int sel);
extern int metadata_read_u32(uint32_t *value);
extern int metadata_wait(struct vframe_s *vf);
extern int metadata_sync(uint32_t frame_id, uint64_t pts);
extern void amvecm_wakeup_queue(void);
extern void lc_load_curve(struct ve_lc_curve_parm_s *p);

#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VPP_REG_BITS(adr, val, start, len)
#else
extern int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
extern u32 VSYNC_RD_MPEG_REG(u32 adr);
extern int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
#endif
extern int amvecm_drm_get_gamma_size(u32 index);
extern void amvecm_drm_init(u32 index);
extern int amvecm_drm_gamma_set(u32 index,
			 struct drm_color_lut *lut, int lut_size);
extern int amvecm_drm_gamma_get(u32 index, u16 *red, u16 *green, u16 *blue);
extern int amvecm_drm_gamma_enable(u32 index);
extern int amvecm_drm_gamma_disable(u32 index);
extern int am_meson_ctm_set(u32 index, struct drm_color_ctm *ctm);
extern int am_meson_ctm_disable(void);

#endif /* AMVECM_H */

