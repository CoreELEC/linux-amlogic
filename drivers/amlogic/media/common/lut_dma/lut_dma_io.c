/*
 * drivers/amlogic/media/common/lut_dma/lut_dma_io.c
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

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include "lut_dma_io.h"

u32 lut_dma_reg_read(u32 reg)
{
	u32 ret = 0;

	ret = (u32)aml_read_vcbus(reg);
	return ret;
};

void lut_dma_reg_write(u32 reg, const u32 val)
{
	aml_write_vcbus(reg, val);
};

void lut_dma_reg_set_mask(u32 reg,
			  const u32 mask)
{
	lut_dma_reg_write(reg, (lut_dma_reg_read(reg) | (mask)));
}

void lut_dma_reg_clr_mask(u32 reg,
			  const u32 mask)
{
	lut_dma_reg_write(reg, (lut_dma_reg_read(reg) & (~(mask))));
}

void lut_dma_reg_set_bits(u32 reg,
			  const u32 value,
			  const u32 start,
			  const u32 len)
{
	lut_dma_reg_write(reg, ((lut_dma_reg_read(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

