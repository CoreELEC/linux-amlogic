/*
 * drivers/amlogic/media/vout/cvbs/cvbs_mode.h
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

#ifndef _CVBS_OUT_H_
#define _CVBS_OUT_H_

#include <linux/amlogic/media/vout/vinfo.h>

enum cvbs_mode_e {
	MODE_480CVBS = 0,
	MODE_576CVBS,
	MODE_PAL_M,
	MODE_PAL_N,
	MODE_NTSC_M,
	MODE_MAX,
};

const struct vinfo_s *get_valid_vinfo(char  *mode);
enum cvbs_mode_e get_local_cvbs_mode(void);
int cvbs_set_current_vmode(enum vmode_e mode);

#endif
