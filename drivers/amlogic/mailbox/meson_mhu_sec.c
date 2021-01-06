/*
 * drivers/amlogic/mailbox/meson_mhu_fifo.c
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
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/mailbox_client.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>

#include "meson_mhu_sec.h"

#define DRIVER_NAME		"secmbox"

#define MSGFULL			0x1
#define MBOX_SEC_TIMEOUT	10000

struct mbox_data {
	char data[MBOX_SEC_SIZE];
} __packed;

struct mhu_ctlr {
	struct device *dev;
	void __iomem *nee2scpu_csr;
	void __iomem *scpu2nee_csr;
	void __iomem *nee2scpu_data_st;
	void __iomem *scpu2nee_data_st;
	struct mbox_controller mbox_con;
	struct mhu_chan *channels;
};

static struct list_head mbox_devs = LIST_HEAD_INIT(mbox_devs);
static struct class *mbox_class;

struct l_msg {
	u32 msg_full;
	struct completion complete;
	char data[MBOX_SEC_SIZE];
} __packed;

/*for store listen message data*/
static struct l_msg lmsg;

struct sp_mb_csr {
	union {
		u32 d32;
		struct {
			/* words lengthm1   [7:0]   */
			unsigned lengthm1:8;
			/* Full             [8]     */
			unsigned full:1;
			/* Empty            [9]     */
			unsigned empty:1;
			/* Reserved         [15:10] */
			unsigned reserved_1:6;
			/* A2B disable      [16]    */
			unsigned a2b_dis:1;
			/* A2B state        [19:17] */
			unsigned a2b_st:3;
			/* A2B error        [20]    */
			unsigned a2b_err:1;
			/* Reserved         [31:21] */
			unsigned reserved_2:11;
		} b;
	} reg;
};

/*
 * This function writes size of data into buffer of mailbox
 * for sending.
 *
 * Note:
 * Due to hardware design, data size for mailbox send/recv
 * should be a **MULTIPLE** of 4 bytes.
 */
static void mb_write(void *to, void *from, u32 size)
{
	u32 *rd_ptr = (u32 *)from;
	u32 *wr_ptr = (u32 *)to;
	u32 i = 0;

	if (!rd_ptr || !wr_ptr) {
		pr_debug("Invalid inputs for mb_write_buf!\n");
		return;
	}

	for (i = 0; i < size; i += sizeof(u32))
		writel(*rd_ptr++, wr_ptr++);
}

/*
 * This function read size of data from buffer of mailbox
 * for receiving.
 *
 * Note:
 * Due to hardware design, data size for mailbox send/recv
 * should be a **MULTIPLE** of 4 bytes.
 */
static void mb_read(void *to, void *from, u32 size)
{
	u32 *rd_ptr = (u32 *)from;
	u32 *wr_ptr = (u32 *)to;
	u32 i = 0;

	if (!rd_ptr || !wr_ptr) {
		pr_debug("Invalid inputs for mb_read_buf!\n");
		return;
	}

	for (i = 0; i < size; i += sizeof(u32))
		*wr_ptr++ = readl(rd_ptr++);
}

static void mbox_chan_report(u32 size, void *msg, int idx)
{
	struct mhu_data_buf *data_buf = (struct mhu_data_buf *)msg;

	if (!lmsg.msg_full)
		lmsg.msg_full = MSGFULL;
	else
		pr_err("replace listen msg data, old data loss!!!\n");

	memcpy(&lmsg.data,
	       data_buf->rx_buf,
	       size);
	complete(&lmsg.complete);
}

