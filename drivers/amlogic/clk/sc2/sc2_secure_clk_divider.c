/*
 * drivers/amlogic/clk/sc2/sc2_secure_clk_divider.c
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
#include <linux/string.h>
#include <linux/log2.h>
#include <linux/arm-smccc.h>

#include "../clkc.h"
#include "../clk-secure.h"

/*
 * DOC: basic adjustable divider clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clk_prepare only ensures that parents are prepared
 * enable - clk_enable only ensures that parents are enabled
 * rate - rate is adjustable.  clk->rate = ceiling(parent->rate / divisor)
 * parent - fixed parent.  No clk_set_parent support
 */

#define div_mask(width)	((1 << (width)) - 1)

static unsigned int _get_table_div(const struct clk_div_table *table,
				   unsigned int val)
{
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->val == val)
			return clkt->div;
	return 0;
}

static unsigned int _get_div(const struct clk_div_table *table,
			     unsigned int val, unsigned long flags, u8 width)
{
	if (flags & CLK_DIVIDER_ONE_BASED)
		return val;
	if (flags & CLK_DIVIDER_POWER_OF_TWO)
		return 1 << val;
	if (flags & CLK_DIVIDER_MAX_AT_ZERO)
		return val ? val : div_mask(width) + 1;
	if (table)
		return _get_table_div(table, val);
	return val + 1;
}

static unsigned long sc2_clk_divider_recalc_rate(struct clk_hw *hw,
						 unsigned long parent_rate)
{
	struct clk_divider *divider = to_clk_divider(hw);
	unsigned long val;
	struct arm_smccc_res res;

	arm_smccc_smc(CPUCLK_SECURE_RW, CLK_DSU_REG_RW,
		      0, 0, 0, 0, 0, 0, &res);
	val = res.a0;
	val >>= divider->shift;
	val &= div_mask(divider->width);

	return divider_recalc_rate(hw, parent_rate, val, divider->table,
				   divider->flags);
}

static long sc2_clk_divider_round_rate(struct clk_hw *hw, unsigned long rate,
				       unsigned long *prate)
{
	struct clk_divider *divider = to_clk_divider(hw);
	u64 bestdiv;
	struct arm_smccc_res res;

	/* if read only, just return current value */
	if (divider->flags & CLK_DIVIDER_READ_ONLY) {
		arm_smccc_smc(CPUCLK_SECURE_RW, CLK_DSU_REG_RW,
			      0, 0, 0, 0, 0, 0, &res);
		bestdiv = res.a0;
		bestdiv >>= divider->shift;
		bestdiv &= div_mask(divider->width);
		bestdiv = _get_div(divider->table, bestdiv, divider->flags,
				   divider->width);
		if (!bestdiv)
			bestdiv = 1;
		return DIV_ROUND_UP_ULL((u64)*prate, bestdiv);
	}

	return divider_round_rate(hw, rate, prate, divider->table,
				  divider->width, divider->flags);
}

static int sc2_clk_divider_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct clk_divider *divider = to_clk_divider(hw);
	unsigned int value;
	unsigned long flags = 0;
	struct arm_smccc_res res;

	value = divider_get_val(rate, parent_rate, divider->table,
				divider->width, divider->flags);

	if (divider->lock)
		spin_lock_irqsave(divider->lock, flags);
	else
		__acquire(divider->lock);

	arm_smccc_smc(CPUCLK_SECURE_RW, DSU_DIVIDER_SET_RATE, value,
		      divider->width, divider->shift, 0, 0, 0, &res);

	if (divider->lock)
		spin_unlock_irqrestore(divider->lock, flags);
	else
		__release(divider->lock);

	return 0;
}

const struct clk_ops sc2_clk_divider_ops = {
	.recalc_rate = sc2_clk_divider_recalc_rate,
	.round_rate = sc2_clk_divider_round_rate,
	.set_rate = sc2_clk_divider_set_rate,
};
EXPORT_SYMBOL_GPL(sc2_clk_divider_ops);

const struct clk_ops sc2_clk_divider_ro_ops = {
	.recalc_rate = sc2_clk_divider_recalc_rate,
	.round_rate = sc2_clk_divider_round_rate,
};
EXPORT_SYMBOL_GPL(sc2_clk_divider_ro_ops);

