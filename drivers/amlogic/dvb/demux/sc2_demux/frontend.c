/*
 * drivers/amlogic/dvb/demux/sc2_demux/frontend.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/pinctrl/consumer.h>
#include <linux/reset.h>
#include <linux/of_gpio.h>
#include "../aml_dvb.h"
#include "demod_gt.h"
#include "sc2_control.h"
#include "../dmx_log.h"

#define dprint_i(fmt, args...)  \
	dprintk(LOG_ERROR, debug_frontend, fmt, ## args)
#define dprint(fmt, args...)   \
	dprintk(LOG_ERROR, debug_frontend, "fend:" fmt, ## args)
#define pr_dbg(fmt, args...)   \
	dprintk(LOG_DBG, debug_frontend, "fend:" fmt, ## args)

MODULE_PARM_DESC(debug_frontend, "\n\t\t Enable debug frontend information");
static int debug_frontend;
module_param(debug_frontend, int, 0644);

enum demod_type {
	DEMOD_INVALID,
	DEMOD_INTERNAL,
	DEMOD_ATBM8881,
	DEMOD_SI2168,
	DEMOD_AVL6762,
	DEMOD_MAX_NUM
};

enum tuner_type {
	TUNER_INVALID,
	TUNER_SI2151,
	TUNER_MXL661,
	TUNER_SI2159,
	TUNER_R842,
	TUNER_R840,
	TUNER_ATBM2040,
	TUNER_MAX_NUM
};

static struct dvb_frontend *frontend[FE_DEV_COUNT] = {
	NULL, NULL
};

static enum demod_type s_demod_type[FE_DEV_COUNT] = {
	DEMOD_INVALID, DEMOD_INVALID
};

static enum tuner_type s_tuner_type[FE_DEV_COUNT] = {
	TUNER_INVALID, TUNER_INVALID
};

static int dvb_attach_tuner(struct dvb_frontend *fe,
			    struct aml_tuner *tuner, enum tuner_type *type)
{
	struct tuner_config *cfg = &tuner->cfg;
	struct i2c_adapter *i2c_adap = tuner->i2c_adp;

	switch (cfg->id) {
	case AM_TUNER_R840:
		if (!dvb_attach(r840_attach, fe, i2c_adap, cfg)) {
			dprint("dvb attach r840_attach tuner error\n");
			return -1;
		}
		dprint("r840_attach  attach success\n");
		*type = TUNER_R840;
		break;
	case AM_TUNER_R842:
		if (!dvb_attach(r842_attach, fe, i2c_adap, cfg)) {
			dprint("dvb attach r842_attach tuner error\n");
			return -1;
		}
		dprint("r842_attach  attach success\n");
		*type = TUNER_R842;
		break;
	case AM_TUNER_SI2151:
		if (!dvb_attach(si2151_attach, fe, i2c_adap, cfg)) {
			dprint("dvb attach tuner error\n");
			return -1;
		}
		dprint("si2151 attach success\n");
		*type = TUNER_SI2151;
		break;
	case AM_TUNER_SI2159:
		if (!dvb_attach(si2159_attach, fe, i2c_adap, cfg)) {
			dprint("dvb attach si2159_attach tuner error\n");
			return -1;
		}
		dprint("si2159_attach  attach success\n");
		*type = TUNER_SI2159;
		break;
	case AM_TUNER_MXL661:
		if (!dvb_attach(mxl661_attach, fe, i2c_adap, cfg)) {
			dprint("dvb attach mxl661_attach tuner error\n");
			return -1;
		}
		dprint("mxl661_attach  attach success\n");
		*type = TUNER_MXL661;
		break;
	case AM_TUNER_ATBM2040:
		if (!dvb_attach(atbm2040_attach, fe, i2c_adap, cfg)) {
			dprint("dvb attach atbm2040_attach tuner error\n");
			return -1;
		}
		dprint("atbm2040_attach  attach success\n");
		*type = TUNER_ATBM2040;
		break;
	default:
		dprint("can't support tuner type: %d\n", cfg->id);
		break;
	}

	return 0;
}

ssize_t tuner_setting_show(struct class *class,
			   struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = aml_get_dvb_device();

	if (dvb->tuner_cur >= 0)
		dprint("dvb current attatch tuner %d, id: %d\n",
		       dvb->tuner_cur, dvb->tuners[dvb->tuner_cur].cfg.id);
	else
		dprint("dvb has no attatch tuner.\n");

	return 0;
}

ssize_t tuner_setting_store(struct class *class,
			    struct class_attribute *attr,
			    const char *buf, size_t count)
{
	int n = 0, i = 0, val = 0;
	unsigned long tmp = 0;
	char *buf_orig = NULL, *ps = NULL, *token = NULL;
	char *parm[4] = { NULL };
	struct aml_dvb *dvb = aml_get_dvb_device();
	int tuner_id = 0;
	struct aml_tuner *tuner = NULL;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;

	while (1) {
		token = strsep(&ps, "\n ");
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (parm[0] && kstrtoul(parm[0], 10, &tmp) == 0) {
		val = tmp;

		for (i = 0; i < dvb->tuner_num; ++i) {
			if (dvb->tuners[i].cfg.id == val) {
				tuner_id = dvb->tuners[i].cfg.id;
				break;
			}
		}

		if (tuner_id == 0 || dvb->tuner_cur == i) {
			dprint("%s: set nonsupport or the same tuner %d.\n",
			       __func__, val);
			goto EXIT;
		}

		dvb->tuner_cur = i;

		for (i = 0; i < FE_DEV_COUNT; i++) {
			tuner = &dvb->tuners[dvb->tuner_cur];

			if (!frontend[i])
				continue;

			if (dvb_attach_tuner(frontend[i],
					     tuner, &s_tuner_type[i]) < 0) {
				dprint("attach tuner %d failed\n",
				       dvb->tuner_cur);
				goto EXIT;
			}
		}

		dprint("%s: attach tuner %d done.\n",
		       __func__, dvb->tuners[dvb->tuner_cur].cfg.id);
	}

EXIT:
	kfree(buf_orig);
	return count;
}

ssize_t ts_setting_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	int r, total = 0;
	int i;
	struct aml_dvb *dvb = aml_get_dvb_device();
	int ctrl;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		struct aml_ts_input *ts = &dvb->ts[i];

		ctrl = ts->control;

		r = sprintf(buf, "ts%d %s control: 0x%x ", i,
			    ts->mode == AM_TS_DISABLE ? "disable" :
			    (ts->mode == AM_TS_SERIAL_3WIRE ?
			     "serial-3wire" :
			     (ts->mode == AM_TS_SERIAL_4WIRE ?
			      "serial-4wire" : "parallel")), ctrl);
		buf += r;
		total += r;
		r = sprintf(buf, "dmx_id:%d header_len:%d ",
			    ts->dmx_id, ts->header_len);
		buf += r;
		total += r;

		r = sprintf(buf, "header:0x%0x sid_offset:%d\n",
			    ts->header, ts->sid_offset);
		buf += r;
		total += r;
	}

	return total;
}

ssize_t ts_setting_store(struct class *class,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	int id, ctrl, r, mode;
	char mname[32];
	char pname[32];
	unsigned long flags;
	struct aml_ts_input *ts;
	struct aml_dvb *dvb = aml_get_dvb_device();

	r = sscanf(buf, "%d %s %x", &id, mname, &ctrl);
	if (r != 4)
		return -EINVAL;

	if (id < 0 || id >= FE_DEV_COUNT)
		return -EINVAL;

	if ((mname[0] == 's') || (mname[0] == 'S')) {
		if (mname[1] == '3') {
			sprintf(pname, "s3_ts%d", id);
			mode = AM_TS_SERIAL_3WIRE;
		} else {
			sprintf(pname, "s4_ts%d", id);
			mode = AM_TS_SERIAL_4WIRE;
		}
	} else if ((mname[0] == 'p') || (mname[0] == 'P')) {
		sprintf(pname, "p_ts%d", id);
		mode = AM_TS_PARALLEL;
	} else {
		mode = AM_TS_DISABLE;
	}
	spin_lock_irqsave(&dvb->slock, flags);

	ts = &dvb->ts[id];

	if ((mode != AM_TS_SERIAL_3WIRE) || (mode != AM_TS_SERIAL_4WIRE)) {
		if (ts->pinctrl) {
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		}

		ts->pinctrl = devm_pinctrl_get_select(&dvb->pdev->dev, pname);
		ts->mode = mode;
		ts->control = ctrl;
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	return count;
}

static void set_dvb_ts(struct platform_device *pdev,
		       struct aml_dvb *advb, int i, const char *str)
{
	char buf[32];

	if (!strcmp(str, "serial-3wire")) {
		dprint("%s: serial\n", buf);

		snprintf(buf, sizeof(buf), "s_ts%d", i);
		advb->ts[i].mode = AM_TS_SERIAL_3WIRE;
		advb->ts[i].pinctrl = devm_pinctrl_get_select(&pdev->dev, buf);

		demod_config_in(i, DEMOD_3WIRE);
	} else if (!strcmp(str, "serial-4wire")) {
		dprint("%s: serial\n", buf);

		snprintf(buf, sizeof(buf), "s_ts%d", i);
		advb->ts[i].mode = AM_TS_SERIAL_4WIRE;
		advb->ts[i].pinctrl = devm_pinctrl_get_select(&pdev->dev, buf);
		demod_config_in(i, DEMOD_4WIRE);
	} else if (!strcmp(str, "parallel")) {
		dprint("%s: parallel\n", buf);
		if (i != 1)
			dprint("error %s:parallel should be ts1\n", buf);
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "p_ts%d", i);
		advb->ts[i].mode = AM_TS_PARALLEL;
		advb->ts[i].pinctrl = devm_pinctrl_get_select(&pdev->dev, buf);
		demod_config_in(i, DEMOD_PARALLEL);
	} else {
		advb->ts[i].mode = AM_TS_DISABLE;
		advb->ts[i].pinctrl = NULL;
	}
}

static void ts_process(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	char buf[32];
	const char *str;
	u32 value;
	struct aml_dvb *advb = aml_get_dvb_device();

	for (i = 0; i < FE_DEV_COUNT; i++) {
		advb->ts[i].mode = AM_TS_DISABLE;
		advb->ts[i].dmx_id = -1;
		advb->ts[i].pinctrl = NULL;
		advb->ts[i].header_len = 0;
		advb->ts[i].header = 0x4b;

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_dmx", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret)
			advb->ts[i].dmx_id = value;

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_header_len", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret)
			advb->ts[i].header_len = value;

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_header", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret)
			advb->ts[i].header = value;

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_sid_offset", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret)
			advb->ts[i].sid_offset = value;

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d", i);
		ret = of_property_read_string(pdev->dev.of_node, buf, &str);
		if (!ret)
			set_dvb_ts(pdev, advb, i, str);

		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "ts%d_control", i);
		ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
		if (!ret) {
			dprint("%s: 0x%x\n", buf, value);
			advb->ts[i].control = value;
		} else {
			dprint("read error:%s: 0x%x\n", buf, value);
		}
	}
}

void set_tuner_id(struct aml_dvb *advb, const char *str, int j)
{
	if (!strncmp(str, "mxl661_tuner", 12)) {
		advb->tuners[j].cfg.id = AM_TUNER_MXL661;
	} else if (!strncmp(str, "si2151_tuner", 12)) {
		advb->tuners[j].cfg.id = AM_TUNER_SI2151;
	} else if (!strncmp(str, "si2159_tuner", 12)) {
		advb->tuners[j].cfg.id = AM_TUNER_SI2159;
	} else if (!strncmp(str, "r840_tuner", 10)) {
		advb->tuners[j].cfg.id = AM_TUNER_R840;
	} else if (!strncmp(str, "r842_tuner", 10)) {
		advb->tuners[j].cfg.id = AM_TUNER_R842;
	} else if (!strncmp(str, "atbm2040_tuner", 14)) {
		advb->tuners[j].cfg.id = AM_TUNER_ATBM2040;
	} else {
		pr_err("nonsupport tuner: %s.\n", str);
		advb->tuners[j].cfg.id = AM_TUNER_NONE;
	}
}

int set_tuner_para(struct aml_dvb *advb, struct device_node *node_tuner,
		   struct device_node *node_i2c, int j)
{
	char buf[32];
	u32 i2c_addr = 0xFFFFFFFF;
	u32 value = 0;
	int ret = 0;

	snprintf(buf, sizeof(buf), "tuner_i2c_adap_%d", j);
	node_i2c = of_parse_phandle(node_tuner, buf, 0);
	if (!node_i2c) {
		dprint("tuner_i2c_adap_id error\n");
	} else {
		advb->tuners[j].i2c_adp = of_find_i2c_adapter_by_node(node_i2c);
		of_node_put(node_i2c);
		if (!advb->tuners[j].i2c_adp) {
			dprint("i2c_get_adapter error\n");
			of_node_put(node_tuner);
			return -1;
		}
	}

	snprintf(buf, sizeof(buf), "tuner_i2c_addr_%d", j);
	ret = of_property_read_u32(node_tuner, buf, &i2c_addr);
	if (ret)
		dprint("i2c_addr error\n");
	else
		advb->tuners[j].cfg.i2c_addr = i2c_addr;

	snprintf(buf, sizeof(buf), "tuner_xtal_%d", j);
	ret = of_property_read_u32(node_tuner, buf, &value);
	if (ret)
		pr_err("tuner_xtal error.\n");
	else
		advb->tuners[j].cfg.xtal = value;

	snprintf(buf, sizeof(buf), "tuner_xtal_mode_%d", j);
	ret = of_property_read_u32(node_tuner, buf, &value);
	if (ret)
		pr_err("tuner_xtal_mode error.\n");
	else
		advb->tuners[j].cfg.xtal_mode = value;

	snprintf(buf, sizeof(buf), "tuner_xtal_cap_%d", j);
	ret = of_property_read_u32(node_tuner, buf, &value);
	if (ret)
		pr_err("tuner_xtal_cap error.\n");
	else
		advb->tuners[j].cfg.xtal_cap = value;
	return 0;
}

int frontend_probe(struct platform_device *pdev)
{
	struct amlfe_exp_config config;
	char buf[32];
	const char *str = NULL;
	struct device_node *node_tuner = NULL;
	struct device_node *node_i2c = NULL;
	struct tuner_config *cfg = NULL;
	u32 value = 0;
	int i = 0;
	int ret = 0;
	int j = 0;
	struct aml_dvb *advb = aml_get_dvb_device();

#ifdef CONFIG_OF
	if (pdev->dev.of_node)
		ts_process(pdev);
#endif

	for (i = 0; i < FE_DEV_COUNT; i++) {
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "fe%d_mode", i);
		ret = of_property_read_string(pdev->dev.of_node, buf, &str);
		if (ret)
			continue;

		if (!strcmp(str, "internal")) {
			config.set_mode = 0;
			frontend[i] = dvb_attach(aml_dtvdm_attach, &config);
			if (!frontend[i]) {
				dprint("dvb attach demod error\n");
				goto error_fe;
			} else {
				dprint("dtvdemod attatch success\n");
				s_demod_type[i] = DEMOD_INTERNAL;
			}

			memset(&cfg, 0, sizeof(struct tuner_config));
			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "fe%d_tuner", i);
			node_tuner = of_parse_phandle(pdev->dev.of_node,
						      buf, 0);
			if (!node_tuner) {
				pr_err("can't find tuner.\n");
				goto error_fe;
			}
			ret = of_property_read_u32(node_tuner,
						   "tuner_num", &value);
			if (ret) {
				pr_err("can't find tuner_num.\n");
				goto error_fe;
			} else {
				advb->tuner_num = value;
			}

			advb->tuners = kcalloc(advb->tuner_num,
					       sizeof(struct aml_tuner),
					       GFP_KERNEL);
			if (!advb->tuners)
				goto error_fe;

			ret = of_property_read_u32(node_tuner,
						   "tuner_cur", &value);
			if (ret) {
				pr_err
				    ("can't find tuner_cur, use default 0.\n");
				advb->tuner_cur = -1;
			} else {
				advb->tuner_cur = value;
			}

			for (j = 0; j < advb->tuner_num; ++j) {
				snprintf(buf, sizeof(buf), "tuner_name_%d", j);
				ret = of_property_read_string(node_tuner,
							      buf, &str);
				if (ret) {
					//dprint("tuner%d type error\n",i);
					ret = 0;
					continue;
				} else {
					set_tuner_id(advb, str, j);
				}
				if (set_tuner_para(advb,
						   node_tuner, node_i2c,
						   j) != 0)
					goto error_fe;
			}

			of_node_put(node_tuner);

			/* define general-purpose callback pointer */
			frontend[i]->callback = NULL;

			if (advb->tuner_cur >= 0) {
				if (dvb_attach_tuner(frontend[i],
						     &advb->tuners[advb->
								   tuner_cur],
						     &s_tuner_type[i]) < 0) {
					dprint("attach tuner failed\n");
					goto error_fe;
				}
			}

			ret = dvb_register_frontend(&advb->dvb_adapter,
						    frontend[i]);
			if (ret) {
				dprint("register dvb frontend failed\n");
				goto error_fe;
			}
		} else if (!strcmp(str, "external")) {
			const char *name = NULL;
			struct amlfe_demod_config config;

			config.dev_id = i;
			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "fe%d_demod", i);
			ret = of_property_read_string(pdev->dev.of_node,
						      buf, &name);
			if (ret) {
				ret = 0;
				continue;
			}

			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "fe%d_i2c_adap_id", i);
			node_i2c = of_parse_phandle(pdev->dev.of_node, buf, 0);
			if (!node_i2c) {
				dprint("demod%d_i2c_adap_id error\n", i);
			} else {
				config.i2c_adap =
				    of_find_i2c_adapter_by_node(node_i2c);
				of_node_put(node_i2c);
				if (!config.i2c_adap) {
					dprint("i2c_get_adapter error\n");
					goto error_fe;
				}
			}

			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "fe%d_demod_i2c_addr", i);
			ret = of_property_read_u32(pdev->dev.of_node,
						   buf, &config.i2c_addr);
			if (ret) {
				dprint("i2c_addr error\n");
				goto error_fe;
			}

			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "fe%d_ts", i);
			ret = of_property_read_u32(pdev->dev.of_node,
						   buf, &config.ts);
			if (ret) {
				dprint("ts error\n");
				goto error_fe;
			}

			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "fe%d_reset_gpio", i);
			ret = of_property_read_string(pdev->dev.of_node,
						      buf, &str);
			if (!ret) {
				config.reset_gpio =
				    of_get_named_gpio_flags(pdev->dev.of_node,
							    buf, 0, NULL);
				dprint("%s: %d\n", buf, config.reset_gpio);
			} else {
				config.reset_gpio = -1;
				dprint("cannot find resource \"%s\"\n", buf);
				goto error_fe;
			}

			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "fe%d_reset_value", i);
			ret = of_property_read_u32(pdev->dev.of_node,
						   buf, &config.reset_value);
			if (ret) {
				dprint("reset_value error\n");
				goto error_fe;
			}

			if (!strcmp(name, "Atbm8881")) {
				frontend[i] = dvb_attach(atbm8881_attach,
							 &config);
				if (!frontend[i]) {
					dprint("dvb attach demod error\n");
					goto error_fe;
				} else {
					dprint("dtvdemod attatch success\n");
					s_demod_type[i] = DEMOD_ATBM8881;
				}
			}
			if (!strcmp(name, "Si2168")) {
				frontend[i] = dvb_attach(si2168_attach,
							 &config);
				if (!frontend[i]) {
					dprint("dvb attach demod error\n");
					goto error_fe;
				} else {
					dprint("dtvdemod attatch success\n");
					s_demod_type[i] = DEMOD_SI2168;
				}
			}
			if (!strcmp(name, "Avl6762")) {
				frontend[i] = dvb_attach(avl6762_attach,
							 &config);
				if (!frontend[i]) {
					dprint("dvb attach demod error\n");
					goto error_fe;
				} else {
					dprint("dtvdemod attatch success\n");
					s_demod_type[i] = DEMOD_AVL6762;
				}
			}
			if (frontend[i]) {
				ret = dvb_register_frontend(&advb->dvb_adapter,
							    frontend[i]);
				if (ret) {
					dprint
					    ("register dvb frontend failed\n");
					goto error_fe;
				}
			}
		}
	}
	kfree(advb->tuners);
	return 0;
