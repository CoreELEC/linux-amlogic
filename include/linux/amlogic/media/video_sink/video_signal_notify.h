/*
 * include/linux/amlogic/media/video_sink/video_signal_notify.h
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

#ifndef VIDEO_SIGNAL_NOTIFY_H
#define VIDEO_SIGNAL_NOTIFY_H
#include <linux/notifier.h>
#include <linux/list.h>

#define VIDEO_SIGNAL_TYPE_CHANGED   0x0001
#define VIDEO_SECURE_TYPE_CHANGED   0x0002

enum vd_format_e {
	SIGNAL_INVALID = -1,
	SIGNAL_SDR = 0,
	SIGNAL_HDR10 = 1,
	SIGNAL_HLG = 2,
	SIGNAL_HDR10PLUS = 3,
	SIGNAL_DOVI = 4,
	SIGNAL_CUVA = 5,
};

struct vd_signal_info_s {
	enum vd_format_e signal_type;
	enum vd_format_e vd1_signal_type;
	enum vd_format_e vd2_signal_type;
	u32 reversed;
};

int vd_signal_register_client(struct notifier_block *nb);
int vd_signal_unregister_client(struct notifier_block *nb);
int vd_signal_notifier_call_chain(unsigned long val, void *v);

#endif /* VIDEO_SIGNAL_NOTIFY_H */
