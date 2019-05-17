/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hw_g12a.c
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

/*
 * NAME		PAD		PINMUX		GPIO
 * HPD		GPIOH_1		reg0[23]	GPIO1[21]
 * SCL		GPIOH_2		reg0[22]	GPIO1[22[
 * SDA		GPIOH_3		reg0[21]	GPIO1[23]
 */

#ifdef P_HHI_HDMI_PLL_CNTL
#undef P_HHI_HDMI_PLL_CNTL
#endif
#ifdef P_HHI_HDMI_PLL_CNTL2
#undef P_HHI_HDMI_PLL_CNTL2
#endif
#ifdef P_HHI_HDMI_PLL_CNTL3
#undef P_HHI_HDMI_PLL_CNTL3
#endif
#ifdef P_HHI_HDMI_PLL_CNTL4
#undef P_HHI_HDMI_PLL_CNTL4
#endif
#ifdef P_HHI_HDMI_PLL_CNTL5
#undef P_HHI_HDMI_PLL_CNTL5
#endif
#ifdef P_HHI_HDMI_PLL_CNTL6
#undef P_HHI_HDMI_PLL_CNTL6
#endif
#ifdef HHI_HDMI_PLL_CNTL_I
#undef HHI_HDMI_PLL_CNTL_I
#endif
#ifdef P_HHI_HDMI_PLL_CNTL_I
#undef P_HHI_HDMI_PLL_CNTL_I
#endif
#ifdef WAIT_FOR_PLL_LOCKED
#undef WAIT_FOR_PLL_LOCKED
#endif

#define WAIT_FOR_PLL_LOCKED(reg) \
	do { \
		unsigned int st = 0; \
		int cnt = 10; \
		while (cnt--) { \
			udelay(50); \
			st = (((hd_read_reg(reg) >> 30) & 0x3) == 3); \
			if (st) \
				break; \
			else { \
				/* reset hpll */ \
				hd_set_reg_bits(reg, 1, 29, 1); \
				hd_set_reg_bits(reg, 0, 29, 1); \
			} \
		} \
		if (cnt < 9) \
			pr_info("pll[0x%x] reset %d times\n", reg, 9 - cnt);\
	} while (0)

#define P_HHI_HDMI_PLL_CNTL HHI_REG_ADDR(0xc8)
#define P_HHI_HDMI_PLL_CNTL0 P_HHI_HDMI_PLL_CNTL
#define P_HHI_HDMI_PLL_CNTL1 HHI_REG_ADDR(0xc9)
#define P_HHI_HDMI_PLL_CNTL2 HHI_REG_ADDR(0xca)
#define P_HHI_HDMI_PLL_CNTL3 HHI_REG_ADDR(0xcb)
#define P_HHI_HDMI_PLL_CNTL4 HHI_REG_ADDR(0xcc)
#define P_HHI_HDMI_PLL_CNTL5 HHI_REG_ADDR(0xcd)
#define P_HHI_HDMI_PLL_CNTL6 HHI_REG_ADDR(0xce)
#define P_HHI_HDMI_PLL_STS HHI_REG_ADDR(0xcf)

/*
 * When VCO outputs 6.0 GHz, if VCO unlock with default v1
 * steps, then need reset with v2 or v3
 */
static bool set_hpll_hclk_v1(unsigned int m, unsigned int frac_val)
{
	int ret = 0;
	struct hdmitx_dev *hdev = get_hdmitx_device();

	hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x0b3a0400 | (m & 0xff));
	hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x3, 28, 2);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL1, frac_val);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);

	if (frac_val == 0x8148) {
		if (((hdev->para->vic == HDMI_3840x2160p50_16x9) ||
			(hdev->para->vic == HDMI_3840x2160p60_16x9) ||
			(hdev->para->vic == HDMI_3840x2160p50_64x27) ||
			(hdev->para->vic == HDMI_3840x2160p60_64x27)) &&
			(hdev->para->cs != COLORSPACE_YUV420)) {
			hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x6a685c00);
			hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x11551293);
		} else {
			hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x6a685c00);
			hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x44331290);
		}
	} else {
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x6a68dc00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x65771290);
	}
	hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39272000);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x56540000);
	hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
	WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
	pr_info("HPLLv1: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));

	ret = (((hd_read_reg(P_HHI_HDMI_PLL_CNTL0) >> 30) & 0x3) == 0x3);
	return ret; /* return hpll locked status */
}

static bool set_hpll_hclk_v2(unsigned int m, unsigned int frac_val)
{
	int ret = 0;

	hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x0b3a0400 | (m & 0xff));
	hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x3, 28, 2);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL1, frac_val);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0xea68dc00);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x65771290);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39272000);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x56540000);
	hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
	WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
	pr_info("HPLLv2: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));

	ret = (((hd_read_reg(P_HHI_HDMI_PLL_CNTL0) >> 30) & 0x3) == 0x3);
	return ret; /* return hpll locked status */
}

static bool set_hpll_hclk_v3(unsigned int m, unsigned int frac_val)
{
	int ret = 0;

	hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x0b3a0400 | (m & 0xff));
	hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x3, 28, 2);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL1, frac_val);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0xea68dc00);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x65771290);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39272000);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x55540000);
	hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
	WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
	pr_info("HPLLv3: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));

	ret = (((hd_read_reg(P_HHI_HDMI_PLL_CNTL0) >> 30) & 0x3) == 0x3);
	return ret; /* return hpll locked status */
}

