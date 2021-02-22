/*
 * drivers/amlogic/media/common/resource_mgr/resourcemanage.h
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

#ifndef _RESOURCE_MANAGE_H_
#define _RESOURCE_MANAGE_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#define RESMAN_IOC_MAGIC  'R'

#define RESMAN_IOC_QUERY_RES		_IOR(RESMAN_IOC_MAGIC, 0x01, int)
#define RESMAN_IOC_ACQUIRE_RES		_IOW(RESMAN_IOC_MAGIC, 0x02, int)
#define RESMAN_IOC_RELEASE_RES		_IOR(RESMAN_IOC_MAGIC, 0x03, int)
#define RESMAN_IOC_SETAPPINFO		_IOW(RESMAN_IOC_MAGIC, 0x04, int)
#define RESMAN_IOC_SUPPORT_RES		_IOR(RESMAN_IOC_MAGIC, 0x05, int)
#define RESMAN_IOC_RELEASE_ALL		_IOR(RESMAN_IOC_MAGIC, 0x06, int)
#define RESMAN_SUPPORT_PREEMPT		1

struct resman_para {
	__u32 k;
	union {
		struct {
			__u32 preempt;
			__u32 timeout;
			char arg[32];
		} acquire;
		struct {
			char name[32];
			__u32 type;
			__s32 value;
			__s32 avail;
		} query;
		struct {
			char name[32];
		} support;
	} v;
};

struct app_info {
	char app_name[32];
	__u32 app_type;
};

enum RESMAN_ID {
	RESMAN_ID_VFM_DEFAULT,
	RESMAN_ID_AMVIDEO,
	RESMAN_ID_PIPVIDEO,
	RESMAN_ID_SEC_TVP,
	RESMAN_ID_TSPARSER,
	RESMAN_ID_CODEC_MM,
	RESMAN_ID_ADC_PLL,
	RESMAN_ID_MAX,
};

enum RESMAN_TYPE {
	RESMAN_TYPE_COUNTER = 1,
	RESMAN_TYPE_TOGGLE,
	RESMAN_TYPE_TVP,
	RESMAN_TYPE_CODEC_MM
};

enum RESMAN_APP {
	RESMAN_APP_NONE	= -1,
	RESMAN_APP_OMX	= 0,
	RESMAN_APP_XDVB,
	RESMAN_APP_HDMI_IN,
	RESMAN_APP_SEC_TVP,
	RESMAN_APP_DVBKIT,
	RESMAN_APP_TSPLAYER,
	RESMAN_APP_OTHER	= 10,
};

enum RESMAN_EVENT {
	RESMAN_EVENT_REGISTER		= 0x1000,
	RESMAN_EVENT_UNREGISTER,
	RESMAN_EVENT_PREEMPT
};

#endif/*_RESOURCE_MANAGE_H_*/
