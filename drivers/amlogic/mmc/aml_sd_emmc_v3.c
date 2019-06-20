/*
 * drivers/amlogic/mmc/aml_sd_emmc_v3.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/highmem.h>
#include <linux/amlogic/sd.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/mmc/emmc_partitions.h>
#include <linux/amlogic/amlsd.h>
#include <linux/amlogic/aml_sd_emmc_internal.h>

int aml_fixdiv_calc(unsigned int *fixdiv, struct clock_lay_t *clk)
{
	int ret = 0;
	unsigned int full_div = 0, source_cycle = 0;	/* in ns*/
	unsigned int sdclk_idx = 0, todly_idx_max = 0, todly_idx_min = 0;
	unsigned int inv_idx_min = 0, inv_idx_max = 0;
	unsigned int val_idx_min = 0, val_idx_max = 0;
	unsigned int val_idx_win = 0, val_idx_sta = 0;

	if (!fixdiv || !clk)
		return -EPERM;

	if (clk->core == clk->old_core)
		return 1;
	clk->old_core = clk->core;

	pr_debug(">>>%s source clk %d, core clk %d\n",
			__func__, clk->source, clk->core);
	full_div = clk->source / clk->core;
	sdclk_idx = full_div / 2;
	sdclk_idx -= full_div % 2 ? 0 : 1;

	pr_debug("full_div %d, sdclk_idx %d\n", full_div, sdclk_idx);
	/* ns */
	source_cycle = 1000000000 / clk->source;
	pr_debug("cycle of source clock is %d\n", source_cycle);
	if (source_cycle < TODLY_MAX_NS) {
		todly_idx_max
			= (TODLY_MAX_NS + source_cycle - 1) / source_cycle;
		todly_idx_min = TODLY_MIN_NS / source_cycle;
		pr_debug("todly_idx_min %d, todly_idx_max %d\n",
				todly_idx_min, todly_idx_max);
		inv_idx_min = todly_idx_min + sdclk_idx;
		inv_idx_max = todly_idx_max + sdclk_idx;
		pr_debug("inv_idx_min %d, inv_idx_max %d\n",
				inv_idx_min, inv_idx_max);
		inv_idx_min = inv_idx_min % full_div;
		inv_idx_max = inv_idx_max % full_div;
		pr_debug("ROUND: inv_idx_min %d, inv_idx_max %d\n",
				inv_idx_min, inv_idx_max);

		if (inv_idx_min > inv_idx_max) {
			val_idx_min = inv_idx_max + 1;
			val_idx_max = inv_idx_min - 1;
			val_idx_sta = val_idx_min;
			val_idx_win = val_idx_max - val_idx_min;
		} else if (inv_idx_min <= inv_idx_max) {
			val_idx_max = inv_idx_max + 1;
			val_idx_min = inv_idx_min - 1;
			val_idx_sta = val_idx_max;
			val_idx_win = (full_div + val_idx_min) -  val_idx_max;
		}
	} else {
		val_idx_max = sdclk_idx + 1;
		val_idx_min = sdclk_idx - 1;
		val_idx_sta = val_idx_max;
		val_idx_win = full_div / 2;
	}

	pr_debug("val_idx_sta %d, val_idx_win %d\n", val_idx_sta, val_idx_win);
	*fixdiv = val_idx_sta + (val_idx_win >> 1);

	pr_debug("fixdiv = %d\n", *fixdiv);
	return ret;
}

int meson_mmc_clk_init_v3(struct amlsd_host *host)
{
	int ret = 0;
	u32 vclkc = 0;
	struct sd_emmc_clock_v3 *pclkc = (struct sd_emmc_clock_v3 *)&vclkc;
	u32 vconf = 0;
	struct sd_emmc_config *pconf = (struct sd_emmc_config *)&vconf;
	struct mmc_phase *init = &(host->data->sdmmc.init);

	writel(0, host->base + SD_EMMC_ADJUST_V3);
	writel(0, host->base + SD_EMMC_DELAY1_V3);
	writel(0, host->base + SD_EMMC_DELAY2_V3);
	writel(0, host->base + SD_EMMC_CLOCK_V3);
#ifndef SD_EMMC_CLK_CTRL
	ret = aml_emmc_clktree_init(host);
	if (ret)
		return ret;
#endif
	/* init SD_EMMC_CLOCK to sane defaults w/min clock rate */
	vclkc = 0;
	pclkc->div = 60;	 /* 400KHz */
	pclkc->src = 0;	  /* 0: Crystal 24MHz */
	pclkc->core_phase = init->core_phase;	  /* 2: 180 phase */
	pclkc->rx_phase = init->rx_phase;
	pclkc->tx_phase = init->tx_phase;
	pclkc->always_on = 1;	  /* Keep clock always on */
	writel(vclkc, host->base + SD_EMMC_CLOCK_V3);

	vconf = 0;
	/* 1bit mode */
	pconf->bus_width = 0;
	/* 512byte block length */
	pconf->bl_len = 9;
	/* 64 CLK cycle, here 2^8 = 256 clk cycles */
	pconf->resp_timeout = 8;
	/* 1024 CLK cycle, Max. 100mS. */
	pconf->rc_cc = 4;
	pconf->err_abort = 0;
	pconf->auto_clk = 1;
	writel(vconf, host->base + SD_EMMC_CFG);

	writel(0xffff, host->base + SD_EMMC_STATUS);
	writel(SD_EMMC_IRQ_ALL, host->base + SD_EMMC_IRQ_EN);

	return ret;
}

static int meson_mmc_clk_set_rate_v3(struct mmc_host *mmc,
		unsigned long clk_ios)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	int ret = 0;

#ifdef SD_EMMC_CLK_CTRL
	u32 clk_rate, clk_div, clk_src_sel;
#else
	struct clk *src0_clk = NULL;
	u32 vcfg = 0;
	struct sd_emmc_config *conf = (struct sd_emmc_config *)&vcfg;
	u32 vclkc = 0;
	struct sd_emmc_clock_v3 *clkc = (struct sd_emmc_clock_v3 *)&vclkc;
#endif

#ifdef SD_EMMC_CLK_CTRL
	if (clk_ios == 0) {
		aml_mmc_clk_switch_off(pdata);
		return ret;
	}

	clk_src_sel = SD_EMMC_CLOCK_SRC_OSC;
	if (clk_ios < 20000000)
		clk_src_sel = SD_EMMC_CLOCK_SRC_OSC;
	else
		clk_src_sel = SD_EMMC_CLOCK_SRC_FCLK_DIV2;
#endif

	if (clk_ios) {
		if (WARN_ON(clk_ios > mmc->f_max))
			clk_ios = mmc->f_max;
		else if (WARN_ON(clk_ios < mmc->f_min))
			clk_ios = mmc->f_min;
	}

#ifdef SD_EMMC_CLK_CTRL
	WARN_ON(clk_src_sel > SD_EMMC_CLOCK_SRC_FCLK_DIV2);
	switch (clk_src_sel) {
	case SD_EMMC_CLOCK_SRC_OSC:
		clk_rate = 24000000;
		break;
	case SD_EMMC_CLOCK_SRC_FCLK_DIV2:
		clk_rate = 1000000000;
		break;
	default:
		pr_err("%s: clock source error: %d\n",
			mmc_hostname(host->mmc), clk_src_sel);
		ret = -1;
	}
	clk_div = (clk_rate / clk_ios) + (!!(clk_rate % clk_ios));

	aml_mmc_clk_switch(host, clk_div, clk_src_sel);
	pdata->clkc = readl(host->base + SD_EMMC_CLOCK_V3);

	mmc->actual_clock = clk_rate / clk_div;
#else
	if (clk_ios == mmc->actual_clock) {
		pr_debug("[%s] clk_ios: %lu,  return .............. clock: 0x%x\n",
			pdata->pinname, clk_ios,
			readl(host->base + SD_EMMC_CLOCK_V3));
		return 0;
	}
	pr_debug("[%s] clk_ios: %lu,before clock: 0x%x\n",
		__func__, clk_ios, readl(host->base + SD_EMMC_CLOCK_V3));
	/* stop clock */
	vcfg = readl(host->base + SD_EMMC_CFG);
	if (!conf->stop_clk) {
		conf->stop_clk = 1;
		writel(vcfg, host->base + SD_EMMC_CFG);
		pdata->stop_clk = 1;
	}

	if (aml_card_type_mmc(pdata)) {
		if ((clk_ios >= 200000000) && conf->ddr) {
			src0_clk = devm_clk_get(host->dev, "clkin2");
			if (ret)
				pr_warn("not get clkin2\n");
			ret = clk_set_parent(host->mux_parent[0], src0_clk);
			if (ret)
				pr_warn("set src0 as comp0 parent error\n");
			ret = clk_set_parent(host->mux_clk,
					host->mux_parent[0]);
			if (ret)
				pr_warn("set comp0 as mux_clk parent error\n");
		} else if (((host->data->chip_type >= MMC_CHIP_TL1)
				|| (host->data->chip_type == MMC_CHIP_G12B))
				&& (clk_ios >= 166000000)) {
			src0_clk = devm_clk_get(host->dev, "clkin2");
			if (ret)
				pr_warn("not get clkin2\n");
			if ((host->data->chip_type == MMC_CHIP_TL1)
				&& (clk_ios <= 198000000)) {
			ret = clk_set_rate(src0_clk, 792000000);
				if (ret)
					pr_warn("not set tl1-gp0\n");
			}
			pr_warn("set rate clkin2>>>>>>>>clk:%lu\n",
					clk_get_rate(src0_clk));
			ret = clk_set_parent(host->mux_parent[0],
					src0_clk);
			if (ret)
				pr_warn("set src0 as comp0 parent error\n");
			ret = clk_set_parent(host->mux_clk,
					host->mux_parent[0]);
			if (ret)
				pr_warn("set comp0 as mux_clk parent error\n");
		} else if (clk_get_rate(host->mux_parent[0]) > 200000000) {
			pr_info("%s %d\n", __func__, __LINE__);
			src0_clk = devm_clk_get(host->dev, "xtal");
			ret = clk_set_parent(host->mux_parent[0], src0_clk);
			if (ret)
				pr_warn("set src0: xtal as comp0 parent error\n");
		}
	}

	dev_dbg(host->dev, "change clock rate %u -> %lu\n",
		mmc->actual_clock, clk_ios);
	ret = clk_set_rate(host->cfg_div_clk, clk_ios);
	if (clk_ios && clk_ios != clk_get_rate(host->cfg_div_clk)) {
		dev_warn(host->dev, "divider requested rate %lu != actual rate %lu: ret=%d\n",
			 clk_ios, clk_get_rate(host->cfg_div_clk), ret);
		mmc->actual_clock = clk_get_rate(host->cfg_div_clk);
	} else
		mmc->actual_clock = clk_ios;

	/* (re)start clock, if non-zero */
	if (clk_ios) {
		if (pdata->calc_f) {
		vclkc = readl(host->base + SD_EMMC_CLOCK_V3);
		pdata->clk_lay.source
			= clk_get_rate(host->cfg_div_clk) * clkc->div;
		pdata->clk_lay.core = clk_get_rate(host->cfg_div_clk);
		}

		vcfg = readl(host->base + SD_EMMC_CFG);
		conf->stop_clk = 0;
		writel(vcfg, host->base + SD_EMMC_CFG);
		pdata->clkc = readl(host->base + SD_EMMC_CLOCK_V3);
		pdata->stop_clk = 0;
	}
