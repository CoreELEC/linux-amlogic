// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 *
 */

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/ldim_fw.h>
#include "ldim_drv.h"
#include "ldim_reg.h"

static void ldim_hw_update_matrix_tm2(int new_bl_matrix[], int bl_matrix_num)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int *ptr;
	int i;

	if (!ldim_drv->rmem)
		return;
	if (!ldim_drv->rmem->rd_mem_vaddr1)
		return;

	ptr = (unsigned int *)ldim_drv->rmem->rd_mem_vaddr1;
	for (i = 0; i < (bl_matrix_num + 1) / 2; i++) {
		*(unsigned int *)(ptr) = (unsigned int)
			(((new_bl_matrix[2 * i + 1] & 0xfff) << 16) |
			 (new_bl_matrix[2 * i] & 0xfff));
		ptr++;
	}
	ldim_wr_vcbus_bits(VPU_DMA_RDMIF_CTRL3, (bl_matrix_num + 7) / 8, 0, 13);
	ldim_wr_vcbus_bits(VPU_DMA_RDMIF_CTRL, 1, 13, 1);
}

static void ldim_hw_update_matrix_tm2b(int new_bl_matrix[], int bl_matrix_num)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int *ptr;
	int i;

	if (!ldim_drv->rmem)
		return;
	if (!ldim_drv->rmem->rd_mem_vaddr1)
		return;

	ptr = (unsigned int *)ldim_drv->rmem->rd_mem_vaddr1;
	for (i = 0; i < (bl_matrix_num + 1) / 2; i++) {
		*(unsigned int *)(ptr) = (unsigned int)
			(((new_bl_matrix[2 * i + 1] & 0xfff) << 16) |
			 (new_bl_matrix[2 * i] & 0xfff));
		ptr++;
	}
	ldim_wr_vcbus_bits(VPU_DMA_RDMIF0_CTRL, (bl_matrix_num + 7) / 8, 0, 13);
}

static void ldim_vpu_dma_mif_set(int rw_sel, unsigned int hist_vnum,
				 unsigned int hist_hnum)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int zone_num, data_num = 0;

	if (ldim_drv->rmem->wr_mem_paddr1 == 0 ||
	    ldim_drv->rmem->wr_mem_paddr2 == 0 ||
	    ldim_drv->rmem->rd_mem_paddr1 == 0 ||
	    ldim_drv->rmem->rd_mem_paddr2 == 0)
		return;
	LDIMPR("set vpu dma mif: %d\n", rw_sel);

/* write mif
 * 0~128(include 128): 5 * (4 * 32)bit of one region, 5bit_width
 * 128~512(include 512): 64bit once region, (1/2)bit_width
 * 512~1536(include 1536): 32bit once region, (1/4)bit_width
 * bit_width: 128bit
 */
	zone_num = hist_vnum * hist_hnum;

	if (rw_sel == LDIM_VPU_DMA_WR) { //write mif
		if (zone_num <= 128)
			data_num = 5 * zone_num;
		else if (zone_num <= 512)
			data_num = zone_num / 2;
		else if (zone_num <= 1536)
			data_num = zone_num / 4;
		else
			LDIMERR("unsuppose zone_num: %d\n", zone_num);
		ldim_wr_vcbus_bits(VPU_DMA_WRMIF_CTRL3, data_num, 0, 13);
		ldim_wr_vcbus(VPU_DMA_WRMIF_BADDR0, ldim_drv->rmem->wr_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_WRMIF_BADDR1, ldim_drv->rmem->wr_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_WRMIF_BADDR2, ldim_drv->rmem->wr_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_WRMIF_BADDR3, ldim_drv->rmem->wr_mem_paddr1);
		ldim_wr_vcbus_bits(VPU_DMA_WRMIF_CTRL, 1, 13, 1); // wr_mif_enable
	} else { //read mif
		ldim_wr_vcbus(VPU_DMA_RDMIF_BADDR0, ldim_drv->rmem->rd_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_RDMIF_BADDR1, ldim_drv->rmem->rd_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_RDMIF_BADDR2, ldim_drv->rmem->rd_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_RDMIF_BADDR3, ldim_drv->rmem->rd_mem_paddr1);
		//reset index to 0
		ldim_wr_vcbus_bits(VPU_DMA_RDMIF_CTRL, 0, 11, 2);
		//4 based address recycle
		ldim_wr_vcbus_bits(VPU_DMA_RDMIF_CTRL, 1, 9, 1);
		//wr_mif_enable disable
		ldim_wr_vcbus_bits(VPU_DMA_RDMIF_CTRL, 0, 13, 1);
	}
}

static void ldim_vpu_dma_mif_set_tm2b(int rw_sel, unsigned int hist_vnum,
				      unsigned int hist_hnum)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int zone_num, data_num = 0;

	if (ldim_drv->rmem->wr_mem_paddr1 == 0 ||
	    ldim_drv->rmem->wr_mem_paddr2 == 0 ||
	    ldim_drv->rmem->rd_mem_paddr1 == 0 ||
	    ldim_drv->rmem->rd_mem_paddr2 == 0)
		return;
	LDIMPR("set vpu dma mif: %d\n", rw_sel);

