/*
 * drivers/amlogic/pwm/pwm_meson_v2.c
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/amlogic/pwm_meson.h>

static int meson_pwm_v2_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct meson_pwm_channel *channel = pwm_get_chip_data(pwm);
	int err;

	if (!channel)
		return -ENODEV;

	err = clk_set_rate(channel->clk_ext, DEFAULT_EXTERN_CLK);
	if (err) {
		pr_err("%s: error in setting pwm rate!\n", __func__);
		return err;
	}

	err = clk_prepare_enable(channel->clk_ext);
	if (err)
		pr_err("Failed to enable pwm clock\n");

	chip->ops->get_state(chip, pwm, &channel->state);

	return 0;
}

static void meson_pwm_v2_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct meson_pwm_channel *channel = pwm_get_chip_data(pwm);

	if (channel)
		clk_disable_unprepare(channel->clk_ext);
}

static int meson_pwm_v2_calc(struct meson_pwm *meson,
			     struct meson_pwm_channel *channel,
			     unsigned int id, unsigned int duty,
			     unsigned int period)
{
	unsigned int pre_div, cnt, duty_cnt;
	unsigned long fin_freq = -1;
	u64 fin_ps;

	if (~(meson->inverter_mask >> id) & 0x1)
		duty = period - duty;

	/* 1. Remove unnecessary logic.
	 * 2. When the polarity is 1, it may cause false early return.
	 * For example the following:
	 * duty_cycle = 0 period = 5555555 enabled = 1 polarity = 1
	 * duty_cycle = 5555555 period = 5555555 enabled = 1 polarity = 1
	 *
	 * if (period == channel->state.period &&
	 *	duty == channel->state.duty_cycle)
	 *	return 0;
	 */
	fin_freq = meson->data->default_extern_clk;
	if (fin_freq == 0) {
		dev_err(meson->chip.dev, "invalid source clock frequency\n");
		return -EINVAL;
	}

	dev_dbg(meson->chip.dev, "fin_freq: %lu Hz\n", fin_freq);
	fin_ps = (u64)NSEC_PER_SEC * 1000;
	do_div(fin_ps, fin_freq);

	/* Calc pre_div with the period */
	for (pre_div = 0; pre_div < MISC_CLK_DIV_MASK; pre_div++) {
		cnt = DIV_ROUND_CLOSEST_ULL((u64)period * 1000,
					    fin_ps * (pre_div + 1));
		dev_dbg(meson->chip.dev, "fin_ps=%llu pre_div=%u cnt=%u\n",
			fin_ps, pre_div, cnt);
		if (cnt <= 0xffff)
			break;
	}

	if (pre_div == MISC_CLK_DIV_MASK) {
		dev_err(meson->chip.dev, "unable to get period pre_div\n");
		return -EINVAL;
	}

	dev_dbg(meson->chip.dev, "period=%u pre_div=%u cnt=%u\n", period,
		pre_div, cnt);

	if (duty == period) {
		channel->pre_div = pre_div;
		channel->hi = cnt;
		channel->lo = 0;
	} else if (duty == 0) {
		channel->pre_div = pre_div;
		channel->hi = 0;
		channel->lo = cnt;
	} else {
		/* Then check is we can have the duty with the same pre_div */
		duty_cnt = DIV_ROUND_CLOSEST_ULL((u64)duty * 1000,
						 fin_ps * (pre_div + 1));
		if (duty_cnt > 0xffff) {
			dev_err(meson->chip.dev, "unable to get duty cycle\n");
			return -EINVAL;
		}

		dev_dbg(meson->chip.dev, "duty=%u pre_div=%u duty_cnt=%u\n",
			duty, pre_div, duty_cnt);

		channel->pre_div = pre_div;
		if (duty_cnt == 0) {
			cnt = (cnt < 2 ? 2 : cnt);
			channel->hi = 0;
			channel->lo = cnt - 2;
		} else if (cnt == duty_cnt) {
			duty_cnt = (duty_cnt < 2 ? 2 : duty_cnt);
			channel->hi = duty_cnt - 2;
			channel->lo = 0;
		} else {
			channel->hi = duty_cnt - 1;
			channel->lo = cnt - duty_cnt - 1;
		}
	}
	/*
	 * duty_cycle equal 0% and 100%,constant should be enabled,
	 * high and low count will not incease one;
	 * otherwise, high and low count increase one.
	 */
	if (id < 2) {
		if (duty == period || duty == 0)
			pwm_constant_enable(meson, id);
		else
			pwm_constant_disable(meson, id);
	}
	return 0;
}