#endif
	pr_info("actual_clock :%u, HHI_nand: 0x%x\n",
			mmc->actual_clock,
			readl(host->clksrc_base + (HHI_NAND_CLK_CNTL << 2)));

	pr_info("[%s] after clock: 0x%x\n",
		 __func__, readl(host->base + SD_EMMC_CLOCK_V3));

	return ret;
}

static void aml_sd_emmc_set_timing_v3(struct amlsd_platform *pdata,
				u32 timing)
{
	struct amlsd_host *host = pdata->host;
	u32 vctrl;
	struct sd_emmc_config *ctrl = (struct sd_emmc_config *)&vctrl;
	u32 vclkc = readl(host->base + SD_EMMC_CLOCK_V3);
	struct sd_emmc_clock_v3 *clkc = (struct sd_emmc_clock_v3 *)&vclkc;
	u32 adjust = 0;
	struct sd_emmc_adjust_v3 *gadjust = (struct sd_emmc_adjust_v3 *)&adjust;
	u8 clk_div = 0;
	struct para_e *para = &(host->data->sdmmc);
	unsigned int fixdiv = 0, ret = 0;
	u32 irq_en = readl(host->base + SD_EMMC_IRQ_EN);

	vctrl = readl(host->base + SD_EMMC_CFG);
	if ((timing == MMC_TIMING_MMC_HS400) ||
			 (timing == MMC_TIMING_MMC_DDR52) ||
				 (timing == MMC_TIMING_UHS_DDR50)) {

		if (timing == MMC_TIMING_MMC_DDR52) {
			if (aml_card_type_mmc(pdata)) {
				clkc->core_phase  = para->ddr.core_phase;
				clkc->tx_phase  = para->ddr.tx_phase;
			}
			pr_info("%s: try set sd/emmc to DDR mode\n",
					mmc_hostname(host->mmc));
		}
		if (timing == MMC_TIMING_MMC_HS400) {
			/*ctrl->chk_ds = 1;*/
			if (host->data->chip_type >= MMC_CHIP_TXLX) {
				adjust = readl(host->base + SD_EMMC_ADJUST_V3);
				gadjust->ds_enable = 1;
				writel(adjust, host->base + SD_EMMC_ADJUST_V3);
				clkc->tx_delay = para->hs4.tx_delay;
				pdata->adj = adjust;
				/*TODO: double check!
				 * overide tx-delay by dts configs
				 */
				if (pdata->tx_delay != 0)
					clkc->tx_delay = pdata->tx_delay;

				if (((host->data->chip_type == MMC_CHIP_TL1)
				|| (host->data->chip_type == MMC_CHIP_G12B))
					&& aml_card_type_mmc(pdata)) {
					clkc->core_phase = para->hs4.core_phase;
					clkc->tx_phase = para->hs4.tx_phase;

					irq_en |= (1<<17);
					writel(irq_en,
						host->base + SD_EMMC_IRQ_EN);
				/*improve CMD setup time by half SD_CLK cycle*/
				}
			}
			pr_info("%s: try set sd/emmc to HS400 mode\n",
				mmc_hostname(host->mmc));
		}
		ctrl->ddr = 1;
		clk_div = clkc->div;
		if (clk_div & 0x01)
			clk_div++;
		clkc->div = clk_div / 2;
	} else if (timing == MMC_TIMING_MMC_HS) {
		clkc->core_phase = para->hs.core_phase;
		/* overide co-phase by dts */
		if (pdata->co_phase)
			clkc->core_phase = pdata->co_phase;
		if (pdata->calc_f) {
			clkc->core_phase = para->calc.core_phase;
			clkc->tx_phase = para->calc.tx_phase;
		}
	} else if (timing == MMC_TIMING_MMC_HS200) {
		clkc->core_phase = para->hs2.core_phase;
		clkc->tx_phase = para->hs2.tx_phase;
	} else if (timing == MMC_TIMING_SD_HS) {
		if (aml_card_type_non_sdio(pdata)
			|| (host->data->chip_type == MMC_CHIP_TXLX)
			|| (host->data->chip_type == MMC_CHIP_G12A))
			clkc->core_phase = para->sd_hs.core_phase;
		if (pdata->calc_f) {
			clkc->core_phase = para->calc.core_phase;
			clkc->tx_phase = para->calc.tx_phase;
		}
	} else if (timing == MMC_TIMING_UHS_SDR104) {
		clkc->core_phase = para->sdr104.core_phase;
		clkc->tx_phase = para->sdr104.tx_phase;
	} else {
		ctrl->ddr = 0;
		clkc->tx_delay = 0;
		clkc->core_phase = para->init.core_phase;
		clkc->tx_phase = para->init.tx_phase;
		irq_en &= ~(1<<17);
		writel(irq_en, host->base + SD_EMMC_IRQ_EN);
		/* timing == MMC_TIMING_LEGACY */
		if (pdata->calc_f) {
			clkc->core_phase = para->calc.core_phase;
			clkc->tx_phase = para->calc.tx_phase;
		}
	}

	if (pdata->calc_f) {
		if (timing <= MMC_TIMING_SD_HS) {
			ret = aml_fixdiv_calc(&fixdiv, &pdata->clk_lay);
			if (!ret) {
				adjust = readl(host->base + SD_EMMC_ADJUST_V3);
				gadjust->adj_delay = fixdiv;
				gadjust->adj_enable = 1;
				writel(adjust, host->base + SD_EMMC_ADJUST_V3);
				pdata->adj = adjust;
				pr_info("%s: fixdiv calc done: adj = %x\n",
						pdata->pinname, adjust);
			}
		} else if ((timing == MMC_TIMING_MMC_DDR52)
				|| (timing == MMC_TIMING_UHS_DDR50)) {
			adjust = readl(host->base + SD_EMMC_ADJUST_V3);
			gadjust->adj_delay = 0;
			gadjust->adj_enable = 0;
			writel(adjust, host->base + SD_EMMC_ADJUST_V3);
			pdata->adj = adjust;
			pr_debug("%s: ddr52 close calc: adj = %x\n",
					pdata->pinname, adjust);
		}
	}

	writel(vclkc, host->base + SD_EMMC_CLOCK_V3);
	pdata->clkc = vclkc;

	writel(vctrl, host->base + SD_EMMC_CFG);
	pr_debug("sd emmc is %s\n",
			ctrl->ddr?"DDR mode":"SDR mode");
}

/*call by mmc, power on, power off ...*/
static void aml_sd_emmc_set_power_v3(struct amlsd_platform *pdata,
					u32 power_mode)
{
	struct amlsd_host *host = pdata->host;

	switch (power_mode) {
	case MMC_POWER_ON:
		if (pdata->pwr_pre)
			pdata->pwr_pre(pdata);
		if (pdata->pwr_on)
			pdata->pwr_on(pdata);
		break;
	case MMC_POWER_UP:
		break;
	case MMC_POWER_OFF:
		writel(0, host->base + SD_EMMC_DELAY1_V3);
		writel(0, host->base + SD_EMMC_DELAY2_V3);
		writel(0, host->base + SD_EMMC_ADJUST_V3);
		writel(0, host->base + SD_EMMC_INTF3);
		break;
	default:
		if (pdata->pwr_pre)
			pdata->pwr_pre(pdata);
		if (pdata->pwr_off)
			pdata->pwr_off(pdata);
		break;
	}
}

