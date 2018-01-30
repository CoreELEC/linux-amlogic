/*
 * Sony CXD2837 DVB-T/T2/C demodulator driver
 *
 * Copyright (C) 2010-2013 Digital Devices GmbH
 * Copyright (C) 2014 Sasa Savic <sasa.savic.sr@gmail.com>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <asm/div64.h>
#include "dvb_math.h"
#include "dvb_frontend.h"
#include "cxd2837.h"


struct ts_clk {
	u8 serialClkMode;      
	u8 serialDutyMode;
	u8 tsClkPeriod; 
	u8 clkSelTsIf;
};

struct cxd_state {
	struct dvb_frontend   frontend;
	struct i2c_adapter   *i2c;
	struct mutex          mutex;

	u8  adrt;
	u8  curbankt;

	u8  adrx;
	u8  curbankx;
	
	enum fe_delivery_system delivery_system;
	u32 freq;
	
	enum EDemodState state;	
	enum xtal_freq xtal;

	u8	IF_AGC;
	u8    IF_FS;
	int   ContinuousClock;
	int   SerialMode;
	u8    SerialClockFrequency;
	u8	ErrorPolarity;
	u8	ClockPolarity;
	u8	RfainMon;
	u32   LockTimeout;
	u32   TSLockTimeout;
	u32   L1PostTimeout;
	u32   DataSliceID;
	int   FirstTimeLock;
	u32   plp;
	u32   last_status;

	u32   bandwidth;
	u32   bw;

	unsigned long tune_time;

	u32   LastBERNominator;
	u32   LastBERDenominator;
	u8    BERScaleMax;
};
static int i2c_write(struct i2c_adapter *adap, u8 adr, u8 *data, int len)
{
	int ret;
	struct i2c_msg msg[1] = {
			{
					.addr = adr, 
					.flags = 0,
					.buf = data,
					.len = len,
			}
	};

	ret = i2c_transfer(adap, msg, 1);
	if (ret == 1) {
		ret = 0;
	} else {
			dev_warn(&adap->dev, "i2c wr failed=%d reg=%02x "
							, ret, data[0]);
			ret = -EREMOTEIO;
	}
	
	return ret;
}

static int writeregs(struct cxd_state *state, u8 adr, u8 reg,
		     u8 *regd, u16 len)
{
	u8 data[len + 1];

	data[0] = reg;
	memcpy(data + 1, regd, len);
	return i2c_write(state->i2c, adr, data, len + 1);
}

static int writereg(struct cxd_state *state, u8 adr, u8 reg, u8 dat)
{
	u8 mm[2] = {reg, dat};

	return i2c_write(state->i2c, adr, mm, 2);
}

static int i2c_read(struct i2c_adapter *adap,
		    u8 adr, u8 *msg, int len, u8 *answ, int alen)
{
	int ret;
	struct i2c_msg msgs[2] = { 
			{ 
					.addr = adr, 
					.flags = 0,
				    .buf = msg,
					.len = len
			}, { 
					.addr = adr,
					.flags = I2C_M_RD,					
				    .buf = answ, 
					.len = alen, 
			} 
	};
	ret = i2c_transfer(adap, msgs, 2);
	if (ret == 2) {
		ret = 0;
	} else {
			dev_warn(&adap->dev, "i2c rd failed=%d reg=%02x "
							, ret, *msg);
			ret = -EREMOTEIO;
	}
	return ret;
}

static int readregs(struct cxd_state *state, u8 adr, u8 reg,
		    u8 *val, int count)
{
	return i2c_read(state->i2c, adr, &reg, 1, val, count);
}

static int readregst_unlocked(struct cxd_state *cxd, u8 bank,
			      u8 Address, u8 *pValue, u16 count)
{
	int status = 0;

	if (bank != 0xFF && cxd->curbankt != bank) {
		status = writereg(cxd, cxd->adrt, 0, bank);
		if (status < 0) {
			cxd->curbankt = 0xFF;
			return status;
		}
		cxd->curbankt = bank;
	}
	status = readregs(cxd, cxd->adrt, Address, pValue, count);
	return status;
}

static int readregst(struct cxd_state *cxd, u8 Bank,
		     u8 Address, u8 *pValue, u16 count)
{
	int status;

	mutex_lock(&cxd->mutex);
	status = readregst_unlocked(cxd, Bank, Address, pValue, count);
	mutex_unlock(&cxd->mutex);
	return status;
}

static int readregsx_unlocked(struct cxd_state *cxd, u8 Bank,
			      u8 Address, u8 *pValue, u16 count)
{
	int status = 0;

	if (Bank != 0xFF && cxd->curbankx != Bank) {
		status = writereg(cxd, cxd->adrx, 0, Bank);
		if (status < 0) {
			cxd->curbankx = 0xFF;
			return status;
		}
		cxd->curbankx = Bank;
	}
	status = readregs(cxd, cxd->adrx, Address, pValue, count);
	return status;
}

static int readregsx(struct cxd_state *cxd, u8 Bank,
		     u8 Address, u8 *pValue, u16 count)
{
	int status;

	mutex_lock(&cxd->mutex);
	status = readregsx_unlocked(cxd, Bank, Address, pValue, count);
	mutex_unlock(&cxd->mutex);
	return status;
}

static int writeregsx_unlocked(struct cxd_state *cxd, u8 Bank,
			       u8 Address, u8 *pValue, u16 count)
{
	int status = 0;

	if (Bank != 0xFF && cxd->curbankx != Bank) {
		status = writereg(cxd, cxd->adrx, 0, Bank);
		if (status < 0) {
			cxd->curbankx = 0xFF;
			return status;
		}
		cxd->curbankx = Bank;
	}
	status = writeregs(cxd, cxd->adrx, Address, pValue, count);
	return status;
}

static int writeregsx(struct cxd_state *cxd, u8 Bank, u8 Address,
		      u8 *pValue, u16 count)
{
	int status;

	mutex_lock(&cxd->mutex);
	status = writeregsx_unlocked(cxd, Bank, Address, pValue, count);
	mutex_unlock(&cxd->mutex);
	return status;
}

static int writeregx(struct cxd_state *cxd, u8 Bank, u8 Address, u8 val)
{
	return writeregsx(cxd, Bank, Address, &val, 1);
}

static int writeregst_unlocked(struct cxd_state *cxd, u8 Bank,
			       u8 Address, u8 *pValue, u16 count)
{
	int status = 0;

	if (Bank != 0xFF && cxd->curbankt != Bank) {
		status = writereg(cxd, cxd->adrt, 0, Bank);
		if (status < 0) {
			cxd->curbankt = 0xFF;
			return status;
		}
		cxd->curbankt = Bank;
	}
	status = writeregs(cxd, cxd->adrt, Address, pValue, count);
	return status;
}

static int writeregst(struct cxd_state *cxd, u8 Bank, u8 Address,
		      u8 *pValue, u16 count)
{
	int status;

	mutex_lock(&cxd->mutex);
	status = writeregst_unlocked(cxd, Bank, Address, pValue, count);
	mutex_unlock(&cxd->mutex);
	return status;
}

static int writeregt(struct cxd_state *cxd, u8 Bank, u8 Address, u8 val)
{
	return writeregst(cxd, Bank, Address, &val, 1);
}

static int writebitsx(struct cxd_state *cxd, u8 Bank, u8 Address,
		      u8 Value, u8 Mask)
{
	int status = 0;
	u8 tmp;

	mutex_lock(&cxd->mutex);
	status = readregsx_unlocked(cxd, Bank, Address, &tmp, 1);
	if (status < 0)
		return status;
	tmp = (tmp & ~Mask) | Value;
	status = writeregsx_unlocked(cxd, Bank, Address, &tmp, 1);
	mutex_unlock(&cxd->mutex);
	return status;
}

static int writebitst(struct cxd_state *cxd, u8 Bank, u8 Address,
		      u8 Value, u8 Mask)
{
	int status = 0;
	u8 Tmp = 0x00;

	mutex_lock(&cxd->mutex);
	status = readregst_unlocked(cxd, Bank, Address, &Tmp, 1);
	if (status < 0)
		return status;
	Tmp = (Tmp & ~Mask) | Value;
	status = writeregst_unlocked(cxd, Bank, Address, &Tmp, 1);
	mutex_unlock(&cxd->mutex);
	return status;
}

static int freeze_regst(struct cxd_state *cxd)
{
	mutex_lock(&cxd->mutex);
	return writereg(cxd, cxd->adrt, 1, 1);
}

static int unfreeze_regst(struct cxd_state *cxd)
{
	int status = 0;

	status = writereg(cxd, cxd->adrt, 1, 0);
	mutex_unlock(&cxd->mutex);
	return status;
}

static inline u32 MulDiv32(u32 a, u32 b, u32 c)
{
	u64 tmp64;

	tmp64 = ((u64)a * (u64)b) + (u64)20500000;
	do_div(tmp64, c);

	return (u32) tmp64;
}

/* TPSData[0] [7:6]  CNST[1:0] */
/* TPSData[0] [5:3]  HIER[2:0] */
/* TPSData[0] [2:0]  HRATE[2:0] */
/* TPSData[1] [7:5]  LRATE[2:0] */
/* TPSData[1] [4:3]  GI[1:0] */
/* TPSData[1] [2:1]  MODE[1:0] */
/* TPSData[2] [7:6]  FNUM[1:0] */
/* TPSData[2] [5:0]  LENGTH_INDICATOR[5:0] */
/* TPSData[3] [7:0]  CELLID[15:8] */
/* TPSData[4] [7:0]  CELLID[7:0] */
/* TPSData[5] [5:0]  RESERVE_EVEN[5:0] */
/* TPSData[6] [5:0]  RESERVE_ODD[5:0] */

