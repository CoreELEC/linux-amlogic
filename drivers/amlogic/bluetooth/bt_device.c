/*
 * drivers/amlogic/bluetooth/bt_device.c
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/rfkill.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/amlogic/bt_device.h>
#include <linux/random.h>
#ifdef CONFIG_AM_WIFI_SD_MMC
#include <linux/amlogic/wifi_dt.h>
#endif
#include "../../gpio/gpiolib.h"

#include <linux/interrupt.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <linux/irq.h>

#include <linux/input.h>


#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static struct early_suspend bt_early_suspend;
#endif

#define BT_RFKILL "bt_rfkill"

#define BT_DBG 0
#if BT_DBG
#define BT_INFO(fmt, args...)	\
	pr_info("[%s] " fmt, __func__, ##args)
#else
#define BT_INFO(fmt, args...)
#endif

#define BT_ERR(fmt, args...)	\
	pr_info("[%s] " fmt, __func__, ##args)

char bt_addr[18] = "";
static struct class *bt_addr_class;
static int btwake_evt;
static int btirq_flag;
static int btpower_evt;
static int flag_n;
static int flag_p;
static int cnt;

static ssize_t bt_addr_show(struct class *cls,
	struct class_attribute *attr, char *_buf)
{
	char local_addr[6];

	if (!_buf)
		return -EINVAL;

	if (strlen(bt_addr) == 0) {
		local_addr[0] = 0x22;
		local_addr[1] = 0x22;
		local_addr[2] = prandom_u32();
		local_addr[3] = prandom_u32();
		local_addr[4] = prandom_u32();
		local_addr[5] = prandom_u32();
		sprintf(bt_addr, "%02x:%02x:%02x:%02x:%02x:%02x",
		local_addr[0], local_addr[1], local_addr[2],
		local_addr[3], local_addr[4], local_addr[5]);
	}

	return sprintf(_buf, "%s\n", bt_addr);
}
static ssize_t bt_addr_store(struct class *cls,
	struct class_attribute *attr, const char __user *buf, size_t count)
{
	int ret = -EINVAL;

	if (!buf)
		return ret;

	snprintf(bt_addr, sizeof(bt_addr), "%s", buf);

	if (bt_addr[strlen(bt_addr)-1] == '\n')
		bt_addr[strlen(bt_addr)-1] = '\0';

	BT_INFO("bt_addr=%s\n", bt_addr);
	return count;
}
static CLASS_ATTR(value, 0644, bt_addr_show, bt_addr_store);

struct bt_dev_runtime_data {
	struct rfkill *bt_rfk;
	struct bt_dev_data *pdata;
};


static void bt_device_off(struct bt_dev_data *pdata)
{
	if (pdata->power_down_disable == 0) {
		if ((btpower_evt == 1) && (pdata->gpio_reset > 0)) {
			if ((pdata->power_on_pin_OD)
				&& (pdata->power_low_level)) {
				gpio_direction_input(pdata->gpio_reset);
			} else {
				gpio_direction_output(pdata->gpio_reset,
					pdata->power_low_level);
			}
		}
		if ((btpower_evt == 1) && (pdata->gpio_en > 0)) {
			if ((pdata->power_on_pin_OD)
				&& (pdata->power_low_level)) {
				gpio_direction_input(pdata->gpio_en);
			} else {
				gpio_direction_output(pdata->gpio_en,
					pdata->power_low_level);
			}
		}

		if (btpower_evt == 2)
			set_usb_bt_power(0);

		if ((btpower_evt == 0) && (pdata->gpio_reset > 0)) {
			if ((pdata->power_on_pin_OD)
				&& (pdata->power_low_level)) {
				gpio_direction_input(pdata->gpio_reset);
			} else {
				gpio_direction_output(pdata->gpio_reset,
					pdata->power_low_level);
			}
		}
		if ((btpower_evt == 0) && (pdata->gpio_en > 0)) {
			if ((pdata->power_on_pin_OD)
				&& (pdata->power_low_level)) {
				gpio_direction_input(pdata->gpio_en);
			} else
				set_usb_bt_power(0);

		}
		msleep(20);
	}
}


static void bt_device_init(struct bt_dev_data *pdata)
{
	int tmp = 0;
	btpower_evt = 0;
	btirq_flag = 0;

	if (pdata->gpio_reset > 0)
		gpio_request(pdata->gpio_reset, BT_RFKILL);

	if (pdata->gpio_en > 0)
		gpio_request(pdata->gpio_en, BT_RFKILL);

	if (pdata->gpio_hostwake > 0) {
		gpio_request(pdata->gpio_hostwake, BT_RFKILL);
		gpio_direction_output(pdata->gpio_hostwake, 1);
	}

	if (pdata->gpio_btwakeup > 0) {
		gpio_request(pdata->gpio_btwakeup, BT_RFKILL);
		gpio_direction_input(pdata->gpio_btwakeup);
	}

	tmp = pdata->power_down_disable;
	pdata->power_down_disable = 0;
	bt_device_off(pdata);
	pdata->power_down_disable = tmp;

}

static void bt_device_deinit(struct bt_dev_data *pdata)
{
	if (pdata->gpio_reset > 0)
		gpio_free(pdata->gpio_reset);
	if (pdata->gpio_en > 0)
		gpio_free(pdata->gpio_en);

	btpower_evt = 0;
	if (pdata->gpio_hostwake > 0)
		gpio_free(pdata->gpio_hostwake);

}

static void bt_device_on(struct bt_dev_data *pdata)
{
	if (pdata->power_down_disable == 0) {
		if ((btpower_evt == 1) && (pdata->gpio_reset > 0)) {
			if ((pdata->power_on_pin_OD)
				&& (pdata->power_low_level)) {
				gpio_direction_input(pdata->gpio_reset);
			} else {
				gpio_direction_output(pdata->gpio_reset,
					pdata->power_low_level);
			}
		}
		if ((btpower_evt == 1) && (pdata->gpio_en > 0)) {
			if ((pdata->power_on_pin_OD)
				&& (pdata->power_low_level)) {
				gpio_direction_input(pdata->gpio_en);
			} else {
				gpio_direction_output(pdata->gpio_en,
						pdata->power_low_level);
			}
		}

		if (btpower_evt == 2) {
			set_usb_bt_power(0);
		}

		if ((btpower_evt == 0) && (pdata->gpio_reset > 0)) {
			if ((pdata->power_on_pin_OD)
				&& (pdata->power_low_level)) {
				gpio_direction_input(pdata->gpio_reset);
			} else {
				gpio_direction_output(pdata->gpio_reset,
					pdata->power_low_level);
			}
		}
		if ((btpower_evt == 0) && (pdata->gpio_en > 0)) {
			if ((pdata->power_on_pin_OD)
				&& (pdata->power_low_level)) {
				gpio_direction_input(pdata->gpio_en);
			} else {
				set_usb_bt_power(0);
			}
		}

		msleep(200);
	}

	if ((btpower_evt == 1) && (pdata->gpio_reset > 0)) {
		if ((pdata->power_on_pin_OD)
			&& (!pdata->power_low_level)) {
			gpio_direction_input(pdata->gpio_reset);
		} else {
			gpio_direction_output(pdata->gpio_reset,
				!pdata->power_low_level);
		}
	}
	if ((btpower_evt == 1) && (pdata->gpio_en > 0)) {
		if ((pdata->power_on_pin_OD)
			&& (!pdata->power_low_level)) {
			gpio_direction_input(pdata->gpio_en);
		} else {
			gpio_direction_output(pdata->gpio_en,
				!pdata->power_low_level);
		}
	}

	if (btpower_evt == 2) {
		set_usb_bt_power(1);
	}

	if ((btpower_evt == 0) && (pdata->gpio_reset > 0)) {
		if ((pdata->power_on_pin_OD)
			&& (!pdata->power_low_level)) {
			gpio_direction_input(pdata->gpio_reset);
		} else {
			gpio_direction_output(pdata->gpio_reset,
				!pdata->power_low_level);
		}
	}
	if ((btpower_evt == 0) && (pdata->gpio_en > 0)) {
		if ((pdata->power_on_pin_OD)
			&& (!pdata->power_low_level)) {
			gpio_direction_input(pdata->gpio_en);
		} else {
			set_usb_bt_power(1);
		}
	}

	msleep(200);
}

/*The system calls this function when GPIOC_14 interrupt occurs*/
static irqreturn_t bt_interrupt(int irq, void *dev_id)
{
	struct bt_dev_data *pdata = (struct bt_dev_data *) dev_id;

	if (btirq_flag == 1) {
		schedule_work(&pdata->btwakeup_work);
		BT_INFO("freeze: test BT IRQ\n");
	}

	return IRQ_HANDLED;
}

