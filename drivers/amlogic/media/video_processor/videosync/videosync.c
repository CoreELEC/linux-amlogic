/*
 * drivers/amlogic/media/video_processor/videosync/videosync.c
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

#define DEBUG

#include "videosync.h"
#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>


#define VIDEOSYNC_DEVICE_NAME   "videosync"
#define RECEIVER_NAME "videosync"
#define PROVIDER_NAME "videosync"

static bool videosync_inited;/*false*/
static bool show_nosync;/*false*/
static bool smooth_sync_enable;
static int enable_video_discontinue_report = 1;
static u32 system_time_scale_base = 1;
static s32 system_time_inc_adj;/*?*/
static u32 vsync_pts_inc;/*?*/
static u32 omx_version = 3;
static u32 vp_debug_flag;
static bool no_render;/* default: false */
static bool async_mode;/* default: false */
/*static u32 video_early_threshold = 900;  default: 900=>10ms */


/* video freerun mode */
#define FREERUN_NONE    0	/* no freerun mode */
#define FREERUN_NODUR   1	/* freerun without duration */
#define FREERUN_DUR     2	/* freerun with duration */
#define M_PTS_SMOOTH_MAX 45000
#define M_PTS_SMOOTH_MIN 2250
#define M_PTS_SMOOTH_ADJUST 900
#define DURATION_GCD 750

static int duration_gcd = DURATION_GCD;

static int omx_pts_interval_upper = 11000;
static int omx_pts_interval_lower = -5500;
#define DUR2PTS(x) ((x) - ((x) >> 4))

#define PRINT_ERROR			0X0
#define PRINT_QUEUE_STATUS	0X0001
#define PRINT_TIMESTAMP		0X0002
#define PRINT_PATTERN		0X0004
#define PRINT_OTHER			0X0008


static struct videosync_dev *vp_dev;
static uint show_first_frame_nosync;
static uint max_delata_time;

static u32 cur_omx_index;

#define PTS_32_PATTERN_DETECT_RANGE 10
#define PTS_22_PATTERN_DETECT_RANGE 10
#define PTS_41_PATTERN_DETECT_RANGE 2
#define PTS_32_PATTERN_DURATION 3750
#define PTS_22_PATTERN_DURATION 3000

enum video_refresh_pattern {
	PTS_32_PATTERN = 0,
	PTS_22_PATTERN,
	PTS_41_PATTERN,
	PTS_MAX_NUM_PATTERNS
};

static int pts_trace;
static int pre_pts_trace;
static bool pts_enforce_pulldown = true;
static int pts_pattern[3] = {0, 0, 0};
static int pts_pattern_enter_cnt[3] = {0, 0, 0};
static int pts_pattern_exit_cnt[3] = {0, 0, 0};
static int pts_log_enable[3] = {0, 0, 0};
static int pts_escape_vsync = -1;
static s32 vsync_pts_align = -DURATION_GCD / 2;
static int pts_pattern_detected = -1;