/* write mif
 * 0~128(include 128): 5 * (4 * 32)bit of one region, 5bit_width
 * 128~512(include 512): 64bit once region, (1/2)bit_width
 * 512~1536(include 1536): 32bit once region, (1/4)bit_width
 * bit_width: 128bit
 */
	zone_num = hist_vnum * hist_hnum;

	if (rw_sel == LDIM_VPU_DMA_WR) { //write mif
		if (zone_num <= 128)
			data_num = 5 * zone_num;
		else if (zone_num <= 512)
			data_num = zone_num / 2;
		else if (zone_num <= 1536)
			data_num = zone_num / 4;
		else
			LDIMERR("unsuppose zone_num: %d\n", zone_num);
		ldim_wr_vcbus_bits(VPU_DMA_WRMIF_CTRL3, data_num, 0, 13);
		ldim_wr_vcbus(VPU_DMA_WRMIF_BADDR0, ldim_drv->rmem->wr_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_WRMIF_BADDR1, ldim_drv->rmem->wr_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_WRMIF_BADDR2, ldim_drv->rmem->wr_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_WRMIF_BADDR3, ldim_drv->rmem->wr_mem_paddr1);
		ldim_wr_vcbus_bits(VPU_DMA_WRMIF_CTRL, 1, 13, 1); // wr_mif_enable
	} else { //read mif
		ldim_wr_vcbus(VPU_DMA_RDMIF0_BADR0, ldim_drv->rmem->rd_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_RDMIF0_BADR1, ldim_drv->rmem->rd_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_RDMIF0_BADR2, ldim_drv->rmem->rd_mem_paddr1);
		ldim_wr_vcbus(VPU_DMA_RDMIF0_BADR3, ldim_drv->rmem->rd_mem_paddr1);
		//reset index to 0
		ldim_wr_vcbus_bits(VPU_DMA_RDMIF0_CTRL, 1, 30, 1);
		//4 based address recycle
		//ldim_wr_vcbus_bits(VPU_DMA_RDMIF0_CTRL, 1, 9, 1);
		//wr_mif_enable disable
		ldim_wr_vcbus_bits(VPU_DMA_RDMIF0_CTRL, 0, 16, 1);
	}
}

void ldim_hw_vpu_dma_mif_en(int rw_sel, int flag)
{
	unsigned int en;

	en = flag ? 1 : 0;
	if (rw_sel == LDIM_VPU_DMA_WR)
		ldim_wr_vcbus_bits(VPU_DMA_WRMIF_CTRL, en, 13, 1);
	else
		ldim_wr_vcbus_bits(VPU_DMA_RDMIF_CTRL, en, 13, 1);
}

void ldim_hw_vpu_dma_mif_en_tm2b(int rw_sel, int flag)
{
	unsigned int en;

	en = flag ? 1 : 0;
	if (rw_sel == LDIM_VPU_DMA_WR)
		ldim_wr_vcbus_bits(VPU_DMA_WRMIF_CTRL, en, 13, 1);
	else
		ldim_wr_vcbus_bits(VPU_DMA_RDMIF0_CTRL, en, 16, 1);
}

void ldim_hw_remap_en(struct aml_ldim_driver_s *ldim_drv, int flag)
{
	if (flag)
		ldim_wr_reg(0x0a, 0x706);
	else
		ldim_wr_reg(0x0a, 0x600);
}

void ldim_hw_remap_demo_en(int flag)
{
	if (flag)
		ldim_wr_reg_bits(REG_LD_RGB_MOD, 1, 19, 1);
	else
		ldim_wr_reg_bits(REG_LD_RGB_MOD, 0, 19, 1);
}

/* VDIN_MATRIX_YUV601_RGB */
/* -16	   1.164  0	 1.596	    0 */
/* -128     1.164 -0.391 -0.813      0 */
/* -128     1.164  2.018  0	     0 */
/*{0x07c00600, 0x00000600, 0x04a80000, 0x066204a8, 0x1e701cbf, 0x04a80812,
 *	0x00000000, 0x00000000, 0x00000000,},
 */
static void ldim_hw_set_matrix_ycbcr2rgb(void)
{
	ldim_wr_vcbus_bits(LDIM_STTS_CTRL0, 1, 2, 1);  /* enable matrix */

	ldim_wr_vcbus(LDIM_STTS_MATRIX_PRE_OFFSET0_1, 0x07c00600);
	ldim_wr_vcbus(LDIM_STTS_MATRIX_PRE_OFFSET2, 0x00000600);
	ldim_wr_vcbus(LDIM_STTS_MATRIX_COEF00_01, 0x04a80000);
	ldim_wr_vcbus(LDIM_STTS_MATRIX_COEF02_10, 0x066204a8);
	ldim_wr_vcbus(LDIM_STTS_MATRIX_COEF11_12, 0x1e701cbf);
	ldim_wr_vcbus(LDIM_STTS_MATRIX_COEF20_21, 0x04a80812);
	ldim_wr_vcbus(LDIM_STTS_MATRIX_COEF22, 0x00000000);
	ldim_wr_vcbus(LDIM_STTS_MATRIX_OFFSET0_1, 0x00000000);
	ldim_wr_vcbus(LDIM_STTS_MATRIX_OFFSET2, 0x00000000);
}

static void ldim_hw_set_matrix_rgb2ycbcr(int mode)
{
	ldim_wr_vcbus_bits(LDIM_STTS_CTRL0, 1, 2, 1);
	if (mode == 0) {/*ycbcr not full range, 601 conversion*/
		ldim_wr_vcbus(LDIM_STTS_MATRIX_PRE_OFFSET0_1, 0x0);
		ldim_wr_vcbus(LDIM_STTS_MATRIX_PRE_OFFSET2, 0x0);
		/*	0.257     0.504   0.098	*/
		/*	-0.148    -0.291  0.439	*/
		/*	0.439     -0.368 -0.071	*/
		ldim_wr_vcbus(LDIM_STTS_MATRIX_COEF00_01, (0x107 << 16) | 0x204);
		ldim_wr_vcbus(LDIM_STTS_MATRIX_COEF02_10, (0x64 << 16) | 0x1f68);
		ldim_wr_vcbus(LDIM_STTS_MATRIX_COEF11_12, (0x1ed6 << 16) | 0x1c2);
		ldim_wr_vcbus(LDIM_STTS_MATRIX_COEF20_21, (0x1c2 << 16) | 0x1e87);
		ldim_wr_vcbus(LDIM_STTS_MATRIX_COEF22, 0x1fb7);

		ldim_wr_vcbus(LDIM_STTS_MATRIX_OFFSET2, 0x0200);
	} else if (mode == 1) {/*ycbcr full range, 601 conversion*/
		/* todo */
	}
}

