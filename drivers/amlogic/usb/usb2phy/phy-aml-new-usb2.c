// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/usb/phy_companion.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/amlogic/usb-gxl.h>
#include "phy-aml-new-usb.h"

static int amlogic_new_usb2_init(struct usb_phy *x)
{
	int time_dly = 500;
	int i, j;

	struct amlogic_usb *phy = phy_to_amlusb(x);
	struct u2p_aml_regs_t u2p_aml_regs;
	union u2p_r0_t reg0;

	if (phy->suspend_flag) {
		phy->suspend_flag = 0;
		for (i = 0; i < phy->portnum; i++) {
			for (j = 0; j < 3; j++) {
				u2p_aml_regs.u2p_r[j] = (void __iomem *)
				((unsigned long)phy->regs + i * PHY_REGISTER_SIZE
					+ 4 * j);
			}

			reg0.d32 = readl(u2p_aml_regs.u2p_r[0]);
			reg0.b.por = 0;
			writel(reg0.d32, u2p_aml_regs.u2p_r[0]);
		}
		return 0;
	}

	amlogic_new_usbphy_reset(phy);

	for (i = 0; i < phy->portnum; i++) {
		for (j = 0; j < 3; j++) {
			u2p_aml_regs.u2p_r[j] = (void __iomem *)
				((unsigned long)phy->regs + i * PHY_REGISTER_SIZE
				+ 4 * j);
		}

		reg0.d32 = readl(u2p_aml_regs.u2p_r[0]);
		reg0.b.por = 1;
		reg0.b.dmpulldown = 1;
		reg0.b.dppulldown = 1;
		if (i == 1)
			reg0.b.idpullup = 1;
		writel(reg0.d32, u2p_aml_regs.u2p_r[0]);

		udelay(time_dly);

		reg0.d32 = readl(u2p_aml_regs.u2p_r[0]);
		reg0.b.por = 0;
		writel(reg0.d32, u2p_aml_regs.u2p_r[0]);
	}

	return 0;
}

static int amlogic_new_usb2_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static void amlogic_new_usb2phy_shutdown(struct usb_phy *x)
{
	struct amlogic_usb *phy = phy_to_amlusb(x);
	struct u2p_aml_regs_t u2p_aml_regs;
	union u2p_r0_t reg0;
	int i, j;

	phy->suspend_flag = 1;
	for (i = phy->portnum - 1; i >= 0; i--) {
		for (j = 0; j < 3; j++) {
			u2p_aml_regs.u2p_r[j] = (void __iomem	*)
				((unsigned long)phy->regs + i * PHY_REGISTER_SIZE
				+ 4 * j);
		}

		reg0.d32 = readl(u2p_aml_regs.u2p_r[0]);
		reg0.b.por = 1;
		writel(reg0.d32, u2p_aml_regs.u2p_r[0]);
	}
}

static int amlogic_new_usb2_probe(struct platform_device *pdev)
{
	struct amlogic_usb			*phy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	struct resource *reset_mem;
	void __iomem	*phy_base;
	void __iomem	*reset_base = NULL;
	int portnum = 0;
	const void *prop;

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum) {
		dev_err(&pdev->dev, "This phy has no usb port\n");
		return -ENOMEM;
	}

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);

	reset_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (reset_mem) {
		reset_base = ioremap(reset_mem->start,
			resource_size(reset_mem));
		if (IS_ERR(reset_base))
			return PTR_ERR(reset_base);
	}

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	dev_info(&pdev->dev, "USB2 phy probe:phy_mem:0x%lx, iomap phy_base:0x%lx\n",
			(unsigned long)phy_mem->start, (unsigned long)phy_base);

	phy->dev		= dev;
	phy->regs		= phy_base;
	phy->reset_regs = reset_base;
	phy->portnum      = portnum;
	phy->suspend_flag = 0;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-usbphy2";
	phy->phy.init		= amlogic_new_usb2_init;
	phy->phy.set_suspend	= amlogic_new_usb2_suspend;
	phy->phy.shutdown	= amlogic_new_usb2phy_shutdown;
	phy->phy.type		= USB_PHY_TYPE_USB2;

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	return 0;
}

static int amlogic_new_usb2_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_new_usb2_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_new_usb2_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops amlogic_new_usb2_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_new_usb2_runtime_suspend,
		amlogic_new_usb2_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_new_usb2_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_new_usb2_id_table[] = {
	{ .compatible = "amlogic, amlogic-new-usb2" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_new_usb2_id_table);
#endif

static struct platform_driver amlogic_new_usb2_driver = {
	.probe		= amlogic_new_usb2_probe,
	.remove		= amlogic_new_usb2_remove,
	.driver		= {
		.name	= "amlogic-new-usb2",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_new_usb2_id_table),
	},
};

module_platform_driver(amlogic_new_usb2_driver);

MODULE_ALIAS("platform: amlogic_usb2");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB2 phy driver");
MODULE_LICENSE("GPL v2");
