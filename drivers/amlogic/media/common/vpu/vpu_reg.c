/*
 * drivers/amlogic/media/common/vpu/vpu_reg.c
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
#include <linux/amlogic/media/vpu/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"

/* ********************************
 * mem map
 * *********************************
 */
#define VPU_MAP_CLK       0
#define VPU_MAP_PWRCTRL   1
#define VPU_MAP_VCBUS     2
#define VPU_MAP_MAX       3

static int vpu_reg_table[] = {
	VPU_MAP_CLK,
	VPU_MAP_PWRCTRL,
	VPU_MAP_VCBUS,
	VPU_MAP_MAX,
};

struct vpu_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

static struct vpu_reg_map_s *vpu_reg_map;
static int vpu_ioremap_flag;

int vpu_ioremap(struct platform_device *pdev)
{
	int i;
	int *table;
	struct resource *res;

	vpu_ioremap_flag = 1;

	vpu_reg_map = kcalloc(VPU_MAP_MAX,
			      sizeof(struct vpu_reg_map_s), GFP_KERNEL);
	if (!vpu_reg_map) {
		VPUERR("%s: vpu_reg_map buf malloc error\n", __func__);
		return -1;
	}
	table = vpu_reg_table;
	for (i = 0; i < VPU_MAP_MAX; i++) {
		if (table[i] == VPU_MAP_MAX)
			break;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			VPUERR("%s: resource get error\n", __func__);
			kfree(vpu_reg_map);
			vpu_reg_map = NULL;
			return -1;
		}
		vpu_reg_map[table[i]].base_addr = res->start;
		vpu_reg_map[table[i]].size = resource_size(res);
		vpu_reg_map[table[i]].p = devm_ioremap_nocache(&pdev->dev,
			res->start, vpu_reg_map[table[i]].size);
		if (!vpu_reg_map[table[i]].p) {
			vpu_reg_map[table[i]].flag = 0;
			VPUERR("%s: reg map failed: 0x%x\n",
			       __func__,
			       vpu_reg_map[table[i]].base_addr);
			kfree(vpu_reg_map);
			vpu_reg_map = NULL;
			return -1;
		}
		vpu_reg_map[table[i]].flag = 1;
		if (vpu_debug_print_flag) {
			VPUPR("%s: reg mapped: 0x%x -> %p\n",
			      __func__,
			      vpu_reg_map[table[i]].base_addr,
			      vpu_reg_map[table[i]].p);
		}
	}

	return 0;
}

static int check_vpu_ioremap(int n)
{
	if (!vpu_reg_map)
		return -1;
	if (n >= VPU_MAP_MAX)
		return -1;
	if (vpu_reg_map[n].flag == 0) {
		VPUERR("reg 0x%x mapped error\n", vpu_reg_map[n].base_addr);
		return -1;
	}
	return 0;
}

static inline void __iomem *check_vpu_clk_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_CLK;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_offset = VPU_REG_OFFSET(_reg);

	if (reg_offset >= vpu_reg_map[reg_bus].size) {
		VPUERR("invalid clk reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_pwrctrl_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_PWRCTRL;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_offset = VPU_REG_OFFSET(_reg);

	if (reg_offset >= vpu_reg_map[reg_bus].size) {
		VPUERR("invalid pwrctrl reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_vcbus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_VCBUS;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_offset = VPU_REG_OFFSET(_reg);

	if (reg_offset >= vpu_reg_map[reg_bus].size) {
		VPUERR("invalid vcbus reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

/* ********************************
 * register access api
 * *********************************
 */
unsigned int vpu_hiu_read(unsigned int _reg)
{
	void __iomem *p;
	unsigned int ret = 0;

	if (vpu_ioremap_flag) {
		p = check_vpu_clk_reg(_reg);
		if (p)
			ret = readl(p);
		else
			ret = 0;
	} else {
		ret = aml_read_hiubus(_reg);
	}

	return ret;
};

void vpu_hiu_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	if (vpu_ioremap_flag) {
		p = check_vpu_clk_reg(_reg);
		if (p)
			writel(_value, p);
	} else {
		aml_write_hiubus(_reg, _value);
	}
};

void vpu_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vpu_hiu_write(_reg, ((vpu_hiu_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

unsigned int vpu_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (vpu_hiu_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void vpu_hiu_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_hiu_write(_reg, (vpu_hiu_read(_reg) | (_mask)));
}

void vpu_hiu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_hiu_write(_reg, (vpu_hiu_read(_reg) & (~(_mask))));
}

unsigned int vpu_pwrctrl_read(unsigned int _reg)
{
	void __iomem *p;
	unsigned int ret = 0;

	p = check_vpu_pwrctrl_reg(_reg);
	if (p)
		ret = readl(p);
	else
		ret = 0;

	return ret;
};

unsigned int vpu_pwrctrl_getb(unsigned int _reg,
			      unsigned int _start, unsigned int _len)
{
	return (vpu_pwrctrl_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

unsigned int vpu_vcbus_read(unsigned int _reg)
{
	void __iomem *p;
	unsigned int ret = 0;

	if (vpu_ioremap_flag) {
		p = check_vpu_vcbus_reg(_reg);
		if (p)
			ret = readl(p);
		else
			ret = 0;
	} else {
		ret = aml_read_vcbus(_reg);
	}

	return ret;
};

void vpu_vcbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	if (vpu_ioremap_flag) {
		p = check_vpu_vcbus_reg(_reg);
		if (p)
			writel(_value, p);
	} else {
		aml_write_vcbus(_reg, _value);
	}
};

void vpu_vcbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vpu_vcbus_write(_reg, ((vpu_vcbus_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

unsigned int vpu_vcbus_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (vpu_vcbus_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void vpu_vcbus_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_vcbus_write(_reg, (vpu_vcbus_read(_reg) | (_mask)));
}

void vpu_vcbus_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_vcbus_write(_reg, (vpu_vcbus_read(_reg) & (~(_mask))));
}

unsigned int vpu_ao_read(unsigned int _reg)
{
	if (vpu_ioremap_flag == 0)
		return aml_read_aobus(_reg);
	else
		return 0;
};

void vpu_ao_write(unsigned int _reg, unsigned int _value)
{
	if (vpu_ioremap_flag == 0)
		aml_write_aobus(_reg, _value);
};

void vpu_ao_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vpu_ao_write(_reg, ((vpu_ao_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

unsigned int vpu_cbus_read(unsigned int _reg)
{
	if (vpu_ioremap_flag == 0)
		return aml_read_cbus(_reg);
	else
		return 0;
};

void vpu_cbus_write(unsigned int _reg, unsigned int _value)
{
	if (vpu_ioremap_flag == 0)
		aml_write_cbus(_reg, _value);
};

void vpu_cbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vpu_cbus_write(_reg, ((vpu_cbus_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

void vpu_cbus_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_cbus_write(_reg, (vpu_cbus_read(_reg) | (_mask)));
}

void vpu_cbus_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_cbus_write(_reg, (vpu_cbus_read(_reg) & (~(_mask))));
}