void ldim_hw_remap_init_tm2(struct ld_reg_s *nprm, unsigned int ldim_bl_en,
			    unsigned int ldim_hvcnt_bypass)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	unsigned int i;
	unsigned int data;
	unsigned int *array_tmp;

	array_tmp = kcalloc(1536, sizeof(unsigned int), GFP_KERNEL);
	if (!array_tmp)
		return;

	/*  LD_FRM_SIZE  */
	data = ((nprm->reg_ld_pic_row_max & 0xfff) << 16) |
		(nprm->reg_ld_pic_col_max & 0xfff);
	ldim_wr_reg(REG_LD_FRM_SIZE, data);

	/* LD_RGB_MOD */
	data = ((0 & 0xfff)                                << 20) |
		((nprm->reg_ld_rgb_mapping_demo & 0x1)     << 19) |
		((nprm->reg_ld_x_lut_interp_mode[2] & 0x1) << 18) |
		((nprm->reg_ld_x_lut_interp_mode[1] & 0x1) << 17) |
		((nprm->reg_ld_x_lut_interp_mode[0] & 0x1) << 16) |
		((0 & 0x1) << 15) |
		((nprm->reg_ld_bklit_lpfmod & 0x7) << 12) |
		((nprm->reg_ld_litshft & 0x7)      << 8)  |
		((nprm->reg_ld_blk_xtlk & 0x1)     << 7)  |
		((nprm->reg_ld_bklit_intmod & 0x1) << 6)  |
		((nprm->reg_ld_bklut_intmod & 0x1) << 5)  |
		((nprm->reg_ld_blk_curmod & 0x1)   << 4)  |
		((nprm->reg_ld_blk_mode & 0x3));
	ldim_wr_reg(REG_LD_RGB_MOD, data);

	/* LD_BLK_HVNUM  */
	data = ((nprm->reg_ld_reflect_vnum & 0x7)  << 20) |
		((nprm->reg_ld_reflect_hnum & 0x7) << 16) |
		((nprm->reg_ld_blk_vnum & 0x3f)    <<  8) |
		((nprm->reg_ld_blk_hnum & 0x3f));
	ldim_wr_reg(REG_LD_BLK_HVNUM, data);

	/* LD_HVGAIN */
	data = ((nprm->reg_ld_vgain & 0xfff) << 16) |
		(nprm->reg_ld_hgain & 0xfff);
	ldim_wr_reg(REG_LD_HVGAIN, data);

	/*  LD_BKLIT_VLD  */
	data = 0;
	for (i = 0; i < 32; i++)
		if (nprm->reg_ld_bklit_valid[i])
			data = data | (1 << i);
	ldim_wr_reg(REG_LD_BKLIT_VLD, data);

	/* LD_BKLIT_PARAM */
	data = ((nprm->reg_ld_bklit_celnum & 0xff) << 16) |
		(nprm->reg_bl_matrix_avg & 0xfff);
	ldim_wr_reg(REG_LD_BKLIT_PARAM, data);

	/*  LD_LIT_GAIN_COMP */
	data = ((nprm->reg_ld_litgain & 0xfff) << 16) |
		(nprm->reg_bl_matrix_compensate & 0xfff);
	ldim_wr_reg(REG_LD_LIT_GAIN_COMP, data);

	/* LD_FRM_RST_POS */
	data = (1 << 16) | (5); /* h=1,v=5 :ldim_param_frm_rst_pos */
	ldim_wr_reg(REG_LD_FRM_RST_POS, data);

	/* LD_FRM_BL_START_POS */
	data = (1 << 16) | (6); /* ldim_param_frm_bl_start_pos; */
	ldim_wr_reg(REG_LD_FRM_BL_START_POS, data);

	/* REG_LD_FRM_HBLAN_VHOLS  */
	data = ((nprm->reg_ld_lut_vho_ls & 0x7) << 16) |
		((6 & 0x1fff)) ;  /*frm_hblank_num */
	ldim_wr_reg(REG_LD_FRM_HBLAN_VHOLS, data);

	/* REG_LD_XLUT_DEMO_ROI_XPOS */
	data = ((nprm->reg_ld_xlut_demo_roi_xend & 0x1fff) << 16) |
		(nprm->reg_ld_xlut_demo_roi_xstart & 0x1fff);
	ldim_wr_reg(REG_LD_XLUT_DEMO_ROI_XPOS, data);

	/* REG_LD_XLUT_DEMO_ROI_YPOS */
	data = ((nprm->reg_ld_xlut_demo_roi_yend & 0x1fff) << 16) |
		(nprm->reg_ld_xlut_demo_roi_ystart & 0x1fff);
	ldim_wr_reg(REG_LD_XLUT_DEMO_ROI_YPOS, data);

	/* REG_LD_XLUT_DEMO_ROI_CTRL */
	data = ((nprm->reg_ld_xlut_oroi_enable & 0x1) << 1) |
		(nprm->reg_ld_xlut_iroi_enable & 0x1);
	ldim_wr_reg(REG_LD_XLUT_DEMO_ROI_CTRL, data);

	/*  X_idx: 12*16  */
	ldim_wr_lut(REG_LD_RGB_IDX_BASE, nprm->x_idx[0], 16, 16);

	/* X_nrm[16]: 4 * 16 */
	ldim_wr_lut(REG_LD_RGB_NRMW_BASE_TM2, nprm->x_nrm[0], 4, 16);

	/*reg_LD_BLK_Hidx[57]: 14*57 */
	ldim_wr_lut(REG_LD_BLK_HIDX_BASE_TM2,
		    nprm->reg_ld_blk_hidx, 16, LD_BLK_LEN_H);

	/* reg_LD_BLK_Vidx[57]: 14*57 */
	ldim_wr_lut(REG_LD_BLK_VIDX_BASE_TM2,
		    nprm->reg_ld_blk_vidx, 16, LD_BLK_LEN_V);

	/* reg_LD_LUT_VHk_pos[32]/reg_LD_LUT_VHk_neg[32]: u8 */
	for (i = 0; i < 32; i++)
		array_tmp[i]    =  nprm->reg_ld_lut_vhk_pos[i];
	for (i = 0; i < 32; i++)
		array_tmp[32 + i] =  nprm->reg_ld_lut_vhk_neg[i];
	ldim_wr_lut(REG_LD_LUT_VHK_NEGPOS_BASE_TM2, array_tmp, 8, 64);

	/* reg_LD_LUT_VHo_pos[32]/reg_LD_LUT_VHo_neg[32]: s8 */
	for (i = 0; i < 32; i++)
		array_tmp[i]    =  nprm->reg_ld_lut_vho_pos[i];
	for (i = 0; i < 32; i++)
		array_tmp[32 + i] =  nprm->reg_ld_lut_vho_neg[i];
	ldim_wr_lut(REG_LD_LUT_VHO_NEGPOS_BASE_TM2, array_tmp, 8, 64);

	/* reg_LD_LUT_HHk[32]:u8 */
	ldim_wr_lut(REG_LD_LUT_HHK_BASE_TM2, nprm->reg_ld_lut_hhk, 8, 32);

	/*reg_LD_Reflect_Hdgr[20],reg_LD_Reflect_Vdgr[20],
	 *	reg_LD_Reflect_Xdgr[4]
	 */
	for (i = 0; i < 20; i++)
		array_tmp[i] = nprm->reg_ld_reflect_hdgr[i];
	for (i = 0; i < 20; i++)
		array_tmp[20 + i] = nprm->reg_ld_reflect_vdgr[i];
	for (i = 0; i < 4; i++)
		array_tmp[40 + i] = nprm->reg_ld_reflect_xdgr[i];
	ldim_wr_lut(REG_LD_REFLECT_DGR_BASE_TM2, array_tmp, 8, 44);

	//reg_LD_LUT_Hdg_LEXT[16]
	//reg_LD_LUT_Vdg_LEXT[16]
	//reg_LD_LUT_VHk_LEXT[16]
	for (i = 0; i < LD_NUM_PROFILE; i++)
		array_tmp[i] = (nprm->reg_ld_lut_hdg_lext_txlx[i] & 0x3ff) |
			((nprm->reg_ld_lut_vhk_lext_txlx[i] & 0x3ff) << 10) |
			((nprm->reg_ld_lut_vdg_lext_txlx[i] & 0x3ff) << 20);
	ldim_wr_lut_drt(REG_LD_LUT_LEXT_BASE_TM2, array_tmp, 8);

	/*reg_LD_LUT_Hdg[16][32]: u10*16*32*/
	ldim_wr_lut(REG_LD_LUT_HDG_BASE_TM2,
		    nprm->reg_ld_lut_hdg_txlx[0], 16, LD_NUM_PROFILE * 32);

	/*reg_LD_LUT_Vdg[16][32]: u10*16*32*/
	ldim_wr_lut(REG_LD_LUT_VDG_BASE_TM2,
		    nprm->reg_ld_lut_vdg_txlx[0], 16, LD_NUM_PROFILE * 32);

	/*reg_LD_LUT_VHk[16][32]: u10*16*32*/
	ldim_wr_lut(REG_LD_LUT_VHK_BASE_TM2,
		    nprm->reg_ld_lut_vhk_txlx[0], 16, LD_NUM_PROFILE * 32);

	/*reg_LD_LUT_Id[16][24]: u3*32*48=u3*1536 */
	ldim_wr_lut(REG_LD_LUT_ID_BASE_TM2, nprm->reg_ld_lut_id, 4, 384);

	/*enable the CBUS configure the RAM*/
	/*LD_MISC_CTRL0  {reg_blmat_ram_mode,
	 *1'h0,ram_bus_sel,ram_clk_sel,ram_clk_gate_en,
	 *2'h0,reg_hvcnt_bypass,reg_demo_synmode,reg_ldbl_synmode,
	 *reg_ldim_bl_en,soft_bl_start,reg_soft_rst)
	 */
	data = ldim_rd_reg(REG_LD_MISC_CTRL0);
	data = (data & (~(3 << 9))) | (1 << 8) | (1 << 4);
	ldim_wr_reg(REG_LD_MISC_CTRL0, data);

	/*X_lut[3][16][16]*/
	ldim_wr_lut_drt(REG_LD_RGB_LUT_BASE, nprm->x_lut2[0][0],
			(3 * 16 * 32 / 2));

	ldim_wr_reg(REG_LD_MISC_CTRL0, data);
	data = 0 | (0 << 1) | ((ldim_bl_en & 0x1) << 2) |
		(1 << 8) | (3 << 9) | (1 << 4);	//(ldim_hvcnt_bypass << 5)
	ldim_wr_reg(REG_LD_MISC_CTRL0, data);

	//LDIM_Update_Matrix(nprm->BL_matrix, 16 * 24);

	if (bdrv->data->chip_type == LCD_CHIP_TM2) {
		ldim_vpu_dma_mif_set(LDIM_VPU_DMA_RD, nprm->reg_ld_blk_vnum,
				     nprm->reg_ld_blk_hnum);
	} else if (bdrv->data->chip_type == LCD_CHIP_TM2B) {
		ldim_vpu_dma_mif_set_tm2b(LDIM_VPU_DMA_RD,
					  nprm->reg_ld_blk_vnum,
					  nprm->reg_ld_blk_hnum);
	}
	kfree(array_tmp);
}

