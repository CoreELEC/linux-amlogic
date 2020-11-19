/*
 * drivers/net/phy/realtek.c
 *
 * Driver for Realtek PHYs
 *
 * Author: Johnson Leung <r58129@freescale.com>
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/phy.h>
#include <linux/module.h>

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/netdevice.h>

#include <linux/amlogic/pm.h>
#include <linux/amlogic/scpi_protocol.h>
#endif
#define RTL821x_LCR		0x10
#define RTL821x_PHYSR		0x11
#define RTL821x_PHYSR_DUPLEX	0x2000
#define RTL821x_PHYSR_SPEED	0xc000
#define RTL821x_INER		0x12
#define RTL821x_INER_INIT	0x6400
#define RTL821x_INSR		0x13
#define RTL8211E_INER_LINK_STATUS 0x400

#define RTL8211F_INER_LINK_STATUS 0x0010
#define RTL8211F_INSR		0x1d
#define RTL8211F_PAGE_SELECT	0x1f
#define RTL8211F_TX_DELAY	0x100

MODULE_DESCRIPTION("Realtek PHY driver");
MODULE_AUTHOR("Johnson Leung");
MODULE_LICENSE("GPL");

static int enable_wol = 0;

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
unsigned int support_external_phy_wol;
unsigned int external_rx_delay;
unsigned int external_tx_delay;
#endif

static int __init init_wol_state(char *str)
{
	enable_wol = simple_strtol(str, NULL, 0);
	support_external_phy_wol = enable_wol;
	pr_info("%s, enable_wol=%d\n",__func__, enable_wol);

	return 0;
}
__setup("enable_wol=", init_wol_state);

static int rtl821x_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL821x_INSR);

	return (err < 0) ? err : 0;
}

static int rtl8211f_ack_interrupt(struct phy_device *phydev)
{
	int err;

	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xa43);
	err = phy_read(phydev, RTL8211F_INSR);
	/* restore to default page 0 */
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0);

	return (err < 0) ? err : 0;
}

static int rtl8211b_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL821x_INER_INIT);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211e_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211E_INER_LINK_STATUS);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211f_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211F_INER_LINK_STATUS);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211f_reset(struct phy_device *phydev)
{
	u16 ret;
	unsigned int retries;

	/* reset phy */
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0);
	phy_write(phydev, MII_BMCR, BMCR_RESET | BMCR_ANENABLE | BMCR_ANRESTART);

	/* wait for ready */
	/* Poll until the reset bit clears (50ms per retry == 0.6 sec) */
	retries = 12;
	do {
		msleep(50);
		ret = phy_read(phydev, MII_BMCR);
	} while (ret & BMCR_RESET && --retries);

	/* Poll until the auto negotiation get set (50ms per retry == 5s sec) */
	retries = 100;
	do {
		msleep(50);
		ret = phy_read(phydev, MII_BMSR);
	} while (!(ret & BMSR_ANEGCOMPLETE) && --retries);

	return 0;
}

static int rtl8211f_config_init(struct phy_device *phydev)
{
	int ret;
	u16 reg;

	ret = genphy_config_init(phydev);
	if (ret < 0)
		return ret;

	scpi_send_usr_data(SCPI_CL_WOL, &enable_wol, sizeof(enable_wol));

	/* enable TX-delay for rgmii-id and rgmii-txid, otherwise disable it */
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd08);
	reg = phy_read(phydev, 0x11);

	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
		phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID ||
		external_tx_delay)
			reg |= RTL8211F_TX_DELAY;
	else
			reg &= ~RTL8211F_TX_DELAY;

	phy_write(phydev, 0x11, reg);

	/*switch page d08*/
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd08);
	reg = phy_read(phydev, 0x15);
	if (external_rx_delay) {
		/*add 2ns delay for rx*/
		phy_write(phydev, 0x15, reg | 0x8);
	} else {
		/*del 2ns rx*/
		phy_write(phydev, 0x15, reg & 0xfff7);
	}

	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0);

	/*disable clk_out pin 35 set page 0x0a43 reg25.0 as 0*/
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0a43);
	reg = phy_read(phydev, 0x19);
	/*set reg25 bit0 as 0*/
	phy_write(phydev, 0x19, reg & 0xfffe);
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
	/*pad isolation*/
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd8a);
	reg = phy_read(phydev, 0x13);
	reg &= ~(0x1 << 15);
	reg |= (0x1 << 12);
	phy_write(phydev, 0x13, reg);

	/*pin 31 pull high*/
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd40);
	reg = phy_read(phydev, 0x16);
	phy_write(phydev, 0x16, reg | (1 << 5));

	/* config mac address for wol*/
	if ((phydev->attached_dev) && (support_external_phy_wol)) {
		unsigned char *mac_addr = phydev->attached_dev->dev_addr;
		pr_info("use mac for wol = %02x:%02x:%02x:%02x:%02x:%02x\n",
			mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

		phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd8c);
		phy_write(phydev, 0x10, mac_addr[0] | (mac_addr[1] << 8));
		phy_write(phydev, 0x11, mac_addr[2] | (mac_addr[3] << 8));
		phy_write(phydev, 0x12, mac_addr[4] | (mac_addr[5] << 8));

		/*set magic packet for wol*/
		phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd8a);
		phy_write(phydev, 0x10, 0x1000);
		phy_write(phydev, 0x11, 0x9fff);
	} else {
		pr_debug("not set wol mac\n");
	}
