/*
 * drivers/amlogic/media/enhancement/amvecm/hdr/am_hdr10_plus_ootf.c
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

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "../arch/vpp_hdr_regs.h"
#include "am_hdr10_plus.h"
#include "am_hdr10_plus_ootf.h"
#include "../set_hdr2_v0.h"

unsigned int hdr10_plus_printk;
module_param(hdr10_plus_printk, uint, 0664);
MODULE_PARM_DESC(hdr10_plus_printk, "hdr10_plus_printk");

#define pr_hdr(fmt, args...)\
	do {\
		if (hdr10_plus_printk)\
			pr_info(fmt, ## args);\
	} while (0)

/*EBZCurveParameters to gen OOTF bezier curves*/
void EBZCurveParametersInit(struct EBZCurveParameters *EBZCurveParameters)
{
	int i;

	EBZCurveParameters->Sx = 0;
	EBZCurveParameters->Sy = 0;
	EBZCurveParameters->order = N;
	for (i = 0; i < N; i++)
		EBZCurveParameters->Anchor[i] = PORCESSING_DATA_MAX;
}

/*percentile actually obtained from metadata */
void PercentileInit(struct Percentiles *percentile,
	struct hdr10_plus_sei_s *hdr10_plus_sei)
{
	int i;

	percentile->num_percentile = hdr10_plus_sei->num_distributions[0];
	for (i = 0; i < percentile->num_percentile; i++) {
		percentile->percentilePercent[i] =
			hdr10_plus_sei->distribution_index[0][i];
		percentile->percentileValue[i] =
			hdr10_plus_sei->distribution_values[0][i] / 10;
	}
}

/*metadata should obtained from 3C or 2094 metadata*/
void MetaDataInit(struct Scene2094Metadata *Metadata,
	struct hdr10_plus_sei_s *hdr10_plus_sei)
{
	int i;

	Metadata->maxSceneSourceLuminance =
		max(max(hdr10_plus_sei->maxscl[0][0],
			hdr10_plus_sei->maxscl[0][1]),
			hdr10_plus_sei->maxscl[0][2]);
	Metadata->minLuminance = MIN_LUMINANCE;
	Metadata->referenceLuminance =
		hdr10_plus_sei->targeted_system_display_maximum_luminance;
	PercentileInit(&(Metadata->percentiles), hdr10_plus_sei);

	Metadata->EBZCurveParameters.Sx = hdr10_plus_sei->knee_point_x[0];
	Metadata->EBZCurveParameters.Sy = hdr10_plus_sei->knee_point_y[0];
	Metadata->EBZCurveParameters.order =
		hdr10_plus_sei->num_bezier_curve_anchors[0];
	for (i = 0; i < N - 1; i++)
		Metadata->EBZCurveParameters.Anchor[i] =
			hdr10_plus_sei->bezier_curve_anchors[0][i];
}

/* get EBZcurve params from metadata.*/
void getMetaData(struct Scene2094Metadata *metadata,
	struct EBZCurveParameters *referenceBezierParams)
{
	int i;

	referenceBezierParams->order = metadata->EBZCurveParameters.order + 1;
	referenceBezierParams->Sy = metadata->EBZCurveParameters.Sy;
	referenceBezierParams->Sx = metadata->EBZCurveParameters.Sx;
	for (i = 0; i < referenceBezierParams->order; i++)
		referenceBezierParams->Anchor[i] =
			metadata->EBZCurveParameters.Anchor[i];
}

