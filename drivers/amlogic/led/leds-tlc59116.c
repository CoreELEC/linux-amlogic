/*
 * leds-tlc59116.c   tlc59116 led module
 *
 * Version: 1.0.0
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/leds.h>
//#include <linux/leds-tlc59116.h>
#include "leds-tlc59116.h"
#include "leds-tlc59116_mdata.h"

struct tlc59116 *tlc59116;


/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define TLC59116_I2C_NAME "tlc59116_led"

#define TLC59116_VERSION "v1.0.0"

#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 5
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 5

#define REG_MODE_1			0x00
#define REG_MODE_2			0x01
#define REG_PWM_0			0x02
#define REG_PWM_1			0x03
#define REG_PWM_2			0x04
#define REG_PWM_3			0x05
#define REG_PWM_4			0x06
#define REG_PWM_5			0x07
#define REG_PWM_6			0x08
#define REG_PWM_7			0x09
#define REG_PWM_8			0x0a
#define REG_PWM_9			0x0b
#define REG_PWM_10			0x0c
#define REG_PWM_11			0x0d
#define REG_PWM_12			0x0e
#define REG_PWM_13			0x0f
#define REG_PWM_14			0x10
#define REG_PWM_15			0x11
#define REG_GRPPWM			0x12
#define REG_GRPFREQ			0x13
#define REG_LEDOUT0			0x14
#define REG_LEDOUT1			0x15
#define REG_LEDOUT2			0x16
#define REG_LEDOUT3			0x17
#define REG_SUBADR1			0x18
#define REG_SUBADR2			0x19
#define REG_SUBADR3			0x1a
#define REG_ALLCALLADR			0x1b
#define REG_IREF			0x1c
#define REG_EFLAG1			0x1d
#define REG_EFLAG2			0x1e

/* tlc59116 register read/write access*/
#define REG_NONE_ACCESS		0
#define REG_RD_ACCESS		(1 << 0)
#define REG_WR_ACCESS		(1 << 1)
#define TLC59116_REG_MAX	0xFF

const unsigned char tlc59116_reg_access[TLC59116_REG_MAX] = {
	[REG_MODE_1] = REG_RD_ACCESS,
	[REG_MODE_2] = REG_RD_ACCESS,
	[REG_PWM_0] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PWM_1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PWM_2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PWM_3] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PWM_4] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PWM_5] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PWM_6] = REG_RD_ACCESS,
	[REG_PWM_7] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PWM_8] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PWM_9] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PWM_10] = REG_WR_ACCESS,
	[REG_PWM_11] = REG_WR_ACCESS,
	[REG_PWM_12] = REG_WR_ACCESS,
	[REG_PWM_13] = REG_WR_ACCESS,
	[REG_PWM_14] = REG_WR_ACCESS,
	[REG_PWM_15] = REG_WR_ACCESS,
	[REG_GRPPWM] = REG_WR_ACCESS,
	[REG_GRPFREQ] = REG_WR_ACCESS,
	[REG_LEDOUT0] = REG_WR_ACCESS,
	[REG_LEDOUT1] = REG_WR_ACCESS,
	[REG_LEDOUT2] = REG_WR_ACCESS,
	[REG_LEDOUT3] = REG_WR_ACCESS,
	[REG_SUBADR1] = REG_WR_ACCESS,
	[REG_SUBADR2] = REG_WR_ACCESS,
	[REG_SUBADR3] = REG_WR_ACCESS,
	[REG_ALLCALLADR] = REG_WR_ACCESS,
	[REG_IREF] = REG_WR_ACCESS,
	[REG_EFLAG1] = REG_WR_ACCESS,
	[REG_EFLAG2] = REG_WR_ACCESS,
};

