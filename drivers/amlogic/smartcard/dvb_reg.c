/*
 * drivers/amlogic/smartcard/dvb_reg.c
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

#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include "dvb_reg.h"

#define LOG_ERROR  0
#define LOG_DBG    1
#define LOG_VER    2

#define dprintk(level, debug, x...)\
	do {\
		if ((level) <= (debug)) \
			printk(x);\
	} while (0)

#define pr_verify(fmt, args...)   \
	dprintk(LOG_VER, debug_rw, "reg:" fmt, ## args)
#define pr_dbg(fmt, args...)      \
	dprintk(LOG_DBG, debug_rw, "reg:" fmt, ## args)
#define dprint(fmt, args...)     \
	dprintk(LOG_ERROR, debug_rw, "reg:" fmt, ## args)

MODULE_PARM_DESC(debug_rw, "\n\t\t Enable sc debug information");
static int debug_rw = 0x3;
module_param(debug_rw, int, 0644);

static void *p_smc_hw_base;

void aml_write_smc(unsigned int reg, unsigned int val)
{
	void *ptr = (void *)(p_smc_hw_base + reg);

	writel(val, ptr);
	if (debug_rw & 0x2) {
		pr_verify("write addr:%lx, org v:0x%0x, ret v:0x%0x\n",
			  (unsigned long)ptr, val, readl(ptr));
	}
}

int aml_read_smc(unsigned int reg)
{
	void *addr = p_smc_hw_base + reg;
	int ret = 0;

	ret = readl(addr);
	pr_dbg("read addr:%lx, value:0x%0x\n", (unsigned long)addr, ret);
	return ret;
}

int init_smc_addr(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dprint("%s fail\n", __func__);
		return -1;
	}

	p_smc_hw_base = devm_ioremap_nocache(&pdev->dev, res->start,
					     resource_size(res));
	if (!p_smc_hw_base) {
		dprint("%s base addr error\n", __func__);
		return -1;
	}
	return 0;
}
