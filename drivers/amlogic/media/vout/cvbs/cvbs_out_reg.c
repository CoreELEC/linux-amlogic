/*
 * drivers/amlogic/media/vout/cvbs/cvbs_out_reg.c
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
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "cvbs_out_reg.h"

#define cvbs_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

/* ********************************
 * mem map
 * *********************************
 */
#define CVBS_MAP_ANA       0
#define CVBS_MAP_CLK       1
#define CVBS_MAP_MAX       2

static int cvbs_reg_table[] = {
	CVBS_MAP_ANA,
	CVBS_MAP_CLK,
	CVBS_MAP_MAX,
};

struct cvbs_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

static struct cvbs_reg_map_s *cvbs_reg_map;
static int cvbs_ioremap_flag;

int cvbs_ioremap(struct platform_device *pdev)
{
	int i;
	int *table;
	struct resource *res;

	cvbs_ioremap_flag = 1;

	cvbs_reg_map = kcalloc(CVBS_MAP_MAX,
			       sizeof(struct cvbs_reg_map_s), GFP_KERNEL);
	if (!cvbs_reg_map)
		return -1;

	table = cvbs_reg_table;
	for (i = 0; i < CVBS_MAP_MAX; i++) {
		if (table[i] == CVBS_MAP_MAX)
			break;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			cvbs_log_info("%s: resource get error\n", __func__);
			kfree(cvbs_reg_map);
			cvbs_reg_map = NULL;
			return -1;
		}
		cvbs_reg_map[table[i]].base_addr = res->start;
		cvbs_reg_map[table[i]].size = resource_size(res);
		cvbs_reg_map[table[i]].p = devm_ioremap_nocache(&pdev->dev,
			res->start, cvbs_reg_map[table[i]].size);
		if (!cvbs_reg_map[table[i]].p) {
			cvbs_reg_map[table[i]].flag = 0;
			cvbs_log_info("%s: reg map failed: 0x%x\n",
				      __func__,
			       cvbs_reg_map[table[i]].base_addr);
			kfree(cvbs_reg_map);
			cvbs_reg_map = NULL;
			return -1;
		}
		cvbs_reg_map[table[i]].flag = 1;
		cvbs_log_info("%s: reg mapped: 0x%x -> %p\n",
			      __func__, cvbs_reg_map[table[i]].base_addr,
			      cvbs_reg_map[table[i]].p);
	}

	return 0;
}

static int check_cvbs_ioremap(int n)
{
	if (!cvbs_reg_map)
		return -1;
	if (n >= CVBS_MAP_MAX)
		return -1;
	if (cvbs_reg_map[n].flag == 0) {
		cvbs_log_info("reg 0x%x mapped error\n",
			      cvbs_reg_map[n].base_addr);
		return -1;
	}
	return 0;
}

static inline void __iomem *check_cvbs_ana_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = CVBS_MAP_ANA;
	if (check_cvbs_ioremap(reg_bus))
		return NULL;

	reg_offset = CVBS_REG_OFFSET(_reg);

	if (reg_offset >= cvbs_reg_map[reg_bus].size) {
		cvbs_log_info("invalid ana reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = cvbs_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_cvbs_clk_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = CVBS_MAP_CLK;
	if (check_cvbs_ioremap(reg_bus))
		return NULL;

	reg_offset = CVBS_REG_OFFSET(_reg);

	if (reg_offset >= cvbs_reg_map[reg_bus].size) {
		cvbs_log_info("invalid clk reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = cvbs_reg_map[reg_bus].p + reg_offset;
	return p;
}

/* ********************************
 * register access api
 * **********************************/
unsigned int cvbs_out_reg_read(unsigned int _reg)
{
	return vpu_vcbus_read(_reg);
}

void cvbs_out_reg_write(unsigned int _reg, unsigned int _value)
{
	return vpu_vcbus_write(_reg, _value);
};

void cvbs_out_reg_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	return vpu_vcbus_setb(reg, value, _start, _len);
}

unsigned int cvbs_out_reg_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return vpu_vcbus_getb(reg, _start, _len);
}

void cvbs_out_reg_set_mask(unsigned int reg, unsigned int _mask)
{
	vpu_vcbus_set_mask(reg, _mask);
}

void cvbs_out_reg_clr_mask(unsigned int reg, unsigned int _mask)
{
	vpu_vcbus_clr_mask(reg, _mask);
}

unsigned int cvbs_out_hiu_read(unsigned int _reg)
{
	void __iomem *p;
	unsigned int ret = 0;

	if (cvbs_ioremap_flag) {
		p = check_cvbs_clk_reg(_reg);
		if (p)
			ret = readl(p);
		else
			ret = 0;
	} else {
		ret = aml_read_hiubus(_reg);
	}

	return ret;
};

void cvbs_out_hiu_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	if (cvbs_ioremap_flag) {
		p = check_cvbs_clk_reg(_reg);
		if (p)
			writel(_value, p);
	} else {
		aml_write_hiubus(_reg, _value);
	}
};

void cvbs_out_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	cvbs_out_hiu_write(_reg, ((cvbs_out_hiu_read(_reg) &
		(~(((1L << _len)-1) << _start))) |
		((_value & ((1L << _len)-1)) << _start)));
}

unsigned int cvbs_out_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (cvbs_out_hiu_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void cvbs_out_hiu_set_mask(unsigned int _reg, unsigned int _mask)
{
	cvbs_out_hiu_write(_reg, (aml_read_hiubus(_reg) | (_mask)));
}

void cvbs_out_hiu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	cvbs_out_hiu_write(_reg, (aml_read_hiubus(_reg) & (~(_mask))));
}

unsigned int cvbs_out_ana_read(unsigned int _reg)
{
	void __iomem *p;
	unsigned int ret = 0;

	if (cvbs_ioremap_flag) {
		p = check_cvbs_ana_reg(_reg);
		if (p)
			ret = readl(p);
		else
			ret = 0;
	} else {
		ret = aml_read_hiubus(_reg);
	}

	return ret;
};

void cvbs_out_ana_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	if (cvbs_ioremap_flag) {
		p = check_cvbs_ana_reg(_reg);
		if (p)
			writel(_value, p);
	} else {
		aml_write_hiubus(_reg, _value);
	}
};

void cvbs_out_ana_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len)
{
	cvbs_out_ana_write(_reg, ((cvbs_out_ana_read(_reg) &
		(~(((1L << _len) - 1) << _start))) |
		((_value & ((1L << _len) - 1)) << _start)));
}

unsigned int cvbs_out_ana_getb(unsigned int _reg,
			       unsigned int _start, unsigned int _len)
{
	return (cvbs_out_ana_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

