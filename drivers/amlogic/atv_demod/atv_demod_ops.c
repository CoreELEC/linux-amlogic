/*
 * drivers/amlogic/atv_demod/atv_demod_ops.c
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

#include <linux/types.h>
#include <linux/i2c.h>
#include <uapi/linux/dvb/frontend.h>

#include <linux/amlogic/aml_atvdemod.h>

#include "drivers/media/tuners/tuner-i2c.h"
#include "drivers/media/dvb-core/dvb_frontend.h"
#include "drivers/amlogic/media/vin/tvin/tvafe/tvafe_general.h"

#include "atvdemod_func.h"
#include "atvauddemod_func.h"
#include "atv_demod_debug.h"
#include "atv_demod_driver.h"
#include "atv_demod_ops.h"
#include "atv_demod_v4l2.h"
#include "atv_demod_afc.h"
#include "atv_demod_monitor.h"

#define DEVICE_NAME "aml_atvdemod"


static DEFINE_MUTEX(atv_demod_list_mutex);
static LIST_HEAD(hybrid_tuner_instance_list);

unsigned int reg_23cf = 0x88188832; /*IIR filter*/
unsigned int btsc_sap_mode = 1;	/*0: off 1:monitor 2:auto */


/*
 * add interface for audio driver to get atv audio state.
 * state:
 * 0 - no data.
 * 1 - has data.
 */
void aml_fe_get_atvaudio_state(int *state)
{
#if 0 /* delay notification stable */
	static unsigned int count;
	static bool mute = true;
#endif
	int av_status = 0;
	int power = 0;
	int vpll_lock = 0;
	int line_lock = 0;
	struct atv_demod_priv *priv = amlatvdemod_devp != NULL
			? amlatvdemod_devp->v4l2_fe.fe.analog_demod_priv : NULL;

	if (priv == NULL) {
		pr_err("priv == NULL\n");
		*state = 0;
		return;
	}

	av_status = tvin_get_av_status();
	/* scan mode need mute */
	if (priv->state == ATVDEMOD_STATE_WORK
			&& !priv->scanning
			&& !priv->standby) {
		retrieve_vpll_carrier_lock(&vpll_lock);
		retrieve_vpll_carrier_line_lock(&line_lock);
		if ((vpll_lock == 0) && (line_lock == 0)) {
			/* retrieve_vpll_carrier_audio_power(&power); */
			*state = 1;
		} else {
			*state = 0;
			pr_audio("vpll_lock: 0x%x, line_lock: 0x%x\n",
					vpll_lock, line_lock);
		}
	} else {
		*state = 0;
		pr_audio("%s, ATV in state[%d], scanning[%d], standby[%d].\n",
				__func__, priv->state,
				priv->scanning, priv->standby);
	}

	/* If the atv signal is locked, it means there is audio data,
	 * so no need to check the power value.
	 */
#if 0
	if (power >= 150)
		*state = 1;
	else
		*state = 0;
#endif
#if 0 /* delay notification stable */
	if (*state) {
		if (mute) {
			count++;
			if (count > 100) {
				count = 0;
				mute = false;
			} else
				*state = 0;
		} else
			count = 0;
	} else {
		count = 0;
		mute = true;
	}
#endif
	pr_audio("aml_fe_get_atvaudio_state: %d, power = %d\n",
			*state, power);
}

int aml_atvdemod_get_btsc_sap_mode(void)
{
	return btsc_sap_mode;
}

int atv_demod_enter_mode(struct dvb_frontend *fe)
{
	int err_code = 0;

	if (amlatvdemod_devp->pin_name != NULL) {
		amlatvdemod_devp->agc_pin =
			devm_pinctrl_get_select(amlatvdemod_devp->dev,
				amlatvdemod_devp->pin_name);
		if (IS_ERR(amlatvdemod_devp->agc_pin)) {
			amlatvdemod_devp->agc_pin = NULL;
			pr_err("%s: get agc pins fail\n", __func__);
		}
	}

	err_code = adc_set_pll_cntl(1, ADC_EN_ATV_DEMOD, NULL);
	vdac_enable(1, 1);
	usleep_range(2000, 2100);
	atvdemod_clk_init();
	/* err_code = atvdemod_init(); */

	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
		aud_demod_clk_gate(1);
		/* atvauddemod_init(); */
	}

	if (err_code) {
		pr_dbg("%s: init atvdemod error %d.\n", __func__, err_code);
		return -1;
	}

	/* aml_afc_timer_enable(fe); */
	/* aml_demod_timer_enable(fe); */

	amlatvdemod_devp->std = 0;
	amlatvdemod_devp->audmode = 0;
	amlatvdemod_devp->soundsys = 0xFF;

	pr_info("%s: OK.\n", __func__);

	return 0;
}

