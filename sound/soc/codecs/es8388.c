/*
 * es8388.c -- es8388 ALSA SoC audio driver
 *
 * Copyright 2009 Wolfson Microelectronics plc
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include "es8388.h"

#if 1
#define DBG(x...) printk(x)
#else
#define DBG(x...) do { } while (0)
#endif
#define alsa_dbg DBG

#define KERNEL_4_9_XX

#define SND_SOC_SPI 1
#define SND_SOC_I2C 2

#define INVALID_GPIO -1

#define ES8388_CODEC_SET_SPK	1
#define ES8388_CODEC_SET_HP	2

static bool hp_irq_flag = 0;

//SPK HP vol control
#ifndef es8388_DEF_VOL
#define es8388_DEF_VOL	0x1b
#endif

static int es8388_set_bias_level(struct snd_soc_component *component,enum snd_soc_bias_level level);

/*
 * es8388 register cache
 * We can't read the es8388 register space when we
 * are using 2 wire for device control, so we cache them instead.
 */
#if 0
static u16 es8388_reg[] = {
	0x06, 0x1C, 0xC3, 0xFC,  /*  0 *////0x0100 0x0180
	0xC0, 0x00, 0x00, 0x7C,  /*  4 */
	0x80, 0x00, 0x00, 0x06,  /*  8 */
	0x00, 0x06, 0x30, 0x30,  /* 12 */
	0xC0, 0xC0, 0x38, 0xB0,  /* 16 */
	0x32, 0x06, 0x00, 0x00,  /* 20 */
	0x06, 0x30, 0xC0, 0xC0,  /* 24 */
	0x08, 0x06, 0x1F, 0xF7,  /* 28 */
	0xFD, 0xFF, 0x1F, 0xF7,  /* 32 */
	0xFD, 0xFF, 0x00, 0x38,  /* 36 */
	0x38, 0x38, 0x38, 0x38,  /* 40 */
	0x38, 0x00, 0x00, 0x00,  /* 44 */
	0x00, 0x00, 0x00, 0x00,  /* 48 */
	0x00, 0x00, 0x00, 0x00,  /* 52 */
};
#endif
/* codec private data */
struct es8388_priv {
	unsigned int sysclk;
	int control_type;
	struct snd_pcm_hw_constraint_list *sysclk_constraints;

	int spk_ctl_gpio;
	int hp_ctl_gpio;
	int hp_det_gpio;

	bool spk_gpio_level;
	bool hp_gpio_level;
	bool hp_det_level;
	void *control_data;
};

struct es8388_priv *es8388_private;

#if 0
static int es8388_set_gpio(int gpio, bool level)
{
	struct es8388_priv *es8388 = es8388_private;

	if (!es8388) {
		printk("%s : es8388_priv is NULL\n", __func__);
		return 0;
	}

	DBG("%s : set %s %s ctl gpio %s\n", __func__,
		gpio & ES8388_CODEC_SET_SPK ? "spk" : "",
		gpio & ES8388_CODEC_SET_HP ? "hp" : "",
		level ? "HIGH" : "LOW");

	if ((gpio & ES8388_CODEC_SET_SPK) && es8388 && es8388->spk_ctl_gpio != INVALID_GPIO) {
		gpio_set_value(es8388->spk_ctl_gpio, level);
	}

	if ((gpio & ES8388_CODEC_SET_HP) && es8388 && es8388->hp_ctl_gpio != INVALID_GPIO) {
		gpio_set_value(es8388->hp_ctl_gpio, level);
	}

	return 0;
}
#endif

static char mute_flag = 1;
static irqreturn_t hp_det_irq_handler(int irq, void *dev_id)
{
	int ret;
	unsigned int type;
	struct es8388_priv *es8388 = es8388_private;

	DBG("%s-- Enter!\n",__FUNCTION__);
	printk("%s\n", __func__);

	disable_irq_nosync(irq);

	type = gpio_get_value(es8388->hp_det_gpio) ? IRQ_TYPE_EDGE_FALLING : IRQ_TYPE_EDGE_RISING;
	ret = irq_set_irq_type(irq, type);
	if (ret < 0) {
		pr_err("%s: irq_set_irq_type(%d, %d) failed\n", __func__, irq, type);
		return -1;
	}
	else
		pr_err("%s: irq_set_irq_type(%d, %d) OK!\n", __func__, irq, type);

	if (type == IRQ_TYPE_EDGE_FALLING)
		hp_irq_flag = 0;
	else
		hp_irq_flag = 1;
/*
	if (mute_flag == 0)
	{
		if(es8388->hp_det_level == gpio_get_value(es8388->hp_det_gpio)){
			DBG("%s--hp_det_level = 0,insert hp\n",__FUNCTION__);
			es8388_set_gpio(ES8388_CODEC_SET_SPK,!es8388->spk_gpio_level);
			es8388_set_gpio(ES8388_CODEC_SET_HP,es8388->hp_gpio_level);
		}else{
			DBG("%s--hp_det_level = 1,deinsert hp\n",__FUNCTION__);
			es8388_set_gpio(ES8388_CODEC_SET_SPK,es8388->spk_gpio_level);
			es8388_set_gpio(ES8388_CODEC_SET_HP,!es8388->hp_gpio_level);
		}
	}
*/
	enable_irq(irq);

	return IRQ_HANDLED;
}


#if 0
static unsigned int es8388_read_reg_cache(struct snd_soc_component *component,
				     unsigned int reg)
{
	//u16 *cache = component->reg_cache;
	if (reg >= ARRAY_SIZE(es8388_reg))
		return -1;
	return es8388_reg[reg];
}
#else
static unsigned int es8388_read_reg(struct snd_soc_component *component,
				     unsigned int reg)
{
	unsigned int ret;
	u8 read_cmd[3] = {0};
	u8 cmd_len = 0;
	u8 rt_value[2];
	struct es8388_priv *es8388 = snd_soc_component_get_drvdata(component);

	read_cmd[0] = reg;
	cmd_len = 1;

//	if (client->adapter == NULL)
//		pr_err("es8388_read client->adapter==NULL\n");

	ret = i2c_master_send(es8388->control_data, read_cmd, cmd_len);
	if (ret != cmd_len) {
		pr_err("es8388_read error1\n");
		return -1;
	}

	ret = i2c_master_recv(es8388->control_data, rt_value, 1);
	if (ret != 1) {
		pr_err("es8388_read error2, ret = %d.\n", ret);
		return -1;
	}
	ret = rt_value[0];
	return ret;
}
#endif