static int read_tps(struct cxd_state *state, u8 *tps)
{
	if (state->last_status != 0x1f)
		return 0;

	freeze_regst(state);
	readregst_unlocked(state, 0x10, 0x2f, tps, 7);
	readregst_unlocked(state, 0x10, 0x3f, &tps[7], 1);
	unfreeze_regst(state);
	return 0;
}

static void Active_to_Sleep(struct cxd_state *state)
{
	if (state->state <= Sleep)
		return;

	writeregt(state, 0x00, 0xC3, 0x01); /* Disable TS */
	writeregt(state, 0x00, 0x80, 0x3F); /* Enable HighZ 1 */
	writeregt(state, 0x00, 0x81, 0xFF); /* Enable HighZ 2 */
	writeregx(state, 0x00, 0x18, 0x01); /* Disable ADC 4 */
	writeregt(state, 0x00, 0x43, 0x0A); /* Disable ADC 2 */
	writeregt(state, 0x00, 0x41, 0x0A); /* Disable ADC 1 */
	writeregt(state, 0x00, 0x30, 0x00); /* Disable ADC Clock */
	writeregt(state, 0x00, 0x2F, 0x00); /* Disable RF level Monitor */
	writeregt(state, 0x00, 0x2C, 0x00); /* Disable Demod Clock */
	state->state = Sleep;
}

static void ActiveT2_to_Sleep(struct cxd_state *state)
{
	if (state->state <= Sleep)
		return;

	writeregt(state, 0x00, 0xC3, 0x01); /* Disable TS */
	writeregt(state, 0x00, 0x80, 0x3F); /* Enable HighZ 1 */
	writeregt(state, 0x00, 0x81, 0xFF); /* Enable HighZ 2 */

	writeregt(state, 0x13, 0x83, 0x40);
	writeregt(state, 0x13, 0x86, 0x21);
	writebitst(state, 0x13, 0x9E, 0x09, 0x0F);
	writeregt(state, 0x13, 0x9F, 0xFB);

	writeregx(state, 0x00, 0x18, 0x01); /* Disable ADC 4 */
	writeregt(state, 0x00, 0x43, 0x0A); /* Disable ADC 2 */
	writeregt(state, 0x00, 0x41, 0x0A); /* Disable ADC 1 */
	writeregt(state, 0x00, 0x30, 0x00); /* Disable ADC Clock */
	writeregt(state, 0x00, 0x2F, 0x00); /* Disable RF level Monitor */
	writeregt(state, 0x00, 0x2C, 0x00); /* Disable Demod Clock */
	state->state = Sleep;
}

static int ConfigureTS(struct cxd_state *state,
		       enum fe_delivery_system delivery_system)
{
	int ret = 0;
	u8 serialTs;
    u8 tsRateCtrlOff;
    u8 tsInOff;
    u8 backwardsCompatible = 0;
	struct ts_clk tsClk;
	
	struct ts_clk sTsClk[2][6] = {
		{ 			
			{ 3, 1, 8, 0 }, 
			{ 3, 1, 8, 1 },
			{ 3, 1, 8, 2 }, 
			{ 0, 2, 16, 0 }, 
			{ 0, 2, 16, 1 }, 
			{ 0, 2, 16, 2 }  
		},
		{ 		
			{ 1, 1, 8, 0 },			
			{ 1, 1, 8, 1 },
			{ 1, 1, 8, 2 }, 
			{ 2, 2, 16, 0 }, 
			{ 2, 2, 16, 1 }, 
			{ 2, 2, 16, 2 }			
		}
	};

    struct ts_clk pTsClk = { 0, 0, 8, 0 };
	
    struct ts_clk bsTsClk[2] = {
		{ 3, 1, 8, 1 },		
		{ 1, 1, 8, 1 }		
	}; 
    struct ts_clk bpTsClk = { 0, 0, 8, 1 };
	
	readregst(state, 0x00, 0xC4, &serialTs, 1);
	readregst(state, 0x00, 0xD3, &tsRateCtrlOff, 1);
	readregst(state, 0x00, 0xDE, &tsInOff, 1);
	
	if ((tsRateCtrlOff & 0x01) != (tsInOff & 0x01)) {
		ret = -EINVAL;
		goto error;
	}
	if ((tsRateCtrlOff & 0x01) && (tsInOff & 0x01)) {
        backwardsCompatible = 1;
          
        if (serialTs & 0x80) 
            tsClk = bsTsClk[state->ContinuousClock];
        else           
            tsClk = bpTsClk;
        
    }
    else if (serialTs & 0x80) 
		tsClk = sTsClk[state->ContinuousClock][state->SerialClockFrequency];
    else       
		tsClk = pTsClk;
	
