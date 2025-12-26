// SPDX-License-Identifier: BSD/GPL-2.0
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright (c) 2024 Shanghai Awinic Technology Co., Ltd. All Rights Reserved
 *
 * GPL LICENSE SUMMARY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
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
#include "leds_aw210xx.h"
#include "leds_aw210xx_reg.h"

/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW210XX_DRIVER_VERSION "V1.0.0"
#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 1
#define AW_READ_CHIPID_RETRIES 2
#define AW_READ_CHIPID_RETRY_DELAY 1
#define AW210XX_CFG_NAME_MAX	64

/******************************************************
 *
 * aw210xx led parameter
 *
 ******************************************************/
struct aw210xx_cfg aw210xx_cfg_array[] = {
	{aw210xx_group_cfg_led_off, sizeof(aw210xx_group_cfg_led_off)},
	{aw21018_group_all_leds_on, sizeof(aw21018_group_all_leds_on)},
	{aw21018_group_red_leds_on, sizeof(aw21018_group_red_leds_on)},
	{aw21018_group_green_leds_on, sizeof(aw21018_group_green_leds_on)},
	{aw21018_group_blue_leds_on, sizeof(aw21018_group_blue_leds_on)},
	{aw21018_group_breath_leds_on, sizeof(aw21018_group_breath_leds_on)},
	{aw21012_group_all_leds_on, sizeof(aw21012_group_all_leds_on)},
	{aw21012_group_red_leds_on, sizeof(aw21012_group_red_leds_on)},
	{aw21012_group_green_leds_on, sizeof(aw21012_group_green_leds_on)},
	{aw21012_group_blue_leds_on, sizeof(aw21012_group_blue_leds_on)},
	{aw21012_group_breath_leds_on, sizeof(aw21012_group_breath_leds_on)},
	{aw21009_group_all_leds_on, sizeof(aw21009_group_all_leds_on)},
	{aw21009_group_red_leds_on, sizeof(aw21009_group_red_leds_on)},
	{aw21009_group_green_leds_on, sizeof(aw21009_group_green_leds_on)},
	{aw21009_group_blue_leds_on, sizeof(aw21009_group_blue_leds_on)},
	{aw21009_group_breath_leds_on, sizeof(aw21009_group_breath_leds_on)}
};
static char aw210xx_cfg_name[][AW210XX_CFG_NAME_MAX] = {
	{"aw210xx_group_cfg_led_off"},
	{"aw21018_group_all_leds_on"},
	{"aw21018_group_red_leds_on"},
	{"aw21018_group_green_leds_on"},
	{"aw21018_group_blue_leds_on"},
	{"aw21018_group_breath_leds_on"},
	{"aw21012_group_all_leds_on"},
	{"aw21012_group_red_leds_on"},
	{"aw21012_group_green_leds_on"},
	{"aw21012_group_blue_leds_on"},
	{"aw21012_group_breath_leds_on"},
	{"aw21009_group_all_leds_on"},
	{"aw21009_group_red_leds_on"},
	{"aw21009_group_green_leds_on"},
	{"aw21009_group_blue_leds_on"},
	{"aw21009_group_breath_leds_on"},
};

/******************************************************
 *
 * aw210xx i2c write/read
 *
 ******************************************************/
static int
aw210xx_i2c_write(struct aw210xx *aw210xx, unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(aw210xx->i2c, reg_addr, reg_data);
		if (ret < 0)
			AW_ERR("i2c_write cnt=%d ret=%d\n", cnt, ret);
		else
			break;
		cnt++;
		usleep_range(1000, 2000);
	}

	return ret;
}

static int
aw210xx_i2c_read(struct aw210xx *aw210xx, unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw210xx->i2c, reg_addr);
		if (ret < 0) {
			AW_ERR("i2c_read cnt=%d ret=%d\n", cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		usleep_range(1000, 2000);
	}

	return ret;
}

