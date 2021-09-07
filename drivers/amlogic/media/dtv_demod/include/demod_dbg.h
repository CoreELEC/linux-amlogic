/*
 * drivers/amlogic/media/dtv_demod/include/demod_dbg.h
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
#ifndef __DTV_DMD_DBG_H
#define __DTV_DMD_DBG_H
#include <linux/bitops.h>

#define CAP_SIZE (50)
#define PER_WR	(BIT(20))

#define ADC_PLL_CNTL0_TL1	(0xb0 << 2)
#define ADC_PLL_CNTL1_TL1	(0xb1 << 2)
#define ADC_PLL_CNTL2_TL1	(0xb2 << 2)

enum {
	ATSC_READ_STRENGTH = 0,
	ATSC_READ_SNR = 1,
	ATSC_READ_LOCK = 2,
	ATSC_READ_SER = 3,
	ATSC_READ_FREQ = 4,
};

enum {
	DTMB_READ_STRENGTH = 0,
	DTMB_READ_SNR = 1,
	DTMB_READ_LOCK = 2,
	DTMB_READ_BCH = 3,
};

struct demod_debugfs_files_t {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

void aml_demod_dbg_init(void);
void aml_demod_dbg_exit(void);
int dtvdemod_create_class_files(struct class *clsp);
void dtvdemod_remove_class_files(struct class *clsp);
#endif
