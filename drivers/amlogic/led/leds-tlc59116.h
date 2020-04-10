#ifndef _TLC59116_H_
#define _TLC59116_H_

#define MAX_I2C_BUFFER_SIZE 65536

#define TLC59116_ID 0x23

struct tlc59116 {
	struct i2c_client *i2c;
	struct device *dev;
	struct led_classdev cdev;
	struct work_struct brightness_work;
	struct work_struct leds_work;
	struct workqueue_struct *leds_wq;
	int reset_gpio;
	unsigned char chipid;
	int imax;
};

static int tlc59116_i2c_write(struct tlc59116 *tlc59116,
			      unsigned char reg_addr,
			      unsigned char reg_data);
static int tlc59116_hw_reset(struct tlc59116 *tlc59116);

#endif