static enum hrtimer_restart btwakeup_timer_handler(struct hrtimer *timer)
{
	struct bt_dev_data *pdata  = container_of(timer,
			struct bt_dev_data, timer);


	if  (!gpio_get_value(pdata->gpio_btwakeup) && cnt  < 5)
		cnt++;
	if (cnt >= 5 && cnt < 15) {
		if (gpio_get_value(pdata->gpio_btwakeup))
			flag_p++;
		else if (!gpio_get_value(pdata->gpio_btwakeup))
			flag_n++;
		cnt++;
	}
	BT_INFO("%s power: %d,netflix:%d\n", __func__, flag_p, flag_n);
	if (flag_p >= 7) {
		BT_INFO("%s power: %d\n", __func__, flag_p);
		btwake_evt = 2;
		cnt = 0;
		flag_p = 0;
		btirq_flag = 0;
		input_event(pdata->input_dev,
			EV_KEY, KEY_POWER, 1);
		input_sync(pdata->input_dev);
		input_event(pdata->input_dev,
			EV_KEY, KEY_POWER, 0);
		input_sync(pdata->input_dev);
	} else if (flag_n >= 7) {
		BT_INFO("%s netflix: %d\n", __func__, flag_n);
		btwake_evt = 2;
		cnt = 0;
		flag_n = 0;
		btirq_flag = 0;
		input_event(pdata->input_dev, EV_KEY, 133, 1);
		input_sync(pdata->input_dev);
		input_event(pdata->input_dev, EV_KEY, 133, 0);
		input_sync(pdata->input_dev);
	}
	if (btwake_evt != 2 && cnt != 0)
		hrtimer_start(&pdata->timer,
			ktime_set(0, 20*1000000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static void get_btwakeup_irq_work(struct work_struct *work)
{
	struct bt_dev_data *pdata  = container_of(work,
		struct bt_dev_data, btwakeup_work);

	if (btwake_evt == 2)
		return;
	BT_INFO("%s", __func__);
	hrtimer_start(&pdata->timer,
			ktime_set(0, 100*1000000), HRTIMER_MODE_REL);
}

static int bt_set_block(void *data, bool blocked)
{
	struct bt_dev_data *pdata = data;

	BT_INFO("BT_RADIO going: %s\n", blocked ? "off" : "on");

	if (!blocked) {
		BT_INFO("AML_BT: going ON,btpower_evt=%d\n", btpower_evt);
		bt_device_on(pdata);
	} else {
		BT_INFO("AML_BT: going OFF,btpower_evt=%d\n", btpower_evt);
		bt_device_off(pdata);
	}
	return 0;
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = bt_set_block,
};
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void bt_earlysuspend(struct early_suspend *h)
{

}

static void bt_lateresume(struct early_suspend *h)
{
}
#endif

static int bt_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct bt_dev_data *pdata = platform_get_drvdata(pdev);

	btwake_evt = 0;
	BT_INFO("bt suspend\n");
	disable_irq(pdata->irqno_wakeup);

	return 0;
}

static int bt_resume(struct platform_device *pdev)
{
	struct bt_dev_data *pdata = platform_get_drvdata(pdev);

	BT_INFO("bt resume\n");
	enable_irq(pdata->irqno_wakeup);
	btwake_evt = 0;
	if ((get_resume_method() == RTC_WAKEUP) ||
		(get_resume_method() == AUTO_WAKEUP)) {
		btwake_evt = 1;
		btirq_flag = 1;
	    flag_n = 0;
		flag_p = 0;
		cnt = 0;
	}

	return 0;
}

static int bt_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	const void *prop;
	struct rfkill *bt_rfk;
	struct bt_dev_data *pdata = NULL;
	struct bt_dev_runtime_data *prdata;
	struct input_dev *input_dev;

#ifdef CONFIG_OF
	if (pdev && pdev->dev.of_node) {
		const char *str;
		struct gpio_desc *desc;

		BT_INFO("enter bt_probe of_node\n");
		pdata = kzalloc(sizeof(struct bt_dev_data), GFP_KERNEL);
		ret = of_property_read_string(pdev->dev.of_node,
			"gpio_reset", &str);
		if (ret) {
			pr_warn("not get gpio_reset\n");
			pdata->gpio_reset = 0;
		} else {
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"gpio_reset", 0, NULL);
			pdata->gpio_reset = desc_to_gpio(desc);
		}

		ret = of_property_read_string(pdev->dev.of_node,
		"gpio_en", &str);
		if (ret) {
			pr_warn("not get gpio_en\n");
			pdata->gpio_en = 0;
		} else {
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"gpio_en", 0, NULL);
			pdata->gpio_en = desc_to_gpio(desc);
		}
		ret = of_property_read_string(pdev->dev.of_node,
		"gpio_hostwake", &str);
		if (ret) {
			pr_warn("not get gpio_hostwake\n");
			pdata->gpio_hostwake = 0;
		} else {
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"gpio_hostwake", 0, NULL);
			pdata->gpio_hostwake = desc_to_gpio(desc);
		}
		/*gpio_btwakeup = BT_WAKE_HOST*/
		ret = of_property_read_string(pdev->dev.of_node,
			"gpio_btwakeup", &str);
		if (ret) {
			pr_warn("not get gpio_btwakeup\n");
			pdata->gpio_btwakeup = 0;
		} else {
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"gpio_btwakeup", 0, NULL);
			pdata->gpio_btwakeup = desc_to_gpio(desc);
		}

		prop = of_get_property(pdev->dev.of_node,
		"power_low_level", NULL);
		if (prop) {
			BT_INFO("power on valid level is low");
			pdata->power_low_level = 1;
		} else {
			BT_INFO("power on valid level is high");
			pdata->power_low_level = 0;
			pdata->power_on_pin_OD = 0;
		}
		ret = of_property_read_u32(pdev->dev.of_node,
		"power_on_pin_OD", &pdata->power_on_pin_OD);
		if (ret)
			pdata->power_on_pin_OD = 0;
		BT_INFO("bt: power_on_pin_OD = %d;\n", pdata->power_on_pin_OD);
		ret = of_property_read_u32(pdev->dev.of_node,
				"power_off_flag", &pdata->power_off_flag);
		if (ret)
			pdata->power_off_flag = 1;/*bt poweroff*/
		BT_INFO("bt: power_off_flag = %d;\n", pdata->power_off_flag);

		ret = of_property_read_u32(pdev->dev.of_node,
			"power_down_disable", &pdata->power_down_disable);
		if (ret)
			pdata->power_down_disable = 0;
		BT_INFO("dis power down = %d;\n", pdata->power_down_disable);
	} else if (pdev) {
		pdata = (struct bt_dev_data *)(pdev->dev.platform_data);
	} else {
		ret = -ENOENT;
		goto err_res;
	}
