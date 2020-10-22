/*
 * drivers/amlogic/drm/meson_hdcp.h
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

#ifndef __AM_MESON_HDCP_H
#define __AM_MESON_HDCP_H

#define HDCP_SLAVE     0x3a
#define HDCP2_VERSION  0x50
#define HDCP_MODE14    1
#define HDCP_MODE22    2
#define HDCP_NULL      0

enum {
	HDCP_TX22_DISCONNECT,
	HDCP_TX22_START,
	HDCP_TX22_STOP
};

int am_hdcp_init(struct am_hdmi_tx *am_hdmi);
int get_hdcp_hdmitx_version(struct am_hdmi_tx *am_hdmi);
int am_hdcp_disable(struct am_hdmi_tx *am_hdmi);
int am_hdcp_disconnect(struct am_hdmi_tx *am_hdmi);
void am_hdcp_enable(struct work_struct *work);
int am_hdcp22_init(struct am_hdmi_tx *am_hdmi);
void drm_hdcp14_on(ulong param);
void drm_hdcp14_off(ulong param);
void drm_hdcp22_init(void);
#endif