static int es8388_write(struct snd_soc_component *component, unsigned int reg,
			     unsigned int value)
{

#if 0
	//u16 *cache = component->reg_cache;
	u8 data[2];
	int ret;
	struct es8388_priv *es8388 = snd_soc_component_get_drvdata(component);

//	DBG("Enter::%s----%d\n",__FUNCTION__,__LINE__);
	printk("%s\n", __func__);
//	BUG_ON(component->volatile_register);
	data[0] = reg & 0xff;
	data[1] = value & 0x00ff;

	if (reg < ARRAY_SIZE(es8388_reg))
		es8388_reg[reg] = value;
	ret = i2c_master_recv(es8388->control_data, data, 2);
	if (ret == 2)
		return 0;
	if (ret < 0)
		return ret;
	else
		return -EIO;
#else
	int ret = 0;
	u8 write_cmd[2] = {0};
	struct es8388_priv *es8388 = snd_soc_component_get_drvdata(component);

	write_cmd[0] = reg;
	write_cmd[1] = value;

	ret = i2c_master_send(es8388->control_data, write_cmd, 2);
	if (ret != 2) {
		pr_err("es8388_write error->[REG-0x%02x,val-0x%02x]\n",
				reg, value);
		return -1;
	}

	return 0;
#endif
}



//#define es8388_reset(c)	snd_soc_component_write(c, es8388_RESET, 0)
 static int es8388_reset(struct snd_soc_component *component)
 {
 	int ret = -1;
	printk("%s\n", __func__);
 	ret = snd_soc_component_write(component, ES8388_CONTROL1, 0x80);
 	if(ret)
 		DBG("%s-- snd_soc_component_write failed!\n",__FUNCTION__);
 	else
 		DBG("%s-- snd_soc_component_write success!\n",__FUNCTION__);

 	msleep(100);

  	return snd_soc_component_write(component, ES8388_CONTROL1, 0x00);
 }

static const char *es8388_line_texts[] = {
	"Line 1", "Line 2", "PGA"};

static const unsigned int es8388_line_values[] = {
	0, 1, 3};
static const char *es8388_pga_sel[] = {"Input 1", "Input 2", "Differential"};
static const char *stereo_3d_txt[] = {"No 3D  ", "Level 1","Level 2","Level 3","Level 4","Level 5","Level 6","Level 7"};
static const char *alc_func_txt[] = {"Off", "Right", "Left", "ALCStereo"};
static const char *ng_type_txt[] = {"Constant PGA Gain","Mute ADC Output"};
static const char *deemph_txt[] = {"None", "32Khz", "44.1Khz", "48Khz"};
static const char *adcpol_txt[] = {"Normal", "L Invert", "R Invert","L + R Invert"};
static const char *es8388_mono_mux[] = {"Stereo", "Mono (Left)","Mono (Right)"};
static const char *es8388_diff_sel[] = {"Diff input 1", "Diff input 2"};

static const struct soc_enum es8388_enum[]={
	SOC_VALUE_ENUM_SINGLE(ES8388_DACCONTROL16, 3, 7, ARRAY_SIZE(es8388_line_texts), es8388_line_texts, es8388_line_values),/* LLINE */
	SOC_VALUE_ENUM_SINGLE(ES8388_DACCONTROL16, 0, 7, ARRAY_SIZE(es8388_line_texts), es8388_line_texts, es8388_line_values),/* rline	*/
	SOC_VALUE_ENUM_SINGLE(ES8388_ADCCONTROL2, 6, 3, ARRAY_SIZE(es8388_pga_sel), es8388_pga_sel, es8388_line_values),/* Left PGA Mux */
	SOC_VALUE_ENUM_SINGLE(ES8388_ADCCONTROL2, 4, 3, ARRAY_SIZE(es8388_pga_sel), es8388_pga_sel, es8388_line_values),/* Right PGA Mux */
	SOC_ENUM_SINGLE(ES8388_DACCONTROL7, 2, 8, stereo_3d_txt),/* stereo-3d */
	SOC_ENUM_SINGLE(ES8388_ADCCONTROL10, 6, 4, alc_func_txt),/*alc func*/
	SOC_ENUM_SINGLE(ES8388_ADCCONTROL14, 1, 2, ng_type_txt),/*noise gate type*/
	SOC_ENUM_SINGLE(ES8388_DACCONTROL6, 6, 4, deemph_txt),/*Playback De-emphasis*/
	SOC_ENUM_SINGLE(ES8388_ADCCONTROL6, 6, 4, adcpol_txt),
	SOC_ENUM_SINGLE(ES8388_ADCCONTROL3, 3, 3, es8388_mono_mux),
	SOC_ENUM_SINGLE(ES8388_ADCCONTROL3, 7, 2, es8388_diff_sel),
};

static const DECLARE_TLV_DB_SCALE(pga_tlv, 0, 300, 0);
static const DECLARE_TLV_DB_SCALE(adc_tlv, -9600, 50, 1);
static const DECLARE_TLV_DB_SCALE(dac_tlv, -9600, 50, 1);
static const DECLARE_TLV_DB_SCALE(out_tlv, -4500, 150, 0);
static const DECLARE_TLV_DB_SCALE(bypass_tlv, -1500, 300, 0);

static const struct snd_kcontrol_new es8388_snd_controls[] = {
	SOC_ENUM("3D Mode", es8388_enum[4]),
	SOC_SINGLE_TLV("L PGA", ES8388_ADCCONTROL1,
    0, 8, 0, pga_tlv),
  SOC_SINGLE_TLV("R PGA", ES8388_ADCCONTROL1,
    4, 8, 0, pga_tlv),
  SOC_ENUM("Diffinput Set",es8388_enum[10]),
	SOC_SINGLE("ALC Capture Target Volume", ES8388_ADCCONTROL11, 4, 15, 0),
	SOC_SINGLE("ALC Capture Max PGA", ES8388_ADCCONTROL10, 3, 7, 0),
	SOC_SINGLE("ALC Capture Min PGA", ES8388_ADCCONTROL10, 0, 7, 0),
	SOC_ENUM("ALC Capture Function", es8388_enum[5]),
	SOC_SINGLE("ALC Capture ZC Switch", ES8388_ADCCONTROL13, 6, 1, 0),
	SOC_SINGLE("ALC Capture Hold Time", ES8388_ADCCONTROL11, 0, 15, 0),
	SOC_SINGLE("ALC Capture Decay Time", ES8388_ADCCONTROL12, 4, 15, 0),
	SOC_SINGLE("ALC Capture Attack Time", ES8388_ADCCONTROL12, 0, 15, 0),
	SOC_SINGLE("ALC Capture NG Threshold", ES8388_ADCCONTROL14, 3, 31, 0),
	SOC_ENUM("ALC Capture NG Type",es8388_enum[6]),
	SOC_SINGLE("ALC Capture NG Switch", ES8388_ADCCONTROL14, 0, 1, 0),
	SOC_SINGLE("ZC Timeout Switch", ES8388_ADCCONTROL13, 6, 1, 0),
	SOC_DOUBLE_R_TLV("Capture Digital Volume", ES8388_ADCCONTROL8, ES8388_ADCCONTROL9,0, 255, 1, adc_tlv),
	SOC_SINGLE("Capture Mute", ES8388_ADCCONTROL7, 2, 1, 0),
	SOC_SINGLE_TLV("Left Channel Capture Volume",	ES8388_ADCCONTROL1, 4, 15, 0, bypass_tlv),
	SOC_SINGLE_TLV("Right Channel Capture Volume",	ES8388_ADCCONTROL1, 0, 15, 0, bypass_tlv),
	SOC_ENUM("Playback De-emphasis", es8388_enum[7]),
	SOC_ENUM("Capture Polarity", es8388_enum[8]),
	SOC_DOUBLE_R_TLV("PCM Volume", ES8388_DACCONTROL4, ES8388_DACCONTROL5, 0, 255, 1, dac_tlv),
	SOC_SINGLE_TLV("Left Mixer Left Bypass Volume", ES8388_DACCONTROL17, 3, 7, 1, bypass_tlv),
	SOC_SINGLE_TLV("Right Mixer Right Bypass Volume", ES8388_DACCONTROL20, 3, 7, 1, bypass_tlv),
	SOC_DOUBLE_R_TLV("Output 1 Playback Volume", ES8388_DACCONTROL24, ES8388_DACCONTROL25, 0, 64, 0, out_tlv),
	SOC_DOUBLE_R_TLV("Output 2 Playback Volume", ES8388_DACCONTROL26, ES8388_DACCONTROL27, 0, 64, 0, out_tlv),
};