#else
	pdata = (struct bt_dev_data *)(pdev->dev.platform_data);
#endif
	bt_addr_class = class_create(THIS_MODULE, "bt_addr");
	ret = class_create_file(bt_addr_class, &class_attr_value);

	bt_device_init(pdata);
	if (pdata->power_down_disable == 1) {
		pdata->power_down_disable = 0;
		bt_device_on(pdata);
		pdata->power_down_disable = 1;
	}

	/* default to bluetooth off */
	/* rfkill_switch_all(RFKILL_TYPE_BLUETOOTH, 1); */
	/* bt_device_off(pdata); */

	bt_rfk = rfkill_alloc("bt-dev", &pdev->dev,
		RFKILL_TYPE_BLUETOOTH,
		&bt_rfkill_ops, pdata);

	if (!bt_rfk) {
		BT_ERR("rfk alloc fail\n");
		ret = -ENOMEM;
		goto err_rfk_alloc;
	}

	rfkill_init_sw_state(bt_rfk, false);
	ret = rfkill_register(bt_rfk);
	if (ret) {
		pr_err("rfkill_register fail\n");
		goto err_rfkill;
	}
	prdata = kmalloc(sizeof(struct bt_dev_runtime_data),
	GFP_KERNEL);

	if (!prdata)
		goto err_rfkill;

	prdata->bt_rfk = bt_rfk;
	prdata->pdata = pdata;
	platform_set_drvdata(pdev, prdata);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	bt_early_suspend.level =
		EARLY_SUSPEND_LEVEL_DISABLE_FB;
	bt_early_suspend.suspend = bt_earlysuspend;
	bt_early_suspend.resume = bt_lateresume;
	bt_early_suspend.param = pdev;
	register_early_suspend(&bt_early_suspend);