error_fe:
	for (i = 0; i < FE_DEV_COUNT; i++) {
		if (s_demod_type[i] == DEMOD_INTERNAL) {
			dvb_detach(aml_dtvdm_attach);
			frontend[i] = NULL;
			s_demod_type[i] = DEMOD_INVALID;
		} else if (s_demod_type[i] == DEMOD_ATBM8881) {
			dvb_detach(atbm8881_attach);
			frontend[i] = NULL;
			s_demod_type[i] = DEMOD_INVALID;
		} else if (s_demod_type[i] == DEMOD_SI2168) {
			dvb_detach(si2168_attach);
			frontend[i] = NULL;
			s_demod_type[i] = DEMOD_INVALID;
		} else if (s_demod_type[i] == DEMOD_AVL6762) {
			dvb_detach(avl6762_attach);
			frontend[i] = NULL;
			s_demod_type[i] = DEMOD_INVALID;
		}
		if (s_tuner_type[i] == TUNER_SI2151) {
			dvb_detach(si2151_attach);
			s_tuner_type[i] = TUNER_INVALID;
		} else if (s_tuner_type[i] == TUNER_MXL661) {
			dvb_detach(mxl661_attach);
			s_tuner_type[i] = TUNER_INVALID;
		} else if (s_tuner_type[i] == TUNER_SI2159) {
			dvb_detach(si2159_attach);
			s_tuner_type[i] = TUNER_INVALID;
		} else if (s_tuner_type[i] == TUNER_R842) {
			dvb_detach(r842_attach);
			s_tuner_type[i] = TUNER_INVALID;
		} else if (s_tuner_type[i] == TUNER_R840) {
			dvb_detach(r840_attach);
			s_tuner_type[i] = TUNER_INVALID;
		}
	}

	kfree(advb->tuners);

	return 0;
}

