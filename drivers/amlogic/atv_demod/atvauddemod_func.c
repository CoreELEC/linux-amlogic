/*
 * drivers/amlogic/atv_demod/atvauddemod_func.c
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

#ifndef __ATVAUDDEMOD_FUN_H
#define __ATVAUDDEMOD_FUN_H

#include <linux/types.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>

#include "atv_demod_debug.h"
#include "atvauddemod_func.h"
#include "aud_demod_settings.c"
#include "atvdemod_func.h"
#include "atv_demod_driver.h"
#include "atv_demod_access.h"

/* #define AUDIO_MOD_DET_INTERNAL */

/* ademod_debug_en for audio demod debug */
unsigned int ademod_debug_en;
/* btsc_detect_delay for btsc detect delay */
unsigned int btsc_detect_delay = 10;
unsigned int nicam_detect_delay = 10;
unsigned int a2_detect_delay = 300;
/* signal_audmode for btsc signal audio mode */
unsigned int signal_audmode;
unsigned int audio_thd_threshold1 = 0x1000;
unsigned int audio_thd_threshold2 = 0xf00;

unsigned int audio_a2_auto = 1;
unsigned int audio_a2_power_threshold = 0x1800;
unsigned int audio_a2_carrier_report = 0xc00;

static int last_nicam_lock = -1;
static int last_nicam_mono_flag = -1;
static int last_stereo_flag = -1;
static int last_dual_flag = -1;
static int last_sap_flag = -1;
static int last_mode = -1;

#undef pr_info
#define pr_info(args...)\
	do {\
		if (ademod_debug_en & 0x1)\
			printk(args);\
	} while (0)
#define pr_carr(args...)\
	do {\
		if (ademod_debug_en & 0x2)\
			printk(args);\
	} while (0)
#undef pr_dbg
#define pr_dbg(a...) \
	do {\
		if (1)\
			printk(a);\
	} while (0)

int deem_75u_30[7]   = {10, 6, 0, -971, 0, -44, 97};
int deem_75u_40[7]   = {10, 6, 0, -971, 0, -26, 80};
int deem_75u_50[7]   = {10, 6, 0, -971, 0, -16, 69};
int deem_75u[7]      = {10, 6, 0, -971, 0, 12, 41};
int deem_50u[7]      = {10, 6, 0, -945, 0, 18, 60};

int deem_j17[7] = {10, 6, 0, -1012, 0, 124, -112};

int lmr15k_0[7] = {10, 6, 778, -1739, 310, -557, 310};
int lmr15k_3[7] = {10, 6, 864, -1783, 430, -756, 430};
int lmr15k_1[7] = {10, 6, 721, -1712, 57, -95, 71};
int lmr15k_2[7] = {10, 6, 942, -1830, 7, 17, 112};
int lmr15k_4[7] = {10, 6, 1000, -1872, 19, 48, 85};
int lpr15k_0[7] = {10, 6, 778, -1739, 310, -557, 310};
int lpr15k_3[7] = {10, 6, 864, -1783, 430, -756, 430};
int lpr15k_1[7] = {10, 6, 721, -1712, 57, -95, 71};
int lpr15k_2[7] = {10, 6, 942, -1830, 7, 17, 112};
int lpr15k_4[7] = {10, 6, 1000, -1872, 19, 48, 85};

int lmr15k_4_4[7] = {10, 6, 885, -1325, 617, -651, 617};
int lmr15k_1_4[7] = {10, 6, 638, -1102, 508, -455, 508};
int lmr15k_0_4[7] = {10, 6, 405, -853, 382, -188, 382};
int lmr15k_2_4[7] = {10, 6, 196, -609, 257, 97, 257};
int lmr15k_3_4[7] = {10, 6, 62, -445, 173, 294, 173};
int lpr15k_4_4[7] = {10, 6, 885, -1325, 617, -651, 617};
int lpr15k_1_4[7] = {10, 6, 638, -1102, 508, -455, 508};
int lpr15k_0_4[7] = {10, 6, 405, -853, 382, -188, 382};
int lpr15k_2_4[7] = {10, 6, 196, -609, 257, 97, 257};
int lpr15k_3_4[7] = {10, 6, 62, -445, 173, 294, 173};

int lmr15k_btsc_0[7] = {10, 6, 997, -1865, 1011, -1865, 1011};
int lmr15k_btsc_1[7] = {10, 6, 794, -1673, 698, -1673, 1121};
int lmr15k_btsc_2[7] = {10, 6, 836, -1773, 137, -188, 137};
int lmr15k_btsc_3[7] = {10, 6, 736, -1724, 16, 4, 16};
int lmr15k_btsc_4[7] = {10, 6, 961, -1851, 342, -550, 342};
int lpr15k_btsc_0[7] = {10, 6, 997, -1865, 1011, -1865, 1011};
int lpr15k_btsc_1[7] = {10, 6, 794, -1673, 698, -1673, 1121};
int lpr15k_btsc_2[7] = {10, 6, 836, -1773, 137, -188, 137};
int lpr15k_btsc_3[7] = {10, 6, 736, -1724, 16, 4, 16};
int lpr15k_btsc_4[7] = {10, 6, 961, -1851, 342, -550, 342};

int btsc_pilot[7] = {10, 6, 1014, -1881, -5, 0, 5};

int fix_deem0[7] = {10, 6, 0, -1014, 0, -74, 85};
int fix_deem1[7] = {10, 6, 0, -969, 0, -7, 62};
int fix_deem2[7] = {9, 5, 0, 0, 0, 0, 512};

int qfilter0[7] =  {10, 6, 838, -1828, 788, -1576, 788};
int qfilter1[7] =  {10, 6, 459, -1434,  -330, 0, 330};
int qfilter2[7] =  {9, 5, 0, 0, 0, 0,  512};
int pfilter0[7] =  {10, 6, 0, -972, 0, -920, 920};
int pfilter1[7] =  {10, 6, 0, -1023, 0, 29, 29};

static int thd_flag;
static uint32_t thd_tmp_v;
static uint8_t thd_cnt;


void set_filter(int *filter, int start_addr, int taps)
{
	int i = 0;
	int filter_len = (taps+1)>>1;
	int tmp = 0;
	int tmp_addr = 0;

	tmp_addr = start_addr;
	for (i = 0; i < filter_len; i++) {
		if ((i&1) == 1) {
			tmp = ((tmp&0xffff))|((filter[i]&0xffff)<<16);
			adec_wr_reg(tmp_addr, tmp);
			/* pr_info("[reg0x%x] = 0x%x\n",tmp_addr,tmp); */
			tmp_addr += 1;
		} else {
			tmp = filter[i];
			if (i == filter_len-1) {
				adec_wr_reg(tmp_addr, tmp);
				/*pr_info("[reg0x%x] = 0x%x\n",tmp_addr,tmp);*/
			}
		}
	}
}

