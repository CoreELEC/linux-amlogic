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

struct tuner_frontend {
	struct platform_device *pdev;
	struct class class;
	struct mutex mutex;

	struct tuner_config cfg;
	struct analog_parameters param;

	enum fe_type fe_type;

	void *private;
};

#if (defined CONFIG_AMLOGIC_DVB_EXTERN)
enum tuner_type aml_get_tuner_type(const char *name);

struct dvb_frontend *aml_attach_detach_tuner(
		const enum tuner_type type,
		struct dvb_frontend *fe,
		struct tuner_config *cfg,
		int attach);
#else
static inline __maybe_unused enum tuner_type aml_get_tuner_type(
		const char *name)
{
	return AM_TUNER_NONE;
}

static inline __maybe_unused struct dvb_frontend *aml_attach_detach_tuner(
		const enum tuner_type type,
		struct dvb_frontend *fe,
		struct tuner_config *cfg,
		int attach)
{
	return NULL;
}
#endif

static __maybe_unused struct dvb_frontend *aml_attach_tuner(
		const enum tuner_type type,
		struct dvb_frontend *fe,
		struct tuner_config *cfg)
{
	return aml_attach_detach_tuner(type, fe, cfg, 1);
}

static __maybe_unused int aml_detach_tuner(const enum tuner_type type)
{
	aml_attach_detach_tuner(type, NULL, NULL, 0);

	return 0;
}

#if (defined CONFIG_AMLOGIC_DVB_EXTERN)
int aml_get_dts_tuner_config(struct device_node *node,
		struct tuner_config *cfg, int index);
#else
static inline __maybe_unused int aml_get_dts_tuner_config(
		struct device_node *node, struct tuner_config *cfg, int index)
{
	return 0;
}
#endif

#endif /* __AML_TUNER_H__ */