static int aw210xx_i2c_write_bits(struct aw210xx *aw210xx,
		unsigned char reg_addr, unsigned int mask,
		unsigned char reg_data)
{
	unsigned char reg_val;

	aw210xx_i2c_read(aw210xx, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= (reg_data & (~mask));
	aw210xx_i2c_write(aw210xx, reg_addr, reg_val);

	return 0;
}

/*****************************************************
 * led Interface: set effect
 *****************************************************/
static void
aw210xx_update_cfg_array(struct aw210xx *aw210xx, uint8_t *p_cfg_data, uint32_t cfg_size)
{
	unsigned int i = 0;

	for (i = 0; i < cfg_size; i += 2)
		aw210xx_i2c_write(aw210xx, p_cfg_data[i], p_cfg_data[i + 1]);
}

void aw210xx_cfg_update(struct aw210xx *aw210xx)
{
	AW_LOG("aw210xx->effect = %d", aw210xx->effect);

	aw210xx_update_cfg_array(aw210xx,
			aw210xx_cfg_array[aw210xx->effect].p,
			aw210xx_cfg_array[aw210xx->effect].count);
}

void aw210xx_uvlo_set(struct aw210xx *aw210xx, bool flag)
{
	if (flag) {
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_UVCR,
				AW210XX_BIT_UVPD_MASK,
				AW210XX_BIT_UVPD_DISENA);
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_UVCR,
				AW210XX_BIT_UVDIS_MASK,
				AW210XX_BIT_UVDIS_DISENA);
	} else {
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_UVCR,
				AW210XX_BIT_UVPD_MASK,
				AW210XX_BIT_UVPD_ENABLE);
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_UVCR,
				AW210XX_BIT_UVDIS_MASK,
				AW210XX_BIT_UVDIS_ENABLE);
	}
}

void aw210xx_sbmd_set(struct aw210xx *aw210xx, bool flag)
{
	if (flag) {
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR2,
				AW210XX_BIT_SBMD_MASK,
				AW210XX_BIT_SBMD_ENABLE);
		aw210xx->sdmd_flag = 1;
	} else {
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR2,
				AW210XX_BIT_SBMD_MASK,
				AW210XX_BIT_SBMD_DISENA);
		aw210xx->sdmd_flag = 0;
	}
}

void aw210xx_rgbmd_set(struct aw210xx *aw210xx, bool flag)
{
	if (flag) {
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR2,
				AW210XX_BIT_RGBMD_MASK,
				AW210XX_BIT_RGBMD_ENABLE);
		aw210xx->rgbmd_flag = 1;
	} else {
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR2,
				AW210XX_BIT_RGBMD_MASK,
				AW210XX_BIT_RGBMD_DISENA);
		aw210xx->rgbmd_flag = 0;
	}
}

void aw210xx_apse_set(struct aw210xx *aw210xx, bool flag)
{
	if (flag) {
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_APSE_MASK,
				AW210XX_BIT_APSE_ENABLE);
	} else {
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_APSE_MASK,
				AW210XX_BIT_APSE_DISENA);
	}
}

/*****************************************************
 * aw210xx led function set
 *****************************************************/
int32_t aw210xx_osc_pwm_set(struct aw210xx *aw210xx)
{
	switch (aw210xx->osc_clk) {
	case CLK_FRQ_16M:
		AW_LOG("osc is 16MHz!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_CLKFRQ_MASK,
				AW210XX_BIT_CLKFRQ_16MHz);
		break;
	case CLK_FRQ_8M:
		AW_LOG("osc is 8MHz!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_CLKFRQ_MASK,
				AW210XX_BIT_CLKFRQ_8MHz);
		break;
	case CLK_FRQ_1M:
		AW_LOG("osc is 1MHz!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_CLKFRQ_MASK,
				AW210XX_BIT_CLKFRQ_1MHz);
		break;
	case CLK_FRQ_512k:
		AW_LOG("osc is 512KHz!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_CLKFRQ_MASK,
				AW210XX_BIT_CLKFRQ_512kHz);
		break;
	case CLK_FRQ_256k:
		AW_LOG("osc is 256KHz!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_CLKFRQ_MASK,
				AW210XX_BIT_CLKFRQ_256kHz);
		break;
	case CLK_FRQ_125K:
		AW_LOG("osc is 125KHz!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_CLKFRQ_MASK,
				AW210XX_BIT_CLKFRQ_125kHz);
		break;
	case CLK_FRQ_62_5K:
		AW_LOG("osc is 62.5KHz!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_CLKFRQ_MASK,
				AW210XX_BIT_CLKFRQ_62_5kHz);
		break;
	case CLK_FRQ_31_25K:
		AW_LOG("osc is 31.25KHz!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_CLKFRQ_MASK,
				AW210XX_BIT_CLKFRQ_31_25kHz);
		break;
	default:
		AW_LOG("this clk_pwm is unsupported!\n");
		return -AW210XX_CLK_MODE_UNSUPPORT;
	}

	return 0;
}

