/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_LCD_VOUT_H
#define _INC_LCD_VOUT_H
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/workqueue.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_data.h>

/* **********************************
 * debug print define
 * **********************************
 */
#define LCD_DEBUG_LEVEL_NORMAL    BIT(0)
#define LCD_DEBUG_LEVEL_CLK       BIT(1)
#define LCD_DEBUG_LEVEL_VSYNC     BIT(3)

#define LCD_DEBUG_VSYNC_INTERVAL  60

/* #define LCD_DEBUG_INFO */
extern unsigned int lcd_debug_print_flag;
#define LCD_DBG_PR_NORMAL       BIT(0)
#define LCD_DBG_PR_ADV          BIT(1) //tcon_data
#define LCD_DBG_PR_CLK          BIT(2)
#define LCD_DBG_PR_ISR          BIT(3)
#define LCD_DBG_PR_BL_NORMAL    BIT(4)
#define LCD_DBG_PR_BL_ADV       BIT(5) //pwm, isr, ext, ldim
#define LCD_DBG_PR_TEST         BIT(6)
#define LCD_DBG_PR_REG          BIT(7)

#define LCDPR(fmt, args...)     pr_info("lcd: " fmt "", ## args)
#define LCDERR(fmt, args...)    pr_err("lcd: error: " fmt "", ## args)

#define LCD_MAX_DRV             3

/* **********************************
 * clk parameter bit define
 * pll_ctrl, div_ctrl, clk_ctrl
 * **********************************
 */
/* ******** pll_ctrl ******** */
#define PLL_CTRL_OD3                20 /* [21:20] */
#define PLL_CTRL_OD2                18 /* [19:18] */
#define PLL_CTRL_OD1                16 /* [17:16] */
#define PLL_CTRL_N                  9  /* [13:9] */
#define PLL_CTRL_M                  0  /* [8:0] */

/* ******** div_ctrl ******** */
#define DIV_CTRL_EDP_DIV1           24 /* [26:24] */
#define DIV_CTRL_EDP_DIV0           20 /* [23:20] */
#define DIV_CTRL_DIV_SEL            8 /* [15:8] */
#define DIV_CTRL_XD                 0 /* [7:0] */

/* ******** clk_ctrl ******** */
#define CLK_CTRL_LEVEL              28 /* [30:28] */
#define CLK_CTRL_FRAC_SHIFT         24 /* [24] */
#define CLK_CTRL_FRAC               0  /* [18:0] */

/* **********************************
 * VENC to TCON sync delay
 * **********************************
 */
#define TTL_DELAY                   13
#define PRE_DE_DELAY                8

/* **********************************
 * global control define
 * **********************************
 */
enum lcd_mode_e {
	LCD_MODE_TV = 0,
	LCD_MODE_TABLET,
	LCD_MODE_MAX,
};

enum lcd_chip_e {
	LCD_CHIP_G12A = 0,
	LCD_CHIP_G12B,  /* 1 */
	LCD_CHIP_TL1,   /* 2 */
	LCD_CHIP_SM1,	/* 3 */
	LCD_CHIP_TM2,   /* 4 */
	LCD_CHIP_TM2B,  /* 5 */
	LCD_CHIP_T5,    /* 6 */
	LCD_CHIP_T5D,   /* 7 */
	LCD_CHIP_T7,    /* 8 */
	LCD_CHIP_T3,	/* 9 */
	LCD_CHIP_MAX,
};

enum lcd_type_e {
	LCD_TTL = 0,
	LCD_LVDS,
	LCD_VBYONE,
	LCD_MIPI,
	LCD_MLVDS,
	LCD_P2P,
	LCD_EDP,
	LCD_TYPE_MAX,
};

#define MOD_LEN_MAX        30
struct lcd_basic_s {
	char model_name[MOD_LEN_MAX];
	enum lcd_type_e lcd_type;
	unsigned char lcd_bits;

	unsigned short h_active;    /* Horizontal display area */
	unsigned short v_active;    /* Vertical display area */
	unsigned short h_period;    /* Horizontal total period time */
	unsigned short v_period;    /* Vertical total period time */
	unsigned short h_period_min;
	unsigned short h_period_max;
	unsigned short v_period_min;
	unsigned short v_period_max;
	unsigned int lcd_clk_min;
	unsigned int lcd_clk_max;

	unsigned short screen_width;  /* screen physical width(unit: mm) */
	unsigned short screen_height; /* screen physical height(unit: mm) */
};

