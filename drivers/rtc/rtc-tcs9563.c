/*
 * An I2C driver for the Torch-Chip TCS9563 RTC
 * Copyright 2005-06 Tower Technologies
 *
 *
 * based on the other drivers in this same directory.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk-provider.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pm_wakeup.h>


#define RTC_DRIVER_NAME		"rtc_tcs9563"

#define TCS9563_REG_ST1		0x00 /* status */
#define TCS9563_REG_ST2		0x01
#define TCS9563_BIT_TIE		(1 << 0)
#define TCS9563_BIT_AIE		(1 << 1)
#define TCS9563_BIT_TF		(1 << 2)
#define TCS9563_BIT_AF		(1 << 3)
#define TCS9563_BIT_TI_TP	(1 << 4)
#define TCS9563_BITS_ST2_N	(7 << 5)

#define TCS9563_REG_SC		0x02 /* datetime */
#define TCS9563_REG_MN		0x03
#define TCS9563_REG_HR		0x04
#define TCS9563_REG_DM		0x05
#define TCS9563_REG_DW		0x06
#define TCS9563_REG_MO		0x07
#define TCS9563_REG_YR		0x08

#define TCS9563_REG_AMN		0x09 /* alarm */

#define TCS9563_REG_CLKO		0x0D /* clock out */
#define TCS9563_REG_CLKO_FE		0x80 /* clock out enabled */
#define TCS9563_REG_CLKO_F_MASK		0x03 /* frequenc mask */
#define TCS9563_REG_CLKO_F_32768HZ	0x00
#define TCS9563_REG_CLKO_F_1024HZ	0x01
#define TCS9563_REG_CLKO_F_32HZ		0x02
#define TCS9563_REG_CLKO_F_1HZ		0x03

#define TCS9563_REG_TMRC	0x0E /* timer control */
#define TCS9563_TMRC_ENABLE	BIT(7)
#define TCS9563_TMRC_4096	0
#define TCS9563_TMRC_64		1
#define TCS9563_TMRC_1		2
#define TCS9563_TMRC_1_60	3
#define TCS9563_TMRC_MASK	3

#define TCS9563_REG_TMR		0x0F /* timer */

#define TCS9563_SC_LV		0x80 /* low voltage */
#define TCS9563_MO_C		0x80 /* century */

static struct i2c_driver tcs9563_driver;
static void __iomem *alarm_reg_vaddr;
static struct i2c_client *g_client;

struct tcs9563 {
	struct rtc_device *rtc;
	/*
	 * The meaning of MO_C bit varies by the chip type.
	 * From TCS9563 datasheet: this bit is toggled when the years
	 * register overflows from 99 to 00
	 *   0 indicates the century is 20xx
	 *   1 indicates the century is 19xx
	 */
	int c_polarity;	/* 0: MO_C=1 means 19xx, otherwise MO_C=1 means 20xx */
	int voltage_low; /* incicates if a low_voltage was detected */

	struct i2c_client *client;
#ifdef CONFIG_COMMON_CLK
	struct clk_hw		clkout_hw;
#endif

	int irq_gpio;
	enum of_gpio_flags irq_gpio_flags;
	struct pinctrl *pin_ctl;
};

static int tcs9563_read_block_data(struct i2c_client *client, unsigned char reg,
				   unsigned char length, unsigned char *buf)
{
	struct i2c_msg msgs[] = {
		{/* setup read ptr */
			.addr = client->addr,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = buf
		},
	};

	if ((i2c_transfer(client->adapter, msgs, 2)) != 2) {
		dev_err(&client->dev, "%s: read error\n", __func__);
		return -EIO;
	}

	return 0;
}

static int tcs9563_write_block_data(struct i2c_client *client,
				   unsigned char reg, unsigned char length,
				   unsigned char *buf)
{
	int i, err;

	for (i = 0; i < length; i++) {
		unsigned char data[2] = { reg + i, buf[i] };

		err = i2c_master_send(client, data, sizeof(data));
		if (err != sizeof(data)) {
			dev_err(&client->dev,
				"%s: err=%d addr=%02x, data=%02x\n",
				__func__, err, data[0], data[1]);
			return -EIO;
		}
	}

	return 0;
}


