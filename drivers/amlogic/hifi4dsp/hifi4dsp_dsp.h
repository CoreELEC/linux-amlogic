/*
 * drivers/amlogic/hifi4dsp/hifi4dsp_dsp.h
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

#ifndef _HIFI4DSP_DSP_H
#define _HIFI4DSP_DSP_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/irqreturn.h>

#include "hifi4dsp_firmware.h"

struct firmware;
struct hifi4dsp_pdata;
struct hifi4dsp_dsp;

/*
 * DSP memory offsets and addresses.
 */
struct hifi4dsp_addr {
	u32 smem_paddr;
	u32 smem_size;
	u32 reg_paddr;
	u32 reg_size;
	u32 fw_paddr;
	u32 fw_size;
	void __iomem *smem;
	void __iomem *reg;
	void __iomem *fw;
};

/*
 * DSP Operations exported by platform Audio DSP driver.
 */
struct hifi4dsp_ops {
	int (*boot)(struct hifi4dsp_dsp *dsp);
	int (*reset)(struct hifi4dsp_dsp *dsp);
	int (*wake)(struct hifi4dsp_dsp *dsp);
	int (*sleep)(struct hifi4dsp_dsp *dsp);
	/* Shim IO */
	void (*write)(void __iomem *addr, u32 offset, u32 value);
	u32 (*read)(void __iomem *addr, u32 offset);

	/* DSP I/DRAM IO */
	void (*ram_read)(struct hifi4dsp_dsp *dsp,
			 void  *dest, void __iomem *src, size_t bytes);
	void (*ram_write)(struct hifi4dsp_dsp *dsp,
			  void __iomem *dest, void *src, size_t bytes);
	void (*write64)(void __iomem *addr, u32 offset, u64 value);
	u64 (*read64)(void __iomem *addr, u32 offset);

	void (*dump)(struct hifi4dsp_dsp *dsp);

	/* IRQ handlers */
	irqreturn_t (*irq_handler)(int irq, void *context);

	/* hifi4dsp init and free */
	int (*init)(struct hifi4dsp_dsp *dsp, struct hifi4dsp_pdata *pdata);
	void (*free)(struct hifi4dsp_dsp *dsp);

	/* FW module parser/loader */
	int (*parse_fw)(struct hifi4dsp_firmware *dsp_fw, void *pinfo);
};

/*
 * hifi4dsp dsp device.
 */
struct hifi4dsp_dsp_device {
	struct hifi4dsp_ops *ops;
	irqreturn_t (*thread)(int irq, void *thread_context);
	void *thread_context;
};

/*
 * hifi4dsp Platform Data.
 */
struct hifi4dsp_pdata {
	int id;
	const char *name;
	int irq;
	unsigned int clk_freq;
	phys_addr_t	reg_paddr;
	unsigned int	reg_size;
	void *reg;

	/* Firmware */
	char fw_name[32];
	phys_addr_t	fw_paddr;   /*physical address of fw data*/
	void *fw_buf;
	int	 fw_size;
	int  fw_max_size;

	void *ops;
	void *dsp;		/*pointer to dsp*/
	void *priv;     /*pointer to priv*/
};

struct hifi4dsp_dsp {
	u32 id;
	int irq;
	int freq;
	int regionsize;
	/* runtime */
	spinlock_t spinlock;	/* used for IPC */
	struct mutex mutex;		/* used for fw */
	struct device *dev;
	u32 major_id;
	void *thread_context;

	struct hifi4dsp_dsp_device *dsp_dev;
	/* operations */
	struct hifi4dsp_ops *ops;

	/* base addresses */
	struct hifi4dsp_addr addr;

	/* platform data */
	struct hifi4dsp_pdata *pdata;

	/*fw support*/
	struct hifi4dsp_firmware *dsp_fw;/*def fw*/
	u32 fw_cnt;
	spinlock_t fw_spinlock; /*spinlock*/
	struct list_head fw_list;

	struct firmware *fw;

	struct clk *dsp_clk;
	struct clk *dsp_gate;

	void *info;
	void *priv;
};

void hifi4dsp_memcpy_toio_32(struct hifi4dsp_dsp *dsp,
			     void __iomem *dest,
			     void *src, size_t bytes);
void hifi4dsp_memcpy_fromio_32(struct hifi4dsp_dsp *dsp,
			       void *dest,
			       void __iomem *src,
			       size_t bytes);

int hifi4dsp_dsp_boot(struct hifi4dsp_dsp *dsp);
void hifi4dsp_dsp_reset(struct hifi4dsp_dsp *dsp);
void hifi4dsp_dsp_sleep(struct hifi4dsp_dsp *dsp);
int hifi4dsp_dsp_wake(struct hifi4dsp_dsp *dsp);
void hifi4dsp_dsp_dump(struct hifi4dsp_dsp *dsp);
//struct hifi4dsp_dsp * hifi4dsp_dsp_new(struct hifi4dsp_priv *priv,
//					 struct hifi4dsp_pdata *pdata,
//					 struct hifi4dsp_ops *ops);
struct hifi4dsp_dsp *hifi4dsp_dsp_new(struct hifi4dsp_priv *priv,
				      struct hifi4dsp_pdata *pdata,
				      struct hifi4dsp_dsp_device *dsp_dev);
#endif /*_HIFI4DSP_DSP_H*/
