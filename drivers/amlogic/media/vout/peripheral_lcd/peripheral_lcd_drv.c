/*
 * drivers/amlogic/media/vout/peripheral_lcd/peripheral_lcd_drv.c
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

#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/peripheral_lcd.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "peripheral_lcd_dev.h"
#include "peripheral_lcd_drv.h"

struct per_gpio_s peripheral_gpio[PER_GPIO_NUM_MAX];

struct per_lcd_reg_map_s per_lcd_reg_map;
struct lcd_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

struct per_gpio_s *per_gpio;
struct peripheral_lcd_driver_s *peripheral_lcd_drv;
unsigned char peripheral_lcd_debug_print;

void peripheral_lcd_delay_us(int us)
{
	if (us > 0 && us < 20000)
		usleep_range(us, us + 1);
	else if (us > 20000)
		msleep(us / 1000);
}

void peripheral_lcd_delay_ms(int ms)
{
	if (ms > 0 && ms < 20)
		usleep_range(ms * 1000, ms * 1000 + 1);
	else if (ms > 20)
		msleep(ms);
}

static void peripheral_lcd_ioremap(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		LCDERR("%s: lcd_reg resource get error\n", __func__);
		return;
	}
	per_lcd_reg_map.base_addr = res->start;
	per_lcd_reg_map.size = resource_size(res);
	per_lcd_reg_map.p = devm_ioremap_nocache(&pdev->dev,
		res->start, per_lcd_reg_map.size);
	if (!per_lcd_reg_map.p) {
		per_lcd_reg_map.flag = 0;
		LCDERR("%s: reg map failed: 0x%x\n",
		       __func__,
		       per_lcd_reg_map.base_addr);
		return;
	}
	per_lcd_reg_map.flag = 1;
//	if (peripheral_lcd_debug_print)
	LCDPR("%s: reg mapped: 0x%x -> %p size: 0x%x\n",
	      __func__,
	      per_lcd_reg_map.base_addr,
	      per_lcd_reg_map.p, per_lcd_reg_map.size);
}

int peripheral_lcd_get_config_dts(struct platform_device *pdev)
{
	unsigned int para[5];
	int ret;

	if (!pdev->dev.of_node) {
		LCDERR("no per_lcd of_node exist\n");
		return -1;
	}
	/* get ldim_dev_index from dts */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "per_lcd_dev_index", &para[0]);
	if (ret)
		LCDERR("failed to get per_lcd_dev_index\n");
	else
		peripheral_lcd_drv->dev_index = (unsigned char)para[0];
	if (peripheral_lcd_drv->dev_index < 0xff)
		LCDPR("get peripheral_lcd_dev_index = %d\n",
		      peripheral_lcd_drv->dev_index);

	return 0;
}

