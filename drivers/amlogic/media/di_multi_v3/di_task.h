/*
 * drivers/amlogic/media/di_multi_v3/di_task.h
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

#ifndef __DI_TASK_H__
#define __DI_TASK_H__

extern unsigned int div3_dbg_task_flg;	/*debug only*/

enum eTSK_STATE {
	eTSK_STATE_IDLE,
	eTSK_STATE_WORKING,
};

void taskv3_stop(void);
int taskv3_start(void);

void dbgv3_task(void);

bool taskv3_send_cmd(unsigned int cmd);
void taskv3_send_ready(void);
void dimv3_htr_prob(void);
void dimv3_htr_start(unsigned int ch);
void dimv3_htr_stop(unsigned int ch);
void dimv3_htr_con_update(unsigned int mask, bool on);
const char *dimv3_htr_get_stsname(void);

#endif /*__DI_TASK_H__*/
