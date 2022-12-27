// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/delay.h>
#include <linux/arm-smccc.h>
#include "common.h"

/*
 * in s5, there are 4 plls for hdmitx module
 * sub-pll, htx-pll, fpll, gp2pll
 * Legacy hdmi 1.4/2.0 tmds modes: sub-pll, htx-pll
 * FRL modes without DSC: sub-pll, htx-pll, fpll
 * FRL modes with DSC: sub-pll, htx-pll, fpll, gp2pll
 */
const static char od_map[9] = {
	0, 0, 1, 0, 2, 0, 0, 0, 3,
};

void disable_hdmitx_s5_plls(struct hdmitx_dev *hdev)
{
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0);
	hd21_write_reg(CLKCTRL_FPLL_CTRL0, 0);
	hd21_write_reg(CLKCTRL_GP2PLL_CTRL0, 0);
}

/* htx pll VCO output: (3G, 6G), for tmds */
static void set_s5_htxpll_clk_other(const u32 clk, const bool frl_en)
{
	u32 quotient;
	u32 remainder;
	u32 div0p5_en;
	u32 rem_1;
	u32 rem_2;

	if (clk < 3000000 || clk >= 6000000) {
		pr_err("%s[%d] clock should be 4~6G\n", __func__, __LINE__);
		return;
	}

	// set sub-pll as 24M output
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, (1 << 16) | (100 << 8) | (1 << 1));
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 0, 1);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0xf00022a5);
	/* 0x8: lp_pll_clk selects pll_clk  0xb: selects lp_pll_clk24m */
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x55813001 | (0xb << 4));
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0, 1, 1);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 2, 1);

	div0p5_en = 1;
	if (clk % 12000 == 0) {
		quotient = clk / 12000;
		remainder = 0;
	} else {
		quotient = clk / 12000;
		remainder = clk - quotient * 12000;
		/* remainder range: 0 ~ 99999, 0x1869f, 17bits */
		/* convert remainder to 0 ~ 2^17 */
		if (remainder) {
			rem_1 = remainder / 16;
			rem_2 = remainder - rem_1 * 16;
			rem_1 *= 1 << 17;
			rem_1 /= 750;
			rem_2 *= 1 << 13;
			rem_2 /= 750;
			remainder = rem_1 + rem_2;
		}
	}

	hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x000c0000 | (quotient << 8));
	/* HDMIPLL_CTRL4[25] enable tx_phy_clk1618 */
	/* bit16: spll_div_0p5_en */
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x414412f2 | (frl_en << 25) | (div0p5_en << 16));
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x00000203);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, (!!remainder << 31) | remainder);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 0, 1);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 1, 1);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 2, 1);
}

/* htx pll VCO output: 4/5/6G, for FRL */
static void set_s5_htxpll_clk_4_5_6g(const u32 clk, const bool frl_en)
{
	u32 htxpll_m = 0;
	u32 htxpll_ref_clk_od = 0;

	if (clk != 6000000 && clk != 5000000 && clk != 4000000) {
		pr_err("%s[%d] clock should be 4, 5, or 6G\n", __func__, __LINE__);
		return;
	}

	/* For 6G clock, here use the 24M as source */
	if (clk == 6000000) {
		/* 250 * 24M = 6G */
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x000c0000 | (250 << 8));
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00006101 | (1 << 16));
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0xf0002211);
		/* use the 24m instead of sub-pll */
		/* 0x8: lp_pll_clk selects pll_clk  0xb: selects lp_pll_clk24m */
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x55813000 | (0xb << 4));
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x0144f2f2 | (frl_en << 25));
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x00000203);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x0);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 0, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 1, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 2, 1);
		return;
	}

	/* For 4, or 5G clock, here use the sub-pll generate 200M as source */
	/* 24MHz * 100 / 12 = 200MHz */
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, (1 << 1) | (100 << 8) | (12 << 16));
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 0, 1);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0xf00022a5);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x55813081);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0, 1, 1);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 2, 1);
	if (clk == 5000000) {
		htxpll_m = 50;
		htxpll_ref_clk_od = 1;
	}
	if (clk == 4000000) {
		htxpll_m = 20;
		htxpll_ref_clk_od = 0;
	}
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL3,
		0x000c0000 | (htxpll_m << 8) | (htxpll_ref_clk_od << 4));
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x03400293 | (frl_en << 25));
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x00000203);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x00000000);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 0, 1);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 1, 1);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 2, 1);
}

