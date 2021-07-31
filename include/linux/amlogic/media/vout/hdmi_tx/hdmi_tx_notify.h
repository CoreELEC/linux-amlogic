/*
 * include/linux/amlogic/media/vout/hdmi_tx/hdmi_tx_notify.h
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

#ifndef __HDMI_NOTIFY_H__
#define __HDMI_NOTIFY_H__

#include <linux/notifier.h>

/* for HDMIRX/CEC notify */
#define HDMITX_PLUG			1
#define HDMITX_UNPLUG			2
#define HDMITX_PHY_ADDR_VALID		3
#define HDMITX_HDCP14_KSVLIST		4

#ifdef CONFIG_AMLOGIC_HDMITX
void hdmitx_event_notify(unsigned long state, void *arg);
int hdmitx_event_notifier_regist(struct notifier_block *nb);
int hdmitx_event_notifier_unregist(struct notifier_block *nb);
#endif
#endif
