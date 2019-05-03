/*
 * include/linux/amlogic/media/video_sink/video.h
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

#ifndef VIDEO_H
#define VIDEO_H

#define MAX_VD_LAYERS 2

#define LAYER1_BUSY (1 << 11)
#define LAYER1_AFBC (1 << 10)
#define LAYER1_SCALER (1 << 9)
#define LAYER1_AVAIL (1 << 8)
#define LAYER0_BUSY (1 << 3)
#define LAYER0_AFBC (1 << 2)
#define LAYER0_SCALER (1 << 1)
#define LAYER0_AVAIL (1 << 0)

#define LAYER_BITS_SHFIT 8

enum {
	VIDEO_WIDEOPTION_NORMAL = 0,
	VIDEO_WIDEOPTION_FULL_STRETCH = 1,
	VIDEO_WIDEOPTION_4_3 = 2,
	VIDEO_WIDEOPTION_16_9 = 3,
	VIDEO_WIDEOPTION_NONLINEAR = 4,
	VIDEO_WIDEOPTION_NORMAL_NOSCALEUP = 5,
	VIDEO_WIDEOPTION_4_3_IGNORE = 6,
	VIDEO_WIDEOPTION_4_3_LETTER_BOX = 7,
	VIDEO_WIDEOPTION_4_3_PAN_SCAN = 8,
	VIDEO_WIDEOPTION_4_3_COMBINED = 9,
	VIDEO_WIDEOPTION_16_9_IGNORE = 10,
	VIDEO_WIDEOPTION_16_9_LETTER_BOX = 11,
	VIDEO_WIDEOPTION_16_9_PAN_SCAN = 12,
	VIDEO_WIDEOPTION_16_9_COMBINED = 13,
	VIDEO_WIDEOPTION_CUSTOM = 14,
	VIDEO_WIDEOPTION_AFD = 15,
	VIDEO_WIDEOPTION_MAX = 16
};

/* TODO: move to register headers */
#define VPP_VADJ2_BLMINUS_EN        (1 << 3)
#define VPP_VADJ2_EN                (1 << 2)
#define VPP_VADJ1_BLMINUS_EN        (1 << 1)
#define VPP_VADJ1_EN                (1 << 0)

#define VPP_VD_SIZE_MASK            0xfff
#define VPP_VD1_START_BIT           16
#define VPP_VD1_END_BIT             0

#define VPP_REGION_MASK             0xfff
#define VPP_REGION1_BIT             16
#define VPP_REGION2_BIT             0
#define VPP_REGION3_BIT             16
#define VPP_REGION4_BIT             0

#define VDIF_PIC_END_BIT             16
#define VDIF_PIC_START_BIT           0

#define VD1_FMT_LUMA_WIDTH_MASK         0xfff
#define VD1_FMT_LUMA_WIDTH_BIT          16
#define VD1_FMT_CHROMA_WIDTH_MASK       0xfff
#define VD1_FMT_CHROMA_WIDTH_BIT        0

#define VIU_MISC_AFBC_VD1           (1 << 20)
#define VPP_CM_ENABLE               (1 << 28)

#define VPP_VD2_ALPHA_WID           9
#define VPP_VD2_ALPHA_MASK          0x1ff
#define VPP_VD2_ALPHA_BIT           18
#define VPP_OSD2_PREBLEND           (1 << 17)
#define VPP_OSD1_PREBLEND           (1 << 16)
#define VPP_VD2_PREBLEND            (1 << 15)
#define VPP_VD1_PREBLEND            (1 << 14)
#define VPP_OSD2_POSTBLEND          (1 << 13)
#define VPP_OSD1_POSTBLEND          (1 << 12)
#define VPP_VD2_POSTBLEND           (1 << 11)
#define VPP_VD1_POSTBLEND           (1 << 10)
#define VPP_OSD1_ALPHA              (1 << 9)
#define VPP_OSD2_ALPHA              (1 << 8)
#define VPP_POSTBLEND_EN            (1 << 7)
#define VPP_PREBLEND_EN             (1 << 6)
#define VPP_PRE_FG_SEL_MASK         (1 << 5)
#define VPP_PRE_FG_OSD2             (1 << 5)
#define VPP_PRE_FG_OSD1             (0 << 5)
#define VPP_POST_FG_SEL_MASK        (1 << 4)
#define VPP_POST_FG_OSD2            (1 << 4)
#define VPP_POST_FG_OSD1            (0 << 4)
#define DNLP_SR1_CM			(1 << 3)
#define SR1_AFTER_POSTBLEN		(0 << 3)
#define VPP_FIFO_RESET_DE           (1 << 2)
#define PREBLD_SR0_VD1_SCALER		(1 << 1)
#define SR0_AFTER_DNLP                (0 << 1)
#define VPP_OUT_SATURATE            (1 << 0)

