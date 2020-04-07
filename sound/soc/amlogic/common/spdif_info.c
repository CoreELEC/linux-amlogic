/*
 * sound/soc/amlogic/common/spdif_info.c
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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
#define DEBUG
#undef pr_fmt
#define pr_fmt(fmt) "spdif_info: " fmt

#include <linux/amlogic/media/sound/aout_notify.h>
#include <linux/amlogic/media/sound/spdif_info.h>
#ifdef CONFIG_AMLOGIC_HDMITX
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ext.h>
#endif

//#include "spdif_info.h"
/*
 * 0 --  other formats except(DD,DD+,DTS)
 * 1 --  DTS
 * 2 --  DD
 * 3 -- DTS with 958 PCM RAW package mode
 * 4 -- DD+
 */
unsigned int IEC958_mode_codec;
EXPORT_SYMBOL(IEC958_mode_codec);

unsigned int spdif_get_codec(void)
{
	return IEC958_mode_codec;
}

bool spdifout_is_raw(void)
{
	return (IEC958_mode_codec != STEREO_PCM &&
		IEC958_mode_codec != MULTI_CHANNEL_LPCM &&
		IEC958_mode_codec != HIGH_SR_STEREO_LPCM);
}

bool spdif_4x_rate_fmt(unsigned int fmt)
{
	if (fmt == DD_PLUS || fmt == DTS_HD ||
	    fmt == TRUEHD || fmt == _DTS_HD_MA) {
		return true;
	}

	return false;
}

bool spdif_is_4x_clk(void)
{
	return spdif_4x_rate_fmt(IEC958_mode_codec);
}

void spdif_get_channel_status_info(
	struct iec958_chsts *chsts,
	unsigned int rate, unsigned int codec)
{
	int rate_bit = snd_pcm_rate_to_rate_bit(rate);

	if (rate_bit == SNDRV_PCM_RATE_KNOT) {
		pr_err("Unsupport sample rate\n");
		return;
	}

	if (codec && codec != HIGH_SR_STEREO_LPCM) {
		if (codec == DTS_RAW_MODE) {
			/* dts, use raw sync-word mode */
			pr_info("iec958 mode RAW\n");
		} else {
			/* ac3,use the same pcm mode as i2s */
		}

		chsts->chstat0_l = 0x1902;
		chsts->chstat0_r = 0x1902;
		if (codec == DD_PLUS || codec == DTS_HD) {
			/* DD+ */
			if (rate_bit == SNDRV_PCM_RATE_32000) {
				chsts->chstat1_l = 0x300;
				chsts->chstat1_r = 0x300;
			} else if (rate_bit == SNDRV_PCM_RATE_44100) {
				chsts->chstat1_l = 0xc00;
				chsts->chstat1_r = 0xc00;
			} else {
				chsts->chstat1_l = 0Xe00;
				chsts->chstat1_r = 0Xe00;
			}
		} else {
			/* DTS,DD */
			if (rate_bit == SNDRV_PCM_RATE_32000) {
				chsts->chstat1_l = 0x300;
				chsts->chstat1_r = 0x300;
			} else if (rate_bit == SNDRV_PCM_RATE_44100) {
				chsts->chstat1_l = 0;
				chsts->chstat1_r = 0;
			} else {
				chsts->chstat1_l = 0x200;
				chsts->chstat1_r = 0x200;
			}
		}
	} else {
		chsts->chstat0_l = 0x0100;
		chsts->chstat0_r = 0x0100;
		chsts->chstat1_l = 0x200;
		chsts->chstat1_r = 0x200;

		if (rate_bit == SNDRV_PCM_RATE_44100) {
			chsts->chstat1_l = 0;
			chsts->chstat1_r = 0;
		} else if (rate_bit == SNDRV_PCM_RATE_88200) {
			chsts->chstat1_l = 0x800;
			chsts->chstat1_r = 0x800;
		} else if (rate_bit == SNDRV_PCM_RATE_96000) {
			chsts->chstat1_l = 0xa00;
			chsts->chstat1_r = 0xa00;
		} else if (rate_bit == SNDRV_PCM_RATE_176400) {
			chsts->chstat1_l = 0xc00;
			chsts->chstat1_r = 0xc00;
		} else if (rate_bit == SNDRV_PCM_RATE_192000) {
			chsts->chstat1_l = 0xe00;
			chsts->chstat1_r = 0xe00;
		}
	}
	pr_debug("rate: %d, channel status ch0_l:0x%x, ch0_r:0x%x, ch1_l:0x%x, ch1_r:0x%x\n",
		rate,
		chsts->chstat0_l,
		chsts->chstat0_r,
		chsts->chstat1_l,
		chsts->chstat1_r);
}


void spdif_notify_to_hdmitx(struct snd_pcm_substream *substream,
			    unsigned int codec)
{
	if (codec == MULTI_CHANNEL_LPCM) {
		pr_warn("%s(), multi-ch pcm priority, exit\n", __func__);
		return;
	}

	if (codec == DOLBY_DIGITAL) {
		aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_AC_3,
			substream);
	} else if (codec == DTS) {
		aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_DTS,
			substream);
	} else if (codec == DD_PLUS) {
		aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS,
			substream);
	} else if (codec == DTS_HD) {
		aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_DTS_HD,
			substream);
	} else if (codec == TRUEHD || codec == _DTS_HD_MA) {
		//aml_aiu_write(AIU_958_CHSTAT_L0, 0x1902);
		//aml_aiu_write(AIU_958_CHSTAT_L1, 0x900);
		//aml_aiu_write(AIU_958_CHSTAT_R0, 0x1902);
		//aml_aiu_write(AIU_958_CHSTAT_R1, 0x900);
		if (codec == _DTS_HD_MA)
			aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_DTS_HD_MA,
			substream);
		else
			aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_MAT_MLP,
			substream);
	} else {
			aout_notifier_call_chain(
				AOUT_EVENT_IEC_60958_PCM,
				substream);
	}
}

static const char *const spdif_format_texts[10] = {
	"2 CH PCM", "DTS RAW Mode", "Dolby Digital", "DTS",
	"DD+", "DTS-HD", "Multi-channel LPCM", "TrueHD", "DTS-HD MA",
	"HIGH SR Stereo LPCM"
};

const struct soc_enum spdif_format_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(spdif_format_texts),
			spdif_format_texts);

int spdif_format_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = IEC958_mode_codec;
	return 0;
}

int spdif_format_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.enumerated.item[0];

	if (index > HIGH_SR_STEREO_LPCM) {
		pr_err("bad parameter for spdif format set\n");
		return -1;
	}
	IEC958_mode_codec = index;
	return 0;
}

#ifdef CONFIG_AMLOGIC_HDMITX
unsigned int aml_audio_hdmiout_mute_flag;
/* call HDMITX API to enable/disable internal audio out */
int aml_get_hdmi_out_audio(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = !hdmitx_ext_get_audio_status();

	aml_audio_hdmiout_mute_flag =
			ucontrol->value.integer.value[0];
	return 0;
}

int aml_set_hdmi_out_audio(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	bool mute = ucontrol->value.integer.value[0];

	if (aml_audio_hdmiout_mute_flag != mute) {
		hdmitx_ext_set_audio_output(!mute);
		aml_audio_hdmiout_mute_flag = mute;
	}
	return 0;
}
#endif
