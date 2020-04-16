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

#define RESMAN_IOC_MAGIC  'R'

#define RESMAN_IOC_QUERY_RES		_IOR(RESMAN_IOC_MAGIC, 0x01, int)
#define RESMAN_IOC_ACQUIRE_RES		_IOW(RESMAN_IOC_MAGIC, 0x02, int)
#define RESMAN_IOC_RELEASE_RES		_IOR(RESMAN_IOC_MAGIC, 0x03, int)
#define RESMAN_IOC_SETAPPINFO		_IOW(RESMAN_IOC_MAGIC, 0x04, int)
#define RESMAN_IOC_SUPPORT_RES		_IOR(RESMAN_IOC_MAGIC, 0x05, int)

#define BASE_AVAILAB_RES		(0x0)
#define VFM_DEFAULT			(BASE_AVAILAB_RES + 0)
#define AMVIDEO			(BASE_AVAILAB_RES + 1)
#define PIPVIDEO			(BASE_AVAILAB_RES + 2)
#define SEC_TVP			(BASE_AVAILAB_RES + 3)
#define TSPARSER			(BASE_AVAILAB_RES + 4)
#define MAX_AVAILAB_RES		(TSPARSER + 1)

struct resman_para {
	int para_in;
	char para_str[32];
};

struct app_info {
	char app_name[32];
	int app_type;
};

enum APP_TYPE {
	TYPE_NONE	= -1,
	TYPE_OMX	= 0,
	TYPE_DVB,
	TYPE_HDMI_IN,
	TYPE_SEC_TVP,
	TYPE_OTHER	= 10,
};
#endif/*_RESOURCE_MANAGE_H_*/
