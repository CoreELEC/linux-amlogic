/*
 * drivers/amlogic/mailbox/meson_mhu.h
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
#ifndef __MESON_MHU_FIFO_H__
#define __MESON_MHU_FIFO_H__

#include <linux/cdev.h>
#include <linux/mailbox_controller.h>
#include <linux/amlogic/meson_mhu_common.h>

#define CHANNEL_FIFO_MAX	6
#define MBOX_MAX		CHANNEL_FIFO_MAX
#define MHUIRQ_MAXNUM		32
#define MHU_BUFFER_SIZE		(0x20 * 4) /*128 char*/

#define PAYLOAD_OFFSET(chan)	(0x80 * (chan))
#define CTL_OFFSET(chan)	((chan) * 0x4)

#define IRQ_REV_BIT(mbox)	(1 << ((mbox) * 2))
#define IRQ_SENDACK_BIT(mbox)	(1 << ((mbox) * 2 + 1))

#define IRQ_MASK_OFFSET(x)	(0x00 + ((x) << 2))
#define IRQ_TYPE_OFFSET(x)	(0x10 + ((x) << 2))
#define IRQ_CLR_OFFSET(x)	(0x20 + ((x) << 2))
#define IRQ_STS_OFFSET(x)	(0x30 + ((x) << 2))

/*inclule status 0x4 task id 0x8, ullclt 0x8, completion 0x8*/
#define MBOX_HEAD_SIZE		0x1c
#define MBOX_RESEV_SIZE		0x4

#define MBOX_DATA_SIZE \
	(MHU_BUFFER_SIZE - MBOX_HEAD_SIZE - MBOX_RESEV_SIZE)

/*user space mailbox cmd len define type int*/
#define MBOX_USER_CMD_LEN	4

#define MBOX_USER_MAX_LEN	(MBOX_DATA_SIZE + MBOX_USER_CMD_LEN)
/*u64 taskid*/
#define MBOX_TASKID_LEN		8
/* u64 compete*/
#define MBOX_COMPETE_LEN	8
/* u64 ullctl*/
#define MBOX_ULLCLT_LEN		8

#define ASYNC_CMD		1
#define SYNC_CMD		2
#define SYNC_SHIFT(val)		((val) << 25)

#define SIZE_MASK		0x1FF
#define SIZE_SHIFT(val)		(((val) & SIZE_MASK) << 16)
#define SIZE_LEN(val)		(((val) >> 16) & SIZE_MASK)

#define CMD_MASK                0xFFFF
#define CMD_SHIFT(val)		(((val) & CMD_MASK) << 0)

#define TYPE_SHIFT              16
#define TYPE_VALUE(val)         ((val) << TYPE_SHIFT)

#define REV_MBOX_MASK		0xAA

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

struct mbox_data {
	u32 status;
	u64 task;
	u64 complete;
	u64 ullclt;
	char data[MBOX_DATA_SIZE];
} __packed;

struct mhu_chan {
	int index;
	int mhu_id;
	char mhu_name[32];
	struct mhu_ctrl *ctrl;
	struct mhu_data_buf *data;
};

struct mhu_ctrl {
	struct device *dev;
	void __iomem *mbox_wr_base;
	void __iomem *mbox_rd_base;
	void __iomem *mbox_set_base;
	void __iomem *mbox_clr_base;
	void __iomem *mbox_sts_base;
	void __iomem *mbox_irq_base;
	void __iomem *mbox_payload_base;
	struct mbox_controller mbox_con;
	struct mhu_chan *channels;
	int mhu_id[MBOX_MAX];
	int mhu_irq;
	int mhu_irqctr;
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

extern struct device *mhu_fifo_device;
#endif