static const struct snd_kcontrol_new es8388_left_line_controls =
	SOC_DAPM_ENUM("Route", es8388_enum[0]);

static const struct snd_kcontrol_new es8388_right_line_controls =
	SOC_DAPM_ENUM("Route", es8388_enum[1]);

/* Left PGA Mux */
static const struct snd_kcontrol_new es8388_left_pga_controls =
	SOC_DAPM_ENUM("Route", es8388_enum[2]);
/* Right PGA Mux */
static const struct snd_kcontrol_new es8388_right_pga_controls =
	SOC_DAPM_ENUM("Route", es8388_enum[3]);

/* Left Mixer */
static const struct snd_kcontrol_new es8388_left_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left Playback Switch", ES8388_DACCONTROL17, 7, 1, 0),
	SOC_DAPM_SINGLE("Left Bypass Switch", ES8388_DACCONTROL17, 6, 1, 0),
};

/* Right Mixer */
static const struct snd_kcontrol_new es8388_right_mixer_controls[] = {
	SOC_DAPM_SINGLE("Right Playback Switch", ES8388_DACCONTROL20, 7, 1, 0),
	SOC_DAPM_SINGLE("Right Bypass Switch", ES8388_DACCONTROL20, 6, 1, 0),
};

/* Differential Mux */
//static const char *es8388_diff_sel[] = {"Line 1", "Line 2"};
static const struct snd_kcontrol_new es8388_diffmux_controls =
	SOC_DAPM_ENUM("Route", es8388_enum[10]);

/* Mono ADC Mux */
static const struct snd_kcontrol_new es8388_monomux_controls =
	SOC_DAPM_ENUM("Route", es8388_enum[9]);

static const struct snd_soc_dapm_widget es8388_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_INPUT("MIC2"),
	SND_SOC_DAPM_INPUT("LINPUT1"),
	SND_SOC_DAPM_INPUT("LINPUT2"),
	SND_SOC_DAPM_INPUT("RINPUT1"),
	SND_SOC_DAPM_INPUT("RINPUT2"),

	SND_SOC_DAPM_MICBIAS("Mic Bias", ES8388_ADCPOWER, 3, 1),

	SND_SOC_DAPM_MUX("Differential Mux", SND_SOC_NOPM, 0, 0,
		&es8388_diffmux_controls),

	SND_SOC_DAPM_MUX("Left ADC Mux", SND_SOC_NOPM, 0, 0,
		&es8388_monomux_controls),
	SND_SOC_DAPM_MUX("Right ADC Mux", SND_SOC_NOPM, 0, 0,
		&es8388_monomux_controls),

	SND_SOC_DAPM_MUX("Left PGA Mux", ES8388_ADCPOWER, 7, 1,
		&es8388_left_pga_controls),
	SND_SOC_DAPM_MUX("Right PGA Mux", ES8388_ADCPOWER, 6, 1,
		&es8388_right_pga_controls),

	SND_SOC_DAPM_MUX("Left Line Mux", SND_SOC_NOPM, 0, 0,
		&es8388_left_line_controls),
	SND_SOC_DAPM_MUX("Right Line Mux", SND_SOC_NOPM, 0, 0,
		&es8388_right_line_controls),

	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", ES8388_ADCPOWER, 4, 1),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", ES8388_ADCPOWER, 5, 1),

	/* gModify.Cmmt Implement when suspend/startup */
	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", ES8388_DACPOWER, 7, 1),
	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", ES8388_DACPOWER, 6, 1),

	SND_SOC_DAPM_MIXER("Left Mixer", SND_SOC_NOPM, 0, 0,
		&es8388_left_mixer_controls[0],
		ARRAY_SIZE(es8388_left_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Mixer", SND_SOC_NOPM, 0, 0,
		&es8388_right_mixer_controls[0],
		ARRAY_SIZE(es8388_right_mixer_controls)),

	SND_SOC_DAPM_PGA("Right Out 2", ES8388_DACPOWER, 2, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 2", ES8388_DACPOWER, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Out 1", ES8388_DACPOWER, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 1", ES8388_DACPOWER, 5, 0, NULL, 0),
	//SND_SOC_DAPM_PGA("LAMP", ES8388_ADCCONTROL1, 4, 0, NULL, 0),
	//SND_SOC_DAPM_PGA("RAMP", ES8388_ADCCONTROL1, 0, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),
	SND_SOC_DAPM_OUTPUT("VREF"),
};

static const struct snd_soc_dapm_route es8388_dapm_routes[] = {
	{"MIC1", NULL, "Mic Bias"},
  {"MIC2", NULL, "Mic Bias"},

	{"Differential Mux", "Diff input 1", "MIC1"},
  {"Differential Mux", "Diff input 2", "MIC2"},


	{ "Left PGA Mux", "Input 1", "LINPUT1" },
	{ "Left PGA Mux", "Input 2", "LINPUT2" },
	{ "Left PGA Mux", "Differential", "Differential Mux" },

	{ "Right PGA Mux", "Input 1", "RINPUT1" },
	{ "Right PGA Mux", "Input 2", "RINPUT2" },
	{ "Right PGA Mux", "Differential", "Differential Mux" },


	{ "Left ADC Mux", "Stereo", "Left PGA Mux" },
	{ "Left ADC Mux", "Mono (Left)", "Left PGA Mux" },


	{ "Right ADC Mux", "Stereo", "Right PGA Mux" },
	{ "Right ADC Mux", "Mono (Right)", "Right PGA Mux" },


	{ "Left ADC", NULL, "Left ADC Mux" },
	{ "Right ADC", NULL, "Right ADC Mux" },

	{ "Left Line Mux", "Line 1", "LINPUT1" },
	{ "Left Line Mux", "Line 2", "LINPUT2" },
	{ "Left Line Mux", "PGA", "Left PGA Mux" },

	{ "Right Line Mux", "Line 1", "RINPUT1" },
	{ "Right Line Mux", "Line 2", "RINPUT2" },
	{ "Right Line Mux", "PGA", "Right PGA Mux" },

	{ "Left Mixer", "Left Playback Switch", "Left DAC" },
	{ "Left Mixer", "Left Bypass Switch", "Left Line Mux" },

	{ "Right Mixer", "Right Playback Switch", "Right DAC" },
	{ "Right Mixer", "Right Bypass Switch", "Right Line Mux" },

	{ "Left Out 1", NULL, "Left Mixer" },
	{ "LOUT1", NULL, "Left Out 1" },
	{ "Right Out 1", NULL, "Right Mixer" },
	{ "ROUT1", NULL, "Right Out 1" },

	{ "Left Out 2", NULL, "Left Mixer" },
	{ "LOUT2", NULL, "Left Out 2" },
	{ "Right Out 2", NULL, "Right Mixer" },
	{ "ROUT2", NULL, "Right Out 2" },
};

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u8 sr:4;
	u8 usb:1;
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{12288000, 8000, 1536, 0xa, 0x0},
	{11289600, 8000, 1408, 0x9, 0x0},
	{18432000, 8000, 2304, 0xc, 0x0},
	{16934400, 8000, 2112, 0xb, 0x0},
	{12000000, 8000, 1500, 0xb, 0x1},

	/* 11.025k */
	{11289600, 11025, 1024, 0x7, 0x0},
	{16934400, 11025, 1536, 0xa, 0x0},
	{12000000, 11025, 1088, 0x9, 0x1},

	/* 16k */
	{4096000, 16000, 256, 0x2, 0x0},
	{12288000, 16000, 768, 0x6, 0x0},
	{18432000, 16000, 1152, 0x8, 0x0},
	{12000000, 16000, 750, 0x7, 0x1},

	/* 22.05k */
	{11289600, 22050, 512, 0x4, 0x0},
	{16934400, 22050, 768, 0x6, 0x0},
	{12000000, 22050, 544, 0x6, 0x1},

	/* 32k */
	{12288000, 32000, 384, 0x3, 0x0},
	{18432000, 32000, 576, 0x5, 0x0},
	{12000000, 32000, 375, 0x4, 0x1},

	/* 44.1k */
	{11289600, 44100, 256, 0x2, 0x0},
	{16934400, 44100, 384, 0x3, 0x0},
	{12000000, 44100, 272, 0x3, 0x1},

	/* 48k */
	{12288000, 48000, 256, 0x2, 0x0},
	{11289600, 48000, 256, 0x2, 0x0},
	{18432000, 48000, 384, 0x3, 0x0},
	{12000000, 48000, 250, 0x2, 0x1},

	/* 88.2k */
	{11289600, 88200, 128, 0x0, 0x0},
	{16934400, 88200, 192, 0x1, 0x0},
	{12000000, 88200, 136, 0x1, 0x1},

	/* 96k */
	{12288000, 96000, 128, 0x0, 0x0},
	{11289600, 96000, 128, 0x0, 0x0},
	{18432000, 96000, 192, 0x1, 0x0},
	{12000000, 96000, 125, 0x0, 0x1},
};