static int vp_print(char *name, int debug_flag, const char *fmt, ...)
{
	if ((vp_debug_flag & debug_flag)
		|| (debug_flag == PRINT_ERROR)) {
		unsigned char buf[512];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "[%s]", name);
		vsnprintf(buf + len, 512-len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static u32 ts_pcrscr_get(struct videosync_s *dev_s)
{
	u32 sys_time = 0;

	sys_time = dev_s->system_time;
	return sys_time;
}

static void ts_pcrscr_set(struct videosync_s *dev_s, u32 pts)
{
	dev_s->system_time = pts;
	vp_print(dev_s->vf_receiver_name, PRINT_TIMESTAMP,
			"ts_pcrscr_set sys_time %u\n", dev_s->system_time);
}

static void ts_pcrscr_enable(struct videosync_s *dev_s, u32 enable)
{
	dev_s->system_time_up = enable;
}
static u32 ts_pcrscr_enable_state(struct videosync_s *dev_s)
{
	return dev_s->system_time_up;
}

static void log_vsync_video_pattern(int pattern)
{
	int factor1 = 0, factor2 = 0, pattern_range = 0;

	if (pattern >= PTS_MAX_NUM_PATTERNS)
		return;

	if (pattern == PTS_32_PATTERN) {
		factor1 = 3;
		factor2 = 2;
		pattern_range =  PTS_32_PATTERN_DETECT_RANGE;
	} else if (pattern == PTS_22_PATTERN) {
		factor1 = 2;
		factor2 = 2;
		pattern_range =  PTS_22_PATTERN_DETECT_RANGE;
	} else if (pattern == PTS_41_PATTERN) {
		pr_info("not support 41 pattern\n");
		return;
	}

	/* update 3:2 or 2:2 mode detection */
	if (((pre_pts_trace == factor1) && (pts_trace == factor2)) ||
	    ((pre_pts_trace == factor2) && (pts_trace == factor1))) {
		if (pts_pattern[pattern] < pattern_range) {
			pts_pattern[pattern]++;
			if (pts_pattern[pattern] == pattern_range) {
				pts_pattern_enter_cnt[pattern]++;
				pts_pattern_detected = pattern;
				if (pts_log_enable[pattern])
					pr_info("video %d:%d mode detected\n",
						factor1, factor2);
			}
		}
	} else if (pts_pattern[pattern] == pattern_range) {
		pts_pattern[pattern] = 0;
		pts_pattern_exit_cnt[pattern]++;
		if (pts_log_enable[pattern])
			pr_info("video %d:%d mode broken\n", factor1, factor2);
	} else {
		pts_pattern[pattern] = 0;
	}
}

static void vsync_video_pattern(void)
{
	/*log_vsync_video_pattern(PTS_32_PATTERN);*/
	log_vsync_video_pattern(PTS_22_PATTERN);
	/*log_vsync_video_pattern(PTS_41_PATTERN);*/
}

static inline void vpts_perform_pulldown(
	struct videosync_s *dev_s,
	struct vframe_s *next_vf,
	bool *expired)
{
	int pattern_range, expected_curr_interval;
	int expected_prev_interval;
	int next_vf_nextpts = 0;
	int nextPts;

	/* Dont do anything if we have invalid data */
	if (!next_vf || !next_vf->pts)
		return;
	if (next_vf->next_vf_pts_valid)
		next_vf_nextpts = next_vf->next_vf_pts;

	switch (pts_pattern_detected) {
	case PTS_32_PATTERN:
		pattern_range = PTS_32_PATTERN_DETECT_RANGE;
		switch (pre_pts_trace) {
		case 3:
			expected_prev_interval = 3;
			expected_curr_interval = 2;
			break;
		case 2:
			expected_prev_interval = 2;
			expected_curr_interval = 3;
			break;
		default:
			return;
		}
		if (!next_vf_nextpts)
			next_vf_nextpts = next_vf->pts +
				PTS_32_PATTERN_DURATION;
		break;
	case PTS_22_PATTERN:
		if (pre_pts_trace != 2)
			return;
		pattern_range =  PTS_22_PATTERN_DETECT_RANGE;
		expected_prev_interval = 2;
		expected_curr_interval = 2;
		if (!next_vf_nextpts)
			next_vf_nextpts = next_vf->pts +
				PTS_22_PATTERN_DURATION;
		break;
	case PTS_41_PATTERN:
		/* TODO */
	default:
		return;
	}

	/* We do nothing if  we dont have enough data*/
	if (pts_pattern[pts_pattern_detected] != pattern_range)
		return;

	if (*expired) {
		if (pts_trace < expected_curr_interval) {
			/* 2323232323..2233..2323, prev=2, curr=3,*/
			/* check if next frame will toggle after 3 vsyncs */
			/* 22222...22222 -> 222..2213(2)22...22 */
			/* check if next frame will toggle after 3 vsyncs */
			nextPts = ts_pcrscr_get(dev_s) + vsync_pts_align;

			if (((int)(nextPts + (expected_prev_interval + 1) *
				vsync_pts_inc - next_vf_nextpts) >= 0)) {
				*expired = false;
				if (pts_log_enable[PTS_32_PATTERN] ||
				    pts_log_enable[PTS_22_PATTERN])
					pr_info("hold frame for pattern: %d",
						pts_pattern_detected);
			}

			/* here need to escape a vsync */
			if (ts_pcrscr_get(dev_s) >
				(next_vf->pts + vsync_pts_inc)) {
				*expired = true;
				pts_escape_vsync = 1;
				if (pts_log_enable[PTS_32_PATTERN] ||
				    pts_log_enable[PTS_22_PATTERN])
					pr_info("escape a vsync pattern: %d",
						pts_pattern_detected);
			}
		}
	} else {
		if (pts_trace == expected_curr_interval) {
			/* 23232323..233223...2323 curr=2, prev=3 */
			/* check if this frame will expire next vsyncs and */
			/* next frame will expire after 3 vsyncs */
			/* 22222...22222 -> 222..223122...22 */
			/* check if this frame will expire next vsyncs and */
			/* next frame will expire after 2 vsyncs */
			int nextPts = ts_pcrscr_get(dev_s) + vsync_pts_align;

			if (((int)(nextPts + vsync_pts_inc - next_vf->pts)
				>= 0) &&
			    ((int)(nextPts +
			    vsync_pts_inc * (expected_prev_interval - 1)
			    - next_vf_nextpts) < 0) &&
			    ((int)(nextPts + expected_prev_interval *
				vsync_pts_inc - next_vf_nextpts) >= 0)) {
				*expired = true;
				if (pts_log_enable[PTS_32_PATTERN] ||
				    pts_log_enable[PTS_22_PATTERN])
					pr_info("pull frame for pattern: %d",
						pts_pattern_detected);
			}
		}
	}
}

void videosync_pcrscr_update(s32 inc, u32 base)
{
	int i = 0;
	u32 r;
	/*unsigned long flags;*/
	struct videosync_s *dev_s;
	u32 current_omx_pts;
	int diff;

	if (!videosync_inited)
		return;
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s && dev_s->active_state == VIDEOSYNC_ACTIVE) {
			if (system_time_scale_base != base) {
				dev_s->system_time_scale_remainder =
					dev_s->system_time_scale_remainder *
						base / system_time_scale_base;
				system_time_scale_base = base;
			}
			if (dev_s->system_time_up) {
				dev_s->time_update = sched_clock();
				dev_s->system_time +=
					div_u64_rem(90000ULL * inc, base, &r)
					+ system_time_inc_adj;
				vsync_pts_inc = 90000 * inc / base;
				dev_s->system_time_scale_remainder += r;
				if (dev_s->system_time_scale_remainder
					>= system_time_scale_base) {
					dev_s->system_time++;
					dev_s->system_time_scale_remainder
						-= system_time_scale_base;
				}
				vp_print(dev_s->vf_receiver_name, PRINT_OTHER,
				"update sys_time %u, system_time_scale_base %d, inc %d\n",
				dev_s->system_time,
				system_time_scale_base,
				inc);
			}

			/*check if need to correct pcr by omx_pts*/
			if (!dev_s->vmaster_mode) {
				current_omx_pts = dev_s->omx_pts;
				diff = dev_s->system_time - current_omx_pts;

				if ((diff - omx_pts_interval_upper) > 0
					|| (diff - omx_pts_interval_lower)
					< 0) {
					vp_print(dev_s->vf_receiver_name,
						PRINT_TIMESTAMP,
						"sys_time=%u, omx_pts=%u, diff=%d\n",
						dev_s->system_time,
						current_omx_pts,
						diff);
					ts_pcrscr_set(dev_s,
						current_omx_pts + duration_gcd);
				}
			}
		}
	}
}

void videosync_pcrscr_inc(s32 inc)
{
	int i = 0;
	/*unsigned long flags;*/
	struct videosync_s *dev_s;
	u32 current_omx_pts;
	int diff;

	if (!videosync_inited)
		return;
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s && dev_s->active_state == VIDEOSYNC_ACTIVE) {
			if (dev_s->system_time_up) {
				dev_s->system_time += inc + system_time_inc_adj;

				vp_print(dev_s->vf_receiver_name, PRINT_OTHER,
				"update sys_time %u, system_time_inc_adj %d, inc %d\n",
				dev_s->system_time,
				system_time_inc_adj,
				inc);
			}

			/*check if need to correct pcr by omx_pts*/
			if (!dev_s->vmaster_mode) {
				current_omx_pts = dev_s->omx_pts;
				diff = dev_s->system_time - current_omx_pts;

				if ((diff - omx_pts_interval_upper) > 0
					|| (diff - omx_pts_interval_lower)
					< 0) {
					vp_print(dev_s->vf_receiver_name,
						PRINT_TIMESTAMP,
						"sys_time=%u, omx_pts=%u, diff=%d\n",
						dev_s->system_time,
						current_omx_pts,
						diff);
					ts_pcrscr_set(dev_s,
						current_omx_pts + duration_gcd);
				}
			}
		}
	}
}


/* -----------------------------------------------------------------
 *           videosync operations
 * -----------------------------------------------------------------
 */
static struct vframe_s *videosync_vf_peek(void *op_arg)
{
	struct vframe_s *vf = NULL;
	struct videosync_s *dev_s = (struct videosync_s *)op_arg;

	vf = vfq_peek(&dev_s->ready_q);
	return vf;
}

static struct vframe_s *videosync_vf_get(void *op_arg)
{
	struct vframe_s *vf = NULL;
	struct videosync_s *dev_s = (struct videosync_s *)op_arg;

	vf = vfq_pop(&dev_s->ready_q);
	if (vf) {
		dev_s->cur_dispbuf = vf;
		dev_s->first_frame_toggled = 1;
		dev_s->get_vpts = vf->pts;
	}
	return vf;
}