#endif
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd04); /*set page 0xd04*/
	phy_write(phydev, RTL821x_LCR, /*led configuration*/
		(1 << 0)                /*led0 - Link 10*/
		| (1 << 8)              /*led1 - Link 1000*/
		| (1 << 11) | (1 << 14) /*led2 - Link 100 + Active 10/100/1000*/
		| (1 << 15));           /*mode B*/
	phy_write(phydev, RTL821x_PHYSR, 0); /*disable eee led indication*/

	/*reset phy to apply*/
	return rtl8211f_reset(phydev);
}

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
int rtl8211f_resume(struct phy_device *phydev)
{
	int ret;
	u16 reg;

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
	/*switch page d08*/
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd08);
	reg = phy_read(phydev, 0x15);
	if (external_rx_delay) {
		/*add 2ns delay for rx*/
		phy_write(phydev, 0x15, reg | 0x8);
	} else {
		/*del 2ns rx*/
		phy_write(phydev, 0x15, reg & 0xfff7);
	}

	if (external_tx_delay) {
		reg = phy_read(phydev, 0x11);
		phy_write(phydev, 0x11, reg | 0x100);
	} else {
		reg = phy_read(phydev, 0x11);
		reg &= ~RTL8211F_TX_DELAY;
		phy_write(phydev, 0x11, reg);
	}
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0);
#endif

	mutex_lock(&phydev->lock);

	/* enable TX-delay for rgmii-id and rgmii-txid, otherwise disable it */
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd08);
	reg = phy_read(phydev, 0x11);
	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
		phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID ||
		external_tx_delay)
			reg |= RTL8211F_TX_DELAY;
	else
			reg &= ~RTL8211F_TX_DELAY;
	phy_write(phydev, 0x11, reg);

	/*pad isolation*/
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd8a);
	reg = phy_read(phydev, 0x13);
	phy_write(phydev, 0x13, reg | (0x1 << 12));

	/*pin 31 pull high*/
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd40);
	reg = phy_read(phydev, 0x16);
	phy_write(phydev, 0x16, reg | (1 << 5));

	/*reset phy to apply*/
	ret = rtl8211f_reset(phydev);

	mutex_unlock(&phydev->lock);

	pr_debug("%s %d\n", __func__, __LINE__);

	return ret;
}
#endif

static struct phy_driver realtek_drvs[] = {
	{
		.phy_id         = 0x00008201,
		.name           = "RTL8201CP Ethernet",
		.phy_id_mask    = 0x0000ffff,
		.features       = PHY_BASIC_FEATURES,
		.flags          = PHY_HAS_INTERRUPT,
		.config_aneg    = &genphy_config_aneg,
		.read_status    = &genphy_read_status,
	}, {
		.phy_id		= 0x001cc912,
		.name		= "RTL8211B Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= &genphy_config_aneg,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl821x_ack_interrupt,
		.config_intr	= &rtl8211b_config_intr,
	}, {
		.phy_id		= 0x001cc914,
		.name		= "RTL8211DN Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= genphy_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= rtl821x_ack_interrupt,
		.config_intr	= rtl8211e_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, {
		.phy_id		= 0x001cc915,
		.name		= "RTL8211E Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= &genphy_config_aneg,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl821x_ack_interrupt,
		.config_intr	= &rtl8211e_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, {
		.phy_id		= 0x001cc916,
		.name		= "RTL8211F Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= &genphy_config_aneg,
		.config_init	= &rtl8211f_config_init,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl8211f_ack_interrupt,
		.config_intr	= &rtl8211f_config_intr,
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
		.resume		= rtl8211f_resume,
#else
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
#endif
	},
};

module_phy_driver(realtek_drvs);

static struct mdio_device_id __maybe_unused realtek_tbl[] = {
	{ 0x001cc912, 0x001fffff },
	{ 0x001cc914, 0x001fffff },
	{ 0x001cc915, 0x001fffff },
	{ 0x001cc916, 0x001fffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, realtek_tbl);