static inline int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}

	return -EINVAL;
}

/* The set of rates we can generate from the above for each SYSCLK */

static unsigned int rates_12288[] = {
	8000, 12000, 16000, 24000, 24000, 32000, 48000, 96000,
};

static struct snd_pcm_hw_constraint_list constraints_12288 = {
	.count	= ARRAY_SIZE(rates_12288),
	.list	= rates_12288,
};

static unsigned int rates_112896[] = {
	8000, 11025, 22050, 44100,
};

static struct snd_pcm_hw_constraint_list constraints_112896 = {
	.count	= ARRAY_SIZE(rates_112896),
	.list	= rates_112896,
};

static unsigned int rates_12[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
	48000, 88235, 96000,
};

static struct snd_pcm_hw_constraint_list constraints_12 = {
	.count	= ARRAY_SIZE(rates_12),
	.list	= rates_12,
};

/*
 * Note that this should be called from init rather than from hw_params.
 */
static int es8388_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *component = codec_dai->component;
	struct es8388_priv *es8388 = snd_soc_component_get_drvdata(component);

    DBG("Enter::%s----%d\n",__FUNCTION__,__LINE__);
	printk("es8388_set_dai_sysclk freq = %d!\n",freq);

	switch (freq) {
	case 11289600:
	case 18432000:
	case 22579200:
	case 36864000:
		es8388->sysclk_constraints = &constraints_112896;
		es8388->sysclk = freq;
		return 0;

	case 12288000:
	case 16934400:
	case 24576000:
	case 33868800:
	case 4096000:
		es8388->sysclk_constraints = &constraints_12288;
		es8388->sysclk = freq;
		return 0;

	case 12000000:
	case 24000000:
		es8388->sysclk_constraints = &constraints_12;
		es8388->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int es8388_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
  struct snd_soc_component *component = codec_dai->component;
    u8 iface = 0;
    u8 adciface = 0;
    u8 daciface = 0;
    alsa_dbg("%s----%d, fmt[%02x]\n",__FUNCTION__,__LINE__,fmt);

	printk("es8388_set_dai_fmt fmt = %d! \n",fmt);

    iface    = snd_soc_component_read32(component, ES8388_IFACE);
    adciface = snd_soc_component_read32(component, ES8388_ADC_IFACE);
    daciface = snd_soc_component_read32(component, ES8388_DAC_IFACE);

    /* set master/slave audio interface */
    switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
        case SND_SOC_DAIFMT_CBM_CFM:    // MASTER MODE
		alsa_dbg("es8388 in master mode");
		//iface |= 0x80;
		break;
        case SND_SOC_DAIFMT_CBS_CFS:    // SLAVE MODE
		alsa_dbg("es8388 in slave mode");
		iface &= 0x7F;
		break;
        default:
            return -EINVAL;
    }

    /* interface format */
    switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
        case SND_SOC_DAIFMT_I2S:
            adciface &= 0xFC;
            //daciface &= 0xF9;  //updated by david-everest,5-25
            daciface &= 0xF9;
            break;
        case SND_SOC_DAIFMT_RIGHT_J:
            break;
        case SND_SOC_DAIFMT_LEFT_J:
            break;
        case SND_SOC_DAIFMT_DSP_A:
            break;
        case SND_SOC_DAIFMT_DSP_B:
            break;
        default:
            return -EINVAL;
    }

    /* clock inversion */
    switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
        case SND_SOC_DAIFMT_NB_NF:
            iface    &= 0xDF;
            adciface &= 0xDF;
            //daciface &= 0xDF;    //UPDATED BY david-everest,5-25
            daciface &= 0xBF;
            break;
        case SND_SOC_DAIFMT_IB_IF:
            iface    |= 0x20;
            //adciface &= 0xDF;    //UPDATED BY david-everest,5-25
            adciface |= 0x20;
            //daciface &= 0xDF;   //UPDATED BY david-everest,5-25
            daciface |= 0x40;
            break;
        case SND_SOC_DAIFMT_IB_NF:
            iface    |= 0x20;
           // adciface |= 0x40;  //UPDATED BY david-everest,5-25
            adciface &= 0xDF;
            //daciface |= 0x40;  //UPDATED BY david-everest,5-25
            daciface &= 0xBF;
            break;
        case SND_SOC_DAIFMT_NB_IF:
            iface    &= 0xDF;
            adciface |= 0x20;
            //daciface |= 0x20;  //UPDATED BY david-everest,5-25
            daciface |= 0x40;
            break;
        default:
            return -EINVAL;
    }

    snd_soc_component_write(component, ES8388_IFACE, iface);
    snd_soc_component_write(component, ES8388_ADC_IFACE, adciface);
    snd_soc_component_write(component, ES8388_DAC_IFACE, daciface);

    return 0;
}
#if 1
static int es8388_pcm_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	//struct snd_soc_component *component = dai->component;
	//struct es8388_priv *es8388 = snd_soc_component_get_drvdata(component);

	bool playback = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);
	printk("es8388_pcm_startup !\n");
	if(playback) {
  /*set Audio PA ON*/
	}

	return 0;
}

