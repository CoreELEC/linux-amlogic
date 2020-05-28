/*
 * include/linux/amlogic/media/lut_dma/lut_dma.h
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

#ifndef LUT_DMA_H_
#define LUT_DMA_H_

#define LOCAL_DIMMING_CHAN     0
#define FILM_GRAIN0_CHAN       1
#define FILM_GRAIN1_CHAN       2
#define FILM_GRAIN_DI_CHAN     3

enum LUT_DMA_DIR_e {
	LUT_DMA_RD,
	LUT_DMA_WR,
};

enum LUT_DMA_MODE_e {
	LUT_DMA_AUTO,
	LUT_DMA_MANUAL,
};

enum IRQ_TYPE_e {
	VIU1_VSYNC,
	ENCL_GO_FEILD,
	ENCP_GO_FEILD,
	ENCI_GO_FEILD,
	VIU2_VSYNC,
	VIU1_LINE_N,
	VDIN0_VSYNC,
	DI_VSYNC,
};

struct lut_dma_set_t {
	u32 dma_dir;
	u32 table_size;
	u32 channel;
	u32 irq_source;
	u32 mode;
};

int lut_dma_register(struct lut_dma_set_t *lut_dma_set);
void lut_dma_unregister(u32 dma_dir, u32 channel);
int lut_dma_read(u32 channel, void *paddr);
int lut_dma_write(u32 channel, void *paddr, u32 size);
int lut_dma_write_phy_addr(u32 channel, u32 phy_addr, u32 size);
void lut_dma_update_irq_source(u32 channel, u32 irq_source);
#endif
