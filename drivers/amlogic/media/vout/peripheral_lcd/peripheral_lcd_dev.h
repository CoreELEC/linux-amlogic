/*
 * drivers/amlogic/media/vout/peripheral_lcd/peripheral_lcd_dev.h
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

#ifndef __LDIM_DEV_DRV_H
#define __LDIM_DEV_DRV_H
#include <linux/amlogic/media/vout/peripheral_lcd.h>

extern unsigned char peripheral_lcd_debug_print;
extern unsigned long long test_t[3];

/* peripheral lcd global api */
void peripheral_lcd_gpio_probe(unsigned int index);
void peripheral_lcd_gpio_set(int index, int value);
int peripheral_lcd_gpio_get(int index);

extern unsigned int rgb888_color_data[8];
extern unsigned int rgb666_color_data[8];
extern unsigned int rgb565_color_data[8];

/* peripheral lcd global api */
void peripheral_lcd_gpio_set(int index, int value);
int peripheral_lcd_gpio_get(int index);
int peripheral_lcd_gpio_set_irq(int index);

/* peripheral lcd intel_8080 dev api */
int peripheral_lcd_dev_intel_8080_probe(struct peripheral_lcd_driver_s
					       *peripheral_lcd_drv);
int peripheral_lcd_dev_intel_8080_remove(struct peripheral_lcd_driver_s
						*peripheral_lcd_drv);
/* peripheral lcd spi interface api */
int peripheral_lcd_dev_spi_probe(struct peripheral_lcd_driver_s
				 *peripheral_lcd_drv);
int peripheral_lcd_dev_spi_remove(struct peripheral_lcd_driver_s
				  *peripheral_lcd_drv);
/* peripheral lcd spi device api */
int peripheral_lcd_spi_driver_add(struct peripheral_lcd_driver_s
				  *peripheral_lcd_drv);
int peripheral_lcd_spi_driver_remove(struct peripheral_lcd_driver_s
				     *peripheral_lcd_drv);
int per_lcd_spi_write(struct spi_device *spi, unsigned char *tbuf, int tlen,
		      int word_bits);

int peripheral_lcd_dev_probe(struct platform_device *pdev);
int peripheral_lcd_dev_remove(struct platform_device *pdev);

#endif /* __PER_LCD_DEV_DRV_H */