static void es8388_pcm_shutdown(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	//struct snd_soc_component *component = dai->component;
	//struct es8388_priv *es8388 = snd_soc_component_get_drvdata(component);
	bool playback = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);
	printk("es8388_pcm_shutdown !\n");
	if(playback) {
  /*set Audio PA OFF*/
	}
//	return 0;
}
#endif


static int es8388_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
/*
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = rtd->component;
*/
	struct snd_soc_component *component = dai->component;
	struct es8388_priv *es8388 = snd_soc_component_get_drvdata(component);
	u16 srate;
	u16 adciface;
	u16 daciface;
	int coeff,temp;
//	int ret2,i;

	printk("%s\n", __func__);

	srate = snd_soc_component_read32(component, ES8388_IFACE) & 0x80;
	adciface = snd_soc_component_read32(component, ES8388_ADC_IFACE) & 0xE3;
	daciface = snd_soc_component_read32(component, ES8388_DAC_IFACE) & 0xC7;

	temp = params_rate(params);
	printk("%s params_rate(params) = %d ! es8388->sysclk = %d !\n", __func__,temp,es8388->sysclk);

//	es8388->sysclk = 12288000;
	coeff = get_coeff(es8388->sysclk, params_rate(params));
	printk("coeff = %d es8388->sysclk = %d \n",coeff,es8388->sysclk);

	if (coeff < 0) {
		coeff = get_coeff(es8388->sysclk / 2, params_rate(params));
		srate |= 0x40;
	}
	if (coeff < 0) {
		dev_err(component->dev,
			"Unable to configure sample rate %dHz with %dHz MCLK\n",
			params_rate(params), es8388->sysclk);
		return coeff;
	}

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		adciface |= 0x000C;
		daciface |= 0x0018;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		adciface |= 0x0004;
		daciface |= 0x0008;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		adciface |= 0x0010;
		daciface |= 0x0020;
		break;
	}

	/* set iface & srate*/
	snd_soc_component_write(component, ES8388_DAC_IFACE, daciface);
	snd_soc_component_write(component, ES8388_ADC_IFACE, adciface);

	if (coeff >= 0) {
		snd_soc_component_write(component, ES8388_IFACE, srate);
		snd_soc_component_write(component, ES8388_ADCCONTROL5, coeff_div[coeff].sr | (coeff_div[coeff].usb) << 4);
		snd_soc_component_write(component, ES8388_DACCONTROL2, coeff_div[coeff].sr | (coeff_div[coeff].usb) << 4);
	}

/*
	for(i=0;i<0x37;i++)
	{
		ret2 = snd_soc_component_read32(component, i);
		printk("%x = %x \n",i,ret2);
		msleep(50);
	}
*/
	pr_info("-%s()\n",__FUNCTION__);
	return 0;
}

static int es8388_mute(struct snd_soc_dai *dai, int mute)
{
	//struct snd_soc_component *component = dai->component;
	//struct es8388_priv *es8388 = es8388_private;
	// u16 mute_reg = snd_soc_component_read32(component, ES8388_DACCONTROL3) & 0xfb;

	printk("Enter::%s----%d--hp_irq_flag=%d  mute=%d\n",__FUNCTION__,__LINE__,hp_irq_flag,mute);

	mute_flag = mute;
/*
	if (mute)
	 {
		es8388_set_gpio(ES8388_CODEC_SET_SPK,!es8388->spk_gpio_level);
		es8388_set_gpio(ES8388_CODEC_SET_HP,!es8388->hp_gpio_level);
		msleep(100);
	    snd_soc_component_write(component, ES8388_DACCONTROL3, 0x06);
	 }
	else
	{
		snd_soc_component_write(component, ES8388_DACCONTROL3, 0x02);
		snd_soc_component_write(component, 0x30,es8388_DEF_VOL);
		snd_soc_component_write(component, 0x31,es8388_DEF_VOL);

		msleep(130);

		if(hp_irq_flag == 0)
			es8388_set_gpio(ES8388_CODEC_SET_SPK,es8388->spk_gpio_level);
		else
			es8388_set_gpio(ES8388_CODEC_SET_HP,es8388->hp_gpio_level);

		msleep(150);
	}
*/
	return 0;
}