static void videosync_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct videosync_s *dev_s = (struct videosync_s *)op_arg;

	if (vf != NULL) {
		vf_put(vf, dev_s->vf_receiver_name);
		dev_s->put_frame_count++;
	} else {
		vp_print(dev_s->vf_receiver_name, 0,
			"videosync_vf_put: NULL!\n");
	}
}

void vsync_notify_videosync(void)
{
	int i = 0;
	struct videosync_s *dev_s = NULL;
	bool has_active = false;

	if (!videosync_inited)
		return;

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s && dev_s->active_state == VIDEOSYNC_ACTIVE) {
			has_active = true;
			break;
		}
	}
	if (has_active) {
		pts_trace++;
		vp_dev->wakeup = 1;
		wake_up_interruptible(&vp_dev->videosync_wait);
	}
}

static int videosync_event_cb(int type, void *data, void *private_data)
{
	return 0;
}

static int videosync_buffer_states(struct videosync_buffer_states *states,
	void *op_arg)
{
	struct videosync_s *dev_s = (struct videosync_s *)op_arg;

	states->buf_ready_num = vfq_level(&dev_s->ready_q);
	states->buf_queued_num = vfq_level(&dev_s->queued_q);
	states->total_num = dev_s->get_frame_count;
	return 0;
}

static int videosync_buffer_states_vf(struct vframe_states *states,
	void *op_arg)
{
	struct videosync_s *dev_s = (struct videosync_s *)op_arg;

	states->vf_pool_size = VIDEOSYNC_S_POOL_SIZE;
	states->buf_recycle_num = 0;
	states->buf_free_num = VIDEOSYNC_S_POOL_SIZE
		- vfq_level(&dev_s->ready_q)
		- vfq_level(&dev_s->queued_q);
	states->buf_avail_num = vfq_level(&dev_s->ready_q)
		+ vfq_level(&dev_s->queued_q);

	return 0;
}

static const struct vframe_operations_s v4lvideo_vf_provider = {
	.peek = videosync_vf_peek,
	.get = videosync_vf_get,
	.put = videosync_vf_put,
	.event_cb = videosync_event_cb,
	.vf_states = videosync_buffer_states_vf,
};

static void videosync_register(struct videosync_s *dev_s)
{
	vf_provider_init(&dev_s->video_vf_prov,
				dev_s->vf_provider_name,
				&v4lvideo_vf_provider, dev_s);
	vf_reg_provider(&dev_s->video_vf_prov);
	vf_notify_receiver(dev_s->vf_provider_name,
				VFRAME_EVENT_PROVIDER_START,
				NULL);
}

static void videosync_unregister(struct videosync_s *dev_s)
{
	vf_unreg_provider(&dev_s->video_vf_prov);
}

static ssize_t dump_queue_state_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int i = 0;
	struct videosync_buffer_states states;
	struct videosync_s *dev_s = NULL;

	if (vp_dev == NULL)
		return -1;

	if (vp_dev->active_dev_s_num == 0)
		ret += sprintf(buf, "no active videosync\n");
	else {
		for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
			dev_s = &vp_dev->video_prov[i];
			if (dev_s->active_state == VIDEOSYNC_INACTIVE)
				continue;

			if (videosync_buffer_states(&states, dev_s) == 0) {
				ret += sprintf(buf + ret,
					    "\n#------ %s state ------#\n",
						dev_s->vf_receiver_name);
				ret += sprintf(buf + ret,
						"queued_q_size=%d\n",
						states.buf_queued_num);
				ret += sprintf(buf + ret,
						"ready_q_size=%d\n",
						states.buf_ready_num);
				ret += sprintf(buf + ret,
						"total frame count=%d\n",
						states.total_num);
			}
		}
	}
	return ret;
}

static ssize_t dump_pts_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int i = 0;
	struct videosync_s *dev_s = NULL;

	if (vp_dev == NULL)
		return -1;

	if (vp_dev->active_dev_s_num == 0)
		ret += sprintf(buf, "no active videosync\n");
	else {
		for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
			dev_s = &vp_dev->video_prov[i];
			if (dev_s->active_state == VIDEOSYNC_INACTIVE)
				continue;

			ret += sprintf(buf + ret,
					   "%s:  ", dev_s->vf_receiver_name);
			ret += sprintf(buf + ret,
					   "omx_pts=%u, ", dev_s->omx_pts);
			ret += sprintf(buf + ret,
					"system_time=%u\n", dev_s->system_time);
		}
	}
	return ret;
}

static ssize_t dump_get_put_framecount_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int i = 0;
	struct videosync_s *dev_s = NULL;

	if (vp_dev == NULL)
		return -1;

	if (vp_dev->active_dev_s_num == 0)
		ret += sprintf(buf, "no active videosync\n");
	else {
		for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
			dev_s = &vp_dev->video_prov[i];
			if (dev_s->active_state == VIDEOSYNC_INACTIVE)
				continue;

			ret += sprintf(buf + ret,
					   "%s:  ", dev_s->vf_receiver_name);
			ret += sprintf(buf + ret,
						"get_frame_count=%d, put_frame_count=%d\n",
						dev_s->get_frame_count,
						dev_s->put_frame_count);
		}
	}
	return ret;
}

static ssize_t dump_rect_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int i = 0;
	struct videosync_s *dev_s = NULL;

	if (vp_dev == NULL)
		return -1;

	if (vp_dev->active_dev_s_num == 0)
		ret += sprintf(buf, "no active videosync\n");
	else {
		for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
			dev_s = &vp_dev->video_prov[i];
			if (dev_s->active_state == VIDEOSYNC_INACTIVE)
				continue;

			ret += sprintf(buf + ret,
					   "%s:  ", dev_s->vf_receiver_name);
			ret += sprintf(buf + ret,
						"rect = [%d,%d,%d,%d], ",
						dev_s->rect.left,
						dev_s->rect.top,
						dev_s->rect.width,
						dev_s->rect.height);
			ret += sprintf(buf + ret,
						"zorder = %d\n", dev_s->zorder);
		}
	}
	return ret;
}

static struct class_attribute videosync_class_attrs[] = {
	__ATTR_RO(dump_queue_state),
	__ATTR_RO(dump_pts),
	__ATTR_RO(dump_get_put_framecount),
	__ATTR_RO(dump_rect),
	__ATTR_NULL
};
static struct class videosync_class = {
	.name = "videosync",
	.class_attrs = videosync_class_attrs,
};


int videosync_assign_map(char **receiver_name, int *inst)
{
	int i = 0;
	struct videosync_s *dev_s  = NULL;

	mutex_lock(&vp_dev->vp_mutex);
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s->inst == *inst) {
			*receiver_name = dev_s->vf_receiver_name;
			pr_info("videosync_assign_map %s %p\n",
				dev_s->vf_receiver_name, dev_s);
			mutex_unlock(&vp_dev->vp_mutex);
			return 0;
		}
	}
	mutex_unlock(&vp_dev->vp_mutex);
	return -ENODEV;
}

