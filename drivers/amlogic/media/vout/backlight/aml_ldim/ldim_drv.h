/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_drv.h
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

#ifndef _AML_LDIM_DRV_H_
#define _AML_LDIM_DRV_H_
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>

/*20180629: initial version */
/*20180725: new pwm control flow support */
/*20180730: algorithm clear up */
/*20180820: pq tooling support, espically optimize some alg parameters */
/*20181101: fix ldim_op_func null mistake, add new spi api support */
/*20181203: add 50/60hz change & iw7027 error handle support */
/*20181220: add tl1 support*/
/*20190103: add analog pwm support*/
/*20190107: add iw7038, iw7027_he support and ldim_delay for delay ms*/
/*20191115: add tm2 support*/
/*20200806: add white remap policy*/
/*20210608: update remapping support*/
/*20210620: support profile and pq parameters*/
#define LDIM_DRV_VER    "20210620"

extern unsigned char ldim_debug_print;

extern int ld_remap_lut[16][32];

#define AML_LDIM_MODULE_NAME "aml_ldim"
#define AML_LDIM_DRIVER_NAME "aml_ldim"
#define AML_LDIM_DEVICE_NAME "aml_ldim"
#define AML_LDIM_CLASS_NAME  "aml_ldim"

/*========================================*/
struct ldim_operate_func_s {
	unsigned short h_region_max;
	unsigned short v_region_max;
	unsigned short total_region_max;
	int (*alloc_rmem)(void);
	void (*stts_init)(unsigned int pic_h, unsigned int pic_v,
			  unsigned int blk_vnum, unsigned int blk_hnum);
	void (*remap_init)(struct LDReg_s *nprm,
			   unsigned int remap_en, unsigned int hvcnt_bypass);
	void (*remap_update)(struct LDReg_s *nprm,
			   unsigned int remap_en, unsigned int hvcnt_bypass);
	void (*vs_arithmetic)(void);
	void (*vs_remap)(struct LDReg_s *nprm, unsigned int avg_update_en,
			 unsigned int matrix_update_en);
};

void ldim_delay(int ms);
/*========================================*/

extern int  ldim_round(int ix, int ib);
void ldim_func_init_remap_lut(struct LDReg_s *nprm);
void ldim_func_profile_update(struct LDReg_s *nprm,
			      struct ldim_profile_s *ld_profile);
void ldim_func_init_prm_config(struct LDReg_s *nprm);

/* ldim hw */
#define LDIM_VPU_DMA_WR    0
#define LDIM_VPU_DMA_RD    1

void ldim_hw_remap_en(int flag);
void ldim_hw_remap_demo_en(int flag);
int ldim_hw_reg_dump(char *buf);
int ldim_hw_reg_dump_tm2(char *buf);
int ldim_hw_reg_dump_tm2b(char *buf);
void ldim_remap_reg_dump_tm2(void);
void ldim_hw_stts_read_zone(unsigned int nrow, unsigned int ncol);

void ldim_hw_remap_init_txlx(struct LDReg_s *nprm, unsigned int remap_en,
			     unsigned int hvcnt_bypass);
void ldim_hw_remap_init_tm2(struct LDReg_s *nprm, unsigned int remap_en,
			    unsigned int hvcnt_bypass);
void ldim_hw_remap_update_tm2(struct LDReg_s *nprm, unsigned int remap_en,
			      unsigned int hvcnt_bypass);
void ldim_hw_stts_initial_txlx(unsigned int pic_h, unsigned int pic_v,
			       unsigned int blk_vnum, unsigned int blk_hnum);
void ldim_hw_stts_initial_tl1(unsigned int pic_h, unsigned int pic_v,
			      unsigned int blk_vnum, unsigned int blk_hnum);
void ldim_hw_stts_initial_tm2(unsigned int pic_h, unsigned int pic_v,
			      unsigned int blk_vnum, unsigned int blk_hnum);
void ldim_hw_stts_initial_tm2b(unsigned int pic_h, unsigned int pic_v,
			       unsigned int hist_vnum, unsigned int hist_hnum);
void ldim_hw_remap_vs_txlx(struct LDReg_s *nprm, unsigned int avg_update_en,
			   unsigned int matrix_update_en);
void ldim_hw_remap_vs_tm2(struct LDReg_s *nprm, unsigned int avg_update_en,
			  unsigned int matrix_update_en);
void ldim_hw_remap_vs_tm2b(struct LDReg_s *nprm, unsigned int avg_update_en,
			   unsigned int matrix_update_en);
void ldim_remap_regs_update(void);

/*==============debug=================*/
void ldim_remap_ctrl(unsigned char status);
void ldim_func_ctrl(unsigned char status);
void ldim_stts_initial(unsigned int pic_h, unsigned int pic_v,
		       unsigned int blk_vnum, unsigned int blk_hnum);
void ldim_db_para_print(struct ldim_fw_para_s *fw_para);
int aml_ldim_debug_probe(struct class *ldim_class);
void aml_ldim_debug_remove(struct class *ldim_class);

#endif
