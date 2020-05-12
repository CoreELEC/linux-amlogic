/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/pm_domain.h>

#define PWR_ON    1
#define PWR_OFF   0
#define DOMAIN_INIT_ON    0
#define DOMAIN_INIT_OFF   1
#define POWER_CTRL_IRQ_SET     0x82000094

enum pm_e {
	PM_DSP = 7,
	PM_DOS_HCODEC,
	PM_DOS_HEVC,
	PM_DOS_VDEC,
	PM_DOS_WAVE,
	PM_VPU_HDMI,
	PM_USB_COMB = 14,
	PM_PCIE,
	PM_GE2D,
	PM_ETH = 23
};

unsigned long pwr_ctrl_psci_smc(enum pm_e power_domain, bool power_control);
unsigned long pwr_ctrl_status_psci_smc(enum pm_e power_domain);

/*
 *irq:irq number for wakeup pwrctrl
 *irq_mask:irq mask,1:enable,0:mask
 *irq_invert:1:invert irq level,0:do not
 */
unsigned long pwr_ctrl_irq_set(u64 irq, u64 irq_mask, u64 irq_invert);

void pd_dev_create_file(struct device *dev, int cnt_start, int cnt_end,
			struct generic_pm_domain **domains);
void pd_dev_remove_file(struct device *dev);

