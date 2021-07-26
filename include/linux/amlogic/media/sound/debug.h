/*
 * include/linux/amlogic/media/sound/debug.h
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

#ifndef __AUDIO_DEBUG_H__
#define __AUDIO_DEBUG_H__

extern bool audio_debug;

#define aud_dbg(dev, fmt, args...) \
({                                 \
	if (audio_debug)               \
		dev_dbg(dev, fmt, ##args); \
})
#endif
