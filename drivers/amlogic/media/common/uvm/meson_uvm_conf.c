/*
 * drivers/amlogic/media/common/uvm/meson_uvm_conf.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>

int use_uvm;
EXPORT_SYMBOL(use_uvm);

static int __init set_use_uvm(char *str)
{
	if (!str)
		use_uvm = 0;
	if (strcmp(str, "1") == 0)
		use_uvm = 1;
	else
		use_uvm = 0;
	return 0;
}
__setup("use_uvm=", set_use_uvm);