static int tlc59116_i2c_writes(struct i2c_client *client,
			       unsigned char sreg_addr,
			       uint8_t length, uint8_t *bufs)
{

	int ret = -1;
	//if(length >=16)
	//	length = 16;

	//pr_err("[tlc59116_i2c_writes]into....slave = 0x%x,
	//	 reg_addr = 0x%2x\n",
	//	 client->addr, sreg_addr);

	struct i2c_msg msg;
	int i, retries = 0;
	uint8_t data[64];

	data[0] = sreg_addr;
	/*start register addr<--write*/
	for (i = 1; i <= length; i++) {
		data[i] = bufs[i-1];
		//pr_err("[tlc59116_i2c_writes]into....data[%d] = 0x%x\n",
		//	 i, bufs[i-1]);
	}

	msg.addr = client->addr;
	msg.flags = !I2C_M_RD;
	msg.len = length+1;
	msg.buf = data;

	while (retries < 3) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			//pr_err("[tlc59116_i2c_writes]success ret = %d\n",ret);
			ret = 1;
			break;
		}

		pr_err("[tlc59116_i2c_writes]continue...\n");
		retries++;
	}

	if (retries >= 3)
		pr_err("[tlc59116_i2c_writes]error...retyies = %d\n", retries);

	return ret;
}

static void leds_tlc59116_work_func(struct work_struct *work)
{
	int i, j;

	while (cycles_val == 0) {
		for (j = 0; j < instruct_val; j++) {
			tlc59116_i2c_writes(tlc59116->i2c,
					   (REG_PWM_0 | (0x5 << 5)),
					   16, tmp_w[j]);
			usleep_range(1000 * (time_val - 10),
				     1000 * (time_val + 10));
			if (state_mode == 0)
				break;
		}
		if (state_mode == 0)
			break;
	}

	for (i = 1; i <= cycles_val; i++) {
		for (j = 0; j < instruct_val; j++) {
			tlc59116_i2c_writes(tlc59116->i2c,
					   (REG_PWM_0 | (0x5 << 5)),
					   16, tmp_w[j]);
			usleep_range(1000 * (time_val - 10),
				     1000 * (time_val + 10));
			if (state_mode == 0)
				break;
		}
		if (state_mode == 0)
			break;
	}
}


static void init_leds_mode(int leds_mode)
{
	int offset, i, j;

	time_val = *(all_data[leds_mode] + 0);
	cycles_val = *((all_data[leds_mode]) + 1);
	instruct_val = (length_data[leds_mode] - 2) / 16;

	//pr_info("[%s:leds_mode=%d]time_val=%d,cycles_val=%d,
	//	  instruct_val=%d\n", __func__, leds_mode, time_val,
	//	  cycles_val, instruct_val);

	for (i = 0; i < instruct_val; i++) {
		for (j = 0; j < 16; j++) {
			offset = 16 * i + j;
			tmp_w[i][j] = *((all_data[leds_mode]) + offset + 2);
			//pr_info("tmp_w[%d][%d]=%x......\n",i,j,tmp_w[i][j]);
		}
	}
}

static void init_leds_config(void)
{
	tlc59116_hw_reset(tlc59116);

	tlc59116_i2c_write(tlc59116, REG_MODE_1, 0x00);
	tlc59116_i2c_write(tlc59116, REG_MODE_2, 0x00);

	tlc59116_i2c_write(tlc59116, REG_LEDOUT0, 0xaa);
	tlc59116_i2c_write(tlc59116, REG_LEDOUT1, 0xaa);
	tlc59116_i2c_write(tlc59116, REG_LEDOUT2, 0xaa);
	tlc59116_i2c_write(tlc59116, REG_LEDOUT3, 0xaa);

	tlc59116_i2c_write(tlc59116, REG_PWM_0, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_1, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_2, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_3, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_4, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_5, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_6, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_7, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_8, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_9, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_10, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_11, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_12, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_13, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_14, 0X00);
	tlc59116_i2c_write(tlc59116, REG_PWM_15, 0X00);
}