int atv_demod_leave_mode(struct dvb_frontend *fe)
{
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	priv->state = ATVDEMOD_STATE_IDEL;

	if (priv->afc.disable)
		priv->afc.disable(&priv->afc);

	if (priv->monitor.disable)
		priv->monitor.disable(&priv->monitor);

	atvdemod_uninit();
	if (!IS_ERR_OR_NULL(amlatvdemod_devp->agc_pin)) {
		devm_pinctrl_put(amlatvdemod_devp->agc_pin);
		amlatvdemod_devp->agc_pin = NULL;
	}

	vdac_enable(0, 1);
	adc_set_pll_cntl(0, ADC_EN_ATV_DEMOD, NULL);
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu())
		aud_demod_clk_gate(0);

	amlatvdemod_devp->std = 0;
	amlatvdemod_devp->audmode = 0;
	amlatvdemod_devp->soundsys = 0xFF;

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static void atv_demod_set_params(struct dvb_frontend *fe,
		struct analog_parameters *params)
{
	int ret = -1;
	u32 if_info[2] = { 0 };
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	struct aml_atvdemod_parameters *p = &priv->atvdemod_param;
	bool reconfig = false;

	priv->standby = true;

	/* afc tune disable,must cancel wq before set tuner freq*/
	if (priv->afc.disable)
		priv->afc.disable(&priv->afc);

	if (priv->monitor.disable)
		priv->monitor.disable(&priv->monitor);

	if (fe->ops.tuner_ops.set_analog_params)
		ret = fe->ops.tuner_ops.set_analog_params(fe, params);

	if (fe->ops.tuner_ops.get_if_frequency)
		ret = fe->ops.tuner_ops.get_if_frequency(fe, if_info);

	p->param.frequency = params->frequency;
	p->param.mode = params->mode;
	p->param.audmode = params->audmode;
	p->param.std = params->std;

	p->if_inv = if_info[0];
	p->if_freq = if_info[1];

#if 0 /* unused */
	last_frq = p->param.frequency;
	last_std = p->param.std;
#endif

	if ((p->tuner_id == AM_TUNER_R840) ||
		(p->tuner_id == AM_TUNER_R842) ||
		(p->tuner_id == AM_TUNER_SI2151) ||
		(p->tuner_id == AM_TUNER_SI2159) ||
		(p->tuner_id == AM_TUNER_MXL661))
		reconfig = false;

	/* In general, demod does not need to be reconfigured
	 * if parameters such as STD remain unchanged,
	 * but when the input signal frequency offset -0.25MHz,
	 * demod will be unlocked. That's very strange.
	 */
	if (reconfig || !priv->scanning ||
		amlatvdemod_devp->std != p->param.std ||
		amlatvdemod_devp->audmode != p->param.audmode ||
		amlatvdemod_devp->if_freq != p->if_freq ||
		amlatvdemod_devp->if_inv != p->if_inv) {

		amlatvdemod_devp->std = p->param.std;
		amlatvdemod_devp->audmode = p->param.audmode;
		amlatvdemod_devp->if_freq = p->if_freq;
		amlatvdemod_devp->if_inv = p->if_inv;

		atv_dmd_set_std();
		atvdemod_init(!priv->scanning);
	} else
		atv_dmd_soft_reset();

	if (!priv->scanning)
		atvauddemod_init();

	/* afc tune enable */
	/* analog_search_flag == 0 or afc_range != 0 means searching */
	if ((fe->ops.info.type == FE_ANALOG)
			&& (priv->scanning == false)
			&& (p->param.mode == 0)) {
		if (priv->afc.enable)
			priv->afc.enable(&priv->afc);

		if (priv->monitor.enable)
			priv->monitor.enable(&priv->monitor);

		/* for searching mute audio */
		priv->standby = false;
	}

	priv->standby = false;
}

static int atv_demod_has_signal(struct dvb_frontend *fe, u16 *signal)
{
	int vpll_lock = 0;
	int line_lock = 0;

	retrieve_vpll_carrier_lock(&vpll_lock);

	/* add line lock status for atv scan */
	retrieve_vpll_carrier_line_lock(&line_lock);

	if (vpll_lock == 0 && line_lock == 0) {
		*signal = V4L2_HAS_LOCK;
		pr_info("%s locked [vpll_lock: 0x%x, line_lock:0x%x]\n",
				__func__, vpll_lock, line_lock);
	} else {
		*signal = V4L2_TIMEDOUT;
		pr_dbg("%s unlocked [vpll_lock: 0x%x, line_lock:0x%x]\n",
				__func__, vpll_lock, line_lock);
	}

	return 0;
}

static void atv_demod_standby(struct dvb_frontend *fe)
{
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	if (priv->state != ATVDEMOD_STATE_IDEL) {
		atv_demod_leave_mode(fe);
		priv->state = ATVDEMOD_STATE_SLEEP;
		priv->standby = true;
	}

	pr_info("%s: OK.\n", __func__);
}

static void atv_demod_tuner_status(struct dvb_frontend *fe)
{
	pr_info("%s.\n", __func__);
}

static int atv_demod_get_afc(struct dvb_frontend *fe, s32 *afc)
{
	*afc = retrieve_vpll_carrier_afc();

	return 0;
}