/*TV side default setting:  bezier curve params to gen basic OOTF curve*/
void BasisOOTF_Params_init(struct BasisOOTF_Params *BasisOOTF_Params)
{
	int P2ToP9_MAX1_init[ORDER - 2] = {
		0.5582 * PORCESSING_DATA_MAX, 0.6745 * PORCESSING_DATA_MAX,
		0.7703 * PORCESSING_DATA_MAX, 0.8231 * PORCESSING_DATA_MAX,
		0.8729 * PORCESSING_DATA_MAX, 0.9130 * PORCESSING_DATA_MAX,
		0.9599 * PORCESSING_DATA_MAX, 0.9844 * PORCESSING_DATA_MAX
	};
	int P2ToP9_MAX2_init[ORDER - 2] = {
		0.4839 * PORCESSING_DATA_MAX, 0.6325 * PORCESSING_DATA_MAX,
		0.7253 * PORCESSING_DATA_MAX, 0.7722 * PORCESSING_DATA_MAX,
		0.8201 * PORCESSING_DATA_MAX, 0.8837 * PORCESSING_DATA_MAX,
		0.9208 * PORCESSING_DATA_MAX, 0.9580 * PORCESSING_DATA_MAX
	};
	int i;

	/*u12*/
	BasisOOTF_Params->SY1_V1  = 0 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->SY1_V2 = 1229 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->SY1_T1  = 901 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->SY1_T2  = 4095 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->SY2_V1  = 0 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->SY2_V2  = 819 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->SY2_T1  = 1024 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->SY2_T2  = 3890 << (PROCESSING_MAX - 12);

	/* KP mixing gain (final KP from bounds KP # 1 and KP # 2*/
	/*as a function of scene percentile) */
	BasisOOTF_Params->KP_G_V1 = 4095 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->KP_G_V2 = 205 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->KP_G_T1 = 205 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->KP_G_T2 = 2048 << (PROCESSING_MAX - 12);

	/*Thresholds of minimum bound of P1 coefficient*/
	BasisOOTF_Params->P1_LIMIT_V1 = 3767 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->P1_LIMIT_V2 = 4013 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->P1_LIMIT_T1 = 41 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->P1_LIMIT_T2 = 410 << (PROCESSING_MAX - 12);

	/*Thresholds to compute relative shape of curve (P2~P9 coefficient)*/
	/*by pre-defined bounds - as a function of scene percentile*/
	BasisOOTF_Params->P2To9_T1 = 205 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->P2To9_T2 = 2252 << (PROCESSING_MAX - 12);

	for (i = 0; i < (ORDER - 2); i++) {
		BasisOOTF_Params->P2ToP9_MAX1[i] = P2ToP9_MAX1_init[i];
		BasisOOTF_Params->P2ToP9_MAX2[i] = P2ToP9_MAX2_init[i];
	}

	/*Ps mixing gain (obtain all Ps coefficients) -*/
	/*as a function of TM dynamic compression ratio*/
	BasisOOTF_Params->PS_G_T1 = 512 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->PS_G_T2 = 3890 << (PROCESSING_MAX - 12);

	/*Post-processing : Reduce P1/P2 (to enhance mid tone)*/
	/*for high TM dynamic range compression cases*/
	BasisOOTF_Params->LOW_SY_T1 = 20 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->LOW_SY_T2 = 164 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->LOW_K_T1  = 491 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->LOW_K_T2  = 1638 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->RED_P1_V1 = 2662 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->RED_P1_T1 = 410 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->RED_P1_T2 = 3071 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->RED_P2_V1 = 3276 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->RED_P2_T1 = 410 << (PROCESSING_MAX - 12);
	BasisOOTF_Params->RED_P2_T2 = 3071 << (PROCESSING_MAX - 12);
}

/*obtain percentile info on psl50 and psl99,*/
/*to calculate content average level and distribution later*/
void getPercentile_50_99(struct Percentiles *scenePercentiles,
	 int *psll50, int *psll99)
{
	int i;
	int npsll  = PERCENTILE_ORDER;
	int delta;

	/*Find exact percentiles if provided , else interpolate*/
	int psll50_1 =  -1, per50_1 = -1;
	int psll50_2 =  -1, per50_2 = -1;
	int psll99_1 =  -1, per99_1 = -1;
	int psll99_2 =  -1, per99_2 = -1;

	/* Search for exact percent index or bounds*/
	int curPercent = 0, prevPercent = 0;
	int curPsll = 0, prevPsll = 0;

	/*Set output to -1 (invalid)*/
	*psll50 =  -1;
	*psll99 =  -1;

	for (i = 0; i < npsll; i++) {
		if (i == 1 || i == 2)
			continue;
		curPercent = scenePercentiles->percentilePercent[i];
		curPsll    = scenePercentiles->percentileValue[i];
		if (curPercent == 50)
			*psll50 = curPsll;
		else if (*psll50 == -1 && curPercent > 50 && prevPercent < 50) {
			per50_1  = prevPercent;
			per50_2  = curPercent;
			psll50_1 = prevPsll;
			psll50_2 = curPsll;
		}

		if (curPercent == 99) {
			*psll99 = curPsll;
		} else if (*psll99 == -1 && curPercent > 99 &&
			prevPercent < 99) {
			per99_1  = prevPercent;
			per99_2  = curPercent;
			psll99_1 = prevPsll;
			psll99_2 = curPsll;
		}
		prevPercent = curPercent;
		prevPsll    = curPsll;
	}

	if (*psll50 == -1) {
		delta = max((per50_2 - per50_1), 1);
		*psll50 = psll50_1 + (psll50_2 - psll50_1) *
		(50 - per50_1) / delta;
	}

	if (*psll99 == -1) {
		delta = max((per99_2 - per99_1), 1);
		*psll99 = psll99_1 + (psll99_2 - psll99_1) *
		(99 - per99_1) / delta;
	}
}

