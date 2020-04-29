/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_task.h
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

extern unsigned int di_dbg_task_flg;	/*debug only*/

enum ETSK_STATE {
	ETSK_STATE_IDLE,
	ETSK_STATE_WORKING,
};

void task_stop(void);
int task_start(void);

void dbg_task(void);

bool task_send_cmd(unsigned int cmd);
void task_send_ready(void);
bool task_send_cmd2(unsigned int ch, unsigned int cmd);
void task_polling_cmd_keep(unsigned int ch);

#endif /*__DI_TASK_H__*/
