/*
 * drivers/amlogic/mailbox/meson_mhu_common.h
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
#ifndef _MESON_MHU_COMMON_H_
#define _MESON_MHU_COMMON_H_

struct mhu_data_buf {
	u32 cmd;
	int tx_size;
	void *tx_buf;
	int rx_size;
	void *rx_buf;
	void *cl_data;
};

extern struct device *mhu_device;
extern struct device *mhu_fifo_device;

#define CONTROLLER_NAME		"mhu_ctlr"

/**wait other core ack time: ms**/
#define MBOX_TIME_OUT		10000

#define MASK_MHU		(BIT(0))
#define MASK_MHU_FIFO		(BIT(1))
#define MASK_MHU_PL		(BIT(2))

extern u32 mhu_f;
#endif