#define IS_DONGLE_MODE(hdev) \
	((hdev->dongle_mode) \
	&& (hdev->para->cs == COLORSPACE_YUV422 \
		|| hdev->para->cd == COLORDEPTH_24B) \
	&& (hdev->cur_VIC == HDMI_1280x720p50_16x9 \
		|| hdev->cur_VIC == HDMI_1280x720p60_16x9 \
		|| hdev->cur_VIC == HDMI_1920x1080i60_16x9 \
		|| hdev->cur_VIC == HDMI_1920x1080i50_16x9 \
		|| hdev->cur_VIC == HDMI_1920x1080p60_16x9 \
		|| hdev->cur_VIC == HDMI_1920x1080p50_16x9))

static void set_hpll_hclk_dongle_5940m(void)
{
	hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x0b3a04f7);
	hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x3, 28, 2);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x10000);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00100140);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x2a295c00);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x65771290);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39272000);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
	pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
	hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
	pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
	WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
	pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
}

void set_g12a_hpll_clk_out(unsigned int frac_rate, unsigned int clk)
{
	unsigned int m, m1, m2;
	struct hdmitx_dev *hdev = get_hdmitx_device();

	switch (clk) {
	case 5940000:
		if (IS_DONGLE_MODE(hdev)) {
			set_hpll_hclk_dongle_5940m();
			break;
		}
		if (set_hpll_hclk_v1(0xf7, frac_rate ? 0x8148 : 0x10000))
			break;
		else if (set_hpll_hclk_v2(0x7b, 0x18000))
			break;
		else if (set_hpll_hclk_v3(0xf7, 0x10000))
			break;
		else
			break;
		break;
	case 5405400:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b0004e1);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00007333);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 4830000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b0004c9);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00014000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x6a685c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x43231290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x29272000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x56540028);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 4455000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b0004b9);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x0000e10e);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00014000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x6a685c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x43231290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x29272000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x56540028);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 4324320:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b0004b4);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00005c29);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 3712500:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00049a);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x000110e1);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00016000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x6a685c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x43231290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x29272000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x56540028);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 3450000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00048f);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 3420000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00048e);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 3243240:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b000487);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x0000451f);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 2970000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00047b);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x000140b4);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 2685000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00046F);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x000140b4);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 2600000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00046C);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x000140b4);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 2560000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00046A);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x000140b4);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 2415000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b000464);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x000140b4);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 2134000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00045A);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x000140b4);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 2058000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b000455);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x000140b4);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 2000000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b000453);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x000140b4);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 1855800:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00044C);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x0001A000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 1560000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b000440);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x0001A000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 1540000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00043F);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x0001A000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 1462500:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00043C);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x0001A000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 1422000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00043A);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x0001A000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 320000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00040D);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	default:
		pr_info("error hpll clk: %d\n", clk);

		/* FIXME : consider pixel clocks over 200MHz */
		/* calculate "m" */
		m1 = (clk * 0x3A) / 1422000;
		m2 = (clk * 0xE1) / 5405400;
		m = (m1 + m2)/2;
		m &= 0xff;
		m |= 0x3b000400;
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, m);
		pr_info("m1 0x%x, m2 0x%x, m 0x%x\n", m1, m2, m);

		/* check pll LOCK time */
		hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);

		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	}
}

void set_hpll_od1_g12a(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0, 16, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 1, 16, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 2, 16, 2);
		break;
	default:
		break;
	}
}

void set_hpll_od2_g12a(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0, 18, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 1, 18, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 2, 18, 2);
		break;
	default:
		break;
	}
}

void set_hpll_od3_g12a(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0, 20, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 1, 20, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 2, 20, 2);
		break;
	default:
		pr_info("Err %s[%d]\n", __func__, __LINE__);
		break;
	}
}

int hdmitx_hpd_hw_op_g12a(enum hpd_op cmd)
{
	int ret = 0;

	switch (cmd) {
	case HPD_INIT_DISABLE_PULLUP:
		hd_set_reg_bits(P_PAD_PULL_UP_REG1, 0, 25, 1);
		break;
	case HPD_INIT_SET_FILTER:
		hdmitx_wr_reg(HDMITX_TOP_HPD_FILTER,
			((0xa << 12) | (0xa0 << 0)));
		break;
	case HPD_IS_HPD_MUXED:
		ret = !!(hd_read_reg(P_PERIPHS_PIN_MUX_6) & (1 << 31));
		break;
	case HPD_MUX_HPD:
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_B, 1, 8, 4);
		break;
	case HPD_UNMUX_HPD:
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_B, 0, 8, 4);
		break;
	case HPD_READ_HPD_GPIO:
		ret = hdmitx_rd_reg(HDMITX_DWC_PHY_STAT0) & (1 << 1);
		break;
	default:
		break;
	}
	return ret;
}


void set_hpll_sspll_g12a(enum hdmi_vic vic)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	switch (vic) {
	case HDMI_1920x1080p60_16x9:
	case HDMI_1920x1080p50_16x9:
	case HDMI_1280x720p60_16x9:
	case HDMI_1280x720p50_16x9:
	case HDMI_1920x1080i60_16x9:
	case HDMI_1920x1080i50_16x9:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 1, 29, 1);
		/* bit[22:20] hdmi_dpll_fref_sel
		 * bit[8] hdmi_dpll_ssc_en
		 * bit[7:4] hdmi_dpll_ssc_dep_sel
		 */
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 20, 3);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 8, 1);
		/* 2: 1000ppm  1: 500ppm */
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 4, 4);
		if (hdev->dongle_mode)
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 4, 4, 4);
		/* bit[15] hdmi_dpll_sdmnc_en */
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL3, 0, 15, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0, 29, 1);
		break;
	default:
		break;
	}
}

