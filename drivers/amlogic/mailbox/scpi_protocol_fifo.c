/*
 * drivers/amlogic/mailbox/scpi_protocol_fifo.c
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
#include <linux/err.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/mailbox_client.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/slab.h>
#include "meson_mhu_fifo.h"

#define SYNC_CMD_TAG			BIT(26)
#define ASYNC_CMD_TAG			BIT(25)

struct scpi_data_buf {
	int client_id;
	struct mhu_data_buf *data;
	struct completion complete;
	int async;
};

enum scpi_error_codes {
	SCPI_SUCCESS = 0, /* Success */
	SCPI_ERR_PARAM = 1, /* Invalid parameter(s) */
	SCPI_ERR_ALIGN = 2, /* Invalid alignment */
	SCPI_ERR_SIZE = 3, /* Invalid size */
	SCPI_ERR_HANDLER = 4, /* Invalid handler/callback */
	SCPI_ERR_ACCESS = 5, /* Invalid access/permission denied */
	SCPI_ERR_RANGE = 6, /* Value out of range */
	SCPI_ERR_TIMEOUT = 7, /* Timeout has occurred */
	SCPI_ERR_NOMEM = 8, /* Invalid memory area or pointer */
	SCPI_ERR_PWRSTATE = 9, /* Invalid power state */
	SCPI_ERR_SUPPORT = 10, /* Not supported or disabled */
	SCPI_ERR_DEVICE = 11, /* Device error */
	SCPI_ERR_MAX
};

static int scpi_linux_errmap[SCPI_ERR_MAX] = {
	0, -EINVAL, -ENOEXEC, -EMSGSIZE,
	-EINVAL, -EACCES, -ERANGE, -ETIMEDOUT,
	-ENOMEM, -EINVAL, -EOPNOTSUPP, -EIO,
};

static inline int scpi_to_linux_errno(int errno)
{
	if (errno >= SCPI_SUCCESS && errno < SCPI_ERR_MAX)
		return scpi_linux_errmap[errno];
	return -EIO;
}

static void scpi_rx_callback(struct mbox_client *cl, void *msg)
{
	struct mhu_data_buf *data = (struct mhu_data_buf *)msg;
	struct scpi_data_buf *scpi_buf = data->cl_data;

	complete(&scpi_buf->complete);
}

static int send_scpi_cmd(struct scpi_data_buf *scpi_buf, int client_id)
{
	struct mbox_chan *chan;
	struct mbox_client cl = {0};
	struct mhu_data_buf *data = scpi_buf->data;
	u32 status;

	cl.dev = scpi_device;
	cl.rx_callback = scpi_rx_callback;
	pr_info("scpi_dev %p %d\n", scpi_device, client_id);
	chan = mbox_request_channel(&cl, client_id);
	if (IS_ERR(chan))
		return PTR_ERR(chan);

	init_completion(&scpi_buf->complete);
	if (mbox_send_message(chan, (void *)data) < 0) {
		status = SCPI_ERR_TIMEOUT;
		goto free_channel;
	}

	wait_for_completion(&scpi_buf->complete);
	status = SCPI_SUCCESS;

free_channel:
	mbox_free_channel(chan);

	return scpi_to_linux_errno(status);
}

#define SCPI_SETUP_DBUF_SIZE(scpi_buf, mhu_buf, _client_id,\
			_cmd, _tx_buf, _tx_size, _rx_buf, _rx_size, _async) \
do {						\
	struct mhu_data_buf *pdata = &(mhu_buf);\
	struct scpi_data_buf *psdata = &(scpi_buf); \
	pdata->cmd = _cmd;			\
	pdata->tx_buf = _tx_buf;		\
	pdata->tx_size = _tx_size;		\
	pdata->rx_buf = _rx_buf;		\
	pdata->rx_size = _rx_size;		\
	psdata->client_id = _client_id;		\
	psdata->data = pdata;			\
	psdata->async = _async;			\
} while (0)

static int scpi_execute_cmd(struct scpi_data_buf *scpi_buf)
{
	struct mhu_data_buf *data;

	if (!scpi_buf || !scpi_buf->data)
		return -EINVAL;
	data = scpi_buf->data;
	data->cmd = (data->cmd & 0xffff)
		    | (data->tx_size & 0x1ff) << 16
		    | (scpi_buf->async); //Sync Flag
	data->cl_data = scpi_buf;

	return send_scpi_cmd(scpi_buf, scpi_buf->client_id);
}

static int scpi_get_c_by_name(struct device *dev, int num, char *inname)
{
	int index = 0;
	int ret = 0;
	const char *name = NULL;

	for (index = 0; index < num; index++) {
		ret = of_property_read_string_index(dev->of_node,
						    "mbox-names",
						     index, &name);
		if (ret) {
			dev_err(dev, "%s get mbox[%d] name fail\n",
				__func__, index);
		} else {
			if (!strncmp(name, inname, strlen(inname)))
				return index;
		}
	}
	return -ENODEV;
}

static int scpi_get_chan(int channel)
{
	struct device *dev = scpi_device;
	int nums = 0;

	of_property_read_u32(dev->of_node,
			     "mbox-nums", &nums);
	if (nums == 0 || nums > CHANNEL_MAX) {
		pr_err("scpi get mbox node fail\n");
		return -ENXIO;
	}

	pr_info("%s,%d, num:%d\n", __func__, __LINE__, nums);
	switch (channel) {
	case SCPI_DSPA:
		return scpi_get_c_by_name(dev, nums, "ap_to_dspa");
	case SCPI_DSPB:
		return scpi_get_c_by_name(dev, nums, "ap_to_dspb");
	case SCPI_AOCPU:
		return scpi_get_c_by_name(dev, nums, "ap_to_ao");
	default:
		return -ENXIO;
	}

	return -ENXIO;
}

int scpi_send_fifo(void *data, int size, int channel,
		   int cmd, int taskid, void *revdata, int revsize)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	struct mbox_data packdata;
	struct mbox_data upackdata;
	int sync = SYNC_CMD_TAG;
	int chan = 0;

	chan = scpi_get_chan(channel);
	if (chan < 0) {
		pr_err("scpi get mbox channel id fail\n");
		return -ENXIO;
	}

	memset(&upackdata, 0, sizeof(upackdata));
	memset(&packdata, 0, sizeof(packdata));

	packdata.task = taskid;
	memcpy(&packdata.data, data, size);

	SCPI_SETUP_DBUF_SIZE(sdata, mdata, chan,
			     cmd, (void *)&packdata,
			     sizeof(packdata), &upackdata,
			     sizeof(upackdata), sync);

	if (scpi_execute_cmd(&sdata))
		return -EPERM;

	pr_info("%s,%d\n", __func__, __LINE__);

	if (upackdata.status == 1) {
		pr_info("%s,%d\n", __func__, __LINE__);
		memcpy(revdata, &upackdata.data, revsize);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(scpi_send_fifo);