static int es8388_set_bias_level(struct snd_soc_component *component,
				 enum snd_soc_bias_level level)
{
#ifdef KERNEL_4_9_XX
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(component);
#endif
	printk("%s\n", __func__);
	switch (level) {
	case SND_SOC_BIAS_ON:
		dev_dbg(component->dev, "%s on\n", __func__);
		break;
	case SND_SOC_BIAS_PREPARE:
		dev_dbg(component->dev, "%s prepare\n", __func__);
		snd_soc_component_write(component, ES8388_ANAVOLMANAG, 0x7a);
		snd_soc_component_write(component, ES8388_CHIPLOPOW1, 0x00);
		snd_soc_component_write(component, ES8388_CHIPLOPOW2, 0x00);
		snd_soc_component_write(component, ES8388_CHIPPOWER, 0x00);
		snd_soc_component_write(component, ES8388_ADCPOWER, 0x59);
		break;
	case SND_SOC_BIAS_STANDBY:
		dev_dbg(component->dev, "%s standby\n", __func__);
	  	snd_soc_component_write(component, ES8388_ANAVOLMANAG, 0x7a);
  		snd_soc_component_write(component, ES8388_CHIPLOPOW1, 0x00);
	  	snd_soc_component_write(component, ES8388_CHIPLOPOW2, 0x00);
		snd_soc_component_write(component, ES8388_CHIPPOWER, 0x00);
		snd_soc_component_write(component, ES8388_ADCPOWER, 0x59);
		break;
	case SND_SOC_BIAS_OFF:
		dev_dbg(component->dev, "%s off\n", __func__);
		snd_soc_component_write(component, ES8388_ADCPOWER, 0xFF);
		snd_soc_component_write(component, ES8388_DACPOWER, 0xC0);
		snd_soc_component_write(component, ES8388_CHIPLOPOW1, 0xFF);
		snd_soc_component_write(component, ES8388_CHIPLOPOW2, 0xFF);
		snd_soc_component_write(component, ES8388_CHIPPOWER, 0xFF);
		snd_soc_component_write(component, ES8388_ANAVOLMANAG, 0x7B);
		break;
	}

#ifdef KERNEL_4_9_XX
	dapm->bias_level = level;
#else
	component->dapm.bias_level = level;
#endif
	return 0;
}

#define es8388_RATES SNDRV_PCM_RATE_8000_96000

#define es8388_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops es8388_ops = {
	.startup = es8388_pcm_startup,
	.shutdown = es8388_pcm_shutdown,
	.hw_params = es8388_pcm_hw_params,
	.set_fmt = es8388_set_dai_fmt,
	.set_sysclk = es8388_set_dai_sysclk,
	.digital_mute = es8388_mute,
};

static struct snd_soc_dai_driver es8388_dai = {
	.name = "es8388",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = es8388_RATES,
		.formats = es8388_FORMATS,
	},

	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = es8388_RATES,
		.formats = es8388_FORMATS,
	 },

	.ops = &es8388_ops,
	.symmetric_rates = 1,
};

static int es8388_suspend(struct snd_soc_component *component)
{
	// u16 i;
	printk("Enter::%s----%d\n",__FUNCTION__,__LINE__);
	snd_soc_component_write(component, 0x19, 0x06);
	snd_soc_component_write(component, ES8388_ADCPOWER, 0xFF);
	snd_soc_component_write(component, ES8388_DACPOWER, 0xc0);
	snd_soc_component_write(component, ES8388_CHIPPOWER, 0xF3);

	msleep(50);

	return 0;
}

static int es8388_resume(struct snd_soc_component *component)
{
	printk("Enter::%s----%d\n",__FUNCTION__,__LINE__);
	snd_soc_component_write(component, ES8388_CHIPPOWER, 0x00);
	snd_soc_component_write(component, ES8388_DACPOWER, 0x0c);
	snd_soc_component_write(component, ES8388_ADCPOWER, 0x00);
	snd_soc_component_write(component, 0x19, 0x02);
	return 0;
}

static struct snd_soc_component *es8388_component;
static int es8388_probe(struct snd_soc_component *component)
{
//	struct es8388_priv *es8388 = snd_soc_component_get_drvdata(component);
	int ret = -1;

   	printk("Enter::%s----%d\n",__FUNCTION__,__LINE__);

	if (component == NULL) {
		dev_err(component->dev, "component device not registered\n");
		return -ENODEV;
	}
	else
		pr_info("%s() component != NULL\n",__FUNCTION__);
/*
	component->read  = es8388_read_reg_cache;
	component->write = es8388_write;
*/
//	component->hw_write = (hw_write_t)i2c_master_recv;//i2c_master_send;
//	component->control_data = container_of(component->dev, struct i2c_client, dev);
//	component->control_data = es8388->control_data;

	es8388_component = component;
	ret = es8388_reset(component);
	if (ret < 0) {
		dev_err(component->dev, "%s--Failed to issue reset\n",__FUNCTION__);
//		return ret;
	}
	else
		pr_info("%s() es8388_reset OK!\n",__FUNCTION__);

	#if 1
	snd_soc_component_write(component, 0x35  , 0xa0);
	snd_soc_component_write(component, 0x38  , 0x02);
	//snd_soc_component_write(component, 0x36  , 0x08); //for 1.8V VDD
	//snd_soc_component_write(component, 0x08,0x80);   //ES8388 salve
	msleep(100);
	ret = snd_soc_component_write(component, 0x02,0xf3);
 	if(ret)
 		DBG("%s-- snd_soc_component_write failed!\n",__FUNCTION__);
 	else
 		DBG("%s-- snd_soc_component_write success!\n",__FUNCTION__);

	snd_soc_component_write(component, 0x2B,0x80);
	snd_soc_component_write(component, 0x08,0x00);   //ES8388 salve
	snd_soc_component_write(component, 0x00,0x16);   //
	snd_soc_component_write(component, 0x01,0x50);   //PLAYBACK & RECORD Mode,EnRefr=1
	snd_soc_component_write(component, 0x03,0x00);   //ori:0x59 yangjr//pdn_ana=0,ibiasgen_pdn=0
	snd_soc_component_write(component, 0x05,0xe8);   //pdn_ana=0,ibiasgen_pdn=0
	snd_soc_component_write(component, 0x06,0xc3);   //pdn_ana=0,ibiasgen_pdn=0
	snd_soc_component_write(component, 0x07,0x7a);
	snd_soc_component_write(component, 0x09,0x80);  //ADC L/R PGA =  +24dB
	snd_soc_component_write(component, 0x0a,0xf0);  //ori:0xf0 yangjr//ADC INPUT=LIN2/RIN2
	snd_soc_component_write(component, 0x0b,0x80);  //ADC INPUT=LIN2/RIN2 //82
	snd_soc_component_write(component, 0x0C,0x0C);  //ori:0x4c//I2S-24BIT
	snd_soc_component_write(component, 0x0d,0x02);  //MCLK/LRCK=256
	snd_soc_component_write(component, 0x10,0x00);  //ADC Left Volume=0db
	snd_soc_component_write(component, 0x11,0x00);  //ADC Right Volume=0db
	snd_soc_component_write(component, 0x12,0x00); // ALC stereo MAXGAIN: 35.5dB,  MINGAIN: +6dB (Record Volume increased!)
	snd_soc_component_write(component, 0x13,0xc0);
	snd_soc_component_write(component, 0x14,0x05);
	snd_soc_component_write(component, 0x15,0x06);
	snd_soc_component_write(component, 0x16,0x93);
	snd_soc_component_write(component, 0x17,0x18);  //ori:0x18 yangjr//I2S-16BIT
	snd_soc_component_write(component, 0x18,0x02);
	snd_soc_component_write(component, 0x1A,0x00);  //ori:0x0A yangjr//DAC VOLUME=0DB
	snd_soc_component_write(component, 0x1B,0x00);  //ori:0x0A yangjr
    /*
    snd_soc_component_write(component, 0x1E,0x01);    //for 47uF capacitors ,15db Bass@90Hz,Fs=44100
    snd_soc_component_write(component, 0x1F,0x84);
    snd_soc_component_write(component, 0x20,0xED);
    snd_soc_component_write(component, 0x21,0xAF);
    snd_soc_component_write(component, 0x22,0x20);
    snd_soc_component_write(component, 0x23,0x6C);
    snd_soc_component_write(component, 0x24,0xE9);
    snd_soc_component_write(component, 0x25,0xBE);
    */
	snd_soc_component_write(component, 0x26,0x12);  //ori:0x12,yangjr//Left DAC TO Left IXER
	snd_soc_component_write(component, 0x27,0xb8);  //ori:0xb8,yangjr//Left DAC TO Left MIXER
	snd_soc_component_write(component, 0x28,0x38);
	snd_soc_component_write(component, 0x29,0x38);
	snd_soc_component_write(component, 0x2A,0xb8);	//ori:0xb8,yangjr
	snd_soc_component_write(component, 0x02,0x00); //aa //START DLL and state-machine,START DSM
	snd_soc_component_write(component, 0x19,0x02);  //SOFT RAMP RATE=32LRCKS/STEP,Enable ZERO-CROSS CHECK,DAC MUTE
	snd_soc_component_write(component, 0x04,0x3c);   //pdn_ana=0,ibiasgen_pdn=0
	msleep(100);
	snd_soc_component_write(component, 0x2e,0x08);
	snd_soc_component_write(component, 0x2f,0x08);
	snd_soc_component_write(component, 0x30,0x08);
	snd_soc_component_write(component, 0x31,0x08);
	msleep(200);
	snd_soc_component_write(component, 0x2e,0x0f);
	snd_soc_component_write(component, 0x2f,0x0f);
	snd_soc_component_write(component, 0x30,0x0f);
	snd_soc_component_write(component, 0x31,0x0f);
	msleep(200);
	snd_soc_component_write(component, 0x2e,0x1e); //ori:0x18
	snd_soc_component_write(component, 0x2f,0x1e); //ori:0x18
	snd_soc_component_write(component, 0x30,0x1e); //ori:0x18
	snd_soc_component_write(component, 0x31,0x1e); //ori:0x18

	//snd_soc_component_write(component, ES8388_DACCONTROL3, 0x06); //yangjr  [2]:1 mute analog outputs for both channels
	#endif

	// es8388_set_bias_level(component, SND_SOC_BIAS_STANDBY);

	snd_soc_component_write(component, ES8388_ANAVOLMANAG, 0x7a);
	snd_soc_component_write(component, ES8388_CHIPLOPOW1, 0x00);
	snd_soc_component_write(component, ES8388_CHIPLOPOW2, 0x00);
	snd_soc_component_write(component, ES8388_CHIPPOWER, 0x00);
	snd_soc_component_write(component, ES8388_ADCPOWER, 0x59);
	pr_info("%s()es8388_set_bias_level succeed!\n",__FUNCTION__);
	return 0;
}

