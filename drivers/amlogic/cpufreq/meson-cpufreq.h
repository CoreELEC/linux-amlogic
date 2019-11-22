/*
 * drivers/amlogic/cpufreq/meson-cpufreq.h
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

#ifndef __MESON_CPUFREQ_H
#define __MESON_CPUFREQ_H
/* Currently we support only two clusters */
#define MAX_CLUSTERS	2

/*core power supply*/
#define CORE_SUPPLY "cpu"

/* Core Clocks */
#define CORE_CLK	"core_clk"
#define LOW_FREQ_CLK_PARENT	"low_freq_clk_parent"
#define HIGH_FREQ_CLK_PARENT	"high_freq_clk_parent"
#define DSU_CLK		"dsu_clk"
#define DSU_PRE_PARENT "dsu_pre_parent"
#define to_meson_dvfs_cpu_nb(_nb) container_of(_nb,	\
		struct meson_cpufreq_driver_data, freq_transition)

static struct clk *clk[MAX_CLUSTERS];
static struct cpufreq_frequency_table *freq_table[MAX_CLUSTERS];

/* Default voltage_tolerance */
#define DEF_VOLT_TOL		0
#define CLK_DIV		12
#define EFUSE_CPUFREQ_MIN 1500
/*mid rate for set parent,Khz*/
static unsigned int mid_rate = (1000 * 1000);
static unsigned int gap_rate = (10 * 1000 * 1000);

/*
 * DSU_LOW_RATE:cpu clk less than DSU_LOW_RATE(1.2G)
 * dsu clk swith to cpu clk
 * DSU_HIGH_RATE:cpu clk between 1.2G to DSU_HIGH_RATE (1.8G)
 * dsu clk set to DSU_LOW_RATE(1.2G)
 * CPU_CMP_RATE: cpu clk greater than CPU_CMP_RATE(1.8G)
 * dsu clk set to DSU_HIGH_RATE(1.5G)
 */

#define DSU_LOW_RATE (1200 * 1000)
#define DSU_HIGH_RATE (1500 * 1000)
#define CPU_CMP_RATE (1800 * 1000)

unsigned int gp1_clk_target;
/*whether use different tables or not*/
bool cpufreq_tables_supply;
#define GET_DVFS_TABLE_INDEX           0x82000088

struct meson_cpufreq_driver_data {
	struct device *cpu_dev;
	struct regulator *reg;
	struct cpufreq_policy *policy;
	/* voltage tolerance in percentage */
	unsigned int volt_tol;
	struct clk *high_freq_clk_p;
	struct clk *low_freq_clk_p;
	struct clk *clk_dsu;
	struct clk *clk_dsu_pre;
	struct notifier_block freq_transition;
};

static struct mutex cluster_lock[MAX_CLUSTERS];
static unsigned int meson_cpufreq_get_rate(unsigned int cpu);
static unsigned int meson_cpufreq_set_rate(struct cpufreq_policy *policy,
					   u32 cur_cluster, u32 rate);
static int meson_regulator_set_volate(struct regulator *regulator, int old_uv,
				      int new_uv, int tol_uv);
int choose_cpufreq_tables_index(const struct device_node *np, u32 cur_cluster);
#endif /* __MESON_CPUFREQ_H */
