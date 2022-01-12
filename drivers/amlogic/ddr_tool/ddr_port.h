/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DDR_PORT_DESC_H__
#define __DDR_PORT_DESC_H__

#define MAX_PORTS			127
#define MAX_NAME			15
#define PORT_MAJOR			32

#define DMC_TYPE_M8B			0x1B
#define DMC_TYPE_GXBB			0x1F
#define DMC_TYPE_GXTVBB			0x20
#define DMC_TYPE_GXL			0x21
#define DMC_TYPE_GXM			0x22
#define DMC_TYPE_TXL			0x23
#define DMC_TYPE_TXLX			0x24
#define DMC_TYPE_AXG			0x25
#define DMC_TYPE_GXLX			0x26
#define DMC_TYPE_TXHD			0x27

#define DMC_TYPE_G12A			0x28
#define DMC_TYPE_G12B			0x29
#define DMC_TYPE_SM1			0x2B
#define DMC_TYPE_A1			0x2C
#define DMC_TYPE_TL1			0x2E
#define DMC_TYPE_TM2			0x2F
#define DMC_TYPE_C1			0x30
#define DMC_TYPE_SC2			0x32
#define DMC_TYPE_T5			0x34
#define DMC_TYPE_T5D			0x35
#define DMC_TYPE_T7			0x36
#define DMC_TYPE_S4			0x37
#define DMC_TYPE_T3			0x38
#define DMC_TYPE_P1			0x39
#define DMC_TYPE_T5W			0x3B
#define DMC_TYPE_A5			0x3C

#define DUAL_DMC			BIT(0)
#define QUAD_DMC			BIT(2)

#define DMC_READ			0
#define DMC_WRITE			1

struct ddr_port_desc {
	char port_name[MAX_NAME];
	unsigned char port_id;
};

struct vpu_sub_desc {
	char vpu_r0_2[MAX_NAME]; /* vpu_r0 same as vpu_r2 */
	char vpu_r1[MAX_NAME];
	char vpu_w0[MAX_NAME];
	char vpu_w1[MAX_NAME];
	unsigned char sub_id;
};

int dmc_find_port_sub(int cpu_type, struct vpu_sub_desc **desc);
char *vpu_to_sub_port(char *name, char rw, int sid, char *id_str);
/*
 * This function used only during boot
 */
int ddr_find_port_desc(int cpu_type, struct ddr_port_desc **desc);
unsigned long dmc_rw(unsigned long addr, unsigned long value, int rw);
#endif /* __DDR_PORT_DESC_H__ */
