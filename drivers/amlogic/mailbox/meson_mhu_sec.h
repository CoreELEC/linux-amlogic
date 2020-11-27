/*
 * drivers/amlogic/mailbox/meson_mhu_fifo.h
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

#ifndef __MESON_MHU_SEC_H__
#define __MESON_MHU_SEC_H__

#include <linux/cdev.h>
#include <linux/mailbox_controller.h>
#include <linux/amlogic/meson_mhu_common.h>

#define CHANNEL_SEC_MAX		2
#define MBOX_MAX		CHANNEL_SEC_MAX

/*inclule status 0x4 task id 0x8, ullclt 0x8, completion 0x8*/

#define MBOX_SEC_SIZE		0x80

/* u64 compete*/
#define MBOX_COMPETE_LEN	8
#define REV_MBOX_MASK		0x5555
#endif
