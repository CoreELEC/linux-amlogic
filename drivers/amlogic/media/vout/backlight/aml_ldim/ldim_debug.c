/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_debug.c
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

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>
#include "ldim_drv.h"
#include "ldim_reg.h"

static int ldim_debug_buf_save(char *path, unsigned char *save_buf,
			       unsigned int size)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();

	if (!save_buf) {
		LDIMERR("%s: save_buf is null\n", __func__);
		return -1;
	}
	if (size == 0) {
		LDIMERR("%s: size is zero\n", __func__);
		return -1;
	}

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR(filp)) {
		LDIMERR("%s: create %s error\n", __func__, path);
		set_fs(old_fs);
		return -1;
	}

	pos = 0;
	buf = (void *)save_buf;
	vfs_write(filp, buf, size, &pos);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);

	pr_info("debug file %s save finished\n", path);

	return 0;
}

void ldim_db_para_print(struct ldim_fw_para_s *fw_para)
{
	int i, len;
	char *buf;

	if (!fw_para->ctrl)
		return;

	len = 32 * 10 + 10;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	LDIMPR("%s:\n", __func__);
	pr_info("rgb_base = %d\n"
		"boost_gain = %d\n"
		"lpf_res = %d\n"
		"fw_LD_ThSF = %d\n\n",
		fw_para->ctrl->rgb_base,
		fw_para->ctrl->boost_gain,
		fw_para->ctrl->lpf_res,
		fw_para->ctrl->fw_LD_ThSF_l);

	pr_info("ld_vgain = %d\n"
		"ld_hgain = %d\n"
		"ld_litgain = %d\n\n",
		fw_para->nprm->reg_LD_Vgain,
		fw_para->nprm->reg_LD_Hgain,
		fw_para->nprm->reg_LD_Litgain);

	pr_info("ld_lut_vdg_lext = %d\n"
		"ld_lut_hdg_lext = %d\n"
		"ld_lut_vhk_lext = %d\n\n",
		fw_para->nprm->reg_LD_LUT_Vdg_LEXT,
		fw_para->nprm->reg_LD_LUT_Hdg_LEXT,
		fw_para->nprm->reg_LD_LUT_VHk_LEXT);

	len = 0;
	pr_info("ld_lut_hdg:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf + len, "\t%d",
			fw_para->nprm->reg_LD_LUT_Hdg[i]);
		if (i % 8 == 7)
			len += sprintf(buf + len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	pr_info("ld_lut_vdg:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf + len, "\t%d",
			fw_para->nprm->reg_LD_LUT_Vdg[i]);
		if (i % 8 == 7)
			len += sprintf(buf + len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	pr_info("ld_lut_vhk:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf + len, "\t%d",
			fw_para->nprm->reg_LD_LUT_VHk[i]);
		if (i % 8 == 7)
			len += sprintf(buf + len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	pr_info("ld_lut_vhk_pos:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf + len, "\t%d",
			fw_para->nprm->reg_LD_LUT_VHk_pos[i]);
		if (i % 8 == 7)
			len += sprintf(buf + len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	pr_info("ld_lut_vhk_neg:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf + len, "\t%d",
			fw_para->nprm->reg_LD_LUT_VHk_neg[i]);
		if (i % 8 == 7)
			len += sprintf(buf + len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	pr_info("ld_lut_hhk:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf + len, "\t%d",
			fw_para->nprm->reg_LD_LUT_HHk[i]);
		if (i % 8 == 7)
			len += sprintf(buf + len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	pr_info("ld_lut_vho_pos:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf + len, "\t%d",
			fw_para->nprm->reg_LD_LUT_VHo_pos[i]);
		if (i % 8 == 7)
			len += sprintf(buf + len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	pr_info("ld_lut_vho_neg:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf + len, "\t%d",
			fw_para->nprm->reg_LD_LUT_VHo_neg[i]);
		if (i % 8 == 7)
			len += sprintf(buf + len, "\n");
	}
	pr_info("%s\n", buf);

	kfree(buf);
}

static void ldim_bl_remap_curve_print(unsigned int *remap_curve)
{
	int i = 0, len;
	char *buf;

	if (!remap_curve) {
		LDIMERR("fw_para no bl_remap_curve\n");
		return;
	}

	len = 16 * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	pr_info("bl_remap_curve:\n");
	len = 0;
	for (i = 0; i < 16; i++)
		len += sprintf(buf + len, "\t%d", remap_curve[i]);
	pr_info("%s\n", buf);

	kfree(buf);
}

static void ldim_fw_LD_Whist_print(unsigned int *ld_whist)
{
	int i = 0, len;
	char *buf;

	if ((!ld_whist)) {
		LDIMERR("fw_para no fw_ld_whist\n");
		return;
	}

	len = 16 * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	pr_info("fw_ld_whist:\n");
	len = 0;
	for (i = 0; i < 16; i++)
		len += sprintf(buf + len, "\t%d", ld_whist[i]);
	pr_info("%s\n", buf);

	kfree(buf);
}

static void ldim_ld_remap_lut_print(int index)
{
	int i, j, len;
	char *buf;

	len = 32 * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	if (index == 0xff) {
		pr_info("ld_remap_lut:\n");
		for (i = 0; i < 16; i++) {
			pr_info("  %d:\n", i);
			len = 0;
			for (j = 0; j < 32; j++) {
				if (j == 16)
					len += sprintf(buf + len, "\n");
				len += sprintf(buf + len, "\t%d",
					ld_remap_lut[i][j]);
			}
			pr_info("%s\n", buf);
		}
	} else if (index < 16) {
		pr_info("ld_remap_lut %d:\n", index);
		len = 0;
		for (j = 0; j < 32; j++) {
			if (j == 16)
				len += sprintf(buf + len, "\n");
			len += sprintf(buf + len, "\t%d",
				ld_remap_lut[index][j]);
		}
		pr_info("%s\n", buf);
	} else {
		pr_info("ld_remap_lut invalid index %d\n", index);
	}
	pr_info("\n");

	kfree(buf);
}

static void ldim_dump_histgram(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, k, n, len;
	unsigned int *p = NULL;
	char *buf;

	n = 16 * 10 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col * 16;
	p = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}
	memcpy(p, ldim_drv->hist_matrix, len * sizeof(unsigned int));

	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		pr_info("%s: row %d:\n", __func__, i);
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			len = sprintf(buf, "col %d:\n", j);
			for (k = 0; k < 16; k++) {
				n = i * 16 * ldim_drv->conf->hist_col +
					j * 16 + k;
				len += sprintf(buf + len, "\t0x%x", *(p + n));
				if (k == 7)
					len += sprintf(buf + len, "\n");
			}
			pr_info("%s\n\n", buf);
		}
	}

	kfree(buf);
	kfree(p);
}

static void ldim_get_matrix_info_max_rgb(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, n, len;
	unsigned int *p = NULL;
	char *buf;

	n = ldim_drv->conf->hist_col * 30 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->hist_col * ldim_drv->conf->hist_row * 3;
	p = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}
	memcpy(p, ldim_drv->max_rgb, len * sizeof(unsigned int));

	pr_info("%s max_rgb:\n", __func__);
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		len = 0;
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			n =  i * ldim_drv->conf->hist_col + j;
			len += sprintf(buf + len, "\t(R:%4d, G:%4d, B:%4d)",
				       p[n * 3],
				       p[n * 3 + 1],
				       p[n * 3 + 2]);
			if ((j + 1) % 4 == 0)
				len += sprintf(buf + len, "\n\n");
		}
		pr_info("%s\n\n", buf);
	}

	kfree(buf);
	kfree(p);
}

static void ldim_get_matrix_info_TF(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, n, len;
	unsigned int *ldim_matrix_t = NULL;
	char *buf;

	n = ldim_drv->conf->hist_col * 10 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;
	ldim_matrix_t = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!ldim_matrix_t) {
		kfree(buf);
		return;
	}
	memcpy(ldim_matrix_t, &ldim_drv->fw_para->fdat->TF_BL_matrix[0],
	       len * sizeof(unsigned int));

	pr_info("%s:\n", __func__);
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		len = 0;
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			n = ldim_drv->conf->hist_col * i + j;
			len += sprintf(buf + len, "\t%4d", ldim_matrix_t[n]);
		}
		pr_info("%s\n", buf);
	}

	kfree(buf);
	kfree(ldim_matrix_t);
}

static void ldim_get_matrix_info_SF(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, n, len;
	unsigned int *ldim_matrix_t = NULL;
	char *buf;

	n = ldim_drv->conf->hist_col * 10 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;
	ldim_matrix_t = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!ldim_matrix_t) {
		kfree(buf);
		return;
	}
	memcpy(ldim_matrix_t, &ldim_drv->fw_para->fdat->SF_BL_matrix[0],
	       len * sizeof(unsigned int));

	pr_info("%s:\n", __func__);
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		len = 0;
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			n = ldim_drv->conf->hist_col * i + j;
			len += sprintf(buf + len, "\t%4d", ldim_matrix_t[n]);
		}
		pr_info("%s\n", buf);
	}

	kfree(buf);
	kfree(ldim_matrix_t);
}

static void ldim_get_matrix_info_4(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, k, n, len;
	unsigned int *ldim_matrix_t = NULL;
	char *buf;

	n = 3 * ldim_drv->conf->hist_col * 10 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col * 3;
	ldim_matrix_t = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!ldim_matrix_t) {
		kfree(buf);
		return;
	}
	memcpy(ldim_matrix_t, &ldim_drv->fw_para->fdat->last_sta1_maxrgb[0],
	       len * sizeof(unsigned int));

	pr_info("%s:\n", __func__);
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		len = 0;
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			len += sprintf(buf + len, "\tcol %d:",
				       ldim_drv->conf->hist_col);
			for (k = 0; k < 3; k++) {
				n = 3 * ldim_drv->conf->hist_col * i +
					j * 3 + k;
				len += sprintf(buf + len, "\t%4d",
					       ldim_matrix_t[n]);
			}
			len += sprintf(buf + len, "\n");
		}
		pr_info("%s\n", buf);
	}

	kfree(buf);
	kfree(ldim_matrix_t);
}

static void ldim_get_matrix_info_alpha(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, n, len;
	unsigned int *ldim_matrix_t = NULL;
	char *buf;

	n = ldim_drv->conf->hist_col * 10 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;
	ldim_matrix_t = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!ldim_matrix_t) {
		kfree(buf);
		return;
	}
	memcpy(ldim_matrix_t, &ldim_drv->fw_para->fdat->TF_BL_alpha[0],
	       len * sizeof(unsigned int));

	pr_info("%s:\n", __func__);
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		len = 0;
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			n = ldim_drv->conf->hist_col * i + j;
			len += sprintf(buf + len, "\t%4d", ldim_matrix_t[n]);
		}
		pr_info("%s\n", buf);
	}

	kfree(buf);
	kfree(ldim_matrix_t);
}

static void ldim_get_matrix(struct aml_ldim_driver_s *ldim_drv,
			    unsigned int *data, unsigned int reg_sel)
{
	unsigned char avg_update_en_bak, matrix_update_en_bak;

	avg_update_en_bak = ldim_drv->avg_update_en;
	matrix_update_en_bak = ldim_drv->matrix_update_en;
	ldim_drv->avg_update_en = 0;
	ldim_drv->matrix_update_en = 0;
	if (avg_update_en_bak || matrix_update_en_bak)
		msleep(20);

	/* gMatrix_LUT: s12*100 */
	if (reg_sel == 0)
		ldim_rd_lut(REG_LD_BLK_VIDX_BASE, data, 16, 32);
	else if (reg_sel == 3)
		ldim_get_matrix_info_TF(ldim_drv);
	else if (reg_sel == 4)
		ldim_get_matrix_info_SF(ldim_drv);
	else if (reg_sel == 5)
		ldim_get_matrix_info_4(ldim_drv);
	else if (reg_sel == 6)
		ldim_get_matrix_info_alpha(ldim_drv);
	else if (reg_sel == 7)
		ldim_get_matrix_info_max_rgb(ldim_drv);
	else if (reg_sel == REG_LD_LUT_HDG_BASE)
		ldim_rd_lut2(REG_LD_LUT_HDG_BASE, data, 10, 32);
	else if (reg_sel == REG_LD_LUT_VHK_BASE)
		ldim_rd_lut2(REG_LD_LUT_VHK_BASE, data, 10, 32);
	else if (reg_sel == REG_LD_LUT_VDG_BASE)
		ldim_rd_lut2(REG_LD_LUT_VDG_BASE, data, 10, 32);

	ldim_drv->avg_update_en = avg_update_en_bak;
	ldim_drv->matrix_update_en = matrix_update_en_bak;
}

