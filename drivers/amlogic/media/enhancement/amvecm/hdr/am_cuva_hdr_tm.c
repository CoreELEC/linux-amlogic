/*
 * drivers/amlogic/media/enhancement/amvecm/hdr/am_cuva_hdr_tm.c
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

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/amvecm/cuva_alg.h>
#include "am_cuva_hdr_tm.h"
#include "am_hdr10_plus.h"

static s64 lut_ogain[149];
static s64 lut_cgain[65];
char cuva_ver[] = "cuva-2021-07-02";

struct aml_gain_reg vm_reg = {
	.ogain_lut = lut_ogain,
	.cgain_lut = lut_cgain,
};

struct aml_gain_reg *get_gain_lut(void)
{
	return &vm_reg;
}

struct aml_cuva_data_s cuva_data = {
	.max_panel_e = 657,
	.min_panel_e = 0,
	.aml_vm_regs = &vm_reg,
	.cuva_md = &cuva_metadata,
	.cuva_hdr_alg = NULL,
};

struct aml_cuva_data_s *get_cuva_data(void)
{
	return &cuva_data;
}
EXPORT_SYMBOL(get_cuva_data);

int cuva_hdr_dbg(void)
{
	struct aml_cuva_data_s *cuva_data = get_cuva_data();

	pr_info("max_panel_e = %d\n", cuva_data->max_panel_e);
	pr_info("min_panel_e = %d\n", cuva_data->min_panel_e);
	pr_info("cuva_hdr_alg = %p\n", cuva_data->cuva_hdr_alg);
	pr_info(" ver: %s\n", cuva_ver);

	return 0;
}

