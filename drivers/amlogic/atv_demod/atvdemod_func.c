/*
 * drivers/amlogic/atv_demod/atvdemod_func.c
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

/* Standard Liniux Headers */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <uapi/linux/videodev2.h>
#include <linux/amlogic/cpu_version.h>

#include "drivers/amlogic/media/vin/tvin/tvafe/tvafe_general.h"

#include "atvdemod_func.h"
#include "atv_demod_debug.h"
#include "atv_demod_driver.h"
#include "atvauddemod_func.h"
#include "atv_demod_ops.h"
#include "atv_demod_driver.h"
#include "atv_demod_access.h"

unsigned int broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC;
unsigned int aud_std = AUDIO_STANDARD_NICAM_DK;
unsigned int aud_mode = AUDIO_OUTMODE_STEREO;
bool aud_auto = true;
bool aud_reinit;
bool aud_mono_only;
unsigned long over_threshold = 0xffff;
unsigned long input_amplitude = 0xffff;

unsigned int non_std_en;

unsigned int non_std_times = 50;
unsigned int non_check_delay_times = 50;
int non_std_thld_4c_h = 100;
int non_std_thld_4c_l = 30;
int non_std_thld_54_h = 500;
int non_std_thld_54_l = 300;
int sum1_thd_h;
int sum1_thd_l = 0x7fffffff;
int sum2_thd_h;
int sum2_thd_l = 0x7fffffff;

unsigned int atv_video_gain;
unsigned int carrier_amplif_val = 0xc010301;/*0xc030901;*/
unsigned int extra_input_fil_val = 0x1030501;
bool aud_dmd_jilinTV;
unsigned int if_freq = 4250000;	/*PAL-DK:3250000;NTSC-M:4250000*/
unsigned int if_inv;

int afc_default = CARR_AFC_DEFAULT_VAL;
int snr_threshold = 30;


/*
 * GDE_Curve
 * 0: CURVE-M
 * 1: CURVE-A
 * 2: CURVE-B
 * 3: CURVE-CHINA
 * 4: BYPASS
 * BG --> CURVE-B(BYPASS)
 * DK --> CURVE-CHINA
 * NM --> CURVE-M
 * I --> BYPASS
 * SECAM --> BYPASS
 */
unsigned int gde_curve;

unsigned int sound_format;
unsigned int freq_hz_cvrt = AML_ATV_DEMOD_FREQ_60HZ_VERT;
unsigned int atvdemod_debug_en;

/*1:gpio mode output low;2:pwm mode*/
unsigned int atvdemod_agc_pinmux = 2;
unsigned int atvdemod_afc_range = 5;
unsigned int atvdemod_afc_offset = 500;

unsigned int pwm_kp = 0x19;
unsigned int audio_gain_val = 512;
unsigned int audio_a2_threshold = 0x800;
unsigned int audio_a2_delay = 10;
unsigned int audio_nicam_delay = 100;

unsigned int audio_atv_ov;
unsigned int audio_atv_ov_flag;

enum AUDIO_SCAN_ID {
	ID_PAL_I = 0,
	ID_PAL_M,
	ID_PAL_DK,
	ID_PAL_BG,
	ID_MAX,
};

static unsigned int mix1_freq;
static int snr_val;
int broad_std_except_pal_m;


int atvdemod_get_snr_val(void)
{
	return snr_val;
}

void amlatvdemod_set_std(int val)
{
	broad_std = val;
}

void set_audio_gain_val(int val)
{
	audio_gain_val = val;
}

void set_video_gain_val(int val)
{
	atv_video_gain = val;
}

void atv_dmd_soft_reset(void)
{
	atv_dmd_wr_long(0x1d, 0x0, 0x1035);/* disable dac */
	atv_dmd_wr_byte(APB_BLOCK_ADDR_SYSTEM_MGT, 0x0, 0x0);
	atv_dmd_wr_byte(APB_BLOCK_ADDR_SYSTEM_MGT, 0x0, 0x1);
	atv_dmd_wr_long(0x1d, 0x0, 0x1037);/* enable dac */
}

void atv_dmd_input_clk_32m(void)
{
	atv_dmd_wr_byte(APB_BLOCK_ADDR_ADC_MGR, 0x2, 0x1);
}

void read_version_register(void)
{
	unsigned long data, Byte1, Byte2, Word;

	pr_info("ATV-DMD read version register\n");
	Byte1 = atv_dmd_rd_byte(APB_BLOCK_ADDR_VERS_REGISTER, 0x0);
	Byte2 = atv_dmd_rd_byte(APB_BLOCK_ADDR_VERS_REGISTER, 0x1);
	Word = atv_dmd_rd_word(APB_BLOCK_ADDR_VERS_REGISTER, 0x2);
	data = atv_dmd_rd_long(APB_BLOCK_ADDR_VERS_REGISTER, 0x0);

	pr_info("atv demod read version register data out %lx\n", data);

	if ((data != 0x516EAB13) || (((Byte1 << 24) | (Byte2 << 16) | Word)
			!= 0x516EAB13))
		pr_info("atv demod read version reg failed\n");
}

void check_communication_interface(void)
{
	unsigned long data_tmp;

	pr_info("ATV-DMD check communication intf\n");
	atv_dmd_wr_long(APB_BLOCK_ADDR_VERS_REGISTER, 0x0, 0xA1B2C3D4);
	atv_dmd_wr_byte(APB_BLOCK_ADDR_VERS_REGISTER, 0x1, 0x34);
	atv_dmd_wr_word(APB_BLOCK_ADDR_VERS_REGISTER, 0x2, 0xBCDE);
	data_tmp = atv_dmd_rd_long(APB_BLOCK_ADDR_VERS_REGISTER, 0x0);
	pr_info("atv demod check communication intf data out %lx\n", data_tmp);

	if (data_tmp != 0xa134bcde)
		pr_info("atv demod check communication intf failed\n");
	atv_dmd_wr_long(APB_BLOCK_ADDR_VERS_REGISTER, 0x0, 0x516EAB13);
}

void power_on_receiver(void)
{
	atv_dmd_wr_byte(APB_BLOCK_ADDR_ADC_MGR, 0x2, 0x11);
}

void atv_dmd_misc(void)
{
	unsigned int reg = 0;

	if (broad_std == AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L) {
		pr_info("broad_std is SECAM_L, no need config misc\n");
		return;
	}

	atv_dmd_wr_byte(APB_BLOCK_ADDR_AGC_PWM, 0x08, 0x38);	/*zhuangwei*/
	/*cpu.write_byte(8'h1A,8'h0E,8'h06);//zhuangwei*/
	/*cpu.write_byte(8'h19,8'h01,8'h7f);//zhuangwei*/
	atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x45, 0x90);	/*zhuangwei*/

	atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x44, 0x5c8808c1);/*zhuangwei*/
	if (amlatvdemod_devp->tuners[amlatvdemod_devp->tuner_cur].cfg.id
			== AM_TUNER_R840 ||
		amlatvdemod_devp->tuners[amlatvdemod_devp->tuner_cur].cfg.id
			== AM_TUNER_R842) {
		/*zhuangwei*/
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x3c, reg_23cf);
		/*guanzhong@20150804a*/
		atv_dmd_wr_byte(APB_BLOCK_ADDR_SIF_STG_2, 0x00, 0x1);
		if (is_meson_txhd_cpu()) {
			atv_dmd_wr_long(APB_BLOCK_ADDR_AGC_PWM,
					0x10, 0x00011020);
			atv_dmd_wr_long(APB_BLOCK_ADDR_AGC_PWM,
					0x08, 0x3d170200);
			atv_dmd_wr_long(APB_BLOCK_ADDR_AGC_PWM,
					0x14, 0x01010855);
			atv_dmd_wr_long(APB_BLOCK_ADDR_AGC_PWM,
					0x1C, 0x03010855);
		} else {
			/* bit[0] = 0 for auto agc */
			/* bit[0] = 1 for foce agc */
			atv_dmd_wr_long(APB_BLOCK_ADDR_AGC_PWM,
					0x08, 0x601b0200);
		}
		/*dezhi@20150610a 0x1a maybe better?!*/
		/* atv_dmd_wr_byte(APB_BLOCK_ADDR_AGC_PWM, 0x09, 0x19); */
	} else {
		/*zhuangwei*/
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x3c, 0x88188832);
		atv_dmd_wr_long(APB_BLOCK_ADDR_AGC_PWM, 0x08, 0x46170200);
	}

	if (amlatvdemod_devp->tuners[amlatvdemod_devp->tuner_cur].cfg.id
			== AM_TUNER_MXL661) {
		/*test in sky*/
		atv_dmd_wr_long(APB_BLOCK_ADDR_DAC_UPS, 0x04, 0xbffa0000);
		atv_dmd_wr_long(APB_BLOCK_ADDR_DAC_UPS, 0x00, 0x6f4000);
		/*guanzhong@20151013 fix nonstd def is:0x0c010301;0x0c020601*/
		atv_dmd_wr_long(APB_BLOCK_ADDR_CARR_RCVY, 0x24, 0xc030901);
	} else {
		/*zhuangwei 0xdafa0000*/
		atv_dmd_wr_long(APB_BLOCK_ADDR_DAC_UPS, 0x04, 0xc8fa0000);
		atv_dmd_wr_long(APB_BLOCK_ADDR_DAC_UPS, 0x00, 0x554000);
	}
	/*zhuangwei*/
	atv_dmd_wr_long(APB_BLOCK_ADDR_DAC_UPS_24M, 0x04, 0xdafa0000);
	atv_dmd_wr_long(APB_BLOCK_ADDR_DAC_UPS_24M, 0x00, 0x4a4000);
	/*atv_dmd_wr_byte(APB_BLOCK_ADDR_DAC_UPS, 0x01, 0x28);//pwd-out gain*/
	/*atv_dmd_wr_byte(APB_BLOCK_ADDR_DAC_UPS, 0x04, 0xc0);//pwd-out offset*/

	aml_audio_valume_gain_set(audio_gain_val);
	/* 20160121 fix audio demodulation over */
	atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0x00, 0x1030501);
	atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0x04, 0x1900000);
	atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x0c, 0x367C0831);
	if (aud_dmd_jilinTV)
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0x00, 0x2030503);
	if (non_std_en == 1) {
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0x00, 0x2030503);
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x44, 0x7c8808c1);
		atv_dmd_wr_long(APB_BLOCK_ADDR_CARR_RCVY, 0x24, 0xc010801);
	} else if (non_std_en == 2) {
		/* fix vsync signal is too weak */
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0x00, 0x1030501);
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x44, 0x8c0808c1);
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x0c, 0x387c0831);
		atv_dmd_wr_long(APB_BLOCK_ADDR_CARR_RCVY, 0x24, 0xc030901);
	} else if (non_std_en == 3) { /* for Hisence */
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0x00, 0x1030501);
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x44, 0x8c0808c1);
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x0c, 0x387c0831);
		atv_dmd_wr_long(APB_BLOCK_ADDR_CARR_RCVY, 0x24, 0xc020901);
	} else {
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0x00,
				extra_input_fil_val);
		if (atv_video_gain)
			atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x44,
					atv_video_gain);
		else
			atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x44, 0xfc0808c1);
		atv_dmd_wr_long(APB_BLOCK_ADDR_CARR_RCVY, 0x24,
			carrier_amplif_val);
	}

	if (audio_atv_ov) {
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
				0x14, 0x8000015);
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
				0x18, 0x7ffff);
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
				0x1c, 0x0f000);
		atvaudio_ctrl_read(&reg);
		if (is_meson_tl1_cpu())
			atvaudio_ctrl_write(reg & (~0x100000));/* bit20 */
		else
			atvaudio_ctrl_write(reg & (~0x3));/* bit[1-0] */
		audio_atv_ov_flag = 1;
	} else {
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
				0x14, 0xf400000);
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
				0x18, 0xc000);
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
				0x1c, 0x1f000);
		atvaudio_ctrl_read(&reg);
		if (is_meson_tl1_cpu())
			atvaudio_ctrl_write(reg | 0x100000);/* bit20 */
		else
			atvaudio_ctrl_write(reg | 0x3);/* bit[1-0] */
		audio_atv_ov_flag = 0;
	}
}