void peripheral_lcd_gpio_probe(unsigned int index)
{
	const char *str;
	int ret;

	if (index >= PER_GPIO_NUM_MAX) {
		LCDERR("gpio index %d, exit\n", index);
		return;
	}
	per_gpio = &peripheral_gpio[index];
	if (per_gpio->probe_flag) {
		if (lcd_debug_print_flag) {
			LCDPR("gpio %s[%d] is already registered\n",
			      per_gpio->name, index);
		}
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(peripheral_lcd_drv->dev->of_node,
					    "per_lcd_gpio_names", index, &str);
	if (ret) {
		LCDERR("failed to get lcd_per_gpio_names: %d\n", index);
		str = "unknown";
	}
	strcpy(per_gpio->name, str);

	/* init gpio flag */
	per_gpio->probe_flag = 1;
	per_gpio->register_flag = 0;
}

static int peripheral_lcd_gpio_register(int index, int init_value)
{
	int value;

	if (index >= PER_GPIO_NUM_MAX) {
		LCDERR("%s: gpio index %d, exit\n", __func__, index);
		return -1;
	}
	per_gpio = &peripheral_gpio[index];
	if (per_gpio->probe_flag == 0) {
		LCDERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (per_gpio->register_flag) {
		if (peripheral_lcd_debug_print) {
			LCDPR("%s: gpio %s[%d] is already registered\n",
			      __func__, per_gpio->name, index);
		}
		return 0;
	}

	switch (init_value) {
	case PER_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case PER_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case PER_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}
	/* request gpio */
	per_gpio->gpio = devm_gpiod_get_index(peripheral_lcd_drv->dev,
					      "per_lcd", index, value);
	if (IS_ERR(per_gpio->gpio)) {
		LCDERR("register gpio %s[%d]: %p, err: %d\n",
		       per_gpio->name, index, per_gpio->gpio,
		       IS_ERR(per_gpio->gpio));
		return -1;
	}
	per_gpio->register_flag = 1;
	if (peripheral_lcd_debug_print) {
		LCDPR("register gpio %s[%d]: %p, init value: %d\n",
		      per_gpio->name, index, per_gpio->gpio, init_value);
	}

	return 0;
}

void peripheral_lcd_gpio_set(int index, int value)
{
	if (index >= PER_GPIO_NUM_MAX) {
		LCDERR("gpio index %d, exit\n", index);
		return;
	}
	per_gpio = &peripheral_gpio[index];
	if (per_gpio->probe_flag == 0) {
		LCDERR("%s: gpio [%d] is not probed\n", __func__, index);
		return;
	}
	if (per_gpio->register_flag == 1) {
		if (peripheral_lcd_debug_print)
			LCDPR("no need regist\n");
	} else {
		peripheral_lcd_gpio_register(index, value);
		return;
	}

	if (IS_ERR_OR_NULL(per_gpio->gpio)) {
		LCDERR("gpio %s[%d]: %p, err: %ld\n",
		       per_gpio->name, index, per_gpio->gpio,
		       PTR_ERR(per_gpio->gpio));
		return;
	}

	switch (value) {
	case PER_GPIO_OUTPUT_LOW:
	case PER_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(per_gpio->gpio, value);
		break;
	case PER_GPIO_INPUT:
	default:
		gpiod_direction_input(per_gpio->gpio);
		break;
	}
	if (peripheral_lcd_debug_print) {
		LCDPR("set gpio %s[%d] value: %d\n",
		      per_gpio->name, index, value);
	}
}

int peripheral_lcd_gpio_set_irq(int index)
{
	int irq_pin;

	if (index >= PER_GPIO_NUM_MAX) {
		LCDERR("gpio index %d, exit\n", index);
		return -1;
	}

	peripheral_lcd_gpio_register(index, 2);
	irq_pin = desc_to_gpio(per_gpio->gpio);
	peripheral_lcd_drv->irq_num = gpio_to_irq(irq_pin);
	if (!peripheral_lcd_drv->irq_num) {
		LCDERR("gpio to irq failed\n");
		return -1;
	}

	return 0;
}

int peripheral_lcd_gpio_get(int index)
{
	if (index >= PER_GPIO_NUM_MAX) {
		LCDERR("gpio index %d, exit\n", index);
		return -1;
	}

	per_gpio = &peripheral_gpio[index];
	if (per_gpio->probe_flag == 0) {
		LCDERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (per_gpio->register_flag == 0) {
		LCDERR("%s: gpio %s[%d] is not registered\n",
		       __func__, per_gpio->name, index);
		return -1;
	}
	if (IS_ERR_OR_NULL(per_gpio->gpio)) {
		LCDERR("gpio %s[%d]: %p, err: %ld\n",
		       per_gpio->name, index,
		       per_gpio->gpio, PTR_ERR(per_gpio->gpio));
		return -1;
	}

	return gpiod_get_value(per_gpio->gpio);
}

static ssize_t peripheral_lcd_info_show(struct class *class,
					struct class_attribute *attr,
					char *buf)
{
	struct per_lcd_dev_config_s *pconf;

	if (!peripheral_lcd_drv)
		return sprintf(buf, "peripheral lcd driver is NULL\n");

	pconf = peripheral_lcd_drv->per_lcd_dev_conf;
	pr_info("peripheral lcd driver %s(%d) info:\n"
		"table_loaded:       %d\n"
		"cmd_size:           %d\n"
		"table_init_on_cnt:  %d\n"
		"table_init_off_cnt: %d\n",
		pconf->name,
		peripheral_lcd_drv->dev_index,
		pconf->init_loaded, pconf->cmd_size,
		pconf->init_on_cnt,
		pconf->init_off_cnt);
	switch (pconf->type) {
	case PER_DEV_TYPE_SPI:
		break;
	case PER_DEV_TYPE_MCU_8080:
		pr_info("reset_index: %d\n"
			"nCS_index:   %d\n"
			"nRD_index:   %d\n"
			"nWR_index:   %d\n"
			"nRS_index:   %d\n"
			"data0_index: %d\n"
			"data1_index: %d\n"
			"data2_index: %d\n"
			"data3_index: %d\n"
			"data4_index: %d\n"
			"data5_index: %d\n"
			"data6_index: %d\n"
			"data7_index: %d\n",
			pconf->reset_index, pconf->nCS_index,
			pconf->nRD_index, pconf->nWR_index,
			pconf->nRS_index, pconf->data0_index,
			pconf->data1_index, pconf->data2_index,
			pconf->data3_index, pconf->data4_index,
			pconf->data5_index, pconf->data6_index,
			pconf->data7_index);
		break;
	default:

		pr_info("not support per_type\n");
		break;
	}

	return sprintf(buf, "\n");
}

static ssize_t peripheral_lcd_init_store(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t count)
{
	if (!peripheral_lcd_drv)
		peripheral_lcd_drv->enable();
	return count;
}

static ssize_t peripheral_lcd_test_store(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t count)
{
	if (!peripheral_lcd_drv)
		peripheral_lcd_drv->test(buf);
	return count;
}

static ssize_t peripheral_lcd_print_store(struct class *class,
					  struct class_attribute *attr,
					  const char *buf, size_t count)
{
	int ret, flag;

	ret = kstrtoint(buf, 10, &flag);
	peripheral_lcd_debug_print = flag;
	return count;
}

static ssize_t peripheral_lcd_print_show(struct class *class,
					 struct class_attribute *attr,
					 char *buf)
{
	return sprintf(buf, "peripheral_lcd_debug_print = %d\n",
		       peripheral_lcd_debug_print);
}

static struct class_attribute peripheral_lcd_class_attrs[] = {
	__ATTR(info, 0644,
	       peripheral_lcd_info_show, NULL),
	__ATTR(print, 0644,
	       peripheral_lcd_print_show, peripheral_lcd_print_store),
	__ATTR(init, 0644, NULL, peripheral_lcd_init_store),
	__ATTR(test, 0644, NULL, peripheral_lcd_test_store),
};

static struct class *peripheral_lcd_class;
static int creat_peripheral_lcd_class(void)
{
	int i;

	peripheral_lcd_class = class_create(THIS_MODULE, "peripheral_lcd");
	if (IS_ERR(peripheral_lcd_class)) {
		LCDERR("create debug class failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(peripheral_lcd_class_attrs); i++) {
		if (class_create_file(peripheral_lcd_class,
				      &peripheral_lcd_class_attrs[i])) {
			LCDERR("create debug attribute %s failed\n",
			       peripheral_lcd_class_attrs[i].attr.name);
		}
	}

	return 0;
}

static int remove_peripheral_lcd_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(peripheral_lcd_class_attrs); i++)
		class_remove_file(peripheral_lcd_class,
				  &peripheral_lcd_class_attrs[i]);

	class_destroy(peripheral_lcd_class);
	peripheral_lcd_class = NULL;

	return 0;
}

struct peripheral_lcd_driver_s *peripheral_lcd_get_driver(void)
{
	return peripheral_lcd_drv;
}

#if 0
struct peripheral_lcd_driver_s *peripheral_lcd_client_register(int *vsync_cb)
{
	if (!peripheral_lcd_drv) {
		LCDERR("%s: driver id null\n", __func__);
		return NULL;
	}
	peripheral_lcd_drv->vsync_isr_cb = vsync_cb;
	return peripheral_lcd_drv;
}
#endif

static int peripheral_lcd_probe(struct platform_device *pdev)
{
	peripheral_lcd_drv = kmalloc(sizeof(*peripheral_lcd_drv),
				     GFP_KERNEL);
	if (!peripheral_lcd_drv) {
		LCDERR("%s: lcd driver no enough memory\n", __func__);
		return -ENOMEM;
	}
	peripheral_lcd_drv->dev = &pdev->dev;
	peripheral_lcd_drv->per_lcd_reg_map = &per_lcd_reg_map;
	peripheral_lcd_ioremap(pdev);
	memset(peripheral_gpio, 0, sizeof(*peripheral_gpio));
	peripheral_lcd_get_config_dts(pdev);
	peripheral_lcd_dev_probe(pdev);
	creat_peripheral_lcd_class();

	LCDPR("%s ok\n", __func__);
	return 0;
}

static int peripheral_lcd_remove(struct platform_device *pdev)
{
	remove_peripheral_lcd_class();
	if (!peripheral_lcd_drv)
		return 0;

	peripheral_lcd_dev_remove(pdev);

	kfree(peripheral_lcd_drv);
	peripheral_lcd_drv = NULL;

	LCDPR("%s ok\n", __func__);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id peripheral_lcd_dt_match[] = {
	{
		.compatible = "amlogic, peripheral_lcd",
	},
	{}
};
#endif

static struct platform_driver peripheral_lcd_platform_driver = {
	.driver = {
		.name  = "peripheral_lcd",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = peripheral_lcd_dt_match,
#endif
	},
	.probe   = peripheral_lcd_probe,
	.remove  = peripheral_lcd_remove,
};

static int __init peripheral_lcd_init(void)
{
	if (platform_driver_register(&peripheral_lcd_platform_driver)) {
		LCDPR("failed to register ldim_dev driver module\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit peripheral_lcd_exit(void)
{
	platform_driver_unregister(&peripheral_lcd_platform_driver);
}

late_initcall(peripheral_lcd_init);
module_exit(peripheral_lcd_exit);

MODULE_DESCRIPTION("PERIPHERAL device Driver for LCD");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