int videosync_alloc_map(int *inst)
{
	int i = 0;
	struct videosync_s *dev_s  = NULL;

	mutex_lock(&vp_dev->vp_mutex);
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s->inst >= 0  && (!dev_s->mapped)) {
			dev_s->mapped = true;
			*inst = dev_s->inst;
			pr_info("videosync_alloc_map %d OK\n", dev_s->inst);
			mutex_unlock(&vp_dev->vp_mutex);
			return 0;
		}
	}
	mutex_unlock(&vp_dev->vp_mutex);
	return -ENODEV;
}

void videosync_release_map(int inst)
{

	int i = 0;
	struct videosync_s *dev_s  = NULL;

	mutex_lock(&vp_dev->vp_mutex);
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s->inst == inst  && (dev_s->mapped)) {
			dev_s->mapped = false;
			pr_info("videosync_release_map %d OK\n", inst);
			break;
		}
	}
	mutex_unlock(&vp_dev->vp_mutex);
}
void videosync_release_map_force(struct videosync_priv_s *priv)
{
	int i = 0;
	struct videosync_s *dev_s  = NULL;

	mutex_lock(&vp_dev->vp_mutex);
	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &vp_dev->video_prov[i];
		if (dev_s->inst == priv->vp_id && (dev_s->mapped)) {
			dev_s->mapped = false;
			pr_info("videosync_release_map_force %d OK\n",
				priv->vp_id);
			break;
		}
	}
	mutex_unlock(&vp_dev->vp_mutex);
}

static int videosync_open(struct inode *inode, struct file *file)
{
	struct videosync_priv_s *priv = NULL;

	pr_info("videosync_open\n");
	priv = kzalloc(sizeof(struct videosync_priv_s), GFP_KERNEL);
	if (priv == NULL) {
		pr_err("kzalloc videosync_priv_s failed!");
	} else {
		priv->vp_id = -1;
		priv->dev_s = NULL;
		file->private_data = priv;
	}
	return 0;
}

static int videosync_release(struct inode *inode, struct file *file)
{
	struct videosync_priv_s *priv = NULL;

	pr_info("videosync_release\n");
	if (file && file->private_data) {
		priv = file->private_data;

		/*check if map released.*/
		videosync_release_map_force(priv);

		kfree(priv);
		priv = NULL;
		file->private_data = NULL;
	}

	return 0;
}

static int set_omx_pts(u32 *p)
{
	struct videosync_s *dev_s = NULL;
	int ret = 0;
	u32 tmp_pts = p[0];
	/*u32 vision = p[1];*/
	u32 set_from_hwc = p[2];
	u32 frame_num = p[3];
	u32 not_reset = p[4];
	u32 session = p[5];
	u32 dev_id = p[6];

	cur_omx_index = frame_num;

	if (dev_id < VIDEOSYNC_S_COUNT)
		dev_s = &vp_dev->video_prov[dev_id];

	if (dev_s != NULL && dev_s->mapped) {
		mutex_lock(&dev_s->omx_mutex);
		vp_print(dev_s->vf_receiver_name, PRINT_TIMESTAMP,
			"set omx_pts %u, hwc %d, not_reset %d, frame_num=%u\n",
			tmp_pts, set_from_hwc, not_reset, frame_num);

		if (dev_s->omx_check_previous_session) {
			if (session != dev_s->omx_cur_session) {
				dev_s->omx_cur_session = session;
				dev_s->omx_check_previous_session = false;
			} else {
				pr_info("videosync: tmp_pts %d, session=0x%x\n",
					tmp_pts, dev_s->omx_cur_session);
				mutex_unlock(&dev_s->omx_mutex);
				return ret;
			}
		}

		if (dev_s->omx_pts_set_index < frame_num)
			dev_s->omx_pts_set_index = frame_num;

		if (not_reset == 0)
			dev_s->omx_pts = tmp_pts;

		mutex_unlock(&dev_s->omx_mutex);

		ts_pcrscr_enable(dev_s, 1);
	} else {
		ret = -EFAULT;
		pr_err("[%s]cannot find dev_id %d device\n",
			dev_s->vf_receiver_name, dev_id);
	}

	return ret;

}
static int set_omx_zorder(u32 *p)
{
	struct videosync_s *dev_s = NULL;
	int ret = 0;
	u32 dev_id = p[5];
	/*32 tmp_pts = p[6];*/

	if (dev_id < VIDEOSYNC_S_COUNT)
		dev_s = &vp_dev->video_prov[dev_id];

	if (dev_s != NULL && dev_s->mapped) {
		dev_s->zorder = p[0];
		dev_s->rect.left = p[1];
		dev_s->rect.top = p[2];
		dev_s->rect.width = p[3];
		dev_s->rect.height = p[4];

		vp_print(dev_s->vf_receiver_name, PRINT_TIMESTAMP,
			"set zorder %d: %d %d %d %d\n",
			p[0], p[1], p[2], p[3], p[4]);

	} else {
		ret = -EFAULT;
		pr_err("[%s]cannot find dev_id %d device\n",
			dev_s->vf_receiver_name, dev_id);
	}

	return ret;

}