static void ldim_get_matrix_info(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, n, len;
	unsigned short *ldim_matrix_t = NULL;
	unsigned short *ldim_matrix_spi_t = NULL;
	char *buf;

	n = ldim_drv->conf->hist_col * 10 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;
	ldim_matrix_t = kcalloc(len, sizeof(unsigned short), GFP_KERNEL);
	if (!ldim_matrix_t) {
		kfree(buf);
		return;
	}
	ldim_matrix_spi_t = kcalloc(len, sizeof(unsigned short), GFP_KERNEL);
	if (!ldim_matrix_spi_t) {
		kfree(buf);
		kfree(ldim_matrix_t);
		return;
	}

	memcpy(ldim_matrix_t, &ldim_drv->local_ldim_matrix[0],
	       len * sizeof(unsigned short));
	memcpy(ldim_matrix_spi_t, &ldim_drv->ldim_matrix_buf[0],
	       len * sizeof(unsigned short));

	pr_info("%s:\n", __func__);
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		len = 0;
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			n = ldim_drv->conf->hist_col * i + j;
			len += sprintf(buf + len, "\t%4d", ldim_matrix_t[n]);
		}
		pr_info("%s\n", buf);
	}

	pr_info("current black_frm: %d\n", ldim_drv->fw_para->ctrl->black_frm);
	pr_info("ldim_dev brightness transfer_matrix:\n");
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		len = 0;
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			n = ldim_drv->conf->hist_col * i + j;
			len += sprintf(buf + len, "\t%4d",
				       ldim_matrix_spi_t[n]);
		}
		pr_info("%s\n", buf);
	}
	pr_info("\n");

	kfree(buf);
	kfree(ldim_matrix_t);
	kfree(ldim_matrix_spi_t);
}

static void ldim_get_test_matrix_info(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, n, len;
	unsigned short *ldim_matrix_t = ldim_drv->test_matrix;
	char *buf;

	n = ldim_drv->conf->hist_col * ldim_drv->conf->hist_row;
	len = n * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	pr_info("%s:\n", __func__);
	pr_info("ldim test_mode: %d, test_matrix:\n", ldim_drv->test_en);
	len = 0;
	for (i = 1; i < n; i++)
		len += sprintf(buf + len, "\t%4d", ldim_matrix_t[i]);
	pr_info("%s\n", buf);

	kfree(buf);
}

static void ldim_sel_int_matrix_mute_print(struct aml_ldim_driver_s *ldim_drv,
					   unsigned int n, unsigned int *matrix)
{
	unsigned int i, len;
	unsigned int *ldim_matrix_t = NULL;
	char *buf;

	len = n * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	ldim_matrix_t = kcalloc(n, sizeof(unsigned int), GFP_KERNEL);
	if (!ldim_matrix_t) {
		kfree(buf);
		return;
	}
	memcpy(ldim_matrix_t, matrix, (n * sizeof(unsigned int)));

	len = 0;
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", ldim_matrix_t[i]);
	pr_info("for_tool:%s\n", buf);

	kfree(buf);
	kfree(ldim_matrix_t);
}

static void ldim_matrix_bl_matrix_mute_print(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, n, len;
	unsigned short *ldim_matrix_t = NULL;
	char *buf;

	len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	n = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;
	ldim_matrix_t = kcalloc(n, sizeof(unsigned short), GFP_KERNEL);
	if (!ldim_matrix_t) {
		kfree(buf);
		return;
	}
	memcpy(ldim_matrix_t, ldim_drv->local_ldim_matrix,
	       (n * sizeof(unsigned short)));

	len = 0;
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", ldim_matrix_t[i]);
	pr_info("for_tool: %d %d%s\n",
		ldim_drv->conf->hist_row, ldim_drv->conf->hist_col, buf);

	kfree(buf);
	kfree(ldim_matrix_t);
}

static void ldim_matrix_histgram_mute_print(struct aml_ldim_driver_s *ldim_drv,
					    unsigned int sel)
{
	unsigned int i, j, k, n, len;
	unsigned int *p = NULL;
	char *buf;

	n = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col * 16;
	if (sel == 0xffff) {
		len = n * 10 + 20;
	} else {
		if (sel >= ldim_drv->conf->hist_row *
		    ldim_drv->conf->hist_col) {
			pr_err("for_tool: wrong hist sel num %d\n", sel);
			return;
		}
		len = 16 * 10 + 20;
	}
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	p = kcalloc(n, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}

	memcpy(p, ldim_drv->hist_matrix, n * sizeof(unsigned int));

	len = 0;
	if (sel == 0xffff) {
		for (i = 0; i < ldim_drv->conf->hist_row; i++) {
			for (j = 0; j < ldim_drv->conf->hist_col; j++) {
				for (k = 0; k < 16; k++) {
					n = i * 16 * ldim_drv->conf->hist_col +
					    j * 16 + k;
					len += sprintf(buf + len, " 0x%x",
						       *(p + n));
				}
			}
		}
		pr_info("for_tool: %d 16%s\n",
			(ldim_drv->conf->hist_row * ldim_drv->conf->hist_col),
			buf);
	} else {
		i = sel / ldim_drv->conf->hist_col;
		j = sel % ldim_drv->conf->hist_col;
		n = i * 16 * ldim_drv->conf->hist_col + j * 16;
		for (k = 0; k < 16; k++)
			len += sprintf(buf + len, " 0x%x", *(p + n + k));
		pr_info("for_tool: %d 16%s\n", sel, buf);
	}

	kfree(buf);
	kfree(p);
}

static void ldim_matrix_max_rgb_mute_print(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, n, len;
	unsigned int *p = NULL;
	char *buf;

	n = ldim_drv->conf->hist_col * ldim_drv->conf->hist_row * 3 * 30 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->hist_col * ldim_drv->conf->hist_row * 3;
	p = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}
	memcpy(p, ldim_drv->max_rgb, len * sizeof(unsigned int));

	len = 0;
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			n = i * ldim_drv->conf->hist_col + j;
			len += sprintf(buf + len, " %d %d %d",
				p[n * 3],
				p[n * 3 + 1],
				p[n * 3 + 2]);
		}
	}
	pr_info("for_tool: %d 3%s\n",
		(ldim_drv->conf->hist_row * ldim_drv->conf->hist_col), buf);

	kfree(buf);
	kfree(p);
}

static void ldim_stts_data_dump(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, k, m, n, hist_len, max_len;
	unsigned int *p_hist = NULL, *p_max = NULL;
	char *buf;

	n = 16 * 20 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	hist_len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col * 16;
	p_hist = kcalloc(hist_len, sizeof(unsigned int), GFP_KERNEL);
	if (!p_hist)
		goto ldim_stts_data_dump_exit_0;

	max_len = ldim_drv->conf->hist_col * ldim_drv->conf->hist_row * 3;
	p_max = kcalloc(max_len, sizeof(unsigned int), GFP_KERNEL);
	if (!p_max)
		goto ldim_stts_data_dump_exit_1;

	memcpy(p_hist, ldim_drv->hist_matrix, hist_len * sizeof(unsigned int));
	memcpy(p_max, ldim_drv->max_rgb, max_len * sizeof(unsigned int));

	pr_info("%s: hist data:\n", __func__);
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			n = 0;
			m = (i * ldim_drv->conf->hist_col + j) * 16;
			for (k = 0; k < 16; k++)
				n += sprintf(buf + n, " 0x%x,", p_hist[m + k]);
			pr_info("%s\n", buf);
		}
		pr_info("\n");
	}

	pr_info("\n%s: maxrgb data:\n", __func__);
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		n = 0;
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			m = (i * ldim_drv->conf->hist_col + j) * 3;
			n += sprintf(buf + n, "  %d, %d, %d,",
				     p_max[m], p_max[m + 1], p_max[m + 2]);
		}
		pr_info("%s\n", buf);
	}

	kfree(p_max);
ldim_stts_data_dump_exit_1:
	kfree(p_hist);
ldim_stts_data_dump_exit_0:
	kfree(buf);
}

static void ldim_matrix_SF_matrix_mute_print(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, n, len;
	unsigned int *ldim_matrix_t = NULL;
	char *buf;

	len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	n = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;
	ldim_matrix_t = kcalloc(n, sizeof(unsigned int), GFP_KERNEL);
	if (!ldim_matrix_t) {
		kfree(buf);
		return;
	}
	memcpy(ldim_matrix_t, ldim_drv->fw_para->fdat->SF_BL_matrix,
	       (n * sizeof(unsigned int)));

	len = 0;
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", ldim_matrix_t[i]);
	pr_info("for_tool: %d %d%s\n",
		ldim_drv->conf->hist_row, ldim_drv->conf->hist_col, buf);

	kfree(buf);
	kfree(ldim_matrix_t);
}

static void ldim_matrix_LD_remap_LUT_mute_print(void)
{
	unsigned int i, j, len;
	char *buf;

	len = 16 * 32 * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = 0;
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 32; j++)
			len += sprintf(buf + len, " %d", ld_remap_lut[i][j]);
	}

	pr_info("for_tool: 16 32%s\n", buf);

	kfree(buf);
}

static void ldim_matrix_LD_remap_LUT_mute_line_print(int index)
{
	unsigned int j, len;
	char *buf;

	if (index >= 16)
		return;

	len = 32 * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = 0;
	for (j = 0; j < 32; j++)
		len += sprintf(buf + len, " %d", ld_remap_lut[index][j]);

	pr_info("for_tool: %d%s\n", index, buf);

	kfree(buf);
}

static void ldim_test_matrix_mute_print(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, n, len;
	unsigned short *p = ldim_drv->test_matrix;
	char *buf;

	n = ldim_drv->conf->hist_col * ldim_drv->conf->hist_row;
	len = n * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = 0;
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", p[i]);
	pr_info("for_tool: %d %d%s\n",
		ldim_drv->conf->hist_row, ldim_drv->conf->hist_col, buf);

	kfree(buf);
}

static void ldim_matrix_bl_matrix_file_save(struct aml_ldim_driver_s *ldim_drv,
					    char *path)
{
	unsigned int i, n, len;
	unsigned short *p = NULL;
	char *buf;

	len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col * 8 + 30;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	n = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;
	p = kcalloc(n, sizeof(unsigned short), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}
	memcpy(p, ldim_drv->local_ldim_matrix, (n * sizeof(unsigned short)));

	len = sprintf(buf, "for_tool: %d %d",
		      ldim_drv->conf->hist_row, ldim_drv->conf->hist_col);
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", p[i]);
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
	kfree(p);
}

static void ldim_matrix_histgram_file_save(struct aml_ldim_driver_s *ldim_drv,
					   char *path)
{
	unsigned int i, j, k, n, len;
	unsigned int *p = NULL;
	char *buf;

	n = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col * 16;
	len = n * 10 + 30;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	p = kcalloc(n, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}

	memcpy(p, ldim_drv->hist_matrix, n * sizeof(unsigned int));

	len = sprintf(buf, "for_tool: %d 16",
		      ldim_drv->conf->hist_row * ldim_drv->conf->hist_col);
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			for (k = 0; k < 16; k++) {
				n = i * 16 * ldim_drv->conf->hist_col +
					j * 16 + k;
				len += sprintf(buf + len, " 0x%x", *(p + n));
			}
		}
	}
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
	kfree(p);
}

static void ldim_matrix_max_rgb_file_save(struct aml_ldim_driver_s *ldim_drv,
					  char *path)
{
	unsigned int i, j, n, len;
	unsigned int *p = NULL;
	char *buf;

	n = ldim_drv->conf->hist_col * ldim_drv->conf->hist_row * 3 * 30 + 30;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->hist_col * ldim_drv->conf->hist_row * 3;
	p = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}
	memcpy(p, ldim_drv->max_rgb, len * sizeof(unsigned int));

	len = sprintf(buf, "for_tool: %d 3",
		      ldim_drv->conf->hist_row * ldim_drv->conf->hist_col);
	for (i = 0; i < ldim_drv->conf->hist_row; i++) {
		for (j = 0; j < ldim_drv->conf->hist_col; j++) {
			n = i * ldim_drv->conf->hist_col + j;
			len += sprintf(buf + len, " %d %d %d",
				       p[n * 3], p[n * 3 + 1], p[n * 3 + 2]);
		}
	}
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
	kfree(p);
}

static void ldim_matrix_sf_matrix_file_save(struct aml_ldim_driver_s *ldim_drv,
					    char *path)
{
	unsigned int i, n, len;
	unsigned int *p = NULL;
	char *buf;

	len = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col * 10 + 30;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	n = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;
	p = kcalloc(n, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}
	memcpy(p, ldim_drv->fw_para->fdat->SF_BL_matrix,
	       (n * sizeof(unsigned int)));

	len = sprintf(buf, "for_tool: %d %d",
		      ldim_drv->conf->hist_row, ldim_drv->conf->hist_col);
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", p[i]);
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
	kfree(p);
}

static void ldim_test_matrix_file_save(struct aml_ldim_driver_s *ldim_drv,
				       char *path)
{
	unsigned int i, n, len;
	unsigned short *p = ldim_drv->test_matrix;
	char *buf;

	n = ldim_drv->conf->hist_col * ldim_drv->conf->hist_row;
	len = n * 10 + 30;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = sprintf(buf, "for_tool: %d %d",
		      ldim_drv->conf->hist_row, ldim_drv->conf->hist_col);
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", p[i]);
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
}