static void atv_demod_release(struct dvb_frontend *fe)
{
	int instance = 0;
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	mutex_lock(&atv_demod_list_mutex);

	atv_demod_leave_mode(fe);

	if (priv)
		instance = hybrid_tuner_release_state(priv);

	if (instance == 0)
		fe->analog_demod_priv = NULL;

	mutex_unlock(&atv_demod_list_mutex);

	pr_info("%s: OK.\n", __func__);
}

static int atv_demod_set_config(struct dvb_frontend *fe, void *priv_cfg)
{
	int *state = (int *) priv_cfg;
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	if (!state) {
		pr_err("%s: state == NULL.\n", __func__);
		return -1;
	}

	mutex_lock(&atv_demod_list_mutex);

	switch (*state) {
	case AML_ATVDEMOD_INIT:
		if (priv->state != ATVDEMOD_STATE_WORK) {
			priv->standby = true;
			if (fe->ops.tuner_ops.set_config)
				fe->ops.tuner_ops.set_config(fe, NULL);
			if (!atv_demod_enter_mode(fe))
				priv->state = ATVDEMOD_STATE_WORK;
		}
		break;

	case AML_ATVDEMOD_UNINIT:
		if (priv->state != ATVDEMOD_STATE_IDEL) {
			atv_demod_leave_mode(fe);
			if (fe->ops.tuner_ops.release)
				fe->ops.tuner_ops.release(fe);
		}
		break;

	case AML_ATVDEMOD_RESUME:
		if (priv->state == ATVDEMOD_STATE_SLEEP) {
			if (!atv_demod_enter_mode(fe)) {
				priv->state = ATVDEMOD_STATE_WORK;
				priv->standby = false;
			}
		}
		break;

	case AML_ATVDEMOD_SCAN_MODE:
		priv->scanning = true;
		if (priv->afc.disable)
			priv->afc.disable(&priv->afc);

		if (priv->monitor.disable)
			priv->monitor.disable(&priv->monitor);
		break;

	case AML_ATVDEMOD_UNSCAN_MODE:
		priv->scanning = false;
		if (priv->afc.enable)
			priv->afc.enable(&priv->afc);

		if (priv->monitor.enable)
			priv->monitor.enable(&priv->monitor);
		break;
	}

	mutex_unlock(&atv_demod_list_mutex);

	return 0;
}

static struct analog_demod_ops atvdemod_ops = {
	.info = {
		.name = DEVICE_NAME,
	},
	.set_params     = atv_demod_set_params,
	.has_signal     = atv_demod_has_signal,
	.standby        = atv_demod_standby,
	.tuner_status   = atv_demod_tuner_status,
	.get_afc        = atv_demod_get_afc,
	.release        = atv_demod_release,
	.set_config     = atv_demod_set_config,
	.i2c_gate_ctrl  = NULL,
};


unsigned int tuner_status_cnt = 4; /* 4-->16 test on sky mxl661 */

bool slow_mode;

typedef int (*hook_func_t) (void);
hook_func_t aml_fe_hook_atv_status;
hook_func_t aml_fe_hook_hv_lock;
hook_func_t aml_fe_hook_get_fmt;

void aml_fe_hook_cvd(hook_func_t atv_mode, hook_func_t cvd_hv_lock,
	hook_func_t get_fmt)
{
	aml_fe_hook_atv_status = atv_mode;
	aml_fe_hook_hv_lock = cvd_hv_lock;
	aml_fe_hook_get_fmt = get_fmt;

	pr_info("%s: OK.\n", __func__);
}
EXPORT_SYMBOL(aml_fe_hook_cvd);

static v4l2_std_id atvdemod_fmt_2_v4l2_std(int fmt)
{
	v4l2_std_id std = 0;

	switch (fmt) {
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK:
		std = V4L2_STD_PAL_DK;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I:
		std = V4L2_STD_PAL_I;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG:
		std = V4L2_STD_PAL_BG;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M:
		std = V4L2_STD_PAL_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_I:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_BG:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_M:
		std = V4L2_STD_NTSC_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L:
		std = V4L2_STD_SECAM_L;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK2:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK3:
		std = V4L2_STD_SECAM_DK;
		break;
	default:
		pr_err("%s: Unsupport fmt: 0x%0x.\n", __func__, fmt);
	}

	return std;
}

static v4l2_std_id atvdemod_fe_tvin_fmt_to_v4l2_std(int fmt)
{
	v4l2_std_id std = 0;

	switch (fmt) {
	case TVIN_SIG_FMT_CVBS_NTSC_M:
		std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
		break;
	case TVIN_SIG_FMT_CVBS_NTSC_443:
		std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_443;
		break;
	case TVIN_SIG_FMT_CVBS_NTSC_50:
		std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_I:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_I;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_M:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_M;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_60:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_60;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_CN:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_Nc;
		break;
	case TVIN_SIG_FMT_CVBS_SECAM:
		std = V4L2_COLOR_STD_SECAM | V4L2_STD_SECAM_L;
		break;
	default:
		pr_err("%s: Unsupport fmt: 0x%x\n", __func__, fmt);
		break;
	}

	return std;
}

