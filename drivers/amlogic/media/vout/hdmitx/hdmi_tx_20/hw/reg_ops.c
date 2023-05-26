/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/reg_ops.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/arm-smccc.h>
#include "common.h"
#include "hdmi_tx_reg.h"
#include "reg_ops.h"
#include "mach_reg.h"

/* For gxb/gxl/gxm */
static struct reg_map reg_maps_def[] = {
	[CBUS_REG_IDX] = { /* CBUS */
		.phy_addr = 0xc1109800,
		.size = 0xa00000,
	},
	[PERIPHS_REG_IDX] = { /* PERIPHS */
		.phy_addr = 0xc8834400,
		.size = 0x200,
	},
	[VCBUS_REG_IDX] = { /* VPU */
		.phy_addr = 0xd0100000,
		.size = 0x40000,
	},
	[AOBUS_REG_IDX] = { /* RTI */
		.phy_addr = 0xc8100000,
		.size = 0x100000,
	},
	[HHI_REG_IDX] = { /* HIU */
		.phy_addr = 0xc883c000,
		.size = 0x2000,
	},
	[RESET_CBUS_REG_IDX] = { /* RESET */
		.phy_addr = 0xc1104400,
		.size = 0x100,
	},
	[HDMITX_SEC_REG_IDX] = { /* HDMITX SECURE */
		.phy_addr = 0xda83a000,
		.size = 0x2000,
	},
	[HDMITX_REG_IDX] = { /* HDMITX NON-SECURE*/
		.phy_addr = 0xc883a000,
		.size = 0x2000,
	},
	[ELP_ESM_REG_IDX] = {
		.phy_addr = 0xd0044000,
		.size = 0x100,
	},
	[REG_IDX_END] = {
		.phy_addr = REG_ADDR_END, /* end */
	}
};

/* For txlx */
static struct reg_map reg_maps_txlx[] = {
	[CBUS_REG_IDX] = { /* CBUS */
		.phy_addr = 0xffd00000,
		.size = 0x100000,
	},
	[PERIPHS_REG_IDX] = { /* PERIPHS */
		.phy_addr = 0xff634400,
		.size = 0x2000,
	},
	[VCBUS_REG_IDX] = { /* VPU */
		.phy_addr = 0xff900000,
		.size = 0x40000,
	},
	[AOBUS_REG_IDX] = { /* RTI */
		.phy_addr = 0xff800000,
		.size = 0x100000,
	},
	[HHI_REG_IDX] = { /* HIU */
		.phy_addr = 0xff63c000,
		.size = 0x2000,
	},
	[RESET_CBUS_REG_IDX] = { /* RESET */
		.phy_addr = 0xffd00000,
		.size = 0x1100,
	},
	[HDMITX_SEC_REG_IDX] = { /* HDMITX SECURE */
		.phy_addr = 0xff63a000,
		.size = 0x2000,
	},
	[HDMITX_REG_IDX] = { /* HDMITX NON-SECURE*/
		.phy_addr = 0xff63a000,
		.size = 0x2000,
	},
	[ELP_ESM_REG_IDX] = {
		.phy_addr = 0xffe01000,
		.size = 0x100,
	},
	[REG_IDX_END] = {
		.phy_addr = REG_ADDR_END, /* end */
	}
};

/* For g12a */
static struct reg_map reg_maps_g12a[] = {
	[CBUS_REG_IDX] = { /* CBUS */
		.phy_addr = 0xffd00000,
		.size = 0x100000,
	},
	[PERIPHS_REG_IDX] = { /* PERIPHS */
		.phy_addr = 0xff634400,
		.size = 0x2000,
	},
	[VCBUS_REG_IDX] = { /* VPU */
		.phy_addr = 0xff900000,
		.size = 0x40000,
	},
	[AOBUS_REG_IDX] = { /* RTI */
		.phy_addr = 0xff800000,
		.size = 0x100000,
	},
	[HHI_REG_IDX] = { /* HIU */
		.phy_addr = 0xff63c000,
		.size = 0x2000,
	},
	[RESET_CBUS_REG_IDX] = { /* RESET */
		.phy_addr = 0xffd00000,
		.size = 0x1100,
	},
	[HDMITX_SEC_REG_IDX] = { /* HDMITX DWC LEVEL*/
		.phy_addr = 0xff600000,
		.size = 0x8000,
	},
	[HDMITX_REG_IDX] = { /* HDMITX TOP LEVEL*/
		.phy_addr = 0xff608000,
		.size = 0x4000,
	},
	[ELP_ESM_REG_IDX] = {
		.phy_addr = 0xffe01000,
		.size = 0x100,
	},
	[REG_IDX_END] = {
		.phy_addr = REG_ADDR_END, /* end */
	}
};

