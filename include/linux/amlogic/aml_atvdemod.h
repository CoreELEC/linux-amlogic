/*
 * include/linux/amlogic/aml_atvdemod.h
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

#ifndef __AML_ATVDEMOD_H__
#define __AML_ATVDEMOD_H__

#include <linux/amlogic/aml_demod_common.h>

typedef int (*hook_func_t)(void);
typedef int (*hook_func1_t)(bool);

#if (defined CONFIG_AMLOGIC_ATV_DEMOD)
/* For audio driver get atv audio state */
void aml_fe_get_atvaudio_state(int *state);
/* For atv demod hook tvafe driver state */
void aml_fe_hook_cvd(hook_func_t atv_mode, hook_func_t cvd_hv_lock,
		     hook_func_t get_fmt, hook_func1_t set_mode);
#else
static inline __maybe_unused void aml_fe_get_atvaudio_state(int *state)
{
	*state = 0;
}

static inline __maybe_unused void aml_fe_hook_cvd(hook_func_t atv_mode,
		hook_func_t cvd_hv_lock, hook_func_t get_fmt,
		hook_func1_t set_mode)
{
}
#endif

#endif /* __AML_ATVDEMOD_H__ */
