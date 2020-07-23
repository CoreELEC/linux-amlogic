/*
 * drivers/amlogic/dvb/demux/dmx_log.h
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

#ifndef _DMX_LOG_H_
#define _DMX_LOG_H_

#define LOG_ERROR      0
#define LOG_DBG        1
#define LOG_VER        2

#define dprintk(level, debug, x...)\
	do {\
		if ((level) <= (debug)) \
			printk(x);\
	} while (0)
#endif