static void atvdemod_fe_try_analog_format(struct v4l2_frontend *v4l2_fe,
		int auto_search_std, v4l2_std_id *video_fmt,
		unsigned int *audio_fmt, unsigned int *soundsys)
{
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct analog_parameters params;
	int i = 0;
	int try_vfmt_cnt = 300;
	int varify_cnt = 0;
	int cvbs_std = 0;
	v4l2_std_id std_bk = 0;
	unsigned int broad_std = 0;
	unsigned int audio = 0;

	if (auto_search_std & 0x01) {
		for (i = 0; i < try_vfmt_cnt; i++) {
			if (aml_fe_hook_get_fmt == NULL) {
				pr_err("%s: aml_fe_hook_get_fmt == NULL.\n",
						__func__);
				break;
			}
			cvbs_std = aml_fe_hook_get_fmt();
			if (cvbs_std) {
				varify_cnt++;
				pr_dbg("get cvbs_std varify_cnt:%d, cnt:%d, cvbs_std:0x%x\n",
						varify_cnt, i,
						(unsigned int) cvbs_std);
				if (((v4l2_fe->tuner_id == AM_TUNER_R840
					|| v4l2_fe->tuner_id == AM_TUNER_R842)
					&& varify_cnt > 0)
					|| varify_cnt > 3)
					break;
			}

			if (i == (try_vfmt_cnt / 3) ||
				(i == (try_vfmt_cnt / 3) * 2)) {
				/* Before enter search,
				 * need set the std,
				 * then, try others std.
				 */
				if (p->std & V4L2_COLOR_STD_PAL)
					p->std = V4L2_COLOR_STD_NTSC
					| V4L2_STD_NTSC_M;
#if 0 /*for secam */
				else if (p->std & V4L2_COLOR_STD_NTSC)
					p->std = V4L2_COLOR_STD_SECAM
					| V4L2_STD_SECAM;
#endif
				else if (p->std & V4L2_COLOR_STD_NTSC)
					p->std = V4L2_COLOR_STD_PAL
					| V4L2_STD_PAL_DK;

				p->frequency += 1;
				params.frequency = p->frequency;
				params.mode = p->afc_range;
				params.audmode = p->audmode;
				params.std = p->std;

				fe->ops.analog_ops.set_params(fe, &params);
			}
			usleep_range(30 * 1000, 30 * 1000 + 100);
		}

		pr_dbg("get cvbs_std cnt:%d, cvbs_std: 0x%x\n",
				i, (unsigned int) cvbs_std);

		if (cvbs_std == 0) {
			pr_err("%s: failed to get video fmt, assume PAL.\n",
					__func__);
			cvbs_std = TVIN_SIG_FMT_CVBS_PAL_I;
			p->std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_DK;
			p->frequency += 1;
			p->audmode = V4L2_STD_PAL_DK;

			params.frequency = p->frequency;
			params.mode = p->afc_range;
			params.audmode = p->audmode;
			params.std = p->std;

			fe->ops.analog_ops.set_params(fe, &params);

			usleep_range(20 * 1000, 20 * 1000 + 100);
		}

		std_bk = atvdemod_fe_tvin_fmt_to_v4l2_std(cvbs_std);
	} else {
		/* Only search std by user setting,
		 * so no need tvafe identify signal.
		 */
		std_bk = p->std;
	}

	*video_fmt = std_bk;

	if (!(auto_search_std & 0x02)) {
		*audio_fmt = p->audmode;
		return;
	}

	if (std_bk & V4L2_COLOR_STD_NTSC) {
#if 1 /* For TV Signal Generator(TG39) test, NTSC need support other audio.*/
		if (cvbs_std == TVIN_SIG_FMT_CVBS_NTSC_M) {
			broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M;
			audio = V4L2_STD_NTSC_M;
		} else {
			amlatvdemod_set_std(AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC);
			broad_std = aml_audiomode_autodet(v4l2_fe);
			audio = atvdemod_fmt_2_v4l2_std(broad_std);
		}
#if 0 /* I don't know what's going on here */
		if (audio == V4L2_STD_PAL_M)
			audio = V4L2_STD_NTSC_M;
		else
			std_bk = V4L2_COLOR_STD_PAL;
#endif
#else /* Now, force to NTSC_M, Ours demod only support M for NTSC.*/
		audio = V4L2_STD_NTSC_M;
#endif
	} else if (std_bk & V4L2_COLOR_STD_SECAM) {
#if 1 /* For support SECAM-DK/BG/I/L */
		amlatvdemod_set_std(AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L);
		broad_std = aml_audiomode_autodet(v4l2_fe);
		audio = atvdemod_fmt_2_v4l2_std(broad_std);
#else /* For force L */
		audio = V4L2_STD_SECAM_L;
#endif
	} else {
		/* V4L2_COLOR_STD_PAL */
		if (cvbs_std == TVIN_SIG_FMT_CVBS_PAL_M) {
			broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M;
			audio = V4L2_STD_PAL_M;
		} else {
			amlatvdemod_set_std(
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK);
			broad_std = aml_audiomode_autodet(v4l2_fe);
			audio = atvdemod_fmt_2_v4l2_std(broad_std);
		}
#if 0 /* Why do this to me? We need support PAL_M.*/
		if (audio == V4L2_STD_PAL_M) {
			audio = atvdemod_fmt_2_v4l2_std(broad_std_except_pal_m);
			pr_info("select audmode 0x%x\n", audio);
		}
#endif
	}

	*audio_fmt = audio;

	/* for audio standard detection */
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu() || is_meson_tl1_cpu()) {
		*soundsys = amlfmt_aud_standard(broad_std);
		*soundsys = (*soundsys << 16) | 0x00FFFF;
	} else
		*soundsys = 0xFFFFFF;

	pr_info("autodet audio broad_std %d, [%s][0x%x] soundsys[0x%x]\n",
			broad_std, v4l2_std_to_str(audio), audio, *soundsys);
}