static unsigned int ldim_hw_reg_dump_table[] = {
	LDIM_STTS_GCLK_CTRL0,
	LDIM_STTS_WIDTHM1_HEIGHTM1,
	LDIM_STTS_CTRL0,
	LDIM_STTS_HIST_REGION_IDX
};

int ldim_hw_reg_dump(char *buf)
{
	unsigned int reg, data32;
	int i, size, len = 0;

	size = sizeof(ldim_hw_reg_dump_table) / sizeof(unsigned int);
	for (i = 0; i < size; i++) {
		reg = ldim_hw_reg_dump_table[i];
		data32 = ldim_rd_vcbus(reg);
		len += sprintf(buf + len, "0x%x = 0x%08x\n", reg, data32);
	}

	data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
	ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xffe0ffff & data32);
	for (i = 0; i < 23; i++) {
		reg = LDIM_STTS_HIST_SET_REGION;
		data32 = ldim_rd_vcbus(reg);
		len += sprintf(buf + len, "0x%x = 0x%08x\n", reg, data32);
	}

	return len;
}

static unsigned int ldim_hw_reg_dump_table_tm2[] = {
	VPU_DMA_RDMIF_CTRL,
	VPU_DMA_RDMIF_BADDR0,
	VPU_DMA_RDMIF_BADDR1,
	VPU_DMA_RDMIF_BADDR2,
	VPU_DMA_RDMIF_BADDR3
};

