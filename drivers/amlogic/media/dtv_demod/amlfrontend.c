/*
 * drivers/amlogic/media/dtv_demod/amlfrontend.c
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

/*****************************************************************
 **  author :
 **	     Shijie.Rong@amlogic.com
 **  version :
 **	v1.0	  12/3/13
 **	v2.0	 15/10/12
 **	v3.0	  17/11/15
 *****************************************************************/
#define __DVB_CORE__	/*ary 2018-1-31*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/firmware.h>
#include <linux/err.h>	/*IS_ERR*/
#include <linux/clk.h>	/*clk tree*/
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/crc32.h>

#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
/* #include <mach/am_regs.h> */
#endif
#include <linux/i2c.h>
#include <linux/gpio.h>


#include <linux/dvb/aml_demod.h>
#include "demod_func.h"
#include "demod_dbg.h"
#include "dvbt_func.h"
#include "dvbs.h"
#include "dvbc_func.h"
#include "dvbs_diseqc.h"

/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vout/vdac_dev.h>
#include <linux/amlogic/aml_dtvdemod.h>
/**************************************************************/
/*V1.0.15  kernel4.9 to kernel4.9-q txlx tl1 bringup          */
/**************************************************************/
#define AMLDTVDEMOD_VER "V1.0.15"

MODULE_PARM_DESC(auto_search_std, "\n\t\t atsc-c std&hrc search");
static unsigned int auto_search_std;
module_param(auto_search_std, int, 0644);

MODULE_PARM_DESC(std_lock_timeout, "\n\t\t atsc-c std lock timeout");
static unsigned int std_lock_timeout = 1000;
module_param(std_lock_timeout, int, 0644);

/*use this flag to mark the new method for dvbc channel fast search
 *it's disabled as default, can be enabled if needed
 *we can make it always enabled after all testing are passed
 */
static unsigned int demod_dvbc_speedup_en = 1;

int aml_demod_debug = DBG_INFO;
module_param(aml_demod_debug, int, 0644);
MODULE_PARM_DESC(aml_demod_debug, "set debug level (info=bit1, reg=bit2, atsc=bit4,");

/*-----------------------------------*/
struct amldtvdemod_device_s *dtvdd_devp;

static int last_lock = -1;
static int cci_thread;
static int freq_dvbc;
static struct aml_demod_sta demod_status;
static int memstart = 0x1ef00000;/* move to aml_dtv_demod*/
long *mem_buf;

static int dvb_tuner_delay = 100;
module_param(dvb_tuner_delay, int, 0644);
MODULE_PARM_DESC(dvb_atsc_count, "dvb_tuner_delay");

static int autoflags, auto_flags_trig;

const char *name_reg[] = {
	"demod",
	"iohiu",
	"aobus",
	"reset",
};

#define END_SYS_DELIVERY	19
const char *name_fe_delivery_system[] = {
	"UNDEFINED",
	"DVBC_ANNEX_A",
	"DVBC_ANNEX_B",
	"DVBT",
	"DSS",
	"DVBS",
	"DVBS2",
	"DVBH",
	"ISDBT",
	"ISDBS",
	"ISDBC",
	"ATSC",
	"ATSCMH",
	"DTMB",
	"CMMB",
	"DAB",
	"DVBT2",
	"TURBO",
	"DVBC_ANNEX_C",
	"ANALOG",	/*19*/
};

const char *dtvdemod_get_cur_delsys(enum fe_delivery_system delsys)
{
	if ((delsys >= SYS_UNDEFINED) && (delsys <= SYS_ANALOG))
		return name_fe_delivery_system[delsys];
	else
		return "invalid delsys";
}

static void dvbc_get_qam_name(enum qam_md_e qam_mode, char *str)
{
	switch (qam_mode) {
	case QAM_MODE_64:
		strcpy(str, "QAM_MODE_64");
		break;

	case QAM_MODE_256:
		strcpy(str, "QAM_MODE_256");
		break;

	case QAM_MODE_16:
		strcpy(str, "QAM_MODE_16");
		break;

	case QAM_MODE_32:
		strcpy(str, "QAM_MODE_32");
		break;

	case QAM_MODE_128:
		strcpy(str, "QAM_MODE_128");
		break;

	default:
		strcpy(str, "QAM_MODE UNKNOWN");
		break;
	}
}

static void dtvdemod_vdac_enable(bool on);
static int dtvdemod_leave_mode(void);
static unsigned int atsc_check_cci(struct amldtvdemod_device_s *devp);

struct amldtvdemod_device_s *dtvdemod_get_dev(void)
{
	return dtvdd_devp;
}

int convert_snr(int in_snr)
{
	int out_snr;
	static int calce_snr[40] = {
		5, 6, 8, 10, 13,
		16, 20, 25, 32, 40,
		50, 63, 80, 100, 126,
		159, 200, 252, 318, 400,
		504, 634, 798, 1005, 1265,
		1592, 2005, 2524, 3177, 4000,
		5036, 6340, 7981, 10048, 12649,
		15924, 20047, 25238, 31773, 40000};
	for (out_snr = 1 ; out_snr < 40; out_snr++)
		if (in_snr <= calce_snr[out_snr])
			break;

	return out_snr;
}

//static void dtvdemod_do_8vsb_rst(void)
//{
	//union atsc_cntl_reg_0x20 val;

	//val.bits = atsc_read_reg_v4(ATSC_CNTR_REG_0X20);
	//val.b.cpu_rst = 1;
	//atsc_write_reg_v4(ATSC_CNTR_REG_0X20, val.bits);
	//val.b.cpu_rst = 0;
	//atsc_write_reg_v4(ATSC_CNTR_REG_0X20, val.bits);
//}

//static int amdemod_check_8vsb_rst(void)
//{
	//int ret = 0;

	/* for tl1/tm2, first time of pwr on,
	 * reset after signal locked
	 * for lock event, 0x79 is safe for reset
	 */
	//if (dtvdd_devp->atsc_rst_done == 0) {
		//if (dtvdd_devp->atsc_rst_needed) {
			//dtvdemod_do_8vsb_rst();
			//dtvdd_devp->atsc_rst_done = 1;
			//ret = 0;
			//PR_ATSC("reset done\n");
		//}

		//if (atsc_read_reg_v4(ATSC_CNTR_REG_0X2E) >= 0x79) {
			//dtvdd_devp->atsc_rst_needed = 1;
			//ret = 0;
			//PR_ATSC("need reset\n");
		//} else {
			//dtvdd_devp->atsc_rst_wait_cnt++;
			//PR_ATSC("wait cnt: %d\n",
				//dtvdd_devp->atsc_rst_wait_cnt);
		//}

		//if ((dtvdd_devp->atsc_rst_wait_cnt >= 3) &&
		   // (atsc_read_reg_v4(ATSC_CNTR_REG_0X2E) >= 0x76)) {
			//dtvdd_devp->atsc_rst_done = 1;
			//ret = 1;
		//}
	//} else if (atsc_read_reg_v4(ATSC_CNTR_REG_0X2E) >= 0x76) {
		//ret = 1;
	//}

	//return ret;
//}

unsigned int demod_is_t5d_cpu(struct amldtvdemod_device_s *devp)
{
	enum dtv_demod_hw_ver_e hw_ver = devp->data->hw_ver;

	return (hw_ver == DTVDEMOD_HW_T5D) || (hw_ver == DTVDEMOD_HW_T5D_B);
}

static int amdemod_stat_islock(enum fe_delivery_system delsys)
{
	struct aml_demod_sts demod_sts;
	int lock_status;
	int dvbt_status1;
	int atsc_fsm;
	unsigned int ret = 0;
	unsigned int val;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!unlikely(devp)) {
		PR_INFO("%s, devp is NULL\n", __func__);
		return -1;
	}

	switch (delsys) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		demod_sts.ch_sts = dvbc_get_ch_sts();
		ret = demod_sts.ch_sts & 0x1;
		break;

	case SYS_ISDBT:
		dvbt_status1 = (dvbt_isdbt_rd_reg((0x0a << 2)) >> 20) & 0x3ff;
		lock_status = (dvbt_isdbt_rd_reg((0x2a << 2))) & 0xf;
		if (((lock_status == 9) || (lock_status == 10)) && (dvbt_status1 != 0))
			ret = 1;
		else
			ret = 0;
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		if ((dtvdd_devp->atsc_mode == QAM_64) || (dtvdd_devp->atsc_mode == QAM_256)) {
			if ((atsc_read_iqr_reg() >> 16) == 0x1f)
				ret = 1;
		} else if (dtvdd_devp->atsc_mode == VSB_8) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
				//ret = amdemod_check_8vsb_rst();
				val = atsc_read_reg_v4(ATSC_CNTR_REG_0X2E);
				if (val >= ATSC_LOCK)
					ret = 1;
				else if (val >= CR_PEAK_LOCK)
					ret = atsc_check_cci(dtvdd_devp);
				else //if (atsc_read_reg_v4(ATSC_CNTR_REG_0X2E) <= 0x50)
					ret = 0;
			} else {
				atsc_fsm = atsc_read_reg(0x0980);
				PR_DBGL("atsc status [%x]\n", atsc_fsm);

				if (atsc_read_reg(0x0980) >= 0x79)
					ret = 1;
				else
					ret = 0;
			}
		} else {
			atsc_fsm = atsc_read_reg(0x0980);
			PR_DBGL("atsc status [%x]\n", atsc_fsm);
			if (atsc_read_reg(0x0980) >= 0x79)
				ret = 1;
			else
				ret = 0;
		}
		break;

	case SYS_DTMB:
		ret = dtmb_reg_r_fec_lock();
		break;

	case SYS_DVBT:
		if (dvbt_t2_rdb(TS_STATUS) & 0x80)
			ret = 1;
		else
			ret = 0;
		break;

	case SYS_DVBT2:
		if (demod_is_t5d_cpu(devp)) {
			val = front_read_reg(0x3e);
		} else {
			val = dvbt_t2_rdb(TS_STATUS);
			val >>= 7;
		}

		/* val[0], 0x581[7] */
		if (val & 0x1)
			ret = 1;
		else
			ret = 0;
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		ret = (dvbs_rd_byte(0x160) >> 3) & 0x1;
		break;

	default:
		break;
	}

	return ret;
}

unsigned int dtvdemod_get_atsc_lock_sts(void)
{
	return amdemod_stat_islock(SYS_ATSC);
}

void store_dvbc_qam_mode(int qam_mode, int symbolrate)
{
	int qam_para;

	switch (qam_mode) {
	case QAM_16:
		qam_para = 4;
		break;
	case QAM_32:
		qam_para = 5;
		break;
	case QAM_64:
		qam_para = 6;
		break;
	case QAM_128:
		qam_para = 7;
		break;
	case QAM_256:
		qam_para = 8;
		break;
	case QAM_AUTO:
		qam_para = 6;
		break;
	default:
		qam_para = 6;
		break;
	}
	dtvdd_devp->para_demod.dvbc_qam = qam_para;
	dtvdd_devp->para_demod.dvbc_symbol = symbolrate;
	PR_DBG("dvbc_qam is %d, symbolrate is %d\n",
	       dtvdd_devp->para_demod.dvbc_qam, dtvdd_devp->para_demod.dvbc_symbol);
}

static void dtvdemod_version(struct amldtvdemod_device_s *dev)
{
	atsc_set_version(dev->atsc_version);
}

static int amdemod_qam(enum fe_modulation qam)
{
	switch (qam) {
	case QAM_16:
		return 0;
	case QAM_32:
		return 1;
	case QAM_64:
		return 2;
	case QAM_128:
		return 3;
	case QAM_256:
		return 4;
	case VSB_8:
		return 5;
	case QAM_AUTO:
		return 6;
	default:
		return 2;
	}
	return 2;
}

static void gxtv_demod_dvbc_release(struct dvb_frontend *fe)
{

}

struct timer_t {
	int enable;
	unsigned int start;
	unsigned int max;
};
static struct timer_t gtimer[4];
enum ddemod_timer_s {
	D_TIMER_DETECT,
	D_TIMER_SET,
	D_TIMER_DBG1,
	D_TIMER_DBG2,
};
int timer_set_max(enum ddemod_timer_s tmid, unsigned int max_val)
{
	gtimer[tmid].max = max_val;
	return 0;
}
int timer_begain(enum ddemod_timer_s tmid)
{
	gtimer[tmid].start = jiffies_to_msecs(jiffies);
	gtimer[tmid].enable = 1;

	PR_DBG("st %d=%d\n", tmid, (int)gtimer[tmid].start);
	return 0;
}
int timer_disable(enum ddemod_timer_s tmid)
{

	gtimer[tmid].enable = 0;

	return 0;
}

int timer_is_en(enum ddemod_timer_s tmid)
{
	return gtimer[tmid].enable;
}

int timer_not_enough(enum ddemod_timer_s tmid)
{
	int ret = 0;
	unsigned int time;

	if (gtimer[tmid].enable) {
		time = jiffies_to_msecs(jiffies);
		if ((time - gtimer[tmid].start) < gtimer[tmid].max)
			ret = 1;
	}
	return ret;
}
int timer_is_enough(enum ddemod_timer_s tmid)
{
	int ret = 0;
	unsigned int time;

	/*Signal stability takes 200ms */
	if (gtimer[tmid].enable) {
		time = jiffies_to_msecs(jiffies);
		if ((time - gtimer[tmid].start) >= gtimer[tmid].max) {
			PR_DBG("now=%d\n", (int)time);
			ret = 1;
		}
	}
	return ret;
}

int timer_tuner_not_enough(void)
{
	int ret = 0;
	unsigned int time;
	enum ddemod_timer_s tmid;

	tmid = D_TIMER_DETECT;

	/*Signal stability takes 200ms */
	if (gtimer[tmid].enable) {
		time = jiffies_to_msecs(jiffies);
		if ((time - gtimer[tmid].start) < 200) {
			PR_DBG("nowt=%d\n", (int)time);
			ret = 1;
		}
	}
	return ret;
}


unsigned int demod_get_adc_clk(void)
{
	return demod_status.adc_freq;
}

unsigned int demod_get_sys_clk(void)
{
	return demod_status.clk_freq;
}

static int gxtv_demod_dvbc_read_status_timer
	(struct dvb_frontend *fe, enum fe_status *status)
{
	struct aml_demod_sts demod_sts;
	int strenth;
	int ilock = 0;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("dvbt2_tune, devp is NULL\n");
		return -1;
	}

	/*check tuner*/
	if (!timer_tuner_not_enough()) {
		strenth = tuner_get_ch_power2();

		/*agc control,fine tune strength*/
		if (!strncmp(fe->ops.tuner_ops.info.name, "r842", 4)) {
			strenth += 22;

			if (strenth <= -80)
				strenth = dvbc_get_power_strength(
					qam_read_reg(0x27) & 0x7ff, strenth);
		}

		if (strenth < -87) {
			PR_DVBC("tuner no signal, strength:%d\n", strenth);
			*status = FE_TIMEDOUT;
			return 0;
		}
	}

	/*demod_sts.ch_sts = qam_read_reg(0x6);*/
	demod_sts.ch_sts = dvbc_get_ch_sts();
	dvbc_status(devp, &demod_sts, NULL);

	if (demod_sts.ch_sts & 0x1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;

		if (timer_not_enough(D_TIMER_DETECT)) {
			*status = 0;
			PR_DBG("s=0\n");
		} else {
			*status = FE_TIMEDOUT;
			timer_disable(D_TIMER_DETECT);
		}
	}

	if (last_lock != ilock) {
		PR_DBG("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
		if (ilock == 0)
			devp->fast_search_finish = 0;
	}

	return 0;
}

static int demod_dvbc_speed_up(enum fe_status *status)
{
	unsigned int cnt, i, sts, check_ok = 0;
	struct aml_demod_sts demod_sts;
	const int dvbc_count = 5;
	int ilock = 0;

	if (*status == 0) {
		for (cnt = 0; cnt < 10; cnt++) {
			demod_sts.ch_sts = dvbc_get_ch_sts();

			if (demod_sts.ch_sts & 0x1) {
				/*have signal*/
				*status =
					FE_HAS_LOCK | FE_HAS_SIGNAL |
					FE_HAS_CARRIER |
					FE_HAS_VITERBI | FE_HAS_SYNC;
				ilock = 1;
				check_ok = 1;
			} else {
				for (i = 0; i < dvbc_count; i++) {
					msleep(25);
					sts = dvbc_get_status();

					if (sts >= 0x3)
						break;
				}

				PR_DBG("[rsj]dvbc_status is 0x%x\n", sts);

				if (sts < 0x3) {
					*status = FE_TIMEDOUT;
					ilock = 0;
					check_ok = 1;
					timer_disable(D_TIMER_DETECT);
				}
			}

			if (check_ok == 1)
				break;

			msleep(20);
		}
	}

	if (last_lock != ilock) {
		PR_DBG("%s : %s.\n", __func__,
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int gxtv_demod_dvbc_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct aml_demod_sts demod_sts;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("dvbt2_tune, devp is NULL\n");
		return -1;
	}

	dvbc_status(devp, &demod_sts, NULL);
	*ber = demod_sts.ch_ber;
	return 0;
}

static int gxtv_demod_dvbc_read_signal_strength
	(struct dvb_frontend *fe, u16 *strength)
{
	int tuner_sr;

	if (!strncmp(fe->ops.tuner_ops.info.name, "r842", 4)) {
		tuner_sr = tuner_get_ch_power2();
		tuner_sr += 22;

		if (tuner_sr <= -80)
			tuner_sr = dvbc_get_power_strength(
				qam_read_reg(0x27) & 0x7ff, tuner_sr);

		if (tuner_sr < -100)
			*strength = 0;
		else
			*strength = tuner_sr + 100;
	} else
		*strength = tuner_get_ch_power3();

	return 0;
}

static int gxtv_demod_dvbc_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	u32 tmp;
	struct aml_demod_sts demod_sts;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!devp->demod_thread)
		return 0;

	tmp = qam_read_reg(0x5) & 0xfff;
	demod_sts.ch_snr = tmp * 100 / 32;
	*snr = demod_sts.ch_snr / 40;

	if (*snr > 100)
		*snr = 100;

	return 0;
}

static int gxtv_demod_dvbc_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

static int Gxtv_Demod_Dvbc_Init(int mode)
{
	struct aml_demod_sys sys;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!devp) {
		pr_err("%s devp is NULL\n", __func__);
		return -EFAULT;
	}

	PR_DBG("%s\n", __func__);
	memset(&sys, 0, sizeof(sys));

	demod_status.delsys = SYS_DVBC_ANNEX_A;

	if (mode == ADC_MODE) {
		sys.adc_clk = ADC_CLK_25M;
		sys.demod_clk = DEMOD_CLK_200M;
		demod_status.tmp = ADC_MODE;
	} else {
		sys.adc_clk = ADC_CLK_24M;
		sys.demod_clk = DEMOD_CLK_72M;
		demod_status.tmp = CRY_MODE;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		sys.adc_clk = ADC_CLK_24M;
		sys.demod_clk = DEMOD_CLK_167M;
		demod_status.tmp = CRY_MODE;
	}

	demod_status.ch_if = SI2176_5M_IF * 1000;
	PR_DBG("[%s]adc_clk is %d,demod_clk is %d\n", __func__, sys.adc_clk,
	       sys.demod_clk);
	auto_flags_trig = 0;
	demod_set_sys(devp, &demod_status, &sys);
	return 0;
}