#endif

	platform_set_drvdata(pdev, pdata);

	/*1.Set BT_WAKE_HOST to the input state;*/
	/*2.Get interrupt number(irqno_wakeup).*/
	pdata->irqno_wakeup = gpio_to_irq(pdata->gpio_btwakeup);

	/*Register interrupt service function*/
	ret = request_irq(pdata->irqno_wakeup, bt_interrupt,
			IRQF_TRIGGER_FALLING, "bt-irq", (void *)pdata);
	if (ret < 0)
		pr_err("request_irq error ret=%d\n", ret);


	ret = device_init_wakeup(&pdev->dev, 1);
	if (ret)
		pr_err("device_init_wakeup failed: %d\n", ret);
	/*Wake up the interrupt*/
	ret = dev_pm_set_wake_irq(&pdev->dev, pdata->irqno_wakeup);
	if (ret)
		pr_err("dev_pm_set_wake_irq failed: %d\n", ret);

	INIT_WORK(&pdata->btwakeup_work, get_btwakeup_irq_work);

	//input
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("[abner test]input_allocate_device failed: %d\n", ret);
		return -EINVAL;
	}
	set_bit(EV_KEY,  input_dev->evbit);
	for (i = KEY_RESERVED; i < BTN_MISC; i++)
		set_bit(i, input_dev->keybit);

	input_dev->name = "input_btrcu";
	input_dev->phys = "input_btrcu/input0";
	input_dev->dev.parent = &pdev->dev;
	input_dev->id.bustype = BUS_ISA;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;
	input_dev->rep[REP_DELAY] = 0xffffffff;
	input_dev->rep[REP_PERIOD] = 0xffffffff;
	input_dev->keycodesize = sizeof(unsigned short);
	input_dev->keycodemax = 0x1ff;
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("[abner test]input_register_device failed: %d\n", ret);
		input_free_device(input_dev);
		return -EINVAL;
	}
	pdata->input_dev = input_dev;


	hrtimer_init(&pdata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pdata->timer.function = btwakeup_timer_handler;

	return 0;

err_rfkill:
	rfkill_destroy(bt_rfk);
err_rfk_alloc:
	bt_device_deinit(pdata);
err_res:
	return ret;

}

