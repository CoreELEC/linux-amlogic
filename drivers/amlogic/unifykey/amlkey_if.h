/*
 * drivers/amlogic/unifykey/amlkey_if.h
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

#ifndef __AMLKEY_IF_H__
#define __AMLKEY_IF_H__

struct amlkey_if {
	s32 (*init)(u8 *seed, u32 len, int encrypt_type);
	u32 (*exsit)(const u8 *name);
	unsigned int (*size)(const u8 *name);
	u32 (*get_attr)(const u8 *name);
	unsigned int (*read)(const u8 *name, u8 *buffer, u32 len);
	ssize_t (*write)(const u8 *name, u8 *buffer, u32 len, u32 attr);
	s32 (*hash)(const u8 *name, u8 *hash);
};

extern struct amlkey_if *amlkey_if;

static inline u32 amlkey_get_attr(const u8 *name)
{
	return amlkey_if->get_attr(name);
}

int amlkey_if_init(struct platform_device *pdev, int secure);

/*
 * init
 */
int32_t amlkey_init_m8b(uint8_t *seed, uint32_t len, int encrypt_type);
static inline s32 amlkey_init_gen(u8 *seed, u32 len, int encrypt_type)
{
	return amlkey_if->init(seed, len, encrypt_type);
}

/*
 * query if the key already programmed
 * exsit 1, non 0
 */
static inline u32 amlkey_isexsit(const u8 *name)
{
	return amlkey_if->exsit(name);
}
/*
 * query if the prgrammed key is secure
 * secure 1, non 0
 */
int32_t amlkey_issecure(const uint8_t *name);
/*
 * query if the prgrammed key is encrypt
 * return encrypt 1, non 0;
 */
int32_t amlkey_isencrypt(const uint8_t *name);
/*
 * actual bytes of key value
 */
static inline unsigned int amlkey_size(const u8 *name)
{
	return amlkey_if->size(name);
}
/*
 * read non-secure key in bytes, return byets readback actully.
 */
static inline unsigned int amlkey_read(const u8 *name, u8 *buffer, u32 len)
{
	return amlkey_if->read(name, buffer, len);
}

/*
 * write key with attr in bytes , return bytes readback actully
 *	attr: bit0, secure/non-secure
 *		  bit8, encrypt/non-encrypt
 */
static inline ssize_t amlkey_write(const u8 *name, u8 *buffer, u32 len,
				   u32 attr)
{
	return amlkey_if->write(name, buffer, len, attr);
}

/*
 * get the hash value of programmed secure key | 32bytes length, sha256
 */
static inline s32 amlkey_hash_4_secure(const u8 *name, u8 *hash)
{
	return amlkey_if->hash(name, hash);
}

extern int32_t nand_key_read(uint8_t *buf,
			uint32_t len, uint32_t *actual_length);

extern int32_t nand_key_write(uint8_t *buf,
			uint32_t len, uint32_t *actual_length);

extern int32_t emmc_key_read(uint8_t *buf,
			uint32_t len, uint32_t *actual_length);

extern int32_t emmc_key_write(uint8_t *buf,
			uint32_t len, uint32_t *actual_length);

#endif

