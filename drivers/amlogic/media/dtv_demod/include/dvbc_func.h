/*
 * drivers/amlogic/media/dtv_demod/include/dvbc_func.h
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

#ifndef __DVBC_FUNC_H__
#define __DVBC_FUNC_H__

enum qam_md_e {
	QAM_MODE_16,
	QAM_MODE_32,
	QAM_MODE_64,
	QAM_MODE_128,
	QAM_MODE_256,
	QAM_MODE_NUM
};

u32 dvbc_get_symb_rate(void);

#define SYMB_CNT_CFG		0x3
#define SR_OFFSET_ACC		0x8
#define SR_SCAN_SPEED		0xc
#define TIM_SWEEP_RANGE_CFG	0xe
#endif
