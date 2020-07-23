/*
 * drivers/amlogic/clk/sc2/sc2_clk_sdemmc.c
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <dt-bindings/clock/amlogic,sc2-clkc.h>

#include "../clkc.h"
#include "sc2.h"

static const char * const sd_emmc_parent_names[] = { "xtal", "fclk_div2",
	"fclk_div3", "hifi_pll", "fclk_div2p5", "mpll2", "mpll3", "gp0_pll" };

static struct clk_mux sc2_nand_clk_mux = {
	.reg = (void *)CLKCTRL_NAND_CLK_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "nand_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = sd_emmc_parent_names,
		.num_parents = ARRAY_SIZE(sd_emmc_parent_names),
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider sc2_nand_clk_div = {
	.reg = (void *)CLKCTRL_NAND_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "nand_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "nand_clk_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
				CLK_SET_RATE_PARENT),
	},
};

static struct clk_gate sc2_nand_clk_gate = {
	.reg = (void *)CLKCTRL_NAND_CLK_CTRL,
	.bit_idx = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "nand_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "nand_clk_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
				CLK_SET_RATE_PARENT),
	},
};

/*cts_sd_emmc_A/B_clk*/
static struct clk_mux sc2_sd_emmc_A_clk_mux = {
	.reg = (void *)CLKCTRL_SD_EMMC_CLK_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_A_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = sd_emmc_parent_names,
		.num_parents = ARRAY_SIZE(sd_emmc_parent_names),
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider sc2_sd_emmc_A_clk_div = {
	.reg = (void *)CLKCTRL_SD_EMMC_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_A_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "sd_emmc_A_clk_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
				CLK_SET_RATE_PARENT),
	},
};

static struct clk_gate sc2_sd_emmc_A_clk_gate = {
	.reg = (void *)CLKCTRL_SD_EMMC_CLK_CTRL,
	.bit_idx = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_A_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "sd_emmc_A_clk_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
				CLK_SET_RATE_PARENT),
	},
};

static struct clk_mux sc2_sd_emmc_B_clk_mux = {
	.reg = (void *)CLKCTRL_SD_EMMC_CLK_CTRL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_B_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = sd_emmc_parent_names,
		.num_parents = ARRAY_SIZE(sd_emmc_parent_names),
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider sc2_sd_emmc_B_clk_div = {
	.reg = (void *)CLKCTRL_SD_EMMC_CLK_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_B_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "sd_emmc_B_clk_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
				CLK_SET_RATE_PARENT),
	},
};

static struct clk_gate sc2_sd_emmc_B_clk_gate = {
	.reg = (void *)CLKCTRL_SD_EMMC_CLK_CTRL,
	.bit_idx = 23,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_B_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "sd_emmc_B_clk_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
				CLK_SET_RATE_PARENT),
	},
};

static struct clk_hw *sd_emmc_clk_hws[] = {
	[CLKID_SD_EMMC_A_CLK_MUX - CLKID_SD_EMMC_A_CLK_MUX]
		= &sc2_sd_emmc_A_clk_mux.hw,
	[CLKID_SD_EMMC_A_CLK_DIV - CLKID_SD_EMMC_A_CLK_MUX]
		= &sc2_sd_emmc_A_clk_div.hw,
	[CLKID_SD_EMMC_A_CLK_GATE - CLKID_SD_EMMC_A_CLK_MUX]
		= &sc2_sd_emmc_A_clk_gate.hw,
	[CLKID_SD_EMMC_B_CLK_MUX - CLKID_SD_EMMC_A_CLK_MUX]
		= &sc2_sd_emmc_B_clk_mux.hw,
	[CLKID_SD_EMMC_B_CLK_DIV - CLKID_SD_EMMC_A_CLK_MUX]
		= &sc2_sd_emmc_B_clk_div.hw,
	[CLKID_SD_EMMC_B_CLK_GATE - CLKID_SD_EMMC_A_CLK_MUX]
		= &sc2_sd_emmc_B_clk_gate.hw,
	[CLKID_NAND_CLK_MUX - CLKID_SD_EMMC_A_CLK_MUX]
		= &sc2_nand_clk_mux.hw,
	[CLKID_NAND_CLK_DIV - CLKID_SD_EMMC_A_CLK_MUX]
		= &sc2_nand_clk_div.hw,
	[CLKID_NAND_CLK_GATE - CLKID_SD_EMMC_A_CLK_MUX]
		= &sc2_nand_clk_gate.hw,
};

