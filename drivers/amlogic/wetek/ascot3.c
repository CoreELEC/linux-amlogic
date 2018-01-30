/*
 * ascot3.c
 *
 * Sony Ascot3 DVB-T/T2/C tuner driver
 *
 * Copyright (C) 2015 Sasa Savic <sasa.savic.sr@gmail.com>
 *
 * Based on ascot2e driver
 *
 * Copyright 2012 Sony Corporation
 * Copyright (C) 2014 NetUP Inc.
 * Copyright (C) 2014 Sergey Kozlov <serjk@netup.ru>
 * Copyright (C) 2014 Abylay Ospan <aospan@netup.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
  */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/dvb/frontend.h>
#include <linux/types.h>
#include "ascot3.h"
#include "dvb_frontend.h"

#define MAX_WRITE_REGSIZE 32

enum ascot3_state {
	STATE_UNKNOWN,
	STATE_SLEEP,
	STATE_ACTIVE
};

struct ascot3_priv {
	u32			frequency;
	u8			i2c_address;
	struct i2c_adapter	*i2c;
	enum ascot3_state	state;
};

enum ascot3_tv_system_t {
	ASCOT3_DTV_DVBT_5,
	ASCOT3_DTV_DVBT_6,
	ASCOT3_DTV_DVBT_7,
	ASCOT3_DTV_DVBT_8,
	ASCOT3_DTV_DVBT2_1_7,
	ASCOT3_DTV_DVBT2_5,
	ASCOT3_DTV_DVBT2_6,
	ASCOT3_DTV_DVBT2_7,
	ASCOT3_DTV_DVBT2_8,
	ASCOT3_DTV_DVBC_6,
	ASCOT3_DTV_DVBC_8,
	ASCOT3_DTV_DVBC2_6,
	ASCOT3_DTV_DVBC2_8,
	ASCOT3_DTV_UNKNOWN
};

struct ascot3_band_sett {
	u8 	outlmt;
	u8	rf_gain;
	u8	if_bpf_gc;
	u8	rfovld_det_lv1_vl;
	u8	rfovld_det_lv1_vh;
	u8	rfovld_det_lv1_u;
	u8	ifovld_det_lv_vl;
	u8	ifovld_det_lv_vh;
	u8	ifovld_det_lv_u;
	u8	if_bpf_f0;
	u8	bw;
	u8	fif_offset;
	u8	bw_offset;
	u8	agc_sel;
	u8	if_out_sel;	
	u8	is_lowerlocal;	
};

#define ASCOT3_AUTO		0xff
#define ASCOT3_OFFSET(ofs)	((u8)(ofs) & 0x1F)
#define ASCOT3_BW_6		0x00
#define ASCOT3_BW_7		0x01
#define ASCOT3_BW_8		0x02
#define ASCOT3_BW_1_7	0x03

static struct ascot3_band_sett ascot3_sett[] = {
	{ 0x00, ASCOT3_AUTO, 0x09, 0x0C, 0x0C, 0x0C, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_6, ASCOT3_OFFSET(-8), ASCOT3_OFFSET(-3), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 }, 
	{ 0x00, ASCOT3_AUTO, 0x09, 0x0C, 0x0C, 0x0C, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_6,  ASCOT3_OFFSET(-8), ASCOT3_OFFSET(-3), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 },
	{ 0x00, ASCOT3_AUTO, 0x09, 0x0C, 0x0C, 0x0C, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_7,  ASCOT3_OFFSET(-6), ASCOT3_OFFSET(-5), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 },
	{ 0x00, ASCOT3_AUTO, 0x09, 0x0C, 0x0C, 0x0C, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_8,  ASCOT3_OFFSET(-4), ASCOT3_OFFSET(-6), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 }, 
	{ 0x00, ASCOT3_AUTO, 0x09, 0x0C, 0x0C, 0x0C, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_1_7,ASCOT3_OFFSET(-10),ASCOT3_OFFSET(-10),ASCOT3_AUTO, ASCOT3_AUTO, 0x00 }, 
	{ 0x00, ASCOT3_AUTO, 0x09, 0x0C, 0x0C, 0x0C, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_6,  ASCOT3_OFFSET(-8), ASCOT3_OFFSET(-3), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 },
	{ 0x00, ASCOT3_AUTO, 0x09, 0x0C, 0x0C, 0x0C, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_6,  ASCOT3_OFFSET(-8), ASCOT3_OFFSET(-3), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 }, 
	{ 0x00, ASCOT3_AUTO, 0x09, 0x0C, 0x0C, 0x0C, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_7,  ASCOT3_OFFSET(-6), ASCOT3_OFFSET(-5), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 }, 
	{ 0x00, ASCOT3_AUTO, 0x09, 0x0C, 0x0C, 0x0C, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_8,  ASCOT3_OFFSET(-4), ASCOT3_OFFSET(-6), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 },
	{ 0x00, ASCOT3_AUTO, 0x05, 0x09, 0x09, 0x09, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_6,  ASCOT3_OFFSET(-6), ASCOT3_OFFSET(-4), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 }, 
	{ 0x00, ASCOT3_AUTO, 0x05, 0x09, 0x09, 0x09, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_8,  ASCOT3_OFFSET(-2), ASCOT3_OFFSET(-3), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 }, 
	{ 0x00, ASCOT3_AUTO, 0x03, 0x0A, 0x0A, 0x0A, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_6,  ASCOT3_OFFSET(-6), ASCOT3_OFFSET(-2), ASCOT3_AUTO, ASCOT3_AUTO, 0x00 },
	{ 0x00, ASCOT3_AUTO, 0x03, 0x0A, 0x0A, 0x0A, 0x02, 0x02, 0x02, 0x00, 
	  ASCOT3_BW_8,  ASCOT3_OFFSET(-2), ASCOT3_OFFSET(0),  ASCOT3_AUTO, ASCOT3_AUTO, 0x00 }
};
  