static int atvdemod_fe_afc_closer(struct v4l2_frontend *v4l2_fe, int minafcfreq,
		int maxafcfreq, int isAutoSearch)
{
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct analog_parameters params;
	int afc = 100;
	__u32 set_freq;
	int count = 25;
	int lock_cnt = 0;
	static int freq_success;
	static int temp_freq, temp_afc;
	struct timespec time_now;
	static struct timespec success_time;
	unsigned int tuner_id = v4l2_fe->tuner_id;

	pr_dbg("[%s] freq_success: %d, freq: %d, minfreq: %d, maxfreq: %d\n",
		__func__, freq_success, p->frequency, minafcfreq, maxafcfreq);

	/* avoid more search the same program, except < 45.00Mhz */
	if (abs(p->frequency - freq_success) < 3000000
			&& p->frequency > 45000000) {
		ktime_get_ts(&time_now);
		pr_err("[%s] tv_sec now:%ld,tv_sec success:%ld\n",
				__func__, time_now.tv_sec, success_time.tv_sec);
		/* beyond 10s search same frequency is ok */
		if ((time_now.tv_sec - success_time.tv_sec) < 10)
			return -1;
	}

	/*do the auto afc make sure the afc < 50k or the range from api */
	if ((fe->ops.analog_ops.get_afc || fe->ops.tuner_ops.get_afc) &&
		fe->ops.tuner_ops.set_analog_params) {

		set_freq = p->frequency;
		while (abs(afc) > AFC_BEST_LOCK) {
			if (tuner_id == AM_TUNER_SI2151 ||
				tuner_id == AM_TUNER_SI2159 ||
				tuner_id == AM_TUNER_R840 ||
				tuner_id == AM_TUNER_R842)
				usleep_range(10 * 1000, 10 * 1000 + 100);
			else if (tuner_id == AM_TUNER_MXL661)
				usleep_range(30 * 1000, 30 * 1000 + 100);

			if (fe->ops.analog_ops.get_afc &&
			((tuner_id == AM_TUNER_R840) ||
			(tuner_id == AM_TUNER_R842) ||
			(tuner_id == AM_TUNER_SI2151) ||
			(tuner_id == AM_TUNER_SI2159) ||
			(tuner_id == AM_TUNER_MXL661)))
				fe->ops.analog_ops.get_afc(fe, &afc);
			else if (fe->ops.tuner_ops.get_afc)
				fe->ops.tuner_ops.get_afc(fe, &afc);

			pr_dbg("[%s] get afc %d khz, freq %u.\n",
					__func__, afc, p->frequency);

			if (afc == 0xffff) {
				/*last lock, but this unlock,so try get afc*/
				if (lock_cnt > 0) {
					p->frequency = temp_freq +
							temp_afc * 1000;
					pr_err("[%s] force lock, f:%d\n",
							__func__, p->frequency);
					break;
				}

				afc = 500;
			} else {
				lock_cnt++;
				temp_freq = p->frequency;
				if (afc > 50)
					temp_afc = 500;
				else if (afc < -50)
					temp_afc = -500;
				else
					temp_afc = afc;
			}

			if (((abs(afc) > (500 - AFC_BEST_LOCK))
				&& (abs(afc) < (500 + AFC_BEST_LOCK))
				&& (abs(afc) != 500))
				|| ((abs(afc) == 500) && (lock_cnt > 0))) {
				p->frequency += afc * 1000;
				break;
			}

			if (afc >= (500 + AFC_BEST_LOCK))
				afc = 500;

			p->frequency += afc * 1000;

			if (unlikely(p->frequency > maxafcfreq)) {
				pr_err("[%s] [%d] is exceed maxafcfreq[%d]\n",
					__func__, p->frequency, maxafcfreq);
				p->frequency = set_freq;
				return -1;
			}
#if 0 /*if enable, it would miss program*/
			if (unlikely(c->frequency < minafcfreq)) {
				pr_dbg("[%s] [%d] is exceed minafcfreq[%d]\n",
						__func__,
						c->frequency, minafcfreq);
				c->frequency = set_freq;
				return -1;
			}
#endif
			if (likely(!(count--))) {
				pr_err("[%s] exceed the afc count\n", __func__);
				p->frequency = set_freq;
				return -1;
			}

			/* delete it will miss program
			 * when c->frequency equal program frequency
			 */
			p->frequency++;
			if (fe->ops.tuner_ops.set_analog_params) {
				params.frequency = p->frequency;
				params.mode = p->afc_range;
				params.audmode = p->audmode;
				params.std = p->std;
				fe->ops.tuner_ops.set_analog_params(fe,
						&params);
			}
		}

		/* After correcting the frequency offset success,
		 * need to set up tuner.
		 */
		if (fe->ops.tuner_ops.set_analog_params) {
			params.frequency = p->frequency;
			params.mode = p->afc_range;
			params.audmode = p->audmode;
			params.std = p->std;
			fe->ops.tuner_ops.set_analog_params(fe,
					&params);

			if (tuner_id == AM_TUNER_SI2151 ||
				tuner_id == AM_TUNER_SI2159 ||
				tuner_id == AM_TUNER_R840 ||
				tuner_id == AM_TUNER_R842)
				usleep_range(10 * 1000, 10 * 1000 + 100);
			else if (tuner_id == AM_TUNER_MXL661)
				usleep_range(30 * 1000, 30 * 1000 + 100);
		}

		freq_success = p->frequency;
		ktime_get_ts(&success_time);
		pr_dbg("[%s] get afc %d khz done, freq %u.\n",
				__func__, afc, p->frequency);
	}

	return 0;
}

