/*
 * drivers/amlogic/drm/meson_cvbs.h
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

#ifndef __MESON_CVBS_H_
#define __MESON_CVBS_H_

#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include <drm/drm_modes.h>

#include <linux/amlogic/media/vout/vinfo.h>

#include "meson_drv.h"

struct am_drm_cvbs_s {
	struct drm_device *drm;
	struct drm_connector connector;
	struct drm_encoder encoder;
};

#endif
