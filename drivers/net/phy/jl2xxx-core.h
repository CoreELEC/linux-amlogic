/*
 * Copyright (C) 2021 JLSemi Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#ifndef _JLSEMI_CORE_H
#define _JLSEMI_CORE_H

#include <linux/phy.h>
#include <linux/version.h>
#include <linux/kernel.h>

#define JL2XX1_PHY_ID		0x937c4030
#define JLSEMI_PHY_ID_MASK	0xfffffff0

#define JL2101_PHY_ID		0x937c4032

#define MII_JLSEMI_PHY_PAGE	0x1f

#define WOL_CTL_PAGE		18
#define WOL_CTL_REG		21
#define WOL_CTL_STAS_PAGE	4608
#define WOL_CTL_STAS_REG	16
#define WOL_MAC_ADDR2_REG	17
#define WOL_MAC_ADDR1_REG	18
#define WOL_MAC_ADDR0_REG	19
#define WOL_EVENT		BIT(1)
#define WOL_POLARITY		BIT(14)
#define WOL_EN			BIT(6)
#define WOL_CTL_EN		BIT(15)

/* 
This is a backport of the commit https://github.com/torvalds/linux/commit/aa2af2eb447c9a21c8c9e8d2336672bb620cf900
It should be added in include/linux/phy.h but since it's only used here and I don't want to break other things,
so I just add neccessary macros for the jl2xxx driver here (another PHY_ID_MATCH_VENDOR macro is omitted)
*/
#define PHY_ID_MATCH_EXACT(id) .phy_id = (id), .phy_id_mask = GENMASK(31, 0)
#define PHY_ID_MATCH_MODEL(id) .phy_id = (id), .phy_id_mask = GENMASK(31, 4)

/************************* Configuration section *************************/

#define JLSEMI_WOL_EN		0


/************************* JLSemi iteration code *************************/
struct jl2xx1_priv {
	u16 sw_info;
};

int jl2xxx_pre_init(struct phy_device *phydev);

int config_phy_info(struct phy_device *phydev,
		    struct jl2xx1_priv *jl2xx1);

int check_rgmii(struct phy_device *phydev);

int dis_rgmii_tx_ctrl(struct phy_device *phydev);

int config_suspend(struct phy_device *phydev);

int config_resume(struct phy_device *phydev);

int enable_wol(struct phy_device *phydev);

int disable_wol(struct phy_device *phydev);

int setup_wol_low_polarity(struct phy_device *phydev);

int setup_wol_high_polarity(struct phy_device *phydev);

int clear_wol_event(struct phy_device *phydev);

int store_mac_addr(struct phy_device *phydev);

int software_version(struct phy_device *phydev);


/********************** Convenience function for phy **********************/

/* Notice: You should change page 0 when you When you call it after*/
int jlsemi_write_page(struct phy_device *phydev, int page);

int jlsemi_read_page(struct phy_device *phydev);

int jlsemi_modify_paged_reg(struct phy_device *phydev,
			    int page, u32 regnum,
			    u16 mask, u16 set);

int jlsemi_set_bits(struct phy_device *phydev,
		    int page, u32 regnum, u16 val);

int jlsemi_clear_bits(struct phy_device *phydev,
		      int page, u32 regnum, u16 val);

int jlsemi_get_bit(struct phy_device *phydev,
		    int page, u32 regnum, u16 val);

int jlsemi_drivers_register(struct phy_driver *phydrvs, int size);

void jlsemi_drivers_unregister(struct phy_driver *phydrvs, int size);

#endif /* _JLSEMI_CORE_H */

