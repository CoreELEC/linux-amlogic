/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


#include "firmware_type.h"
#include "../chips/decoder_cpu_ver_info.h"

static const struct format_name_s format_name[] = {
	{VIDEO_DEC_MPEG12,		"mpeg12"},
	{VIDEO_DEC_MPEG12_MULTI,	"mpeg12_multi"},
	{VIDEO_DEC_MPEG4_3,		"mpeg4_3"},
	{VIDEO_DEC_MPEG4_4,		"mpeg4_4"},
	{VIDEO_DEC_MPEG4_4_MULTI,	"mpeg4_4_multi"},
	{VIDEO_DEC_MPEG4_5,		"xvid"},
	{VIDEO_DEC_MPEG4_5_MULTI,	"xvid_multi"},
	{VIDEO_DEC_H263,		"h263"},
	{VIDEO_DEC_H263_MULTI,		"h263_multi"},
	{VIDEO_DEC_MJPEG,		"mjpeg"},
	{VIDEO_DEC_MJPEG_MULTI,		"mjpeg_multi"},
	{VIDEO_DEC_REAL_V8,		"real_v8"},
	{VIDEO_DEC_REAL_V9,		"real_v9"},
	{VIDEO_DEC_VC1,			"vc1"},
	{VIDEO_DEC_VC1_G12A,		"vc1_g12a"},
	{VIDEO_DEC_AVS,			"avs"},
	{VIDEO_DEC_AVS_GXM,		"avs_gxm"},
	{VIDEO_DEC_AVS_NOCABAC,		"avs_no_cabac"},
	{VIDEO_DEC_H264,		"h264"},
	{VIDEO_DEC_H264_4k2K,		"h264_4k2k"},
	{VIDEO_DEC_H264_4k2K_SINGLE,	"h264_4k2k_single"},
	{VIDEO_DEC_H264_MVC,		"h264_mvc"},
	{VIDEO_DEC_H264_MVC_GXM,	"h264_mvc_gxm"},
	{VIDEO_DEC_H264_MULTI,		"h264_multi"},
	{VIDEO_DEC_H264_MULTI_MMU,	"h264_multi_mmu"},
	{VIDEO_DEC_H264_MULTI_GXM,	"h264_multi_gxm"},
	{VIDEO_DEC_HEVC,		"hevc"},
	{VIDEO_DEC_HEVC_MMU,		"hevc_mmu"},
	{VIDEO_DEC_HEVC_G12A,		"hevc_g12a"},
	{VIDEO_DEC_VP9,			"vp9"},
	{VIDEO_DEC_VP9_MMU,		"vp9_mmu"},
	{VIDEO_DEC_VP9_G12A,		"vp9_g12a"},
	{VIDEO_DEC_AVS2,		"avs2"},
	{VIDEO_DEC_AVS2_MMU,		"avs2_mmu"},
	{VIDEO_ENC_H264,		"h264_enc"},
	{VIDEO_ENC_JPEG,		"jpeg_enc"},
	{FIRMWARE_MAX,			"unknown"},
};

static const struct cpu_type_s cpu_type[] = {
	{AM_MESON_CPU_MAJOR_ID_GXL,	"gxl"},
	{AM_MESON_CPU_MAJOR_ID_GXM,	"gxm"},
	{AM_MESON_CPU_MAJOR_ID_G12A,	"g12a"},
	{AM_MESON_CPU_MAJOR_ID_G12B,	"g12b"},
	{AM_MESON_CPU_MAJOR_ID_GXLX2,	"gxlx2"},
	{AM_MESON_CPU_MAJOR_ID_TL1, 	"tl1"},
};

const char *get_fw_format_name(unsigned int format)
{
	const char *name = "unknown";
	int i, size = ARRAY_SIZE(format_name);

	for (i = 0; i < size; i++) {
		if (format == format_name[i].format)
			name = format_name[i].name;
	}

	return name;
}
EXPORT_SYMBOL(get_fw_format_name);

unsigned int get_fw_format(const char *name)
{
	unsigned int format = FIRMWARE_MAX;
	int i, size = ARRAY_SIZE(format_name);

	for (i = 0; i < size; i++) {
		if (!strcmp(name, format_name[i].name))
			format = format_name[i].format;
	}

	return format;
}
EXPORT_SYMBOL(get_fw_format);

int fw_get_cpu(const char *name)
{
	int type = 0;
	int i, size = ARRAY_SIZE(cpu_type);

	for (i = 0; i < size; i++) {
		if (!strcmp(name, cpu_type[i].name))
			type = cpu_type[i].type;
	}

	return type;
}
EXPORT_SYMBOL(fw_get_cpu);

