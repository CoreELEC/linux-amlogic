/*
 * include/linux/amlogic/media/di/di.h
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

#ifndef DI_H
#define DI_H

void dim_post_keep_cmd_release2(struct vframe_s *vframe);

/************************************************
 * dim_polic_cfg
 ************************************************/
void dim_polic_cfg(unsigned int cmd, bool on);
#define K_DIM_BYPASS_CLEAR_ALL	(0)
#define K_DIM_I_FIRST		(1)
#define K_DIM_BYPASS_ALL_P	(2)

/************************************************
 * di_api_get_instance_id
 *	only for deinterlace
 *	get current instance_id
 ************************************************/
u32 di_api_get_instance_id(void);

/************************************************
 * di_api_post_disable
 *	only for deinterlace
 ************************************************/

void di_api_post_disable(void);

#endif /* VIDEO_H */
