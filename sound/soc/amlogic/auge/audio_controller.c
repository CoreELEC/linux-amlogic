// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

/*#define DEBUG*/

#include <linux/device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/clk-provider.h>

#include "../common/iomapres.h"

#include "audio_io.h"
#include "regs.h"
#include "audio_aed_reg_list.h"
#include "audio_top_reg_list.h"
#include "audio_controller.h"

#define DRV_NAME "aml-audio-controller"

unsigned int chip_id;

static unsigned int aml_audio_mmio_read(struct aml_audio_controller *actrlr,
					unsigned int reg)
{
	struct regmap *regmap = actrlr->regmap;

	return mmio_read(regmap, reg);
}

static int aml_audio_mmio_write(struct aml_audio_controller *actrlr,
				unsigned int reg, unsigned int value)
{
	struct regmap *regmap = actrlr->regmap;

	pr_debug("audio top reg:[%s] addr: [%#x] val: [%#x]\n",
		 top_register_table[reg].name,
		 top_register_table[reg].addr,
		 value);

	return mmio_write(regmap, reg, value);
}

static int aml_audio_mmio_update_bits(struct aml_audio_controller *actrlr,
				      unsigned int reg, unsigned int mask,
				      unsigned int value)
{
	struct regmap *regmap = actrlr->regmap;

	pr_debug("audio top reg:[%s] addr: [%#x] mask: [%#x] val: [%#x]\n",
		 top_register_table[reg].name,
		 top_register_table[reg].addr,
		 mask, value);

	return mmio_update_bits(regmap, reg, mask, value);
}

int aml_return_chip_id(void)
{
	return chip_id;
}

struct aml_audio_ctrl_ops aml_actrl_mmio_ops = {
	.read		= aml_audio_mmio_read,
	.write		= aml_audio_mmio_write,
	.update_bits	= aml_audio_mmio_update_bits,
};

static struct regmap_config aml_audio_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static const struct of_device_id amlogic_audio_controller_of_match[] = {
	{ .compatible = "amlogic, audio-controller" },
	{},
};

static int register_audio_controller(struct platform_device *pdev,
				     struct aml_audio_controller *actrl)
{
	struct resource *res_mem;
	void __iomem *regs;
	struct regmap *regmap;

	/* get platform res from dtb */
	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENOENT;

	regs = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	aml_audio_regmap_config.max_register = 4 * resource_size(res_mem);

	regmap = devm_regmap_init_mmio(&pdev->dev, regs,
				       &aml_audio_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	/* init aml audio bus mmio controller */
	aml_init_audio_controller(actrl, regmap, &aml_actrl_mmio_ops);
	platform_set_drvdata(pdev, actrl);

	/* gate on all clks on bringup stage, need gate separately */
	aml_audiobus_write(actrl, EE_AUDIO_CLK_GATE_EN0, 0xffffffff);
	aml_audiobus_update_bits(actrl, EE_AUDIO_CLK_GATE_EN1, 0x7, 0x7);
	return 0;
}

static int aml_audio_controller_probe(struct platform_device *pdev)
{
	struct aml_audio_controller *actrl;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);
	actrl = devm_kzalloc(&pdev->dev, sizeof(*actrl), GFP_KERNEL);
	if (!actrl)
		return -ENOMEM;

	ret = of_property_read_u32(node, "chip_id", &chip_id);
	if (ret < 0)
		/* defulat set 0 */
		chip_id = 0;

	return register_audio_controller(pdev, actrl);
}

static struct platform_driver aml_audio_controller_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = amlogic_audio_controller_of_match,
	},
	.probe = aml_audio_controller_probe,
};

int __init audio_controller_init(void)
{
	return platform_driver_register(&aml_audio_controller_driver);
}

void __exit audio_controller_exit(void)
{
	platform_driver_unregister(&aml_audio_controller_driver);
}

#ifndef MODULE
module_init(audio_controller_init);
module_exit(audio_controller_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic audio controller ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, amlogic_audio_controller_of_match);
#endif
