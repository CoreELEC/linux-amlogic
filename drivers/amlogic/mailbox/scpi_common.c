/*
 * drivers/amlogic/mailbox/scpi_protocol.c
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
#define DEBUG 1
#include <linux/err.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/mailbox_client.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/slab.h>

/*
 * BIT0 MHU mbox
 * BIT1 MHU DSP mbox
 * BIT2 MHU FIFO mbox
 */

u32 mhu_f;
u32 mhu_dsp_f;
u32 mhu_fifo_f;

int scpi_get_vrtc(u32 *p_vrtc)
{
	int ret = 0;

	if (mhu_fifo_f == 0xff) {
		u32 temp = 0;
		u32 revdata = 0;

		ret = scpi_send_fifo(&temp, sizeof(temp),
				     SCPI_AOCPU, SCPI_CMD_GET_RTC,
				     0, &revdata, sizeof(revdata));
		if (!ret)
			*p_vrtc = revdata;

	} else {
		if (mhu_f == 0xff)
			ret =  _scpi_get_vrtc(p_vrtc);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(scpi_get_vrtc);