void atv_dmd_ring_filter(bool on)
{
	if (!is_meson_tl1_cpu())
		return;

	if (on) {
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x10, 0x8274bf);
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x14, 0x1d175c);
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x18, 0x2aa526);
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x1c, 0x1d175c);
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x20, 0x2d19e4);
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x24, 0x8274bf);
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x28, 0x1d175c);
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x2c, 0x2aa526);
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x30, 0x1d175c);
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x34, 0x2d19e4);

		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x4c, 0x1);
	} else
		atv_dmd_wr_long(APB_BLOCK_ADDR_GDE_EQUAL, 0x4c, 0x0);

	pr_dbg("%s do atv_dmd_ring_filter %d ...\n", __func__, on);
}

void atv_dmd_non_std_set(bool enable)
{
	unsigned long temp = 0;
	int temp1 = 0;
	int temp2 = 0;
	int vdagc1_diff = 0;
	unsigned int vdagc2_diff = 0;
	static int times;
	static int vdagc1_max;
	static int vdagc1_min = 0xffff;
	static int vdagc2_max = -0x7fff;
	static int vdagc2_min = 0x7fff;

	static int vdagc1_d1;
	static int vdagc2_d1;
	static int sum1;
	static int sum2;
	static unsigned char delay_times;
	static bool has_entry;
	static unsigned char non_std_counter;

	int vpll_lock = 0;
	int line_lock = 0;

	if (!enable || broad_std == AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L) {
		times = 0;
		vdagc1_max = 0;
		vdagc1_min = 0xffff;
		vdagc2_max = -0x7fff;
		vdagc2_min = 0x7fff;
		vdagc1_d1 = 0;
		vdagc2_d1 = 0;
		sum1 = 0;
		sum2 = 0;
		delay_times = 0;
		has_entry = false;
		non_std_counter = 0;

		return;
	}

	/* pll lock */
	vpll_lock = atv_dmd_rd_byte(APB_BLOCK_ADDR_CARR_RCVY, 0x43) & 0x01;
	/* line lock */
	line_lock = atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x4f) & 0x10;

	if (vpll_lock != 0 || line_lock != 0) {
		has_entry = false;
		delay_times = 0;

		/* reset non std params */
		vdagc1_d1 = 0;
		vdagc2_d1 = 0;

		vdagc1_max = 0;
		vdagc1_min = 0xffff;
		vdagc2_max = -0x7fff;
		vdagc2_min = 0x7fff;
		times = 0;
		sum1 = 0;
		sum2 = 0;
		non_std_counter = 0;

		return;
	}

	/* delay total 5s = 100ms (timer) * non_check_delay_times (times). */
	if (delay_times++ <= non_check_delay_times && !has_entry) {
		/* reset non std params */
		vdagc1_d1 = 0;
		vdagc2_d1 = 0;

		vdagc1_max = 0;
		vdagc1_min = 0xffff;
		vdagc2_max = -0x7fff;
		vdagc2_min = 0x7fff;
		times = 0;
		sum1 = 0;
		sum2 = 0;
		non_std_counter = 0;

		return;
	}

	delay_times = 0;
	has_entry = true;

	temp = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x4c);
	temp1 = temp >> 16 & 0xffff;
	if (temp1 < vdagc1_min)
		vdagc1_min = temp1;

	if (temp1 > vdagc1_max)
		vdagc1_max = temp1;

	temp = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x54);
	temp = temp >> 16 & 0xffff;
	if ((temp >> 15) & 1)
		temp2 = temp - 0x10000;
	else
		temp2 = temp;

	if (temp2 < vdagc2_min)
		vdagc2_min = temp2;

	if (temp2 > vdagc2_max)
		vdagc2_max = temp2;

	/*Count the value in total = 100ms (timer) * non_std_times (times). */
	if (times == non_std_times) {
		vdagc1_diff = vdagc1_max - vdagc1_min;
		vdagc2_diff = vdagc2_max - vdagc2_min;

		if ((vdagc1_diff > non_std_thld_4c_h &&
			vdagc2_diff > non_std_thld_54_h &&
			sum1 > sum1_thd_h &&
			sum2 > sum2_thd_h)) {

			non_std_counter++;
			if (non_std_counter == 2) {
				atv_dmd_wr_long(0x09, 0x00, 0x2030503);
				atv_dmd_wr_long(0x0f, 0x44, 0x4d0808c1);
				atv_dmd_wr_long(APB_BLOCK_ADDR_CARR_RCVY,
						0x24, 0x0c010801);

				non_std_counter = 0;

				pr_info("===> atv entry non std setting\n");
			}
		} else if (vdagc1_diff < non_std_thld_4c_l &&
				vdagc2_diff < non_std_thld_54_l &&
				sum1 < sum1_thd_l &&
				sum2 < sum2_thd_l) {
			atv_dmd_wr_long(0x09, 0x00, extra_input_fil_val);
			if (atv_video_gain)
				atv_dmd_wr_long(0x0f, 0x44, atv_video_gain);
			else
				atv_dmd_wr_long(0x0f, 0x44, 0xfc0808c1);
			atv_dmd_wr_long(APB_BLOCK_ADDR_CARR_RCVY, 0x24,
				carrier_amplif_val);

			non_std_counter = 0;

			pr_info("===> atv exit non std setting\n");
		}

		times = 0;

		pr_info("===> vdagc1_diff[0x0f4c]: %d\n", vdagc1_diff);
		pr_info("===> vdagc2_diff[0x0f54]: %d\n", vdagc2_diff);
		pr_info("===> vdagc1_sum1[0x0f4c]: 0x%x\n", sum1);
		pr_info("===> vdagc2_sum2[0x0f4c]: 0x%x\n", sum2);

		vdagc1_max = 0;
		vdagc1_min = 0xffff;
		vdagc2_max = -0x7fff;
		vdagc2_min = 0x7fff;

		sum1 = 0;
		sum2 = 0;
	} else {
		times++;
		sum1 += abs(vdagc1_d1 - temp1);
		sum2 += abs(vdagc2_d1 - temp2);
	}

	vdagc1_d1 = temp1;
	vdagc2_d1 = temp2;
}