void set_iir(int *filter, int start_addr)
{
	unsigned int reg32;

	reg32 = (filter[6]&0xffff) | ((filter[5]&0xffff)<<16);
	adec_wr_reg(start_addr, reg32);

	reg32 = (filter[4]&0xffff) | ((filter[3]&0xffff)<<16);
	adec_wr_reg(start_addr+1, reg32);

	reg32 = (filter[2]&0xffff) | ((filter[1]&0xff)<<16)
		|((filter[0]&0xff)<<24);
	adec_wr_reg(start_addr+2, reg32);
}

void bypass_iir(int start_addr)
{
	adec_wr_reg(start_addr, 512);
	adec_wr_reg(start_addr+1, 0);
	adec_wr_reg(start_addr+2, 0x09050000);
}

void bypass_15k_iir(void)
{
	pr_info("Bypass 15k LPF\n");
	bypass_iir(ADDR_IIR_LPR_15K_0);
	bypass_iir(ADDR_IIR_LPR_15K_1);
	bypass_iir(ADDR_IIR_LPR_15K_2);
	bypass_iir(ADDR_IIR_LPR_15K_3);
	bypass_iir(ADDR_IIR_LPR_15K_4);
	bypass_iir(ADDR_IIR_LMR_15K_0);
	bypass_iir(ADDR_IIR_LMR_15K_1);
	bypass_iir(ADDR_IIR_LMR_15K_2);
	bypass_iir(ADDR_IIR_LMR_15K_3);
	bypass_iir(ADDR_IIR_LMR_15K_4);
}

void set_lpf15k(void)
{
	set_iir(lpr15k_0, ADDR_IIR_LPR_15K_0);
	set_iir(lpr15k_1, ADDR_IIR_LPR_15K_1);
	set_iir(lpr15k_2, ADDR_IIR_LPR_15K_2);
	set_iir(lpr15k_3, ADDR_IIR_LPR_15K_3);
	set_iir(lpr15k_4, ADDR_IIR_LPR_15K_4);

	set_iir(lpr15k_0, ADDR_IIR_LMR_15K_0);
	set_iir(lpr15k_1, ADDR_IIR_LMR_15K_1);
	set_iir(lpr15k_2, ADDR_IIR_LMR_15K_2);
	set_iir(lpr15k_3, ADDR_IIR_LMR_15K_3);
	set_iir(lpr15k_4, ADDR_IIR_LMR_15K_4);
}

void set_deem(int deem_mode)
{
	if (deem_mode == AUDIO_DEEM_75US) {
		set_iir(deem_75u_40, ADDR_IIR_LPR_DEEMPHASIS);
		set_iir(deem_75u_40, ADDR_IIR_LMR_DEEMPHASIS);
	} else if (deem_mode == AUDIO_DEEM_50US) {
		set_iir(deem_50u, ADDR_IIR_LPR_DEEMPHASIS);
		set_iir(deem_50u, ADDR_IIR_LMR_DEEMPHASIS);
	} else if (deem_mode == AUDIO_DEEM_J17) {
		set_iir(deem_j17, ADDR_IIR_LPR_DEEMPHASIS);
		set_iir(deem_j17, ADDR_IIR_LMR_DEEMPHASIS);
	} else {
		bypass_iir(ADDR_IIR_LPR_DEEMPHASIS);
		bypass_iir(ADDR_IIR_LMR_DEEMPHASIS);
	}
}

void set_i2s_intrp(int freq)
{
	double fs; /* system clock */
	double fout = 48e3; /* I2S output frequency */
	double rate = 16*8; /* system clock/data sample frequency*/
	int inc = 150;
	double m1, m2, m3;
	int M1, M2, M3;
	double s1;
	int S1;
	int count;

	int tmp;

	fs = FCLK;
	m1 = fs/fout/32*inc;
	m2 = fs/fout*inc;
	m3 = rate*inc;
	M1 = (int) (m1);
	M2 = (int) (m2);
	M3 = (int) (m3);

	s1 = 1/(rate*inc)*4096*4096;
	S1 = (int) (s1);

	count = (int)(fs/fout/32/2);

	tmp = (count<<16)|S1;

	adec_wr_reg((SRC_CTRL0), M1);
	adec_wr_reg((SRC_CTRL1), M2);
	adec_wr_reg((SRC_CTRL2), M3);
	adec_wr_reg((SRC_CTRL3), inc);
	adec_wr_reg((SRC_CTRL4), inc);
	adec_wr_reg((SRC_CTRL5), inc);
	adec_wr_reg((SRC_CTRL9), tmp);
}

void set_general(void)
{
	/* Initial the bb filter */

	set_filter(filter_bb_10k, ADDR_BB_FIR0_A_COEF, 16);
	set_filter(filter_bb_20k, ADDR_BB_FIR0_B_COEF, 16);

	set_filter(filter_bb_10k, ADDR_BB_FIR1_A_COEF, 16);
	set_filter(filter_bb_20k, ADDR_BB_FIR1_B_COEF, 16);

	set_filter(filter_bb_10k, ADDR_BB_FIR2_A_COEF, 16);
	set_filter(filter_bb_20k, ADDR_BB_FIR2_B_COEF, 16);


	adec_wr_reg(ADDR_BB_MUTE_THRESHOLD0, 0x0000000f);
	adec_wr_reg(ADDR_BB_MUTE_THRESHOLD1, 0x0000000f);
	adec_wr_reg(ADDR_BB_MUTE_THRESHOLD2, 0x0000000f);

#if 0
	write_reg32(ADDR_BB_NOISE_THRESHOLD1, 0x00000a3d);
	write_reg32(ADDR_BB_NOISE_THRESHOLD2, 0x00000a3d);
#else
	adec_wr_reg(ADDR_BB_NOISE_THRESHOLD0, 0x000013dc);
	adec_wr_reg(ADDR_BB_NOISE_THRESHOLD1, 0x000013dc);
	adec_wr_reg(ADDR_BB_NOISE_THRESHOLD2, 0x000013dc);
#endif

	adec_wr_reg(SAP_DET_THD, 0x200);
}

