/*
 * sound/soc/amlogic/auge/earc_hw.c
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/types.h>
#include <linux/kernel.h>

#include "earc_hw.h"


void earcrx_cmdc_init(void)
{
	/* set irq mask */
	earcrx_top_write(EARCRX_CMDC_INT_MASK,
		(1 << 15) |  /* idle2_int */
		(1 << 14) |  /* idle1_int */
		(1 << 13) |  /* disc2_int */
		(1 << 12) |  /* disc1_int */
		(1 << 11) |  /* earc_int */
		(1 << 10) |  /* hb_status_int */
		(1 <<  9) |  /* losthb_int */
		(1 <<  8) |  /* timeout_int */
		(0 <<  7) |  /* status_ch_int */
		(0 <<  6) |  /* int_rec_invalid_id */
		(0 <<  5) |  /* int_rec_invalid_offset */
		(0 <<  4) |  /* int_rec_unexp */
		(0 <<  3) |  /* int_rec_ecc_err */
		(0 <<  2) |  /* int_rec_parity_err */
		(0 <<  1) |  /* int_recv_packet */
		(0 <<  0)	 /* int_rec_time_out */
		);

	earcrx_top_write(EARCRX_ANA_CTRL0,
		0x1  << 31 | /* earcrx_en_d2a */
		0x10 << 24 | /* earcrx_cmdcrx_reftrim */
		0x8  << 20 | /* earcrx_idr_trim */
		0x10 << 15 | /* earcrx_rterm_trim */
		0x4  << 12 | /* earcrx_cmdctx_ack_hystrim */
		0x10 << 7  | /* earcrx_cmdctx_ack_reftrim */
		0x1  << 4  | /* earcrx_cmdcrx_rcfilter_sel */
		0x4  << 0    /* earcrx_cmdcrx_hystrim */
		);

	earcrx_top_write(EARCRX_PLL_CTRL3,
		0x2 << 20 | /* earcrx_pll_bias_adj */
		0x4 << 16 | /* earcrx_pll_rou */
		0x1 << 13   /* earcrx_pll_dco_sdm_e */
		);

	earcrx_top_write(EARCRX_PLL_CTRL0,
		0x1 << 28 | /* earcrx_pll_en */
		0x1 << 23 | /* earcrx_pll_dmacrx_sqout_rstn_sel */
		0x1 << 10   /* earcrx_pll_n */
		);
}

void earcrx_cmdc_arc_connect(bool init)
{
	if (init)
		earcrx_cmdc_update_bits(
			EARC_RX_CMDC_VSM_CTRL0,
			0x7 << 25,
			0x1 << 27 | /* arc_initiated */
			0x0 << 26 | /* arc_terminated */
			0x1 << 25   /* arc_enable */
			);
	else
		earcrx_cmdc_update_bits(
			EARC_RX_CMDC_VSM_CTRL0,
			0x7 << 25,
			0x0 << 27 | /* arc_initiated */
			0x1 << 26 | /* arc_terminated */
			0x0 << 25   /* arc_enable */
			);
}

void earcrx_cmdc_hpd_detect(bool st)
{
	if (st) {
		earcrx_cmdc_update_bits(
			EARC_RX_CMDC_VSM_CTRL0,
			0x1 << 19,
			0x1 << 19	/* comma_cnt_rst */
			);

		earcrx_cmdc_update_bits(
			EARC_RX_CMDC_VSM_CTRL0,
			0x1 << 19 | 0xff << 0,
			0x1 << 19 | /* comma_cnt_rst */
			0xff << 0
			);
	} else {
		/* soft reset */
		earcrx_cmdc_update_bits(
			EARC_RX_CMDC_TOP_CTRL1,
			0xf << 1,
			0xf << 1);
		earcrx_cmdc_update_bits(
			EARC_RX_CMDC_TOP_CTRL1,
			0xf << 1,
			0x0 << 1);
	}
}

