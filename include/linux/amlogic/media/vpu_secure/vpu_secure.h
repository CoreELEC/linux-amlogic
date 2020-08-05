/*
 * include/linux/amlogic/media/vpu_secure/vpu_secure.h
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

#ifndef VPU_SECURE_H_
#define VPU_SECURE_H_

enum secure_module_e {
	OSD_MODULE,
	VIDEO_MODULE,
	DI_MODULE,
	VDIN_MODULE,
};

#define VPP_OUTPUT_SECURE       BIT(8)
#define MALI_AFBCD_SECURE       BIT(7)
#define DV_INPUT_SECURE         BIT(6)
#define AFBCD_INPUT_SECURE      BIT(5)
#define OSD3_INPUT_SECURE       BIT(4)
#define VD2_INPUT_SECURE        BIT(3)
#define VD1_INPUT_SECURE        BIT(2)
#define OSD2_INPUT_SECURE       BIT(1)
#define OSD1_INPUT_SECURE       BIT(0)

int secure_register(enum secure_module_e module,
		    int config_delay,
		    void *reg_op,
		    void *cb);
int secure_unregister(enum secure_module_e module);
int secure_config(enum secure_module_e module, int secure_src);
#endif