int32_t aw210xx_br_res_set(struct aw210xx *aw210xx)
{
	switch (aw210xx->br_res) {
	case BR_RESOLUTION_8BIT:
		AW_LOG("br resolution select 8bit!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_PWMRES_MASK,
				AW210XX_BIT_PWMRES_8BIT);
		break;
	case BR_RESOLUTION_9BIT:
		AW_LOG("br resolution select 9bit!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_PWMRES_MASK,
				AW210XX_BIT_PWMRES_9BIT);
		break;
	case BR_RESOLUTION_12BIT:
		AW_LOG("br resolution select 12bit!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_PWMRES_MASK,
				AW210XX_BIT_PWMRES_12BIT);
		break;
	case BR_RESOLUTION_9_AND_3_BIT:
		AW_LOG("br resolution select 9+3bit!\n");
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_PWMRES_MASK,
				AW210XX_BIT_PWMRES_9_AND_3_BIT);
		break;
	default:
		AW_LOG("this br_res is unsupported!\n");
		return -AW210XX_CLK_MODE_UNSUPPORT;
	}

	return 0;
}

/*****************************************************
 * aw210xx debug interface set
 *****************************************************/
static void aw210xx_update(struct aw210xx *aw210xx)
{
	aw210xx_i2c_write(aw210xx, AW210XX_REG_UPDATE, AW210XX_UPDATE_BR_SL);
}

void aw210xx_global_set(struct aw210xx *aw210xx)
{
	aw210xx_i2c_write(aw210xx, AW210XX_REG_GCCR, aw210xx->glo_current);
}
void aw210xx_current_set(struct aw210xx *aw210xx)
{
	aw210xx_i2c_write(aw210xx, AW210XX_REG_GCCR, aw210xx->set_current);
}

/*****************************************************
 *
 * aw210xx led cfg
 *
 *****************************************************/
static void aw210xx_brightness_work(struct work_struct *work)
{
	struct aw210xx *aw210xx = container_of(work, struct aw210xx, brightness_work);

	if (aw210xx->cdev.brightness > aw210xx->cdev.max_brightness)
		aw210xx->cdev.brightness = aw210xx->cdev.max_brightness;

	aw210xx_i2c_write(aw210xx, AW210XX_REG_GCCR, aw210xx->cdev.brightness);
}

static void aw210xx_set_brightness(struct led_classdev *cdev, enum led_brightness brightness)
{
	struct aw210xx *aw210xx = container_of(cdev, struct aw210xx, cdev);

	aw210xx->cdev.brightness = brightness;

	schedule_work(&aw210xx->brightness_work);
}

/*****************************************************
 * aw210xx basic function set
 *****************************************************/
void aw210xx_chipen_set(struct aw210xx *aw210xx, bool flag)
{
	if (flag) {
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_CHIPEN_MASK,
				AW210XX_BIT_CHIPEN_ENABLE);
	} else {
		aw210xx_i2c_write_bits(aw210xx,
				AW210XX_REG_GCR,
				AW210XX_BIT_CHIPEN_MASK,
				AW210XX_BIT_CHIPEN_DISENA);
	}
}

static int aw210xx_hw_enable(struct aw210xx *aw210xx, bool flag)
{
	if (aw210xx && gpio_is_valid(aw210xx->enable_gpio)) {
		if (flag) {
			gpio_set_value_cansleep(aw210xx->enable_gpio, 1);
			usleep_range(2000, 2500);
		} else {
			gpio_set_value_cansleep(aw210xx->enable_gpio, 0);
		}
	} else {
		AW_ERR("failed\n");
	}

	return 0;
}

static int aw210xx_led_init(struct aw210xx *aw210xx)
{
	aw210xx->sdmd_flag = 0;
	aw210xx->rgbmd_flag = 0;
	/* chip enable */
	aw210xx_chipen_set(aw210xx, true);
	/* sbmd enable */
	aw210xx_sbmd_set(aw210xx, true);
	/* rgbmd enable */
	aw210xx_rgbmd_set(aw210xx, true);
	/* clk_pwm selsect */
	aw210xx_osc_pwm_set(aw210xx);
	/* br_res select */
	aw210xx_br_res_set(aw210xx);
	/* global set */
	aw210xx_global_set(aw210xx);
	/* under voltage lock out */
	aw210xx_uvlo_set(aw210xx, true);
	/* apse enable */
	aw210xx_apse_set(aw210xx, true);

	return 0;
}