static void ldim_prm_regs_print(struct LDReg_s *nprm)
{
	char *buf;
	int i, j, len;

	if (!nprm)
		return;

	buf = kcalloc(1024, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	pr_info("reg_LD_BackLit_Xtlk=%d\n"
		"reg_LD_BackLit_mode=%d\n"
		"reg_LD_BkLit_curmod=%d\n"
		"reg_LD_BkLUT_Intmod=%d\n"
		"reg_LD_BkLit_Intmod=%d\n"
		"reg_LD_BkLit_LPFmod=%d\n"
		"reg_LD_Litshft=%d\n"
		"reg_LD_RGBmapping_demo=%d\n"
		"reg_LD_R_LUT_interp_mode=%d\n"
		"reg_LD_G_LUT_interp_mode=%d\n"
		"reg_LD_B_LUT_interp_mode=%d\n",
		nprm->reg_LD_BackLit_Xtlk,
		nprm->reg_LD_BackLit_mode,
		nprm->reg_LD_BkLit_curmod,
		nprm->reg_LD_BkLUT_Intmod,
		nprm->reg_LD_BkLit_Intmod,
		nprm->reg_LD_BkLit_LPFmod,
		nprm->reg_LD_Litshft,
		nprm->reg_LD_RGBmapping_demo,
		nprm->reg_LD_X_LUT_interp_mode[0],
		nprm->reg_LD_X_LUT_interp_mode[1],
		nprm->reg_LD_X_LUT_interp_mode[2]);

	pr_info("reg_LD_Reflect_Hnum=%d\n"
		"reg_LD_Reflect_Vnum=%d\n",
		nprm->reg_LD_Reflect_Hnum,
		nprm->reg_LD_Reflect_Vnum);

	pr_info("reg_LD_BkLit_Celnum=%d\n",
		nprm->reg_LD_BkLit_Celnum);

	len = 0;
	for (i = 0; i < 20; i++)
		len += sprintf(buf + len, " %d,", nprm->reg_LD_Reflect_Hdgr[i]);
	pr_info("reg_LD_Reflect_Hdgr[20]=%s\n", buf);
	len = 0;
	for (i = 0; i < 20; i++)
		len += sprintf(buf + len, " %d,", nprm->reg_LD_Reflect_Vdgr[i]);
	pr_info("reg_LD_Reflect_Vdgr[20]=%s\n", buf);
	len = 0;
	for (i = 0; i < 4; i++)
		len += sprintf(buf + len, " %d,", nprm->reg_LD_Reflect_Xdgr[i]);
	pr_info("reg_LD_Reflect_Xdgr[4]=%s\n", buf);

	pr_info("reg_LD_Vgain=%d\n"
		"reg_LD_Hgain=%d\n",
		nprm->reg_LD_Vgain,
		nprm->reg_LD_Hgain);

	pr_info("reg_LD_Litgain=%d\n",
		nprm->reg_LD_Litgain);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,", nprm->reg_LD_BkLit_valid[i]);
	pr_info("reg_LD_BkLit_valid[%d]=%s\n", LD_LUT_LEN, buf);

	len = 0;
	for (i = 0; i < LD_BLK_LEN_H; i++)
		len += sprintf(buf + len, " %d,", nprm->reg_LD_BLK_Hidx[i]);
	pr_info("reg_LD_BLK_Hidx[%d]=%s\n", LD_BLK_LEN_H, buf);

	len = 0;
	for (i = 0; i < LD_BLK_LEN_V; i++)
		len += sprintf(buf + len, " %d,", nprm->reg_LD_BLK_Vidx[i]);
	pr_info("reg_LD_BLK_Vidx[%d]=%s\n", LD_BLK_LEN_V, buf);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,",
			       nprm->reg_LD_LUT_Hdg_TXLX[0][i]);
	pr_info("reg_LD_LUT_Hdg_0[%d]=%s\n", LD_LUT_LEN, buf);
	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,",
			       nprm->reg_LD_LUT_Hdg_TXLX[1][i]);
	pr_info("reg_LD_LUT_Hdg_1[%d]=%s\n", LD_LUT_LEN, buf);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,",
			       nprm->reg_LD_LUT_Vdg_TXLX[0][i]);
	pr_info("reg_LD_LUT_Vdg_0[%d]=%s\n", LD_LUT_LEN, buf);
	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,",
			       nprm->reg_LD_LUT_Vdg_TXLX[1][i]);
	pr_info("reg_LD_LUT_Vdg_1[%d]=%s\n", LD_LUT_LEN, buf);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,",
			       nprm->reg_LD_LUT_VHk_TXLX[0][i]);
	pr_info("reg_LD_LUT_VHk_0[%d]=%s\n", LD_LUT_LEN, buf);
	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,",
			       nprm->reg_LD_LUT_VHk_TXLX[1][i]);
	pr_info("reg_LD_LUT_VHk_1[%d]=%s\n", LD_LUT_LEN, buf);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,",
			       nprm->reg_LD_LUT_VHk_pos[i]);
	pr_info("reg_LD_LUT_VHk_pos[%d]=%s\n", LD_LUT_LEN, buf);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,", nprm->reg_LD_LUT_VHk_neg[i]);
	pr_info("reg_LD_LUT_VHk_neg[%d]=%s\n", LD_LUT_LEN, buf);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,", nprm->reg_LD_LUT_HHk[i]);
	pr_info("reg_LD_LUT_HHk[%d]=%s\n", LD_LUT_LEN, buf);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,", nprm->reg_LD_LUT_VHo_pos[i]);
	pr_info("reg_LD_LUT_VHo_pos[%d]=%s\n", LD_LUT_LEN, buf);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,", nprm->reg_LD_LUT_VHo_neg[i]);
	pr_info("reg_LD_LUT_VHo_neg[%d]=%s\n", LD_LUT_LEN, buf);

	pr_info("reg_LD_LUT_VHo_LS=%d\n", nprm->reg_LD_LUT_VHo_LS);

	pr_info("reg_LD_LUT_Hdg_LEXT_0=%d\n",
		nprm->reg_LD_LUT_Hdg_LEXT_TXLX[0]);
	pr_info("reg_LD_LUT_Hdg_LEXT_1=%d\n",
		nprm->reg_LD_LUT_Hdg_LEXT_TXLX[1]);

	pr_info("reg_LD_LUT_Vdg_LEXT_0=%d\n",
		nprm->reg_LD_LUT_Vdg_LEXT_TXLX[0]);
	pr_info("reg_LD_LUT_Vdg_LEXT_1=%d\n",
		nprm->reg_LD_LUT_Vdg_LEXT_TXLX[1]);

	pr_info("reg_LD_LUT_VHk_LEXT_0=%d\n",
		nprm->reg_LD_LUT_VHk_LEXT_TXLX[0]);
	pr_info("reg_LD_LUT_VHk_LEXT_1=%d\n",
		nprm->reg_LD_LUT_VHk_LEXT_TXLX[1]);

	pr_info("reg_BL_matrix_AVG=%d\n", nprm->reg_BL_matrix_AVG);
	pr_info("reg_BL_matrix_Compensate=%d\n",
		nprm->reg_BL_matrix_Compensate);

	len = 0;
	j = 0;
	pr_info("reg_LD_LUT_Id[1536]=\n");
	for (i = 0; i < 1536; i++) {
		len += sprintf(buf + len, " %d,", nprm->reg_LD_LUT_Id[i]);
		j++;
		if (j >= 32) {
			pr_info("%s\n", buf);
			j = 0;
			len = 0;
		}
	}
	if (len > 0)
		pr_info("%s\n", buf);

	ldim_ld_remap_lut_print(0xff);

	kfree(buf);
}

static void ldim_prm_reg_ld_lut_mute_print(unsigned int *p, int n)
{
	unsigned int i, len;
	char *buf;

	len = n * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = 0;
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", p[i]);
	pr_info("for_tool: %d %s\n", n, buf);

	kfree(buf);
}

static void ldim_profile_print(struct ldim_profile_s *ld_profile)
{
	char *buf;
	int i, len;

	if (!ld_profile) {
		pr_info("%s: ld_profile is null\n", __func__);
		return;
	}

	buf = kcalloc(1024, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	pr_info("bl_profile_mode=%d\n", ld_profile->ld_profile_mode);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,", ld_profile->ld_lut_hdg[i]);
	pr_info("ld_lut_hdg:[%d]=%s\n", LD_LUT_LEN, buf);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,", ld_profile->ld_lut_vdg[i]);
	pr_info("ld_lut_vdg[%d]=%s\n", LD_LUT_LEN, buf);

	len = 0;
	for (i = 0; i < LD_LUT_LEN; i++)
		len += sprintf(buf + len, " %d,", ld_profile->ld_lut_vhk[i]);
	pr_info("ld_lut_vhk[%d]=%s\n", LD_LUT_LEN, buf);

	kfree(buf);
}

static void ldim_set_matrix(unsigned int *data, unsigned int reg_sel,
			    unsigned int cnt)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned char avg_update_en_bak, matrix_update_en_bak;

	avg_update_en_bak = ldim_drv->avg_update_en;
	matrix_update_en_bak = ldim_drv->matrix_update_en;
	ldim_drv->avg_update_en = 0;
	ldim_drv->matrix_update_en = 0;
	if (avg_update_en_bak || matrix_update_en_bak)
		msleep(20);

	/* gMatrix_LUT: s12*100 */
	if (reg_sel == 0)
		ldim_wr_lut(REG_LD_BLK_VIDX_BASE, data, 16, 32);
	else if (reg_sel == 2)
		ldim_wr_lut(REG_LD_LUT_VHK_NEGPOS_BASE, data, 16, 32);
	else if (reg_sel == 3)
		ldim_wr_lut(REG_LD_LUT_VHO_NEGPOS_BASE, data, 16, 4);
	else if (reg_sel == REG_LD_LUT_HDG_BASE)
		ldim_wr_lut(REG_LD_LUT_HDG_BASE, data, 16, cnt);
	else if (reg_sel == REG_LD_LUT_VHK_BASE)
		ldim_wr_lut(REG_LD_LUT_VHK_BASE, data, 16, cnt);
	else if (reg_sel == REG_LD_LUT_VDG_BASE)
		ldim_wr_lut(REG_LD_LUT_VDG_BASE, data, 16, cnt);

	ldim_drv->avg_update_en = avg_update_en_bak;
	ldim_drv->matrix_update_en = matrix_update_en_bak;
}

static ssize_t ldim_attr_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len,
		       "\necho hist > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo maxrgb > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo matrix > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo ldim_matrix_get 7 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo ldim_matrix_get 0/1/2/3 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo info > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo alg > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo litgain 4096 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo test_mode 1 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo test_set 0 4095 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo test_set_all 4095 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf + len,
		       "echo ldim_stts_init 8 2 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo remap_init 8 2 0 1 0 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf + len,
		       "echo fw_LD_ThSF_l 1600 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo fw_LD_ThTF_l 32 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo boost_gain 4 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo alpha_delta 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo boost_gain_neg 4 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo TF_alpha 256 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo lpf_gain 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo lpf_res 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo rgb_base 128 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf + len,
		       "echo ov_gain 16 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo avg_gain 2048 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf + len,
		       "echo LPF_method 3 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo LD_TF_STEP_TH 100 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo TF_step_method 3 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo TF_FRESH_BL 8 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf + len,
		       "echo TF_BLK_FRESH_BL 5 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo side_blk_diff_th 100 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo bbd_th 200 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo bbd_detect_en 1 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo diff_blk_luma_en 1 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf + len,
		       "echo bl_remap_curve > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo fw_ld_whist > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo ld_remap_lut > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf + len,
		       "echo Sf_bypass 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo Boost_light_bypass 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo Lpf_bypass 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo Ld_remap_bypass 0 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf + len,
		       "echo fw_hist_print 1 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo fw_print_frequent 8 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo Dbprint_lv 1 > /sys/class/aml_ldim/attr\n");

	return len;
}