void set21_s5_htxpll_clk_out(const u32 clk, const u32 div)
{
	u32 div1;
	u32 div2;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	enum hdmi_colorspace cs = HDMI_COLORSPACE_YUV444;
	enum hdmi_color_depth cd = COLORDEPTH_24B;

	if (!hdev || !hdev->para)
		return;

	cs = hdev->para->cs;
	cd = hdev->para->cd;

	pr_info("%s[%d] htxpll vco %d div %d\n", __func__, __LINE__, clk, div);

	if (clk <= 3000000 || clk > 6000000) {
		pr_info("%s[%d] %d out of htxpll range(3~6G]\n", __func__, __LINE__, clk);
		return;
	}

	/* due to the VCO work performance, here needs to consider 3 cases
	 * 1. 3G to 4G * 2. 4G to 6G
	 * 3. 6G
	 */
	if (clk == 6000000 || clk == 5000000 || clk == 4000000)
		set_s5_htxpll_clk_4_5_6g(clk, hdev->frl_rate ? 1 : 0);
	else
		set_s5_htxpll_clk_other(clk, hdev->frl_rate ? 1 : 0);

	/* setting htxpll div */
	if (div > 8) {
		div1 = 8;
		div2 = div / 8;
	} else {
		div1 = div;
		div2 = 1;
	}
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, od_map[div1], 20, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, od_map[div2], 22, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 0, 24, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL4, 0, 30, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0, 2, 1);
	if (cs == HDMI_COLORSPACE_YUV420) {
		if (cd == COLORDEPTH_24B)
			hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 20, 2);
		else
			hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 0, 20, 2);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL4, 1, 30, 2);
	}
}

void set_frl_hpll_od(enum frl_rate_enum rate)
{
	if (rate == FRL_NONE || rate > FRL_12G4L) {
		pr_info("hdmitx: frl: wrong rate %d\n", rate);
		return;
	}

	/* fixed OD */
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 0, 22, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 0, 24, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL4, 0, 30, 2);
	switch (rate) {
	case FRL_3G3L:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 2, 20, 2);
		break;
	case FRL_6G3L:
	case FRL_6G4L:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 20, 2);
		break;
	case FRL_8G4L:
	case FRL_10G4L:
	case FRL_12G4L:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 0, 20, 2);
		break;
	default:
		break;
	};
}

// fpll: 2376M  2376/24=99=0x63
//       2376/16=148.5M
/* div: 1, 2, 4, 8, ... 64 */
/* pixel_od: 1:1, 1:1.25, 1:1.5, 1:2 */
void hdmitx_set_s5_fpll(u32 clk, u32 div, u32 pixel_od)
{
	u32 div1;
	u32 div2;
	u32 quotient;
	u32 remainder;

	pr_info("%s[%d] clk %d div %d pixel_od %d\n", __func__, __LINE__, clk, div, pixel_od);
	/* setting fpll vco */
	quotient = clk / 24000;
	remainder = clk - quotient * 24000;
	/* remainder range: 0 ~ 23999, 0x5dbf, 15bits */
	remainder *= 1 << 17;
	remainder /= 24000;
	hd21_write_reg(CLKCTRL_FPLL_CTRL0, 0x21210000 | quotient);
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, 1, 28, 1);
	hd21_write_reg(CLKCTRL_FPLL_CTRL1, 0x03a00000 | remainder);
	hd21_write_reg(CLKCTRL_FPLL_CTRL2, 0x00040000);
	hd21_write_reg(CLKCTRL_FPLL_CTRL3, 0x0b0da000);
	if (remainder)
		hd21_set_reg_bits(CLKCTRL_FPLL_CTRL3, 1, 27, 1); /* enable frac */
	usleep_range(20, 30);
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, 0, 29, 1);
	usleep_range(20, 30);
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL3, 1, 9, 1); /* enable pll_lock_rst */

	/* setting fpll div */
	if (div > 8) {
		div1 = 8;
		div2 = div / 8;
	} else {
		div1 = div;
		div2 = 1;
	}
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, od_map[div1], 23, 2); // fpll_tmds_od<3:2> div8
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, od_map[div2], 21, 2); // fpll_tmds_od<1:0> div2

	/* setting pixel_od */
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, pixel_od, 13, 3); // pixel_od
}

