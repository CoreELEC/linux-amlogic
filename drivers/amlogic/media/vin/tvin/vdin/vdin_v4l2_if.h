/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_v4l2_if.h
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

#ifndef __VDIN_V4L2_IF_H
#define __VDIN_V4L2_IF_H

#define VDIN_DEV_VER		0x20201003
#define VDIN_DEV_VER2		"change video dev number"
#define VDIN_V4L_DV_NAME	"videovdin"
#define VDIN_VD_NUMBER		(70)

#define NUM_PLANES_YUYV		1
#define NUM_PLANES_NV21		2

#define VDIN_NUM_PLANES		NUM_PLANES_YUYV

struct vdin_vb_buff {
	struct vb2_v4l2_buffer vb;
	struct list_head list;

	struct dma_buf *dmabuf[VB2_MAX_PLANES];

	unsigned int tag;
};

#define to_vdin_vb_buf(buf)	container_of(buf, struct vdin_vb_buff, vb)

char *vb2_memory_sts_to_str(uint32_t memory);

#endif

