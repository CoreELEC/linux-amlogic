// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Motorcomm PHYs
 *
 * Author: Peter Geis <pgwipeout@gmail.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>

#define PHY_ID_YT8511		0x0000010a
#define PHY_ID_YT8531		0x4f51e91b

#define YT8511_PAGE_SELECT	0x1e
#define YT8511_PAGE		0x1f
#define YT8511_EXT_CLK_GATE	0x0c
#define YT8511_EXT_DELAY_DRIVE	0x0d
#define YT8511_EXT_SLEEP_CTRL	0x27

/* 2b00 25m from pll
 * 2b01 25m from xtl *default*
 * 2b10 62.m from pll
 * 2b11 125m from pll
 */
#define YT8511_CLK_125M		(BIT(2) | BIT(1))
#define YT8511_PLLON_SLP	BIT(14)

/* RX Delay enabled = 1.8ns 1000T, 8ns 10/100T */
#define YT8511_DELAY_RX		BIT(0)

/* TX Gig-E Delay is bits 7:4, default 0x5
 * TX Fast-E Delay is bits 15:12, default 0xf
 * Delay = 150ps * N - 250ps
 * On = 2000ps, off = 50ps
 */
#define YT8511_DELAY_GE_TX_EN	(0xf << 4)
#define YT8511_DELAY_GE_TX_DIS	(0x2 << 4)
#define YT8511_DELAY_FE_TX_EN	(0xf << 12)
#define YT8511_DELAY_FE_TX_DIS	(0x2 << 12)

#define YT8531_RGMII_CONFIG1	0xa003

/* TX Gig-E Delay is bits 3:0, default 0x1
 * TX Fast-E Delay is bits 7:4, default 0xf
 * RX Delay is bits 13:10, default 0x0
 * Delay = 150ps * N
 * On = 2000ps, off = 50ps
 */
#define YT8531_DELAY_GE_TX_EN	(0xd << 0)
#define YT8531_DELAY_GE_TX_DIS	(0x0 << 0)
#define YT8531_DELAY_FE_TX_EN	(0xd << 4)
#define YT8531_DELAY_FE_TX_DIS	(0x0 << 4)
#define YT8531_DELAY_RX_EN	(0xd << 10)
#define YT8531_DELAY_RX_DIS	(0x0 << 10)
#define YT8531_DELAY_MASK	(GENMASK(13, 10) | GENMASK(7, 0))

#define YT8531_SYNCE_CFG	0xa012

/* Clk src config is bits 3:1
 * 3b000 src from pll
 * 3b001 src from rx_clk
 * 3b010 src from serdes
 * 3b011 src from ptp_in
 * 3b100 src from 25mhz refclk *default*
 * 3b101 src from 25mhz ssc
 * Clk rate select is bit 4
 * 1b0 25mhz clk output *default*
 * 1b1 125mhz clk output
 * Clkout enable is bit 6
 */
#define YT8531_CLKCFG_125M	BIT(6) | BIT(4) | (0x0 < 1)


static int yt8511_read_page(struct phy_device *phydev)
{
	return __phy_read(phydev, YT8511_PAGE_SELECT);
};

static int yt8511_write_page(struct phy_device *phydev, int page)
{
	return __phy_write(phydev, YT8511_PAGE_SELECT, page);
};

