/*

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef CONFIG_ARM64
#include <mach/am_regs.h>
#else
#include <linux/reset.h>
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/platform_device.h>
#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#endif

#include "aml_fe.h"

#include "avl6862.h"
#include "r848a.h"
#include "mxl608.h"
#include "rda5815m.h"

#include "tuner_ftm4862.h"

#include "aml_dvb.h"
#undef pr_err

#define pr_dbg(fmt, args...) \
	do {\
		if (debug_fe)\
			printk("DVB FE: " fmt, ##args);\
	} while (0)
#define pr_err(fmt, args...) printk("DVB FE: " fmt, ## args)
#define pr_inf(fmt, args...)   printk("DVB FE: " fmt, ## args)

MODULE_PARM_DESC(debug_fe, "\n\t\t Enable frontend debug information");
static int debug_fe;
module_param(debug_fe, int, 0644);

MODULE_PARM_DESC(frontend_power, "\n\t\t Power GPIO of frontend");
static int frontend_power = -1;
module_param(frontend_power, int, 0644);

MODULE_PARM_DESC(frontend_reset, "\n\t\t Reset GPIO of frontend");
static int frontend_reset = -1;
module_param(frontend_reset, int, 0644);

MODULE_PARM_DESC(frontend_antoverload, "\n\t\t Antoverload GPIO of frontend");
static int frontend_antoverload = -1;
module_param(frontend_antoverload, int, 0644);

#if defined(CONFIG_PROC_FS)
static DEFINE_SPINLOCK(aml_fe_lock);
static struct proc_dir_entry *proc_aml_fe;
static struct proc_dir_entry *proc_antoverload;
#define PROCREG_AML_FE		"aml_fe"
#define PROCREG_ANT_POWER	"antoverload"
static int set_ant_power = 0; /* default is off */
#endif
extern void dmx_reset_dmx_sw(void);

static struct aml_fe avl6862_fe[FE_DEV_COUNT];

static char *device_name = "avl6862";

enum {
	TUNER_BOARD_UNKNOWN,
	TUNER_BOARD_MEECOOL,
	TUNER_BOARD_MAGICSEE
};

static struct r848_config r848_config = {
	.i2c_address = 0x7A,
};

static struct ftm4862_config ftm4862_config = {
	.reserved = 0,
};

static struct avl6862_config avl6862_config = {
	.demod_address = 0x14,
	.tuner_address = 0,
	.ts_serial = 0,
};

int avl6862_Reset(void)
{
	pr_dbg("avl6862_Reset!\n");

	if(frontend_reset < 0)
		return 0;
	gpio_request(frontend_reset,device_name);
	gpio_direction_output(frontend_reset, 0);
	msleep(600);
	gpio_request(frontend_reset,device_name);
	gpio_direction_output(frontend_reset, 1);
	msleep(600);

	return 0;
}

int avl6862_gpio(void)
{
	pr_dbg("avl6862_gpio!\n");

	if(frontend_power >= 0) {
		gpio_request(frontend_power,device_name);
		gpio_direction_output(frontend_power, 1);
	}

	if(frontend_antoverload >= 0) {
		gpio_request(frontend_antoverload,device_name);
		gpio_direction_output(frontend_antoverload, 0);
	}

	return 0;
}