	if (serialTs & 0x80) {
		writebitst(state, 0x00, 0xC4, tsClk.serialClkMode, 0x03); 
		writebitst(state, 0x00, 0xD1, tsClk.serialDutyMode, 0x03);
	}
	writeregt(state, 0x00, 0xD9, tsClk.tsClkPeriod);
	writebitst(state, 0x00, 0x32, 0x00, 0x01); /* Disable TS IF */
	
	writebitst(state, 0x00, 0x33, tsClk.clkSelTsIf, 0x03);
	writebitst(state, 0x00, 0x32, 0x01, 0x01); /* Enable TS IF */

	if (delivery_system == SYS_DVBT)
		writebitst(state, 0x10, 0x66, 0x01, 0x01);
	if (delivery_system == SYS_DVBC_ANNEX_A)
		writebitst(state, 0x40, 0x66, 0x01, 0x01);

error:
	return ret;
}

static void BandSettingT(struct cxd_state *state, u32 iffreq)
{
	u8 IF_data[3] = { (iffreq >> 16) & 0xff,
			  (iffreq >> 8) & 0xff, iffreq & 0xff};

	switch (state->bw) {
	default:
	case 8:
	{
		u8 TR_data[] = { 0x11, 0xF0, 0x00, 0x00, 0x00 };
		u8 CL_data[] = { 0x01, 0xE0 };
		u8 NF_data[] = { 0x01, 0x02 };

		writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x00, 0x07);
		writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		writeregst(state, 0x17, 0x38, NF_data, sizeof(NF_data));
		break;
	}
	case 7:
	{
		u8 TR_data[] = { 0x14, 0x80, 0x00, 0x00, 0x00 };
		u8 CL_data[] = { 0x12, 0xF8 };
		u8 NF_data[] = { 0x00, 0x03 };

		writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x02, 0x07);
		writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		writeregst(state, 0x17, 0x38, NF_data, sizeof(NF_data));
		break;
	}
	case 6:
	{
		u8 TR_data[] = { 0x17, 0xEA, 0xAA, 0xAA, 0xAA };
		u8 CL_data[] = { 0x1F, 0xDC };
		u8 NF_data[] = { 0x00, 0x03 };

		writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x04, 0x07);
		writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		writeregst(state, 0x17, 0x38, NF_data, sizeof(NF_data));
		break;
	}
	case 5:
	{
		static u8 TR_data[] = { 0x1C, 0xB3, 0x33, 0x33, 0x33 };
		static u8 CL_data[] = { 0x26, 0x3C };
		static u8 NF_data[] = { 0x00, 0x03 };

		writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x06, 0x07);
		writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		writeregst(state, 0x17, 0x38, NF_data, sizeof(NF_data));
		break;
	}
	}
}

static void Sleep_to_ActiveT(struct cxd_state *state, u32 iffreq)
{
	u8 data[2] = { 0x00, 0x00 }; 
	
	if( state->xtal == XTAL_41000KHz) {
		data[0] = 0x0A;
        data[1] = 0xD4;
	} else {
		data[0] = 0x09;
        data[1] = 0x54;
	}
	
	ConfigureTS(state, SYS_DVBT);
	writeregx(state, 0x00, 0x17, 0x01);   /* Mode */
	writeregt(state, 0x00, 0x2C, 0x01);   /* Demod Clock */
	writeregt(state, 0x00, 0x2F, 0x00);   /* Disable RF Monitor */
	writeregt(state, 0x00, 0x30, 0x00);   /* Enable ADC Clock */
	writeregt(state, 0x00, 0x41, 0x1A);   /* Enable ADC1 */
	
	writeregst(state, 0x00, 0x43, data, 2);   /* Enable ADC 2+3 */
	
	writeregx(state, 0x00, 0x18, 0x00);   /* Enable ADC 4 */

	writebitst(state, 0x10, 0xD2, 0x0C, 0x1F); /* IF AGC Gain */
	writeregt(state, 0x11, 0x6A, 0x48); /* BB AGC Target Level */

	writebitst(state, 0x10, 0xA5, 0x00, 0x01); /* ASCOT Off */

	writebitst(state, 0x18, 0x36, 0x40, 0x07); /* Pre RS Monitoring */
	writebitst(state, 0x18, 0x30, 0x01, 0x01); /* FEC Autorecover */
	writebitst(state, 0x18, 0x31, 0x01, 0x01); /* FEC Autorecover */

	writebitst(state, 0x00, 0xCE, 0x01, 0x01); /* TSIF ONOPARITY */
	writebitst(state, 0x00, 0xCF, 0x01, 0x01);/*TSIF ONOPARITY_MANUAL_ON*/

	BandSettingT(state, iffreq);

	writebitst(state, 0x10, 0x60, 11, 0x1f); /* BER scaling */

	writeregt(state, 0x00, 0x80, 0x28); /* Disable HiZ Setting 1 */
	writeregt(state, 0x00, 0x81, 0x00); /* Disable HiZ Setting 2 */
}

static void BandSettingT2(struct cxd_state *state, u32 iffreq)
{
	u8 IF_data[3] = {(iffreq >> 16) & 0xff, (iffreq >> 8) & 0xff,
			 iffreq & 0xff};

	switch (state->bw) {
	default:
	case 8:
	{
		u8 TR_data[] = { 0x11, 0xF0, 0x00, 0x00, 0x00 };
		/* Timing recovery */
		writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		/* Add EQ Optimisation for tuner here */
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		/* System Bandwidth */
		writebitst(state, 0x10, 0xD7, 0x00, 0x07);
	}
	break;
	case 7:
	{
		u8 TR_data[] = { 0x14, 0x80, 0x00, 0x00, 0x00 };
		writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x02, 0x07);
	}
	break;
	case 6:
	{
		u8 TR_data[] = { 0x17, 0xEA, 0xAA, 0xAA, 0xAA };
		writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x04, 0x07);
	}
	break;
	case 5:
	{
		u8 TR_data[] = { 0x1C, 0xB3, 0x33, 0x33, 0x33 };
		writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x06, 0x07);
	}
	break;
	case 1: /* 1.7 MHz */
	{
		u8 TR_data[] = { 0x58, 0xE2, 0xAF, 0xE0, 0xBC };
		writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x03, 0x07);
	}
	break;
	}
}