/* function to calculate linear interpolation*/
int rampWeight(int v1,  int v2, int t1, int t2, int t)
{
	int retVal = v1;

	if (t1 == t2)
		retVal = (t < t1) ? (v1) : (v2);
	else {
		if (t <= t1)
			retVal = v1;
		else if (t >= t2)
			retVal = v2;
		else
			retVal = v1 + (v2 - v1) * (t - t1) / (t2 - t1);
	}

	return retVal;
}

/*p1 calculation based on default setting and*/
/*content statistic information(percentile)*/
int calcP1(int sx, int sy, int tgtL, int calcMaxL,
	struct BasisOOTF_Params *basisOOTF_Params,
	int *p1_red_gain)

{
	int ax, ay, k, p1_limit, k2, p1_t, p1;
	int low_sy_g, high_k_g, high_tm_g, red_p1;

	ax = min(sx, PORCESSING_DATA_MAX);
	ay = min(sy, PORCESSING_DATA_MAX);

	k = tgtL * PORCESSING_DATA_MAX / max(tgtL, calcMaxL);
	p1_limit = rampWeight(basisOOTF_Params->P1_LIMIT_V2,
		basisOOTF_Params->P1_LIMIT_V1,
		basisOOTF_Params->P1_LIMIT_T1,
		basisOOTF_Params->P1_LIMIT_T2, sy);
	k2 = (PORCESSING_DATA_MAX + 1 - ax) * PORCESSING_DATA_MAX /
		(ORDER * (PORCESSING_DATA_MAX + 1 - ay));
	p1_t = k2 * PORCESSING_DATA_MAX / k;
	p1 = max(min(p1_t, p1_limit), 0);
	low_sy_g = rampWeight(PORCESSING_DATA_MAX + 1, 0,
		basisOOTF_Params->LOW_SY_T1, basisOOTF_Params->LOW_SY_T2, sy);
	high_k_g = rampWeight(PORCESSING_DATA_MAX + 1, 0,
		basisOOTF_Params->LOW_K_T1, basisOOTF_Params->LOW_K_T2, k);
	high_tm_g = low_sy_g * high_k_g >> PROCESSING_MAX;
	red_p1 = rampWeight(PORCESSING_DATA_MAX + 1,
		basisOOTF_Params->RED_P1_V1, basisOOTF_Params->RED_P1_T1,
		basisOOTF_Params->RED_P1_T2, high_tm_g);
	p1 = min(max((p1 * red_p1) >> PROCESSING_MAX, P1MIN), p1_limit);

	if (p1_red_gain != NULL)
		*p1_red_gain = high_tm_g;

	return p1;
}

/*gen BasisOOTF parameters (sx,sy,p1~pn-1),based on content percentiles*/
	/*and default bezier params*/