#define LCD_CLK_FRAC_UPDATE     BIT(0)
#define LCD_CLK_PLL_CHANGE      BIT(1)
struct lcd_timing_s {
	unsigned char clk_auto; /* clk parameters auto generation */
	unsigned char fr_adjust_type; /* 0=clock, 1=htotal, 2=vtotal */
	unsigned char clk_change; /* internal used */
	unsigned int lcd_clk;   /* pixel clock(unit: Hz) */
	unsigned int lcd_clk_dft; /* internal used */
	unsigned int bit_rate; /* Hz */
	unsigned int h_period_dft; /* internal used */
	unsigned int v_period_dft; /* internal used */
	unsigned int pll_ctrl;  /* pll settings */
	unsigned int div_ctrl;  /* divider settings */
	unsigned int clk_ctrl;  /* clock settings */
	unsigned int ss_level;
	unsigned int ss_freq;
	unsigned int ss_mode;

	unsigned int sync_duration_num;
	unsigned int sync_duration_den;

	unsigned short video_on_pixel;
	unsigned short video_on_line;

	unsigned short hsync_width;
	unsigned short hsync_bp;
	unsigned short hsync_pol;
	unsigned short vsync_width;
	unsigned short vsync_bp;
	unsigned short vsync_pol;
	/* unsigned int vsync_h_phase; // [31]sign, [15:0]value */
	unsigned int h_offset;
	unsigned int v_offset;

	unsigned short de_hs_addr;
	unsigned short de_he_addr;
	unsigned short de_vs_addr;
	unsigned short de_ve_addr;

	unsigned short hs_hs_addr;
	unsigned short hs_he_addr;
	unsigned short hs_vs_addr;
	unsigned short hs_ve_addr;

	unsigned short vs_hs_addr;
	unsigned short vs_he_addr;
	unsigned short vs_vs_addr;
	unsigned short vs_ve_addr;
};

/* **********************************
 * HDR info define
 * **********************************
 */
struct lcd_optical_info_s {
	unsigned int hdr_support;
	unsigned int features;
	unsigned int primaries_r_x;
	unsigned int primaries_r_y;
	unsigned int primaries_g_x;
	unsigned int primaries_g_y;
	unsigned int primaries_b_x;
	unsigned int primaries_b_y;
	unsigned int white_point_x;
	unsigned int white_point_y;
	unsigned int luma_max;
	unsigned int luma_min;
	unsigned int luma_avg;
};

struct ttl_config_s {
	unsigned int clk_pol;
	unsigned int sync_valid; /* [1]DE, [0]hvsync */
	unsigned int swap_ctrl; /* [1]rb swap, [0]bit swap */
};

#define LVDS_PHY_VSWING_DFT        3
#define LVDS_PHY_PREEM_DFT         0
#define LVDS_PHY_CLK_VSWING_DFT    0
#define LVDS_PHY_CLK_PREEM_DFT     0
struct lvds_config_s {
	unsigned int lvds_vswing;
	unsigned int lvds_repack;
	unsigned int dual_port;
	unsigned int pn_swap;
	unsigned int port_swap;
	unsigned int lane_reverse;
	unsigned int port_sel;
	unsigned int phy_vswing;
	unsigned int phy_preem;
	unsigned int phy_clk_vswing;
	unsigned int phy_clk_preem;
};

#define VX1_PHY_VSWING_DFT        3
#define VX1_PHY_PREEM_DFT         0

#define VX1_PWR_ON_RESET_DLY_DFT  500 /* 500ms */
#define VX1_HPD_DATA_DELAY_DFT    10 /* 10ms */
#define VX1_CDR_TRAINING_HOLD_DFT 200 /* 200ms */

struct vbyone_config_s {
	unsigned int lane_count;
	unsigned int region_num;
	unsigned int byte_mode;
	unsigned int color_fmt;
	unsigned int phy_div;
	unsigned int phy_vswing; /*[4]:ext_pullup, [3:0]vswing*/
	unsigned int phy_preem;
	unsigned int intr_en;
	unsigned int vsync_intr_en;

	unsigned int ctrl_flag;
		/*  bit[0]:power_on_reset_en
		 *  bit[1]:hpd_data_delay_en
		 *  bit[2]:cdr_training_hold_en
		 *  bit[3]:hw_filter_en
		 */

	/* ctrl timing */
	unsigned int power_on_reset_delay; /* ms */
	unsigned int hpd_data_delay; /* ms */
	unsigned int cdr_training_hold; /* ms */
	/* hw filter */
	unsigned int hw_filter_time; /* ms */
	unsigned int hw_filter_cnt;
};

/* mipi-dsi config */
/* Operation mode parameters */
#define OPERATION_VIDEO_MODE     0
#define OPERATION_COMMAND_MODE   1

