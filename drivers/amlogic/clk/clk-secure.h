/*
 * drivers/amlogic/clk/clk-secure.h
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

#ifndef __CLK_SECURE_H
#define __CLK_SECURE_H

#include <linux/clk-provider.h>

#define CLK_SECURE_RW	0x8200009A

#define CLK_SMC_READ 0
#define CLK_SMC_WRITE 1

#define SYS_PLL_STEP0 0
#define SYS_PLL_STEP1 1
#define SYS_PLL_DISABLE 2
#define GP1_PLL_STEP0 3
#define GP1_PLL_STEP1 4
#define GP1_PLL_DISABLE 5
#define CLK_REG_RW 6

extern const struct clk_ops clk_secure_gate_ops;
extern const struct clk_ops meson_sc2_secure_pll_ops;

#endif /* __CLK_SECURE_H */
