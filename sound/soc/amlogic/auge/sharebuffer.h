/*
 * sound/soc/amlogic/auge/sharebuffer.h
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
#ifndef __AML_AUDIO_SHAREBUFFER_H__
#define __AML_AUDIO_SHAREBUFFER_H__

#include <sound/pcm.h>
#include <linux/amlogic/media/sound/iec_info.h>

enum sharebuffer_srcs {
	SHAREBUFFER_NONE = -1,
	SHAREBUFFER_TDMA = 0,
	SHAREBUFFER_TDMB = 1,
	SHAREBUFFER_TDMC = 2,
	SHAREBUFFER_SPDIFA = 3,
	SHAREBUFFER_SPDIFB = 4,
	SHAREBUFFER_EARCTX = 5,
	SHAREBUFFER_SRC_NUM = 6
};

struct clk;

struct samesrc_ops {
	enum sharebuffer_srcs src;
	void *private;

	int (*prepare)(struct snd_pcm_substream *substream,
			void *pfrddr,
			int samesource_sel,
			int lane_i2s,
			enum aud_codec_types type,
			int share_lvl,
			int separated);
	int (*trigger)(int cmd, int samesource_sel, bool reenable);
	int (*hw_free)(struct snd_pcm_substream *substream,
		     void *pfrddr,
		     int samesource_sel,
		     int share_lvl);
	int (*set_clks)(int id,
		struct clk *clk_src, int rate, int same);
	void (*mute)(int id, bool mute);
	void (*reset)(unsigned int id, int offset);
};

struct samesrc_ops *get_samesrc_ops(enum sharebuffer_srcs src);
int register_samesrc_ops(enum sharebuffer_srcs src, struct samesrc_ops *ops);

int sharebuffer_prepare(struct snd_pcm_substream *substream,
			void *pfrddr,
			int samesource_sel,
			int lane_i2s,
			enum aud_codec_types type,
			int share_lvl,
			int separated);
int sharebuffer_free(struct snd_pcm_substream *substream,
		     void *pfrddr,
		     int samesource_sel,
		     int share_lvl);
int release_spdif_same_src(struct snd_pcm_substream *substream);
int sharebuffer_trigger(int cmd, int samesource_sel, bool reenable);

void sharebuffer_get_mclk_fs_ratio(int samesource_sel,
				   int *pll_mclk_ratio,
				   int *mclk_fs_ratio);
#endif
