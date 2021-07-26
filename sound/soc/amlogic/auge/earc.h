/*
 * sound/soc/amlogic/auge/earc.h
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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

#ifndef __EARC_H__
#define __EARC_H__
#include <linux/amlogic/media/sound/iec_info.h>
/* earc probe is at arch_initcall stage which is earlier to normal driver */
bool is_earc_spdif(void);
void aml_earctx_enable(bool enable);
int sharebuffer_earctx_prepare(struct snd_pcm_substream *substream,
	struct frddr *fr, enum aud_codec_types type);
bool aml_get_earctx_enable(void);

#endif

