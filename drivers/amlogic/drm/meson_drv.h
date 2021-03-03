/*
 * drivers/amlogic/drm/meson_drv.h
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

#ifndef __AM_MESON_DRV_H
#define __AM_MESON_DRV_H

#include <linux/platform_device.h>
#include <linux/of.h>
#include <drm/drmP.h>
#ifdef CONFIG_DRM_MESON_USE_ION
#include <ion/ion_priv.h>
#endif

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vfm_ext.h>

#define MESON_MAX_CRTC		2
#define MESON_MAX_OSD		4
#define MESON_MAX_VIDEO		2

/*
 * Amlogic drm private crtc funcs.
 * @loader_protect: protect loader logo crtc's power
 * @enable_vblank: enable crtc vblank irq.
 * @disable_vblank: disable crtc vblank irq.
 */
struct meson_crtc_funcs {
	int (*loader_protect)(struct drm_crtc *crtc, bool on);
	int (*enable_vblank)(struct drm_crtc *crtc);
	void (*disable_vblank)(struct drm_crtc *crtc);
};

struct meson_drm_fbdev_config {
	u32 ui_w;
	u32 ui_h;
	u32 fb_w;
	u32 fb_h;
	u32 fb_bpp;
};

struct meson_drm {
	struct device *dev;

	struct drm_device *drm;
	struct drm_crtc *crtc;
	const struct meson_crtc_funcs *crtc_funcs[MESON_MAX_CRTC];
	struct drm_fbdev_cma *fbdev;
	struct drm_fb_helper *fbdev_helper;
	struct drm_gem_object *fbdev_bo;
	struct drm_plane *primary_plane;
	struct drm_plane *cursor_plane;
	struct drm_property_blob *gamma_lut_blob;

#ifdef CONFIG_DRM_MESON_USE_ION
	struct ion_client *gem_client;
#endif

	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_funcs *funcs;
	struct am_meson_logo *logo;
	struct drm_atomic_state *state;

	u32 num_crtcs;
	struct am_meson_crtc *crtcs[MESON_MAX_CRTC];

	u32 num_planes;
	struct am_osd_plane *osd_planes[MESON_MAX_OSD];
	struct am_video_plane *video_planes[MESON_MAX_VIDEO];
	struct meson_drm_fbdev_config ui_config;
};

static inline int meson_vpu_is_compatible(struct meson_drm *priv,
					  const char *compat)
{
	return of_device_is_compatible(priv->dev->of_node, compat);
}

int am_meson_register_crtc_funcs(struct drm_crtc *crtc,
				 const struct meson_crtc_funcs *crtc_funcs);
void am_meson_unregister_crtc_funcs(struct drm_crtc *crtc);
struct drm_connector *am_meson_hdmi_connector(void);

#ifdef CONFIG_DEBUG_FS
int meson_debugfs_init(struct drm_minor *minor);
void meson_debugfs_cleanup(struct drm_minor *minor);
#endif
int __am_meson_drm_set_config(struct drm_mode_set *set,
			      struct drm_atomic_state *state);
#endif /* __AM_MESON_DRV_H */