void set_btsc(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_BTSC);

	aa = (int)(4.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	aa = (int)((15.734e3*5)/(FCLK/4/16)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ1, aa);

	set_filter(filter_150k, ADDR_DDC_FIR0_COEF, 65);

	set_filter(filter_sap_15k_2, ADDR_DDC_FIR1_COEF, 65);

	set_iir(fix_deem0, ADDR_IIR_FIXED_DEEM_0);
	set_iir(fix_deem1, ADDR_IIR_FIXED_DEEM_1);
	set_iir(fix_deem2, ADDR_IIR_FIXED_DEEM_2);

	set_iir(qfilter0, ADDR_IIR_Q_FILTER_0);
	set_iir(qfilter1, ADDR_IIR_Q_FILTER_1);
	set_iir(qfilter2, ADDR_IIR_Q_FILTER_2);

	set_iir(pfilter0, ADDR_IIR_P_FILTER_0);
	set_iir(pfilter1, ADDR_IIR_P_FILTER_1);

	set_iir(lpr15k_btsc_0, ADDR_IIR_LPR_15K_0);
	set_iir(lpr15k_btsc_1, ADDR_IIR_LPR_15K_1);
	set_iir(lpr15k_btsc_2, ADDR_IIR_LPR_15K_2);
	set_iir(lpr15k_btsc_3, ADDR_IIR_LPR_15K_3);
	set_iir(lpr15k_btsc_4, ADDR_IIR_LPR_15K_4);

	set_iir(lpr15k_btsc_0, ADDR_IIR_LMR_15K_0);
	set_iir(lpr15k_btsc_1, ADDR_IIR_LMR_15K_1);
	set_iir(lpr15k_btsc_2, ADDR_IIR_LMR_15K_2);
	set_iir(lpr15k_btsc_3, ADDR_IIR_LMR_15K_3);
	set_iir(lpr15k_btsc_4, ADDR_IIR_LMR_15K_4);


	set_iir(btsc_pilot, ADDR_IIR_PILOT_0);
	set_iir(btsc_pilot, ADDR_IIR_PILOT_1);
	set_iir(btsc_pilot, ADDR_IIR_PILOT_2);

	set_deem(0);

	adec_wr_reg(ADDR_EXPANDER_SPECTRAL_ADJ, 0x198);

	adec_wr_reg(ADDR_EXPANDER_GAIN_ADJ, 0x02e0);
	adec_wr_reg(ADDR_EXPANDER_B2C_ADJ, 0x3e7d);

	adec_wr_reg(ADDR_LMR_ADJ, 0x1e8);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x3e0);
	adec_wr_reg(ADDR_LPR_COMP_CTRL, 0x3015);
	adec_wr_reg(ADDR_SAP_ADJ, 0x2ef);

	adec_wr_reg(ADDR_DEMOD_GAIN, 0x12);

	pr_info("Set Btsc relative setting done\n");
}

void set_a2k(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_A2_K);

	set_filter(filter_50k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_50k, ADDR_DDC_FIR1_COEF, 65);


	adec_wr_reg(ADDR_INDICATOR_STEREO_DTO, A2K_STEREO_DTO);
	adec_wr_reg(ADDR_INDICATOR_DUAL_DTO, A2K_DUAL_DTO);
	adec_wr_reg(ADDR_INDICATOR_CENTER_DTO, A2K_INDICATOR_DTO);


	aa = (int)(4.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	aa = (int)((4.724212e6-4.5e6)/(FCLK/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ1, aa);

	aa = (int)((55.0699e3+149.9)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_STEREO_DTO, aa);
	aa = (int)((55.0699e3+276.0)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_DUAL_DTO, aa);
	aa = (int)((55.0699e3)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_CENTER_DTO, aa);

	set_lpf15k();
	set_deem(0);

	adec_wr_reg(ADDR_DEMOD_GAIN, 0x12);

	adec_wr_reg(ADDR_LMR_ADJ, 0x3e0);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x3e0);

	adec_wr_reg(ADDR_LPR_COMP_CTRL, 0x010);
	adec_wr_reg(ADDR_IIR_SPEED_CTRL, 0xff5d7f7f/*0xd65d7f7f*/);
	adec_wr_reg(STEREO_DET_THD, 0x4000);
	adec_wr_reg(DUAL_DET_THD, 0x4000);

	adec_wr_reg((ADDR_SEL_CTRL), 0x1000);
}

void set_a2g(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_A2_BG);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(5.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	aa = (int)((5.7421875e6-5.5e6)/(FCLK/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ1, aa);

	aa = (int)((54.6875e3+117.5)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_STEREO_DTO, aa);
	aa = (int)((54.6875e3+274.1)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_DUAL_DTO, aa);
	aa = (int)((54.6875e3)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_CENTER_DTO, aa);

	set_lpf15k();
	set_deem(1);

	adec_wr_reg(ADDR_DEMOD_GAIN, 0x12);

	adec_wr_reg(ADDR_LMR_ADJ, 0x3e0);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x3e0);

	adec_wr_reg(ADDR_LPR_COMP_CTRL, 0x010);
	adec_wr_reg(ADDR_IIR_SPEED_CTRL, 0xff5d7f7f/*0xd65d7f7f*/);
	adec_wr_reg(STEREO_DET_THD, 0x4000);
	adec_wr_reg(DUAL_DET_THD, 0x4000);
}

void set_a2bg(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_A2_BG);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(5.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	aa = (int)((5.7421875e6-5.5e6)/(FCLK/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ1, aa);

	aa = (int)((54.6875e3+117.5)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_STEREO_DTO, aa);
	aa = (int)((54.6875e3+274.1)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_DUAL_DTO, aa);
	aa = (int)((54.6875e3)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_CENTER_DTO, aa);

	set_lpf15k();
	set_deem(1);

	adec_wr_reg(ADDR_DEMOD_GAIN, 0x12);

	adec_wr_reg(ADDR_LMR_ADJ, 0x3e0);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x3e0);

	adec_wr_reg(ADDR_LPR_COMP_CTRL, 0x010);
	adec_wr_reg(ADDR_IIR_SPEED_CTRL, 0xff5d7f7f/*0xd65d7f7f*/);
	adec_wr_reg(STEREO_DET_THD, 0x4000);
	adec_wr_reg(DUAL_DET_THD, 0x4000);
}

void set_a2dk1(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_A2_DK1);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(6.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	aa = (int)((6.2578125e6-6.5e6)/(FCLK/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ1, aa);

	aa = (int)((54.6875e3+117.5)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_STEREO_DTO, aa);
	aa = (int)((54.6875e3+274.1)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_DUAL_DTO, aa);
	aa = (int)((54.6875e3)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_CENTER_DTO, aa);

	set_lpf15k();
	set_deem(1);

	adec_wr_reg(ADDR_DEMOD_GAIN, 0x12);

	adec_wr_reg(ADDR_LMR_ADJ, 0x3e0);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x3e0);

	adec_wr_reg(ADDR_LPR_COMP_CTRL, 0x010);
	adec_wr_reg(ADDR_IIR_SPEED_CTRL, 0xff5d7f7f/*0xd65d7f7f*/);
	adec_wr_reg(STEREO_DET_THD, 0x4000);
	adec_wr_reg(DUAL_DET_THD, 0x4000);
}

void set_a2dk2(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_A2_DK2);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(6.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	aa = (int)((6.7421875e6-6.5e6)/(FCLK/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ1, aa);

	aa = (int)((54.6875e3+117.5)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_STEREO_DTO, aa);
	aa = (int)((54.6875e3+274.1)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_DUAL_DTO, aa);
	aa = (int)((54.6875e3)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_CENTER_DTO, aa);

	set_lpf15k();
	set_deem(1);

	adec_wr_reg(ADDR_DEMOD_GAIN, 0x12);

	adec_wr_reg(ADDR_LMR_ADJ, 0x3e0);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x3e0);

	adec_wr_reg(ADDR_LPR_COMP_CTRL, 0x010);
	adec_wr_reg(ADDR_IIR_SPEED_CTRL, 0xff5d7f7f/*0xd65d7f7f*/);
	adec_wr_reg(STEREO_DET_THD, 0x4000);
	adec_wr_reg(DUAL_DET_THD, 0x4000);
}