static irqreturn_t mbox_handler(int irq, void *p)
{
	struct mbox_chan *link = (struct mbox_chan *)p;
	struct mhu_chan *mhu_chan = link->con_priv;
	struct mhu_ctlr *ctlr = mhu_chan->ctlr;
	int idx = mhu_chan->index;
	void __iomem *mbox_scpu2nee_csr = ctlr->scpu2nee_csr;
	void __iomem *mbox_scpu2nee_data_st = ctlr->scpu2nee_data_st;
	struct mhu_data_buf *data;
	struct sp_mb_csr vcsr;
	u32 rx_size;

	vcsr.reg.d32 = readl(mbox_scpu2nee_csr);
	rx_size = (vcsr.reg.b.lengthm1 + 1) * sizeof(u32);

	if (vcsr.reg.b.full) {
		data = mhu_chan->data;
		if (!data)
			return IRQ_NONE;

		if (data->rx_buf) {
			mb_read(data->rx_buf, mbox_scpu2nee_data_st, rx_size);
			data->rx_size = rx_size;
			mbox_chan_report(rx_size, data, idx);
		}
	}

	return IRQ_HANDLED;
}

static int mhu_send_data(struct mbox_chan *link, void *msg)
{
	struct mhu_chan *chan = link->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_nee2scpu_csr = ctlr->nee2scpu_csr;
	void __iomem *mbox_nee2scpu_data_st = ctlr->nee2scpu_data_st;
	struct mhu_data_buf *data = (struct mhu_data_buf *)msg;
	u32 tx_size = data->tx_size & 0xff;
	struct sp_mb_csr vcsr;

	if (!data)
		return -EINVAL;

	chan->data = data;

	vcsr.reg.d32 = readl(mbox_nee2scpu_csr);
	vcsr.reg.b.lengthm1 = (tx_size / sizeof(u32)) - 1;
	writel(vcsr.reg.d32, mbox_nee2scpu_csr);

	if (data->tx_buf) {
		mb_write(mbox_nee2scpu_data_st,
			 data->tx_buf, tx_size);
	}
	return 0;
}

static int mhu_startup(struct mbox_chan *link)
{
	return 0;
}

static void mhu_shutdown(struct mbox_chan *link)
{
}

static bool mhu_last_tx_done(struct mbox_chan *link)
{
	struct mhu_chan *chan = link->con_priv;
	struct mhu_ctlr *ctlr = chan->ctlr;
	void __iomem *mbox_nee2scpu_csr = ctlr->nee2scpu_csr;
	struct sp_mb_csr vcsr;

	vcsr.reg.d32 = readl(mbox_nee2scpu_csr);

	return !(vcsr.reg.b.empty);
}

static struct mbox_chan_ops mhu_ops = {
	.send_data = mhu_send_data,
	.startup = mhu_startup,
	.shutdown = mhu_shutdown,
	.last_tx_done = mhu_last_tx_done,
};

static ssize_t mbox_message_write(struct file *filp,
				  const char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	int ret;
	struct mbox_client cl = {0};
	struct mbox_chan *chan;
	struct mbox_data mbox_data;
	struct mhu_data_buf data_buf;
	struct mhu_mbox *mbox_dev = filp->private_data;
	struct device *dev = mbox_dev->mhu_dev;
	int send_channel =  mbox_dev->channel_id;

	if (lmsg.msg_full) {
		dev_err(dev,
			"send fail, listem msg is full\n");
		return -EINVAL;
	}

	if (count > MBOX_SEC_SIZE) {
		dev_err(dev,
			"Message length %zd over range\n",
			count);
		return -EINVAL;
	}
	ret = copy_from_user(mbox_data.data, userbuf,
			     count);
	if (ret) {
		ret = -EFAULT;
		goto err_probe0;
	}

	data_buf.tx_buf = (void *)&mbox_data;
	data_buf.tx_size = count;
	data_buf.rx_buf = NULL;
	data_buf.rx_size = 0;
	cl.dev = dev;
	cl.tx_block = false;/*this diff other mhu*/
	cl.tx_tout = MBOX_TIME_OUT;
	cl.rx_callback = NULL;
	mutex_lock(&mbox_dev->mutex);
	init_completion(&lmsg.complete);
	chan = mbox_request_channel(&cl, send_channel);
	if (IS_ERR(chan)) {
		mutex_unlock(&mbox_dev->mutex);
		dev_err(dev, "Failed Req Chan\n");
		ret = PTR_ERR(chan);
		goto err_probe0;
	}
	ret = mbox_send_message(chan, (void *)(&data_buf));
	if (ret < 0) {
		pr_info("%s, %d, %d\n", __func__, __LINE__, ret);
		dev_err(dev, "Failed to send message via mailbox %d\n", ret);
	}

	mbox_free_channel(chan);
	mutex_unlock(&mbox_dev->mutex);
err_probe0:
	return ret;
}