/*Broadcast_Standard*/
/*                      0: NTSC*/
/*                      1: NTSC-J*/
/*                      2: PAL-M,*/
/*                      3: PAL-BG*/
/*                      4: DTV*/
/*                      5: SECAM- DK2*/
/*                      6: SECAM -DK3*/
/*                      7: PAL-BG, NICAM*/
/*                      8: PAL-DK-CHINA*/
/*                      9: SECAM-L / SECAM-DK3*/
/*                      10: PAL-I*/
/*                      11: PAL-DK1*/
/*GDE_Curve*/
/*                      0: CURVE-M*/
/*                      1: CURVE-A*/
/*                      2: CURVE-B*/
/*                      3: CURVE-CHINA*/
/*                      4: BYPASS*/
/*sound format 0: MONO;1:NICAM*/
void configure_receiver(int Broadcast_Standard, unsigned int Tuner_IF_Frequency,
			int Tuner_Input_IF_inverted, int GDE_Curve,
			int sound_format)
{
	int tmp_int = 0;
	int mixer1 = 0;
	int mixer3 = 0;
	int mixer3_bypass = 0;
	int cv = 0;
	/*int if_freq = 0;*/

	int i = 0;
	int super_coef0 = 0;
	int super_coef1 = 0;
	int super_coef2 = 0;
	int gp_coeff_1[37] = { 0 };
	int gp_coeff_2[37] = { 0 };
	int gp_cv_g1 = 0;
	int gp_cv_g2 = 0;
	int crvy_reg_1 = 0;
	int crvy_reg_2 = 0;
	int sif_co_mx = 0;
	int sif_fi_mx = 0;
	int sif_ic_bw = 0;
	int sif_bb_bw = 0;
	int sif_deemp = 0;
	int sif_cfg_demod = 0;
	int sif_fm_gain = 0;
	int gd_coeff[6] = { 0 };
	int gd_bypass = 0;

	pr_dbg("ATV-DMD configure receiver register\n");

	if ((Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC) ||
		(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_J) ||
		(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M) ||
		(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK) ||
		(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_BG) ||
		(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_I) ||
		(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_M)) {
		gp_coeff_1[0] = 0x57777;
		gp_coeff_1[1] = 0xdd777;
		gp_coeff_1[2] = 0x7d777;
		gp_coeff_1[3] = 0x75777;
		gp_coeff_1[4] = 0x75777;
		gp_coeff_1[5] = 0x7c777;
		gp_coeff_1[6] = 0x5c777;
		gp_coeff_1[7] = 0x44777;
		gp_coeff_1[8] = 0x54777;
		gp_coeff_1[9] = 0x47d77;
		gp_coeff_1[10] = 0x55d77;
		gp_coeff_1[11] = 0x55577;
		gp_coeff_1[12] = 0x77577;
		gp_coeff_1[13] = 0xc4c77;
		gp_coeff_1[14] = 0xd7d77;
		gp_coeff_1[15] = 0x75477;
		gp_coeff_1[16] = 0xcc477;
		gp_coeff_1[17] = 0x575d7;
		gp_coeff_1[18] = 0xc4c77;
		gp_coeff_1[19] = 0xdd757;
		gp_coeff_1[20] = 0xdd477;
		gp_coeff_1[21] = 0x77dd7;
		gp_coeff_1[22] = 0x5dc77;
		gp_coeff_1[23] = 0x47c47;
		gp_coeff_1[24] = 0x57477;
		gp_coeff_1[25] = 0x5c7c7;
		gp_coeff_1[26] = 0xccc77;
		gp_coeff_1[27] = 0x5ddd5;
		gp_coeff_1[28] = 0x54477;
		gp_coeff_1[29] = 0x7757d;
		gp_coeff_1[30] = 0x755d7;
		gp_coeff_1[31] = 0x47cc4;
		gp_coeff_1[32] = 0x57d57;
		gp_coeff_1[33] = 0x554cc;
		gp_coeff_1[34] = 0x755d7;
		gp_coeff_1[35] = 0x7d3b2;
		gp_coeff_1[36] = 0x73a91;
		gp_coeff_2[0] = 0xd5777;
		gp_coeff_2[1] = 0x77777;
		gp_coeff_2[2] = 0x7c777;
		gp_coeff_2[3] = 0xcc777;
		gp_coeff_2[4] = 0xc7777;
		gp_coeff_2[5] = 0xdd777;
		gp_coeff_2[6] = 0x44c77;
		gp_coeff_2[7] = 0x54c77;
		gp_coeff_2[8] = 0xdd777;
		gp_coeff_2[9] = 0x7c777;
		gp_coeff_2[10] = 0xc7c77;
		gp_coeff_2[11] = 0x75c77;
		gp_coeff_2[12] = 0xdd577;
		gp_coeff_2[13] = 0x44777;
		gp_coeff_2[14] = 0xd5c77;
		gp_coeff_2[15] = 0xdc777;
		gp_coeff_2[16] = 0xd7757;
		gp_coeff_2[17] = 0x4c757;
		gp_coeff_2[18] = 0x7d777;
		gp_coeff_2[19] = 0x75477;
		gp_coeff_2[20] = 0x57547;
		gp_coeff_2[21] = 0xdc747;
		gp_coeff_2[22] = 0x74777;
		gp_coeff_2[23] = 0x75757;
		gp_coeff_2[24] = 0x4cc75;
		gp_coeff_2[25] = 0xd4747;
		gp_coeff_2[26] = 0x7d7d7;
		gp_coeff_2[27] = 0xd5577;
		gp_coeff_2[28] = 0xc4c75;
		gp_coeff_2[29] = 0xcc477;
		gp_coeff_2[30] = 0xdd54c;
		gp_coeff_2[31] = 0x7547d;
		gp_coeff_2[32] = 0x55547;
		gp_coeff_2[33] = 0x5575c;
		gp_coeff_2[34] = 0xd543a;
		gp_coeff_2[35] = 0x57b3a;
		gp_coeff_2[36] = 0x77777;
		gp_cv_g1 = 0x2b062d;
		gp_cv_g2 = 0x40fa2d;
	} else if ((Broadcast_Standard ==
	AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG) ||
	(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK2) ||
	(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK3) ||
	(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG_NICAM)) {
		gp_coeff_1[0] = 0x75777;
		gp_coeff_1[1] = 0x57777;
		gp_coeff_1[2] = 0x7d777;
		gp_coeff_1[3] = 0x75777;
		gp_coeff_1[4] = 0x75777;
		gp_coeff_1[5] = 0x7c777;
		gp_coeff_1[6] = 0x47777;
		gp_coeff_1[7] = 0x74777;
		gp_coeff_1[8] = 0xd5d77;
		gp_coeff_1[9] = 0xc7777;
		gp_coeff_1[10] = 0x77577;
		gp_coeff_1[11] = 0xd7d77;
		gp_coeff_1[12] = 0x75d77;
		gp_coeff_1[13] = 0xdd477;
		gp_coeff_1[14] = 0x77d77;
		gp_coeff_1[15] = 0x75c77;
		gp_coeff_1[16] = 0xc4477;
		gp_coeff_1[17] = 0x4c777;
		gp_coeff_1[18] = 0x5d5d7;
		gp_coeff_1[19] = 0xd7d57;
		gp_coeff_1[20] = 0x47577;
		gp_coeff_1[21] = 0xd7dd7;
		gp_coeff_1[22] = 0xd7d57;
		gp_coeff_1[23] = 0xdd757;
		gp_coeff_1[24] = 0xc75c7;
		gp_coeff_1[25] = 0x7d477;
		gp_coeff_1[26] = 0x5d747;
		gp_coeff_1[27] = 0x7ddc7;
		gp_coeff_1[28] = 0xc4c77;
		gp_coeff_1[29] = 0xd4c75;
		gp_coeff_1[30] = 0xc755d;
		gp_coeff_1[31] = 0x47cc7;
		gp_coeff_1[32] = 0xdd7d4;
		gp_coeff_1[33] = 0x4c75d;
		gp_coeff_1[34] = 0xc7dcc;
		gp_coeff_1[35] = 0xd52a2;
		gp_coeff_1[36] = 0x555a1;
		gp_coeff_2[0] = 0x5d777;
		gp_coeff_2[1] = 0x47777;
		gp_coeff_2[2] = 0x7d777;
		gp_coeff_2[3] = 0xcc777;
		gp_coeff_2[4] = 0xd7777;
		gp_coeff_2[5] = 0x7c777;
		gp_coeff_2[6] = 0x7dd77;
		gp_coeff_2[7] = 0xdd777;
		gp_coeff_2[8] = 0x7c777;
		gp_coeff_2[9] = 0x57c77;
		gp_coeff_2[10] = 0x7c777;
		gp_coeff_2[11] = 0xd5777;
		gp_coeff_2[12] = 0xd7c77;
		gp_coeff_2[13] = 0xdd777;
		gp_coeff_2[14] = 0x77477;
		gp_coeff_2[15] = 0xc7d77;
		gp_coeff_2[16] = 0xc4777;
		gp_coeff_2[17] = 0x57557;
		gp_coeff_2[18] = 0xd5577;
		gp_coeff_2[19] = 0xd5577;
		gp_coeff_2[20] = 0x7d547;
		gp_coeff_2[21] = 0x74757;
		gp_coeff_2[22] = 0xc7577;
		gp_coeff_2[23] = 0xcc7d5;
		gp_coeff_2[24] = 0x4c747;
		gp_coeff_2[25] = 0xddc77;
		gp_coeff_2[26] = 0x54447;
		gp_coeff_2[27] = 0xcc447;
		gp_coeff_2[28] = 0x5755d;
		gp_coeff_2[29] = 0x5dd57;
		gp_coeff_2[30] = 0x54747;
		gp_coeff_2[31] = 0x5747c;
		gp_coeff_2[32] = 0xc77dd;
		gp_coeff_2[33] = 0x47557;
		gp_coeff_2[34] = 0x7a22a;
		gp_coeff_2[35] = 0xc73aa;
		gp_coeff_2[36] = 0x77777;
		gp_cv_g1 = 0x2b2834;
		gp_cv_g2 = 0x3f6c2e;
	} else if ((Broadcast_Standard ==
	AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK) ||
	(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK3) ||
	(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L)) {
		gp_coeff_1[0] = 0x47777;
		gp_coeff_1[1] = 0x77777;
		gp_coeff_1[2] = 0x5d777;
		gp_coeff_1[3] = 0x47777;
		gp_coeff_1[4] = 0x75777;
		gp_coeff_1[5] = 0x5c777;
		gp_coeff_1[6] = 0x57777;
		gp_coeff_1[7] = 0x44777;
		gp_coeff_1[8] = 0x55d77;
		gp_coeff_1[9] = 0x7d777;
		gp_coeff_1[10] = 0x55577;
		gp_coeff_1[11] = 0xd5d77;
		gp_coeff_1[12] = 0xd7d77;
		gp_coeff_1[13] = 0x47477;
		gp_coeff_1[14] = 0xdc777;
		gp_coeff_1[15] = 0x4cc77;
		gp_coeff_1[16] = 0x77d57;
		gp_coeff_1[17] = 0xc4777;
		gp_coeff_1[18] = 0xdd7d7;
		gp_coeff_1[19] = 0x7c757;
		gp_coeff_1[20] = 0xd4477;
		gp_coeff_1[21] = 0x755c7;
		gp_coeff_1[22] = 0x47d57;
		gp_coeff_1[23] = 0xd7c47;
		gp_coeff_1[24] = 0xd4cc7;
		gp_coeff_1[25] = 0x47577;
		gp_coeff_1[26] = 0x5c7d5;
		gp_coeff_1[27] = 0x4c75d;
		gp_coeff_1[28] = 0xd57d7;
		gp_coeff_1[29] = 0x44755;
		gp_coeff_1[30] = 0x7557d;
		gp_coeff_1[31] = 0xc477d;
		gp_coeff_1[32] = 0xd5d44;
		gp_coeff_1[33] = 0xdd77d;
		gp_coeff_1[34] = 0x5d75b;
		gp_coeff_1[35] = 0x74332;
		gp_coeff_1[36] = 0xd4311;
		gp_coeff_2[0] = 0xd7777;
		gp_coeff_2[1] = 0x77777;
		gp_coeff_2[2] = 0xdd777;
		gp_coeff_2[3] = 0xdc777;
		gp_coeff_2[4] = 0xc7777;
		gp_coeff_2[5] = 0xdd777;
		gp_coeff_2[6] = 0x77d77;
		gp_coeff_2[7] = 0x77777;
		gp_coeff_2[8] = 0x55777;
		gp_coeff_2[9] = 0xc7d77;
		gp_coeff_2[10] = 0xd4777;
		gp_coeff_2[11] = 0xc7477;
		gp_coeff_2[12] = 0x7c777;
		gp_coeff_2[13] = 0xd5577;
		gp_coeff_2[14] = 0xdd557;
		gp_coeff_2[15] = 0x47577;
		gp_coeff_2[16] = 0xd7477;
		gp_coeff_2[17] = 0x55747;
		gp_coeff_2[18] = 0xdd757;
		gp_coeff_2[19] = 0xd7477;
		gp_coeff_2[20] = 0x7d7d5;
		gp_coeff_2[21] = 0xddd47;
		gp_coeff_2[22] = 0xdd777;
		gp_coeff_2[23] = 0x575d5;
		gp_coeff_2[24] = 0x47547;
		gp_coeff_2[25] = 0x555c7;
		gp_coeff_2[26] = 0x7d447;
		gp_coeff_2[27] = 0xd7447;
		gp_coeff_2[28] = 0x757dd;
		gp_coeff_2[29] = 0x7dc77;
		gp_coeff_2[30] = 0x54747;
		gp_coeff_2[31] = 0xc743b;
		gp_coeff_2[32] = 0xd7c7c;
		gp_coeff_2[33] = 0xd7557;
		gp_coeff_2[34] = 0x55c7a;
		gp_coeff_2[35] = 0x4cc29;
		gp_coeff_2[36] = 0x77777;
		gp_cv_g1 = 0x20682b;
		gp_cv_g2 = 0x29322f;
	} else if (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I) {
		gp_coeff_1[0] = 0x77777;
		gp_coeff_1[1] = 0x75777;
		gp_coeff_1[2] = 0x7d777;
		gp_coeff_1[3] = 0xd7777;
		gp_coeff_1[4] = 0x74777;
		gp_coeff_1[5] = 0xcc777;
		gp_coeff_1[6] = 0x57777;
		gp_coeff_1[7] = 0x5d577;
		gp_coeff_1[8] = 0x5dd77;
		gp_coeff_1[9] = 0x74777;
		gp_coeff_1[10] = 0x77577;
		gp_coeff_1[11] = 0x77c77;
		gp_coeff_1[12] = 0xdc477;
		gp_coeff_1[13] = 0x5d577;
		gp_coeff_1[14] = 0x575d7;
		gp_coeff_1[15] = 0xc7d57;
		gp_coeff_1[16] = 0x77777;
		gp_coeff_1[17] = 0x557d7;
		gp_coeff_1[18] = 0xc7557;
		gp_coeff_1[19] = 0x75c77;
		gp_coeff_1[20] = 0x477d7;
		gp_coeff_1[21] = 0xcc747;
		gp_coeff_1[22] = 0x47dd7;
		gp_coeff_1[23] = 0x775d7;
		gp_coeff_1[24] = 0x47447;
		gp_coeff_1[25] = 0x75cc7;
		gp_coeff_1[26] = 0xc7777;
		gp_coeff_1[27] = 0xc75d5;
		gp_coeff_1[28] = 0x44c7d;
		gp_coeff_1[29] = 0x74c47;
		gp_coeff_1[30] = 0x47d75;
		gp_coeff_1[31] = 0x7d57c;
		gp_coeff_1[32] = 0xd5dc4;
		gp_coeff_1[33] = 0xdd575;
		gp_coeff_1[34] = 0xdb3bb;
		gp_coeff_1[35] = 0x5c752;
		gp_coeff_1[36] = 0x90880;
		gp_coeff_2[0] = 0x5d777;
		gp_coeff_2[1] = 0xd7777;
		gp_coeff_2[2] = 0x77777;
		gp_coeff_2[3] = 0xd5d77;
		gp_coeff_2[4] = 0xc7777;
		gp_coeff_2[5] = 0xd7777;
		gp_coeff_2[6] = 0xddd77;
		gp_coeff_2[7] = 0x55777;
		gp_coeff_2[8] = 0x57777;
		gp_coeff_2[9] = 0x54c77;
		gp_coeff_2[10] = 0x4c477;
		gp_coeff_2[11] = 0x74777;
		gp_coeff_2[12] = 0xd5d77;
		gp_coeff_2[13] = 0x47757;
		gp_coeff_2[14] = 0x75577;
		gp_coeff_2[15] = 0xc7577;
		gp_coeff_2[16] = 0x4c747;
		gp_coeff_2[17] = 0x7d477;
		gp_coeff_2[18] = 0x7c757;
		gp_coeff_2[19] = 0x55dd5;
		gp_coeff_2[20] = 0x57577;
		gp_coeff_2[21] = 0x44c47;
		gp_coeff_2[22] = 0x5cc75;
		gp_coeff_2[23] = 0x4cc77;
		gp_coeff_2[24] = 0x47547;
		gp_coeff_2[25] = 0x777d5;
		gp_coeff_2[26] = 0xcccc7;
		gp_coeff_2[27] = 0x57447;
		gp_coeff_2[28] = 0xdc757;
		gp_coeff_2[29] = 0x5755c;
		gp_coeff_2[30] = 0x44747;
		gp_coeff_2[31] = 0x5d5dd;
		gp_coeff_2[32] = 0x5747b;
		gp_coeff_2[33] = 0x77557;
		gp_coeff_2[34] = 0xdcb2a;
		gp_coeff_2[35] = 0xd5779;
		gp_coeff_2[36] = 0x77777;
		gp_cv_g1 = 0x72242f;
		gp_cv_g2 = 0x28822a;
	}

	if ((Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC)  ||
		(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_J)) {
		sif_co_mx = 0xb8;
		sif_fi_mx = 0x0;
		sif_ic_bw = 0x1;
		sif_bb_bw = 0x1;
		sif_deemp = 0x1;
		sif_cfg_demod = (sound_format == 0) ? 0x0:0x2;
		sif_fm_gain = 0x4;
	} else if ((Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG)
	|| (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_BG)) {
		sif_co_mx = 0xa6;
		sif_fi_mx = 0x10;
		sif_ic_bw = 0x0;
		sif_bb_bw = 0x0;
		sif_deemp = 0x2;
		sif_cfg_demod = (sound_format == 0) ? 0x0:0x2;
		sif_fm_gain = 0x3;
	} else if (Broadcast_Standard ==
		AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK1) {
		sif_co_mx = 154;
		sif_fi_mx = 240;
		sif_ic_bw = 2;
		sif_bb_bw = 0;
		sif_deemp = 2;
		sif_cfg_demod = (sound_format == 0) ? 0:2;
		sif_fm_gain = 3;
	} else if (Broadcast_Standard ==
		AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK2) {
		sif_co_mx = 150;
		sif_fi_mx = 16;
		sif_ic_bw = 2;
		sif_bb_bw = 0;
		sif_deemp = 2;
		sif_cfg_demod = (sound_format == 0) ? 0:2;
		sif_fm_gain = 3;
	} else if (Broadcast_Standard ==
		AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK3) {
		sif_co_mx = 158;
		sif_fi_mx = 208;
		sif_ic_bw = 3;
		sif_bb_bw = 0;
		sif_deemp = 2;
		sif_cfg_demod = (sound_format == 0) ? 0:2;
		sif_fm_gain = 3;
	} else if ((Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I)
	|| (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_I)) {
		sif_co_mx = 153;
		sif_fi_mx = 56;
		sif_ic_bw = 3;
		sif_bb_bw = 0;
		sif_deemp = 2;
		sif_cfg_demod = (sound_format == 0) ? 0:2;
		sif_fm_gain = 3;
	} else if (Broadcast_Standard ==
		AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG_NICAM) {
		sif_co_mx = 163;
		sif_fi_mx = 40;
		sif_ic_bw = 0;
		sif_bb_bw = 0;
		sif_deemp = 2;
		sif_cfg_demod = (sound_format == 0) ? 0:2;
		sif_fm_gain = 3;
	} else if (Broadcast_Standard ==
		AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L) {
		sif_co_mx = 159;
		sif_fi_mx = 200;
		sif_ic_bw = 3;
		sif_bb_bw = 0;
		sif_deemp = 0;
		sif_cfg_demod = (sound_format == 0) ? 1:2;
		sif_fm_gain = 5;
	} else if ((Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK)
	|| (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK)) {
		sif_co_mx = 159;
		sif_fi_mx = 200;
		sif_ic_bw = 3;
		sif_bb_bw = 0;
		sif_deemp = 2;
		sif_cfg_demod = (sound_format == 0) ? 0:2;
		sif_fm_gain = 3;
	} else if (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M) {
		sif_co_mx = 182;
		sif_fi_mx = 16;
		sif_ic_bw = 1;
		sif_bb_bw = 0;
		sif_deemp = 1;
		sif_cfg_demod = (sound_format == 0) ? 0:2;
		sif_fm_gain = 3;
	}
	sif_fm_gain -= 2;	/*avoid sound overflow@guanzhong*/
	/*FE PATH*/
	pr_dbg("ATV-DMD configure mixer\n");
	if (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_DTV) {
		tmp_int = (Tuner_IF_Frequency/125000);
		if (Tuner_Input_IF_inverted == 0x0)
			mixer1 = -tmp_int;
		else
			mixer1 = tmp_int;

		mixer3 = 0;
		mixer3_bypass = 0;
	} else {
		tmp_int = (Tuner_IF_Frequency/125000);
		pr_dbg("ATV-DMD configure mixer 1\n");

		if (Tuner_Input_IF_inverted == 0x0)
			mixer1 = 0xe8 - tmp_int;
		else
			mixer1 = tmp_int - 0x18;

		pr_dbg("ATV-DMD configure mixer 2\n");
		mixer3 = 0x30;
		mixer3_bypass = 0x1;
	}

	pr_dbg("ATV-DMD configure mixer 3\n");
	atv_dmd_wr_byte(APB_BLOCK_ADDR_MIXER_1, 0x0, mixer1);
	atv_dmd_wr_word(APB_BLOCK_ADDR_MIXER_3, 0x0,
			(((mixer3 & 0xff) << 8) | (mixer3_bypass & 0xff)));

	if (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L)
		atv_dmd_wr_long(APB_BLOCK_ADDR_ADC_SE, 0x0, 0x03180e0f);
	else
		atv_dmd_wr_long(APB_BLOCK_ADDR_ADC_SE, 0x0, 0x03150e0f);
	if (amlatvdemod_devp->tuners[amlatvdemod_devp->tuner_cur].cfg.id
			== AM_TUNER_R840 ||
		amlatvdemod_devp->tuners[amlatvdemod_devp->tuner_cur].cfg.id
			== AM_TUNER_R842) {
		/*config pwm for tuner r840*/
		atv_dmd_wr_byte(APB_BLOCK_ADDR_ADC_SE, 1, 0xf);
	}

	/*GP Filter*/
	pr_dbg("ATV-DMD configure GP_filter\n");
	if (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_DTV) {
		cv = gp_cv_g1;
		atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0x0,
				(0x08000000 | (cv & 0x7fffff)));
		atv_dmd_wr_byte(APB_BLOCK_ADDR_GP_VD_FLT, 0x4, 0x04);
		for (i = 0; i < 9; i = i + 1) {
			/*super_coef = {gp_coeff_1[i*4],gp_coeff_1[i*4+1],*/
			/*gp_coeff_1[i*4+2],gp_coeff_1[i*4+3]};*/
			super_coef0 =
			    (((gp_coeff_1[i * 4 + 2] & 0xfff) << 20) |
			     (gp_coeff_1[i * 4 + 3] & 0xfffff));
			super_coef1 =
			    (((gp_coeff_1[i * 4] & 0xf) << 28) |
			     ((gp_coeff_1[i * 4 + 1] & 0xfffff) << 8) |
			     ((gp_coeff_1[i * 4 + 2] >> 12) & 0xff));
			super_coef2 = ((gp_coeff_1[i * 4] >> 4) & 0xffff);

			/*atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT,*/
			/*0x8,super_coef[79:48]);*/
			/*atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT,*/
			/*0xC,super_coef[47:16]);*/
			/*atv_dmd_wr_word(APB_BLOCK_ADDR_GP_VD_FLT,*/
			/*0x10,super_coef[15:0]);*/
			atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0x8,
					(((super_coef2 & 0xffff) << 16) |
					 ((super_coef1 & 0xffff0000) >> 16)));
			atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0xC,
					(((super_coef1 & 0xffff) << 16) |
					 ((super_coef0 & 0xffff0000) >> 16)));
			atv_dmd_wr_word(APB_BLOCK_ADDR_GP_VD_FLT, 0x10,
					(super_coef0 & 0xffff));
			atv_dmd_wr_byte(APB_BLOCK_ADDR_GP_VD_FLT, 0x05, i);
		}
		/*atv_dmd_wr_long*/
		/*(APB_BLOCK_ADDR_GP_VD_FLT,0x8,{gp_coeff_1[36],12'd0});*/
		atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0x8,
				((gp_coeff_1[36] & 0xfffff) << 12));
		atv_dmd_wr_byte(APB_BLOCK_ADDR_GP_VD_FLT, 0x05, 0x09);

	} else {
		cv = gp_cv_g1 - gp_cv_g2;
		atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0x0, cv & 0x7fffff);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_GP_VD_FLT, 0x4, 0x00);
		for (i = 0; i < 9; i = i + 1) {
			/*super_coef = {gp_coeff_1[i*4],gp_coeff_1[i*4+1],*/
			/*gp_coeff_1[i*4+2],gp_coeff_1[i*4+3]};*/
			super_coef0 =
			    (((gp_coeff_1[i * 4 + 2] & 0xfff) << 20) |
			     (gp_coeff_1[i * 4 + 3] & 0xfffff));
			super_coef1 =
			    (((gp_coeff_1[i * 4] & 0xf) << 28) |
			     ((gp_coeff_1[i * 4 + 1] & 0xfffff) << 8) |
			     ((gp_coeff_1[i * 4 + 2] >> 12) & 0xff));
			super_coef2 = ((gp_coeff_1[i * 4] >> 4) & 0xffff);

			/*atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT,*/
			/*0x8,super_coef[79:48]);*/
			/*atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT,*/
			/*0xC,super_coef[47:16]);*/
			/*atv_dmd_wr_word(APB_BLOCK_ADDR_GP_VD_FLT,*/
			/*0x10,super_coef[15:0]);*/
			atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0x8,
					(((super_coef2 & 0xffff) << 16) |
					 ((super_coef1 & 0xffff0000) >> 16)));
			atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0xC,
					(((super_coef1 & 0xffff) << 16) |
					 ((super_coef0 & 0xffff0000) >> 16)));
			atv_dmd_wr_word(APB_BLOCK_ADDR_GP_VD_FLT, 0x10,
					(super_coef0 & 0xffff));
			atv_dmd_wr_byte(APB_BLOCK_ADDR_GP_VD_FLT, 0x05, i);

			/*
			 * atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT,
			 * 0x8,{gp_coeff_1[36],12'd0});
			 */
		}
		atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0x8,
				((gp_coeff_1[36] & 0xfffff) << 12));
		atv_dmd_wr_byte(APB_BLOCK_ADDR_GP_VD_FLT, 0x05, 9);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_GP_VD_FLT, 0x4, 0x01);

		for (i = 0; i < 9; i = i + 1) {
			/*super_coef = {gp_coeff_2[i*4],gp_coeff_2[i*4+1],*/
			/*gp_coeff_2[i*4+2],gp_coeff_2[i*4+3]};*/
			super_coef0 =
			    (((gp_coeff_2[i * 4 + 2] & 0xfff) << 20) |
			     (gp_coeff_2[i * 4 + 3] & 0xfffff));
			super_coef1 =
			    (((gp_coeff_2[i * 4] & 0xf) << 28) |
			     ((gp_coeff_2[i * 4 + 1] & 0xfffff) << 8) |
			     ((gp_coeff_2[i * 4 + 2] >> 12) & 0xff));
			super_coef2 = ((gp_coeff_2[i * 4] >> 4) & 0xffff);

			atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0x8,
					(((super_coef2 & 0xffff) << 16) |
					 ((super_coef1 & 0xffff0000) >> 16)));
			atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0xC,
					(((super_coef1 & 0xffff) << 16) |
					 ((super_coef0 & 0xffff0000) >> 16)));
			atv_dmd_wr_word(APB_BLOCK_ADDR_GP_VD_FLT, 0x10,
					(super_coef0 & 0xffff));
			atv_dmd_wr_byte(APB_BLOCK_ADDR_GP_VD_FLT, 0x05, i);
		}

		atv_dmd_wr_long(APB_BLOCK_ADDR_GP_VD_FLT, 0x8,
				((gp_coeff_2[36] & 0xfffff) << 12));
		atv_dmd_wr_byte(APB_BLOCK_ADDR_GP_VD_FLT, 0x05, 0x09);
	}

	/*CRVY*/
	pr_dbg("ATV-DMD configure CRVY\n");
	if (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_DTV) {
		crvy_reg_1 = 0xFF;
		crvy_reg_2 = 0x00;
	} else {
		crvy_reg_1 = 0x04;
		crvy_reg_2 = 0x01;
	}

	atv_dmd_wr_byte(APB_BLOCK_ADDR_CARR_RCVY, 0x29, crvy_reg_1);
	pr_dbg("ATV-DMD configure rcvy 2\n");
	pr_dbg("ATV-DMD configure rcvy, crvy_reg_2 = %x\n", crvy_reg_2);
	atv_dmd_wr_byte(APB_BLOCK_ADDR_CARR_RCVY, 0x20, crvy_reg_2);

	/*SOUND SUPPRESS*/
	pr_dbg("ATV-DMD configure sound suppress\n");

	if ((Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_DTV) ||
		(sound_format == 0))
		atv_dmd_wr_byte(APB_BLOCK_ADDR_SIF_VD_IF, 0x02, 0x01);
	else
		atv_dmd_wr_byte(APB_BLOCK_ADDR_SIF_VD_IF, 0x02, 0x00);

	/*SIF*/
	pr_dbg("ATV-DMD configure sif\n");
	if (!(Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_DTV)) {
		atv_dmd_wr_byte(APB_BLOCK_ADDR_SIF_IC_STD, 0x03, sif_ic_bw);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_SIF_IC_STD, 0x01, sif_fi_mx);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_SIF_IC_STD, 0x02, sif_co_mx);

		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0x00,
				(((sif_bb_bw & 0xff) << 24) |
				 ((sif_deemp & 0xff) << 16) | 0x0500 |
				 sif_fm_gain));
		atv_dmd_wr_byte(APB_BLOCK_ADDR_SIF_STG_2, 0x06, sif_cfg_demod);
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0x24,
				(((sif_bb_bw & 0xff) << 24) |
				 0xfffff));
		atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0x1c, 0x1f000);
	}

	if (Broadcast_Standard != AML_ATV_DEMOD_VIDEO_MODE_PROP_DTV) {
		if (sound_format == 0) {
			tmp_int = 0;
			atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_3, 0x00,
					(0x01000000 | (tmp_int & 0xffffff)));
		} else {
			tmp_int = (256 - sif_co_mx) << 13;
			atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_3, 0x00,
					(tmp_int & 0xffffff));
		}
	}

	if (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_DTV) {
		atv_dmd_wr_long(APB_BLOCK_ADDR_IC_AGC, 0x00, 0x02040E0A);
		atv_dmd_wr_word(APB_BLOCK_ADDR_IC_AGC, 0x04, 0x0F0D);
	} else if (sound_format == 0)
		atv_dmd_wr_byte(APB_BLOCK_ADDR_IC_AGC, 0x00, 0x04);
	else if (Broadcast_Standard ==
		AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L) {
		atv_dmd_wr_long(APB_BLOCK_ADDR_IC_AGC, 0x00, 0x0003140A);
		atv_dmd_wr_word(APB_BLOCK_ADDR_IC_AGC, 0x04, 0x1244);
	} else {
		atv_dmd_wr_long(APB_BLOCK_ADDR_IC_AGC, 0x00, 0x00040E0A);
		atv_dmd_wr_word(APB_BLOCK_ADDR_IC_AGC, 0x04, 0x0D68);
	}

	/*VAGC*/
	pr_dbg("ATV-DMD configure vagc\n");
	atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x48, 0x9B6F2C00);
	/*bw select mode*/
	atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x37, 0x1C);
	/*disable prefilter*/

	if (Broadcast_Standard == AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L) {
		atv_dmd_wr_word(APB_BLOCK_ADDR_VDAGC, 0x44, 0x4450);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x46, 0x44);
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x4, 0x3E04FC);
		atv_dmd_wr_word(APB_BLOCK_ADDR_VDAGC, 0x3C, 0x4848);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x3E, 0x48);
	} else {
		atv_dmd_wr_word(APB_BLOCK_ADDR_VDAGC, 0x44, 0xB800);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x46, 0x08);
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x4, 0x3C04FC);
		atv_dmd_wr_word(APB_BLOCK_ADDR_VDAGC, 0x3C, 0x1818);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x3E, 0x10);
	}

	/*tmp_real = $itor(Hz_Freq)/0.23841858; //TODO*/
	/*tmp_int = $rtoi(tmp_real); //TODO*/
	/*tmp_int = Hz_Freq/0.23841858; //TODO*/
	/*tmp_int_2 = ((unsigned long)15625)*10000/23841858;*/
	/*tmp_int_2 = ((unsigned long)Hz_Freq)*10000/23841858;*/
	atv_dmd_wr_word(APB_BLOCK_ADDR_VDAGC, 0x10,
			(freq_hz_cvrt >> 8) & 0xffff);
	atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x12, (freq_hz_cvrt & 0xff));

	/*OUTPUT STAGE*/
	pr_dbg("ATV-DMD configure output stage\n");
	if (Broadcast_Standard != AML_ATV_DEMOD_VIDEO_MODE_PROP_DTV) {
		atv_dmd_wr_byte(APB_BLOCK_ADDR_DAC_UPS, 0x0, 0x00);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_DAC_UPS, 0x1, 0x40);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_DAC_UPS, 0x2, 0x40);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_DAC_UPS, 0x4, 0xFA);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_DAC_UPS, 0x5, 0xFA);
	}

	/*GDE FILTER*/
	pr_dbg("ATV-DMD configure gde filter\n");
	if (GDE_Curve == 0) {
		gd_coeff[0] = 0x020;	/*12'sd32;*/
		gd_coeff[1] = 0xf5f;	/*-12'sd161;*/
		gd_coeff[2] = 0x1fe;	/*12'sd510;*/
		gd_coeff[3] = 0xc0b;	/*-12'sd1013;*/
		gd_coeff[4] = 0x536;	/*12'sd1334;*/
		gd_coeff[5] = 0xb34;	/*-12'sd1228;*/
		gd_bypass = 0x1;
	} else if (GDE_Curve == 1) {
		gd_coeff[0] = 0x8;	/*12'sd8;*/
		gd_coeff[1] = 0xfd5;	/*-12'sd43;*/
		gd_coeff[2] = 0x8d;	/*12'sd141;*/
		gd_coeff[3] = 0xe69;	/*-12'sd407;*/
		gd_coeff[4] = 0x1f1;	/*12'sd497;*/
		gd_coeff[5] = 0xe7e;	/*-12'sd386;*/
		gd_bypass = 0x1;
	} else if (GDE_Curve == 2) {
		gd_coeff[0] = 0x35;	/*12'sd53;*/
		gd_coeff[1] = 0xf41;	/*-12'sd191;*/
		gd_coeff[2] = 0x68;	/*12'sd104;*/
		gd_coeff[3] = 0xea5;	/*-12'sd347;*/
		gd_coeff[4] = 0x322;	/*12'sd802;*/
		gd_coeff[5] = 0x1bb;	/*12'sd443;*/
		gd_bypass = 0x1;
	} else if (GDE_Curve == 3) {
		gd_coeff[0] = 0xf;	/*12'sd15;*/
		gd_coeff[1] = 0xfb5;	/*-12'sd75;*/
		gd_coeff[2] = 0xcc;	/*12'sd204;*/
		gd_coeff[3] = 0xe51;
		gd_coeff[4] = 0x226;	/*12'sd550;*/
		gd_coeff[5] = 0xd02;
		gd_bypass = 0x1;
	} else
		gd_bypass = 0x0;

	if (gd_bypass == 0x0)
		atv_dmd_wr_byte(APB_BLOCK_ADDR_GDE_EQUAL, 0x0D, gd_bypass);
	else {
		for (i = 0; i < 6; i = i + 1)
			atv_dmd_wr_word(APB_BLOCK_ADDR_GDE_EQUAL, i << 1,
					gd_coeff[i]);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_GDE_EQUAL, 0x0C, 0x01);
		atv_dmd_wr_byte(APB_BLOCK_ADDR_GDE_EQUAL, 0x0D, gd_bypass);
	}

	/*PWM*/
	pr_dbg("ATV-DMD configure pwm\n");
	atv_dmd_wr_long(APB_BLOCK_ADDR_AGC_PWM, 0x00, 0x1f40);	/*4KHz*/
	atv_dmd_wr_long(APB_BLOCK_ADDR_AGC_PWM, 0x04, 0xc8);
	/*26 dB dynamic range*/
	atv_dmd_wr_byte(APB_BLOCK_ADDR_AGC_PWM, 0x09, 0xa);
	if (amlatvdemod_devp->tuners[amlatvdemod_devp->tuner_cur].cfg.id
			== AM_TUNER_R840 ||
		amlatvdemod_devp->tuners[amlatvdemod_devp->tuner_cur].cfg.id
			== AM_TUNER_R842) {
		/*config pwm for tuner r840*/
		atv_dmd_wr_long(APB_BLOCK_ADDR_AGC_PWM, 0, 0xc80);
		/* guanzhong for Tuner AGC shock */
		atv_dmd_wr_long(APB_BLOCK_ADDR_AGC_PWM, 0x08, 0x46180200);
		/* atv_dmd_wr_byte(APB_BLOCK_ADDR_ADC_SE,1,0xf);//Kd = 0xf */
	}
}

