/*
 * drivers/amlogic/clk/sc2/sc2_secure_clk_cpu.c
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

/*
 * CPU clock path:
 *
 *                           +-[/N]-----|3|
 *             MUX2  +--[/3]-+----------|2| MUX1
 * [sys_pll]---|1|   |--[/2]------------|1|-|1|
 *             | |---+------------------|0| | |----- [a5_clk]
 *          +--|0|                          | |
 * [xtal]---+-------------------------------|0|
 *
 *
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/arm-smccc.h>
#include "../clk-secure.h"
#include "../clkc.h"

#define to_clk_mux_divider(_hw) \
		container_of(_hw, struct meson_cpu_mux_divider, hw)

static unsigned int gap_rate = (10 * 1000 * 1000);

static const struct fclk_rate_table *dsu_cpu_clk_get_pll_settings
	(struct meson_cpu_mux_divider *pll, unsigned long rate)
{
	const struct fclk_rate_table *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_count; i++) {
		if (abs(rate - rate_table[i].rate) < gap_rate)
			return &rate_table[i];
	}
	return NULL;
}

static unsigned long sc2_dsu_cpu_clk_recalc_rate(struct clk_hw *hw,
						 unsigned long parent_rate)
{
	struct meson_cpu_mux_divider *mux_divider =
		to_clk_mux_divider(hw);
	struct parm_fclk *p_premux, *p_postmux, *p_div;
	unsigned long rate, val;
	u32 final_dyn_mask, div;
	u8 final_dyn_shift;
	struct arm_smccc_res res;

	/*pr_info("%s,clkname=%s\n", __func__, clk_hw_get_name(hw));*/
	final_dyn_mask = mux_divider->cpu_fclk_p.mask;
	final_dyn_shift = mux_divider->cpu_fclk_p.shift;
	arm_smccc_smc(CPUCLK_SECURE_RW, CLK_CPU_REG_RW, 0,
		      0, 0, 0, 0, 0, &res);

	val = res.a0;
	if ((val >> final_dyn_shift) & final_dyn_mask) {
		p_premux = &mux_divider->cpu_fclk_p10;
		p_postmux = &mux_divider->cpu_fclk_p1;
		p_div = &mux_divider->cpu_fclk_p11;
	} else {
		p_premux = &mux_divider->cpu_fclk_p00;
		p_postmux = &mux_divider->cpu_fclk_p0;
		p_div = &mux_divider->cpu_fclk_p01;
	}

	div = PARM_GET(p_div->width, p_div->shift, res.a0);
	rate = parent_rate / (div + 1);

	return rate;
}

static u8 sc2_dsu_cpu_clk_get_parent(struct clk_hw *hw)
{
	struct meson_cpu_mux_divider *mux_divider =
		to_clk_mux_divider(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 final_dyn_mask, premux_mask;
	u8 final_dyn_shift,  premux_shift;
	struct arm_smccc_res res;
	u64 val, tmp;

	final_dyn_mask = mux_divider->cpu_fclk_p.mask;
	final_dyn_shift = mux_divider->cpu_fclk_p.shift;
	arm_smccc_smc(CPUCLK_SECURE_RW, CLK_CPU_REG_RW, 0,
		      0, 0, 0, 0, 0, &res);

	val = res.a0;
	tmp = res.a0;

	if ((tmp >> final_dyn_shift) & final_dyn_mask) {
		premux_mask = mux_divider->cpu_fclk_p10.mask;
		premux_shift = mux_divider->cpu_fclk_p10.shift;
	} else {
		premux_mask = mux_divider->cpu_fclk_p00.mask;
		premux_shift = mux_divider->cpu_fclk_p00.shift;
	}

	val = (val >> premux_shift) & premux_mask;

	if (val >= num_parents)
		return -EINVAL;

	if (mux_divider->table) {
		int i;

		for (i = 0; i < num_parents; i++)
			if (mux_divider->table[i] == val)
				return i;
	}
	return val;
}

static int sc2_dsu_cpu_clk_set_parent(struct clk_hw *hw, u8 index)
{
	struct meson_cpu_mux_divider *mux_divider =
		to_clk_mux_divider(hw);
	unsigned long flags = 0;
	struct arm_smccc_res res;

	if (mux_divider->table) {
		index = mux_divider->table[index];
	} else {
		if (mux_divider->flags & CLK_MUX_INDEX_BIT)
			index = (1 << ffs(index));

		if (mux_divider->flags & CLK_MUX_INDEX_ONE)
			index++;
	}

	if (mux_divider->lock)
		spin_lock_irqsave(mux_divider->lock, flags);
	else
		__acquire(mux_divider->lock);

	arm_smccc_smc(CPUCLK_SECURE_RW, CPU_CLK_SET_PARENT, index,
		      0, 0, 0, 0, 0, &res);

	if (mux_divider->lock)
		spin_unlock_irqrestore(mux_divider->lock, flags);
	else
		__release(mux_divider->lock);

	return 0;
}

static int sc2_dsu_cpu_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct meson_cpu_mux_divider *mux_divider =
		to_clk_mux_divider(hw);
	const struct fclk_rate_table *rate_set;
	unsigned long flags = 0;
	struct arm_smccc_res res;

	if (parent_rate == 0 || rate == 0)
		return -EINVAL;

	rate_set = dsu_cpu_clk_get_pll_settings(mux_divider, rate);
	if (!rate_set)
		return -EINVAL;

	if (mux_divider->lock)
		spin_lock_irqsave(mux_divider->lock, flags);
	else
		__acquire(mux_divider->lock);

	arm_smccc_smc(CPUCLK_SECURE_RW, CPU_CLK_SET_RATE,
		      rate_set->premux, rate_set->postmux,
		      rate_set->mux_div, 0, 0, 0, &res);

	if (mux_divider->lock)
		spin_unlock_irqrestore(mux_divider->lock, flags);
	else
		__release(mux_divider->lock);

	return 0;
}

int sc2_dsu_cpu_determine_rate(struct clk_hw *hw,
			       struct clk_rate_request *req)
{
	struct clk_hw *best_parent = NULL;
	unsigned long best = 0;
	struct clk_rate_request parent_req = *req;
	struct meson_cpu_mux_divider *mux_divider =
		to_clk_mux_divider(hw);
	const struct fclk_rate_table *rate_set;
	u32 premux;

	rate_set = dsu_cpu_clk_get_pll_settings(mux_divider, req->rate);
	if (!rate_set)
		return -EINVAL;

	premux = rate_set->premux;
	best_parent = clk_hw_get_parent_by_index(hw, premux);

	if (!best_parent)
		return -EINVAL;

	best = clk_hw_get_rate(best_parent);

	if (best != parent_req.rate)
		sc2_dsu_cpu_clk_set_parent(hw, premux);

	if (best_parent)
		req->best_parent_hw = best_parent;

	req->best_parent_rate = best;

	return 0;
}

const struct clk_ops sc2_secure_dsu_cpu_clk_ops = {
	.determine_rate	= sc2_dsu_cpu_determine_rate,
	.recalc_rate	= sc2_dsu_cpu_clk_recalc_rate,
	.get_parent	= sc2_dsu_cpu_clk_get_parent,
	.set_parent	= sc2_dsu_cpu_clk_set_parent,
	.set_rate	= sc2_dsu_cpu_clk_set_rate,
};

