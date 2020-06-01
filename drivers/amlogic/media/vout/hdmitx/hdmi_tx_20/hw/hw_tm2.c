/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hw_tm2.c
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

#include <linux/printk.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "common.h"
#include "mach_reg.h"

void hdmitx_phy_bandgap_en_tm2(void)
{
	unsigned int val = 0;

	val = hd_read_reg(P_TM2_HHI_HDMI_PHY_CNTL0);
	if (val == 0)
		hd_write_reg(P_TM2_HHI_HDMI_PHY_CNTL0, 0x0b4242);
}

void set_phy_by_mode_tm2(unsigned int mode)
{
	switch (mode) {
	case HDMI_PHYPARA_6G: /* 5.94/4.5/3.7Gbps */
	case HDMI_PHYPARA_4p5G:
	case HDMI_PHYPARA_3p7G:
		hd_write_reg(P_TM2_HHI_HDMI_PHY_CNTL0, 0x33EB65c4);
		hd_write_reg(P_TM2_HHI_HDMI_PHY_CNTL3, 0x2ab0ff3b);
		hd_write_reg(P_TM2_HHI_HDMI_PHY_CNTL5, 0x0000080b);
		break;
	case HDMI_PHYPARA_3G: /* 2.97Gbps */
		hd_write_reg(P_TM2_HHI_HDMI_PHY_CNTL0, 0x33eb42a5);
		hd_write_reg(P_TM2_HHI_HDMI_PHY_CNTL3, 0x2ab0ff3b);
		hd_write_reg(P_TM2_HHI_HDMI_PHY_CNTL5, 0x00000003);
		break;
	case HDMI_PHYPARA_270M: /* 1.485Gbps, and below */
	case HDMI_PHYPARA_DEF:
	default:
		hd_write_reg(P_TM2_HHI_HDMI_PHY_CNTL0, 0x33eb4262);
		hd_write_reg(P_TM2_HHI_HDMI_PHY_CNTL3, 0x2ab0ff3b);
		hd_write_reg(P_TM2_HHI_HDMI_PHY_CNTL5, 0x00000003);
		break;
	}
}