static ssize_t ldim_attr_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_para_s *fw_para = ldim_drv->fw_para;
	struct fw_ctrl_config_s *fw_ctrl;
	unsigned int n = 0;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};
	unsigned int size;
	int i, j;
	char *pr_buf;
	ssize_t pr_len = 0;

	unsigned int hist_row = 0, hist_col = 0;
	unsigned int blk_mode = 0, ldim_bl_en = 0, hvcnt_bypass = 0;
	unsigned long val1 = 0;

	if (!fw_para)
		return len;
	if (!fw_para->ctrl)
		return len;
	fw_ctrl = fw_para->ctrl;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return len;
	parm = kcalloc(520, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		return len;
	}
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "hist")) {
		if (parm[2]) {
			if (!strcmp(parm[1], "r")) {
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_attr_store_err;
				ldim_matrix_histgram_mute_print(ldim_drv, i);
				goto ldim_attr_store_end;
			}
			if (!strcmp(parm[1], "rf")) {
				ldim_matrix_histgram_file_save(ldim_drv,
							       parm[2]);
				goto ldim_attr_store_end;
			}
		} else if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_matrix_histgram_mute_print
					(ldim_drv, 0xffff);
				goto ldim_attr_store_end;
			}
		}
		ldim_dump_histgram(ldim_drv);
	} else if (!strcmp(parm[0], "maxrgb")) {
		if (parm[2]) {
			if (!strcmp(parm[1], "rf")) {
				ldim_matrix_max_rgb_file_save(ldim_drv,
							      parm[2]);
				goto ldim_attr_store_end;
			}
		} else if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_matrix_max_rgb_mute_print(ldim_drv);
				goto ldim_attr_store_end;
			}
		}
		ldim_get_matrix_info_max_rgb(ldim_drv);
	} else if (!strcmp(parm[0], "matrix")) {
		if (parm[2]) {
			if (!strcmp(parm[1], "rf")) {
				ldim_matrix_bl_matrix_file_save(ldim_drv,
								parm[2]);
				goto ldim_attr_store_end;
			}
		} else if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_matrix_bl_matrix_mute_print(ldim_drv);
				goto ldim_attr_store_end;
			}
		}
		ldim_get_matrix_info(ldim_drv);
	} else if (!strcmp(parm[0], "SF_matrix")) {
		if (parm[2]) {
			if (!strcmp(parm[1], "rf")) {
				ldim_matrix_sf_matrix_file_save(ldim_drv,
								parm[2]);
				goto ldim_attr_store_end;
			}
		} else if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_matrix_SF_matrix_mute_print(ldim_drv);
				goto ldim_attr_store_end;
			}
		}
		ldim_get_matrix_info_SF(ldim_drv);
	} else if (!strcmp(parm[0], "stts")) {
		ldim_stts_data_dump(ldim_drv);
	} else if (!strcmp(parm[0], "db_load")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", ldim_drv->load_db_en);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->load_db_en = val1 ? 1 : 0;
		}
		pr_info("fw para load db: %d\n", ldim_drv->load_db_en);
	} else if (!strcmp(parm[0], "db_print")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					ldim_drv->db_print_flag);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->db_print_flag = val1 ? 1 : 0;
		}
		pr_info("db_print_flag: %d\n", ldim_drv->db_print_flag);
	} else if (!strcmp(parm[0], "db_dump")) {
		ldim_db_para_print(fw_para);
	} else if (!strcmp(parm[0], "remap_init")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d %d %d %d %d\n",
					ldim_drv->conf->hist_row,
					ldim_drv->conf->hist_col,
					ldim_drv->conf->bl_mode,
					ldim_drv->conf->remap_en,
					ldim_drv->conf->hvcnt_bypass);
				goto ldim_attr_store_end;
			}
		}
		if (parm[5]) {
			if (kstrtouint(parm[1], 10, &hist_row) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &hist_col) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[3], 10, &blk_mode) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[4], 10, &ldim_bl_en) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[5], 10, &hvcnt_bypass) < 0)
				goto ldim_attr_store_err;

			ldim_drv->conf->hist_row = (unsigned char)hist_row;
			ldim_drv->conf->hist_col = (unsigned char)hist_col;
			ldim_drv->conf->bl_mode = (unsigned char)blk_mode;
			ldim_drv->conf->remap_en = (unsigned char)ldim_bl_en;
			ldim_drv->conf->hvcnt_bypass =
					(unsigned char)hvcnt_bypass;
			fw_para->nprm->reg_LD_STA_Vnum =
					ldim_drv->conf->hist_row;
			fw_para->nprm->reg_LD_STA_Hnum =
					ldim_drv->conf->hist_col;
			fw_para->nprm->reg_LD_BLK_Vnum =
					ldim_drv->conf->hist_row;
			fw_para->nprm->reg_LD_BLK_Hnum =
					ldim_drv->conf->hist_col;
			fw_para->nprm->reg_LD_BackLit_mode =
					ldim_drv->conf->bl_mode;

			/* update in vsync isr */
			ldim_drv->remap_en = ldim_drv->conf->remap_en;
			pr_info("**************ldim init ok*************\n");
		}
		pr_info("remap_init param: %d %d %d %d %d\n",
			ldim_drv->conf->hist_row, ldim_drv->conf->hist_col,
			ldim_drv->conf->bl_mode,
			ldim_drv->conf->remap_en,
			ldim_drv->conf->hvcnt_bypass);
	} else if (!strcmp(parm[0], "ldim_stts_init")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d %d\n",
					ldim_drv->conf->hist_row,
					ldim_drv->conf->hist_col);
				goto ldim_attr_store_end;
			}
		}
		if (parm[2]) {
			if (kstrtouint(parm[1], 10, &hist_row) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &hist_col) < 0)
				goto ldim_attr_store_err;

			ldim_drv->conf->hist_row = (unsigned char)hist_row;
			ldim_drv->conf->hist_col = (unsigned char)hist_col;
			ldim_stts_initial(ldim_drv->conf->hsize,
					  ldim_drv->conf->vsize,
					  ldim_drv->conf->hist_row,
					  ldim_drv->conf->hist_col);
			pr_info("************ldim stts init ok*************\n");
		}
		pr_info("ldim_stts_init param: %d %d\n",
			ldim_drv->conf->hist_row, ldim_drv->conf->hist_col);
	} else if (!strcmp(parm[0], "frm_size")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d %d\n",
					ldim_drv->conf->hsize,
					ldim_drv->conf->vsize);
				goto ldim_attr_store_end;
			}
		}
		pr_info("frm_width: %d, frm_height: %d\n",
			ldim_drv->conf->hsize, ldim_drv->conf->vsize);
	} else if (!strcmp(parm[0], "func")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					ldim_drv->conf->func_en);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->conf->func_en = val1 ? 1 : 0;
			ldim_func_ctrl(ldim_drv->conf->func_en);
		}
		pr_info("ldim_func_en: %d\n", ldim_drv->conf->func_en);
	} else if (!strcmp(parm[0], "remap")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					ldim_drv->conf->remap_en);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			if (val1) {
				if (ldim_drv->conf->remap_en == 0) {
					ldim_drv->conf->remap_en = 1;
					if (ldim_drv->func_en)
						ldim_remap_ctrl(1);
				}
			} else {
				if (ldim_drv->conf->remap_en) {
					ldim_drv->conf->remap_en = 0;
					if (ldim_drv->func_en)
						ldim_remap_ctrl(0);
				}
			}
		}
		pr_info("ldim_remap_en: %d\n", ldim_drv->conf->remap_en);
	} else if (!strcmp(parm[0], "demo")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", ldim_drv->demo_en);
				goto ldim_attr_store_end;
			}
			if (ldim_drv->remap_en == 0) {
				pr_info("ldim_remap is disabled\n");
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			if (val1) {
				ldim_drv->demo_en = 1;
				ldim_drv->fw_para->nprm->reg_LD_RGBmapping_demo
						= 1;
				ldim_remap_regs_update();
			} else {
				ldim_drv->demo_en = 0;
				ldim_drv->fw_para->nprm->reg_LD_RGBmapping_demo
						= 0;
				ldim_remap_regs_update();
			}
		}
		pr_info("ldim_demo_en: %d\n", ldim_drv->demo_en);
	} else if (!strcmp(parm[0], "ldim_matrix_get")) {
		unsigned int data[32] = {0};
		unsigned int k, g, reg_sel = 0;

		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &reg_sel) < 0)
				goto ldim_attr_store_err;

			pr_buf = kzalloc(sizeof(char) * 200, GFP_KERNEL);
			if (!pr_buf) {
				LDIMERR("buf malloc error\n");
				goto ldim_attr_store_err;
			}
			ldim_get_matrix(ldim_drv, &data[0], reg_sel);
			if ((reg_sel == 0) || (reg_sel == 1)) {
				pr_info("******ldim matrix info start******\n");
				for (k = 0; k < 4; k++) {
					pr_len = 0;
					for (g = 0; g < 8; g++) {
						pr_len +=
						sprintf(pr_buf + pr_len, "\t%d",
							data[8 * k + g]);
					}
					pr_info("%s\n", pr_buf);
				}
				pr_info("*******ldim matrix info end*******\n");
			}
			kfree(pr_buf);
		}
	} else if (!strcmp(parm[0], "ldim_matrix_set")) {
		unsigned int data_set[32] = {0};
		unsigned int reg_sel_1 = 0, k1, cnt1 = 0;

		if (parm[2]) {
			if (kstrtouint(parm[1], 10, &reg_sel_1) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &cnt1) < 0)
				goto ldim_attr_store_err;

			for (k1 = 0; k1 < cnt1; k1++) {
				if (parm[k1 + 2]) {
					if (kstrtouint(parm[k1 + 2], 10,
						       &data_set[k1]) < 0)
						goto ldim_attr_store_err;
				}
			}
			ldim_set_matrix(&data_set[0], reg_sel_1, cnt1);
			pr_info("***********ldim matrix set over***********\n");
		}
	} else if (!strcmp(parm[0], "data_min")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", ldim_drv->data_min);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &ldim_drv->data_min) < 0)
				goto ldim_attr_store_err;

			ldim_drv->set_level(ldim_drv->brightness_level);
		}
		LDIMPR("brightness data_min: %d\n", ldim_drv->data_min);
	} else if (!strcmp(parm[0], "litgain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", ldim_drv->litgain);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &ldim_drv->litgain) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("litgain = %d\n", ldim_drv->litgain);
	} else if (!strcmp(parm[0], "litstep_en")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", ldim_drv->litstep_en);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->litstep_en = val1 ? 1 : 0;
		}
		pr_info("litstep_en = %d\n", ldim_drv->litstep_en);
	} else if (!strcmp(parm[0], "brightness_bypass")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					ldim_drv->brightness_bypass);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->brightness_bypass = (unsigned char)val1;
			if (ldim_drv->brightness_bypass == 0)
				ldim_drv->set_level(ldim_drv->brightness_level);
		}
		pr_info("brightness_bypass = %d\n",
			ldim_drv->brightness_bypass);
	} else if (!strcmp(parm[0], "test_mode")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", ldim_drv->test_en);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->test_en = (unsigned char)val1;
		}
		LDIMPR("test_mode: %d\n", ldim_drv->test_en);
	} else if (!strcmp(parm[0], "test_set")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_test_matrix_mute_print(ldim_drv);
				goto ldim_attr_store_end;
			}
			if (!strcmp(parm[1], "w")) {
				size = ldim_drv->conf->hist_row *
					ldim_drv->conf->hist_col;
				if (!parm[size + 3])
					goto ldim_attr_store_err;
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_attr_store_err;
				if (kstrtouint(parm[3], 10, &j) < 0)
					goto ldim_attr_store_err;
				if ((i != ldim_drv->conf->hist_row) ||
				    (j != ldim_drv->conf->hist_col))
					goto ldim_attr_store_err;
				for (i = 0; i < size; i++) {
					if (kstrtouint(parm[i + 4], 10, &j) < 0)
						goto ldim_attr_store_err;
					ldim_drv->test_matrix[i] =
						(unsigned short)j;
				}
				goto ldim_attr_store_end;
			}
		}
		if (parm[2]) {
			if (!strcmp(parm[1], "rf")) {
				ldim_test_matrix_file_save(ldim_drv, parm[2]);
				goto ldim_attr_store_end;
			}

			if (kstrtouint(parm[1], 10, &i) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &j) < 0)
				goto ldim_attr_store_err;

			size = ldim_drv->conf->hist_row *
				ldim_drv->conf->hist_col;
			if (i < size) {
				ldim_drv->test_matrix[i] = (unsigned short)j;
				LDIMPR("set test_matrix[%d] = %4d\n", i, j);
			} else {
				LDIMERR("invalid index for test_matrix[%d]\n",
					i);
			}
			goto ldim_attr_store_end;
		}
		ldim_get_test_matrix_info(ldim_drv);
	} else if (!strcmp(parm[0], "test_set_all")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &j) < 0)
				goto ldim_attr_store_err;

			size = ldim_drv->conf->hist_row *
				ldim_drv->conf->hist_col;
			for (i = 0; i < size; i++)
				ldim_drv->test_matrix[i] = (unsigned short)j;

			LDIMPR("set all test_matrix to %4d\n", j);
			goto ldim_attr_store_end;
		}
		ldim_get_test_matrix_info(ldim_drv);
	} else if (!strcmp(parm[0], "bl_remap_curve")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_sel_int_matrix_mute_print
					(ldim_drv, 16, fw_para->bl_remap_curve);
				goto ldim_attr_store_end;
			}
		}
		if (parm[16]) {
			for (i = 0; i < 16; i++) {
				if (kstrtouint(parm[i + 1], 10,
					       &fw_para->bl_remap_curve[i]) < 0)
					goto ldim_attr_store_err;
			}
		}
		ldim_bl_remap_curve_print(fw_para->bl_remap_curve);
	} else if (!strcmp(parm[0], "fw_LD_Whist") ||
		   !strcmp(parm[0], "fw_ld_whist")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_sel_int_matrix_mute_print
					(ldim_drv, 16, fw_para->fw_ld_whist);
				goto ldim_attr_store_end;
			}
		}
		if (parm[16]) {
			for (i = 0; i < 16; i++) {
				if (kstrtouint(parm[i + 1], 10,
					       &fw_para->fw_ld_whist[i]) < 0) {
					goto ldim_attr_store_err;
				}
			}
		}
		ldim_fw_LD_Whist_print(fw_para->fw_ld_whist);
	} else if (!strcmp(parm[0], "ld_remap_lut")) {
		if (!parm[1]) {
			ldim_ld_remap_lut_print(0xff);
			goto ldim_attr_store_end;
		}
		if (!strcmp(parm[1], "r")) {
			ldim_matrix_LD_remap_LUT_mute_print();
			goto ldim_attr_store_end;
		}
		if (!strcmp(parm[1], "lr")) { /* line read */
			if (kstrtouint(parm[2], 10, &i) < 0)
				goto ldim_attr_store_err;
			ldim_matrix_LD_remap_LUT_mute_line_print(i);
			goto ldim_attr_store_end;
		}
		if (!strcmp(parm[1], "w")) {
			if (!parm[515])
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &i) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[3], 10, &j) < 0)
				goto ldim_attr_store_err;
			if ((i != 16) || (j != 32))
				goto ldim_attr_store_err;
			for (i = 0; i < 16; i++) {
				for (j = 0; j < 32; j++) {
					if (kstrtouint(parm[i * 32 + j + 4], 10,
						       &ld_remap_lut[i][j]) < 0)
						goto ldim_attr_store_err;
				}
			}
			ldim_func_init_remap_lut(fw_para->nprm);
			i = 0;
			while (ldim_drv->remap_ram_step != 2) {
				if (i++ >= 5) {
					LDIMERR
					("%s: wait remap_ram_step timeout\n",
					 __func__);
					break;
				}
				msleep(20);
			}
			ldim_drv->remap_ram_flag = 1;
			ldim_drv->remap_ram_step = 0;
			ldim_remap_regs_update();
			goto ldim_attr_store_end;
		}
		if (!strcmp(parm[1], "lw")) { /* line write */
			if (!parm[2]) {
				ldim_func_init_remap_lut(fw_para->nprm);
				i = 0;
				while (ldim_drv->remap_ram_step != 2) {
					if (i++ >= 5) {
						LDIMERR
					("%s: wait remap_ram_step timeout\n",
					 __func__);
						break;
					}
					msleep(20);
				}
				ldim_drv->remap_ram_flag = 1;
				ldim_drv->remap_ram_step = 0;
				ldim_remap_regs_update();
				goto ldim_attr_store_end;
			}
			if (!parm[34])
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &i) < 0)
				goto ldim_attr_store_err;
			for (j = 0; j < 32; j++) {
				if (kstrtouint(parm[j + 3], 10,
					       &ld_remap_lut[i][j]) < 0) {
					goto ldim_attr_store_err;
				}
			}
		}

		if (kstrtouint(parm[1], 10, &i) < 0)
			goto ldim_attr_store_err;
		if (parm[33]) {
			for (j = 0; j < 32; j++) {
				if (kstrtouint(parm[j + 2], 10,
					       &ld_remap_lut[i][j]) < 0) {
					goto ldim_attr_store_err;
				}
			}
		}
		ldim_ld_remap_lut_print(i);
	} else if (!strcmp(parm[0], "Sf_bypass")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->Sf_bypass = (unsigned char)val1;
		}
		pr_info("Sf_bypass = %d\n", fw_ctrl->Sf_bypass);
	} else if (!strcmp(parm[0], "Boost_light_bypass")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->Boost_light_bypass = (unsigned char)val1;
		}
		pr_info("Boost_light_bypass = %d\n",
			fw_ctrl->Boost_light_bypass);
	} else if (!strcmp(parm[0], "Lpf_bypass")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->Lpf_bypass = (unsigned char)val1;
		}
		pr_info("Lpf_bypass = %d\n", fw_ctrl->Lpf_bypass);
	} else if (!strcmp(parm[0], "Ld_remap_bypass")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->Ld_remap_bypass);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->Ld_remap_bypass = (unsigned char)val1;
		}
		pr_info("Ld_remap_bypass = %d\n", fw_ctrl->Ld_remap_bypass);
	} else if (!strcmp(parm[0], "ov_gain")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &fw_ctrl->ov_gain) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("ov_gain = %d\n", fw_ctrl->ov_gain);
	} else if (!strcmp(parm[0], "fw_LD_ThSF_l")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->fw_LD_ThSF_l);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->fw_LD_ThSF_l) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("fw_LD_ThSF_l = %d\n", fw_ctrl->fw_LD_ThSF_l);
	} else if (!strcmp(parm[0], "fw_LD_ThTF_l")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->fw_LD_ThTF_l);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->fw_LD_ThTF_l) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("fw_LD_ThTF_l = %d\n", fw_ctrl->fw_LD_ThTF_l);
	} else if (!strcmp(parm[0], "fw_ld_thist")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->fw_ld_thist);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->fw_ld_thist) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("fw_ld_thist = %d\n", fw_ctrl->fw_ld_thist);
	} else if (!strcmp(parm[0], "boost_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->boost_gain);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->boost_gain) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("boost_gain = %d\n", fw_ctrl->boost_gain);
	} else if (!strcmp(parm[0], "boost_gain_neg")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->boost_gain_neg) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("boost_gain_neg = %d\n", fw_ctrl->boost_gain_neg);
	} else if (!strcmp(parm[0], "alpha_delta")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->alpha_delta) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("alpha_delta = %d\n", fw_ctrl->alpha_delta);
	} else if (!strcmp(parm[0], "TF_alpha")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->TF_alpha);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->TF_alpha) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("TF_alpha = %d\n", fw_ctrl->TF_alpha);
	} else if (!strcmp(parm[0], "sc_method")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->sc_method);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->sc_method) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("sc_method = %d\n", fw_ctrl->sc_method);
	} else if (!strcmp(parm[0], "sc_step")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->sc_step);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->sc_step) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("sc_step = %d\n", fw_ctrl->sc_step);
	} else if (!strcmp(parm[0], "lpf_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->lpf_gain);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->lpf_gain) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("lpf_gain = %d\n", fw_ctrl->lpf_gain);
	} else if (!strcmp(parm[0], "lpf_res")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->lpf_res);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->lpf_res) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("lpf_res = %d\n", fw_ctrl->lpf_res);
	} else if (!strcmp(parm[0], "rgb_base")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->rgb_base);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->rgb_base) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("rgb_base = %d\n", fw_ctrl->rgb_base);
	} else if (!strcmp(parm[0], "avg_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->avg_gain);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->avg_gain) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("avg_gain = %d\n", fw_ctrl->avg_gain);
	} else if (!strcmp(parm[0], "fw_rgb_diff_th")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->fw_rgb_diff_th);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->fw_rgb_diff_th) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("fw_rgb_diff_th = %d\n", fw_ctrl->fw_rgb_diff_th);
	} else if (!strcmp(parm[0], "max_luma")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &fw_ctrl->max_luma) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("max_luma = %d\n", fw_ctrl->max_luma);
	} else if (!strcmp(parm[0], "lmh_avg_TH")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &fw_ctrl->lmh_avg_TH) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("lmh_avg_TH = %d\n", fw_ctrl->lmh_avg_TH);
	} else if (!strcmp(parm[0], "fw_TF_sum_th")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &fw_ctrl->fw_TF_sum_th) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("fw_TF_sum_th = %d\n", fw_ctrl->fw_TF_sum_th);
	} else if (!strcmp(parm[0], "LPF_method")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->LPF_method);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->LPF_method) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("LPF_method = %d\n", fw_ctrl->LPF_method);
	} else if (!strcmp(parm[0], "LD_TF_STEP_TH")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->LD_TF_STEP_TH);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->LD_TF_STEP_TH) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("LD_TF_STEP_TH = %d\n", fw_ctrl->LD_TF_STEP_TH);
	} else if (!strcmp(parm[0], "TF_step_method")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->TF_step_method) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("TF_step_method = %d\n", fw_ctrl->TF_step_method);
	} else if (!strcmp(parm[0], "TF_FRESH_BL")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->TF_FRESH_BL);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->TF_FRESH_BL) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("TF_FRESH_BL = %d\n", fw_ctrl->TF_FRESH_BL);
	} else if (!strcmp(parm[0], "TF_BLK_FRESH_BL")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->TF_BLK_FRESH_BL);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->TF_BLK_FRESH_BL) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("TF_BLK_FRESH_BL = %d\n", fw_ctrl->TF_BLK_FRESH_BL);
	} else if (!strcmp(parm[0], "bbd_detect_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->bbd_detect_en = (unsigned char)val1;
		}
		pr_info("bbd_detect_en = %d\n", fw_ctrl->bbd_detect_en);
	} else if (!strcmp(parm[0], "side_blk_diff_th")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->side_blk_diff_th);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->side_blk_diff_th) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("side_blk_diff_th = %d\n", fw_ctrl->side_blk_diff_th);
	} else if (!strcmp(parm[0], "bbd_th")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->bbd_th);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->bbd_th) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("bbd_th = %d\n", fw_ctrl->bbd_th);
	} else if (!strcmp(parm[0], "diff_blk_luma_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->diff_blk_luma_en = (unsigned char)val1;
		}
		pr_info("diff_blk_luma_en = %d\n", fw_ctrl->diff_blk_luma_en);
	} else if (!strcmp(parm[0], "avg_update_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->avg_update_en = (unsigned char)val1;
		}
		pr_info("avg_update_en = %d\n", ldim_drv->avg_update_en);
	} else if (!strcmp(parm[0], "matrix_update_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->matrix_update_en = (unsigned char)val1;
		}
		pr_info("matrix_update_en = %d\n", ldim_drv->matrix_update_en);
	} else if (!strcmp(parm[0], "alg_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->alg_en = (unsigned char)val1;
		}
		pr_info("ldim_alg_en = %d\n", ldim_drv->alg_en);
	} else if (!strcmp(parm[0], "top_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->top_en = (unsigned char)val1;
		}
		pr_info("ldim_top_en = %d\n", ldim_drv->top_en);
	} else if (!strcmp(parm[0], "hist_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->hist_en = (unsigned char)val1;
		}
		pr_info("ldim_hist_en = %d\n", ldim_drv->hist_en);
	} else if (!strcmp(parm[0], "black_frm_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->black_frm_en = (unsigned char)val1;
		}
		pr_info("black_frm_en = %d\n", fw_ctrl->black_frm_en);
	} else if (!strcmp(parm[0], "black_frm_rgbmax_th")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->black_frm_rgbmax_th = val1;
		}
		pr_info("black_frm_rgbmax_th = %d\n",
			fw_ctrl->black_frm_rgbmax_th);
	} else if (!strcmp(parm[0], "black_frm_cnt_th")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->black_frm_cnt_th = val1;
		}
		pr_info("black_frm_cnt_th = %d\n", fw_ctrl->black_frm_cnt_th);
	} else if (!strcmp(parm[0], "white_area_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->white_area_remap_en = (unsigned char)val1;
		}
		pr_info("white_area_remap_en = %d\n",
			fw_ctrl->white_area_remap_en);
	} else if (!strcmp(parm[0], "white_area_map")) {
		if (parm[4]) {
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->white_area_th_max) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10,
				       &fw_ctrl->white_area_th_min) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[3], 10,
				       &fw_ctrl->white_lvl_th_max) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[4], 10,
				       &fw_ctrl->white_lvl_th_min) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("white_area_th max = %d, min = %d\n",
			fw_ctrl->white_area_th_max,
			fw_ctrl->white_area_th_min);
		pr_info("white_lvl_th max = %d, min = %d\n",
			fw_ctrl->white_lvl_th_max,
			fw_ctrl->white_lvl_th_min);
	} else if ((!strcmp(parm[0], "fw_ld_blest_acmode")) ||
		   (!strcmp(parm[0], "fw_LD_BLEst_ACmode"))) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->fw_ld_blest_acmode);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
			    &fw_ctrl->fw_ld_blest_acmode) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("fw_LD_BLEst_ACmode = %d\n",
			fw_ctrl->fw_ld_blest_acmode);
	} else if (!strcmp(parm[0], "min_bl_alpha")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->min_bl_alpha);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->min_bl_alpha) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("min_bl_alpha = %d\n", fw_ctrl->min_bl_alpha);
	} else if (!strcmp(parm[0], "glb_blend_alpha")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->glb_blend_alpha);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
			    &fw_ctrl->glb_blend_alpha) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("glb_blend_alpha = %d\n", fw_ctrl->glb_blend_alpha);
	} else if (!strcmp(parm[0], "fw_hist_print")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_para->fw_hist_print = (unsigned char)val1;
		}
		pr_info("fw_hist_print = %d\n", fw_para->fw_hist_print);
	} else if (!strcmp(parm[0], "fw_print_frequent")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_para->fw_print_frequent);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_para->fw_print_frequent) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("fw_print_frequent = %d\n", fw_para->fw_print_frequent);
	} else if ((!strcmp(parm[0], "Dbprint_lv")) ||
		   (!strcmp(parm[0], "alg_print"))) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_para->Dbprint_lv);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_para->Dbprint_lv) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("fw Dbprint_lv = %d\n", fw_para->Dbprint_lv);
	} else if (!strcmp(parm[0], "info")) {
		pr_info("ldim_drv_ver: %s\n", LDIM_DRV_VER);
		if (ldim_drv->config_print)
			ldim_drv->config_print();
		pr_info("\nldim_hist_row         = %d\n"
			"ldim_hist_col         = %d\n"
			"ldim_bl_mode          = %d\n\n",
			ldim_drv->conf->hist_row, ldim_drv->conf->hist_col,
			ldim_drv->conf->bl_mode);
		pr_info("state                 = 0x%x\n"
			"ldim_on_flag          = %d\n"
			"ldim_func_en          = %d\n"
			"ldim_remap_en         = %d\n"
			"ldim_demo_en          = %d\n"
			"ldim_func_bypass      = %d\n"
			"ldim_ld_sel           = %d\n"
			"ldim_test_en          = %d\n"
			"ldim_avg_update_en    = %d\n"
			"ldim_matrix_update_en = %d\n"
			"ldim_alg_en           = %d\n"
			"ldim_top_en           = %d\n"
			"ldim_hist_en          = %d\n"
			"ldim_data_min         = %d\n"
			"ldim_data_max         = %d\n"
			"ldim_irq_cnt          = %d\n\n",
			ldim_drv->state,
			ldim_drv->init_on_flag, ldim_drv->func_en,
			ldim_drv->remap_en, ldim_drv->demo_en,
			ldim_drv->func_bypass, ldim_drv->ld_sel,
			ldim_drv->test_en, ldim_drv->avg_update_en,
			ldim_drv->matrix_update_en,
			ldim_drv->alg_en, ldim_drv->top_en,
			ldim_drv->hist_en,
			ldim_drv->data_min, ldim_drv->data_max,
			ldim_drv->irq_cnt);
		pr_info("nprm.reg_LD_BLK_Hnum   = %d\n"
			"nprm.reg_LD_BLK_Vnum   = %d\n"
			"nprm.reg_LD_pic_RowMax = %d\n"
			"nprm.reg_LD_pic_ColMax = %d\n\n",
			fw_para->nprm->reg_LD_BLK_Hnum,
			fw_para->nprm->reg_LD_BLK_Vnum,
			fw_para->nprm->reg_LD_pic_RowMax,
			fw_para->nprm->reg_LD_pic_ColMax);
		pr_info("litgain                = %d\n"
			"white_area             = %d\n"
			"white_lv               = %d\n",
			ldim_drv->litgain, fw_ctrl->white_area,
			fw_ctrl->white_lvl);
	} else if (!strcmp(parm[0], "print")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_debug_print = (unsigned char)val1;
		}
		pr_info("ldim_debug_print = %d\n", ldim_debug_print);
	} else if (!strcmp(parm[0], "alg")) {
		if (fw_para->fw_alg_para_print)
			fw_para->fw_alg_para_print(fw_para);
		else
			pr_info("ldim_alg para_print is null\n");
	} else {
		pr_info("no support cmd!!!\n");
	}

