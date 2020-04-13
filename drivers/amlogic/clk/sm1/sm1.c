/*
 * drivers/amlogic/clk/sm1/sm1.c
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
#include <dt-bindings/clock/amlogic,g12a-clkc.h>

#include "../clkc.h"
#include "../g12a/g12a.h"
#include "sm1.h"
#include <linux/delay.h>

static struct meson_clk_pll sm1_gp1_pll = {
	.m = {
		.reg_off = HHI_GP1_PLL_CNTL0,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_GP1_PLL_CNTL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_GP1_PLL_CNTL0,
		.shift   = 16,
		.width   = 3,
	},
	.rate_table = g12a_pll_rate_table,
	.rate_count = ARRAY_SIZE(g12a_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_g12a_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static const char * const media_parent_names[] = { "xtal",
	"gp0_pll", "hifi_pll", "fclk_div2p5", "fclk_div3", "fclk_div4",
	"fclk_div5",  "fclk_div7"};

static struct clk_mux cts_vipnanoq_core_clk_mux = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_vipnanoq_core_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_vipnanoq_core_clk_div = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_vipnanoq_core_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_vipnanoq_core_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_vipnanoq_core_clk_gate = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vipnanoq_core_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_vipnanoq_core_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux cts_vipnanoq_axi_clk_mux = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_vipnanoq_axi_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_vipnanoq_axi_clk_div = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_vipnanoq_axi_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_vipnanoq_axi_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_vipnanoq_axi_clk_gate = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vipnanoq_axi_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_vipnanoq_axi_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static const char * const media_parent_names_mipi[] = { "xtal",
	"gp0_pll", "mpll1", "mpll2", "fclk_div3", "fclk_div4",
	"fclk_div5",  "fclk_div7"
};

static struct clk_mux cts_mipi_csi_phy_clk_mux = {
	.reg = (void *)HHI_MIPI_CSI_PHY_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_mipi_csi_phy_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names_mipi,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_mipi_csi_phy_clk_div = {
	.reg = (void *)HHI_MIPI_CSI_PHY_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_mipi_csi_phy_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_mipi_csi_phy_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_mipi_csi_phy_clk_gate = {
	.reg = (void *)HHI_MIPI_CSI_PHY_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
	.name = "cts_mipi_csi_phy_clk_gate",
	.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_mipi_csi_phy_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static const char * const media_parent_names_adapt[] = { "xtal",
	"fclk_div4", "fclk_div3", "fclk_div5", "fclk_div7", "mpll2",
	"mpll3",  "gp0_pll"
};

static struct clk_mux cts_csi_adapt_clk_mux = {
	.reg = (void *)HHI_CSI2_ADAPT_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_csi_adapt_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names_adapt,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_csi_adapt_clk_div = {
	.reg = (void *)HHI_CSI2_ADAPT_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_csi_adapt_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_csi_adapt_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_csi_adapt_clk_gate = {
	.reg = (void *)HHI_CSI2_ADAPT_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
	.name = "cts_csi_adapt_clk_gate",
	.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_csi_adapt_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux sm1_dsu_pre_src_clk_mux0 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL5,
	.mask = 0x3,
	.shift = 0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre_src0",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "xtal", "fclk_div2",
				"fclk_div3", "gp1_pll" },
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_mux sm1_dsu_pre_src_clk_mux1 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL5,
	.mask = 0x3,
	.shift = 16,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre_src1",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "xtal", "fclk_div2",
				"fclk_div3", "gp1_pll" },
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_divider sm1_dsu_clk_div0 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL5,
	.shift = 4,
	.width = 6,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_div0",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "dsu_pre_src0" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_divider sm1_dsu_clk_div1 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL5,
	.shift = 20,
	.width = 6,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_div1",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "dsu_pre_src1" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sm1_dsu_pre_clk_mux0 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL5,
	.mask = 0x1,
	.shift = 2,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre0",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "dsu_pre_src0",
						"dsu_clk_div0",},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sm1_dsu_pre_clk_mux1 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL5,
	.mask = 0x1,
	.shift = 18,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre1",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "dsu_pre_src1",
						"dsu_clk_div1",},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sm1_dsu_pre_post_clk_mux = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL5,
	.mask = 0x1,
	.shift = 10,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre_post",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "dsu_pre0",
						"dsu_pre1",},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_mux sm1_dsu_pre_clk = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL5,
	.mask = 0x1,
	.shift = 11,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre_clk",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "dsu_pre_post",
						"sys_pll",},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_mux sm1_dsu_clk = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL6,
	.mask = 0x1,
	.shift = 27,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "cpu_clk",
						"dsu_pre_clk",},
		.num_parents = 2,
	},
};

static struct meson_clk_pll *const sm1_clk_plls[] = {
	&sm1_gp1_pll,
};

static MESON_GATE(sm1_csi_dig, HHI_GCLK_MPEG1, 18);
static MESON_GATE(sm1_nna, HHI_GCLK_MPEG1, 19);
static MESON_GATE(sm1_parser1, HHI_GCLK_MPEG1, 28);
static MESON_GATE(sm1_csi_host, HHI_GCLK_MPEG2, 16);
static MESON_GATE(sm1_csi_adpat, HHI_GCLK_MPEG2, 17);
static MESON_GATE(sm1_temp_sensor, HHI_GCLK_MPEG2, 22);
static MESON_GATE(sm1_csi_phy, HHI_GCLK_MPEG2, 29);

static struct clk_gate *sm1_clk_gates[] = {
	&sm1_csi_dig,
	&sm1_nna,
	&sm1_parser1,
	&sm1_csi_host,
	&sm1_csi_adpat,
	&sm1_temp_sensor,
	&sm1_csi_phy,
};

static struct clk_hw *sm1_clk_hws[] = {
	[CLKID_GP1_PLL - CLKID_SM1_ADD_BASE] = &sm1_gp1_pll.hw,
	[CLKID_DSU_PRE_SRC0 - CLKID_SM1_ADD_BASE] =
				&sm1_dsu_pre_src_clk_mux0.hw,
	[CLKID_DSU_PRE_SRC1 - CLKID_SM1_ADD_BASE] =
				&sm1_dsu_pre_src_clk_mux1.hw,
	[CLKID_DSU_CLK_DIV0 - CLKID_SM1_ADD_BASE] =
				&sm1_dsu_clk_div0.hw,
	[CLKID_DSU_CLK_DIV1 - CLKID_SM1_ADD_BASE] =
				&sm1_dsu_clk_div1.hw,
	[CLKID_DSU_PRE_MUX0 - CLKID_SM1_ADD_BASE] =
				&sm1_dsu_pre_clk_mux0.hw,
	[CLKID_DSU_PRE_MUX1 - CLKID_SM1_ADD_BASE] =
				&sm1_dsu_pre_clk_mux1.hw,
	[CLKID_DSU_PRE_POST_MUX - CLKID_SM1_ADD_BASE] =
				&sm1_dsu_pre_post_clk_mux.hw,
	[CLKID_DSU_PRE_CLK - CLKID_SM1_ADD_BASE] =
				&sm1_dsu_pre_clk.hw,
	[CLKID_DSU_CLK - CLKID_SM1_ADD_BASE] = &sm1_dsu_clk.hw,
	[CLKID_CSI_DIG_CLK - CLKID_SM1_ADD_BASE] =
				&sm1_csi_dig.hw,
	[CLKID_NNA_CLK - CLKID_SM1_ADD_BASE] =
				&sm1_nna.hw,
	[CLKID_PARSER1_CLK - CLKID_SM1_ADD_BASE] =
				&sm1_parser1.hw,
	[CLKID_CSI_HOST_CLK - CLKID_SM1_ADD_BASE] =
				&sm1_csi_host.hw,
	[CLKID_CSI_ADPAT_CLK - CLKID_SM1_ADD_BASE] =
				&sm1_csi_adpat.hw,
	[CLKID_TEMP_SENSOR_CLK - CLKID_SM1_ADD_BASE] =
				&sm1_temp_sensor.hw,
	[CLKID_CSI_PHY_CLK - CLKID_SM1_ADD_BASE] =
				&sm1_csi_phy.hw,
};

struct sm1_nb_data {
	struct notifier_block nb;
};

static int sm1_dsu_mux_clk_notifier_cb(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct clk *dsu_pre_clk, *parent_clk;
	int ret;

	switch (event) {
	case PRE_RATE_CHANGE:
		parent_clk = sm1_dsu_pre_clk_mux1.hw.clk;
		if (strcmp(current->comm, "cfinteractive"))
			pr_debug("PRE The curent process commond: \"%s\", the pid ：%i\n",
				current->comm, current->pid);
		break;
	case POST_RATE_CHANGE:
		parent_clk = sm1_dsu_pre_clk_mux0.hw.clk;
		if (strcmp(current->comm, "cfinteractive"))
			pr_debug("POST The curent process commond：\"%s\", the pid ：%i\n",
				current->comm, current->pid);
		break;
	default:
		return NOTIFY_DONE;
	}

	dsu_pre_clk = sm1_dsu_pre_post_clk_mux.hw.clk;

	ret = clk_set_parent(dsu_pre_clk, parent_clk);
	if (ret)
		return notifier_from_errno(ret);

	usleep_range(80, 120);

	return NOTIFY_OK;
}

static struct sm1_nb_data sm1_dsu_nb_data = {
	.nb.notifier_call = sm1_dsu_mux_clk_notifier_cb,
};

static void __init sm1_clkc_init(struct device_node *np)
{
	int ret = 0, clkid, i;
	struct clk *fclk_div2;

	if (!clk_base)
		clk_base = of_iomap(np, 0);

	if (!clk_base) {
		pr_err("%s: Unable to map clk base\n", __func__);
		return;
	}
	for (i = 0; i < ARRAY_SIZE(sm1_clk_plls); i++)
		sm1_clk_plls[i]->base = clk_base;

	sm1_dsu_pre_src_clk_mux0.reg = clk_base
		+ (unsigned long)sm1_dsu_pre_src_clk_mux0.reg;
	sm1_dsu_pre_src_clk_mux1.reg = clk_base
		+ (unsigned long)sm1_dsu_pre_src_clk_mux1.reg;
	sm1_dsu_clk_div0.reg = clk_base
		+ (unsigned long)sm1_dsu_clk_div0.reg;
	sm1_dsu_clk_div1.reg = clk_base
		+ (unsigned long)sm1_dsu_clk_div1.reg;
	sm1_dsu_pre_clk_mux0.reg = clk_base
		+ (unsigned long)sm1_dsu_pre_clk_mux0.reg;
	sm1_dsu_pre_clk_mux1.reg = clk_base
		+ (unsigned long)sm1_dsu_pre_clk_mux1.reg;
	sm1_dsu_pre_post_clk_mux.reg = clk_base
		+ (unsigned long)sm1_dsu_pre_post_clk_mux.reg;
	sm1_dsu_pre_clk.reg = clk_base
		+ (unsigned long)sm1_dsu_pre_clk.reg;
	sm1_dsu_clk.reg = clk_base
		+ (unsigned long)sm1_dsu_clk.reg;
	cts_vipnanoq_core_clk_mux.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_core_clk_mux.reg);
	cts_vipnanoq_core_clk_gate.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_core_clk_gate.reg);
	cts_vipnanoq_core_clk_div.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_core_clk_div.reg);
	cts_vipnanoq_axi_clk_mux.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_axi_clk_mux.reg);
	cts_vipnanoq_axi_clk_gate.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_axi_clk_gate.reg);
	cts_vipnanoq_axi_clk_div.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_axi_clk_div.reg);

	cts_mipi_csi_phy_clk_mux.reg = clk_base
		+ (unsigned long)(cts_mipi_csi_phy_clk_mux.reg);
	cts_mipi_csi_phy_clk_div.reg = clk_base
		+ (unsigned long)(cts_mipi_csi_phy_clk_div.reg);
	cts_mipi_csi_phy_clk_gate.reg = clk_base
		+ (unsigned long)(cts_mipi_csi_phy_clk_gate.reg);
	cts_csi_adapt_clk_mux.reg = clk_base
		+ (unsigned long)(cts_csi_adapt_clk_mux.reg);
	cts_csi_adapt_clk_div.reg = clk_base
		+ (unsigned long)(cts_csi_adapt_clk_div.reg);
	cts_csi_adapt_clk_gate.reg = clk_base
		+ (unsigned long)(cts_csi_adapt_clk_gate.reg);

	/* Populate base address for gates */
	for (i = 0; i < ARRAY_SIZE(sm1_clk_gates); i++)
		sm1_clk_gates[i]->reg = clk_base +
			(unsigned long)sm1_clk_gates[i]->reg;
	if (!clks) {
		clks = kzalloc(NR_CLKS*sizeof(struct clk *), GFP_KERNEL);
		if (!clks) {
			pr_err("%s: alloc clks fail!", __func__);
			return;
		}
		clk_numbers = NR_CLKS;
	}

	/*
	 * register all clks
	 */
	for (clkid = 0; clkid < ARRAY_SIZE(sm1_clk_hws); clkid++) {
		if (sm1_clk_hws[clkid]) {
			clks[clkid + CLKID_SM1_ADD_BASE]
				= clk_register(NULL, sm1_clk_hws[clkid]);
			if (IS_ERR(clks[clkid + CLKID_SM1_ADD_BASE])) {
				pr_err("%s: failed to register %s\n", __func__,
					clk_hw_get_name(sm1_clk_hws[clkid]));
				goto iounmap;
			}
		}
	}
	clks[CLKID_VNANOQ_CORE_CLK_COMP] = clk_register_composite(NULL,
		"cts_vipnanoq_core_clk_composite",
		media_parent_names, 8,
		&cts_vipnanoq_core_clk_mux.hw,
		&clk_mux_ops,
		&cts_vipnanoq_core_clk_div.hw,
		&clk_divider_ops,
		&cts_vipnanoq_core_clk_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VNANOQ_CORE_CLK_COMP]))
		panic("%s: %d register cts_vipnanoq_core_clk_composite error\n",
			__func__, __LINE__);

	clks[CLKID_VNANOQ_AXI_CLK_COMP] = clk_register_composite(NULL,
		"cts_vipnanoq_axi_clk_composite",
		media_parent_names, 8,
		&cts_vipnanoq_axi_clk_mux.hw,
		&clk_mux_ops,
		&cts_vipnanoq_axi_clk_div.hw,
		&clk_divider_ops,
		&cts_vipnanoq_axi_clk_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VNANOQ_AXI_CLK_COMP]))
		panic("%s: %d register cts_vipnanoq_axi_clk_composite error\n",
			__func__, __LINE__);

	clks[CLKID_MIPI_CSI_PHY_CLK_COMP] = clk_register_composite(NULL,
		"cts_csi_phy_clk_composite",
		media_parent_names_mipi, 8,
		&cts_mipi_csi_phy_clk_mux.hw,
		&clk_mux_ops,
		&cts_mipi_csi_phy_clk_div.hw,
		&clk_divider_ops,
		&cts_mipi_csi_phy_clk_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_MIPI_CSI_PHY_CLK_COMP]))
		panic("%s: %d register cts_csi_phy_clk_composite error\n",
			__func__, __LINE__);

	clks[CLKID_CSI_ADAPT_CLK_COMP] = clk_register_composite(NULL,
		"cts_csi_adapt_clk_composite",
		media_parent_names_adapt, 8,
		&cts_csi_adapt_clk_mux.hw,
		&clk_mux_ops,
		&cts_csi_adapt_clk_div.hw,
		&clk_divider_ops,
		&cts_csi_adapt_clk_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_CSI_ADAPT_CLK_COMP]))
		panic("%s: %d register cts_csi_adapt_clk_composite error\n",
			__func__, __LINE__);

	/* set tl1_dsu_fixed_sel1 to 1G (default 24M) */
	fclk_div2 = of_clk_get_by_name(np, "clkin0");
	if (IS_ERR(fclk_div2)) {
		pr_err("faied to get clkin0\n");
		return;
	}

	ret = clk_set_parent(sm1_dsu_pre_src_clk_mux1.hw.clk, fclk_div2);
	if (ret < 0) {
		pr_err("%s: failed to set parent for tl1_dsu_fixed_source_sel1\n",
		__func__);
		return;
	}

	if (of_property_read_bool(np, "own-dsu-clk")) {
		/*
		 * when change sm1_dsu_pre_clk_mux0, switch to
		 * sm1_dsu_pre_clk_mux1 to avoid crash
		 */
		ret = clk_notifier_register(sm1_dsu_pre_clk_mux0.hw.clk,
					&sm1_dsu_nb_data.nb);
		if (ret) {
			pr_err("%s: failed to register clock notifier for cpu_clk\n",
			__func__);
			goto iounmap;
		}
	}
	pr_err("sm1 clk probe ok\n");
	return;

iounmap:
	iounmap(clk_base);
	pr_info("%s: %d: ret: %d\n", __func__, __LINE__, ret);
}

CLK_OF_DECLARE(sm1, "amlogic,sm1-clkc-2", sm1_clkc_init);