int basisOOTF(struct Scene2094Metadata *metadata,
	struct BasisOOTF_Params *basisOOTF_Params, int productPeak,
	int sourceMaxL, struct EBZCurveParameters *sceneBezierParams)
{
	int order = ORDER;
	int psll50, psll99, centerLuminance, k, sy1, sy2, sy, sx, rem;
	int high_tm_g, p1, rem_p29, rem_ps, rem_red_p2, ps2to9[ORDER - 1];
	int coeffi, plin[ORDER - 1], pcoeff[ORDER - 1];
	int targetLuminance = productPeak;
	int sourceLuminance = max(targetLuminance, sourceMaxL);
	int i;

	getPercentile_50_99(&(metadata->percentiles), &psll50, &psll99);

	centerLuminance = psll50 * PORCESSING_DATA_MAX / max(psll99, 1);
	k = targetLuminance * PORCESSING_DATA_MAX / sourceLuminance;

	sy1 = rampWeight(basisOOTF_Params->SY1_V1, basisOOTF_Params->SY1_V2,
		basisOOTF_Params->SY1_T1, basisOOTF_Params->SY1_T2, k);
	sy2 = rampWeight(basisOOTF_Params->SY2_V1, basisOOTF_Params->SY2_V2,
		basisOOTF_Params->SY2_T1, basisOOTF_Params->SY2_T2, k);
	rem = rampWeight(basisOOTF_Params->KP_G_V1, basisOOTF_Params->KP_G_V2,
		basisOOTF_Params->KP_G_T1, basisOOTF_Params->KP_G_T2,
		centerLuminance);
	sy = (rem * sy1 + (PORCESSING_DATA_MAX + 1 - rem) * sy2) >>
		PROCESSING_MAX;
	sx = sy * k;

	/*P coefficient*/
	high_tm_g = 0;

	p1 = calcP1(sx, sy, targetLuminance, sourceMaxL,
		basisOOTF_Params, &high_tm_g);

	for (i = 0; i < (NPCOEFF - 1); i++) {
		rem_p29 = rampWeight(basisOOTF_Params->P2ToP9_MAX2[i],
		basisOOTF_Params->P2ToP9_MAX1[i], basisOOTF_Params->P2To9_T1,
		basisOOTF_Params->P2To9_T2, centerLuminance);
		ps2to9[i] = (rem_p29 * basisOOTF_Params->P2ToP9_MAX1[i] +
		(PORCESSING_DATA_MAX + 1 - rem_p29) *
		basisOOTF_Params->P2ToP9_MAX2[i]) >> PROCESSING_MAX;
	}

	rem_ps = rampWeight(PORCESSING_DATA_MAX + 1, 0,
		basisOOTF_Params->PS_G_T1, basisOOTF_Params->PS_G_T2, k);

	pcoeff[0] = p1;
	for (i = 1; i < NPCOEFF; i++) {
		coeffi = i + 1;
		plin[i] = (coeffi * PORCESSING_DATA_MAX / order);
		pcoeff[i] = max(min((rem_ps * ps2to9[i - 1] +
		(PORCESSING_DATA_MAX + 1 - rem_ps) * plin[i]) >> PROCESSING_MAX,
		coeffi * p1), plin[i]);
	}

	/*p[1] recalc precision:0.01,bad*/
	rem_red_p2 = rampWeight(PORCESSING_DATA_MAX + 1,
	basisOOTF_Params->RED_P2_V1, basisOOTF_Params->RED_P2_T1,
	basisOOTF_Params->RED_P2_T2, high_tm_g);
	pcoeff[1] = max(min((pcoeff[1] * rem_red_p2) >> PROCESSING_MAX, 2 * p1),
	2 * PORCESSING_DATA_MAX / ORDER);

	/*finally  pcoeff[ORDER-1],sy,sx*/
	sceneBezierParams->Sx = sx;
	sceneBezierParams->Sy = sy;
	/*only calc  ORDER(10)  < actual order(15)*/
	for (i = 0; i < NPCOEFF; i++)
		sceneBezierParams->Anchor[i] = pcoeff[i];

	return 0;
}

/* receive bezier parameter(Kx,ky,P)and return guided parameter base on*/
/*product panel luminance out: product curve params(Kx,ky,P) */
int GuidedOOTF(struct Scene2094Metadata *metadata,
	struct BasisOOTF_Params *basisOOTF_Params,
	struct EBZCurveParameters *referenceBezierParams,
	int ProductLuminance,
	struct EBZCurveParameters *productBezierParams)
{
	int KP_BYPASS = 1229;
	int refenceLuminance = metadata->referenceLuminance;
	int productLuminance = ProductLuminance;
	int minLuminance     = metadata->minLuminance;
	int maxLuminance     = metadata->maxSceneSourceLuminance;
	int order = referenceBezierParams->order;
	int numP = order - 1;

	int blendCoeff, norm;
	int anchorLinear[14];
	int i;
	int ps1;

	struct EBZCurveParameters minBezierParams;

	EBZCurveParametersInit(&minBezierParams);