static void meson_pwm_v2_enable(struct meson_pwm *meson,
				struct meson_pwm_channel *channel,
				unsigned int id)
{
	u32 value, clk_shift, clk_enable, enable;
	unsigned int offset;
	unsigned long set_clk;
	int err;
#ifdef MESON_PWM_SPINLOCK
	unsigned long flags;
#endif

	switch (id) {
	case MESON_PWM_0:
		clk_shift = MISC_A_CLK_DIV_SHIFT;
		clk_enable = MISC_A_CLK_EN;
		enable = MISC_A_EN;
		offset = REG_PWM_A;
		break;

	case MESON_PWM_1:
		clk_shift = MISC_B_CLK_DIV_SHIFT;
		clk_enable = MISC_B_CLK_EN;
		enable = MISC_B_EN;
		offset = REG_PWM_B;
		break;

	case MESON_PWM_2:
		clk_shift = MISC_A_CLK_DIV_SHIFT;
		clk_enable = MISC_A_CLK_EN;
		enable = MISC_A2_EN;
		offset = REG_PWM_A2;
		break;

	case MESON_PWM_3:
		clk_shift = MISC_B_CLK_DIV_SHIFT;
		clk_enable = MISC_B_CLK_EN;
		enable = MISC_B2_EN;
		offset = REG_PWM_B2;
		break;

	default:
		return;
	}

	set_clk = meson->data->default_extern_clk;
	if (set_clk == 0)
		dev_err(meson->chip.dev, "invalid source clock frequency\n");

	set_clk /= (channel->pre_div + 1);
	err = clk_set_rate(channel->clk_ext, set_clk);
	if (err)
		pr_err("%s: error in setting pwm rate!\n", __func__);

#ifdef MESON_PWM_SPINLOCK
	spin_lock_irqsave(&meson->lock, flags);
#endif
	value = (channel->hi << PWM_HIGH_SHIFT) | channel->lo;
	writel(value, meson->base + offset);

	value = readl(meson->base + REG_MISC_AB);
	value |= enable;
	writel(value, meson->base + REG_MISC_AB);
#ifdef MESON_PWM_SPINLOCK
	spin_unlock_irqrestore(&meson->lock, flags);
#endif
}

static void meson_pwm_v2_disable(struct meson_pwm *meson, unsigned int id)
{
	u32 value, enable;
#ifdef MESON_PWM_SPINLOCK
	unsigned long flags;
#endif

	switch (id) {
	case MESON_PWM_0:
		enable = MISC_A_EN;
		break;

	case MESON_PWM_1:
		enable = MISC_B_EN;
		break;

	case MESON_PWM_2:
		enable = MISC_A2_EN;
		break;

	case MESON_PWM_3:
		enable = MISC_B2_EN;
		break;

	default:
		return;
	}

#ifdef MESON_PWM_SPINLOCK
	spin_lock_irqsave(&meson->lock, flags);
#endif
	value = readl(meson->base + REG_MISC_AB);
	value &= ~enable;
	writel(value, meson->base + REG_MISC_AB);
#ifdef MESON_PWM_SPINLOCK
	spin_unlock_irqrestore(&meson->lock, flags);
#endif

}

static int meson_pwm_v2_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			      struct pwm_state *state)
{
	struct meson_pwm_channel *channel = pwm_get_chip_data(pwm);
	struct meson_pwm *meson = to_meson_pwm(chip);
	int err = 0;

	if (!state)
		return -EINVAL;

#ifndef MESON_PWM_SPINLOCK
	mutex_lock(&meson->lock);
#endif

	if (!state->enabled) {
		meson_pwm_v2_disable(meson, pwm->hwpwm);
		channel->state.enabled = false;

		goto unlock;
	}

	if (state->period != channel->state.period ||
	    state->duty_cycle != channel->state.duty_cycle ||
	    state->polarity != channel->state.polarity) {
		if (channel->state.enabled)
			channel->state.enabled = false;

		if (state->polarity != channel->state.polarity) {
			if (state->polarity == PWM_POLARITY_NORMAL)
				meson->inverter_mask |= BIT(pwm->hwpwm);
			else
				meson->inverter_mask &= ~BIT(pwm->hwpwm);
		}

		err = meson_pwm_v2_calc(meson, channel, pwm->hwpwm,
					state->duty_cycle, state->period);
		if (err < 0)
			goto unlock;

		channel->state.polarity = state->polarity;
		channel->state.period = state->period;
		channel->state.duty_cycle = state->duty_cycle;
	}

	if (state->enabled && !channel->state.enabled) {
		meson_pwm_v2_enable(meson, channel, pwm->hwpwm);
		channel->state.enabled = true;
	}

unlock:
#ifndef MESON_PWM_SPINLOCK
	mutex_unlock(&meson->lock);
#endif
	return err;
}

static void meson_pwm_v2_get_state(struct pwm_chip *chip,
				   struct pwm_device *pwm,
				   struct pwm_state *state)
{
	struct meson_pwm *meson = to_meson_pwm(chip);
	u32 value, mask;

	if (!state)
		return;

	switch (pwm->hwpwm) {
	case MESON_PWM_0:
		mask = MISC_A_EN;
		break;

	case MESON_PWM_1:
		mask = MISC_B_EN;
		break;

	case MESON_PWM_2:
		mask = MISC_A2_EN;
		break;

	case MESON_PWM_3:
		mask = MISC_B2_EN;

	default:
		return;
	}

