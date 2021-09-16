/*
 * drivers/amlogic/media/vout/peripheral_lcd/spi.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/peripheral_lcd.h>
#include "peripheral_lcd_drv.h"
#include "peripheral_lcd_dev.h"

static struct per_lcd_dev_config_s *pconf;
struct spi_device *spi;
static int init_flag;

static inline void dcx_init(void)
{
	peripheral_lcd_gpio_set(pconf->dcx_index, 2);
}

static inline void dcx_high(void)
{
	peripheral_lcd_gpio_set(pconf->dcx_index, 1);
}

static inline void dcx_low(void)
{
	peripheral_lcd_gpio_set(pconf->dcx_index, 0);
}

static inline void rst_high(void)
{
	peripheral_lcd_gpio_set(pconf->reset_index, 1);
}

static inline void rst_low(void)
{
	peripheral_lcd_gpio_set(pconf->reset_index, 0);
}

static unsigned long long reverse_bytes(unsigned long long value)
{
	return (value & 0x00000000000000FFUL) << 48 |
	       (value & 0x000000000000FF00UL) << 48 |
	       (value & 0x0000000000FF0000UL) << 16 |
	       (value & 0x00000000FF000000UL) << 16 |
	       (value & 0x000000FF00000000UL) >> 16 |
	       (value & 0x0000FF0000000000UL) >> 16 |
	       (value & 0x00FF000000000000UL) >> 48 |
	       (value & 0xFF00000000000000UL) >> 48;
}

static int endian64bit_convert(void *src_data, int size)
{
	unsigned char *src = (unsigned char *)src_data;

	if (size % 8) {
		LCDPR("endian64bit_convert size error %d\n", size);
		return -1;
	}
	while (size > 0) {
		*(unsigned long long *)src =
			reverse_bytes(*(unsigned long long *)src);
		src += 8;
		size -= 8;
	}
	return 0;
}

static void write_comm(struct spi_device *spi, unsigned char command)
{
	dcx_low();
	peripheral_lcd_delay_us(1);
	per_lcd_spi_write(spi, &command, 1, 8);
}

static void write_data(struct spi_device *spi, unsigned char data)
{
	dcx_high();
	peripheral_lcd_delay_us(1);
	per_lcd_spi_write(spi, &data, 1, 8);
}

static void set_spi_display_window(struct spi_device *spi,
				   unsigned int x_start, unsigned int y_start,
				   unsigned int x_end, unsigned int y_end)
{
	write_comm(spi, 0x2A);
	write_data(spi, x_start >> 8);
	write_data(spi, x_start & 0xff);
	write_data(spi, x_end >> 8);
	write_data(spi, x_end & 0xff);

	write_comm(spi, 0x2B);
	write_data(spi, y_start >> 8);
	write_data(spi, y_start & 0xff);
	write_data(spi, y_end >> 8);
	write_data(spi, y_end & 0xff);

	write_comm(spi, 0x2c);
}

static void per_lcd_write_frame(unsigned char *addr, unsigned short x0,
				unsigned short x1, unsigned short y0,
				unsigned short y1)
{
	int ret, size;

	set_spi_display_window(spi, x0, y0, x1, y1);
	dcx_high();
	peripheral_lcd_delay_ms(1);
	size = (x1 - x0) * (y1 - y0) * 2;
	endian64bit_convert(addr, size);
	ret = per_lcd_spi_write(spi, addr, size, 64);
	if (ret)
		LCDERR("%s: fail\n", __func__);
}

static int frame_flush(void)
{
	return 0;
}

static int set_color_format(unsigned int cfmt)
{
	return 0;
}

static int set_gamma(unsigned char *table, unsigned int rgb_sel)
{
	return 0;
}

static int set_flush_rate(unsigned int rate)
{
	return 0;
}

static int frame_post(unsigned char *addr, unsigned short x0,
	       unsigned short x1, unsigned short y0, unsigned short y1)
{
	if (!addr) {
		LCDERR("buff is null\n");
		return -1;
	}

	per_lcd_write_frame(addr, x0, x1, y0, y1);
	return 0;
}

static int spi_power_cmd_dynamic_size(int flag)
{
	unsigned char *table;
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	if (flag) {
		table = pconf->init_on;
		max_len = pconf->init_on_cnt;
	} else {
		table = pconf->init_off;
		max_len = pconf->init_off_cnt;
	}

	while ((i + 1) < max_len) {
		type = table[i];
		if (type == PER_LCD_CMD_TYPE_END)
			break;
		if (peripheral_lcd_debug_print) {
			LCDPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
			      __func__, step, type, table[i + 1]);
		}

		cmd_size = table[i + 1];
		if (cmd_size == 0)
			goto power_cmd_dynamic_next;
		if ((i + 2 + cmd_size) > max_len)
			break;

		if (type == PER_LCD_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == PER_LCD_CMD_TYPE_GPIO) {
			if (cmd_size < 2) {
				LCDERR(
				     "step %d: invalid cmd_size %d for GPIO\n",
				       step, cmd_size);
				goto power_cmd_dynamic_next;
			}
			if (table[i + 2] < LCD_GPIO_MAX) {
				peripheral_lcd_gpio_set(table[i + 2],
							table[i + 3]);
			}
			if (cmd_size > 2) {
				if (table[i + 4] > 0)
					peripheral_lcd_delay_ms(table[i + 4]);
			}
		} else if (type == PER_LCD_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += table[i + 2 + j];
			if (delay_ms > 0)
				peripheral_lcd_delay_ms(delay_ms);
		} else if (type == PER_LCD_CMD_TYPE_CMD) {
			per_lcd_spi_write(spi, &table[i + 2],
					  cmd_size, 8);
			peripheral_lcd_delay_us(1);
		} else if (type == PER_LCD_CMD_TYPE_CMD_DELAY) {
			per_lcd_spi_write(spi, &table[i + 2],
					  (cmd_size - 1), 8);
			peripheral_lcd_delay_us(1);
			if (table[i + cmd_size + 1] > 0)
				peripheral_lcd_delay_ms(table[i +
							cmd_size + 1]);
		} else {
			LCDERR("%s: type 0x%02x invalid\n", __func__, type);
		}
power_cmd_dynamic_next:
		i += (cmd_size + 2);
		step++;
	}

	return ret;
}

static int spi_power_cmd_fixed_size(int flag)
{
	unsigned char *table;
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	cmd_size = pconf->cmd_size;
	if (cmd_size < 2) {
		LCDERR("%s: invalid cmd_size %d\n", __func__, cmd_size);
		return -1;
	}

	if (flag) {
		table = pconf->init_on;
		max_len = pconf->init_on_cnt;
	} else {
		table = pconf->init_off;
		max_len = pconf->init_off_cnt;
	}

	while ((i + cmd_size) <= max_len) {
		type = table[i];
		if (type == PER_LCD_CMD_TYPE_END)
			break;
		if (peripheral_lcd_debug_print) {
			LCDPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
			      __func__, step, type, cmd_size);
		}

		if (type == PER_LCD_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == PER_LCD_CMD_TYPE_GPIO) {
			if (table[i + 1] < LCD_GPIO_MAX) {
				peripheral_lcd_gpio_set(table[i + 1],
							table[i + 2]);
			}
			if (cmd_size > 3) {
				if (table[i + 3] > 0)
					peripheral_lcd_delay_ms(table[i + 3]);
			}
		} else if (type == PER_LCD_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < (cmd_size - 1); j++)
				delay_ms += table[i + 1 + j];
			if (delay_ms > 0)
				peripheral_lcd_delay_ms(delay_ms);
		} else if (type == PER_LCD_CMD_TYPE_CMD) {
			per_lcd_spi_write(spi, &table[i + 1],
					  (cmd_size - 1), 8);
			peripheral_lcd_delay_us(1);
		} else if (type == PER_LCD_CMD_TYPE_CMD_DELAY) {
			per_lcd_spi_write(spi, &table[i + 1],
					  cmd_size, 8);
			if (table[i + cmd_size - 1] > 0)
				peripheral_lcd_delay_ms(table[i +
							cmd_size - 1]);
		} else {
			LCDERR("%s: type 0x%02x invalid\n", __func__, type);
		}
		i += cmd_size;
		step++;
	}

	return ret;
}

static irqreturn_t spi_vsync_isr(int irq, void *dev_id)
{
	struct peripheral_lcd_driver_s *peripheral_lcd_driver =
				peripheral_lcd_get_driver();

	if (peripheral_lcd_driver->vsync_isr_cb)
		peripheral_lcd_driver->vsync_isr_cb();

	return IRQ_HANDLED;
}

static int spi_vsync_irq_init(void)
{
	struct peripheral_lcd_driver_s *peripheral_lcd_drv =
		peripheral_lcd_get_driver();
	int ret;

	ret = peripheral_lcd_gpio_set_irq(pconf->irq_index);
	if (ret) {
		LCDERR("gpio_set_irq failed\n");
		return -1;
	}
	ret = request_irq(peripheral_lcd_drv->irq_num, spi_vsync_isr,
			  IRQF_TRIGGER_RISING, "per_lcd_spi_vsync_irq",
			  (void *)"per_lcd");
	if (ret) {
		LCDERR("can't request lcd_spi_vsync_irq\n");
	} else {
		if (lcd_debug_print_flag)
			LCDPR("request lcd_spi_vsync_irq successful\n");
	}
	return 0;
}

static void spi_fill_screen_color(unsigned int index)
{
	unsigned char *buf;
	unsigned long c = 0;
	unsigned int size;
	int i = 0;

	if (pconf->data_format == 0) {
		c = rgb888_color_data[index];
	} else if (pconf->data_format == 1) {
		c = rgb666_color_data[index];
	} else if (pconf->data_format == 2) {
		c = rgb565_color_data[i];
		size = pconf->row * pconf->col * 2;
		buf = kmalloc((sizeof(unsigned int) * size * 2), GFP_KERNEL);
		if (!buf) {
			LCDERR("%s: failed to alloc buf\n", __func__);
		return;
		}
		for (i = 0; i < (size * 2); i += 2) {
			buf[i] = c & 0xff;
			buf[i + 1] = (c >> 8) & 0xff;
		}
		per_lcd_write_frame(buf, 0, pconf->col, 0,
				    pconf->row);
		kfree(buf);
		buf = NULL;
	} else {
		LCDERR("unsupport data_format\n");
	}
}

static void spi_write_color_bars(void)
{
	unsigned int i, j, k = 0;
	unsigned char *buf;
	unsigned int size;

	if (!pconf->data_format) {
		size = pconf->row * pconf->col * 3;
		buf = kmalloc((sizeof(unsigned int) * size), GFP_KERNEL);
		if (!buf) {
			LCDERR("%s: failed to alloc buf\n", __func__);
			return;
		}
		for (i = 0; i < 8; i++) {
			for (j = 0; j < (size >> 3); j++) {
				buf[k] =  rgb565_color_data[i] & 0xff;
				buf[k + 1] = (rgb565_color_data[i] >> 8) & 0xff;
				buf[k + 2] = (rgb888_color_data[i] >> 16)
					& 0xff;
				k += 3;
			}
		}
	} else if (pconf->data_format == 1) {
		size = pconf->row * pconf->col * 3;
		buf = kmalloc((sizeof(unsigned int) * size), GFP_KERNEL);
		if (!buf) {
			LCDERR("%s: failed to alloc buf\n", __func__);
			return;
		}
		for (i = 0; i < 8; i++) {
			for (j = 0; j < (size >> 3); j++) {
				buf[k] =  rgb565_color_data[i] & 0xff;
				buf[k + 1] = (rgb565_color_data[i] >> 8) & 0xff;
				buf[k + 2] = (rgb888_color_data[i] >> 16) & 0x3;
				k += 3;
			}
		}
	} else if (pconf->data_format == 2) {
		size = pconf->row * pconf->col * 2;
		buf = kmalloc((sizeof(unsigned int) * size), GFP_KERNEL);
		if (!buf) {
			LCDERR("%s: failed to alloc buf\n", __func__);
			return;
		}
		for (i = 0; i < 8; i++) {
			for (j = 0; j < (size >> 3); j += 2) {
				buf[k] =  rgb565_color_data[i] & 0xff;
				buf[k + 1] = (rgb565_color_data[i] >> 8) & 0xff;
				k += 2;
			}
		}
	} else {
		LCDERR("unsupport data_format\n");
		return;
	}
	per_lcd_write_frame(buf, 0, pconf->col, 0, pconf->row);
	kfree(buf);
	buf = NULL;
}

static int spi_power_on_init(void)
{
	int ret = 0;

	if (pconf->cmd_size < 1) {
		LCDERR("%s: cmd_size %d is invalid\n",
		       __func__, pconf->cmd_size);
		return -1;
	}
	if (!pconf->init_on) {
		LCDERR("%s: init_data is null\n", __func__);
		return -1;
	}

	if (pconf->cmd_size == PER_LCD_CMD_SIZE_DYNAMIC)
		ret = spi_power_cmd_dynamic_size(1);
	else
		ret = spi_power_cmd_fixed_size(1);
	spi_vsync_irq_init();
	init_flag = 1;
	return ret;
}

static int spi_test(const char *buf)
{
	int ret;
	unsigned int index;

	switch (buf[0]) {
	case 'c': /*color fill*/
		if (!init_flag)
			spi_power_on_init();
		ret = sscanf(buf, "color %d", &index);
		spi_fill_screen_color(index);
		break;
	case 'b': /*colorbar*/
		if (!init_flag)
			spi_power_on_init();
		spi_write_color_bars();
		break;
	default:
		LCDPR("unsupport\n");
		break;
	}
	return 0;
}

int peripheral_lcd_dev_spi_probe(struct peripheral_lcd_driver_s
				 *peripheral_lcd_drv)
{
	int ret;

	if (!peripheral_lcd_drv->spi_dev) {
		LCDERR("spi_dev is null\n");
		return -1;
	}

	pconf = peripheral_lcd_drv->per_lcd_dev_conf;
	peripheral_lcd_drv->frame_post = frame_post;
	peripheral_lcd_drv->frame_flush = frame_flush;
	peripheral_lcd_drv->set_color_format = set_color_format;
	peripheral_lcd_drv->set_gamma = set_gamma;
	peripheral_lcd_drv->set_flush_rate = set_flush_rate;
	peripheral_lcd_drv->vsync_isr_cb = te_cb;
	peripheral_lcd_drv->enable = spi_power_on_init;
	peripheral_lcd_drv->test = spi_test;

	spi = peripheral_lcd_drv->spi_dev;

	ret = spi_power_on_init();
	if (!ret)
		spi_fill_screen_color(0);
	return 0;
}

int peripheral_lcd_dev_spi_remove(struct peripheral_lcd_driver_s
					 *peripheral_lcd_drv)
{
	return 0;
}