#define SYNC_PULSE               0x0
#define SYNC_EVENT               0x1
#define BURST_MODE               0x2

/* unit: MHz */
#define MIPI_BIT_RATE_MAX        1000

/* command config */
#define DSI_CMD_SIZE_INDEX       1  /* byte[1] */
#define DSI_GPIO_INDEX           2  /* byte[2] */

#define DSI_INIT_ON_MAX          100
#define DSI_INIT_OFF_MAX         30

#define DSI_READ_CNT_MAX         30
struct dsi_read_s {
	unsigned char flag;
	unsigned char reg;
	unsigned char cnt;
	unsigned char *value;
	unsigned char ret_code;

	unsigned int line_start;
	unsigned int line_end;
};

struct dsi_config_s {
	unsigned char lane_num;
	unsigned int bit_rate_max; /* MHz */
	unsigned int bit_rate_min; /* MHz*/
	unsigned int clk_factor; /* bit_rate/pclk */
	unsigned int factor_numerator;
	unsigned int factor_denominator; /* 100 */
	unsigned char operation_mode_init; /* 0=video mode, 1=command mode */
	unsigned char operation_mode_display; /* 0=video mode, 1=command mode */
	unsigned char video_mode_type; /* 0=sync_pulse, 1=sync_event, 2=burst */
	unsigned char clk_always_hs; /* 0=disable, 1=enable */
	unsigned char phy_switch; /* 0=auto, 1=standard, 2=slow */

	unsigned int venc_data_width;
	unsigned int dpi_data_format;
	unsigned int data_bits;

	unsigned char *dsi_init_on;
	unsigned char *dsi_init_off;
	unsigned char extern_init;

	unsigned char check_en;
	unsigned char check_reg;
	unsigned char check_cnt;
	unsigned char check_state;

	unsigned char current_mode;

	struct dsi_read_s *dread;
};

#define EDP_EDID_STATE_LOAD     BIT(0)
#define EDP_EDID_STATE_APPLY    BIT(1)
#define EDP_EDID_RETRY_MAX      3
struct edp_config_s {
	unsigned char max_lane_count;
	unsigned char max_link_rate;
	unsigned char training_mode; /* 0=fast training, 1=auto training */
	unsigned char dpcd_caps_en;
	unsigned char sync_clk_mode;
	unsigned char scramb_mode;
	unsigned char enhanced_framing_en;
	unsigned char edid_en;
	unsigned char pn_swap;

	unsigned int phy_vswing;
	unsigned int phy_preem;

	/* internal used */
	unsigned char lane_count;
	unsigned char link_rate;
	unsigned int bit_rate;
	unsigned char edid_state;
	unsigned char edid_retry_cnt;
	unsigned char link_update;
	unsigned char training_settings;
	unsigned char main_stream_enable;

	unsigned char edid_data[128];
};

struct mlvds_config_s {
	unsigned int channel_num;
	unsigned int channel_sel0;
	unsigned int channel_sel1;
	unsigned int clk_phase; /* [13:12]=clk01_pi_sel,
				 * [11:8]=pi2, [7:4]=pi1, [3:0]=pi0
				 */
	unsigned int pn_swap;
	unsigned int bit_swap; /* MSB/LSB reverse */
	unsigned int phy_vswing;
	unsigned int phy_preem;

	/* internal used */
	unsigned int pi_clk_sel; /* bit[9:0] */
};

enum p2p_type_e {
	P2P_CEDS = 0,
	P2P_CMPI,
	P2P_ISP,
	P2P_EPI,
	P2P_CHPI = 0x10, /* low common mode */
	P2P_CSPI,
	P2P_USIT,
	P2P_MAX,
};

struct p2p_config_s {
	unsigned int p2p_type; /* bit[4:0] for type, bit[5] for vcm flag */
	unsigned int lane_num;
	unsigned int channel_sel0;
	unsigned int channel_sel1;
	unsigned int pn_swap;
	unsigned int bit_swap; /* MSB/LSB reverse */
	unsigned int phy_vswing;
	unsigned int phy_preem;
};

union lcd_ctrl_config_u {
	struct ttl_config_s ttl_cfg;
	struct lvds_config_s lvds_cfg;
	struct vbyone_config_s vbyone_cfg;
	struct dsi_config_s mipi_cfg;
	struct edp_config_s edp_cfg;
	struct mlvds_config_s mlvds_cfg;
	struct p2p_config_s p2p_cfg;
};

/* **********************************
 * power control define
 * **********************************
 */
