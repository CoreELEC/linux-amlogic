/*
 * include/linux/amlogic/meson_mhu_common.h
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
#include <linux/cdev.h>
#include <linux/mailbox_controller.h>

#define ASYNC_CMD		1
#define SYNC_CMD		2
#define SYNC_SHIFT(val)		((val) << 25)
#define SYNC_MASK		0x7

#define SIZE_MASK		0x1FF
#define SIZE_SHIFT(val)		(((val) & SIZE_MASK) << 16)
#define SIZE_LEN(val)		(((val) >> 16) & SIZE_MASK)

#define CMD_MASK                0xFFFF
#define CMD_SHIFT(val)		(((val) & CMD_MASK) << 0)

#define TYPE_SHIFT              16
#define TYPE_VALUE(val)         ((val) << TYPE_SHIFT)

#define MBOX_TIME_OUT		10000

#define MASK_MHU                (BIT(0))
#define MASK_MHU_FIFO           (BIT(1))
#define MASK_MHU_PL             (BIT(2))
#define MASK_MHU_SEC            (BIT(3))

int get_dsp_online_status(const char *dsp_name);

extern struct device *mhu_device;
extern struct device *mhu_fifo_device;
extern struct device *mhu_pl_device;
extern struct device *mhu_sec_device;

extern u32 mhu_f;

struct mhu_data_buf {
	u32 cmd;
	int tx_size;
	void *tx_buf;
	int rx_size;
	void *rx_buf;
	void *cl_data;
};

enum call_type {
	LISTEN_CALLBACK = TYPE_VALUE(0),
	LISTEN_DATA = TYPE_VALUE(1),
};

struct mbox_message {
	struct list_head list;
	struct task_struct *task;
	struct completion complete;
	int cmd;
	char *data;
};

struct mhu_chan {
	int index;
	int rx_irq;
	int mhu_id;
	char mhu_name[32];
	struct mhu_ctlr *ctlr;
	struct mhu_data_buf *data;
};

struct mhu_mbox {
	int channel_id;
	/*for mhu channel*/
	struct mutex mutex;
	struct list_head char_list;
	dev_t char_no;
	struct cdev char_cdev;
	struct device *char_dev;
	char char_name[32];
	int mhu_id;
	struct device *mhu_dev;
	/*mhu lock for mhu hw reg*/
	spinlock_t mhu_lock;
};
#endif