static int atvdemod_fe_set_property(struct v4l2_frontend *v4l2_fe,
		struct v4l2_property *tvp)
{
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	struct v4l2_analog_parameters *params = &v4l2_fe->params;

	pr_dbg("%s: cmd = 0x%x.\n", __func__, tvp->cmd);

	switch (tvp->cmd) {
	case V4L2_SOUND_SYS:
		/* aud_mode = tvp->data & 0xFF; */
		amlatvdemod_devp->soundsys = tvp->data & 0xFF;
		if (amlatvdemod_devp->soundsys != 0xFF) {
			aud_mode = amlatvdemod_devp->soundsys;
			params->soundsys = aud_mode;
		}
		priv->sound_sys.output_mode = tvp->data & 0xFF;
		break;

	case V4L2_SLOW_SEARCH_MODE:
		slow_mode = tvp->data;
		break;

	case V4L2_SIF_OVER_MODULATION:
		priv->sound_sys.sif_over_modulation = tvp->data;
		break;

	default:
		pr_dbg("%s: property %d doesn't exist\n",
				__func__, tvp->cmd);
		return -EINVAL;
	}

	return 0;
}

static int atvdemod_fe_get_property(struct v4l2_frontend *v4l2_fe,
		struct v4l2_property *tvp)
{
	pr_dbg("%s: cmd = 0x%x.\n", __func__, tvp->cmd);

	switch (tvp->cmd) {
	case V4L2_SOUND_SYS:
		tvp->data = ((aud_std  & 0xFF) << 16)
				| ((signal_audmode & 0xFF) << 8)
				| (aud_mode & 0xFF);
		break;

	case V4L2_SLOW_SEARCH_MODE:
		tvp->data = slow_mode;
		break;

	default:
		pr_dbg("%s: property %d doesn't exist\n",
				__func__, tvp->cmd);
		return -EINVAL;
	}

	return 0;
}

static enum v4l2_search atvdemod_fe_search(struct v4l2_frontend *v4l2_fe)
{
	struct analog_parameters params;
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct atv_demod_priv *priv = NULL;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	enum v4l2_status tuner_state = V4L2_TIMEDOUT;
	enum v4l2_status ade_state = V4L2_TIMEDOUT;
	bool pll_lock = false;
	/*struct atv_status_s atv_status;*/
	__u32 set_freq = 0;
	__u32 minafcfreq = 0, maxafcfreq = 0;
	__u32 afc_step = 0;
	int tuner_status_cnt_local = tuner_status_cnt;
	v4l2_std_id std_bk = 0;
	unsigned int audio = 0;
	unsigned int soundsys = 0;
	int double_check_cnt = 1;
	int auto_search_std = 0;
	int search_count = 0;
	/* bool try_secam = false; */
	int ret = -1;
	unsigned int tuner_id = v4l2_fe->tuner_id;
	int priv_cfg = 0;

	if (unlikely(!fe || !p ||
			!fe->ops.tuner_ops.get_status ||
			!fe->ops.analog_ops.has_signal ||
			!fe->ops.analog_ops.set_params ||
			!fe->ops.analog_ops.set_config)) {
		pr_err("[%s] error: NULL function or pointer.\n", __func__);
		return V4L2_SEARCH_INVALID;
	}