static void Sleep_to_ActiveT2(struct cxd_state *state, u32 iffreq)
{
	u8 data[2] = { 0x00, 0x00 }; 
	
	if( state->xtal == XTAL_41000KHz) {
		data[0] = 0x0A;
        data[1] = 0xD4;
	} else {
		data[0] = 0x09;
        data[1] = 0x54;
	}
	
	ConfigureTS(state, SYS_DVBT2);

	writeregx(state, 0x00, 0x17, 0x02);   /* Mode */
	writeregt(state, 0x00, 0x2C, 0x01);   /* Demod Clock */
	writeregt(state, 0x00, 0x2F, state->RfainMon ? 0x01 : 0x00);   /* Enable/Disable RF Monitor */
	writeregt(state, 0x00, 0x30, 0x00);   /* Enable ADC Clock */
	writeregt(state, 0x00, 0x41, 0x1A);   /* Enable ADC1 */

	writeregst(state, 0x00, 0x43, data, 2);   /* Enable ADC 2+3 */
	
	writeregx(state, 0x00, 0x18, 0x00);   /* Enable ADC 4 */

	writebitst(state, 0x10, 0xD2, 0x0C, 0x1F); /* IFAGC  coarse gain */
	writeregt(state, 0x11, 0x6A, 0x50); /* BB AGC Target Level */
	writebitst(state, 0x10, 0xA5, 0x00, 0x01); /* ASCOT Off */

	writeregt(state, 0x20, 0x8B, 0x3C); /* SNR Good count */
	writebitst(state, 0x2B, 0x76, 0x20, 0x70); /* Noise Gain ACQ */

	writebitst(state, 0x00, 0xCE, 0x01, 0x01); /* TSIF ONOPARITY */
	writebitst(state, 0x00, 0xCF, 0x01, 0x01);/*TSIF ONOPARITY_MANUAL_ON*/

	writeregt(state, 0x13, 0x83, 0x10); /* T2 Inital settings */
	writeregt(state, 0x13, 0x86, 0x34);
	writebitst(state, 0x13, 0x9E, 0x09, 0x0F);
	writeregt(state, 0x13, 0x9F, 0xD8);

	BandSettingT2(state, iffreq);

	writebitst(state, 0x20, 0x72, 0x08, 0x0f); /* BER scaling */

	writeregt(state, 0x00, 0x80, 0x28); /* Disable HiZ Setting 1 */
	writeregt(state, 0x00, 0x81, 0x00); /* Disable HiZ Setting 2 */
}


static void BandSettingC(struct cxd_state *state, u32 iffreq)
{
	u8 data[3];

	data[0] = (iffreq >> 16) & 0xFF;
	data[1] = (iffreq >>  8) & 0xFF;
	data[2] = (iffreq) & 0xFF;
	writeregst(state, 0x10, 0xB6, data, 3);
}

static void Sleep_to_ActiveC(struct cxd_state *state, u32 iffreq)
{
	u8 data[2] = { 0x00, 0x00 }; 
	
	if( state->xtal == XTAL_41000KHz) {
		data[0] = 0x0A;
        data[1] = 0xD4;
	} else {
		data[0] = 0x09;
        data[1] = 0x54;
	}
	
	ConfigureTS(state, SYS_DVBC_ANNEX_A);

	writeregx(state, 0x00, 0x17, 0x04);   /* Mode */
	writeregt(state, 0x00, 0x2C, 0x01);   /* Demod Clock */
	writeregt(state, 0x00, 0x2F, 0x00);   /* Disable RF Monitor */
	writeregt(state, 0x00, 0x30, 0x00);   /* Enable ADC Clock */
	writeregt(state, 0x00, 0x41, 0x1A);   /* Enable ADC1 */


	writeregst(state, 0x00, 0x43, data, 2);   /* Enable ADC 2+3 */
	
	writeregx(state, 0x00, 0x18, 0x00);   /* Enable ADC 4 */

	writebitst(state, 0x10, 0xD2, 0x09, 0x1F); /* IF AGC Gain */
	writeregt(state, 0x11, 0x6A, 0x48); /* BB AGC Target Level */
	writebitst(state, 0x10, 0xA5, 0x00, 0x01); /* ASCOT Off */

	writebitst(state, 0x40, 0xC3, 0x00, 0x04); /* OREG_BNDET_EN_64 */

	writebitst(state, 0x00, 0xCE, 0x01, 0x01); /* TSIF ONOPARITY */
	writebitst(state, 0x00, 0xCF, 0x01, 0x01);/*TSIF ONOPARITY_MANUAL_ON*/

	BandSettingC(state, iffreq);

	writebitst(state, 0x40, 0x60, 11, 0x1f); /* BER scaling */

	writeregt(state, 0x00, 0x80, 0x28); /* Disable HiZ Setting 1 */
	writeregt(state, 0x00, 0x81, 0x00); /* Disable HiZ Setting 2 */
}
static void T2_SetParameters(struct cxd_state *state)
{
	u8 Profile = 0x00;    /* Automatic Profile detection */
	u8 notT2time = 40;    /* early unlock detection time */
	
	writeregt(state, 0x23, 0xAD, 0x00);
	
	writebitst(state, 0x2E, 0x10, Profile, 0x07);
	writeregt(state, 0x2B, 0x9D, notT2time);
	
}
static void Stop(struct cxd_state *state)
{

	writeregt(state, 0x00, 0xC3, 0x01); /* Disable TS */
}

static void ShutDown(struct cxd_state *state)
{
	switch (state->delivery_system) {
	case SYS_DVBT2:
		ActiveT2_to_Sleep(state);
		break;
	default:
		Active_to_Sleep(state);
		break;
	}
}

static int gate_ctrl(struct dvb_frontend *fe, int enable)
{
	struct cxd_state *state = fe->demodulator_priv;

	return writebitsx(state, 0xFF, 0x08, enable ? 0x01 : 0x00, 0x01);
}

static void release(struct dvb_frontend *fe)
{
	struct cxd_state *state = fe->demodulator_priv;

	Stop(state);
	ShutDown(state);
	kfree(state);
}
static int read_status(struct dvb_frontend *fe, enum fe_status *status);

