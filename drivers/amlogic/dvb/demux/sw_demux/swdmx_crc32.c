/*
 * drivers/amlogic/dvb/demux/sw_demux/swdmx_crc32.c
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

#include "swdemux_internal.h"

static u32 crc32_table[256];

static void crc32_table_init(void)
{
	u32 i, j, k;

	for (i = 0; i < 256; i++) {
		k = 0;
		for (j = (i << 24) | 0x800000; j != 0x80000000; j <<= 1)
			k = (k << 1) ^ (((k ^ j) & 0x80000000)
					? 0x04c11db7 : 0);
		crc32_table[i] = k;
	}
}

u32 swdmx_crc32(const u8 *p, int len)
{
	u32 i_crc = 0xffffffff;

	if (crc32_table[1] == 0)
		crc32_table_init();

	while (len) {
		i_crc = (i_crc << 8) ^ crc32_table[(i_crc >> 24) ^ *p];
		p++;
		len--;
	}
	return i_crc;
}
