#include <linux/init.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/i2c.h>
#include <linux/delay.h>
/* #include <mach/am_regs.h> */
#include <linux/device.h>
#include <linux/cdev.h>

/* #include <asm/fiq.h> */
#include <linux/uaccess.h>
#include <linux/dvb/aml_demod.h>
#include "demod_func.h"

#include <linux/slab.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
/*#include "sdio/sdio_init.h"*/
#define DRIVER_NAME "aml_demod"
#define MODULE_NAME "aml_demod"
#define DEVICE_NAME "aml_demod"
#define DEVICE_UI_NAME "aml_demod_ui"

#define pr_dbg(a ...) \
	do { \
		if (1) { \
			printk(a); \
		} \
	} while (0)


const char aml_demod_dev_id[] = "aml_demod";

/*#ifndef CONFIG_AM_DEMOD_DVBAPI
 * static struct aml_demod_i2c demod_i2c;
 * static struct aml_demod_sta demod_sta;
 * #else
 * extern struct aml_demod_i2c demod_i2c;
 * extern struct aml_demod_sta demod_sta;
 #endif*/

static struct aml_demod_i2c demod_i2c;
static struct aml_demod_sta demod_sta;
static int read_start;

int sdio_read_ddr(unsigned long sdio_addr, unsigned long byte_count,
		  unsigned char *data_buf)
{
	return 0;
}

int sdio_write_ddr(unsigned long sdio_addr, unsigned long byte_count,
		   unsigned char *data_buf)
{
	return 0;
}

int read_reg(int addr)
{
	addr = addr + DEMOD_BASE;
	return apb_read_reg(addr);
}

void wait_capture(int cap_cur_addr, int depth_MB, int start)
{
	int readfirst;
	int tmp;
	int time_out;
	int last = 0x90000000;

	time_out = readfirst = 0;
	tmp = depth_MB << 20;
	while (tmp && (time_out < 1000)) {      /*10seconds time out */
		time_out = time_out + 1;
		msleep(20);
		readfirst = app_apb_read_reg(cap_cur_addr);
		if ((last - readfirst) > 0)
			tmp = 0;
		else
			last = readfirst;
		/*      usleep(1000); */
		/*      readsecond= app_apb_read_reg(cap_cur_addr); */

		/*      if((readsecond-start)>tmp) */
/*                      tmp=0;*/
/*              if((readsecond-readfirst)<0)  // turn around*/
/*                      tmp=0;*/
		pr_dbg("First  %x = [%08x],[%08x]%x\n", cap_cur_addr, readfirst,
		       last, (last - readfirst));
/*              printf("Second %x = [%08x]\n",cap_cur_addr, readsecond);*/
		msleep(20);
	}
	read_start = readfirst + 0x40000000;
	pr_dbg("read_start is %x\n", read_start);
}

int cap_adc_data(struct aml_cap_data *cap)
{
	int tmp;
	int tb_depth;

	pr_dbg("capture ADC\n ");
	/*      printf("set mem_start (you can read in kernel start log
	 * (memstart is ).(hex)  :  ");*/
	/*      scanf("%x",&tmp);*/
	tmp = 0x94400000;
	app_apb_write_reg(0x9d, cap->cap_addr);
	app_apb_write_reg(0x9e, cap->cap_addr + cap->cap_size * 0x100000);
	/*0x8000000-128m, 0x400000-4m */
	read_start = tmp + 0x40000000;
	/*printf("set afifo rate. (hex)(adc_clk/demod_clk)*256+2 :  ");   //
	 * (adc_clk/demod_clk)*256+2 */
	/*  scanf("%x",&tmp); */
	cap->cap_afifo = 0x60;
	app_apb_write_reg(0x15, 0x18715f2);
	app_apb_write_reg(0x15, (app_apb_read_reg(0x15) & 0xfff00fff) |
			  ((cap->cap_afifo & 0xff) << 12));     /* set afifo */
	app_apb_write_reg(0x9b, 0x1c9);	/* capture ADC 10bits */
	app_apb_write_reg(0x7f, 0x00008000);	/* enable testbus 0x8000 */

	tb_depth = cap->cap_size;                               /*127; */
	tmp = 9;
	app_apb_write_reg(0x9b, (app_apb_read_reg(0x9b) & ~0x1f) | tmp);
	/* set testbus width */

	tmp = 0x100000;
	app_apb_write_reg(0x9c, tmp);   /* by ADC data enable */
	/*  printf("Set test mode. (0 is normal ,1 is testmode) :  ");  //0 */
	/*  scanf("%d",&tmp); */
	tmp = 0;
	if (tmp == 1)
		app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) | (1 << 10));
	/* set test mode; */
	else
		app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) & ~(1 << 10));
	/* close test mode; */

	app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) & ~(1 << 9));
	/* close cap; */
	app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) | (1 << 9));
	/* open  cap; */

	app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) | (1 << 7));
	/* close tb; */
	app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) & ~(1 << 7));
	/* open  tb; */

	app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) | (1 << 5));
	/* close intlv; */

	app_apb_write_reg(0x303, 0x8);  /* open dc_arbit */

	tmp = 0;
	if (tmp)
		app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) & ~(1 << 5));
	/* open  intlv; */

	app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) & ~(1 << 8));
	/* go  tb; */

	wait_capture(0x9f, tb_depth, app_apb_read_reg(0x9d));

	app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) | (1 << 8));
	/* stop  tb; */
	app_apb_write_reg(0x9b, app_apb_read_reg(0x9b) | (1 << 7));
	/* close tb; */
	return 0;
}

