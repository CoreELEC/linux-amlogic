/*
 * drivers/amlogic/media/di_multi_v3/deinterlace_dbg.h
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

#ifndef _DI_DBG_H
#define _DI_DBG_H
#include "deinterlace.h"

void dimv3_parse_cmd_params(char *buf_orig, char **parm);
void dimv3_dump_pre_stru(struct di_pre_stru_s *ppre);
void dimv3_dump_post_stru(struct di_post_stru_s *di_post_stru_p);
void dimv3_dump_di_buf(struct di_buf_s *di_buf);
void dimv3_dump_pool(struct queue_s *q);
void dimv3_dump_vframe(vframe_t *vf);
void dimv3_print_di_buf(struct di_buf_s *di_buf, int format);
void dimv3_dump_pre_mif_state(void);
void dimv3_dump_post_mif_reg(void);
void dimv3_dump_buf_addr(struct di_buf_s *di_buf, unsigned int num);
void dimv3_dump_mif_size_state(struct di_pre_stru_s *pre,
			     struct di_post_stru_s *post);
void debugv3_device_files_add(struct device *dev);
void debugv3_device_files_del(struct device *dev);
void dimv3_debugfs_init(void);
void dimv3_debugfs_exit(void);
int dimv3_state_show(struct seq_file *seq, void *v,
		   unsigned int channel);
int dimv3_dump_mif_size_state_show(struct seq_file *seq, void *v,
				   unsigned int channel);

#endif
