/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_v4l2_dbg.h
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

extern unsigned int vdin_v4l_debug;

 /*external test function*/
void vdin_parse_param(char *buf_orig, char **parm);
void vdin_v4l2_create_device_files(struct device *dev);
void vdin_v4l2_remove_device_files(struct device *dev);

