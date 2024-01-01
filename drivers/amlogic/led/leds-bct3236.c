// SPDX-License-Identifier: GPL-2.0+
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (C) 2023 CoreELEC.
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * BSD LICENSE
 *
 * Copyright (C) 2023 CoreELEC.
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

struct group_rgb {
	u32 red;
	u32 green;
	u32 blue;
};

struct bct3236 {
	struct device *dev;
	struct i2c_client *i2c;
	struct led_classdev cdev;
	struct group_rgb *io;
	int led_counts;
	u32 *led_colors;
	int ignore_led_suspend;
	int brightness;
	u32 edge_color_on;
	u32 edge_color_off;
	u32 edge_color_suspend;
};

#define CONV_BRI(bri, state) ((((bri)-1) ^ 0x03) << 1 | (state))

#define BCT3236_I2C_NAME "bct3236_led"
#define LEDS_CDEV_NAME "bct3236"

#define BCT3236_MAX_IO 36
#define BCT3236_COLORS_COUNT 3
#define BCT3236_MAX_LED (BCT3236_MAX_IO / BCT3236_COLORS_COUNT)

#define BCT3236_BRI_IOUT_MIN 1
#define BCT3236_BRI_IOUT_MAX 4

#define BCT3236_BRI_IOUT_LED_ON 0x01
#define BCT3236_BRI_IOUT_LED_OFF 0x00

#define BCT3236_REG_SHUTDOWN 0x00
#define BCT3236_REG_BRIGHTNESS_LED 0x01
#define BCT3236_REG_UPDATE_LED 0x25
#define BCT3236_REG_LED_CONTROL 0x26
#define BCT3236_REG_GLOBAL_CONTROL 0x4a
#define BCT3236_REG_RESET 0x4f
#define BCT3236_REG_MAX 0x50

#define RGB_TO_COLOR(rgb) ((rgb).red << 16 | (rgb).green << 8 | (rgb).blue)
#define COLOR_TO_RED(color) (((color) >> 16) & 0xff)
#define COLOR_TO_GREEN(color) (((color) >> 8) & 0xff)
#define COLOR_TO_BLUE(color) ((color)&0xff)

static uint debug = 0;
module_param(debug, uint, 0644);
static DEFINE_MUTEX(bct3236_lock);

static int bct3236_i2c_writes(struct bct3236 *bct3236, u8 reg_addr, int len,
			      u8 *bufs)
{
	struct i2c_msg msg;
	int i, ret;
	u8 data[1 + BCT3236_MAX_IO];

	if (!bct3236->i2c) {
		pr_err("%s: i2c client not set\n", __func__);
		return -1;
	}

	if (len >= BCT3236_MAX_IO) {
		pr_err("%s: length is too long\n", __func__);
		len = BCT3236_MAX_IO;
	}

	if (debug) {
		if (len == 1) {
			pr_info("%s: len = %d addr = %02x data = %02x\n",
				__func__, len, reg_addr, bufs[0]);
		} else {
			for (i = 1; i < len; i++) {
				if (bufs[0] != bufs[i])
					break;
			}

			if (i == len) {
				pr_info("%s: len = %d addr = %02x data = %02x ...",
					__func__, len, reg_addr, bufs[0]);
			} else {
				pr_info("%s: len = %d addr = %02x data =",
					__func__, len, reg_addr);

				for (i = 0; i < len; i++)
					pr_cont(" %02x", bufs[i]);
				pr_cont("\n");
			}
		}
	}

	data[0] = reg_addr;
	for (i = 1; i <= len; i++)
		data[i] = bufs[i - 1];

	msg.addr = bct3236->i2c->addr;
	msg.flags = !I2C_M_RD;
	msg.len = len + 1;
	msg.buf = data;

	ret = i2c_transfer(bct3236->i2c->adapter, &msg, 1);
	if (ret < 0) {
		pr_err("%s: i2c transfer failed with error %d\n", __func__,
		       ret);
		return ret;
	}

	return 0;
}