// gp2pll: 2376M  2376/24=99=0x63
//       2376/16=148.5M
/* div: 1, 2, 4, 8, ... 64 */
void hdmitx_set_s5_gp2pll(u32 clk, u32 div)
{
	u32 quotient;
	u32 remainder;

	pr_info("%s[%d] clk %d div %d\n", __func__, __LINE__, clk, div);
	/* setting fpll vco */
	quotient = clk / 24000;
	remainder = clk - quotient * 24000;
	/* remainder range: 0 ~ 23999, 0x5dbf, 15bits */
	remainder *= 1 << 17;
	remainder /= 24000;
	hd21_write_reg(CLKCTRL_GP2PLL_CTRL0, 0x20010800 | quotient);
	hd21_set_reg_bits(CLKCTRL_GP2PLL_CTRL0, 1, 28, 1);
	hd21_write_reg(CLKCTRL_GP2PLL_CTRL1, 0x03a00000 | remainder);
	hd21_write_reg(CLKCTRL_GP2PLL_CTRL2, 0x00040000);
	hd21_write_reg(CLKCTRL_GP2PLL_CTRL3, 0x010da000);
	if (remainder)
		hd21_set_reg_bits(CLKCTRL_GP2PLL_CTRL3, 1, 27, 1); /* enable frac */
	usleep_range(20, 30);
	hd21_set_reg_bits(CLKCTRL_GP2PLL_CTRL0, 0, 29, 1);
	usleep_range(20, 30);
	hd21_set_reg_bits(CLKCTRL_GP2PLL_CTRL3, 1, 9, 1); /* enable pll_lock_rst */

	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, od_map[div], 23, 2); // gp2pll_tmds_od<2:0>
}

void hdmitx_set_s5_clkdiv(struct hdmitx_dev *hdev)
{
	if (!hdev && !hdev->para)
		return;

	/* cts_htx_tmds_clk selects the htx_tmds20_clk or fll_tmds_clk */
	hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, hdev->frl_rate ? 1 : 0, 25, 2);
	if (!hdev->frl_rate && hdev->para->cs == HDMI_COLORSPACE_YUV420)
		hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, 1, 16, 7);
	else
		hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, 0, 16, 7);
	/* master_clk selects the vid_pll_clk or fpll_pixel_clk */
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, hdev->frl_rate ? 4 : 0, 16, 3);
}

void hdmitx21_phy_bandgap_en_s5(void)
{
	u32 val = 0;

	val = hd21_read_reg(ANACTRL_HDMIPHY_CTRL0);
	if (val == 0)
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x0b4242);
}

void hdmitx_s5_phy_pre_init(struct hdmitx_dev *hdev)
{
	enum frl_rate_enum frl_rate = hdev->frl_rate;

	/* Stage1: reset registers */
	hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x0);
	hd21_write_reg(ANACTRL_HDMIPHY_CTRL1, 0x0);
	hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0x0);
	hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0);
	hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0);
	ndelay(10);

	/* Stage2: enable Bandgap */
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL5, 0x03, 0, 8);
	udelay(10);

	/* Stage3: enable LDO */
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 0xbd, 24, 8);
	udelay(10);

	/* Stage4: enable dcc */
	if (frl_rate == FRL_3G3L || frl_rate == FRL_6G3L)
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL6, 0x57, 8, 8); /* power down channel 4 */
	else
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL6, 0x77, 8, 8);
	ndelay(10);

	/* Stage5: enable p2s */
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL6, 0x0b, 0, 8);
	if (frl_rate == FRL_3G3L || frl_rate == FRL_6G3L)
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL5, 0x3f, 8, 8); /* power down channel 4 */
	else
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL5, 0xff, 8, 8);
	ndelay(10);

	/* set phy ch0/1/2/3 swap as default */
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 0, 18, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 1, 20, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 2, 22, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 3, 24, 2);
	// wire    wr_enable = control[3];
	// wire    fifo_enable = control[2];
	// assign  phy_clk_en = control[1];
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 0, 0, 1); // Enable Soft_reset
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 1, 0, 1); // Enable Soft_reset
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 0, 0, 1); // Enable Soft_reset
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 1, 1, 1); // Enable tmds_clk
	// Enable enable the write/read decoupling state machine
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 1, 3, 1);
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 1, 2, 1); // Enable the decoupling FIFO
}