int ldim_hw_reg_dump_tm2(char *buf)
{
	unsigned int reg, data32;
	int i, size, len = 0;

	data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
	ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xffc0ffff & data32);

	size = sizeof(ldim_hw_reg_dump_table) / sizeof(unsigned int);
	for (i = 0; i < size; i++) {
		reg = ldim_hw_reg_dump_table[i];
		data32 = ldim_rd_vcbus(reg);
		len += sprintf(buf + len, "0x%x = 0x%08x\n", reg, data32);
	}

	for (i = 0; i < 51; i++) {
		reg = LDIM_STTS_HIST_SET_REGION;
		data32 = ldim_rd_vcbus(reg);
		len += sprintf(buf + len, "0x%x = 0x%08x\n", reg, data32);
	}

	size = sizeof(ldim_hw_reg_dump_table_tm2) / sizeof(unsigned int);
	for (i = 0; i < size; i++) {
		reg = ldim_hw_reg_dump_table_tm2[i];
		data32 = ldim_rd_vcbus(reg);
		len += sprintf(buf + len, "0x%x = 0x%08x\n", reg, data32);
	}

	return len;
}

static unsigned int ldim_hw_reg_dump_table_tm2b[] = {
	VPU_DMA_RDMIF0_CTRL,
	VPU_DMA_RDMIF0_BADR0,
	VPU_DMA_RDMIF0_BADR1,
	VPU_DMA_RDMIF0_BADR2,
	VPU_DMA_RDMIF0_BADR3
};

int ldim_hw_reg_dump_tm2b(char *buf)
{
	unsigned int reg, data32;
	int i, size, len = 0;

	data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
	ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xffc0ffff & data32);

	size = sizeof(ldim_hw_reg_dump_table) / sizeof(unsigned int);
	for (i = 0; i < size; i++) {
		reg = ldim_hw_reg_dump_table[i];
		data32 = ldim_rd_vcbus(reg);
		len += sprintf(buf + len, "0x%x = 0x%08x\n", reg, data32);
	}

	for (i = 0; i < 51; i++) {
		reg = LDIM_STTS_HIST_SET_REGION;
		data32 = ldim_rd_vcbus(reg);
		len += sprintf(buf + len, "0x%x = 0x%08x\n", reg, data32);
	}

	size = sizeof(ldim_hw_reg_dump_table_tm2b) / sizeof(unsigned int);
	for (i = 0; i < size; i++) {
		reg = ldim_hw_reg_dump_table_tm2b[i];
		data32 = ldim_rd_vcbus(reg);
		len += sprintf(buf + len, "0x%x = 0x%08x\n", reg, data32);
	}

	return len;
}

/***** local dimming stts functions begin *****/
/*hist mode: 0: comp0 hist only, 1: Max(comp0,1,2) for hist,
 *2: the hist of all comp0,1,2 are calculated
 */
/*lpf_en   1: 1,2,1 filter on before finding max& hist*/
/*rd_idx_auto_inc_mode 0: no self increase, 1: read index increase after
 *read a 25/48 block, 2: increases every read and lock sub-idx
 */