void frontend_config_ts_sid(void)
{
	int i;
	int sid = 0;
	struct aml_dvb *advb = aml_get_dvb_device();

	for (i = 0; i < FE_DEV_COUNT; i++) {
		if (advb->ts[i].header_len == 0) {
			if (advb->ts[i].dmx_id != -1) {
				sid = advb->dmx[advb->ts[i].dmx_id].demod_sid;
				demod_config_single(i, sid);
			}
		} else
			demod_config_multi(i, advb->ts[i].header_len / 4,
					   advb->ts[i].header,
					   advb->ts[i].sid_offset);
	}
}

int frontend_remove(void)
{
	int i;

	for (i = 0; i < FE_DEV_COUNT; i++) {
		if (s_demod_type[i] == DEMOD_INTERNAL) {
			dvb_detach(aml_dtvdm_attach);
		} else if (s_demod_type[i] == DEMOD_ATBM8881) {
			dvb_detach(atbm8881_attach);
		} else if (s_demod_type[i] == DEMOD_SI2168) {
			dvb_detach(si2168_attach);
			frontend[i] = NULL;
			s_demod_type[i] = DEMOD_INVALID;
		} else if (s_demod_type[i] == DEMOD_AVL6762) {
			dvb_detach(avl6762_attach);
			frontend[i] = NULL;
			s_demod_type[i] = DEMOD_INVALID;
		}
		if (s_tuner_type[i] == TUNER_SI2151)
			dvb_detach(si2151_attach);
		else if (s_tuner_type[i] == TUNER_MXL661)
			dvb_detach(mxl661_attach);
		else if (s_tuner_type[i] == TUNER_SI2159)
			dvb_detach(si2159_attach);
		else if (s_tuner_type[i] == TUNER_R842)
			dvb_detach(r842_attach);
		else if (s_tuner_type[i] == TUNER_R840)
			dvb_detach(r840_attach);

		if (frontend[i] &&
		    ((s_tuner_type[i] == TUNER_SI2151) ||
		     (s_tuner_type[i] == TUNER_MXL661) ||
		     (s_tuner_type[i] == TUNER_SI2159) ||
		     (s_tuner_type[i] == TUNER_R842) ||
		     (s_tuner_type[i] == TUNER_R840))) {
			dvb_unregister_frontend(frontend[i]);
			dvb_frontend_detach(frontend[i]);
		}
		frontend[i] = NULL;
		s_demod_type[i] = DEMOD_INVALID;
		s_tuner_type[i] = TUNER_INVALID;
	}
	return 0;
}