static int bct3236_i2c_write(struct bct3236 *bct3236, u8 reg_addr, u8 reg_data)
{
	int ret = -1;

	if (!bct3236->i2c) {
		pr_err("%s: i2c client not set\n", __func__);
		return ret;
	}

	if (debug)
		pr_info("%s: addr = %02x data = %02x\n", __func__, reg_addr,
			reg_data);

	ret = i2c_smbus_write_byte_data(bct3236->i2c, reg_addr, reg_data);
	if (ret < 0) {
		pr_err("%s: i2c write failed with error %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static ssize_t io_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bct3236 *bct3236 = container_of(led_cdev, struct bct3236, cdev);
	ssize_t len = 0;
	int i;
	int led_num;

	mutex_lock(&bct3236_lock);

	for (i = 0; i < bct3236->led_counts; i++) {
		led_num = (bct3236->led_counts - 1) - i;

		len += snprintf(buf + len, PAGE_SIZE - len,
				"led[%d]  R: %2d  G: %2d  B: %2d\n", i,
				bct3236->io[led_num].red,
				bct3236->io[led_num].green,
				bct3236->io[led_num].blue);
	}

	mutex_unlock(&bct3236_lock);
	return len;
}

static int bct3236_set_colors(struct bct3236 *bct3236)
{
	struct group_rgb *io;
	u8 color_data[BCT3236_MAX_IO] = {};
	int i, ret;
	int led_num;

	if (debug) {
		pr_info("%s: colors =", __func__);

		if (bct3236->led_counts == 1) {
			pr_cont(" %06x\n", bct3236->led_colors[0]);
		} else {
			for (i = 1; i < bct3236->led_counts; i++)
				if (bct3236->led_colors[0] !=
				    bct3236->led_colors[i])
					break;

			if (i == bct3236->led_counts) {
				pr_cont(" %06x ...\n", bct3236->led_colors[0]);
			} else {
				for (i = 0; i < bct3236->led_counts; i++)
					pr_cont(" %06x",
						bct3236->led_colors[i]);
				pr_cont("\n");
			}
		}
	}

	for (i = 0; i < bct3236->led_counts; i++) {
		led_num = (bct3236->led_counts - 1) - i;
		io = &bct3236->io[led_num];

		color_data[io->red] = COLOR_TO_RED(bct3236->led_colors[i]);
		color_data[io->green] = COLOR_TO_GREEN(bct3236->led_colors[i]);
		color_data[io->blue] = COLOR_TO_BLUE(bct3236->led_colors[i]);
	}

	ret = bct3236_i2c_writes(bct3236, BCT3236_REG_BRIGHTNESS_LED,
				 bct3236->led_counts * BCT3236_COLORS_COUNT,
				 color_data);
	if (ret < 0)
		return ret;

	return bct3236_i2c_write(bct3236, BCT3236_REG_UPDATE_LED, 0x00);
}

static ssize_t edge_color_on_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bct3236 *bct3236 = container_of(led_cdev, struct bct3236, cdev);
	u32 color;
	int i, ret;

	mutex_lock(&bct3236_lock);

	ret = sscanf(buf, "%x", &color);
	if (ret != 1) {
		pr_info("bct3236_%s: Invalid number of arguments\n", __func__);
		goto unlock;
	}

	bct3236->edge_color_on = color;
	for (i = 0; i < bct3236->led_counts; i++)
		bct3236->led_colors[i] = color;

	ret = bct3236_set_colors(bct3236);
	if (ret < 0)
		pr_info("bct3236_%s: set color failed\n", __func__);

unlock:
	mutex_unlock(&bct3236_lock);
	return count;
}

static ssize_t edge_color_off_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bct3236 *bct3236 = container_of(led_cdev, struct bct3236, cdev);
	u32 color;
	int ret;

	mutex_lock(&bct3236_lock);

	ret = sscanf(buf, "%x", &color);
	if (ret != 1) {
		pr_info("bct3236_%s: Invalid number of arguments\n", __func__);
		goto unlock;
	}

	bct3236->edge_color_off = color;

unlock:
	mutex_unlock(&bct3236_lock);
	return count;
}

static ssize_t edge_color_suspend_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bct3236 *bct3236 = container_of(led_cdev, struct bct3236, cdev);
	u32 color;
	int ret;

	mutex_lock(&bct3236_lock);

	ret = sscanf(buf, "%x", &color);
	if (ret != 1) {
		pr_info("bct3236_%s: Invalid number of arguments\n", __func__);
		goto unlock;
	}

	bct3236->edge_color_suspend = color;

unlock:
	mutex_unlock(&bct3236_lock);
	return count;
}