/* For TM2 */
static struct reg_map reg_maps_tm2[] = {
	[CBUS_REG_IDX] = { /* CBUS */
		.phy_addr = 0xffd00000,
		.size = 0x100000,
	},
	[PERIPHS_REG_IDX] = { /* PERIPHS */
		.phy_addr = 0xff634400,
		.size = 0x2000,
	},
	[VCBUS_REG_IDX] = { /* VPU */
		.phy_addr = 0xff900000,
		.size = 0x40000,
	},
	[AOBUS_REG_IDX] = { /* RTI */
		.phy_addr = 0xff800000,
		.size = 0x100000,
	},
	[HHI_REG_IDX] = { /* HIU */
		.phy_addr = 0xff63c000,
		.size = 0x2000,
	},
	[RESET_CBUS_REG_IDX] = { /* RESET */
		.phy_addr = 0xffd00000,
		.size = 0x1100,
	},
	[HDMITX_SEC_REG_IDX] = { /* HDMITX DWC LEVEL*/
		.phy_addr = 0xff670000,
		.size = 0x8000,
	},
	[HDMITX_REG_IDX] = { /* HDMITX TOP LEVEL*/
		.phy_addr = 0xff678000,
		.size = 0x4000,
	},
	[ELP_ESM_REG_IDX] = {
		.phy_addr = 0xffe01000,
		.size = 0x100,
	},
	[REG_IDX_END] = {
		.phy_addr = REG_ADDR_END, /* end */
	}
};

/* For sc2 */
static struct reg_map reg_maps_sc2[] = {
	[SYSCTRL_REG_IDX] = { /* CBUS */
		.phy_addr = 0xfe010000,
		.size = 0x100,
	},
	[PERIPHS_REG_IDX] = { /* PERIPHS */
		.phy_addr = 0xfe004000,
		.size = 0x2000,
	},
	[VCBUS_REG_IDX] = { /* VPU */
		.phy_addr = 0xff000000,
		.size = 0x40000,
	},
	[CLKCTRL_REG_IDX] = { /* HIU */
		.phy_addr = 0xfe000000,
		.size = 0x2000,
	},
	[RESETCTRL_REG_IDX] = { /* RESET */
		.phy_addr = 0xfe002000,
		.size = 0x100 << 2,
	},
	[ANACTRL_REG_IDX] = { /* ANACTRL */
		.phy_addr = 0xfe008000,
		.size = 0x100 << 2,
	},
	[PWRCTRL_REG_IDX] = { /* PWRCTRL */
		.phy_addr = 0xfe00c000,
		.size = 0x200 << 2,
	},
	[HDMITX_SEC_REG_IDX] = { /* HDMITX DWC LEVEL*/
		.phy_addr = 0xfe300000,
		.size = 0x8000,
	},
	[HDMITX_REG_IDX] = { /* HDMITX TOP LEVEL*/
		.phy_addr = 0xfe308000,
		.size = 0x4000,
	},
	[ELP_ESM_REG_IDX] = {
		.phy_addr = 0xfe032000,
		.size = 0x100,
	},
	[REG_IDX_END] = {
		.phy_addr = REG_ADDR_END, /* end */
	}
};

struct reg_map *map;

void init_reg_map(unsigned int type)
{
	int i;

	switch (type) {
	case MESON_CPU_ID_SC2:
		map = reg_maps_sc2;
		break;
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
		map = reg_maps_g12a;
		break;
	case MESON_CPU_ID_TM2:
		map = reg_maps_tm2;
		break;
	case MESON_CPU_ID_TXLX:
		map = reg_maps_txlx;
		break;
	default:
		map = reg_maps_def;
		break;
	}
	for (i = 0; map[i].phy_addr != REG_ADDR_END; i++) {
		/* ANACTRL_REG_IDX is new added in SC2,
		 * and should be skipped for others
		 */
		if (!map[i].phy_addr)
			continue;
		map[i].p = ioremap(map[i].phy_addr, map[i].size);
		if (!map[i].p) {
			pr_info("hdmitx20: failed Mapped PHY: 0x%x\n",
				map[i].phy_addr);
		}
	}
}

