/*
 * include/linux/amlogic/media/sound/spdif_info.h
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

#ifndef __SPDIF_INFO_H__
#define __SPDIF_INFO_H__

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/control.h>

/* IEC958_mode_codec */
#define STEREO_PCM		0
#define DTS_RAW_MODE		1
#define DOLBY_DIGITAL		2
#define DTS			3
#define DD_PLUS		4
#define DTS_HD			5
#define MULTI_CHANNEL_LPCM	6
#define TRUEHD			7
#define _DTS_HD_MA		8
#define HIGH_SR_STEREO_LPCM	9

struct iec958_chsts {
	unsigned short chstat0_l;
	unsigned short chstat1_l;
	unsigned short chstat0_r;
	unsigned short chstat1_r;
};

unsigned int spdif_get_codec(void);
bool spdifout_is_raw(void);
bool spdif_4x_rate_fmt(unsigned int fmt);
bool spdif_is_4x_clk(void);

void spdif_get_channel_status_info(struct iec958_chsts *chsts,
				   unsigned int rate,
				   unsigned int codec);

void spdif_notify_to_hdmitx(struct snd_pcm_substream *substream,
			    unsigned int codec);

extern const struct soc_enum spdif_format_enum;

int spdif_format_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int spdif_format_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
#ifdef CONFIG_AMLOGIC_HDMITX
int aml_get_hdmi_out_audio(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int aml_set_hdmi_out_audio(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
#endif
#endif
