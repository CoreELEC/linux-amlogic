/*
 * drivers/amlogic/soc_info/soc_info.h
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

#ifndef __SOC_INFO_H
#define __SOC_INFO_H

#define SOCDATA_DEVICE_NAME	"socdata"
#define SOCDATA_CLASS_NAME "socdata"
#define NOCS_DATA_LENGTH (48)
#define CMD_SOCVER1_DATA _IO('s', 0x01)
#define CMD_SOCVER2_DATA  _IO('s', 0x02)
#define CMD_POC_DATA  _IO('s', 0x03)
#define CMD_NOCSDATA_READ  _IO('s', 0x04)
#define CMD_NOCSDATA_WRITE  _IO('s', 0x05)

#define EFUSE_HAL_NOCS_API_READ	 4
#define EFUSE_HAL_NOCS_API_WRITE 5

extern unsigned int read_nocsdata_cmd;
extern unsigned int write_nocsdata_cmd;
ssize_t nocsdata_read(char *buf, size_t count, loff_t *ppos);
ssize_t nocsdata_write(const char *buf, size_t count, loff_t *ppos);

/* efuse HAL_API arg */
struct efuse_hal_api_arg {
	unsigned int cmd;		/* R/W */
	unsigned int offset;
	unsigned int size;
	unsigned long buffer;
	unsigned long retcnt;
};
#endif
