/*
 * Driver for Amlogic Meson IR remote receiver
 *
 * Copyright (C) 2014 Beniamino Galvani <b.galvani@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <media/rc-core.h>
#include <linux/amlogic/iomap.h>
#define DRIVER_NAME		"meson-ir"

/* valid on all Meson platforms */
#define IR_DEC_LDR_ACTIVE	0x00
#define IR_DEC_LDR_IDLE		0x04
#define IR_DEC_LDR_REPEAT	0x08
#define IR_DEC_BIT_0		0x0c
#define IR_DEC_REG0		0x10
#define IR_DEC_FRAME		0x14
#define IR_DEC_STATUS		0x18
#define IR_DEC_REG1		0x1c
/* only available on Meson 8b and newer */
#define IR_DEC_REG2		0x20
#define IR_DEC_DURATN2	0x24
#define IR_DEC_DURATN3	0x28
#define IR_DEC_REG3		0x38

#define REG0_RATE_MASK		(BIT(11) - 1)

#define DECODE_MODE_NEC		0x0
#define DECODE_MODE_RAW		0x2

/* Meson 6b uses REG1 to configure the mode */
#define REG1_MODE_MASK		GENMASK(8, 7)
#define REG1_MODE_SHIFT		7

/* Meson 8b / GXBB use REG2 to configure the mode */
#define REG2_MODE_MASK		GENMASK(3, 0)
#define REG2_MODE_SHIFT		0

#define REG1_TIME_IV_SHIFT	16
#define REG1_TIME_IV_MASK	((BIT(13) - 1) << REG1_TIME_IV_SHIFT)

#define REG1_IRQSEL_MASK	(BIT(2) | BIT(3))
#define REG1_IRQSEL_NEC_MODE	(0 << 2)
#define REG1_IRQSEL_RISE_FALL	(1 << 2)
#define REG1_IRQSEL_FALL	(2 << 2)
#define REG1_IRQSEL_RISE	(3 << 2)

#define REG1_RESET		BIT(0)
#define REG1_POL		BIT(1)
#define REG1_ENABLE		BIT(15)

#define AO_RTI_PIN_MUX_REG	0x14	/* offset 0x5 */
#define STATUS_IR_DEC_IN	BIT(8)

#define MESON_TRATE		10	/* us */

struct meson_ir {
	void __iomem	*reg;
	struct rc_dev	*rc;
	int		irq;
	spinlock_t	lock;
	struct timer_list flush_timer;
};

unsigned backup_IR_DEC_LDR_ACTIVE;
unsigned backup_IR_DEC_LDR_IDLE;
unsigned bakeup_IR_DEC_LDR_REPEAT;
unsigned backup_IR_DEC_BIT_0;
unsigned backup_IR_DEC_REG0;
unsigned backup_IR_DEC_STATUS;
unsigned backup_IR_DEC_REG1;

static void backup_remote_register(struct meson_ir *ir)
{
	backup_IR_DEC_LDR_ACTIVE = readl(ir->reg + IR_DEC_LDR_ACTIVE);
	backup_IR_DEC_LDR_IDLE = readl(ir->reg + IR_DEC_LDR_IDLE);
	bakeup_IR_DEC_LDR_REPEAT = readl(ir->reg + IR_DEC_LDR_REPEAT);
	backup_IR_DEC_BIT_0 = readl(ir->reg + IR_DEC_BIT_0);
	backup_IR_DEC_REG0 = readl(ir->reg + IR_DEC_REG0);
	backup_IR_DEC_STATUS = readl(ir->reg + IR_DEC_STATUS);
	backup_IR_DEC_REG1 = readl(ir->reg + IR_DEC_REG1);
}

static void restore_remote_register(struct meson_ir *ir)
{
	writel(backup_IR_DEC_LDR_ACTIVE, ir->reg + IR_DEC_LDR_ACTIVE);
	writel(backup_IR_DEC_LDR_IDLE, ir->reg + IR_DEC_LDR_IDLE);
	writel(bakeup_IR_DEC_LDR_REPEAT, ir->reg + IR_DEC_LDR_REPEAT);
	writel(backup_IR_DEC_BIT_0, ir->reg + IR_DEC_BIT_0);
	writel(backup_IR_DEC_REG0, ir->reg + IR_DEC_REG0);
	writel(backup_IR_DEC_STATUS, ir->reg + IR_DEC_STATUS);
	writel(backup_IR_DEC_REG1, ir->reg + IR_DEC_REG1);
	readl(ir->reg + IR_DEC_FRAME);
}

static void meson_ir_set(struct meson_ir *ir, unsigned int reg, u32 value)
{
	writel(value, ir->reg + reg);
}

