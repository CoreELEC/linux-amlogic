/*
 * drivers/amlogic/media/di_multi_v3/di_pres.h
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

#ifndef __DI_PRES_H__
#define __DI_PRES_H__

void dimv3_pre_de_done_buf_config(struct di_ch_s *pch,
				  bool flg_timeout);
void addv3_dummy_vframe_type_pre(struct di_buf_s *src_buf, unsigned int ch);

void div3_vf_l_put(struct vframe_s *vf, struct di_ch_s *pch);
struct vframe_s *div3_vf_l_get(struct di_ch_s *pch);
struct vframe_s *div3_vf_l_peek(struct di_ch_s *pch);

#endif	/*__DI_PRES_H__*/