static ssize_t colors_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bct3236 *bct3236 = container_of(led_cdev, struct bct3236, cdev);
	u32 colors[BCT3236_MAX_LED] = {};
	int i, ret;

	mutex_lock(&bct3236_lock);

	ret = sscanf(buf, "%x %x %x %x %x %x %x %x %x %x %x %x", &colors[0],
		     &colors[1], &colors[2], &colors[3], &colors[4], &colors[5],
		     &colors[6], &colors[7], &colors[8], &colors[9],
		     &colors[10], &colors[11]);
	if (ret != bct3236->led_counts) {
		pr_info("bct3236_%s: Invalid number of arguments\n", __func__);
		goto unlock;
	}

	for (i = 0; i < bct3236->led_counts; i++)
		bct3236->led_colors[i] = colors[i];

	ret = bct3236_set_colors(bct3236);
	if (ret < 0)
		pr_info("bct3236_%s: store colors failed\n", __func__);

unlock:
	mutex_unlock(&bct3236_lock);
	return count;
}

static ssize_t colors_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bct3236 *bct3236 = container_of(led_cdev, struct bct3236, cdev);
	int len = 0;
	int i;

	mutex_lock(&bct3236_lock);

	len = snprintf(buf, PAGE_SIZE - len, "     current: ");
	for (i = 0; i < bct3236->led_counts; i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%06x ",
				bct3236->led_colors[i]);
	buf[len - 1] = '\n'; /* last space to newline */

	len += snprintf(buf + len, PAGE_SIZE - len, "     edge on: %06x\n",
			bct3236->edge_color_on);
	len += snprintf(buf + len, PAGE_SIZE - len, "    edge off: %06x\n",
			bct3236->edge_color_off);
	len += snprintf(buf + len, PAGE_SIZE - len, "edge suspend: %06x\n",
			bct3236->edge_color_suspend);

	mutex_unlock(&bct3236_lock);
	return len;
}

static int bct3236_set_single_color(struct bct3236 *bct3236, int led_num)
{
	struct group_rgb *io;
	u8 color_data[BCT3236_MAX_IO] = {};
	int ret;

	if (debug)
		pr_info("%s: color = %06x\n", __func__,
			bct3236->led_colors[led_num]);

	io = &bct3236->io[led_num];
	color_data[io->red] = COLOR_TO_RED(bct3236->led_colors[led_num]);
	color_data[io->green] = COLOR_TO_GREEN(bct3236->led_colors[led_num]);
	color_data[io->blue] = COLOR_TO_BLUE(bct3236->led_colors[led_num]);

	ret = bct3236_i2c_writes(bct3236,
				 BCT3236_REG_BRIGHTNESS_LED +
					 (led_num * BCT3236_COLORS_COUNT),
				 BCT3236_COLORS_COUNT,
				 color_data + (led_num * BCT3236_COLORS_COUNT));
	if (ret < 0)
		return ret;

	return bct3236_i2c_write(bct3236, BCT3236_REG_UPDATE_LED, 0x00);
}

static ssize_t single_color_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int led_num;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bct3236 *bct3236 = container_of(led_cdev, struct bct3236, cdev);
	u32 color = 0;
	int ret;

	mutex_lock(&bct3236_lock);

	ret = sscanf(buf, "%u %x", &led_num, &color);
	if (ret != 2) {
		pr_info("bct3236_%s: Invalid number of arguments\n", __func__);
		goto unlock;
	}

	if (led_num == 0 || led_num > bct3236->led_counts) {
		pr_info("%s: Wrong led number!\n", __func__);
		goto unlock;
	}

	led_num--; /* leds starts with 0 */
	led_num = (bct3236->led_counts - 1) - led_num;

	bct3236->led_colors[led_num] = color;
	ret = bct3236_set_single_color(bct3236, led_num);
	if (ret < 0)
		pr_info("bct3236_%s: set color failed\n", __func__);

unlock:
	mutex_unlock(&bct3236_lock);
	return count;
}

static ssize_t reg_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bct3236 *bct3236 = container_of(led_cdev, struct bct3236, cdev);
	int ret;
	u8 reg_addr, reg_data;

	mutex_lock(&bct3236_lock);

	if (sscanf(buf, "%hhx %hhx", &reg_addr, &reg_data) != 2) {
		pr_err("bct3236_%s: Invalid number of arguments\n", __func__);
		goto unlock;
	}

	if (reg_addr >= BCT3236_REG_MAX) {
		pr_err("bct3236_%s: error max register addr = 0x%x\n", __func__,
		       BCT3236_REG_MAX);
		goto unlock;
	}

	if (debug)
		pr_info("bct3236_%s: addr = 0x%02x data = 0x%02x\n", __func__,
			reg_addr, reg_data);

	ret = bct3236_i2c_write(bct3236, reg_addr, reg_data);
	if (ret < 0)
		pr_info("bct3236_%s: set register failed\n", __func__);

