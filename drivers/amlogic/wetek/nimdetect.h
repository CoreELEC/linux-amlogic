/*
 * Wetek NIM tuner(s) detection
 *
 * Copyright (C) 2014 Sasa Savic <sasa.savic.sr@gmail.com>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#ifndef __NIMDETECT_H
#define __NIMDETECT_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/dvb/version.h>
#include <linux/platform_device.h>
#include "dvb_frontend.h"

struct ts_input {
	int                  mode;
	struct pinctrl      *pinctrl;
	int                  control;
};

struct wetek_nims {
	struct dvb_frontend 	*fe[2];
	struct i2c_adapter 		*i2c[2];	
	struct ts_input	   		ts[3];
	struct device       	*dev;
	struct platform_device  *pdev;
	struct pinctrl      	*card_pinctrl;
	u32 total_nims;
#ifdef CONFIG_ARM64
	int fec_reset;
	int power_ctrl;
#endif
};

void get_nims_infos(struct wetek_nims *p);
int set_external_vol_gpio(int *demod_id, int on);

#endif /* __NIMDETECT_H */