ldim_attr_store_end:
	kfree(buf_orig);
	kfree(parm);
	return len;

ldim_attr_store_err:
	kfree(buf_orig);
	kfree(parm);
	return -EINVAL;
}

static ssize_t ldim_func_en_show(struct class *class,
				 struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	ret = sprintf(buf, "%d\n", ldim_drv->conf->func_en);

	return ret;
}

static ssize_t ldim_func_en_store(struct class *class,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &val);
	LDIMPR("local diming function: %s\n", (val ? "enable" : "disable"));
	ldim_drv->conf->func_en = val ? 1 : 0;
	ldim_func_ctrl(ldim_drv->conf->func_en);
	return count;
}

static ssize_t ldim_remap_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	ret = sprintf(buf, "%d\n", ldim_drv->conf->remap_en);

	return ret;
}

static ssize_t ldim_remap_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &val);
	LDIMPR("local diming remap: %s\n", (val ? "enable" : "disable"));

	if (val) {
		if (ldim_drv->conf->remap_en == 0) {
			ldim_drv->conf->remap_en = 1;
			if (ldim_drv->func_en)
				ldim_remap_ctrl(1);
		}
	} else {
		if (ldim_drv->conf->remap_en) {
			ldim_drv->conf->remap_en = 0;
			if (ldim_drv->func_en)
				ldim_remap_ctrl(0);
		}
	}
	return count;
}