void hdmitx_set_s5_phypara(enum frl_rate_enum frl_rate, u32 tmds_clk)
{
	const u16 swing[] = {
		[FRL_NONE] = 0x0000,
		[FRL_3G3L] = 0x00d5,
		[FRL_6G3L] = 0x00cf,
		[FRL_6G4L] = 0xcfcf,
		[FRL_8G4L] = 0xcfcf,
		[FRL_10G4L] = 0xcfcf,
		[FRL_12G4L] = 0xafaf,
	};
	const u8 ffe[] = {
		[FRL_NONE] = 0x00,
		[FRL_3G3L] = 0x00,
		[FRL_6G3L] = 0x00,
		[FRL_6G4L] = 0x00,
		[FRL_8G4L] = 0x01,
		[FRL_10G4L] = 0x0a,
		[FRL_12G4L] = 0x0b,
	};
	const u8 drv[] = {
		[FRL_NONE] = 0x11,
		[FRL_3G3L] = 0x03,
		[FRL_6G3L] = 0x03,
		[FRL_6G4L] = 0x33,
		[FRL_8G4L] = 0x77,
		[FRL_10G4L] = 0x77,
		[FRL_12G4L] = 0x77,
	};
	u8 rterm = 0; /* this will get from ufuse */
	struct arm_smccc_res res;

	/* Stage6: enable Rterm */
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL0, 0xd8, 16, 8);
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL0, 0x3, 24, 2);
	arm_smccc_smc(HDCPTX_IOOPR, HDMITX_GET_RTERM, 0, 0, 0, 0, 0, 0, &res);
	rterm = (unsigned int)((res.a0) & 0xffffffff);
	rterm = rterm & 0x3f;
	pr_info("%s[%d] rterm = %d\n", __func__, __LINE__, rterm);
	if (!rterm)
		rterm = 9; /* default value when efuse invalid */
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL0, rterm, 26, 6);
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 0x06, 8, 8);
	ndelay(10);

	/* Stage7: set output swing */
	if (frl_rate != FRL_NONE && frl_rate <= FRL_12G4L) {
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL0, swing[frl_rate], 0, 16);
	} else {
		u32 swing = 0;

		if (tmds_clk >= 290000)
			swing = 0x90d5;
		else if (tmds_clk >= 148000)
			swing = 0x90d4;
		else
			swing = 0x90d3;
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL0, swing, 0, 16);
	};
	ndelay(10);

	/* Stage8: set ffe */
	if (frl_rate != FRL_NONE && frl_rate <= FRL_12G4L)
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, ffe[frl_rate], 0, 8);
	else
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 0, 0, 8);
	ndelay(10);

	/* Stage9: enable driver */
	if (frl_rate != FRL_NONE && frl_rate <= FRL_12G4L) {
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, drv[frl_rate], 16, 8);
	} else {
		u32 drv = 0;

		if (tmds_clk >= 290000)
			drv = 0x17;
		else if (tmds_clk >= 148000)
			drv = 0x13;
		else
			drv = 0x11;
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, drv, 16, 8);
	}
}

void hdmitx21_sys_reset_s5(void)
{
	/* Refer to system-Registers.docx */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 29); /* hdmi_tx */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 22); /* hdmitxphy */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 19); /* vid_pll_div */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 16); /* hdmitx_apb */
}

void set21_hpll_sspll_s5(enum hdmi_vic vic)
{
	switch (vic) {
	case HDMI_16_1920x1080p60_16x9:
	case HDMI_31_1920x1080p50_16x9:
	case HDMI_4_1280x720p60_16x9:
	case HDMI_19_1280x720p50_16x9:
	case HDMI_5_1920x1080i60_16x9:
	case HDMI_20_1920x1080i50_16x9:
		break;
	default:
		break;
	}
}
