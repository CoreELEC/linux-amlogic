/*
 * drivers/amlogic/media/enhancement/amvecm/am_hdr10_tmo_fw.h
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

#include "../amcsc.h"

void hdr10_tmo_gen(u32 *oo_gain);
int hdr10_tmo_dbg(char **param);
void hdr10_tmo_parm_show(void);
void hdr10_tmo_reg_set(struct hdr_tmo_sw *pre_tmo_reg);
void hdr10_tmo_reg_get(struct hdr_tmo_sw *pre_tmo_reg_s);
void hdr_tmo_adb_show(char *str);