unlock:
	mutex_unlock(&bct3236_lock);
	return count;
}

static ssize_t debug_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	unsigned int debug_read = 0;

	if (!kstrtoint(buf, 10, &debug_read)) {
		debug = debug_read;
		pr_info("bct3236_%s: bct3236 debug = %u (%s)\n", __func__,
			debug, debug ? "on" : "off");
	}

	return count;
}

static ssize_t debug_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%d\n", debug);
}

static DEVICE_ATTR_RO(io);
static DEVICE_ATTR_WO(edge_color_on);
static DEVICE_ATTR_WO(edge_color_off);
static DEVICE_ATTR_WO(edge_color_suspend);
static DEVICE_ATTR_RW(colors);
static DEVICE_ATTR_WO(single_color);
static DEVICE_ATTR_WO(reg);
static DEVICE_ATTR_RW(debug);

static struct attribute *bct3236_attributes[] = {
	&dev_attr_io.attr,
	&dev_attr_edge_color_on.attr,
	&dev_attr_edge_color_off.attr,
	&dev_attr_edge_color_suspend.attr,
	&dev_attr_colors.attr,
	&dev_attr_single_color.attr,
	&dev_attr_reg.attr,
	&dev_attr_debug.attr,
	NULL
};

static struct attribute_group bct3236_attribute_group = {
	.attrs = bct3236_attributes
};

static int bct3236_init(struct bct3236 *bct3236)
{
	u8 brightness_iout[BCT3236_MAX_IO];
	u8 brightness_reg;
	int i, ret;

	if (bct3236_i2c_write(bct3236, BCT3236_REG_RESET, 0x00) < 0 ||
	    bct3236_i2c_write(bct3236, BCT3236_REG_SHUTDOWN, 0x01) < 0 ||
	    bct3236_i2c_write(bct3236, BCT3236_REG_GLOBAL_CONTROL, 0x00) < 0)
		return -1;

	if (bct3236->cdev.default_trigger &&
	    strncmp(bct3236->cdev.default_trigger, "none", 4) != 0 &&
	    bct3236->brightness > 0)
		brightness_reg =
			CONV_BRI(bct3236->brightness, BCT3236_BRI_IOUT_LED_ON);
	else
		brightness_reg = CONV_BRI(BCT3236_BRI_IOUT_MIN,
					  BCT3236_BRI_IOUT_LED_OFF);

	for (i = 0; i < BCT3236_MAX_IO; i++)
		brightness_iout[i] = brightness_reg;

	ret = bct3236_i2c_writes(bct3236, BCT3236_REG_LED_CONTROL,
				 bct3236->led_counts * BCT3236_COLORS_COUNT,
				 brightness_iout);
	if (ret < 0)
		return 0;

	return bct3236_set_colors(bct3236);
}

static enum led_brightness bct3236_brightness_get(struct led_classdev *cdev)
{
	struct bct3236 *bct3236 = container_of(cdev, struct bct3236, cdev);

	return (enum led_brightness)bct3236->cdev.brightness;
}

static int bct3236_brightness_set(struct led_classdev *cdev,
				  enum led_brightness brightness)
{
	struct bct3236 *bct3236 = container_of(cdev, struct bct3236, cdev);
	int i, ret;
	u8 brightness_val;
	u8 brightness_iout[BCT3236_MAX_IO];

	mutex_lock(&bct3236_lock);

	if (brightness > 0)
		brightness_val = CONV_BRI(brightness, BCT3236_BRI_IOUT_LED_ON);
	else
		brightness_val = CONV_BRI(BCT3236_BRI_IOUT_MIN,
					  BCT3236_BRI_IOUT_LED_OFF);

	if (debug)
		pr_info("%s: brightness = %d reg val = 0x%02x\n", __func__,
			brightness, brightness_val);

	for (i = 0; i < BCT3236_MAX_IO; i++)
		brightness_iout[i] = brightness_val;

	ret = bct3236_i2c_writes(bct3236, BCT3236_REG_LED_CONTROL,
				 bct3236->led_counts * BCT3236_COLORS_COUNT,
				 brightness_iout);
	if (ret < 0)
		goto unlock;

	ret = bct3236_i2c_write(bct3236, BCT3236_REG_UPDATE_LED, 0x00);

unlock:
	mutex_unlock(&bct3236_lock);
	return ret;
}