unsigned int TO_PHY_ADDR(unsigned int addr)
{
	unsigned int index;
	unsigned int offset;

	index = addr >> BASE_REG_OFFSET;
	offset = addr & (((1 << BASE_REG_OFFSET) - 1));

	return (map[index].phy_addr + offset);
}

void __iomem *TO_PMAP_ADDR(unsigned int addr)
{
	unsigned int index;
	unsigned int offset;

	index = addr >> BASE_REG_OFFSET;
	offset = addr & (((1 << BASE_REG_OFFSET) - 1));

	return (void __iomem *)(map[index].p + offset);
}

unsigned int get_hdcp22_base(void)
{
	if (map)
		return map[ELP_ESM_REG_IDX].phy_addr;
	else
		return reg_maps_def[ELP_ESM_REG_IDX].phy_addr;
}

#define CHECKADDR(addr) \
	do { \
		if (map[(addr) >> BASE_REG_OFFSET].phy_addr == 0) { \
			pr_info("addr = 0x%08x", addr); \
			dump_stack(); \
			break; \
		} \
	} while (0)

unsigned int hd_read_reg(unsigned int addr)
{
	unsigned int val = 0;
	unsigned int paddr = TO_PHY_ADDR(addr);
	unsigned int index = (addr) >> BASE_REG_OFFSET;

	struct hdmitx_dev *hdev = get_hdmitx_device();
	pr_debug(REG "Rd[0x%x] 0x%x\n", paddr, val);

	CHECKADDR(addr);

	switch (hdev->chip_type) {
	case MESON_CPU_ID_TXLX:
		switch (index) {
		case CBUS_REG_IDX:
		case RESET_CBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			val = aml_read_cbus(addr);
			break;
		case VCBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			val = aml_read_vcbus(addr);
			break;
		case AOBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			val = aml_read_aobus(addr);
			break;
		case HHI_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			val = aml_read_hiubus(addr);
			break;
		default:
			break;
		}
		break;
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_SC2:
	default:
		val = readl(TO_PMAP_ADDR(addr));
		break;
	}

	return val;
}
EXPORT_SYMBOL(hd_read_reg);

void hd_write_reg(unsigned int addr, unsigned int val)
{
	unsigned int paddr = TO_PHY_ADDR(addr);
	unsigned int index = (addr) >> BASE_REG_OFFSET;

	struct hdmitx_dev *hdev = get_hdmitx_device();
	pr_debug(REG "Wr[0x%x] 0x%x\n", paddr, val);

	CHECKADDR(addr);

	switch (hdev->chip_type) {
	case MESON_CPU_ID_TXLX:
		switch (index) {
		case CBUS_REG_IDX:
		case RESET_CBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			aml_write_cbus(addr, val);
			break;
		case VCBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			aml_write_vcbus(addr, val);
			break;
		case AOBUS_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			aml_write_aobus(addr, val);
			break;
		case HHI_REG_IDX:
			addr &= ~(index << BASE_REG_OFFSET);
			addr >>= 2;
			aml_write_hiubus(addr, val);
			break;
		default:
			break;
		}
		break;
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_SC2:
	default:
		writel(val, TO_PMAP_ADDR(addr));
		break;
	}
}
EXPORT_SYMBOL(hd_write_reg);

void hd_set_reg_bits(unsigned int addr, unsigned int value,
	unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = hd_read_reg(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	hd_write_reg(addr, data32);
}
EXPORT_SYMBOL(hd_set_reg_bits);


unsigned int hdmitx_rd_reg_normal(unsigned int addr)
{
	unsigned long offset = (addr & DWC_OFFSET_MASK) >> 24;
	unsigned int data;
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000018, (unsigned long)addr, 0, 0, 0, 0, 0, 0, &res);

	data = (unsigned int)((res.a0)&0xffffffff);

	pr_debug(REG "%s rd[0x%x] 0x%x\n", offset ? "DWC" : "TOP",
			addr, data);
	return data;
}

unsigned int hdmitx_rd_reg_g12a(unsigned int addr)
{
	unsigned int large_offset = addr >> 24;
	unsigned int small_offset = addr & ((1 << 24)  - 1);
	unsigned long hdmitx_addr = 0;
	unsigned int val;

	switch  (large_offset) {
	case 0x10:
		/*DWC*/
		hdmitx_addr = HDMITX_SEC_REG_ADDR(small_offset);
		val = readb(TO_PMAP_ADDR(hdmitx_addr));
		break;
	case 0x11:
	case 0x01:
		/*SECURITY DWC/TOP*/
		val = hdmitx_rd_reg_normal(addr);
		break;
	case 00:
	default:
		/*TOP*/
		if ((small_offset >= 0x2000) && (small_offset <= 0x365E)) {
			hdmitx_addr = HDMITX_REG_ADDR(small_offset);
			val = readb(TO_PMAP_ADDR(hdmitx_addr));
		} else {
			hdmitx_addr = HDMITX_REG_ADDR((small_offset << 2));
			val = readl(TO_PMAP_ADDR(hdmitx_addr));
		}
		break;
	}
	return val;
}