static int set_parameters(struct dvb_frontend *fe)
{
	struct cxd_state *state = fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int ret;
	u32 IF = 0;

	if (c->frequency == state->freq && c->delivery_system == state->delivery_system) {
		enum fe_status status;
		ret = read_status(fe, &status);
		if (!ret && status == 0x1F) {
			dev_dbg(&state->i2c->dev, "Ignoring tuning to same freq, allready locked!\n");	
			return 0;
		}
	}
	if (c->delivery_system != SYS_DVBC_ANNEX_A) {	
		switch (c->bandwidth_hz) {
		case 1700000:
			state->bw = 1;
			break;
		case 5000000:
			state->bw = 5;
			break;
		case 6000000:
			state->bw = 6;
			break;
		case 7000000:
			state->bw = 7;
			break;
		case 8000000:
			state->bw = 8;
			break;
		default:
			return -EINVAL;
		}
	}	
		
		
	if (fe->ops.tuner_ops.set_params) {
		ret = fe->ops.tuner_ops.set_params(fe);
		if (ret)
			goto err;
	}
		
	if (fe->ops.tuner_ops.get_if_frequency) {
		ret = fe->ops.tuner_ops.get_if_frequency(fe, &IF);
		if (ret)
			goto err;
	}
		
	IF = MulDiv32(IF, 16777216, 41000000);
	
	state->LockTimeout = 0;
	state->TSLockTimeout = 0;
	state->L1PostTimeout = 0;
	state->last_status = 0;
	state->FirstTimeLock = 1;
	state->LastBERNominator = 0;
	state->LastBERDenominator = 1;
	state->BERScaleMax = 19;
	
	switch (c->delivery_system) {
	case SYS_DVBC_ANNEX_A:
		state->BERScaleMax = 19;
		if ( state->state == Active && c->delivery_system == state->delivery_system) {
			writeregt(state, 0x00, 0xC3, 0x01);   /* Disable TS Output */
		} else if (state->state == Active && c->delivery_system != state->delivery_system) {
			Active_to_Sleep(state);
			Sleep_to_ActiveC(state, IF);
			state->delivery_system = c->delivery_system;
		} else if (state->state == Sleep) {
			Sleep_to_ActiveC(state, IF);
			state->delivery_system = c->delivery_system;
			state->state = Active;
		}
	break;
	case SYS_DVBT:
		state->BERScaleMax = 18;
		/* Stick with HP ( 0x01 = LP ) */
		writeregt(state, 0x10, 0x67, 0x00);
		if ( state->state == Active && c->delivery_system == state->delivery_system) {
			writeregt(state, 0x00, 0xC3, 0x01);   /* Disable TS Output */
			BandSettingT(state, IF);
		} else if (state->state == Active && c->delivery_system != state->delivery_system) {	
			Active_to_Sleep(state);
			Sleep_to_ActiveT(state, IF);
			state->delivery_system = c->delivery_system;
		} else if (state->state == Sleep) {
			Sleep_to_ActiveT(state, IF);
			state->delivery_system = c->delivery_system;
			state->state = Active;
		}
	break;
	case SYS_DVBT2:
		state->BERScaleMax = 12;
		T2_SetParameters(state);
		if ( state->state == Active && c->delivery_system == state->delivery_system) {
			writeregt(state, 0x00, 0xC3, 0x01);   /* Disable TS Output */
			BandSettingT2(state, IF);
		} else if (state->state == Active && c->delivery_system != state->delivery_system) {	
			ActiveT2_to_Sleep(state);
			Sleep_to_ActiveT2(state, IF);
			state->delivery_system = c->delivery_system;
		} else if (state->state == Sleep) {
			Sleep_to_ActiveT2(state, IF);
			state->delivery_system = c->delivery_system;
			state->state = Active;
		}
		break;
	default:
		ret = -EINVAL;
		goto err;
	}

	writeregt(state, 0x00, 0xFE, 0x01);   /* SW Reset */
	writeregt(state, 0x00, 0xC3, 0x00);   /* Enable TS Output */
	state->freq = c->frequency;
	return 0;
err:
	dev_dbg(&state->i2c->dev, "failed=%d\n", ret);	
	return ret;
}


static int init(struct dvb_frontend *fe)
{
	struct cxd_state *state = fe->demodulator_priv;

	u8 data[2] = { 0x00, state->xtal == XTAL_41000KHz ? 0x01 : 0x00 }; 
	
	
	state->delivery_system = SYS_UNDEFINED;
	state->state = Unknown;

	state->curbankt = 0xff;
	state->curbankx = 0xff;
	
	/* Start: demod any state to Sleep T / C state. */
	writeregx(state, 0xFF, 0x02, 0x00);
	usleep_range(4000, 5000);
	
	writeregx(state, 0x00, 0x10, 0x01);

	writeregsx(state, 0x00, 0x13, data, 2);
	
	writeregx(state, 0x00, 0x10, 0x00);
	usleep_range(2000, 3000);
	
	writeregt(state, 0x00, 0x43, 0x0A);
	writeregt(state, 0x00, 0x41, 0x0A);	
	
	state->state = Sleep;
	
	/* Set demod config */
	writebitst(state, 0x10, 0xCB, state->IF_AGC ? 0x40 : 0x00, 0x40);
	writeregt(state, 0x10, 0xCD, state->IF_FS);

	writebitst(state, 0x00, 0xCB, state->ErrorPolarity ? 0x00 : 0x01, 0x01); 
	writebitst(state, 0x00, 0xC5, state->ClockPolarity ? 0x01 : 0x00, 0x01); 
	
	return 0;
}


static void init_state(struct cxd_state *state, struct cxd2837_cfg *cfg)
{
	state->adrt = cfg->adr;
	state->adrx = cfg->adr + 0x02;
	state->curbankt = 0xff;
	state->curbankx = 0xff;
	mutex_init(&state->mutex);

	state->RfainMon = cfg->rfain_monitoring;
	state->ErrorPolarity = cfg->ts_error_polarity;
	state->ClockPolarity = cfg->clock_polarity;
	state->IF_AGC = cfg->if_agc_polarity;
	state->xtal = cfg->xtal;
	state->ContinuousClock = 1;
	state->SerialClockFrequency = cfg->ts_clock; /* 1 = fastest (82 MBit/s), 5 = slowest */
	state->IF_FS = 0x50; /* 1.4Vpp - Default */
		
		
	switch(cfg->ifagc_adc_range) {
	case 0:	
		break;
	case 1:		        
        state->IF_FS = 0x39; /* 1.0Vpp */
		break;
	case 2:          
        state->IF_FS = 0x28; /* 0.7Vpp */
        break;
	default:	
		break;
	}	
}

static int get_tune_settings(struct dvb_frontend *fe,
			     struct dvb_frontend_tune_settings *s)
{
	s->min_delay_ms = 1500;
	s->step_size = fe->ops.info.frequency_stepsize;
	
	return 0;
}

static int read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	struct cxd_state *state = fe->demodulator_priv;
	u8 rdata;

	*status = 0;
	switch (state->delivery_system) {
	case SYS_DVBC_ANNEX_A:
		readregst(state, 0x40, 0x88, &rdata, 1);
		if (rdata & 0x02)
			break;
		if (rdata & 0x01) {
			*status |= 0x07;
			readregst(state, 0x40, 0x10, &rdata, 1);
			if (rdata & 0x20)
				*status |= 0x1f;
		}
		break;
	case SYS_DVBT:
		readregst(state, 0x10, 0x10, &rdata, 1);
		if (rdata & 0x10)
			break;
		if ((rdata & 0x07) == 0x06) {
			*status |= 0x07;
			if (rdata & 0x20)
				*status |= 0x1f;
		}
		break;
	case SYS_DVBT2:
		readregst(state, 0x20, 0x10, &rdata, 1);		
		if (rdata & 0x10)
			break;
		if ((rdata & 0x07) == 0x06) {
			*status |= 0x07;
			if (rdata & 0x20)
				*status |= 0x08;
		}
		if (*status & 0x08) {
			readregst(state, 0x22, 0x12, &rdata, 1);
			if (rdata & 0x01)
				*status |= 0x10;
		}
		break;
	default:
		break;
	}
	state->last_status = *status;
	return 0;
}

