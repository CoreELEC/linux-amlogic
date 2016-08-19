/*
	TurboSight TBS CI driver for CX23102
    	Copyright (C) 2014 Konstantin Dimitrov <kosio.dimitrov@gmail.com>

    	Copyright (C) 2014 TurboSight.com
*/

#include "tbscxci.h"

#define TBSCXCI_I2C_ADDR 0x1b

struct tbscxci_state {
	struct dvb_ca_en50221 ca;
	struct mutex ca_mutex;
	struct i2c_adapter *i2c_adap;
	int nr;
	void *priv; /* struct cx231xx_dvb *priv; */
};

int tbscxci_i2c_read(struct tbscxci_state *state)
{
	int ret;
	u8 buf = 0;

	struct i2c_msg msg = { .addr = TBSCXCI_I2C_ADDR, .flags = I2C_M_RD,
			.buf = &buf, .len = 1 };

	if (state->nr == 1)
		msg.addr -= 1;
		
	ret = i2c_transfer(state->i2c_adap, &msg, 1);

	if (ret != 1) {
		printk("tbscxci: read error=%d\n", ret);
		return -EREMOTEIO;
	}

	return buf;
};

int tbscxci_i2c_write(struct tbscxci_state *state,
	u8 addr, u8 data[], int len)
{
	int ret;
	unsigned char buf[len + 1];

	struct i2c_msg msg = { .addr = TBSCXCI_I2C_ADDR, .flags = 0,
			.buf = &buf[0], .len = len + 1 };

	if (state->nr == 1)
		msg.addr -= 1;
		
	memcpy(&buf[1], data, len);
	buf[0] = addr;

	ret = i2c_transfer(state->i2c_adap, &msg, 1);

	if (ret != 1) {
		/* printk("tbscxci: error=%d\n", ret); */
		return -EREMOTEIO;
	}

	return 0;
};

int tbscxci_read_cam_control(struct dvb_ca_en50221 *ca, 
	int slot, u8 address)
{
	struct tbscxci_state *state = ca->data;
	int ret;
	unsigned char data;

	if (slot != 0)
		return -EINVAL;

	mutex_lock(&state->ca_mutex);

	data = (address & 3);
	ret = tbscxci_i2c_write(state, 0x80, &data, 1);
	data = tbscxci_i2c_read(state);

	mutex_unlock(&state->ca_mutex);

	if (ret < 0)
		return ret;

	return data;
}

int tbscxci_write_cam_control(struct dvb_ca_en50221 *ca, int slot,
	u8 address, u8 value)
{
	struct tbscxci_state *state = ca->data;
	int ret;
	unsigned char data[2];

	if (slot != 0)
		return -EINVAL;

	mutex_lock(&state->ca_mutex);

	data[0] = (address & 3);
	data[1] = value;
	ret = tbscxci_i2c_write(state, 0x80, data, 2);

	mutex_unlock(&state->ca_mutex);

	if (ret < 0)
		return ret;

	return 0;
}

int tbscxci_read_attribute_mem(struct dvb_ca_en50221 *ca,
	int slot, int address)
{
	struct tbscxci_state *state = ca->data;
	int ret;
	unsigned char data;

	if (slot != 0)
		return -EINVAL;

	mutex_lock(&state->ca_mutex);

	data = (address & 0xff);
	ret = tbscxci_i2c_write(state,
		((address >> 8) & 0x7f), &data, 1);
	data = tbscxci_i2c_read(state);

	mutex_unlock(&state->ca_mutex);

	if (ret < 0)
		return ret;

	return data;
}

int tbscxci_write_attribute_mem(struct dvb_ca_en50221 *ca,
	int slot, int address, u8 value)
{
	struct tbscxci_state *state = ca->data;
	int ret;
	unsigned char data[2];

	if (slot != 0)
		return -EINVAL;

	mutex_lock(&state->ca_mutex);

	data[0] = (address & 0xff);
	data[1] = value;
	ret = tbscxci_i2c_write(state,
		((address >> 8) & 0x7f), data, 2);

	mutex_unlock(&state->ca_mutex);

	if (ret < 0)
		return ret;

	return 0;
}

static int tbscxci_set_video_port(struct dvb_ca_en50221 *ca,
	int slot, int enable)
{
	struct tbscxci_state *state = ca->data;
	struct cx231xx_dvb *adap = state->priv;
	unsigned char data;
	int ret;

	if (slot != 0)
		return -EINVAL;

	mutex_lock(&state->ca_mutex);

	data = enable & 1;
	ret = tbscxci_i2c_write(state, 0xc0, &data, 1);

	mutex_unlock(&state->ca_mutex);

	if (ret < 0)
		return ret;

	printk("tbscxci: Adapter %d CI slot %sabled\n",
//		adap->fe->dvb->num,
		adap->adapter.num,
		enable ? "en" : "dis");

	return 0;
}

