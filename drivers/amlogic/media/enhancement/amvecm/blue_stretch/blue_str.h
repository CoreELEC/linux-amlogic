/*
 * drivers/amlogic/media/enhancement/amvecm/blue_stretch/blue_str.h
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

#ifndef BLUE_STRETCH_H
#define BLUE_STRETCH_H

#define BITDEPTH 10
void bls_par_show(void);
int bls_par_dbg(char **param);
//void lut3d_yuv2rgb(int en);
extern int bs_proc_en;
#endif