static int get_ber_t(struct cxd_state *state, u32 *ber)
{
	u8 BERRegs[3];
	u8 Scale;
	u32 bitError = 0;
    u32 period = 0;
	u32 div = 0;
    u32 Q = 0;
    u32 R = 0;

	freeze_regst(state);
	readregst_unlocked(state, 0x10, 0x39, BERRegs, 1);
	readregst_unlocked(state, 0x10, 0x6F, &Scale, 1);
	readregst_unlocked(state, 0x10, 0x22, &BERRegs[1], 2);
	unfreeze_regst(state);
	
	if (!(BERRegs[0] & 0x10)) {
		dev_dbg(&state->i2c->dev, "%s: no valid BER data\n", __func__);	
		return 0;
	}
	
	bitError = ((u32)BERRegs[1] << 8) | (u32)BERRegs[2];
	period = ((Scale & 0x07) == 0) ? 256 : (4096 << (Scale & 0x07));
	
	div = period / 128;
	Q = (bitError * 3125) / div;
    R = (bitError * 3125) % div;
	R *= 25;
    Q = Q * 25 + R / div;
    R = R % div;
	*ber = (R >= div / 2) ? Q + 1 : Q;
	
	return 0;
}

static int get_ber_t2(struct cxd_state *state, u32 *ber)
{
	u8 BERRegs[4];
	u8 Scale;
	u8 plp;
	u32 bitError = 0;
    u32 periodExp = 0;
	u32 div = 0;
    u32 Q = 0;
    u32 R = 0;
	u32 n_ldpc = 0;
	
	freeze_regst(state);
	readregst_unlocked(state, 0x20, 0x39, BERRegs, 4);
	readregst_unlocked(state, 0x20, 0x6F, &Scale, 1);
	readregst_unlocked(state, 0x22, 0x5E, &plp, 1);
	unfreeze_regst(state);
	
	if (!(BERRegs[0] & 0x10)) {
		dev_dbg(&state->i2c->dev, "%s: no valid BER data\n", __func__);	
		return 0;
	}
	
	bitError = ((BERRegs[0] & 0x0F) << 24) | (BERRegs[1] << 16) | (BERRegs[2] << 8) | BERRegs[3];	
	periodExp = (Scale & 0x0F);
	n_ldpc = ((plp & 0x03) == 0 ? 16200 : 64800);
			
	if (bitError > ((1U << periodExp) * n_ldpc)) {
		dev_dbg(&state->i2c->dev, "%s: invalid BER value\n", __func__);	
        return -EINVAL;
    }

    if (periodExp >= 4) {    
		div = (1U << (periodExp - 4)) * (n_ldpc / 200);
		Q = (bitError * 5) / div;
		R = (bitError * 5) % div;
		R *= 625;
		Q = Q * 625 + R / div;
		R = R % div;
	} 
	else {            
		div = (1U << periodExp) * (n_ldpc / 200);
		Q = (bitError * 10) / div;
		R = (bitError * 10) % div;
		R *= 5000;
		Q = Q * 5000 + R / div;
		R = R % div;
	}

	*ber = (R >= div/2) ? Q + 1 : Q;	
	return 0;
}

static int get_ber_c(struct cxd_state *state, u32 *ber)
{	
	u8 BERRegs[3];
	u8 Scale;
	u32 bitError = 0;
    u32 periodExp = 0;
	u32 div = 0;
    u32 Q = 0;
    u32 R = 0;

	readregst(state, 0x40, 0x62, BERRegs, 3);
	readregst(state, 0x40, 0x60, &Scale, 1);

		
	if ((BERRegs[0] & 0x80) == 0) {
		dev_dbg(&state->i2c->dev, "%s: no valid BER data\n", __func__);	
		return 0;
	}
	
	bitError = ((BERRegs[0] & 0x3F) << 16) | (BERRegs[1] << 8) | BERRegs[2];
	periodExp = (Scale & 0x1F);

	if ((periodExp <= 11) && (bitError > (1U << periodExp) * 204 * 8)) {
		dev_dbg(&state->i2c->dev, "%s: invalid BER value\n", __func__);	
        return -EINVAL;
    }
	
	div = (periodExp <= 8) ? ((1U << periodExp) * 51) : ((1U << 8) * 51);
    Q = (bitError * 250) / div;
    R = (bitError * 250) % div;
    R *= 1250;
    Q = Q * 1250 + R / div;
    R = R % div;

    if (periodExp > 8) 
		*ber = (Q + (1 << (periodExp - 9))) >> (periodExp - 8);
    else 
		*ber = (R >= div/2) ? Q + 1 : Q;
		
	return 0;
}
static int read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct cxd_state *state = fe->demodulator_priv;
	int ret = 0;

	*ber = 0;
	
	if (state->last_status != 0x1f)
		return 0;
		
	switch (state->delivery_system) {
	case SYS_DVBT:
		ret = get_ber_t(state, ber);
		break;
	case SYS_DVBT2:
		ret = get_ber_t2(state, ber);
		break;
	case SYS_DVBC_ANNEX_A:
		ret = get_ber_c(state, ber);
		break;
	default:
		break;
	}
	
	return 0;
}
static u32 sony_math_log10(u32 x)
{
	u32 count = 0;

	for (x >>= 1; x > 0; x >>= 1)
		count++;
	return 10000 * count / 332;
}

static u32 sony_math_log(u32 x)
{
	u32 count = 0;

	for (x >>= 1; x > 0; x >>= 1)
		count++;
	return 10000 * count / 144;
}

static void GetSignalToNoiseT2(struct cxd_state *state, int *SignalToNoise)
{
	u8 Data[2];
	u32 reg;

	freeze_regst(state);
	readregst_unlocked(state, 0x20, 0x28, Data, sizeof(Data));
	unfreeze_regst(state);

	reg = ((u32)Data[0] << 8) | (u32)Data[1];
	if (reg == 0) {
		dev_dbg(&state->i2c->dev, "%s(): reg value out of range\n", __func__);
		return;
	}
	if (reg > 10876)
		reg = 10876;

	*SignalToNoise = 100 * ((int)sony_math_log10(reg) - (int)sony_math_log10(12600 - reg));
	*SignalToNoise += 32000;
	
}

static void GetSignalToNoiseT(struct cxd_state *state, int *SignalToNoise)
{
	u8 Data[2];
	u32 reg;

	freeze_regst(state);
	readregst_unlocked(state, 0x10, 0x28, Data, sizeof(Data));
	unfreeze_regst(state);

	reg = ((u32)Data[0] << 8) | (u32)Data[1];
	if (reg == 0) {
		dev_dbg(&state->i2c->dev, "%s(): reg value out of range\n", __func__);
		return;
	}
	if (reg > 4996)
		reg = 4996;

	*SignalToNoise = 100 * ((int)sony_math_log10(reg) - (int)sony_math_log10(5350 - reg));
	*SignalToNoise += 28500;
	
	
	return;
}
static void GetSignalToNoiseC(struct cxd_state *state, int *SignalToNoise)
{
	u8 Data[2];
	u8 Constellation = 0;
	u32 reg;
		
	*SignalToNoise = 0;

	freeze_regst(state);
	readregst_unlocked(state, 0x40, 0x19, &Constellation, 1);
	readregst_unlocked(state, 0x40, 0x4C, Data, sizeof(Data));
	unfreeze_regst(state);

	reg = ((u32)(Data[0] & 0x1F) << 8) | ((u32)Data[1]);
	if (reg == 0) {
		dev_dbg(&state->i2c->dev, "%s(): reg value out of range\n", __func__);
		return;
	}

	switch (Constellation & 0x07) {
	case 0: /* QAM 16 */
	case 2: /* QAM 64 */
	case 4: /* QAM 256 */
		if (reg < 126)
			reg = 126;
		*SignalToNoise = -95 * (int)sony_math_log(reg) + 95941;
		break;
	case 1: /* QAM 32 */
	case 3: /* QAM 128 */
		if (reg < 69)
			reg = 69;
		*SignalToNoise = -88 * (int)sony_math_log(reg) + 86999;
		break;
	}
	
}