static int tcs9563_set_alarm_mode(struct i2c_client *client, bool on)
{
	unsigned char buf;
	int err;

	err = tcs9563_read_block_data(client, TCS9563_REG_ST2, 1, &buf);
	if (err < 0)
		return err;

	if (on)
		buf |= TCS9563_BIT_AIE;
	else
		buf &= ~TCS9563_BIT_AIE;

	buf &= ~(TCS9563_BIT_AF | TCS9563_BITS_ST2_N);

	err = tcs9563_write_block_data(client, TCS9563_REG_ST2, 1, &buf);
	if (err < 0) {
		dev_err(&client->dev, "%s: write error\n", __func__);
		return -EIO;
	}

	return 0;
}

static int set_wakeup_time(unsigned long time)
{
	int ret = -1;

	if (alarm_reg_vaddr) {
		writel(time, alarm_reg_vaddr);
		ret = 0;
		printk("set_wakeup_time: %lu\n", time);
	}
	return ret;
}

static int tcs9563_get_alarm_mode(struct i2c_client *client, unsigned char *en,
				  unsigned char *pen)
{
	unsigned char buf;
	int err;

	err = tcs9563_read_block_data(client, TCS9563_REG_ST2, 1, &buf);
	if (err)
		return err;

	if (en)
		*en = !!(buf & TCS9563_BIT_AIE);
	if (pen)
		*pen = !!(buf & TCS9563_BIT_AF);

	return 0;
}