	value = readl(meson->base + REG_MISC_AB);
	state->enabled = (value & mask) != 0;
}

static const struct pwm_ops meson_pwm_v2_ops = {
	.request = meson_pwm_v2_request,
	.free = meson_pwm_v2_free,
	.apply = meson_pwm_v2_apply,
	.get_state = meson_pwm_v2_get_state,
	.owner = THIS_MODULE,
};

static const struct meson_pwm_data pwm_v2_data = {
	.double_channel = true,
	.default_extern_clk = 24000000,
};

/*defer probe cause crash*/
static const struct of_device_id meson_pwm_v2_matches[] = {
	{
		.compatible = "amlogic,meson-v2-pwm",
		.data = &pwm_v2_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, meson_pwm_v2_matches);

static int meson_pwm_v2_init_channels(struct meson_pwm *meson,
				      struct meson_pwm_channel *channels)
{
	struct device *dev = meson->chip.dev;
	unsigned int i;
	char name[255];

	for (i = 0; i < (meson->chip.npwm / 2); i++) {
		snprintf(name, sizeof(name), "clkin%u", i);
		(channels + i)->clk_ext = devm_clk_get(dev, name);
		if (IS_ERR((channels + i)->clk_ext)) {
			dev_err(meson->chip.dev, "can't get device clock\n");
			return PTR_ERR((channels + i)->clk_ext);
		}
		(channels + i + 2)->clk_ext = (channels + i)->clk_ext;
	}

	return 0;
}

static void meson_pwm_v2_add_channels(struct meson_pwm *meson,
				      struct meson_pwm_channel *channels)
{
	unsigned int i;

	for (i = 0; i < meson->chip.npwm; i++)
		pwm_set_chip_data(&meson->chip.pwms[i], &channels[i]);
}

static int meson_pwm_v2_probe(struct platform_device *pdev)
{
	struct meson_pwm_channel *channels;
	struct meson_pwm *meson;
	struct resource *regs;
	int err;

	meson = devm_kzalloc(&pdev->dev, sizeof(*meson), GFP_KERNEL);
	if (!meson)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	meson->base = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(meson->base))
		return PTR_ERR(meson->base);
#ifdef MESON_PWM_SPINLOCK
	spin_lock_init(&meson->lock);
#else
	mutex_init(&meson->lock);
#endif
	spin_lock_init(&meson->pwm_lock);
	meson->chip.dev = &pdev->dev;
	meson->chip.ops = &meson_pwm_v2_ops;
	meson->chip.base = -1;
	meson->chip.of_xlate = of_pwm_xlate_with_flags;
	meson->chip.of_pwm_n_cells = 3;

	meson->data = of_device_get_match_data(&pdev->dev);
	if (meson->data->double_channel)
		meson->chip.npwm = 4;
	else
		meson->chip.npwm = 2;

	meson->inverter_mask = BIT(meson->chip.npwm) - 1;

	channels = devm_kcalloc(&pdev->dev, meson->chip.npwm,
				sizeof(*channels), GFP_KERNEL);
	if (!channels)
		return -ENOMEM;

	/* if the PWM clock is transferred to the clk tree */
	err = meson_pwm_v2_init_channels(meson, channels);
	if (err < 0)
		return err;

	err = pwmchip_add(&meson->chip);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register PWM chip: %d\n", err);
		return err;
	}

	/*for constant,blinks functions*/
	if (meson->data->double_channel)
		meson_pwm_sysfs_init(&pdev->dev);

	meson_pwm_v2_add_channels(meson, channels);

	platform_set_drvdata(pdev, meson);

	return 0;
}

static int meson_pwm_v2_remove(struct platform_device *pdev)
{
	struct meson_pwm *meson = platform_get_drvdata(pdev);

	if (meson->data->double_channel)
		meson_pwm_sysfs_exit(&pdev->dev);

	return pwmchip_remove(&meson->chip);
}

static struct platform_driver meson_pwm_v2_driver = {
	.driver = {
		.name = "meson-pwm-v2",
	},
	.probe = meson_pwm_v2_probe,
	.remove = meson_pwm_v2_remove,
};

static int __init meson_pwm_v2_init(void)
{
	const struct of_device_id *match_id;
	int ret;

	match_id = meson_pwm_v2_matches;
	meson_pwm_v2_driver.driver.of_match_table = match_id;
	ret = platform_driver_register(&meson_pwm_v2_driver);
	return ret;
}

static void __exit meson_pwm_v2_exit(void)
{
	platform_driver_unregister(&meson_pwm_v2_driver);
}
fs_initcall_sync(meson_pwm_v2_init);
module_exit(meson_pwm_v2_exit);
MODULE_DESCRIPTION("Amlogic Meson PWM Generator driver");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_LICENSE("Dual BSD/GPL");