void set_a2dk3(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_A2_DK3);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(6.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	aa = (int)((5.7421875e6-6.5e6)/(FCLK/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ1, aa);

	aa = (int)((54.6875e3+117.5)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_STEREO_DTO, aa);
	aa = (int)((54.6875e3+274.1)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_DUAL_DTO, aa);
	aa = (int)((54.6875e3)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_CENTER_DTO, aa);

	set_lpf15k();
	set_deem(1);

	adec_wr_reg(ADDR_DEMOD_GAIN, 0x12);

	adec_wr_reg(ADDR_LMR_ADJ, 0x3e0);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x3e0);

	adec_wr_reg(ADDR_LPR_COMP_CTRL, 0x010);
	adec_wr_reg(ADDR_IIR_SPEED_CTRL, 0xff5d7f7f/*0xd65d7f7f*/);
	adec_wr_reg(STEREO_DET_THD, 0x4000);
	adec_wr_reg(DUAL_DET_THD, 0x4000);
}

void set_eiaj(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_EIAJ);

	/* DDC FILTER */
	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);

	aa = (int)(4.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	aa = (int)((15.734e3*2)/(FCLK/4/16)*1024.0*1024.0*8.0);
	/* aa = (0x80e40); */
	adec_wr_reg(ADDR_DDC_FREQ1, aa);


	aa = (int)((15.734e3*3.5+982.5)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_STEREO_DTO, aa);
	aa = (int)((15.734e3*3.5+922.5)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_DUAL_DTO, aa);
	aa = (int)((15.734e3*3.5)/(FCLK/16/4)*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_INDICATOR_CENTER_DTO, aa);

#if 1
		set_filter(filter_sap_15k_2, ADDR_DDC_FIR1_COEF, 65);

		set_lpf15k();

		set_deem(0);

		adec_wr_reg(ADDR_DEMOD_GAIN, 0x1);
		adec_wr_reg(ADDR_LMR_ADJ, 0x3c0);
		adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x2e0);

		adec_wr_reg(ADDR_LPR_COMP_CTRL, 0x410);
#else
		set_filter(filter_sap_15k_4, ADDR_DDC_FIR1_COEF, 65);

		set_iir(lpr15k_0_4, ADDR_IIR_LPR_15K_0);
		set_iir(lpr15k_1_4, ADDR_IIR_LPR_15K_1);
		set_iir(lpr15k_2_4, ADDR_IIR_LPR_15K_2);
		set_iir(lpr15k_3_4, ADDR_IIR_LPR_15K_3);
		set_iir(lpr15k_4_4, ADDR_IIR_LPR_15K_4);

		set_iir(lpr15k_0_4, ADDR_IIR_LMR_15K_0);
		set_iir(lpr15k_1_4, ADDR_IIR_LMR_15K_1);
		set_iir(lpr15k_2_4, ADDR_IIR_LMR_15K_2);
		set_iir(lpr15k_3_4, ADDR_IIR_LMR_15K_3);
		set_iir(lpr15k_4_4, ADDR_IIR_LMR_15K_4);

		set_deem(0);

		adec_wr_reg(ADDR_DEMOD_GAIN, 0x22);
		adec_wr_reg(ADDR_LMR_ADJ, 0x3e0);
		adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x2e0);

		adec_wr_reg(ADDR_LPR_COMP_CTRL, 0x510);
#endif


	adec_wr_reg((ADDR_SEL_CTRL), 0x1000);
	pr_info("set eia-j done\n");
}

void set_nicam_dk(void)
{
	int aa;

	adec_wr_reg(0x103, 0x1000000);
	adec_wr_reg(0x115, 0x1503d);

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_NICAM_DK);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(6.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x200);

	set_deem(2);

	adec_wr_reg(0x103, 0x7f);

	aa = (int)((FCLK-5.85e6)/FCLK*1024.0*1024.0*16.0);
	adec_wr_reg(0x110, aa);
}

void set_nicam_i(void)
{
	int aa;

	adec_wr_reg(0x103, 0x1000000);
	adec_wr_reg(0x110, 0xcb9581);
	adec_wr_reg(0x115, 0x1503d);

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_NICAM_I);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(6.0e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x200);

	set_deem(2);
	adec_wr_reg(0x103, 0x7f);

	aa = (int)((FCLK-6.552e6)/FCLK*1024.0*1024.0*16.0);
	adec_wr_reg(0x110, aa);
}

void set_nicam_bg(void)
{
	int aa;

	adec_wr_reg(0x103, 0x1000000);
	adec_wr_reg(0x115, 0x1503d);

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_NICAM_BG);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(5.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x200);

	set_deem(2);
	adec_wr_reg(0x103, 0x7f);

	aa = (int)((FCLK-5.85e6)/FCLK*1024.0*1024.0*16.0);
	adec_wr_reg(0x110, aa);
}

void set_nicam_l(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, AUDIO_STANDARD_NICAM_L);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(6.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x200);

	set_deem(2);
	adec_wr_reg(0x103, 0x7f);

	aa = (int)((FCLK-5.85e6)/FCLK*1024.0*1024.0*16.0);
	adec_wr_reg(0x110, aa);
}

void set_mono_m(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, 0x2);

	set_filter(filter_50k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_50k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(4.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	set_lpf15k();
	set_deem(0);

	adec_wr_reg(ADDR_DEMOD_GAIN, 0x12);

	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x200);

	adec_wr_reg((ADDR_SEL_CTRL), 0x1000);

	set_lpf15k();
	set_deem(0);
}

void set_mono_dk(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, 0xa);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(6.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);

	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x200);

	set_deem(1);
	set_lpf15k();
}

void set_mono_i(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, 0x7);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(6.0e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x200);

	set_deem(1);
	set_lpf15k();
}

void set_mono_bg(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, 0x8);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(5.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x200);

	set_deem(1);
	set_lpf15k();
}

void set_mono_l(void)
{
	int aa;

	adec_wr_reg(ADDR_ADEC_CTRL, 0x9);

	set_filter(filter_100k, ADDR_DDC_FIR0_COEF, 65);
	set_filter(filter_100k, ADDR_DDC_FIR1_COEF, 65);

	aa = (int)(6.5e6/FCLK*1024.0*1024.0*8.0);
	adec_wr_reg(ADDR_DDC_FREQ0, aa);
	adec_wr_reg(ADDR_LPR_GAIN_ADJ, 0x200);

	set_deem(1);
	set_lpf15k();
}