static irqreturn_t tcs9563_irq(int irq, void *dev_id)
{
	struct tcs9563 *tcs9563 = i2c_get_clientdata(dev_id);
	int err;
	char pending;

	err = tcs9563_get_alarm_mode(tcs9563->client, NULL, &pending);
	if (err)
		return IRQ_NONE;

	if (pending) {
		rtc_update_irq(tcs9563->rtc, 1, RTC_IRQF | RTC_AF);
		tcs9563_set_alarm_mode(tcs9563->client, 1);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

/*
 * In the routines that deal directly with the tcs9563 hardware, we use
 * rtc_time -- month 0-11, hour 0-23, yr = calendar year-epoch.
 */
static int tcs9563_get_datetime(struct i2c_client *client, struct rtc_time *tm)
{
	struct tcs9563 *tcs9563 = i2c_get_clientdata(client);
	unsigned char buf[9];
	int err;

	err = tcs9563_read_block_data(client, TCS9563_REG_ST1, 9, buf);
	if (err)
		return err;
/*
	if (buf[TCS9563_REG_SC] & TCS9563_SC_LV) {
		tcs9563->voltage_low = 1;
		dev_err(&client->dev,
			"low voltage detected, date/time is not reliable.\n");
		return -EINVAL;
	}
*/
	printk(
		"%s: raw data is st1=%02x, st2=%02x, sec=%02x, min=%02x, hr=%02x, "
		"mday=%02x, wday=%02x, mon=%02x, year=%02x\n",
		__func__,
		buf[0], buf[1], buf[2], buf[3],
		buf[4], buf[5], buf[6], buf[7],
		buf[8]);


	tm->tm_sec = bcd2bin(buf[TCS9563_REG_SC] & 0x7F);
	tm->tm_min = bcd2bin(buf[TCS9563_REG_MN] & 0x7F);
	tm->tm_hour = bcd2bin(buf[TCS9563_REG_HR] & 0x3F); /* rtc hr 0-23 */
	tm->tm_mday = bcd2bin(buf[TCS9563_REG_DM] & 0x3F);
	tm->tm_wday = buf[TCS9563_REG_DW] & 0x07;
	tm->tm_mon = bcd2bin(buf[TCS9563_REG_MO] & 0x1F) - 1; /* rtc mn 1-12 */
	tm->tm_year = bcd2bin(buf[TCS9563_REG_YR]);
	if (tm->tm_year < 70)
		tm->tm_year += 100;	/* assume we are in 1970...2069 */
	/* detect the polarity heuristically. see note above. */
	tcs9563->c_polarity = (buf[TCS9563_REG_MO] & TCS9563_MO_C) ?
		(tm->tm_year >= 100) : (tm->tm_year < 100);

	printk( "%s: tm is secs=%d, mins=%d, hours=%d, "
		"mday=%d, mon=%d, year=%d, wday=%d\n",
		__func__,
		tm->tm_sec, tm->tm_min, tm->tm_hour,
		tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_wday);
	printk("%s: Current RTC date/time is %d-%d-%d, %02d:%02d:%02d.\n", __func__,
		 tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900,
		 tm->tm_hour, tm->tm_min, tm->tm_sec);

	{
		char pending, irq_af;

		tcs9563_get_alarm_mode(client, &irq_af, &pending);
		
		printk("pending= %d irq_af=%d\n", pending, irq_af);
		
	}

	return 0;
}

static int tcs9563_set_datetime(struct i2c_client *client, struct rtc_time *tm)
{
	struct tcs9563 *tcs9563 = i2c_get_clientdata(client);
	unsigned char buf[9];

	printk("%s: secs=%d, mins=%d, hours=%d, "
		"mday=%d, mon=%d, year=%d, wday=%d\n",
		__func__,
		tm->tm_sec, tm->tm_min, tm->tm_hour,
		tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_wday);
	printk("%s: Set RTC date/time is %d-%d-%d, %02d:%02d:%02d.\n", __func__,
		 tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900,
		 tm->tm_hour, tm->tm_min, tm->tm_sec);

	/* hours, minutes and seconds */
	buf[TCS9563_REG_SC] = bin2bcd(tm->tm_sec);
	buf[TCS9563_REG_MN] = bin2bcd(tm->tm_min);
	buf[TCS9563_REG_HR] = bin2bcd(tm->tm_hour);

	buf[TCS9563_REG_DM] = bin2bcd(tm->tm_mday);

	/* month, 1 - 12 */
	buf[TCS9563_REG_MO] = bin2bcd(tm->tm_mon + 1);

	/* year and century */
	buf[TCS9563_REG_YR] = bin2bcd(tm->tm_year % 100);
	if (tcs9563->c_polarity ? (tm->tm_year >= 100) : (tm->tm_year < 100))
		buf[TCS9563_REG_MO] |= TCS9563_MO_C;

	buf[TCS9563_REG_DW] = tm->tm_wday & 0x07;

	return tcs9563_write_block_data(client, TCS9563_REG_SC,
				9 - TCS9563_REG_SC, buf + TCS9563_REG_SC);
}

static int tcs9563_time_calibration(struct i2c_client *client)
{

	int err;
	unsigned char buf[9], tmp;	
	struct rtc_time tm;

	//pr_info("%s: Time calibration\n", __func__);
	err = tcs9563_read_block_data(client, TCS9563_REG_ST1, 9, buf);
	if (err) {
		pr_err("%s: unable to get times\n", __func__);
		return err;
	}

	/* date/time is not reliable */
	if (buf[TCS9563_REG_SC] & TCS9563_SC_LV) {
		tmp = buf[TCS9563_REG_SC] & 0x7F;
		tcs9563_write_block_data(client, TCS9563_REG_SC, 1, &tmp);
	}


	tm.tm_sec = bcd2bin(buf[TCS9563_REG_SC] & 0x7F);
	tm.tm_min = bcd2bin(buf[TCS9563_REG_MN] & 0x7F);
	tm.tm_hour = bcd2bin(buf[TCS9563_REG_HR] & 0x3F); /* rtc hr 0-23 */
	tm.tm_mday = bcd2bin(buf[TCS9563_REG_DM] & 0x3F);
	tm.tm_wday = buf[TCS9563_REG_DW] & 0x07;
	tm.tm_mon = bcd2bin(buf[TCS9563_REG_MO] & 0x1F) - 1; /* rtc mn 1-12 */
	tm.tm_year = bcd2bin(buf[TCS9563_REG_YR]);
	if (tm.tm_year < 70)
		tm.tm_year += 100;	/* assume we are in 1970...2069 */


	if ((rtc_valid_tm(&tm) < 0) || (tm.tm_year < (2018 - 1900))) {
		tm.tm_sec = 0;
		tm.tm_min = 0;
		tm.tm_hour = 0;
		tm.tm_mday = 2;
		tm.tm_wday = 5;
		tm.tm_mon = 2;
		tm.tm_year = 2018 - 1900;

		err = tcs9563_set_datetime(client, &tm);
		if (err) {
			pr_err("%s: set time error\n", __func__);
			return err;
		}
	}

	return 0;
}

#ifdef CONFIG_RTC_INTF_DEV
static int tcs9563_rtc_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	struct tcs9563 *tcs9563 = i2c_get_clientdata(to_i2c_client(dev));
	struct rtc_time tm;
	
	switch (cmd) {
	case RTC_VL_READ:
		if (tcs9563->voltage_low)
			dev_info(dev, "low voltage detected, date/time is not reliable.\n");

		if (copy_to_user((void __user *)arg, &tcs9563->voltage_low,
					sizeof(int)))
			return -EFAULT;
		return 0;
	case RTC_VL_CLR:
		/*
		 * Clear the VL bit in the seconds register in case
		 * the time has not been set already (which would
		 * have cleared it). This does not really matter
		 * because of the cached voltage_low value but do it
		 * anyway for consistency.
		 */
		if (tcs9563_get_datetime(to_i2c_client(dev), &tm))
			tcs9563_set_datetime(to_i2c_client(dev), &tm);

		/* Clear the cached value. */
		tcs9563->voltage_low = 0;

		return 0;
	default:
		printk("this  %s, method return -ENOIOCTLCMD", __func__);
		return -ENOIOCTLCMD;
	}
}
#else
#define tcs9563_rtc_ioctl NULL
#endif


static int tcs9563_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	return tcs9563_get_datetime(to_i2c_client(dev), tm);
}