static int gxtv_demod_dvbc_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dvbc param;    /*mode 0:16, 1:32, 2:64, 3:128, 4:256*/
	struct aml_demod_sts demod_sts;
	int ret = 0;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("dvbt2_tune, devp is NULL\n");
		return -1;
	}

	PR_DVBC("%s: delsys: %d, freq: %d, symbol_rate: %d, bw: %d, modulation: %d.\n",
			__func__, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation);

	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;
	param.mode = amdemod_qam(c->modulation);
	param.symb_rate = c->symbol_rate / 1000;

	if (devp->symb_rate_en)
		param.symb_rate = devp->symbol_rate_manu;

	/* symbol rate, 0=auto, set to max sr to track by hw */
	if (!param.symb_rate) {
		param.symb_rate = 7000;
		devp->auto_sr = 1;
		PR_DVBC("auto sr mode, set sr=7000\n");
	} else {
		devp->auto_sr = 0;
		PR_DVBC("sr=%d\n", param.symb_rate);
	}

	devp->sr_val_hw = param.symb_rate;

	store_dvbc_qam_mode(c->modulation, param.symb_rate);

	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if ((param.mode == 3) && (demod_status.tmp != ADC_MODE))
			ret = Gxtv_Demod_Dvbc_Init(ADC_MODE);
	}

	if ((autoflags == 1) && (auto_flags_trig == 0)
	    && (freq_dvbc == param.ch_freq)) {
		PR_DBG("now is auto symbrating\n");
		return 0;
	}
	auto_flags_trig = 0;
	last_lock = -1;
	PR_DBG("[gxtv_demod_dvbc_set_frontend]PARA\t"
	       "param.ch_freq is %d||||param.symb_rate is %d,\t"
	       "param.mode is %d\n",
	       param.ch_freq, param.symb_rate, param.mode);
	tuner_set_params(fe);/*aml_fe_analog_set_frontend(fe);*/
	dvbc_set_ch(&demod_status, &param);
	/*0xf33 dvbc mode, 0x10f33 j.83b mode*/
	#if 0
	if (is_meson_txlx_cpu() || is_meson_gxlx_cpu())
		/*qam_write_reg(0x7, 0xf33);*/
		dvbc_init_reg_ext();
	#endif
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX) && !is_meson_txhd_cpu())
		dvbc_init_reg_ext();

	if (autoflags == 1) {
		PR_DBG("QAM_PLAYING mode,start auto sym\n");
		dvbc_set_auto_symtrack();
		/*      flag=1;*/
	}
	dvbc_status(devp, &demod_sts, NULL);
	freq_dvbc = param.ch_freq;

	PR_DBG("AML amldemod => frequency=%d,symbol_rate=%d\r\n", c->frequency,
	       c->symbol_rate);
	return ret;
}

static int gxtv_demod_dvbc_get_frontend(struct dvb_frontend *fe)
{
	#if 0
	/*these content will be writed into eeprom .*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	u32 qam_mode;


	qam_mode = dvbc_get_qam_mode();
	c->modulation = qam_mode + 1;
	PR_DBG("[mode] is %d\n", c->modulation);
	#endif

	return 0;
}

static void gxtv_demod_dvbt_release(struct dvb_frontend *fe)
{
}

static int dvbt_isdbt_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	int ilock;
	unsigned char s = 0;

	s = dvbt_get_status_ops()->get_status();

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		if (timer_not_enough(D_TIMER_DETECT)) {
			ilock = 0;
			*status = 0;
			PR_INFO("timer not enough\n");

		} else {
			ilock = 0;
			*status = FE_TIMEDOUT;
			timer_disable(D_TIMER_DETECT);
		}
	}
	if (last_lock != ilock) {
		PR_INFO("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int dvbt_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	int ilock;
	unsigned char s = 0;
	int strenth;
	int strength_limit = THRD_TUNER_STRENTH_DVBT;
	struct amldtvdemod_device_s *devp = dtvdd_devp;
	unsigned int tps_coderate, ts_fifo_cnt = 0, ts_cnt = 0;

	strenth = tuner_get_ch_power(fe);
	if (devp->tuner_strength_limit)
		strength_limit = devp->tuner_strength_limit;

	if (strenth < strength_limit) {
		*status = FE_TIMEDOUT;
		last_lock = -1;
		devp->last_status = *status;
		PR_DVBT("tuner:no signal! strength:%d\n", strenth);
		return 0;
	}

	devp->time_passed = jiffies_to_msecs(jiffies) - devp->time_start;
	if (devp->time_passed >= 200) {
		if ((dvbt_t2_rdb(0x2901) & 0xf) < 4) {
			*status = FE_TIMEDOUT;
			last_lock = -1;
			devp->last_status = *status;
			PR_DVBT("not dvbt signal\n");
			return 0;
		}
	}

	s = amdemod_stat_islock(SYS_DVBT);

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		if (timer_not_enough(D_TIMER_DETECT)) {
			ilock = 0;
			*status = 0;
		} else {
			ilock = 0;
			*status = FE_TIMEDOUT;
			timer_disable(D_TIMER_DETECT);
		}
	}

	/* porting from ST driver FE_368dvbt_LockFec() */
	if (ilock) {
		do {
			dvbt_t2_wr_byte_bits(0x53d, 0, 6, 1);
			dvbt_t2_wr_byte_bits(0x572, 0, 0, 1);
			dvbt_t2_rdb(0x2913);

			if (dvbt_t2_rdb(0x3760) >> 5 & 1)
				tps_coderate = dvbt_t2_rdb(0x2913) >> 4 & 0x7;
			else
				tps_coderate = dvbt_t2_rdb(0x2913) & 0x7;

			switch (tps_coderate) {
			case 0: /*  CR=1/2*/
				dvbt_t2_wrb(0x53c, 0x41);
				break;
			case 1: /*  CR=2/3*/
				dvbt_t2_wrb(0x53c, 0x42);
				break;
			case 2: /*  CR=3/4*/
				dvbt_t2_wrb(0x53c, 0x44);
				break;
			case 3: /*  CR=5/6*/
				dvbt_t2_wrb(0x53c, 0x48);
				break;
			case 4: /*  CR=7/8*/
				dvbt_t2_wrb(0x53c, 0x60);
				break;
			default:
				dvbt_t2_wrb(0x53c, 0x6f);
				break;
			}

			switch (tps_coderate) {
			case 0: /*  CR=1/2*/
				dvbt_t2_wrb(0x5d1, 0x78);
				break;
			default: /* other CR */
				dvbt_t2_wrb(0x5d1, 0x60);
				break;
			}

			if (amdemod_stat_islock(SYS_DVBT))
				ts_fifo_cnt++;
			else
				ts_fifo_cnt = 0;

			ts_cnt++;
			usleep_range(10000, 10001);
		} while (ts_fifo_cnt < 4 && ts_cnt <= 10);
	}

	if (ilock && ts_fifo_cnt < 4)
		*status = 0;

	if (last_lock != ilock) {
		if (*status == (FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
		    FE_HAS_VITERBI | FE_HAS_SYNC)) {
			PR_INFO("!!  >> LOCKT << !!, freq:%d\n", fe->dtv_property_cache.frequency);
			last_lock = ilock;
		} else if (*status == FE_TIMEDOUT) {
			PR_INFO("!!  >> UNLOCKT << !!, freq:%d\n",
				fe->dtv_property_cache.frequency);
			last_lock = ilock;
		} else {
			PR_INFO("!!  >> WAITT << !!\n");
		}
	}
	devp->last_status = *status;

	return 0;
}

static int dvbt2_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	int ilock;
	unsigned char s = 0;
	int strenth;
	int strength_limit = THRD_TUNER_STRENTH_DVBT;
	struct amldtvdemod_device_s *devp = dtvdd_devp;
	unsigned int p1_peak, val;

	if (!devp->demod_thread)
		return 0;

	strenth = tuner_get_ch_power(fe);
	if (devp->tuner_strength_limit)
		strength_limit = devp->tuner_strength_limit;

	if (strenth < strength_limit) {
		*status = FE_TIMEDOUT;
		last_lock = 0;
		devp->last_status = *status;
		PR_DVBT("tuner:no signal! strength:%d\n", strenth);
		return 0;
	}

	val = front_read_reg(0x3e);
	p1_peak = (val >> 1) & 0x01;//test bus val[1];
	PR_DVBT("DVBT2 testbus:0x%x bit1:0x%x tuner strength:%d\n", val, p1_peak, strenth);

	devp->time_passed = jiffies_to_msecs(jiffies) - devp->time_start;
	if (devp->time_passed >= 800 && devp->p1_peak == 0) {
		//T5D testbus read reg val
		if (demod_is_t5d_cpu(devp)) {
			val = front_read_reg(0x3e);
			p1_peak = (val >> 1) & 0x01;//test bus val[1];
			//PR_DVBT("DVBT2 testbus:0x%x p1_peak:0x%x\n", val, p1_peak);
			if (p1_peak == 1) {
				*status = FE_TIMEDOUT;
				last_lock = 0;
				PR_DVBT("not dvbt2 signal\n");
				devp->last_status = *status;
				devp->p1_peak = 0;
				goto exit;
			} else {
				devp->p1_peak = 1;
				PR_DVBT("dvbt2 signal\n");
			}
		} else {
			p1_peak = (dvbt_t2_rdb(0x2838) + (dvbt_t2_rdb(0x2839) << 8) +
				(dvbt_t2_rdb(0x283A) << 16));
			PR_DVBT("DVBT2 p1_peak:0x%x\n", p1_peak);
			if (p1_peak == 0xe00) {
				*status = FE_TIMEDOUT;
				last_lock = 0;
				PR_DVBT("not dvbt2 signal\n");
				devp->last_status = *status;
				devp->p1_peak = 0;
				goto exit;
			} else {
				devp->p1_peak = 1;
				PR_DVBT("dvbt2 signal\n");
			}
		}
	}

	s = amdemod_stat_islock(SYS_DVBT2);

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		if (timer_not_enough(D_TIMER_DETECT)) {
			ilock = 0;
			*status = 0;
		} else {
			ilock = 0;
			*status = FE_TIMEDOUT;
			timer_disable(D_TIMER_DETECT);
		}
	}
	if (last_lock != ilock) {
		if (*status == (FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
		    FE_HAS_VITERBI | FE_HAS_SYNC)) {
			PR_INFO("!!  >> LOCKT2 << !!, freq:%d\n", fe->dtv_property_cache.frequency);
			last_lock = ilock;
		} else if (*status == FE_TIMEDOUT) {
			PR_INFO("!!  >> UNLOCKT2 << !!, freq:%d\n",
				fe->dtv_property_cache.frequency);
			last_lock = ilock;
		} else {
			PR_INFO("!!  >> WAITT2 << !!\n");
		}
	}
	devp->last_status = *status;

exit:
	return 0;
}

static int dvbt_isdbt_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	*ber = dvbt_get_status_ops()->get_ber() & 0xffff;
	return 0;
}

static int dvbt_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	*ber = 0;
	return 0;
}

static int gxtv_demod_dvbt_read_signal_strength
	(struct dvb_frontend *fe, u16 *strength)
{
	*strength = tuner_get_ch_power3();

	PR_DBGL("tuner strength is %d dbm\n", *strength);
	return 0;
}

static int dtvdemod_dvbs_read_signal_strength
	(struct dvb_frontend *fe, u16 *strength)
{
	*strength = tuner_get_ch_power3();

	PR_DBGL("[RSJ]tuner strength is %d dbm\n", *strength);
	return 0;
}

static int gxtv_demod_dvbt_isdbt_read_snr(struct dvb_frontend *fe, u16 *snr)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
/*      struct aml_demod_sts demod_sts;*/
/*	struct aml_demod_i2c demod_i2c;*/
	struct aml_demod_sta demod_sta;

	*snr = dvbt_get_status_ops()->get_snr(&demod_sta/*, &demod_i2c*/);
	*snr /= 8;
	PR_DBGL("[RSJ]snr is %d dbm\n", *snr);
	return 0;
}

static int dvbt_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct amldtvdemod_device_s *devp = dtvdd_devp;

	if (!devp->demod_thread)
		return 0;

	*snr = ((dvbt_t2_rdb(CHC_CIR_SNR1) & 0x7) << 8) | dvbt_t2_rdb(CHC_CIR_SNR0);
	*snr = *snr * 3 / 64;

	/* 1-34db, mappting to 0-100% */
	*snr = *snr * 100 / 34;

	if (*snr > 100)
		*snr = 100;

	PR_DVBT("signal quality is %d%%\n", *snr);
	return 0;
}

static int dvbt2_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	unsigned int val;
	struct amldtvdemod_device_s *devp = dtvdd_devp;

	if (!devp->demod_thread)
		return 0;

	if (demod_is_t5d_cpu(devp)) {
		val = front_read_reg(0x3e);
		/* val[23-21]:0x2a09,val[20-13]:0x2a08 */
		val >>= 13;
	} else {
		val = dvbt_t2_rdb(0x2a09) << 8;
		val |= dvbt_t2_rdb(0x2a08);
	}

	*snr = val & 0x7ff;
	*snr = *snr * 3 / 64;

	/* 1-34db, mappting to 0-100% */
	*snr = *snr * 100 / 34;

	if (*snr > 100)
		*snr = 100;

	PR_DVBT("signal quality is %d%%\n", *snr);
	return 0;
}

static int dvbs_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct amldtvdemod_device_s *devp = dtvdd_devp;
	unsigned int quality_snr = 0;

	if (!devp->demod_thread)
		return 0;

	quality_snr = dvbs_get_quality();

	*snr = quality_snr / 10;

	PR_DVBS("snr is %d dbm\n", *snr);

	return 0;
}

static int gxtv_demod_dvbt_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

static enum fe_bandwidth dtvdemod_convert_bandwidth(unsigned int input)
{
	enum fe_bandwidth output = BANDWIDTH_AUTO;

	switch (input) {
	case 10000000:
		output = BANDWIDTH_10_MHZ;
		break;

	case 8000000:
		output = BANDWIDTH_8_MHZ;
		break;

	case 7000000:
		output = BANDWIDTH_7_MHZ;
		break;

	case 6000000:
		output = BANDWIDTH_6_MHZ;
		break;

	case 5000000:
		output = BANDWIDTH_5_MHZ;
		break;

	case 1712000:
		output = BANDWIDTH_1_712_MHZ;
		break;

	case 0:
		output = BANDWIDTH_AUTO;
		break;
	}

	return output;


}

static int dvbt_isdbt_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	/*struct aml_demod_sts demod_sts;*/
	struct aml_demod_dvbt param;

	PR_ISDBT("%s: delsys: %d, freq: %d, symbol_rate: %d, bw: %d, modulation: %d, invert: %d.\n",
			__func__, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);

	/* bw == 0 : 8M*/
	/*       1 : 7M*/
	/*       2 : 6M*/
	/*       3 : 5M*/
	/* agc_mode == 0: single AGC*/
	/*             1: dual AGC*/
	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;
	param.bw = dtvdemod_convert_bandwidth(c->bandwidth_hz);
	param.agc_mode = 1;
	/*ISDBT or DVBT : 0 is QAM, 1 is DVBT, 2 is ISDBT,*/
	/* 3 is DTMB, 4 is ATSC */
	param.dat0 = 1;
	last_lock = -1;

	tuner_set_params(fe);
	dvbt_isdbt_set_ch(&demod_status, /*&demod_i2c,*/ &param);
	return 0;
}

static int dvbt_set_frontend(struct amldtvdemod_device_s *devp, struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	/*struct aml_demod_sts demod_sts;*/
	struct aml_demod_dvbt param;

	PR_INFO("%s: delsys: %d, freq: %d, symbol_rate: %d, bw: %d, modulation: %d, invert: %d.\n",
			__func__, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);

	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;
	devp->freq = c->frequency / 1000;
	param.bw = dtvdemod_convert_bandwidth(c->bandwidth_hz);
	param.agc_mode = 1;
	param.dat0 = 1;
	last_lock = -1;
	devp->last_status = 0;

	tuner_set_params(fe);
	dvbt_set_ch(&demod_status, &param);
	devp->time_start = jiffies_to_msecs(jiffies);
	/* wait tuner stable */
	msleep(30);

	return 0;
}

static int dvbt2_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("dvbt2_set_frontend, devp is NULL\n");
		return -1;
	}

	PR_DVBT("%s: delsys: %d, freq: %d, symbol_rate: %d, bw: %d, modulation: %d, invert: %d.\n",
			__func__, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);
	devp->bw = dtvdemod_convert_bandwidth(c->bandwidth_hz);
	devp->freq = c->frequency / 1000;
	last_lock = -1;
	devp->last_status = 0;
	tuner_set_params(fe);
	dvbt2_set_ch(devp);
	devp->p1_peak = 0;
	devp->time_start = jiffies_to_msecs(jiffies);
	/* wait tuner stable */
	msleep(30);

	return 0;
}

static int gxtv_demod_dvbt_get_frontend(struct dvb_frontend *fe)
{
	return 0;
}

int dvbt_isdbt_Init(void)
{
	struct aml_demod_sys sys;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!devp) {
		pr_err("%s devp is NULL\n", __func__);
		return -EFAULT;
	}

	PR_DBG("AML Demod DVB-T/isdbt init\r\n");

	memset(&sys, 0, sizeof(sys));

	memset(&demod_status, 0, sizeof(demod_status));

	demod_status.delsys = SYS_ISDBT;
	sys.adc_clk = ADC_CLK_24M;
	sys.demod_clk = DEMOD_CLK_60M;
	demod_status.ch_if = SI2176_5M_IF * 1000;

	demod_set_sys(devp, &demod_status, &sys);
	return 0;
}

static unsigned int dvbt_init(void)
{
	struct aml_demod_sys sys;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!devp) {
		pr_err("%s devp is NULL\n", __func__);
		return -EFAULT;
	}

	PR_DBG("AML Demod DVB-T init\r\n");

	memset(&sys, 0, sizeof(sys));

	memset(&demod_status, 0, sizeof(demod_status));

	demod_status.delsys = SYS_DVBT;
	sys.adc_clk = ADC_CLK_54M;
	sys.demod_clk = DEMOD_CLK_216M;
	demod_status.ch_if = SI2176_5M_IF * 1000;

	demod_set_sys(devp, &demod_status, &sys);
	devp->last_status = 0;

	return 0;
}

static unsigned int dtvdemod_dvbt2_init(void)
{
	struct aml_demod_sys sys;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!devp) {
		pr_err("%s devp is NULL\n", __func__);
		return -EFAULT;
	}

	PR_DBG("AML Demod DVB-T2 init\r\n");

	memset(&sys, 0, sizeof(sys));

	memset(&demod_status, 0, sizeof(demod_status));

	demod_status.delsys = SYS_DVBT2;
	sys.adc_clk = ADC_CLK_54M;
	sys.demod_clk = DEMOD_CLK_216M;
	demod_status.ch_if = SI2176_5M_IF * 1000;

	demod_set_sys(devp, &demod_status, &sys);
	devp->last_status = 0;

	return 0;
}

static void gxtv_demod_atsc_release(struct dvb_frontend *fe)
{
}

static int gxtv_demod_get_frontend_algo(struct dvb_frontend *fe)
{
	int ret = DVBFE_ALGO_HW;

	return ret;
}

