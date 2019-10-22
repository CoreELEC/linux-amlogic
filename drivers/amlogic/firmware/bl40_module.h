/*
 * drivers/amlogic/firmware/bl40_module.h
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

#ifndef __BL40_MODULE_H__
#define __BL40_MODULE_H__

#ifdef CONFIG_AMLOGIC_FIRMWARE
void bl40_rx_msg(void *msg, int size);
#else
static inline void bl40_rx_msg(void *msg, int size)
{
}
#endif

#endif /*__BL40_MODULE_H__*/
