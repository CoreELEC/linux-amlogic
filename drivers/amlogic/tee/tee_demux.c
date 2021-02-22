/*
 * drivers/amlogic/tee/tee_demux.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/arm-smccc.h>
#include <linux/platform_device.h>

#include <linux/amlogic/secmon.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/tee_demux.h>
#include <asm/cputype.h>
#include <linux/printk.h>

static void __iomem *dmx_shm_input;
static void __iomem *dmx_shm_output;

static int get_share_memory(void)
{
	if (!dmx_shm_input)
		dmx_shm_input = get_secmon_sharemem_input_base();

	if (!dmx_shm_output)
		dmx_shm_output = get_secmon_sharemem_output_base();

	if (dmx_shm_input && dmx_shm_output)
		return 0;

	return -1;
}

int tee_demux_set(enum tee_dmx_cmd cmd, void *data, u32 len)
{
	struct arm_smccc_res res;

	if (!data || !len || get_share_memory())
		return -1;

	sharemem_mutex_lock();
	memcpy((void *)dmx_shm_input, (const void *)data, len);
	arm_smccc_smc(0x82000076, cmd, len, 0, 0, 0, 0, 0, &res);
	sharemem_mutex_unlock();

	return res.a0;
}
EXPORT_SYMBOL(tee_demux_set);

int tee_demux_get(enum tee_dmx_cmd cmd,
		void *in, u32 in_len, void *out, u32 out_len)
{
	struct arm_smccc_res res;

	if (!out || !out_len || get_share_memory())
		return -1;

	sharemem_mutex_lock();
	if (in && in_len)
		memcpy((void *)dmx_shm_input, (const void *)in, in_len);
	arm_smccc_smc(0x82000076, cmd, in_len, out_len, 0, 0, 0, 0, &res);
	if (!res.a0)
		memcpy((void *)out, (void *)dmx_shm_output, out_len);
	sharemem_mutex_unlock();

	return res.a0;
}
EXPORT_SYMBOL(tee_demux_get);
