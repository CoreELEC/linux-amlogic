/*
 * drivers/amlogic/clk/sc2/sc2.c
 *
 * Copyright (C) 2020 Amlogic, Inc. All rights reserved.
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
#include <linux/delay.h>
#include <dt-bindings/clock/amlogic,sc2-clkc.h>

#include <linux/amlogic/cpu_version.h>
#include "../clkc.h"
#include "../clk-secure.h"
#include "../clk-dualdiv.h"
#include "sc2.h"

static struct clk_onecell_data clk_data;

#define MESON_SC2_XTAL_GATE(_name, _reg, _bit)				\
struct clk_gate _name = {						\
	.reg = (void __iomem *)_reg,					\
	.bit_idx = (_bit),						\
	.lock = &clk_lock,						\
	.hw.init = &(struct clk_init_data) {				\
		.name = #_name,						\
		.ops = &clk_secure_gate_ops,				\
		.parent_names = (const char *[]){ "xtal" },		\
		.num_parents = 1,					\
		.flags = (CLK_IS_CRITICAL | CLK_IGNORE_UNUSED),		\
	},								\
}

/*CLKCTRL_OSCIN_CTRL*/
static MESON_SC2_XTAL_GATE(sc2_xtal_ddr_pll, CLKCTRL_OSCIN_CTRL, 1);
/*PLL_TOP(FIXPLL/HIFIPLL/SYSPLL/GP0PLL)*/
static MESON_SC2_XTAL_GATE(sc2_xtal_pll_top, CLKCTRL_OSCIN_CTRL, 4);
static MESON_SC2_XTAL_GATE(sc2_xtal_hdmi_pll, CLKCTRL_OSCIN_CTRL, 5);
static MESON_SC2_XTAL_GATE(sc2_xtal_usb_pll0, CLKCTRL_OSCIN_CTRL, 6);
static MESON_SC2_XTAL_GATE(sc2_xtal_usb_pll1, CLKCTRL_OSCIN_CTRL, 7);
static MESON_SC2_XTAL_GATE(sc2_xtal_usb_pcie_ctrl, CLKCTRL_OSCIN_CTRL, 9);
static MESON_SC2_XTAL_GATE(sc2_xtal_eth_pll, CLKCTRL_OSCIN_CTRL, 10);
static MESON_SC2_XTAL_GATE(sc2_xtal_pcie_refclk_pll, CLKCTRL_OSCIN_CTRL, 11);
static MESON_SC2_XTAL_GATE(sc2_xtal_earc, CLKCTRL_OSCIN_CTRL, 12);

