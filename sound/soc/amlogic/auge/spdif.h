/*
 * sound/soc/amlogic/auge/spdif.h
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

#ifndef __AML_SPDIF_H__
#define __AML_SPDIF_H__
#include <linux/clk.h>

/* For spdifout mask lane register offset V1:
 * EE_AUDIO_SPDIFOUT_CTRL0, offset: 4 - 11
 */
#define SPDIFOUT_LANE_MASK_V1 1
/* For spdifout mask lane register offset V2:
 * EE_AUDIO_SPDIFOUT_CTRL0, offset: 0 - 15
 */
#define SPDIFOUT_LANE_MASK_V2 2

extern int spdif_set_audio_clk(int id,
		struct clk *clk_src, int rate, int same);

int spdifout_get_lane_mask_version(int id);
unsigned int spdif_get_codec(void);
#endif