static int ascot3_write_regs(struct ascot3_priv *priv,
			      u8 reg, const u8 *data, u32 len)
{
	int ret;
	u8 buf[MAX_WRITE_REGSIZE + 1];
	struct i2c_msg msg[1] = {
		{
			.addr = priv->i2c_address,
			.flags = 0,
			.len = len + 1,
			.buf = buf,
		}
	};

	if (len + 1 >= sizeof(buf)) {
		dev_warn(&priv->i2c->dev,"wr reg=%04x: len=%d is too big!\n",
			 reg, len + 1);
		return -E2BIG;
	}


	buf[0] = reg;
	memcpy(&buf[1], data, len);
	ret = i2c_transfer(priv->i2c, msg, 1);
	if (ret >= 0 && ret != 1)
		ret = -EREMOTEIO;
	if (ret < 0) {
		dev_warn(&priv->i2c->dev,
			"i2c wr failed=%d reg=%02x len=%d\n",
			ret, reg, len);
		return ret;
	}
	return 0;
}

static int ascot3_write_reg(struct ascot3_priv *priv, u8 reg, u8 val)
{
	return ascot3_write_regs(priv, reg, &val, 1);
}

static int ascot3_read_regs(struct ascot3_priv *priv,
			     u8 reg, u8 *val, u32 len)
{
	int ret;
	struct i2c_msg msg[2] = {
		{
			.addr = priv->i2c_address,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		}, {
			.addr = priv->i2c_address,
			.flags = I2C_M_RD,
			.len = len,
			.buf = val,
		}
	};

	ret = i2c_transfer(priv->i2c, &msg[0], 1);
	if (ret >= 0 && ret != 1)
		ret = -EREMOTEIO;
	if (ret < 0) {
		dev_warn(&priv->i2c->dev,
			"I2C rw failed=%d addr=%02x reg=%02x\n",
			ret, priv->i2c_address, reg);
		return ret;
	}
	ret = i2c_transfer(priv->i2c, &msg[1], 1);
	if (ret >= 0 && ret != 1)
		ret = -EREMOTEIO;
	if (ret < 0) {
		dev_warn(&priv->i2c->dev,
			"i2c rd failed=%d addr=%02x reg=%02x\n",
			 ret, priv->i2c_address, reg);
		return ret;
	}

	return 0;
}

static int ascot3_read_reg(struct ascot3_priv *priv, u8 reg, u8 *val)
{
	return ascot3_read_regs(priv, reg, val, 1);
}

static int ascot3_set_reg_bits(struct ascot3_priv *priv,
				u8 reg, u8 data, u8 mask)
{
	int res;
	u8 rdata;

	if (mask != 0xff) {
		res = ascot3_read_reg(priv, reg, &rdata);
		if (res != 0)
			return res;
		data = ((data & mask) | (rdata & (mask ^ 0xFF)));
	}
	return ascot3_write_reg(priv, reg, data);
}