enum lcd_power_type_e {
	LCD_POWER_TYPE_CPU = 0,
	LCD_POWER_TYPE_PMU,
	LCD_POWER_TYPE_SIGNAL,
	LCD_POWER_TYPE_EXTERN,
	LCD_POWER_TYPE_WAIT_GPIO,
	LCD_POWER_TYPE_CLK_SS,
	LCD_POWER_TYPE_TCON_SPI_DATA_LOAD,
	LCD_POWER_TYPE_MAX,
};

enum lcd_pmu_gpio_e {
	LCD_PMU_GPIO0 = 0,
	LCD_PMU_GPIO1,
	LCD_PMU_GPIO2,
	LCD_PMU_GPIO3,
	LCD_PMU_GPIO4,
	LCD_PMU_GPIO_MAX,
};

#define LCD_CLK_SS_BIT_FREQ             0
#define LCD_CLK_SS_BIT_MODE             4

/* Power Control */
#define LCD_CPU_GPIO_NUM_MAX         10
struct lcd_cpu_gpio_s {
	char name[LCD_CPU_GPIO_NAME_MAX];
	struct gpio_desc *gpio;
	int probe_flag;
	int register_flag;
};

#define LCD_PMU_GPIO_NUM_MAX         3
struct lcd_pmu_gpio_s {
	char name[LCD_CPU_GPIO_NAME_MAX];
	int gpio;
};

#define LCD_PWR_STEP_MAX         15
struct lcd_power_step_s {
	unsigned char type;
	unsigned int index; /* point to lcd_cpu_gpio_s or lcd_pmu_gpio_s or lcd_extern */
	unsigned short value;
	unsigned short delay;
};

struct lcd_power_ctrl_s {
	struct lcd_cpu_gpio_s cpu_gpio[LCD_CPU_GPIO_NUM_MAX];
	struct lcd_pmu_gpio_s pmu_gpio[LCD_PMU_GPIO_NUM_MAX];
	struct lcd_power_step_s power_on_step[LCD_PWR_STEP_MAX];
	struct lcd_power_step_s power_off_step[LCD_PWR_STEP_MAX];
	int power_on_step_max; /*  internal use for debug */
	int power_off_step_max; /* internal use for debug */
};

#define LCD_ENABLE_RETRY_MAX    3
struct lcd_config_s {
	char propname[24];
	unsigned int backlight_index;
	struct lcd_basic_s basic;
	struct lcd_timing_s timing;
	union lcd_ctrl_config_u control;
	struct lcd_power_ctrl_s power;
	struct lcd_optical_info_s optical;
	unsigned int vlock_param[5];
	struct pinctrl *pin;
	unsigned char pinmux_flag;
	unsigned char change_flag;
	unsigned char retry_enable_flag;
	unsigned char retry_enable_cnt;
};

#define LCD_INIT_LEVEL_NORMAL         0
#define LCD_INIT_LEVEL_PWR_OFF        1
#define LCD_INIT_LEVEL_KERNEL_ON      2
#define LCD_INIT_LEVEL_KERNEL_OFF     3

/*
 *bit[31:20]: reserved
 *bit[19:18]: lcd_init_level
 *bit[17:16]: reserved
 *bit[15:8]: advanced flag(p2p_type when lcd_type=p2p)
 *bit[7:4]: lcd bits
 *bit[3:0]: lcd_type
 */
struct lcd_boot_ctrl_s {
	unsigned char lcd_type;
	unsigned char lcd_bits;
	unsigned char advanced_flag;
	unsigned char init_level;
};

/*
 *bit[31:30]: lcd mode(0=normal, 1=tv; 2=tablet, 3=TBD)
 *bit[29:28]: lcd debug para source(0=normal, 1=dts, 2=unifykey,
 *                                  3=bsp for uboot)
 *bit[27:16]: reserved
 *bit[15:8]: lcd test pattern
 *bit[7:0]:  lcd debug print flag
 */
struct lcd_debug_ctrl_s {
	unsigned char debug_print_flag;
	unsigned char debug_test_pattern;
	unsigned char debug_para_source;
	unsigned char debug_lcd_mode;
};

struct lcd_duration_s {
	unsigned int duration_num;
	unsigned int duration_den;
	unsigned int frac;
};

struct lcd_data_s {
	enum lcd_chip_e chip_type;
	const char *chip_name;
	int *reg_map_table;
	unsigned char drv_max;
	unsigned int offset_venc[LCD_MAX_DRV];
	unsigned int offset_venc_if[LCD_MAX_DRV];
	unsigned int offset_venc_data[LCD_MAX_DRV];
};

struct lcd_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

#define LCD_RESUME_PREPARE       BIT(0)
#define LCD_RESUME_ENABLE        BIT(1)