void meson_mmc_set_ios_v3(struct mmc_host *mmc,
				struct mmc_ios *ios)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;

	if ((host->mem->start == host->data->port_b_base)
			&& host->data->tdma_f
			&& (host->init_volt == 0))
		wait_for_completion(&host->drv_completion);

	if (!pdata->is_in) {
		if ((host->mem->start == host->data->port_b_base)
				&& host->data->tdma_f
				&& (host->init_volt == 0))
			complete(&host->drv_completion);
		return;
	}

	/*Set Power*/
	aml_sd_emmc_set_power_v3(pdata, ios->power_mode);

	/*Set Clock*/
	meson_mmc_clk_set_rate_v3(mmc, ios->clock);

	/*Set Bus Width*/
	aml_sd_emmc_set_buswidth(pdata, ios->bus_width);

	/* Set Date Mode */
	aml_sd_emmc_set_timing_v3(pdata, ios->timing);

	if (ios->chip_select == MMC_CS_HIGH)
		aml_cs_high(mmc);
	else if (ios->chip_select == MMC_CS_DONTCARE)
		aml_cs_dont_care(mmc);
	if ((host->mem->start == host->data->port_b_base)
			&& host->data->tdma_f
			&& (host->init_volt == 0))
		complete(&host->drv_completion);
}


irqreturn_t meson_mmc_irq_thread_v3(int irq, void *dev_id)
{
	struct amlsd_host *host = dev_id;
	struct amlsd_platform *pdata;
	struct mmc_request *mrq;
	unsigned long flags;
	enum aml_mmc_waitfor xfer_step;
	u32 status, xfer_bytes = 0;

	spin_lock_irqsave(&host->mrq_lock, flags);
	pdata = mmc_priv(host->mmc);
	mrq = host->mrq;
	xfer_step = host->xfer_step;
	status = host->status;

	if ((xfer_step == XFER_FINISHED) || (xfer_step == XFER_TIMER_TIMEOUT)) {
		pr_err("Warning: %s xfer_step=%d, host->status=%d\n",
				mmc_hostname(host->mmc), xfer_step, status);
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		return IRQ_HANDLED;
	}

	WARN_ON((host->xfer_step != XFER_IRQ_OCCUR)
		 && (host->xfer_step != XFER_IRQ_TASKLET_BUSY));

	if (!mrq) {
		pr_err("%s: !mrq xfer_step %d\n",
				mmc_hostname(host->mmc), xfer_step);
			spin_unlock_irqrestore(&host->mrq_lock, flags);
			return IRQ_HANDLED;
		/* aml_sd_emmc_print_err(host); */
	}
	/* process stop cmd we sent on porpos */
	if (host->cmd_is_stop) {
		/* --new irq enter, */
		host->cmd_is_stop = 0;
		mrq->cmd->error = host->error_bak;
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		meson_mmc_request_done(host->mmc, host->mrq);

		return IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&host->mrq_lock, flags);

	WARN_ON(!host->mrq->cmd);

	switch (status) {
	case HOST_TASKLET_DATA:
	case HOST_TASKLET_CMD:
		/* WARN_ON(aml_sd_emmc_desc_check(host)); */
		pr_debug("%s %d cmd:%d\n",
				__func__, __LINE__, mrq->cmd->opcode);
		host->error_flag = 0;
		if (mrq->cmd->data &&  mrq->cmd->opcode) {
			xfer_bytes = mrq->data->blksz*mrq->data->blocks;
			/* copy buffer from dma to data->sg in read cmd*/
#ifdef SD_EMMC_REQ_DMA_SGMAP
			WARN_ON(host->post_cmd_op(host, mrq));
#else
			if (host->mrq->data->flags & MMC_DATA_READ) {
				aml_sg_copy_buffer(mrq->data->sg,
				mrq->data->sg_len, host->bn_buf,
				 xfer_bytes, 0);
			}
#endif
			mrq->data->bytes_xfered = xfer_bytes;
			host->xfer_step = XFER_TASKLET_DATA;
		} else {
			host->xfer_step = XFER_TASKLET_CMD;
		}
		spin_lock_irqsave(&host->mrq_lock, flags);
		mrq->cmd->error = 0;
		spin_unlock_irqrestore(&host->mrq_lock, flags);

		meson_mmc_read_resp(host->mmc, mrq->cmd);
		meson_mmc_request_done(host->mmc, mrq);

		break;

	case HOST_RSP_TIMEOUT_ERR:
	case HOST_DAT_TIMEOUT_ERR:
	case HOST_RSP_CRC_ERR:
	case HOST_DAT_CRC_ERR:
		if (host->is_tunning == 0)
			pr_info("%s %d %s: cmd:%d\n", __func__, __LINE__,
				mmc_hostname(host->mmc), mrq->cmd->opcode);
		if (mrq->cmd->data) {
			dma_unmap_sg(mmc_dev(host->mmc), mrq->cmd->data->sg,
				mrq->cmd->data->sg_len,
				(mrq->cmd->data->flags & MMC_DATA_READ) ?
					DMA_FROM_DEVICE : DMA_TO_DEVICE);
		}
		meson_mmc_read_resp(host->mmc, host->mrq->cmd);

		/* set retry @ 1st error happens! */
		if ((host->error_flag == 0)
			&& (aml_card_type_mmc(pdata)
				|| aml_card_type_non_sdio(pdata))
			&& (host->is_tunning == 0)) {

			pr_err("%s() %d: set 1st retry!\n",
					__func__, __LINE__);
			host->error_flag |= (1<<0);
			spin_lock_irqsave(&host->mrq_lock, flags);
			mrq->cmd->retries = 3;
			spin_unlock_irqrestore(&host->mrq_lock, flags);
		}

		if (aml_card_type_mmc(pdata) &&
			(host->error_flag & (1<<0)) && mrq->cmd->retries) {
			pr_err("retry cmd %d the %d-th time(s)\n",
					mrq->cmd->opcode, mrq->cmd->retries);
			/* chage configs on current host */
		}
		/* last retry effort! */
		if ((aml_card_type_mmc(pdata) || aml_card_type_non_sdio(pdata))
			&& host->error_flag && (mrq->cmd->retries == 0)) {
			host->error_flag |= (1<<30);
			pr_err("Command retried failed line:%d, cmd:%d\n",
					__LINE__, mrq->cmd->opcode);
		}
		/* retry need send a stop 2 emmc... */
		/* do not send stop for sdio wifi case */
		if (host->mrq->stop
			&& (aml_card_type_mmc(pdata)
				|| aml_card_type_non_sdio(pdata))
			&& pdata->is_in
			&& (host->mrq->cmd->opcode != MMC_SEND_TUNING_BLOCK)
			&& (host->mrq->cmd->opcode !=
					MMC_SEND_TUNING_BLOCK_HS200))
			aml_sd_emmc_send_stop(host);
		else
			meson_mmc_request_done(host->mmc, mrq);
		break;

	default:
		pr_err("BUG %s: xfer_step=%d, host->status=%d\n",
				mmc_hostname(host->mmc),  xfer_step, status);
		/* aml_sd_emmc_print_err(host); */
	}

	return IRQ_HANDLED;
}

static int aml_sd_emmc_cali_v3(struct mmc_host *mmc,
	u8 opcode, u8 *blk_test, u32 blksz, u32 blocks, u8 *pattern)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_command stop = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;

	cmd.opcode = opcode;
	if (!strcmp(pattern, MMC_PATTERN_NAME))
		cmd.arg = MMC_PATTERN_OFFSET;
	else if (!strcmp(pattern, MMC_MAGIC_NAME))
		cmd.arg = MMC_MAGIC_OFFSET;
	else if (!strcmp(pattern, MMC_RANDOM_NAME))
		cmd.arg = MMC_RANDOM_OFFSET;
	else if (!strcmp(pattern, MMC_DTB_NAME))
		cmd.arg = MMC_DTB_OFFSET;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	stop.opcode = MMC_STOP_TRANSMISSION;
	stop.arg = 0;
	stop.flags = MMC_RSP_R1B | MMC_CMD_AC;

	data.blksz = blksz;
	data.blocks = blocks;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	memset(blk_test, 0, blksz * data.blocks);
	sg_init_one(&sg, blk_test, blksz * data.blocks);

	mrq.cmd = &cmd;
	mrq.stop = &stop;
	mrq.data = &data;
	host->mrq = &mrq;
	mmc_wait_for_req(mmc, &mrq);
	return data.error | cmd.error;
}

static int emmc_test_bus(struct mmc_host *mmc)
{
	int err = 0;
	u32 blksz = 512;
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;

	err = aml_sd_emmc_cali_v3(mmc, MMC_READ_MULTIPLE_BLOCK,
				host->blk_test, blksz, 40, MMC_PATTERN_NAME);
	if (err)
		return err;
	err = aml_sd_emmc_cali_v3(mmc, MMC_READ_MULTIPLE_BLOCK,
				host->blk_test, blksz, 80, MMC_RANDOM_NAME);
	if (err)
		return err;
	err = aml_sd_emmc_cali_v3(mmc, MMC_READ_MULTIPLE_BLOCK,
				host->blk_test, blksz, 40, MMC_MAGIC_NAME);
	if (err)
		return err;
	return err;
}

static int emmc_send_cmd(struct mmc_host *mmc, u32 opcode,
		u32 arg, unsigned int flags)
{
	struct mmc_command cmd = {0};
	u32 err = 0;

	cmd.opcode = opcode;
	cmd.arg = arg;
	cmd.flags = flags;

	err = mmc_wait_for_cmd(mmc, &cmd, 3);
	if (err) {
		pr_debug("[%s][%d] cmd:0x%x send error\n",
				__func__, __LINE__, cmd.opcode);
		return err;
	}
	return err;
}

