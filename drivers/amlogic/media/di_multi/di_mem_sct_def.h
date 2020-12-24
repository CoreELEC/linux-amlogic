/*
 * drivers/amlogic/media/di_multi/di_mem_sct_def.h
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

#ifndef __DI_MEM_SCT_DEF_H__
#define __DI_MEM_SCT_DEF_H__

//#define USE_DECODER_BOX (1)

#ifdef USE_DECODER_BOX
void *decoder_mmu_box_alloc_box(const char *name,
	int channel_id,
	int max_num,
	int min_size_M,
	int mem_flags);

//int decoder_mmu_box_sc_check(void *handle, int is_tvp);

int decoder_mmu_box_alloc_idx(
	void *handle, int idx, int num_pages,
	unsigned int *mmu_index_adr);

int decoder_mmu_box_free_idx_tail(void *handle, int idx,
	int start_release_index);

int decoder_mmu_box_free_idx(void *handle, int idx);
void *decoder_mmu_box_get_mem_handle(void *box_handle, int idx);

int decoder_mmu_box_free(void *handle);
#endif

int codec_mm_keeper_unmask_keeper(int keep_id, int delayms);

/*-------------------------*/
int di_mmu_box_init(void);
void di_mmu_box_exit(void);

void *di_mmu_box_alloc_box(const char *name,
	int channel_id,
	int max_num,
	int min_size_M,
	int mem_flags);

//int decoder_mmu_box_sc_check(void *handle, int is_tvp);

int di_mmu_box_alloc_idx(
	void *handle, int idx, int num_pages,
	unsigned int *mmu_index_adr);

int di_mmu_box_free_idx_tail(void *handle, int idx,
	int start_release_index);

int di_mmu_box_free_idx(void *handle, int idx);
void *di_mmu_box_get_mem_handle(void *box_handle, int idx);

int di_mmu_box_free(void *handle);

#endif	/*__DI_MEM_SCT_DEF_H__*/