static void meson_ir_set_mask(struct meson_ir *ir, unsigned int reg,
			      u32 mask, u32 value)
{
	u32 data;

	data = readl(ir->reg + reg);
	data &= ~mask;
	data |= (value & mask);
	meson_ir_set(ir, reg, data);
}

#ifdef CONFIG_PM
// Set operation mode by last_protocol, it can be forced by node <wakeup_protocol>
static void meson_ir_enable_HW_decoder(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct meson_ir *ir = platform_get_drvdata(pdev);
	unsigned long flags;
	int ret;
	int protocol = ir->rc->last_protocol;

	ret = of_property_read_u32(pdev->dev.of_node,
			"wakeup_protocol", &protocol);
	if (ret)
		dev_info(dev, "don't find the node <wakeup_protocol>, use last protocol %d\n", protocol);

	dev_info(dev, "use wake up protocol %d\n", protocol);

	switch (protocol) {
		case RC_TYPE_RC5:
			meson_ir_set(ir, IR_DEC_LDR_ACTIVE, 0);
			meson_ir_set(ir, IR_DEC_LDR_IDLE, 0);
			meson_ir_set(ir, IR_DEC_LDR_REPEAT, 0);
			meson_ir_set(ir, IR_DEC_BIT_0, 0);
			meson_ir_set(ir, IR_DEC_REG0, 3 << 28 | 0x1644 << 12 | 0x13);
			meson_ir_set(ir, IR_DEC_STATUS, 1 << 30);
			meson_ir_set(ir, IR_DEC_REG1, 1 << 15 | 13 << 8);
			meson_ir_set(ir, IR_DEC_REG2, 1 << 13 | 1 << 11 | 1 << 8 | 0x7);
			meson_ir_set(ir, IR_DEC_DURATN2, 56 << 16 | 32 << 0);
			meson_ir_set(ir, IR_DEC_DURATN3, 102 << 16 | 76 << 0);
			meson_ir_set(ir, IR_DEC_REG3, 0);
			break;

		case RC_TYPE_RC6_0:
			meson_ir_set(ir, IR_DEC_LDR_ACTIVE, 210 << 16 | 120 << 0);
			meson_ir_set(ir, IR_DEC_LDR_IDLE, 55 << 16 | 38 << 0);
			meson_ir_set(ir, IR_DEC_LDR_REPEAT, 145 << 16 | 125 << 0);
			meson_ir_set(ir, IR_DEC_BIT_0, 51 << 16 | 38 << 0);
			meson_ir_set(ir, IR_DEC_REG0, 3 << 28 | 0xFA0 << 12 | 0x13);
			meson_ir_set(ir, IR_DEC_STATUS, 94 << 20 | 82 << 10);
			meson_ir_set(ir, IR_DEC_REG1, 1 << 15 | 20 << 8 | 1 << 6);
			meson_ir_set(ir, IR_DEC_REG2, 1 << 8 | 0x9);
			meson_ir_set(ir, IR_DEC_DURATN2, 28 << 16 | 16 << 0);
			meson_ir_set(ir, IR_DEC_DURATN3, 51 << 16 | 38 << 0);
			break;

		case RC_TYPE_RC6_6A_20:
		case RC_TYPE_RC6_6A_24:
		case RC_TYPE_RC6_6A_32:
		case RC_TYPE_RC6_MCE:
			meson_ir_set(ir, IR_DEC_LDR_ACTIVE, 210 << 16 | 120 << 0);
			meson_ir_set(ir, IR_DEC_LDR_IDLE, 55 << 16 | 38 << 0);
			meson_ir_set(ir, IR_DEC_LDR_REPEAT, 145 << 16 | 125 << 0);
			meson_ir_set(ir, IR_DEC_BIT_0, 51 << 16 | 38 << 0);
			meson_ir_set(ir, IR_DEC_REG0, 3 << 28 | 0xFA0 << 12 | 0x13);
			meson_ir_set(ir, IR_DEC_STATUS, 94 << 20 | 82 << 10);
			meson_ir_set(ir, IR_DEC_REG1, 1 << 15 | 36 << 8 | 1 << 6);
			meson_ir_set(ir, IR_DEC_REG2, 1 << 8 | 0x9);
			meson_ir_set(ir, IR_DEC_DURATN2, 28 << 16 | 16 << 0);
			meson_ir_set(ir, IR_DEC_DURATN3, 51 << 16 | 38 << 0);
			break;

		// default is NEC
		default:
			meson_ir_set(ir, IR_DEC_LDR_ACTIVE, 500 << 16 | 202 << 0);
			meson_ir_set(ir, IR_DEC_LDR_IDLE, 300 << 16 | 202 << 0);
			meson_ir_set(ir, IR_DEC_LDR_REPEAT, 150 << 16 | 80 << 0);
			meson_ir_set(ir, IR_DEC_BIT_0, 72 << 16 | 40 << 0);
			meson_ir_set(ir, IR_DEC_REG0, 7 << 28 | 0xFA0 << 12 | 0x13);
			meson_ir_set(ir, IR_DEC_STATUS, 134 << 20 | 90 << 10);
			meson_ir_set(ir, IR_DEC_REG1, 0x9f00);
			break;
	}
}
#endif

