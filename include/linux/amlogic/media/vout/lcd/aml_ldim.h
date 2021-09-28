/*
 * include/linux/amlogic/media/vout/lcd/aml_ldim.h
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

#ifndef _INC_AML_LDIM_H_
#define _INC_AML_LDIM_H_
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>
#include <linux/spi/spi.h>

/*#define LDIM_DEBUG_INFO*/
#define LDIMPR(fmt, args...)     pr_info("ldim: "fmt"", ## args)
#define LDIMERR(fmt, args...)    pr_err("ldim: error: "fmt"", ## args)

#define LD_DATA_DEPTH   12
#define LD_DATA_MIN     10
#define LD_DATA_MAX     0xfff

/* **********************************
 * IOCTL define
 * **********************************
 */
#define _VE_LDIM  'C'
#define AML_LDIM_IOC_NR_GET_INFO	0x51
#define AML_LDIM_IOC_NR_SET_INFO	0x52
#define AML_LDIM_IOC_NR_GET_REMAP_INFO	0x53
#define AML_LDIM_IOC_NR_SET_REMAP_INFO	0x54

#define AML_LDIM_IOC_CMD_GET_INFO \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_INFO, struct aml_ldim_info_s)
#define AML_LDIM_IOC_CMD_SET_INFO \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_INFO, struct aml_ldim_info_s)
#define AML_LDIM_IOC_CMD_GET_REMAP_INFO \
	_IOR(_VE_LDIM, AML_LDIM_IOC_NR_GET_REMAP_INFO, \
	     struct aml_ldim_remap_info_s)
#define AML_LDIM_IOC_CMD_SET_REMAP_INFO \
	_IOW(_VE_LDIM, AML_LDIM_IOC_NR_SET_REMAP_INFO, \
	     struct aml_ldim_remap_info_s)

enum ldim_dev_type_e {
	LDIM_DEV_TYPE_NORMAL = 0,
	LDIM_DEV_TYPE_SPI,
	LDIM_DEV_TYPE_I2C,
	LDIM_DEV_TYPE_MAX,
};

struct aml_ldim_info_s {
	unsigned int func_en;
	unsigned int remapping_en;
	unsigned int alpha;
	unsigned int LPF_method;
	unsigned int lpf_gain;
	unsigned int lpf_res;
	unsigned int side_blk_diff_th;
	unsigned int bbd_th;
	unsigned int boost_gain;
	unsigned int rgb_base;
	unsigned int Ld_remap_bypass;
	unsigned int LD_TF_STEP_TH;
	unsigned int TF_BLK_FRESH_BL;
	unsigned int TF_FRESH_BL;
	unsigned int fw_LD_ThTF_l;
	unsigned int fw_rgb_diff_th;
	unsigned int fw_ld_thist;
	unsigned int fw_ld_blest_acmode;
	unsigned int bl_remap_curve[16];
	unsigned int fw_ld_whist[16];
};

struct aml_ldim_remap_info_s {
	unsigned int reg_LD_BackLit_Xtlk;
	unsigned int reg_LD_BackLit_mode;
	unsigned int reg_LD_Reflect_Hnum;
	unsigned int reg_LD_Reflect_Vnum;
	unsigned int reg_LD_BkLit_curmod;
	unsigned int reg_LD_BkLUT_Intmod;
	unsigned int reg_LD_BkLit_Intmod;
	unsigned int reg_LD_BkLit_LPFmod;
	unsigned int reg_LD_BkLit_Celnum;
	unsigned int reg_LD_Reflect_Hdgr[20];
	unsigned int reg_LD_Reflect_Vdgr[20];
	unsigned int reg_LD_Reflect_Xdgr[4];
	unsigned int reg_LD_Vgain;
	unsigned int reg_LD_Hgain;
	unsigned int reg_LD_Litgain;
	unsigned int reg_LD_Litshft;
	unsigned int reg_LD_BkLit_valid[32];
	unsigned int reg_LD_LUT_VHk_pos[32];
	unsigned int reg_LD_LUT_VHk_neg[32];
	unsigned int reg_LD_LUT_HHk[32];
	unsigned int reg_LD_LUT_VHo_pos[32];
	unsigned int reg_LD_LUT_VHo_neg[32];
	unsigned int reg_LD_LUT_VHo_LS;
	unsigned int reg_LD_RGBmapping_demo;
	unsigned int reg_LD_R_LUT_interp_mode;
	unsigned int reg_LD_G_LUT_interp_mode;
	unsigned int reg_LD_B_LUT_interp_mode;
	unsigned int reg_LD_LUT_Hdg_LEXT;
	unsigned int reg_LD_LUT_Vdg_LEXT;
	unsigned int reg_LD_LUT_VHk_LEXT;
	unsigned int Reg_LD_remap_LUT[16][32];
};

struct ldim_config_s {
	unsigned short hsize;
	unsigned short vsize;
	unsigned char hist_row;
	unsigned char hist_col;
	unsigned char bl_mode;
	unsigned char func_en;
	unsigned char remap_en;
	unsigned char hvcnt_bypass;
	unsigned char dev_index;
	struct aml_ldim_info_s *ldim_info;
	struct aml_ldim_remap_info_s *remap_info;
};