	priv = fe->analog_demod_priv;
	if (priv->state != ATVDEMOD_STATE_WORK) {
		pr_err("[%s] ATV state is not work.\n", __func__);
		return V4L2_SEARCH_INVALID;
	}

	if (p->afc_range == 0) {
		pr_err("[%s] afc_range == 0, skip the search\n", __func__);
		return V4L2_SEARCH_INVALID;
	}

	pr_info("[%s] afc_range: [%d], tuner: [%d], freq: [%d], flag: [%d].\n",
			__func__, p->afc_range, tuner_id,
			p->frequency, p->flag);

	/* backup the freq by api */
	set_freq = p->frequency;

	/* Before enter search, need set the std first.
	 * If set p->analog.std == 0, will search all std (PAL/NTSC/SECAM),
	 * and need tvafe identify signal type.
	 */
	if (p->std == 0) {
		p->std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
		/* p->std = V4L2_COLOR_STD_PAL | V4L2_STD_DK; */
		auto_search_std = 0x01;
		pr_dbg("[%s] user std is 0, so set it to NTSC | M.\n",
				__func__);
	}

	if (p->audmode == 0) {
		if (auto_search_std)
			p->audmode = p->std & 0x00FFFFFF;
		else {
			if (p->std & V4L2_COLOR_STD_PAL)
				p->audmode = V4L2_STD_PAL_DK;
			else if (p->std & V4L2_COLOR_STD_NTSC)
				p->audmode = V4L2_STD_NTSC_M;
			else if (p->std & V4L2_COLOR_STD_SECAM)
				p->audmode = V4L2_STD_PAL_DK;

			p->std = (p->std & 0xFF000000) | p->audmode;
		}
		auto_search_std |= 0x02;
		pr_dbg("[%s] user audmode is 0, so set it to %s.\n",
				__func__, v4l2_std_to_str(p->audmode));
	}

	priv_cfg = AML_ATVDEMOD_SCAN_MODE;
	fe->ops.analog_ops.set_config(fe, &priv_cfg);

	/*set the afc_range and start freq*/
	minafcfreq = p->frequency - p->afc_range;
	maxafcfreq = p->frequency + p->afc_range;

	/*from the current freq start, and set the afc_step*/
	/*if step is 2Mhz,r840 will miss program*/
	if (slow_mode ||
		(tuner_id == AM_TUNER_R840 || tuner_id == AM_TUNER_R842) ||
		(p->afc_range == ATV_AFC_1_0MHZ)) {
		pr_dbg("[%s] slow mode to search the channel\n", __func__);
		afc_step = ATV_AFC_1_0MHZ;
	} else if (!slow_mode) {
		afc_step = p->afc_range/* ATV_AFC_2_0MHZ */;
	} else {
		pr_dbg("[%s] slow mode to search the channel\n", __func__);
		afc_step = ATV_AFC_1_0MHZ;
	}

	/**enter auto search mode**/
	pr_dbg("[%s] Auto search std: 0x%08x, audmode: 0x%08x\n",
			__func__, (unsigned int) p->std, p->audmode);

