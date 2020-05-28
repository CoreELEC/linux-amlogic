/*
 * drivers/amlogic/media/common/lut_dma/lut_dma_io.h
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

#ifndef _LUT_DMA_IO_H_
#define _LUT_DMA_IO_H_
#include <linux/amlogic/iomap.h>

u32 lut_dma_reg_read(u32 reg);
void lut_dma_reg_write(u32 reg, const u32 val);
void lut_dma_reg_set_mask(u32 reg,
			  const u32 mask);
void lut_dma_reg_clr_mask(u32 reg,
			  const u32 mask);
void lut_dma_reg_set_bits(u32 reg,
			  const u32 value,
			  const u32 start,
			  const u32 len);
#endif
