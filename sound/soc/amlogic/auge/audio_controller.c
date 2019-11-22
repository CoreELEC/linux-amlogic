/*
 * sound/soc/amlogic/auge/audio_controller.c
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

#include <linux/amlogic/media/sound/iomapres.h>

#include "audio_io.h"
#include "regs.h"
#include "audio_aed_reg_list.h"
#include "audio_top_reg_list.h"

#define DRV_NAME "aml-audio-controller"

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
			unsigned int reg, unsigned int mask, unsigned int value)
{
	struct regmap *regmap = actrlr->regmap;

	pr_debug("audio top reg:[%s] addr: [%#x] mask: [%#x] val: [%#x]\n",
			top_register_table[reg].name,
			top_register_table[reg].addr,
			mask, value);

	return mmio_update_bits(regmap, reg, mask, value);
}

struct aml_audio_ctrl_ops aml_actrl_mmio_ops = {
	.read			= aml_audio_mmio_read,
	.write			= aml_audio_mmio_write,
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
	aml_audiobus_write(actrl, EE_AUDIO_CLK_GATE_EN1, 0xffffffff);

	return 0;
}

static int aml_audio_controller_probe(struct platform_device *pdev)
{
	struct aml_audio_controller *actrl;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);
	actrl = devm_kzalloc(&pdev->dev, sizeof(*actrl), GFP_KERNEL);
	if (!actrl)
		return -ENOMEM;

	return register_audio_controller(pdev, actrl);
}

static struct platform_driver aml_audio_controller_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = amlogic_audio_controller_of_match,
	},
	.probe = aml_audio_controller_probe,
};
module_platform_driver(aml_audio_controller_driver);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic audio controller ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, amlogic_audio_controller_of_match);
