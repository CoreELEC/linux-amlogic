/*
 * include/linux/amlogic/aml_tuner.h
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

#ifndef __AML_TUNER_H__
#define __AML_TUNER_H__

#include <linux/amlogic/aml_demod_common.h>
#include <dvb_frontend.h>

struct tuner_module {
	char *name;

	u8 id;
	u8 delsys[AML_MAX_DELSYS]; /* Delivery system supported by tuner. */
	u8 type[AML_MAX_FE]; /* FE type supported by tuner. */

	void *attach_symbol; /* The actual attach function symbolic address. */

	struct dvb_frontend *(*attach)(const struct tuner_module *module,
			struct dvb_frontend *fe, const struct tuner_config *cfg);
	int (*detach)(const struct tuner_module *module);
	int (*match)(const struct tuner_module *module, int std);
	int (*detect)(const struct tuner_config *cfg);
};

#if (defined CONFIG_AMLOGIC_DVB_EXTERN)
enum tuner_type aml_get_tuner_type(const char *name);
int aml_get_dts_tuner_config(struct device_node *node,
		struct tuner_config *cfg, int index);
void aml_show_tuner_config(const char *title, const struct tuner_config *cfg);
const struct tuner_module *aml_get_tuner_module(enum tuner_type type);
#else
static inline __maybe_unused enum tuner_type aml_get_tuner_type(
		const char *name)
{
	return AM_TUNER_NONE;
}

static inline __maybe_unused int aml_get_dts_tuner_config(
		struct device_node *node, struct tuner_config *cfg, int index)
{
	return -ENODEV;
}

static inline __maybe_unused void aml_show_tuner_config(const char *title,
		const struct tuner_config *cfg)
{
}

static inline __maybe_unused const struct tuner_module *aml_get_tuner_module(
		enum tuner_type type)
{
	return NULL;
}
#endif

#endif /* __AML_TUNER_H__ */