static int avl6862_fe_init(struct aml_dvb *advb, struct platform_device *pdev, struct aml_fe *fe, int id)
{
	struct dvb_frontend_ops *ops;
	int ret, i2c_adap_id = 1, /* i2c_addr = 0x14, */ tun_board = TUNER_BOARD_MEECOOL;
	struct i2c_adapter *i2c_handle;
#ifdef CONFIG_OF
	const char *str;
#ifdef CONFIG_ARM64
        struct gpio_desc *desc;
	int gpio_reset, gpio_power, gpio_antoverload;
#endif
#endif

	pr_inf("Init AVL6862 frontend %d\n", id);

#ifdef CONFIG_OF
	if(!of_property_read_string(pdev->dev.of_node, "dev_name", &str)) {
		if(strlen(str) > 0 && !strcmp(str, "magicsee"))
		{
			tun_board = TUNER_BOARD_MAGICSEE;
			pr_dbg("dev_name=%s\n", str);
		}
	}
	//if(!of_property_read_u32(pdev->dev.of_node, "dtv_demod0_i2c_addr", &i2c_addr)) {
	//	avl6862_config.demod_address = i2c_addr;
	//	pr_dbg("i2c_addr=0x%02x\n", avl6862_config.demod_address);
	//}
	of_property_read_u32(pdev->dev.of_node, "fe0_ts", &avl6862_config.ts_serial);
	pr_dbg("fe0_ts=%d\n", avl6862_config.ts_serial);
	if (of_property_read_u32(pdev->dev.of_node, "dtv_demod0_i2c_adap_id", &i2c_adap_id)) {
		ret = -ENOMEM;
		goto err_resource;
	}
	pr_dbg("i2c_adap_id=%d\n", i2c_adap_id);
	desc = of_get_named_gpiod_flags(pdev->dev.of_node, "dtv_demod0_reset_gpio-gpios", 0, NULL);
	if (!PTR_RET(desc)) {
		gpio_reset = desc_to_gpio(desc);
		pr_dbg("gpio_reset=%d\n", gpio_reset);
	} else
		gpio_reset = -1;

	desc = of_get_named_gpiod_flags(pdev->dev.of_node, "dtv_demod0_power_gpio-gpios", 0, NULL);
	if (!PTR_RET(desc)) {
		gpio_power = desc_to_gpio(desc);
		pr_dbg("gpio_power=%d\n", gpio_power);
	} else
		gpio_power = -1;

	desc = of_get_named_gpiod_flags(pdev->dev.of_node, "dtv_demod0_antoverload_gpio-gpios", 0, NULL);
	if (!PTR_RET(desc)) {
		gpio_antoverload = desc_to_gpio(desc);
		pr_dbg("gpio_antoverload=%d\n", gpio_antoverload);
	} else
		gpio_antoverload = -1;

	avl6862_config.gpio_lock_led = 0;
	/*desc = of_get_named_gpiod_flags(pdev->dev.of_node, "dtv_demod0_lock_gpio-gpios", 0, NULL);
	if (!PTR_RET(desc)) {
		avl6862_config.gpio_lock_led = desc_to_gpio(desc);
		pr_dbg("gpio_lock_led=%d\n", avl6862_config.gpio_lock_led);
        }*/

	frontend_reset = gpio_reset;
	frontend_power = gpio_power;
	frontend_antoverload = gpio_antoverload;
#endif /*CONFIG_OF*/

	i2c_handle = i2c_get_adapter(i2c_adap_id);

	if (!i2c_handle) {
		pr_err("Cannot get i2c adapter for id:%d! \n", i2c_adap_id);
		ret = -ENOMEM;
		goto err_resource;
	}

	avl6862_gpio();
	avl6862_Reset();

	fe->fe = dvb_attach(avl6862_attach, &avl6862_config, i2c_handle);

	if (!fe->fe) {
		pr_err("avl6862_attach attach failed!!!\n");
		ret = -ENOMEM;
		goto err_resource;
	}

	if (tun_board == TUNER_BOARD_MEECOOL)
	{
		if (dvb_attach(r848x_attach, fe->fe, &r848_config, i2c_handle) == NULL) {
			dvb_frontend_detach(fe->fe);
			fe->fe = NULL;
			pr_err("r848_attach attach failed!!!\n");
			ret = -ENOMEM;
			goto err_resource;
		}

		pr_inf("AVL6862 and R848 attached!\n");

		if ((ret=dvb_register_frontend(&advb->dvb_adapter, fe->fe))) {
			pr_err("Frontend avl6862 registration failed!!!\n");
			dvb_frontend_detach(fe->fe);
			ops = &fe->fe->ops;
			if (ops->release != NULL)
				ops->release(fe->fe);
			fe->fe = NULL;
			ret = -ENOMEM;
			goto err_resource;
		}

		goto tun_board_end;
	}
	else if (tun_board == TUNER_BOARD_MAGICSEE)
	{
		if (dvb_attach(ftm4862_attach, fe->fe, &ftm4862_config, i2c_handle) == NULL) {
			dvb_frontend_detach(fe->fe);
			fe->fe = NULL;
			pr_err("ftm4862_attach attach failed!!!\n");
			ret = -ENOMEM;
			goto err_resource;
		}

		pr_inf("FTM4862 attached!\n");

		if ((ret=dvb_register_frontend(&advb->dvb_adapter, fe->fe))) {
			pr_err("Frontend avl6862 registration failed!!!\n");
			dvb_frontend_detach(fe->fe);
			ops = &fe->fe->ops;
			if (ops->release != NULL)
				ops->release(fe->fe);
			fe->fe = NULL;
			ret = -ENOMEM;
			goto err_resource;
		}

		goto tun_board_end;
	}
	else {
		pr_err("Tuners not found, frontend avl6862 registration failed!!!\n");
		dvb_frontend_detach(fe->fe);
		ops = &fe->fe->ops;
		if (ops->release != NULL)
			ops->release(fe->fe);
		fe->fe = NULL;
		ret = -ENOMEM;
		goto err_resource;
	}

tun_board_end:
	pr_inf("Frontend AVL6862 registred!\n");
	dmx_reset_dmx_sw();

	return 0;

err_resource:
	return ret;
}

