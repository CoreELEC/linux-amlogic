/*
 * drivers/amlogic/debug/debug_scrambler_ramoops.c
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

int scrambler_ramoops_init(void)
{
	struct device_node *np;
	struct resource res;
	void __iomem *vaddr;
	unsigned int val;

	np = of_find_compatible_node(NULL, NULL,
		"amlogic, ddr-scrambler-preserve");
	if (!np)
		return -ENODEV;

	if (of_address_to_resource(np, 0, &res))
		return -ENODEV;

	vaddr = ioremap(res.start, resource_size(&res));
	if (IS_ERR(vaddr))
		return PTR_ERR(vaddr);

	val = readl(vaddr);
	val = val | 0x1;
	writel(val, vaddr);

	iounmap(vaddr);
	pr_warn("%s preserve startup key\n", __func__);

	return 0;
}