static void init_leds_data(void)
{
	all_data[0] = (int *)&data_mode0;
	all_data[1] = (int *)&data_mode1;
	all_data[2] = (int *)&data_mode2;
	all_data[3] = (int *)&data_mode3;
	all_data[4] = (int *)&data_mode4;
	all_data[5] = (int *)&data_mode5;
	all_data[6] = (int *)&data_mode6;
	all_data[7] = (int *)&data_mode7;
	all_data[8] = (int *)&data_mode8;
	all_data[9] = (int *)&data_mode9;
//	all_data[10] = (int *)&data_mode10;
//	all_data[11] = (int *)&data_mode11;
//	all_data[12] = (int *)&data_mode12;

	length_data[0] = ARRAY_SIZE(data_mode0);
	length_data[1] = ARRAY_SIZE(data_mode1);
	length_data[2] = ARRAY_SIZE(data_mode2);
	length_data[3] = ARRAY_SIZE(data_mode3);
	length_data[4] = ARRAY_SIZE(data_mode4);
	length_data[5] = ARRAY_SIZE(data_mode5);
	length_data[6] = ARRAY_SIZE(data_mode6);
	length_data[7] = ARRAY_SIZE(data_mode7);
	length_data[8] = ARRAY_SIZE(data_mode8);
	length_data[9] = ARRAY_SIZE(data_mode9);
//	length_data[10] = ARRAY_SIZE(data_mode10);
//	length_data[11] = ARRAY_SIZE(data_mode11);
	init_leds_config();
}


/******************************************************
 *
 * tlc59116 i2c write/read
 *
 ******************************************************/
static int tlc59116_i2c_write(struct tlc59116 *tlc59116,
			      unsigned char reg_addr,
			      unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(tlc59116->i2c,
						reg_addr,
						reg_data);
		if (ret < 0)
			pr_err("%s: i2c_write cnt = %d error = %d\n",
			       __func__, cnt, ret);
		else
			break;

		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int tlc59116_i2c_read(struct tlc59116 *tlc59116,
			     unsigned char reg_addr,
			     unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(tlc59116->i2c, reg_addr);
		if (ret < 0) {
			pr_err("%s: i2c_read cnt = %d error = %d\n",
			       __func__, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}

		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

/******************************************************
 *
 * tlc59116 led
 *
 ******************************************************/
static void tlc59116_brightness_work(struct work_struct *work)
{
}

static void tlc59116_set_brightness(struct led_classdev *cdev,
				    enum led_brightness brightness)
{
	struct tlc59116 *tlc59116 = container_of(cdev, struct tlc59116, cdev);

	tlc59116->cdev.brightness = brightness;

	schedule_work(&tlc59116->brightness_work);
}

static int tlc59116_hw_reset(struct tlc59116 *tlc59116)
{
	pr_info("%s enter,Line:%d\n", __func__, __LINE__);

	if (tlc59116 && gpio_is_valid(tlc59116->reset_gpio)) {
		gpio_set_value_cansleep(tlc59116->reset_gpio, 0);
		//msleep(1);
		gpio_set_value_cansleep(tlc59116->reset_gpio, 1);
		//msleep(1);
	} else {
		dev_err(tlc59116->dev, "%s:  failed\n", __func__);
	}
	return 0;
}
#if 0
static int tlc59116_hw_off(struct tlc59116 *tlc59116)
{
	pr_info("%s enter,Line:%d\n", __func__, __LINE__);

	if (tlc59116 && gpio_is_valid(tlc59116->reset_gpio)) {
		gpio_set_value_cansleep(tlc59116->reset_gpio, 0);
		//msleep(1);
	} else {
		dev_err(tlc59116->dev, "%s:  failed\n", __func__);
	}
	return 0;
}
#endif
/******************************************************
 *
 * sys group attribute: mode
 *
 ******************************************************/
static ssize_t tlc59116_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct tlc59116 *tlc59116 = container_of(led_cdev,
						 struct tlc59116, cdev);
	int ret;

	state_mode = 0;
	usleep_range(80000, 100000);
	cancel_work_sync(&tlc59116->leds_work);
	flush_workqueue(tlc59116->leds_wq);
//	pr_info("%s enter,Line:%d...state_mode=%d\n",
//		__func__, __LINE__, state_mode);
	ret = kstrtouint(buf, 10, &state_mode);
	if (ret) {
		pr_info("%s enter,Line:%d...state_mode=%d set mode fail\n",
		__func__, __LINE__, state_mode);
		return count;
	}
	pr_info("%s enter,Line:%d...state_mode=%d\n",
		__func__, __LINE__, state_mode);
	init_leds_mode(state_mode);
	queue_work(tlc59116->leds_wq, &tlc59116->leds_work);

	return count;
}

static ssize_t tlc59116_mode_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "current mode:%d\n",
			state_mode);

	return len;
}