static int read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct cxd_state *state = fe->demodulator_priv;
	int tSnr = 0;
	*snr = 0;
	
	if (state->last_status != 0x1f)
		return 0;

	switch (state->delivery_system) {
	case SYS_DVBC_ANNEX_A:
		GetSignalToNoiseC(state, &tSnr);	
		break;
	case SYS_DVBT:
		GetSignalToNoiseT(state, &tSnr);	
		break;
	case SYS_DVBT2:
		GetSignalToNoiseT2(state, &tSnr);	
		break;
	default:
		break;
	}
	*snr = tSnr & 0xffff;
	return 0;
}
static int get_signal_strengthC(struct cxd_state *state, u16 *strength)
{
	u8 data[2];
	
	readregst(state, 0x40, 0x49, data, 2);
	*strength = 65535 - (((((u16)data[0] & 0x0F) << 8) | (u16)(data[1] & 0xFF)) << 4);
	return 0;
}
static int get_signal_strengthT(struct cxd_state *state, u16 *strength)
{
	u8 data[2];
	
	readregst(state, 0x10, 0x26, data, 2);
	*strength = 65535 - (((((u16)data[0] & 0x0F) << 8) | (u16)(data[1] & 0xFF)) << 4);
	return 0;
	
}
static int get_signal_strengthT2(struct cxd_state *state, u16 *strength)
{	
	u8 data[2];
	
	readregst(state, 0x20, 0x26, data, 2);
	*strength = 65535 - (((((u16)data[0] & 0x0F) << 8) | (u16)(data[1] & 0xFF)) << 4);
	return 0;
}

static int read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct cxd_state *state = fe->demodulator_priv;
	int ret = 0;
	*strength = 0;
				
	if (state->last_status != 0x1f)
		return 0;
			
	switch (state->delivery_system) {
	case SYS_DVBC_ANNEX_A:
		ret = get_signal_strengthC(state, strength);
		break;
	case SYS_DVBT:
		ret = get_signal_strengthT(state, strength);
		break;
	case SYS_DVBT2:
		ret = get_signal_strengthT2(state, strength);
		break;
	default:
		break;
	}
	
	return ret;
}
static int get_ucblocksC(struct cxd_state *state, u32 *ucblocks)
{
	u8 data[3];
	
	readregst(state, 0x40, 0xEA, data, 3);
	
	if (!(data[2] & 0x01))
		return 0;

    *ucblocks = (data[0] << 8) | data[1]; 
	return 0;
}
static int get_ucblocksT(struct cxd_state *state, u32 *ucblocks)
{
	u8 data[3];
	
	readregst(state, 0x10, 0xEA, data, 3);
	
	if (!(data[2] & 0x01))
		return 0;

    *ucblocks = (data[0] << 8) | data[1]; 
	return 0;
	
}
static int get_ucblocksT2(struct cxd_state *state, u32 *ucblocks)
{	
	u8 data[3];
	
	readregst(state, 0x24, 0xFD, data, 3);
	if (!(data[0] & 0x01))
		return 0;
   
	*ucblocks =  ((data[1] << 0x08) | data[2]);
	
	return 0;
}


static int read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	struct cxd_state *state = fe->demodulator_priv;
	int ret = 0;
	*ucblocks = 0;
				
	if (state->last_status != 0x1f)
		return 0;
			
	switch (state->delivery_system) {
	case SYS_DVBC_ANNEX_A:
		ret = get_ucblocksC(state, ucblocks);
		break;
	case SYS_DVBT:
		ret = get_ucblocksT(state, ucblocks);
		break;
	case SYS_DVBT2:
		ret = get_ucblocksT2(state, ucblocks);
		break;
	default:
		break;
	}
	
	return 0;
}
static int get_fe_t(struct cxd_state *state)
{
	struct dvb_frontend *fe = &state->frontend;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u8 tps[8];

	read_tps(state, tps);

/*  TPSData[0] [7:6]  CNST[1:0]
    TPSData[0] [5:3]  HIER[2:0]
    TPSData[0] [2:0]  HRATE[2:0]
*/
	switch ((tps[0] >> 6) & 0x03) {
	case 0:
		p->modulation = QPSK;
		break;
	case 1:
		p->modulation = QAM_16;
		break;
	case 2:
		p->modulation = QAM_64;
		break;
	}
	switch ((tps[0] >> 3) & 0x07) {
	case 0:
		p->hierarchy = HIERARCHY_NONE;
		break;
	case 1:
		p->hierarchy = HIERARCHY_1;
		break;
	case 2:
		p->hierarchy = HIERARCHY_2;
		break;
	case 3:
		p->hierarchy = HIERARCHY_4;
		break;
	}
	switch ((tps[0] >> 0) & 0x07) {
	case 0:
		p->code_rate_HP = FEC_1_2;
		break;
	case 1:
		p->code_rate_HP = FEC_2_3;
		break;
	case 2:
		p->code_rate_HP = FEC_3_4;
		break;
	case 3:
		p->code_rate_HP = FEC_5_6;
		break;
	case 4:
		p->code_rate_HP = FEC_7_8;
		break;
	}

/*  TPSData[1] [7:5]  LRATE[2:0]
    TPSData[1] [4:3]  GI[1:0]
    TPSData[1] [2:1]  MODE[1:0]
*/
	switch ((tps[1] >> 5) & 0x07) {
	case 0:
		p->code_rate_LP = FEC_1_2;
		break;
	case 1:
		p->code_rate_LP = FEC_2_3;
		break;
	case 2:
		p->code_rate_LP = FEC_3_4;
		break;
	case 3:
		p->code_rate_LP = FEC_5_6;
		break;
	case 4:
		p->code_rate_LP = FEC_7_8;
		break;
	}
	switch ((tps[1] >> 3) & 0x03) {
	case 0:
		p->guard_interval = GUARD_INTERVAL_1_32;
		break;
	case 1:
		p->guard_interval = GUARD_INTERVAL_1_16;
		break;
	case 2:
		p->guard_interval = GUARD_INTERVAL_1_8;
		break;
	case 3:
		p->guard_interval = GUARD_INTERVAL_1_4;
		break;
	}
	switch ((tps[1] >> 1) & 0x03) {
	case 0:
		p->transmission_mode = TRANSMISSION_MODE_2K;
		break;
	case 1:
		p->transmission_mode = TRANSMISSION_MODE_8K;
		break;
	}

	switch ((tps[7] >> 0) & 0x01) {
	case 0:
		p->inversion = INVERSION_OFF;
		break;
	case 1:
		p->inversion = INVERSION_ON;
		break;
	}
	
	return 0;
}
static int get_fe_t2(struct cxd_state *state)
{
	struct dvb_frontend *fe = &state->frontend;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u8 tps[5];
	
	freeze_regst(state);
	readregst_unlocked(state, 0x20, 0x5C, tps, 2); 
	readregst_unlocked(state, 0x22, 0x5B, &tps[2], 1); 
	readregst_unlocked(state, 0x22, 0x5C, &tps[3], 1); 
	readregst_unlocked(state, 0x28, 0xE6, &tps[4], 1); 
	unfreeze_regst(state);
	
	switch ((tps[0] >> 0) & 0x07) {
	case 0:
		p->transmission_mode = TRANSMISSION_MODE_2K;
		break;
	case 1:
		p->transmission_mode = TRANSMISSION_MODE_8K;
		break;
	case 2:
		p->transmission_mode = TRANSMISSION_MODE_4K;
		break;
	case 3:
		p->transmission_mode = TRANSMISSION_MODE_1K;
		break;
	case 4:
		p->transmission_mode = TRANSMISSION_MODE_16K;
		break;
	case 5:
		p->transmission_mode = TRANSMISSION_MODE_32K;
		break;
	}

	switch ((tps[1] >> 4) & 0x07) {
	case 0:
		p->guard_interval = GUARD_INTERVAL_1_32;
		break;
	case 1:
		p->guard_interval = GUARD_INTERVAL_1_16;
		break;
	case 2:
		p->guard_interval = GUARD_INTERVAL_1_8;
		break;
	case 3:
		p->guard_interval = GUARD_INTERVAL_1_4;
		break;
	case 4:
		p->guard_interval = GUARD_INTERVAL_1_128;
		break;
	case 5:
		p->guard_interval = GUARD_INTERVAL_19_128;
		break;
	case 6:
		p->guard_interval = GUARD_INTERVAL_19_256;
		break;
	}
	
	switch ((tps[2] >> 0) & 0x07) {
	case 0:
		p->fec_inner = FEC_1_2;
		break;
	case 1:
		p->fec_inner = FEC_3_5;
		break;
	case 2:
		p->fec_inner = FEC_2_3;
		break;
	case 3:
		p->fec_inner = FEC_3_4;
		break;
	case 4:
		p->fec_inner = FEC_4_5;
		break;
	case 5:
		p->fec_inner = FEC_5_6;
		break;
	}

	switch ((tps[3] >> 0) & 0x07) {
	case 0:
		p->modulation = QPSK;
		break;
	case 1:
		p->modulation = QAM_16;
		break;
	case 2:
		p->modulation = QAM_64;
		break;
	case 3:
		p->modulation = QAM_256;
		break;
	}
	
	switch (tps[4] & 0x01) {
	case 0:
		p->inversion = INVERSION_OFF;
		break;
	case 1:
		p->inversion = INVERSION_ON;
		break;
	}
	
	return 0;
	
}
static int get_fe_c(struct cxd_state *state)
{
	struct dvb_frontend *fe = &state->frontend;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u8 qam;
	u8 rate[2];

	freeze_regst(state);
	readregst_unlocked(state, 0x40, 0x19, &qam, 1);
	readregst_unlocked(state, 0x40, 0x1A, rate, 2);
	unfreeze_regst(state);
	
	p->symbol_rate = 2500 * ((rate[0] & 0x0f) << 8 | rate[1]);
	
	p->modulation = qam & 0x07;
	
	switch (qam & 0x80) {
	case 0:
		p->inversion = INVERSION_OFF;
		break;
	case 1:
		p->inversion = INVERSION_ON;
		break;
	}
	
	return 0;
}