static int ascot3_enter_power_save(struct ascot3_priv *priv)
{
	u8 data[3];
	dev_dbg(&priv->i2c->dev, "%s()\n", __func__);
	
	if (priv->state == STATE_SLEEP)
		return 0;
		
	/* Loop Through setting And RFIN matching in Power Save */
	ascot3_write_reg(priv, 0x67, 0x06);
	/* Disable IF signal output (IF_OUT_SEL setting) */
	ascot3_set_reg_bits(priv, 0x74, 0x02, 0x03);
	/* Power save setting for analog block */
	data[0] = 0x15;
	data[1] = 0x00;
	data[2] = 0x00;
	ascot3_write_regs(priv, 0x5E, data, 3);
	/* Standby setting for CPU */
	ascot3_write_reg(priv, 0x88, 0x00);
	/* Standby setting for internal logic block */
	ascot3_write_reg(priv, 0x87, 0xC0);	
	priv->state = STATE_SLEEP;	
	return 0;
}

static int ascot3_init(struct dvb_frontend *fe)
{
	struct ascot3_priv *priv = fe->tuner_priv;

	dev_dbg(&priv->i2c->dev, "%s()\n", __func__);
	return 0;
}

static int ascot3_release(struct dvb_frontend *fe)
{
	struct ascot3_priv *priv = fe->tuner_priv;

	dev_dbg(&priv->i2c->dev, "%s()\n", __func__);
	kfree(fe->tuner_priv);
	fe->tuner_priv = NULL;
	return 0;
}

static int ascot3_sleep(struct dvb_frontend *fe)
{
	struct ascot3_priv *priv = fe->tuner_priv;

	dev_dbg(&priv->i2c->dev, "%s()\n", __func__);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
		
	ascot3_enter_power_save(priv);
	
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);
		
	return 0;
}

static enum ascot3_tv_system_t ascot3_get_tv_system(struct dvb_frontend *fe)
{
	enum ascot3_tv_system_t system = ASCOT3_DTV_UNKNOWN;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct ascot3_priv *priv = fe->tuner_priv;

	if (p->delivery_system == SYS_DVBT) {
		if (p->bandwidth_hz <= 5000000)
			system = ASCOT3_DTV_DVBT_5;
		else if (p->bandwidth_hz <= 6000000)
			system = ASCOT3_DTV_DVBT_6;
		else if (p->bandwidth_hz <= 7000000)
			system = ASCOT3_DTV_DVBT_7;
		else if (p->bandwidth_hz <= 8000000)
			system = ASCOT3_DTV_DVBT_8;
		else {
			system = ASCOT3_DTV_DVBT_8;
			p->bandwidth_hz = 8000000;
		}
	} else if (p->delivery_system == SYS_DVBT2) {
		if (p->bandwidth_hz <= 5000000)
			system = ASCOT3_DTV_DVBT2_5;
		else if (p->bandwidth_hz <= 6000000)
			system = ASCOT3_DTV_DVBT2_6;
		else if (p->bandwidth_hz <= 7000000)
			system = ASCOT3_DTV_DVBT2_7;
		else if (p->bandwidth_hz <= 8000000)
			system = ASCOT3_DTV_DVBT2_8;
		else {
			system = ASCOT3_DTV_DVBT2_8;
			p->bandwidth_hz = 8000000;
		}
	} else if (p->delivery_system == SYS_DVBC_ANNEX_A) {
		if (p->bandwidth_hz <= 6000000)
			system = ASCOT3_DTV_DVBC_6;
		else if (p->bandwidth_hz <= 8000000)
			system = ASCOT3_DTV_DVBC_8;
	}
	dev_dbg(&priv->i2c->dev,
		"%s(): ASCOT2E DTV system %d (delsys %d, bandwidth %d)\n",
		__func__, (int)system, p->delivery_system, p->bandwidth_hz);
	return system;
}

