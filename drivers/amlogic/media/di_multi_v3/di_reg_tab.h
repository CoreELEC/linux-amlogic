/*
 * drivers/amlogic/media/di_multi_v3/di_reg_tab.h
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

#ifndef __DI_REG_TABL_H__
#define __DI_REG_TABL_H__

int regv3_con_show(struct seq_file *seq, void *v);

bool dimv3_wr_cue_int(void);
int dimv3_reg_cue_int_show(struct seq_file *seq, void *v);

#endif	/*__DI_REG_TABL_H__*/