static int bct3236_parse_led_dt(struct bct3236 *bct3236, struct device_node *np)
{
	struct device_node *temp;
	struct group_rgb color_rgb;
	int i = 0;
	int ret = -1;

	bct3236->cdev.default_trigger =
		of_get_property(np, "linux,default-trigger", NULL);

	if (of_property_read_bool(np, "ignore-led-suspend"))
		bct3236->ignore_led_suspend = 1;

	ret = of_property_read_u32(np, "brightness", &bct3236->brightness);
	if (!ret) {
		if (bct3236->brightness > BCT3236_BRI_IOUT_MAX)
			bct3236->brightness = BCT3236_BRI_IOUT_MAX;
	} else
		bct3236->brightness = BCT3236_BRI_IOUT_MIN;

	for_each_child_of_node (np, temp) {
		ret = of_property_read_u32_array(temp, "default_colors",
						 (u32 *)&color_rgb,
						 BCT3236_COLORS_COUNT);
		if (ret < 0) {
			pr_err("Failure reading default colors ret = %d\n",
			       ret);
			return ret;
		} else {
			bct3236->led_colors[i] = RGB_TO_COLOR(color_rgb);
		}

		ret = of_property_read_u32(temp, "red_io_number",
					   &bct3236->io[i].red);
		if (ret < 0) {
			pr_err("Failure reading red io number ret = %d\n", ret);
			return ret;
		}

		ret = of_property_read_u32(temp, "green_io_number",
					   &bct3236->io[i].green);
		if (ret < 0) {
			pr_err("Failure reading green io number ret = %d\n",
			       ret);
			return ret;
		}

		ret = of_property_read_u32(temp, "blue_io_number",
					   &bct3236->io[i].blue);
		if (ret < 0) {
			pr_err("Failure reading blue io number ret = %d\n",
			       ret);
			return ret;
		}

		if (bct3236->io[i].red >= BCT3236_MAX_IO ||
		    bct3236->io[i].green >= BCT3236_MAX_IO ||
		    bct3236->io[i].blue >= BCT3236_MAX_IO) {
			pr_err("%s: io number wrong value\n", __func__);
			return -EINVAL;
		}

		i++;
	}

	/* overwrite values with just single color */
	ret = of_property_read_u32_array(np, "edge_color_on", (u32 *)&color_rgb,
					 BCT3236_COLORS_COUNT);
	if (!ret) {
		bct3236->edge_color_on = RGB_TO_COLOR(color_rgb);

		for (i = 0; i < bct3236->led_counts; i++)
			bct3236->led_colors[i] = RGB_TO_COLOR(color_rgb);
	}

	ret = of_property_read_u32_array(
		np, "edge_color_off", (u32 *)&color_rgb, BCT3236_COLORS_COUNT);
	if (!ret)
		bct3236->edge_color_off = RGB_TO_COLOR(color_rgb);

	ret = of_property_read_u32_array(np, "edge_color_suspend",
					 (u32 *)&color_rgb,
					 BCT3236_COLORS_COUNT);
	if (!ret)
		bct3236->edge_color_suspend = RGB_TO_COLOR(color_rgb);

	return 0;
}

static int bct3236_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct device_node *np = i2c->dev.of_node;
	struct bct3236 *bct3236;
	int ret;

	pr_info("%s: loading driver\n", __func__);

	if (!i2c_check_functionality(i2c->adapter,
				     I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL)) {
		pr_err("%s: i2c functionality failed\n", __func__);
		return -EIO;
	}

	bct3236 = devm_kzalloc(&i2c->dev, sizeof(struct bct3236), GFP_KERNEL);
	if (!bct3236)
		return -ENOMEM;

	bct3236->dev = &i2c->dev;
	bct3236->i2c = i2c;
	bct3236->led_counts = device_get_child_node_count(&i2c->dev);

	if (bct3236->led_counts > BCT3236_MAX_LED)
		bct3236->led_counts = BCT3236_MAX_LED;

	bct3236->io =
		devm_kzalloc(&i2c->dev,
			     bct3236->led_counts * sizeof(struct group_rgb),
			     GFP_KERNEL);
	if (!bct3236->io)
		return -ENOMEM;

	bct3236->led_colors = devm_kzalloc(
		&i2c->dev, bct3236->led_counts * sizeof(u32), GFP_KERNEL);
	if (!bct3236->led_colors)
		return -ENOMEM;

	ret = bct3236_parse_led_dt(bct3236, np);
	if (ret < 0) {
		pr_err("%s: error bct3236 parse dts fail!\n", __func__);
		return ret;
	}

	ret = bct3236_init(bct3236);
	if (ret < 0) {
		pr_err("%s: bct3236 init failed\n", __func__);
		return -EIO;
	}

	i2c_set_clientdata(i2c, bct3236);
	dev_set_drvdata(&i2c->dev, bct3236);
	bct3236->cdev.name = LEDS_CDEV_NAME;
	bct3236->cdev.brightness = bct3236->brightness;
	bct3236->cdev.blink_brightness = bct3236->brightness;
	bct3236->cdev.max_brightness = BCT3236_BRI_IOUT_MAX;
	bct3236->cdev.brightness_get = bct3236_brightness_get;
	bct3236->cdev.brightness_set_blocking = bct3236_brightness_set;

	ret = led_classdev_register(bct3236->dev, &bct3236->cdev);
	if (ret) {
		pr_err("%s: unable to register led! ret = %d\n", __func__, ret);
		return ret;
	}

	/* fix brightness which is set to max with default-on trigger */
	if (bct3236->cdev.brightness != bct3236->brightness) {
		bct3236->cdev.brightness = bct3236->brightness;
		bct3236_brightness_set(&bct3236->cdev, bct3236->brightness);
	}

	ret = sysfs_create_group(&bct3236->cdev.dev->kobj,
				 &bct3236_attribute_group);
	if (ret) {
		pr_err("%s: unable to create led sysfs! ret = %d\n", __func__,
		       ret);
		led_classdev_unregister(&bct3236->cdev);
		return ret;
	}

	return 0;
}