static int aml_sd_emmc_cmd_v3(struct mmc_host *mmc)
{
	int i;

	emmc_send_cmd(mmc, MMC_SEND_STATUS,
			1<<16, MMC_RSP_R1 | MMC_CMD_AC);
	emmc_send_cmd(mmc, MMC_SELECT_CARD,
			0, MMC_RSP_NONE | MMC_CMD_AC);
	for (i = 0; i < 2; i++)
		emmc_send_cmd(mmc, MMC_SEND_CID,
				1 << 16, MMC_RSP_R2 | MMC_CMD_BCR);
	emmc_send_cmd(mmc, MMC_SELECT_CARD,
			1<<16, MMC_RSP_R1 | MMC_CMD_AC);
	return 0;
}

static int emmc_eyetest_log(struct mmc_host *mmc, u32 line_x)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 adjust = readl(host->base + SD_EMMC_ADJUST_V3);
	struct sd_emmc_adjust_v3 *gadjust =
		(struct sd_emmc_adjust_v3 *)&adjust;
	u32 eyetest_log = 0;
	struct eyetest_log *geyetest_log = (struct eyetest_log *)&(eyetest_log);
	u32 eyetest_out0 = 0, eyetest_out1 = 0;
	u32 intf3 = readl(host->base + SD_EMMC_INTF3);
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	int retry = 3;
	u64 tmp = 0;
	u32 blksz = 512;

	pr_debug("delay1: 0x%x , delay2: 0x%x, line_x: %d\n",
			readl(host->base + SD_EMMC_DELAY1_V3),
			readl(host->base + SD_EMMC_DELAY2_V3), line_x);
	gadjust->cali_enable = 1;
	gadjust->cali_sel = line_x;
	writel(adjust, host->base + SD_EMMC_ADJUST_V3);
	pdata->adj = adjust;
	if (line_x < 9)
		gintf3->eyetest_exp = 7;
	else
		gintf3->eyetest_exp = 3;
RETRY:

	gintf3->eyetest_on = 1;
	writel(intf3, host->base + SD_EMMC_INTF3);
	pdata->intf3 = intf3;

	/*****test start*************/
	udelay(5);
	host->is_tunning = 1;
	if (line_x < 9)
		aml_sd_emmc_cali_v3(mmc,
			MMC_READ_MULTIPLE_BLOCK,
			host->blk_test, blksz, 40, MMC_PATTERN_NAME);
	else
		aml_sd_emmc_cmd_v3(mmc);
	host->is_tunning = 0;
	udelay(1);
	eyetest_log = readl(host->base + SD_EMMC_EYETEST_LOG);

	if (!(geyetest_log->eyetest_done & 0x1)) {
		pr_warn("testing eyetest times:0x%x,out:0x%x,0x%x,line:%d\n",
			readl(host->base + SD_EMMC_EYETEST_LOG),
			eyetest_out0, eyetest_out1, line_x);
		gintf3->eyetest_on = 0;
		writel(intf3, host->base + SD_EMMC_INTF3);
		pdata->intf3 = intf3;
		retry--;
		if (retry == 0) {
			pr_debug("[%s][%d] retry eyetest failed-line:%d\n",
					__func__, __LINE__, line_x);
			return 1;
		}
		goto RETRY;
	}
	eyetest_out0 = readl(host->base + SD_EMMC_EYETEST_OUT0);
	eyetest_out1 = readl(host->base + SD_EMMC_EYETEST_OUT1);
	gintf3->eyetest_on = 0;
	writel(intf3, host->base + SD_EMMC_INTF3);
	writel(0, host->base + SD_EMMC_ADJUST_V3);
	pdata->intf3 = intf3;
	pdata->align[line_x] = ((tmp | eyetest_out1) << 32) | eyetest_out0;
	pr_debug("d1:0x%x,d2:0x%x,u64eyet:0x%016llx,l_x:%d\n",
			readl(host->base + SD_EMMC_DELAY1_V3),
			readl(host->base + SD_EMMC_DELAY2_V3),
			pdata->align[line_x], line_x);
	return 0;
}

static int fbinary(u64 x)
{
	int i;

	for (i = 0; i < 64; i++) {
		if ((x >> i) & 0x1)
			return i;
	}
	return -1;
}

static int emmc_detect_base_line(u64 *arr)
{
	u32 i = 0, first[10] = {0};
	u32  max = 0, l_max = 0xff;

	for (i = 0; i < 8; i++) {
		first[i] = fbinary(arr[i]);
		if (first[i] > max) {
			l_max = i;
			max = first[i];
		}
	}
	pr_debug("%s detect line:%d, max: %u\n",
			__func__, l_max, max);
	return max;
}

/**************** start all data align ********************/
static int emmc_all_data_line_alignment(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 delay1 = 0, delay2 = 0;
	int result;
	int temp = 0, base_line = 0, line_x = 0;

	pdata->base_line = emmc_detect_base_line(pdata->align);
	base_line = pdata->base_line;
	for (line_x = 0; line_x < 9; line_x++) {
		if (line_x == 8)
			continue;
		temp = fbinary(pdata->align[line_x]);
		if (temp <= 4)
			continue;
		result = base_line - temp;
		pr_debug("line_x: %d, result: %d\n",
				line_x, result);
		if (line_x < 5)
			delay1 |= result << (6 * line_x);
		else
			delay2 |= result << (6 * (line_x - 5));
	}
	pr_debug("delay1: 0x%x, delay2: 0x%x\n",
			delay1, delay2);
	delay1 += readl(host->base + SD_EMMC_DELAY1_V3);
	delay2 += readl(host->base + SD_EMMC_DELAY2_V3);
	writel(delay1, host->base + SD_EMMC_DELAY1_V3);
	writel(delay2, host->base + SD_EMMC_DELAY2_V3);
	pdata->dly1 = delay1;
	pdata->dly2 = delay2;
	pr_debug("gdelay1: 0x%x, gdelay2: 0x%x\n",
			readl(host->base + SD_EMMC_DELAY1_V3),
			readl(host->base + SD_EMMC_DELAY2_V3));

	return 0;
}

static int emmc_ds_data_alignment(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 delay1 = readl(host->base + SD_EMMC_DELAY1_V3);
	u32 delay2 = readl(host->base + SD_EMMC_DELAY2_V3);
	int i, line_x, temp = 0;

	for (line_x = 0; line_x < 8; line_x++) {
		temp = fbinary(pdata->align[line_x]);
		if (temp <= 4)
			continue;
		for (i = 0; i < 64; i++) {
			pr_debug("i = %d, delay1: 0x%x, delay2: 0x%x\n",
					i,
					readl(host->base + SD_EMMC_DELAY1_V3),
					readl(host->base + SD_EMMC_DELAY2_V3));
			if (line_x < 5)
				delay1 += 1 << (6 * line_x);
			else
				delay2 += 1 << (6 * (line_x - 5));
			writel(delay1, host->base + SD_EMMC_DELAY1_V3);
			writel(delay2, host->base + SD_EMMC_DELAY2_V3);
			pdata->dly1 = delay1;
			pdata->dly2 = delay2;
			emmc_eyetest_log(mmc, line_x);
			if (pdata->align[line_x] & 0xf0)
				break;
		}
		if (i == 64) {
			pr_warn("%s [%d] Don't find line delay which aligned with DS\n",
				__func__, __LINE__);
			return 1;
		}
	}
	return 0;
}

static void update_all_line_eyetest(struct mmc_host *mmc)
{
	int line_x;
	struct amlsd_platform *pdata = mmc_priv(mmc);

	for (line_x = 0; line_x < 10; line_x++) {
		if ((line_x == 8) && !(pdata->caps2 & MMC_CAP2_HS400))
			continue;
		emmc_eyetest_log(mmc, line_x);
	}
}

static unsigned int tl1_emmc_line_timing(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 delay1 = 0, delay2 = 0, count = 12;

	delay1 = (count<<0)|(count<<6)|(count<<12)
		|(count<<18)|(count<<24);
	delay2 = (count<<0)|(count<<6)|(count<<12)
		|(pdata->cmd_c<<24);
	writel(delay1, host->base + SD_EMMC_DELAY1_V3);
	writel(delay2, host->base + SD_EMMC_DELAY2_V3);
	pr_info("[%s], delay1: 0x%x, delay2: 0x%x\n",
		__func__, readl(host->base + SD_EMMC_DELAY1_V3),
		readl(host->base + SD_EMMC_DELAY2_V3));
	return 0;

}

static unsigned int get_emmc_cmd_win(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 delay2 = readl(host->base + SD_EMMC_DELAY2_V3);
	u32 max = 0, i, temp;
	u32 str[64] = {0};
	int best_start = -1, best_size = -1;
	int cur_start = -1, cur_size = 0;

	for (i = 0; i < 64; i++) {
		delay2 &= ~(0x3f << 24);
		delay2 |= (i << 24);
		writel(delay2, host->base + SD_EMMC_DELAY2_V3);
		emmc_eyetest_log(mmc, 9);
		temp = fbinary(pdata->align[9]);
		str[i] = temp;
		if (max < temp)
			max = temp;
	}
	for (i = 0; i < 64; i++) {
		if (str[i] >= 4) {
			if (cur_start < 0)
				cur_start = i;
			cur_size++;
		} else {
			if (cur_start >= 0) {
				if (best_start < 0) {
					best_start = cur_start;
					best_size = cur_size;
				} else {
					if (best_size < cur_size) {
						best_start = cur_start;
						best_size = cur_size;
					}
				}
				cur_start = -1;
				cur_size = 0;
			}
		}
	}
	if (cur_start >= 0) {
		if (best_start < 0) {
			best_start = cur_start;
			best_size = cur_size;
		} else if (best_size < cur_size) {
			best_start = cur_start;
			best_size = cur_size;
		}
		cur_start = -1;
		cur_size = -1;
	}
	delay2 &= ~(0x3f << 24);
	delay2 |= ((best_start + best_size / 4) << 24);
	writel(delay2, host->base + SD_EMMC_DELAY2_V3);
	emmc_eyetest_log(mmc, 9);
	return max;
}

