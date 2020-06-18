/*
 * drivers/amlogic/dvb/demux/sc2_demux/frontend.h
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

#ifndef _FRONTEND_H_
#define _FRONTEND_H_
#include <linux/platform_device.h>

int frontend_probe(struct platform_device *pdev);
int frontend_remove(void);
void frontend_config_ts_sid(void);

ssize_t tuner_setting_show(struct class *class,
			   struct class_attribute *attr, char *buf);

ssize_t tuner_setting_store(struct class *class,
			    struct class_attribute *attr,
			    const char *buf, size_t count);
ssize_t ts_setting_show(struct class *class,
			struct class_attribute *attr, char *buf);
ssize_t ts_setting_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count);

#endif