static ssize_t ldim_para_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	struct ldim_fw_para_s *fw_para = aml_ldim_get_fw_para();
	int len = 0;

	len = sprintf(buf, "boost_gain=%d\n", fw_para->ctrl->boost_gain);

	return len;
}

static ssize_t ldim_mem_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int len = 0;

	len = sprintf(buf,
			"wr_mem1_paddr:      0x%lx\n"
			"wr_mem1_vaddr:      0x%p\n"
			"wr_mem2_paddr:      0x%lx\n"
			"wr_mem2_vaddr:      0x%p\n"
			"rd_mem1_paddr:      0x%lx\n"
			"rd_mem1_vaddr:      0x%p\n"
			"rd_mem2_paddr:      0x%lx\n"
			"rd_mem2_vaddr:      0x%p\n",
			(unsigned long)ldim_drv->rmem->wr_mem_paddr1,
			ldim_drv->rmem->wr_mem_vaddr1,
			(unsigned long)ldim_drv->rmem->wr_mem_paddr2,
			ldim_drv->rmem->wr_mem_vaddr2,
			(unsigned long)ldim_drv->rmem->rd_mem_paddr1,
			ldim_drv->rmem->rd_mem_vaddr1,
			(unsigned long)ldim_drv->rmem->rd_mem_paddr2,
			ldim_drv->rmem->rd_mem_vaddr2);

	return len;
}

static ssize_t ldim_mem_store(struct class *class,
			      struct class_attribute *attr,
			      const char *buf, size_t count)
{
	int i, ret;
	unsigned int *data_buf, region_num, data, index, temp;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	region_num = ldim_drv->conf->hist_row * ldim_drv->conf->hist_col;

	switch (buf[0]) {
	case 'w':
		data_buf = (unsigned int *)ldim_drv->rmem->wr_mem_vaddr1;
		if (!data_buf) {
			LDIMERR("wr buffer is null\n");
			return count;
		}
		pr_info("ldim wr buffer:\n");
		for (i = 0; i < region_num * 20; i++) {
			pr_info(" 0x%p : %04x\n", data_buf, *data_buf);
			data_buf++;
		}
		pr_info("\n");
		break;
	case 'r':
		data_buf = (unsigned int *)ldim_drv->rmem->rd_mem_vaddr1;
		if (!data_buf) {
			LDIMERR("rd buffer is null\n");
			return count;
		}
		pr_info("ldim rd buffer:\n");
		for (i = 0; i < region_num; i++) {
			pr_info(" 0x%p : %04x\n", data_buf, *data_buf);
			data_buf++;
		}
		pr_info("\n");
		break;
	case 't':
		if (!ldim_drv->rmem->rd_mem_vaddr1) {
			LDIMERR("read_buf is null\n");
			break;
		}
		data_buf = (unsigned int *)ldim_drv->rmem->rd_mem_vaddr1;

		ret = sscanf(buf, "test %x %d", &data, &index);
		if (ret == 2) {
			ldim_drv->matrix_update_en = 0;
			msleep(20);
			data_buf += (index / 2);
			if (index % 2) {
				temp = *data_buf & 0xffff;
				*data_buf = temp | (data << 16);
			} else {
				temp = *data_buf & 0xffff0000;
				*data_buf = temp | data;
			}
			LDIMPR("set rd buffer %d to 0x%x\n", index, data);
		} else if (ret == 1) {
			ldim_drv->matrix_update_en = 0;
			msleep(20);
			temp = (region_num + 1) / 2;
			for (i = 0; i < temp; i++) {
				*data_buf = data | data << 16;
				data_buf++;
			}
			LDIMPR("set all rd buffer to 0x%x\n", data);
		} else {
			ldim_drv->matrix_update_en = 1;
			LDIMPR("disable mem test\n");
		}
		break;
	default:
		LDIMERR("no support cmd\n");
	}
	return count;
}

static ssize_t ldim_reg_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();
	int len = 0;

	if (bl_drv->data->chip_type == BL_CHIP_TM2)
		len = ldim_hw_reg_dump_tm2(buf);
	else if (bl_drv->data->chip_type == BL_CHIP_TM2B)
		len = ldim_hw_reg_dump_tm2b(buf);
	else
		len = ldim_hw_reg_dump(buf);

	return len;
}

static ssize_t ldim_reg_store(struct class *cla,
			      struct class_attribute *attr,
			      const char *buf, size_t len)
{
	unsigned int n = 0, i;
	char *buf_orig, *ps, *token;
	char *parm[3];
	char str[3] = {' ', '\n', '\0'};
	unsigned int reg, val, size, temp;
	unsigned char avg_update_en_bak, matrix_update_en_bak;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return len;
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "rv")) {
		if (!parm[1])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		val = ldim_rd_vcbus(reg);
		pr_info("reg 0x%04x = 0x%08x\n", reg, val);
	} else if (!strcmp(parm[0], "dv")) {
		if (!parm[2])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		if (kstrtouint(parm[2], 10, &size) < 0)
			goto ldim_reg_store_err;
		for (i = 0; i < size; i++) {
			val = ldim_rd_vcbus(reg + i);
			pr_info("reg 0x%04x = 0x%08x\n", (reg + i), val);
		}
	} else if (!strcmp(parm[0], "wv")) {
		if (!parm[2])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		if (kstrtouint(parm[2], 16, &val) < 0)
			goto ldim_reg_store_err;
		ldim_wr_vcbus(reg, val);
		temp = ldim_rd_vcbus(reg);
		pr_info("write reg 0x%04x = 0x%08x, readback 0x%08x\n",
			reg, val, temp);
	} else if (!strcmp(parm[0], "r")) {
		if (!parm[1])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;

		avg_update_en_bak = ldim_drv->avg_update_en;
		matrix_update_en_bak = ldim_drv->matrix_update_en;
		ldim_drv->avg_update_en = 0;
		ldim_drv->matrix_update_en = 0;
		if (avg_update_en_bak || matrix_update_en_bak)
			msleep(20);

		val = ldim_rd_reg(reg);

		ldim_drv->avg_update_en = avg_update_en_bak;
		ldim_drv->matrix_update_en = matrix_update_en_bak;
		pr_info("ldim port reg 0x%04x = 0x%08x\n", reg, val);
	} else if (!strcmp(parm[0], "d")) {
		if (!parm[2])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		if (kstrtouint(parm[2], 10, &size) < 0)
			goto ldim_reg_store_err;

		avg_update_en_bak = ldim_drv->avg_update_en;
		matrix_update_en_bak = ldim_drv->matrix_update_en;
		ldim_drv->avg_update_en = 0;
		ldim_drv->matrix_update_en = 0;
		if (avg_update_en_bak || matrix_update_en_bak)
			msleep(20);

		for (i = 0; i < size; i++) {
			val = ldim_rd_reg(reg + i);
			pr_info("ldim port reg 0x%04x = 0x%08x\n",
				(reg + i), val);
		}

		ldim_drv->avg_update_en = avg_update_en_bak;
		ldim_drv->matrix_update_en = matrix_update_en_bak;
	} else if (!strcmp(parm[0], "w")) {
		if (!parm[2])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		if (kstrtouint(parm[2], 16, &val) < 0)
			goto ldim_reg_store_err;

		avg_update_en_bak = ldim_drv->avg_update_en;
		matrix_update_en_bak = ldim_drv->matrix_update_en;
		ldim_drv->avg_update_en = 0;
		ldim_drv->matrix_update_en = 0;
		if (avg_update_en_bak || matrix_update_en_bak)
			msleep(20);

		ldim_wr_reg(reg, val);
		msleep(30); /* wait for rdma write */
		temp = ldim_rd_reg(reg);

		ldim_drv->avg_update_en = avg_update_en_bak;
		ldim_drv->matrix_update_en = matrix_update_en_bak;
		pr_info("wr ldim port reg 0x%04x = 0x%08x, readback 0x%08x\n",
			reg, val, temp);
	} else {
		pr_info("no support cmd!!!\n");
	}

	kfree(buf_orig);
	return len;

ldim_reg_store_err:
	LDIMERR("invalid parameters\n");
	kfree(buf_orig);
	return -EINVAL;
}