static int tcs9563_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	return tcs9563_set_datetime(to_i2c_client(dev), tm);
}

static int tcs9563_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char buf[4];
	int err;

	err = tcs9563_read_block_data(client, TCS9563_REG_AMN, 4, buf);
	if (err)
		return err;

	printk(
		"%s: raw data is min=%02x, hr=%02x, mday=%02x, wday=%02x\n",
		__func__, buf[0], buf[1], buf[2], buf[3]);

	tm->time.tm_sec = 0;
	tm->time.tm_min = bcd2bin(buf[0] & 0x7F);
	tm->time.tm_hour = bcd2bin(buf[1] & 0x3F);
	tm->time.tm_mday = bcd2bin(buf[2] & 0x3F);
	tm->time.tm_wday = bcd2bin(buf[3] & 0x7);

	err = tcs9563_get_alarm_mode(client, &tm->enabled, &tm->pending);
	if (err < 0)
		return err;

	printk( "%s: tm is mins=%d, hours=%d, mday=%d, wday=%d,"
		" enabled=%d, pending=%d\n", __func__, tm->time.tm_min,
		tm->time.tm_hour, tm->time.tm_mday, tm->time.tm_wday,
		tm->enabled, tm->pending);
	printk("%s: Current RTC alarm/time is %d-%d-%d, %02d:%02d:%02d.\n", __func__,
		 tm->time.tm_mday, tm->time.tm_mon + 1, tm->time.tm_year + 1900,
		 tm->time.tm_hour, tm->time.tm_min, tm->time.tm_sec);

	return 0;
}


