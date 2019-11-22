/*
 * drivers/amlogic/media/vout/lcd/lcd_extern/lcd_extern.h
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

#ifndef _LCD_EXTERN_H_
#define _LCD_EXTERN_H_
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

#define EXTPR(fmt, args...)     pr_info("lcd extern: "fmt"", ## args)
#define EXTERR(fmt, args...)    pr_info("lcd extern: error: "fmt"", ## args)

#define LCD_EXTERN_DRIVER		"lcd_extern"

#ifdef CONFIG_USE_OF
extern struct device_node *aml_lcd_extern_get_dts_child(int index);
#endif
extern void lcd_extern_gpio_probe(unsigned char index);
extern void lcd_extern_gpio_set(unsigned char index, int value);
extern unsigned int lcd_extern_gpio_get(unsigned char index);
extern void lcd_extern_pinmux_set(int status);

/* common API */
extern struct aml_lcd_extern_i2c_dev_s *lcd_extern_get_i2c_device(
		unsigned char addr);
extern int lcd_extern_i2c_write(struct i2c_client *i2client,
		unsigned char *buff, unsigned int len);
extern int lcd_extern_i2c_read(struct i2c_client *i2client,
		unsigned char *buff, unsigned int len);


/* specific API */
extern int aml_lcd_extern_default_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
extern int aml_lcd_extern_mipi_default_probe(
	struct aml_lcd_extern_driver_s *ext_drv);

#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_T5800Q
extern int aml_lcd_extern_i2c_T5800Q_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_ANX6862_7911
extern int aml_lcd_extern_i2c_ANX6862_7911_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_DLPC3439
extern int aml_lcd_extern_i2c_DLPC3439_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_TC101
extern int aml_lcd_extern_i2c_tc101_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_ANX6345
extern int aml_lcd_extern_i2c_anx6345_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_CS602
extern int aml_lcd_extern_i2c_CS602_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_SPI_LD070WS2
extern int aml_lcd_extern_spi_LD070WS2_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_N070ICN
extern int aml_lcd_extern_mipi_N070ICN_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_KD080D13
extern int aml_lcd_extern_mipi_KD080D13_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_TV070WSM
extern int aml_lcd_extern_mipi_TV070WSM_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_ST7701
extern int aml_lcd_extern_mipi_st7701_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_P070ACB
extern int aml_lcd_extern_mipi_p070acb_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_TL050FHV02CT
extern int aml_lcd_extern_mipi_tl050fhv02ct_probe(
	struct aml_lcd_extern_driver_s *ext_drv);
#endif

#endif

