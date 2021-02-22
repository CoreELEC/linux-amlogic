/*
 * drivers/amlogic/dvb/demux/aml_dvb.h
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

#ifndef _AML_DVB_H_
#define _AML_DVB_H_

#include "aml_dmx.h"
#include "aml_dsc.h"
//#include "hw_demux/hwdemux.h"

#define DMX_DEV_COUNT     32
#define DSC_DEV_COUNT     DMX_DEV_COUNT
/*TSIN just 4, but maxlinear maybe send 8 TS at one TSIN.*/
/*will save all dts config*/
#define FE_DEV_COUNT	  8
#define TS_HEADER_LEN    12

enum {
	AM_TS_DISABLE,
	AM_TS_PARALLEL,
	AM_TS_SERIAL_3WIRE,
	AM_TS_SERIAL_4WIRE
};

struct aml_s2p {
	int invert;
};

struct aml_ts_input {
	int mode;
	struct pinctrl *pinctrl;
	int control;
	int ts_sid;
	int header_len;
	int header[TS_HEADER_LEN];
	int sid_offset;
};

struct aml_dvb {
	struct dvb_device dvb_dev;
	struct dvb_adapter dvb_adapter;

	struct device *dev;
	struct platform_device *pdev;

	struct swdmx_ts_parser *tsp[DMX_DEV_COUNT];
	struct swdmx_demux *swdmx[DMX_DEV_COUNT];
	struct aml_dmx dmx[DMX_DEV_COUNT];
	struct aml_dsc dsc[DSC_DEV_COUNT];
	struct aml_ts_input ts[FE_DEV_COUNT];
	/*protect many user operate*/
	struct mutex mutex;
	/*protect register operate*/
	spinlock_t slock;

	/*1: dsc connect demod
	 *0: dsc connect local
	 */
	unsigned int dsc_pipeline;
};

struct aml_dvb *aml_get_dvb_device(void);
struct device *aml_get_device(void);

extern int is_security_dmx;

#endif