void retrieve_adc_power(int *adc_level)
{
	*adc_level = atv_dmd_rd_long(APB_BLOCK_ADDR_ADC_SE, 0x0c);
	/*adc_level = adc_level/32768*100;*/
	*adc_level = (*adc_level) * 100 / 32768;
}

void retrieve_vpll_carrier_lock(int *lock)
{
	unsigned int data = 0;

	data = atv_dmd_rd_byte(APB_BLOCK_ADDR_CARR_RCVY, 0x43);
	*lock = (data & 0x1);
}

void retrieve_vpll_carrier_line_lock(int *lock)
{
	unsigned long data = 0;
	int line_lock = 0;
	int line_lock_strong = 0;

	data = atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x4f);
	line_lock = data & 0x10;
	line_lock_strong = data & 0x8;
	/*
	 * pr_dbg("line_lock = 0x%x, line_lock_strong = 0x%x\n",
	 * line_lock, line_lock_strong);
	 */
	*lock = (line_lock | line_lock_strong);
}

void retrieve_vpll_carrier_audio_power(int *power)
{
	unsigned long data = atv_dmd_rd_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02);

	if (!(data & 0x80)) {
		atv_dmd_wr_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02, data | 0x80);

		usleep_range(10000, 10000 + 100);

		data = atv_dmd_rd_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x03);
		*power = data & 0xffff;

		/* keep open for carrier audio power update */
		/*
		 * data = atv_dmd_rd_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02);
		 * atv_dmd_wr_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02,data&(~0x80));
		 */
	} else {
		data = atv_dmd_rd_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x03);
		*power = data & 0xffff;
	}

	pr_audio("retrieve_vpll_carrier_audio_power: %d.\n", *power);
}

