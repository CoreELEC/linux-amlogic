/*
 * drivers/amlogic/mtd/nand_bootpart.h
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

#ifndef _AML_NAND_BOOTPART_
#define _AML_NAND_BOOTPART_

struct nand_startup_parameter {
	int page_size;
	int block_size;
	int layout_reserve_size;
	int pages_per_block;
	int setup_data;
	int page0_disable;
};

#define BL2E_STORAGE_PARAM_SIZE		(0x80)
#define BOOT_FIRST_BLOB_SIZE		(254 * 1024)
#define BOOT_FILLER_SIZE	(4 * 1024)
#define BOOT_RESERVED_SIZE	(4 * 1024)
#define BOOT_RANDOM_NONCE	(16)
#define BOOT_BL2E_SIZE		(66672) //74864-8K
#define BOOT_EBL2E_SIZE		\
	(BOOT_FILLER_SIZE + BOOT_RESERVED_SIZE + BOOT_BL2E_SIZE)
#define BOOT_BL2X_SIZE		(66672)
#define MAX_BOOT_AREA_ENTRIES	(8)
#define BL2_CORE_BASE_OFFSET_EMMC	(0x200000)
#define BOOT_AREA_BB1ST             (0)
#define BOOT_AREA_BL2E              (1)
#define BOOT_AREA_BL2X              (2)
#define BOOT_AREA_DDRFIP            (3)
#define BOOT_AREA_DEVFIP            (4)
#define BOOT_AREA_INVALID           (MAX_BOOT_AREA_ENTRIES)
#define BAE_BB1ST                   "1STBLOB"
#define BAE_BL2E                    "BL2E"
#define BAE_BL2X                    "BL2X"
#define BAE_DDRFIP                  "DDRFIP"
#define BAE_DEVFIP                  "DEVFIP"

struct boot_area_entry {
	char name[11];
	unsigned char idx;
	u64 offset;
	u64 size;
};

struct boot_layout {
	struct boot_area_entry *boot_entry;
};

struct storage_boot_entry {
	unsigned int offset;
	unsigned int size;
};

union storage_independent_parameter {
	struct nand_startup_parameter nsp;
};

struct storage_startup_parameter {
	unsigned char boot_device;
	unsigned char	boot_seq;
	unsigned char	boot_bakups;
	unsigned char reserved;
	struct storage_boot_entry boot_entry[MAX_BOOT_AREA_ENTRIES];
	union storage_independent_parameter sip;
};

extern struct storage_startup_parameter g_ssp;
int aml_nand_param_check_and_layout_init(void);
#endif

