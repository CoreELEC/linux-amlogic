/*
 * drivers/amlogic/media/di_multi_v3/di_pre_hw.h
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

#ifndef __DI_PRE_HW_H__
#define __DI_PRE_HW_H__

void hprev3_prob(void);
void hprev3_remove(void);
unsigned char isv3_source_change_dfvm(struct dim_dvfm_s *plstdvfm,
				    struct dim_dvfm_s *pdvfm,
				    unsigned int ch);
void dimv3_set_pre_dfvm(struct dim_dvfm_s *plstdvfm,
		      struct dim_dvfm_s *pdvfm);
void dimv3_set_pre_dfvm_last_bypass(struct di_ch_s *pch);

#endif	/*__DI_PRE_HW_H__*/
