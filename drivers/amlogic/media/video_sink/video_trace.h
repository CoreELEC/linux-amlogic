/*
 * drivers/amlogic/media/video_sink/video_trace.h
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

#undef TRACE_SYSTEM
#define TRACE_SYSTEM video

#if !defined(_VIDEO_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _VIDEO_TRACE_H

#include <linux/tracepoint.h>

/* single lifecycle events */
DECLARE_EVENT_CLASS(
	video_event_class,
	TP_PROTO(const char *name, int left, int top, int right, int bottom),
	TP_ARGS(name, left, top, bottom, right),
	TP_STRUCT__entry(
		__string(name, name)
		__field(int, left)
		__field(int, top)
		__field(int, right)
		__field(int, bottom)
	),
	TP_fast_assign(
		__assign_str(name, name);
		__entry->left = left;
		__entry->top = top;
		__entry->right = right;
		__entry->bottom = bottom;
	),
	TP_printk("[%s:%d %d %d %d]",  __get_str(name),
		  __entry->left, __entry->top,
		  __entry->right, __entry->bottom)
);

#define DEFINE_VIDEO_EVENT(name) \
DEFINE_EVENT(video_event_class, name, \
	TP_PROTO(const char *name, int left, int top, int right, int bottom), \
	TP_ARGS(name, left, top, right, bottom))

DEFINE_VIDEO_EVENT(axis);
#endif /* _VIDEO_TRACE_H */