static unsigned int atsc_check_cci(struct amldtvdemod_device_s *devp)
{
	unsigned int fsm_status;
	int time[10], time_table[10];
	unsigned int ret = CFO_FAIL;
	unsigned int i;

	fsm_status = atsc_read_reg_v4(ATSC_CNTR_REG_0X2E);
	PR_ATSC("fsm[%x]not lock,need to run cci\n", fsm_status);
	time[0] = jiffies_to_msecs(jiffies);
	set_cr_ck_rate_new();
	time[1] = jiffies_to_msecs(jiffies);
	time_table[0] = (time[1] - time[0]);
	fsm_status = atsc_read_reg_v4(ATSC_CNTR_REG_0X2E);
	PR_ATSC("fsm[%x][atsc_time]cci finish cost %d ms\n",
		fsm_status, time_table[0]);
	if (fsm_status >= ATSC_LOCK) {
		goto exit;
	} else if (fsm_status >= CR_PEAK_LOCK) {
	//else if (fsm_status < CR_LOCK) {
		//ret = cfo_run_new();
	//}

		ret = CFO_OK;
		//msleep(100);
	}

	time[2] = jiffies_to_msecs(jiffies);
	time_table[1] = (time[2] - time[1]);
	//PR_ATSC("fsm[%x][atsc_time]cfo done,cost %d ms,\n",
		//atsc_read_reg_v4(ATSC_CNTR_REG_0X2E), time_table[1]);

	if (ret == CFO_FAIL)
		goto exit;

	cci_run_new(devp);
	ret = 2;

	for (i = 0; i < 2; i++) {
		fsm_status = atsc_read_reg_v4(ATSC_CNTR_REG_0X2E);
		if (fsm_status >= ATSC_LOCK) {
			time[3] = jiffies_to_msecs(jiffies);
			PR_ATSC("----------------------\n");
			time_table[2] = (time[3] - time[2]);
			time_table[3] = (time[3] - time[0]);
			time_table[4] = (time[3] - time[5]);
			PR_ATSC("fsm[%x][atsc_time]fec lock cost %d ms\n",
				fsm_status, time_table[2]);
			PR_ATSC("fsm[%x][atsc_time]lock,one cost %d ms,\n",
				fsm_status, time_table[3]);
			break;
		} else if (fsm_status <= IDLE) {
			PR_ATSC("atsc idle,retune, and reset\n");
			set_cr_ck_rate_new();
			atsc_reset_new();
			break;
		}
		msleep(20);
	}

exit:
	return ret;
}

unsigned  int ats_thread_flg;
static int gxtv_demod_atsc_read_status
	(struct amldtvdemod_device_s *devp, enum fe_status *status)
{
	struct dvb_frontend *fe = &devp->frontend;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_sts demod_sts;
	int ilock;
	unsigned char s = 0;
	int strength = 0;
	/*debug only*/
	static enum fe_status dbg_lst_status;	/* last status */
	unsigned int ret = 0;

	if (!devp->demod_thread) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
		return 0;
	}
	if (!get_dtvpll_init_flag())
		return 0;

	/* j83b */
	if ((c->modulation <= QAM_AUTO) && (c->modulation != QPSK)) {
		s = amdemod_stat_islock(SYS_DVBC_ANNEX_A);
		dvbc_status(devp, &demod_sts, NULL);
	} else if (c->modulation > QAM_AUTO) {/* atsc */
		/*atsc_thread();*/
		s = amdemod_stat_islock(SYS_ATSC);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			atsc_check_fsm_status();

			if (!s) {
				PR_ATSC("ber dalta:%d\n",
					atsc_read_reg_v4(ATSC_FEC_BER) - devp->ber_base);
			}
		} else {
			if ((s == 0) && (last_lock == 1)
				&& (atsc_read_reg(0x0980) >= 0x76)) {
				s = 1;
				PR_ATSC("[rsj] unlock,but fsm >= 0x76\n");
			}
		}
	}

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (timer_not_enough(D_TIMER_DETECT)) {
				*status = 0;
				PR_INFO("WAIT!\n");
			} else {
				*status = FE_TIMEDOUT;
				timer_disable(D_TIMER_DETECT);
			}
		} else {
			*status = FE_TIMEDOUT;
		}
	}

	if (last_lock != ilock) {
		PR_INFO("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;

		if (c->modulation > QAM_AUTO && ilock) {
			devp->ber_base = atsc_read_reg_v4(ATSC_FEC_BER);
			PR_ATSC("ber base:%d\n", devp->ber_base);
		}
	}

	if (aml_demod_debug & DBG_ATSC) {
		if ((dbg_lst_status != s) || (last_lock != ilock)) {
			/* check tuner */
			strength = tuner_get_ch_power2();

			PR_ATSC("s=%d(1 is lock),lock=%d\n", s, ilock);
			PR_ATSC("[rsj_test]freq[%d] strength[%d]\n",
				dtvdd_devp->freq, strength);

			/*update */
			dbg_lst_status = s;
			last_lock = ilock;
		}
	}

	return ret;
}

static int gxtv_demod_atsc_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	if (!get_dtvpll_init_flag())
		return 0;

	if (c->modulation > QAM_AUTO) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			*ber = atsc_read_reg_v4(ATSC_FEC_BER);
		else
			*ber = atsc_read_reg(0x980) & 0xffff;
	} else if ((c->modulation == QAM_256) || (c->modulation == QAM_64)) {
		*ber = dvbc_get_status();
	}

	return 0;
}

static int gxtv_demod_atsc_read_signal_strength
	(struct dvb_frontend *fe, u16 *strength)
{
	int strenth;

	strenth = tuner_get_ch_power(fe);

	if (!strncmp(fe->ops.tuner_ops.info.name, "r842", 4)) {
		if ((fe->dtv_property_cache.modulation <= QAM_AUTO) &&
			(fe->dtv_property_cache.modulation != QPSK))
			strenth += 18;
		else {
			strenth += 15;
			if (strenth <= -80)
				strenth = atsc_get_power_strength(
					atsc_read_reg_v4(0x44) & 0xfff,
						strenth);
		}

		if (strenth < -100)
			*strength = 0;
		else
			*strength = strenth + 100;
	} else
		*strength = tuner_get_ch_power3();

	if (*strength > 100)
		*strength = 100;

	return 0;
}

static int gxtv_demod_atsc_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_sts demod_sts;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("dvbt2_tune, devp is NULL\n");
		return -1;
	}

	if ((c->modulation <= QAM_AUTO)	&& (c->modulation != QPSK)) {
		dvbc_status(devp, &demod_sts, NULL);
		*snr = demod_sts.ch_snr / 100;
	} else if (c->modulation > QAM_AUTO) {
		*snr = atsc_read_snr();
	}

	return 0;
}

static int gxtv_demod_atsc_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct amldtvdemod_device_s *devp = dtvdd_devp;

	if (!devp->demod_thread)
		return 0;
	if (c->modulation > QAM_AUTO)
		atsc_thread();
	*ucblocks = 0;
	return 0;
}

static int gxtv_demod_atsc_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_atsc param_atsc;
	struct aml_demod_dvbc param_j83b;
	int temp_freq = 0;
	union ATSC_DEMOD_REG_0X6A_BITS Val_0x6a;
	union atsc_cntl_reg_0x20 val;
	int nco_rate;
	/*[0]: specturm inverse(1),normal(0); [1]:if_frequency*/
	unsigned int tuner_freq[2] = {0};
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;
	struct amldtvdemod_device_s *devp = dtvdd_devp;

	PR_ATSC("%s: delsys: %d, freq: %d, symbol_rate: %d, bw: %d, modulation: %d, invert: %d.\n",
			__func__, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);

	memset(&param_atsc, 0, sizeof(param_atsc));
	memset(&param_j83b, 0, sizeof(param_j83b));
	if (!devp->demod_thread)
		return 0;

	dtvdd_devp->freq = c->frequency / 1000;
	last_lock = -1;
	dtvdd_devp->atsc_mode = c->modulation;
	tuner_set_params(fe);

	if ((c->modulation <= QAM_AUTO) && (c->modulation != QPSK)) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			/* add for j83b 256 qam, livetv and timeshif */
			demod_status.clk_freq = DEMOD_CLK_167M;
			nco_rate = (demod_status.adc_freq * 256) / demod_status.clk_freq + 2;
			front_write_bits(AFIFO_ADC, nco_rate,
					 AFIFO_NCO_RATE_BIT, AFIFO_NCO_RATE_WID);
			front_write_reg(SFIFO_OUT_LENS, 0x5);
			/* sys clk = 167M */
			dd_tvafe_hiu_reg_write(D_HHI_DEMOD_CLK_CNTL, 0x502);
		} else {
			/* txlx j.83b set sys clk to 222M */
			dd_tvafe_hiu_reg_write(D_HHI_DEMOD_CLK_CNTL, 0x502);
		}

		demod_set_mode_ts(SYS_DVBC_ANNEX_A);
		param_j83b.ch_freq = c->frequency / 1000;
		param_j83b.mode = amdemod_qam(c->modulation);
		PR_ATSC("gxtv_demod_atsc_set_frontend, modulation: %d\n", c->modulation);
		if (c->modulation == QAM_64)
			param_j83b.symb_rate = 5057;
		else if (c->modulation == QAM_256)
			param_j83b.symb_rate = 5361;
		else
			param_j83b.symb_rate = 5361;

		dvbc_set_ch(&demod_status, &param_j83b);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			qam_write_reg(0x7, 0x10f33);
			set_j83b_filter_reg_v4();
			qam_write_reg(0x12, 0x50e1000);
			qam_write_reg(0x30, 0x41f2f69);
		}
	} else if (c->modulation > QAM_AUTO) {
		if (fe->ops.tuner_ops.get_if_frequency)
			fe->ops.tuner_ops.get_if_frequency(fe, tuner_freq);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			Val_0x6a.bits = atsc_read_reg_v4(ATSC_DEMOD_REG_0X6A);
			Val_0x6a.b.peak_thd = 0x6;//Let CCFO Quality over 6
			atsc_write_reg_v4(ATSC_DEMOD_REG_0X6A, Val_0x6a.bits);
			atsc_write_reg_v4(ATSC_EQ_REG_0XA5, 0x8c);
			/* bit30: enable CCI */
			atsc_write_reg_v4(ATSC_EQ_REG_0X92, 0x40000240);
			/* bit0~3: AGC bandwidth select */
			atsc_write_reg_v4(ATSC_DEMOD_REG_0X58, 0x528220d);
			/* clk recover confidence control */
			atsc_write_reg_v4(ATSC_DEMOD_REG_0X5D, 0x14400202);
			/* CW bin frequency */
			atsc_write_reg_v4(ATSC_DEMOD_REG_0X61, 0x2ee);

			/*bit 2: invert specturm, for r842 tuner AGC control*/
			if (tuner_freq[0] == 1)
				atsc_write_reg_v4(ATSC_DEMOD_REG_0X56, 0x4);
			else
				atsc_write_reg_v4(ATSC_DEMOD_REG_0X56, 0x0);

			if (demod_status.adc_freq == ADC_CLK_24M) {
				atsc_write_reg_v4(ATSC_DEMOD_REG_0X54,
					0x1aaaaa);

				atsc_write_reg_v4(ATSC_DEMOD_REG_0X55,
					0x3ae28d);

				atsc_write_reg_v4(ATSC_DEMOD_REG_0X6E,
					0x16e3600);
			}

			/*for timeshift mosaic issue*/
			atsc_write_reg_v4(0x12, 0x18);
			val.bits = atsc_read_reg_v4(ATSC_CNTR_REG_0X20);
			val.b.cpu_rst = 1;
			atsc_write_reg_v4(ATSC_CNTR_REG_0X20, val.bits);
			usleep_range(1000, 1001);
			val.b.cpu_rst = 0;
			atsc_write_reg_v4(ATSC_CNTR_REG_0X20, val.bits);
			usleep_range(5000, 5001);
			devp->last_status = 0;
		} else {
			/*demod_set_demod_reg(0x507, TXLX_ADC_REG6);*/
			dd_tvafe_hiu_reg_write(D_HHI_DEMOD_CLK_CNTL, 0x507);
			demod_set_mode_ts(delsys);
			param_atsc.ch_freq = c->frequency / 1000;
			param_atsc.mode = c->modulation;
			atsc_set_ch(&demod_status, /*&demod_i2c,*/ &param_atsc);

			/* bit 2: invert specturm, 0:normal, 1:invert */
			if (tuner_freq[0] == 1)
				atsc_write_reg(0x716, atsc_read_reg(0x716) | 0x4);
			else
				atsc_write_reg(0x716, atsc_read_reg(0x716) & ~0x4);
		}
	}

	if ((auto_search_std == 1) && ((c->modulation <= QAM_AUTO)
	&& (c->modulation != QPSK))) {
		unsigned char s = 0;

		msleep(std_lock_timeout);
		s = amdemod_stat_islock(SYS_DVBC_ANNEX_A);
		if (s == 1) {
			PR_DBG("atsc std mode is %d locked\n", dtvdd_devp->atsc_mode);

			return 0;
		}
		if ((c->frequency == 79000000) || (c->frequency == 85000000)) {
			temp_freq = (c->frequency + 2000000) / 1000;
			param_j83b.ch_freq = temp_freq;
			PR_DBG("irc fre:%d\n", param_j83b.ch_freq);
			c->frequency = param_j83b.ch_freq * 1000;


			tuner_set_params(fe);
			demod_set_mode_ts(SYS_DVBC_ANNEX_A);
			param_j83b.mode = amdemod_qam(c->modulation);
			if (c->modulation == QAM_64)
				param_j83b.symb_rate = 5057;
			else if (c->modulation == QAM_256)
				param_j83b.symb_rate = 5361;
			else
				param_j83b.symb_rate = 5361;
			dvbc_set_ch(&demod_status, &param_j83b);

			msleep(std_lock_timeout);
			s = amdemod_stat_islock(SYS_DVBC_ANNEX_A);
			if (s == 1) {
				PR_DBG("irc mode is %d locked\n", dtvdd_devp->atsc_mode);
			} else {
				temp_freq = (c->frequency - 1250000) / 1000;
				param_j83b.ch_freq = temp_freq;
				PR_DBG("hrc fre:%d\n", param_j83b.ch_freq);
				c->frequency = param_j83b.ch_freq * 1000;

				tuner_set_params(fe);
				demod_set_mode_ts(SYS_DVBC_ANNEX_A);
				param_j83b.mode = amdemod_qam(c->modulation);
				if (c->modulation == QAM_64)
					param_j83b.symb_rate = 5057;
				else if (c->modulation == QAM_256)
					param_j83b.symb_rate = 5361;
				else
					param_j83b.symb_rate = 5361;
				dvbc_set_ch(&demod_status, &param_j83b);
			}
		} else {
			param_j83b.ch_freq = (c->frequency - 1250000) / 1000;
			PR_DBG("hrc fre:%d\n", param_j83b.ch_freq);
			c->frequency = param_j83b.ch_freq * 1000;

			demod_set_mode_ts(SYS_DVBC_ANNEX_A);
			param_j83b.mode = amdemod_qam(c->modulation);
			if (c->modulation == QAM_64)
				param_j83b.symb_rate = 5057;
			else if (c->modulation == QAM_256)
				param_j83b.symb_rate = 5361;
			else
				param_j83b.symb_rate = 5361;
			dvbc_set_ch(&demod_status, &param_j83b);
		}
	}
	PR_DBG("atsc_mode is %d\n", dtvdd_devp->atsc_mode);
	return 0;
}

static int gxtv_demod_atsc_get_frontend(struct dvb_frontend *fe)
{                               /*these content will be writed into eeprom .*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	PR_DBG("c->frequency is %d\n", c->frequency);
	return 0;
}

void atsc_detect_first(struct dvb_frontend *fe, enum fe_status *status, unsigned int re_tune)
{
	unsigned int ucblocks;
	unsigned int atsc_status;
	enum fe_status s;
	int strenth;
	int cnt;
	int check_ok;
	static unsigned int times;
	enum ATSC_SYS_STA sys_sts;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!unlikely(devp)) {
		PR_ERR("%s, devp is NULL\n", __func__);
		return;
	}

	strenth = tuner_get_ch_power(fe);

	/*agc control,fine tune strength*/
	if (!strncmp(fe->ops.tuner_ops.info.name, "r842", 4)) {
		strenth += 15;
		if (strenth <= -80)
			strenth = atsc_get_power_strength(
				atsc_read_reg_v4(0x44) & 0xfff, strenth);
	}

	PR_ATSC("tuner strength: %d\n", strenth);
	if (strenth < THRD_TUNER_STRENTH_ATSC) {
		*status = FE_TIMEDOUT;
		devp->last_status = *status;
		PR_ATSC("tuner:no signal!\n");
		return;
	}

	sys_sts = (atsc_read_reg_v4(ATSC_CNTR_REG_0X2E) >> 4) & 0xf;
	if (re_tune || (sys_sts < ATSC_SYS_STA_DETECT_CFO_USE_PILOT))
		times = 0;
	else
		times++;

	#define CNT_FIRST_ATSC  (2)
	check_ok = 0;

	for (cnt = 0; cnt < CNT_FIRST_ATSC; cnt++) {
		if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			gxtv_demod_atsc_read_ucblocks(fe, &ucblocks);

		if (gxtv_demod_atsc_read_status(devp, &s) == 2)
			times = 0;

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			/* detect pn after detect cfo after 375 ms */
			if ((times == 2) && (sys_sts < ATSC_SYS_STA_DETECT_PN_IN_EQ_OUT)) {
				*status = FE_TIMEDOUT;
				PR_INFO("can't detect pn, not atsc sig\n");
			} else {
				*status = s;
			}

			devp->last_status = *status;

			break;
		}

		if (s != 0x1f) {
			gxtv_demod_atsc_read_ber(fe, &atsc_status);
			if ((atsc_status < 0x60)) {
				*status = FE_TIMEDOUT;
				check_ok = 1;
			}
		} else {
			check_ok = 1;
			*status = s;
		}

		if (check_ok)
			break;

	}

	PR_DBGL("%s,detect=0x%x,cnt=%d\n", __func__, (unsigned int)*status, cnt);
}


static int dvb_j83b_count = 5;
module_param(dvb_j83b_count, int, 0644);
MODULE_PARM_DESC(dvb_atsc_count, "dvb_j83b_count");
/*come from j83b_speedup_func*/

static int atsc_j83b_detect_first(struct dvb_frontend *fe, enum fe_status *s)
{
	int j83b_status, i;
	/*struct dvb_frontend_private *fepriv = fe->frontend_priv;*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int strenth;
	enum fe_status cs;
	int cnt;
	int check_ok;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!unlikely(devp)) {
		PR_ERR("%s, devp is NULL\n", __func__);
		return -1;
	}

	j83b_status = 0;

	/*tuner:*/
	if (dvb_tuner_delay > 9)
		msleep(dvb_tuner_delay);

	strenth = tuner_get_ch_power(fe);

	/*agc control,fine tune strength*/
	if (!strncmp(fe->ops.tuner_ops.info.name, "r842", 4))
		strenth += 18;

	if (strenth < THRD_TUNER_STRENTH_J83) {
		*s = FE_TIMEDOUT;
		PR_ATSC("tuner:no signal!j83\n");
		return 0;
	}
	check_ok = 0;

	/*first check signal max time*/
	#define CNT_FIRST  (10)

	for (cnt = 0; cnt < CNT_FIRST; cnt++) {
		gxtv_demod_atsc_read_status(devp, &cs);

		if (cs != 0x1f) {
			/*msleep(200);*/
			PR_DBG("[j.83b] 1\n");
			for (i = 0; i < dvb_j83b_count; i++) {
				msleep(25);
				gxtv_demod_atsc_read_ber(fe, &j83b_status);

				/*J.83 status >=0x38,has signal*/
				if (j83b_status >= 0x3)
					break;
			}
			PR_DBG("[rsj]j.83b_status is %x,modulation is %d\n",
					j83b_status,
					c->modulation);

			if (j83b_status < 0x3) {
				*s = FE_TIMEDOUT;
				check_ok = 1;
			}

		} else {
			/*have signal*/
			*s = cs;
			check_ok = 1;
		}

		if (check_ok)
			break;

		msleep(50);
	}


	if (!check_ok)
		*s = FE_TIMEDOUT;

	return 0;
}

