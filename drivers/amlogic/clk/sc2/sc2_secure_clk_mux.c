/*
 * drivers/amlogic/clk/sc2/sc2_secure_clk_mux.c
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

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/arm-smccc.h>

#include "../clkc.h"
#include "../clk-secure.h"

/*
 * DOC: basic adjustable multiplexer clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clk_prepare only ensures that parents are prepared
 * enable - clk_enable only ensures that parents are enabled
 * rate - rate is only affected by parent switching.  No clk_set_rate support
 * parent - parent is adjustable through clk_set_parent
 */

static u8 sc2_clk_mux_get_parent(struct clk_hw *hw)
{
	struct clk_mux *mux = to_clk_mux(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	struct arm_smccc_res res;
	u64 val;

	if (!strcmp(clk_hw_get_name(hw), "cpu_clk"))
		arm_smccc_smc(CPUCLK_SECURE_RW, CLK_CPU_REG_RW,
			      0, 0, 0, 0, 0, 0, &res);
	else if (!strcmp(clk_hw_get_name(hw), "cpu1_clk") ||
		 !strcmp(clk_hw_get_name(hw), "cpu2_clk") ||
		 !strcmp(clk_hw_get_name(hw), "cpu3_clk") ||
		 !strcmp(clk_hw_get_name(hw), "dsu_clk"))
		arm_smccc_smc(CPUCLK_SECURE_RW, CLK_CPU_DSU_REG_RW,
			      0, 0, 0, 0, 0, 0, &res);
	else
		arm_smccc_smc(CPUCLK_SECURE_RW, CLK_DSU_REG_RW,
			      0, 0, 0, 0, 0, 0, &res);

	val = res.a0;
	val >>= mux->shift;
	val &= mux->mask;

	if (mux->table) {
		int i;

		for (i = 0; i < num_parents; i++)
			if (mux->table[i] == val)
				return i;
		return -EINVAL;
	}

	if (val && (mux->flags & CLK_MUX_INDEX_BIT))
		val = ffs(val) - 1;

	if (val && (mux->flags & CLK_MUX_INDEX_ONE))
		val--;

	if (val >= num_parents)
		return -EINVAL;

	return val;
}

static int sc2_clk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_mux *mux = to_clk_mux(hw);
	unsigned long flags = 0;
	struct arm_smccc_res res;

	if (mux->table) {
		index = mux->table[index];
	} else {
		if (mux->flags & CLK_MUX_INDEX_BIT)
			index = 1 << index;

		if (mux->flags & CLK_MUX_INDEX_ONE)
			index++;
	}

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);
	else
		__acquire(mux->lock);

	if (!strcmp(clk_hw_get_name(hw), "cpu_clk"))
		arm_smccc_smc(CPUCLK_SECURE_RW, SET_CPU0_MUX_PARENT,
			      index, mux->mask, mux->shift, 0, 0, 0, &res);
	else if (!strcmp(clk_hw_get_name(hw), "cpu1_clk") ||
		 !strcmp(clk_hw_get_name(hw), "cpu2_clk") ||
		 !strcmp(clk_hw_get_name(hw), "cpu3_clk") ||
		 !strcmp(clk_hw_get_name(hw), "dsu_clk"))
		arm_smccc_smc(CPUCLK_SECURE_RW, SET_CPU123_DSU_MUX_PARENT,
			      index, mux->mask, mux->shift, 0, 0, 0, &res);
	else
		arm_smccc_smc(CPUCLK_SECURE_RW, SET_DSU_PRE_MUX_PARENT,
			      index, mux->mask, mux->shift, 0, 0, 0, &res);

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	else
		__release(mux->lock);

	return 0;
}

const struct clk_ops sc2_clk_mux_ops = {
	.get_parent = sc2_clk_mux_get_parent,
	.set_parent = sc2_clk_mux_set_parent,
	.determine_rate = __clk_mux_determine_rate,
};
EXPORT_SYMBOL_GPL(sc2_clk_mux_ops);

const struct clk_ops sc2_clk_mux_ro_ops = {
	.get_parent = sc2_clk_mux_get_parent,
};
EXPORT_SYMBOL_GPL(sc2_clk_mux_ro_ops);