static ssize_t mbox_message_read(struct file *filp, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	int ret;
	unsigned long flags;
	struct mhu_mbox *mbox_dev = filp->private_data;
	struct device *dev = mbox_dev->mhu_dev;
	unsigned long wait;

	if (!lmsg.msg_full) {
		wait = msecs_to_jiffies(MBOX_SEC_TIMEOUT);
		ret =
		wait_for_completion_killable_timeout(&lmsg.complete,
						      wait);
		if (ret <= 0) {
			dev_err(dev, "Cannot wait ack msg, timeout!!!\n");
			return -ENXIO;
		}
	}

	*ppos = 0;

	ret = simple_read_from_buffer(userbuf, count, ppos,
				      &lmsg.data, MBOX_SEC_SIZE);

	spin_lock_irqsave(&mbox_dev->mhu_lock, flags);
	memset(&lmsg, 0, sizeof(lmsg));
	spin_unlock_irqrestore(&mbox_dev->mhu_lock, flags);
	return ret;
}

static int mbox_message_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev = inode->i_cdev;
	struct mhu_mbox *dev = container_of(cdev, struct mhu_mbox, char_cdev);

	filp->private_data = dev;
	return 0;
}

static const struct file_operations mbox_message_ops = {
	.write		= mbox_message_write,
	.read		= mbox_message_read,
	.open		= mbox_message_open,
};

static void mhu_cleanup_devs(void)
{
	struct mhu_mbox *cur, *n;

	list_for_each_entry_safe(cur, n, &mbox_devs, char_list) {
		if (cur->char_dev) {
			cdev_del(&cur->char_cdev);
			device_del(cur->char_dev);
		}
		list_del(&cur->char_list);
		kfree(cur);
	}
}

static int mhu_cdev_init(struct device *dev, struct mhu_ctlr *mhu_ctlr)
{
	struct mhu_chan *mhu_chan;
	dev_t char_dev;
	int char_major;
	const char *name = NULL;
	int mbdevs, mbox_nums = 0;
	int index, i;
	int err = 0;

	of_property_read_u32(dev->of_node,
			     "mbox-nums", &mbox_nums);
	if (mbox_nums == 0 || mbox_nums > CHANNEL_SEC_MAX)
		mbox_nums = CHANNEL_SEC_MAX;

	mbdevs = mbox_nums;

	mbox_class = class_create(THIS_MODULE, "secmbox");
	if (IS_ERR(mbox_class))
		goto err;

	err = alloc_chrdev_region(&char_dev, 0, mbdevs, DRIVER_NAME);
	if (err < 0) {
		dev_err(dev, "%s mhu alloc dev_t number failed\n", __func__);
		err = -1;
		goto class_err;
	}

	char_major = MAJOR(char_dev);
	for (i = 0; i < mbdevs; i++) {
		struct mhu_mbox *cur =
			kzalloc(sizeof(struct mhu_mbox), GFP_KERNEL);
		if (!cur) {
			dev_err(dev, "mbox unable to alloc dev\n");
			goto out_err;
		}

		mhu_chan = &mhu_ctlr->channels[i];
		cur->channel_id = i;
		cur->char_no = MKDEV(char_major, i);
		mutex_init(&cur->mutex);

		if (!of_get_property(dev->of_node, "mbox-names", NULL)) {
			dev_err(dev, "%s() get mbox name fail\n", __func__);
			goto out_err;
		}

		index = i;
		if (of_property_read_string_index(dev->of_node,
						  "mbox-names", index, &name)) {
			dev_err(dev, "%s get mbox[%d] name fail\n",
				__func__, index);
			goto out_err;
		}

		strncpy(cur->char_name, name, 32);
		pr_debug("dts char name[%d]: %s\n", index, cur->char_name);

		cur->mhu_id = mhu_chan->mhu_id;
		cur->mhu_dev = dev;

		cdev_init(&cur->char_cdev, &mbox_message_ops);
		err = cdev_add(&cur->char_cdev, cur->char_no, 1);
		if (err) {
			dev_err(dev, "mbox fail to add cdev\n");
			goto out_err;
		}

		cur->char_dev =
			device_create(mbox_class, NULL, cur->char_no,
				      cur, "%s", cur->char_name);
		if (IS_ERR(cur->char_dev)) {
			dev_err(dev, "mbox fail to create device\n");
			goto out_err;
		}
		list_add_tail(&cur->char_list, &mbox_devs);
	}
	return 0;
out_err:
	mhu_cleanup_devs();
	unregister_chrdev_region(char_dev, mbdevs);
class_err:
	class_destroy(mbox_class);
err:
	return err;
}