static int atsc_j83b_polling(struct dvb_frontend *fe, enum fe_status *s)
{
	int j83b_status, i;
	/*struct dvb_frontend_private *fepriv = fe->frontend_priv;*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int strenth;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!unlikely(devp)) {
		PR_ERR("%s, devp is NULL\n", __func__);
		return -1;
	}

	j83b_status = 0;
	strenth = tuner_get_ch_power(fe);
	if (strenth < THRD_TUNER_STRENTH_J83) {
		*s = FE_TIMEDOUT;
		PR_DBGL("tuner:no signal!j83\n");
		return 0;
	}

	gxtv_demod_atsc_read_status(devp, s);

	if (*s != 0x1f) {
		/*msleep(200);*/
		PR_DBG("[j.83b] 1\n");
		for (i = 0; i < dvb_j83b_count; i++) {
			msleep(25);
			gxtv_demod_atsc_read_ber(fe, &j83b_status);

			/*J.83 status >=0x38,has signal*/
			if (j83b_status >= 0x3)
				break;
		}
		PR_DBG("[rsj]j.83b_status is %x,modulation is %d\n",
				j83b_status,
				c->modulation);

		if (j83b_status < 0x3)
			*s = FE_TIMEDOUT;
	}




	return 0;
}

void atsc_polling(struct dvb_frontend *fe, enum fe_status *status)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct amldtvdemod_device_s *devp = dtvdd_devp;

	if (!devp->demod_thread)
		return;

	if (c->modulation == QPSK) {
		PR_DBG("mode is qpsk, return;\n");
		/*return;*/
	} else if (c->modulation <= QAM_AUTO) {
		atsc_j83b_polling(fe, status);
	} else {
		atsc_thread();
		gxtv_demod_atsc_read_status(devp, status);
	}



}

static int gxtv_demod_atsc_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	/*
	 * It is safe to discard "params" here, as the DVB core will sync
	 * fe->dtv_property_cache with fepriv->parameters_in, where the
	 * DVBv3 params are stored. The only practical usage for it indicate
	 * that re-tuning is needed, e. g. (fepriv->state & FESTATE_RETUNE) is
	 * true.
	 */
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct amldtvdemod_device_s *devp = dtvdd_devp;

	*delay = HZ / 4;

	if (!devp->demod_thread)
		return 0;

	if (re_tune) {
		dtvdd_devp->en_detect = 1; /*fist set*/
		gxtv_demod_atsc_set_frontend(fe);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			timer_begain(D_TIMER_DETECT);

		if (c->modulation ==  QPSK) {
			PR_ATSC("modulation is QPSK do nothing!");
		} else if (c->modulation <= QAM_AUTO) {
			PR_ATSC("j83\n");
			atsc_j83b_detect_first(fe, status);
		} else if (c->modulation > QAM_AUTO) {
			atsc_detect_first(fe, status, re_tune);
		}

		return 0;
	}

	if (!dtvdd_devp->en_detect) {
		PR_DBGL("tune:not enable\n");
		return 0;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (c->modulation > QAM_AUTO)
			atsc_detect_first(fe, status, re_tune);
		else if (c->modulation <= QAM_AUTO &&	(c->modulation !=  QPSK))
			atsc_j83b_detect_first(fe, status);
	} else {
		atsc_polling(fe, status);
	}

	return 0;

}

static int dvbt_isdbt_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	/*
	 * It is safe to discard "params" here, as the DVB core will sync
	 * fe->dtv_property_cache with fepriv->parameters_in, where the
	 * DVBv3 params are stored. The only practical usage for it indicate
	 * that re-tuning is needed, e. g. (fepriv->state & FESTATE_RETUNE) is
	 * true.
	 */

	*delay = HZ/2;

	/*PR_ATSC("%s:\n", __func__);*/
#if 1
	if (re_tune) {

		timer_begain(D_TIMER_DETECT);
		PR_INFO("%s:\n", __func__);
		dtvdd_devp->en_detect = 1; /*fist set*/

		dvbt_isdbt_set_frontend(fe);
		dvbt_isdbt_read_status(fe, status);
		return 0;
	}
#endif
	if (!dtvdd_devp->en_detect) {
		PR_DBGL("tune:not enable\n");
		return 0;
	}
	/*polling*/
	dvbt_isdbt_read_status(fe, status);

	if (*status & FE_HAS_LOCK) {
		timer_disable(D_TIMER_SET);
	} else {
		if (!timer_is_en(D_TIMER_SET))
			timer_begain(D_TIMER_SET);
	}

	if (timer_is_enough(D_TIMER_SET)) {
		dvbt_isdbt_set_frontend(fe);
		timer_disable(D_TIMER_SET);
	}

	return 0;
}

static void dvbt_rst_demod(struct amldtvdemod_device_s *devp)
{
	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
		demod_top_write_reg(DEMOD_TOP_REGC, 0x110011);
		demod_top_write_reg(DEMOD_TOP_REGC, 0x110010);
		usleep_range(1000, 1001);
		demod_top_write_reg(DEMOD_TOP_REGC, 0x110011);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
		front_write_bits(AFIFO_ADC, 0x80, AFIFO_NCO_RATE_BIT,
				 AFIFO_NCO_RATE_WID);
		front_write_reg(0x22, 0x7200a06);
		front_write_reg(0x2f, 0x0);
		front_write_reg(0x39, 0x40001000);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
	}

	dvbt_reg_initial(devp->bw);
}

static int dvbt_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	unsigned int fsm;
	static unsigned int cnt;

	if (unlikely(!devp))
		return -1;

	*delay = HZ / 5;

	if (!devp->demod_thread)
		return 0;

	if (re_tune) {
		PR_INFO("%s:\n", __func__);
		dtvdd_devp->en_detect = 1; /*fist set*/

		dvbt_set_frontend(devp, fe);
		timer_begain(D_TIMER_DETECT);
		dvbt_read_status(fe, status);
		cnt = 1;
		return 0;
	}

	if (!dtvdd_devp->en_detect) {
		PR_DBGL("tune:not enable\n");
		return 0;
	}

	/*polling*/
	dvbt_read_status(fe, status);

	dvbt_info(devp, NULL);

	/* GID lock */
	if (dvbt_t2_rdb(0x2901) >> 5 & 0x1)
		/* reduce agc target to default, otherwise will influence on snr */
		dvbt_t2_wrb(0x15d6, 0x50);
	else
		/* increase agc target to make signal strong enouth for locking */
		dvbt_t2_wrb(0x15d6, 0xa0);

	if (*status ==
	    (FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC)) {
		dvbt_t2_wr_byte_bits(0x2906, 2, 3, 4);
		dvbt_t2_wrb(0x2815, 0x02);
	} else {
		dvbt_t2_wr_byte_bits(0x2906, 0, 3, 4);
		dvbt_t2_wrb(0x2815, 0x03);
	}

	fsm = dvbt_t2_rdb(DVBT_STATUS);

	if (((cnt % 5) == 2 && (fsm & 0xf) < 9) ||
	    ((cnt % 5) == 2 && (fsm & 0xf) == 9 && (fsm >> 6 & 1) && (*status != 0x1f))) {
		dvbt_rst_demod(devp);
		cnt = 0;
		PR_INFO("rst, tps or ts unlock\n");
	}

	cnt++;

	return 0;
}

static int dvbt2_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	static unsigned int cnt;
	static unsigned int prt_sts_cnt;
	unsigned int val, snr, l1_post_decoded, ldpc;
	unsigned int modulation, code_rate;
	unsigned int snr_min_x10db = 0;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("dvbt2_tune, devp is NULL\n");
		return -1;
	}

	*delay = HZ / 8;

	if (!devp->demod_thread)
		return 0;

	if (re_tune) {
		PR_INFO("%s:\n", __func__);
		dtvdd_devp->en_detect = 1; /*fist set*/

		dvbt2_set_frontend(fe);
		timer_begain(D_TIMER_DETECT);
		dvbt2_read_status(fe, status);
		/* return directly, cnt increase 1 */
		cnt = 1;
		prt_sts_cnt = 1;
		return 0;
	}

	if (!dtvdd_devp->en_detect) {
		PR_DBGL("tune:not enable\n");
		return 0;
	}

	/*polling*/
	dvbt2_read_status(fe, status);
	if (devp->debug_on && !(prt_sts_cnt++ % 3)) {
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x182);
		dvbt2_info(NULL);

		if (demod_is_t5d_cpu(devp))
			demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
	}

	/* test bus definition
	 * val[30]:0x839[3],[29-24]:0x805,[23-21]:0x2a09,[20-13]:0x2a08,[12:7]:0xa50
	 * val[6-2]:0x8c3,[1]:0x361b[7],[0]:0x581[7]
	 */
	if (demod_is_t5d_cpu(devp)) {
		val = front_read_reg(0x3e);
		snr = val >> 13;
		modulation = (val >> 2) & 0x1f;
		modulation = (modulation >> 3) & 0x3;
		code_rate = (val >> 2) & 0x1f;
		code_rate = code_rate & 0x7;
		l1_post_decoded = (val >> 30) & 0x01;
		ldpc = (val >> 7) & 0x3f;
	} else {
		snr = dvbt_t2_rdb(0x2a09) << 8;
		snr |= dvbt_t2_rdb(0x2a08);
		modulation = (dvbt_t2_rdb(0x8c3) >> 4) & 0x7;
		code_rate = (dvbt_t2_rdb(0x8c3) >> 1) & 0x7;
		l1_post_decoded = (dvbt_t2_rdb(0x839) >> 3) & 0x1;
		ldpc = dvbt_t2_rdb(0xa50);
	}

	snr &= 0x7ff;
	snr = snr * 3 / 64;

	if (modulation < 4)
		snr_min_x10db = minimum_snr_x10[modulation][code_rate];
	else
		PR_ERR("modulation is overflow\n");

	PR_DVBT("modulation:%d, code_rate: %d\n", modulation, code_rate);
	PR_DVBT("snr*10 = %d, snr_min_x10dB=%d, L1 POST:%d,LDPC: %d\n",
		snr * 10, snr_min_x10db, l1_post_decoded, ldpc);

	/* reset demod after ts unlock more than 2s */
	if (*status != 0x1f)  {
		if (cnt == 17) {
			dvbt2_reset(devp);
			cnt = 0;
			PR_INFO("rst demod due to ts unlock more than 2s\n");
		}
	} else {
		cnt = 0;
	}

	cnt++;
	return 0;
}

static int dtvdemod_atsc_init(struct amldtvdemod_device_s *devp)
{
	struct aml_demod_sys sys;
	struct dtv_frontend_properties *c = &devp->frontend.dtv_property_cache;

	if (!devp) {
		pr_err("%s devp is NULL\n", __func__);
		return -EFAULT;
	}

	PR_DBG("%s\n", __func__);

	memset(&sys, 0, sizeof(sys));
	memset(&demod_status, 0, sizeof(demod_status));
	demod_status.delsys = SYS_ATSC;
	sys.adc_clk = ADC_CLK_24M;

	PR_ATSC("c->modulation : %d\n", c->modulation);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		sys.demod_clk = DEMOD_CLK_250M;
	else
		sys.demod_clk = DEMOD_CLK_225M;

	demod_status.ch_if = 5000;
	demod_status.tmp = ADC_MODE;
	demod_set_sys(devp, &demod_status, &sys);

	return 0;
}

static void gxtv_demod_dtmb_release(struct dvb_frontend *fe)
{
}

static int gxtv_demod_dtmb_read_status
	(struct dvb_frontend *fe, enum fe_status *status)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	*status = pollm->last_s;
	return 0;
}

void dtmb_save_status(unsigned int s)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	if (s) {
		pollm->last_s =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;

	} else {
		pollm->last_s = FE_TIMEDOUT;
	}
}

void dtmb_poll_start(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	pollm->last_s = 0;
	pollm->flg_restart = 1;
	PR_DTMB("dtmb_poll_start2\n");
}


void dtmb_poll_stop(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	pollm->flg_stop = 1;

}
void dtmb_set_delay(unsigned int delay)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	pollm->delayms = delay;
	pollm->flg_updelay = 1;
}
unsigned int dtmb_is_update_delay(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	return pollm->flg_updelay;
}
unsigned int dtmb_get_delay_clear(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	pollm->flg_updelay = 0;

	return pollm->delayms;
}

void dtmb_poll_clear(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	memset(pollm, 0, sizeof(struct poll_machie_s));

}

#define DTMBM_NO_SIGNEL_CHECK	0x01
#define DTMBM_HV_SIGNEL_CHECK	0x02
#define DTMBM_BCH_OVER_CHEK	0x04

#define DTMBM_CHEK_NO		(DTMBM_NO_SIGNEL_CHECK)
#define DTMBM_CHEK_HV		(DTMBM_HV_SIGNEL_CHECK | DTMBM_BCH_OVER_CHEK)
#define DTMBM_WORK		(DTMBM_NO_SIGNEL_CHECK | DTMBM_HV_SIGNEL_CHECK\
					| DTMBM_BCH_OVER_CHEK)

#define DTMBM_POLL_CNT_NO_SIGNAL	(10)
#define DTMBM_POLL_CNT_WAIT_LOCK	(3) /*from 30 to 3 */
#define DTMBM_POLL_DELAY_NO_SIGNAL	(120)
#define DTMBM_POLL_DELAY_HAVE_SIGNAL	(100)
/*dtmb_poll_v3 is same as dtmb_check_status_txl*/
void dtmb_poll_v3(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;
	unsigned int bch_tmp;
	unsigned int s;

	if (!pollm->state) {
		/* idle */
		/* idle -> start check */
		if (!pollm->flg_restart) {
			PR_DBG("x");
			return;
		}
	} else {
		if (pollm->flg_stop) {
			PR_DBG("dtmb poll stop !\n");
			dtmb_poll_clear();
			dtmb_set_delay(3*HZ);
			return;
		}
	}

	/* restart: clear */
	if (pollm->flg_restart) {
		PR_DBG("dtmb poll restart!\n");
		dtmb_poll_clear();

	}
	PR_DBG("-");
	s = check_dtmb_fec_lock();

	/* bch exceed the threshold: wait lock*/
	if (pollm->state & DTMBM_BCH_OVER_CHEK) {
		if (pollm->crrcnt < DTMBM_POLL_CNT_WAIT_LOCK) {
			pollm->crrcnt++;

			if (s) {
				PR_DBG("after reset get lock again!cnt=%d\n",
					pollm->crrcnt);
				dtmb_constell_check();
				pollm->state = DTMBM_HV_SIGNEL_CHECK;
				pollm->crrcnt = 0;
				pollm->bch = dtmb_reg_r_bch();


				dtmb_save_status(s);
			}
		} else {
			PR_DBG("can't lock after reset!\n");
			pollm->state = DTMBM_NO_SIGNEL_CHECK;
			pollm->crrcnt = 0;
			/* to no signal*/
			dtmb_save_status(s);
		}
		return;
	}


	if (s) {
		/*have signal*/
		if (!pollm->state) {
			pollm->state = DTMBM_CHEK_NO;
			PR_DBG("from idle to have signal wait 1\n");
			return;
		}
		if (pollm->state & DTMBM_CHEK_NO) {
			/*no to have*/
			PR_DBG("poll machie: from no signal to have signal\n");
			pollm->bch = dtmb_reg_r_bch();
			pollm->state = DTMBM_HV_SIGNEL_CHECK;

			dtmb_set_delay(DTMBM_POLL_DELAY_HAVE_SIGNAL);
			return;
		}


		bch_tmp = dtmb_reg_r_bch();
		if (bch_tmp > (pollm->bch + 50)) {
			pollm->state = DTMBM_BCH_OVER_CHEK;

			PR_DBG("bch add ,need reset,wait not to reset\n");
			dtmb_reset();

			pollm->crrcnt = 0;
			dtmb_set_delay(DTMBM_POLL_DELAY_HAVE_SIGNAL);
		} else {
			pollm->bch = bch_tmp;
			pollm->state = DTMBM_HV_SIGNEL_CHECK;

			dtmb_save_status(s);
			/*have signale to have signal*/
			dtmb_set_delay(300);
		}
		return;
	}


	/*no signal */
	if (!pollm->state) {
		/* idle -> no signal */
		PR_DBG("poll machie: from idle to no signal\n");
		pollm->crrcnt = 0;

		pollm->state = DTMBM_NO_SIGNEL_CHECK;
	} else if (pollm->state & DTMBM_CHEK_HV) {
		/*have signal -> no signal*/
		PR_DBG("poll machie: from have signal to no signal\n");
		pollm->crrcnt = 0;
		pollm->state = DTMBM_NO_SIGNEL_CHECK;
		dtmb_save_status(s);
	}

	/*no siganel check process */
	if (pollm->crrcnt < DTMBM_POLL_CNT_NO_SIGNAL) {
		dtmb_no_signal_check_v3();
		pollm->crrcnt++;

		dtmb_set_delay(DTMBM_POLL_DELAY_NO_SIGNAL);
	} else {
		dtmb_no_signal_check_finishi_v3();
		pollm->crrcnt = 0;

		dtmb_save_status(s);
		/*no signal to no signal*/
		dtmb_set_delay(300);
	}

}

void dtmb_poll_start_tune(unsigned int state)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	dtmb_poll_clear();

	pollm->state = state;
	if (state & DTMBM_NO_SIGNEL_CHECK)
		dtmb_save_status(0);
	else
		dtmb_save_status(1);
	PR_DTMB("dtmb_poll_start tune to %d\n", state);

}

/*come from gxtv_demod_dtmb_read_status, have ms_delay*/
int dtmb_poll_v2(struct dvb_frontend *fe, enum fe_status *status)
{
	int ilock;
	unsigned char s = 0;

	s = dtmb_check_status_gxtv(fe);

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		*status = FE_TIMEDOUT;
	}
	if (last_lock != ilock) {
		PR_INFO("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}


/*this is ori gxtv_demod_dtmb_read_status*/
static int gxtv_demod_dtmb_read_status_old
	(struct dvb_frontend *fe, enum fe_status *status)
{

	int ilock;
	unsigned char s = 0;

	if (is_meson_gxtvbb_cpu()) {
		dtmb_check_status_gxtv(fe);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		dtmb_bch_check();
		if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			dtmb_check_status_txl(fe);
	} else {
		return -1;
	}

	s = amdemod_stat_islock(SYS_DTMB);

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		*status = FE_TIMEDOUT;
	}
	if (last_lock != ilock) {
		PR_INFO("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int gxtv_demod_dtmb_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	*ber = 0;

	return 0;
}

static int gxtv_demod_dtmb_read_signal_strength
		(struct dvb_frontend *fe, u16 *strength)
{
	int tuner_sr;

	if (!strncmp(fe->ops.tuner_ops.info.name, "r842", 4)) {
		tuner_sr = tuner_get_ch_power2();
		tuner_sr += 16;

		if (tuner_sr < -100)
			*strength = 0;
		else
			*strength = tuner_sr + 100;
	} else
		*strength = tuner_get_ch_power3();
	return 0;
}

static int gxtv_demod_dtmb_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	int tmp, snr_avg;

	tmp = snr_avg = 0;
	/* tmp = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR);*/
	tmp = dtmb_reg_r_che_snr();

	*snr = convert_snr(tmp);

	return 0;
}

static int gxtv_demod_dtmb_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}


static int gxtv_demod_dtmb_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dtmb param;
	int times;
	/*[0]: specturm inverse(1),normal(0); [1]:if_frequency*/
	unsigned int tuner_freq[2] = {0};
	struct amldtvdemod_device_s *devp = dtvdd_devp;

	PR_DTMB("%s: delsys: %d, freq: %d, symbol_rate: %d, bw: %d, modulation: %d, invert: %d.\n",
			__func__, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz, c->modulation, c->inversion);

	if (!devp->demod_thread)
		return 0;

	times = 2;
	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;

	last_lock = -1;
	tuner_set_params(fe);	/*aml_fe_analog_set_frontend(fe);*/
	msleep(100);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (fe->ops.tuner_ops.get_if_frequency)
			fe->ops.tuner_ops.get_if_frequency(fe, tuner_freq);
		if (tuner_freq[0] == 0)
			demod_status.spectrum = 0;
		else if (tuner_freq[0] == 1)
			demod_status.spectrum = 1;
		else
			pr_err("wrong specturm val get from tuner\n");
	}

	dtmb_set_ch(&demod_status, /*&demod_i2c,*/ &param);

	return 0;
}