static int yt8511_config_init(struct phy_device *phydev)
{
	int oldpage, ret = 0;
	unsigned int ge, fe;

	oldpage = phy_select_page(phydev, YT8511_EXT_CLK_GATE);
	if (oldpage < 0)
		goto err_restore_page;

	/* set rgmii delay mode */
	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_RGMII:
		ge = YT8511_DELAY_GE_TX_DIS;
		fe = YT8511_DELAY_FE_TX_DIS;
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		ge = YT8511_DELAY_RX | YT8511_DELAY_GE_TX_DIS;
		fe = YT8511_DELAY_FE_TX_DIS;
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		ge = YT8511_DELAY_GE_TX_EN;
		fe = YT8511_DELAY_FE_TX_EN;
		break;
	case PHY_INTERFACE_MODE_RGMII_ID:
		ge = YT8511_DELAY_RX | YT8511_DELAY_GE_TX_EN;
		fe = YT8511_DELAY_FE_TX_EN;
		break;
	default: /* do not support other modes */
		ret = -EOPNOTSUPP;
		goto err_restore_page;
	}

	ret = __phy_modify(phydev, YT8511_PAGE, (YT8511_DELAY_RX | YT8511_DELAY_GE_TX_EN), ge);
	if (ret < 0)
		goto err_restore_page;

	/* set clock mode to 125mhz */
	ret = __phy_modify(phydev, YT8511_PAGE, 0, YT8511_CLK_125M);
	if (ret < 0)
		goto err_restore_page;

	/* fast ethernet delay is in a separate page */
	ret = __phy_write(phydev, YT8511_PAGE_SELECT, YT8511_EXT_DELAY_DRIVE);
	if (ret < 0)
		goto err_restore_page;

	ret = __phy_modify(phydev, YT8511_PAGE, YT8511_DELAY_FE_TX_EN, fe);
	if (ret < 0)
		goto err_restore_page;

	/* leave pll enabled in sleep */
	ret = __phy_write(phydev, YT8511_PAGE_SELECT, YT8511_EXT_SLEEP_CTRL);
	if (ret < 0)
		goto err_restore_page;

	ret = __phy_modify(phydev, YT8511_PAGE, 0, YT8511_PLLON_SLP);
	if (ret < 0)
		goto err_restore_page;

err_restore_page:
	return phy_restore_page(phydev, oldpage, ret);
}

static int yt8531_config_init(struct phy_device *phydev)
{
	int oldpage, ret = 0;
	unsigned int val;

	oldpage = phy_select_page(phydev, YT8531_RGMII_CONFIG1);
	if (oldpage < 0)
		goto err_restore_page;

	/* set rgmii delay mode */
	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_RGMII:
		val = YT8531_DELAY_RX_DIS | YT8531_DELAY_GE_TX_DIS | YT8531_DELAY_FE_TX_DIS;
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		val = YT8531_DELAY_RX_EN | YT8531_DELAY_GE_TX_DIS | YT8531_DELAY_FE_TX_DIS;
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		val = YT8531_DELAY_RX_DIS | YT8531_DELAY_GE_TX_EN | YT8531_DELAY_FE_TX_EN;
		break;
	case PHY_INTERFACE_MODE_RGMII_ID:
		val = YT8531_DELAY_RX_EN | YT8531_DELAY_GE_TX_EN | YT8531_DELAY_FE_TX_EN;
		break;
	default: /* do not support other modes */
		ret = -EOPNOTSUPP;
		goto err_restore_page;
	}

	ret = __phy_modify(phydev, YT8511_PAGE, YT8531_DELAY_MASK, val);
	if (ret < 0)
		goto err_restore_page;

	/* set clock mode to 125mhz */
	ret = __phy_write(phydev, YT8511_PAGE_SELECT, YT8531_SYNCE_CFG);
	if (ret < 0)
		goto err_restore_page;

	ret = __phy_write(phydev, YT8511_PAGE, YT8531_CLKCFG_125M);
	if (ret < 0)
		goto err_restore_page;

err_restore_page:
	return phy_restore_page(phydev, oldpage, ret);
}


static struct phy_driver motorcomm_phy_drvs[] = {
	{
		PHY_ID_MATCH_EXACT(PHY_ID_YT8511),
		.name		= "YT8511 Gigabit Ethernet",
		.config_init	= yt8511_config_init,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= yt8511_read_page,
		.write_page	= yt8511_write_page,
	}, {
		PHY_ID_MATCH_EXACT(PHY_ID_YT8531),
		.name		= "YT8531 Gigabit Ethernet",
		.config_init	= yt8531_config_init,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= yt8511_read_page,
		.write_page	= yt8511_write_page,
	},

};

module_phy_driver(motorcomm_phy_drvs);

MODULE_DESCRIPTION("Motorcomm PHY driver");
MODULE_AUTHOR("Peter Geis");
MODULE_LICENSE("GPL");

static const struct mdio_device_id __maybe_unused motorcomm_tbl[] = {
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8511) },
	{ PHY_ID_MATCH_EXACT(PHY_ID_YT8531) },
	{ /* sentinal */ }
};

MODULE_DEVICE_TABLE(mdio, motorcomm_tbl);