/*one_ram_en 1: one ram mode;  0:double ram mode*/
static void ldim_hw_stts_en(unsigned int resolution,
			    unsigned int pix_drop_mode, unsigned int eol_en,
			    unsigned int hist_mode, unsigned int lpf_en,
			    unsigned int rd_idx_auto_inc_mode,
			    unsigned int one_ram_en)
{
	int data32;

	ldim_wr_vcbus(LDIM_STTS_GCLK_CTRL0, 0x0);
	ldim_wr_vcbus(LDIM_STTS_WIDTHM1_HEIGHTM1, resolution);

	data32 = 0x80000000 | ((pix_drop_mode & 0x3) << 29);
	data32 = data32 | ((eol_en & 0x1) << 28);
	data32 = data32 | ((one_ram_en & 0x1) << 27);
	data32 = data32 | ((hist_mode & 0x3) << 22);
	data32 = data32 | ((lpf_en & 0x1) << 21);
	data32 = data32 | ((rd_idx_auto_inc_mode & 0xff) << 14);
	ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, data32);
}

static void ldim_hw_stts_en_tm2(unsigned int resolution,
				unsigned int pix_drop_mode, unsigned int eol_en,
				unsigned int hist_mode, unsigned int lpf_en,
				unsigned int rd_idx_auto_inc_mode,
				unsigned int zone_num)
{
	int data32;

	ldim_wr_vcbus(LDIM_STTS_GCLK_CTRL0, zone_num << 8);
	ldim_wr_vcbus(LDIM_STTS_WIDTHM1_HEIGHTM1, resolution);

	data32 = 0x80000000 | ((pix_drop_mode & 0x3) << 29);
	data32 = data32 | ((eol_en & 0x1) << 28);
	//data32 = data32 | ((one_ram_en & 0x1) << 27);
	data32 = data32 | ((hist_mode & 0x3) << 22);
	data32 = data32 | ((lpf_en & 0x1) << 21);
	//data32 = data32 | ((rd_idx_auto_inc_mode & 0xff) << 14);
	ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, data32);
}

static void ldim_hw_stts_set_region_tl1(unsigned int resolution,
					/* 0: auto calc by height/width/
					 *                 row_start/col_start
					 * 1: 720p
					 * 2: 1080p
					 */
					unsigned int height,
					unsigned int width,
					unsigned int blk_height,
					unsigned int blk_width,
					unsigned int row_start,
					unsigned int col_start,
					unsigned int hist_vnum,
					unsigned int hist_hnum)
{
	unsigned int data32, h_region_num;
	unsigned int heightm1, widthm1;
	unsigned int lights_mode = 0;
	unsigned int hend[24], vend[16], i;

	memset(hend, 0, (sizeof(unsigned int) * 24));
	memset(vend, 0, (sizeof(unsigned int) * 16));

	/* 0:normal   1:>24 h region */
	if (hist_hnum > 24)
		lights_mode = 1;
	h_region_num = hist_hnum;
	if (ldim_debug_print) {
		pr_info("%s: lights_mode=%d, h_region_num=%d\n",
			__func__, lights_mode, h_region_num);
	}

	heightm1 = height - 1;
	widthm1  = width - 1;

	if (resolution == 0) {
		hend[0] = col_start + blk_width - 1;
		for (i = 1; i < 24; i++) {
			hend[i] = (hend[i - 1] + blk_width >= widthm1) ?
				hend[i - 1] : (hend[i - 1] + blk_width);
		}
		vend[0] = row_start + blk_height - 1;
		for (i = 1; i < 16; i++) {
			vend[i] = (vend[i - 1] + blk_height >= heightm1) ?
				vend[i - 1] : (vend[i - 1] + blk_height);
		}

		if (lights_mode == 1) {  /* 31 h region */
			vend[8] = (hend[23] + blk_width >= widthm1) ?
				hend[23] : (hend[23] + blk_width);
			for (i = 9; i < 16; i++) {
				vend[i] = (vend[i - 1] + blk_width >= widthm1) ?
					vend[i - 1] : (vend[i - 1] + blk_width);
			}
		}

		ldim_wr_vcbus_bits(LDIM_STTS_CTRL0, lights_mode, 22, 2);
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xffe0ffff & data32);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION,
			      ((((row_start & 0x1fff) << 16) & 0xffff0000) |
			       (col_start & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[1] & 0x1fff) << 16) |
			(hend[0] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((vend[1] & 0x1fff) << 16) |
			(vend[0] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[3] & 0x1fff) << 16) |
			(hend[2] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((vend[3] & 0x1fff) << 16) |
			(vend[2] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[5] & 0x1fff) << 16) |
			(hend[4] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((vend[5] & 0x1fff) << 16) |
			(vend[4] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[7] & 0x1fff) << 16) |
			(hend[6] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((vend[7] & 0x1fff) << 16) |
			(vend[6] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[9] & 0x1fff) << 16) |
			(hend[8] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((vend[9] & 0x1fff) << 16) |
			(vend[8] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[11] & 0x1fff) << 16) |
			(hend[10] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((vend[11] & 0x1fff) << 16) |
			(vend[10] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[13] & 0x1fff) << 16) |
			(hend[12] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((vend[13] & 0x1fff) << 16) |
			(vend[12] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[15] & 0x1fff) << 16) |
			(hend[14] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((vend[15] & 0x1fff) << 16) |
			(vend[14] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[17] & 0x1fff) << 16) |
			(hend[16] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[19] & 0x1fff) << 16) |
			(hend[18] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[21] & 0x1fff) << 16) |
			(hend[20] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, (((hend[23] & 0x1fff) << 16) |
			(hend[22] & 0x1fff)));
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, h_region_num); /*h region number*/
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0); /* line_n_int_num */
	} else if (resolution == 1) {
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0010010);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x1000080);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0800040);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x2000180);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x10000c0);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x3000280);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x1800140);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4000380);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x20001c0);
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4ff0480); */
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x2cf0260); */
	} else if (resolution == 2) {
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0000000);/* hv00 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x17f00bf);/* h01 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0d7006b);/* v01 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x2ff023f);/* h23 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x1af0143);/* v23 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x47f03bf);/* h45 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x287021b);/* v45 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x5ff053f);/* h67 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x35f02f3);/* v67 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h89 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* v89 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h1011 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* v1011 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h1213 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* v1213 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h1415 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* v1415 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h1617 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h1819 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h2021 */
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0xffe0ffe);/* h2223 */
	} else if (resolution == 3) {
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x1df00ef);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x10d0086);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x3bf02cf);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x21b0194);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x59f04af);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x32902a2);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x77f068f);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x43703b0);
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 4) { /* 5x6 */
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0040001);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x27f0136);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x1af00d7);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4ff03bf);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x35f0287);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x77f063f);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380437);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 5) { /* 8x2 */
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0030002);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x31f02bb);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0940031);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x233012b);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x30b0243);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x42d03d3);
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 6) { /* 2x1 */
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0030002);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x78002bb);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0940031);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 7) { /* 2x2 */
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x77f03bf);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x437021b);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 8) { /* 3x5 */
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0000000);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x2ff017f);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x2cf0167);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x5ff047f);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380437);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x780077f);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 9) { /* 4x3 */
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0010001);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4560333);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x2220180);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800666);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4000338);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	} else if (resolution == 10) { /* 6x8 */
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xfff0ffff & data32);

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x0010001);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x2430167);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x2220180);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4000350);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4000338);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x6000510);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4370410);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x77f0700);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438);
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x7800780); */
		/*    ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0x4380438); */
	}
}