static int bt_remove(struct platform_device *pdev)
{
	struct bt_dev_runtime_data *prdata =
		platform_get_drvdata(pdev);
	struct rfkill *rfk = NULL;
	struct bt_dev_data *pdata = NULL;

	platform_set_drvdata(pdev, NULL);

	if (prdata) {
		rfk = prdata->bt_rfk;
		pdata = prdata->pdata;
	}

	if (pdata) {
		bt_device_deinit(pdata);
		kfree(pdata);
	}

	if (rfk) {
		rfkill_unregister(rfk);
		rfkill_destroy(rfk);
	}
	rfk = NULL;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id bt_dev_dt_match[] = {
	{	.compatible = "amlogic, bt-dev",
	},
	{},
};
#else
#define bt_dev_dt_match NULL
#endif

static struct platform_driver bt_driver = {
	.driver		= {
		.name	= "bt-dev",
		.of_match_table = bt_dev_dt_match,
	},
	.probe		= bt_probe,
	.remove		= bt_remove,
	.suspend	= bt_suspend,
	.resume	 = bt_resume,
};

static int __init bt_init(void)
{
	BT_INFO("amlogic rfkill init\n");

	return platform_driver_register(&bt_driver);
}
static void __exit bt_exit(void)
{
	platform_driver_unregister(&bt_driver);
}

module_param(btpower_evt, int, 0664);
MODULE_PARM_DESC(btpower_evt, "btpower_evt");

module_param(btwake_evt, int, 0664);
MODULE_PARM_DESC(btwake_evt, "btwake_evt");
module_init(bt_init);
module_exit(bt_exit);
MODULE_DESCRIPTION("bt rfkill");
MODULE_AUTHOR("");
MODULE_LICENSE("GPL");

/**************** bt mac *****************/

static int __init mac_addr_set(char *line)
{

	if (line) {
		BT_INFO("try to read bt mac from emmc key!\n");
		strncpy(bt_addr, line, sizeof(bt_addr)-1);
		bt_addr[sizeof(bt_addr)-1] = '\0';
	}

	return 1;
}

__setup("mac_bt=", mac_addr_set);


