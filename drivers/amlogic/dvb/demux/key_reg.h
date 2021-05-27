/*
 * drivers/amlogic/dvb/demux/key_reg.h
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

#ifndef _KEY_REG_H_
#define _KEY_REG_H_

#define KT_REE_S17_CONFIG                          0x442408
#define KT_REE_RDY                                 ((0x0020  << 2) + 0x440500)
#define KT_REE_RDY2                                ((0x0021  << 2) + 0x440500)
#define KT_REE_CFG                                 ((0x0022  << 2) + 0x440500)
#define KT_REE_STS                                 ((0x0023  << 2) + 0x440500)
#define KT_REE_KEY0                                ((0x0024  << 2) + 0x440500)
#define KT_REE_KEY1                                ((0x0025  << 2) + 0x440500)
#define KT_REE_KEY2                                ((0x0026  << 2) + 0x440500)
#define KT_REE_KEY3                                ((0x0027  << 2) + 0x440500)

#define KTE_PENDING_OFFSET   (31)
#define KTE_STATUS_OFFSET    (29)
#define KTE_CLEAN_OFFSET     (28)
#define KTE_MODE_OFFSET      (26)
#define KTE_FLAG_OFFSET      (24)
#define KTE_KEYALGO_OFFSET   (20)
#define KTE_USERID_OFFSET    (16)
#define KTE_KTE_OFFSET       (8)
#define KTE_TEE_PRIV_OFFSET  (7)
#define KTE_LEVEL_OFFSET     (4)
/* kl_ree_cfg
 * KT Pending: 31
 * KT Ret:29
 * clean KTE:28
 * KT Mode:26
 * Decrypt:25
 * Encrypt:24
 * Algo: 20
 * User:16
 * Key Table Entry:8
 * Tee private:7
 * Protected Buffer Secure level: 4
 * Reserved: 0
 */
#endif