static int32_t aw210xx_group_gcfg_set(struct aw210xx *aw210xx, bool flag)
{
	if (flag) {
		switch (aw210xx->chipid) {
		case AW21018_CHIPID:
			aw210xx_i2c_write(aw210xx, AW210XX_REG_GCFG, AW21018_GROUP_ENABLE);
			return 0;
		case AW21012_CHIPID:
			aw210xx_i2c_write(aw210xx, AW210XX_REG_GCFG, AW21012_GROUP_ENABLE);
			return 0;
		case AW21009_CHIPID:
			aw210xx_i2c_write(aw210xx, AW210XX_REG_GCFG, AW21009_GROUP_ENABLE);
			return 0;
		default:
			AW_LOG("%s: chip is unsupported device!\n", __func__);
			return -AW210XX_CHIPID_FAILD;
		}
	} else {
		switch (aw210xx->chipid) {
		case AW21018_CHIPID:
			aw210xx_i2c_write(aw210xx, AW210XX_REG_GCFG, AW21018_GROUP_DISABLE);
			return 0;
		case AW21012_CHIPID:
			aw210xx_i2c_write(aw210xx, AW210XX_REG_GCFG, AW21012_GROUP_DISABLE);
			return 0;
		case AW21009_CHIPID:
			aw210xx_i2c_write(aw210xx, AW210XX_REG_GCFG, AW21009_GROUP_DISABLE);
			return 0;
		default:
			AW_LOG("%s: chip is unsupported device!\n", __func__);
			return -AW210XX_CHIPID_FAILD;
		}
	}
}

void
aw210xx_singleled_set(struct aw210xx *aw210xx, uint32_t rgb_reg, uint32_t rgb_sl, uint32_t rgb_br)
{
	/* chip enable */
	aw210xx_chipen_set(aw210xx, true);
	/* global set */
	aw210xx->set_current = rgb_br;
	aw210xx_current_set(aw210xx);
	/* group set disable */
	aw210xx_group_gcfg_set(aw210xx, false);

	aw210xx_sbmd_set(aw210xx, true);
	aw210xx_rgbmd_set(aw210xx, false);
	aw210xx_uvlo_set(aw210xx, true);

	/* set sl */
	aw210xx->rgbcolor = rgb_sl & 0xff;
	aw210xx_i2c_write(aw210xx, AW210XX_REG_SL00 + rgb_reg, aw210xx->rgbcolor);

	/* br set */
	aw210xx_i2c_write(aw210xx, AW210XX_REG_BR00L + rgb_reg, rgb_br);
	if (aw210xx->sdmd_flag == 0)
		aw210xx_i2c_write(aw210xx, AW210XX_REG_BR00H + rgb_reg, rgb_br);
	/* update */
	aw210xx_update(aw210xx);
}

/*****************************************************
 * open short detect
 *****************************************************/
void aw210xx_open_detect_cfg(struct aw210xx *aw210xx)
{
	/*enable open detect*/
	aw210xx_i2c_write(aw210xx, AW210XX_REG_OSDCR, AW210XX_OPEN_DETECT_EN);
	/*set DCPWM = 1*/
	aw210xx_i2c_write_bits(aw210xx, AW210XX_REG_SSCR,
							AW210XX_DCPWM_SET_MASK,
							AW210XX_DCPWM_SET);
	/*set Open threshold = 0.2v*/
	aw210xx_i2c_write_bits(aw210xx,
							AW210XX_REG_OSDCR,
							AW210XX_OPEN_THRESHOLD_SET_MASK,
							AW210XX_OPEN_THRESHOLD_SET);
}