static void es8388_remove(struct snd_soc_component *component)
{
	pr_info("+%s()\n",__FUNCTION__);
	es8388_set_bias_level(component, SND_SOC_BIAS_OFF);
	return;
}

static struct snd_soc_component_driver soc_codec_dev_es8388 = {
	.probe =	es8388_probe,
	.remove =	es8388_remove,
	.suspend =	es8388_suspend,
	.resume =	es8388_resume,
	.set_bias_level = es8388_set_bias_level,
	//.reg_cache_size = ARRAY_SIZE(es8388_reg),
	//.reg_word_size = sizeof(u16),
	//.reg_cache_default = es8388_reg,
	//------------------------------------------
	//.volatile_register = es8388_volatile_register,
	//.readable_register = es8388_readable_register,
	//.reg_cache_step = 1,
	.read = es8388_read_reg,
	.write = es8388_write,

//#ifdef KERNEL_4_9_XX
//	.component_driver = {
//#endif
	.controls = es8388_snd_controls,
	.num_controls = ARRAY_SIZE(es8388_snd_controls),
 	.dapm_routes = es8388_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(es8388_dapm_routes),
	.dapm_widgets = es8388_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(es8388_dapm_widgets),

	//--------------------------------------------------
//#ifdef KERNEL_4_9_XX
//	}
//#endif
};

/*
dts:
	codec@10 {
		compatible = "es8388";
		reg = <0x10>;
		spk_ctl_gpio = <&gpio2 GPIO_D7 GPIO_ACTIVE_HIGH>;
		hp-con-gpio = <&gpio2 GPIO_D7 GPIO_ACTIVE_HIGH>;
		hp-det-gpio = <&gpio0 GPIO_B5 GPIO_ACTIVE_HIGH>;
	};
*/

static int es8388_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct es8388_priv *es8388;
	int ret = -1;
	unsigned long irq_flag=0;
	int hp_irq = 0;
	enum of_gpio_flags flags;
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	char reg;

	pr_info("+%s()\n",__FUNCTION__);

	 if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
    		dev_warn(&adapter->dev,
        		"I2C-Adapter doesn't support I2C_FUNC_I2C\n");
	        return -EIO;
	    }

	es8388 = devm_kzalloc(&i2c->dev,sizeof(struct es8388_priv), GFP_KERNEL);
	if (es8388 == NULL)
		return -ENOMEM;

//	snd_soc_codec_set_drvdata(codec, es8388);

	i2c_set_clientdata(i2c, es8388);
	es8388->control_type = SND_SOC_I2C;
	es8388->control_data = i2c;

	es8388->spk_ctl_gpio = of_get_named_gpio(i2c->dev.of_node, "es8388-ce-gpio", 0);
	if (es8388->spk_ctl_gpio < 0){
		pr_info("%s() spk_ctl_gpio < 0\n",__FUNCTION__);
		es8388->spk_ctl_gpio = -1;
	}
	if (!gpio_is_valid(es8388->spk_ctl_gpio)) {
		pr_notice("es8388 pdn pin(%u) is invalid\n", es8388->spk_ctl_gpio);
		es8388->spk_ctl_gpio = -1;
	}
	else
	{
		ret = gpio_request(es8388->spk_ctl_gpio, "es8328 ce-gpio");
		pr_info("%s : gpio_request ret = %d\n", __func__, ret);
		gpio_direction_output(es8388->spk_ctl_gpio, 0);
	}

	reg = ES8388_DACCONTROL18;
	ret = i2c_master_recv(i2c, &reg, 1);
	if (ret < 0){
		printk("es8388 probe error\n");
		return ret;
	}
	else
		printk("es8388 probe reg:%d value:%d\n",ES8388_DACCONTROL18,reg);

	es8388_private = es8388;

