/*
 * drivers/amlogic/clk/sc2/sc2_clk_secure.c
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
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/arm-smccc.h>
#include "../clk-secure.h"

static int clk_secure_gate_enable(struct clk_hw *hw)
{
	struct clk_gate *gate = to_clk_gate(hw);
	struct arm_smccc_res res;

	arm_smccc_smc(CLK_SECURE_RW, CLK_REG_RW, 1,
		      gate->bit_idx, 0, 0, 0, 0, &res);

	return 0;
}

static void clk_secure_gate_disable(struct clk_hw *hw)
{
	struct clk_gate *gate = to_clk_gate(hw);
	struct arm_smccc_res res;

	arm_smccc_smc(CLK_SECURE_RW, CLK_REG_RW, 0,
		      gate->bit_idx, 0, 0, 0, 0, &res);
}

static int clk_secure_gate_is_enabled(struct clk_hw *hw)
{
	u32 reg;
	struct clk_gate *gate = to_clk_gate(hw);

	reg = clk_readl(gate->reg);

	/* if a set bit disables this clk, flip it before masking */
	if (gate->flags & CLK_GATE_SET_TO_DISABLE)
		reg ^= BIT(gate->bit_idx);

	reg &= BIT(gate->bit_idx);

	return reg ? 1 : 0;
}

const struct clk_ops clk_secure_gate_ops = {
	.enable = clk_secure_gate_enable,
	.disable = clk_secure_gate_disable,
	.is_enabled = clk_secure_gate_is_enabled,
};
EXPORT_SYMBOL_GPL(clk_secure_gate_ops);
