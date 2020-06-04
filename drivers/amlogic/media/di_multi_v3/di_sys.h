/*
 * drivers/amlogic/media/di_multi_v3/di_sys.h
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

#ifndef __DI_SYS_H__
#define __DI_SYS_H__

#define DEVICE_NAME		"di_multi_v3"
#define CLASS_NAME		"deinterlace"

u8 *dimv3_vmap(ulong addr, u32 size, bool *bflg);
void dimv3_unmap_phyaddr(u8 *vaddr);
void dimv3_mcinfo_v_alloc(struct di_buf_s *pbuf, unsigned int bsize);
void dimv3_mcinfo_v_release(struct di_buf_s *pbuf);

struct dim_mm_s {
	struct page	*ppage;
	unsigned long	addr;
};

bool dimv3_mm_alloc_api(int cma_mode, size_t count, struct dim_mm_s *o);
bool dimv3_mm_release_api(int cma_mode,
			  struct page *pages,
			  int count,
			  unsigned long addr);
bool dimv3_cma_top_alloc(unsigned int ch);
bool dimv3_cma_top_release(unsigned int ch);
bool dimv3_rev_mem_check(void);
/*--Different DI versions flag---*/
void dil_set_diffver_flag(unsigned int para);

unsigned int dil_get_diffver_flag(void);
/*-------------------------*/
#endif	/*__DI_SYS_H__*/