/*fixed pll*/
static struct meson_clk_pll sc2_fixed_pll = {
	.m = {
		.reg_off = ANACTRL_FIXPLL_CTRL0,
		.shift   = 0,
		.width   = 8,
	},
	.n = {
		.reg_off = ANACTRL_FIXPLL_CTRL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = ANACTRL_FIXPLL_CTRL0,
		.shift   = 16,
		.width   = 2,
	},
	.frac = {
		.reg_off = ANACTRL_FIXPLL_CTRL1,
		.shift   = 0,
		.width   = 19,
	},
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &meson_sc2_pll_ro_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

/*sys_pll*/
static struct meson_clk_pll sc2_sys_pll = {
	.m = {
		.reg_off = ANACTRL_SYSPLL_CTRL0,
		.shift   = 0,
		.width   = 8,
	},
	.n = {
		.reg_off = ANACTRL_SYSPLL_CTRL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = ANACTRL_SYSPLL_CTRL0,
		.shift   = 16,
		.width   = 3,
	},
	.rate_table = sc2_pll_rate_table,
	.rate_count = ARRAY_SIZE(sc2_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_sc2_secure_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*gp0_pll*/
static struct meson_clk_pll sc2_gp0_pll = {
	.m = {
		.reg_off = ANACTRL_GP0PLL_CTRL0,
		.shift   = 0,
		.width   = 8,
	},
	.n = {
		.reg_off = ANACTRL_GP0PLL_CTRL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = ANACTRL_GP0PLL_CTRL0,
		.shift   = 16,
		.width   = 3,
	},
	.rate_table = sc2_pll_rate_table,
	.rate_count = ARRAY_SIZE(sc2_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_sc2_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*gp1_pll*/
static struct meson_clk_pll sc2_gp1_pll = {
	.m = {
		.reg_off = ANACTRL_GP1PLL_CTRL0,
		.shift   = 0,
		.width   = 8,
	},
	.n = {
		.reg_off = ANACTRL_GP1PLL_CTRL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = ANACTRL_GP1PLL_CTRL0,
		.shift   = 16,
		.width   = 3,
	},
	.rate_table = sc2_pll_rate_table,
	.rate_count = ARRAY_SIZE(sc2_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_sc2_secure_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*hifi_pll*/
static struct meson_clk_pll sc2_hifi_pll = {
	.m = {
		.reg_off = ANACTRL_HIFIPLL_CTRL0,
		.shift   = 0,
		.width   = 8,
	},
	.n = {
		.reg_off = ANACTRL_HIFIPLL_CTRL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = ANACTRL_HIFIPLL_CTRL0,
		.shift   = 16,
		.width   = 2,
	},
	.frac = {
		.reg_off = ANACTRL_HIFIPLL_CTRL1,
		.shift   = 0,
		.width   = 19,
	},
	.rate_table = sc2_hifi_pll_rate_table,
	.rate_count = ARRAY_SIZE(sc2_hifi_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &meson_sc2_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*pcie_pll*/
static struct meson_clk_pll sc2_pcie_pll = {
	.m = {
		.reg_off = ANACTRL_PCIEPLL_CTRL0,
		.shift   = 0,
		.width   = 8,
	},
	.n = {
		.reg_off = ANACTRL_PCIEPLL_CTRL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = ANACTRL_PCIEPLL_CTRL0,
		.shift   = 16,
		.width   = 5,
	},

	.frac = {
		.reg_off = ANACTRL_PCIEPLL_CTRL1,
		.shift   = 0,
		.width   = 12,
	},
	.rate_table = sc2_pcie_pll_rate_table,
	.rate_count = ARRAY_SIZE(sc2_pcie_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll",
		.ops = &meson_sc2_pcie_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate sc2_pcie_hcsl = {
	.reg = (void *)ANACTRL_PCIEPLL_CTRL5,
	.bit_idx = 3,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_hcsl",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "pcie_pll" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE),
	},
};

static struct clk_fixed_factor sc2_fclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor sc2_fclk_div3 = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor sc2_fclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor sc2_fclk_div5 = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor sc2_fclk_div7 = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor sc2_fclk_div2p5 = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_mpll sc2_mpll0 = {
	.sdm = {
		.reg_off = ANACTRL_MPLL_CTRL1,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = ANACTRL_MPLL_CTRL1,
		.shift   = 20,
		.width   = 9,
	},
	.sdm_en = 30,
	.en_dds = 31,
	.mpll_cntl0_reg = ANACTRL_MPLL_CTRL0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &meson_sc2_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL),
	},
};

static struct meson_clk_mpll sc2_mpll1 = {
	.sdm = {
		.reg_off = ANACTRL_MPLL_CTRL3,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = ANACTRL_MPLL_CTRL3,
		.shift   = 20,
		.width   = 9,
	},
	.sdm_en = 30,
	.en_dds = 31,
	.mpll_cntl0_reg = ANACTRL_MPLL_CTRL0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &meson_sc2_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_mpll sc2_mpll2 = {
	.sdm = {
		.reg_off = ANACTRL_MPLL_CTRL5,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = ANACTRL_MPLL_CTRL5,
		.shift   = 20,
		.width   = 9,
	},
	.sdm_en = 30,
	.en_dds = 31,
	.mpll_cntl0_reg = ANACTRL_MPLL_CTRL0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &meson_sc2_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_mpll sc2_mpll3 = {
	.sdm = {
		.reg_off = ANACTRL_MPLL_CTRL7,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = ANACTRL_MPLL_CTRL7,
		.shift   = 20,
		.width   = 9,
	},
	.sdm_en = 30,
	.en_dds = 31,
	.mpll_cntl0_reg = ANACTRL_MPLL_CTRL0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &meson_sc2_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*cpu_clk*/
static u32 mux_table_cpu_p[]	= { 0, 1, 2 };

static struct meson_cpu_mux_divider sc2_cpu_fclk_p = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL,
	.cpu_fclk_p00 = {
		.mask = 0x3,
		.shift = 0,
		.width = 2,
	},
	.cpu_fclk_p0 = {
		.mask = 0x1,
		.shift = 2,
		.width = 1,
	},
	.cpu_fclk_p10 = {
		.mask = 0x3,
		.shift = 16,
		.width = 2,
	},
	.cpu_fclk_p1 = {
		.mask = 0x1,
		.shift = 18,
		.width = 1,
	},
	.cpu_fclk_p = {
		.mask = 0x1,
		.shift = 10,
		.width = 1,
	},
	.cpu_fclk_p01 = {
		.shift = 4,
		.width = 6,
	},
	.cpu_fclk_p11 = {
		.shift = 20,
		.width = 6,
	},
	.table = mux_table_cpu_p,
	.rate_table = fclk_pll_rate_table,
	.rate_count = ARRAY_SIZE(fclk_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_fixedpll_p",
		.ops = &sc2_secure_dsu_cpu_clk_ops,
		.parent_names = (const char *[]){ "xtal", "fclk_div2",
			"fclk_div3"},
		.num_parents = 3,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux sc2_cpu_clk = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL,
	.mask = 0x1,
	.shift = 11,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk",
		.ops = &sc2_clk_mux_ops,
		.parent_names = (const char *[]){ "cpu_fixedpll_p",
				"sys_pll" },
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*cpu1_clk*/
static struct clk_mux sc2_cpu1_clk = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL6,
	.mask = 0x1,
	.shift = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu1_clk",
		.ops = &sc2_clk_mux_ro_ops,
		/* This CPU also have a dedicated clock tree */
		.parent_names = (const char *[]){ "cpu_clk" },
		.num_parents = 1,
	},
};

/*cpu2_clk*/
static struct clk_mux sc2_cpu2_clk = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL6,
	.mask = 0x1,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu2_clk",
		.ops = &sc2_clk_mux_ro_ops,
		/* This CPU also have a dedicated clock tree */
		.parent_names = (const char *[]){ "cpu_clk" },
		.num_parents = 1,
	},
};

/*cpu3_clk*/
static struct clk_mux sc2_cpu3_clk = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL6,
	.mask = 0x1,
	.shift = 26,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu3_clk",
		.ops = &sc2_clk_mux_ro_ops,
		/* This CPU also have a dedicated clock tree */
		.parent_names = (const char *[]){ "cpu_clk" },
		.num_parents = 1,
	},
};

/*dsu_clk*/
static struct clk_mux sc2_dsu_pre_src_clk_mux0 = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL5,
	.mask = 0x3,
	.shift = 0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre_src0",
		.ops = &sc2_clk_mux_ops,
		.parent_names = (const char *[]){ "xtal", "fclk_div2",
				"fclk_div3", "gp1_pll" },
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_mux sc2_dsu_pre_src_clk_mux1 = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL5,
	.mask = 0x3,
	.shift = 16,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre_src1",
		.ops = &sc2_clk_mux_ops,
		.parent_names = (const char *[]){ "xtal", "fclk_div2",
				"fclk_div3", "gp1_pll" },
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_divider sc2_dsu_clk_div0 = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL5,
	.shift = 4,
	.width = 6,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_div0",
		.ops = &sc2_clk_divider_ops,
		.parent_names = (const char *[]){ "dsu_pre_src0" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_divider sc2_dsu_clk_div1 = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL5,
	.shift = 20,
	.width = 6,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_div1",
		.ops = &sc2_clk_divider_ops,
		.parent_names = (const char *[]){ "dsu_pre_src1" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_dsu_pre_clk_mux0 = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL5,
	.mask = 0x1,
	.shift = 2,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre0",
		.ops = &sc2_clk_mux_ops,
		.parent_names = (const char *[]){ "dsu_pre_src0",
						"dsu_clk_div0",},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_dsu_pre_clk_mux1 = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL5,
	.mask = 0x1,
	.shift = 18,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre1",
		.ops = &sc2_clk_mux_ops,
		.parent_names = (const char *[]){ "dsu_pre_src1",
						"dsu_clk_div1",},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_dsu_pre_post_clk_mux = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL5,
	.mask = 0x1,
	.shift = 10,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre_post",
		.ops = &sc2_clk_mux_ops,
		.parent_names = (const char *[]){ "dsu_pre0",
						"dsu_pre1",},
		.num_parents = 2,
		/*.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,*/
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_dsu_pre_clk = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL5,
	.mask = 0x1,
	.shift = 11,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_pre_clk",
		.ops = &sc2_clk_mux_ops,
		.parent_names = (const char *[]){ "dsu_pre_post",
						"sys_pll",},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_mux sc2_dsu_clk = {
	.reg = (void *)CPUCTRL_SYS_CPU_CLK_CTRL6,
	.mask = 0x1,
	.shift = 27,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk",
		.ops = &sc2_clk_mux_ops,
		.parent_names = (const char *[]){ "cpu_clk",
						"dsu_pre_clk",},
		.num_parents = 2,
	},
};

/*sys clk*/
/* sys_clk parrent skip
 * 5:axi_clk_frcpu
 * 7:cts_rtc_clk
 */
static const char * const sysclk_parent_names[] = { "xtal", "fclk_div2",
	"fclk_div3", "fclk_div4", "fclk_div5", "null", "null", "null"};

static struct clk_mux sc2_sysclk_b_sel = {
	.reg = (void *)CLKCTRL_SYS_CLK_CTRL0,
	.mask = 0x7,
	.shift = 26,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_b_sel",
		.ops = &clk_mux_ro_ops,
		.parent_names = sysclk_parent_names,
		.num_parents = ARRAY_SIZE(sysclk_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_sysclk_b_div = {
	.reg = (void *)CLKCTRL_SYS_CLK_CTRL0,
	.shift = 16,
	.width = 10,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "sysclk_b_sel" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_sysclk_b_gate = {
	.reg = (void *)CLKCTRL_SYS_CLK_CTRL0,
	.bit_idx = 29,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "sysclk_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_sysclk_a_sel = {
	.reg = (void *)CLKCTRL_SYS_CLK_CTRL0,
	.mask = 0x7,
	.shift = 10,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_a_sel",
		.ops = &clk_mux_ro_ops,
		.parent_names = sysclk_parent_names,
		.num_parents = ARRAY_SIZE(sysclk_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_sysclk_a_div = {
	.reg = (void *)CLKCTRL_SYS_CLK_CTRL0,
	.shift = 0,
	.width = 10,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "sysclk_a_sel" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_sysclk_a_gate = {
	.reg = (void *)CLKCTRL_SYS_CLK_CTRL0,
	.bit_idx = 13,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "sysclk_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_sysclk_sel = {
	.reg = (void *)CLKCTRL_SYS_CLK_CTRL0,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_mux_ro_ops,
		.parent_names = (const char *[]){"sysclk_a_gate",
			"sysclk_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*
 *rtc 32k clock
 *
 *xtal--GATE------------------GATE---------------------|\
 *	              |  --------                      | \
 *	              |  |      |                      |  \
 *	              ---| DUAL |----------------------|   |
 *	                 |      |                      |   |____GATE__
 *	                 --------                      |   |     rtc_32k_out
 *	   PAD-----------------------------------------|  /
 *	                                               | /
 *	   DUAL function:                              |/
 *	   bit 28 in RTC_BY_OSCIN_CTRL0 control the dual function.
 *	   when bit 28 = 0
 *	         f = 24M/N0
 *	   when bit 28 = 1
 *	         output N1 and N2 in rurn.
 *	   T = (x*T1 + y*T2)/x+y
 *	   f = (24M/(N0*M0 + N1*M1)) * (M0 + M1)
 *	   f: the frequecy value (HZ)
 *	       |      | |      |
 *	       | Div1 |-| Cnt1 |
 *	      /|______| |______|\
 *	    -|  ______   ______  ---> Out
 *	      \|      | |      |/
 *	       | Div2 |-| Cnt2 |
 *	       |______| |______|
 **/

/*
 * rtc 32k clock in gate
 */
static struct clk_gate sc2_rtc_32k_clkin = {
	.reg = (void *)CLKCTRL_RTC_BY_OSCIN_CTRL0,
	.bit_idx = 31,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_clkin",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param clk_32k_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	},
	{}
};

static struct meson_dualdiv_clk sc2_rtc_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = clk_32k_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_names = (const char *[]){ "rtc_32k_clkin" },
		.num_parents = 1,
	},
};

static struct clk_gate sc2_rtc_32k_xtal = {
	.reg = (void *)CLKCTRL_RTC_BY_OSCIN_CTRL1,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_xtal",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "rtc_32k_c" },
		.num_parents = 1,
	},
};

/*
 * three parent for rtc clock out
 * pad is from where?
 */
static u32 rtc_32k_sel[] = { 0, 1};
static const char * const rtc_32k_sel_parent_names[] = {
	"rtc_32k_xtal", "rtc_32k_div", "pad"
};

static struct clk_mux sc2_rtc_32k_sel = {
	.reg = (void *)CLKCTRL_RTC_CTRL,
	.mask = 0x3,
	.shift = 0,
	.table = rtc_32k_sel,
	.flags = CLK_MUX_ROUND_CLOSEST,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_sel",
		.ops = &clk_mux_ops,
		.parent_names = rtc_32k_sel_parent_names,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_rtc_clk = {
	.reg = (void *)CLKCTRL_RTC_BY_OSCIN_CTRL0,
	.bit_idx = 30,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_clk",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "rtc_32k_s" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*axi clk*/
/* axi_clk parrent skip
 * 5:axi_clk_frcpu
 * 7:cts_rtc_clk
 */
static const char * const axiclk_parent_names[] = { "xtal", "fclk_div2",
	"fclk_div3", "fclk_div4", "fclk_div5", "null", "null", "null"};

static struct clk_mux sc2_axiclk_b_sel = {
	.reg = (void *)CLKCTRL_AXI_CLK_CTRL0,
	.mask = 0x7,
	.shift = 26,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "axiclk_b_sel",
		.ops = &clk_mux_ro_ops,
		.parent_names = axiclk_parent_names,
		.num_parents = ARRAY_SIZE(axiclk_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_axiclk_b_div = {
	.reg = (void *)CLKCTRL_AXI_CLK_CTRL0,
	.shift = 16,
	.width = 10,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "axiclk_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "axiclk_b_sel" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_axiclk_b_gate = {
	.reg = (void *)CLKCTRL_AXI_CLK_CTRL0,
	.bit_idx = 29,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "axiclk_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "axiclk_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_axiclk_a_sel = {
	.reg = (void *)CLKCTRL_AXI_CLK_CTRL0,
	.mask = 0x7,
	.shift = 10,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "axiclk_a_sel",
		.ops = &clk_mux_ro_ops,
		.parent_names = axiclk_parent_names,
		.num_parents = ARRAY_SIZE(axiclk_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_axiclk_a_div = {
	.reg = (void *)CLKCTRL_AXI_CLK_CTRL0,
	.shift = 0,
	.width = 10,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "axiclk_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "axiclk_a_sel" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_axiclk_a_gate = {
	.reg = (void *)CLKCTRL_AXI_CLK_CTRL0,
	.bit_idx = 13,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "axiclk_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "axiclk_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_axiclk_sel = {
	.reg = (void *)CLKCTRL_AXI_CLK_CTRL0,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "axi_clk",
		.ops = &clk_mux_ro_ops,
		.parent_names = (const char *[]){"axiclk_a_gate",
			"axiclk_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*ceca_clk*/
static struct clk_gate sc2_ceca_32k_clkin = {
	.reg = (void *)CLKCTRL_CECA_CTRL0,
	.bit_idx = 31,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "ceca_32k_clkin",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct meson_dualdiv_clk sc2_ceca_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_CECA_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_CECA_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_CECA_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_CECA_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_CECA_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = clk_32k_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_names = (const char *[]){ "ceca_32k_clkin" },
		.num_parents = 1,
	},
};

static struct clk_mux sc2_ceca_32k_sel_pre = {
	.reg = (void *)CLKCTRL_CECA_CTRL1,
	.mask = 0x1,
	.shift = 24,
	.flags = CLK_MUX_ROUND_CLOSEST,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_sel_pre",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "ceca_32k_div",
						"ceca_32k_clkin" },
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_ceca_32k_sel = {
	.reg = (void *)CLKCTRL_CECA_CTRL1,
	.mask = 0x1,
	.shift = 31,
	.flags = CLK_MUX_ROUND_CLOSEST,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_sel",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "ceca_32k_sel_pre",
						"rtc_clk" },
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_ceca_32k_clkout = {
	.reg = (void *)CLKCTRL_CECA_CTRL0,
	.bit_idx = 30,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "ceca_32k_clkout",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "ceca_32k_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*cecb_clk*/
static struct clk_gate sc2_cecb_32k_clkin = {
	.reg = (void *)CLKCTRL_CECB_CTRL0,
	.bit_idx = 31,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_32k_clkin",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct meson_dualdiv_clk sc2_cecb_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_CECB_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_CECB_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = clk_32k_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_names = (const char *[]){ "cecb_32k_clkin" },
		.num_parents = 1,
	},
};

static struct clk_mux sc2_cecb_32k_sel_pre = {
	.reg = (void *)CLKCTRL_CECB_CTRL1,
	.mask = 0x1,
	.shift = 24,
	.flags = CLK_MUX_ROUND_CLOSEST,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_sel_pre",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "cecb_32k_div",
						"cecb_32k_clkin" },
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_cecb_32k_sel = {
	.reg = (void *)CLKCTRL_CECB_CTRL1,
	.mask = 0x1,
	.shift = 31,
	.flags = CLK_MUX_ROUND_CLOSEST,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_sel",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "cecb_32k_sel_pre",
						"rtc_clk" },
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_cecb_32k_clkout = {
	.reg = (void *)CLKCTRL_CECA_CTRL0,
	.bit_idx = 30,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_32k_clkout",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cecb_32k_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*sc_clk*/
static struct clk_mux sc2_sc_clk_mux = {
	.reg = (void *)CLKCTRL_SC_CLK_CTRL,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sc_clk_mux",
		.ops = &clk_mux_ro_ops,
		.parent_names = (const char *[]){"fclk_div4",
			"fclk_div3", "fclk_div5", "xtal"},
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_sc_clk_div = {
	.reg = (void *)CLKCTRL_SC_CLK_CTRL,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "sc_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "sc_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_sc_clk_gate = {
	.reg = (void *)CLKCTRL_SC_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "sc_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "sc_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*rama_clk*/
/* rama_clk parent skip
 * 5:axi_clk_frcpu
 * 7:cts_rtc_clk
 */
static const char * const ramaclk_parent_names[] = { "xtal", "fclk_div2",
	"fclk_div3", "fclk_div4", "fclk_div5", "null",
	"fclk_div7", "null"};

static struct clk_mux sc2_ramaclk_b_sel = {
	.reg = (void *)CLKCTRL_RAMA_CLK_CTRL0,
	.mask = 0x7,
	.shift = 26,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "ramaclk_b_sel",
		.ops = &clk_mux_ro_ops,
		.parent_names = ramaclk_parent_names,
		.num_parents = ARRAY_SIZE(ramaclk_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_ramaclk_b_div = {
	.reg = (void *)CLKCTRL_RAMA_CLK_CTRL0,
	.shift = 16,
	.width = 10,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "ramaclk_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "ramaclk_b_sel" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_ramaclk_b_gate = {
	.reg = (void *)CLKCTRL_RAMA_CLK_CTRL0,
	.bit_idx = 29,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "ramaclk_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "ramaclk_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_ramaclk_a_sel = {
	.reg = (void *)CLKCTRL_RAMA_CLK_CTRL0,
	.mask = 0x7,
	.shift = 10,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "ramaclk_a_sel",
		.ops = &clk_mux_ro_ops,
		.parent_names = ramaclk_parent_names,
		.num_parents = ARRAY_SIZE(ramaclk_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_ramaclk_a_div = {
	.reg = (void *)CLKCTRL_RAMA_CLK_CTRL0,
	.shift = 0,
	.width = 10,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "ramaclk_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "ramaclk_a_sel" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_ramaclk_a_gate = {
	.reg = (void *)CLKCTRL_RAMA_CLK_CTRL0,
	.bit_idx = 13,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "ramaclk_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "ramaclk_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_ramaclk_sel = {
	.reg = (void *)CLKCTRL_RAMA_CLK_CTRL0,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "rama_clk",
		.ops = &clk_mux_ro_ops,
		.parent_names = (const char *[]){"ramaclk_a_gate",
			"ramaclk_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*dspa_clk*/
static const char * const dspaclk_parent_names[] = { "xtal", "fclk_div2p5",
	"fclk_div3", "fclk_div5", "hifi_pll", "fclk_div4",
	"fclk_div7", "null"};

static struct clk_mux sc2_dspaclk_b_sel = {
	.reg = (void *)CLKCTRL_DSPA_CLK_CTRL0,
	.mask = 0x7,
	.shift = 26,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dspaclk_b_sel",
		.ops = &clk_mux_ops,
		.parent_names = dspaclk_parent_names,
		.num_parents = ARRAY_SIZE(dspaclk_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_dspaclk_b_div = {
	.reg = (void *)CLKCTRL_DSPA_CLK_CTRL0,
	.shift = 16,
	.width = 10,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "dspaclk_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "dspaclk_b_sel" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_dspaclk_b_gate = {
	.reg = (void *)CLKCTRL_DSPA_CLK_CTRL0,
	.bit_idx = 29,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "dspaclk_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "dspaclk_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_dspaclk_a_sel = {
	.reg = (void *)CLKCTRL_DSPA_CLK_CTRL0,
	.mask = 0x7,
	.shift = 10,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dspaclk_a_sel",
		.ops = &clk_mux_ops,
		.parent_names = dspaclk_parent_names,
		.num_parents = ARRAY_SIZE(dspaclk_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_dspaclk_a_div = {
	.reg = (void *)CLKCTRL_DSPA_CLK_CTRL0,
	.shift = 0,
	.width = 10,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "dspaclk_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "dspaclk_a_sel" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_dspaclk_a_gate = {
	.reg = (void *)CLKCTRL_DSPA_CLK_CTRL0,
	.bit_idx = 13,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "dspaclk_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "dspaclk_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_dspaclk_sel = {
	.reg = (void *)CLKCTRL_DSPA_CLK_CTRL0,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dspa_clk",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){"dspaclk_a_gate",
			"dspaclk_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*12_24Mclk*/
static struct clk_gate sc2_24M_clk_gate = {
	.reg = (void *)CLKCTRL_CLK12_24_CTRL,
	.bit_idx = 11,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "24M",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor sc2_24M_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "24M_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "24M" },
		.num_parents = 1,
	},
};

static struct clk_gate sc2_12M_clk_gate = {
	.reg = (void *)CLKCTRL_CLK12_24_CTRL,
	.bit_idx = 10,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "12M",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "24M_div2" },
		.num_parents = 1,
	},
};

static struct clk_divider sc2_25M_clk_div = {
	.reg = (void *)CLKCTRL_CLK12_24_CTRL,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "25M_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "fclk_div2" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate sc2_25M_clk_gate = {
	.reg = (void *)CLKCTRL_CLK12_24_CTRL,
	.bit_idx = 12,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "25M",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "25M_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*vid_clk*/

/*hdmi_clk*/
static struct clk_mux sc2_hdmiclk_mux = {
	.reg = (void *)CLKCTRL_HDMI_CLK_CTRL,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hdmiclk_mux",
		.ops = &clk_mux_ro_ops,
		.parent_names =  (const char *[]){"xtal", "fclk_div4",
			"fclk_div3", "fclk_div5"},
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_divider sc2_hdmiclk_div = {
	.reg = (void *)CLKCTRL_HDMI_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "hdmiclk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hdmiclk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_gate sc2_hdmiclk_gate = {
	.reg = (void *)CLKCTRL_HDMI_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hdmiclk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hdmiclk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

/*vid_lock_clk*/

/*eth_clk :125M*/

/*ts_clk*/
static struct clk_divider sc2_ts_clk_div = {
	.reg = (void *)CLKCTRL_TS_CLK_CTRL,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate sc2_ts_clk_gate = {
	.reg = (void *)CLKCTRL_TS_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "ts_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*mali_clk*/
const char *sc2_gpu_parent_names[] = { "xtal", "gp0_pll", "hifi_pll",
	"fclk_div2p5", "fclk_div3", "fclk_div4", "fclk_div5", "fclk_div7"};

static struct clk_mux sc2_gpu_a_mux = {
	.reg = (void *)CLKCTRL_MALI_CLK_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_a_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_gpu_parent_names,
		.num_parents = ARRAY_SIZE(sc2_gpu_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_gpu_a_div = {
	.reg = (void *)CLKCTRL_MALI_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "gpu_a_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_gpu_a_gate = {
	.reg = (void *)CLKCTRL_MALI_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "gpu_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "gpu_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
			| CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_gpu_b_mux = {
	.reg = (void *)CLKCTRL_MALI_CLK_CTRL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_b_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_gpu_parent_names,
		.num_parents = ARRAY_SIZE(sc2_gpu_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_gpu_b_div = {
	.reg = (void *)CLKCTRL_MALI_CLK_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "gpu_b_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_gpu_b_gate = {
	.reg = (void *)CLKCTRL_MALI_CLK_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "gpu_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "gpu_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
			| CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_gpu_mux = {
	.reg = (void *)CLKCTRL_MALI_CLK_CTRL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "gpu_a_gate",
			"gpu_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_UNGATE,
	},
};

/* cts_vdec_clk */
const char *sc2_dec_parent_names[] = { "fclk_div2p5", "fclk_div3",
	"fclk_div4", "fclk_div5", "fclk_div7", "hifi_pll", "gp0_pll", "xtal"};

static struct clk_mux sc2_vdec_a_mux = {
	.reg = (void *)CLKCTRL_VDEC_CLK_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_a_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_dec_parent_names,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_vdec_a_div = {
	.reg = (void *)CLKCTRL_VDEC_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vdec_a_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vdec_a_gate = {
	.reg = (void *)CLKCTRL_VDEC_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vdec_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
			| CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_vdec_b_mux = {
	.reg = (void *)CLKCTRL_VDEC3_CLK_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_b_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_dec_parent_names,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_vdec_b_div = {
	.reg = (void *)CLKCTRL_VDEC3_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vdec_b_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vdec_b_gate = {
	.reg = (void *)CLKCTRL_VDEC3_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vdec_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
			| CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_vdec_mux = {
	.reg = (void *)CLKCTRL_VDEC3_CLK_CTRL,
	.mask = 0x1,
	.shift = 15,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "vdec_a_gate",
			"vdec_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_UNGATE,
	},
};

/* cts_hcodec_clk */
static struct clk_mux sc2_hcodec_a_mux = {
	.reg = (void *)CLKCTRL_VDEC_CLK_CTRL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_a_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_dec_parent_names,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_hcodec_a_div = {
	.reg = (void *)CLKCTRL_VDEC_CLK_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hcodec_a_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_hcodec_a_gate = {
	.reg = (void *)CLKCTRL_VDEC_CLK_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hcodec_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
			| CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_hcodec_b_mux = {
	.reg = (void *)CLKCTRL_VDEC3_CLK_CTRL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_b_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_dec_parent_names,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_hcodec_b_div = {
	.reg = (void *)CLKCTRL_VDEC3_CLK_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hcodec_b_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_hcodec_b_gate = {
	.reg = (void *)CLKCTRL_VDEC3_CLK_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hcodec_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
			| CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_hcodec_mux = {
	.reg = (void *)CLKCTRL_VDEC3_CLK_CTRL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "hcodec_a_gate",
			"hcodec_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_UNGATE,
	},
};

/* cts_hevcb_clk */
static struct clk_mux sc2_hevcb_a_mux = {
	.reg = (void *)CLKCTRL_VDEC2_CLK_CTRL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_a_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_dec_parent_names,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_hevcb_a_div = {
	.reg = (void *)CLKCTRL_VDEC2_CLK_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hevcb_a_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_hevcb_a_gate = {
	.reg = (void *)CLKCTRL_VDEC2_CLK_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hevcb_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
			| CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_hevcb_b_mux = {
	.reg = (void *)CLKCTRL_VDEC4_CLK_CTRL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_b_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_dec_parent_names,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_hevcb_b_div = {
	.reg = (void *)CLKCTRL_VDEC4_CLK_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hevcb_b_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_hevcb_b_gate = {
	.reg = (void *)CLKCTRL_VDEC4_CLK_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hevcb_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
			| CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_hevcb_mux = {
	.reg = (void *)CLKCTRL_VDEC4_CLK_CTRL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "hevcb_a_gate",
			"hevcb_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_UNGATE,
	},
};

/* cts_hevcf_clk */
static struct clk_mux sc2_hevcf_a_mux = {
	.reg = (void *)CLKCTRL_VDEC2_CLK_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_a_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_dec_parent_names,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_hevcf_a_div = {
	.reg = (void *)CLKCTRL_VDEC2_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hevcf_a_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_hevcf_a_gate = {
	.reg = (void *)CLKCTRL_VDEC2_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hevcf_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_hevcf_b_mux = {
	.reg = (void *)CLKCTRL_VDEC4_CLK_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_b_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_dec_parent_names,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_hevcf_b_div = {
	.reg = (void *)CLKCTRL_VDEC4_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hevcf_b_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_hevcf_b_gate = {
	.reg = (void *)CLKCTRL_VDEC4_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hevcf_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_hevcf_mux = {
	.reg = (void *)CLKCTRL_VDEC4_CLK_CTRL,
	.mask = 0x1,
	.shift = 15,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "hevcf_a_gate",
			"hevcf_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_UNGATE,
	},
};

/*cts_wave420l_a/b/c_clk*/
const char *sc2_wave_parent_names[] = { "xtal", "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "mpll2", "mpll3", "gp0_pll"};

static struct clk_mux sc2_wave_a_mux = {
	.reg = (void *)CLKCTRL_WAVE420L_CLK_CTRL2,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "wave_a_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_wave_parent_names,
		.num_parents = ARRAY_SIZE(sc2_wave_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_wave_a_div = {
	.reg = (void *)CLKCTRL_WAVE420L_CLK_CTRL2,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "wave_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "wave_a_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_wave_a_gate = {
	.reg = (void *)CLKCTRL_WAVE420L_CLK_CTRL2,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "wave_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "wave_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_wave_b_mux = {
	.reg = (void *)CLKCTRL_WAVE420L_CLK_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "wave_b_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_wave_parent_names,
		.num_parents = ARRAY_SIZE(sc2_wave_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_wave_b_div = {
	.reg = (void *)CLKCTRL_WAVE420L_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "wave_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "wave_b_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_wave_b_gate = {
	.reg = (void *)CLKCTRL_WAVE420L_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "wave_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "wave_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_wave_c_mux = {
	.reg = (void *)CLKCTRL_WAVE420L_CLK_CTRL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "wave_c_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_wave_parent_names,
		.num_parents = ARRAY_SIZE(sc2_wave_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_wave_c_div = {
	.reg = (void *)CLKCTRL_WAVE420L_CLK_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "wave_c_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "wave_c_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_wave_c_gate = {
	.reg = (void *)CLKCTRL_WAVE420L_CLK_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "wave_c_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "wave_c_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_vpu_clk */
static const char * const sc2_vpu_parent_names[] = { "fclk_div3",
	"fclk_div4", "fclk_div5", "fclk_div7", "null", "null",
	"null",  "null"};

static struct clk_mux sc2_vpu_a_mux = {
	.reg = (void *)CLKCTRL_VPU_CLK_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_a_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_vpu_parent_names,
		.num_parents = ARRAY_SIZE(sc2_vpu_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_vpu_a_div = {
	.reg = (void *)CLKCTRL_VPU_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_a_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vpu_a_gate = {
	.reg = (void *)CLKCTRL_VPU_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_vpu_b_mux = {
	.reg = (void *)CLKCTRL_VPU_CLK_CTRL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_b_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_vpu_parent_names,
		.num_parents = ARRAY_SIZE(sc2_vpu_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_vpu_b_div = {
	.reg = (void *)CLKCTRL_VPU_CLK_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_b_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vpu_b_gate = {
	.reg = (void *)CLKCTRL_VPU_CLK_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_vpu_mux = {
	.reg = (void *)CLKCTRL_VPU_CLK_CTRL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "vpu_a_gate",
			"vpu_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_UNGATE,
	},
};

/*cts_vpu_clkb*/
static const char * const sc2_vpu_clkb_parent_names[] = { "vpu_mux",
	"fclk_div4", "fclk_div5", "fclk_div7"};

static struct clk_mux sc2_vpu_clkb_tmp_mux = {
	.reg = (void *)CLKCTRL_VPU_CLKB_CTRL,
	.mask = 0x3,
	.shift = 20,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_vpu_clkb_parent_names,
		.num_parents = ARRAY_SIZE(sc2_vpu_clkb_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_vpu_clkb_tmp_div = {
	.reg = (void *)CLKCTRL_VPU_CLKB_CTRL,
	.shift = 16,
	.width = 4,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_clkb_tmp_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vpu_clkb_tmp_gate = {
	.reg = (void *)CLKCTRL_VPU_CLKB_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_clkb_tmp_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_divider sc2_vpu_clkb_div = {
	.reg = (void *)CLKCTRL_VPU_CLKB_CTRL,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_clkb_tmp_gate" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vpu_clkb_gate = {
	.reg = (void *)CLKCTRL_VPU_CLKB_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_clkb_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_vpu_clkc*/
static const char * const sc2_vpu_clkc_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "mpll1", "null",
	"mpll2",  "fclk_div2p5"};

static struct clk_mux sc2_vpu_clkc_a_mux = {
	.reg = (void *)CLKCTRL_VPU_CLKC_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_a_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_vpu_clkc_parent_names,
		.num_parents = ARRAY_SIZE(sc2_vpu_clkc_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_vpu_clkc_a_div = {
	.reg = (void *)CLKCTRL_VPU_CLKC_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_clkc_a_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vpu_clkc_a_gate = {
	.reg = (void *)CLKCTRL_VPU_CLKC_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_clkc_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_vpu_clkc_b_mux = {
	.reg = (void *)CLKCTRL_VPU_CLKC_CTRL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_b_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_vpu_clkc_parent_names,
		.num_parents = ARRAY_SIZE(sc2_vpu_clkc_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_vpu_clkc_b_div = {
	.reg = (void *)CLKCTRL_VPU_CLKC_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_clkc_b_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vpu_clkc_b_gate = {
	.reg = (void *)CLKCTRL_VPU_CLKC_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_clkc_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_vpu_clkc_mux = {
	.reg = (void *)CLKCTRL_VPU_CLKC_CTRL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "vpu_clkc_a_gate",
			"vpu_clkc_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_UNGATE,
	},
};

/*cts_vapb_clk*/
static const char * const sc2_vapb_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "mpll1", "null",
	"mpll2",  "fclk_div2p5"};

static struct clk_mux sc2_vapb_a_mux = {
	.reg = (void *)CLKCTRL_VAPBCLK_CTRL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_a_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_vapb_parent_names,
		.num_parents = ARRAY_SIZE(sc2_vapb_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_vapb_a_div = {
	.reg = (void *)CLKCTRL_VAPBCLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_a_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vapb_a_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vapb_a_gate = {
	.reg = (void *)CLKCTRL_VAPBCLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_a_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vapb_a_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_vapb_b_mux = {
	.reg = (void *)CLKCTRL_VAPBCLK_CTRL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_b_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_vapb_parent_names,
		.num_parents = ARRAY_SIZE(sc2_vapb_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_vapb_b_div = {
	.reg = (void *)CLKCTRL_VAPBCLK_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_b_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vapb_b_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vapb_b_gate = {
	.reg = (void *)CLKCTRL_VAPBCLK_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_b_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vapb_b_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			CLK_IS_CRITICAL,
	},
};

static struct clk_mux sc2_vapb_mux = {
	.reg = (void *)CLKCTRL_VAPBCLK_CTRL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "vapb_a_gate",
			"vapb_b_gate"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_UNGATE,
	},
};

/*cts_ge2d_clk*/
static struct clk_gate sc2_ge2d_gate = {
	.reg = (void *)CLKCTRL_VAPBCLK_CTRL,
	.bit_idx = 30,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vapb_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*cts_hdcp22_esmclk*/
static struct clk_mux sc2_hdcp22_esmclk_mux = {
	.reg = (void *)CLKCTRL_HDCP22_CTRL,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hdcp22_esmclk_mux",
		.ops = &clk_mux_ro_ops,
		.parent_names =  (const char *[]){"fclk_div7",
			"fclk_div4", "fclk_div3", "fclk_div5"},
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_hdcp22_esmclk_div = {
	.reg = (void *)CLKCTRL_HDCP22_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "hdcp22_esmclk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hdcp22_esmclk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_hdcp22_esmclk_gate = {
	.reg = (void *)CLKCTRL_HDCP22_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_esmclk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hdcp22_esmclk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_hdcp22_skpclk*/
static struct clk_mux sc2_hdcp22_skpclk_mux = {
	.reg = (void *)CLKCTRL_HDCP22_CTRL,
	.mask = 0x3,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hdcp22_skpclk_mux",
		.ops = &clk_mux_ro_ops,
		.parent_names =  (const char *[]){"xtal",
			"fclk_div4", "fclk_div3", "fclk_div5"},
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_hdcp22_skpclk_div = {
	.reg = (void *)CLKCTRL_HDCP22_CTRL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "hdcp22_skpclk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hdcp22_skpclk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_hdcp22_skpclk_gate = {
	.reg = (void *)CLKCTRL_HDCP22_CTRL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_skpclk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hdcp22_skpclk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_vdin_meas_clk*/
static struct clk_mux sc2_vdin_meas_clk_mux = {
	.reg = (void *)CLKCTRL_VDIN_MEAS_CLK_CTRL,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_clk_mux",
		.ops = &clk_mux_ro_ops,
		.parent_names =  (const char *[]){"xtal", "fclk_div4",
			"fclk_div3", "fclk_div5"},
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_vdin_meas_clk_div = {
	.reg = (void *)CLKCTRL_VDIN_MEAS_CLK_CTRL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vdin_meas_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_vdin_meas_clk_gate = {
	.reg = (void *)CLKCTRL_VDIN_MEAS_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vdin_meas_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_cdac_clk*/
static struct clk_mux sc2_cdac_mux = {
	.reg = (void *)CLKCTRL_CDAC_CLK_CTRL,
	.mask = 0x3,
	.shift = 16,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cdac_mux",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){"xtal", "fclk_div5"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_cdac_div = {
	.reg = (void *)CLKCTRL_CDAC_CLK_CTRL,
	.shift = 0,
	.width = 16,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "cdac_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cdac_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_cdac_gate = {
	.reg = (void *)CLKCTRL_CDAC_CLK_CTRL,
	.bit_idx = 20,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cdac_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cdac_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_spicc_0/1_clk*/
static const char * const sc2_spicc_parent_names[] = { "xtal",
	"sys_clk", "fclk_div4", "fclk_div3", "fclk_div2", "fclk_div5",
	"fclk_div7", "gp0_pll"};

static struct clk_mux sc2_spicc0_mux = {
	.reg = (void *)CLKCTRL_SPICC_CLK_CTRL,
	.mask = 0x7,
	.shift = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_spicc_parent_names,
		.num_parents = ARRAY_SIZE(sc2_spicc_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_spicc0_div = {
	.reg = (void *)CLKCTRL_SPICC_CLK_CTRL,
	.shift = 0,
	.width = 6,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "spicc0_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_spicc0_gate = {
	.reg = (void *)CLKCTRL_SPICC_CLK_CTRL,
	.bit_idx = 6,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_clk",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "spicc0_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_mux sc2_spicc1_mux = {
	.reg = (void *)CLKCTRL_SPICC_CLK_CTRL,
	.mask = 0x7,
	.shift = 23,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "spicc1_mux",
		.ops = &clk_mux_ops,
		.parent_names = sc2_spicc_parent_names,
		.num_parents = ARRAY_SIZE(sc2_spicc_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider sc2_spicc1_div = {
	.reg = (void *)CLKCTRL_SPICC_CLK_CTRL,
	.shift = 16,
	.width = 6,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "spicc1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "spicc1_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_spicc1_gate = {
	.reg = (void *)CLKCTRL_SPICC_CLK_CTRL,
	.bit_idx = 22,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_clk",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "spicc1_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*cts_pwm_*_clk*/
PNAME(sc2_pwm_parent_names) = {"xtal", "null", "fclk_div3", "fclk_div4"};

static MUX(sc2_pwm_a_mux, CLKCTRL_PWM_CLK_AB_CTRL, 0x3, 9,
	   sc2_pwm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sc2_pwm_a_div, CLKCTRL_PWM_CLK_AB_CTRL, 0, 8, "sc2_pwm_a_mux",
	   CLK_SET_RATE_PARENT);
static GATE(sc2_pwm_a_gate, CLKCTRL_PWM_CLK_AB_CTRL, 8, "sc2_pwm_a_div",
	    CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);

static MUX(sc2_pwm_b_mux, CLKCTRL_PWM_CLK_AB_CTRL, 0x3, 25,
	   sc2_pwm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sc2_pwm_b_div, CLKCTRL_PWM_CLK_AB_CTRL, 16, 8, "sc2_pwm_b_mux",
	   CLK_SET_RATE_PARENT);
static GATE(sc2_pwm_b_gate, CLKCTRL_PWM_CLK_AB_CTRL, 24, "sc2_pwm_b_div",
	    CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);

static MUX(sc2_pwm_c_mux, CLKCTRL_PWM_CLK_CD_CTRL, 0x3, 9,
	   sc2_pwm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sc2_pwm_c_div, CLKCTRL_PWM_CLK_CD_CTRL, 0, 8, "sc2_pwm_c_mux",
	   CLK_SET_RATE_PARENT);
static GATE(sc2_pwm_c_gate, CLKCTRL_PWM_CLK_CD_CTRL, 8, "sc2_pwm_c_div",
	    CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);

static MUX(sc2_pwm_d_mux, CLKCTRL_PWM_CLK_CD_CTRL, 0x3, 25,
	   sc2_pwm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sc2_pwm_d_div, CLKCTRL_PWM_CLK_CD_CTRL, 16, 8, "sc2_pwm_d_mux",
	   CLK_SET_RATE_PARENT);
static GATE(sc2_pwm_d_gate, CLKCTRL_PWM_CLK_CD_CTRL, 24, "sc2_pwm_d_div",
	    CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);

static MUX(sc2_pwm_e_mux, CLKCTRL_PWM_CLK_EF_CTRL, 0x3, 9,
	   sc2_pwm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sc2_pwm_e_div, CLKCTRL_PWM_CLK_EF_CTRL, 0, 8, "sc2_pwm_e_mux",
	   CLK_SET_RATE_PARENT);
static GATE(sc2_pwm_e_gate, CLKCTRL_PWM_CLK_EF_CTRL, 8, "sc2_pwm_e_div",
	    CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);

static MUX(sc2_pwm_f_mux, CLKCTRL_PWM_CLK_EF_CTRL, 0x3, 25,
	   sc2_pwm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sc2_pwm_f_div, CLKCTRL_PWM_CLK_EF_CTRL, 16, 8, "sc2_pwm_f_mux",
	   CLK_SET_RATE_PARENT);
static GATE(sc2_pwm_f_gate, CLKCTRL_PWM_CLK_EF_CTRL, 24, "sc2_pwm_f_div",
	    CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);

static MUX(sc2_pwm_g_mux, CLKCTRL_PWM_CLK_GH_CTRL, 0x3, 9,
	   sc2_pwm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sc2_pwm_g_div, CLKCTRL_PWM_CLK_GH_CTRL, 0, 8, "sc2_pwm_g_mux",
	   CLK_SET_RATE_PARENT);
static GATE(sc2_pwm_g_gate, CLKCTRL_PWM_CLK_GH_CTRL, 8, "sc2_pwm_g_div",
	    CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);

static MUX(sc2_pwm_h_mux, CLKCTRL_PWM_CLK_GH_CTRL, 0x3, 25,
	   sc2_pwm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sc2_pwm_h_div, CLKCTRL_PWM_CLK_GH_CTRL, 16, 8, "sc2_pwm_h_mux",
	   CLK_SET_RATE_PARENT);
static GATE(sc2_pwm_h_gate, CLKCTRL_PWM_CLK_GH_CTRL, 24, "sc2_pwm_h_div",
	    CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);

static MUX(sc2_pwm_i_mux, CLKCTRL_PWM_CLK_IJ_CTRL, 0x3, 9,
	   sc2_pwm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sc2_pwm_i_div, CLKCTRL_PWM_CLK_IJ_CTRL, 0, 8, "sc2_pwm_i_mux",
	   CLK_SET_RATE_PARENT);
static GATE(sc2_pwm_i_gate, CLKCTRL_PWM_CLK_IJ_CTRL, 8, "sc2_pwm_i_div",
	    CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);

static MUX(sc2_pwm_j_mux, CLKCTRL_PWM_CLK_IJ_CTRL, 0x3, 25,
	   sc2_pwm_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sc2_pwm_j_div, CLKCTRL_PWM_CLK_IJ_CTRL, 16, 8, "sc2_pwm_j_mux",
	   CLK_SET_RATE_PARENT);
static GATE(sc2_pwm_j_gate, CLKCTRL_PWM_CLK_IJ_CTRL, 24, "sc2_pwm_j_div",
	    CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);

/*cts_sar_adc_clk*/
static struct clk_mux sc2_saradc_mux = {
	.reg = (void *)CLKCTRL_SAR_CLK_CTRL,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "saradc_mux",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "xtal", "sys_clk"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE),
	},
};

static struct clk_divider sc2_saradc_div = {
	.reg = (void *)CLKCTRL_SAR_CLK_CTRL,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "saradc_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "saradc_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_saradc_gate = {
	.reg = (void *)CLKCTRL_SAR_CLK_CTRL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "saradc_clk",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "saradc_div" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED),
	},
};

static u32 sc2_gen_clk_mux_table[] = { 0, 5, 6, 7, 19, 21, 22,
				23, 24, 25, 26, 27, 28, };
static const char * const sc2_gen_clk_parent_names[] = {
	"xtal", "gp0_pll", "gp1_pll", "hifi_pll", "fclk_div2", "fclk_div3",
	"fclk_div4", "fclk_div5", "fclk_div7", "mpll0", "mpll1",
	"mpll2", "mpll3"
};

static struct clk_mux sc2_gen_clk_sel = {
	.reg = (void *)CLKCTRL_GEN_CLK_CTRL,
	.mask = 0x1f,
	.shift = 12,
	.table = sc2_gen_clk_mux_table,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gen_clk_sel",
		.ops = &clk_mux_ops,
		.parent_names = sc2_gen_clk_parent_names,
		.num_parents = ARRAY_SIZE(sc2_gen_clk_parent_names),
	},
};

static struct clk_divider sc2_gen_clk_div = {
	.reg = (void *)CLKCTRL_GEN_CLK_CTRL,
	.shift = 0,
	.width = 11,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "gen_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "gen_clk_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate sc2_gen_clk = {
	.reg = (void *)CLKCTRL_GEN_CLK_CTRL,
	.bit_idx = 11,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gen_clk",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "gen_clk_div" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_SC2_SYS_GATE(_name, _reg, _bit)				\
struct clk_gate _name = {						\
	.reg = (void __iomem *)_reg,					\
	.bit_idx = (_bit),						\
	.lock = &clk_lock,						\
	.hw.init = &(struct clk_init_data) {				\
		.name = #_name,						\
		.ops = &clk_gate_ops,					\
		.parent_names = (const char *[]){ "sys_clk" },		\
		.num_parents = 1,					\
		.flags = CLK_IGNORE_UNUSED,				\
	},								\
}

/*CLKCTRL_SYS_CLK_EN0_REG0*/
static MESON_SC2_SYS_GATE(sc2_ddr, CLKCTRL_SYS_CLK_EN0_REG0, 0);
static MESON_SC2_SYS_GATE(sc2_dos, CLKCTRL_SYS_CLK_EN0_REG0, 1);
static MESON_SC2_SYS_GATE(sc2_ethphy, CLKCTRL_SYS_CLK_EN0_REG0, 4);
static MESON_SC2_SYS_GATE(sc2_mali, CLKCTRL_SYS_CLK_EN0_REG0, 6);
static MESON_SC2_SYS_GATE(sc2_aocpu, CLKCTRL_SYS_CLK_EN0_REG0, 13);
static MESON_SC2_SYS_GATE(sc2_aucpu, CLKCTRL_SYS_CLK_EN0_REG0, 14);
static MESON_SC2_SYS_GATE(sc2_cec, CLKCTRL_SYS_CLK_EN0_REG0, 16);
static MESON_SC2_SYS_GATE(sc2_sdemmca, CLKCTRL_SYS_CLK_EN0_REG0, 24);
static MESON_SC2_SYS_GATE(sc2_sdemmcb, CLKCTRL_SYS_CLK_EN0_REG0, 25);
static MESON_SC2_SYS_GATE(sc2_nand, CLKCTRL_SYS_CLK_EN0_REG0, 26);
static MESON_SC2_SYS_GATE(sc2_smartcard, CLKCTRL_SYS_CLK_EN0_REG0, 27);
static MESON_SC2_SYS_GATE(sc2_acodec, CLKCTRL_SYS_CLK_EN0_REG0, 28);
static MESON_SC2_SYS_GATE(sc2_spifc, CLKCTRL_SYS_CLK_EN0_REG0, 29);
static MESON_SC2_SYS_GATE(sc2_msr_clk, CLKCTRL_SYS_CLK_EN0_REG0, 30);
static MESON_SC2_SYS_GATE(sc2_ir_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 31);

/*CLKCTRL_SYS_CLK_EN0_REG1*/
static MESON_SC2_SYS_GATE(sc2_audio, CLKCTRL_SYS_CLK_EN0_REG1, 0);
static MESON_SC2_SYS_GATE(sc2_eth, CLKCTRL_SYS_CLK_EN0_REG1, 3);
static MESON_SC2_SYS_GATE(sc2_uart_a, CLKCTRL_SYS_CLK_EN0_REG1, 5);
static MESON_SC2_SYS_GATE(sc2_uart_b, CLKCTRL_SYS_CLK_EN0_REG1, 6);
static MESON_SC2_SYS_GATE(sc2_uart_c, CLKCTRL_SYS_CLK_EN0_REG1, 7);
static MESON_SC2_SYS_GATE(sc2_uart_d, CLKCTRL_SYS_CLK_EN0_REG1, 8);
static MESON_SC2_SYS_GATE(sc2_uart_e, CLKCTRL_SYS_CLK_EN0_REG1, 9);
static MESON_SC2_SYS_GATE(sc2_aififo, CLKCTRL_SYS_CLK_EN0_REG1, 11);
static MESON_SC2_SYS_GATE(sc2_ts_ddr, CLKCTRL_SYS_CLK_EN0_REG1, 15);
static MESON_SC2_SYS_GATE(sc2_ts_pll, CLKCTRL_SYS_CLK_EN0_REG1, 16);
static MESON_SC2_SYS_GATE(sc2_g2d, CLKCTRL_SYS_CLK_EN0_REG1, 20);
static MESON_SC2_SYS_GATE(sc2_spicc0, CLKCTRL_SYS_CLK_EN0_REG1, 21);
static MESON_SC2_SYS_GATE(sc2_spicc1, CLKCTRL_SYS_CLK_EN0_REG1, 22);
static MESON_SC2_SYS_GATE(sc2_pcie, CLKCTRL_SYS_CLK_EN0_REG1, 24);
static MESON_SC2_SYS_GATE(sc2_usb, CLKCTRL_SYS_CLK_EN0_REG1, 26);
static MESON_SC2_SYS_GATE(sc2_pcie_phy, CLKCTRL_SYS_CLK_EN0_REG1, 27);
static MESON_SC2_SYS_GATE(sc2_i2c_m_a, CLKCTRL_SYS_CLK_EN0_REG1, 30);
static MESON_SC2_SYS_GATE(sc2_i2c_m_b, CLKCTRL_SYS_CLK_EN0_REG1, 31);

/*CLKCTRL_SYS_CLK_EN0_REG2*/
static MESON_SC2_SYS_GATE(sc2_i2c_m_c, CLKCTRL_SYS_CLK_EN0_REG2, 0);
static MESON_SC2_SYS_GATE(sc2_i2c_m_d, CLKCTRL_SYS_CLK_EN0_REG2, 1);
static MESON_SC2_SYS_GATE(sc2_i2c_m_e, CLKCTRL_SYS_CLK_EN0_REG2, 2);
static MESON_SC2_SYS_GATE(sc2_i2c_m_f, CLKCTRL_SYS_CLK_EN0_REG2, 3);
static MESON_SC2_SYS_GATE(sc2_hdmitx_apb, CLKCTRL_SYS_CLK_EN0_REG2, 4);
static MESON_SC2_SYS_GATE(sc2_i2c_s_a, CLKCTRL_SYS_CLK_EN0_REG2, 5);
static MESON_SC2_SYS_GATE(sc2_usb1_to_ddr, CLKCTRL_SYS_CLK_EN0_REG2, 8);
static MESON_SC2_SYS_GATE(sc2_hdcp22, CLKCTRL_SYS_CLK_EN0_REG2, 10);
static MESON_SC2_SYS_GATE(sc2_mmc_apb, CLKCTRL_SYS_CLK_EN0_REG2, 11);
static MESON_SC2_SYS_GATE(sc2_rsa, CLKCTRL_SYS_CLK_EN0_REG2, 18);
static MESON_SC2_SYS_GATE(sc2_cpu_debug, CLKCTRL_SYS_CLK_EN0_REG2, 19);
static MESON_SC2_SYS_GATE(sc2_dspa, CLKCTRL_SYS_CLK_EN0_REG2, 21);
static MESON_SC2_SYS_GATE(sc2_vpu_intr, CLKCTRL_SYS_CLK_EN0_REG2, 25);
static MESON_SC2_SYS_GATE(sc2_sar_adc, CLKCTRL_SYS_CLK_EN0_REG2, 28);
static MESON_SC2_SYS_GATE(sc2_gic, CLKCTRL_SYS_CLK_EN0_REG2, 30);

/*CLKCTRL_SYS_CLK_EN0_REG3*/
static MESON_SC2_SYS_GATE(sc2_pwm_ab, CLKCTRL_SYS_CLK_EN0_REG3, 7);
static MESON_SC2_SYS_GATE(sc2_pwm_cd, CLKCTRL_SYS_CLK_EN0_REG3, 8);
static MESON_SC2_SYS_GATE(sc2_pwm_ef, CLKCTRL_SYS_CLK_EN0_REG3, 9);
static MESON_SC2_SYS_GATE(sc2_pwm_gh, CLKCTRL_SYS_CLK_EN0_REG3, 10);
static MESON_SC2_SYS_GATE(sc2_pwm_ij, CLKCTRL_SYS_CLK_EN0_REG3, 11);

/* Array of all clocks provided by this provider */

static struct clk_hw *sc2_clk_hws[] = {
	[CLKID_FIXED_PLL]	= &sc2_fixed_pll.hw,
	[CLKID_SYS_PLL]		= &sc2_sys_pll.hw,
	[CLKID_GP0_PLL]		= &sc2_gp0_pll.hw,
	[CLKID_GP1_PLL]		= &sc2_gp1_pll.hw,
	[CLKID_HIFI_PLL]	= &sc2_hifi_pll.hw,
	[CLKID_PCIE_PLL]	= &sc2_pcie_pll.hw,
	[CLKID_PCIE_HCSL]	= &sc2_pcie_hcsl.hw,
	[CLKID_FCLK_DIV2]	= &sc2_fclk_div2.hw,
	[CLKID_FCLK_DIV3]	= &sc2_fclk_div3.hw,
	[CLKID_FCLK_DIV4]	= &sc2_fclk_div4.hw,
	[CLKID_FCLK_DIV5]	= &sc2_fclk_div5.hw,
	[CLKID_FCLK_DIV7]	= &sc2_fclk_div7.hw,
	[CLKID_FCLK_DIV2P5]	= &sc2_fclk_div2p5.hw,

	[CLKID_MPLL0]		= &sc2_mpll0.hw,
	[CLKID_MPLL1]		= &sc2_mpll1.hw,
	[CLKID_MPLL2]		= &sc2_mpll2.hw,
	[CLKID_MPLL3]		= &sc2_mpll3.hw,

	[CLKID_XTAL_DDR_PLL]	= &sc2_xtal_ddr_pll.hw,
	[CLKID_XTAL_PLL_TOP]	= &sc2_xtal_pll_top.hw,
	[CLKID_XTAL_USB_PLL0]	= &sc2_xtal_usb_pll0.hw,
	[CLKID_XTAL_USB_PLL1]	= &sc2_xtal_usb_pll1.hw,
	[CLKID_XTAL_USB_PCIE_CTRL]	= &sc2_xtal_usb_pcie_ctrl.hw,
	[CLKID_XTAL_ETH_PLL]	= &sc2_xtal_eth_pll.hw,
	[CLKID_XTAL_PCIE_REFCLK_Pll]	= &sc2_xtal_pcie_refclk_pll.hw,
	[CLKID_XTAL_EARC]	= &sc2_xtal_earc.hw,

	[CLKID_CPU_FCLK_P]	= &sc2_cpu_fclk_p.hw,
	[CLKID_CPU_CLK]		= &sc2_cpu_clk.hw,
	[CLKID_CPU1_CLK]	= &sc2_cpu1_clk.hw,
	[CLKID_CPU2_CLK]	= &sc2_cpu2_clk.hw,
	[CLKID_CPU3_CLK]	= &sc2_cpu3_clk.hw,
	[CLKID_DSU_PRE_SRC0]	= &sc2_dsu_pre_src_clk_mux0.hw,
	[CLKID_DSU_PRE_SRC1]	= &sc2_dsu_pre_src_clk_mux1.hw,
	[CLKID_DSU_CLK_DIV0]	= &sc2_dsu_clk_div0.hw,
	[CLKID_DSU_CLK_DIV1]	= &sc2_dsu_clk_div1.hw,
	[CLKID_DSU_PRE_MUX0]	= &sc2_dsu_pre_clk_mux0.hw,
	[CLKID_DSU_PRE_MUX1]	= &sc2_dsu_pre_clk_mux1.hw,
	[CLKID_DSU_PRE_POST_MUX]	= &sc2_dsu_pre_post_clk_mux.hw,
	[CLKID_DSU_PRE_CLK]	= &sc2_dsu_pre_clk.hw,
	[CLKID_DSU_CLK]		= &sc2_dsu_clk.hw,

	[CLKID_SYS_CLK_B_MUX]	= &sc2_sysclk_b_sel.hw,
	[CLKID_SYS_CLK_B_DIV]	= &sc2_sysclk_b_div.hw,
	[CLKID_SYS_CLK_B_GATE]	= &sc2_sysclk_b_gate.hw,
	[CLKID_SYS_CLK_A_MUX]	= &sc2_sysclk_a_sel.hw,
	[CLKID_SYS_CLK_A_DIV]	= &sc2_sysclk_a_div.hw,
	[CLKID_SYS_CLK_A_GATE]	= &sc2_sysclk_a_gate.hw,
	[CLKID_SYS_CLK]		= &sc2_sysclk_sel.hw,
	[CLKID_AXI_CLK_B_MUX]	= &sc2_axiclk_b_sel.hw,
	[CLKID_AXI_CLK_B_DIV]	= &sc2_axiclk_b_div.hw,
	[CLKID_AXI_CLK_B_GATE]	= &sc2_axiclk_b_gate.hw,
	[CLKID_AXI_CLK_A_MUX]	= &sc2_axiclk_a_sel.hw,
	[CLKID_AXI_CLK_A_DIV]	= &sc2_axiclk_a_div.hw,
	[CLKID_AXI_CLK_A_GATE]	= &sc2_axiclk_a_gate.hw,
	[CLKID_AXI_CLK]		= &sc2_axiclk_sel.hw,

	[CLKID_SC_CLK_MUX]	= &sc2_sc_clk_mux.hw,
	[CLKID_SC_CLK_DIV]	= &sc2_sc_clk_div.hw,
	[CLKID_SC_CLK_GATE]	= &sc2_sc_clk_gate.hw,

	[CLKID_RAMA_CLK_B_MUX]	= &sc2_ramaclk_b_sel.hw,
	[CLKID_RAMA_CLK_B_DIV]	= &sc2_ramaclk_b_div.hw,
	[CLKID_RAMA_CLK_B_GATE]	= &sc2_ramaclk_b_gate.hw,
	[CLKID_RAMA_CLK_A_MUX]	= &sc2_ramaclk_a_sel.hw,
	[CLKID_RAMA_CLK_A_DIV]	= &sc2_ramaclk_a_div.hw,
	[CLKID_RAMA_CLK_A_GATE]	= &sc2_ramaclk_a_gate.hw,
	[CLKID_RAMA_CLK]	= &sc2_ramaclk_sel.hw,

	[CLKID_DSPA_CLK_B_MUX]	= &sc2_dspaclk_b_sel.hw,
	[CLKID_DSPA_CLK_B_DIV]	= &sc2_dspaclk_b_div.hw,
	[CLKID_DSPA_CLK_B_GATE]	= &sc2_dspaclk_b_gate.hw,
	[CLKID_DSPA_CLK_A_MUX]	= &sc2_dspaclk_a_sel.hw,
	[CLKID_DSPA_CLK_A_DIV]	= &sc2_dspaclk_a_div.hw,
	[CLKID_DSPA_CLK_A_GATE]	= &sc2_dspaclk_a_gate.hw,
	[CLKID_DSPA_CLK]	= &sc2_dspaclk_sel.hw,

	[CLKID_24M_CLK_GATE]	= &sc2_24M_clk_gate.hw,
	[CLKID_24M_CLK_DIV2]	= &sc2_24M_div2.hw,
	[CLKID_12M_CLK_GATE]	= &sc2_12M_clk_gate.hw,
	[CLKID_25M_CLK_DIV]	= &sc2_25M_clk_div.hw,
	[CLKID_25M_CLK_GATE]	= &sc2_25M_clk_gate.hw,

	[CLKID_HDMI_CLK_MUX]	= &sc2_hdmiclk_mux.hw,
	[CLKID_HDMI_CLK_DIV]	= &sc2_hdmiclk_div.hw,
	[CLKID_HDMI_CLK_GATE]	= &sc2_hdmiclk_gate.hw,

	[CLKID_TS_CLK_DIV]	= &sc2_ts_clk_div.hw,
	[CLKID_TS_CLK_GATE]	= &sc2_ts_clk_gate.hw,

	[CLKID_GPU_A_MUX]	= &sc2_gpu_a_mux.hw,
	[CLKID_GPU_A_DIV]	= &sc2_gpu_a_div.hw,
	[CLKID_GPU_A_GATE]	= &sc2_gpu_a_gate.hw,
	[CLKID_GPU_B_MUX]	= &sc2_gpu_b_mux.hw,
	[CLKID_GPU_B_DIV]	= &sc2_gpu_b_div.hw,
	[CLKID_GPU_B_GATE]	= &sc2_gpu_b_gate.hw,
	[CLKID_GPU_MUX]		= &sc2_gpu_mux.hw,

	[CLKID_VDEC_A_MUX]	= &sc2_vdec_a_mux.hw,
	[CLKID_VDEC_A_DIV]	= &sc2_vdec_a_div.hw,
	[CLKID_VDEC_A_GATE]	= &sc2_vdec_a_gate.hw,
	[CLKID_VDEC_B_MUX]	= &sc2_vdec_b_mux.hw,
	[CLKID_VDEC_B_DIV]	= &sc2_vdec_b_div.hw,
	[CLKID_VDEC_B_GATE]	= &sc2_vdec_b_gate.hw,
	[CLKID_VDEC_MUX]	= &sc2_vdec_mux.hw,

	[CLKID_HCODEC_A_MUX]	= &sc2_hcodec_a_mux.hw,
	[CLKID_HCODEC_A_DIV]	= &sc2_hcodec_a_div.hw,
	[CLKID_HCODEC_A_GATE]	= &sc2_hcodec_a_gate.hw,
	[CLKID_HCODEC_B_MUX]	= &sc2_hcodec_b_mux.hw,
	[CLKID_HCODEC_B_DIV]	= &sc2_hcodec_b_div.hw,
	[CLKID_HCODEC_B_GATE]	= &sc2_hcodec_b_gate.hw,
	[CLKID_HCODEC_MUX]	= &sc2_hcodec_mux.hw,

	[CLKID_HEVCB_A_MUX]	= &sc2_hevcb_a_mux.hw,
	[CLKID_HEVCB_A_DIV]	= &sc2_hevcb_a_div.hw,
	[CLKID_HEVCB_A_GATE]	= &sc2_hevcb_a_gate.hw,
	[CLKID_HEVCB_B_MUX]	= &sc2_hevcb_b_mux.hw,
	[CLKID_HEVCB_B_DIV]	= &sc2_hevcb_b_div.hw,
	[CLKID_HEVCB_B_GATE]	= &sc2_hevcb_b_gate.hw,
	[CLKID_HEVCB_MUX]	= &sc2_hevcb_mux.hw,

	[CLKID_HEVCF_A_MUX]	= &sc2_hevcf_a_mux.hw,
	[CLKID_HEVCF_A_DIV]	= &sc2_hevcf_a_div.hw,
	[CLKID_HEVCF_A_GATE]	= &sc2_hevcf_a_gate.hw,
	[CLKID_HEVCF_B_MUX]	= &sc2_hevcf_b_mux.hw,
	[CLKID_HEVCF_B_DIV]	= &sc2_hevcf_b_div.hw,
	[CLKID_HEVCF_B_GATE]	= &sc2_hevcf_b_gate.hw,
	[CLKID_HEVCF_MUX]	= &sc2_hevcf_mux.hw,

	[CLKID_VPU_A_MUX]	= &sc2_vpu_a_mux.hw,
	[CLKID_VPU_A_DIV]	= &sc2_vpu_a_div.hw,
	[CLKID_VPU_A_GATE]	= &sc2_vpu_a_gate.hw,
	[CLKID_VPU_B_MUX]	= &sc2_vpu_b_mux.hw,
	[CLKID_VPU_B_DIV]	= &sc2_vpu_b_div.hw,
	[CLKID_VPU_B_GATE]	= &sc2_vpu_b_gate.hw,
	[CLKID_VPU_MUX]		= &sc2_vpu_mux.hw,

	[CLKID_VPU_CLKB_TMP_MUX]	= &sc2_vpu_clkb_tmp_mux.hw,
	[CLKID_VPU_CLKB_TMP_DIV]	= &sc2_vpu_clkb_tmp_div.hw,
	[CLKID_VPU_CLKB_TMP_GATE]	= &sc2_vpu_clkb_tmp_gate.hw,
	[CLKID_VPU_CLKB_DIV]	= &sc2_vpu_clkb_div.hw,
	[CLKID_VPU_CLKB_GATE]	= &sc2_vpu_clkb_gate.hw,

	[CLKID_VPU_CLKC_A_MUX]	= &sc2_vpu_clkc_a_mux.hw,
	[CLKID_VPU_CLKC_A_DIV]	= &sc2_vpu_clkc_a_div.hw,
	[CLKID_VPU_CLKC_A_GATE]	= &sc2_vpu_clkc_a_gate.hw,
	[CLKID_VPU_CLKC_B_MUX]	= &sc2_vpu_clkc_b_mux.hw,
	[CLKID_VPU_CLKC_B_DIV]	= &sc2_vpu_clkc_b_div.hw,
	[CLKID_VPU_CLKC_B_GATE]	= &sc2_vpu_clkc_b_gate.hw,
	[CLKID_VPU_CLKC_MUX]	= &sc2_vpu_clkc_mux.hw,

	[CLKID_VAPB_A_MUX]	= &sc2_vapb_a_mux.hw,
	[CLKID_VAPB_A_DIV]	= &sc2_vapb_a_div.hw,
	[CLKID_VAPB_A_GATE]	= &sc2_vapb_a_gate.hw,
	[CLKID_VAPB_B_MUX]	= &sc2_vapb_b_mux.hw,
	[CLKID_VAPB_B_DIV]	= &sc2_vapb_b_div.hw,
	[CLKID_VAPB_B_GATE]	= &sc2_vapb_b_gate.hw,
	[CLKID_VAPB_MUX]	= &sc2_vapb_mux.hw,

	[CLKID_GE2D_GATE]	= &sc2_ge2d_gate.hw,
	[CLKID_HDCP22_ESMCLK_MUX]	= &sc2_hdcp22_esmclk_mux.hw,
	[CLKID_HDCP22_ESMCLK_DIV]	= &sc2_hdcp22_esmclk_div.hw,
	[CLKID_HDCP22_ESMCLK_GATE]	= &sc2_hdcp22_esmclk_gate.hw,
	[CLKID_HDCP22_SKPCLK_MUX]	= &sc2_hdcp22_skpclk_mux.hw,
	[CLKID_HDCP22_SKPCLK_DIV]	= &sc2_hdcp22_skpclk_div.hw,
	[CLKID_HDCP22_SKPCLK_GATE]	= &sc2_hdcp22_skpclk_gate.hw,

	[CLKID_VDIN_MEAS_CLK_MUX]	= &sc2_vdin_meas_clk_mux.hw,
	[CLKID_VDIN_MEAS_CLK_DIV]	= &sc2_vdin_meas_clk_div.hw,
	[CLKID_VDIN_MEAS_CLK_GATE]	= &sc2_vdin_meas_clk_gate.hw,

	[CLKID_CDAC_MUX]	= &sc2_cdac_mux.hw,
	[CLKID_CDAC_DIV]	= &sc2_cdac_div.hw,
	[CLKID_CDAC_GATE]	= &sc2_cdac_gate.hw,

	[CLKID_SPICC0_MUX]	= &sc2_spicc0_mux.hw,
	[CLKID_SPICC0_DIV]	= &sc2_spicc0_div.hw,
	[CLKID_SPICC0_GATE]	= &sc2_spicc0_gate.hw,
	[CLKID_SPICC1_MUX]	= &sc2_spicc1_mux.hw,
	[CLKID_SPICC1_DIV]	= &sc2_spicc1_div.hw,
	[CLKID_SPICC1_GATE]	= &sc2_spicc1_gate.hw,

	[CLKID_PWM_A_MUX]	= &sc2_pwm_a_mux.hw,
	[CLKID_PWM_A_DIV]	= &sc2_pwm_a_div.hw,
	[CLKID_PWM_A_GATE]	= &sc2_pwm_a_gate.hw,
	[CLKID_PWM_B_MUX]	= &sc2_pwm_b_mux.hw,
	[CLKID_PWM_B_DIV]	= &sc2_pwm_b_div.hw,
	[CLKID_PWM_B_GATE]	= &sc2_pwm_b_gate.hw,
	[CLKID_PWM_C_MUX]	= &sc2_pwm_c_mux.hw,
	[CLKID_PWM_C_DIV]	= &sc2_pwm_c_div.hw,
	[CLKID_PWM_C_GATE]	= &sc2_pwm_c_gate.hw,
	[CLKID_PWM_D_MUX]	= &sc2_pwm_d_mux.hw,
	[CLKID_PWM_D_DIV]	= &sc2_pwm_d_div.hw,
	[CLKID_PWM_D_GATE]	= &sc2_pwm_d_gate.hw,
	[CLKID_PWM_E_MUX]	= &sc2_pwm_e_mux.hw,
	[CLKID_PWM_E_DIV]	= &sc2_pwm_e_div.hw,
	[CLKID_PWM_E_GATE]	= &sc2_pwm_e_gate.hw,
	[CLKID_PWM_F_MUX]	= &sc2_pwm_f_mux.hw,
	[CLKID_PWM_F_DIV]	= &sc2_pwm_f_div.hw,
	[CLKID_PWM_F_GATE]	= &sc2_pwm_f_gate.hw,
	[CLKID_PWM_G_MUX]	= &sc2_pwm_g_mux.hw,
	[CLKID_PWM_G_DIV]	= &sc2_pwm_g_div.hw,
	[CLKID_PWM_G_GATE]	= &sc2_pwm_g_gate.hw,
	[CLKID_PWM_H_MUX]	= &sc2_pwm_h_mux.hw,
	[CLKID_PWM_H_DIV]	= &sc2_pwm_h_div.hw,
	[CLKID_PWM_H_GATE]	= &sc2_pwm_h_gate.hw,
	[CLKID_PWM_I_MUX]	= &sc2_pwm_i_mux.hw,
	[CLKID_PWM_I_DIV]	= &sc2_pwm_i_div.hw,
	[CLKID_PWM_I_GATE]	= &sc2_pwm_i_gate.hw,
	[CLKID_PWM_J_MUX]	= &sc2_pwm_j_mux.hw,
	[CLKID_PWM_J_DIV]	= &sc2_pwm_j_div.hw,
	[CLKID_PWM_J_GATE]	= &sc2_pwm_j_gate.hw,

	[CLKID_SARADC_MUX]	= &sc2_saradc_mux.hw,
	[CLKID_SARADC_DIV]	= &sc2_saradc_div.hw,
	[CLKID_SARADC_GATE]	= &sc2_saradc_gate.hw,
	[CLKID_GEN_CLK_MUX]	= &sc2_gen_clk_sel.hw,
	[CLKID_GEN_CLK_DIV]	= &sc2_gen_clk_div.hw,
	[CLKID_GEN_CLK_GATE]	= &sc2_gen_clk.hw,

	[CLKID_DDR]		= &sc2_ddr.hw,
	[CLKID_DOS]		= &sc2_dos.hw,
	[CLKID_ETHPHY]		= &sc2_ethphy.hw,
	[CLKID_MALI]		= &sc2_mali.hw,
	[CLKID_AOCPU]		= &sc2_aocpu.hw,
	[CLKID_AUCPU]		= &sc2_aucpu.hw,
	[CLKID_CEC]		= &sc2_cec.hw,
	[CLKID_SD_EMMC_A]	= &sc2_sdemmca.hw,
	[CLKID_SD_EMMC_B]	= &sc2_sdemmcb.hw,
	[CLKID_NAND]		= &sc2_nand.hw,
	[CLKID_SMARTCARD]	= &sc2_smartcard.hw,
	[CLKID_ACODEC]		= &sc2_acodec.hw,
	[CLKID_SPIFC]		= &sc2_spifc.hw,
	[CLKID_MSR_CLK]		= &sc2_msr_clk.hw,
	[CLKID_IR_CTRL]		= &sc2_ir_ctrl.hw,

	[CLKID_AUDIO]		= &sc2_audio.hw,
	[CLKID_ETH]		= &sc2_eth.hw,
	[CLKID_UART_A]		= &sc2_uart_a.hw,
	[CLKID_UART_B]		= &sc2_uart_b.hw,
	[CLKID_UART_C]		= &sc2_uart_c.hw,
	[CLKID_UART_D]		= &sc2_uart_d.hw,
	[CLKID_UART_E]		= &sc2_uart_e.hw,
	[CLKID_AIFIFO]		= &sc2_aififo.hw,
	[CLKID_TS_DDR]		= &sc2_ts_ddr.hw,
	[CLKID_TS_PLL]		= &sc2_ts_pll.hw,
	[CLKID_G2D]		= &sc2_g2d.hw,
	[CLKID_SPICC0]		= &sc2_spicc0.hw,
	[CLKID_SPICC1]		= &sc2_spicc1.hw,
	[CLKID_PCIE]		= &sc2_pcie.hw,
	[CLKID_USB]		= &sc2_usb.hw,
	[CLKID_PCIE_PHY]	= &sc2_pcie_phy.hw,
	[CLKID_I2C_M_A]		= &sc2_i2c_m_a.hw,
	[CLKID_I2C_M_B]		= &sc2_i2c_m_b.hw,
	[CLKID_I2C_M_C]		= &sc2_i2c_m_c.hw,
	[CLKID_I2C_M_D]		= &sc2_i2c_m_d.hw,
	[CLKID_I2C_M_E]		= &sc2_i2c_m_e.hw,
	[CLKID_I2C_M_F]		= &sc2_i2c_m_f.hw,
	[CLKID_HDMITX_APB]	= &sc2_hdmitx_apb.hw,
	[CLKID_I2C_S_A]		= &sc2_i2c_s_a.hw,
	[CLKID_USB1_TO_DDR]	= &sc2_usb1_to_ddr.hw,
	[CLKID_HDCP22]		= &sc2_hdcp22.hw,
	[CLKID_MMC_APB]		= &sc2_mmc_apb.hw,
	[CLKID_RSA]		= &sc2_rsa.hw,
	[CLKID_CPU_DEBUG]	= &sc2_cpu_debug.hw,
	[CLKID_DSPA]		= &sc2_dspa.hw,
	[CLKID_VPU_INTR]	= &sc2_vpu_intr.hw,
	[CLKID_SAR_ADC]		= &sc2_sar_adc.hw,
	[CLKID_GIC]		= &sc2_gic.hw,
	[CLKID_PWM_AB]		= &sc2_pwm_ab.hw,
	[CLKID_PWM_CD]		= &sc2_pwm_cd.hw,
	[CLKID_PWM_EF]		= &sc2_pwm_ef.hw,
	[CLKID_PWM_GH]		= &sc2_pwm_gh.hw,
	[CLKID_PWM_IJ]		= &sc2_pwm_ij.hw,
	[CLKID_WAVE_A_MUX]	= &sc2_wave_a_mux.hw,
	[CLKID_WAVE_A_DIV]	= &sc2_wave_a_div.hw,
	[CLKID_WAVE_A_GATE]	= &sc2_wave_a_gate.hw,
	[CLKID_WAVE_B_MUX]	= &sc2_wave_b_mux.hw,
	[CLKID_WAVE_B_DIV]	= &sc2_wave_b_div.hw,
	[CLKID_WAVE_B_GATE]	= &sc2_wave_b_gate.hw,
	[CLKID_WAVE_C_MUX]	= &sc2_wave_c_mux.hw,
	[CLKID_WAVE_C_DIV]	= &sc2_wave_c_div.hw,
	[CLKID_WAVE_C_GATE]	= &sc2_wave_c_gate.hw,

	[CLKID_RTC_32K_CLKIN]	= &sc2_rtc_32k_clkin.hw,
	[CLKID_RTC_32K_DIV]	= &sc2_rtc_32k_div.hw,
	[CLKID_RTC_32K_XATL]	= &sc2_rtc_32k_xtal.hw,
	[CLKID_RTC_32K_MUX]	= &sc2_rtc_32k_sel.hw,
	[CLKID_RTC_CLK]		= &sc2_rtc_clk.hw

/*
 *cec clk writel register
	[CLKID_CECA_32K_CLKIN]	= &sc2_ceca_32k_clkin.hw,
	[CLKID_CECA_32K_DIV]	= &sc2_ceca_32k_div.hw,
	[CLKID_CECA_32K_MUX_PRE] = &sc2_ceca_32k_sel_pre.hw,
	[CLKID_CECA_32K_MUX]	= &sc2_ceca_32k_sel.hw,
	[CLKID_CECA_32K_CLKOUT]	= &sc2_ceca_32k_clkout.hw,

	[CLKID_CECB_32K_CLKIN]	= &sc2_cecb_32k_clkin.hw,
	[CLKID_CECB_32K_DIV]	= &sc2_cecb_32k_div.hw,
	[CLKID_CECB_32K_MUX_PRE] = &sc2_cecb_32k_sel_pre.hw,
	[CLKID_CECB_32K_MUX]	= &sc2_cecb_32k_sel.hw,
	[CLKID_CECB_32K_CLKOUT]	= &sc2_cecb_32k_clkout.hw
*/
};

/* Convenience tables to populate base addresses in .probe */
static struct meson_clk_pll *const sc2_clk_plls[] = {
	&sc2_fixed_pll,
	&sc2_sys_pll,
	&sc2_gp0_pll,
	&sc2_gp1_pll,
	&sc2_hifi_pll,
	&sc2_pcie_pll
};

static struct meson_clk_mpll *const sc2_clk_mplls[] = {
	&sc2_mpll0,
	&sc2_mpll1,
	&sc2_mpll2,
	&sc2_mpll3
};

static struct meson_dualdiv_clk *const sc2_dualdiv_clks[] = {
	&sc2_rtc_32k_div,
	&sc2_ceca_32k_div,
	&sc2_cecb_32k_div
};

static struct clk_mux *sc2_clk_muxs[] = {
	&sc2_sysclk_b_sel,
	&sc2_sysclk_a_sel,
	&sc2_sysclk_sel,
	&sc2_axiclk_b_sel,
	&sc2_axiclk_a_sel,
	&sc2_axiclk_sel,
	&sc2_sc_clk_mux,
	&sc2_ramaclk_b_sel,
	&sc2_ramaclk_a_sel,
	&sc2_ramaclk_sel,
	&sc2_dspaclk_b_sel,
	&sc2_dspaclk_a_sel,
	&sc2_dspaclk_sel,
	&sc2_hdmiclk_mux,
	&sc2_gpu_a_mux,
	&sc2_gpu_b_mux,
	&sc2_gpu_mux,
	&sc2_vdec_a_mux,
	&sc2_vdec_b_mux,
	&sc2_vdec_mux,
	&sc2_hcodec_a_mux,
	&sc2_hcodec_b_mux,
	&sc2_hcodec_mux,
	&sc2_hevcb_a_mux,
	&sc2_hevcb_b_mux,
	&sc2_hevcb_mux,
	&sc2_hevcf_a_mux,
	&sc2_hevcf_b_mux,
	&sc2_hevcf_mux,
	&sc2_vpu_a_mux,
	&sc2_vpu_b_mux,
	&sc2_vpu_mux,
	&sc2_vpu_clkb_tmp_mux,
	&sc2_vpu_clkc_a_mux,
	&sc2_vpu_clkc_b_mux,
	&sc2_vpu_clkc_mux,
	&sc2_vapb_a_mux,
	&sc2_vapb_b_mux,
	&sc2_vapb_mux,
	&sc2_hdcp22_esmclk_mux,
	&sc2_hdcp22_skpclk_mux,
	&sc2_vdin_meas_clk_mux,
	&sc2_cdac_mux,
	&sc2_spicc0_mux,
	&sc2_spicc1_mux,
	&sc2_pwm_a_mux,
	&sc2_pwm_b_mux,
	&sc2_pwm_c_mux,
	&sc2_pwm_d_mux,
	&sc2_pwm_e_mux,
	&sc2_pwm_f_mux,
	&sc2_pwm_g_mux,
	&sc2_pwm_h_mux,
	&sc2_pwm_i_mux,
	&sc2_pwm_j_mux,
	&sc2_saradc_mux,
	&sc2_gen_clk_sel,
	&sc2_wave_a_mux,
	&sc2_wave_b_mux,
	&sc2_wave_c_mux,
	&sc2_rtc_32k_sel,
	&sc2_ceca_32k_sel_pre,
	&sc2_ceca_32k_sel,
	&sc2_cecb_32k_sel_pre,
	&sc2_cecb_32k_sel

};

static struct clk_divider *sc2_clk_divs[] = {
	&sc2_sysclk_b_div,
	&sc2_sysclk_a_div,
	&sc2_axiclk_b_div,
	&sc2_axiclk_a_div,
	&sc2_sc_clk_div,
	&sc2_ramaclk_b_div,
	&sc2_ramaclk_a_div,
	&sc2_dspaclk_b_div,
	&sc2_dspaclk_a_div,
	&sc2_25M_clk_div,
	&sc2_hdmiclk_div,
	&sc2_ts_clk_div,
	&sc2_gpu_a_div,
	&sc2_gpu_b_div,
	&sc2_vdec_a_div,
	&sc2_vdec_b_div,
	&sc2_hcodec_a_div,
	&sc2_hcodec_b_div,
	&sc2_hevcb_a_div,
	&sc2_hevcb_b_div,
	&sc2_hevcf_a_div,
	&sc2_hevcf_b_div,
	&sc2_vpu_a_div,
	&sc2_vpu_b_div,
	&sc2_vpu_clkb_tmp_div,
	&sc2_vpu_clkb_div,
	&sc2_vpu_clkc_a_div,
	&sc2_vpu_clkc_b_div,
	&sc2_vapb_a_div,
	&sc2_vapb_b_div,
	&sc2_hdcp22_esmclk_div,
	&sc2_hdcp22_skpclk_div,
	&sc2_vdin_meas_clk_div,
	&sc2_cdac_div,
	&sc2_spicc0_div,
	&sc2_spicc1_div,
	&sc2_pwm_a_div,
	&sc2_pwm_b_div,
	&sc2_pwm_c_div,
	&sc2_pwm_d_div,
	&sc2_pwm_e_div,
	&sc2_pwm_f_div,
	&sc2_pwm_g_div,
	&sc2_pwm_h_div,
	&sc2_pwm_i_div,
	&sc2_pwm_j_div,
	&sc2_saradc_div,
	&sc2_gen_clk_div,
	&sc2_wave_a_div,
	&sc2_wave_b_div,
	&sc2_wave_c_div

};

static struct clk_gate *sc2_clk_gates[] = {
	&sc2_xtal_ddr_pll,
	&sc2_xtal_pll_top,
	&sc2_xtal_hdmi_pll,
	&sc2_xtal_usb_pll0,
	&sc2_xtal_usb_pll1,
	&sc2_xtal_usb_pcie_ctrl,
	&sc2_xtal_eth_pll,
	&sc2_xtal_pcie_refclk_pll,
	&sc2_xtal_earc,

	&sc2_sysclk_b_gate,
	&sc2_sysclk_a_gate,
	&sc2_axiclk_b_gate,
	&sc2_axiclk_a_gate,
	&sc2_sc_clk_gate,
	&sc2_ramaclk_b_gate,
	&sc2_ramaclk_a_gate,
	&sc2_dspaclk_b_gate,
	&sc2_dspaclk_a_gate,
	&sc2_24M_clk_gate,
	&sc2_12M_clk_gate,
	&sc2_25M_clk_gate,
	&sc2_hdmiclk_gate,
	&sc2_ts_clk_gate,
	&sc2_gpu_a_gate,
	&sc2_gpu_b_gate,
	&sc2_vdec_a_gate,
	&sc2_vdec_b_gate,
	&sc2_hcodec_a_gate,
	&sc2_hcodec_b_gate,
	&sc2_hevcb_a_gate,
	&sc2_hevcb_b_gate,
	&sc2_hevcf_a_gate,
	&sc2_hevcf_b_gate,
	&sc2_vpu_a_gate,
	&sc2_vpu_b_gate,
	&sc2_vpu_clkb_tmp_gate,
	&sc2_vpu_clkb_gate,
	&sc2_vpu_clkc_a_gate,
	&sc2_vpu_clkc_b_gate,
	&sc2_vapb_a_gate,
	&sc2_vapb_b_gate,
	&sc2_ge2d_gate,
	&sc2_hdcp22_esmclk_gate,
	&sc2_hdcp22_skpclk_gate,
	&sc2_vdin_meas_clk_gate,
	&sc2_cdac_gate,
	&sc2_spicc0_gate,
	&sc2_spicc1_gate,
	&sc2_pwm_a_gate,
	&sc2_pwm_b_gate,
	&sc2_pwm_c_gate,
	&sc2_pwm_d_gate,
	&sc2_pwm_e_gate,
	&sc2_pwm_f_gate,
	&sc2_pwm_g_gate,
	&sc2_pwm_h_gate,
	&sc2_pwm_i_gate,
	&sc2_pwm_j_gate,
	&sc2_saradc_gate,
	&sc2_gen_clk,

	&sc2_ddr,
	&sc2_dos,
	&sc2_ethphy,
	&sc2_mali,
	&sc2_aocpu,
	&sc2_aucpu,
	&sc2_cec,
	&sc2_sdemmca,
	&sc2_sdemmcb,
	&sc2_nand,
	&sc2_smartcard,
	&sc2_acodec,
	&sc2_spifc,
	&sc2_msr_clk,
	&sc2_ir_ctrl,
	&sc2_audio,
	&sc2_eth,
	&sc2_uart_a,
	&sc2_uart_b,
	&sc2_uart_c,
	&sc2_uart_d,
	&sc2_uart_e,
	&sc2_aififo,
	&sc2_ts_ddr,
	&sc2_ts_pll,
	&sc2_g2d,
	&sc2_spicc0,
	&sc2_spicc1,
	&sc2_pcie,
	&sc2_usb,
	&sc2_pcie_phy,
	&sc2_i2c_m_a,
	&sc2_i2c_m_b,
	&sc2_i2c_m_c,
	&sc2_i2c_m_d,
	&sc2_i2c_m_e,
	&sc2_i2c_m_f,
	&sc2_hdmitx_apb,
	&sc2_i2c_s_a,
	&sc2_usb1_to_ddr,
	&sc2_hdcp22,
	&sc2_mmc_apb,
	&sc2_rsa,
	&sc2_cpu_debug,
	&sc2_dspa,
	&sc2_vpu_intr,
	&sc2_sar_adc,
	&sc2_gic,
	&sc2_pwm_ab,
	&sc2_pwm_cd,
	&sc2_pwm_ef,
	&sc2_pwm_gh,
	&sc2_pwm_ij,
	&sc2_wave_a_gate,
	&sc2_wave_b_gate,
	&sc2_wave_c_gate,

	&sc2_rtc_32k_clkin,
	&sc2_rtc_32k_xtal,
	&sc2_rtc_clk,

	&sc2_ceca_32k_clkin,
	&sc2_ceca_32k_clkout,
	&sc2_cecb_32k_clkin,
	&sc2_cecb_32k_clkout

};

static int sc2_cpu_clk_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct clk_hw **hws = sc2_clk_hws;
	struct clk_hw *cpu_clk_hw, *parent_clk_hw;
	struct clk *cpu_clk, *parent_clk;
	int ret;

	switch (event) {
	case PRE_RATE_CHANGE:
		parent_clk_hw = hws[CLKID_CPU_FCLK_P];
		break;
	case POST_RATE_CHANGE:
		parent_clk_hw = &sc2_sys_pll.hw;
		break;
	default:
		return NOTIFY_DONE;
	}

	cpu_clk_hw = hws[CLKID_CPU_CLK];
	cpu_clk = __clk_lookup(clk_hw_get_name(cpu_clk_hw));
	parent_clk = __clk_lookup(clk_hw_get_name(parent_clk_hw));

	ret = clk_set_parent(cpu_clk, parent_clk);
	if (ret)
		return notifier_from_errno(ret);

	udelay(80);

	return NOTIFY_OK;
}

static struct notifier_block sc2_cpu_nb_data = {
	.notifier_call = sc2_cpu_clk_notifier_cb,
};

static int sc2_dsu_mux_clk_notifier_cb(struct notifier_block *nb,
				       unsigned long event, void *data)
{
	struct clk *dsu_pre_clk, *parent_clk;
	int ret;

	switch (event) {
	case PRE_RATE_CHANGE:
		parent_clk = sc2_dsu_pre_clk_mux1.hw.clk;
		break;
	case POST_RATE_CHANGE:
		parent_clk = sc2_dsu_pre_clk_mux0.hw.clk;
		break;
	default:
		return NOTIFY_DONE;
	}

	dsu_pre_clk = sc2_dsu_pre_post_clk_mux.hw.clk;

	ret = clk_set_parent(dsu_pre_clk, parent_clk);
	if (ret)
		return notifier_from_errno(ret);

	udelay(80);

	return NOTIFY_OK;
}

static struct notifier_block sc2_dsu_nb_data = {
	.notifier_call = sc2_dsu_mux_clk_notifier_cb,
};

static void __init sc2_clkc_init(struct device_node *np)
{
	int ret = 0, clkid, i;
	void __iomem *basic_clk_base;
	void __iomem *pll_base;
	void __iomem *cpu_clk_base;

	basic_clk_base = of_iomap(np, 0);
	if (!basic_clk_base)
		return;

	pll_base = of_iomap(np, 1);
	if (!pll_base)
		return;

	cpu_clk_base = of_iomap(np, 2);
	if (!cpu_clk_base)
		return;

	/* Populate base address for PLLs */
	for (i = 0; i < ARRAY_SIZE(sc2_clk_plls); i++)
		sc2_clk_plls[i]->base = pll_base;

	/* Populate base address for MPLLs */
	for (i = 0; i < ARRAY_SIZE(sc2_clk_mplls); i++)
		sc2_clk_mplls[i]->base = pll_base;

	sc2_pcie_hcsl.reg = pll_base + (unsigned long)sc2_pcie_hcsl.reg;

	/* Populate the base address for CPU clk */
	sc2_cpu_fclk_p.reg = cpu_clk_base + (unsigned long)sc2_cpu_fclk_p.reg;
	sc2_cpu_clk.reg = cpu_clk_base + (unsigned long)sc2_cpu_clk.reg;
	sc2_cpu1_clk.reg = cpu_clk_base + (unsigned long)sc2_cpu1_clk.reg;
	sc2_cpu2_clk.reg = cpu_clk_base + (unsigned long)sc2_cpu2_clk.reg;
	sc2_cpu3_clk.reg = cpu_clk_base + (unsigned long)sc2_cpu3_clk.reg;
	sc2_dsu_pre_src_clk_mux0.reg = cpu_clk_base +
		(unsigned long)sc2_dsu_pre_src_clk_mux0.reg;
	sc2_dsu_pre_src_clk_mux1.reg = cpu_clk_base +
		(unsigned long)sc2_dsu_pre_src_clk_mux1.reg;
	sc2_dsu_clk_div0.reg = cpu_clk_base +
		(unsigned long)sc2_dsu_clk_div0.reg;
	sc2_dsu_clk_div1.reg = cpu_clk_base +
		(unsigned long)sc2_dsu_clk_div1.reg;
	sc2_dsu_pre_clk_mux0.reg = cpu_clk_base +
		(unsigned long)sc2_dsu_pre_clk_mux0.reg;
	sc2_dsu_pre_clk_mux1.reg = cpu_clk_base +
		(unsigned long)sc2_dsu_pre_clk_mux1.reg;
	sc2_dsu_pre_post_clk_mux.reg = cpu_clk_base +
		(unsigned long)sc2_dsu_pre_post_clk_mux.reg;
	sc2_dsu_pre_clk.reg = cpu_clk_base +
		(unsigned long)sc2_dsu_pre_clk.reg;
	sc2_dsu_clk.reg = cpu_clk_base +
		(unsigned long)sc2_dsu_clk.reg;

	/* Populate base address for gates */
	for (i = 0; i < ARRAY_SIZE(sc2_clk_gates); i++)
		sc2_clk_gates[i]->reg = basic_clk_base +
			(unsigned long)sc2_clk_gates[i]->reg;

	/* Populate base address for dividers  */
	for (i = 0; i < ARRAY_SIZE(sc2_clk_divs); i++)
		sc2_clk_divs[i]->reg = basic_clk_base +
			(unsigned long)sc2_clk_divs[i]->reg;

	/* Populate base address for muxs  */
	for (i = 0; i < ARRAY_SIZE(sc2_clk_muxs); i++)
		sc2_clk_muxs[i]->reg = basic_clk_base +
			(unsigned long)sc2_clk_muxs[i]->reg;

	/* Populate base address for muxs  */
	for (i = 0; i < ARRAY_SIZE(sc2_dualdiv_clks); i++)
		sc2_dualdiv_clks[i]->base = basic_clk_base;

	if (!clks) {
		clks = kcalloc(NR_CLKS, sizeof(struct clk *), GFP_KERNEL);
		if (!clks) {
			/* pr_err("%s: alloc clks fail!", __func__); */
			/* return -ENOMEM; */
			return;
		}
		clk_numbers = NR_CLKS;
	}

	clk_data.clks = clks;
	clk_data.clk_num = NR_CLKS;

	/*
	 * register all clks
	 */
	for (clkid = 0; clkid < ARRAY_SIZE(sc2_clk_hws); clkid++) {
		if (sc2_clk_hws[clkid]) {
			clks[clkid] = clk_register(NULL, sc2_clk_hws[clkid]);
			if (IS_ERR(clks[clkid])) {
				pr_err("%s: failed to register %s\n", __func__,
				       clk_hw_get_name(sc2_clk_hws[clkid]));
				goto iounmap;
			}
		}
	}

	meson_sc2_sdemmc_init(basic_clk_base);
	pr_debug("%s: register all clk ok!", __func__);

	/* set sc2_dsu_fixed_sel1 to 1G (default 24M) */
	ret = clk_set_parent(sc2_dsu_pre_src_clk_mux1.hw.clk,
			     sc2_fclk_div2.hw.clk);
	if (ret < 0) {
		pr_err("%s: failed to set parent for dsu src\n", __func__);
		goto iounmap;
	}

	/*
	 * when change sc2_dsu_pre_clk_mux0, switch to
	 * sc2_dsu_pre_clk_mux1 to avoid crash
	 */
	ret = clk_notifier_register(sc2_dsu_pre_clk_mux0.hw.clk,
				    &sc2_dsu_nb_data);
	if (ret) {
		pr_err("%s: failed to register clock notifier for dsu_clk\n",
		       __func__);
		goto iounmap;
	}

	/*
	 * Register CPU clk notifier
	 *
	 * FIXME this is wrong for a lot of reasons. First, the muxes should be
	 * struct clk_hw objects. Second, we shouldn't program the muxes in
	 * notifier handlers. The tricky programming sequence will be handled
	 * by the forthcoming coordinated clock rates mechanism once that
	 * feature is released.
	 *
	 * Furthermore, looking up the parent this way is terrible. At some
	 * point we will stop allocating a default struct clk when registering
	 * a new clk_hw, and this hack will no longer work. Releasing the ccr
	 * feature before that time solves the problem :-)
	 */
	ret = clk_notifier_register(sc2_sys_pll.hw.clk,
				    &sc2_cpu_nb_data);
		/*parent_hw = clk_hw_get_parent(&sc2_cpu_clk.mux.hw);
		 *parent_clk = parent_hw->clk;
		 *ret = clk_notifier_register(parent_clk,
		 * &sc2_cpu_clk.clk_nb);
		 */

	if (ret) {
		pr_err("%s: failed to register clock notifier for cpu_clk\n",
		       __func__);
		goto iounmap;
	}

	pr_debug("%s: cpu clk register notifier ok!", __func__);

	ret = of_clk_add_provider(np, of_clk_src_onecell_get,
				  &clk_data);
	if (ret < 0)
		pr_err("%s fail ret: %d\n", __func__, ret);
	else
		pr_info("%s initialization complete\n", __func__);
	return;

iounmap:
	iounmap(cpu_clk_base);
	iounmap(pll_base);
	iounmap(basic_clk_base);
	/* return; */
}

CLK_OF_DECLARE(sc2, "amlogic,sc2-clkc", sc2_clkc_init);

