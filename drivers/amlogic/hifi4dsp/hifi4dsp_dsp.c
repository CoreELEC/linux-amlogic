/*
 * drivers/amlogic/hifi4dsp/hifi4dsp_dsp.c
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
//#define DEBUG

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/amlogic/major.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"

int hifi4dsp_dsp_boot(struct hifi4dsp_dsp *dsp)
{
	if (dsp->ops->boot)
		dsp->ops->boot(dsp);
	pr_debug("%s done\n", __func__);
	return 0;
}

void hifi4dsp_dsp_reset(struct hifi4dsp_dsp *dsp)
{
	if (dsp->ops->reset)
		dsp->ops->reset(dsp);
	pr_debug("%s done\n", __func__);
}

void hifi4dsp_dsp_sleep(struct hifi4dsp_dsp *dsp)
{
	if (dsp->ops->sleep)
		dsp->ops->sleep(dsp);
}

int hifi4dsp_dsp_wake(struct hifi4dsp_dsp *dsp)
{
	if (dsp->ops->wake)
		return dsp->ops->wake(dsp);
	return 0;
}

void hifi4dsp_dsp_dump(struct hifi4dsp_dsp *dsp)
{
	if (dsp->ops->dump)
		dsp->ops->dump(dsp);
}

struct hifi4dsp_dsp *hifi4dsp_dsp_new(struct hifi4dsp_priv *priv,
				      struct hifi4dsp_pdata *pdata,
				      struct hifi4dsp_dsp_device *dsp_dev)
{
	int err = 0;
	struct hifi4dsp_dsp *dsp;

	dsp = kzalloc(sizeof(*dsp), GFP_KERNEL);
	if (!dsp)
		goto dsp_malloc_error;

	mutex_init(&dsp->mutex);
	spin_lock_init(&dsp->spinlock);
	spin_lock_init(&dsp->fw_spinlock);
	INIT_LIST_HEAD(&dsp->fw_list);

	dsp->id = pdata->id;
	dsp->irq = pdata->irq;
	dsp->major_id = MAJOR(priv->dev->devt);
	dsp->dev = priv->dev;
	dsp->pdata = pdata;
	dsp->priv = priv;
	dsp->ops = dsp_dev->ops;

	/* Initialise Audio DSP */
	if (dsp->ops->init) {
		err = dsp->ops->init(dsp, pdata);
		if (err < 0)
			return NULL;
	}

	/*Register the ISR here if necessary*/
	/*
	 * err = request_threaded_irq(dsp->irq, dsp->ops->irq_handler,
	 *	dsp_dev->thread, IRQF_SHARED, "HIFI4DSP", dsp);
	 * if (err)
	 *	goto irq_err;
	 */
	goto dsp_new_done;
	/*
	 * irq_err:
	 *	if (dsp->ops->free)
	 *		dsp->ops->free(dsp);
	 */
dsp_malloc_error:
	return NULL;
dsp_new_done:
	return dsp;
}