static int tcs9563_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char buf[4];
	int err;

	/* The alarm has no seconds, round up to nearest minute */
	if (tm->time.tm_sec) {
		time64_t alarm_time = rtc_tm_to_time64(&tm->time);

		alarm_time += 60 - tm->time.tm_sec;
		rtc_time64_to_tm(alarm_time, &tm->time);
	}

	//tm->enable
	{
		struct rtc_time cur_tm;
		time64_t cur_time;
		time64_t alarm_time = rtc_tm_to_time64(&tm->time);
		tcs9563_rtc_read_time(dev, &cur_tm);
		cur_time = rtc_tm_to_time64(&cur_tm);
		if (alarm_time >= cur_time) {
			alarm_time = alarm_time - cur_time;
			set_wakeup_time(alarm_time);
		}
	}

	printk("%s, min=%d hour=%d wday=%d mday=%d "
		"enabled=%d pending=%d\n", __func__,
		tm->time.tm_min, tm->time.tm_hour, tm->time.tm_wday,
		tm->time.tm_mday, tm->enabled, tm->pending);

	printk("%s: Set RTC alarm/time is %d-%d-%d, %02d:%02d:%02d.\n", __func__,
		 tm->time.tm_mday, tm->time.tm_mon + 1, tm->time.tm_year + 1900,
		 tm->time.tm_hour, tm->time.tm_min, tm->time.tm_sec);
	
	buf[0] = bin2bcd(tm->time.tm_min);
	buf[1] = bin2bcd(tm->time.tm_hour);
	buf[2] = bin2bcd(tm->time.tm_mday);
	buf[3] = tm->time.tm_wday & 0x07;

	err = tcs9563_write_block_data(client, TCS9563_REG_AMN, 4, buf);
	if (err)
		return err;
	printk("L%d %s\n",  __LINE__, __func__);
	return tcs9563_set_alarm_mode(client, 1);
}

static int tcs9563_irq_enable(struct device *dev, unsigned int enabled)
{
	dev_dbg(dev, "%s: en=%d\n", __func__, enabled);
	return tcs9563_set_alarm_mode(to_i2c_client(dev), !!enabled);
}

#ifdef CONFIG_COMMON_CLK
/*
 * Handling of the clkout
 */

#define clkout_hw_to_tcs9563(_hw) container_of(_hw, struct tcs9563, clkout_hw)

static int clkout_rates[] = {
	32768,
	1024,
	32,
	1,
};

static unsigned long tcs9563_clkout_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct tcs9563 *tcs9563 = clkout_hw_to_tcs9563(hw);
	struct i2c_client *client = tcs9563->client;
	unsigned char buf;
	int ret = tcs9563_read_block_data(client, TCS9563_REG_CLKO, 1, &buf);

	if (ret < 0)
		return 0;

	buf &= TCS9563_REG_CLKO_F_MASK;
	return clkout_rates[ret];
}

static long tcs9563_clkout_round_rate(struct clk_hw *hw, unsigned long rate,
				      unsigned long *prate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(clkout_rates); i++)
		if (clkout_rates[i] <= rate)
			return clkout_rates[i];

	return 0;
}

static int tcs9563_clkout_set_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long parent_rate)
{
	struct tcs9563 *tcs9563 = clkout_hw_to_tcs9563(hw);
	struct i2c_client *client = tcs9563->client;
	unsigned char buf;
	int ret = tcs9563_read_block_data(client, TCS9563_REG_CLKO, 1, &buf);
	int i;

	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(clkout_rates); i++)
		if (clkout_rates[i] == rate) {
			buf &= ~TCS9563_REG_CLKO_F_MASK;
			buf |= i;
			ret = tcs9563_write_block_data(client,
						       TCS9563_REG_CLKO, 1,
						       &buf);
			return ret;
		}

	return -EINVAL;
}

static int tcs9563_clkout_control(struct clk_hw *hw, bool enable)
{
	struct tcs9563 *tcs9563 = clkout_hw_to_tcs9563(hw);
	struct i2c_client *client = tcs9563->client;
	unsigned char buf;
	int ret = tcs9563_read_block_data(client, TCS9563_REG_CLKO, 1, &buf);

	if (ret < 0)
		return ret;

	if (enable)
		buf |= TCS9563_REG_CLKO_FE;
	else
		buf &= ~TCS9563_REG_CLKO_FE;

	ret = tcs9563_write_block_data(client, TCS9563_REG_CLKO, 1, &buf);
	return ret;
}