static long videosync_ioctl(struct file *file,
			    unsigned int cmd,
			    ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct videosync_priv_s *priv = NULL;

	switch (cmd) {
	case VIDEOSYNC_IOC_ALLOC_ID:{
		u32 videosync_id = 0;

		ret = videosync_alloc_map(&videosync_id);
		if (ret != 0)
			break;
		put_user(videosync_id, (u32 __user *)argp);
		pr_info("alloc-id %d\n", videosync_id);

		if (file && file->private_data) {
			priv = file->private_data;
			priv->vp_id = videosync_id;
			priv->dev_s = &vp_dev->video_prov[videosync_id];
			pr_info("get dev_s %p\n", priv->dev_s);
		}
	}
	break;
	case VIDEOSYNC_IOC_FREE_ID:{
		u32 videosync_id;

		get_user(videosync_id, (u32 __user *)argp);
		pr_info("free-id %d\n", videosync_id);
		videosync_release_map(videosync_id);
	}
	break;

	case VIDEOSYNC_IOC_SET_FREERUN_MODE:
		if (arg > FREERUN_DUR)
			ret = -EFAULT;
		else {
			if (file && file->private_data) {
				priv = file->private_data;
				if (priv && priv->dev_s)
					priv->dev_s->freerun_mode = arg;
				else
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}
		}
	break;

	case VIDEOSYNC_IOC_GET_FREERUN_MODE:
		if (file && file->private_data) {
			priv = file->private_data;
			if (priv && priv->dev_s)
				put_user(priv->dev_s->freerun_mode,
					(u32 __user *)argp);
			else
				ret = -EFAULT;

		} else
			ret = -EFAULT;

	break;
	case VIDEOSYNC_IOC_SET_OMX_VPTS:{
		u32 pts[7];

		if (copy_from_user(pts, argp, sizeof(pts)) == 0)
			ret = set_omx_pts(pts);

	}
	break;
	case VIDEOSYNC_IOC_SET_OMX_ZORDER:{
		u32 zorder[7];

		if (copy_from_user(zorder, argp, sizeof(zorder)) == 0)
			ret = set_omx_zorder(zorder);

	}
	break;
	case VIDEOSYNC_IOC_GET_OMX_VPTS:
		if (file && file->private_data) {
			priv = file->private_data;
			if (priv && priv->dev_s)
				put_user(priv->dev_s->omx_pts,
					(u32 __user *)argp);
			else
				ret = -EFAULT;
		} else {
			ret = -EFAULT;
		}
	break;
	case VIDEOSYNC_IOC_GET_OMX_VERSION:
		put_user(omx_version, (u32 __user *)argp);
		pr_info("get omx_version %d\n", omx_version);
	break;
	case VIDEOSYNC_IOC_SET_FIRST_FRAME_NOSYNC: {
		u32 info[5];
		struct videosync_s *dev_s = NULL;
		u32 dev_id;

		if (copy_from_user(info, argp, sizeof(info)) == 0) {
			dev_id = info[0];
			if (dev_id < VIDEOSYNC_S_COUNT)
				dev_s = &vp_dev->video_prov[dev_id];

			if (dev_s && dev_s->mapped) {
				dev_s->show_first_frame_nosync = info[1];
				pr_info("show_first_frame_nosync =%d\n",
					dev_s->show_first_frame_nosync);
			}
		}
	}
	break;
	case VIDEOSYNC_IOC_SET_VPAUSE: {
		u32 info[2];
		struct videosync_s *dev_s = NULL;
		u32 dev_id;

		if (copy_from_user(info, argp, sizeof(info)) == 0) {
			dev_id = info[0];
			if (dev_id < VIDEOSYNC_S_COUNT)
				dev_s = &vp_dev->video_prov[dev_id];

			if (dev_s && dev_s->mapped) {
				pr_info("set vpause:%d\n", info[1]);
				ts_pcrscr_enable(dev_s, !info[1]);
				if (!info[1]) {
					dev_s->video_started = 1;
					if (dev_s->first_frame_queued)
						ts_pcrscr_set(dev_s,
						dev_s->first_frame_vpts);
				} else {
					dev_s->video_started = 0;
				}
			}
		}
	}
	break;
	case VIDEOSYNC_IOC_SET_VMASTER: {
		u32 info[2];
		struct videosync_s *dev_s = NULL;
		u32 dev_id;

		if (copy_from_user(info, argp, sizeof(info)) == 0) {
			dev_id = info[0];
			if (dev_id < VIDEOSYNC_S_COUNT)
				dev_s = &vp_dev->video_prov[dev_id];

			if (dev_s && dev_s->mapped) {
				pr_info("set vmaster:%d\n", info[1]);
				dev_s->vmaster_mode = info[1];
			}
		}
	}
	break;
	case VIDEOSYNC_IOC_GET_VPTS: {
		u32 info[2];
		struct videosync_s *dev_s = NULL;
		u32 dev_id;

		if (copy_from_user(info, argp, sizeof(info)) == 0) {
			dev_id = info[0];
			if (dev_id < VIDEOSYNC_S_COUNT)
				dev_s = &vp_dev->video_prov[dev_id];

			if (dev_s && dev_s->mapped) {
				info[1] = dev_s->get_vpts;
				if (copy_to_user(argp, &info[0],
					sizeof(info)) != 0)
					ret = -EFAULT;
			} else
			ret = -EFAULT;
		}
	}
	break;
	case VIDEOSYNC_IOC_GET_PCRSCR: {
		u32 info[2];
		struct videosync_s *dev_s = NULL;
		u32 dev_id;

		if (copy_from_user(info, argp, sizeof(info)) == 0) {
			dev_id = info[0];
			if (dev_id < VIDEOSYNC_S_COUNT)
				dev_s = &vp_dev->video_prov[dev_id];

			if (dev_s && dev_s->mapped) {
				info[1] = ts_pcrscr_get(dev_s);
				if (copy_to_user(argp, &info[0],
					sizeof(info)) != 0)
					ret = -EFAULT;
			} else
			ret = -EFAULT;
		}
	}
	break;
	default:
		pr_info("ioctl invalid cmd 0x%x\n", cmd);
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long videosync_compat_ioctl(struct file *file,
				unsigned int cmd,
				ulong arg)
{
	long ret = 0;

	ret = videosync_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static void clear_queued_queue(struct videosync_s *dev_s)
{
	struct vframe_s *vf = NULL;

	vp_print(dev_s->vf_receiver_name, PRINT_QUEUE_STATUS,
		"clear_queued_queue: size %d!\n", vfq_level(&dev_s->queued_q));

	vf = vfq_pop(&dev_s->queued_q);
	while (vf) {
		vf_put(vf, dev_s->vf_receiver_name);
		vf = vfq_pop(&dev_s->queued_q);
	}

}

static void clear_ready_queue(struct videosync_s *dev_s)
{
	struct vframe_s *vf = NULL;

	vp_print(dev_s->vf_receiver_name, PRINT_QUEUE_STATUS,
		"clear_ready_queue: size %d!\n",
		vfq_level(&dev_s->ready_q));
	vf = vfq_pop(&dev_s->ready_q);
	while (vf) {
		vf_put(vf, dev_s->vf_receiver_name);
		vf = vfq_pop(&dev_s->ready_q);
	}
}

static u64 func_div(u64 number, u32 divid)
{
	u64 tmp = number;

	do_div(tmp, divid);
	return tmp;
}

static inline bool omx_vpts_expire(struct vframe_s *cur_vf,
			       struct vframe_s *next_vf,
			       struct videosync_s *dev_s,
			       int toggled_cnt)

{
	u32 pts;
#ifdef VIDEO_PTS_CHASE
	u32 vid_pts, scr_pts;
#endif
	u32 systime;
	u32 adjust_pts, org_vpts;
	unsigned long delta = 0;
	int delta_32 = 0;
	/*u32 dur_pts = 0;*/
	bool expired;

	if (!next_vf)
		return false;

	pts = next_vf->pts;

	if (dev_s->freerun_mode == FREERUN_NODUR)
		return true;

	if (next_vf->duration == 0)
		return true;

	if (dev_s->show_first_frame_nosync
		|| show_first_frame_nosync) {
		if (next_vf->omx_index == 0)
			return true;
	}
	systime = ts_pcrscr_get(dev_s);

	if (no_render)
		dev_s->first_frame_toggled = 1; /*just for debug, not render*/

	vp_print(dev_s->vf_receiver_name, PRINT_TIMESTAMP,
		"sys_time=%u, vf->pts=%u, diff=%d, index=%d\n",
		systime, pts, (int)(systime - pts), next_vf->omx_index);

	if (0) {
		pts =
		    timestamp_vpts_get() +
		    (cur_vf ? DUR2PTS(cur_vf->duration) : 0);
	}
	/* check video PTS discontinuity */
	else if (ts_pcrscr_enable_state(dev_s) > 0 &&
		 (enable_video_discontinue_report) &&
		 (dev_s->first_frame_toggled) &&
		 (abs(systime - pts) > tsync_vpts_discontinuity_margin()) &&
		 ((next_vf->flag & VFRAME_FLAG_NO_DISCONTINUE) == 0
		 || tsync_vpts_discontinuity_margin() <= 90000)) {

		vp_print(dev_s->vf_receiver_name, PRINT_TIMESTAMP,
			"discontinue, systime = %u, next_vf->pts = %u\n",
			systime, next_vf->pts);
		return true;
	} else if ((!dev_s->vmaster_mode)
			&& (dev_s->omx_pts + omx_pts_interval_upper
			< next_vf->pts)
			&& (dev_s->omx_pts_set_index >= next_vf->omx_index)) {
		pr_info("videosync, omx_pts=%d omx_pts_set_index=%d pts=%d omx_index=%d pcr=%d\n",
					dev_s->omx_pts,
					dev_s->omx_pts_set_index,
					next_vf->pts,
					next_vf->omx_index,
					ts_pcrscr_get(dev_s));
		return true;
	}

	if (0/*smooth_sync_enable*/) {
		org_vpts = timestamp_vpts_get();
		if ((abs(org_vpts + vsync_pts_inc - systime) <
			M_PTS_SMOOTH_MAX)
		    && (abs(org_vpts + vsync_pts_inc - systime) >
			M_PTS_SMOOTH_MIN)) {

			if (!dev_s->video_frame_repeat_count) {
				dev_s->vpts_ref = org_vpts;
				dev_s->video_frame_repeat_count++;
			}

			if ((int)(org_vpts + vsync_pts_inc - systime) > 0) {
				adjust_pts =
				    dev_s->vpts_ref + (vsync_pts_inc -
						M_PTS_SMOOTH_ADJUST) *
				    dev_s->video_frame_repeat_count;
			} else {
				adjust_pts =
				    dev_s->vpts_ref + (vsync_pts_inc +
						M_PTS_SMOOTH_ADJUST) *
				    dev_s->video_frame_repeat_count;
			}

			return (int)(adjust_pts - pts) >= 0;
		}

		if (dev_s->video_frame_repeat_count) {
			dev_s->vpts_ref = 0;
			dev_s->video_frame_repeat_count = 0;
		}
	}
	delta = func_div(sched_clock() - dev_s->time_update, 1000);
	delta_32 = delta * 90 / 1000;
	if (delta_32 > max_delata_time)
		max_delata_time = delta_32;

	expired = (int)(systime + vsync_pts_align - pts) >= 0;

	vp_print(dev_s->vf_receiver_name, PRINT_PATTERN,
		 "expired=%d, valid=%d, next_pts=%d, cnt=%d, systime=%d, inc=%d\n",
		 expired,
		 next_vf->next_vf_pts_valid,
		 next_vf->pts,
		 toggled_cnt,
		 systime,
		 vsync_pts_inc);

	if (expired && next_vf->next_vf_pts_valid &&
	    pts_enforce_pulldown &&
	    next_vf->next_vf_pts &&
	    (toggled_cnt > 0) &&
	    ((int)(systime + vsync_pts_inc +
	    vsync_pts_align - next_vf->next_vf_pts) < 0)) {
		expired = false;
		vp_print(dev_s->vf_receiver_name, PRINT_PATTERN,
			 "force expired false\n");
	} else if (!expired && next_vf->next_vf_pts_valid &&
		pts_enforce_pulldown &&
		next_vf->next_vf_pts &&
		(toggled_cnt == 0) &&
		((int)(systime + vsync_pts_inc +
		vsync_pts_align - next_vf->next_vf_pts) >= 0)) {
		expired = true;
		vp_print(dev_s->vf_receiver_name, PRINT_PATTERN,
			 "force expired true\n");
	}

	if (pts_enforce_pulldown)
		vpts_perform_pulldown(dev_s, next_vf, &expired);
	return expired;

}

void videosync_sync(struct videosync_s *dev_s)
{
	int ready_q_size = 0;
	struct vframe_s *vf;
	int expire_count = 0;

	if (smooth_sync_enable) {
		if (dev_s->video_frame_repeat_count)
			dev_s->video_frame_repeat_count++;
	}

	vf = vfq_peek(&dev_s->queued_q);

	while (vf) {
		if (omx_vpts_expire(dev_s->cur_dispbuf, vf, dev_s, expire_count)
			|| show_nosync) {

			vf = vfq_pop(&dev_s->queued_q);
			if (vf) {
				if (async_mode)	{
					if (vfq_level(&dev_s->ready_q) > 0)
						clear_ready_queue(dev_s);
				}

				if (pts_escape_vsync == 1) {
					pts_trace++;
					pts_escape_vsync = 0;
				}
				vsync_video_pattern();
				pre_pts_trace = pts_trace;
				pts_trace = 0;
				expire_count++;

				vfq_push(&dev_s->ready_q, vf);
				ready_q_size = vfq_level(&dev_s->ready_q);
				vp_print(dev_s->vf_receiver_name,
					PRINT_QUEUE_STATUS,
					"add pts %u index 0x%x to ready_q, size %d\n",
					vf->pts, vf->index, ready_q_size);

				vf_notify_receiver(
					dev_s->vf_provider_name,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
				if (ready_q_size > VIDEOSYNC_S_POOL_SIZE - 1) {
					vp_print(dev_s->vf_receiver_name,
						PRINT_QUEUE_STATUS,
						"ready_q full!\n");
					break;
				}
			}

			vf = vfq_peek(&dev_s->queued_q);
			if (!vf)
				break;
		} else {
			break;
		}
	}

}


static void prepare_queued_queue(struct videosync_dev *dev)
{
	int i = 0;
	struct videosync_s *dev_s;
	struct vframe_s *vf;

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &dev->video_prov[i];
		if (dev_s->active_state == VIDEOSYNC_INACTIVE)
			continue;

		if (vfq_full(&dev_s->queued_q)) {
			pr_info("[%s]queued_q full!\n",
				dev_s->vf_receiver_name);
			continue;
		}

		while (vf_peek(dev_s->vf_receiver_name)) {
			vf = vf_get(dev_s->vf_receiver_name);
			if (vf) {
				if (!dev_s->first_frame_queued) {
					dev_s->first_frame_queued = 1;
					dev_s->first_frame_vpts = vf->pts;
					if (dev_s->video_started) {
						ts_pcrscr_set(dev_s,
						dev_s->first_frame_vpts);
					}
				}
				vfq_push(&dev_s->queued_q, vf);
				dev_s->get_frame_count++;
				vp_print(dev_s->vf_receiver_name,
					PRINT_QUEUE_STATUS,
					"add pts %u index 0x%x to queued_q, size %d\n",
					vf->pts,
					vf->index,
					vfq_level(&dev_s->queued_q));
			}
		}
	}

}
static void prepare_ready_queue(struct videosync_dev *dev)
{
	int i = 0;
	struct videosync_s *dev_s;

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &dev->video_prov[i];
		if (dev_s->active_state == VIDEOSYNC_INACTIVE)
			continue;

		if (vfq_full(&dev_s->ready_q)) {
			pr_info("[%s]ready_q full!\n", dev_s->vf_receiver_name);
			continue;
		}

		videosync_sync(dev_s);

		if (no_render)
			clear_ready_queue(dev_s);/*just for debug, not render*/

	}

}

static void reinit_dev_s(struct videosync_s *dev_s)
{
	dev_s->get_frame_count = 0;
	dev_s->put_frame_count = 0;
	dev_s->system_time_up = 0;
	dev_s->system_time = 0;
	dev_s->first_frame_toggled = 0;
	dev_s->freerun_mode = 0;
	dev_s->system_time_scale_remainder = 0;
	dev_s->omx_pts = 0;
	dev_s->vpts_ref = 0;
	dev_s->video_frame_repeat_count = 0;
	dev_s->rect.top = 0;
	dev_s->rect.left = 0;
	dev_s->rect.width = 0;
	dev_s->rect.height = 0;
	dev_s->zorder = 0;
}
static int videosync_receiver_event_fun(int type, void *data,
	void *private_data)
{
	struct videosync_s *dev_s = (struct videosync_s *)private_data;
	unsigned long flags = 0;
	struct videosync_dev *dev;
	unsigned long time_left;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		pr_info("videosync: try to unreg %p, %s\n",
			dev_s, dev_s->vf_receiver_name);
		dev = (struct videosync_dev *)dev_s->dev;

		spin_lock_irqsave(&dev->dev_s_num_slock, flags);
		--dev->active_dev_s_num;
		spin_unlock_irqrestore(&dev->dev_s_num_slock, flags);

		videosync_unregister(dev_s);
		dev_s->receiver_register = false;

		if (dev_s->active_state == VIDEOSYNC_ACTIVE) {
			dev_s->active_state = VIDEOSYNC_INACTIVE_REQ;
			dev->wakeup = 1;
			wake_up_interruptible(&dev->videosync_wait);
			time_left = wait_for_completion_timeout(
				&dev_s->inactive_done,
				msecs_to_jiffies(100));
			if (time_left == 0)
				vp_print(RECEIVER_NAME, PRINT_OTHER,
					"videosync: unreg timeout\n");
		}
		clear_ready_queue(dev_s);
		clear_queued_queue(dev_s);

		/*tsync_avevent(VIDEO_STOP, 0);*/
		pr_info("videosync: unreg %p, %s\n",
			dev_s, dev_s->vf_receiver_name);
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		//omx_secret_mode = false;
		struct videosync_dev *dev = (struct videosync_dev *)dev_s->dev;

		reinit_dev_s(dev_s);
		//videosync_register(dev_s);
		dev_s->receiver_register = true;
		dev_s->active_state = VIDEOSYNC_ACTIVE;
		dev_s->omx_check_previous_session = true;
		dev_s->omx_pts_set_index = 0;
		dev_s->mapped = true;

		spin_lock_irqsave(&dev->dev_s_num_slock, flags);
		++dev->active_dev_s_num;
		spin_unlock_irqrestore(&dev->dev_s_num_slock, flags);

		vfq_init(&dev_s->queued_q, VIDEOSYNC_S_POOL_SIZE + 1,
				&dev_s->videosync_pool_queued[0]);
		vfq_init(&dev_s->ready_q, VIDEOSYNC_S_POOL_SIZE + 1,
				&dev_s->videosync_pool_ready[0]);

		init_completion(&dev_s->inactive_done);
		complete(&dev->thread_active);
		pr_info("videosync: reg %p, %s\n",
			dev_s, dev_s->vf_receiver_name);
		pts_trace = 0;
		pts_pattern_detected = -1;
		pre_pts_trace = 0;
		pts_escape_vsync = 0;
		dev_s->first_frame_queued = 0;
		dev_s->first_frame_vpts = 0;
		dev_s->vmaster_mode = 0;
		dev_s->video_started = 0;
	} else if (type == VFRAME_EVENT_PROVIDER_VFRAME_READY) {

	} else if (type == VFRAME_EVENT_PROVIDER_START) {
		videosync_register(dev_s);
		pr_info("videosync: start %p, %s\n",
			dev_s, dev_s->vf_receiver_name);
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		struct videosync_buffer_states states;

		videosync_buffer_states(&states, dev_s);
		if (states.buf_queued_num + states.buf_ready_num > 0)
			return RECEIVER_ACTIVE;
		else {
			vp_print(dev_s->vf_receiver_name, 0,
				"buf queue empty!!\n");
			if (vf_notify_receiver(
				dev_s->vf_provider_name,
				VFRAME_EVENT_PROVIDER_QUREY_STATE,
				NULL) == RECEIVER_ACTIVE)
				return RECEIVER_ACTIVE;
			return RECEIVER_INACTIVE;
		}
	}
	return 0;
}

static const struct file_operations videosync_fops = {

	.owner = THIS_MODULE,
	.open = videosync_open,
	.release = videosync_release,
	.unlocked_ioctl = videosync_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = videosync_compat_ioctl,
#endif
	.poll = NULL,
};

static const struct vframe_receiver_op_s videosync_vf_receiver = {
	.event_cb = videosync_receiver_event_fun,
};

static const struct videosync_operations_s videosync_vf_provider = {
	.peek = videosync_vf_peek,
	.get = videosync_vf_get,
	.put = videosync_vf_put,
	.event_cb = videosync_event_cb,
	.buffer_states = videosync_buffer_states,
};

static int __init videosync_create_instance(int inst)
{
	struct videosync_s *dev_s;

	dev_s = &vp_dev->video_prov[inst];
	dev_s->dev = vp_dev;
	dev_s->fd_num = 0;
	dev_s->ops = &videosync_vf_provider;
	dev_s->active_state = VIDEOSYNC_INACTIVE;
	dev_s->omx_cur_session = 0xffffffff;
	pr_info("videosync_create_instance dev_s %p,dev_s->dev %p\n",
		dev_s, dev_s->dev);

	/* initialize locks */
	mutex_init(&dev_s->omx_mutex);
	spin_lock_init(&dev_s->timestamp_lock);

	dev_s->inst = inst;
	dev_s->index = inst;
	dev_s->mapped = true;
	snprintf(dev_s->vf_receiver_name, VIDEOSYNC_S_VF_RECEIVER_NAME_SIZE,
		RECEIVER_NAME ".%x", inst & 0xff);
	snprintf(dev_s->vf_provider_name, VIDEOSYNC_S_VF_RECEIVER_NAME_SIZE,
		PROVIDER_NAME ".%x", inst & 0xff);

	pr_info("videosync_create_instance reg %s\n",
		dev_s->vf_receiver_name);

	vf_receiver_init(&dev_s->vp_vf_receiver,
			dev_s->vf_receiver_name,
			&videosync_vf_receiver, dev_s);

	vf_reg_receiver(&dev_s->vp_vf_receiver);

	return 0;

}

static void __init videosync_destroy_instance(int inst)
{
	pr_info("videosync_destroy_instance %s\n",
		vp_dev->video_prov[inst].vf_receiver_name);
	vf_unreg_receiver(&vp_dev->video_prov[inst].vp_vf_receiver);
}


static void videosync_thread_tick(struct videosync_dev *dev)
{
	int i = 0;
	struct videosync_s *dev_s;
	unsigned long flags = 0;
	unsigned long time_left;

	if (!dev)
		return;

	wait_event_interruptible_timeout(dev->videosync_wait, dev->wakeup,
					 msecs_to_jiffies(500));
	dev->wakeup = 0;

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		dev_s = &dev->video_prov[i];
		if (dev_s->active_state == VIDEOSYNC_INACTIVE_REQ) {
			dev_s->active_state = VIDEOSYNC_INACTIVE;
			complete(&dev_s->inactive_done);
		}
	}

	vp_print(RECEIVER_NAME, PRINT_OTHER,
		"active num %d\n", dev->active_dev_s_num);

	spin_lock_irqsave(&dev->dev_s_num_slock, flags);
	if (dev->active_dev_s_num > 0) {
		spin_unlock_irqrestore(&dev->dev_s_num_slock, flags);
		prepare_queued_queue(dev);
		prepare_ready_queue(dev);
	} else {
		spin_unlock_irqrestore(&dev->dev_s_num_slock, flags);
		vp_print(RECEIVER_NAME, PRINT_OTHER,
			"videosync: no active dev, thread go to sleep\n");
		time_left = wait_for_completion_timeout(&dev->thread_active,
			msecs_to_jiffies(500));
		if (time_left == 0)
			vp_print(RECEIVER_NAME, PRINT_OTHER,
				"videosync: thread_tick timeout\n");
	}
}