/*
	es8388->spk_ctl_gpio = of_get_named_gpio_flags(i2c->dev.of_node, "spk_ctl_gpio", 0, &flags);
	if (es8388->spk_ctl_gpio < 0) {
		DBG("%s() Can not read property spk codec-en-gpio\n", __FUNCTION__);
		es8388->spk_ctl_gpio = INVALID_GPIO;
	}
	else
	{
	    es8388->spk_gpio_level = (flags & OF_GPIO_ACTIVE_LOW)? 0:1;
	    ret = gpio_request(es8388->spk_ctl_gpio, NULL);
	    if (ret != 0) {
		    printk("%s request SPK_CON error", __func__);
		    return ret;
	    }
	    gpio_direction_output(es8388->spk_ctl_gpio,!es8388->spk_gpio_level);
	}

	es8388->hp_ctl_gpio = of_get_named_gpio_flags(i2c->dev.of_node, "hp-con-gpio", 0, &flags);
	if (es8388->hp_ctl_gpio < 0) {
		DBG("%s() Can not read property hp codec-en-gpio\n", __FUNCTION__);
		es8388->hp_ctl_gpio = INVALID_GPIO;
	}
	else
	{
	    es8388->hp_gpio_level = (flags & OF_GPIO_ACTIVE_LOW)? 0:1;
	    ret = gpio_request(es8388->hp_ctl_gpio, NULL);
	    if (ret != 0) {
		    printk("%s request hp_ctl error", __func__);
		    return ret;
	    }
	    gpio_direction_output(es8388->hp_ctl_gpio,!es8388->hp_gpio_level);
	}
*/
	es8388->hp_det_gpio = of_get_named_gpio_flags(i2c->dev.of_node, "hp-det-gpio", 0, &flags);
	if (es8388->hp_det_gpio < 0) {
		DBG("%s() Can not read property hp_det gpio\n", __FUNCTION__);
		es8388->hp_det_gpio = INVALID_GPIO;
	}
	else
	{
		DBG("%s() hp_det_gpio:%d OK!\n", __FUNCTION__,es8388->hp_det_gpio);
		es8388->hp_det_level = (flags & OF_GPIO_ACTIVE_LOW)? 0:1;
		ret = gpio_request(es8388->hp_det_gpio, NULL);
		if (ret != 0) {
			printk("%s request HP_DET error", __func__);
			return ret;
		}
		else
			DBG("%s() gpio_request hp_det_gpio OK!\n", __FUNCTION__);
		ret = gpio_direction_input(es8388->hp_det_gpio);
	//	ret = gpio_direction_output(es8388->hp_det_gpio,1);
		DBG("%s() gpio_direction_input hp_det_gpio:%d!\n", __FUNCTION__,ret);

		if (ret) {
			printk("%s gpio_direction_input HP_DET error", __func__);
			return ret;
		}
		else
			DBG("%s() gpio_direction_input hp_det_gpio OK!\n", __FUNCTION__);
		ret = gpio_get_value(es8388->hp_det_gpio);

		irq_flag = IRQF_TRIGGER_LOW |IRQF_ONESHOT;
		hp_irq = gpio_to_irq(es8388->hp_det_gpio);

	    if (hp_irq){
		ret = request_threaded_irq(hp_irq, NULL, hp_det_irq_handler, irq_flag, "ES8388", NULL);
		if(ret == 0){
		    printk("%s:register ISR (irq=%d)\n", __FUNCTION__,hp_irq);
		}
		else
			printk("%s:request_irq hp_irq failed\n",__FUNCTION__);
	    }
	}

	ret =  snd_soc_register_component(&i2c->dev,
			&soc_codec_dev_es8388, &es8388_dai, 1);

	pr_info("-%s() ret:%d\n",__FUNCTION__,ret);

  #ifdef CONFIG_MACH_RK_FAC
  	es8388_hdmi_ctrl=1;
  #endif
	return ret;
}

static int es8388_i2c_remove(struct i2c_client *client)
{
	pr_info("+%s()\n",__FUNCTION__);
	devm_kfree(&client->dev, i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id es8388_i2c_id[] = {
	{ "es8388", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, es8388_i2c_id);
/*
static const struct of_device_id es8388_of_match[] = {
	{ .compatible = "everest,es8388", },
	{ }
};
MODULE_DEVICE_TABLE(of, es8388_of_match);
*/
#ifdef CONFIG_OF
static const struct of_device_id es8388_i2c_dt_ids[] = {
	{ .compatible = "everest,es8388"},
	{ }
};
MODULE_DEVICE_TABLE(of, es8388_i2c_dt_ids);
#endif

void es8388_i2c_shutdown(struct i2c_client *client)
{
	//struct es8388_priv *es8388 = es8388_private;
	pr_info("+%s()\n",__FUNCTION__);
	if (es8388_component != NULL) {
	//	es8388_set_gpio(ES8388_CODEC_SET_SPK,!es8388->spk_gpio_level);
	//	es8388_set_gpio(ES8388_CODEC_SET_HP,!es8388->hp_gpio_level);
		snd_soc_component_write(es8388_component, ES8388_CONTROL2, 0x58);
		snd_soc_component_write(es8388_component, ES8388_CONTROL1, 0x32);
		snd_soc_component_write(es8388_component, ES8388_CHIPPOWER, 0xf3);
		snd_soc_component_write(es8388_component, ES8388_DACPOWER, 0xc0);
		mdelay(150);
		snd_soc_component_write(es8388_component, ES8388_DACCONTROL26, 0x00);
		snd_soc_component_write(es8388_component, ES8388_DACCONTROL27, 0x00);
		mdelay(150);
		snd_soc_component_write(es8388_component, ES8388_CONTROL1, 0x30);
		snd_soc_component_write(es8388_component, ES8388_CONTROL1, 0x34);
	}
}

static struct i2c_driver es8388_i2c_driver = {
	.driver = {
		.name = "ES8388",
#ifndef KERNEL_4_9_XX
		.owner = THIS_MODULE,
#endif
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(es8388_i2c_dt_ids),
#endif
	},
	.shutdown = es8388_i2c_shutdown,
	.probe = es8388_i2c_probe,
	.remove = es8388_i2c_remove,
	.id_table = es8388_i2c_id,
};

static int __init es8388_init(void)
{
	pr_info("+%s()\n",__FUNCTION__);
	return i2c_add_driver(&es8388_i2c_driver);
}

static void __exit es8388_exit(void)
{
	pr_info("+%s()\n",__FUNCTION__);
	i2c_del_driver(&es8388_i2c_driver);
}

module_init(es8388_init);
module_exit(es8388_exit);

MODULE_DESCRIPTION("ASoC es8388 driver");
MODULE_AUTHOR("Mark Brown <will@everset-semi.com>");
MODULE_LICENSE("GPL");