static int gxtv_demod_dtmb_get_frontend(struct dvb_frontend *fe)
{                               /*these content will be writed into eeprom .*/

	return 0;
}

int Gxtv_Demod_Dtmb_Init(struct amldtvdemod_device_s *dev)
{
	struct aml_demod_sys sys;
	/*struct aml_demod_i2c i2c;*/
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!devp) {
		pr_err("%s devp is NULL\n", __func__);
		return -EFAULT;
	}

	PR_DBG("AML Demod DTMB init\r\n");

	memset(&sys, 0, sizeof(sys));
	/*memset(&i2c, 0, sizeof(i2c));*/
	memset(&demod_status, 0, sizeof(demod_status));
	demod_status.delsys = SYS_DTMB;

	if (is_meson_gxtvbb_cpu()) {
		sys.adc_clk = ADC_CLK_25M;
		sys.demod_clk = DEMOD_CLK_200M;
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		if (is_meson_txl_cpu()) {
			sys.adc_clk = ADC_CLK_25M;
			sys.demod_clk = DEMOD_CLK_225M;
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			sys.adc_clk = ADC_CLK_24M;
			sys.demod_clk = DEMOD_CLK_250M;
		} else {
			sys.adc_clk = ADC_CLK_24M;
			sys.demod_clk = DEMOD_CLK_225M;
		}
	} else {
		return -1;
	}

	demod_status.ch_if = SI2176_5M_IF;
	demod_status.tmp = ADC_MODE;
	demod_status.spectrum = dev->spectrum;
	demod_set_sys(devp, &demod_status, &sys);

	return 0;
}

#ifdef DVB_CORE_ORI
unsigned int demod_dvbc_get_fast_search(void)
{
	return demod_dvbc_speedup_en;
}

void demod_dvbc_set_fast_search(unsigned int en)
{
	if (en)
		demod_dvbc_speedup_en = 1;
	else
		demod_dvbc_speedup_en = 0;
}

void demod_dvbc_fsm_reset(void)
{
	qam_write_reg(0x7, qam_read_reg(0x7) & ~(1 << 4));
	qam_write_reg(0x3a, 0x0);
	qam_write_reg(0x7, qam_read_reg(0x7) | (1 << 4));
	qam_write_reg(0x3a, 0x4);
}

static enum qam_md_e dvbc_switch_qam(enum qam_md_e qam_mode)
{
	enum qam_md_e ret;

	switch (qam_mode) {
	case QAM_MODE_16:
		ret = QAM_MODE_256;
		break;

	case QAM_MODE_32:
		ret = QAM_MODE_16;
		break;

	case QAM_MODE_64:
		ret = QAM_MODE_32;
		break;

	case QAM_MODE_128:
		ret = QAM_MODE_64;
		break;

	case QAM_MODE_256:
		ret = QAM_MODE_128;
		break;

	default:
		ret = QAM_MODE_256;
		break;
	}

	return ret;
}

//return val : 0=no signal,1=has signal,2=waiting
unsigned int dvbc_auto_fast(struct amldtvdemod_device_s *devp, unsigned int *delay, bool re_tune)
{
	static unsigned int times, no_sig_cnt;
	static enum qam_md_e qam_mode = QAM_MODE_64;
	unsigned int i;
	char qam_name[20];
	static unsigned int time_start;

	if (re_tune) {
		/*avoid that the previous channel has not been locked/unlocked*/
		/*the next channel is set again*/
		/*the wait time is too long,then miss channel*/
		no_sig_cnt = 0;
		times = 1;
		qam_mode = QAM_MODE_256;
		demod_dvbc_set_qam(qam_mode);
		demod_dvbc_fsm_reset();
		time_start = jiffies_to_msecs(jiffies);
		PR_DVBC("%s : retune reset\n", __func__);
		return 2;
	}

	if (tuner_get_ch_power2() < -87) {
		no_sig_cnt = 0;
		times = 0;
		*delay = HZ / 4;
		qam_mode = QAM_MODE_256;
		PR_DVBC("%s: tuner no signal\n", __func__);
		return 0;
	}

	/* symbol rate, 0=auto */
	if (devp->auto_sr) {
		for (i = 0; i < 3; i++) {
			devp->sr_val_hw = dvbc_get_symb_rate();
			PR_DVBC("srate : %d\n", devp->sr_val_hw);
			qam_write_bits(0xd, devp->sr_val_hw & 0xffff, 0, 16);
			qam_write_bits(0x11, devp->sr_val_hw & 0xffff, 8, 16);
			msleep(30);
		}

		if (jiffies_to_msecs(jiffies) - time_start < 900)
			return 2;
	}

	dvbc_get_qam_name(qam_mode, qam_name);
	PR_DVBC("fast search : times = %d, %s\n", times, qam_name);
	if (qam_mode == QAM_MODE_256) {
		*delay = HZ / 2;//500ms
	} else {
		*delay = HZ / 5;//200ms
	}

	if ((qam_read_reg(0x31) & 0xf) < 3) {
		no_sig_cnt++;

		if (no_sig_cnt == 2 && times == 2) {//250ms
			no_sig_cnt = 0;
			times = 0;
			*delay = HZ / 4;
			qam_mode = QAM_MODE_256;
			return 0;
		}
	} else if ((qam_read_reg(0x31) & 0xf) == 5) {
		no_sig_cnt = 0;
		times = 0;
		*delay = HZ / 4;
		qam_mode = QAM_MODE_256;
		return 1;
	}

	if (times == 15) {
		times = 0;
		no_sig_cnt = 0;
		*delay = HZ / 4;
		qam_mode = QAM_MODE_256;
		return 0;
	}

	times++;
	/* loop from 16 to 256 */
	qam_mode = dvbc_switch_qam(qam_mode);
	demod_dvbc_set_qam(qam_mode);
//	demod_dvbc_fsm_reset();
	if (qam_mode == QAM_MODE_16)
		demod_dvbc_fsm_reset();

	return 2;
}

//return val : 0=no signal,1=has signal,2=waiting
static unsigned int dvbc_fast_search(unsigned int *delay, bool re_tune)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	unsigned int i;
	static unsigned int time_start;
	static unsigned int times, no_sig_cnt;

	if (re_tune) {
		/*avoid that the previous channel has not been locked/unlocked*/
		/*the next channel is set again*/
		/*the wait time is too long,then miss channel*/
		no_sig_cnt = 0;
		times = 1;
		demod_dvbc_fsm_reset();
		time_start = jiffies_to_msecs(jiffies);
		PR_DVBC("fast search : retune reset\n");
		return 2;
	}

	if (!unlikely(devp)) {
		PR_ERR("%s, devp is NULL\n", __func__);
		return 1;
	}

	if (tuner_get_ch_power2() < -87) {
		no_sig_cnt = 0;
		times = 0;
		*delay = HZ / 4;
		PR_DVBC("%s: tuner no signal\n", __func__);
		return 0;
	}

	/* symbol rate, 0=auto */
	if (devp->auto_sr) {
		for (i = 0; i < 3; i++) {
			devp->sr_val_hw = dvbc_get_symb_rate();
			PR_DVBC("srate : %d\n", devp->sr_val_hw);
			qam_write_bits(0xd, devp->sr_val_hw & 0xffff, 0, 16);
			qam_write_bits(0x11, devp->sr_val_hw & 0xffff, 8, 16);
			msleep(30);
		}

		if (jiffies_to_msecs(jiffies) - time_start < 900)
			return 2;
	}

	PR_DVBC("%s : times = %d\n", __func__, times);

	if (times < 3)
		*delay = HZ / 8;//125ms
	else
		*delay = HZ / 2;//500ms

	if ((qam_read_reg(0x31) & 0xf) < 3) {
		no_sig_cnt++;

		if (no_sig_cnt == 2 && times == 2) {//250ms
			no_sig_cnt = 0;
			times = 0;
			*delay = HZ / 4;
			return 0;
		}
	} else if ((qam_read_reg(0x31) & 0xf) == 5) {
		no_sig_cnt = 0;
		times = 0;
		*delay = HZ / 4;
		return 1;
	}

	if (times == 7) {
		times = 0;
		no_sig_cnt = 0;
		*delay = HZ / 4;
		return 0;
	}

	demod_dvbc_fsm_reset();
	times++;

	return 2;
}

static int gxtv_demod_dvbc_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	int ret = 0;
	unsigned int sig_flg;
	struct amldtvdemod_device_s *devp = dtvdd_devp;

	*delay = HZ / 4;

	if (!devp->demod_thread)
		return 0;

	if (re_tune) {
		/*first*/
		dtvdd_devp->en_detect = 1;

		gxtv_demod_dvbc_set_frontend(fe);

		if (demod_dvbc_speedup_en == 1) {
			devp->fast_search_finish = 0;
			*status = 0;
			*delay = HZ / 8;
			qam_write_reg(0x65, 0x400c);
			qam_write_reg(0x60, 0x10466000);
			qam_write_reg(0xac, (qam_read_reg(0xac) & (~0xff00))
				| 0x800);
			qam_write_reg(0xae, (qam_read_reg(0xae)
				& (~0xff000000)) | 0x8000000);

			if (fe->dtv_property_cache.modulation == QAM_AUTO)
				dvbc_auto_fast(devp, delay, re_tune);
		} else
			qam_write_reg(0x65, 0x800c);

		if (demod_dvbc_speedup_en == 1)
			return 0;

		timer_begain(D_TIMER_DETECT);
		gxtv_demod_dvbc_read_status_timer(fe, status);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			demod_dvbc_speed_up(status);

		PR_DBG("tune finish!\n");

		return ret;
	}

	if (!dtvdd_devp->en_detect) {
		PR_DBGL("tune:not enable\n");
		return ret;
	}

	if (demod_dvbc_speedup_en == 1) {
		if (!devp->fast_search_finish) {
			if (fe->dtv_property_cache.modulation == QAM_AUTO)
				sig_flg = dvbc_auto_fast(devp, delay, re_tune);
			else
				sig_flg = dvbc_fast_search(delay, re_tune);

			switch (sig_flg) {
			case 0:
				*status = FE_TIMEDOUT;
				devp->fast_search_finish = 0;
				demod_dvbc_fsm_reset();
				PR_DVBC(">>>unlock<<<\n");
				break;
			case 1:
				*status =
				FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
				FE_HAS_VITERBI | FE_HAS_SYNC;
				devp->fast_search_finish = 1;
				PR_DVBC(">>>lock<<<\n");
				break;
			case 2:
				*status = 0;
				break;
			default:
				PR_DVBC("wrong return value\n");
				break;
			}
		}	else {
			gxtv_demod_dvbc_read_status_timer(fe, status);
		}
	}	else {
		gxtv_demod_dvbc_read_status_timer(fe, status);
	}

	if (demod_dvbc_speedup_en == 1)
		return 0;

	if (*status & FE_HAS_LOCK) {
		timer_disable(D_TIMER_SET);
	} else {
		if (!timer_is_en(D_TIMER_SET))
			timer_begain(D_TIMER_SET);
	}

	if (timer_is_enough(D_TIMER_SET)) {
		gxtv_demod_dvbc_set_frontend(fe);
		timer_disable(D_TIMER_SET);
	}

	return ret;
}

static int dtvdemod_dvbs_set_ch(struct aml_demod_sta *demod_sta)
{
	int ret = 0;
	unsigned int symb_rate;

	symb_rate = demod_sta->symb_rate;

	/* 0:16, 1:32, 2:64, 3:128, 4:256 */
	demod_sta->agc_mode = 1;
	demod_sta->ch_bw = 8000;
	if (demod_sta->ch_if == 0)
		demod_sta->ch_if = 5000;

	dvbs2_reg_initial(symb_rate);

	return ret;
}

int dtvdemod_dvbs_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int ret = 0;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!devp) {
		pr_err("%s devp is NULL\n", __func__);
		return -EFAULT;
	}

	PR_DVBS("%s: delsys: %d, freq: %d, symbol_rate: %d, bw: %d.\n",
			__func__, c->delivery_system, c->frequency, c->symbol_rate,
			c->bandwidth_hz);

	demod_status.symb_rate = c->symbol_rate / 1000;
	last_lock = -1;

	tuner_set_params(fe);
	dtvdemod_dvbs_set_ch(&demod_status);
	devp->time_start = jiffies_to_msecs(jiffies);

	return ret;
}

int dtvdemod_dvbs_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	int ilock;
	unsigned char s = 0;
	int strenth;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!devp) {
		pr_err("%s devp is NULL\n", __func__);
		return -EFAULT;
	}

	strenth = tuner_get_ch_power(fe);

	PR_DVBS("tuner strength: %d\n", strenth);
	if (strenth <= -50) {
		strenth = (5 * 100) / 17;
		strenth *= dvbs_rd_byte(0x91a);
		strenth /= 100;
		strenth -= 122;
	}
	PR_DVBS("strength: %d\n", strenth);

//	if (strenth < THRD_TUNER_STRENTH_DVBS) {
//		*status = FE_TIMEDOUT;
//		last_lock = -1;
//		PR_DVBS("tuner:no signal!\n");
//		return 0;
//	}

	devp->time_passed = jiffies_to_msecs(jiffies) - devp->time_start;
	if (devp->time_passed >= 500) {
		if (!dvbs_rd_byte(0x160)) {
			*status = FE_TIMEDOUT;
			last_lock = -1;
			PR_DVBS("not dvbs signal\n");
			return 0;
		}
	}

	s = amdemod_stat_islock(SYS_DVBS);

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		if (timer_not_enough(D_TIMER_DETECT)) {
			ilock = 0;
			*status = 0;

		} else {
			ilock = 0;
			*status = FE_TIMEDOUT;
			timer_disable(D_TIMER_DETECT);
		}
	}

	if (last_lock != ilock) {
		PR_INFO("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int dtvdemod_dvbs_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	int ret = 0;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	*delay = HZ / 4;

	if (unlikely(!devp)) {
		PR_ERR("%s, devp is NULL\n", __func__);
		return -1;
	}

	if (!devp->blind_scan_stop)
		return ret;

	if (re_tune) {
		/*first*/
		dtvdd_devp->en_detect = 1;
		dtvdemod_dvbs_set_frontend(fe);
		timer_begain(D_TIMER_DETECT);
		dtvdemod_dvbs_read_status(fe, status);
		pr_info("tune finish!\n");
		return ret;
	}

	if (!dtvdd_devp->en_detect) {
		pr_err("tune:not enable\n");
		return ret;
	}

	dtvdemod_dvbs_read_status(fe, status);
	dvbs_check_status(NULL);

	return ret;
}

static int dtvdemod_dvbs2_init(void)
{
	struct aml_demod_sys sys;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (!devp) {
		pr_err("%s devp is NULL\n", __func__);
		return -EFAULT;
	}

	PR_DBG("%s\n", __func__);
	memset(&sys, 0, sizeof(sys));

	demod_status.delsys = SYS_DVBS2;
	sys.adc_clk = ADC_CLK_135M;
	sys.demod_clk = DEMOD_CLK_270M;
	demod_status.tmp = CRY_MODE;

	demod_status.ch_if = SI2176_5M_IF * 1000;
	PR_DBG("[%s]adc_clk is %d,demod_clk is %d\n", __func__, sys.adc_clk,
	       sys.demod_clk);
	auto_flags_trig = 0;
	demod_set_sys(devp, &demod_status, &sys);
	aml_dtv_demode_isr_en(devp, 1);
	/*enable diseqc irq*/
	return 0;
}

static int gxtv_demod_dtmb_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	/*struct dtv_frontend_properties *c = &fe->dtv_property_cache;*/
	int ret = 0;
//	unsigned int up_delay;
	unsigned int firstdetet;



	if (re_tune) {
		/*first*/
		dtvdd_devp->en_detect = 1;

		*delay = HZ / 4;
		gxtv_demod_dtmb_set_frontend(fe);
		firstdetet = dtmb_detect_first();

		if (firstdetet == 1) {
			*status = FE_TIMEDOUT;
			/*polling mode*/
			dtmb_poll_start_tune(DTMBM_NO_SIGNEL_CHECK);

		} else if (firstdetet == 2) {  /*need check*/
			*status = FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
			dtmb_poll_start_tune(DTMBM_HV_SIGNEL_CHECK);

		} else if (firstdetet == 0) {
			PR_DBG("use read_status\n");
			gxtv_demod_dtmb_read_status_old(fe, status);
			if (*status == (0x1f))
				dtmb_poll_start_tune(DTMBM_HV_SIGNEL_CHECK);
			else
				dtmb_poll_start_tune(DTMBM_NO_SIGNEL_CHECK);


		}
		PR_DBG("tune finish!\n");

		return ret;
	}

	if (!dtvdd_devp->en_detect) {
		PR_DBG("tune:not enable\n");
		return ret;
	}

	*delay = HZ / 4;
	gxtv_demod_dtmb_read_status_old(fe, status);

	if (*status == (FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
		FE_HAS_VITERBI | FE_HAS_SYNC))
		dtmb_poll_start_tune(DTMBM_HV_SIGNEL_CHECK);
	else
		dtmb_poll_start_tune(DTMBM_NO_SIGNEL_CHECK);

	return ret;

}

#endif



#ifdef CONFIG_CMA
bool dtvdemod_cma_alloc(struct amldtvdemod_device_s *devp)
{
	bool ret;

	unsigned int mem_size = devp->cma_mem_size;
	/*	dma_alloc_from_contiguous*/
	devp->venc_pages =
		dma_alloc_from_contiguous(&(devp->this_pdev->dev),
			mem_size >> PAGE_SHIFT, 0);
	PR_DBG("[cma]mem_size is %d,%d\n",
		mem_size, mem_size >> PAGE_SHIFT);
	if (devp->venc_pages) {
		devp->mem_start = page_to_phys(devp->venc_pages);
		devp->mem_size  = mem_size;
		devp->flg_cma_allc = true;
		PR_DBG("demod mem_start = 0x%x, mem_size = 0x%x\n",
			devp->mem_start, devp->mem_size);
		PR_DBG("demod cma alloc ok!\n");
		ret = true;
	} else {
		PR_DBG("demod cma mem undefined2.\n");
		ret = false;
	}
	return ret;
}

void dtvdemod_cma_release(struct amldtvdemod_device_s *devp)
{
	/*	dma_release_from_contiguous*/
	dma_release_from_contiguous(&(devp->this_pdev->dev),
			devp->venc_pages,
			devp->cma_mem_size>>PAGE_SHIFT);
		PR_DBG("demod cma release ok!\n");
	devp->mem_start = 0;
	devp->mem_size = 0;
}
#endif

static void set_agc_pinmux(enum fe_delivery_system delsys, unsigned int on)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	char pin_name[20];

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		/* dvbs connects to rf agc pin due to no IF */
		strcpy(pin_name, "rf_agc_pins");
		break;

	default:
		strcpy(pin_name, "if_agc_pins");
		break;
	}

	if (on) {
		devp->pin = devm_pinctrl_get_select(devp->dev, pin_name);
		if (IS_ERR(devp->pin)) {
			devp->pin = NULL;
			PR_ERR("get agc pins fail: %s\n", pin_name);
		}
	} else {
		if (!IS_ERR_OR_NULL(devp->pin)) {
			devm_pinctrl_put(devp->pin);
			devp->pin = NULL;
		}
	}

	PR_INFO("%s '%s' %d done.\n", __func__, pin_name, on);
}