static void videosync_sleep(struct videosync_dev *dev)
{
	videosync_thread_tick(dev);
	try_to_freeze();
}

static int videosync_thread(void *data)
{
	struct videosync_dev *dev = (struct videosync_dev *)data;

	pr_info("videosync_thread started\n");

	set_freezable();

	while (!kthread_should_stop())
		videosync_sleep(dev);

	pr_info("videosync_thread exit\n");
	return 0;
}


static int __init videosync_init(void)
{
	int ret = -1, i;
	struct device *devp;

	ret = class_register(&videosync_class);
	if (ret < 0)
		return ret;
	ret = register_chrdev(VIDEOSYNC_MAJOR, "videosync", &videosync_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for videosync device\n");
		goto error1;
	}

	devp = device_create(&videosync_class,
				NULL,
				MKDEV(VIDEOSYNC_MAJOR, 0),
				NULL,
				VIDEOSYNC_DEVICE_NAME);
	if (IS_ERR(devp)) {
		pr_err("failed to create videosync device node\n");
		ret = PTR_ERR(devp);
		return ret;
	}

	vp_dev = kzalloc(sizeof(*vp_dev), GFP_KERNEL);
	if (!vp_dev)
		return -ENOMEM;

	vp_dev->video_prov = kzalloc(
		VIDEOSYNC_S_COUNT * sizeof(struct videosync_s),
		GFP_KERNEL);
	if (!vp_dev->video_prov)
		return -ENOMEM;

	mutex_init(&vp_dev->vp_mutex);
	spin_lock_init(&vp_dev->dev_s_num_slock);

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++) {
		ret = videosync_create_instance(i);
		if (ret) {
			pr_err("videosync: error %d while create instance\n",
				ret);
			goto error1;
		}
	}

	init_completion(&vp_dev->thread_active);
	vp_dev->wakeup = 0;
	init_waitqueue_head(&vp_dev->videosync_wait);
	vp_dev->kthread = kthread_run(videosync_thread, vp_dev, "videosync");
	videosync_inited = true;
	return ret;