	for (i = 0; i < numP; i++)
		anchorLinear[i] = (i + 1) * PORCESSING_DATA_MAX / order;

/*---------case 0: productPeak < minL ----------------- */
	if (productLuminance < minLuminance) {
		productBezierParams->Sx = 0;
		productBezierParams->Sy = 0;
		for (i = 0; i < numP; i++)
			productBezierParams->Anchor[i] = anchorLinear[i];
		productBezierParams->order = order;
		return -1;
	}
/*---------case 1: productPeak = ref ----------------- */
	if (productLuminance == refenceLuminance) {
		productBezierParams->Sx = referenceBezierParams->Sx;
		productBezierParams->Sy = referenceBezierParams->Sy;
		productBezierParams->order = referenceBezierParams->order;

		for (i = 0; i < numP; i++) {
			productBezierParams->Anchor[i] =
			referenceBezierParams->Anchor[i];
		}
	}
/*---------case 2: productPeak > maxL ----------------- */
	else if (productLuminance > maxLuminance) {
		productBezierParams->Sx = KP_BYPASS;
		productBezierParams->Sy = KP_BYPASS;
		productBezierParams->order = order;
		for (i = 0; i < numP; i++)
			productBezierParams->Anchor[i] = anchorLinear[i];
	}
/*---------case 3: minL < productPeak < maxL ----------------- */
	else {
		productBezierParams->order = referenceBezierParams->order;
/*---------case 3.1: minL < productPeak < ref ----------------- */
		if (productLuminance < refenceLuminance) {
			norm = refenceLuminance - minLuminance;
			blendCoeff = refenceLuminance - productLuminance;

			basisOOTF(metadata, basisOOTF_Params, minLuminance,
			maxLuminance, &minBezierParams);

			productBezierParams->Sy =
			(blendCoeff * minBezierParams.Sy +
			(norm - blendCoeff) * referenceBezierParams->Sy +
			(norm >> 1)) / norm;
			productBezierParams->Sx =
			productBezierParams->Sy * productLuminance /
				maxLuminance;
			for (i = 0; i < numP ; i++) {
				productBezierParams->Anchor[i] =
				(blendCoeff * referenceBezierParams->Anchor[i] +
				(norm - blendCoeff) *
				minBezierParams.Anchor[i] +
				(norm >> 1)) / norm;
			}
/*---------case 3.2: ref < productPeak < maxL ----------------- */
		} else {
			norm       =  maxLuminance - refenceLuminance;
			blendCoeff =  productLuminance - refenceLuminance;

			productBezierParams->Sy = (blendCoeff * KP_BYPASS +
			(norm - blendCoeff) * referenceBezierParams->Sy +
			(norm >> 1)) / norm;
			productBezierParams->Sx = productBezierParams->Sy *
			productLuminance / maxLuminance;

			for (i = 0; i < numP ; i++) {
				productBezierParams->Anchor[i] =
				(blendCoeff * anchorLinear[i] +
				(norm - blendCoeff) *
				referenceBezierParams->Anchor[i] +
				(norm >> 1)) / norm;
			}
		}

		ps1 = calcP1(productBezierParams->Sx, productBezierParams->Sy,
			productLuminance, maxLuminance, basisOOTF_Params, NULL);

		productBezierParams->Anchor[0] = ps1;

		for (i = 1; i < numP; i++) {
			productBezierParams->Anchor[i] =
			min(productBezierParams->Anchor[i],
			(i + 1) * productBezierParams->Anchor[0]);
		}
	}

	return 0;
}

/*bezier optimise method to gen bezier function*/
int Decasteliau(uint64_t *BezierCurve, uint64_t *AnchorY,
	uint64_t u, int order, uint64_t range_ebz_x)
{
	uint64_t PointY[16];
	int i, j;

	for (i = 0; i < order + 1; i++)
		PointY[i] = AnchorY[i];

	for (i = 1; i <= order; i++)
		for (j = 0; j <= order - i; j++) {
			PointY[j] = (PointY[j] * (range_ebz_x - u) +
			PointY[j + 1] * u + range_ebz_x / 2);
			PointY[j] = div64_u64(PointY[j], range_ebz_x);
		}

	BezierCurve[1] = PointY[0];
	return 0;
}

#define org_anchory

uint64_t oo_lut_x[OOLUT_NUM] = {
	0, 16, 32, 64, 128, 256, 512, 1024, 2048, 2560, 3072, 3584, 4096,
	5120, 6144, 7168, 8192, 10240, 12288, 14336, 16384, 20480, 24576,
	28672, 32768, 40960, 49152, 57344, 65536, 81920, 98304, 114688,
	131072, 163840, 196608, 229376, 262144, 327680, 393216, 458752,
	524288, 655360, 786432, 917504, 1048576, 1179648, 1310720, 1441792,
	1572864, 1703936, 1835008, 1966080, 2097152, 2359296, 2621440, 2883584,
	3145728, 3407872, 3670016, 3932160, 4194304, 4718592, 5242880, 5767168,
	6291456, 6815744, 7340032, 7864320, 8388608, 9437184, 10485760,
	11534336, 12582912, 13631488, 14680064, 15728640, 16777216, 18874368,
	20971520, 23068672, 25165824, 27262976, 29360128, 31457280, 33554432,
	37748736, 41943040, 46137344, 50331648, 54525952, 58720256, 62914560,
	67108864, 75497472, 83886080, 92274688, 100663296, 109051904, 117440512,
	125829120, 134217728, 150994944, 167772160, 184549376, 201326592,
	218103808, 234881024, 251658240, 268435456, 301989888, 335544320,
	369098752, 402653184, 436207616, 469762048, 503316480, 536870912,
	603979776, 671088640, 738197504, 805306368, 872415232, 939524096,
	1006632960, 1073741824, 1207959552, 1342177280, 1476395008, 1610612736,
	1744830464, 1879048192, 2013265920ULL, 2147483648ULL, 2281701376ULL,
	2415919104ULL, 2550136832ULL, 2684354560ULL, 2818572288ULL,
	2952790016ULL, 3087007744ULL, 3221225472ULL, 3355443200ULL,
	3489660928ULL, 3623878656ULL, 3758096384ULL,
	3892314112ULL, 4026531840ULL, 4160749568ULL, 4294967296ULL
};

