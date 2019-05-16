/*
 * sound/soc/amlogic/auge/earc_hw.h
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
#ifndef __EARC_HW_H__
#define __EARC_HW_H__

#include "regs.h"
#include "iomap.h"

#define INT_EARCRX_CMDC_IDLE2               (0x1 << 15)
#define INT_EARCRX_CMDC_IDLE1               (0x1 << 14)
#define INT_EARCRX_CMDC_DISC2               (0x1 << 13)
#define INT_EARCRX_CMDC_DISC1               (0x1 << 12)
#define INT_EARCRX_CMDC_EARC                (0x1 << 11)
#define INT_EARCRX_CMDC_HB_STATUS           (0x1 << 10)
#define INT_EARCRX_CMDC_LOSTHB              (0x1 << 9)
#define INT_EARCRX_CMDC_TIMEOUT             (0x1 << 8)
#define INT_EARCRX_CMDC_STATUS_CH           (0x1 << 7)
#define INT_EARCRX_CMDC_REC_INVALID_ID      (0x1 << 6)
#define INT_EARCRX_CMDC_REC_INVALID_OFFSET  (0x1 << 5)
#define INT_EARCRX_CMDC_REC_UNEXP           (0x1 << 4)
#define INT_EARCRX_CMDC_REC_ECC_ERR         (0x1 << 3)
#define INT_EARCRX_CMDC_REC_PARITY_ERR      (0x1 << 2)
#define INT_EARCRX_CMDC_RECV_PACKET         (0x1 << 1)
#define INT_EARCRX_CMDC_REC_TIME_OUT        (0x1 << 0)

#define INT_EARCRX_ANA_RST_C_NEW_FORMAT_SET                (0x1 << 17)
#define INT_EARCRX_ANA_RST_C_EARCRX_DIV2_HOLD_SET          (0x1 << 16)
#define INT_EARCRX_ERR_CORRECT_C_BCHERR_INT_SET            (0x1 << 15)
#define INT_EARCRX_ERR_CORRECT_R_AFIFO_OVERFLOW_SET        (0x1 << 14)
#define INT_EARCRX_ERR_CORRECT_R_FIFO_OVERFLOW_SET         (0x1 << 13)
#define INT_EARCRX_USER_BIT_CHECK_R_FIFO_OVERFLOW          (0x1 << 12)
#define INT_EARCRX_USER_BIT_CHECK_C_FIFO_THD_PASS          (0x1 << 11)
#define INT_EARCRX_USER_BIT_CHECK_C_U_PK_LOST_INT_SET      (0x1 << 10)
#define INT_ARCRX_USER_BIT_CHECK_C_IU_PK_END               (0x1 << 9)
#define INT_ARCRX_BIPHASE_DECODE_C_CHST_MUTE_CLR           (0x1 << 8)
#define INT_ARCRX_BIPHASE_DECODE_C_FIND_PAPB               (0x1 << 7)
#define INT_ARCRX_BIPHASE_DECODE_C_VALID_CHANGE            (0x1 << 6)
#define INT_ARCRX_BIPHASE_DECODE_C_FIND_NONPCM2PCM         (0x1 << 5)
#define INT_ARCRX_BIPHASE_DECODE_C_PCPD_CHANGE             (0x1 << 4)
#define INT_ARCRX_BIPHASE_DECODE_C_CH_STATUS_CHANGE        (0x1 << 3)
#define INT_ARCRX_BIPHASE_DECODE_I_SAMPLE_MODE_CHANGE      (0x1 << 2)
#define INT_ARCRX_BIPHASE_DECODE_R_PARITY_ERR              (0x1 << 1)
#define INT_ARCRX_DMAC_SYNC_AFIFO_OVERFLOW                 (0x1 << 0)

/* cmdc discovery and disconnect state */
enum cmdc_st {
	CMDC_ST_OFF,
	CMDC_ST_IDLE1,
	CMDC_ST_IDLE2,
	CMDC_ST_DISC1,
	CMDC_ST_DISC2,
	CMDC_ST_EARC,
	CMDC_ST_ARC
};

/* attended type: disconect, ARC, eARC */
enum attend_type {
	ATNDTYP_DISCNCT,
	ATNDTYP_ARC,
	ATNDTYP_eARC
};

extern void earcrx_cmdc_init(void);
void earcrx_cmdc_arc_connect(bool init);
void earcrx_cmdc_hpd_detect(bool st);
extern void earcrx_dmac_init(void);
extern void earcrx_arc_init(void);
enum cmdc_st earcrx_cmdc_get_state(void);
enum attend_type earcrx_cmdc_get_attended_type(void);
extern void earcrx_cdmc_clr_irqs(int clr);
extern int earcrx_cdmc_get_irqs(void);
extern void earcrx_dmac_clr_irqs(int clr);
extern int earcrx_dmac_get_irqs(void);
extern void earcrx_enable(bool enable);
#endif