#define VDIF_RESET_ON_GO_FIELD       (1<<29)
#define VDIF_URGENT_BIT              27
#define VDIF_CHROMA_END_AT_LAST_LINE (1<<26)
#define VDIF_LUMA_END_AT_LAST_LINE   (1<<25)
#define VDIF_HOLD_LINES_BIT          19
#define VDIF_HOLD_LINES_MASK         0x3f
#define VDIF_LAST_LINE               (1<<18)
#define VDIF_BUSY                    (1<<17)
#define VDIF_DEMUX_MODE              (1<<16)
#define VDIF_DEMUX_MODE_422          (0<<16)
#define VDIF_DEMUX_MODE_RGB_444      (1<<16)
#define VDIF_FORMAT_BIT              14
#define VDIF_FORMAT_MASK             3
#define VDIF_FORMAT_SPLIT            (0<<14)
#define VDIF_FORMAT_422              (1<<14)
#define VDIF_FORMAT_RGB888_YUV444    (2<<14)
#define VDIF_BURSTSIZE_MASK          3
#define VDIF_BURSTSIZE_CR_BIT        12
#define VDIF_BURSTSIZE_CB_BIT        10
#define VDIF_BURSTSIZE_Y_BIT         8
#define VDIF_MANULE_START_FRAME      (1<<7)
#define VDIF_CHRO_RPT_LAST           (1<<6)
#define VDIF_CHROMA_HZ_AVG           (1<<3)
#define VDIF_LUMA_HZ_AVG             (1<<2)
#define VDIF_SEPARATE_EN             (1<<1)
#define VDIF_ENABLE                  (1<<0)

#define VDIF_LOOP_MASK              0xff
#define VDIF_CHROMA_LOOP1_BIT       24
#define VDIF_LUMA_LOOP1_BIT         16
#define VDIF_CHROMA_LOOP0_BIT       8
#define VDIF_LUMA_LOOP0_BIT         0

#define HFORMATTER_REPEAT                (1<<28)
#define HFORMATTER_BILINEAR              (0<<28)
#define HFORMATTER_PHASE_MASK    0xf
#define HFORMATTER_PHASE_BIT     24
#define HFORMATTER_RRT_PIXEL0    (1<<23)
#define HFORMATTER_YC_RATIO_1_1  (0<<21)
#define HFORMATTER_YC_RATIO_2_1  (1<<21)
#define HFORMATTER_YC_RATIO_4_1  (2<<21)
#define HFORMATTER_EN                    (1<<20)
#define VFORMATTER_ALWAYS_RPT    (1<<19)
#define VFORMATTER_LUMA_RPTLINE0_DE     (1<<18)
#define VFORMATTER_CHROMA_RPTLINE0_DE   (1<<17)
#define VFORMATTER_RPTLINE0_EN   (1<<16)
#define VFORMATTER_SKIPLINE_NUM_MASK    0xf
#define VFORMATTER_SKIPLINE_NUM_BIT             12
#define VFORMATTER_INIPHASE_MASK        0xf
#define VFORMATTER_INIPHASE_BIT         8
#define VFORMATTER_PHASE_MASK   (0x7f)
#define VFORMATTER_PHASE_BIT    1
#define VFORMATTER_EN                   (1<<0)

#define VPP_PHASECTL_DOUBLE_LINE    (1<<17)
#define VPP_PHASECTL_TYPE           (1<<16)
#define VPP_PHASECTL_TYPE_PROG      (0<<16)
#define VPP_PHASECTL_TYPE_INTERLACE (1<<16)
#define VPP_PHASECTL_VSL0B          (1<<15)
#define VPP_PHASECTL_DOUBLELINE_BIT 17
#define VPP_PHASECTL_DOUBLELINE_WID 2
#define VPP_PHASECTL_INIRPTNUM_MASK 0x3
#define VPP_PHASECTL_INIRPTNUM_WID  2
#define VPP_PHASECTL_INIRPTNUMB_BIT 13
#define VPP_PHASECTL_INIRCVNUM_MASK 0xf
#define VPP_PHASECTL_INIRCVNUM_WID  5
#define VPP_PHASECTL_INIRCVNUMB_BIT 8
#define VPP_PHASECTL_VSL0T          (1<<7)
#define VPP_PHASECTL_INIRPTNUMT_BIT 5
#define VPP_PHASECTL_INIRCVNUMT_BIT 0

