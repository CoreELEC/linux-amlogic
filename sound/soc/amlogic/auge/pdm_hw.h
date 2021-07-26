/*
 * sound/soc/amlogic/auge/pdm_hw.h
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

#ifndef __AML_PDM_HW_H__
#define __AML_PDM_HW_H__
#include "audio_io.h"

#define PDM_CHANNELS_MAX		8 /* 8ch pdm in */
#define PDM_LANE_MAX            4 /* 4 data pins, for 8ch maxs*/

struct pdm_info {
	int bitdepth;
	int channels;
	int lane_masks;

	int dclk_idx;  /* mapping for dclk value */
	int bypass;    /* bypass all filter, capture raw data */
	int sample_count;
};

void aml_pdm_ctrl(struct pdm_info *info);

void pdm_force_sysclk_to_oscin(bool force);
void pdm_set_channel_ctrl(int sample_count);

void aml_pdm_arb_config(struct aml_audio_controller *actrl);

int aml_pmd_set_HPF_filter_parameters(void *array);

void aml_pdm_filter_ctrl(int pdm_gain_index, int osr, int set);

void pdm_enable(int is_enable);

void pdm_fifo_reset(void);

int pdm_get_mute_value(void);
void pdm_set_mute_value(int val);
int pdm_get_mute_channel(void);
void pdm_set_mute_channel(int mute_chmask);
void pdm_set_bypass_data(bool bypass);
void pdm_init_truncate_data(int freq);
void pdm_train_en(bool en);
void pdm_train_clr(void);
int pdm_train_sts(void);

int pdm_dclkidx2rate(int idx);
int pdm_get_sample_count(int is_low_power, int dclk_idx);
int pdm_get_ors(int dclk_idx, int sample_rate);

#endif /*__AML_PDM_HW_H__*/
