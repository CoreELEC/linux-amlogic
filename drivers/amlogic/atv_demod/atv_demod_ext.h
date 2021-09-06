/*
 * drivers/amlogic/atv_demod/atv_demod_ext.h
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

#ifndef __ATV_DEMOD_EXT_H__
#define __ATV_DEMOD_EXT_H__

/* This module is atv demod interacts with other modules */

extern bool aml_fe_has_hook_up(void);
extern bool aml_fe_hook_call_get_fmt(int *fmt);
extern bool aml_fe_hook_call_set_mode(bool mode);

extern void atvdemod_power_switch(bool on);

#endif /* __ATV_DEMOD_EXT_H__ */