int retrieve_vpll_carrier_afc(void)
{
	int data_ret, pll_lock, field_lock, line_lock, line_lock_strong;
	unsigned long vd_lock = 0;

	pll_lock = atv_dmd_rd_byte(APB_BLOCK_ADDR_CARR_RCVY, 0x43)&0x1;
	vd_lock = atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x4f);
	field_lock = vd_lock & 0x4;
	line_lock = vd_lock & 0x10;
	line_lock_strong = vd_lock & 0x8;

	if ((pll_lock == 1) || (line_lock == 0x10)) {
		/*if pll unlock, afc is invalid*/
		pr_dbg("[afc invalid] pll: %d, line: %d, line_strong: %d, field: %d.\n",
				pll_lock, line_lock,
				line_lock_strong, field_lock);

		data_ret = 0xffff;/* 500; */
		return data_ret;
	}

	retrieve_frequency_offset(&data_ret);

	if ((abs(data_ret) < 50) && (line_lock_strong == 0x8) &&
		(field_lock == 0x4)) {
		data_ret = 100;
		return data_ret;
	}

	return data_ret;
}

void set_pll_lpf(unsigned int lock)
{
	atv_dmd_wr_byte(APB_BLOCK_ADDR_CARR_RCVY, 0x24, lock);
}

void retrieve_frequency_offset(int *freq_offset)
{
	unsigned int data_h, data_l, data_exg;
	int data_ret;

	data_h = atv_dmd_rd_byte(APB_BLOCK_ADDR_CARR_RCVY, 0x40);
	data_l = atv_dmd_rd_byte(APB_BLOCK_ADDR_CARR_RCVY, 0x41);
	data_exg = ((data_h & 0x7) << 8) | data_l;
	if (data_h & 0x8) {
		data_ret = (((~data_exg) & 0x7ff) - 1);
		*freq_offset = data_ret * 488 * (-1) / 1000;
	} else {
		data_ret = data_exg * 488 / 1000;
		*freq_offset = data_ret;
	}
}
//EXPORT_SYMBOL(retrieve_frequency_offset);

void retrieve_video_lock(int *lock)
{
	unsigned int data, wlock, slock;

	data = atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x4f);
	wlock = data & 0x10;
	slock = data & 0x8;
	*lock = wlock & slock;
}

void retrieve_field_lock(int *lock)
{
	unsigned int data, field_lock, line_lock;

	data = atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x4f);
	line_lock = data & 0x10;
	field_lock = data & 0x4;
	*lock = ((field_lock == 0) && (line_lock == 0));
}