static DECLARE_WAIT_QUEUE_HEAD(lock_wq);

static ssize_t aml_demod_info(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	return 0;
}

static struct class_attribute aml_demod_class_attrs[] = {
	__ATTR(info,
	       S_IRUGO | S_IWUSR,
	       aml_demod_info,
	       NULL),
	__ATTR_NULL
};

static struct class aml_demod_class = {
	.name		= "aml_demod",
	.class_attrs	= aml_demod_class_attrs,
};

#if 0

static irqreturn_t aml_demod_isr(int irq, void *dev_id)
{
	if (demod_sta.dvb_mode == 0) {
		/*dvbc_isr(&demod_sta); */
		if (dvbc_isr_islock()) {
			pr_dbg("sync4\n");
			if (waitqueue_active(&lock_wq))
				wake_up_interruptible(&lock_wq);
		}
	} else {
		dvbt_isr(&demod_sta);
	}

	return IRQ_HANDLED;
}
#endif

static int aml_demod_open(struct inode *inode, struct file *file)
{
	pr_dbg("Amlogic Demod DVB-T/C Open\n");
	return 0;
}

static int aml_demod_release(struct inode *inode, struct file *file)
{
	pr_dbg("Amlogic Demod DVB-T/C Release\n");
	return 0;
}

#if 0
static int amdemod_islock(void)
{
	struct aml_demod_sts demod_sts;

	if (demod_sta.dvb_mode == 0) {
		dvbc_status(&demod_sta, &demod_i2c, &demod_sts);
		return demod_sts.ch_sts & 0x1;
	} else if (demod_sta.dvb_mode == 1) {
		dvbt_status(&demod_sta, &demod_i2c, &demod_sts);
		return demod_sts.ch_sts >> 12 & 0x1;
	}
	return 0;
}
#endif

