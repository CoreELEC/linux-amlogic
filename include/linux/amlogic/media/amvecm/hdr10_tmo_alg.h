/*
 * include/linux/amlogic/media/amvecm/am_hdr10_tmo_alg.h
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#ifndef HDR10_TMO
#define HDR10_TMO

#ifndef MAX
#define MAX(a, b) ({ \
			const typeof(a) _a = a; \
			const typeof(b) _b = b; \
			_a > _b ? _a : _b; \
		})
#endif // MAX

#ifndef MIN
#define MIN(a, b) ({ \
			const typeof(a) _a = a; \
			const typeof(b) _b = b; \
			_a < _b ? _a : _b; \
		})
#endif // MIN

#ifndef ABS
#define ABS(x) ({ \
			const typeof(x) _x = x; \
			_x > 0 ? _x : -_x; \
		})
#endif

struct aml_vm_reg {
	u32 ogain_lut[149];
};

struct aml_tmo_reg_sw {
	int tmo_en;              // 0 1
	int reg_highlight;       //u10: control overexposure level
	int reg_hist_th;         //u7
	int reg_light_th;
	int reg_highlight_th1;
	int reg_highlight_th2;
	int reg_display_e;       //u10
	int reg_middle_a;        //u7
	int reg_middle_a_adj;    //u10
	int reg_middle_b;        //u7
	int reg_middle_s;        //u7
	int reg_max_th1;          //u10
	int reg_middle_th;          //u10
	int reg_thold1;          //u10
	int reg_thold2;          //u10
	int reg_thold3;          //u10
	int reg_thold4;          //u10
	int reg_max_th2;          //u10
	int reg_pnum_th;          //u16
	int reg_hl0;
	int reg_hl1;             //u7
	int reg_hl2;             //u7
	int reg_hl3;             //u7
	int reg_display_adj;     //u7
	int reg_avg_th;
	int reg_avg_adj;
	int reg_low_adj;         //u7
	int reg_high_en;         //u3
	int reg_high_adj1;       //u7
	int reg_high_adj2;       //u7
	int reg_high_maxdiff;    //u7
	int reg_high_mindiff;    //u7
	unsigned int alpha;
	unsigned int *eo_lut;

	/*param for tmo alg*/
	int w;
	int h;
	int *oo_gain;
	void (*pre_hdr10_tmo_alg)(
		struct aml_tmo_reg_sw *reg_tmo,
		u32 *gain,
		int *hist
		);
};

/*adjust for user*/
struct hdr_tmo_sw {
	int tmo_en;              // 0 1
	int reg_highlight;       //u10: control overexposure level
	int reg_hist_th;         //u7
	int reg_light_th;
	int reg_highlight_th1;
	int reg_highlight_th2;
	int reg_display_e;       //u10
	int reg_middle_a;        //u7
	int reg_middle_a_adj;    //u10
	int reg_middle_b;        //u7
	int reg_middle_s;        //u7
	int reg_max_th1;          //u10
	int reg_middle_th;          //u10
	int reg_thold1;          //u10
	int reg_thold2;          //u10
	int reg_thold3;          //u10
	int reg_thold4;          //u10
	int reg_max_th2;          //u10
	int reg_pnum_th;          //u16
	int reg_hl0;
	int reg_hl1;             //u7
	int reg_hl2;             //u7
	int reg_hl3;             //u7
	int reg_display_adj;     //u7
	int reg_avg_th;
	int reg_avg_adj;
	int reg_low_adj;         //u7
	int reg_high_en;         //u3
	int reg_high_adj1;       //u7
	int reg_high_adj2;       //u7
	int reg_high_maxdiff;    //u7
	int reg_high_mindiff;    //u7
	unsigned int alpha;
};

struct aml_tmo_reg_sw *tmo_fw_param_get(void);
#endif