/* first step*/
static int emmc_ds_core_align(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 delay1 = readl(host->base + SD_EMMC_DELAY1_V3);
	u32 delay2 = readl(host->base + SD_EMMC_DELAY2_V3);
	u32 delay2_bak = delay2;
	u32 count = 0, ds_count = 0, cmd_count = 0;

	ds_count = fbinary(pdata->align[8]);
	if (ds_count == 0)
		if ((pdata->align[8] & 0x1e0) == 0)
			goto out_cmd;
	pr_debug("ds_count:%d,delay1:0x%x,delay2:0x%x\n",
			ds_count, readl(host->base + SD_EMMC_DELAY1_V3),
			readl(host->base + SD_EMMC_DELAY2_V3));
	if (ds_count < 20) {
		delay2 += ((20 - ds_count) << 18);
		writel(delay2, host->base + SD_EMMC_DELAY2_V3);
	} else {
		delay2 += (1<<18);
		writel(delay2, host->base + SD_EMMC_DELAY2_V3);
	}
	emmc_eyetest_log(mmc, 8);
	while (!(pdata->align[8] & 0xf)) {
		delay2 += (1<<18);
		writel(delay2, host->base + SD_EMMC_DELAY2_V3);
		emmc_eyetest_log(mmc, 8);
	}
	delay1 = readl(host->base + SD_EMMC_DELAY1_V3);
	delay2 = readl(host->base + SD_EMMC_DELAY2_V3);
	count = ((delay2>>18) & 0x3f) - ((delay2_bak>>18) & 0x3f);
	delay1 += (count<<0)|(count<<6)|(count<<12)|(count<<18)|(count<<24);
	delay2 += (count<<0)|(count<<6)|(count<<12);
	writel(delay1, host->base + SD_EMMC_DELAY1_V3);
	writel(delay2, host->base + SD_EMMC_DELAY2_V3);

out_cmd:

	cmd_count = get_emmc_cmd_win(mmc);
	pdata->dly1 = readl(host->base + SD_EMMC_DELAY1_V3);
	pdata->dly2 = readl(host->base + SD_EMMC_DELAY2_V3);
	pr_info("ds_count:%u,count:%d, cmd_count:%u\n",
			ds_count, count, cmd_count);
	pr_info("delay1:0x%x,delay2:0x%x\n",
			readl(host->base + SD_EMMC_DELAY1_V3),
			readl(host->base + SD_EMMC_DELAY2_V3));
	return 0;
}

#if 1
static int emmc_ds_manual_sht(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 intf3 = readl(host->base + SD_EMMC_INTF3);
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	int i, err = 0;
	int match[64];
	int best_start = -1, best_size = -1;
	int cur_start = -1, cur_size = 0;

	host->is_tunning = 1;
	for (i = 0; i < 64; i++) {
		err = emmc_test_bus(mmc);
		pr_debug("intf3: 0x%x, err[%d]: %d\n",
				readl(host->base + SD_EMMC_INTF3), i, err);
		if (!err)
			match[i] = 0;
		else
			match[i] = -1;
		gintf3->ds_sht_m += 1;
		writel(intf3, host->base + SD_EMMC_INTF3);
		pdata->intf3 = intf3;
	}
	for (i = 0; i < 64; i++) {
		if (match[i] == 0) {
			if (cur_start < 0)
				cur_start = i;
			cur_size++;
		} else {
			if (cur_start >= 0) {
				if (best_start < 0) {
					best_start = cur_start;
					best_size = cur_size;
				} else {
					if (best_size < cur_size) {
						best_start = cur_start;
						best_size = cur_size;
					}
				}
				cur_start = -1;
				cur_size = 0;
			}
		}
	}
	if (cur_start >= 0) {
		if (best_start < 0) {
			best_start = cur_start;
			best_size = cur_size;
		} else if (best_size < cur_size) {
			best_start = cur_start;
			best_size = cur_size;
		}
		cur_start = -1;
		cur_size = -1;
	}

	gintf3->ds_sht_m = best_start + best_size / 2;
	writel(intf3, host->base + SD_EMMC_INTF3);
	pdata->intf3 = intf3;
	pdata->win_start = best_start;
	pr_info("ds_sht:%u, window:%d, intf3:0x%x, clock:0x%x",
			gintf3->ds_sht_m, best_size,
			readl(host->base + SD_EMMC_INTF3),
			readl(host->base + SD_EMMC_CLOCK_V3));
	pr_info("adjust:0x%x\n", readl(host->base + SD_EMMC_ADJUST_V3));
	host->is_tunning = 0;
	return 0;
}
#endif

static void aml_emmc_hs400_general(struct mmc_host *mmc)
{

	update_all_line_eyetest(mmc);
	emmc_ds_core_align(mmc);
	update_all_line_eyetest(mmc);
	emmc_all_data_line_alignment(mmc);
	update_all_line_eyetest(mmc);
	emmc_ds_data_alignment(mmc);
	update_all_line_eyetest(mmc);
	emmc_ds_manual_sht(mmc);
}

static void aml_emmc_hs400_tl1(struct mmc_host *mmc)
{

	tl1_emmc_line_timing(mmc);
	emmc_ds_manual_sht(mmc);
}

static int emmc_data_alignment(struct mmc_host *mmc, int best_size)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 delay1 = readl(host->base + SD_EMMC_DELAY1_V3);
	u32 delay2 = readl(host->base + SD_EMMC_DELAY2_V3);
	u32 intf3 = readl(host->base + SD_EMMC_INTF3);
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	u32 delay1_bak = delay1;
	u32 delay2_bak = delay2;
	u32 intf3_bak = intf3;
	int line_x, i, err = 0, win_new, blksz = 512;
	u32 d[8];

	host->is_tunning = 1;
	gintf3->ds_sht_m = pdata->win_start + 4;
	writel(intf3, host->base + SD_EMMC_INTF3);
	for (line_x = 0; line_x < 8; line_x++) {
		for (i = 0; i < 20; i++) {
			if (line_x < 5) {
				delay1 += (1 << 6*line_x);
				writel(delay1, host->base + SD_EMMC_DELAY1_V3);
			} else {
				delay2 += (1 << 6*(line_x - 5));
				writel(delay2, host->base + SD_EMMC_DELAY2_V3);
			}
			err = aml_sd_emmc_cali_v3(mmc, MMC_READ_MULTIPLE_BLOCK,
				host->blk_test, blksz, 40, MMC_RANDOM_NAME);
			if (err) {
				pr_info("[%s]adjust line_x[%d]:%d\n",
						__func__, line_x, i);
				d[line_x] = i;
				delay1 = delay1_bak;
				delay2 = delay2_bak;
				writel(delay1_bak,
					host->base + SD_EMMC_DELAY1_V3);
				writel(delay2_bak,
					host->base + SD_EMMC_DELAY2_V3);
				break;
			}
		}
		if (i == 20) {
			pr_info("[%s][%d] return set default value",
					__func__, __LINE__);
			writel(delay1_bak, host->base + SD_EMMC_DELAY1_V3);
			writel(delay2_bak, host->base + SD_EMMC_DELAY2_V3);
			writel(intf3_bak, host->base + SD_EMMC_INTF3);
			host->is_tunning = 0;
			return -1;
		}
	}
	delay1 += (d[0]<<0)|(d[1]<<6)|(d[2]<<12)|(d[3]<<18)|(d[4]<<24);
	delay2 += (d[5]<<0)|(d[6]<<6)|(d[7]<<12);
	writel(delay1, host->base + SD_EMMC_DELAY1_V3);
	writel(delay2, host->base + SD_EMMC_DELAY2_V3);
	pr_info("delay1:0x%x, delay2:0x%x\n",
		readl(host->base + SD_EMMC_DELAY1_V3),
		readl(host->base + SD_EMMC_DELAY2_V3));
	gintf3->ds_sht_m = 0;
	writel(intf3, host->base + SD_EMMC_INTF3);
	win_new = emmc_ds_manual_sht(mmc);
	if (win_new < best_size) {
		pr_info("[%s][%d] win_new:%d < win_old:%d,set default!",
			__func__, __LINE__, win_new, best_size);
		writel(delay1_bak, host->base + SD_EMMC_DELAY1_V3);
		writel(delay2_bak, host->base + SD_EMMC_DELAY2_V3);
		writel(intf3_bak, host->base + SD_EMMC_INTF3);
		pr_info("intf3:0x%x, delay1:0x%x, delay2:0x%x\n",
			readl(host->base + SD_EMMC_INTF3),
			readl(host->base + SD_EMMC_DELAY1_V3),
			readl(host->base + SD_EMMC_DELAY2_V3));
	}
	host->is_tunning = 0;
	return 0;
}

static void aml_emmc_hs400_Revb(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 delay2 = 0;
	int win_size = 0;

	delay2 = readl(host->base + SD_EMMC_DELAY2_V3);
	delay2 += (pdata->cmd_c<<24);
	writel(delay2, host->base + SD_EMMC_DELAY2_V3);
	pr_info("[%s], delay1: 0x%x, delay2: 0x%x\n",
		__func__, readl(host->base + SD_EMMC_DELAY1_V3),
		readl(host->base + SD_EMMC_DELAY2_V3));
	win_size = emmc_ds_manual_sht(mmc);
	emmc_data_alignment(mmc, win_size);
}
/* test clock, return delay cells for one cycle
 */