static int get_frontend(struct dvb_frontend *fe)
{
	struct cxd_state *state = fe->demodulator_priv;

	if (state->last_status != 0x1f)
		return 0;

	switch (state->delivery_system) {
	case SYS_DVBT:
		get_fe_t(state);
		break;
	case SYS_DVBT2:
		get_fe_t2(state);
		break;
	case SYS_DVBC_ANNEX_A:
		get_fe_c(state);
		break;
	default:
		break;
	}
	return 0;
}

static struct dvb_frontend_ops cxd_2837_ops = {
	.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBT, SYS_DVBT2 },
	.info = {
		.name = "CXD2837 DVB-C DVB-T/T2",
		.frequency_min = 47000000,	/* DVB-T: 47125000 */
		.frequency_max = 865000000,	/* DVB-C: 862000000 */
		.caps = 	FE_CAN_FEC_1_2 |
					FE_CAN_FEC_2_3 |
					FE_CAN_FEC_3_4 |
					FE_CAN_FEC_5_6 |
					FE_CAN_FEC_7_8 |
					FE_CAN_FEC_AUTO |
					FE_CAN_QPSK |
					FE_CAN_QAM_16 |
					FE_CAN_QAM_32 |
					FE_CAN_QAM_64 |
					FE_CAN_QAM_128 |
					FE_CAN_QAM_256 |
					FE_CAN_QAM_AUTO |
					FE_CAN_TRANSMISSION_MODE_AUTO |
					FE_CAN_GUARD_INTERVAL_AUTO |
					FE_CAN_HIERARCHY_AUTO |
					FE_CAN_MUTE_TS |
					FE_CAN_2G_MODULATION |
					FE_CAN_MULTISTREAM
	},
	.init = init,
	.release = release,
	.i2c_gate_ctrl = gate_ctrl,
	.set_frontend = set_parameters,
	.get_tune_settings = get_tune_settings,
	.read_status = read_status,
	.read_ber = read_ber,
	.read_signal_strength = read_signal_strength,
	.read_snr = read_snr,
	.read_ucblocks = read_ucblocks,
	.get_frontend = get_frontend,
	
};

struct dvb_frontend *cxd2837_attach(struct i2c_adapter *i2c,
				    struct cxd2837_cfg *cfg)
{
	struct cxd_state *state = NULL;
	int ret;
	u8 ChipID = 0x00;
	
	state = kzalloc(sizeof(struct cxd_state), GFP_KERNEL);
	if (!state) {
		ret = -ENOMEM;
		dev_err(&i2c->dev, "kzalloc() failed\n");
		goto err1;
	}

	state->i2c = i2c;
	init_state(state, cfg);
	
	ret = readregst(state, 0x00, 0xFD, &ChipID, 1);
	if (ret) {
		ret = readregsx(state, 0x00, 0xFD, &ChipID, 1);
		if (ret)
			goto err2;
	}
		
	if (ChipID != 0xB1)
		goto err2;
		
	dev_info(&i2c->dev, "CXD2837 DVB-T/T2/C successfully attached\n");						
		
	memcpy(&state->frontend.ops, &cxd_2837_ops,
		       sizeof(struct dvb_frontend_ops));
			   
	state->frontend.demodulator_priv = state;
	
	return &state->frontend;
	
err2:	
	kfree(state);
err1:	
	dev_dbg(&i2c->dev, "%s: failed=%d\n", __func__, ret);	
	return NULL;
}
EXPORT_SYMBOL(cxd2837_attach);

MODULE_DESCRIPTION("Sony CXD2837 DVB-T/T2/C demodulator driver");
MODULE_AUTHOR("Ralph Metzler, Manfred Voelkel");
MODULE_LICENSE("GPL");