struct ldim_profile_s {
	unsigned int ld_profile_mode;
	unsigned int ld_lut_hdg[32];
	unsigned int ld_lut_vdg[32];
	unsigned int ld_lut_vhk[32];
};

#define LDIM_INIT_ON_MAX     300
#define LDIM_INIT_OFF_MAX    20
struct ldim_dev_config_s {
	unsigned char index;
	char name[20];
	char pinmux_name[20];
	unsigned char type;
	int cs_hold_delay;
	int cs_clk_delay;
	int en_gpio;
	int en_gpio_on;
	int en_gpio_off;
	int lamp_err_gpio;
	unsigned char fault_check;
	unsigned char write_check;
	unsigned char device_count;
	unsigned char pinmux_flag;

	unsigned int blk_row;
	unsigned int blk_col;
	unsigned int dim_min;
	unsigned int dim_max;

	unsigned char init_loaded;
	unsigned char cmd_size;
	unsigned char *init_on;
	unsigned char *init_off;
	unsigned int init_on_cnt;
	unsigned int init_off_cnt;

	struct bl_pwm_config_s ldim_pwm_config;
	struct bl_pwm_config_s analog_pwm_config;

	unsigned short bl_zone_num;
	unsigned short bl_mapping[LD_BLKREGNUM];

	struct ldim_profile_s *ld_profile;

	void (*dim_range_update)(void);
	int (*dev_reg_write)(unsigned int dev_id, unsigned char *buf,
			     unsigned int len);
	int (*dev_reg_read)(unsigned int dev_id, unsigned char *buf,
			    unsigned int len);

	struct pinctrl *pin;
	struct device *dev;
	struct class *dev_class;
	struct spi_device *spi_dev;
	struct spi_board_info *spi_info;
};

struct ldim_rmem_s {
	void *wr_mem_vaddr1;
	phys_addr_t wr_mem_paddr1;
	void *wr_mem_vaddr2;
	phys_addr_t wr_mem_paddr2;
	void *rd_mem_vaddr1;
	phys_addr_t rd_mem_paddr1;
	void *rd_mem_vaddr2;
	phys_addr_t rd_mem_paddr2;
	unsigned int wr_mem_size;
	unsigned int rd_mem_size;
};

/*******global API******/
#define LDIM_STATE_POWER_ON             BIT(0)
#define LDIM_STATE_FUNC_EN              BIT(1)
#define LDIM_STATE_REMAP_EN             BIT(2)
#define LDIM_STATE_REMAP_FORCE_UPDATE   BIT(3)
#define LDIM_STATE_LD_EN                BIT(4)

struct aml_ldim_driver_s {
	unsigned char valid_flag;
	unsigned char static_pic_flag;
	unsigned char vsync_change_flag;

	unsigned char init_on_flag;
	unsigned char func_en;
	unsigned char remap_en;
	unsigned char demo_en;
	unsigned char func_bypass;  /* for lcd bist pattern */
	unsigned char ld_sel;  /* for gd bypass */
	unsigned char brightness_bypass;
	unsigned char test_en;
	unsigned char avg_update_en;
	unsigned char matrix_update_en;
	unsigned char remap_ram_flag;
	unsigned char remap_ram_step;
	unsigned char remap_mif_flag;
	unsigned char remap_init_flag;
	unsigned char remap_bypass_flag;
	unsigned char alg_en;
	unsigned char top_en;
	unsigned char hist_en;
	unsigned char load_db_en;
	unsigned char db_print_flag;
	unsigned char rdmif_flag;
	unsigned char litstep_en;
	unsigned int state;

	unsigned int data_min;
	unsigned int data_max;
	unsigned int brightness_level;
	unsigned int litgain;
	unsigned int irq_cnt;

	struct ldim_config_s *conf;
	struct ldim_dev_config_s *ldev_conf;
	struct ldim_rmem_s *rmem;
	unsigned int *hist_matrix;
	unsigned int *max_rgb;
	unsigned short *test_matrix;
	unsigned short *local_ldim_matrix;
	unsigned short *ldim_matrix_buf;
	unsigned int *array_tmp;
	struct ldim_fw_para_s *fw_para;
	int (*init)(void);
	int (*power_on)(void);
	int (*power_off)(void);
	int (*set_level)(unsigned int level);
	int (*pinmux_ctrl)(int status);
	int (*pwm_vs_update)(void);
	int (*device_power_on)(void);
	int (*device_power_off)(void);
	int (*device_bri_update)(unsigned short *buf, unsigned char len);
	int (*device_bri_check)(void);
	void (*config_print)(void);
	void (*test_ctrl)(int flag);
	void (*ld_sel_ctrl)(int flag);
};

extern struct aml_ldim_driver_s *aml_ldim_get_driver(void);

extern int aml_ldim_get_config_dts(struct device_node *child);
extern int aml_ldim_get_config_unifykey(unsigned char *buf);
extern int aml_ldim_probe(struct platform_device *pdev);
extern int aml_ldim_remove(void);

#endif