static unsigned int aml_sd_emmc_clktest(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 intf3 = readl(host->base + SD_EMMC_INTF3);
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	u32 clktest = 0, delay_cell = 0, clktest_log = 0, count = 0;
	u32 vcfg = readl(host->base + SD_EMMC_CFG);
	int i = 0;
	unsigned int cycle = 0;

	writel(0, (host->base + SD_EMMC_ADJUST_V3));

	/* one cycle = xxx(ps) */
	cycle = (1000000000 / mmc->actual_clock) * 1000;
	vcfg &= ~(1 << 23);
	writel(vcfg, host->base + SD_EMMC_CFG);
	writel(0, host->base + SD_EMMC_DELAY1_V3);
	writel(0, host->base + SD_EMMC_DELAY2_V3);
	pdata->dly1 = 0;
	pdata->dly2 = 0;
	gintf3->clktest_exp = 8;
	gintf3->clktest_on_m = 1;
	writel(intf3, host->base + SD_EMMC_INTF3);
	pdata->intf3 = intf3;

	clktest_log = readl(host->base + SD_EMMC_CLKTEST_LOG);
	clktest = readl(host->base + SD_EMMC_CLKTEST_OUT);
	while (!(clktest_log & 0x80000000)) {
		mdelay(1);
		i++;
		clktest_log = readl(host->base + SD_EMMC_CLKTEST_LOG);
		clktest = readl(host->base + SD_EMMC_CLKTEST_OUT);
		if (i > 4) {
			pr_warn("[%s] [%d] emmc clktest error\n",
				__func__, __LINE__);
			break;
		}
	}
	if (clktest_log & 0x80000000) {
		clktest = readl(host->base + SD_EMMC_CLKTEST_OUT);
		count = clktest / (1 << 8);
		if (vcfg & 0x4)
			delay_cell = ((cycle / 2) / count);
		else
			delay_cell = (cycle / count);
	}
	pr_info("%s [%d] clktest : %u, delay_cell: %d, count: %u\n",
			__func__, __LINE__, clktest, delay_cell, count);
	intf3 = readl(host->base + SD_EMMC_INTF3);
	gintf3->clktest_on_m = 0;
	writel(intf3, host->base + SD_EMMC_INTF3);
	pdata->intf3 = intf3;
	vcfg = readl(host->base + SD_EMMC_CFG);
	vcfg |= (1 << 23);
	writel(vcfg, host->base + SD_EMMC_CFG);
	pdata->count = count;
	pdata->delay_cell = delay_cell;
	return count;
}

static int _aml_sd_emmc_execute_tuning(struct mmc_host *mmc, u32 opcode,
					struct aml_tuning_data *tuning_data,
					u32 adj_win_start)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 vclk;
	struct sd_emmc_clock_v3 *clkc = (struct sd_emmc_clock_v3 *)&(vclk);
	u32 adjust = readl(host->base + SD_EMMC_ADJUST_V3);
	struct sd_emmc_adjust_v3 *gadjust =
		(struct sd_emmc_adjust_v3 *)&adjust;
	const u8 *blk_pattern = tuning_data->blk_pattern;
	unsigned int blksz = tuning_data->blksz;
	unsigned long flags;
	int ret = 0;
	u32 nmatch = 0;
	int adj_delay = 0;
	u8 tuning_num = 0;
	u32 clk_div;
	u32 adj_delay_find;
	int wrap_win_start, wrap_win_size;
	int best_win_start, best_win_size;
	int curr_win_start, curr_win_size;
	u32 old_dly, d1_dly, dly;
	struct para_e *para = &(host->data->sdmmc);

	if ((host->mem->start == host->data->port_b_base)
			&& host->data->tdma_f)
		wait_for_completion(&host->drv_completion);

	old_dly = readl(host->base + SD_EMMC_DELAY1_V3);
	d1_dly = (old_dly >> 0x6) & 0x3F;
	pr_info("Data 1 aligned delay is %d\n", d1_dly);
	writel(0, host->base + SD_EMMC_ADJUST_V3);

tunning:
	/* renew */
	wrap_win_start = -1;
	wrap_win_size = 0;
	best_win_start = -1;
	best_win_size = 0;
	curr_win_start = -1;
	curr_win_size = 0;

	spin_lock_irqsave(&host->mrq_lock, flags);
	pdata->need_retuning = false;
	spin_unlock_irqrestore(&host->mrq_lock, flags);
	vclk = readl(host->base + SD_EMMC_CLOCK_V3);
	clk_div = clkc->div;

	host->is_tunning = 1;
	pr_info("%s: clk %d tuning start\n",
			mmc_hostname(mmc), mmc->actual_clock);

	/*retry adj[clk_div-1] tuning result*/
	if ((clk_div == 5) && (aml_card_type_mmc(pdata))) {
		gadjust->adj_delay = clk_div-1;
		gadjust->adj_enable = 1;
		gadjust->cali_enable = 0;
		gadjust->cali_rise = 0;
		writel(adjust, host->base +	SD_EMMC_ADJUST_V3);
		nmatch = aml_sd_emmc_tuning_transfer(mmc, opcode,
				blk_pattern, host->blk_test, blksz);
		if (nmatch != TUNING_NUM_PER_POINT) {
			if (host->data->chip_type != MMC_CHIP_SM1) {
				clkc->core_phase = para->hs2.tx_phase;
				clkc->tx_phase = para->hs2.core_phase;
			}
			writel(vclk, host->base + SD_EMMC_CLOCK_V3);
			pr_info("%s:try clock:0x%x>>>rx_tuning[%d] = %d\n",
				mmc_hostname(host->mmc),
				readl(host->base + SD_EMMC_CLOCK_V3),
				gadjust->adj_delay, nmatch);
		}
	}
	for (adj_delay = 0; adj_delay < clk_div; adj_delay++) {
		gadjust->adj_delay = adj_delay;
		gadjust->adj_enable = 1;
		gadjust->cali_enable = 0;
		gadjust->cali_rise = 0;
		writel(adjust, host->base +	SD_EMMC_ADJUST_V3);
		pdata->adj = adjust;
		nmatch = aml_sd_emmc_tuning_transfer(mmc, opcode,
				blk_pattern, host->blk_test, blksz);
		/*get a ok adjust point!*/
		if (nmatch == TUNING_NUM_PER_POINT) {
			if (adj_delay == 0)
				wrap_win_start = adj_delay;

			if (wrap_win_start >= 0)
				wrap_win_size++;

			if (curr_win_start < 0)
				curr_win_start = adj_delay;

			curr_win_size++;
			pr_info("%s: rx_tuning_result[%d] = %d\n",
				mmc_hostname(host->mmc), adj_delay, nmatch);
		} else {
			if (curr_win_start >= 0) {
				if (best_win_start < 0) {
					best_win_start = curr_win_start;
					best_win_size = curr_win_size;
				} else {
					if (best_win_size < curr_win_size) {
						best_win_start = curr_win_start;
						best_win_size = curr_win_size;
					}
				}

				wrap_win_start = -1;
				curr_win_start = -1;
				curr_win_size = 0;
			}
		}
	}
	/* last point is ok! */
	if (curr_win_start >= 0) {
		if (best_win_start < 0) {
			best_win_start = curr_win_start;
			best_win_size = curr_win_size;
		} else if (wrap_win_size > 0) {
			/* Wrap around case */
			if (curr_win_size + wrap_win_size > best_win_size) {
				best_win_start = curr_win_start;
				best_win_size = curr_win_size + wrap_win_size;
			}
		} else if (best_win_size < curr_win_size) {
			best_win_start = curr_win_start;
			best_win_size = curr_win_size;
		}

		curr_win_start = -1;
		curr_win_size = 0;
	}
	if (best_win_size <= 0) {
		if ((tuning_num++ > MAX_TUNING_RETRY)
			|| (clkc->div >= 10)) {
			pr_info("%s: final result of tuning failed\n",
				 mmc_hostname(host->mmc));
			host->is_tunning = 0;
			if ((host->mem->start == host->data->port_b_base)
				&& host->data->tdma_f)
				complete(&host->drv_completion);
			return -1;
		}
		clkc->div += 1;
		writel(vclk, host->base + SD_EMMC_CLOCK_V3);
		pdata->clkc = readl(host->base + SD_EMMC_CLOCK_V3);
		pr_info("%s: tuning failed, reduce freq and retuning\n",
			mmc_hostname(host->mmc));
		goto tunning;
	} else if (best_win_size == clk_div) {
		dly = readl(host->base + SD_EMMC_DELAY1_V3);
		d1_dly = (dly >> 0x6) & 0x3F;
		pr_warn("%s() d1_dly %d, window start %d, size %d\n",
			__func__, d1_dly, best_win_start, best_win_size);
		if (++d1_dly > 0x3F) {
			pr_err("%s: tuning failed\n",
				mmc_hostname(host->mmc));
			host->is_tunning = 0;
			if ((host->mem->start == host->data->port_b_base)
				&& host->data->tdma_f)
				complete(&host->drv_completion);
			return -1;
		}
		dly &= ~(0x3F << 6);
		dly |= d1_dly << 6;
		pdata->dly1 = dly;
		writel(dly, host->base + SD_EMMC_DELAY1_V3);
		goto tunning;
	} else
		pr_info("%s: best_win_start =%d, best_win_size =%d\n",
				mmc_hostname(host->mmc),
				best_win_start, best_win_size);

	adj_delay_find = best_win_start + (best_win_size - 1) / 2
		+ (best_win_size - 1) % 2;
	adj_delay_find = adj_delay_find % clk_div;

	gadjust->adj_delay = adj_delay_find;
	gadjust->adj_enable = 1;
	gadjust->cali_enable = 0;
	gadjust->cali_rise = 0;
	writel(adjust, host->base + SD_EMMC_ADJUST_V3);
	pdata->adj = adjust;
	pdata->dly1 = old_dly;
	writel(old_dly, host->base + SD_EMMC_DELAY1_V3);

	pr_info("%s: sd_emmc_regs->gclock=0x%x,sd_emmc_regs->gadjust=0x%x\n",
			mmc_hostname(host->mmc),
			readl(host->base + SD_EMMC_CLOCK_V3),
			readl(host->base + SD_EMMC_ADJUST_V3));
	host->is_tunning = 0;
	if ((host->mem->start == host->data->port_b_base)
			&& host->data->tdma_f)
		complete(&host->drv_completion);

	return ret;
}