void mem_read(struct aml_demod_mem *arg)
{
	int data;
	int addr;

	addr = arg->addr;
	data = arg->dat;
/*      memcpy(mem_buf[addr],data,1);*/
	pr_dbg("[addr %x] data is %x\n", addr, data);
}
static long aml_demod_ioctl(struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	int strength = 0;
	struct dvb_frontend *dvbfe;
	struct aml_tuner_sys *tuner;
	switch (cmd) {
	case AML_DEMOD_GET_RSSI:
		pr_dbg("Ioctl Demod GET_RSSI.\n");
		dvbfe = get_si2177_tuner();
		if (dvbfe != NULL)
			strength = dvbfe->ops.tuner_ops.get_strength(dvbfe);
		pr_dbg("[si2177] strength is %d\n", strength - 256);
		if (strength < 0)
			strength = 0 - strength;
		tuner = (struct aml_tuner_sys *)arg;
		tuner->rssi = strength;
		break;

	case AML_DEMOD_SET_TUNER:
		pr_dbg("Ioctl Demod Set Tuner.\n");
		dvbfe = get_si2177_tuner();
		if (dvbfe != NULL)
			dvbfe->ops.tuner_ops.set_tuner(dvbfe, &demod_sta,
						       &demod_i2c,
						       (struct aml_tuner_sys *)
						       arg);
		break;

	case AML_DEMOD_SET_SYS:
		pr_dbg("Ioctl Demod Set System\n");
		demod_set_sys(&demod_sta, &demod_i2c,
			      (struct aml_demod_sys *)arg);
		break;

	case AML_DEMOD_GET_SYS:
		pr_dbg("Ioctl Demod Get System\n");

		/*demod_get_sys(&demod_i2c, (struct aml_demod_sys *)arg); */
		break;

	case AML_DEMOD_TEST:
		pr_dbg("Ioctl Demod Test. It is blank now\n");
		/*demod_msr_clk(13); */
		/*demod_msr_clk(14); */
		/*demod_calc_clk(&demod_sta); */
		break;

	case AML_DEMOD_TURN_ON:
		pr_dbg("Ioctl Demod Turn ON.It is blank now\n");
		/*demod_turn_on(&demod_sta, (struct aml_demod_sys *)arg); */
		break;

	case AML_DEMOD_TURN_OFF:
		pr_dbg("Ioctl Demod Turn OFF.It is blank now\n");
		/*demod_turn_off(&demod_sta, (struct aml_demod_sys *)arg); */
		break;

	case AML_DEMOD_DVBC_SET_CH:
		pr_dbg("Ioctl DVB-C Set Channel.\n");
		dvbc_set_ch(&demod_sta, &demod_i2c,
			    (struct aml_demod_dvbc *)arg);
		break;

	case AML_DEMOD_DVBC_GET_CH:
		/*      pr_dbg("Ioctl DVB-C Get Channel. It is blank\n"); */
		dvbc_status(&demod_sta, &demod_i2c,
			    (struct aml_demod_sts *)arg);
		break;
	case AML_DEMOD_DVBC_TEST:
		pr_dbg("Ioctl DVB-C Test. It is blank\n");
		/*dvbc_get_test_out(0xb, 1000, (u32 *)arg); */
		break;
	case AML_DEMOD_DVBT_SET_CH:
		pr_dbg("Ioctl DVB-T Set Channel\n");
		dvbt_set_ch(&demod_sta, &demod_i2c,
			    (struct aml_demod_dvbt *)arg);
		break;

	case AML_DEMOD_DVBT_GET_CH:
		pr_dbg("Ioctl DVB-T Get Channel\n");
		/*dvbt_status(&demod_sta, &demod_i2c,
		 * (struct aml_demod_sts *)arg); */
		break;

	case AML_DEMOD_DVBT_TEST:
		pr_dbg("Ioctl DVB-T Test. It is blank\n");
		/*dvbt_get_test_out(0x1e, 1000, (u32 *)arg); */
		break;

	case AML_DEMOD_DTMB_SET_CH:
		dtmb_set_ch(&demod_sta, &demod_i2c,
			    (struct aml_demod_dtmb *)arg);
		break;

	case AML_DEMOD_DTMB_GET_CH:
		break;

	case AML_DEMOD_DTMB_TEST:
		break;

	case AML_DEMOD_ATSC_SET_CH:
		atsc_set_ch(&demod_sta, &demod_i2c,
			    (struct aml_demod_atsc *)arg);
		break;

	case AML_DEMOD_ATSC_GET_CH:
		check_atsc_fsm_status();
		break;

	case AML_DEMOD_ATSC_TEST:
		break;

	case AML_DEMOD_SET_REG:
		/*      pr_dbg("Ioctl Set Register\n"); */
		demod_set_reg((struct aml_demod_reg *)arg);
		break;

	case AML_DEMOD_GET_REG:
		/*      pr_dbg("Ioctl Get Register\n"); */
		demod_get_reg((struct aml_demod_reg *)arg);
		break;

/* case AML_DEMOD_SET_REGS: */
/* break; */

/* case AML_DEMOD_GET_REGS: */
/* break; */

	case AML_DEMOD_RESET_MEM:
		pr_dbg("set mem ok\n");
		break;

	case AML_DEMOD_READ_MEM:
		break;
	case AML_DEMOD_SET_MEM:
		/*step=(struct aml_demod_mem)arg;
		 * pr_dbg("[%x]0x%x------------------\n",i,mem_buf[step]);
		 * for(i=step;i<1024-1;i++){
		 * pr_dbg("0x%x,",mem_buf[i]);
		 * } */
		mem_read((struct aml_demod_mem *)arg);
		break;

	case AML_DEMOD_ATSC_IRQ:
		atsc_read_iqr_reg();
		break;

	default:
		pr_dbg("Enter Default ! 0x%X\n", cmd);
/* pr_dbg("AML_DEMOD_GET_REGS=0x%08X\n", AML_DEMOD_GET_REGS); */
/* pr_dbg("AML_DEMOD_SET_REGS=0x%08X\n", AML_DEMOD_SET_REGS); */
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_COMPAT

static long aml_demod_compat_ioctl(struct file *file, unsigned int cmd,
				   ulong arg)
{
	return aml_demod_ioctl(file, cmd, (ulong)compat_ptr(arg));
}

#endif


static const struct file_operations aml_demod_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_demod_open,
	.release	= aml_demod_release,
	.unlocked_ioctl = aml_demod_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= aml_demod_compat_ioctl,
#endif
};

static int aml_demod_ui_open(struct inode *inode, struct file *file)
{
	pr_dbg("Amlogic aml_demod_ui_open Open\n");
	return 0;
}

static int aml_demod_ui_release(struct inode *inode, struct file *file)
{
	pr_dbg("Amlogic aml_demod_ui_open Release\n");
	return 0;
}
char buf_all[100];
static ssize_t aml_demod_ui_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char *capture_buf = buf_all;
	int res = 0;

	if (count >= 4 * 1024 * 1024)
		count = 4 * 1024 * 1024;
	else if (count == 0)
		return 0;

	res = copy_to_user((void *)buf, (char *)capture_buf, count);
	if (res < 0) {
		pr_dbg("[aml_demod_ui_read]res is %d", res);
		return res;
	}

	return count;
}