/*gen OOTF curve and gain from (sx,sy) and P1~pn-1*/
int genEBZCurve(uint64_t *CurveX, uint64_t *CurveY,
	unsigned int *gain, unsigned int *gain_ter,
	uint64_t nKx, uint64_t nKy,
	uint64_t *AnchorY, int order)
{
	uint64_t myAnchorY[16];
	uint64_t temp;
	uint64_t linearX[POINTS];
	uint64_t Kx, Ky;

	uint64_t range_ebz_x;
	uint64_t range_ebz_y;

	uint64_t step_alpha;
	uint64_t step_ter[POINTS];
	uint64_t BezierCurve[2];
	int i;
	int numP = N-1;


	/*u12->U16*/
	Kx = nKx<<(U32 - PROCESSING_MAX);
	Ky = nKy<<(U32 - PROCESSING_MAX);

	range_ebz_x = _U32_MAX - Kx;
	range_ebz_y = _U32_MAX - Ky;

#ifdef org_anchory
	for (i = 0; i < numP; i++)/* u12-> ebz_y, u32*/
		/*anchorY default range:PROCESSING_MAX */
		myAnchorY[i + 1] = AnchorY[i] << (U32-PROCESSING_MAX);
	myAnchorY[0] = 0;
	myAnchorY[N] = _U32_MAX; /* u12 */
#else
	for (i = 0; i < numP; i++)/* u12-> ebz_y, u32*/
		/*anchorY default range:PROCESSING_MAX */
		myAnchorY[i + 1] = (AnchorY[i] * range_ebz_y) >> PROCESSING_MAX;
	myAnchorY[0] = 0;
	myAnchorY[N] = range_ebz_y; /*u12 -> U32*/
#endif  /* org_anchory */

	for (i = 0; i < POINTS; i++) {
		#if 0
		linearX[i] = (uint64_t)(i*_U32_MAX/POINTS); /* x index sample */
		#else
		linearX[i] = oo_lut_x[i];/* x index sample,u32 */
		#endif
		CurveY[i] = 0;
		step_ter[i] = 1023;
	}

	for (i = 0; i < POINTS; i++) {
		if (linearX[i] < Kx) {
			CurveX[i] = linearX[i];/*u32*/
			CurveY[i] = linearX[i] * Ky;
			CurveY[i] = div64_u64(CurveY[i], Kx + 1);
			temp = CurveY[i] << GAIN_BIT;/*u12*/
			gain[i] = div64_u64(temp, CurveX[i] + 1);
			step_ter[i] = 1024;
			/*just as a mark for debugging*/
			/*printk("%d(int64):x->y,%lld->%lld,gain=%d\n",*/
			/*i, CurveX[i],CurveY[i],gain[i]);*/
		} else{
			/* step_alpha = (linearX[i]-Kx)/range_ebz_x,*/
			/*norm in Decasteliau() function*/
			step_alpha = (linearX[i] - Kx);
			step_ter[i] = step_alpha;
			/* calc each point from 1st to N-th layer*/
			Decasteliau(&BezierCurve[0], myAnchorY, step_alpha,
			order, range_ebz_x);
			/*  u32    u32   u32*/
			#ifdef org_anchory   /*range_ebz_y = _U32_MAX*/
			CurveY[i] = Ky + ((range_ebz_y * BezierCurve[1] +
				range_ebz_y / 2) >> U32);
			#else
			CurveY[i] = Ky + ((range_ebz_y * BezierCurve[1] +
				range_ebz_y / 2));
			CurveY[i] = div64_u64(CurveY[i], range_ebz_y);
			#endif /* org_anchory*/

			/*CurveX[i] = Kx +*/
			/*((range_ebz_x * BezierCurve[0] + range_ebz_x / 2) / */
			/*range_ebz_x);*/
			CurveX[i] = Kx + step_alpha;
			temp = CurveY[i] << GAIN_BIT;
			gain[i] = div64_u64(temp, CurveX[i] + 1);

		}
		temp = CurveY[i] << GAIN_BIT;/*u12*/
		gain[i] = div64_u64(temp, CurveX[i] + 1);
		temp = CurveY[i] << GAIN_BIT;
		gain_ter[i] = div64_u64(temp, linearX[i] + 1);
	}
	return 0;
}