void aw210xx_short_detect_cfg(struct aw210xx *aw210xx)
{
	/*enable short detect*/
	aw210xx_i2c_write(aw210xx, AW210XX_REG_OSDCR, AW210XX_SHORT_DETECT_EN);
	/*set DCPWM = 1*/
	aw210xx_i2c_write_bits(aw210xx, AW210XX_REG_SSCR,
							AW210XX_DCPWM_SET_MASK,
							AW210XX_DCPWM_SET);
	/*set Short threshold = 1v*/
	aw210xx_i2c_write_bits(aw210xx,
							AW210XX_REG_OSDCR,
							AW210XX_SHORT_THRESHOLD_SET_MASK,
							AW210XX_SHORT_THRESHOLD_SET);
}

void aw210xx_open_short_dis(struct aw210xx *aw210xx)
{
	aw210xx_i2c_write(aw210xx, AW210XX_REG_OSDCR, AW210XX_OPEN_SHORT_DIS);
	/*SET DCPWM = 0*/
	aw210xx_i2c_write_bits(aw210xx, AW210XX_REG_SSCR, AW210XX_DCPWM_SET_MASK,
							AW210XX_DCPWM_CLEAN);
}
void aw210xx_open_short_detect(struct aw210xx *aw210xx, int32_t detect_flg, u8 *reg_val)
{
	/*config for open shor detect*/
	if (detect_flg == AW210XX_OPEN_DETECT)
		aw210xx_open_detect_cfg(aw210xx);
	else if (detect_flg == AW210XX_SHORT_DETECT)
		aw210xx_short_detect_cfg(aw210xx);
	/*read detect result*/
	aw210xx_i2c_read(aw210xx, AW210XX_REG_OSST0, &reg_val[0]);
	aw210xx_i2c_read(aw210xx, AW210XX_REG_OSST1, &reg_val[1]);
	aw210xx_i2c_read(aw210xx, AW210XX_REG_OSST2, &reg_val[2]);
	/*close for open short detect*/
	aw210xx_open_short_dis(aw210xx);
}

/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t
reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw210xx *aw210xx = container_of(led_cdev, struct aw210xx, cdev);
	uint32_t databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (aw210xx_reg_access[(uint8_t)databuf[0]] & REG_WR_ACCESS)
			aw210xx_i2c_write(aw210xx, (uint8_t)databuf[0], (uint8_t)databuf[1]);
	}

	return len;
}

static ssize_t reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw210xx *aw210xx = container_of(led_cdev, struct aw210xx, cdev);
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;
	uint8_t br_max = 0;
	uint8_t sl_val = 0;

	aw210xx_i2c_read(aw210xx, AW210XX_REG_GCR, &reg_val);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"reg:0x%02x=0x%02x\n", AW210XX_REG_GCR, reg_val);
	switch (aw210xx->chipid) {
	case AW21018_CHIPID:
		br_max = AW210XX_REG_BR17H;
		sl_val = AW210XX_REG_SL17;
		break;
	case AW21012_CHIPID:
		br_max = AW210XX_REG_BR11H;
		sl_val = AW210XX_REG_SL11;
		break;
	case AW21009_CHIPID:
		br_max = AW210XX_REG_BR08H;
		sl_val = AW210XX_REG_SL08;
		break;
	default:
		AW_LOG("chip is unsupported device!\n");
		return len;
	}

	for (i = AW210XX_REG_BR00L; i <= br_max; i++) {
		if (!(aw210xx_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw210xx_i2c_read(aw210xx, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len, "reg:0x%02x=0x%02x\n", i, reg_val);
	}
	for (i = AW210XX_REG_SL00; i <= sl_val; i++) {
		if (!(aw210xx_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw210xx_i2c_read(aw210xx, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len, "reg:0x%02x=0x%02x\n", i, reg_val);
	}
	for (i = AW210XX_REG_GCCR; i <= AW210XX_REG_GCFG; i++) {
		if (!(aw210xx_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw210xx_i2c_read(aw210xx, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len, "reg:0x%02x=0x%02x\n", i, reg_val);
	}

	return len;
}

static ssize_t
hwen_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw210xx *aw210xx = container_of(led_cdev, struct aw210xx, cdev);
	int rc;
	unsigned int val = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val > 0)
		aw210xx_hw_enable(aw210xx, true);
	else
		aw210xx_hw_enable(aw210xx, false);

	return len;
}

static ssize_t
hwen_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw210xx *aw210xx = container_of(led_cdev, struct aw210xx, cdev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "hwen=%d\n",
			gpio_get_value(aw210xx->enable_gpio));
	return len;
}

static ssize_t
effect_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw210xx *aw210xx = container_of(led_cdev, struct aw210xx, cdev);
	ssize_t len = 0;
	unsigned int i;

	for (i = 0; i < (sizeof(aw210xx_cfg_array) / sizeof(struct aw210xx_cfg)); i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "effect[%x]: %s\n",
				i, aw210xx_cfg_name[i]);
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "current effect[%d]: %s\n",
			aw210xx->effect, aw210xx_cfg_name[aw210xx->effect]);
	return len;
}