static bool enter_mode(enum fe_delivery_system delsys)
{
	/*struct aml_fe_dev *dev = fe->dtv_demod;*/
	struct amldtvdemod_device_s *devp = dtvdd_devp;
	int ret = 0;

	if (delsys < SYS_ANALOG)
		PR_INFO("%s:%s\n", __func__, name_fe_delivery_system[delsys]);
	else
		PR_ERR("%s:%d\n", __func__, delsys);

	set_agc_pinmux(delsys, 1);

	/*-------------------*/
	/* must enable the adc ref signal for demod, */
	/*vdac_enable(1, VDAC_MODULE_DTV_DEMOD);*/
	dtvdemod_vdac_enable(1);/*on*/
	dtvdd_devp->en_detect = 0;/**/
	dtmb_poll_stop();/*polling mode*/

	auto_flags_trig = 1;
	if (cci_thread)
		if (dvbc_get_cci_task() == 1)
			dvbc_create_cci_task();
	/*mem_buf = (long *)phys_to_virt(memstart);*/
	if (devp->cma_flag == 1 && !devp->flg_cma_allc) {
		PR_DBG("CMA MODE, cma flag is %d,mem size is %d",
				devp->cma_flag, devp->cma_mem_size);

		if (!dtvdemod_cma_alloc(devp)) {
			ret = -ENOMEM;
			return ret;
		}
	}

	switch (delsys) {
	case SYS_DTMB:
		ret = Gxtv_Demod_Dtmb_Init(devp);
		devp->act_dtmb = true;
		dtmb_set_mem_st(devp->mem_start);

		if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			demod_top_write_reg(DEMOD_TOP_REGC, 0x8);
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = Gxtv_Demod_Dvbc_Init(ADC_MODE);

		/* The maximum time of signal detection is 3s */
		timer_set_max(D_TIMER_DETECT, devp->timeout_dvbc_ms);
		/* reset is 4s */
		timer_set_max(D_TIMER_SET, 4000);
		PR_DVBC("timeout is %dms\n", devp->timeout_dvbc_ms);
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		ret = dtvdemod_atsc_init(devp);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
			timer_set_max(D_TIMER_DETECT, devp->timeout_atsc_ms);

		PR_ATSC("timeout is %dms\n", devp->timeout_atsc_ms);
		break;

	case SYS_DVBT:
		ret = dvbt_init();
		timer_set_max(D_TIMER_DETECT, devp->timeout_dvbt_ms);
		PR_DVBT("timeout is %dms\n", devp->timeout_dvbt_ms);
		break;

	case SYS_DVBT2:
		if (devp->data->hw_ver == DTVDEMOD_HW_T5D) {
			devp->dmc_saved = dtvdemod_dmc_reg_read(0x274);
			PR_INFO("dmc val 0x%x\n", devp->dmc_saved);
			dtvdemod_dmc_reg_write(0x274, 0x18100000);
		}

		dtvdemod_dvbt2_init();
		timer_set_max(D_TIMER_DETECT, devp->timeout_dvbt_ms);
		PR_DVBT("timeout is %dms\n", devp->timeout_dvbt_ms);
		break;

	case SYS_ISDBT:
		ret = dvbt_isdbt_Init();
		/*The maximum time of signal detection is 2s */
		timer_set_max(D_TIMER_DETECT, 2000);
		/*reset is 4s*/
		timer_set_max(D_TIMER_SET, 4000);

		PR_DBG("[im]memstart is %x\n", devp->mem_start);
		dvbt_isdbt_wr_reg((0x10 << 2), devp->mem_start);
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		dtvdemod_dvbs2_init();
		timer_set_max(D_TIMER_DETECT, devp->timeout_dvbs_ms);
		PR_DVBS("timeout is %dms\n", devp->timeout_dvbs_ms);
		break;

	default:
		break;
	}

	return ret;
}

static int leave_mode(enum fe_delivery_system delsys)
{
/*	struct aml_fe_dev *dev = fe->dtv_demod;*/
	struct amldtvdemod_device_s *devn = dtvdd_devp;

	if (delsys < SYS_ANALOG)
		PR_INFO("%s:%s\n", __func__, name_fe_delivery_system[delsys]);
	else
		PR_ERR("%s:%d\n", __func__, delsys);

	devn->en_detect = 0;
	devn->last_delsys = SYS_UNDEFINED;

	dtvpll_init_flag(0);
	/*dvbc_timer_exit();*/
	if (cci_thread)
		dvbc_kill_cci_task();

	switch (delsys) {
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		devn->atsc_mode = 0;
		break;

	case SYS_DTMB:
		if (devn->act_dtmb) {
			dtmb_poll_stop();	/*polling mode*/
			/* close arbit */
			demod_top_write_reg(DEMOD_TOP_REG0, 0x0);
			demod_top_write_reg(DEMOD_TOP_REGC, 0x0);
			devn->act_dtmb = false;
		}
		break;

	case SYS_DVBT:
		/* disable dvbt mode to avoid hang when switch to other demod */
		demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
		break;

	case SYS_ISDBT:
	case SYS_DVBT2:
		if ((devn->data->hw_ver == DTVDEMOD_HW_T5D) && (delsys == SYS_DVBT2)) {
			PR_INFO("resume dmc val 0x%x\n", devn->dmc_saved);
			dtvdemod_dmc_reg_write(0x274, devn->dmc_saved);
		}

		/* disable dvbt mode to avoid hang when switch to other demod */
		demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
		demod_top_write_reg(DEMOD_TOP_CFG_REG_4, 0x0);
		break;
	case SYS_DVBS:
	case SYS_DVBS2:
		/*disable irq*/
		aml_dtv_demode_isr_en(devn, 0);
		/* disable dvbs mode to avoid hang when switch to other demod */
		demod_top_write_reg(DEMOD_TOP_REGC, 0x11);
		break;
	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	adc_set_pll_cntl(0, ADC_DTV_DEMOD, NULL);
	adc_set_pll_cntl(0, ADC_DTV_DEMODPLL, NULL);
#endif
	/* should disable the adc ref signal for demod */
	/*vdac_enable(0, VDAC_MODULE_DTV_DEMOD);*/
	dtvdemod_vdac_enable(0);/*off*/
	set_agc_pinmux(delsys, 0);

	if (devn->cma_flag == 1 && devn->flg_cma_allc) {
		dtvdemod_cma_release(devn);
		devn->flg_cma_allc = false;
	}

	return 0;
}

/* when can't get ic_config by dts, use this*/
const struct meson_ddemod_data  data_gxtvbb = {
	.regoff = {
		.off_demod_top = 0xc00,
		.off_dvbc = 0x400,
		.off_dtmb = 0x00,
	},
};

const struct meson_ddemod_data  data_txlx = {
	.regoff = {
		.off_demod_top = 0xf00,
		.off_dvbc = 0xc00,
		.off_dtmb = 0x000,
		.off_dvbt_isdbt = 0x400,
		.off_isdbt = 0x400,
		.off_atsc = 0x800,
	},
	.hw_ver = DTVDEMOD_HW_TXLX,
};

const struct meson_ddemod_data  data_tl1 = {
	.regoff = {
		.off_demod_top = 0x3c00,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_front = 0x3800,
	},
};

const struct meson_ddemod_data  data_t5 = {
	.regoff = {
		.off_demod_top = 0x3c00,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_front = 0x3800,
	},
	.hw_ver = DTVDEMOD_HW_T5,
};

const struct meson_ddemod_data  data_t5d = {
	.regoff = {
		.off_demod_top = 0xf000,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_isdbt = 0x800,
		.off_front = 0x3800,
		.off_dvbs = 0x2000,
		.off_dvbt_isdbt = 0x800,
		.off_dvbt_t2 = 0x0000,
	},
	.hw_ver = DTVDEMOD_HW_T5D,
};

const struct meson_ddemod_data  data_t5d_revb = {
	.regoff = {
		.off_demod_top = 0xf000,
		.off_dvbc = 0x1000,
		.off_dtmb = 0x0000,
		.off_atsc = 0x0c00,
		.off_isdbt = 0x800,
		.off_front = 0x3800,
		.off_dvbs = 0x2000,
		.off_dvbt_isdbt = 0x800,
		.off_dvbt_t2 = 0x0000,
	},
	.hw_ver = DTVDEMOD_HW_T5D_B,
};

static const struct of_device_id meson_ddemod_match[] = {
	{
		.compatible = "amlogic, ddemod-gxtvbb",
		.data		= &data_gxtvbb,
	}, {
		.compatible = "amlogic, ddemod-txl",
		.data		= &data_gxtvbb,
	}, {
		.compatible = "amlogic, ddemod-txlx",
		.data		= &data_txlx,
	}, {
		.compatible = "amlogic, ddemod-gxlx",
		.data		= &data_txlx,
	}, {
		.compatible = "amlogic, ddemod-txhd",
		.data		= &data_txlx,
	}, {
		.compatible = "amlogic, ddemod-tl1",
		.data		= &data_tl1,
	}, {
		.compatible = "amlogic, ddemod-tm2",
		.data		= &data_tl1,
	}, {
		.compatible = "amlogic, ddemod-t5",
		.data		= &data_t5,
	}, {
		.compatible = "amlogic, ddemod-t5d",
		.data		= &data_t5d,
	}, {
		.compatible = "amlogic, ddemod-t5d-revB",
		.data		= &data_t5d_revb,
	},
	/* DO NOT remove, to avoid scan err of KASAN */
	{}
};



/*
 * dds_init_reg_map - physical addr map
 *
 * map physical address of I/O memory resources
 * into the core virtual address space
 */
static int dds_init_reg_map(struct platform_device *pdev)
{

	struct ss_reg_phy *preg = &dtvdd_devp->reg_p[0];
	struct ss_reg_vt *pv = &dtvdd_devp->reg_v[0];
	int i;
	struct resource *res = 0;
	int size = 0;
	int ret = 0;

	for (i = 0; i < ES_MAP_ADDR_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			PR_ERR("%s: res %d is faile\n", __func__, i);
			ret = -ENOMEM;
			break;
		}
		size = resource_size(res);
		preg[i].size = size;
		preg[i].phy_addr = res->start;
		pv[i].v = devm_ioremap_nocache(&pdev->dev, res->start, size);
	}

	if (dtvdd_devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		dtvdd_devp->dmc_phy_addr = 0xff638000;
		dtvdd_devp->dmc_v_addr = devm_ioremap_nocache(&pdev->dev, 0xff638000, 0x2000);

		dtvdd_devp->ddr_phy_addr = 0xff63c000;
		dtvdd_devp->ddr_v_addr = devm_ioremap_nocache(&pdev->dev, 0xff63c000, 0x2000);
	}

	return ret;
}

irqreturn_t aml_dtv_demode_isr(int irq, void *dev_id)
{
	struct amldtvdemod_device_s *devn = dtvdd_devp;

	if (devn->last_delsys == SYS_DVBS || devn->last_delsys == SYS_DVBS2)
		aml_diseqc_isr();

	/*PR_INFO("%s\n", __func__);*/
	return IRQ_HANDLED;
}

void aml_dtv_demode_isr_en(struct amldtvdemod_device_s *devp, u32 en)
{
	if (en) {
		if (!devp->demod_irq_en && devp->demod_irq_num > 0) {
			enable_irq(devp->demod_irq_num);
			devp->demod_irq_en = 1;
			PR_INFO("enable demod_irq\n");
		}
	} else {
		if (devp->demod_irq_en && devp->demod_irq_num > 0) {
			disable_irq(devp->demod_irq_num);
			devp->demod_irq_en = 0;
			PR_INFO("disable demod_irq\n");
		}
	}
}

int dtvdemod_set_iccfg_by_dts(struct platform_device *pdev)
{
	u32 value;
	int ret;
	struct pinctrl_state *pin_agc_st;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	PR_DBG("%s:\n", __func__);

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0)
		PR_INFO("no reserved mem.\n");

	/*agc pinmux: option*/
	if (of_get_property(pdev->dev.of_node, "pinctrl-names", NULL)) {
		devp->pin = devm_pinctrl_get(&pdev->dev);

		pin_agc_st = pinctrl_lookup_state(devp->pin, "if_agc_pins");
		if (IS_ERR(pin_agc_st))
			PR_ERR("no pin_agc_st: if_agc_pins\n");

		pin_agc_st = pinctrl_lookup_state(devp->pin, "rf_agc_pins");
		if (IS_ERR(pin_agc_st))
			PR_ERR("no pin_agc_st: rf_agc_pins\n");
	} else {
		PR_INFO("pinmux:not define in dts\n");
		devp->pin = NULL;
	}

#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, "spectrum", &value);
	if (!ret) {
		devp->spectrum = value;
		PR_INFO("spectrum: %d\n", value);
	} else {
		devp->spectrum = 2;
	}
#else
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spectrum");
	if (res) {
		int spectrum = res->start;

		devp->spectrum = spectrum;
	} else {
		devp->spectrum = 0;
	}
#endif

#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, "cma_flag", &value);
	if (!ret) {
		devp->cma_flag = value;
		PR_INFO("cma_flag: %d\n", value);
	} else {
		devp->cma_flag = 0;
	}
#else
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cma_flag");
	if (res) {
		int cma_flag = res->start;

		devp->cma_flag = cma_flag;
	} else {
		devp->cma_flag = 0;
	}
#endif

#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, "atsc_version", &value);
	if (!ret) {
		devp->atsc_version = value;
		PR_INFO("atsc_version: %d\n", value);
	} else {
		devp->atsc_version = 0;
	}
#else
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					"atsc_version");
	if (res) {
		int atsc_version = res->start;

		devp->atsc_version = atsc_version;
	} else {
		devp->atsc_version = 0;
	}
#endif

	if (devp->cma_flag == 1) {
#ifdef CONFIG_CMA
#ifdef CONFIG_OF
		ret = of_property_read_u32(pdev->dev.of_node,
					"cma_mem_size", &value);
		if (!ret) {
			devp->cma_mem_size = value;
			PR_INFO("cma_mem_size: %d\n", value);
		} else {
			devp->cma_mem_size = 0;
		}
#else
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"cma_mem_size");
		if (res) {
			int cma_mem_size = res->start;

			devp->cma_mem_size = cma_mem_size;
		} else {
			devp->cma_mem_size = 0;
		}
#endif
		devp->cma_mem_size =
				dma_get_cma_size_int_byte(&pdev->dev);
			devp->this_pdev = pdev;
			devp->cma_mem_alloc = 0;
			PR_INFO("[cma]demod cma_mem_size = %d MB\n",
					(u32)devp->cma_mem_size / SZ_1M);
#endif
		} else {
#ifdef CONFIG_OF
		devp->mem_start = memstart;
#endif
	}

	/* diseqc sel */
	ret = of_property_read_string(pdev->dev.of_node, "diseqc_name",
			&devp->diseqc_name);
	if (ret) {
		PR_INFO("diseqc_name:not define in dts\n");
		devp->diseqc_name = NULL;
	} else {
		PR_INFO("diseqc_name name:%s\n", devp->diseqc_name);
	}

	/*get demod irq*/
	ret = of_irq_get_byname(pdev->dev.of_node, "demod_isr");
	if (ret > 0) {
		devp->demod_irq_num = ret;
		ret = request_irq(devp->demod_irq_num, aml_dtv_demode_isr,
				  IRQF_SHARED, "demod_isr",
				  (void *)devp);
		if (ret != 0)
			PR_INFO("req demod_isr fail\n");
		disable_irq(devp->demod_irq_num);
		devp->demod_irq_en = false;
		PR_INFO("demod_isr num:%d\n", devp->demod_irq_num);
	} else {
		devp->demod_irq_num = 0;
		PR_INFO("no demod isr\n");
	}

	/*diseqc pin ctrl*/
	if (!IS_ERR_OR_NULL(devp->pin)) {
		devp->diseqc_pin_st =
			pinctrl_lookup_state(devp->pin, "diseqc");
		if (IS_ERR_OR_NULL(devp->diseqc_pin_st))
			PR_INFO("no diseqc pinctrl\n");
		else
			pinctrl_select_state(devp->pin,
					     devp->diseqc_pin_st);

		devm_pinctrl_put(devp->pin);
		devp->pin = NULL;
	}

	return 0;

}

#if (defined CONFIG_AMLOGIC_DTV_DEMOD)	/*move to aml_dtv_demod*/
static int rmem_demod_device_init(struct reserved_mem *rmem, struct device *dev)
{
	unsigned int demod_mem_start;
	unsigned int demod_mem_size;

	demod_mem_start = rmem->base;
	demod_mem_size = rmem->size;
	memstart = demod_mem_start;
	pr_info("demod reveser memory 0x%x, size %dMB.\n",
		demod_mem_start, (demod_mem_size >> 20));
	return 1;
}

static void rmem_demod_device_release(struct reserved_mem *rmem,
				      struct device *dev)
{
}

static const struct reserved_mem_ops rmem_demod_ops = {
	.device_init = rmem_demod_device_init,
	.device_release = rmem_demod_device_release,
};

