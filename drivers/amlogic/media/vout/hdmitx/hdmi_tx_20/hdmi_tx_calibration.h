/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdmi_tx_calibration.h
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

#ifndef __HDMITX_CALIBRATION_H__
#define __HDMITX_CALIBRATION_H__

#include <linux/amlogic/media/vout/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ddc.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>

void cedst_free_buf(struct hdmitx_dev *hdev);
void cedst_malloc_buf(struct hdmitx_dev *hdev);
void cedst_store_buf(struct hdmitx_dev *hdev);
int cedst_phy_evaluation(struct hdmitx_dev *hdev);

#endif