static int bct3236_i2c_remove(struct i2c_client *i2c)
{
	struct bct3236 *bct3236 = i2c_get_clientdata(i2c);

	pr_info("%s: enter\n", __func__);
	sysfs_remove_group(&bct3236->cdev.dev->kobj, &bct3236_attribute_group);
	led_classdev_unregister(&bct3236->cdev);
	return 0;
}

static int bct3236_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct bct3236 *bct3236 = i2c_get_clientdata(i2c);
	int i;

	if (!bct3236) {
		pr_err("%s: bct3236 not set\n", __func__);
		return -ENXIO;
	}

	if (bct3236->ignore_led_suspend)
		return 0;

	mutex_lock(&bct3236_lock);

	for (i = 0; i < bct3236->led_counts; i++)
		bct3236->led_colors[i] = bct3236->edge_color_suspend;

	bct3236_set_colors(bct3236);
	mutex_unlock(&bct3236_lock);
	return 0;
}

static int bct3236_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct bct3236 *bct3236 = i2c_get_clientdata(i2c);
	int i;

	if (!bct3236) {
		pr_err("%s: bct3236 not set\n", __func__);
		return -ENXIO;
	}

	if (bct3236->ignore_led_suspend)
		return 0;

	mutex_lock(&bct3236_lock);

	for (i = 0; i < bct3236->led_counts; i++)
		bct3236->led_colors[i] = bct3236->edge_color_on;

	bct3236_set_colors(bct3236);
	mutex_unlock(&bct3236_lock);
	return 0;
}

static void bct3236_i2c_shutdown(struct i2c_client *i2c)
{
	struct bct3236 *bct3236 = i2c_get_clientdata(i2c);
	int i;

	if (!bct3236) {
		pr_err("%s: bct3236 not set\n", __func__);
		return;
	}

	mutex_lock(&bct3236_lock);

	for (i = 0; i < bct3236->led_counts; i++)
		bct3236->led_colors[i] = bct3236->edge_color_off;

	bct3236_set_colors(bct3236);
	mutex_unlock(&bct3236_lock);
}

static const struct i2c_device_id bct3236_i2c_id[] = { { BCT3236_I2C_NAME, 0 },
						       {} };

MODULE_DEVICE_TABLE(i2c, bct3236_i2c_id);

static const struct of_device_id bct3236_dt_match[] = {
	{ .compatible = "amlogic,bct3236_led" },
	{}
};

static SIMPLE_DEV_PM_OPS(bct3236_pm, bct3236_suspend, bct3236_resume);

static struct i2c_driver bct3236_driver = {
	.driver = {
		.name = BCT3236_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(bct3236_dt_match),
		.pm = &bct3236_pm,
	},
	.probe = bct3236_i2c_probe,
	.remove = bct3236_i2c_remove,
	.shutdown = bct3236_i2c_shutdown,
	.id_table = bct3236_i2c_id,
};

module_i2c_driver(bct3236_driver);
MODULE_AUTHOR("Bichao Zheng <bichao.zheng@amlogic.com>, Peter V.");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("BCT3236 LED Driver");