void vframe_hdr_plus_sei_s_init(struct hdr10_plus_sei_s *hdr10_plus_sei)
{

	int i;

	int percentilePercent_init[PERCENTILE_ORDER] = {
		1, 5, 10, 25, 50, 75, 90, 95, 99};
	int percentileValue_init[PERCENTILE_ORDER] = {
		0, 1, 2, 79, 2537, 9900, 9901, 9902, 9904};

	hdr10_plus_sei->num_distributions[0] = PERCENTILE_ORDER - 1;

	for (i = 0; i < 3; i++)
		hdr10_plus_sei->maxscl[0][i] = hdr_plus_sei.maxscl[0][i] / 10;

	hdr10_plus_sei->targeted_system_display_maximum_luminance =
		hdr_plus_sei.tgt_sys_disp_max_lumi;

	for (i = 0; i < (hdr10_plus_sei->num_distributions[0]); i++) {
		hdr10_plus_sei->distribution_index[0][i] =
			percentilePercent_init[i];
		hdr10_plus_sei->distribution_values[0][i] =
			percentileValue_init[i];

	}

	hdr10_plus_sei->knee_point_x[0] = hdr_plus_sei.knee_point_x[0];
	hdr10_plus_sei->knee_point_y[0] = hdr_plus_sei.knee_point_y[0];

	hdr10_plus_sei->num_bezier_curve_anchors[0] =
		hdr_plus_sei.num_bezier_curve_anchors[0];

	for (i = 0; i < (hdr10_plus_sei->num_bezier_curve_anchors[0]); i++) {
		hdr10_plus_sei->bezier_curve_anchors[0][i] =
		hdr_plus_sei.bezier_curve_anchors[0][i]<<(PROCESSING_MAX-10);
	}

	/*debug--*/
	if (hdr10_plus_printk) {

		for (i = 0; i < 3; i++)
			pr_hdr("hdr10_plus_sei->maxscl[0][%d]=%d\n",
			i, hdr10_plus_sei->maxscl[0][i]);

		pr_hdr("targeted_system_display_maximum_luminance=%d\n",
		hdr10_plus_sei->targeted_system_display_maximum_luminance);

		for (i = 0; i < (hdr10_plus_sei->num_distributions[0]); i++) {
			pr_hdr("distribution_values[0][%d]=%d\n",
				i, hdr10_plus_sei->distribution_values[0][i]);
			pr_hdr("hdr10_plus_sei->distribution_index[0][%d]=%d\n",
				i, hdr10_plus_sei->distribution_index[0][i]);
		}

		pr_hdr("hdr10_plus_sei->knee_point_x = %d\n"
			"hdr10_plus_sei->knee_point_y = %d\n",
			hdr10_plus_sei->knee_point_x[0],
			hdr10_plus_sei->knee_point_y[0]);

		pr_hdr("hdr10_plus_sei->num_bezier_curve_anchors[0] = %d\n",
			hdr10_plus_sei->num_bezier_curve_anchors[0]);

		for (i = 0; i < (hdr10_plus_sei->num_bezier_curve_anchors[0]);
			i++) {
			pr_hdr("bezier_curve_anchors[0][%d] = ", i);
			pr_hdr("%d\n", hdr_plus_sei.bezier_curve_anchors[0][i]);
		}
		for (i = 0; i < 3; i++) {
			pr_hdr("average_maxrgb[%d] = ", i);
			pr_hdr("%d\n", hdr_plus_sei.average_maxrgb[i]);
		}

		for (i = 0;
		i < (hdr_plus_sei.num_distribution_maxrgb_percentiles[0]);
		i++) {
			pr_hdr("distribution_maxrgb_percentages[0][%d] = ", i);
			pr_hdr("%d\n",
			hdr_plus_sei.distribution_maxrgb_percentages[0][i]);
		}
		for (i = 0;
		i < (hdr_plus_sei.num_distribution_maxrgb_percentiles[0]);
		i++) {
			pr_hdr("distribution_maxrgb_percentiles[0][%d] = ", i);
			pr_hdr("%d\n",
			hdr_plus_sei.distribution_maxrgb_percentiles[0][i]);
		}
	}

}