static int tcs9563_clkout_prepare(struct clk_hw *hw)
{
	return tcs9563_clkout_control(hw, 1);
}

static void tcs9563_clkout_unprepare(struct clk_hw *hw)
{
	tcs9563_clkout_control(hw, 0);
}

static int tcs9563_clkout_is_prepared(struct clk_hw *hw)
{
	struct tcs9563 *tcs9563 = clkout_hw_to_tcs9563(hw);
	struct i2c_client *client = tcs9563->client;
	unsigned char buf;
	int ret = tcs9563_read_block_data(client, TCS9563_REG_CLKO, 1, &buf);

	if (ret < 0)
		return ret;

	return !!(buf & TCS9563_REG_CLKO_FE);
}

static const struct clk_ops tcs9563_clkout_ops = {
	.prepare = tcs9563_clkout_prepare,
	.unprepare = tcs9563_clkout_unprepare,
	.is_prepared = tcs9563_clkout_is_prepared,
	.recalc_rate = tcs9563_clkout_recalc_rate,
	.round_rate = tcs9563_clkout_round_rate,
	.set_rate = tcs9563_clkout_set_rate,
};

static struct clk *tcs9563_clkout_register_clk(struct tcs9563 *tcs9563)
{
	struct i2c_client *client = tcs9563->client;
	struct device_node *node = client->dev.of_node;
	struct clk *clk;
	struct clk_init_data init;
	int ret;
	unsigned char buf;

	/* disable the clkout output */
	buf = 0;
	ret = tcs9563_write_block_data(client, TCS9563_REG_CLKO, 1, &buf);
	if (ret < 0)
		return ERR_PTR(ret);

	init.name = "tcs9563-clkout";
	init.ops = &tcs9563_clkout_ops;
	init.flags = 0;
	init.parent_names = NULL;
	init.num_parents = 0;
	tcs9563->clkout_hw.init = &init;

	/* optional override of the clockname */
	of_property_read_string(node, "clock-output-names", &init.name);

	/* register the clock */
	clk = devm_clk_register(&client->dev, &tcs9563->clkout_hw);

	if (!IS_ERR(clk))
		of_clk_add_provider(node, of_clk_src_simple_get, clk);

	return clk;
}
#endif

static const struct rtc_class_ops tcs9563_rtc_ops = {
	.ioctl		= tcs9563_rtc_ioctl,
	.read_time	= tcs9563_rtc_read_time,
	.set_time	= tcs9563_rtc_set_time,
	.read_alarm	= tcs9563_rtc_read_alarm,
	.set_alarm	= tcs9563_rtc_set_alarm,
	.alarm_irq_enable = tcs9563_irq_enable,
};

#if !defined(CONFIG_FB) && defined(CONFIG_PM)
/**
 * gt1x_ts_suspend - i2c suspend callback function.
 * @dev: i2c device.
 * Return  0: succeed, -1: failed.
 */
static int tcs9563_suspend(struct device *dev)
{
	printk("L%d %s\n", __LINE__, __func__);
    return 0;
}

/**
 * gt1x_ts_resume - i2c resume callback function.
 * @dev: i2c device.
 * Return  0: succeed, -1: failed.
 */
static int tcs9563_resume(struct device *dev)
{
	int err;
	unsigned char alm_pending;
	struct i2c_client *client = to_i2c_client(dev);

	printk("L%d %s\n", __LINE__, __func__);
	set_wakeup_time(0);

	err = tcs9563_get_alarm_mode(client, NULL, &alm_pending);
	if (err) {
		dev_err(&client->dev, "%s: read error\n", __func__);
		return err;
	}

	if (alm_pending)
		tcs9563_set_alarm_mode(client, 0);
	return 0;
}

static const struct dev_pm_ops tcs9563_ts_pm_ops = {
	.suspend = tcs9563_suspend,
	.resume = tcs9563_resume,
};

#elif defined(CONFIG_HAS_EARLYSUSPEND)
/* earlysuspend module the suspend/resume procedure */
static void tcs9563_early_suspend(struct early_suspend *h)
{
	printk("L%d %s\n", __LINE__, __func__);
}

