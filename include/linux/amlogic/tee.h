/*
 * include/linux/amlogic/tee.h
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

#ifndef __TEE_H__
#define __TEE_H__

extern bool tee_enabled(void);
extern int is_secload_get(void);
extern int tee_load_video_fw(uint32_t index, uint32_t vdec, bool is_swap);
#endif /* __TEE_H__ */

