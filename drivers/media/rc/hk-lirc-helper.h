/*
 * Driver for configuration of remote wakeup functionality
 *   Avilable to assign user remote wakeup key
 *   and its decode protocol
 *
 * Copyright (C) 2019 Hardkernel Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef _HK_LIRC_HELPER_H_
#define _HK_LIRC_HELPER_H_

struct remote_reg_map {
	unsigned int reg;
	unsigned int val;
};

struct remote_reg_proto {
	char *name;
	int decode_type;
	struct remote_reg_map *reg_map;
	int reg_map_size;
};

/*
 * refer to protocol type definitions
 * in include/dt-bindings/input/meson_rc.h
 */
enum ir_decode_type {
	IR_DECODE_UNKNOWN = 0x0,
	IR_DECODE_NEC,
	IR_DECODE_DUOKAN,
	IR_DECODE_XMP_1,
	IR_DECODE_RC5,
	IR_DECODE_RC6,
	IR_DECODE_XMP_1_SW,
	IR_DECODE_NEC_SW,
	IR_DECODE_LEGACY_NEC,
	IR_DECODE_RC6_21BIT,
};

enum remote_reg {
	REG_LDR_ACTIVE = 0x00<<2,
	REG_LDR_IDLE   = 0x01<<2,
	REG_LDR_REPEAT = 0x02<<2,
	REG_BIT_0      = 0x03<<2,
	REG_REG0       = 0x04<<2,
	REG_FRAME      = 0x05<<2,
	REG_STATUS     = 0x06<<2,
	REG_REG1       = 0x07<<2,
	REG_REG2       = 0x08<<2,
	REG_DURATN2    = 0x09<<2,
	REG_DURATN3    = 0x0a<<2,
	REG_FRAME1     = 0x0b<<2,
	REG_STATUS1    = 0x0c<<2,
	REG_STATUS2    = 0x0d<<2,
	REG_REG3       = 0x0e<<2,
	REG_FRAME_RSV0 = 0x0f<<2,
	REG_FRAME_RSV1 = 0x10<<2
};

static struct remote_reg_map regs_legacy_nec[] = {
	{REG_LDR_ACTIVE,    (500 << 16) | (400 << 0)},
	{REG_LDR_IDLE,      300 << 16 | 200 << 0},
	{REG_LDR_REPEAT,    150 << 16 | 80 << 0},
	{REG_BIT_0,         72 << 16 | 40 << 0 },
	{REG_REG0,          7 << 28 | (0xFA0 << 12) | 0x13},
	{REG_STATUS,        (134 << 20) | (90 << 10)},
	{REG_REG1,          0xbe00},
};

static struct remote_reg_map regs_default_nec[] = {
	{ REG_LDR_ACTIVE,   (500 << 16) | (400 << 0)},
	{ REG_LDR_IDLE,     300 << 16 | 200 << 0},
	{ REG_LDR_REPEAT,   150 << 16 | 80 << 0},
	{ REG_BIT_0,        72 << 16 | 40 << 0},
	{ REG_REG0,         7 << 28 | (0xFA0 << 12) | 0x13},
	{ REG_STATUS,       (134 << 20) | (90 << 10)},
	{ REG_REG1,         0x9f00},
	{ REG_REG2,         0x00},
	{ REG_DURATN2,      0x00},
	{ REG_DURATN3,      0x00}
};

static struct remote_reg_map regs_default_duokan[] = {
	{ REG_LDR_ACTIVE,   ((70 << 16) | (30 << 0))},
	{ REG_LDR_IDLE,     ((50 << 16) | (15 << 0))},
	{ REG_LDR_REPEAT,   ((30 << 16) | (26 << 0))},
	{ REG_BIT_0,        ((66 << 16) | (40 << 0))},
	{ REG_REG0,         ((3 << 28) | (0x4e2 << 12) | (0x13))},
	{ REG_STATUS,       ((80 << 20) | (66 << 10))},
	{ REG_REG1,         0x9300},
	{ REG_REG2,         0xb90b},
	{ REG_DURATN2,      ((97 << 16) | (80 << 0))},
	{ REG_DURATN3,      ((120 << 16) | (97 << 0))},
	{ REG_REG3,         5000<<0}
};

static struct remote_reg_map regs_default_xmp_1_sw[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        0},
	{ REG_REG0,        ((3 << 28) | (0xFA0 << 12) | (9))},
	{ REG_STATUS,       0},
	{ REG_REG1,         0x8574},
	{ REG_REG2,         0x02},
	{ REG_DURATN2,      0},
	{ REG_DURATN3,      0},
	{ REG_REG3,         0}
};

static struct remote_reg_map regs_default_xmp_1[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        (52 << 16) | (45<<0)},
	{ REG_REG0,         ((7 << 28) | (0x5DC << 12) | (0x13))},
	{ REG_STATUS,       (87 << 20) | (80 << 10)},
	{ REG_REG1,         0x9f00},
	{ REG_REG2,         0xa90e},
	/*n=10,758+137*10=2128us,2128/20= 106*/
	{ REG_DURATN2,      (121<<16) | (114<<0)},
	{ REG_DURATN3,      (7<<16) | (7<<0)},
	{ REG_REG3,         0}
};

static struct remote_reg_map regs_default_nec_sw[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        0},
	{ REG_REG0,        ((3 << 28) | (0xFA0 << 12) | (9))},
	{ REG_STATUS,       0},
	{ REG_REG1,         0x8574},
	{ REG_REG2,         0x02},
	{ REG_DURATN2,      0},
	{ REG_DURATN3,      0},
	{ REG_REG3,         0}
};