static void ldim_hw_stts_set_region_tm2(unsigned int resolution,
					/* 0: auto calc by height/width/
					 *                 row_start/col_start
					 * 1: 720p
					 * 2: 1080p
					 */
					unsigned int height,
					unsigned int width,
					/*unsigned int blk_height,
					 *unsigned int blk_width,
					 */
					unsigned int row_start,
					unsigned int col_start,
					unsigned int hist_vnum,
					unsigned int hist_hnum)
{
	int data32;
	int heightm1, widthm1;
	int i;
	int hend_odd, hend_even;
	int vend_odd, vend_even;
	int blk_height, blk_width;

	blk_height = (height - row_start) / hist_vnum;
	blk_width = (width - col_start) / hist_hnum;

	heightm1 = height - 1;
	widthm1  = width - 1;

	LDIMPR("%s: %d %d %d %d %d %d %d %d\n",
		__func__, height, width, blk_height, blk_width,
		row_start, col_start, hist_vnum, hist_hnum);

	if (resolution == 0) {
		hend_odd  = col_start + blk_width - 1;
		data32 = ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX);
		ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX, 0xffc0ffff & data32);
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION,
			      ((((row_start & 0x1fff) << 16) & 0xffff0000) |
			       (col_start & 0x1fff)));
		for (i = 0; i < 24; i++) {
			hend_even = (hend_odd  + blk_width > widthm1) ?
			hend_odd : (hend_odd  + blk_width);
			ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION,
				      (((hend_even & 0x1fff) << 16) |
				       (hend_odd & 0x1fff)));
			hend_odd  = (hend_even  + blk_width > widthm1) ?
			hend_even : (hend_even + blk_width);
		}
		vend_odd  = row_start + blk_height - 1;
		for (i = 0; i < 24; i++) {
			vend_even = (vend_odd  + blk_height > heightm1) ?
			vend_odd : (vend_odd  + blk_height);
			ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION,
				      (((vend_even & 0x1fff) << 16) |
				       (vend_odd & 0x1fff)));
			vend_odd  = (vend_even + blk_height > heightm1) ?
				vend_even : (vend_even + blk_height);
		}

		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, hist_hnum); //h region number
		ldim_wr_vcbus(LDIM_STTS_HIST_SET_REGION, 0); // line_n_int_num
	} else {
		LDIMERR("unsupport resolution\n");
		return;
	}
}

void ldim_hw_stts_initial_tl1(unsigned int pic_h, unsigned int pic_v,
			      unsigned int hist_vnum, unsigned int hist_hnum)
{
	unsigned int resolution, blk_height, blk_width;
	unsigned int row_start, col_start;

	hist_vnum = (hist_vnum == 0) ? 1 : hist_vnum;
	hist_hnum = (hist_hnum == 0) ? 1 : hist_hnum;

	resolution = (((pic_h - 1) & 0xffff) << 16) | ((pic_v - 1) & 0xffff);
	/*ldim_wr_vcbus(VDIN0_HIST_CTRL, 0x10d);*/

	blk_height = (pic_v - 8) / hist_vnum;
	blk_width = (pic_h - 8) / hist_hnum;
	row_start = (pic_v - (blk_height * hist_vnum)) >> 1;
	col_start = (pic_h - (blk_width * hist_hnum)) >> 1;

	ldim_hw_stts_en(resolution, 0, 0, 1, 1, 1, 0);
	ldim_hw_set_matrix_ycbcr2rgb();
	/*0:di  1:vdin  2:null  3:postblend  4:vpp out  5:vd1  6:vd2  7:osd1*/
	ldim_wr_vcbus_bits(LDIM_STTS_CTRL0, 3, 3, 3);

	ldim_hw_stts_set_region_tl1(0, pic_v, pic_h, blk_height, blk_width,
				    row_start, col_start, hist_vnum, hist_hnum);
}