unsigned int hdmitx_rd_reg(unsigned int addr)
{
	unsigned int data;
	struct hdmitx_dev *hdev = get_hdmitx_device();

	if (hdev->chip_type >= MESON_CPU_ID_G12A)
		data = hdmitx_rd_reg_g12a(addr);
	else
		data = hdmitx_rd_reg_normal(addr);
	return data;
}
EXPORT_SYMBOL(hdmitx_rd_reg);

bool hdmitx_get_bit(unsigned int addr, unsigned int bit_nr)
{
	return (hdmitx_rd_reg(addr) & (1 << bit_nr)) == (1 << bit_nr);
}

void hdmitx_wr_reg_normal(unsigned int addr, unsigned int data)
{
	unsigned long offset = (addr & DWC_OFFSET_MASK) >> 24;
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000019,
			(unsigned long)addr,
			data,
			0, 0, 0, 0, 0, &res);

	pr_debug("%s wr[0x%x] 0x%x\n", offset ? "DWC" : "TOP",
			addr, data);
}

void hdmitx_wr_reg_g12a(unsigned int addr, unsigned int data)
{
	unsigned int large_offset = addr >> 24;
	unsigned int small_offset = addr & ((1 << 24)  - 1);
	unsigned long hdmitx_addr = 0;

	switch (large_offset) {
	case 0x10:
		/*DWC*/
		hdmitx_addr = HDMITX_SEC_REG_ADDR(small_offset);
		writeb(data & 0xff, TO_PMAP_ADDR(hdmitx_addr));
		break;
	case 0x11:
	case 0x01:
		/*SECURITY DWC/TOP*/
		hdmitx_wr_reg_normal(addr, data);
		break;
	case 00:
	default:
		/*TOP*/
		if ((small_offset >= 0x2000) && (small_offset <= 0x365E)) {
			hdmitx_addr = HDMITX_REG_ADDR(small_offset);
			writeb(data & 0xff, TO_PMAP_ADDR(hdmitx_addr));
		} else {
			hdmitx_addr = HDMITX_REG_ADDR((small_offset << 2));
			writel(data, TO_PMAP_ADDR(hdmitx_addr));
		}
	}
}

void hdmitx_wr_reg(unsigned int addr, unsigned int data)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	if (hdev->chip_type >= MESON_CPU_ID_G12A)
		hdmitx_wr_reg_g12a(addr, data);
	else
		hdmitx_wr_reg_normal(addr, data);
}
EXPORT_SYMBOL(hdmitx_wr_reg);

void hdmitx_set_reg_bits(unsigned int addr, unsigned int value,
	unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = hdmitx_rd_reg(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	hdmitx_wr_reg(addr, data32);
}
EXPORT_SYMBOL(hdmitx_set_reg_bits);

void hdmitx_poll_reg(unsigned int addr, unsigned int val, unsigned long timeout)
{
	unsigned long time = 0;

	time = jiffies;
	while ((!(hdmitx_rd_reg(addr) & val)) &&
		time_before(jiffies, time + timeout)) {
		mdelay(2);
	}
	if (time_after(jiffies, time + timeout))
		pr_info(REG "hdmitx poll:0x%x  val:0x%x T1=%lu t=%lu T2=%lu timeout\n",
			addr, val, time, timeout, jiffies);
}
EXPORT_SYMBOL(hdmitx_poll_reg);

unsigned int hdmitx_rd_check_reg(unsigned int addr, unsigned int exp_data,
	unsigned int mask)
{
	unsigned long rd_data;

	rd_data = hdmitx_rd_reg(addr);
	if ((rd_data | mask) != (exp_data | mask)) {
		pr_info(REG "HDMITX-DWC addr=0x%04x rd_data=0x%02x\n",
			(unsigned int)addr, (unsigned int)rd_data);
		pr_info(REG "Error: HDMITX-DWC exp_data=0x%02x mask=0x%02x\n",
			(unsigned int)exp_data, (unsigned int)mask);
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(hdmitx_rd_check_reg);
