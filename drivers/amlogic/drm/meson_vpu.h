/*
 * drivers/amlogic/drm/meson_vpu.h
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

#ifndef __AM_MESON_VPU_H
#define __AM_MESON_VPU_H

#include <linux/amlogic/media/vout/vout_notify.h>

#define VIDEO_LATENCY_VSYNC 2

struct am_vout_mode {
	char name[DRM_DISPLAY_MODE_LEN];
	enum vmode_e mode;
	int width, height, vrefresh;
	unsigned int flags;
};

extern struct am_meson_logo logo;
char *am_meson_crtc_get_voutmode(struct drm_display_mode *mode);
void am_meson_free_logo_memory(void);
#ifdef CONFIG_DRM_MESON_HDMI
char *am_meson_hdmi_get_voutmode(struct drm_display_mode *mode);
#endif
#ifdef CONFIG_DRM_MESON_CVBS
char *am_cvbs_get_voutmode(struct drm_display_mode *mode);
#endif

#endif /* __AM_MESON_VPU_H */