int aml_get_data_eyetest(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 delay1 = readl(host->base + SD_EMMC_DELAY1_V3);
	u32 delay2 = readl(host->base + SD_EMMC_DELAY2_V3);
	int ret = 0, retry = 10, line_x;

	host->is_timming = 1;
	host->is_tunning = 1;
	pr_info("[%s] 2018-4-18 emmc HS200 Timming\n", __func__);
	aml_sd_emmc_clktest(mmc);
	for (line_x = 0; line_x < 10; line_x++) {
		if (line_x == 8)
			continue;
RETRY:
		ret = emmc_eyetest_log(mmc, line_x);
		if (ret && retry) {
			pr_info("add dly [%d],retry%d...\n",
				line_x, retry);
			if (line_x < 5) {
				delay1 += (2<<(6*line_x));
				writel(delay1,
					(host->base + SD_EMMC_DELAY1_V3));
			} else {
				delay2 += (2<<(6*(line_x-5)));
				writel(delay2,
					(host->base + SD_EMMC_DELAY2_V3));
			}
			pr_debug("gdelay1: 0x%x, gdelay2: 0x%x\n",
				readl(host->base + SD_EMMC_DELAY1_V3),
				readl(host->base + SD_EMMC_DELAY2_V3));
			retry--;
			goto RETRY;
		} else if (ret && !retry) {
			pr_info("retry failed,line:%d\n",
				line_x);
			return 1;
		}
		retry = 10;
	}
	pr_debug("gadjust:0x%x,intf3:0x%x\n",
		readl(host->base + SD_EMMC_ADJUST_V3),
		readl(host->base + SD_EMMC_INTF3));
	pr_info("gdelay1: 0x%x, gdelay2: 0x%x\n",
		readl(host->base + SD_EMMC_DELAY1_V3),
		readl(host->base + SD_EMMC_DELAY2_V3));
	update_all_line_eyetest(mmc);
	host->is_timming = 0;
	host->is_tunning = 0;
	return 0;
}

int aml_emmc_hs200_tl1(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 vclkc = readl(host->base + SD_EMMC_CLOCK_V3);
	struct sd_emmc_clock_v3 *clkc = (struct sd_emmc_clock_v3 *)&vclkc;
	struct para_e *para = &(host->data->sdmmc);
	u32 clk_bak = 0;
	u32 delay2 = 0, count = 0;
	int i, err = 0;

	clk_bak = vclkc;
	clkc->tx_phase = para->hs4.tx_phase;
	clkc->core_phase = para->hs4.core_phase;
	clkc->tx_delay = para->hs4.tx_delay;
	if (pdata->tx_delay != 0)
		clkc->tx_delay = pdata->tx_delay;
	writel(vclkc, host->base + SD_EMMC_CLOCK_V3);
	pr_info("[%s][%d] clk config:0x%x\n",
		__func__, __LINE__, readl(host->base + SD_EMMC_CLOCK_V3));
	for (i = 0; i < 63; i++) {
		delay2 += (1 << 24);
		writel(delay2, host->base + SD_EMMC_DELAY2_V3);
		err = emmc_eyetest_log(mmc, 9);
		if (err)
			continue;
		count = fbinary(pdata->align[9]);
		if (((count >= 10) && (count <= 22))
			|| ((count >= 45) && (count <= 56)))
			break;
	}
	if (i == 63)
		pr_err("[%s]no find cmd timing\n", __func__);
	pdata->cmd_c = (delay2 >> 24);
	pr_info("cmd->u64eyet:0x%016llx\n",
			pdata->align[9]);
	writel(0, host->base + SD_EMMC_DELAY2_V3);
	writel(clk_bak, host->base + SD_EMMC_CLOCK_V3);
	pr_info("[%s][%d] clk config:0x%x\n",
		__func__, __LINE__, readl(host->base + SD_EMMC_CLOCK_V3));
	return 0;

}

int __attribute__((unused)) aml_emmc_hs200_timming(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 count = 0, delay1 = 0, delay2 = 0, line_x = 0;
	u32 dat = host->data->latest_dat;
	int ret = 0, add = 0, base, temp, result;

	ret = aml_get_data_eyetest(mmc);
	if (ret) {
		pr_info("[%s]hs200 timing err!\n",
				__func__);
		return 1;
	}
	delay1 = readl(host->base + SD_EMMC_DELAY1_V3);
	delay2 = readl(host->base + SD_EMMC_DELAY2_V3);
	if (pdata->latest_dat != 0)
		dat = pdata->latest_dat;
	if (dat < 5)
		add = (delay1 >> (dat * 6)) & 0x3f;
	else
		add = (delay2 >> (dat * 6)) & 0x3f;
	count = fbinary(pdata->align[dat]);
	if (count <= pdata->count/3) {
		count = pdata->count/3 - count;
		if (add)
			count += add;
	} else if (count <= (pdata->count*2)/3)
		count = 0;
	else
		count = (pdata->count - count) + pdata->count/3;
	delay1 = (count<<0)|(count<<6)|(count<<12)
		|(count<<18)|(count<<24);
	writel(delay1, (host->base + SD_EMMC_DELAY1_V3));
	delay2 = (count<<0)|(count<<6)|(count<<12);
	writel(delay2, (host->base + SD_EMMC_DELAY2_V3));
	update_all_line_eyetest(mmc);
	pr_info("delay1: 0x%x, delay2: 0x%x, add:%d\n",
		readl(host->base + SD_EMMC_DELAY1_V3),
		readl(host->base + SD_EMMC_DELAY2_V3), add);
	/* align all data */
	base = fbinary(pdata->align[dat]);
	delay1 = readl(host->base + SD_EMMC_DELAY1_V3);
	delay2 = readl(host->base + SD_EMMC_DELAY2_V3);
	for (line_x = 0; line_x < 8; line_x++) {
		temp = fbinary(pdata->align[line_x]);
		result = base - temp;
		pr_debug("*****line_x: %d, result: %d\n",
				line_x, result);
		if (result < 0)
			continue;
		if (line_x < 5)
			delay1 += result << (6 * line_x);
		else
			delay2 += result << (6 * (line_x - 5));
	}
	writel(delay1, (host->base + SD_EMMC_DELAY1_V3));
	writel(delay2, (host->base + SD_EMMC_DELAY2_V3));
	/* end */

	count = fbinary(pdata->align[9]);
	if (count <= pdata->count/4)
		count = pdata->count/4 - count;
	else if (count <= pdata->count*3/4)
		count = 0;
	else
		count = (64 - count) + pdata->count/4;
	delay2 += (count<<24);
	writel(delay2, (host->base + SD_EMMC_DELAY2_V3));
	pr_info("delay1: 0x%x , delay2: 0x%x, latest_dat:%d\n",
	    readl(host->base + SD_EMMC_DELAY1_V3),
		readl(host->base + SD_EMMC_DELAY2_V3), dat);
	update_all_line_eyetest(mmc);
	return 0;
}

static int sdio_eyetest_log(struct mmc_host *mmc, u32 line_x, u32 opcode,
		struct aml_tuning_data *tuning_data)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 adjust = readl(host->base + SD_EMMC_ADJUST_V3);
	struct sd_emmc_adjust_v3 *gadjust =
		(struct sd_emmc_adjust_v3 *)&adjust;
	u32 eyetest_log = 0;
	struct eyetest_log *geyetest_log = (struct eyetest_log *)&(eyetest_log);
	u32 eyetest_out0 = 0, eyetest_out1 = 0;
	u32 intf3 = readl(host->base + SD_EMMC_INTF3);
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	int retry = 3, i;
	u64 tmp = 0;
	const u8 *blk_pattern = tuning_data->blk_pattern;
	unsigned int blksz = tuning_data->blksz;

	/****** calculate  line_x ***************************/
	/******* init eyetest register ************************/
	pr_debug("delay1: 0x%x , delay2: 0x%x, line_x: %d\n",
			readl(host->base + SD_EMMC_DELAY1_V3),
			readl(host->base + SD_EMMC_DELAY2_V3), line_x);
	gadjust->cali_enable = 1;
	gadjust->cali_sel = line_x;
	writel(adjust, host->base + SD_EMMC_ADJUST_V3);
	pdata->adj = adjust;
	gintf3->eyetest_exp = 4;