	while (minafcfreq <= p->frequency &&
			p->frequency <= maxafcfreq) {

		params.frequency = p->frequency;
		params.mode = p->afc_range;
		params.audmode = p->audmode;
		params.std = p->std;
		fe->ops.analog_ops.set_params(fe, &params);

		pr_dbg("[%s] [%d] is processing, [min=%d, max=%d].\n",
				__func__, p->frequency, minafcfreq, maxafcfreq);

		pll_lock = false;
		tuner_status_cnt_local = tuner_status_cnt;
		do {
			if (tuner_id == AM_TUNER_MXL661) {
				usleep_range(30 * 1000, 30 * 1000 + 100);
			} else if (tuner_id == AM_TUNER_R840 ||
					tuner_id == AM_TUNER_R842) {
				usleep_range(10 * 1000, 10 * 1000 + 100);
				fe->ops.tuner_ops.get_status(fe,
						(u32 *)&tuner_state);
			} else {
				/* AM_TUNER_SI2151 and AM_TUNER_SI2159 */
				usleep_range(10 * 1000, 10 * 1000 + 100);
			}

			fe->ops.analog_ops.has_signal(fe, (u16 *)&ade_state);
			tuner_status_cnt_local--;
			if (((ade_state == V4L2_HAS_LOCK ||
				tuner_state == V4L2_HAS_LOCK) &&
				(tuner_id != AM_TUNER_R840 &&
				tuner_id != AM_TUNER_R842)) ||
				((ade_state == V4L2_HAS_LOCK &&
				tuner_state == V4L2_HAS_LOCK) &&
				(tuner_id == AM_TUNER_R840 ||
				tuner_id == AM_TUNER_R842))) {
				pll_lock = true;
				break;
			}

			if (tuner_status_cnt_local == 0) {
#if 0 /* when need to support secam-l, will enable it */
				if (auto_search_std &&
					try_secam == false &&
					!(p->std & V4L2_COLOR_STD_SECAM) &&
					!(p->std & V4L2_STD_SECAM_L)) {
					/* backup the std and audio mode */
					std_bk = p->std;
					audio = p->audmode;

					p->std = (V4L2_COLOR_STD_SECAM
							| V4L2_STD_SECAM_L);
					p->audmode = V4L2_STD_SECAM_L;

					params.frequency = p->frequency;
					params.mode = p->afc_range;
					params.audmode = p->audmode;
					params.std = p->std;
					fe->ops.analog_ops.set_params(fe,
							&params);

					try_secam = true;

					tuner_status_cnt_local =
						tuner_status_cnt / 2;

					continue;
				}

				if (try_secam) {
					p->std = std_bk;
					p->audmode = audio;

					params.frequency = p->frequency;
					params.mode = p->afc_range;
					params.audmode = p->audmode;
					params.std = p->std;
					fe->ops.analog_ops.set_params(fe,
							&params);

					try_secam = false;
				}
#endif
				break;
			}
		} while (1);

		std_bk = 0;
		audio = 0;

		if (pll_lock) {

			pr_dbg("[%s] freq: [%d] pll lock success\n",
					__func__, p->frequency);

			ret = atvdemod_fe_afc_closer(v4l2_fe, minafcfreq,
					maxafcfreq + ATV_AFC_500KHZ, 1);
			if (ret == 0) {
				atvdemod_fe_try_analog_format(v4l2_fe,
						auto_search_std,
						&std_bk, &audio, &soundsys);

				pr_info("[%s] freq:%d, std_bk:0x%x, audmode:0x%x, search OK.\n",
						__func__, p->frequency,
						(unsigned int) std_bk, audio);

				if (std_bk != 0) {
					p->audmode = audio;
					p->std = std_bk;
					/*avoid std unenable */
					p->frequency -= 1;
					p->soundsys = soundsys;
					std_bk = 0;
					audio = 0;
				}

				/* sync param */
				/* aml_fe_analog_sync_frontend(fe); */
				priv_cfg = AML_ATVDEMOD_UNSCAN_MODE;
				fe->ops.analog_ops.set_config(fe, &priv_cfg);
				return V4L2_SEARCH_SUCCESS;
			}
		}

		pr_dbg("[%s] freq[analog.std:0x%08x] is[%d] unlock\n",
				__func__,
				(uint32_t) p->std, p->frequency);

		/* when manual search, just search current freq */
		if (p->flag == ANALOG_FLAG_MANUL_SCAN)
			break;

		if (p->frequency >= 44200000 &&
			p->frequency <= 44300000 &&
			double_check_cnt) {
			double_check_cnt--;
			pr_err("%s 44.25Mhz double check\n", __func__);
		} else {
			++search_count;
			p->frequency += afc_step * ((search_count % 2) ?
					-search_count : search_count);
		}
	}

	pr_dbg("[%s] [%d] over of range [min=%d, max=%d], search failed.\n",
			__func__, p->frequency, minafcfreq, maxafcfreq);
	p->frequency = set_freq;

	priv_cfg = AML_ATVDEMOD_UNSCAN_MODE;
	fe->ops.analog_ops.set_config(fe, &priv_cfg);

	return DVBFE_ALGO_SEARCH_FAILED;
}

static struct v4l2_frontend_ops atvdemod_fe_ops = {
	.set_property = atvdemod_fe_set_property,
	.get_property = atvdemod_fe_get_property,
	.search = atvdemod_fe_search,
};

struct dvb_frontend *aml_atvdemod_attach(struct dvb_frontend *fe,
		struct v4l2_frontend *v4l2_fe,
		struct i2c_adapter *i2c_adap, u8 i2c_addr, u32 tuner_id)
{
	int instance = 0;
	struct atv_demod_priv *priv = NULL;

	mutex_lock(&atv_demod_list_mutex);

	instance = hybrid_tuner_request_state(struct atv_demod_priv, priv,
			hybrid_tuner_instance_list,
			i2c_adap, i2c_addr, DEVICE_NAME);

	priv->atvdemod_param.tuner_id = tuner_id;

	switch (instance) {
	case 0:
		mutex_unlock(&atv_demod_list_mutex);
		return NULL;
	case 1:
		fe->analog_demod_priv = priv;

		priv->afc.fe = fe;
		atv_demod_afc_init(&priv->afc);

		priv->monitor.fe = fe;
		atv_demod_monitor_init(&priv->monitor);

		priv->standby = true;

		pr_info("%s: aml_atvdemod found.\n", __func__);
		break;
	default:
		fe->analog_demod_priv = priv;
		break;
	}

	mutex_unlock(&atv_demod_list_mutex);

	fe->ops.info.type = FE_ANALOG;

	memcpy(&fe->ops.analog_ops, &atvdemod_ops,
			sizeof(struct analog_demod_ops));

	memcpy(&v4l2_fe->ops, &atvdemod_fe_ops,
			sizeof(struct v4l2_frontend_ops));

	return fe;
}
EXPORT_SYMBOL(aml_atvdemod_attach);