static void tcs9563_late_resume(struct early_suspend *h)
{
	int err;
	unsigned char alm_pending;

	printk("L%d %s\n", __LINE__, __func__);
	set_wakeup_time(0);

	err = tcs9563_get_alarm_mode(g_client, NULL, &alm_pending);
	if (err) {
		dev_err(&g_client->dev, "%s: read error\n", __func__);
		return err;
	}

	if (alm_pending)
		tcs9563_set_alarm_mode(g_client, 0);
}

static struct early_suspend tcs9563_early_suspend = {
	.suspend = tcs9563_early_suspend,
	.resume =  tcs9563_late_resume,
};
#endif

static int tcs9563_init_device(struct i2c_client *client)
{
	int err;
	unsigned char buf = 0;

	/* Clear stop flag if present */
	err = tcs9563_write_block_data(client, TCS9563_REG_ST1, 1, &buf);
	if (err < 0)
		return err;

	err = tcs9563_read_block_data(client, TCS9563_REG_ST2, 1, &buf);
	if (err < 0)
		return err;

	/* Disable alarm and timer interrupts */
	buf &= ~TCS9563_BIT_AIE;
	buf &= ~TCS9563_BIT_TIE;

	/* Clear any pending alarm and timer flags */
	if (buf & TCS9563_BIT_AF)
		buf &= ~TCS9563_BIT_AF;

	if (buf & TCS9563_BIT_TF)
		buf &= ~TCS9563_BIT_TF;

	buf &= ~TCS9563_BIT_TI_TP;

	return tcs9563_write_block_data(client, TCS9563_REG_ST2, 1, &buf);
}

static int tcs9563_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct tcs9563 *tcs9563;
	int err;
	unsigned char buf;
	unsigned char alm_pending;
	u32 paddr = 0;
	int ret;

	struct device_node *np;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_info(&client->dev, "%s: I2C Check Failed\n", __func__);
		return -ENODEV;
	}

	tcs9563 = devm_kzalloc(&client->dev, sizeof(struct tcs9563), GFP_KERNEL);
	if (!tcs9563) {
		dev_info(&client->dev, "%s: unable to malloc memory\n", __func__);
		return -ENOMEM;
	}

	np = client->dev.of_node;

	ret = of_property_read_u32(np, "alarm_reg_addr", &paddr);
	if (!ret) {
		pr_debug("alarm_reg_paddr: 0x%x\n", paddr);
		alarm_reg_vaddr = ioremap(paddr, 0x4);
	}

	client->irq = 0;
	/* irq gpio info */
	tcs9563->irq_gpio = of_get_named_gpio_flags(np, "irq-gpio", 0, &tcs9563->irq_gpio_flags);
	if (tcs9563->irq_gpio < 0) {
		dev_warn(&client->dev, "%s: unable to get irq_gpio\n", __func__);
		printk("unable to get irq_gpio\n");
	}
	else
	{
		printk("get irq_gpio succeed\n");
		if (gpio_is_valid(tcs9563->irq_gpio)) {
			tcs9563->pin_ctl = devm_pinctrl_get_select(&client->dev, "irq_rtc");
			if (IS_ERR(tcs9563->pin_ctl)) {
				printk("RTC GPO pinctrl config error! Turn to request gpio\n");

				if (gpio_request(tcs9563->irq_gpio, "tcs9563_irq"))
				{
					dev_err(&client->dev, "%s: [GPIO]irq gpio request failed\n", __func__);
					goto err_get_irq_gpio;
				}

				if (gpio_direction_input(tcs9563->irq_gpio)) 
				{
					dev_err(&client->dev, "%s: [GPIO]set_direction for irq gpio failed\n", __func__);
					goto err_irq_gpio_dir;
				}
			}
			client->irq = gpio_to_irq(tcs9563->irq_gpio);
		}
		else {
			dev_warn(&client->dev, "%s: [GPIO]irq is not valid\n", __func__);
		}
	}

	i2c_set_clientdata(client, tcs9563);
	tcs9563->client = client;
	device_set_wakeup_capable(&client->dev, 1);
	//device_init_wakeup(&client->dev, 1);

	ret = tcs9563_init_device(client);
	if (ret) {
		dev_err(&client->dev, "could not init device, %d\n", ret);
		return ret;
	}

	/* Set timer to lowest frequency to save power (ref Haoyu datasheet) */
	buf = TCS9563_TMRC_1_60;
	err = tcs9563_write_block_data(client, TCS9563_REG_TMRC, 1, &buf);
	if (err < 0) {
		dev_err(&client->dev, "%s: write error\n", __func__);
		return err;
	}

	err = tcs9563_get_alarm_mode(client, NULL, &alm_pending);
	if (err) {
		dev_err(&client->dev, "%s: read error\n", __func__);
		return err;
	}

	if (alm_pending)
		tcs9563_set_alarm_mode(client, 0);

	err = tcs9563_time_calibration(client);
	if (err) {
		dev_err(&client->dev, "%s: Time calibration Failed\n", __func__);
		//return err;
		
	}

	tcs9563->rtc = devm_rtc_device_register(&client->dev,
				tcs9563_driver.driver.name,
				&tcs9563_rtc_ops, THIS_MODULE);

	if (IS_ERR(tcs9563->rtc))
		return PTR_ERR(tcs9563->rtc);

	if (client->irq > 0) {
		err = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, tcs9563_irq,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				tcs9563->client->name, client);