void retrieve_fh_frequency(int *fh)
{
	unsigned long data1, data2;

	data1 = atv_dmd_rd_word(APB_BLOCK_ADDR_VDAGC, 0x58);
	data2 = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x10);
	data1 = data1 >> 11;
	data2 = data2 >> 3;
	*fh = data1 + data2;
}
/*tune mix to adapt afc*/
void atvdemod_mixer_tune(void)
{
	/* int adc_level,lock,freq_offset,fh; */
	int freq_offset, lock, mix1_freq_cur, delta_mix1_freq;

	/* retrieve_adc_power(&adc_level); */
	/* pr_info("adc_level: 0x%x\n",adc_level); */
	retrieve_vpll_carrier_lock(&lock);
	mix1_freq_cur = atv_dmd_rd_byte(APB_BLOCK_ADDR_MIXER_1, 0x0);
	delta_mix1_freq = abs(mix1_freq_cur - mix1_freq);
	if ((lock&0x1) == 0)
		pr_dbg("%s visual carrier lock:locked\n", __func__);
	else
		pr_dbg("%s visual carrier lock:unlocked\n", __func__);
	/* set_pll_lpf(lock); */
	retrieve_frequency_offset(&freq_offset);
	/* pr_info("visual carrier offset:%d Hz\n",*/
	/* freq_offset*48828125/100000); */
	/* retrieve_video_lock(&lock); */
	if ((lock&0x1) == 1) {
		if (delta_mix1_freq == atvdemod_afc_range)
			atv_dmd_wr_byte(APB_BLOCK_ADDR_MIXER_1, 0x0, mix1_freq);
		else if ((freq_offset >= atvdemod_afc_offset) &&
			(delta_mix1_freq < atvdemod_afc_range))
			atv_dmd_wr_byte(APB_BLOCK_ADDR_MIXER_1, 0x0,
				mix1_freq_cur-1);
		else if ((freq_offset <= (-1)*atvdemod_afc_offset) &&
			(delta_mix1_freq < atvdemod_afc_range-1))
			atv_dmd_wr_byte(APB_BLOCK_ADDR_MIXER_1, 0x0,
				mix1_freq_cur+1);
		/* pr_info("video lock:locked\n"); */
	}
	/* retrieve_fh_frequency(&fh); */
	/* pr_info("horizontal frequency:%d Hz\n",fh*190735/100000); */
}

static enum atvdemod_snr_level_e atvdemod_get_snr_level(void)
{
	unsigned int snr_val = 0, i = 0, snr_d[8] = { 0 };
	enum atvdemod_snr_level_e ret = very_low;
	unsigned long fsnr = 0;

	snr_val = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x50) >> 8;
	fsnr = snr_val;
	for (i = 1; i < 8; i++) {
		snr_d[i] = snr_d[i-1];
		fsnr = fsnr + snr_d[i];
	}
	snr_d[0] = snr_val;
	fsnr = fsnr >> 3;
	if (fsnr < 316)
		ret = high;
	else if (fsnr < 31600)
		ret = ok_plus;
	else if (fsnr < 158000)
		ret = ok_minus;
	else if (fsnr < 700000)
		ret = low;
	else
		ret = very_low;
	return ret;
}

void atvdemod_video_overmodulated(void)
{
	enum atvdemod_snr_level_e snr_level;
	unsigned int vagc_bw_typ, vagc_bw_fast, vpll_kptrack, vpll_kitrack;
	unsigned int agc_register, vfmat_reg, agc_pll_kptrack, agc_pll_kitrack;
	/*1.get current snr*/
	snr_level = atvdemod_get_snr_level();
	/*2.*/
	if (snr_level > very_low) {
		vagc_bw_typ = 0x1818;
		vagc_bw_fast = (snr_level == low) ? 0x18:0x10;
		vpll_kptrack = 0x05;
		vpll_kitrack = 0x0c;
		agc_pll_kptrack = 0x6;
		agc_pll_kitrack = 0xc;
	} else {
		vagc_bw_typ = 0x6f6f;
		vagc_bw_fast = 0x6f;
		vpll_kptrack = 0x06;
		vpll_kitrack = 0x0e;
		agc_pll_kptrack = 0x8;
		agc_pll_kitrack = 0xf;
	}
	atv_dmd_wr_word(APB_BLOCK_ADDR_VDAGC, 0x3c, vagc_bw_typ);
	atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x3e, vagc_bw_fast);
	atv_dmd_wr_byte(APB_BLOCK_ADDR_CARR_RCVY, 0x23, vpll_kptrack);
	atv_dmd_wr_byte(APB_BLOCK_ADDR_CARR_RCVY, 0x24, vpll_kitrack);
	atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x0c,
		((atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x0c) & 0xf0)|
		agc_pll_kptrack));
	atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x0d,
		((atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x0d) & 0xf0)|
		agc_pll_kitrack));
	/*3.*/
	agc_register = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x28);
	if (snr_level < low) {
		agc_register = ((agc_register&0xff80fe03) | (25 << 16) |
			(15 << 2));
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x28, agc_register);
	} else if (snr_level > low) {
		agc_register = ((agc_register&0xff80fe03) | (38 << 16) |
			(30 << 2));
		atv_dmd_wr_long(APB_BLOCK_ADDR_VDAGC, 0x28, agc_register);
	}
	/*4.*/
	if (snr_level < ok_minus)
		atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x47,
			(atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x47) & 0x7f));
	else
		atv_dmd_wr_byte(APB_BLOCK_ADDR_VDAGC, 0x47,
			(atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x47) | 0x80));
	/*5.vformat*/
	if (snr_level < ok_minus) {
		if (atv_dmd_rd_byte(APB_BLOCK_ADDR_VFORMAT, 0xe) != 0xf)
			atv_dmd_wr_byte(APB_BLOCK_ADDR_VFORMAT, 0xe, 0xf);
	} else if (snr_level > ok_minus) {
		vfmat_reg = atv_dmd_rd_word(APB_BLOCK_ADDR_VFORMAT, 0x16);
		if ((vfmat_reg << 4) < 0xf000) {
			if (atv_dmd_rd_byte(APB_BLOCK_ADDR_VFORMAT, 0xe) ==
				0x0f)
				atv_dmd_wr_byte(APB_BLOCK_ADDR_VFORMAT, 0xe,
					0x6);
			else
				atv_dmd_wr_byte(APB_BLOCK_ADDR_VFORMAT, 0xe,
					0xe);
		}
	} else {
		if (atv_dmd_rd_byte(APB_BLOCK_ADDR_VFORMAT, 0xe) == 0x0f)
			atv_dmd_wr_byte(APB_BLOCK_ADDR_VFORMAT, 0xe, 0xe);
		else
			atv_dmd_wr_byte(APB_BLOCK_ADDR_VFORMAT, 0xe, 0x6);
	}
}

static int atvdemod_get_snr(void)
{
	unsigned int snr_val = 0;
	int ret = 0;

	snr_val = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x50) >> 8;
	/* snr_val:900000~0xffffff,ret:5~15 */
	if (snr_val > 900000)
		ret = 15 - (snr_val - 900000)*10/(0xffffff - 900000);
	/* snr_val:158000~900000,ret:15~30 */
	else if (snr_val > 158000)
		ret = 30 - (snr_val - 158000)*15/(900000 - 158000);
	/* snr_val:31600~158000,ret:30~50 */
	else if (snr_val > 31600)
		ret = 50 - (snr_val - 31600)*20/(158000 - 31600);
	/* snr_val:316~31600,ret:50~80 */
	else if (snr_val > 316)
		ret = 80 - (snr_val - 316)*30/(31600 - 316);
	/* snr_val:0~316,ret:80~100 */
	else
		ret = 100 - (316 - snr_val)*20/316;
	return ret;
}

void atvdemod_det_snr_serice(void)
{
	snr_val = atvdemod_get_snr();
}

int atvdemod_clk_init(void)
{
	/* clocks_set_hdtv (); */
	/* 1.set system clock */
#if 0 /* now set pll in tvafe_general.c */
	if (is_meson_txl_cpu()) {
		amlatvdemod_hiu_reg_write(HHI_VDAC_CNTL0, 0x6e0201);
		amlatvdemod_hiu_reg_write(HHI_VDAC_CNTL1, 0x8);
		/* for TXL(T962)  */
		pr_err("%s in TXL\n", __func__);

		/* W_HIU_REG(HHI_ADC_PLL_CNTL,  0x30c54260); */
	#if 0
		W_HIU_REG(HHI_ADC_PLL_CNTL,  0x30f14250);
		W_HIU_REG(HHI_ADC_PLL_CNTL1, 0x22000442);
		W_HIU_REG(HHI_ADC_PLL_CNTL2, 0x5ba00380);
		W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xac6a2114);
		W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x02953004);
		W_HIU_REG(HHI_ADC_PLL_CNTL5, 0x00030a00);
		W_HIU_REG(HHI_ADC_PLL_CNTL6, 0x00005000);
		W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x2c6a2114);
	#else /* get from feijun 2015/07/19 */
		W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
		W_HIU_REG(HHI_ADC_PLL_CNTL, 0x30f14250);
		W_HIU_REG(HHI_ADC_PLL_CNTL1, 0x22000442);
		/*0x5ba00380 from pll;0x5ba00384 clk form crystal*/
		W_HIU_REG(HHI_ADC_PLL_CNTL2, 0x5ba00384);
		W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
		W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x02913004);
		W_HIU_REG(HHI_ADC_PLL_CNTL5, 0x00034a00);
		W_HIU_REG(HHI_ADC_PLL_CNTL6, 0x00005000);
		W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xca6a2110);
		W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
	#endif
		W_HIU_REG(HHI_DADC_CNTL,  0x00102038);
		W_HIU_REG(HHI_DADC_CNTL2, 0x00000406);
		W_HIU_REG(HHI_DADC_CNTL3, 0x00082183);

	} else {
		W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xca2a2110);
		W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x2933800);
		W_HIU_REG(HHI_ADC_PLL_CNTL, 0xe0644220);
		W_HIU_REG(HHI_ADC_PLL_CNTL2, 0x34e0bf84);
		W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a2a2110);

		W_HIU_REG(HHI_ATV_DMD_SYS_CLK_CNTL, 0x80);
		/* TVFE reset */
		W_HIU_BIT(RESET1_REGISTER, 1, 7, 1);
	}
#endif
	W_HIU_REG(HHI_ATV_DMD_SYS_CLK_CNTL, 0x80);

	/* read_version_register(); */

	/*2.set atv demod top page control register*/
	atv_dmd_input_clk_32m();
	atv_dmd_wr_long(APB_BLOCK_ADDR_TOP, ATV_DMD_TOP_CTRL, 0x1037);
	atv_dmd_wr_long(APB_BLOCK_ADDR_TOP, (ATV_DMD_TOP_CTRL1 << 2), 0x1f);

	/*3.configure atv demod*/
	check_communication_interface();
	power_on_receiver();
	pr_err("%s done\n", __func__);

	return 0;
}

