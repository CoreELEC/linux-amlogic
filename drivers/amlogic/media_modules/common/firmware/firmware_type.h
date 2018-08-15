#ifndef __VIDEO_FIRMWARE_FORMAT_
#define __VIDEO_FIRMWARE_FORMAT_

#include <linux/slab.h>

/* example: #define VIDEO_DEC_AV1  TAG('A', 'V', '1', '-')*/
#define TAG(a, b, c, d)\
	((a << 24) | (b << 16) | (c << 8) | d)

/* fws define */
#define VIDEO_DEC_MPEG12		(0)
#define VIDEO_DEC_MPEG4_3		(1)
#define VIDEO_DEC_MPEG4_4		(2)
#define VIDEO_DEC_MPEG4_5		(3)
#define VIDEO_DEC_H263			(4)
#define VIDEO_DEC_MJPEG			(5)
#define VIDEO_DEC_MJPEG_MULTI		(6)
#define VIDEO_DEC_REAL_V8		(7)
#define VIDEO_DEC_REAL_V9		(8)
#define VIDEO_DEC_VC1			(9)
#define VIDEO_DEC_AVS			(10)
#define VIDEO_DEC_H264			(11)
#define VIDEO_DEC_H264_4k2K		(12)
#define VIDEO_DEC_H264_4k2K_SINGLE	(13)
#define VIDEO_DEC_H264_MVC		(14)
#define VIDEO_DEC_H264_MULTI		(15)
#define VIDEO_DEC_HEVC			(16)
#define VIDEO_DEC_HEVC_MMU		(17)
#define VIDEO_DEC_VP9			(18)
#define VIDEO_DEC_VP9_MMU		(19)
#define VIDEO_ENC_H264			(20)
#define VIDEO_ENC_JPEG			(21)
#define VIDEO_DEC_H264_MULTI_MMU	(23)
#define VIDEO_DEC_HEVC_G12A		(24)
#define VIDEO_DEC_VP9_G12A		(25)
#define VIDEO_DEC_AVS2			(26)
#define VIDEO_DEC_AVS2_MMU		(27)
#define VIDEO_DEC_AVS_GXM		(28)
#define VIDEO_DEC_AVS_NOCABAC		(29)
#define VIDEO_DEC_H264_MULTI_GXM	(30)
#define VIDEO_DEC_H264_MVC_GXM		(31)
#define VIDEO_DEC_VC1_G12A		(32)
#define VIDEO_DEC_MPEG12_MULTI		TAG('M', '1', '2', 'M')
#define VIDEO_DEC_MPEG4_4_MULTI		TAG('M', '4', '4', 'M')
#define VIDEO_DEC_MPEG4_5_MULTI		TAG('M', '4', '5', 'M')
#define VIDEO_DEC_H263_MULTI		TAG('2', '6', '3', 'M')

/* ... */
#define FIRMWARE_MAX			(UINT_MAX)

#define VIDEO_PACKAGE			(0)
#define VIDEO_FW_FILE			(1)

#define VIDEO_DECODE			(0)
#define VIDEO_ENCODE			(1)
#define VIDEO_MISC			(2)

#define OPTEE_VDEC_LEGENCY		(0)
#define OPTEE_VDEC			(1)
#define OPTEE_VDEC_HEVC			(2)

struct format_name_s {
	unsigned int format;
	const char *name;
};

struct cpu_type_s {
	int type;
	const char *name;
};

const char *get_fw_format_name(unsigned int format);
unsigned int get_fw_format(const char *name);
int fw_get_cpu(const char *name);

#endif