static irqreturn_t meson_ir_irq(int irqno, void *dev_id)
{
	struct meson_ir *ir = dev_id;

	spin_lock(&ir->lock);

	ir_raw_event_store_edge(ir->rc,
		(readl(ir->reg + IR_DEC_STATUS) & STATUS_IR_DEC_IN)
		? IR_PULSE : IR_SPACE);

	mod_timer(&ir->flush_timer,
		jiffies + nsecs_to_jiffies(ir->rc->timeout));

	ir_raw_event_handle(ir->rc);

	spin_unlock(&ir->lock);

	return IRQ_HANDLED;
}

static void flush_timer(unsigned long arg)
{
	struct meson_ir *ir = (struct meson_ir *)arg;
	DEFINE_IR_RAW_EVENT(rawir);

	rawir.timeout = true;
	rawir.duration = ir->rc->timeout;
	ir_raw_event_store(ir->rc, &rawir);
	ir_raw_event_handle(ir->rc);
}

static int meson_ir_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;
	const char *map_name;
	struct meson_ir *ir;
	int ret;
	unsigned int reg_val;
	bool pulse_inverted = false;

	ir = devm_kzalloc(dev, sizeof(struct meson_ir), GFP_KERNEL);
	if (!ir)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ir->reg = devm_ioremap_resource(dev, res);
	if (IS_ERR(ir->reg)) {
		dev_err(dev, "failed to map registers\n");
		return PTR_ERR(ir->reg);
	}

	ir->irq = platform_get_irq(pdev, 0);
	if (ir->irq < 0) {
		dev_err(dev, "no irq resource\n");
		return ir->irq;
	}

	ir->rc = rc_allocate_device();
	if (!ir->rc) {
		dev_err(dev, "failed to allocate rc device\n");
		return -ENOMEM;
	}

	ir->rc->priv = ir;
	ir->rc->input_name = DRIVER_NAME;
	ir->rc->input_phys = DRIVER_NAME "/input0";
	ir->rc->input_id.bustype = BUS_HOST;
	map_name = of_get_property(node, "linux,rc-map-name", NULL);
	ir->rc->map_name = map_name ? map_name : RC_MAP_EMPTY;
	ir->rc->dev.parent = dev;
	ir->rc->driver_type = RC_DRIVER_IR_RAW;
	ir->rc->allowed_protocols = RC_BIT_ALL;
	ir->rc->rx_resolution = US_TO_NS(MESON_TRATE);
	ir->rc->min_timeout = 1;
	ir->rc->timeout = MS_TO_NS(125);
	ir->rc->max_timeout = MS_TO_NS(1250);
	ir->rc->driver_name = DRIVER_NAME;
	pulse_inverted = of_property_read_bool(node, "pulse-inverted");

	spin_lock_init(&ir->lock);
	platform_set_drvdata(pdev, ir);

	ret = rc_register_device(ir->rc);
	if (ret) {
		dev_err(dev, "failed to register rc device\n");
		goto out_free;
	}

	setup_timer(&ir->flush_timer, flush_timer, (unsigned long) ir);

	ret = devm_request_irq(dev, ir->irq, meson_ir_irq, 0, "ir-meson", ir);
	if (ret) {
		dev_err(dev, "failed to request irq\n");
		goto out_unreg;
	}

	/* Set remote_input alternative function - GPIOAO.BIT5 */
	reg_val = aml_read_aobus(AO_RTI_PIN_MUX_REG);
	reg_val |= (0x1 << 20); /* [23:20], func1 IR_REMOTE_IN */
	aml_write_aobus(AO_RTI_PIN_MUX_REG, reg_val);

	reg_val = aml_read_aobus(AO_RTI_PIN_MUX_REG);
	dev_info(dev, "AO_RTI_PIN_MUX : 0x%x\n", reg_val);

	/* Reset the decoder */
	meson_ir_set_mask(ir, IR_DEC_REG1, REG1_RESET, REG1_RESET);
	meson_ir_set_mask(ir, IR_DEC_REG1, REG1_RESET, 0);

	/* Set general operation mode (= raw/software decoding) */
	if (of_device_is_compatible(node, "amlogic,meson6-ir"))
		meson_ir_set_mask(ir, IR_DEC_REG1, REG1_MODE_MASK,
				  DECODE_MODE_RAW << REG1_MODE_SHIFT);
	else
		meson_ir_set_mask(ir, IR_DEC_REG2, REG2_MODE_MASK,
				  DECODE_MODE_RAW << REG2_MODE_SHIFT);

	/* Set rate */
	meson_ir_set_mask(ir, IR_DEC_REG0, REG0_RATE_MASK, MESON_TRATE - 1);
	/* IRQ on rising and falling edges */
	meson_ir_set_mask(ir, IR_DEC_REG1, REG1_IRQSEL_MASK,
			  REG1_IRQSEL_RISE_FALL);
	/* Set polarity Invert input polarity */
	meson_ir_set_mask(ir, IR_DEC_REG1, REG1_POL,
			pulse_inverted ? REG1_POL : 0);
	/* Enable the decoder */
	meson_ir_set_mask(ir, IR_DEC_REG1, REG1_ENABLE, REG1_ENABLE);

	dev_info(dev, "receiver initialized\n");

	return 0;