static int ascot3_set_params(struct dvb_frontend *fe)
{
	u8 data[20];
	u32 frequency;
	enum ascot3_tv_system_t tv_system;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct ascot3_priv *priv = fe->tuner_priv;

	dev_dbg(&priv->i2c->dev, "%s(): tune frequency %dkHz\n",
		__func__, p->frequency / 1000);
	tv_system = ascot3_get_tv_system(fe);

	if (tv_system == ASCOT3_DTV_UNKNOWN) {
		dev_dbg(&priv->i2c->dev, "%s(): unknown DTV system\n",
			__func__);
		return -EINVAL;
	}

	frequency = roundup(p->frequency / 1000, 25);

	/* Disable IF signal output (IF_OUT_SEL setting) */
	ascot3_set_reg_bits(priv, 0x74, 0x02, 0x03);
	/* Clock enable for internal logic block, CPU wake-up */
	data[0] = 0xC4;
	data[1] = 0x40;
	ascot3_write_regs(priv, 0x87, data, 2);
	
	/* Initial setting for internal analog block */	
	if (tv_system == ASCOT3_DTV_DVBC_6 ||
			tv_system == ASCOT3_DTV_DVBC_8) {		
		data[0] = 0x16;
		data[1] = 0x26;
	} else {		
		data[0] = 0x10;
		data[1] = 0x20;
	}
	ascot3_write_regs(priv, 0x91, data, 2);
	
	/* Setting for analog block */
	data[0] = 0x00;
	data[1] = (u8)(ascot3_sett[tv_system].is_lowerlocal & 0x01);
	ascot3_write_regs(priv, 0x9C, data, 2);
	
	/* Enable for analog block */
	data[0] = 0xEE;
	data[1] = 0x02;
	data[2] = 0x1E;
	/* Tuning setting for CPU */
	data[3] = 0x67;
	/* Setting for PLL reference divider (REF_R) */
	data[4] = 0x02;
	/* Tuning setting for analog block*/
	if (tv_system == ASCOT3_DTV_DVBC_6 ||
			tv_system == ASCOT3_DTV_DVBC_8) {		
		data[5] = 0x50;
		data[6] = 0x78;
		data[7] = 0x08;
		data[8] = 0x30;
	} else {		
		data[5] = 0xAF;
		data[6] = 0x78;
		data[7] = 0x08;
		data[8] = 0x30;
	}
	ascot3_write_regs(priv, 0x5E, data, 9);
	
	/* Setting for IFOUT_LIMIT */
	data[0] = (u8)(ascot3_sett[tv_system].outlmt & 0x03);
	/* Setting for IF BPF buffer gain */
	/* RF_GAIN setting */
	if (ascot3_sett[tv_system].rf_gain == ASCOT3_AUTO) 
		data[1] = 0x80;
	else
		data[1] = (u8)((ascot3_sett[tv_system].rf_gain << 4) & 0x70);
	
	/* IF_BPF_GC setting */
	data[1] |= (u8)(ascot3_sett[tv_system].if_bpf_gc & 0x0F);

	/* Setting for internal RFAGC */
	data[2] = 0x00;	
	if (frequency <= 172000) {
		data[3] = (u8)(ascot3_sett[tv_system].rfovld_det_lv1_vl & 0x0F);
		data[4] = (u8)(ascot3_sett[tv_system].ifovld_det_lv_vl & 0x07);
	} else if (frequency <= 464000) {		
		data[3] = (u8)(ascot3_sett[tv_system].rfovld_det_lv1_vh & 0x0F);
		data[4] = (u8)(ascot3_sett[tv_system].ifovld_det_lv_vh & 0x07);
	} else {	
		data[3] = (u8)(ascot3_sett[tv_system].rfovld_det_lv1_u & 0x0F);
		data[4] = (u8)(ascot3_sett[tv_system].ifovld_det_lv_u & 0x07);
	}	
	data[4] |= 0x20;
	
	/* Setting for IF frequency and bandwidth */
	data[5] = (u8)((ascot3_sett[tv_system].if_bpf_f0 << 4) & 0x30);
	data[5] |= (u8)(ascot3_sett[tv_system].bw & 0x03);
	data[6] = (u8)(ascot3_sett[tv_system].fif_offset & 0x1F);
	data[7] = (u8)(ascot3_sett[tv_system].bw_offset & 0x1F);
	
	/* RF tuning frequency setting */
	data[8] = (u8)(frequency & 0xFF);         /* 0x10: FRF_L */
	data[9] = (u8)((frequency >> 8) & 0xFF);  /* 0x11: FRF_M */
	data[10] = (u8)((frequency >> 16) & 0x0F); /* 0x12: FRF_H (bit[3:0]) */
	/* Tuning command */
	data[11] = 0xFF;
	/* Enable IF output, AGC and IFOUT pin selection */
	data[12] = 0x11;
	/* Tuning setting for analog block*/
	if (tv_system == ASCOT3_DTV_DVBC_6 ||
			tv_system == ASCOT3_DTV_DVBC_8) {		
		data[13] = 0xD9;
		data[14] = 0x0F;
		data[15] = 0x25;
		data[16] = 0x87;
	} else {		
		data[13] = 0x99;
		data[14] = 0x00;
		data[15] = 0x24;
		data[16] = 0x87;
	}
	ascot3_write_regs(priv, 0x68, data, 17);

	msleep(50);
	
	priv->state = STATE_ACTIVE;	
	ascot3_write_reg(priv, 0x88, 0x00);
	ascot3_write_reg(priv, 0x87, 0xC0);
	
	priv->frequency = frequency;
	return 0;
}