int amlfmt_aud_standard(int broad_std)
{
	int std = 0;
	int nicam_lock = 0;
	uint32_t reg_value = 0;
	int vpll_lock = 0, line_lock = 0;

	switch (broad_std) {
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC:
		if (aud_mono_only) {
			std = AUDIO_STANDARD_MONO_M;
			aud_mode = AUDIO_OUTMODE_MONO;
			configure_adec(std);
			adec_soft_reset();
			break;
		}

		std = AUDIO_STANDARD_A2_K;
		configure_adec(std);
		adec_soft_reset();
		msleep(audio_a2_delay);

		retrieve_vpll_carrier_lock(&vpll_lock);
		retrieve_vpll_carrier_line_lock(&line_lock);

		/* maybe need wait */
		reg_value = adec_rd_reg(CARRIER_MAG_REPORT);
		pr_info("\n%s CARRIER_MAG_REPORT: 0x%x\n",
				__func__, (reg_value >> 16) & 0xffff);
		if (((reg_value>>16)&0xffff) > audio_a2_threshold) {
			std = AUDIO_STANDARD_A2_K;
			if (amlatvdemod_devp->soundsys == 0xFF)
				aud_mode = AUDIO_OUTMODE_A2_STEREO;
			else
				aud_mode = amlatvdemod_devp->soundsys;
		} else {
			std = AUDIO_STANDARD_BTSC;
			if (amlatvdemod_devp->soundsys == 0xFF)
				aud_mode = AUDIO_OUTMODE_STEREO;
			else
				aud_mode = amlatvdemod_devp->soundsys;
			configure_adec(std);
			adec_soft_reset();
		}
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_J:
		if (aud_mono_only) {
			std = AUDIO_STANDARD_MONO_M;
			aud_mode = AUDIO_OUTMODE_MONO;
			configure_adec(std);
			adec_soft_reset();
			break;
		}

		std = AUDIO_STANDARD_EIAJ;
		configure_adec(std);
		adec_soft_reset();
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG:
		if (aud_mono_only) {
			std = AUDIO_STANDARD_MONO_BG;
			aud_mode = AUDIO_OUTMODE_MONO;
			configure_adec(std);
			adec_soft_reset();
			break;
		}

		std = AUDIO_STANDARD_NICAM_BG;
		configure_adec(std);
		adec_soft_reset();
		msleep(audio_nicam_delay);
		/* need wait */

		retrieve_vpll_carrier_lock(&vpll_lock);
		retrieve_vpll_carrier_line_lock(&line_lock);

		reg_value = adec_rd_reg(NICAM_LEVEL_REPORT);
		nicam_lock = (reg_value >> 28) & 1;
		pr_info("\n%s NICAM_LEVEL_REPORT: 0x%x\n",
				__func__, reg_value);
		if (nicam_lock) {
			std = AUDIO_STANDARD_NICAM_BG;
			if (amlatvdemod_devp->soundsys == 0xFF)
				aud_mode = AUDIO_OUTMODE_NICAM_STEREO;
			else
				aud_mode = amlatvdemod_devp->soundsys;
		} else {
			std = AUDIO_STANDARD_A2_BG;
			if (amlatvdemod_devp->soundsys == 0xFF)
				aud_mode = AUDIO_OUTMODE_A2_STEREO;
			else
				aud_mode = amlatvdemod_devp->soundsys;
			configure_adec(std);
			adec_soft_reset();
		}
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK:
		if (aud_mono_only) {
			std = AUDIO_STANDARD_MONO_DK;
			aud_mode = AUDIO_OUTMODE_MONO;
			configure_adec(std);
			adec_soft_reset();
			break;
		}

		std = AUDIO_STANDARD_NICAM_DK;
		configure_adec(std);
		adec_soft_reset();
		mdelay(audio_nicam_delay);
		/* need wait */

		retrieve_vpll_carrier_lock(&vpll_lock);
		retrieve_vpll_carrier_line_lock(&line_lock);

		reg_value = adec_rd_reg(NICAM_LEVEL_REPORT);
		nicam_lock = (reg_value >> 28) & 1;
		pr_info("\n%s NICAM_LEVEL_REPORT: 0x%x\n",
				__func__, reg_value);
		if (nicam_lock) {
			std = AUDIO_STANDARD_NICAM_DK;
			if (amlatvdemod_devp->soundsys == 0xFF)
				aud_mode = AUDIO_OUTMODE_NICAM_STEREO;
			else
				aud_mode = amlatvdemod_devp->soundsys;
		} else {
			std = AUDIO_STANDARD_A2_DK1;
			if (amlatvdemod_devp->soundsys == 0xFF)
				aud_mode = AUDIO_OUTMODE_A2_STEREO;
			else
				aud_mode = amlatvdemod_devp->soundsys;
			configure_adec(std);
			adec_soft_reset();
		}
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I:
		if (aud_mono_only) {
			std = AUDIO_STANDARD_MONO_I;
			aud_mode = AUDIO_OUTMODE_MONO;
			configure_adec(std);
			adec_soft_reset();
			break;
		}

		std = AUDIO_STANDARD_NICAM_I;
		configure_adec(std);
		adec_soft_reset();
		msleep(audio_nicam_delay);
		/* need wait */

		retrieve_vpll_carrier_lock(&vpll_lock);
		retrieve_vpll_carrier_line_lock(&line_lock);

		reg_value = adec_rd_reg(NICAM_LEVEL_REPORT);
		nicam_lock = (reg_value >> 28) & 1;
		pr_info("\n%s NICAM_LEVEL_REPORT: 0x%x\n",
				__func__, reg_value);
		if (nicam_lock) {
			std = AUDIO_STANDARD_NICAM_I;
			if (amlatvdemod_devp->soundsys == 0xFF)
				aud_mode = AUDIO_OUTMODE_NICAM_STEREO;
			else
				aud_mode = amlatvdemod_devp->soundsys;
		} else {
			std = AUDIO_STANDARD_MONO_I;
			aud_mode = AUDIO_OUTMODE_MONO;
			configure_adec(std);
			adec_soft_reset();
		}
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L:
		if (aud_mono_only) {
			std = AUDIO_STANDARD_MONO_L;
			aud_mode = AUDIO_OUTMODE_MONO;
			configure_adec(std);
			adec_soft_reset();
			break;
		}

		std = AUDIO_STANDARD_NICAM_L;
		configure_adec(std);
		adec_soft_reset();
		msleep(audio_nicam_delay);
		/* need wait */

		retrieve_vpll_carrier_lock(&vpll_lock);
		retrieve_vpll_carrier_line_lock(&line_lock);

		reg_value = adec_rd_reg(NICAM_LEVEL_REPORT);
		nicam_lock = (reg_value >> 28) & 1;
		pr_info("\n%s NICAM_LEVEL_REPORT: 0x%x\n",
				__func__, reg_value);
		if (nicam_lock) {
			std = AUDIO_STANDARD_NICAM_L;
			if (amlatvdemod_devp->soundsys == 0xFF)
				aud_mode = AUDIO_OUTMODE_NICAM_STEREO;
			else
				aud_mode = amlatvdemod_devp->soundsys;
		} else {
			std = AUDIO_STANDARD_MONO_L;
			aud_mode = AUDIO_OUTMODE_MONO;
			configure_adec(std);
			adec_soft_reset();
		}
		break;
	}

	if ((vpll_lock == 0) && (line_lock == 0)) {
		aud_reinit = false;
	} else {
		aud_reinit = true;
		pr_err("pll lock: 0x%x, line lock: 0x%x.\n",
				vpll_lock, line_lock);
	}

	pr_err("%s detect aud std:%d, aud_reinit:%d.\n", __func__,
			std, aud_reinit);
	return std;
}

int atvauddemod_init(void)
{
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
		if (audio_thd_en)
			audio_thd_init();

		if (aud_auto)
			aud_std = amlfmt_aud_standard(broad_std);
		else {
			configure_adec(aud_std);
			adec_soft_reset();
		}
		set_outputmode_status_init();
		set_outputmode(aud_std, aud_mode);
	} else {
		/* for non support adec */
		aud_std = 0;
		aud_mode = AUDIO_OUTMODE_MONO;
	}

	return 0;
}

void atvauddemod_set_outputmode(void)
{
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu() || is_meson_tl1_cpu()) {
		if (aud_reinit) {
			/* before maybe need check afc status */
			atvauddemod_init();
		} else
			set_outputmode(aud_std, aud_mode);
	}
}

int atvdemod_init(bool on)
{
	/* 1.set system clock when atv enter*/

	pr_dbg("%s do configure_receiver ...\n", __func__);
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu() || is_meson_tl1_cpu())
		sound_format = 1;
	configure_receiver(broad_std, if_freq, if_inv, gde_curve, sound_format);
	pr_dbg("%s do atv_dmd_misc ...\n", __func__);
	atv_dmd_misc();

	if (on && (broad_std == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_M ||
		broad_std == AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC))
		atv_dmd_ring_filter(true);
	else
		atv_dmd_ring_filter(false);

	pr_dbg("%s do atv_dmd_soft_reset ...\n", __func__);
	/*4.software reset*/
	atv_dmd_soft_reset();

	/* check the PLL, line lock status, don't need to check. */
	/*	while (!all_lock) {
	 *	data32 = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC,0x13<<2);
	 *	if ((data32 & 0x1c) == 0x0) {
	 *		all_lock = 1;
	 *	}
	 *	delay_us(400);
	 * }
	 */

	mix1_freq = atv_dmd_rd_byte(APB_BLOCK_ADDR_MIXER_1, 0x0);

	pr_info("%s done\n", __func__);

	return 0;
}

void atvdemod_uninit(void)
{
	/* mute atv audio output */
	if (is_meson_txl_cpu())
		atv_dmd_wr_long(APB_BLOCK_ADDR_MONO_PROC, 0x50, 0);

	atv_dmd_non_std_set(false);
}

void atv_dmd_set_std(void)
{
	v4l2_std_id ptstd = amlatvdemod_devp->std;
	int tuner_index = amlatvdemod_devp->tuner_cur;
	int tuner_id = amlatvdemod_devp->tuners[tuner_index].cfg.id;

	/* set broad standard of tuner*/
	if (((ptstd & V4L2_COLOR_STD_PAL)
			|| (ptstd & V4L2_COLOR_STD_SECAM)
			|| (ptstd & V4L2_COLOR_STD_NTSC))
			&& ((ptstd & V4L2_STD_B) || (ptstd & V4L2_STD_G))) {
		amlatvdemod_devp->fre_offset = 2250000;
		freq_hz_cvrt = AML_ATV_DEMOD_FREQ_50HZ_VERT;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG;
		if_freq = 3250000;
		gde_curve = 2;
	} else if (((ptstd & V4L2_COLOR_STD_PAL)
			|| (ptstd & V4L2_COLOR_STD_SECAM)
			|| (ptstd & V4L2_COLOR_STD_NTSC))
			&& (ptstd & V4L2_STD_DK)) {
		amlatvdemod_devp->fre_offset = 2250000;
		freq_hz_cvrt = AML_ATV_DEMOD_FREQ_50HZ_VERT;
		if_freq = 3250000;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK;
		gde_curve = 3;
	} else if (((ptstd & V4L2_COLOR_STD_PAL)
			|| (ptstd & V4L2_COLOR_STD_SECAM))
			&& ((ptstd & V4L2_STD_PAL_M)
			|| (ptstd & V4L2_STD_NTSC_M))) {
		amlatvdemod_devp->fre_offset = 2250000;
		freq_hz_cvrt = AML_ATV_DEMOD_FREQ_60HZ_VERT;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M;
		if_freq = 4250000;
		gde_curve = 0;
	} else if (((ptstd & V4L2_COLOR_STD_PAL) ||
			(ptstd & V4L2_COLOR_STD_NTSC))
			&& (ptstd & V4L2_STD_PAL_I)) {
		amlatvdemod_devp->fre_offset = 2750000;
		freq_hz_cvrt = AML_ATV_DEMOD_FREQ_50HZ_VERT;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I;
		if_freq = 3250000;
		gde_curve = 4;
	} else if ((ptstd & V4L2_COLOR_STD_NTSC) && ((ptstd & V4L2_STD_NTSC_M)
			|| (ptstd & V4L2_STD_PAL_M))) {
		amlatvdemod_devp->fre_offset = 1750000;
		freq_hz_cvrt = AML_ATV_DEMOD_FREQ_60HZ_VERT;
		if_freq = 4250000;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC;
		gde_curve = 0;
	} else if ((ptstd & V4L2_COLOR_STD_NTSC) && (ptstd & V4L2_STD_DK)) {
		amlatvdemod_devp->fre_offset = 1750000;
		freq_hz_cvrt = AML_ATV_DEMOD_FREQ_60HZ_VERT;
		if_freq = 4250000;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK;
		gde_curve = 0;
	} else if ((ptstd & V4L2_COLOR_STD_NTSC) && (ptstd & V4L2_STD_BG)) {
		amlatvdemod_devp->fre_offset = 1750000;
		freq_hz_cvrt = AML_ATV_DEMOD_FREQ_60HZ_VERT;
		if_freq = 4250000;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_BG;
		gde_curve = 0;
	} else if ((ptstd & V4L2_COLOR_STD_NTSC) &&
		(ptstd & V4L2_STD_NTSC_M_JP)) {
		amlatvdemod_devp->fre_offset = 1750000;
		freq_hz_cvrt = AML_ATV_DEMOD_FREQ_50HZ_VERT;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_J;
		if_freq = 4250000;
		gde_curve = 0;
	} else if ((ptstd & V4L2_COLOR_STD_PAL) && (ptstd & V4L2_STD_PAL_I)) {
		amlatvdemod_devp->fre_offset = 2750000;
		freq_hz_cvrt = AML_ATV_DEMOD_FREQ_50HZ_VERT;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I;
		if_freq = 3250000;
		gde_curve = 4;
	} else if (ptstd & (V4L2_STD_SECAM_L | V4L2_STD_SECAM_LC)) {
		amlatvdemod_devp->fre_offset = 2750000;
		freq_hz_cvrt = AML_ATV_DEMOD_FREQ_50HZ_VERT;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L;
		gde_curve = 4;
	}

	/* Tuner returns the if and signal inverted states */
	if ((tuner_id == AM_TUNER_R840) ||
		(tuner_id == AM_TUNER_R842) ||
		(tuner_id == AM_TUNER_SI2151) ||
		(tuner_id == AM_TUNER_SI2159) ||
		(tuner_id == AM_TUNER_MXL661)) {
		if_freq = amlatvdemod_devp->if_freq;
		if_inv = amlatvdemod_devp->if_inv;
	}

	pr_dbg("[%s] set broad_std %d, hz_cvrt 0x%x, offset %d.\n",
			__func__, broad_std, freq_hz_cvrt,
			amlatvdemod_devp->fre_offset);

	pr_dbg("[%s] set std color %s, audio type %s.\n",
		__func__,
		v4l2_std_to_str((0xff000000 & amlatvdemod_devp->std)),
		v4l2_std_to_str((0xffffff & amlatvdemod_devp->std)));

	pr_dbg("[%s] set if_freq %d, if_inv %d.\n",
		__func__, amlatvdemod_devp->if_freq,
		amlatvdemod_devp->if_inv);
}