void meson_sc2_sdemmc_init(void __iomem *clk_base)
{
	/* Populate base address for reg */
	pr_info("%s: register amlogic sdemmc clk\n", __func__);

	sc2_sd_emmc_A_clk_mux.reg = clk_base
				+ (unsigned long)(sc2_sd_emmc_A_clk_mux.reg);
	sc2_sd_emmc_A_clk_div.reg = clk_base
				+ (unsigned long)(sc2_sd_emmc_A_clk_div.reg);
	sc2_sd_emmc_A_clk_gate.reg = clk_base
				+ (unsigned long)(sc2_sd_emmc_A_clk_gate.reg);
	sc2_sd_emmc_B_clk_mux.reg = clk_base
				+ (unsigned long)(sc2_sd_emmc_B_clk_mux.reg);
	sc2_sd_emmc_B_clk_div.reg = clk_base
				+ (unsigned long)(sc2_sd_emmc_B_clk_div.reg);
	sc2_sd_emmc_B_clk_gate.reg = clk_base
				+ (unsigned long)(sc2_sd_emmc_B_clk_gate.reg);
	sc2_nand_clk_mux.reg = clk_base
				+ (unsigned long)(sc2_nand_clk_mux.reg);
	sc2_nand_clk_div.reg = clk_base
				+ (unsigned long)(sc2_nand_clk_div.reg);
	sc2_nand_clk_gate.reg = clk_base
				+ (unsigned long)(sc2_nand_clk_gate.reg);

	clks[CLKID_SD_EMMC_A_CLK_COMP] = clk_register_composite(NULL,
		"sd_emmc_p0_A_comp",
	    sd_emmc_parent_names, 8,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_A_CLK_MUX - CLKID_SD_EMMC_A_CLK_MUX],
	    &clk_mux_ops,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_A_CLK_DIV - CLKID_SD_EMMC_A_CLK_MUX],
	    &clk_divider_ops,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_A_CLK_GATE - CLKID_SD_EMMC_A_CLK_MUX],
	    &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_SD_EMMC_A_CLK_COMP]))
		pr_err("%s: %d sd_emmc_p0_A_comp error\n", __func__, __LINE__);

	clks[CLKID_SD_EMMC_B_CLK_COMP] = clk_register_composite(NULL,
		"sd_emmc_p0_B_comp",
	    sd_emmc_parent_names, 8,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_B_CLK_MUX - CLKID_SD_EMMC_A_CLK_MUX],
	    &clk_mux_ops,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_B_CLK_DIV - CLKID_SD_EMMC_A_CLK_MUX],
	    &clk_divider_ops,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_B_CLK_GATE - CLKID_SD_EMMC_A_CLK_MUX],
	    &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_SD_EMMC_B_CLK_COMP]))
		pr_err("%s: %d sd_emmc_p0_B_comp error\n", __func__, __LINE__);

	clks[CLKID_NAND_CLK_COMP] = clk_register_composite(NULL,
		"sd_emmc_p0_C_comp",
	    sd_emmc_parent_names, 8,
	    sd_emmc_clk_hws[CLKID_NAND_CLK_MUX - CLKID_SD_EMMC_A_CLK_MUX],
	    &clk_mux_ops,
	    sd_emmc_clk_hws[CLKID_NAND_CLK_DIV - CLKID_SD_EMMC_A_CLK_MUX],
	    &clk_divider_ops,
	    sd_emmc_clk_hws[CLKID_NAND_CLK_GATE - CLKID_SD_EMMC_A_CLK_MUX],
	     &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_NAND_CLK_COMP]))
		pr_err("%s: %d sd_emmc_p0_C_comp error\n", __func__, __LINE__);

	pr_info("%s: register amlogic sdemmc clk\n", __func__);
}