RETRY:
	gintf3->eyetest_on = 1;
	writel(intf3, host->base + SD_EMMC_INTF3);
	pdata->intf3 = intf3;
	udelay(5);

	host->is_tunning = 1;
	for (i = 0; i < 40; i++)
		aml_sd_emmc_tuning_transfer(mmc, opcode,
				blk_pattern, host->blk_test, blksz);
	host->is_tunning = 0;
	udelay(1);
	eyetest_log = readl(host->base + SD_EMMC_EYETEST_LOG);
	eyetest_out0 = readl(host->base + SD_EMMC_EYETEST_OUT0);
	eyetest_out1 = readl(host->base + SD_EMMC_EYETEST_OUT1);

	if (!(geyetest_log->eyetest_done & 0x1)) {
		pr_warn("testing eyetest times: 0x%x, out: 0x%x, 0x%x\n",
				geyetest_log->eyetest_times,
				eyetest_out0, eyetest_out1);
		gintf3->eyetest_on = 0;
		writel(intf3, host->base + SD_EMMC_INTF3);
		pdata->intf3 = intf3;
		retry--;
		if (retry == 0) {
			pr_warn("[%s][%d] retry eyetest failed\n",
					__func__, __LINE__);
			return 1;
		}
		goto RETRY;
	}
	pr_debug("test done! eyetest times: 0x%x, out: 0x%x, 0x%x\n",
			geyetest_log->eyetest_times,
			eyetest_out0, eyetest_out1);
	gintf3->eyetest_on = 0;
	writel(intf3, host->base + SD_EMMC_INTF3);
	pdata->intf3 = intf3;
	pdata->align[line_x] = ((tmp | eyetest_out1) << 32) | eyetest_out0;
	pr_debug("u64 eyetestout 0x%llx\n", pdata->align[line_x]);
	return 0;
}

static int aml_sdio_timing(struct mmc_host *mmc, u32 opcode,
					struct aml_tuning_data *tuning_data,
					u32 adj_win_start)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 line_x = 0, delay1 = 0, retry = 1, temp;
	int ret;

	delay1 = 0;
	for (line_x = 0; line_x < 4; line_x++) {
		writel(0, host->base + SD_EMMC_DELAY1_V3);
		pdata->dly1 = 0;
		retry = 1;
RETRY:
		ret = sdio_eyetest_log(mmc, line_x, opcode, tuning_data);
		if (ret && retry) {
			pr_info("[%s][%d] add delay for data, retry...\n",
					__func__, __LINE__);
			writel(5 << (6 * line_x),
					host->base + SD_EMMC_DELAY1_V3);
			pdata->dly1 = readl(host->base + SD_EMMC_DELAY1_V3);
			delay1 |= (5 << (6 * line_x));
			retry--;
			goto RETRY;
		} else if (ret && !retry) {
			pr_info("[%s][%d] retry failed...\n",
					__func__, __LINE__);
			return 1;
		}
	}

	writel(delay1, host->base + SD_EMMC_DELAY1_V3);
	pdata->dly1 = delay1;
	delay1 = 0;
	for (line_x = 0; line_x < 4; line_x++) {
		temp = fbinary(pdata->align[line_x]);
		if (temp < 31)
			temp = 31 - temp;
		else
			temp = 0;
		pr_debug("line_x: %d, result: %d\n",
				line_x, temp);
		delay1 |= temp << (6 * line_x);
	}
	delay1 += readl(host->base + SD_EMMC_DELAY1_V3);
	writel(delay1, host->base + SD_EMMC_DELAY1_V3);
	pdata->dly1 = delay1;

	pr_info("%s: gadjust=0x%x, gdelay1=0x%x, gclock=0x%x\n",
			mmc_hostname(host->mmc),
			readl(host->base + SD_EMMC_ADJUST_V3),
			readl(host->base + SD_EMMC_DELAY1_V3),
			readl(host->base + SD_EMMC_CLOCK_V3));
	return 0;
}

int aml_mmc_execute_tuning_v3(struct mmc_host *mmc, u32 opcode)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct aml_tuning_data tuning_data;
	int err = -EINVAL;
	u32 adj_win_start = 100;
	u32 intf3;

	if (opcode == MMC_SEND_TUNING_BLOCK_HS200) {
		if (mmc->ios.bus_width == MMC_BUS_WIDTH_8) {
			tuning_data.blk_pattern = tuning_blk_pattern_8bit;
			tuning_data.blksz = sizeof(tuning_blk_pattern_8bit);
		} else if (mmc->ios.bus_width == MMC_BUS_WIDTH_4) {
			tuning_data.blk_pattern = tuning_blk_pattern_4bit;
			tuning_data.blksz = sizeof(tuning_blk_pattern_4bit);
		} else {
			return -EINVAL;
		}
	} else if (opcode == MMC_SEND_TUNING_BLOCK) {
		tuning_data.blk_pattern = tuning_blk_pattern_4bit;
		tuning_data.blksz = sizeof(tuning_blk_pattern_4bit);
	} else {
		pr_err("Undefined command(%d) for tuning\n", opcode);
		return -EINVAL;
	}

	if (aml_card_type_sdio(pdata)) {
		if (host->data->chip_type >= MMC_CHIP_TXLX)
			err = _aml_sd_emmc_execute_tuning(mmc, opcode,
					&tuning_data, adj_win_start);
		else {
			intf3 = readl(host->base + SD_EMMC_INTF3);
			intf3 |= (1<<22);
			writel(intf3, (host->base + SD_EMMC_INTF3));
			pdata->intf3 = intf3;
			aml_sd_emmc_clktest(mmc);
			err = aml_sdio_timing(mmc, opcode,
				&tuning_data, adj_win_start);
		}
	} else if (!(pdata->caps2 & MMC_CAP2_HS400)) {
		/*if (mmc->actual_clock >= 200000000) {
		 *	intf3 = readl(host->base + SD_EMMC_INTF3);
		 *	intf3 |= (1<<22);
		 *	writel(intf3, (host->base + SD_EMMC_INTF3));
		 *	pdata->intf3 = intf3;
		 *	err = aml_emmc_hs200_timming(mmc);
		 *} else
		 */
		err = _aml_sd_emmc_execute_tuning(mmc, opcode,
					&tuning_data, adj_win_start);
	} else {
		intf3 = readl(host->base + SD_EMMC_INTF3);
		intf3 |= (1<<22);
		writel(intf3, (host->base + SD_EMMC_INTF3));
		pdata->intf3 = intf3;
		if ((host->data->chip_type == MMC_CHIP_TL1)
			|| (host->data->chip_type == MMC_CHIP_G12B))
			aml_emmc_hs200_tl1(mmc);
		err = 0;
	}

	pr_debug("%s: gclock=0x%x, gdelay1=0x%x, gdelay2=0x%x,intf3=0x%x\n",
		mmc_hostname(mmc), readl(host->base + SD_EMMC_CLOCK_V3),
		readl(host->base + SD_EMMC_DELAY1_V3),
		readl(host->base + SD_EMMC_DELAY2_V3),
		readl(host->base + SD_EMMC_INTF3));
	return err;
}
int aml_post_hs400_timming(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	aml_sd_emmc_clktest(mmc);
	if (host->data->chip_type == MMC_CHIP_TL1)
		aml_emmc_hs400_tl1(mmc);
	else if (host->data->chip_type == MMC_CHIP_G12B)
		aml_emmc_hs400_Revb(mmc);
	else
		aml_emmc_hs400_general(mmc);
	return 0;
}

ssize_t emmc_eyetest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct amlsd_host *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->mmc;

	mmc_claim_host(mmc);
	update_all_line_eyetest(mmc);
	mmc_release_host(mmc);
	return sprintf(buf, "%s\n", "Emmc all lines eyetest.\n");
}

ssize_t emmc_clktest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct amlsd_host *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->mmc;
	u32 intf3 = readl(host->base + SD_EMMC_INTF3);
	struct intf3 *gintf3 = (struct intf3 *)&(intf3);
	u32 clktest = 0, delay_cell = 0, clktest_log = 0, count = 0;
	u32 vcfg = readl(host->base + SD_EMMC_CFG);
	int i = 0;
	unsigned int cycle = 0;

	writel(0, (host->base + SD_EMMC_ADJUST_V3));

	/* one cycle = xxx(ps) */
	cycle = (1000000000 / mmc->actual_clock) * 1000;
	vcfg &= ~(1 << 23);
	writel(vcfg, host->base + SD_EMMC_CFG);
	gintf3->clktest_exp = 8;
	gintf3->clktest_on_m = 1;
	writel(intf3, host->base + SD_EMMC_INTF3);
	clktest_log = readl(host->base + SD_EMMC_CLKTEST_LOG);
	clktest = readl(host->base + SD_EMMC_CLKTEST_OUT);
	while (!(clktest_log & 0x80000000)) {
		udelay(1);
		i++;
		clktest_log = readl(host->base + SD_EMMC_CLKTEST_LOG);
		clktest = readl(host->base + SD_EMMC_CLKTEST_OUT);
		if (i > 4000) {
			pr_warn("[%s] [%d] emmc clktest error\n",
				__func__, __LINE__);
			break;
		}
	}
	if (clktest_log & 0x80000000) {
		clktest = readl(host->base + SD_EMMC_CLKTEST_OUT);
		count = clktest / (1 << 8);
		if (vcfg & 0x4)
			delay_cell = ((cycle / 2) / count);
		else
			delay_cell = (cycle / count);
	}
	pr_info("%s [%d] clktest : %u, delay_cell: %d, count: %u\n",
			__func__, __LINE__, clktest, delay_cell, count);
	intf3 = readl(host->base + SD_EMMC_INTF3);
	gintf3->clktest_on_m = 0;
	writel(intf3, host->base + SD_EMMC_INTF3);
	vcfg = readl(host->base + SD_EMMC_CFG);
	vcfg |= (1 << 23);
	writel(vcfg, host->base + SD_EMMC_CFG);
	return count;
}