static ssize_t aml_demod_ui_write(struct file *file, const char *buf,
				  size_t count, loff_t *ppos)
{
	return 0;
}

static struct device *aml_demod_ui_dev;
static dev_t aml_demod_devno_ui;
static struct cdev *aml_demod_cdevp_ui;
static const struct file_operations aml_demod_ui_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_demod_ui_open,
	.release	= aml_demod_ui_release,
	.read		= aml_demod_ui_read,
	.write		= aml_demod_ui_write,
	/*   .unlocked_ioctl    = aml_demod_ui_ioctl, */
};

#if 0
static ssize_t aml_demod_ui_info(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	return 0;
}

static struct class_attribute aml_demod_ui_class_attrs[] = {
	__ATTR(info,
	       S_IRUGO | S_IWUSR,
	       aml_demod_ui_info,
	       NULL),
	__ATTR_NULL
};
#endif

static struct class aml_demod_ui_class = {
	.name	= "aml_demod_ui",
/*    .class_attrs = aml_demod_ui_class_attrs,*/
};

int aml_demod_ui_init(void)
{
	int r = 0;

	r = class_register(&aml_demod_ui_class);
	if (r) {
		pr_dbg("create aml_demod class fail\r\n");
		class_unregister(&aml_demod_ui_class);
		return r;
	}

	r = alloc_chrdev_region(&aml_demod_devno_ui, 0, 1, DEVICE_UI_NAME);
	if (r < 0) {
		pr_dbg(KERN_ERR "aml_demod_ui: faild to alloc major number\n");
		r = -ENODEV;
		unregister_chrdev_region(aml_demod_devno_ui, 1);
		class_unregister(&aml_demod_ui_class);
		return r;
	}

	aml_demod_cdevp_ui = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!aml_demod_cdevp_ui) {
		pr_dbg(KERN_ERR "aml_demod_ui: failed to allocate memory\n");
		r = -ENOMEM;
		unregister_chrdev_region(aml_demod_devno_ui, 1);
		kfree(aml_demod_cdevp_ui);
		class_unregister(&aml_demod_ui_class);
		return r;
	}
	/* connect the file operation with cdev */
	cdev_init(aml_demod_cdevp_ui, &aml_demod_ui_fops);
	aml_demod_cdevp_ui->owner = THIS_MODULE;
	/* connect the major/minor number to cdev */
	r = cdev_add(aml_demod_cdevp_ui, aml_demod_devno_ui, 1);
	if (r) {
		pr_dbg(KERN_ERR "aml_demod_ui:failed to add cdev\n");
		unregister_chrdev_region(aml_demod_devno_ui, 1);
		cdev_del(aml_demod_cdevp_ui);
		kfree(aml_demod_cdevp_ui);
		class_unregister(&aml_demod_ui_class);
		return r;
	}

	aml_demod_ui_dev = device_create(&aml_demod_ui_class, NULL,
					 MKDEV(MAJOR(aml_demod_devno_ui), 0),
					 NULL, DEVICE_UI_NAME);

	if (IS_ERR(aml_demod_ui_dev)) {
		pr_dbg("Can't create aml_demod device\n");
		unregister_chrdev_region(aml_demod_devno_ui, 1);
		cdev_del(aml_demod_cdevp_ui);
		kfree(aml_demod_cdevp_ui);
		class_unregister(&aml_demod_ui_class);
		return r;
	}

	return r;
}

