/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/reg_ops.h
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

#ifndef __REG_OPS_H__
#define __REG_OPS_H__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/amlogic/iomap.h>

/*
 * RePacket HDMI related registers rd/wr
 */
struct reg_map {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *p;
};

extern struct reg_map *map;

unsigned int TO_PHY_ADDR(unsigned int addr);
void __iomem *TO_PMAP_ADDR(unsigned int addr);

extern unsigned int hd_read_reg(unsigned int addr);
extern void hd_write_reg(unsigned int addr, unsigned int val);
extern void hd_set_reg_bits(unsigned int addr, unsigned int value,
		unsigned int offset, unsigned int len);
extern void init_reg_map(unsigned int type);

#endif
