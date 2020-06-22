// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
//#define DEBUG
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#include <linux/amlogic/pwr_ctrl.h>
//#include <dt-bindings/power/amlogic,pd.h>
#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"
#include <linux/amlogic/scpi_protocol.h>
#include <linux/arm-smccc.h>

/*DSP TOP*/
#define REG_DSP_CFG0			(0x0)

static inline void soc_dsp_top_reg_dump(char *name,
					void __iomem *reg_base,
					u32 reg_offset)
{
	pr_info("%s (%lx) = 0x%x\n", name,
		(unsigned long)(reg_base + reg_offset),
		readl(reg_base + reg_offset));
}

void soc_dsp_regs_iounmem(void)
{
	iounmap(g_regbases.dspa_addr);
	iounmap(g_regbases.dspb_addr);

	pr_debug("%s done\n", __func__);
}

static inline void __iomem *get_dsp_addr(int dsp_id)
{
	if (dsp_id == 1)
		return g_regbases.dspb_addr;
	else
		return g_regbases.dspa_addr;
}

unsigned long init_dsp_psci_smc(u32 id, u32 addr, u32 cfg0)
{
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000090, id, addr, cfg0,
		      0, 0, 0, 0, &res);
	return res.a0;
}

static void start_dsp(u32 dsp_id, u32 reset_addr)
{
	u32 StatVectorSel;
	u32 strobe = 1;
	u32 tmp;
	u32 read;
	void __iomem *reg;

	reg = get_dsp_addr(dsp_id);
	StatVectorSel = (reset_addr != 0xfffa0000);

	tmp = 0x1 |  StatVectorSel << 1 | strobe << 2;
	//scpi_init_dsp_cfg0(dsp_id, reset_addr, tmp);
	init_dsp_psci_smc(dsp_id, reset_addr, tmp);
	read = readl(reg + REG_DSP_CFG0);

	if (dsp_id == 0) {
		read  = (read & ~(0xffff << 0)) | (0x2018 << 0) | (1 << 29);
		read = read & ~(1 << 31);         //dreset assert
		read = read & ~(1 << 30);     //Breset
	} else {
		read  = (read & ~(0xffff << 0)) | (0x2019 << 0) | (1 << 29);
		read = read & ~(1 << 31);         //dreset assert
		read = read & ~(1 << 30);     //Breset
	}
	writel(read, reg + REG_DSP_CFG0);
	soc_dsp_top_reg_dump("REG_DSP_CFG0", reg, REG_DSP_CFG0);

	pr_debug("%s\n", __func__);
}

unsigned long pwr_dsp_psci_smc(int dsp_id, bool power_control)
{
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000092, dsp_id, power_control, 0,
		      0, 0, 0, 0, &res);
	return res.a0;
}

static void soc_dsp_power_switch(int dsp_id, int pwr_cntl)
{
	if (dspcount == 1) {
		pr_debug("power on/off dsp by smc...\n");
		pwr_ctrl_psci_smc(0, pwr_cntl);
	} else {
		pwr_dsp_psci_smc(dsp_id, pwr_cntl);
	}
}

void soc_dsp_start(int dsp_id, int reset_addr)
{
	start_dsp(dsp_id, reset_addr);
}

void soc_dsp_bootup(int dsp_id, u32 reset_addr, int freq_sel)
{
	pr_debug("%s dsp_id=%d, address=0x%x\n",
		 __func__, dsp_id, reset_addr);

	soc_dsp_power_switch(dsp_id, PWR_ON);
	soc_dsp_start(dsp_id, reset_addr);
}

void soc_dsp_poweroff(int dsp_id)
{
	soc_dsp_power_switch(dsp_id,  PWR_OFF);
}