static int __init rmem_demod_setup(struct reserved_mem *rmem)
{
	/*
	 * struct cma *cma;
	 * int err;
	 * pr_info("%s setup.\n",__func__);
	 * err = cma_init_reserved_mem(rmem->base, rmem->size, 0, &cma);
	 * if (err) {
	 *      pr_err("Reserved memory: unable to setup CMA region\n");
	 *      return err;
	 * }
	 */
	rmem->ops = &rmem_demod_ops;
	/* rmem->priv = cma; */

	pr_info
	    ("DTV demod reserved memory: %pa, size %ld MiB\n",
	     &rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}

RESERVEDMEM_OF_DECLARE(demod, "amlogic, demod-mem", rmem_demod_setup);
#endif

/* It's a correspondence with enum es_map_addr*/

void dbg_ic_cfg(void)
{
	struct ss_reg_phy *preg = &dtvdd_devp->reg_p[0];
	int i;

	for (i = 0; i < ES_MAP_ADDR_NUM; i++)
		PR_INFO("reg:%s:st=0x%x,size=0x%x\n",
			name_reg[i], preg[i].phy_addr, preg[i].size);
}

void dbg_reg_addr(void)
{
	struct ss_reg_vt *regv = &dtvdd_devp->reg_v[0];
	int i;

	PR_INFO("%s\n", __func__);

	PR_INFO("reg address offset:\n");
	PR_INFO("\tdemod top:\t0x%x\n", dtvdd_devp->data->regoff.off_demod_top);
	PR_INFO("\tdvbc:\t0x%x\n", dtvdd_devp->data->regoff.off_dvbc);
	PR_INFO("\tdtmb:\t0x%x\n", dtvdd_devp->data->regoff.off_dtmb);
	PR_INFO("\tdvbt/isdbt:\t0x%x\n", dtvdd_devp->data->regoff.off_dvbt_isdbt);
	PR_INFO("\tisdbt:\t0x%x\n", dtvdd_devp->data->regoff.off_isdbt);
	PR_INFO("\tatsc:\t0x%x\n", dtvdd_devp->data->regoff.off_atsc);
	PR_INFO("\tfront:\t0x%x\n", dtvdd_devp->data->regoff.off_front);

	PR_INFO("virtual addr:\n");
	for (i = 0; i < ES_MAP_ADDR_NUM; i++)
		PR_INFO("\t%s:\t0x%p\n", name_reg[i], regv[i].v);


}

static void dtvdemod_clktree_probe(struct device *dev)
{
	dtvdd_devp->clk_gate_state = 0;

	dtvdd_devp->vdac_clk_gate = devm_clk_get(dev, "vdac_clk_gate");
	if (IS_ERR(dtvdd_devp->vdac_clk_gate))
		PR_ERR("error: %s: clk vdac_clk_gate\n", __func__);
}

static void dtvdemod_clktree_remove(struct device *dev)
{
	if (!IS_ERR(dtvdd_devp->vdac_clk_gate))
		devm_clk_put(dev, dtvdd_devp->vdac_clk_gate);
}
static void vdac_clk_gate_ctrl(int status)
{
	if (status) {
		if (dtvdd_devp->clk_gate_state) {
			PR_INFO("clk_gate is already on\n");
			return;
		}

		if (IS_ERR(dtvdd_devp->vdac_clk_gate))
			PR_ERR("error: %s: vdac_clk_gate\n", __func__);
		else
			clk_prepare_enable(dtvdd_devp->vdac_clk_gate);

		dtvdd_devp->clk_gate_state = 1;
	} else {
		if (dtvdd_devp->clk_gate_state == 0) {
			PR_INFO("clk_gate is already off\n");
			return;
		}

		if (IS_ERR(dtvdd_devp->vdac_clk_gate))
			PR_ERR("error: %s: vdac_clk_gate\n", __func__);
		else
			clk_disable_unprepare(dtvdd_devp->vdac_clk_gate);

		dtvdd_devp->clk_gate_state = 0;
	}
}
/*
 * use dtvdemod_vdac_enable replace vdac_enable
 */
static void dtvdemod_vdac_enable(bool on)
{
	if (on) {
		vdac_clk_gate_ctrl(1);
		vdac_enable(1, VDAC_MODULE_DTV_DEMOD);
	} else {
		vdac_clk_gate_ctrl(0);
		vdac_enable(0, VDAC_MODULE_DTV_DEMOD);
	}
}

static int dtvdemod_request_firmware(const char *file_name, char *buf, int size)
{
	int ret = -1;
	const struct firmware *fw;
	int offset = 0;
	unsigned int i;

	if (!buf) {
		pr_err("%s fw buf is NULL\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	ret = request_firmware(&fw, file_name, dtvdd_devp->dev);
	if (ret < 0) {
		pr_err("%d can't load the %s.\n", ret, file_name);
		goto err;
	}

	if (fw->size > size) {
		pr_err("not enough memory size for fw.\n");
		ret = -ENOMEM;
		goto release;
	}

	memcpy(buf, (char *)fw->data + offset, fw->size - offset);
	ret = fw->size;

	PR_DBGL("dtvdemod_request_firmware:\n");
	for (i = 0; i < 100; i++)
		PR_DBGL("[%d] = 0x%x\n", i, fw->data[i]);
release:
	release_firmware(fw);
err:
	return ret;
}

static int fw_check_sum(char *buf, unsigned int len)
{
	unsigned int crc;

	crc = crc32_le(~0U, buf, len);

	PR_INFO("firmware crc result : 0x%x, len: %d\n", crc ^ ~0U, len);

	/* return fw->head.checksum != (crc ^ ~0U) ? 0 : 1; */
	return 0;
}

static int dtvdemod_download_firmware(struct amldtvdemod_device_s *devp)
{
	char *path = __getname();
	int len = 0;
	int size = 0;

	if (!path)
		return -ENOMEM;

	len = snprintf(path, PATH_MAX_LEN, "%s/%s", FIRMWARE_DIR, FIRMWARE_NAME);
	if (len >= PATH_MAX_LEN)
		goto path_fail;

	strncpy(devp->firmware_path, path, sizeof(devp->firmware_path));
	devp->firmware_path[sizeof(devp->firmware_path) - 1] = '\0';

	size = dtvdemod_request_firmware(devp->firmware_path, devp->fw_buf, FW_BUFF_SIZE);

	if (size >= 0)
		fw_check_sum(devp->fw_buf, size);

path_fail:
	__putname(path);

	return size;
}

static void dtvdemod_fw_dwork(struct work_struct *work)
{
	int ret;
	static unsigned int cnt;
	struct delayed_work *dwork = to_delayed_work(work);
	struct amldtvdemod_device_s *devp =
		container_of(dwork, struct amldtvdemod_device_s, fw_dwork);

	if (!devp) {
		pr_info("%s, dwork error !!!\n", __func__);
		return;
	}

	ret = dtvdemod_download_firmware(dtvdd_devp);

	if ((ret < 0) && (cnt < 10))
		schedule_delayed_work(&dtvdd_devp->fw_dwork, 5 * HZ);
	else
		cancel_delayed_work(&dtvdd_devp->fw_dwork);

	cnt++;
}

static void dvbs_blind_scan_work(struct work_struct *work)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	unsigned int freq, srate = 45000000;
	unsigned int freq_min = devp->blind_min_fre;
	unsigned int freq_max = devp->blind_max_fre;
	unsigned int freq_step = devp->blind_fre_step;
	//unsigned int symbol_rate_hw;
	struct dvb_frontend *fe = aml_get_fe();
	enum fe_status status;
	unsigned int freq_one_percent;

	status = 0;

	PR_INFO("%s\n", __func__);
	if (unlikely(!devp)) {
		PR_ERR("%s err, devp is NULL\n", __func__);
		return;
	}

	if (unlikely(!fe)) {
		PR_ERR("%s err, fe is NULL\n", __func__);
		return;
	}

	/* symbol_rate_hw = dvbs_rd_byte(0x9fc) << 24;
	 * symbol_rate_hw |= dvbs_rd_byte(0x9fd) << 16;
	 * symbol_rate_hw |= dvbs_rd_byte(0x9fe) << 8;
	 * symbol_rate_hw |= dvbs_rd_byte(0x9ff);
	 * PR_INFO("hw sr = 0x%x\n", symbol_rate_hw);
	 */

	/* map blind scan process */
	freq_one_percent = (freq_max - freq_min) / 100;
	PR_DVBS("freq_one_percent : %d\n", freq_one_percent);
	timer_set_max(D_TIMER_DETECT, devp->timeout_dvbs_ms);
	fe->ops.info.type = FE_QPSK;

	if (fe->ops.tuner_ops.set_config)
		fe->ops.tuner_ops.set_config(fe, NULL);

	for (freq = freq_min; freq <= freq_max;) {
		/* no need sr step, just try by hw tuner and demod set to 45M */

		if (devp->blind_scan_stop)
			break;

		fe->dtv_property_cache.frequency = freq;
		fe->dtv_property_cache.bandwidth_hz = srate;
		fe->dtv_property_cache.symbol_rate = srate;
		dtvdemod_dvbs_set_frontend(fe);
		timer_begain(D_TIMER_DETECT);

		do {
			dtvdemod_dvbs_read_status(fe, &status);
			usleep_range(100000, 100001);
		} while (!(unsigned int)status);

		if (status == FE_TIMEDOUT) {/* unlock */
			fe->dtv_property_cache.frequency =
				(freq - freq_min) / freq_one_percent;
			status = BLINDSCAN_UPDATEPROCESS | FE_HAS_LOCK;
			PR_DVBS("unlock:blind process %d\n",
				fe->dtv_property_cache.frequency);
		} else {/* lock */
			//fe->dtv_property_cache.frequency = freq;
			fe->dtv_property_cache.symbol_rate = srate;
			fe->dtv_property_cache.delivery_system = devp->last_delsys;
			status = BLINDSCAN_UPDATERESULTFREQ | FE_HAS_LOCK;
			PR_DVBS("lock:freq %d,srate %d\n", freq, srate);
		}
		freq += freq_step;
		if (freq > freq_max &&
			status == (BLINDSCAN_UPDATEPROCESS | FE_HAS_LOCK))
			fe->dtv_property_cache.frequency = 100;
		dvb_frontend_add_event(fe, status);
	}

	if (status == (BLINDSCAN_UPDATERESULTFREQ | FE_HAS_LOCK)) {
	/* force process to 100% in case the lock freq is the last one */
		usleep_range(500000, 600000);
		fe->dtv_property_cache.frequency = 100;
		status = BLINDSCAN_UPDATEPROCESS | FE_HAS_LOCK;
		PR_DVBS("%s:force 100%% to upper layer\n", __func__);
		dvb_frontend_add_event(fe, status);
	}
}

/* platform driver*/
static int aml_dtvdemod_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct of_device_id *match;
	struct amldtvdemod_device_s *devp;

	PR_INFO("%s\n", __func__);
	/*memory*/
	dtvdd_devp = kzalloc(sizeof(*dtvdd_devp), GFP_KERNEL);
	devp = dtvdd_devp;

	if (!devp)
		goto fail_alloc_region;

	devp->state = DTVDEMOD_ST_NOT_INI;


	/*class attr */
	devp->clsp = class_create(THIS_MODULE, DEMOD_DEVICE_NAME);
	if (!devp->clsp)
		goto fail_create_class;

	ret = dtvdemod_create_class_files(devp->clsp);
	if (ret)
		goto fail_class_create_file;

	match = of_match_device(meson_ddemod_match, &pdev->dev);
	if (match == NULL) {
		PR_ERR("%s,no matched table\n", __func__);
		goto fail_ic_config;
	}
	devp->data = (struct meson_ddemod_data *)match->data;

	/*reg*/
	ret = dds_init_reg_map(pdev);
	if (ret)
		goto fail_ic_config;

	ret = dtvdemod_set_iccfg_by_dts(pdev);
	if (ret)
		goto fail_ic_config;

	/*debug:*/
	dbg_ic_cfg();
	dbg_reg_addr();

	/**/
	dtvpll_lock_init();
	demod_init_mutex();
	mutex_init(&devp->lock);
	mutex_init(&devp->mutex_tx_msg);
	devp->dev = &pdev->dev;
	dtvdemod_clktree_probe(&pdev->dev);


	devp->state = DTVDEMOD_ST_IDLE;
	dtvdemod_version(devp);
	devp->flg_cma_allc = false;
	devp->act_dtmb = false;
	devp->timeout_atsc_ms = TIMEOUT_ATSC;
	devp->timeout_dvbt_ms = TIMEOUT_DVBT;
	devp->timeout_dvbs_ms = TIMEOUT_DVBS;
	devp->demod_thread = 1;
	devp->blind_scan_stop = 1;
	devp->fast_search_finish = 1;
	//ary temp:
	aml_demod_init();

	if (devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		pm_runtime_enable(devp->dev);
		if (pm_runtime_get_sync(devp->dev) < 0)
			pr_err("failed to set pwr\n");

		devp->fw_buf = kzalloc(FW_BUFF_SIZE, GFP_KERNEL);
		if (!devp->fw_buf) {
			pr_err("%s alloc fw buf fail\n", __func__);
			ret = -ENOMEM;
		}

		/* delayed workqueue for dvbt2 fw downloading */
		INIT_DELAYED_WORK(&devp->fw_dwork, dtvdemod_fw_dwork);
		schedule_delayed_work(&devp->fw_dwork, 10 * HZ);

		/* workqueue for dvbs blind scan process */
		INIT_WORK(&devp->blind_scan_work, dvbs_blind_scan_work);
	}

	PR_INFO("[amldtvdemod.] : version: %s, probe ok.\n", AMLDTVDEMOD_VER);
	return 0;
fail_ic_config:
	PR_ERR("ic config error.\n");
fail_class_create_file:
	PR_ERR("dtvdemod class file create error.\n");
	class_destroy(devp->clsp);
fail_create_class:
	PR_ERR("dtvdemod class create error.\n");
	kfree(devp);
fail_alloc_region:
	PR_ERR("dtvdemod alloc error.\n");
	PR_ERR("dtvdemod_init fail.\n");

	return ret;
}

static int __exit aml_dtvdemod_remove(struct platform_device *pdev)
{
	if (dtvdd_devp == NULL)
		return -1;

	dtvdemod_clktree_remove(&pdev->dev);
	mutex_destroy(&dtvdd_devp->lock);
	dtvdemod_remove_class_files(dtvdd_devp->clsp);
	class_destroy(dtvdd_devp->clsp);

	if (dtvdd_devp->data->hw_ver >= DTVDEMOD_HW_T5D) {
		kfree(dtvdd_devp->fw_buf);
		pm_runtime_put_sync(dtvdd_devp->dev);
		pm_runtime_disable(dtvdd_devp->dev);
	}

	kfree(dtvdd_devp);
	PR_INFO("%s:remove.\n", __func__);
	aml_demod_exit();

	return 0;
}

static void aml_dtvdemod_shutdown(struct platform_device *pdev)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;

	pr_info("%s delsys = %d OK.\n", __func__, delsys);

	if (dtvdd_devp->state != DTVDEMOD_ST_IDLE) {
		if (delsys != SYS_UNDEFINED) {
			leave_mode(delsys);
			if (dtvdd_devp->frontend.ops.tuner_ops.release)
				dtvdd_devp->frontend.ops.tuner_ops.release(&dtvdd_devp->frontend);
		}
		dtvdd_devp->state = DTVDEMOD_ST_IDLE;
	}
}

static int aml_dtvdemod_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	int ret = 0;

	ret = dtvdemod_leave_mode();

	PR_INFO("%s state event %d, ret %d, OK\n", __func__, state.event, ret);

	return 0;
}

static int aml_dtvdemod_resume(struct platform_device *pdev)
{
	PR_INFO("%s is called\n", __func__);
	return 0;
}

static int dtvdemod_leave_mode(void)
{
	enum fe_delivery_system delsys = SYS_UNDEFINED;

	if (IS_ERR_OR_NULL(dtvdd_devp))
		return -EFAULT;

	delsys = dtvdd_devp->last_delsys;

	PR_INFO("%s, delsys = %s\n", __func__, name_fe_delivery_system[delsys]);

	if (delsys != SYS_UNDEFINED) {
		leave_mode(delsys);
		if (dtvdd_devp->frontend.ops.tuner_ops.release)
			dtvdd_devp->frontend.ops.tuner_ops.release(&dtvdd_devp->frontend);
	}

	return 0;
}

static __maybe_unused int dtv_demod_pm_suspend(struct device *dev)
{
	int ret = 0;

	ret = dtvdemod_leave_mode();

	PR_INFO("%s ret %d, OK.\n", __func__, ret);

	return 0;
}

static __maybe_unused int dtv_demod_pm_resume(struct device *dev)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("%s, devp is NULL\n", __func__);
		return -1;
	}

	/* download fw again after STR in case sram was power down */
	devp->fw_wr_done = 0;
	PR_INFO("%s OK.\n", __func__);

	return 0;
}

static int __maybe_unused dtv_demod_runtime_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused dtv_demod_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops dtv_demod_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dtv_demod_pm_suspend, dtv_demod_pm_resume)
	SET_RUNTIME_PM_OPS(dtv_demod_runtime_suspend,
			   dtv_demod_runtime_resume, NULL)
};

static struct platform_driver aml_dtvdemod_driver = {
	.driver = {
		.name = "aml_dtv_demod",
		.owner = THIS_MODULE,
		.pm = &dtv_demod_pm_ops,
		/*aml_dtvdemod_dt_match*/
		.of_match_table = meson_ddemod_match,
	},
	.shutdown   = aml_dtvdemod_shutdown,
	.probe = aml_dtvdemod_probe,
	.remove = __exit_p(aml_dtvdemod_remove),
	.suspend  = aml_dtvdemod_suspend,
	.resume   = aml_dtvdemod_resume,
};


static int __init aml_dtvdemod_init(void)
{
	if (platform_driver_register(&aml_dtvdemod_driver)) {
		pr_err("failed to register amldtvdemod driver module\n");
		return -ENODEV;
	}

	PR_INFO("[amldtvdemod..]%s.\n", __func__);
	return 0;
}

static void __exit aml_dtvdemod_exit(void)
{
	platform_driver_unregister(&aml_dtvdemod_driver);
	PR_INFO("[amldtvdemod..]%s: driver removed ok.\n", __func__);
}

static int delsys_set(struct dvb_frontend *fe, unsigned int delsys)
{
	struct amldtvdemod_device_s *devp = dtvdd_devp;
	enum fe_delivery_system ldelsys = devp->last_delsys;
	enum fe_delivery_system cdelsys = delsys;

	int ncaps, support;

	if (ldelsys == cdelsys)
		return 0;

	ncaps = 0;
	support = 0;
	while (ncaps < MAX_DELSYS && fe->ops.delsys[ncaps]) {
		if (fe->ops.delsys[ncaps] == cdelsys) {

			support = 1;
			break;
		}
		ncaps++;
	}

	if (!support) {
		PR_INFO("delsys:%d is not support!\n", cdelsys);
		return 0;
	}

	if (ldelsys <= END_SYS_DELIVERY && cdelsys <= END_SYS_DELIVERY) {
		PR_DBG("%s:l=%s,c=%s\n", __func__,
			name_fe_delivery_system[ldelsys],
			name_fe_delivery_system[cdelsys]);
	} else
		PR_ERR("%s:last=%d,cur=%d\n", __func__, ldelsys, cdelsys);

	switch (cdelsys) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		/* dvbc */
		fe->ops.info.type = FE_QAM;
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		/* atsc */
		fe->ops.info.type = FE_ATSC;
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		/* dvbt, OFDM */
		fe->ops.info.type = FE_OFDM;
		break;

	case SYS_ISDBT:
		fe->ops.info.type = FE_ISDBT;
		break;

	case SYS_DTMB:
		/* dtmb */
		fe->ops.info.type = FE_DTMB;
		break;

	case SYS_DVBS:
	case SYS_DVBS2:
		/* QPSK */
		fe->ops.info.type = FE_QPSK;
		break;

	case SYS_DSS:
	case SYS_DVBH:
	case SYS_ISDBS:
	case SYS_ISDBC:
	case SYS_CMMB:
	case SYS_DAB:
	case SYS_TURBO:
	case SYS_UNDEFINED:
		return 0;

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	case SYS_ANALOG:
		if (get_dtvpll_init_flag()) {
			PR_INFO("delsys not support : %d\n", cdelsys);
			leave_mode(ldelsys);

			if (fe->ops.tuner_ops.release)
				fe->ops.tuner_ops.release(fe);
		}

		return 0;
#endif
	}

	if (cdelsys != SYS_UNDEFINED) {
		if (ldelsys != SYS_UNDEFINED) {
			leave_mode(ldelsys);

			if (fe->ops.tuner_ops.release)
				fe->ops.tuner_ops.release(fe);
		}

		if (enter_mode(cdelsys)) {
			PR_INFO("enter_mode failed,leave!\n");
			leave_mode(cdelsys);

			if (fe->ops.tuner_ops.release)
				fe->ops.tuner_ops.release(fe);
			return 0;
		}
	}

	if (!get_dtvpll_init_flag()) {
		PR_INFO("pll is not set!\n");
		leave_mode(cdelsys);

		if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
		return 0;
	}

	dtvdd_devp->last_delsys = cdelsys;
	PR_INFO("info fe type:%d.\n", fe->ops.info.type);

	if (fe->ops.tuner_ops.set_config)
		fe->ops.tuner_ops.set_config(fe, NULL);

	return 0;
}

static int is_not_active(struct dvb_frontend *fe)
{
	enum fe_delivery_system cdelsys;
	enum fe_delivery_system ldelsys = dtvdd_devp->last_delsys;

	if (!get_dtvpll_init_flag())
		return 1;

	cdelsys = fe->dtv_property_cache.delivery_system;
	if (ldelsys != cdelsys)
		return 2;


	return 0;/*active*/
}
/*ko attach==============================*/
static int aml_dtvdm_init(struct dvb_frontend *fe)
{
	dtvdd_devp->last_delsys = SYS_UNDEFINED;

	PR_INFO("%s OK.\n", __func__);

	return 0;
}
static int aml_dtvdm_sleep(struct dvb_frontend *fe)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;

	if (get_dtvpll_init_flag()) {
		PR_INFO("%s\n", __func__);
		if (delsys != SYS_UNDEFINED)
			leave_mode(delsys);

		if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
	}

	PR_INFO("%s OK.\n", __func__);

	return 0;
}
static int aml_dtvdm_set_parameters(struct dvb_frontend *fe)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;
	int ret = 0;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	PR_DBGL("%s delsys %d.\n", __func__, delsys);
	PR_DBGL("delivery_sys=%d freq=%d,symbol_rate=%d,bw=%d,modulation=%d,inversion=%d.\n",
		c->delivery_system, c->frequency, c->symbol_rate, c->bandwidth_hz, c->modulation,
		c->inversion);

	if (is_not_active(fe)) {
		PR_DBG("set parm:not active\n");
		return 0;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		PR_INFO("FE_QAM\n");
		timer_begain(D_TIMER_DETECT);
		dtvdd_devp->en_detect = 1; /*fist set*/
		ret = gxtv_demod_dvbc_set_frontend(fe);
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		break;

	case SYS_ISDBT:
		PR_INFO("FE_ISDBT\n");

		timer_begain(D_TIMER_DETECT);
		dtvdd_devp->en_detect = 1; /*fist set*/
		ret = dvbt_isdbt_set_frontend(fe);
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		PR_INFO("FE_ATSC\n");
		ret = gxtv_demod_atsc_set_frontend(fe);
		break;

	case SYS_DTMB:
		PR_INFO("FE_DTMB\n");
		ret = gxtv_demod_dtmb_set_frontend(fe);
		break;

	case SYS_UNDEFINED:
	default:
		break;
	}

	return ret;
}

