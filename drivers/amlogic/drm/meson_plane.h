/*
 * drivers/amlogic/drm/meson_plane.h
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

#ifndef __MESON_PLANE_H
#define __MESON_PLANE_H

#include <linux/kernel.h>
#include <drm/drmP.h>
#include <drm/drm_plane.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/meson_uvm_core.h>
#include "meson_fb.h"

/*legacy video driver issue caused zorder problem.*/
#define OSD_PLANE_BEGIN_ZORDER	65
#define OSD_PLANE_END_ZORDER		128

struct am_meson_plane_state {
	struct drm_plane_state base;
	u32 premult_en;
};

enum meson_plane_type {
	OSD_PLANE = 0,
	VIDEO_PLANE,
};

struct am_osd_plane {
	/*base struct, same as am_video_plane*/
	struct drm_plane base; //must be first element.
	struct meson_drm *drv; //point to struct parent.
	struct dentry *plane_debugfs_dir;
	int plane_index;
	int plane_type;

	/*osd extend*/
	u32 osd_reverse;
	u32 osd_blend_bypass;
	struct drm_property *prop_premult_en;
};

struct am_video_plane {
	/*base struct, same as am_video_plane*/
	struct drm_plane base; //must be first element.
	struct meson_drm *drv; //point to struct parent.
	struct dentry *plane_debugfs_dir;
	int plane_index;
	int plane_type;

	/*video exted*/
};

#define to_am_osd_plane(x) container_of(x, \
	struct am_osd_plane, base)
#define to_am_meson_plane_state(x) container_of(x, \
	struct am_meson_plane_state, base)
#define to_am_video_plane(x) container_of(x, \
	struct am_video_plane, base)

int am_meson_plane_create(struct meson_drm *priv);

#endif