static int mhu_sec_probe(struct platform_device *pdev)
{
	struct mhu_ctlr *mhu_ctlr;
	struct mhu_chan *mhu_chan;
	struct mbox_chan *mbox_chans;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int idx, err;
	u32 num_chans = 0;
	const char *name = NULL;

	pr_info("mhu sec start\n");
	mhu_ctlr = devm_kzalloc(dev, sizeof(*mhu_ctlr), GFP_KERNEL);
	if (!mhu_ctlr)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource 0\n");
		return -ENXIO;
	}
	mhu_ctlr->nee2scpu_csr = devm_ioremap_resource(dev, res);
	if (IS_ERR_OR_NULL(mhu_ctlr->nee2scpu_csr))
		return PTR_ERR(mhu_ctlr->nee2scpu_csr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource 1\n");
		return -ENXIO;
	}
	mhu_ctlr->scpu2nee_csr = devm_ioremap_resource(dev, res);
	if (IS_ERR_OR_NULL(mhu_ctlr->scpu2nee_csr))
		return PTR_ERR(mhu_ctlr->scpu2nee_csr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource 2\n");
		return -ENXIO;
	}
	mhu_ctlr->nee2scpu_data_st = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctlr->nee2scpu_data_st))
		return PTR_ERR(mhu_ctlr->nee2scpu_data_st);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource 3\n");
		return -ENXIO;
	}
	mhu_ctlr->scpu2nee_data_st = devm_ioremap_resource(dev, res);
	if (IS_ERR(mhu_ctlr->scpu2nee_data_st))
		return PTR_ERR(mhu_ctlr->scpu2nee_data_st);

	mhu_ctlr->dev = dev;
	platform_set_drvdata(pdev, mhu_ctlr);

	num_chans = 0;
	err = of_property_read_u32(dev->of_node,
				   "mbox-nums", &num_chans);
	if (err) {
		dev_err(dev, "failed to get mbox num %d\n", err);
		return -ENXIO;
	}

	mbox_chans = devm_kzalloc(dev,
				  sizeof(*mbox_chans) * num_chans,
				  GFP_KERNEL);
	if (!mbox_chans)
		return -ENOMEM;

	mhu_ctlr->mbox_con.chans = mbox_chans;
	mhu_ctlr->mbox_con.num_chans = num_chans;
	mhu_ctlr->mbox_con.txdone_irq = true;
	mhu_ctlr->mbox_con.ops = &mhu_ops;
	mhu_ctlr->mbox_con.dev = dev;

	mhu_ctlr->channels = devm_kzalloc(dev,
					  sizeof(struct mhu_chan) * num_chans,
					  GFP_KERNEL);

	for (idx = 0; idx < num_chans; idx++) {
		mhu_chan = &mhu_ctlr->channels[idx];
		mhu_chan->index = idx;
		mhu_chan->mhu_id = idx;//mhu_sec_ctlr->mhu_id[idx];
		mhu_chan->ctlr = mhu_ctlr;

		if (!of_property_read_string_index(dev->of_node,
			    "mbox-names", idx, &name)) {
			/* At most copy (buf size - 1) bytes for making
			 * a null-terminated string
			 */
			strncpy(mhu_chan->mhu_name, name,
				sizeof(mhu_chan->mhu_name) - 1);
		}
		pr_debug("%s, sec chan name: %s, index: %d, idx: %d, sec_mbu_id:%d\n",
			 __func__, mhu_chan->mhu_name, mhu_chan->index, idx,
			 mhu_chan->mhu_id);

		mbox_chans[idx].con_priv = mhu_chan;
	}
	if (mbox_controller_register(&mhu_ctlr->mbox_con)) {
		dev_err(dev, "failed to register mailbox controller\n");
		return -ENOMEM;
	}
	for (idx = 0; idx < num_chans; idx++) {
		if (BIT(idx) & REV_MBOX_MASK)
			continue;
		mhu_chan =  &mhu_ctlr->channels[idx];
		mhu_chan->index = idx;
		mhu_chan->rx_irq = platform_get_irq(pdev, idx / 2);
		pr_debug("rx_irq: %X\n", mhu_chan->rx_irq);
		if (mhu_chan->rx_irq < 0) {
			dev_err(dev, "failed to get interrupt %d\n", idx / 2);
			return -ENXIO;
		}

		mhu_chan->data = devm_kzalloc(dev,
					      sizeof(struct mhu_data_buf),
					      GFP_KERNEL);
		if (!mhu_chan->data)
			return -ENOMEM;

		mhu_chan->data->rx_buf = devm_kzalloc(dev,
						      MBOX_SEC_SIZE,
						      GFP_KERNEL);
		if (!mhu_chan->data->rx_buf)
			return -ENOMEM;

		mhu_chan->data->rx_size = MBOX_SEC_SIZE;

		err = request_threaded_irq(mhu_chan->rx_irq, mbox_handler,
					   NULL, IRQF_ONESHOT | IRQF_NO_SUSPEND,
					    DRIVER_NAME, &mbox_chans[idx]);
		if (err) {
			dev_err(dev, "request irq error\n");
			return err;
		}
	}

	err = mhu_cdev_init(dev, mhu_ctlr);
	if (err < 0) {
		dev_err(dev, "init cdev fail\n");
		return err;
	}

	mhu_sec_device = dev;
	/*set mhu type*/
	mhu_f |= MASK_MHU_SEC;
	pr_info("mbox sec init done 12051 node:%pK, mhuf:0x%x\n",
		dev, mhu_f);
	return 0;
}

static int mhu_sec_remove(struct platform_device *pdev)
{
	struct mhu_ctlr *ctlr = platform_get_drvdata(pdev);

	mbox_controller_unregister(&ctlr->mbox_con);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id mhu_of_match[] = {
	{ .compatible = "amlogic, meson_mhu_sec" },
	{},
};

static struct platform_driver mhu_sec_driver = {
	.probe = mhu_sec_probe,
	.remove = mhu_sec_remove,
	.driver = {
		.owner		= THIS_MODULE,
		.name = DRIVER_NAME,
		.of_match_table = mhu_of_match,
	},
};

static int __init mhu_sec_init(void)
{
	return platform_driver_register(&mhu_sec_driver);
}
core_initcall(mhu_sec_init);

static void __exit mhu_sec_exit(void)
{
	platform_driver_unregister(&mhu_sec_driver);
}
module_exit(mhu_sec_exit);

MODULE_AUTHOR("Huan.Biao <huan.biao@amlogic.com>");
MODULE_DESCRIPTION("MESON MHU SEC mailbox driver");
MODULE_LICENSE("GPL");