error1:
	kfree(vp_dev);
	vp_dev = NULL;
	unregister_chrdev(VIDEOSYNC_MAJOR, "videosync");
	class_unregister(&videosync_class);
	return ret;
}

static void __exit videosync_exit(void)
{
	int i, ret;

	complete(&vp_dev->thread_active);

	if (vp_dev->kthread) {
		ret = kthread_stop(vp_dev->kthread);
		if (ret < 0)
			pr_info("%s, kthread_stop return %d.\n", __func__, ret);
		vp_dev->kthread = NULL;
	}
	videosync_inited = false;

	for (i = 0; i < VIDEOSYNC_S_COUNT; i++)
		videosync_destroy_instance(i);

	kfree(vp_dev->video_prov);
	vp_dev->video_prov = NULL;

	kfree(vp_dev);
	vp_dev = NULL;

	device_destroy(&videosync_class, MKDEV(VIDEOSYNC_MAJOR, 0));
	unregister_chrdev(VIDEOSYNC_MAJOR, VIDEOSYNC_DEVICE_NAME);
	class_unregister(&videosync_class);
}

module_init(videosync_init);
module_exit(videosync_exit);

MODULE_PARM_DESC(smooth_sync_enable, "\n smooth_sync_enable\n");
module_param(smooth_sync_enable, bool, 0664);