void ldim_hw_stts_initial_tm2(unsigned int pic_h, unsigned int pic_v,
			      unsigned int hist_vnum, unsigned int hist_hnum)
{
	unsigned int resolution, blk_height, blk_width;
	unsigned int row_start, col_start;

	hist_vnum = (hist_vnum == 0) ? 1 : hist_vnum;
	hist_hnum = (hist_hnum == 0) ? 1 : hist_hnum;

	resolution = (((pic_h - 1) & 0xffff) << 16) | ((pic_v - 1) & 0xffff);
	/*ldim_wr_vcbus(VDIN0_HIST_CTRL, 0x10d);*/

	blk_height = (pic_v - 8) / hist_vnum;
	blk_width = (pic_h - 8) / hist_hnum;
	row_start = (pic_v - (blk_height * hist_vnum)) >> 1;
	col_start = (pic_h - (blk_width * hist_hnum)) >> 1;

	ldim_hw_stts_en_tm2(resolution, 0, 0, 1, 1, 1, (hist_vnum * hist_hnum));
	ldim_hw_set_matrix_rgb2ycbcr(0);// vpp out format : RGB

	/*0:di  1:vdin  2:null  3:postblend  4:vpp out  5:vd1  6:vd2  7:osd1*/
	ldim_wr_vcbus_bits(LDIM_STTS_CTRL0, 4, 3, 3);

	/* bit21:20  3: input is RGB   0:input is YUV */
	ldim_wr_vcbus_bits(LDIM_STTS_CTRL0, 3, 20, 2);

	//ldim_hw_stts_set_region_tm2(0, pic_v, pic_h, blk_height, blk_width,
	ldim_hw_stts_set_region_tm2(0, pic_v, pic_h,
				    row_start, col_start, hist_vnum, hist_hnum);

	//config vpu dma
	ldim_vpu_dma_mif_set(LDIM_VPU_DMA_WR, hist_vnum, hist_hnum);
}

static unsigned int invalid_val_cnt;
void ldim_hw_stts_read_zone(unsigned int nrow, unsigned int ncol)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_stts_s *stts = ldim_drv->stts;
	unsigned int i, j, k;
	unsigned int n, tmp;

	if (invalid_val_cnt > 0xfffffff)
		invalid_val_cnt = 0;

	ldim_wr_vcbus(LDIM_STTS_HIST_REGION_IDX,
		      ldim_rd_vcbus(LDIM_STTS_HIST_REGION_IDX) & 0xffffc000);
	tmp = ldim_rd_vcbus(LDIM_STTS_HIST_START_RD_REGION);

	for (i = 0; i < nrow; i++) {
		for (j = 0; j < ncol; j++) {
			tmp = ldim_rd_vcbus(LDIM_STTS_HIST_START_RD_REGION);
			n = i * ncol + j;
			for (k = 0; k < 17; k++) {
				tmp = ldim_rd_vcbus(LDIM_STTS_HIST_READ_REGION);
				if (k == 16) {
					stts->max_rgb[n * 3] = tmp & 0x3ff;
					stts->max_rgb[n * 3 + 1] =
						(tmp >> 10) & 0x3ff;
					stts->max_rgb[n * 3 + 2] =
						(tmp >> 20) & 0x3ff;
				} else {
					stts->hist_matrix[n * 16 + k] = tmp;
				}
				if (!(tmp & 0x40000000))
					invalid_val_cnt++;
			}
		}
	}
}

void ldim_hw_remap_update_tm2(struct ld_reg_s *nprm, unsigned int avg_update_en,
			      unsigned int matrix_update_en)
{
	int data;

	if (avg_update_en) {
		/* LD_BKLIT_PARAM */
		data = ldim_rd_reg(REG_LD_BKLIT_PARAM);
		data = (data & (~0xfff)) | (nprm->reg_bl_matrix_avg & 0xfff);
		ldim_wr_reg(REG_LD_BKLIT_PARAM, data);

		/* compensate */
		data = ldim_rd_reg(REG_LD_LIT_GAIN_COMP);
		data = (data & (~0xfff)) |
			(nprm->reg_bl_matrix_compensate & 0xfff);
		ldim_wr_reg(REG_LD_LIT_GAIN_COMP, data);
	}

	if (matrix_update_en) {
		data = nprm->reg_ld_blk_vnum * nprm->reg_ld_blk_hnum;
		ldim_hw_update_matrix_tm2(nprm->bl_matrix, data);
	}
}

void ldim_hw_remap_update_tm2b(struct ld_reg_s *nprm, unsigned int avg_update_en,
			       unsigned int matrix_update_en)
{
	int data;

	if (avg_update_en) {
		/* LD_BKLIT_PARAM */
		data = ldim_rd_reg(REG_LD_BKLIT_PARAM);
		data = (data & (~0xfff)) | (nprm->reg_bl_matrix_avg & 0xfff);
		ldim_wr_reg(REG_LD_BKLIT_PARAM, data);

		/* compensate */
		data = ldim_rd_reg(REG_LD_LIT_GAIN_COMP);
		data = (data & (~0xfff)) |
			(nprm->reg_bl_matrix_compensate & 0xfff);
		ldim_wr_reg(REG_LD_LIT_GAIN_COMP, data);
	}

	if (matrix_update_en) {
		data = nprm->reg_ld_blk_vnum * nprm->reg_ld_blk_hnum;
		ldim_hw_update_matrix_tm2b(nprm->bl_matrix, data);
	}
}

