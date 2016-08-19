/*
	TurboSight TBS CI driver for CX23102
    	Copyright (C) 2014 Konstantin Dimitrov <kosio.dimitrov@gmail.com>

    	Copyright (C) 2014 TurboSight.com
*/

#ifndef TBSCXCI_H
#define TBSCXCI_H

#include "cx231xx.h"

#include <media/dvb_ca_en50221.h>

extern int tbscxci_read_attribute_mem(struct dvb_ca_en50221 *en50221, 
	int slot, int addr);
extern int tbscxci_write_attribute_mem(struct dvb_ca_en50221 *en50221, 
	int slot, int addr, u8 data);
extern int tbscxci_read_cam_control(struct dvb_ca_en50221 *en50221, 
	int slot, u8 addr);
extern int tbscxci_write_cam_control(struct dvb_ca_en50221 *en50221, 
	int slot, u8 addr, u8 data);
extern int tbscxci_slot_reset(struct dvb_ca_en50221 *en50221, 
	int slot);
extern int tbscxci_slot_shutdown(struct dvb_ca_en50221 *en50221, 
	int slot);
extern int tbscxci_slot_ts_enable(struct dvb_ca_en50221 *en50221, 
	int slot);
extern int tbscxci_poll_slot_status(struct dvb_ca_en50221 *en50221, 
	int slot, int open);
extern int tbscxci_init(struct cx231xx_dvb *adap, int nr);
extern void tbscxci_release(struct cx231xx_dvb *adap);

#endif