void earcrx_dmac_init(void)
{
	earcrx_top_write(EARCRX_DMAC_INT_MASK,
		(0x0 << 17) | /* earcrx_ana_rst c_new_format_set */
		(0x0 << 16) | /* earcrx_ana_rst c_earcrx_div2_hold_set */
		(0x0 << 15) | /* earcrx_err_correct c_bcherr_int_set */
		(0x0 << 14) | /* earcrx_err_correct r_afifo_overflow_set */
		(0x0 << 13) | /* earcrx_err_correct r_fifo_overflow_set */
		(0x0 << 12) | /* earcrx_user_bit_check r_fifo_overflow */
		(0x0 << 11) | /* earcrx_user_bit_check c_fifo_thd_pass */
		(0x0 << 10) | /* earcrx_user_bit_check c_u_pk_lost_int_set */
		(0x0 << 9)	| /* arcrx_user_bit_check c_iu_pk_end */
		(0x0 << 8)	| /* arcrx_biphase_decode c_chst_mute_clr */
		(0x1 << 7)	| /* arcrx_biphase_decode c_find_papb */
		(0x1 << 6)	| /* arcrx_biphase_decode c_valid_change */
		(0x1 << 5)	| /* arcrx_biphase_decode c_find_nonpcm2pcm */
		(0x1 << 4)	| /* arcrx_biphase_decode c_pcpd_change */
		(0x1 << 3)	| /* arcrx_biphase_decode c_ch_status_change */
		(0x1 << 2)	| /* arcrx_biphase_decode sample_mod_change */
		(0x1 << 1)	| /* arcrx_biphase_decode r_parity_err */
		(0x0 << 0)	  /* arcrx_dmac_sync afifo_overflow */
		);

	earcrx_dmac_write(EARCRX_DMAC_SYNC_CTRL0,
		(1 << 16)	|	 /* reg_ana_buf_data_sel_en */
		(3 << 12)	|	 /* reg_ana_buf_data_sel */
		(7 << 8)	|	 /* reg_ana_clr_cnt */
		(7 << 4)		 /* reg_ana_set_cnt */
		);
	earcrx_dmac_write(EARCRX_DMAC_UBIT_CTRL0,
		(47 << 16) | /* reg_fifo_thd */
		(1 << 12)  | /* reg_user_lr */
		(29 << 0)	/* reg_data_bit */
		);
	earcrx_dmac_write(EARCRX_ANA_RST_CTRL0, 1 << 31);
}

void earcrx_arc_init(void)
{
	unsigned int spdifin_clk = 500000000;

	/* sysclk/rate/32(bit)/2(ch)/2(bmc) */
	unsigned int counter_32k  = (spdifin_clk / (32000  * 64));
	unsigned int counter_44k  = (spdifin_clk / (44100  * 64));
	unsigned int counter_48k  = (spdifin_clk / (48000  * 64));
	unsigned int counter_88k  = (spdifin_clk / (88200  * 64));
	unsigned int counter_96k  = (spdifin_clk / (96000  * 64));
	unsigned int counter_176k = (spdifin_clk / (176400 * 64));
	unsigned int counter_192k = (spdifin_clk / (192000 * 64));
	unsigned int mode0_th = 3 * (counter_32k + counter_44k) >> 1;
	unsigned int mode1_th = 3 * (counter_44k + counter_48k) >> 1;
	unsigned int mode2_th = 3 * (counter_48k + counter_88k) >> 1;
	unsigned int mode3_th = 3 * (counter_88k + counter_96k) >> 1;
	unsigned int mode4_th = 3 * (counter_96k + counter_176k) >> 1;
	unsigned int mode5_th = 3 * (counter_176k + counter_192k) >> 1;
	unsigned int mode0_timer = counter_32k >> 1;
	unsigned int mode1_timer = counter_44k >> 1;
	unsigned int mode2_timer = counter_48k >> 1;
	unsigned int mode3_timer = counter_88k >> 1;
	unsigned int mode4_timer = counter_96k >> 1;
	unsigned int mode5_timer = (counter_176k >> 1);
	unsigned int mode6_timer = (counter_192k >> 1);

	earcrx_dmac_write(
		EARCRX_SPDIFIN_SAMPLE_CTRL0,
		0x0 << 28 |                /* detect by max_width */
		(spdifin_clk / 10000) << 0 /* base timer */
		);

	earcrx_dmac_write(
		EARCRX_SPDIFIN_SAMPLE_CTRL1,
		mode0_th << 20 |
		mode1_th << 10 |
		mode2_th << 0);

	earcrx_dmac_write(
		EARCRX_SPDIFIN_SAMPLE_CTRL2,
		mode3_th << 20 |
		mode4_th << 10 |
		mode5_th << 0);

	earcrx_dmac_write(
		EARCRX_SPDIFIN_SAMPLE_CTRL3,
		(mode0_timer << 24) |
		(mode1_timer << 16) |
		(mode2_timer << 8)	|
		(mode3_timer << 0)
		);

	earcrx_dmac_write(
		EARCRX_SPDIFIN_SAMPLE_CTRL4,
		(mode4_timer << 24) |
		(mode5_timer << 16) |
		(mode6_timer << 8)
		);


	earcrx_dmac_write(EARCRX_SPDIFIN_CTRL0,
		0x1 << 31 | /* reg_work_en */
		0x1 << 30 | /* reg_chnum_sel */
		0x1 << 25 | /* reg_findpapb_en */
		0x1 << 24 | /* nonpcm2pcm_th enable */
		0xFFF<<12   /* reg_nonpcm2pcm_th */
		);
	earcrx_dmac_write(EARCRX_SPDIFIN_CTRL2,
		(1 << 14) | /* reg_earc_auto */
		(1 << 13)  /* reg_earcin_papb_lr */
		);
	earcrx_dmac_write(EARCRX_SPDIFIN_CTRL3,
		(0xEC37<<16) | /* reg_earc_pa_value */
		(0x5A5A<<0)    /* reg_earc_pb_value */
		);
}