#define LCD_STATUS_IF_ON         BIT(0)
#define LCD_STATUS_ENCL_ON       BIT(1)
#define LCD_STATUS_VMODE_ACTIVE  BIT(2)
#define LCD_STATUS_ON         (LCD_STATUS_IF_ON | LCD_STATUS_ENCL_ON)

#define LCD_VIU_SEL_NONE         0
#define EXTERN_MUL_MAX	         10
struct aml_lcd_drv_s {
	unsigned int index;
	unsigned int status;
	unsigned char mode;
	unsigned char lcd_pxp;
	unsigned char key_valid;
	unsigned char clk_path; /* 0=hpll, 1=gp0_pll */
	unsigned char config_load;
	unsigned char resume_type; /* 0=directly, 1=workqueue */
	unsigned char resume_flag;
	unsigned char init_flag; /* 0=none, 1=power on request */
	unsigned char auto_test;
	unsigned char test_state;
	unsigned char test_flag;
	unsigned char mute_state;
	unsigned char mute_flag;
	unsigned char probe_done;
	unsigned char viu_sel;
	unsigned char gamma_en_flag;
	unsigned char projection_en;
	unsigned char vsync_none_timer_flag;
	char vsync_isr_name[3][15];
	char vbyone_isr_name[10];
	char output_name[30];
	unsigned int output_vmode;

	struct lcd_data_s *data;
	struct cdev cdev;
	struct device *dev;
	struct lcd_config_s config;
	struct lcd_duration_s std_duration;
	struct vinfo_s vinfo;
	void *clk_conf;
	struct lcd_reg_map_s *reg_map;
	struct lcd_boot_ctrl_s *boot_ctrl;
	struct lcd_debug_ctrl_s *debug_ctrl;
	struct vout_server_s *vout_server[3];
#ifdef CONFIG_OF
	struct device_node *of_node;
#endif

	unsigned int vout_state;
	unsigned int fr_auto_policy;
	unsigned int fr_mode;
	unsigned int fr_duration;
	unsigned int tcon_status;
	unsigned int vsync_cnt;
	unsigned int vsync_cnt_previous;

	void (*driver_init_pre)(struct aml_lcd_drv_s *pdrv);
	void (*driver_disable_post)(struct aml_lcd_drv_s *pdrv);
	int (*driver_init)(struct aml_lcd_drv_s *pdrv);
	void (*driver_disable)(struct aml_lcd_drv_s *pdrv);
	int (*driver_change)(struct aml_lcd_drv_s *pdrv);
	void (*module_reset)(struct aml_lcd_drv_s *pdrv);
	void (*module_tiny_reset)(struct aml_lcd_drv_s *pdrv);
	void (*lcd_screen_black)(struct aml_lcd_drv_s *pdrv);
	void (*lcd_screen_restore)(struct aml_lcd_drv_s *pdrv);
	void (*phy_set)(struct aml_lcd_drv_s *pdrv, int status);
	void (*fr_adjust)(struct aml_lcd_drv_s *pdrv, int duration);
	int (*vbyone_vsync_handler)(struct aml_lcd_drv_s *pdrv);

	struct work_struct config_probe_work;
	struct work_struct tcon_config_work;
	struct work_struct test_check_work;
	struct work_struct late_resume_work;
	struct work_struct vx1_reset_work;
	struct delayed_work test_delayed_work;
	struct resource *res_vsync_irq[3];
	struct resource *res_vx1_irq;
	struct resource *res_tcon_irq;
	struct timer_list pll_mnt_timer;
	struct timer_list vs_none_timer;
	spinlock_t isr_lock; /* for mute and test isr */

#ifdef CONFIG_AMLOGIC_VPU
	struct vpu_dev_s *lcd_vpu_dev;
#endif
};

struct aml_lcd_drv_s *aml_lcd_get_driver(int index);

void lcd_vlock_m_update(int index, unsigned int vlock_m);
void lcd_vlock_frac_update(int index, unsigned int vlock_farc);

extern struct mutex lcd_power_mutex;

/* **********************************
 * IOCTL define
 * **********************************
 */
#define LCD_IOC_TYPE               'C'
#define LCD_IOC_NR_GET_HDR_INFO    0x0
#define LCD_IOC_NR_SET_HDR_INFO    0x1

#define LCD_IOC_CMD_GET_HDR_INFO   \
	_IOR(LCD_IOC_TYPE, LCD_IOC_NR_GET_HDR_INFO, struct lcd_optical_info_s)
#define LCD_IOC_CMD_SET_HDR_INFO   \
	_IOW(LCD_IOC_TYPE, LCD_IOC_NR_SET_HDR_INFO, struct lcd_optical_info_s)

#endif
