/*
 * Pin controller and GPIO driver for Amlogic Meson8b.
 *
 * Copyright (C) 2015 Endless Mobile, Inc.
 * Author: Carlo Caione <carlo@endlessm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <dt-bindings/gpio/meson8b-gpio.h>
#include "pinctrl-meson.h"

#define AO_OFF	130


struct meson_pinctrl_data meson8b_cbus_pinctrl_data = {
	.name		= "cbus-banks",
	.pin_base	= 0,
};

struct meson_pinctrl_data meson8b_aobus_pinctrl_data = {
	.name		= "aobus-banks",
	.pin_base	= 130,
};
