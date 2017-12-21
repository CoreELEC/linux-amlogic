
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>

#include "aml_pcmcia.h"

static int aml_pcmcia_debug = 1;

module_param_named(pcmcia_debug, aml_pcmcia_debug, int, 0644);
MODULE_PARM_DESC(pcmcia_debug, "enable verbose debug messages");

#define pr_dbg(args...)\
	do {\
		if (aml_pcmcia_debug)\
			printk(args);\
	} while (0)
#define pr_error(fmt, args...) printk("PCMCIA: " fmt, ## args)


static int pcmcia_plugin(struct aml_pcmcia *pc)
{
	if (pc->slot_state == MODULE_XTRACTED) {
		pr_dbg(" CAM Plugged IN: Adapter(%d) Slot(0)\n", 0);
		udelay(50);
		aml_pcmcia_reset(pc);
		/*wait unplug*/
		pc->init_irq(pc, IRQF_TRIGGER_RISING);
		udelay(500);
		pc->slot_state = MODULE_INSERTED;
	} else {
		pr_error("repeat into pcmcia insert \r\n");
		aml_pcmcia_reset(pc);
	}
	udelay(100);
	pc->pcmcia_plugin(pc, 1);

	return 0;
}

static int pcmcia_unplug(struct aml_pcmcia *pc)
{
	if (pc->slot_state == MODULE_INSERTED) {
		pr_dbg(" CAM Unplugged: Adapter(%d) Slot(0)\n", 0);
		/*udelay(50);*/
		/*aml_pcmcia_reset(pc);*/
		/*wait plugin*/
		pc->init_irq(pc, IRQF_TRIGGER_FALLING);
		udelay(500);
		pc->slot_state = MODULE_XTRACTED;
	}
	udelay(100);
	pc->pcmcia_plugin(pc, 0);

	return 0;
}

static irqreturn_t pcmcia_irq_handler(int irq, void *dev_id)
{
	struct aml_pcmcia *pc = (struct aml_pcmcia *)dev_id;

	pr_dbg("pcmcia_irq_handler--into--\r\n");
	disable_irq_nosync(pc->irq);
	schedule_work(&pc->pcmcia_work);
	enable_irq(pc->irq);
	return IRQ_HANDLED;
}

static void aml_pcmcia_work(struct work_struct *work)
{
	int cd1, cd2;
	struct aml_pcmcia *pc = container_of(
		work, struct aml_pcmcia, pcmcia_work);

	cd1 = pc->get_cd1(pc);
	cd2 = pc->get_cd2(pc);

	if (cd1 != cd2)
		pr_error("CAM card not inerted.\n");
	else {
		if (!cd1) {
			pr_error("Adapter(%d) Slot(0): CAM Plugin\n", 0);
			pcmcia_plugin(pc);
		} else {
			pr_error("Adapter(%d) Slot(0): CAM Unplug\n", 0);
			pcmcia_unplug(pc);
		}
	}
}

static struct aml_pcmcia *pc_cur;

int aml_pcmcia_init(struct aml_pcmcia *pc)
{
	int err = 0;

	pr_dbg("aml_pcmcia_init start pc->irq=%d\r\n", pc->irq);
	pc->rst(pc, AML_L);
	/*power on*/
	pc->pwr(pc, AML_PWR_OPEN);/*hi is open power*/
	/*assuming cam unpluged, config the INT to waiting-for-plugin mode*/
	pc->init_irq(pc, IRQF_TRIGGER_LOW);

	INIT_WORK(&pc->pcmcia_work, aml_pcmcia_work);

	err = request_irq(pc->irq,
	pcmcia_irq_handler,
	IRQF_ONESHOT, "aml-pcmcia", pc);
	if (err != 0) {
		pr_error("ERROR: IRQ registration failed ! <%d>", err);
		return -ENODEV;
	}

	pc_cur = pc;
	pr_dbg("aml_pcmcia_init ok\r\n");
	return 0;
}
EXPORT_SYMBOL(aml_pcmcia_init);

int aml_pcmcia_exit(struct aml_pcmcia *pc)
{
	free_irq(pc->irq, pc);
	return 0;
}
EXPORT_SYMBOL(aml_pcmcia_exit);

int aml_pcmcia_reset(struct aml_pcmcia *pc)
{
		pr_dbg("CAM RESET-->\n");
		/* viaccess neotion cam need delay 2000 and 3000 */
		/* smit cam need delay 1000 and 1500 */
		/* need change delay according cam vendor */
		pc->rst(pc, AML_H);/*HI is reset*/
		mdelay(1000);
		pc->rst(pc, AML_L);/*defaule LOW*/
		pr_dbg("CAM RESET--\n");
		mdelay(1500);
		pr_dbg("CAM RESET--end\n");
	return 0;
}
EXPORT_SYMBOL(aml_pcmcia_reset);



static ssize_t aml_pcmcia_test_cmd(struct class *class,
struct class_attribute *attr, const char *buf, size_t size)
{
	pr_dbg("pcmcia cmd: %s\n", buf);
	if (pc_cur) {
		if (memcmp(buf, "reset", 5) == 0)
			aml_pcmcia_reset(pc_cur);
		else if (memcmp(buf, "on", 2) == 0)
			pc_cur->pwr(pc_cur, AML_PWR_OPEN);
		else if (memcmp(buf, "off", 3) == 0)
			pc_cur->pwr(pc_cur, AML_PWR_CLOSE);
		else if (memcmp(buf, "poll", 4) == 0)
			schedule_work(&pc_cur->pcmcia_work);
		else if (memcmp(buf, "intr", 4) == 0)
			pc_cur->init_irq(pc_cur, IRQF_TRIGGER_RISING);
		else if (memcmp(buf, "intf", 4) == 0)
			pc_cur->init_irq(pc_cur, IRQF_TRIGGER_FALLING);
	}
	return size;
}

static struct class_attribute aml_pcmcia_class_attrs[] = {
	__ATTR(cmd,  0644, NULL, aml_pcmcia_test_cmd),
	__ATTR_NULL
};

static struct class aml_pcmcia_class = {
	.name = "aml_pcmcia_test",
	.class_attrs = aml_pcmcia_class_attrs,
};

static int __init aml_pcmcia_mod_init(void)
{
	pr_dbg("Amlogic PCMCIA Init\n");

	class_register(&aml_pcmcia_class);

	return 0;
}

static void __exit aml_pcmcia_mod_exit(void)
{
	pr_dbg("Amlogic PCMCIA Exit\n");

	class_unregister(&aml_pcmcia_class);
}



module_init(aml_pcmcia_mod_init);
module_exit(aml_pcmcia_mod_exit);

MODULE_LICENSE("GPL");