unsigned int gain[POINTS];
unsigned int gain_ter[POINTS];
uint64_t curveX[POINTS], curveY[POINTS];
// //mapping
// struct hdr_proc_lut_param_s _hdr_lut_param;

int hdr10_plus_ootf_gen(void)
{
	/*int referenceCurve_flag = 1;*/
	int order, i;
	uint64_t Kx, Ky;
	uint64_t  AnchorY[15];

	/* bezier params obtained from metadata */
	struct hdr10_plus_sei_s hdr10_plus_sei;
	struct Scene2094Metadata metadata;
	struct EBZCurveParameters referenceBezierParams;
	struct EBZCurveParameters productBezierParams;
	struct BasisOOTF_Params basisOOTF_Params;

	// mapping
	enum hdr_module_sel {
		VD1_HDR = 0x1,
		VD2_HDR = 0x2,
		OSD1_HDR = 0x4,
		VDIN0_HDR = 0x8,
		VDIN1_HDR = 0x10,
		DI_HDR = 0x20,
		HDR_MAX
	};

	int productpeak = 700;

	memset(curveX,   0, sizeof(uint64_t) * POINTS);
	memset(curveY,   0, sizeof(uint64_t) * POINTS);
	memset(gain,     0, sizeof(unsigned int) * POINTS);
	memset(gain_ter, 0, sizeof(unsigned int) * POINTS);

	BasisOOTF_Params_init(&basisOOTF_Params);

	/* the final tv OOTF curve params init*/
	EBZCurveParametersInit(&productBezierParams);

	/* the bezier parameters from metadata init*/
	EBZCurveParametersInit(&referenceBezierParams);

	/* repace with real vframe data*/
	vframe_hdr_plus_sei_s_init(&hdr10_plus_sei);

	/*step 1. get metadata from vframe*/
	MetaDataInit(&metadata, &hdr10_plus_sei);

	/*step 2. get bezier params from metadata*/
	getMetaData(&metadata, &referenceBezierParams);

	/*step 3. gen final guided OOTF*/
	/*if (referenceCurve_flag == 0)*/
		/* Basis OOTF : Direct calculation of product TM curve from*/
		/*ST-2094 percentile metadata */
		/*basisOOTF(&metadata, &basisOOTF_Params, productPeak,*/
		/* here  length(minBezierParams->Anchor) =order*/
		/*metadata.maxSceneSourceLuminance, &productBezierParams);*/
	/*else*/

	GuidedOOTF(
		&metadata, &basisOOTF_Params,
		&referenceBezierParams, productpeak,
		&productBezierParams);

	/*step 4. get guided bezier params*/
	Kx    = (uint64_t)productBezierParams.Sx;
	Ky    = (uint64_t)productBezierParams.Sy;
	order = productBezierParams.order;
	for (i = 0; i < productBezierParams.order - 1; i++)
		AnchorY[i] = (uint64_t)productBezierParams.Anchor[i];

	/*step 5. gen bezier curve*/
	genEBZCurve(&curveX[0], &curveY[0], &gain[0], &gain_ter[0],
		Kx, Ky, &AnchorY[0], order);
	/* debug */
	if (hdr10_plus_printk) {
		for (i = 0; i < POINTS; i++) {
			pr_hdr("fixed version:: %3d:(%lld, %lld)->%4d\n",
			i, curveX[i], curveY[i], gain[i]);
		}
		hdr10_plus_printk = 0;
	}

	/*hdr10+ temporary does not support other anchors  20190603*/
	if (hdr_plus_sei.num_bezier_curve_anchors[0] == 9) {
		for (i = 0; i < POINTS; i++)
			hdr_lut_param.ogain_lut[i] = gain[i];
		VSYNC_WR_MPEG_REG(VD1_HDR2_ADPS_ALPHA0, 0x28002800);
		/* VSYNC_WR_MPEG_REG(VD1_HDR2_ADPS_ALPHA1,0x0a7a2800);*/
		VSYNC_WR_MPEG_REG_BITS(VD1_HDR2_ADPS_ALPHA1, 0x2800, 0, 16);
		set_ootf_lut(VD1_HDR, &hdr_lut_param);
	}
	return 0;
}

