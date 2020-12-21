/*
 * include/linux/amlogic/aml_dtvdemod.h
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

#ifndef __AML_DTVDEMOD_H__
#define __AML_DTVDEMOD_H__

#include <linux/amlogic/aml_demod_common.h>
#include <dvb_frontend.h>

struct demod_module {
	char *name;

	u8 id;
	u8 type[AML_MAX_FE]; /* FE type supported by demod. */

	void *attach_symbol; /* The actual attach function symbolic address. */

	struct dvb_frontend *(*attach)(const struct demod_module *module,
			const struct demod_config *cfg);
	int (*detach)(const struct demod_module *module);
	int (*match)(const struct demod_module *module, int std);
	int (*detect)(const struct demod_config *cfg);
	int (*register_frontend)(struct dvb_adapter *dvb,
			struct dvb_frontend *fe);
	int (*unregister_frontend)(struct dvb_frontend *fe);
};

#if (defined CONFIG_AMLOGIC_DTV_DEMOD)
struct dvb_frontend *aml_dtvdm_attach(const struct demod_config *cfg);
#else
static inline __maybe_unused struct dvb_frontend *aml_dtvdm_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
#endif

#if (defined CONFIG_AMLOGIC_DVB_EXTERN)
enum dtv_demod_type aml_get_dtvdemod_type(const char *name);
int aml_get_dts_demod_config(struct device_node *node,
		struct demod_config *cfg, int index);
void aml_show_demod_config(const char *title, const struct demod_config *cfg);
const struct demod_module *aml_get_demod_module(enum dtv_demod_type type);
#else
static inline __maybe_unused enum dtv_demod_type aml_get_dtvdemod_type(
		const char *name)
{
	return AM_DTV_DEMOD_NONE;
}

static inline __maybe_unused int aml_get_dts_demod_config(
		struct device_node *node, struct demod_config *cfg, int index)
{
	return -ENODEV;
}

static inline __maybe_unused void aml_show_demod_config(const char *title,
		const struct demod_config *cfg)
{
}

static inline __maybe_unused const struct demod_module *aml_get_demod_module(
		enum dtv_demod_type type)
{
	return NULL;
}

#endif

/* For attach demod driver end*/
#endif /* __AML_DTVDEMOD_H__ */