/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t tlc59116_reg_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct tlc59116 *tlc59116 = container_of(led_cdev, struct tlc59116,
						 cdev);

	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		tlc59116_i2c_write(tlc59116, (unsigned char)databuf[0],
				   (unsigned char)databuf[1]);

	return count;
}

static ssize_t tlc59116_reg_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct tlc59116 *tlc59116 = container_of(led_cdev,
						 struct tlc59116, cdev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < TLC59116_REG_MAX; i++) {
		if (!(tlc59116_reg_access[i]&REG_RD_ACCESS))
			continue;

		tlc59116_i2c_read(tlc59116, i, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len,
				"reg:0x%02x=0x%02x\n", i, reg_val);
	}

	return len;
}
#if 0
static ssize_t tlc59116_hwen_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct tlc59116 *tlc59116 = container_of(led_cdev,
						 struct tlc59116,
						 cdev);
	unsigned int databuf[1] = {0};

	tlc59116_hw_off(tlc59116);

	return count;
}

static ssize_t tlc59116_hwen_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct tlc59116 *tlc59116 = container_of(led_cdev,
						 struct tlc59116,
						 cdev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "hwen=%d\n",
	gpio_get_value(tlc59116->reset_gpio));

	return len;
}
#endif
static DEVICE_ATTR(reg, 0644, tlc59116_reg_show, tlc59116_reg_store);
//static DEVICE_ATTR(hwen, 0644, tlc59116_hwen_show, tlc59116_hwen_store);
static DEVICE_ATTR(mode, 0644, tlc59116_mode_show, tlc59116_mode_store);

static struct attribute *tlc59116_attributes[] = {
	&dev_attr_reg.attr,
	//&dev_attr_hwen.attr,
	&dev_attr_mode.attr,
	NULL
};

static struct attribute_group tlc59116_attribute_group = {
	.attrs = tlc59116_attributes
};


/******************************************************
 *
 * led class dev
 *
 ******************************************************/
static int tlc59116_parse_led_cdev(struct tlc59116 *tlc59116,
				   struct device_node *np)
{
	struct device_node *temp;
	int ret = -1;

	for_each_child_of_node(np, temp) {
		ret = of_property_read_string(temp, "tlc59116,name",
		&tlc59116->cdev.name);
		if (ret < 0) {
			dev_err(tlc59116->dev,
				"Failure reading led name ret = %d\n", ret);
			goto free_pdata;
		}

		ret = of_property_read_u32(temp, "tlc59116,imax",
					   &tlc59116->imax);
		if (ret < 0) {
			dev_err(tlc59116->dev,
				"Failure reading imax ret = %d\n", ret);
			goto free_pdata;
		}

		ret = of_property_read_u32(temp, "tlc59116,brightness",
					   &tlc59116->cdev.brightness);
		if (ret < 0) {
			dev_err(tlc59116->dev,
				"Failure reading brightness ret = %d\n", ret);
			goto free_pdata;
		}

		ret = of_property_read_u32(temp, "tlc59116,max_brightness",
					   &tlc59116->cdev.max_brightness);
		if (ret < 0) {
			dev_err(tlc59116->dev,
				"Failure reading max brightness ret = %d\n",
				ret);
			goto free_pdata;
		}
	}