static int ascot3_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct ascot3_priv *priv = fe->tuner_priv;

	*frequency = priv->frequency * 1000;
	return 0;
}

static struct dvb_tuner_ops ascot3_tuner_ops = {
	.info = {
		.name = "Sony ASCOT3",
		.frequency_min = 1000000,
		.frequency_max = 1200000000,
		.frequency_step = 25000,
	},
	.init = ascot3_init,
	.release = ascot3_release,
	.sleep = ascot3_sleep,
	.set_params = ascot3_set_params,
	.get_frequency = ascot3_get_frequency,
};

struct dvb_frontend *ascot3_attach(struct dvb_frontend *fe,
				    const struct ascot3_config *config,
				    struct i2c_adapter *i2c)
{
	u8 data[20];
	struct ascot3_priv *priv = NULL;

	priv = kzalloc(sizeof(struct ascot3_priv), GFP_KERNEL);
	if (priv == NULL)
		return NULL;
	priv->i2c_address = config->i2c_address;
	priv->i2c = i2c;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	/* Check if tuner is Sony ASCOT3 */
	data[0] = 0x00;
	ascot3_read_reg(priv, 0x7F, data);
	if (((data[0] & 0xF0) != 0xC0) && ((data[0] & 0xF0) != 0xD0)) {
		kfree(priv);
		return NULL;	
	}	
	/* Initial setting for internal logic block */	
	data[0] = 0x7A;
	data[1] = 0x01;
	ascot3_write_regs(priv, 0x99, data, 2);
	/* 16 MHz xTal frequency */
	data[0] = 16;
	/* Driver current setting for crystal oscillator */	
	/* Load capacitance setting for crystal oscillator */
	data[1] = 0x84;
	data[2] = 0xB0;
	/* Setting for REFOUT signal output */
	data[3] = 0x00;
	/* GPIO0, GPIO1 port setting */
	data[4] = 0x00;
	data[5] = 0x00;
	/* Logic wake up, CPU boot */
	data[6] = 0xC4;
	data[7] = 0x40;
	/* For burst-write */
	data[8] = 0x10;
	/* Setting for internal RFAGC */
	data[9] = 0x00;
	data[10] = 0x45;
	data[11] = 0x56;
	/* Setting for analog block */
	data[12] = 0x07;
	/* Initial setting for internal analog block */
	data[13] = 0x1C;
	data[14] = 0x3F;
	data[15] = 0x02;
	data[16] = 0x10;
	data[17] = 0x20;
	data[18] = 0x0A;
	data[19] = 0x00;
	ascot3_write_regs(priv, 0x81, data, 20);
	/* Setting for internal RFAGC */
	ascot3_write_reg(priv, 0x9B, 0x00);
	msleep(10);
	/* VCO current setting */
	data[0] = 0x8D;
	data[1] = 0x06;
	ascot3_write_regs(priv, 0x17, data, 2);
	msleep(1);
	ascot3_read_reg(priv, 0x19, data);
	ascot3_write_reg(priv, 0x95, ((data[0] >> 4) & 0x0F));
	ascot3_enter_power_save(priv);	
	/* Load capacitance control setting for crystal oscillator */
	ascot3_write_reg(priv, 0x80, 0x01);
	
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	memcpy(&fe->ops.tuner_ops, &ascot3_tuner_ops,
				sizeof(struct dvb_tuner_ops));
	fe->tuner_priv = priv;
	dev_info(&priv->i2c->dev,
		"Sony ASCOT3 attached on addr=%x at I2C adapter %p\n",
		priv->i2c_address, priv->i2c);
	return fe;
}
EXPORT_SYMBOL(ascot3_attach);

MODULE_DESCRIPTION("Sony ASCOT3 terr/cab tuner driver");
MODULE_AUTHOR("sasa.savic.sr@gmail.com");
MODULE_LICENSE("GPL");
