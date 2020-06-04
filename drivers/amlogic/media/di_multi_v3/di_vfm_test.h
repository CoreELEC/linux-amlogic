/*
 * drivers/amlogic/media/di_multi_v3/di_vfm_test.h
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

#ifndef __DI_VFM_TEST_H__
#define __DI_VFM_TEST_H__

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

int vfmtst_que_show(struct seq_file *s, void *what);
void vfmtst_exit(void);
void vfmtst_init(void);
void dtst_prob(void);
void dtst_exit(void);

int vfmtst_vfmo_show(struct seq_file *s, void *what);
int vfmtst_vfmi_show(struct seq_file *s, void *what);
int vfmtst_bufo_show(struct seq_file *s, void *what);
void tst_new_trig_eos(bool on);

#endif /*__DI_VFM_TEST_H__*/