	INIT_WORK(&tlc59116->brightness_work, tlc59116_brightness_work);
	tlc59116->cdev.brightness_set = tlc59116_set_brightness;
	ret = led_classdev_register(tlc59116->dev, &tlc59116->cdev);
	if (ret) {
		dev_err(tlc59116->dev, "unable to register led ret=%d\n", ret);
		goto free_pdata;
	}

	ret = sysfs_create_group(&tlc59116->cdev.dev->kobj,
				 &tlc59116_attribute_group);
	if (ret) {
		dev_err(tlc59116->dev, "led sysfs ret: %d\n", ret);
		goto free_class;
	}

	return 0;
free_class:
	led_classdev_unregister(&tlc59116->cdev);
free_pdata:
	return ret;
}

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int tlc59116_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	struct device_node *np = i2c->dev.of_node;
	int ret;

	pr_info("%s enter,Line:%d\n", __func__, __LINE__);

	if (!i2c_check_functionality(i2c->adapter,
				     I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	tlc59116 = devm_kzalloc(&i2c->dev, sizeof(struct tlc59116),
				GFP_KERNEL);
	if (tlc59116 == NULL)
		return -ENOMEM;

	tlc59116->dev = &i2c->dev;
	tlc59116->i2c = i2c;
	i2c_set_clientdata(i2c, tlc59116);
	dev_set_drvdata(&i2c->dev, tlc59116);
	ret = tlc59116_parse_led_cdev(tlc59116, np);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s error creating led class dev\n",
			__func__);
		goto err_sysfs;
	}

	init_leds_data();
	INIT_WORK(&tlc59116->leds_work, leds_tlc59116_work_func);
	tlc59116->leds_wq = create_singlethread_workqueue("leds_wq");
	if (tlc59116->leds_wq == NULL) {
		dev_err(&i2c->dev, "create leds_work->input_wq fail!\n");
		return -ENOMEM;
	}

	queue_work(tlc59116->leds_wq, &tlc59116->leds_work);

	return 0;
err_sysfs:
	return ret;
}

static int tlc59116_i2c_remove(struct i2c_client *i2c)
{
	struct tlc59116 *tlc59116 = i2c_get_clientdata(i2c);

	pr_info("%s enter\n", __func__);

	tlc59116_hw_reset(tlc59116);

	cancel_work_sync(&tlc59116->leds_work);
	destroy_workqueue(tlc59116->leds_wq);
	devm_kfree(&i2c->dev, tlc59116);

	if (gpio_is_valid(tlc59116->reset_gpio))
		devm_gpio_free(&i2c->dev, tlc59116->reset_gpio);

	return 0;
}

static const struct i2c_device_id tlc59116_i2c_id[] = {
	{ TLC59116_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tlc59116_i2c_id);

static const struct of_device_id tlc59116_dt_match[] = {
	{ .compatible = "amlogic,tlc59116_led" },
	{ }
};

static struct i2c_driver tlc59116_i2c_driver = {
	.driver = {
		.name = TLC59116_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tlc59116_dt_match),
	},
	.probe = tlc59116_i2c_probe,
	.remove = tlc59116_i2c_remove,
	.id_table = tlc59116_i2c_id,
};


static int __init tlc59116_i2c_init(void)
{
	int ret = 0;

	pr_info("tlc59116 driver version %s\n", TLC59116_VERSION);

	ret = i2c_add_driver(&tlc59116_i2c_driver);
	if (ret) {
		pr_err("fail to add tlc59116 device into i2c\n");
		return ret;
	}

	return 0;
}
late_initcall(tlc59116_i2c_init);

static void __exit tlc59116_i2c_exit(void)
{
//	tlc59116_hw_reset(tlc59116);
	i2c_del_driver(&tlc59116_i2c_driver);
}
module_exit(tlc59116_i2c_exit);
MODULE_DESCRIPTION("TLC59116 LED Driver");
MODULE_LICENSE("GPL v2");