enum cmdc_st earcrx_cmdc_get_state(void)
{
	int val = earcrx_cmdc_read(EARC_RX_CMDC_STATUS0);
	enum cmdc_st state = (enum cmdc_st)(val & 0x7);

	return state;
}

enum attend_type earcrx_cmdc_get_attended_type(void)
{
	int val = earcrx_cmdc_read(EARC_RX_CMDC_STATUS0);
	enum cmdc_st state = (enum cmdc_st)(val & 0x7);
	enum attend_type type = ATNDTYP_DISCNCT;

	if ((val & (1 << 0x3)) && (state == CMDC_ST_ARC))
		type = ATNDTYP_ARC;
	else if ((val & (1 << 0x4)) && (state == CMDC_ST_EARC))
		type = ATNDTYP_eARC;

	return type;
}

void earcrx_cdmc_clr_irqs(int clr)
{
	earcrx_top_write(EARCRX_CMDC_INT_PENDING, clr);
}

int earcrx_cdmc_get_irqs(void)
{
	return earcrx_top_read(EARCRX_CMDC_INT_PENDING);
}

void earcrx_dmac_clr_irqs(int clr)
{
	earcrx_top_write(EARCRX_DMAC_INT_PENDING, clr);
}

int earcrx_dmac_get_irqs(void)
{
	return earcrx_top_read(EARCRX_DMAC_INT_PENDING);
}

void earcrx_enable(bool enable)
{
	enum attend_type type = earcrx_cmdc_get_attended_type();

	if (enable) {
		earcrx_dmac_update_bits(EARCRX_DMAC_SYNC_CTRL0,
			1 << 30,	 /* reg_rst_afifo_out_n */
			1 << 30);

		earcrx_dmac_update_bits(EARCRX_DMAC_SYNC_CTRL0,
			1 << 29,	 /* reg_rst_afifo_in_n */
			0x1 << 29);

		earcrx_dmac_update_bits(EARCRX_ERR_CORRECT_CTRL0,
			1 << 29,  /* reg_rst_afifo_out_n */
			1 << 29
			);
		earcrx_dmac_update_bits(EARCRX_ERR_CORRECT_CTRL0,
			1 << 28, /* reg_rst_afifo_in_n */
			1 << 28	/* reg_rst_afifo_in_n */
			);
	} else {
		earcrx_dmac_update_bits(EARCRX_DMAC_SYNC_CTRL0,
			0x3 << 29,
			0x0 << 29);

		earcrx_dmac_update_bits(EARCRX_ERR_CORRECT_CTRL0,
			0x3 << 28, 0x0 << 28);
	}

	if (type == ATNDTYP_eARC)
		earcrx_dmac_update_bits(EARCRX_DMAC_SYNC_CTRL0,
			1 << 31,	 /* reg_work_en */
			enable << 31);
	else if (type == ATNDTYP_ARC) {
		earcrx_dmac_update_bits(
			EARCRX_SPDIFIN_SAMPLE_CTRL0,
			0x1 << 31, /* reg_work_enable */
			enable << 31);

		earcrx_dmac_write(EARCRX_DMAC_SYNC_CTRL0, 0x0);
	}

	earcrx_dmac_update_bits(EARCRX_DMAC_UBIT_CTRL0,
		1 << 31, /* reg_work_enable */
		enable << 31);

	earcrx_dmac_update_bits(EARCRX_ERR_CORRECT_CTRL0,
		1 << 31,
		enable << 31); /* reg_work_en */

	earcrx_dmac_update_bits(EARCRX_DMAC_TOP_CTRL0,
		1 << 31,
		enable << 31); /* reg_top_work_en */
}
