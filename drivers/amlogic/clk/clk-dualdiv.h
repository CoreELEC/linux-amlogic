/*
 * drivers/amlogic/clk/clk-dualdiv.h
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

#ifndef __MESON_CLK_DUALDIV_H
#define __MESON_CLK_DUALDIV_H

#include <linux/clk-provider.h>
#include "clkc.h"

struct meson_dualdiv_clk {
	struct clk_hw hw;
	void __iomem *base;
	void   *data;
};

#define to_meson_dualdiv_clk(_hw) container_of(_hw, \
		struct meson_dualdiv_clk, hw)

struct meson_clk_dualdiv_param {
	unsigned int n1;
	unsigned int n2;
	unsigned int m1;
	unsigned int m2;
	unsigned int dual;
};

struct meson_clk_dualdiv_data {
	struct parm n1;
	struct parm n2;
	struct parm m1;
	struct parm m2;
	struct parm dual;
	const struct meson_clk_dualdiv_param *table;
};

extern const struct clk_ops meson_clk_dualdiv_ops;
extern const struct clk_ops meson_clk_dualdiv_ro_ops;

#endif /* __MESON_CLK_DUALDIV_H */