/*
		err = request_threaded_irq(client->irq, NULL, tcs9563_irq,
                               tcs9563->irq_gpio_flags | IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
                               tcs9563->rtc->name, client);
*/
		if (err) {
			dev_info(&client->dev, "unable to request IRQ %d\n",
								client->irq);
			return err;
		}

	}
	else {
		dev_info(&client->dev, "%s:  Not to request IRQ\n", __func__);
	}


#ifdef CONFIG_COMMON_CLK
	/* register clk in common clk framework */
	tcs9563_clkout_register_clk(tcs9563);
#endif

	/* the tcs9563 alarm only supports a minute accuracy */
	tcs9563->rtc->uie_unsupported = 1;

	g_client = client;
	#if defined(CONFIG_HAS_EARLYSUSPEND)

		register_early_suspend(&tcs9563_early_suspend);
	#endif

	return 0;

err_irq_gpio_dir:
    if (gpio_is_valid(tcs9563->irq_gpio))
        gpio_free(tcs9563->irq_gpio);	

err_get_irq_gpio:
	kfree(tcs9563);
	
	return -ENOENT;

}

static int tcs9563_remove(struct i2c_client *client)
{
	struct tcs9563 *tcs9563 = i2c_get_clientdata(client);

	#if defined(CONFIG_HAS_EARLYSUSPEND)
		unregister_early_suspend(&tcs9563_early_suspend);
	#endif

	if (gpio_is_valid(tcs9563->irq_gpio))
        gpio_free(tcs9563->irq_gpio);	


	kfree(tcs9563);
	return 0;
}

static const struct i2c_device_id tcs9563_id[] = {
	{ RTC_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tcs9563_id);


#ifdef CONFIG_OF
static const struct of_device_id tcs9563_of_match[] = {
	{ .compatible = "torch_chip,tcs9563" },
	{}
};
MODULE_DEVICE_TABLE(of, tcs9563_of_match);
#endif

static struct i2c_driver tcs9563_driver = {
	.driver		= {
		.name	= RTC_DRIVER_NAME,
		.of_match_table = of_match_ptr(tcs9563_of_match),
		#if !defined(CONFIG_FB) && defined(CONFIG_PM)
		.pm = &tcs9563_ts_pm_ops,
		#endif
	},
	.probe		= tcs9563_probe,
	.remove 	= tcs9563_remove,
	.id_table	= tcs9563_id,
};

module_i2c_driver(tcs9563_driver);

MODULE_AUTHOR("Yekertech <software@yekertech.com>");
MODULE_DESCRIPTION("Torch-Chip TCS9563 RTC driver");
MODULE_LICENSE("GPL");