static void set_standard(uint32_t standard)
{
	pr_info("\n<<<<<<<<<<<<<<< start configure register\n");
	switch (standard) {
	case AUDIO_STANDARD_BTSC:
		pr_info("<<<<<<<<<<<<<<< Set BTSC  and Test\n");
		set_btsc();
		pr_info("<<<<<<<<<<<<<<< Set BTSC  Done\n");
		break;
	case AUDIO_STANDARD_A2_K:
		pr_info("<<<<<<<<<<<<<<< Set A2K and Test\n");
		set_a2k();
		break;
	case AUDIO_STANDARD_EIAJ:
		pr_info("<<<<<<<<<<<<<<< Set EIAJ and Test\n");
		set_eiaj();
		break;
	case AUDIO_STANDARD_A2_BG:
		pr_info("<<<<<<<<<<<<<<< Set A2BG and Test\n");
		set_a2bg();
		break;
	case AUDIO_STANDARD_A2_DK1:
		pr_info("<<<<<<<<<<<<<<< Set A2 DK1 and Test\n");
		set_a2dk1();
		break;
	case AUDIO_STANDARD_A2_DK2:
		pr_info("<<<<<<<<<<<<<<< Set A2 DK2 and Test\n");
		set_a2dk2();
		break;
	case AUDIO_STANDARD_A2_DK3:
		pr_info("<<<<<<<<<<<<<<< Set A2DK3 and Test\n");
		set_a2dk3();
		break;
	case AUDIO_STANDARD_NICAM_DK:
		pr_info("<<<<<<<<<<<<<<< Set NICAM DK and Test\n");
		set_nicam_dk();
		break;
	case AUDIO_STANDARD_NICAM_I:
		pr_info("<<<<<<<<<<<<<<< Set NICAM I and Test\n");
		set_nicam_i();
		break;
	case AUDIO_STANDARD_NICAM_BG:
		pr_info("<<<<<<<<<<<<<<< Set NICAM BG and Test\n");
		set_nicam_bg();
		break;
	case AUDIO_STANDARD_NICAM_L:
		pr_info("<<<<<<<<<<<<<<< Set NICAM L and Test\n");
		set_nicam_l();
		break;
	case AUDIO_STANDARD_MONO_M:
		pr_info("<<<<<<<<<<<<<<< Set mono M and Test\n");
		set_mono_m();
		break;
	case AUDIO_STANDARD_MONO_DK:
		pr_info("<<<<<<<<<<<<<<< Set mono DK and Test\n");
		set_mono_dk();
		break;
	case AUDIO_STANDARD_MONO_I:
		pr_info("<<<<<<<<<<<<<<< Set mono I and Test\n");
		set_mono_i();
		break;
	case AUDIO_STANDARD_MONO_BG:
		pr_info("<<<<<<<<<<<<<<< Set mono BG and Test\n");
		set_mono_bg();
		break;
	case AUDIO_STANDARD_MONO_L:
		pr_info("<<<<<<<<<<<<<<< Set mono L and Test\n");
		set_mono_l();
		break;
	}

	pr_info("\n<<<<<<<<<<<<<<< configure register finished\n");
}

void update_car_power_measure(int *sc1_power, int *sc2_power)
{
	uint32_t reg_value;

	reg_value = adec_rd_reg(CARRIER_MAG_REPORT);
	*sc1_power = reg_value & 0xffff;
	*sc2_power = (reg_value >> 16) & 0xffff;
}

void update_a2_eiaj_mode(int auto_en, int *stereo_flag, int *dual_flag)
{
	uint32_t reg_value = 0;
	uint32_t stereo_power = 0, dual_power = 0;

	msleep(a2_detect_delay);

	if (auto_en) {
		reg_value = adec_rd_reg(CARRIER_MAG_REPORT);
		pr_info("%s CARRIER_MAG_REPORT: 0x%x [threshold: 0x%x].\n",
				__func__, reg_value, audio_a2_carrier_report);
		if (((reg_value >> 16) & 0xffff) < audio_a2_carrier_report) {
			*stereo_flag = 0;
			*dual_flag = 0;
		} else {
			reg_value = adec_rd_reg(AUDIO_MODE_REPORT);
			*stereo_flag = reg_value&0x1;
			*dual_flag = (reg_value>>1)&0x1;
		}
	} else {
		reg_value = adec_rd_reg(POWER_REPORT);
		pr_info("%s POWER_REPORT: 0x%x [threshold: 0x%x].\n",
				__func__, reg_value, audio_a2_power_threshold);
		stereo_power = reg_value & 0xffff;
		dual_power = (reg_value >> 16) & 0xffff;

		if (stereo_power > dual_power &&
			stereo_power > audio_a2_power_threshold)
			*stereo_flag = 1;
		else
			*stereo_flag = 0;

		if (stereo_power < dual_power &&
			dual_power > audio_a2_power_threshold)
			*dual_flag = 1;
		else
			*dual_flag = 0;
	}

	/* 0:MONO 1:Stereo 2:Dual */
	if (*dual_flag == 0 && *stereo_flag == 0)
		signal_audmode = 0;
	else if (*stereo_flag)
		signal_audmode = 1;
	else if (*dual_flag)
		signal_audmode = 2;
}

void update_btsc_mode(int auto_en, int *stereo_flag, int *sap_flag)
{
	uint32_t reg_value;
	uint32_t stereo_level, sap_level;

	mdelay(btsc_detect_delay);
	if (auto_en) {
		reg_value = adec_rd_reg(AUDIO_MODE_REPORT);
		*stereo_flag = reg_value&0x1;
		*sap_flag = (reg_value>>1)&0x1;
	} else {
		reg_value = adec_rd_reg(STEREO_LEVEL_REPORT);
		stereo_level = reg_value&0xffff;

		if (stereo_level > 0x1800)
			*stereo_flag = 1;
		else
			*stereo_flag = 0;

		reg_value = adec_rd_reg(CARRIER_MAG_REPORT);
		sap_level = (reg_value>>16)&0xffff;

		if (sap_level > 0x200)
			*sap_flag = 1;
		else
			*sap_flag = 0;
	}

	/*0:MONO 1:Stereo 2:MONO+SAP 3:Stereo+SAP*/
	signal_audmode = (*stereo_flag) | (*sap_flag << 1);

}

void update_nicam_mode(int *nicam_flag, int *nicam_mono_flag,
		int *nicam_stereo_flag, int *nicam_dual_flag)
{
	uint32_t reg_value = 0;
	int nicam_lock = 0;
	int cic = 0;

	mdelay(nicam_detect_delay);

	reg_value = adec_rd_reg(NICAM_LEVEL_REPORT);
	nicam_lock = (reg_value>>28)&1;
	reg_value = adec_rd_reg(NICAM_MODE_REPORT);
	cic = (reg_value>>17)&3;

	*nicam_flag = nicam_lock;
	*nicam_mono_flag = (cic == 1) && nicam_lock;
	*nicam_stereo_flag = (cic == 0) && nicam_lock;
	*nicam_dual_flag = (cic == 2) && nicam_lock;

	/* 0:MONO 1:Stereo 2:Dual 3:NICAM MONO */
	if (*nicam_mono_flag)
		signal_audmode = 3;
	else if (*nicam_stereo_flag)
		signal_audmode = 1;
	else if (*nicam_dual_flag)
		signal_audmode = 2;
	else
		signal_audmode = 0;
}

