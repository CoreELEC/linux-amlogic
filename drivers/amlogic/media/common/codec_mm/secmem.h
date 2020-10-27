/*
 * drivers/amlogic/media/common/codec_mm/secmem.h
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

#ifndef _LINUX_SECMEM_H_
#define _LINUX_SECMEM_H_

#include <linux/ioctl.h>
#include <linux/types.h>

struct secmem_block {
	__u32 paddr;
	__u32 size;
	__u32 handle;
};

#define SECMEM_IOC_MAGIC		'S'

#define SECMEM_EXPORT_DMA		_IOWR(SECMEM_IOC_MAGIC, 0x01, int)
#define SECMEM_GET_HANDLE		_IOWR(SECMEM_IOC_MAGIC, 0x02, int)
#define SECMEM_GET_PHYADDR		_IOWR(SECMEM_IOC_MAGIC, 0x03, int)
#define SECMEM_IMPORT_DMA		_IOWR(SECMEM_IOC_MAGIC, 0x04, int)

#endif /* _LINUX_SECMEM_H_ */