static int avl6862_fe_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct aml_dvb *dvb = aml_get_dvb_device();

	if(avl6862_fe_init(dvb, pdev, &avl6862_fe[0], 0)<0)
		return -ENXIO;

	platform_set_drvdata(pdev, &avl6862_fe[0]);

	return ret;
}

static void avl6862_fe_release(struct aml_dvb *advb, struct aml_fe *fe)
{
	if(fe && fe->fe) {
		dvb_unregister_frontend(fe->fe);
		dvb_frontend_detach(fe->fe);
		fe->fe = NULL;
	}
}

static int avl6862_fe_remove(struct platform_device *pdev)
{
        struct aml_fe *drv_data = platform_get_drvdata(pdev);
        struct aml_dvb *dvb = aml_get_dvb_device();

	platform_set_drvdata(pdev, NULL);
	avl6862_fe_release(dvb, drv_data);

	return 0;
}

static int avl6862_fe_resume(struct platform_device *pdev)
{
	pr_dbg("avl6862_fe_resume \n");
	return 0;
}

static int avl6862_fe_suspend(struct platform_device *pdev, pm_message_t state)
{
	pr_dbg("avl6862_fe_suspend \n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_fe_dt_match[]={
        {
                .compatible = "amlogic,dvbfe",
        },
        {},
};
#endif /*CONFIG_OF*/

static struct platform_driver aml_fe_driver = {
        .probe = avl6862_fe_probe,
        .remove = avl6862_fe_remove,
        .resume = avl6862_fe_resume,
        .suspend = avl6862_fe_suspend,
        .driver = {
    	.name = "avl6862",
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = aml_fe_dt_match,
#endif
        }
};

#if defined(CONFIG_PROC_FS)
static int antoverload_read(struct seq_file *seq, void *v)
{
	seq_printf(seq, "%s\n", set_ant_power ? "1" : "0" );
	return 0;
}

static int antoverload_open(struct inode *inode, struct file *file)
{
	return single_open(file, antoverload_read, NULL);
}

static ssize_t antoverload_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char buf[2];
	/* set on/off output 5V, echo 1|0 > /proc/aml_fe/antoverload */

	if(copy_from_user(buf, buffer, count))
		return -EFAULT;

	spin_lock_bh(&aml_fe_lock);

	set_ant_power = simple_strtol(buf, 0, 10);
	if(!set_ant_power) {
		gpio_request(frontend_antoverload,device_name);
		gpio_direction_output(frontend_antoverload, 0);
	} else {
		gpio_request(frontend_antoverload,device_name);
		gpio_direction_output(frontend_antoverload, 1);
	}
	pr_info("aml_fe: Output 5V is %s.\n", set_ant_power ? "on" : "off" );

	spin_unlock_bh(&aml_fe_lock);

	return count;
}

static const struct file_operations aml_fe_antoverload_fops = {
	.owner		= THIS_MODULE,
	.open		= antoverload_open,
	.read		= seq_read,
	.write		= antoverload_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void aml_fe_proc_init(void)
{
	if(proc_aml_fe == NULL)
		proc_aml_fe = proc_mkdir(PROCREG_AML_FE, NULL);

	if(frontend_antoverload >= 0) {
		if(!(proc_antoverload = proc_create(PROCREG_ANT_POWER, 0, proc_aml_fe, &aml_fe_antoverload_fops)))
			pr_err("!! FAIL to create %s PROC !!\n", PROCREG_ANT_POWER);
	}
	pr_info("PROC AML FE INIT OK!\n");
}

static void aml_fe_proc_exit(void)
{
	if((frontend_antoverload >= 0) && proc_antoverload)
		remove_proc_entry(PROCREG_ANT_POWER, proc_aml_fe);
	if(proc_aml_fe)
		remove_proc_entry(PROCREG_AML_FE, 0);
	pr_info("PROC AML FE EXIT OK!\n");
}
#endif

static int __init avlfrontend_init(void) {
	int ret;
	ret = platform_driver_register(&aml_fe_driver);
#if defined(CONFIG_PROC_FS)
	aml_fe_proc_init();
#endif
	return ret;
}

static void __exit avlfrontend_exit(void) {
#if defined(CONFIG_PROC_FS)
	aml_fe_proc_exit();
#endif
	platform_driver_unregister(&aml_fe_driver);
}

module_init(avlfrontend_init);
module_exit(avlfrontend_exit);
MODULE_AUTHOR("afl1");
MODULE_DESCRIPTION("AVL6862 DVB-Sx/Tx/C frontend driver");
MODULE_LICENSE("GPL");