#define VPP_LINE_BUFFER_EN_BIT          21
#define VPP_SC_PREHORZ_EN_BIT           20
#define VPP_SC_PREVERT_EN_BIT           19
#define VPP_LINE_BUFFER_EN          (1 << 21)
#define VPP_SC_PREHORZ_EN           (1 << 20)
#define VPP_SC_PREVERT_EN           (1 << 19)
#define VPP_SC_VERT_EN              (1 << 18)
#define VPP_SC_HORZ_EN              (1 << 17)
#define VPP_SC_TOP_EN               (1 << 16)
#define VPP_SC_V1OUT_EN             (1 << 15)
#define VPP_SC_RGN14_HNOLINEAR      (1 << 12)
#define VPP_SC_TOP_EN_WID	    1
#define VPP_SC_TOP_EN_BIT	    16
#define VPP_SC_BANK_LENGTH_WID      3
#define VPP_SC_BANK_LENGTH_MASK     0x7
#define VPP_SC_HBANK_LENGTH_BIT     8
#define VPP_SC_RGN14_VNOLINEAR      (1 << 4)
#define VPP_SC_VBANK_LENGTH_BIT     0

#define VPP_HSC_INIRPT_NUM_MASK     0x3
#define VPP_HSC_INIRPT_NUM_BIT      21
#define VPP_HSC_INIRCV_NUM_MASK     0xf
#define VPP_HSC_INIRCV_NUM_BIT      16
#define VPP_HSC_TOP_INI_PHASE_WID   16
#define VPP_HSC_TOP_INI_PHASE_BIT   0

#define VPP_OFIFO_LINELEN_MASK      0xfff
#define VPP_OFIFO_LINELEN_BIT       20
#define VPP_INV_VS                  (1 << 19)
#define VPP_INV_HS                  (1 << 18)
#define VPP_FORCE_FIELD_EN          (1 << 17)
#define VPP_FORCE_FIELD_TYPE_MASK   (1 << 16)
#define VPP_FORCE_FIELD_TOP         (0 << 16)
#define VPP_FORCE_FIELD_BOTTOM      (1 << 16)
#define VPP_FOURCE_GO_FIELD         (1 << 15)
#define VPP_FOURCE_GO_LINE          (1 << 14)
#define VPP_OFIFO_SIZE_WID          13
#define VPP_OFIFO_SIZE_MASK         0xfff
#define VPP_OFIFO_SIZE_BIT          0

#define VPP_COEF_IDXINC         (1 << 15)
#define VPP_COEF_RD_CBUS        (1 << 14)
#define VPP_COEF_SEP_EN	        (1 << 13)
#define VPP_COEF_9BIT           (1 << 9)
#define VPP_COEF_TYPE           (1 << 8)
#define VPP_COEF_VERT           (0 << 8)
#define VPP_COEF_VERT_CHROMA    (1 << 7)
#define VPP_COEF_HORZ           (1 << 8)
#define VPP_COEF_INDEX_MASK     0x7f
#define VPP_COEF_INDEX_BIT      0

#define AMVIDEO_UPDATE_OSD_MODE	0x00000001
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
int amvideo_notifier_call_chain(unsigned long val, void *v);
#else
static inline int amvideo_notifier_call_chain(unsigned long val, void *v)
{
	return 0;
}
#endif

int query_video_status(int type, int *value);
u32 set_blackout_policy(int policy);
u32 get_blackout_policy(void);
u32 set_blackout_pip_policy(int policy);
u32 get_blackout_pip_policy(void);
void set_video_angle(u32 s_value);
u32 get_video_angle(void);
extern unsigned int DI_POST_REG_RD(unsigned int addr);
extern int DI_POST_WR_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
extern void videosync_pcrscr_update(s32 inc, u32 base);
#endif /* VIDEO_H */
