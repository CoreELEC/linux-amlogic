/*
 * drivers/amlogic/media/dtv_demod/include/dvbs_diseqc.h
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
#ifndef __DVBS_DISEQC_H__
#define __DVBS_DISEQC_H__
#include "amlfrontend.h"

#define IRQ_STS_GAPBURST	0x01
#define IRQ_STS_TXFIFO64B	0x02
#define IRQ_STS_TXEND		0x04
#define IRQ_STS_TIMEOUT		0x08
#define IRQ_STS_TRFINISH	0x10
#define IRQ_STS_RXFIFO8B	0x20
#define IRQ_STS_RXEND		0x40

#define DISEQC_MSG_MAX_LEN	20

struct aml_def_diseqc_lnb {
	struct amldtvdemod_device_s *demod;
	struct gpio_desc *gpio_lnb_en;
	struct gpio_desc *gpio_lnb_sel;
	unsigned int lnb_en;
	unsigned int lnb_sel;
	unsigned int tone_on;
	unsigned int voltage;/*fe_sec_voltage*/
	/*unsigned char msg_len;*/
	/*unsigned char msg_cmd[DISEQC_MSG_MAX_LEN];*/
	struct completion rx_msg_ok;
	struct completion tx_msg_ok;
};

extern struct aml_def_diseqc_lnb aml_lnb;
extern u32 sendburst_on;
extern u32 diseqc_cmd_bypass;

void aml_def_set_lnb_en(int high_low);
void aml_def_set_lnb_sel(int high_low);
void demod_dump_reg_diseqc(void);
void aml_diseqc_attach(struct amldtvdemod_device_s *devp,
		       struct dvb_frontend *fe);
u32 aml_diseqc_send_cmd(struct dvb_diseqc_master_cmd *cmd);
void aml_diseqc_dbg_en(unsigned int val);
void aml_diseqc_isr(void);
void aml_diseqc_status(void);
void aml_diseqc_toneburst_sa(void);
void aml_diseqc_toneburst_sb(void);
void aml_diseqc_tone_on(u32 onoff);
void aml_diseqc_flag_tone_on(u32 onoff);
#endif
