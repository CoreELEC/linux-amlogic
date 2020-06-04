/*
 * drivers/amlogic/media/di_multi_v3/di_pre.h
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

#ifndef __DI_PRE_H__
#define __DI_PRE_H__

void dprev3_process(void);

void dprev3_init(void);

const char *dprev3_state_name_get(enum eDI_PRE_ST state);
void dprev3_dbg_f_trig(unsigned int cmd);
void prev3_vinfo_set(unsigned int ch, struct di_in_inf_s *pinf);

unsigned int isv3_vinfo_change(unsigned int ch);
bool dprev3_can_exit(unsigned int ch);
//no use bool is_bypass_i_p(void);
bool dimv3_bypass_detect(unsigned int ch, struct vframe_s *vfm);

void prev3_mode_setting(void);
bool dprev3_process_step4(void);
const char *dprev3_state4_name_get(enum eDI_PRE_ST4 state);

#endif	/*__DI_PRE_H__*/