MODULE_PARM_DESC(vp_debug_flag, "\n vp_debug_flag\n");
module_param(vp_debug_flag, uint, 0664);

MODULE_PARM_DESC(show_nosync, "\n show_nosync\n");
module_param(show_nosync, bool, 0664);

MODULE_PARM_DESC(no_render, "\n no_render\n");
module_param(no_render, bool, 0664);

MODULE_PARM_DESC(async_mode, "\n async_mode\n");
module_param(async_mode, bool, 0664);

MODULE_PARM_DESC(vsync_pts_align, "\n vsync_pts_align\n");
module_param(vsync_pts_align, int, 0664);

MODULE_PARM_DESC(cur_omx_index, "\n cur_omx_index\n");
module_param(cur_omx_index, uint, 0664);

MODULE_PARM_DESC(show_first_frame_nosync, "\n show_first_frame_nosync\n");
module_param(show_first_frame_nosync, uint, 0664);

MODULE_PARM_DESC(max_delata_time, "\n max_delata_time\n");
module_param(max_delata_time, uint, 0664);

MODULE_PARM_DESC(duration_gcd, "\n duration_gcd\n");
module_param(duration_gcd, uint, 0664);

module_param(pts_enforce_pulldown, bool, 0644);
MODULE_PARM_DESC(pts_enforce_pulldown, "enforce video frame pulldown if needed");