static int aml_dtvdm_get_frontend(struct dvb_frontend *fe,
				 struct dtv_frontend_properties *p)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("get parm:not active\n");
		return 0;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		PR_DVBS("freq %d,srate %d\n", fe->dtv_property_cache.frequency,
			fe->dtv_property_cache.symbol_rate);
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_get_frontend(fe);
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		ret = gxtv_demod_dvbt_get_frontend(fe);
		break;

	case SYS_ISDBT:
		ret = gxtv_demod_dvbt_get_frontend(fe);
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_get_frontend(fe);
		break;

	case SYS_DTMB:
		ret = gxtv_demod_dtmb_get_frontend(fe);
		break;

	case SYS_UNDEFINED:
	default:
		break;
	}

	return ret;
}
static int aml_dtvdm_get_tune_settings(struct dvb_frontend *fe,
				      struct dvb_frontend_tune_settings
					*fe_tune_settings)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("get parm:not active\n");
		return 0;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		fe_tune_settings->min_delay_ms = 300;
		fe_tune_settings->step_size = 0; /* no zigzag */
		fe_tune_settings->max_drift = 0;
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		fe_tune_settings->min_delay_ms = 500;
		fe_tune_settings->step_size = 0;
		fe_tune_settings->max_drift = 0;
		break;

	case SYS_ISDBT:
		fe_tune_settings->min_delay_ms = 300;
		fe_tune_settings->step_size = 0;
		fe_tune_settings->max_drift = 0;
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		break;

	case SYS_DTMB:
		break;

	case SYS_UNDEFINED:
	default:
		break;
	}

	return ret;
}
static int aml_dtvdm_read_status(struct dvb_frontend *fe,
					enum fe_status *status)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	enum fe_delivery_system delsys = devp->last_delsys;

	int ret = 0;

	if (delsys == SYS_UNDEFINED)
		return 0;

	if (!unlikely(devp))
		PR_ERR("%s, devp is NULL\n", __func__);

	if (is_not_active(fe)) {
		PR_DBGL("read status:not active\n");
		return 0;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = dtvdemod_dvbs_read_status(fe, status);
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_read_status_timer(fe, status);
		break;

	case SYS_DVBT:
		*status = devp->last_status;
		break;

	case SYS_DVBT2:
		*status = devp->last_status;
		break;

	case SYS_ISDBT:
		ret = dvbt_isdbt_read_status(fe, status);
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		*status = devp->last_status;
		break;

	case SYS_DTMB:
		ret = gxtv_demod_dtmb_read_status(fe, status);
		break;

	default:
		break;
	}

	return ret;

}
static int aml_dtvdm_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("read ber:not active\n");
		return 0;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_read_ber(fe, ber);
		break;

	case SYS_DVBT:
		ret = dvbt_read_ber(fe, ber);
		break;

	case SYS_DVBT2:
		break;

	case SYS_ISDBT:
		ret = dvbt_isdbt_read_ber(fe, ber);
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_read_ber(fe, ber);
		break;

	case SYS_DTMB:
		ret = gxtv_demod_dtmb_read_ber(fe, ber);
		break;

	default:
		break;
	}
	return ret;

}

static int aml_dtvdm_read_signal_strength(struct dvb_frontend *fe,
					 u16 *strength)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("read strength:not active\n");
		return 0;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = dtvdemod_dvbs_read_signal_strength(fe, strength);
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_read_signal_strength(fe, strength);
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		ret = gxtv_demod_dvbt_read_signal_strength(fe, strength);
		break;

	case SYS_ISDBT:
		ret = gxtv_demod_dvbt_read_signal_strength(fe, strength);
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_read_signal_strength(fe, strength);
		break;

	case SYS_DTMB:
		ret = gxtv_demod_dtmb_read_signal_strength(fe, strength);
		break;

	default:
		break;
	}
	return ret;

}
static int aml_dtvdm_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("read snr :not active\n");
		return 0;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = dvbs_read_snr(fe, snr);
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_read_snr(fe, snr);
		break;

	case SYS_DVBT:
		ret = dvbt_read_snr(fe, snr);
		break;

	case SYS_DVBT2:
		ret = dvbt2_read_snr(fe, snr);
		break;

	case SYS_ISDBT:
		ret = gxtv_demod_dvbt_isdbt_read_snr(fe, snr);
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_read_snr(fe, snr);
		break;

	case SYS_DTMB:
		ret = gxtv_demod_dtmb_read_snr(fe, snr);
		break;

	default:
		break;
	}

	return ret;

}

static int aml_dtvdm_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("read ucblocks :not active\n");
		return 0;
	}

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		ret = gxtv_demod_dvbc_read_ucblocks(fe, ucblocks);
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		ret = gxtv_demod_dvbt_read_ucblocks(fe, ucblocks);
		break;

	case SYS_ISDBT:
		ret = gxtv_demod_dvbt_read_ucblocks(fe, ucblocks);
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_read_ucblocks(fe, ucblocks);
		break;

	case SYS_DTMB:
		ret = gxtv_demod_dtmb_read_ucblocks(fe, ucblocks);
		break;

	default:
		break;
	}

	return ret;

}
static void aml_dtvdm_release(struct dvb_frontend *fe)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		gxtv_demod_dvbc_release(fe);
		break;

	case SYS_DVBT:
	case SYS_DVBT2:
		break;

	case SYS_ISDBT:
		gxtv_demod_dvbt_release(fe);
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		gxtv_demod_atsc_release(fe);
		break;

	case SYS_DTMB:
		gxtv_demod_dtmb_release(fe);
		break;

	default:
		break;
	}

	if (get_dtvpll_init_flag()) {
		PR_INFO("%s\n", __func__);
		if (delsys != SYS_UNDEFINED)
			leave_mode(delsys);

		if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
	}

	PR_INFO("%s OK.\n", __func__);
}


static int aml_dtvdm_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	enum fe_delivery_system delsys = dtvdd_devp->last_delsys;
	int ret = 0;
	static int flg;	/*debug only*/

	if (delsys == SYS_UNDEFINED) {
		*delay = HZ * 5;
		*status = 0;
		return 0;
	}

	if (is_not_active(fe)) {
		*status = 0;
		PR_INFO("tune :not active\n");
		return 0;
	}

	if ((flg > 0) && (flg < 5))
		PR_INFO("%s\n", __func__);

	switch (delsys) {
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = dtvdemod_dvbs_tune(fe, re_tune, mode_flags, delay, status);
		break;

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		gxtv_demod_dvbc_tune(fe, re_tune, mode_flags, delay, status);
		break;

	case SYS_DVBT:
		ret = dvbt_tune(fe, re_tune, mode_flags, delay, status);
		break;

	case SYS_DVBT2:
		ret = dvbt2_tune(fe, re_tune, mode_flags, delay, status);
		break;

	case SYS_ISDBT:
		ret = dvbt_isdbt_tune(fe, re_tune, mode_flags, delay, status);
		break;

	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		ret = gxtv_demod_atsc_tune(fe, re_tune, mode_flags, delay, status);
		flg++;
		break;

	case SYS_DTMB:
		ret = gxtv_demod_dtmb_tune(fe, re_tune, mode_flags, delay, status);
		break;

	default:
		flg = 0;
		break;
	}

	return ret;

}

static int aml_dtvdm_set_property(struct dvb_frontend *dev,	struct dtv_property *tvp)
{
	int r = 0;
	u32 delsys;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	if (unlikely(!devp)) {
		PR_ERR("aml_dtvdm_set_property, devp is NULL\n");
		return -1;
	}

	switch (tvp->cmd) {
	case DTV_DELIVERY_SYSTEM:
		delsys = tvp->u.data;
		delsys_set(dev, delsys);
		break;

	case DTV_DVBT2_PLP_ID:
		devp->plp_id = tvp->u.data;
		PR_INFO("DTV_DVBT2_PLP_ID, %d\n", devp->plp_id);
		break;

	case DTV_BLIND_SCAN_MIN_FRE:
		devp->blind_min_fre = tvp->u.data;
		PR_DVBS("DTV_BLIND_SCAN_MIN_FRE: %d\n", devp->blind_min_fre);
		break;

	case DTV_BLIND_SCAN_MAX_FRE:
		devp->blind_max_fre = tvp->u.data;
		PR_DVBS("DTV_BLIND_SCAN_MAX_FRE: %d\n", devp->blind_max_fre);
		break;

	case DTV_BLIND_SCAN_MIN_SRATE:
		devp->blind_min_srate = tvp->u.data;
		PR_DVBS("DTV_BLIND_SCAN_MIN_SRATE: %d\n", devp->blind_min_srate);
		break;

	case DTV_BLIND_SCAN_MAX_SRATE:
		devp->blind_max_srate = tvp->u.data;
		PR_DVBS("DTV_BLIND_SCAN_MAX_SRATE: %d\n", devp->blind_max_srate);
		break;

	case DTV_BLIND_SCAN_FRE_RANGE:
		devp->blind_fre_range = tvp->u.data;
		PR_DVBS("DTV_BLIND_SCAN_FRE_RANGE: %d\n", devp->blind_fre_range);
		break;

	case DTV_BLIND_SCAN_FRE_STEP:
		devp->blind_fre_step = tvp->u.data;

		if (!devp->blind_fre_step)
			devp->blind_fre_step = 2000;/* 2M */

		PR_DVBS("DTV_BLIND_SCAN_FRE_STEP: %d\n", devp->blind_fre_step);
		break;

	case DTV_BLIND_SCAN_TIMEOUT:
		devp->blind_timeout = tvp->u.data;
		PR_DVBS("DTV_BLIND_SCAN_TIMEOUT: %d\n", devp->blind_timeout);
		break;

	case DTV_START_BLIND_SCAN:
		PR_DVBS("DTV_START_BLIND_SCAN\n");
		devp->blind_scan_stop = 0;
		schedule_work(&devp->blind_scan_work);
		PR_INFO("schedule workqueue for blind scan, return\n");
		break;

	case DTV_CANCEL_BLIND_SCAN:
		devp->blind_scan_stop = 1;
		PR_DVBS("DTV_CANCEL_BLIND_SCAN\n");
		break;

	default:
		break;
	}

	return r;
}

static int aml_dtvdm_get_property(struct dvb_frontend *dev,
			struct dtv_property *tvp)
{
	char v;
	unsigned char modulation = 0x00;
	struct amldtvdemod_device_s *devp = dtvdd_devp;

	switch (tvp->cmd) {
	case DTV_DELIVERY_SYSTEM:
		tvp->u.data = devp->last_delsys;
		if (devp->last_delsys == SYS_DVBS || devp->last_delsys == SYS_DVBS2) {
			v = dvbs_rd_byte(0x932) & 0x60;//bit5.6
			modulation = dvbs_rd_byte(0x930) >> 2;
			if (v == 0x40) {//bit6=1.bit5=0 means S2
				tvp->u.data = SYS_DVBS2;
				if ((modulation >= 0x0c && modulation <= 0x11) ||
					(modulation >= 0x47 && modulation <= 0x49))
					tvp->reserved[0] = PSK_8;
				else if ((modulation >= 0x12 && modulation <= 0x17) ||
					(modulation >= 0x4a && modulation <= 0x56))
					tvp->reserved[0] = APSK_16;
				else if ((modulation >= 0x18 && modulation <= 0x1c) ||
					(modulation >= 0x57 && modulation <= 0x59))
					tvp->reserved[0] = APSK_32;
				else
					tvp->reserved[0] = QPSK;
			} else if (v == 0x60) {//bit6=1.bit5=1 means S
				tvp->u.data = SYS_DVBS;
				tvp->reserved[0] = QPSK;
			}
		}
		PR_INFO("get delivery system : %d,modulation:0x%x\n",
			tvp->u.data, tvp->reserved[0]);
		break;

	case DTV_DVBT2_PLP_ID:
		if (!devp->demod_thread)
			break;

		/* plp nums & ids */
		dtvdemod_get_plp(devp, tvp);
		break;

	default:
		break;
	}

	return 0;
}

static struct dvb_frontend_ops aml_dtvdm_ops = {
	.delsys = {SYS_UNDEFINED},
	.info = {
		/*in aml_fe, it is 'amlogic dvb frontend' */
		.name = "",
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS
	},
	.init = aml_dtvdm_init,
	.sleep = aml_dtvdm_sleep,
	.set_frontend = aml_dtvdm_set_parameters,
	.get_frontend = aml_dtvdm_get_frontend,
	.get_tune_settings = aml_dtvdm_get_tune_settings,
	.read_status = aml_dtvdm_read_status,
	.read_ber = aml_dtvdm_read_ber,
	.read_signal_strength = aml_dtvdm_read_signal_strength,
	.read_snr = aml_dtvdm_read_snr,
	.read_ucblocks = aml_dtvdm_read_ucblocks,
	.release = aml_dtvdm_release,
	.set_property = aml_dtvdm_set_property,
	.get_property = aml_dtvdm_get_property,
	.tune = aml_dtvdm_tune,
	.get_frontend_algo = gxtv_demod_get_frontend_algo,
};

struct dvb_frontend *aml_dtvdm_attach(const struct demod_config *config)
{
	int ic_version = get_cpu_type();
	unsigned int ic_is_supportted = 1;
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();
	struct dvb_frontend *fe = NULL;

	if (IS_ERR_OR_NULL(devp)) {
		pr_err("%s devp is NULL\n", __func__);
		return fe;
	}

	fe = &devp->frontend;

	switch (ic_version) {
	case MESON_CPU_MAJOR_ID_GXTVBB:
	case MESON_CPU_MAJOR_ID_TXL:
		aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
		aml_dtvdm_ops.delsys[1] = SYS_DTMB;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
		aml_dtvdm_ops.delsys[2] = SYS_ANALOG;
#endif
		if (ic_version == MESON_CPU_MAJOR_ID_GXTVBB)
			strcpy(aml_dtvdm_ops.info.name, "amlogic dtv demod gxtvbb");
		else
			strcpy(aml_dtvdm_ops.info.name, "amlogic dtv demod txl");
		break;

	case MESON_CPU_MAJOR_ID_TXLX:
		aml_dtvdm_ops.delsys[0] = SYS_ATSC;
		aml_dtvdm_ops.delsys[1] = SYS_DVBC_ANNEX_B;
		aml_dtvdm_ops.delsys[2] = SYS_DVBC_ANNEX_A;
		aml_dtvdm_ops.delsys[3] = SYS_DVBT;
		aml_dtvdm_ops.delsys[4] = SYS_ISDBT;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
		aml_dtvdm_ops.delsys[5] = SYS_ANALOG;
#endif
		strcpy(aml_dtvdm_ops.info.name, "amlogic dtv demod txlx");
		break;

	case MESON_CPU_MAJOR_ID_GXLX:
		aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
		strcpy(aml_dtvdm_ops.info.name, "amlogic dtv demod gxlx");
		break;

	case MESON_CPU_MAJOR_ID_TXHD:
		aml_dtvdm_ops.delsys[0] = SYS_DTMB;
		strcpy(aml_dtvdm_ops.info.name, "amlogic dtv demod txhd");
		break;

	case MESON_CPU_MAJOR_ID_TL1:
	case MESON_CPU_MAJOR_ID_TM2:
		aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
		aml_dtvdm_ops.delsys[1] = SYS_DVBC_ANNEX_B;
		aml_dtvdm_ops.delsys[2] = SYS_ATSC;
		aml_dtvdm_ops.delsys[3] = SYS_DTMB;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
		aml_dtvdm_ops.delsys[4] = SYS_ANALOG;
#endif
		if (ic_version == MESON_CPU_MAJOR_ID_TL1)
			strcpy(aml_dtvdm_ops.info.name, "amlogic dtv demod tl1");
		else
			strcpy(aml_dtvdm_ops.info.name, "amlogic dtv demod tm2");
		break;

	case MESON_CPU_MAJOR_ID_T5:
		/* max delsys is 8, index: 0~7 */
		aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
		aml_dtvdm_ops.delsys[1] = SYS_DTMB;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
		aml_dtvdm_ops.delsys[2] = SYS_ANALOG;
#endif
		strcpy(aml_dtvdm_ops.info.name, "amlogic dtv demod t5");
		break;

	default:
		switch (devp->data->hw_ver) {
		case DTVDEMOD_HW_T5D:
		case DTVDEMOD_HW_T5D_B:
			/* max delsys is 8, index: 0~7 */
			aml_dtvdm_ops.delsys[0] = SYS_DVBC_ANNEX_A;
			aml_dtvdm_ops.delsys[1] = SYS_ATSC;
			aml_dtvdm_ops.delsys[2] = SYS_DVBS2;
			aml_dtvdm_ops.delsys[3] = SYS_ISDBT;
			aml_dtvdm_ops.delsys[4] = SYS_DVBS;
			aml_dtvdm_ops.delsys[5] = SYS_DVBT2;
			aml_dtvdm_ops.delsys[6] = SYS_DVBT;
			aml_dtvdm_ops.delsys[7] = SYS_DVBC_ANNEX_B;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
			aml_dtvdm_ops.delsys[8] = SYS_ANALOG;
#endif
			strcpy(aml_dtvdm_ops.info.name, "amlogic dtv demod t5d");
			break;
		default:
			ic_is_supportted = 0;
			PR_ERR("attach fail! ic=%d\n", ic_version);
			fe = NULL;
			break;
		}
		break;
	}

	if (ic_is_supportted && !IS_ERR_OR_NULL(fe))
		memcpy(&fe->ops, &aml_dtvdm_ops, sizeof(struct dvb_frontend_ops));

	dtvdd_devp->last_delsys = SYS_UNDEFINED;

	/*diseqc attach*/
	if (!IS_ERR_OR_NULL(dtvdd_devp->diseqc_name) && !IS_ERR_OR_NULL(fe) &&
	    !IS_ERR_OR_NULL(devp))
		aml_diseqc_attach(devp, fe);

	return fe;
}
EXPORT_SYMBOL(aml_dtvdm_attach);

static struct aml_exp_func aml_exp_ops = {
	.leave_mode = leave_mode,
};

struct aml_exp_func *aml_dtvdm_exp_attach(struct aml_exp_func *exp)
{
	if (exp) {
		memcpy(exp, &aml_exp_ops, sizeof(struct aml_exp_func));
	} else {
		PR_ERR("%s:fail!\n", __func__);
		return NULL;

	}
	return exp;
}
EXPORT_SYMBOL(aml_dtvdm_exp_attach);

struct dvb_frontend *aml_get_fe(void)
{
	return &dtvdd_devp->frontend;
}

void aml_exp_attach(struct aml_exp_func *afe)
{

}
EXPORT_SYMBOL(aml_exp_attach);

fs_initcall(aml_dtvdemod_init);
module_exit(aml_dtvdemod_exit);
MODULE_VERSION(AMLDTVDEMOD_VER);
MODULE_DESCRIPTION("gxtv_demod DVB-T/DVB-C/DTMB Demodulator driver");
MODULE_AUTHOR("RSJ");
MODULE_LICENSE("GPL");