static ssize_t
effect_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw210xx *aw210xx = container_of(led_cdev, struct aw210xx, cdev);
	int rc;
	unsigned int val = 0;

	rc = kstrtouint(buf, 10, &val);
	if (rc < 0)
		return rc;
	if ((val >= (sizeof(aw210xx_cfg_array) / sizeof(struct aw210xx_cfg))) || (val < 0)) {
		pr_err("%s, store effect num error.\n", __func__);
		return -EINVAL;
	}

	aw210xx->effect = val;

	aw210xx_cfg_update(aw210xx);

	return len;
}

static ssize_t
rgbcolor_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw210xx *aw210xx = container_of(led_cdev, struct aw210xx, cdev);
	uint32_t rgb_num = 0;
	uint32_t rgb_data = 0;

	if (sscanf(buf, "%x %x", &rgb_num, &rgb_data) == 2) {
		aw210xx_chipen_set(aw210xx, true);
		aw210xx_sbmd_set(aw210xx, true);
		aw210xx_rgbmd_set(aw210xx, true);
		aw210xx_global_set(aw210xx);
		aw210xx_uvlo_set(aw210xx, true);

		/* set sl */
		aw210xx->rgbcolor = (rgb_data & 0xff0000) >> 16;
		aw210xx_i2c_write(aw210xx,
				AW210XX_REG_SL00 + (uint8_t)rgb_num * 3,
				aw210xx->rgbcolor);

		aw210xx->rgbcolor = (rgb_data & 0x00ff00) >> 8;
		aw210xx_i2c_write(aw210xx,
				AW210XX_REG_SL00 + (uint8_t)rgb_num * 3 + 1,
				aw210xx->rgbcolor);

		aw210xx->rgbcolor = (rgb_data & 0x0000ff);
		aw210xx_i2c_write(aw210xx,
				AW210XX_REG_SL00 + (uint8_t)rgb_num * 3 + 2,
				aw210xx->rgbcolor);

		/* br set */
		aw210xx_i2c_write(aw210xx,
				AW210XX_REG_BR00L + (uint8_t)rgb_num,
				AW210XX_GLOBAL_DEFAULT_SET);

		/* update */
		aw210xx_update(aw210xx);
	}

	return len;
}

static ssize_t
singleled_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw210xx *aw210xx = container_of(led_cdev, struct aw210xx, cdev);
	uint32_t led_num = 0;
	uint32_t rgb_data = 0;
	uint32_t rgb_brightness = 0;

	if (sscanf(buf, "%x %x %x", &led_num, &rgb_data, &rgb_brightness) == 3) {
		if (aw210xx->chipid == AW21018_CHIPID) {
			if (led_num > AW21018_LED_NUM)
				led_num = AW21018_LED_NUM;
		} else if (aw210xx->chipid == AW21012_CHIPID) {
			if (led_num > AW21012_LED_NUM)
				led_num = AW21012_LED_NUM;
		} else {
			if (led_num > AW21009_LED_NUM)
				led_num = AW21009_LED_NUM;
		}
		aw210xx_singleled_set(aw210xx, led_num, rgb_data, rgb_brightness);
	}

	return len;
}

static ssize_t
opdetect_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw210xx *aw210xx = container_of(led_cdev, struct aw210xx, cdev);
	ssize_t len = 0;
	int i = 0;
	uint8_t reg_val[3] = {0};

	aw210xx_open_short_detect(aw210xx, AW210XX_OPEN_DETECT, reg_val);
	for (i = 0; i < sizeof(reg_val); i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "OSST%d:%#x\n", i, reg_val[i]);

	return len;
}

static ssize_t
stdetect_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw210xx *aw210xx = container_of(led_cdev, struct aw210xx, cdev);
	ssize_t len = 0;
	int i = 0;
	uint8_t reg_val[3] = {0};

	aw210xx_open_short_detect(aw210xx, AW210XX_SHORT_DETECT, reg_val);
	for (i = 0; i < sizeof(reg_val); i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "OSST%d:%#x\n", i, reg_val[i]);
	return len;
}

