/*
 * drivers/amlogic/media/vin/adc/adc.h
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

#ifndef __ADC_H_
#define __ADC_H_

#define TVDIN_ADC_VER "2021/09/01 txlx/tl1 bringup"

#define HIU_WR aml_write_hiubus

#define ADC_CLK_24M       24000
#define ADC_CLK_25M       25000

/* afe regisgers */
#define AFE_VAFE_CTRL0		(0x3B0)
#define AFE_VAFE_CTRL1		(0x3B1)
#define AFE_VAFE_CTRL2		(0x3B2)

#define HHI_CADC_CNTL           0x20
#define HHI_CADC_CNTL2          0x21

/* HIU registers */
#define HHI_DADC_CNTL		0x27
#define HHI_DADC_CNTL2		0x28
#define HHI_DADC_CNTL3		0x2a
#define HHI_DADC_CNTL4		0x2b
#define HHI_S2_DADC_CNTL	0x41
#define HHI_S2_DADC_CNTL2	0x42

#define HHI_ADC_PLL_CNTL0_TL1	0xb0
#define HHI_ADC_PLL_CNTL1_TL1	0xb1
#define HHI_ADC_PLL_CNTL2_TL1	0xb2
#define HHI_ADC_PLL_CNTL3_TL1	0xb3
#define HHI_ADC_PLL_CNTL4_TL1	0xb4
#define HHI_ADC_PLL_CNTL5_TL1	0xb5
#define HHI_ADC_PLL_CNTL6_TL1	0xb6
#define HHI_VDAC_CNTL0_T5	0xbb
#define HHI_VDAC_CNTL1_T5	0xbc

#define HHI_ADC_PLL_CNTL3	0xac
#define HHI_ADC_PLL_CNTL	0xaa
#define HHI_ADC_PLL_CNTL1	0xaf
#define HHI_ADC_PLL_CNTL2	0xab
#define HHI_ADC_PLL_CNTL4	0xad
#define HHI_ADC_PLL_CNTL5	0x9e
#define HHI_ADC_PLL_CNTL6	0x9f

#define HHI_DEMOD_CLK_CNTL	0x74
#define HHI_DEMOD_CLK_CNTL1	0x75

#define RESET1_REGISTER		0x1102

enum adc_chip_ver {
	ADC_CHIP_GXL = 0,
	ADC_CHIP_GXM,
	ADC_CHIP_TXL,
	ADC_CHIP_TXLX,
	ADC_CHIP_GXLX,
	ADC_CHIP_TXHD,
	ADC_CHIP_G12A,
	ADC_CHIP_G12B,
	ADC_CHIP_SM1,
	ADC_CHIP_TL1,
	ADC_CHIP_TM2,
	ADC_CHIP_T5D,
};

struct adc_platform_data_s {
	enum adc_chip_ver chip_id;
};

struct tvin_adc_dev {
	struct device_node *node;
	struct device *dev;
	/*struct platform_device *pdev;*/
	dev_t dev_no;
	const struct of_device_id *of_id;
	/*struct device *cdev;*/

	struct adc_platform_data_s *plat_data;
	struct mutex ioctl_mutex;/* aviod re-entry of ioctl calling */
	struct mutex pll_mutex; /* protect pll setting for multi modules */
	struct mutex filter_mutex;
	unsigned int pll_flg;
	unsigned int filter_flg;
	unsigned int print_en;
	unsigned int afe_phy_addr;
	unsigned int afe_phy_size;
	void __iomem *afe_vir_addr;
};

int dd_tvafe_hiu_reg_write(unsigned int reg, unsigned int val);
unsigned int dd_tvafe_hiu_reg_read(unsigned int addr);

#endif