out_unreg:
	rc_unregister_device(ir->rc);
	ir->rc = NULL;
out_free:
	rc_free_device(ir->rc);

	return ret;
}

static int meson_ir_remove(struct platform_device *pdev)
{
	struct meson_ir *ir = platform_get_drvdata(pdev);
	unsigned long flags;

	/* Disable the decoder */
	spin_lock_irqsave(&ir->lock, flags);
	meson_ir_set_mask(ir, IR_DEC_REG1, REG1_ENABLE, 0);
	spin_unlock_irqrestore(&ir->lock, flags);

	del_timer_sync(&ir->flush_timer);

	rc_unregister_device(ir->rc);

	return 0;
}

#ifdef CONFIG_PM
static void meson_ir_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct meson_ir *ir = platform_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&ir->lock, flags);

	/*
	 * Set operation mode default to NEC/hardware decoding to give
	 * bootloader a chance to power the system back on
	 */
	if (of_device_is_compatible(node, "amlogic,meson6-ir"))
		meson_ir_enable_HW_decoder(pdev);
	else
		meson_ir_set_mask(ir, IR_DEC_REG2, REG2_MODE_MASK,
				  DECODE_MODE_NEC << REG2_MODE_SHIFT);

	/* Set rate to default value */
	meson_ir_set_mask(ir, IR_DEC_REG0, REG0_RATE_MASK, 0x13);

	spin_unlock_irqrestore(&ir->lock, flags);
}

static int meson_ir_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct meson_ir *ir = platform_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&ir->lock, flags);

	/*
	 * Set operation mode default to NEC/hardware decoding to give
	 * bootloader a chance to resume the system back on
	 */
	backup_remote_register(ir);

	if (of_device_is_compatible(node, "amlogic,meson6-ir"))
		meson_ir_enable_HW_decoder(pdev);
	else
		meson_ir_set_mask(ir, IR_DEC_REG2, REG2_MODE_MASK,
				  DECODE_MODE_NEC << REG2_MODE_SHIFT);

	/* Set rate to default value */
	meson_ir_set_mask(ir, IR_DEC_REG0, REG0_RATE_MASK, 0x13);

	spin_unlock_irqrestore(&ir->lock, flags);
	return 0;
}

static int meson_ir_resume(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct meson_ir *ir = platform_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&ir->lock, flags);

	restore_remote_register(ir);

	spin_unlock_irqrestore(&ir->lock, flags);
	return 0;
}
#endif

static const struct of_device_id meson_ir_match[] = {
	{ .compatible = "amlogic,meson6-ir" },
	{ .compatible = "amlogic,meson8b-ir" },
	{ .compatible = "amlogic,meson-gxbb-ir" },
	{ },
};
MODULE_DEVICE_TABLE(of, meson_ir_match);

static struct platform_driver meson_ir_driver = {
	.probe		= meson_ir_probe,
	.remove		= meson_ir_remove,
#ifdef CONFIG_PM
	.shutdown	= meson_ir_shutdown,
	.suspend	= meson_ir_suspend,
	.resume		= meson_ir_resume,
#endif
	.driver = {
		.name		= DRIVER_NAME,
		.of_match_table	= meson_ir_match,
	},
};

module_platform_driver(meson_ir_driver);

MODULE_DESCRIPTION("Amlogic Meson IR remote receiver driver");
MODULE_AUTHOR("Beniamino Galvani <b.galvani@gmail.com>");
MODULE_LICENSE("GPL v2");
