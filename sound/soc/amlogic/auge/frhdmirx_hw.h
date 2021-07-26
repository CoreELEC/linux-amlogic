/*
 * sound/soc/amlogic/auge/frhdmirx_hw.h
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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
#ifndef __FRHDMIRX_HW_H__
#define __FRHDMIRX_HW_H__

#define INT_PAO_PAPB_MASK    24
#define INT_PAO_PCPD_MASK    16

enum hdmirx_mode {
	HDMIRX_MODE_SPDIFIN = 0,
	HDMIRX_MODE_PAO = 1,
};

enum {
	EARCTX_SPDIF_TO_HDMIRX = 0,
	SPDIFA_TO_HDMIRX = 1,
	SPDIFB_TO_HDMIRX = 2,
	HDMIRX_SPDIF_TO_HDMIRX = 3,
};

void arc_source_enable(int src, bool enable);
void arc_earc_source_select(int src);
void arc_enable(bool enable);

void frhdmirx_enable(bool enable);
void frhdmirx_src_select(int src);
void frhdmirx_ctrl(int channels, int src);
void frhdmirx_clr_PAO_irq_bits(void);
void frhdmirx_clr_SPDIF_irq_bits(void);
unsigned int frhdmirx_get_ch_status(int num);
unsigned int frhdmirx_get_chan_status_pc(enum hdmirx_mode mode);
void frhdmirx_clr_all_irq_bits(void);
void frhdmirx_afifo_reset(void);

#endif