static DEVICE_ATTR_RW(reg);
static DEVICE_ATTR_RW(hwen);
static DEVICE_ATTR_RW(effect);
static DEVICE_ATTR_WO(rgbcolor);
static DEVICE_ATTR_WO(singleled);
static DEVICE_ATTR_RO(opdetect);
static DEVICE_ATTR_RO(stdetect);

static struct attribute *aw210xx_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_hwen.attr,
	&dev_attr_effect.attr,
	&dev_attr_rgbcolor.attr,
	&dev_attr_singleled.attr,
	&dev_attr_opdetect.attr,
	&dev_attr_stdetect.attr,
	NULL,
};

static struct attribute_group aw210xx_attribute_group = {
	.attrs = aw210xx_attributes
};
/******************************************************
 *
 * led class dev
 ******************************************************/

static int aw210xx_parse_led_cdev(struct aw210xx *aw210xx, struct device_node *np)
{
	int ret = -1;
	struct device_node *temp;

	for_each_child_of_node(np, temp) {
		ret = of_property_read_string(temp, "aw210xx,name", &aw210xx->cdev.name);
		if (ret < 0) {
			dev_err(aw210xx->dev, "Failure reading led name, ret = %d\n", ret);
			goto free_pdata;
		}
		ret = of_property_read_u32(temp, "aw210xx,imax", &aw210xx->imax);
		if (ret < 0) {
			dev_err(aw210xx->dev, "Failure reading imax, ret = %d\n", ret);
			goto free_pdata;
		}
		ret = of_property_read_u32(temp, "aw210xx,brightness", &aw210xx->cdev.brightness);
		if (ret < 0) {
			dev_err(aw210xx->dev, "Failure reading brightness, ret = %d\n", ret);
			goto free_pdata;
		}
		ret = of_property_read_u32(temp, "aw210xx,max_brightness",
				&aw210xx->cdev.max_brightness);
		if (ret < 0) {
			dev_err(aw210xx->dev, "Failure reading max brightness, ret = %d\n", ret);
			goto free_pdata;
		}
	}

	INIT_WORK(&aw210xx->brightness_work, aw210xx_brightness_work);
	aw210xx->cdev.brightness_set = aw210xx_set_brightness;

	ret = led_classdev_register(aw210xx->dev, &aw210xx->cdev);
	if (ret) {
		AW_ERR("unable to register led ret=%d\n", ret);
		goto free_pdata;
	}

	ret = sysfs_create_group(&aw210xx->cdev.dev->kobj, &aw210xx_attribute_group);
	if (ret) {
		AW_ERR("led sysfs ret: %d\n", ret);
		goto free_class;
	}

	aw210xx_led_init(aw210xx);

	return 0;

free_class:
	led_classdev_unregister(&aw210xx->cdev);
free_pdata:
	return ret;
}

/*****************************************************
 *
 * check chip id and version
 *
 *****************************************************/
