/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_sys.h
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

#define DEVICE_NAME		"di_multi"
#define CLASS_NAME		"deinterlace"

u8 *dim_vmap(ulong addr, u32 size, bool *bflg);
void dim_unmap_phyaddr(u8 *vaddr);
void dim_mcinfo_v_alloc(struct di_buf_s *pbuf, unsigned int bsize);
void dim_mcinfo_v_release(struct di_buf_s *pbuf);
struct dim_mm_s {
	struct page	*ppage;
	unsigned long	addr;
	unsigned int	flg;/*bit0 for tvp*/
};

bool dim_mm_alloc_api(int cma_mode, size_t count, struct dim_mm_s *o);
bool dim_mm_release_api(int cma_mode,
			struct page *pages,
			int count,
			unsigned long addr);
//bool dim_cma_top_alloc(unsigned int ch);
//bool dim_cma_top_release(unsigned int ch);

int dpst_cma_re_alloc_re_alloc(unsigned int ch);
int dpst_cma_re_alloc_unreg(unsigned int ch);
int dpst_cma_r_back_unreg(unsigned int ch);

bool dim_rev_mem_check(void);
/*--Different DI versions flag---*/
void dil_set_diffver_flag(unsigned int para);

unsigned int dil_get_diffver_flag(void);
void blk_polling(unsigned int ch, struct mtsk_cmd_s *cmd);

/* blk and mem 2020-06*/
void bufq_blk_int(struct di_ch_s *pch);
void bufq_blk_exit(struct di_ch_s *pch);

void bufq_mem_int(struct di_ch_s *pch);
void bufq_mem_exit(struct di_ch_s *pch);

bool mem_cfg(struct di_ch_s *pch);
void mem_release(struct di_ch_s *pch);
bool mem_2_blk(struct di_ch_s *pch);
void mem_release_all_inused(struct di_ch_s *pch);
void mem_release_one_inused(struct di_ch_s *pch, struct dim_mm_blk_s *blk_buf);
void mem_release_keep_back(struct di_ch_s *pch);
bool mem_cfg_realloc(struct di_ch_s *pch); /*temp for re-alloc mem*/
unsigned int  mem_release_free(struct di_ch_s *pch);
void mem_cfg_realloc_wait(struct di_ch_s *pch);

void bufq_mem_clear(struct di_ch_s *pch);
/*-------------------------*/
#endif	/*__DI_SYS_H__*/