void aml_demod_exit_ui(void)
{
	unregister_chrdev_region(aml_demod_devno_ui, 1);
	cdev_del(aml_demod_cdevp_ui);
	kfree(aml_demod_cdevp_ui);
	class_unregister(&aml_demod_ui_class);
}

static struct device *aml_demod_dev;
static dev_t aml_demod_devno;
static struct cdev *aml_demod_cdevp;

#ifdef CONFIG_AM_DEMOD_DVBAPI
int aml_demod_init(void)
#else
static int __init aml_demod_init(void)
#endif
{
	int r = 0;

	pr_dbg("Amlogic Demod DVB-T/C DebugIF Init\n");

	init_waitqueue_head(&lock_wq);

	/* hook demod isr */
	/* r = request_irq(INT_DEMOD, &aml_demod_isr,
	 *              IRQF_SHARED, "aml_demod",
	 *              (void *)aml_demod_dev_id);
	 * if (r) {
	 *      pr_dbg("aml_demod irq register error.\n");
	 *      r = -ENOENT;
	 *      goto err0;
	 * }*/

	/* sysfs node creation */
	r = class_register(&aml_demod_class);
	if (r) {
		pr_dbg("create aml_demod class fail\r\n");
		goto err1;
	}

	r = alloc_chrdev_region(&aml_demod_devno, 0, 1, DEVICE_NAME);
	if (r < 0) {
		pr_dbg(KERN_ERR "aml_demod: faild to alloc major number\n");
		r = -ENODEV;
		goto err2;
	}

	aml_demod_cdevp = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!aml_demod_cdevp) {
		pr_dbg(KERN_ERR "aml_demod: failed to allocate memory\n");
		r = -ENOMEM;
		goto err3;
	}
	/* connect the file operation with cdev */
	cdev_init(aml_demod_cdevp, &aml_demod_fops);
	aml_demod_cdevp->owner = THIS_MODULE;
	/* connect the major/minor number to cdev */
	r = cdev_add(aml_demod_cdevp, aml_demod_devno, 1);
	if (r) {
		pr_dbg(KERN_ERR "aml_demod:failed to add cdev\n");
		goto err4;
	}

	aml_demod_dev = device_create(&aml_demod_class, NULL,
				      MKDEV(MAJOR(aml_demod_devno), 0), NULL,
				      DEVICE_NAME);

	if (IS_ERR(aml_demod_dev)) {
		pr_dbg("Can't create aml_demod device\n");
		goto err5;
	}
	pr_dbg("Amlogic Demod DVB-T/C DebugIF Init ok----------------\n");
#if defined(CONFIG_AM_AMDEMOD_FPGA_VER) && !defined(CONFIG_AM_DEMOD_DVBAPI)
	pr_dbg("sdio_init\n");
	sdio_init();
#endif
	aml_demod_ui_init();

	return 0;

err5:
	cdev_del(aml_demod_cdevp);
err4:
	kfree(aml_demod_cdevp);

err3:
	unregister_chrdev_region(aml_demod_devno, 1);

err2:
/*    free_irq(INT_DEMOD, (void *)aml_demod_dev_id);*/

err1:
	class_unregister(&aml_demod_class);

/*  err0:*/
	return r;
}

#ifdef CONFIG_AM_DEMOD_DVBAPI
void aml_demod_exit(void)
#else
static void __exit aml_demod_exit(void)
#endif
{
	pr_dbg("Amlogic Demod DVB-T/C DebugIF Exit\n");

	unregister_chrdev_region(aml_demod_devno, 1);
	device_destroy(&aml_demod_class, MKDEV(MAJOR(aml_demod_devno), 0));
	cdev_del(aml_demod_cdevp);
	kfree(aml_demod_cdevp);

	/*   free_irq(INT_DEMOD, (void *)aml_demod_dev_id); */

	class_unregister(&aml_demod_class);

	aml_demod_exit_ui();
}

#ifndef CONFIG_AM_DEMOD_DVBAPI
module_init(aml_demod_init);
module_exit(aml_demod_exit);

MODULE_LICENSE("GPL");
/*MODULE_AUTHOR(DRV_AUTHOR);*/
/*MODULE_DESCRIPTION(DRV_DESC);*/
#endif