int tbscxci_slot_shutdown(struct dvb_ca_en50221 *ca, int slot)
{
	return tbscxci_set_video_port(ca, slot, /* enable */ 0);
}

int tbscxci_slot_ts_enable(struct dvb_ca_en50221 *ca, int slot)
{
	return tbscxci_set_video_port(ca, slot, /* enable */ 1);
}

int tbscxci_slot_reset(struct dvb_ca_en50221 *ca, int slot)
{
	struct tbscxci_state *state = ca->data;
	int ret;
	unsigned char data;

	if (slot != 0)
		return -EINVAL;
	
	mutex_lock (&state->ca_mutex);

	data = 1;
	ret = tbscxci_i2c_write(state, 0xc1, &data, 1);
	msleep (5);

	data = 0;
	ret = tbscxci_i2c_write(state, 0xc1, &data, 1);
	msleep (1400);

	mutex_unlock (&state->ca_mutex);

	if (ret != 0)
		return ret;

	return 0;
}

int tbscxci_poll_slot_status(struct dvb_ca_en50221 *ca, 
	int slot, int open)
{
	struct tbscxci_state *state = ca->data;
	struct cx231xx_dvb *adap = state->priv;
	struct cx231xx *dev = adap->adapter.priv;
	unsigned char data;

	if (slot != 0)
		return -EINVAL;

	mutex_lock(&state->ca_mutex);

	cx231xx_get_gpio_bit(dev, dev->gpio_dir, &dev->gpio_val);
	data = dev->gpio_val >> (state->nr ? 25 : 27);
	
	mutex_unlock(&state->ca_mutex);

	if (data & 1) {
		return (DVB_CA_EN50221_POLL_CAM_PRESENT |
			DVB_CA_EN50221_POLL_CAM_READY);
	} else {
		return 0;
	}
}

int tbscxci_init(struct cx231xx_dvb *adap, int nr)
{
	struct cx231xx *dev = adap->adapter.priv;
	struct tbscxci_state *state;
	int ret;
	unsigned char data;

	/* allocate memory for the internal state */
	state = kzalloc(sizeof(struct tbscxci_state), GFP_KERNEL);
	if (state == NULL) {
		ret = -ENOMEM;
		goto error1;
	}

	adap->adap_priv = state;
	
	state->nr = nr;

	mutex_init(&state->ca_mutex);

	state->i2c_adap = cx231xx_get_i2c_adap(dev, dev->board.demod_i2c_master[nr]);

	switch (state->nr) {
	case 0:
		
		dev->gpio_dir &= ~(1 << 27);
		cx231xx_set_gpio_bit(dev, dev->gpio_dir, dev->gpio_val);
		break;
	case 1:
		dev->gpio_dir &= ~(1 << 25);
		cx231xx_set_gpio_bit(dev, dev->gpio_dir, dev->gpio_val);
		break;
	}
	
	state->ca.owner = THIS_MODULE;
	state->ca.read_attribute_mem = tbscxci_read_attribute_mem;
	state->ca.write_attribute_mem = tbscxci_write_attribute_mem;
	state->ca.read_cam_control = tbscxci_read_cam_control;
	state->ca.write_cam_control = tbscxci_write_cam_control;
	state->ca.slot_reset = tbscxci_slot_reset;
	state->ca.slot_shutdown = tbscxci_slot_shutdown;
	state->ca.slot_ts_enable = tbscxci_slot_ts_enable;
	state->ca.poll_slot_status = tbscxci_poll_slot_status;
	state->ca.data = state;
	state->priv = adap;

	data = 1;
	tbscxci_i2c_write(state, 0xc2, &data, 1);
	data = tbscxci_i2c_read(state);

	switch (data) {
		case 0x62:
		case 0x60:
			printk("tbscxci: Initializing TBS CX CI#%d slot\n", nr);
			break;
		default:
			ret = -EREMOTEIO;
			goto error2;
	}

	ret = dvb_ca_en50221_init(&adap->adapter, &state->ca,
		/* flags */ 0, /* n_slots */ 1);
	if (ret != 0) goto error2;

	printk("tbscxci: Adapter %d CI slot initialized\n",
		adap->adapter.num);

	ret = tbscxci_poll_slot_status(&state->ca, 0, 0);
	if (0 == ret)
		tbscxci_set_video_port(&state->ca, /* slot */ 0, /* enable */ 0);

	return 0;
	
error2: 
	//memset (&state->ca, 0, sizeof (state->ca)); 
	kfree(state);
error1:
	printk("tbscxci: Adapter %d CI slot initialization failed\n",
		adap->adapter.num);
	return ret;
}

void tbscxci_release(struct cx231xx_dvb *adap)
{
	struct tbscxci_state *state;

	if (NULL == adap) return;

	state = (struct tbscxci_state *)adap->adap_priv;

	if (NULL == state) return;

	if (NULL == state->ca.data) return;

	dvb_ca_en50221_release(&state->ca);
	//memset(&state->ca, 0, sizeof(state->ca));
	kfree(state);
}
