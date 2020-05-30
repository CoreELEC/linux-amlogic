/*
 * drivers/amlogic/media/vout/vout_serve/vout_reg.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/amlogic/iomap.h>
#include "vout_func.h"
#include "vout_reg.h"

/* ********************************
 * mem map
 * *********************************
 */
struct vout_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

static struct vout_reg_map_s *vout_reg_map;
static int vout_ioremap_flag;

int vout_ioremap(struct platform_device *pdev)
{
	struct resource *res;

	vout_ioremap_flag = 1;

	vout_reg_map = kzalloc(sizeof(*vout_reg_map), GFP_KERNEL);
	if (!vout_reg_map) {
		VOUTERR("%s: vout_reg_map buf malloc error\n", __func__);
		return -1;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		VOUTERR("%s: resource get error\n", __func__);
		kfree(vout_reg_map);
		vout_reg_map = NULL;
		return -1;
	}
	vout_reg_map->base_addr = res->start;
	vout_reg_map->size = resource_size(res);
	vout_reg_map->p = devm_ioremap_nocache(&pdev->dev, res->start,
					       vout_reg_map->size);
	if (!vout_reg_map->p) {
		vout_reg_map->flag = 0;
		VOUTERR("%s: reg map failed: 0x%x\n",
			__func__,
			vout_reg_map->base_addr);
		kfree(vout_reg_map);
		vout_reg_map = NULL;
		return -1;
	}
	vout_reg_map->flag = 1;

	VOUTPR("%s: reg mapped: 0x%x -> %p\n",
	       __func__,
	       vout_reg_map->base_addr,
	       vout_reg_map->p);

#ifndef CONFIG_AMLOGIC_VPU
	VOUTERR("%s: detect no CONFIG_AMLOGIC_VPU, vcbus can't access\n",
		__func__);
#endif

	return 0;
}

static int check_vout_ioremap(void)
{
	if (!vout_reg_map)
		return -1;
	if (vout_reg_map->flag == 0) {
		VOUTERR("reg 0x%x mapped error\n", vout_reg_map->base_addr);
		return -1;
	}
	return 0;
}

static inline void __iomem *check_vout_hiu_reg(unsigned int _reg)
{
	void __iomem *p;
	unsigned int reg_offset;

	if (check_vout_ioremap())
		return NULL;

	reg_offset = VOUT_REG_OFFSET(_reg);
	if (reg_offset >= vout_reg_map->size) {
		VOUTERR("invalid vcbus reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = vout_reg_map->p + reg_offset;
	return p;
}

/* ********************************
 * register access api
 * *********************************
 */
unsigned int vout_hiu_read(unsigned int _reg)
{
	void __iomem *p;
	unsigned int ret = 0;

	if (vout_ioremap_flag) {
		p = check_vout_hiu_reg(_reg);
		if (p)
			ret = readl(p);
		else
			ret = 0;
	} else {
		ret = aml_read_hiubus(_reg);
	}

	return ret;
};

void vout_hiu_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	if (vout_ioremap_flag) {
		p = check_vout_hiu_reg(_reg);
		if (p)
			writel(_value, p);
	} else {
		aml_write_hiubus(_reg, _value);
	}
};

void vout_hiu_setb(unsigned int _reg, unsigned int _value,
		   unsigned int _start, unsigned int _len)
{
	vout_hiu_write(_reg, ((vout_hiu_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

unsigned int vout_hiu_getb(unsigned int reg,
			   unsigned int _start, unsigned int _len)
{
	return (vout_hiu_read(reg) >> _start) & ((1L << _len) - 1);
}

unsigned int vout_vcbus_read(unsigned int _reg)
{
	unsigned int ret = 0;

	if (vout_ioremap_flag) {
#ifdef CONFIG_AMLOGIC_VPU
		ret = vpu_vcbus_read(_reg);
#else
		ret = 0;
#endif
	} else {
		ret = aml_read_vcbus(_reg);
	}

	return ret;
};

void vout_vcbus_write(unsigned int _reg, unsigned int _value)
{
	if (vout_ioremap_flag) {
#ifdef CONFIG_AMLOGIC_VPU
		vpu_vcbus_write(_reg, _value);
#endif
	} else {
		aml_write_vcbus(_reg, _value);
	}
};

void vout_vcbus_setb(unsigned int _reg, unsigned int _value,
		     unsigned int _start, unsigned int _len)
{
	vout_vcbus_write(_reg, ((vout_vcbus_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

unsigned int vout_vcbus_getb(unsigned int reg,
			     unsigned int _start, unsigned int _len)
{
	return (vout_vcbus_read(reg) >> _start) & ((1L << _len) - 1);
}