void set_btsc_outputmode(uint32_t outmode)
{
	uint32_t reg_value = 0;
	uint32_t tmp_value = 0, tmp_value1 = 0;
	int stereo_flag = 0, sap_flag = 0;
	/*static int last_stereo_flag = -1,last_sap_flag = -1,last_mode = -1;*/

	update_btsc_mode(1, &stereo_flag, &sap_flag);

	/* ADEC_CTRL: 0x10
	 * bits[8]: Audio_mute, 1 = mute, 0 = not mute.
	 * bits[7:4]: Output select control.
	 * 0 = nicam mono / dual first.
	 * 1 = stereo / dual first + second.
	 * 2 = dual second.
	 * 3 = fm mono.
	 * bits[3:0]: Std_sel.
	 */
	reg_value = adec_rd_reg(ADDR_ADEC_CTRL);

	pr_info("%s regval:0x%x, signal_audmode:%d, outmode:%d\n",
				__func__, reg_value, signal_audmode, outmode);

	if (last_stereo_flag == stereo_flag
			&& last_sap_flag == sap_flag
			&& last_mode == outmode)
		return;

	/* priority: Stereo > MONO > SAP */
	/* 0:MONO 1:Stereo 2:MONO+SAP 3:Stereo+SAP */
	switch (signal_audmode) {
	case 0: /* MONO */
		if (outmode != AUDIO_OUTMODE_BTSC_MONO) {
			outmode = AUDIO_OUTMODE_BTSC_MONO;
			aud_mode = AUDIO_OUTMODE_BTSC_MONO;
		}
		break;
	case 1: /* Stereo */
		if (outmode != AUDIO_OUTMODE_BTSC_MONO &&
			outmode != AUDIO_OUTMODE_BTSC_STEREO) {
			outmode = AUDIO_OUTMODE_BTSC_STEREO;
			aud_mode = AUDIO_OUTMODE_BTSC_STEREO;
		}
		break;
	case 2: /* MONO+SAP */
		if (outmode != AUDIO_OUTMODE_BTSC_MONO &&
			outmode != AUDIO_OUTMODE_BTSC_SAP) {
			outmode = AUDIO_OUTMODE_BTSC_MONO;
			aud_mode = AUDIO_OUTMODE_BTSC_MONO;
		}
		break;
	case 3: /* Stereo+SAP */
		if (outmode != AUDIO_OUTMODE_BTSC_MONO &&
			outmode != AUDIO_OUTMODE_BTSC_SAP &&
			outmode != AUDIO_OUTMODE_BTSC_STEREO) {
			outmode = AUDIO_OUTMODE_BTSC_STEREO;
			aud_mode = AUDIO_OUTMODE_BTSC_STEREO;
		}
		break;
	}

	switch (outmode) {
	case AUDIO_OUTMODE_BTSC_MONO:
		tmp_value = (reg_value & 0xf) | (0 << 4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		break;

	case AUDIO_OUTMODE_BTSC_STEREO:
		if (stereo_flag) {
			tmp_value = (reg_value & 0xf) | (1 << 4);
			adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);

			reg_value = adec_rd_reg(ADDR_LPR_COMP_CTRL);
			tmp_value1 = (reg_value & 0xffff);
			adec_wr_reg(ADDR_LPR_COMP_CTRL, tmp_value1);
		} else {
			tmp_value = (reg_value & 0xf) | (0 << 4);
			adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		}
		break;

	case AUDIO_OUTMODE_BTSC_SAP:
		if (sap_flag) {
			if (aml_atvdemod_get_btsc_sap_mode() != 0)
				tmp_value = (reg_value & 0xf) | (6 << 4);
			else
				tmp_value = (reg_value & 0xf) | (2 << 4);
			adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);

			reg_value = adec_rd_reg(ADDR_LPR_COMP_CTRL);
			tmp_value1 = (reg_value & 0xffff)|(1 << 16);
			adec_wr_reg(ADDR_LPR_COMP_CTRL, tmp_value1);
		} else if (stereo_flag) {
			tmp_value = (reg_value & 0xf) | (1 << 4);
			adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);

			reg_value = adec_rd_reg(ADDR_LPR_COMP_CTRL);
			tmp_value1 = (reg_value & 0xffff);
			adec_wr_reg(ADDR_LPR_COMP_CTRL, tmp_value1);
		} else {
			tmp_value = (reg_value & 0xf) | (0 << 4);
			adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		}
		break;
	}

	last_stereo_flag = stereo_flag;
	last_sap_flag = sap_flag;
	last_mode = outmode;
}

