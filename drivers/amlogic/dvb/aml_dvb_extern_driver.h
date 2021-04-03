/*
 * drivers/amlogic/dvb/aml_dvb_extern_driver.c
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

#ifndef __AML_DVB_EXTERN_DRIVER_H__
#define __AML_DVB_EXTERN_DRIVER_H__

#include <linux/device.h>
#include <dvb_frontend.h>

struct dvb_extern_device {
	char *name;
	struct class class;
	struct device *dev;

	struct proc_dir_entry *debug_proc_dir;

	/* for debug. */
	struct dvb_frontend *tuner_fe;
	struct dvb_frontend *demod_fe;
	struct analog_parameters para;

	struct gpio_config dvb_power;

	int tuner_num;
	int tuner_cur;
	int tuner_cur_attached;

	int demod_num;
	int demod_cur;
	int demod_cur_attached;
};

#endif /* __AML_DVB_EXTERN_DRIVER_H__ */
