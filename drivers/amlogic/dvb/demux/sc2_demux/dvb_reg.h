/*
 * drivers/amlogic/dvb/demux/sc2_demux/dvb_reg.h
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

#ifndef _DVB_REG_H_
#define _DVB_REG_H_

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/platform_device.h>

void aml_write_self(unsigned int reg, unsigned int val);
int aml_read_self(unsigned int reg);
int init_demux_addr(struct platform_device *pdev);

#define WRITE_CBUS_REG(_r, _v)   aml_write_self((_r), _v)
#define READ_CBUS_REG(_r)        aml_read_self((_r))

#endif
