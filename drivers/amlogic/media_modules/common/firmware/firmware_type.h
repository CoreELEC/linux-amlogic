#ifndef __VIDEO_FIRMWARE_FORMAT_
#define __VIDEO_FIRMWARE_FORMAT_

#include <linux/slab.h>

enum firmware_type_e {
	VIDEO_DEC_MPEG12,
	VIDEO_DEC_MPEG4_3,
	VIDEO_DEC_MPEG4_4,
	VIDEO_DEC_MPEG4_5,
	VIDEO_DEC_H263,
	VIDEO_DEC_MJPEG,
	VIDEO_DEC_MJPEG_MULTI,
	VIDEO_DEC_REAL_V8,
	VIDEO_DEC_REAL_V9,
	VIDEO_DEC_VC1,
	VIDEO_DEC_AVS,
	VIDEO_DEC_H264,
	VIDEO_DEC_H264_4k2K,
	VIDEO_DEC_H264_4k2K_SINGLE,
	VIDEO_DEC_H264_MVC,
	VIDEO_DEC_H264_MULTI,
	VIDEO_DEC_HEVC,
	VIDEO_DEC_HEVC_MMU,
	VIDEO_DEC_VP9,
	VIDEO_DEC_VP9_MMU,
	VIDEO_ENC_H264,
	VIDEO_ENC_JPEG,
	VIDEO_PACKAGE,
	VIDEO_DEC_H264_MULTI_MMU,
	FIRMWARE_MAX
};

struct type_name_s {
	enum firmware_type_e type;
	const char *name;
};

const char *get_firmware_type_name(enum firmware_type_e type);
enum firmware_type_e get_firmware_type(const char *name);

#endif