void set_a2_eiaj_outputmode(uint32_t outmode)
{
	uint32_t reg_value = 0;
	uint32_t tmp_value = 0;
	int stereo_flag = 0, dual_flag = 0;
	/*static int last_stereo_flag = -1,last_dual_flag = -1, last_mode=-1;*/

	update_a2_eiaj_mode(audio_a2_auto, &stereo_flag, &dual_flag);

	/* ADEC_CTRL: 0x10
	 * bits[8]: Audio_mute, 1 = mute, 0 = not mute.
	 * bits[7:4]: Output select control.
	 * 0 = nicam mono / dual first.
	 * 1 = stereo / dual first + second.
	 * 2 = dual second.
	 * 3 = fm mono.
	 * bits[3:0]: Std_sel.
	 */
	reg_value = adec_rd_reg(ADDR_ADEC_CTRL);

	pr_info("%s regval:0x%x, signal_audmode:%d, outmode:%d\n",
				__func__, reg_value, signal_audmode, outmode);

	if (last_stereo_flag == stereo_flag
			&& last_dual_flag == dual_flag
			&& last_mode == outmode)
		return;

	set_output_left_right_exchange(1);

	/* priority: Stereo > NICAM MONO > Dual > FM MONO */
	/* 0:MONO 1:Stereo 2:Dual */
	switch (signal_audmode) {
	case 0: /* MONO */
		if (outmode != AUDIO_OUTMODE_A2_MONO) {
			outmode = AUDIO_OUTMODE_A2_MONO;
			aud_mode = AUDIO_OUTMODE_A2_MONO;
		}
		break;
	case 1: /* Stereo */
		if ((outmode != AUDIO_OUTMODE_A2_MONO &&
			outmode != AUDIO_OUTMODE_A2_STEREO)
			|| (last_stereo_flag != stereo_flag)) {
			outmode = AUDIO_OUTMODE_A2_STEREO;
			aud_mode = AUDIO_OUTMODE_A2_STEREO;
		}
		break;
	case 2: /* Dual */
		if ((outmode != AUDIO_OUTMODE_A2_DUAL_A &&
			outmode != AUDIO_OUTMODE_A2_DUAL_B &&
			outmode != AUDIO_OUTMODE_A2_DUAL_AB)
			|| (last_dual_flag != dual_flag)) {
			outmode = AUDIO_OUTMODE_A2_DUAL_A;
			aud_mode = AUDIO_OUTMODE_A2_DUAL_A;
		}
		break;
	}

	switch (outmode) {
	case AUDIO_OUTMODE_A2_MONO:
		tmp_value = (reg_value&0xf)|(0<<4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		break;

	case AUDIO_OUTMODE_A2_STEREO:
		tmp_value = (reg_value&0xf)|(1<<4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		set_output_left_right_exchange(0);
		break;

	case AUDIO_OUTMODE_A2_DUAL_A:
		tmp_value = (reg_value&0xf)|(0<<4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		break;

	case AUDIO_OUTMODE_A2_DUAL_B:
		tmp_value = (reg_value&0xf)|(2<<4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		break;

	case AUDIO_OUTMODE_A2_DUAL_AB:
		tmp_value = (reg_value&0xf)|(3<<4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		set_output_left_right_exchange(0);
		break;
	}

	last_stereo_flag = stereo_flag;
	last_dual_flag = dual_flag;
	last_mode = outmode;
}

void set_nicam_outputmode(uint32_t outmode)
{
	uint32_t reg_value = 0;
	uint32_t tmp_value = 0;
	int nicam_mono_flag = 0, nicam_stereo_flag = 0, nicam_dual_flag = 0;
	int nicam_lock = 0;
	/*static int last_nicam_lock = -1, last_nicam_mono_flag = -1;*/
	/*static int last_stereo_flag = -1,last_dual_flag = -1, last_mode=-1;*/

	update_nicam_mode(&nicam_lock, &nicam_mono_flag,
			&nicam_stereo_flag, &nicam_dual_flag);

	/* ADEC_CTRL: 0x10
	 * bits[8]: Audio_mute, 1 = mute, 0 = not mute.
	 * bits[7:4]: Output select control.
	 * 0 = nicam mono / dual first.
	 * 1 = stereo / dual first + second.
	 * 2 = dual second.
	 * 3 = fm mono.
	 * bits[3:0]: Std_sel.
	 */
	reg_value = adec_rd_reg(ADDR_ADEC_CTRL);

	pr_info("%s nicam_lock:%d, regval:0x%x, signal_mode:%d, outmode:%d\n",
			__func__, nicam_lock, reg_value,
			signal_audmode, outmode);

	if (last_nicam_lock == nicam_lock
			&& last_nicam_mono_flag == nicam_mono_flag
			&& last_stereo_flag == nicam_stereo_flag
			&& last_dual_flag == nicam_dual_flag
			&& last_mode == outmode)
		return;

	set_output_left_right_exchange(1);

	/* priority: Stereo > NICAM MONO > Dual > FM MONO */
	/* 0:FM MONO 1:Stereo 2:Dual 3:NICAM MONO */
	switch (signal_audmode) {
	case 0: /* FM MONO */
		if (outmode != AUDIO_OUTMODE_NICAM_MONO) {
			outmode = AUDIO_OUTMODE_NICAM_MONO;
			aud_mode = AUDIO_OUTMODE_NICAM_MONO;
		}
		break;
	case 1: /* Stereo */
		if ((outmode != AUDIO_OUTMODE_NICAM_MONO &&
			outmode != AUDIO_OUTMODE_NICAM_STEREO)
			|| (last_stereo_flag != nicam_stereo_flag)) {
			outmode = AUDIO_OUTMODE_NICAM_STEREO;
			aud_mode = AUDIO_OUTMODE_NICAM_STEREO;
		}
		break;
	case 2: /* Dual */
		if ((outmode != AUDIO_OUTMODE_NICAM_MONO &&
			outmode != AUDIO_OUTMODE_NICAM_DUAL_A &&
			outmode != AUDIO_OUTMODE_NICAM_DUAL_B &&
			outmode != AUDIO_OUTMODE_NICAM_DUAL_AB)
			|| (last_dual_flag != nicam_dual_flag)) {
			outmode = AUDIO_OUTMODE_NICAM_DUAL_A;
			aud_mode = AUDIO_OUTMODE_NICAM_DUAL_A;
		}
		break;
	case 3: /* NICAM MONO */
		if ((outmode != AUDIO_OUTMODE_NICAM_MONO &&
			outmode != AUDIO_OUTMODE_NICAM_MONO1)
			|| (last_nicam_mono_flag != nicam_mono_flag)) {
			outmode = AUDIO_OUTMODE_NICAM_MONO1;
			aud_mode = AUDIO_OUTMODE_NICAM_MONO1;
		}
		break;
	}

	switch (outmode) {
	case AUDIO_OUTMODE_NICAM_MONO:/* fm mono */
		if (aud_std == AUDIO_STANDARD_NICAM_BG)
			tmp_value = (AUDIO_STANDARD_A2_BG & 0xf) | (0 << 4);
		else if (aud_std == AUDIO_STANDARD_NICAM_DK)
			tmp_value = (AUDIO_STANDARD_A2_DK2 & 0xf) | (0 << 4);
		else if (aud_std == AUDIO_STANDARD_NICAM_I)
			tmp_value = (AUDIO_STANDARD_CHINA & 0xf) | (0 << 4);
		else if (aud_std == AUDIO_STANDARD_NICAM_L)
			tmp_value = (AUDIO_STANDARD_MONO_ONLY & 0xf) | (0 << 4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		break;

	case AUDIO_OUTMODE_NICAM_MONO1:/* nicam mono */
		if ((reg_value & 0xf) != (aud_std & 0xf))
			reg_value = (reg_value & ~0xf) | (aud_std & 0xf);

		tmp_value = (reg_value&0xf)|(0<<4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		break;

	case AUDIO_OUTMODE_NICAM_STEREO:
		if ((reg_value & 0xf) != (aud_std & 0xf))
			reg_value = (reg_value & ~0xf) | (aud_std & 0xf);

		tmp_value = (reg_value&0xf)|(1<<4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		set_output_left_right_exchange(0);
		break;

	case AUDIO_OUTMODE_NICAM_DUAL_A:
		if ((reg_value & 0xf) != (aud_std & 0xf))
			reg_value = (reg_value & ~0xf) | (aud_std & 0xf);

		tmp_value = (reg_value&0xf)|(0<<4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		break;

	case AUDIO_OUTMODE_NICAM_DUAL_B:
		if ((reg_value & 0xf) != (aud_std & 0xf))
			reg_value = (reg_value & ~0xf) | (aud_std & 0xf);

		tmp_value = (reg_value&0xf)|(2<<4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		break;

	case AUDIO_OUTMODE_NICAM_DUAL_AB:
		if ((reg_value & 0xf) != (aud_std & 0xf))
			reg_value = (reg_value & ~0xf) | (aud_std & 0xf);

		tmp_value = (reg_value&0xf)|(1<<4);
		adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);
		set_output_left_right_exchange(0);
		break;
	}

	pr_info("[%s] tmp_value: 0x%x.\n", __func__, reg_value);

	last_nicam_lock = nicam_lock;
	last_nicam_mono_flag = nicam_mono_flag;
	last_stereo_flag = nicam_stereo_flag;
	last_dual_flag = nicam_dual_flag;
	last_mode = outmode;
}

void set_outputmode(uint32_t standard, uint32_t outmode)
{
#ifdef AUDIO_MOD_DET_INTERNAL
	uint32_t tmp_value = 0;
	uint32_t reg_value = 0;

	reg_value = adec_rd_reg(ADDR_ADEC_CTRL);
	tmp_value = (reg_value&0xf)|(outmode<<4)|(1<<6);
	adec_wr_reg(ADDR_ADEC_CTRL, tmp_value);

	pr_info("set_outputmode:std:%d, outmod:%d\n", standard, outmode);
	if (standard == AUDIO_STANDARD_BTSC &&
			outmode == AUDIO_OUTMODE_SAP_DUAL) {
		reg_value = adec_rd_reg(ADDR_LPR_COMP_CTRL);
		tmp_value = (reg_value & 0xffff)|(1 << 16);
		adec_wr_reg(ADDR_LPR_COMP_CTRL, tmp_value);
	}

	if (standard == AUDIO_STANDARD_NICAM_DK ||
			standard == AUDIO_STANDARD_NICAM_I ||
			standard == AUDIO_STANDARD_NICAM_BG ||
			standard == AUDIO_STANDARD_NICAM_L) {
		pr_info("set_outputmode NICAM...\n");
		set_nicam_outputmode(outmode);
	}

#else
	switch (standard) {
	case AUDIO_STANDARD_BTSC:
		set_btsc_outputmode(outmode);
		break;
	case AUDIO_STANDARD_A2_K:
	case AUDIO_STANDARD_EIAJ:
	case AUDIO_STANDARD_A2_BG:
	case AUDIO_STANDARD_A2_DK1:
	case AUDIO_STANDARD_A2_DK2:
	case AUDIO_STANDARD_A2_DK3:
		set_a2_eiaj_outputmode(outmode);
		break;
	case AUDIO_STANDARD_NICAM_DK:
	case AUDIO_STANDARD_NICAM_I:
	case AUDIO_STANDARD_NICAM_BG:
	case AUDIO_STANDARD_NICAM_L:
		set_nicam_outputmode(outmode);
		break;
	}
#endif
}

void aud_demod_clk_gate(int on)
{
	if (on)
		adec_wr_reg(1, 0xf13);
	else
		adec_wr_reg(1, 0);
}

void configure_adec(int Audio_mode)
{
	pr_info("configure audio demod register\n");

	aud_demod_clk_gate(1);

	/*
	 * set gate clk for btsc and nicam .
	 */
	if (is_meson_txhd_cpu())
		adec_wr_reg(0x28, 0xa);

	set_standard(Audio_mode);

	set_general();
	set_i2s_intrp(0);
}

void adec_soft_reset(void)
{
	adec_wr_reg(ADEC_RESET, 0x0);
	adec_wr_reg(ADEC_RESET, 0x1);
}

void audio_thd_init(void)
{
	adec_wr_reg(CNT_MAX0, 0x6f2a9);
	adec_wr_reg(THD_OV0, 0x18000);
	thd_flag = 0;
	thd_tmp_v = 0;
	thd_cnt = 0;
}

void audio_thd_det(void)
{
	thd_cnt++;
	if (thd_cnt % 5 != 0)
		return;

	if (thd_flag == 0) {
		thd_tmp_v += adec_rd_reg(OV_CNT_REPORT) & 0xffff;

		pr_info("#0x12:0x%x\n", (adec_rd_reg(OV_CNT_REPORT) & 0xffff));
		if (thd_cnt == 15) {
			thd_tmp_v /= 3;
			if (thd_tmp_v > audio_thd_threshold1) {
				thd_flag = 1;
				adec_wr_reg(ADDR_DEMOD_GAIN, 0x13);
			}
			thd_cnt = 0;
			thd_tmp_v = 0;
		}
	} else if (thd_flag == 1) {
		thd_tmp_v += adec_rd_reg(OV_CNT_REPORT) & 0xffff;

		pr_info("#0x13:0x%x\n", (adec_rd_reg(OV_CNT_REPORT) & 0xffff));
		if (thd_cnt == 15) {
			thd_tmp_v /= 3;
			if (thd_tmp_v <= audio_thd_threshold2) {
				thd_flag = 0;
				adec_wr_reg(ADDR_DEMOD_GAIN, 0x12);
			}
			thd_cnt = 0;
			thd_tmp_v = 0;
		}
	}
}

void audio_carrier_offset_det(void)
{
	unsigned int carrier_freq = 0, report = 0;
	int threshold = 0;

	report = adec_rd_reg(DC_REPORT);
	carrier_freq = adec_rd_reg(ADDR_DDC_FREQ0);

	pr_carr("\n\nreport: 0x%x.\n", report);
	pr_carr("read carrier_freq: 0x%x.\n", carrier_freq);
	report = report & 0xFFFF;

	if (report > (1 << 15))
		threshold = report - (1 << 16);
	else
		threshold = report;

	threshold = threshold >> 8;
	pr_carr("threshold: %d.\n", threshold);

	if (threshold > 30) {
		carrier_freq = carrier_freq - 0x100;
		adec_wr_reg(ADDR_DDC_FREQ0, carrier_freq);
	} else if (threshold < -30) {
		carrier_freq = carrier_freq + 0x100;
		adec_wr_reg(ADDR_DDC_FREQ0, carrier_freq);
	}

	pr_carr("write carrier_freq: 0x%x.\n", carrier_freq);
}

void set_outputmode_status_init(void)
{
	last_nicam_lock = -1;
	last_nicam_mono_flag = -1;
	last_stereo_flag = -1;
	last_dual_flag = -1;
	last_sap_flag = -1;
	last_mode = -1;
}

void set_output_left_right_exchange(unsigned int ch)
{
	unsigned int read = 0;

	atvaudio_ctrl_read(&read);

	if (is_meson_tl1_cpu()) { /* bit[19] */
		if ((read & (1 << 19)) != ((ch & 0x01) << 19))
			atvaudio_ctrl_write((read & ~(1 << 19)) |
					((ch & 0x01) << 19));
	} else { /* bit[2] */
		if ((read & (1 << 2)) != ((ch & 0x01) << 2))
			atvaudio_ctrl_write((read & ~(1 << 2)) |
					((ch & 0x01) << 2));
	}
}

#endif /* __ATVAUDDEMOD_FUN_H */