static int aw210xx_read_chipid(struct aw210xx *aw210xx)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char chipid = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw210xx_i2c_read(aw210xx, AW210XX_REG_RESET, &chipid);
		if (ret < 0) {
			AW_ERR("failed to read chipid: %d\n", ret);
		} else {
			aw210xx->chipid = chipid;
			switch (aw210xx->chipid) {
			case AW21018_CHIPID:
				AW_LOG("AW21018, read chipid = 0x%02x!!\n", chipid);
				return 0;
			case AW21012_CHIPID:
				AW_LOG("AW21012, read chipid = 0x%02x!!\n", chipid);
				return 0;
			case AW21009_CHIPID:
				AW_LOG("AW21009, read chipid = 0x%02x!!\n", chipid);
				return 0;
			default:
				AW_LOG("chip is unsupported device id = %x\n", chipid);
				break;
			}
		}
		cnt++;
		usleep_range(1000, 2000);
	}

	return -EINVAL;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw210xx_parse_dt(struct device *dev, struct aw210xx *aw210xx, struct device_node *np)
{
	int ret = -EINVAL;

	aw210xx->enable_gpio = of_get_named_gpio(np, "enable-gpio", 0);

	ret = of_property_read_u32(np, "osc_clk", &aw210xx->osc_clk);
	if (ret < 0) {
		AW_ERR("no osc_clk provided, osc clk unsupported\n");
		return ret;
	}

	ret = of_property_read_u32(np, "br_res", &aw210xx->br_res);
	if (ret < 0) {
		AW_ERR("brightness resolution unsupported\n");
		return ret;
	}

	ret = of_property_read_u32(np, "global_current", &aw210xx->glo_current);
	if (ret < 0) {
		AW_ERR("global current resolution unsupported\n");
		return ret;
	}

	return 0;
}

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw210xx_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct aw210xx *aw210xx;
	struct device_node *np = i2c->dev.of_node;
	int ret;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		pr_err("check_functionality failed\n");
		return -EIO;
	}

	aw210xx = devm_kzalloc(&i2c->dev, sizeof(struct aw210xx), GFP_KERNEL);
	if (aw210xx == NULL)
		return -ENOMEM;

	aw210xx->dev = &i2c->dev;
	aw210xx->i2c = i2c;
	i2c_set_clientdata(i2c, aw210xx);

	/* aw210xx parse device tree */
	if (np) {
		ret = aw210xx_parse_dt(&i2c->dev, aw210xx, np);
		if (ret) {
			pr_err("failed to parse device tree node\n");
			goto err_parse_dt;
		}
	}

	if (gpio_is_valid(aw210xx->enable_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw210xx->enable_gpio,
				GPIOF_OUT_INIT_LOW, "aw210xx_en");
		if (ret) {
			pr_err("enable gpio request failed\n");
			goto err_gpio_request;
		}
	}

	/* hardware enable */
	aw210xx_hw_enable(aw210xx, true);

	/* aw210xx identify */
	ret = aw210xx_read_chipid(aw210xx);
	if (ret < 0) {
		pr_err("aw210xx_read_chipid failed ret=%d\n", ret);
		goto err_id;
	}

	dev_set_drvdata(&i2c->dev, aw210xx);
	aw210xx_parse_led_cdev(aw210xx, np);
	if (ret < 0) {
		pr_err("error creating led class dev\n");
		goto err_sysfs;
	}

	AW_LOG("probe completed!\n");

	return 0;

err_sysfs:
err_id:
	devm_gpio_free(&i2c->dev, aw210xx->enable_gpio);
err_gpio_request:
err_parse_dt:
	devm_kfree(&i2c->dev, aw210xx);
	aw210xx = NULL;
	return ret;
}

static int aw210xx_i2c_remove(struct i2c_client *i2c)
{
	struct aw210xx *aw210xx = i2c_get_clientdata(i2c);

	sysfs_remove_group(&aw210xx->cdev.dev->kobj, &aw210xx_attribute_group);
	led_classdev_unregister(&aw210xx->cdev);
	if (gpio_is_valid(aw210xx->enable_gpio))
		devm_gpio_free(&i2c->dev, aw210xx->enable_gpio);
	devm_kfree(&i2c->dev, aw210xx);
	aw210xx = NULL;

	return 0;
}

static const struct i2c_device_id aw210xx_i2c_id[] = {
	{AW210XX_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, aw210xx_i2c_id);

static const struct of_device_id aw210xx_dt_match[] = {
	{.compatible = "awinic,aw210xx_led"},
	{}
};

static struct i2c_driver aw210xx_i2c_driver = {
	.driver = {
		.name = AW210XX_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw210xx_dt_match),
		},
	.probe = aw210xx_i2c_probe,
	.remove = aw210xx_i2c_remove,
	.id_table = aw210xx_i2c_id,
};

static int __init aw210xx_i2c_init(void)
{
	int ret = 0;

	pr_info("enter, aw210xx driver version %s\n", AW210XX_DRIVER_VERSION);

	ret = i2c_add_driver(&aw210xx_i2c_driver);
	if (ret) {
		pr_err("failed to register aw210xx driver!\n");
		return ret;
	}

	return 0;
}
module_init(aw210xx_i2c_init);

static void __exit aw210xx_i2c_exit(void)
{
	i2c_del_driver(&aw210xx_i2c_driver);
}
module_exit(aw210xx_i2c_exit);

MODULE_DESCRIPTION("AW210XX LED Driver");
MODULE_LICENSE("GPL v2");