static void ldim_remap_prm_line_parse(struct ldim_fw_para_s *fw_para,
				      char *line_buf)
{
	struct LDReg_s *nprm;
	char *buf_orig, *ps, *token, *p;
	char str[] = {' ', '[', '=', '{', '}', ',', ';', '\n', '\0'};
	char *name = NULL;
	int *param, name_flag = 0, multi_lut_flag = 0;
	int size = 1, size_check = 1, val;
	int index = 0, ret, i, j;

	if (!fw_para)
		return;
	if (!line_buf)
		return;
	buf_orig = kstrdup(line_buf, GFP_KERNEL);
	if (!buf_orig)
		return;
	ps = buf_orig;
	nprm = fw_para->nprm;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;

		if (name_flag == 0) {
			name_flag = 1;
			name = token;
			if (strcmp(token, "reg_LD_BackLit_Xtlk") == 0) {
				param = &nprm->reg_LD_BackLit_Xtlk;
			} else if (strcmp(token, "reg_LD_BackLit_mode") == 0) {
				param = &nprm->reg_LD_BackLit_mode;
			} else if (strcmp(token, "reg_LD_Reflect_Hnum") == 0) {
				param = &nprm->reg_LD_Reflect_Hnum;
			} else if (strcmp(token, "reg_LD_Reflect_Vnum") == 0) {
				param = &nprm->reg_LD_Reflect_Vnum;
			} else if (strcmp(token, "reg_LD_BkLit_curmod") == 0) {
				param = &nprm->reg_LD_BkLit_curmod;
			} else if (strcmp(token, "reg_LD_BkLUT_Intmod") == 0) {
				param = &nprm->reg_LD_BkLUT_Intmod;
			} else if (strcmp(token, "reg_LD_BkLit_Intmod") == 0) {
				param = &nprm->reg_LD_BkLit_Intmod;
			} else if (strcmp(token, "reg_LD_BkLit_LPFmod") == 0) {
				param = &nprm->reg_LD_BkLit_LPFmod;
			} else if (strcmp(token, "reg_LD_BkLit_Celnum") == 0) {
				param = &nprm->reg_LD_BkLit_Celnum;
			} else if (strcmp(token, "reg_LD_Reflect_Hdgr") == 0) {
				param = nprm->reg_LD_Reflect_Hdgr;
				size_check = 20;
			} else if (strcmp(token, "reg_LD_Reflect_Vdgr") == 0) {
				param = nprm->reg_LD_Reflect_Vdgr;
				size_check = 20;
			} else if (strcmp(token, "reg_LD_Reflect_Xdgr") == 0) {
				param = nprm->reg_LD_Reflect_Xdgr;
				size_check = 4;
			} else if (strcmp(token, "reg_LD_Vgain") == 0) {
				param = &nprm->reg_LD_Vgain;
			} else if (strcmp(token, "reg_LD_Hgain") == 0) {
				param = &nprm->reg_LD_Hgain;
			} else if (strcmp(token, "reg_LD_Litgain") == 0) {
				param = &nprm->reg_LD_Litgain;
			} else if (strcmp(token, "reg_LD_Litshft") == 0) {
				param = &nprm->reg_LD_Litshft;
			} else if (strcmp(token, "reg_LD_BkLit_valid") == 0) {
				param = nprm->reg_LD_BkLit_valid;
				size_check = 32;
			} else if (strcmp(token, "reg_LD_LUT_Hdg") == 0) {
				param = nprm->reg_LD_LUT_Hdg;
				multi_lut_flag = 1;
				size_check = 32;
			} else if (strcmp(token, "reg_LD_LUT_Vdg") == 0) {
				param = nprm->reg_LD_LUT_Vdg;
				multi_lut_flag = 2;
				size_check = 32;
			} else if (strcmp(token, "reg_LD_LUT_VHk") == 0) {
				param = nprm->reg_LD_LUT_VHk;
				multi_lut_flag = 3;
				size_check = 32;
			} else if (strcmp(token, "reg_LD_LUT_VHk_pos") == 0) {
				param = nprm->reg_LD_LUT_VHk_pos;
				size_check = 32;
			} else if (strcmp(token, "reg_LD_LUT_VHk_neg") == 0) {
				param = nprm->reg_LD_LUT_VHk_neg;
				size_check = 32;
			} else if (strcmp(token, "reg_LD_LUT_HHk") == 0) {
				param = nprm->reg_LD_LUT_HHk;
				size_check = 32;
			} else if (strcmp(token, "reg_LD_LUT_VHo_pos") == 0) {
				param = nprm->reg_LD_LUT_VHo_pos;
				size_check = 32;
			} else if (strcmp(token, "reg_LD_LUT_VHo_neg") == 0) {
				param = nprm->reg_LD_LUT_VHo_neg;
				size_check = 32;
			} else if (strcmp(token, "reg_LD_LUT_VHo_LS") == 0) {
				param = &nprm->reg_LD_LUT_VHo_LS;
			} else if (strcmp(token, "reg_LD_LUT_Hdg_LEXT") == 0) {
				param = &nprm->reg_LD_LUT_Hdg_LEXT;
				multi_lut_flag = 4;
			} else if (strcmp(token, "reg_LD_LUT_Vdg_LEXT") == 0) {
				param = &nprm->reg_LD_LUT_Vdg_LEXT;
				multi_lut_flag = 5;
			} else if (strcmp(token, "reg_LD_LUT_VHk_LEXT") == 0) {
				param = &nprm->reg_LD_LUT_VHk_LEXT;
				multi_lut_flag = 6;
			} else if (strcmp(token, "reg_LD_RGBmapping_demo")
				   == 0) {
				param = &nprm->reg_LD_RGBmapping_demo;
			} else if (strcmp(token, "reg_LD_R_LUT_interp_mode")
				   == 0) {
				param = &nprm->reg_LD_X_LUT_interp_mode[0];
			} else if (strcmp(token, "reg_LD_G_LUT_interp_mode")
				   == 0) {
				param = &nprm->reg_LD_X_LUT_interp_mode[1];
			} else if (strcmp(token, "reg_LD_B_LUT_interp_mode")
				   == 0) {
				param = &nprm->reg_LD_X_LUT_interp_mode[2];
			} else if (strcmp(token, "reg_LD_LUT_Id") == 0) {
				param = nprm->reg_LD_LUT_Id;
				size_check = 1536;
			} else if (strcmp(token, "fw_LD_BLEst_ACmode") == 0) {
				param = &fw_para->ctrl->fw_ld_blest_acmode;
			} else {
				goto ldim_remap_prm_line_parse_end;
			}
			continue;
		}

		p = strchr(token, ']');
		if (p) {
			*p = '\0';
			ret = kstrtoint(token, 10, &size);
			if (ret) {
				pr_info("%s size error\n", name);
				goto ldim_remap_prm_line_parse_end;
			}
			if (ldim_debug_print)
				pr_info("%s size=%d\n", name, size);
			continue;
		}

		if (size != size_check) {
			pr_info("%s size %d invalid\n", name, size);
			goto ldim_remap_prm_line_parse_end;
		}

		ret = kstrtoint(token, 10, &val);
		if (ret) {
			pr_info("%s val[%d] parse error\n", name, index);
			goto ldim_remap_prm_line_parse_end;
		}
		param[index] = val;

		if (size == 1) {
			if (ldim_debug_print) {
				if (name)
					pr_info("%s=%d\n", name, val);
			}
		} else {
			if (ldim_debug_print) {
				if (name)
					pr_info("%s[%d]=%d\n", name,
						index, val);
			}
			index++;
		}
	}

	switch (multi_lut_flag) {
	case 1:
		for (i = 0; i < 8; i++) {
			for (j = 0; j < 32; j++)
				nprm->reg_LD_LUT_Hdg_TXLX[i][j] =
					nprm->reg_LD_LUT_Hdg[j];
		}
		break;
	case 2:
		for (i = 0; i < 8; i++) {
			for (j = 0; j < 32; j++)
				nprm->reg_LD_LUT_Vdg_TXLX[i][j] =
					nprm->reg_LD_LUT_Vdg[j];
		}
		break;
	case 3:
		for (i = 0; i < 8; i++) {
			for (j = 0; j < 32; j++)
				nprm->reg_LD_LUT_VHk_TXLX[i][j] =
					nprm->reg_LD_LUT_VHk[j];
		}
		break;
	case 4:
		for (i = 0; i < 8; i++)
			nprm->reg_LD_LUT_Hdg_LEXT_TXLX[i] =
					nprm->reg_LD_LUT_Hdg_LEXT;
		break;
	case 5:
		for (i = 0; i < 8; i++)
			nprm->reg_LD_LUT_Vdg_LEXT_TXLX[i] =
					nprm->reg_LD_LUT_Vdg_LEXT;
		break;
	case 6:
		for (i = 0; i < 8; i++)
			nprm->reg_LD_LUT_VHk_LEXT_TXLX[i] =
					nprm->reg_LD_LUT_VHk_LEXT;
		break;
	default:
		break;
	}

ldim_remap_prm_line_parse_end:
	kfree(buf_orig);
}

static void ldim_remap_prm_reg_setting_load(struct ldim_fw_para_s *fw_para,
					    char *path)
{
	unsigned int size = 0, file_size = 0, len = 0;
	char *file_buf;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	/*struct kstat stat;*/
	unsigned int i, n;
	char *ps, *token;
	char str[3] = {';', '\n', '\0'};

	set_fs(KERNEL_DS);

	/*vfs_stat(path, &stat);
	 *file_size = stat.size;
	 */

	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", path);
		return;
	}
	file_size = filp->f_inode->i_size;
	file_buf = kzalloc((file_size + 2), GFP_KERNEL);
	if (!file_buf) {
		filp_close(filp, NULL);
		set_fs(old_fs);
		return;
	}

	size = vfs_read(filp, file_buf, file_size, &pos);
	if (size < file_size) {
		pr_info("%s read size %u < %u error.\n",
			__func__, size, file_size);
		filp_close(filp, NULL);
		set_fs(old_fs);
		kfree(file_buf);
		return;
	}
	vfs_fsync(filp, 0);

	filp_close(filp, NULL);
	set_fs(old_fs);

	ps = file_buf;
	len = 0;
	i = 0;
	n = 0;
	while (1) {
		if (len >= file_size)
			break;
		if (!ps)
			break;
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0') {
			len++;
			continue;
		}
		ldim_remap_prm_line_parse(fw_para, token);
		n++;
		len += (strlen(token) + 1);
		i++;
	}

	pr_info("load setting file path: %s finish, total line %d\n", path, n);
	kfree(file_buf);

	if (ldim_debug_print) {
		if (fw_para)
			ldim_prm_regs_print(fw_para->nprm);
	}
	ldim_remap_regs_update();
}