int aml_audiomode_autodet(struct v4l2_frontend *v4l2_fe)
{
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct analog_parameters params;

	unsigned long carrier_power = 0;
	unsigned long carrier_power_max = 0;
	unsigned long carrier_power_average_max = 0;
	unsigned long carrier_power_average[4] = {0};
	unsigned long temp_data = 0;
	int lock = 0, line_lock = 0;
	int broad_std_final = 0;
	int num = 0, i = 0, final_id = 0;
	int delay_ms = 10, delay_ms_default = 10;
	int cur_std = ID_PAL_DK;
	bool secam_signal = false;
	bool ntsc_signal = false;
	bool pal_signal = false;
	bool has_audio = false;

	switch (broad_std) {
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M:
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M;
		pal_signal = true;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_I:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_BG:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_M:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC:
		ntsc_signal = true;
		broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK2:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK3:
		if (!(p->std & V4L2_COLOR_STD_SECAM) ||
				!(p->std & V4L2_STD_SECAM_L)) {
			secam_signal = true;
			broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M;
		} else {
			broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L;
			atvdemod_init(false);
			temp_data = atv_dmd_rd_reg(APB_BLOCK_ADDR_SIF_STG_2,
					0x02);

			temp_data = temp_data & (~0x80); /* 0xbf; */

			atv_dmd_wr_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02,
					temp_data);
			/* pr_err("%s, SECAM ,audio set SECAM_L\n",
			 *  __func__);
			 */
			return broad_std;
		}
		break;

	default:
		pr_err("unsupport broadcast_standard!!!\n");
		temp_data = atv_dmd_rd_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02);
		temp_data = temp_data & (~0x80); /* 0xbf; */
		atv_dmd_wr_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02, temp_data);
		return broad_std;
	}
	/* ----------------read carrier_power--------------------- */
	/* SIF_STG_2[0x09],address 0x03 */
	while (1) {
		if (num >= 4) {
			temp_data =
				atv_dmd_rd_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02);
			temp_data = temp_data & (~0x80);
			atv_dmd_wr_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02,
					temp_data);
			carrier_power_max = carrier_power_average[0];
			for (i = 0; i < ID_MAX; i++) {
				if (carrier_power_max
					< carrier_power_average[i]) {
					carrier_power_max =
						carrier_power_average[i];
					final_id = i;
				}
			}

			switch (final_id) {
			case ID_PAL_I:
				broad_std_final =
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I;
				break;
			case ID_PAL_BG:
				broad_std_final =
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG;
				break;
			case ID_PAL_M:
				broad_std_final =
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M;
				break;
			case ID_PAL_DK:
				broad_std_final =
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK;
				break;
			}

			carrier_power_average_max = carrier_power_max;
			broad_std = broad_std_final;
			pr_err("%s:broad_std:%d,carrier_power_average_max:%lu\n",
				__func__, broad_std, carrier_power_average_max);

			if (carrier_power_average_max < 150) {
				pr_err("%s,carrier too low error\n", __func__);
				if (secam_signal) {
					broad_std =
					AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L;
					pr_err("%s,set broad_std to SECAM_L\n",
							__func__);
				}
			}

			if (broad_std == AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M
					&& pal_signal) {
				/*the max except palm*/
				carrier_power_average[final_id] = 0;
				final_id = 0;
				carrier_power_max = carrier_power_average[0];
				for (i = 0; i < ID_MAX; i++) {
					if (carrier_power_max
						< carrier_power_average[i]) {
						carrier_power_max =
						carrier_power_average[i];
						final_id = i;
					}
				}

				switch (final_id) {
				case ID_PAL_I:
					broad_std_except_pal_m =
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I;
					break;
				case ID_PAL_BG:
					broad_std_except_pal_m =
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG;
					break;
				case ID_PAL_DK:
					broad_std_except_pal_m =
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK;
					break;
				}
				/* pal signal and pal-m power max,
				 * so set to second max std.
				 */
				broad_std = broad_std_except_pal_m;
				pr_err("%s:pal signal and pal-m power max, set broad_std:%d\n",
							__func__, broad_std);
			}

			p->std = V4L2_COLOR_STD_PAL;
			switch (broad_std) {
			case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK:
				p->std |= V4L2_STD_PAL_DK;
				p->audmode = V4L2_STD_PAL_DK;
				break;
			case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I:
				p->std |= V4L2_STD_PAL_I;
				p->audmode = V4L2_STD_PAL_I;
				break;
			case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG:
				p->std |= V4L2_STD_PAL_BG;
				p->audmode = V4L2_STD_PAL_BG;
				break;
			case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M:
				p->std |= V4L2_STD_PAL_M;
				p->audmode = V4L2_STD_PAL_M;
				break;
			default:
				p->std |= V4L2_STD_PAL_DK;
				p->audmode = V4L2_STD_PAL_DK;
			}

			p->frequency += 1;
			params.frequency = p->frequency;
			params.mode = p->afc_range;
			params.audmode = p->audmode;
			params.std = p->std;

			fe->ops.analog_ops.set_params(fe, &params);

			return broad_std;
		}

		switch (broad_std) {
		case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK:
			broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I;
			cur_std = ID_PAL_I;

			p->std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_I;
			p->frequency += 1;
			p->audmode = V4L2_STD_PAL_I;

			delay_ms = delay_ms_default;
			break;
		case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I:
			broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG;
			cur_std = ID_PAL_BG;

			p->std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_BG;
			p->frequency += 1;
			p->audmode = V4L2_STD_PAL_BG;

			delay_ms = delay_ms_default;
			break;
		case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG:
			broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M;
			cur_std = ID_PAL_M;

			if (!ntsc_signal) {
				p->std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_M;
				p->frequency += 1;
				p->audmode = V4L2_STD_PAL_M;
			} else {
				p->std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
				p->frequency += 1;
				p->audmode = V4L2_STD_NTSC_M;
			}

			delay_ms = delay_ms_default;
			break;
		case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M:
			broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK;
			cur_std = ID_PAL_DK;

			p->std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_DK;
			p->frequency += 1;
			p->audmode = V4L2_STD_PAL_DK;

			delay_ms = delay_ms_default;
			break;

		default:
			pr_err("unsupport broadcast_standard!!!\n");
			break;
		}

		params.frequency = p->frequency;
		params.mode = p->afc_range;
		params.audmode = p->audmode;
		params.std = p->std;
		fe->ops.analog_ops.set_params(fe, &params);

		/* enable audio detect function */
		temp_data = atv_dmd_rd_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02);
		temp_data = temp_data | 0x87;/* 0x40 */
		atv_dmd_wr_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02, temp_data);

		usleep_range(delay_ms * 1000, delay_ms * 1000 + 100);

		/* ----------------judgment signal state--------------------- */
		i = 4;
		has_audio = false;
		while (i--) {
			retrieve_vpll_carrier_lock(&lock);
			line_lock = atv_dmd_rd_byte(APB_BLOCK_ADDR_VDAGC, 0x4f);
			if (lock == 0 && (line_lock & 0x10) == 0) {
				has_audio = true;
				break;
			}

			usleep_range(6000, 9000);
		}
		/* ----------------read carrier_power--------------------- */
		if (has_audio) {
			for (i = 0; i < 100; i++) {
				carrier_power = atv_dmd_rd_reg(
					APB_BLOCK_ADDR_SIF_STG_2, 0x03);
				carrier_power_max += carrier_power;
			}
			carrier_power = carrier_power_max/i;
		} else {
			carrier_power = 0;
			pr_err("[%s] pll and line unlock.\n", __func__);
		}

		carrier_power_max = 0;
		pr_err("[%s] [num:%d] [broad_std:%d] audio carrier power: %lu. @@@@@@@@@@\n",
			__func__, num, broad_std, carrier_power);
		carrier_power_average[cur_std] += carrier_power;
		num++;
	}

	return broad_std;
}

void aml_audio_valume_gain_set(unsigned int audio_gain)
{
	unsigned long audio_gain_data = 0, temp_data = 0;

	if (audio_gain > 0xfff) {
		pr_err("Error: atv in gain max 7.998, min 0.002! gain = value/512\n");
		pr_err("value (0~0xfff)\n");
		return;
	}
	audio_gain_data = audio_gain & 0xfff;
	temp_data = atv_dmd_rd_word(APB_BLOCK_ADDR_MONO_PROC, 0x52);
	temp_data = (temp_data & 0xf000) | audio_gain_data;
	atv_dmd_wr_word(APB_BLOCK_ADDR_MONO_PROC, 0x52, temp_data);
}

unsigned int aml_audio_valume_gain_get(void)
{
	unsigned long audio_gain_data = 0;

	audio_gain_data = atv_dmd_rd_word(APB_BLOCK_ADDR_MONO_PROC, 0x52);
	audio_gain_data = audio_gain_data & 0xfff;
	return audio_gain_data;
}

void aml_fix_PWM_adjust(int enable)
{
	unsigned long temp_data = 0;
	/*
	 * temp_data = atv_dmd_rd_byte(APB_BLOCK_ADDR_AGC_PWM, 0x08);
	 * temp_data = temp_data | 0x01;
	 * atv_dmd_wr_byte(APB_BLOCK_ADDR_AGC_PWM, 0x08, temp_data);
	 */
	temp_data = atv_dmd_rd_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02);
	if (enable)
		temp_data = temp_data & ~((0x3)<<8);
	else
		temp_data = temp_data & ~((0x1)<<9);

	atv_dmd_wr_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02, temp_data);
	if (enable) {
		temp_data = temp_data | ((0x3)<<8);
		atv_dmd_wr_reg(APB_BLOCK_ADDR_SIF_STG_2, 0x02, temp_data);
	}
}

void aml_audio_overmodulation(int enable)
{
	unsigned long tmp_v = 0;
	unsigned long tmp_v1 = 0;
	unsigned int reg = 0;
	u32 Broadcast_Standard = broad_std;

	/* False entry on weak signal */
	if (atvdemod_get_snr() < snr_threshold)
		return;

	if (enable && Broadcast_Standard ==
		AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK) {
		tmp_v = atv_dmd_rd_long(APB_BLOCK_ADDR_SIF_STG_2, 0x28);
		tmp_v = tmp_v&0xffff;
		if (tmp_v > 0x10 && audio_atv_ov_flag == 0) {
			tmp_v1 =
				atv_dmd_rd_long(APB_BLOCK_ADDR_SIF_STG_2, 0);
			tmp_v1 = (tmp_v1&0xffffff)|(1<<24);
			atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0, tmp_v1);
			atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
					0x14, 0x8000015);
			atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
					0x18, 0x7ffff);
			atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
					0x1c, 0x0f000);
			atvaudio_ctrl_read(&reg);
			if (is_meson_tl1_cpu()) /* bit20 */
				atvaudio_ctrl_write(reg & (~0x100000));
			else
				atvaudio_ctrl_write(reg & (~0x3));/* bit[1-0] */
			/* audio_atv_ov_flag = 1;*/ /* Enter and hold */
			pr_info("tmp_v[0x%lx] > 0x10 && audio_atv_ov_flag == 0.\n",
					tmp_v);
		} else if (tmp_v <= 0x10 && audio_atv_ov_flag == 1) {
			tmp_v1 = atv_dmd_rd_long(APB_BLOCK_ADDR_SIF_STG_2, 0);
			tmp_v1 = (tmp_v1&0xffffff)|(0<<24);
			atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2, 0, tmp_v1);
			atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
					0x14, 0xf400000);
			atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
					0x18, 0xc000);
			atv_dmd_wr_long(APB_BLOCK_ADDR_SIF_STG_2,
					0x1c, 0x1f000);
			atvaudio_ctrl_read(&reg);
			if (is_meson_tl1_cpu()) /* bit20 */
				atvaudio_ctrl_write(reg | 0x100000);
			else
				atvaudio_ctrl_write(reg | 0x3);/* bit[1-0] */
			audio_atv_ov_flag = 0;
			pr_info("tmp_v[0x%lx] <= 0x10 && audio_atv_ov_flag == 1.\n",
					tmp_v);
		}
	}
}
