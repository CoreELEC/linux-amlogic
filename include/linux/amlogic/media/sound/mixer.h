/*
 * include/linux/amlogic/media/sound/mixer.h
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

#ifndef __MIXER_H__
#define __MIXER_H__

#include <sound/control.h>

static int snd_int_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffffffff;
	uinfo->count = 1;

	return 0;
}

#define SND_INT(xname, xhandler_get, xhandler_put)     \
{                                      \
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name  = xname,               \
	.info  = snd_int_info,        \
	.get   = xhandler_get,        \
	.put   = xhandler_put,        \
}

/*
 * IEC958 controller(mixer) functions
 *
 *	Channel status get/put control
 *	User bit value get/put control
 *	Valid bit value get control
 *	DPLL lock status get control
 *	User bit sync mode selection control
 */
static int IEC958_info(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;

	return 0;
}

#define SND_IEC958(xname, xhandler_get, xhandler_put) \
{                                          \
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,   \
	.name = xname,           \
	.info = IEC958_info,      \
	.get = xhandler_get,     \
	.put = xhandler_put,     \
}

#endif