static ssize_t ldim_prm_reg_store(struct class *cla,
				  struct class_attribute *attr,
				  const char *buf, size_t len)
{
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_para_s *fw_para = aml_ldim_get_fw_para();
	struct LDReg_s *nprm;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};
	int i, j, n = 0, val1;

	if (!fw_para)
		return len;
	if (!fw_para->nprm)
		return len;
	nprm = fw_para->nprm;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return len;
	parm = kcalloc(520, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		return len;
	}
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "info")) {
		ldim_prm_regs_print(nprm);
	} else if (!strcmp(parm[0], "reg")) {
		if ((bl_drv->data->chip_type == BL_CHIP_TM2) ||
		    (bl_drv->data->chip_type == BL_CHIP_TM2B))
			ldim_remap_reg_dump_tm2();
	} else if (!strcmp(parm[0], "profile")) {
		if (!ldim_drv->ldev_conf)
			goto ldim_prm_reg_store_end;
		ldim_profile_print(ldim_drv->ldev_conf->ld_profile);
	} else if (!strcmp(parm[0], "cfg")) {
		ldim_func_init_prm_config(nprm);
	} else if (!strcmp(parm[0], "update")) {
		ldim_remap_regs_update();
	} else if (!strcmp(parm[0], "load")) {
		if (!parm[1])
			goto ldim_prm_reg_store_err;
		ldim_remap_prm_reg_setting_load(fw_para, parm[1]);
	} else if (!strcmp(parm[0], "rdmif_flag")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			ldim_drv->rdmif_flag = (unsigned char)val1;
		}
		pr_info("rdmif_flag: %d\n", ldim_drv->rdmif_flag);
	} else if (!strcmp(parm[0], "reg_LD_BackLit_Xtlk")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_BackLit_Xtlk);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_BackLit_Xtlk = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_BackLit_Xtlk: %d\n", nprm->reg_LD_BackLit_Xtlk);
	} else if (!strcmp(parm[0], "reg_LD_BackLit_mode")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_BackLit_mode);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_BackLit_mode = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_BackLit_mode: %d\n", nprm->reg_LD_BackLit_mode);
	} else if (!strcmp(parm[0], "reg_LD_BkLit_curmod")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_BkLit_curmod);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_BkLit_curmod = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_BkLit_curmod: %d\n", nprm->reg_LD_BkLit_curmod);
	} else if (!strcmp(parm[0], "reg_LD_BkLUT_Intmod")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_BkLUT_Intmod);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_BkLUT_Intmod = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_BkLUT_Intmod: %d\n", nprm->reg_LD_BkLUT_Intmod);
	} else if (!strcmp(parm[0], "reg_LD_BkLit_Intmod")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_BkLit_Intmod);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_BkLit_Intmod = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_BkLit_Intmod: %d\n", nprm->reg_LD_BkLit_Intmod);
	} else if (!strcmp(parm[0], "reg_LD_BkLit_LPFmod")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_BkLit_LPFmod);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_BkLit_LPFmod = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_BkLit_LPFmod: %d\n", nprm->reg_LD_BkLit_LPFmod);
	} else if (!strcmp(parm[0], "reg_LD_Litshft")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", nprm->reg_LD_Litshft);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_Litshft = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_Litshft: %d\n", nprm->reg_LD_Litshft);
	} else if (!strcmp(parm[0], "reg_LD_RGBmapping_demo")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_RGBmapping_demo);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_RGBmapping_demo = val1;
		}
		pr_info("reg_LD_RGBmapping_demo: %d\n",
			nprm->reg_LD_RGBmapping_demo);
	} else if (!strcmp(parm[0], "reg_LD_R_LUT_interp_mode")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_X_LUT_interp_mode[0]);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_X_LUT_interp_mode[0] = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_R_LUT_interp_mode: %d\n",
			nprm->reg_LD_X_LUT_interp_mode[0]);
	} else if (!strcmp(parm[0], "reg_LD_G_LUT_interp_mode")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_X_LUT_interp_mode[1]);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_X_LUT_interp_mode[1] = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_G_LUT_interp_mode: %d\n",
			nprm->reg_LD_X_LUT_interp_mode[1]);
	} else if (!strcmp(parm[0], "reg_LD_B_LUT_interp_mode")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_X_LUT_interp_mode[2]);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_X_LUT_interp_mode[2] = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_B_LUT_interp_mode: %d\n",
			nprm->reg_LD_X_LUT_interp_mode[2]);
	} else if (!strcmp(parm[0], "reg_LD_Reflect_Hnum")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_Reflect_Hnum);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_Reflect_Hnum = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_Reflect_Hnum: %d\n", nprm->reg_LD_Reflect_Hnum);
	} else if (!strcmp(parm[0], "reg_LD_Reflect_Vnum")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_Reflect_Vnum);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_Reflect_Vnum = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_Reflect_Vnum: %d\n", nprm->reg_LD_Reflect_Vnum);
	} else if (!strcmp(parm[0], "reg_LD_BkLit_Celnum")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_BkLit_Celnum);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_BkLit_Celnum = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_BkLit_Celnum: %d\n", nprm->reg_LD_BkLit_Celnum);
	} else if (!strcmp(parm[0], "reg_LD_Reflect_Hdgr")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_Reflect_Hdgr, 20);
				goto ldim_prm_reg_store_end;
			}
		}
		if (parm[21]) {
			for (i = 0; i < 20; i++) {
				if (kstrtouint
				    (parm[i + 1], 10,
				     &nprm->reg_LD_Reflect_Hdgr[i]) < 0)
					goto ldim_prm_reg_store_err;
			}
			ldim_remap_regs_update();
		}
	} else if (!strcmp(parm[0], "reg_LD_Reflect_Vdgr")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_Reflect_Vdgr, 20);
				goto ldim_prm_reg_store_end;
			}
		}
		if (parm[21]) {
			for (i = 0; i < 20; i++) {
				if (kstrtouint
				    (parm[i + 1], 10,
				     &nprm->reg_LD_Reflect_Vdgr[i]) < 0)
					goto ldim_prm_reg_store_err;
			}
			ldim_remap_regs_update();
		}
	} else if (!strcmp(parm[0], "reg_LD_Reflect_Xdgr")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_Reflect_Xdgr, 4);
				goto ldim_prm_reg_store_end;
			}
		}
		if (parm[5]) {
			for (i = 0; i < 4; i++) {
				if (kstrtouint
				    (parm[i + 1], 10,
				     &nprm->reg_LD_Reflect_Xdgr[i]) < 0)
					goto ldim_prm_reg_store_err;
			}
			ldim_remap_regs_update();
		}
	} else if (!strcmp(parm[0], "reg_LD_Vgain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", nprm->reg_LD_Vgain);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_Vgain = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_Vgain: %d\n", nprm->reg_LD_Vgain);
	} else if (!strcmp(parm[0], "reg_LD_Hgain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", nprm->reg_LD_Hgain);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_Hgain = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_Hgain: %d\n", nprm->reg_LD_Hgain);
	} else if (!strcmp(parm[0], "reg_LD_Litgain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", nprm->reg_LD_Litgain);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_Litgain = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_Litgain: %d\n", nprm->reg_LD_Litgain);
	} else if (!strcmp(parm[0], "reg_LD_LUT_VHo_LS")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					nprm->reg_LD_LUT_VHo_LS);
				goto ldim_prm_reg_store_end;
			}
			if (kstrtouint(parm[1], 10, &val1) < 0)
				goto ldim_prm_reg_store_err;
			nprm->reg_LD_LUT_VHo_LS = val1;
			ldim_remap_regs_update();
		}
		pr_info("reg_LD_LUT_VHo_LS: %d\n", nprm->reg_LD_LUT_VHo_LS);
	} else if (!strcmp(parm[0], "reg_LD_BkLit_valid")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_BkLit_valid, 32);
				goto ldim_prm_reg_store_end;
			}
		}
		if (parm[33]) {
			for (i = 0; i < 32; i++) {
				if (kstrtouint
				    (parm[i + 1], 10,
				     &nprm->reg_LD_BkLit_valid[i]) < 0)
					goto ldim_prm_reg_store_err;
			}
			ldim_remap_regs_update();
		}
	} else if (!strcmp(parm[0], "reg_LD_LUT_Hdg")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) { /* index read */
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_prm_reg_store_err;
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_LUT_Hdg_TXLX[i], 32);
				goto ldim_prm_reg_store_end;
			}
		}
		/* index write */
		if (!parm[33])
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[1], 10, &i) < 0)
			goto ldim_prm_reg_store_err;
		for (j = 0; j < 32; j++) {
			if (kstrtouint
			    (parm[j + 2], 10,
			     &nprm->reg_LD_LUT_Hdg_TXLX[i][j]) < 0) {
				goto ldim_prm_reg_store_err;
			}
		}
		ldim_remap_regs_update();
	} else if (!strcmp(parm[0], "reg_LD_LUT_Vdg")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) { /* index read */
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_prm_reg_store_err;
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_LUT_Vdg_TXLX[i], 32);
				goto ldim_prm_reg_store_end;
			}
		}
		/* index write */
		if (!parm[33])
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[1], 10, &i) < 0)
			goto ldim_prm_reg_store_err;
		for (j = 0; j < 32; j++) {
			if (kstrtouint(parm[j + 2], 10,
				       &nprm->reg_LD_LUT_Vdg_TXLX[i][j]) < 0) {
				goto ldim_prm_reg_store_err;
			}
		}
		ldim_remap_regs_update();
	} else if (!strcmp(parm[0], "reg_LD_LUT_VHk")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) { /* index read */
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_prm_reg_store_err;
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_LUT_VHk_TXLX[i], 32);
				goto ldim_prm_reg_store_end;
			}
		}
		/* index write */
		if (!parm[33])
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[1], 10, &i) < 0)
			goto ldim_prm_reg_store_err;
		for (j = 0; j < 32; j++) {
			if (kstrtouint(parm[j + 2], 10,
				       &nprm->reg_LD_LUT_VHk_TXLX[i][j]) < 0) {
				goto ldim_prm_reg_store_err;
			}
		}
		ldim_remap_regs_update();
	} else if (!strcmp(parm[0], "reg_LD_LUT_VHk_pos")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_LUT_VHk_pos, 32);
				goto ldim_prm_reg_store_end;
			}
		}
		if (parm[32]) {
			for (i = 0; i < 32; i++) {
				if (kstrtouint
				    (parm[i + 1], 10,
				     &nprm->reg_LD_LUT_VHk_pos[i]) < 0)
					goto ldim_prm_reg_store_err;
			}
			ldim_remap_regs_update();
		}
	} else if (!strcmp(parm[0], "reg_LD_LUT_VHk_neg")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_LUT_VHk_neg, 32);
				goto ldim_prm_reg_store_end;
			}
		}
		if (parm[32]) {
			for (i = 0; i < 32; i++) {
				if (kstrtouint
				    (parm[i + 1], 10,
				    &nprm->reg_LD_LUT_VHk_neg[i]) < 0)
					goto ldim_prm_reg_store_err;
			}
			ldim_remap_regs_update();
		}
	} else if (!strcmp(parm[0], "reg_LD_LUT_HHk")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_LUT_HHk, 32);
				goto ldim_prm_reg_store_end;
			}
		}
		if (parm[32]) {
			for (i = 0; i < 32; i++) {
				if (kstrtouint
				    (parm[i + 1], 10,
				     &nprm->reg_LD_LUT_HHk[i]) < 0)
					goto ldim_prm_reg_store_err;
			}
			ldim_remap_regs_update();
		}
	} else if (!strcmp(parm[0], "reg_LD_LUT_VHo_pos")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_LUT_VHo_pos, 32);
				goto ldim_prm_reg_store_end;
			}
		}
		if (parm[32]) {
			for (i = 0; i < 32; i++) {
				if (kstrtouint
				    (parm[i + 1], 10,
				    &nprm->reg_LD_LUT_VHo_pos[i]) < 0)
					goto ldim_prm_reg_store_err;
			}
			ldim_remap_regs_update();
		}
	} else if (!strcmp(parm[0], "reg_LD_LUT_VHo_neg")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_LUT_VHo_neg, 32);
				goto ldim_prm_reg_store_end;
			}
		}
		if (parm[32]) {
			for (i = 0; i < 32; i++) {
				if (kstrtouint
				    (parm[i + 1], 10,
				    &nprm->reg_LD_LUT_VHo_neg[i]) < 0)
					goto ldim_prm_reg_store_err;
			}
			ldim_remap_regs_update();
		}
	} else if (!strcmp(parm[0], "reg_LD_LUT_Hdg_LEXT")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) { /* index read */
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_prm_reg_store_err;
				if (i >= 8)
					goto ldim_prm_reg_store_err;
				pr_info("for_tool: %d\n",
					nprm->reg_LD_LUT_Hdg_LEXT_TXLX[i]);
				goto ldim_prm_reg_store_end;
			}
		}
		/* index write */
		if (!parm[2])
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[1], 10, &i) < 0)
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[2], 10,
			       &nprm->reg_LD_LUT_Hdg_LEXT_TXLX[i]) < 0)
			goto ldim_prm_reg_store_err;
		ldim_remap_regs_update();
	} else if (!strcmp(parm[0], "reg_LD_LUT_Vdg_LEXT")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) { /* index read */
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_prm_reg_store_err;
				if (i >= 8)
					goto ldim_prm_reg_store_err;
				pr_info("for_tool: %d\n",
					nprm->reg_LD_LUT_Vdg_LEXT_TXLX[i]);
				goto ldim_prm_reg_store_end;
			}
		}
		/* index write */
		if (!parm[2])
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[1], 10, &i) < 0)
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[2], 10,
			       &nprm->reg_LD_LUT_Vdg_LEXT_TXLX[i]) < 0)
			goto ldim_prm_reg_store_err;
		ldim_remap_regs_update();
	} else if (!strcmp(parm[0], "reg_LD_LUT_VHk_LEXT")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) { /* index read */
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_prm_reg_store_err;
				if (i >= 8)
					goto ldim_prm_reg_store_err;
				pr_info("for_tool: %d\n",
					nprm->reg_LD_LUT_VHk_LEXT_TXLX[i]);
				goto ldim_prm_reg_store_end;
			}
		}
		/* index write */
		if (!parm[2])
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[1], 10, &i) < 0)
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[2], 10,
			       &nprm->reg_LD_LUT_VHk_LEXT_TXLX[i]) < 0)
			goto ldim_prm_reg_store_err;
		ldim_remap_regs_update();
	} else if (!strcmp(parm[0], "reg_LD_LUT_Id")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_prm_reg_store_err;
				ldim_prm_reg_ld_lut_mute_print
					(nprm->reg_LD_LUT_Id, 1536);
				goto ldim_prm_reg_store_end;
			}
		}
		/* index write */
		if (!parm[2])
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[1], 10, &i) < 0)
			goto ldim_prm_reg_store_err;
		if (kstrtouint(parm[2], 10, &nprm->reg_LD_LUT_Id[i]) < 0)
			goto ldim_prm_reg_store_err;
		ldim_remap_regs_update();
	} else if (!strcmp(parm[0], "demo_roi")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 10,
			    &nprm->reg_LD_xlut_iroi_enable) < 0)
				goto ldim_prm_reg_store_err;
			if (kstrtouint(parm[2], 10,
				       &nprm->reg_LD_xlut_oroi_enable) < 0)
				goto ldim_prm_reg_store_err;
			ldim_remap_regs_update();
		}
		pr_info
		("reg_LD_xlut_iroi_enable=%d, reg_LD_xlut_oroi_enable=%d\n",
		 nprm->reg_LD_xlut_iroi_enable, nprm->reg_LD_xlut_oroi_enable);
	} else {
		pr_info("no support cmd!!!\n");
	}

ldim_prm_reg_store_end:
	kfree(buf_orig);
	kfree(parm);
	return len;

ldim_prm_reg_store_err:
	pr_info("error: invalid parameters\n");
	kfree(buf_orig);
	kfree(parm);
	return -EINVAL;
}

static ssize_t ldim_demo_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	ret = sprintf(buf, "%d\n", ldim_drv->demo_en);

	return ret;
}

static ssize_t ldim_demo_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val = 0;

	if (ldim_drv->remap_en == 0) {
		LDIMERR("%s: ldim_remap is disabled\n", __func__);
		return count;
	}

	if (kstrtouint(buf, 10, &val) < 0)
		return count;
	LDIMPR("local diming demo: %d\n", val);
	if (val) {
		ldim_drv->demo_en = 1;
		//ldim_drv->fw_para->nprm->reg_LD_BackLit_Xtlk = 0;
		//ldim_drv->fw_para->nprm->reg_LD_RGBmapping_demo = 1;
		//ldim_remap_regs_update();
		ldim_hw_remap_demo_en(1);
	} else {
		ldim_drv->demo_en = 0;
		//ldim_drv->fw_para->nprm->reg_LD_BackLit_Xtlk = 1;
		//ldim_drv->fw_para->nprm->reg_LD_RGBmapping_demo = 0;
		//ldim_remap_regs_update();
		ldim_hw_remap_demo_en(0);
	}
	return count;
}

static struct class_attribute aml_ldim_class_attrs[] = {
	__ATTR(attr, 0644, ldim_attr_show, ldim_attr_store),
	__ATTR(func_en, 0644, ldim_func_en_show, ldim_func_en_store),
	__ATTR(remap, 0644, ldim_remap_show, ldim_remap_store),
	__ATTR(para, 0644, ldim_para_show, NULL),
	__ATTR(mem, 0644, ldim_mem_show, ldim_mem_store),
	__ATTR(reg, 0644, ldim_reg_show, ldim_reg_store),
	__ATTR(prm, 0644, NULL, ldim_prm_reg_store),
	__ATTR(demo, 0644, ldim_demo_show, ldim_demo_store)
};

int aml_ldim_debug_probe(struct class *ldim_class)
{
	int i;

	for (i = 0; aml_ldim_class_attrs[i].attr.name; i++) {
		if (class_create_file(ldim_class, &aml_ldim_class_attrs[i]) < 0)
			return -1;
	}

	return 0;
}

void aml_ldim_debug_remove(struct class *ldim_class)
{
	int i;

	for (i = 0; aml_ldim_class_attrs[i].attr.name; i++)
		class_remove_file(ldim_class, &aml_ldim_class_attrs[i]);
}