static struct remote_reg_map regs_default_rc5[] = {
	{ REG_LDR_ACTIVE,   0},
	{ REG_LDR_IDLE,     0},
	{ REG_LDR_REPEAT,   0},
	{ REG_BIT_0,        0},
	{ REG_REG0,         ((3 << 28) | (0x1644 << 12) | 0x13)},
	{ REG_STATUS,       (1 << 30)},
	{ REG_REG1,         ((1 << 15) | (13 << 8))},
	/*bit[0-3]: RC5; bit[8]: MSB first mode; bit[11]: compare frame method*/
	{ REG_REG2,         ((1 << 13) | (1 << 11) | (1 << 8) | 0x7)},
	/*Half bit for RC5 format: 888.89us*/
	{ REG_DURATN2,      ((49 << 16) | (40 << 0))},
	/*RC5 typically 1777.78us for whole bit*/
	{ REG_DURATN3,      ((94 << 16) | (83 << 0))},
	{ REG_REG3,         0}
};

static struct remote_reg_map regs_default_rc6[] = {
	{REG_LDR_ACTIVE,    (210 << 16) | (125 << 0)},
	/*rca leader 4000us,200* timebase*/
	{REG_LDR_IDLE,      50 << 16 | 38 << 0}, /* leader idle 400*/
	{REG_LDR_REPEAT,    145 << 16 | 125 << 0}, /* leader repeat*/
	/* logic '0' or '00' 1500us*/
	{REG_BIT_0,         51 << 16 | 38 << 0 },
	{REG_REG0,          (7 << 28)|(0xFA0 << 12)|0x13},
	/* sys clock boby time.base time = 20 body frame*/
	{REG_STATUS,       (94 << 20) | (82 << 10)},
	/*20bit:9440 32bit:9f40 36bit:a340 37bit:a440*/
	{REG_REG1,         0xa440},
	/*it may get the wrong customer value and key value from register if
	 *the value is set to 0x4,so the register value must set to 0x104
	 */
	{REG_REG2,         0x2909},
	{REG_DURATN2,      ((28 << 16) | (16 << 0))},
	{REG_DURATN3,      ((51 << 16) | (38 << 0))},
};

static struct remote_reg_map regs_default_rc6_21bit[] = {
	{REG_LDR_ACTIVE,    (210 << 16) | (125 << 0)},
	/*rca leader 4000us,200* timebase*/
	{REG_LDR_IDLE,      50 << 16 | 38 << 0}, /* leader idle 400*/
	{REG_LDR_REPEAT,    145 << 16 | 125 << 0}, /* leader repeat*/
	/* logic '0' or '00' 1500us*/
	{REG_BIT_0,         51 << 16 | 38 << 0 },
	{REG_REG0,          (7 << 28)|(0xFA0 << 12)|0x13},
	/* sys clock boby time.base time = 20 body frame*/
	{REG_STATUS,       (94 << 20) | (82 << 10)},
	/*20bit:9440 32bit:9f40 36bit:a340 37bit:a440*/
	{REG_REG1,         0x9440},
	/*it may get the wrong customer value and key value from register if
	 *the value is set to 0x4,so the register value must set to 0x104
	 */
	{REG_REG2,         0x2909},
	{REG_DURATN2,      ((28 << 16) | (16 << 0))},
	{REG_DURATN3,      ((51 << 16) | (38 << 0))},
};

static struct remote_reg_proto reg_legacy_nec = {
	.name     = "LEGACY_NEC",
	.decode_type = IR_DECODE_LEGACY_NEC,
	.reg_map      = regs_legacy_nec,
	.reg_map_size = ARRAY_SIZE(regs_legacy_nec),
};

static struct remote_reg_proto reg_nec = {
	.name     = "NEC",
	.decode_type = IR_DECODE_NEC,
	.reg_map      = regs_default_nec,
	.reg_map_size = ARRAY_SIZE(regs_default_nec),
};

static struct remote_reg_proto reg_duokan = {
	.name	  = "DUOKAN",
	.decode_type = IR_DECODE_DUOKAN,
	.reg_map      = regs_default_duokan,
	.reg_map_size = ARRAY_SIZE(regs_default_duokan),
};

static struct remote_reg_proto reg_xmp_1_sw = {
	.name	  = "XMP-1-RAW",
	.decode_type = IR_DECODE_XMP_1_SW,
	.reg_map      = regs_default_xmp_1_sw,
	.reg_map_size = ARRAY_SIZE(regs_default_xmp_1_sw),
};

static struct remote_reg_proto reg_xmp_1 = {
	.name	  = "XMP-1",
	.decode_type = IR_DECODE_XMP_1,
	.reg_map      = regs_default_xmp_1,
	.reg_map_size = ARRAY_SIZE(regs_default_xmp_1),
};

static struct remote_reg_proto reg_nec_sw = {
	.name	  = "NEC-SW",
	.decode_type = IR_DECODE_NEC_SW,
	.reg_map      = regs_default_nec_sw,
	.reg_map_size = ARRAY_SIZE(regs_default_nec_sw),
};

static struct remote_reg_proto reg_rc5 = {
	.name	  = "RC5",
	.decode_type = IR_DECODE_RC5,
	.reg_map      = regs_default_rc5,
	.reg_map_size = ARRAY_SIZE(regs_default_rc5),
};

static struct remote_reg_proto reg_rc6 = {
	.name	  = "RC6",
	.decode_type = IR_DECODE_RC6,
	.reg_map      = regs_default_rc6,
	.reg_map_size = ARRAY_SIZE(regs_default_rc6),
};

static struct remote_reg_proto reg_rc6_21bit = {
	.name	  = "RC6_21BIT",
	.decode_type = IR_DECODE_RC6_21BIT,
	.reg_map      = regs_default_rc6_21bit,
	.reg_map_size = ARRAY_SIZE(regs_default_rc6_21bit),
};

#endif /* _HK_LIRC_HELPER_H_ */
