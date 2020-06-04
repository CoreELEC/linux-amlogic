/*
 * drivers/amlogic/media/di_multi_v3/di_prc.c
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/of_fdt.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include "deinterlace.h"

#include "di_data_l.h"
#include "di_data.h"
#include "di_dbg.h"
#include "di_sys.h"
#include "di_vframe.h"
#include "di_que.h"
#include "di_task.h"

#include "di_prc.h"
#include "di_pre.h"
#include "di_pre_hw.h"
#include "di_post.h"
#include "di_api.h"

/**************************************
 *
 * cfg ctr top
 *	bool
 **************************************/

const struct di_cfg_ctr_s div3_cfg_top_ctr[K_DI_CFG_NUB] = {
	/*same order with enum eDI_DBG_CFG*/
	/* cfg for top */
	[EDI_CFG_BEGIN]  = {"cfg top begin ", EDI_CFG_BEGIN, 0,
			K_DI_CFG_T_FLG_NONE},
	[EDI_CFG_CH_NUB]  = {"ch_nub", EDI_CFG_CH_NUB, 1,
				K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_mem_flg]  = {"flag_cma", EDI_CFG_mem_flg,
				eDI_MEM_M_cma,
				K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_first_bypass]  = {"first_bypass",
				EDI_CFG_first_bypass,
				0,
				K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_ref_2]  = {"ref_2",
		EDI_CFG_ref_2, 0, K_DI_CFG_T_FLG_NOTHING},
	[EDI_CFG_PMODE]  = {"pmode:0:as p;1:as i;2:use 2 i buf",
			EDI_CFG_PMODE,
			1,
			K_DI_CFG_T_FLG_NOTHING},
	[EDI_CFG_KEEP_CLEAR_AUTO]  = {"keep_buf clear auto",
			EDI_CFG_KEEP_CLEAR_AUTO,
			1,
			K_DI_CFG_T_FLG_NOTHING},
	[EDI_CFG_PRE_RST_OLD_FLOW]  = {"pre_rst_old_flow",
			EDI_CFG_PRE_RST_OLD_FLOW,
			0,
			K_DI_CFG_T_FLG_NOTHING},

	[EDI_CFG_TMODE_1]  = {"tmode1",
			EDI_CFG_TMODE_1,
			1,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_TMODE_2]  = {"tmode2",
			EDI_CFG_TMODE_2,
			1,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_TMODE_3]  = {"tmode3",
			EDI_CFG_TMODE_3,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_DBG]  = {"dbg only",
		EDI_CFG_DBG, 0x00, K_DI_CFG_T_FLG_NOTHING},
	[EDI_CFG_END]  = {"cfg top end ", EDI_CFG_END, 0,
			K_DI_CFG_T_FLG_NONE},

};

char *div3_cfg_top_get_name(enum eDI_CFG_TOP_IDX idx)
{
	return div3_cfg_top_ctr[idx].dts_name;
}

void div3_cfg_top_get_info(unsigned int idx, char **name)
{
	if (div3_cfg_top_ctr[idx].id != idx)
		PR_ERR("%s:err:idx not map [%d->%d]\n", __func__,
		       idx, div3_cfg_top_ctr[idx].id);

	*name = div3_cfg_top_ctr[idx].dts_name;
}

bool div3_cfg_top_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(div3_cfg_top_ctr);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (div3_cfg_top_ctr[idx].flg & K_DI_CFG_T_FLG_NONE)
		return false;
	if (idx != div3_cfg_top_ctr[idx].id) {
		PR_ERR("%s:%s:err:not map:%d->%d\n",
		       __func__,
		       div3_cfg_top_ctr[idx].dts_name,
		       idx,
		       div3_cfg_top_ctr[idx].id);
		return false;
	}
	#if 0
	pr_info("\t%-15s=%d\n", div3_cfg_top_ctr[idx].name,
		div3_cfg_top_ctr[idx].default_val);
	#endif
	return true;
}

void div3_cfg_top_init_val(void)
{
	int i;
	union di_cfg_tdata_u *pd;
	const struct di_cfg_ctr_s *pt;

	PR_INF("%s:\n", __func__);
	for (i = EDI_CFG_BEGIN; i < EDI_CFG_END; i++) {
		if (!div3_cfg_top_check(i))
			continue;
		pd = &get_datal()->cfg_en[i];
		pt = &div3_cfg_top_ctr[i];

		/*di_cfg_top_set(i, di_cfg_top_ctr[i].default_val);*/
		pd->d32 = 0;/*clear*/
		pd->b.val_df = pt->default_val;
		pd->b.val_c = pd->b.val_df;
	}
	PR_INF("%s:finish\n", __func__);
}

/*after init*/
void div3_cfg_top_dts(void)
{
	struct platform_device *pdev = getv3_dim_de_devp()->pdev;
	int i;
	union di_cfg_tdata_u *pd;
	const struct di_cfg_ctr_s *pt;
	int ret;
	unsigned int uval;

	if (!pdev) {
		PR_ERR("%s:no pdev\n", __func__);
		return;
	}
	PR_INF("%s\n", __func__);

	for (i = EDI_CFG_BEGIN; i < EDI_CFG_END; i++) {
		if (!div3_cfg_top_check(i))
			continue;
		if (!(div3_cfg_top_ctr[i].flg & K_DI_CFG_T_FLG_DTS))
			continue;

		pd = &get_datal()->cfg_en[i];
		pt = &div3_cfg_top_ctr[i];
		pd->b.dts_en = 1;

		ret = of_property_read_u32(pdev->dev.of_node,
					   pt->dts_name,
					   &uval);
		if (ret)
			continue;
		PR_INF("\t%s:%d\n", pt->dts_name, uval);

		pd->b.dts_have = 1;
		pd->b.val_dts = uval;
		pd->b.val_c = pd->b.val_dts;
	}
	PR_INF("%s end\n", __func__);
}

/*item: one /all; include table/val*/
/*val:  one /all; only  val*/
static void di_cfgt_show_item_one(struct seq_file *s, unsigned int index)
{
	union di_cfg_tdata_u *pd;
	const struct di_cfg_ctr_s *pt;

	if (!div3_cfg_top_check(index))
		return;

	pd = &get_datal()->cfg_en[index];
	pt = &div3_cfg_top_ctr[index];

	seq_printf(s, "id:%2d:%-10s\n", index, pt->dts_name);
	/*tab*/
	seq_printf(s, "\t%-5s:0x%2x[%d]\n",
		   "tdf", pt->default_val, pt->default_val);
	seq_printf(s, "\t%-5s:%d\n",
		   "tdts", pt->flg & K_DI_CFG_T_FLG_DTS);
	/*val*/
	seq_printf(s, "\t%-5s:0x%-4x\n", "d32", pd->d32);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n",
		   "vdf", pd->b.val_df, pd->b.val_df);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n",
		   "vdts", pd->b.val_dts, pd->b.val_dts);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n",
		   "vdbg", pd->b.val_dbg, pd->b.val_dbg);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n", "vc", pd->b.val_c, pd->b.val_c);
	seq_printf(s, "\t%-5s:%d\n", "endts", pd->b.dts_en);
	seq_printf(s, "\t%-5s:%d\n", "hdts", pd->b.dts_have);
	seq_printf(s, "\t%-5s:%d\n", "hdbg", pd->b.dbg_have);
}

void div3_cfgt_show_item_sel(struct seq_file *s)
{
	int i = get_datal()->cfg_sel;

	di_cfgt_show_item_one(s, i);
}

void div3_cfgt_set_sel(unsigned int dbg_mode, unsigned int id)
{
	if (!div3_cfg_top_check(id)) {
		PR_ERR("%s:%d is overflow\n", __func__, id);
		return;
	}

	get_datal()->cfg_sel = id;
	get_datal()->cfg_dbg_mode = dbg_mode;
}

void div3_cfgt_show_item_all(struct seq_file *s)
{
	int i;

	for (i = EDI_CFG_BEGIN; i < EDI_CFG_END; i++)
		di_cfgt_show_item_one(s, i);
}

static void di_cfgt_show_val_one(struct seq_file *s, unsigned int index)
{
	union di_cfg_tdata_u *pd;
	const struct di_cfg_ctr_s *pt;

	if (!div3_cfg_top_check(index))
		return;

	pd = &get_datal()->cfg_en[index];
	pt = &div3_cfg_top_ctr[index];

	seq_printf(s, "id:%2d:%-10s\n", index, pt->dts_name);

	seq_printf(s, "\t%-5s:0x%-4x\n", "d32", pd->d32);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n", "vc", pd->b.val_c, pd->b.val_c);
}

void div3_cfgt_show_val_sel(struct seq_file *s)
{
	unsigned int i = get_datal()->cfg_sel;

	di_cfgt_show_val_one(s, i);
}

void div3_cfgt_show_val_all(struct seq_file *s)
{
	int i;

	for (i = EDI_CFG_BEGIN; i < EDI_CFG_END; i++)
		di_cfgt_show_val_one(s, i);
}

unsigned int div3_cfg_top_get(enum eDI_CFG_TOP_IDX id)
{
	union di_cfg_tdata_u *pd;

	pd = &get_datal()->cfg_en[id];
	return pd->b.val_c;
}

void div3_cfg_top_set(enum eDI_CFG_TOP_IDX id, unsigned int val)
{
	union di_cfg_tdata_u *pd;

	pd = &get_datal()->cfg_en[id];
	pd->b.val_dbg = val;
	pd->b.dbg_have = 1;
	pd->b.val_c = val;
}

/**************************************
 *
 * cfg ctr x
 *	bool
 **************************************/

const struct di_cfgx_ctr_s div3_cfgx_ctr[K_DI_CFGX_NUB] = {
	/*same order with enum eDI_DBG_CFG*/

	/* cfg channel x*/
	[eDI_CFGX_BEGIN]  = {"cfg x begin ", eDI_CFGX_BEGIN, 0},
	/* bypass_all */
	[eDI_CFGX_BYPASS_ALL]  = {"bypass_all", eDI_CFGX_BYPASS_ALL, 0},
	[EDI_CFGX_HOLD_VIDEO]  = {"hold_video", EDI_CFGX_HOLD_VIDEO, 0},
	[eDI_CFGX_END]  = {"cfg x end ", eDI_CFGX_END, 0},

	/* debug cfg x */
	[eDI_DBG_CFGX_BEGIN]  = {"cfg dbg begin ", eDI_DBG_CFGX_BEGIN, 0},
	[eDI_DBG_CFGX_IDX_VFM_IN] = {"vfm_in", eDI_DBG_CFGX_IDX_VFM_IN, 0},
	[eDI_DBG_CFGX_IDX_VFM_OT] = {"vfm_out", eDI_DBG_CFGX_IDX_VFM_OT, 1},
	[eDI_DBG_CFGX_END]    = {"cfg dbg end", eDI_DBG_CFGX_END, 0},
};

char *div3_cfgx_get_name(enum eDI_CFGX_IDX idx)
{
	return div3_cfgx_ctr[idx].name;
}

void div3_cfgx_get_info(enum eDI_CFGX_IDX idx, char **name)
{
	if (div3_cfgx_ctr[idx].id != idx)
		PR_ERR("%s:err:idx not map [%d->%d]\n", __func__,
		       idx, div3_cfgx_ctr[idx].id);

	*name = div3_cfgx_ctr[idx].name;
}

bool div3_cfgx_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(div3_cfgx_ctr);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (idx != div3_cfgx_ctr[idx].id) {
		PR_ERR("%s:err:not map:%d->%d\n",
		       __func__, idx, div3_cfgx_ctr[idx].id);
		return false;
	}
	PR_INF("\t%-15s=%d\n", div3_cfgx_ctr[idx].name,
	       div3_cfgx_ctr[idx].default_val);
	return true;
}

void div3_cfgx_init_val(void)
{
	int i, ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		for (i = eDI_CFGX_BEGIN; i < eDI_DBG_CFGX_END; i++)
			div3_cfgx_set(ch, i, div3_cfgx_ctr[i].default_val);
	}
}

bool div3_cfgx_get(unsigned int ch, enum eDI_CFGX_IDX idx)
{
	return get_datal()->ch_data[ch].cfgx_en[idx];
}

void div3_cfgx_set(unsigned int ch, enum eDI_CFGX_IDX idx, bool en)
{
	get_datal()->ch_data[ch].cfgx_en[idx] = en;
}

/**************************************
 *
 * module para top
 *	int
 **************************************/

const struct di_mp_uit_s div3_mp_ui_top[] = {
	/*same order with enum eDI_MP_UI*/
	/* for top */
	[eDI_MP_UI_T_BEGIN]  = {"module para top begin ",
			eDI_MP_UI_T_BEGIN, 0},
	/**************************************/
	[eDI_MP_SUB_DI_B]  = {"di begin ",
			eDI_MP_SUB_DI_B, 0},
	[eDI_MP_force_prog]  = {"bool:force_prog:1",
			eDI_MP_force_prog, 1},
	[edi_mp_combing_fix_en]  = {"bool:combing_fix_en,def:1",
			edi_mp_combing_fix_en, 1},
	[eDI_MP_cur_lev]  = {"int cur_lev,def:2",
			eDI_MP_cur_lev, 2},
	[eDI_MP_pps_dstw]  = {"pps_dstw:int",
			eDI_MP_pps_dstw, 0},
	[eDI_MP_pps_dsth]  = {"pps_dsth:int",
			eDI_MP_pps_dsth, 0},
	[eDI_MP_pps_en]  = {"pps_en:bool",
			eDI_MP_pps_en, 0},
	[eDI_MP_pps_position]  = {"pps_position:uint:def:1",
			eDI_MP_pps_position, 1},
	[eDI_MP_pre_enable_mask]  = {"pre_enable_mask:bit0:ma;bit1:mc:def:3",
			eDI_MP_pre_enable_mask, 3},
	[eDI_MP_post_refresh]  = {"post_refresh:bool",
			eDI_MP_post_refresh, 0},
	[eDI_MP_nrds_en]  = {"nrds_en:bool",
			eDI_MP_nrds_en, 0},
	[eDI_MP_bypass_3d]  = {"bypass_3d:int:def:1",
			eDI_MP_bypass_3d, 1},
	[eDI_MP_bypass_trick_mode]  = {"bypass_trick_mode:int:def:1",
			eDI_MP_bypass_trick_mode, 1},
	[eDI_MP_invert_top_bot]  = {"invert_top_bot:int",
			eDI_MP_invert_top_bot, 0},
	[eDI_MP_skip_top_bot]  = {"skip_top_bot:int:",
			/*1or2: may affect atv when bypass di*/
			eDI_MP_skip_top_bot, 0},
	[eDI_MP_force_width]  = {"force_width:int",
			eDI_MP_force_width, 0},
	[eDI_MP_force_height]  = {"force_height:int",
			eDI_MP_force_height, 0},
	[eDI_MP_prog_proc_config]  = {"prog_proc_config:int:def:0x23",
/* prog_proc_config,
 * bit[2:1]: when two field buffers are used,
 * 0 use vpp for blending ,
 * 1 use post_di module for blending
 * 2 debug mode, bob with top field
 * 3 debug mode, bot with bot field
 * bit[0]:
 * 0 "prog vdin" use two field buffers,
 * 1 "prog vdin" use single frame buffer
 * bit[4]:
 * 0 "prog frame from decoder/vdin" use two field buffers,
 * 1 use single frame buffer
 * bit[5]:
 * when two field buffers are used for decoder (bit[4] is 0):
 * 1,handle prog frame as two interlace frames
 * bit[6]:(bit[4] is 0,bit[5] is 0,use_2_interlace_buff is 0): 0,
 * process progress frame as field,blend by post;
 * 1, process progress frame as field,process by normal di
 */
			eDI_MP_prog_proc_config, ((1 << 5) | (1 << 1) | 1)},
	[eDI_MP_start_frame_drop_count]  = {"start_frame_drop_count:int:2",
			eDI_MP_start_frame_drop_count, 0}, /*from 2 to 0*/
	[eDI_MP_same_field_top_count]  = {"same_field_top_count:long?",
			eDI_MP_same_field_top_count, 0},
	[eDI_MP_same_field_bot_count]  = {"same_field_bot_count:long?",
			eDI_MP_same_field_bot_count, 0},
	[eDI_MP_vpp_3d_mode]  = {"vpp_3d_mode:int",
			eDI_MP_vpp_3d_mode, 0},
	[eDI_MP_force_recovery_count]  = {"force_recovery_count:uint",
			eDI_MP_force_recovery_count, 0},
	[eDI_MP_pre_process_time]  = {"pre_process_time:int",
			eDI_MP_pre_process_time, 0},
	[eDI_MP_bypass_post]  = {"bypass_post:int",
			eDI_MP_bypass_post, 0},
	[eDI_MP_post_wr_en]  = {"post_wr_en:bool:1",
			eDI_MP_post_wr_en, 1},
	[eDI_MP_post_wr_support]  = {"post_wr_support:uint",
			eDI_MP_post_wr_support, 0},
	[eDI_MP_bypass_post_state]  = {"bypass_post_state:int",
/* 0, use di_wr_buf still;
 * 1, call dim_pre_de_done_buf_clear to clear di_wr_buf;
 * 2, do nothing
 */
			eDI_MP_bypass_post_state, 0},
	[eDI_MP_use_2_interlace_buff]  = {"use_2_interlace_buff:int",
			eDI_MP_use_2_interlace_buff, 0},
	[eDI_MP_debug_blend_mode]  = {"debug_blend_mode:int:-1",
			eDI_MP_debug_blend_mode, -1},
	[eDI_MP_nr10bit_support]  = {"nr10bit_support:uint",
		/* 0: not support nr10bit, 1: support nr10bit */
			eDI_MP_nr10bit_support, 0},
	[eDI_MP_di_stop_reg_flag]  = {"di_stop_reg_flag:uint",
			eDI_MP_di_stop_reg_flag, 0},
	[eDI_MP_mcpre_en]  = {"mcpre_en:bool:true",
			eDI_MP_mcpre_en, 1},
	[eDI_MP_check_start_drop_prog]  = {"check_start_drop_prog:bool",
			eDI_MP_check_start_drop_prog, 0},
	[eDI_MP_overturn]  = {"overturn:bool:?",
			eDI_MP_overturn, 0},
	[eDI_MP_full_422_pack]  = {"full_422_pack:bool",
			eDI_MP_full_422_pack, 0},
	[eDI_MP_cma_print]  = {"cma_print:bool:1",
			eDI_MP_cma_print, 0},
	[eDI_MP_pulldown_enable]  = {"pulldown_enable:bool:1",
			eDI_MP_pulldown_enable, 1},
	[eDI_MP_di_force_bit_mode]  = {"di_force_bit_mode:uint:10",
			eDI_MP_di_force_bit_mode, 10},
	[eDI_MP_calc_mcinfo_en]  = {"calc_mcinfo_en:bool:1",
			eDI_MP_calc_mcinfo_en, 1},
	[eDI_MP_colcfd_thr]  = {"colcfd_thr:uint:128",
			eDI_MP_colcfd_thr, 128},
	[eDI_MP_post_blend]  = {"post_blend:uint",
			eDI_MP_post_blend, 0},
	[eDI_MP_post_ei]  = {"post_ei:uint",
			eDI_MP_post_ei, 0},
	[eDI_MP_post_cnt]  = {"post_cnt:uint",
			eDI_MP_post_cnt, 0},
	[eDI_MP_di_log_flag]  = {"di_log_flag:uint",
			eDI_MP_di_log_flag, 0},
	[eDI_MP_di_debug_flag]  = {"di_debug_flag:uint",
			eDI_MP_di_debug_flag, 0},
	[eDI_MP_buf_state_log_threshold]  = {"buf_state_log_threshold:unit:16",
			eDI_MP_buf_state_log_threshold, 16},
	[eDI_MP_di_vscale_skip_enable]  = {"di_vscale_skip_enable:int",
/*
 * bit[2]: enable bypass all when skip
 * bit[1:0]: enable bypass post when skip
 */
			eDI_MP_di_vscale_skip_enable, 0},
	[eDI_MP_di_vscale_skip_count]  = {"di_vscale_skip_count:int",
			eDI_MP_di_vscale_skip_count, 0},
	[eDI_MP_di_vscale_skip_count_real]  = {"di_vscale_skip_count_real:int",
			eDI_MP_di_vscale_skip_count_real, 0},
	[eDI_MP_det3d_en]  = {"det3d_en:bool",
			eDI_MP_det3d_en, 0},
	[eDI_MP_post_hold_line]  = {"post_hold_line:int:8",
			eDI_MP_post_hold_line, 8},
	[eDI_MP_post_urgent]  = {"post_urgent:int:1",
			eDI_MP_post_urgent, 1},
	[eDI_MP_di_printk_flag]  = {"di_printk_flag:uint",
			eDI_MP_di_printk_flag, 0},
	[eDI_MP_force_recovery]  = {"force_recovery:uint:1",
			eDI_MP_force_recovery, 1},
#if 0
	[eDI_MP_debug_blend_mode]  = {"debug_blend_mode:int:-1",
			eDI_MP_debug_blend_mode, -1},
#endif
	[eDI_MP_di_dbg_mask]  = {"di_dbg_mask:uint:0x02",
			eDI_MP_di_dbg_mask, 2},
	[eDI_MP_nr_done_check_cnt]  = {"nr_done_check_cnt:uint:5",
			eDI_MP_nr_done_check_cnt, 5},
	[eDI_MP_pre_hsc_down_en]  = {"pre_hsc_down_en:bool:0",
			eDI_MP_pre_hsc_down_en, 0},
	[eDI_MP_pre_hsc_down_width]  = {"pre_hsc_down_width:int:480",
				eDI_MP_pre_hsc_down_width, 480},
	[eDI_MP_show_nrwr]  = {"show_nrwr:bool:0",
				eDI_MP_show_nrwr, 0},

	/******deinterlace_hw.c**********/
	[eDI_MP_pq_load_dbg]  = {"pq_load_dbg:uint",
			eDI_MP_pq_load_dbg, 0},
	[eDI_MP_lmv_lock_win_en]  = {"lmv_lock_win_en:bool",
			eDI_MP_lmv_lock_win_en, 0},
	[eDI_MP_lmv_dist]  = {"lmv_dist:short:5",
			eDI_MP_lmv_dist, 5},
	[eDI_MP_pr_mcinfo_cnt]  = {"pr_mcinfo_cnt:ushort",
			eDI_MP_pr_mcinfo_cnt, 0},
	[eDI_MP_offset_lmv]  = {"offset_lmv:short:100",
			eDI_MP_offset_lmv, 100},
	[eDI_MP_post_ctrl]  = {"post_ctrl:uint",
			eDI_MP_post_ctrl, 0},
	[eDI_MP_if2_disable]  = {"if2_disable:bool",
			eDI_MP_if2_disable, 0},
	[eDI_MP_pre_flag]  = {"pre_flag:ushort:2",
			eDI_MP_pre_flag, 2},
	[eDI_MP_pre_mif_gate]  = {"pre_mif_gate:bool",
			eDI_MP_pre_mif_gate, 0},
	[eDI_MP_pre_urgent]  = {"pre_urgent:ushort",
			eDI_MP_pre_urgent, 0},
	[eDI_MP_pre_hold_line]  = {"pre_hold_line:ushort:10",
			eDI_MP_pre_hold_line, 10},
	[eDI_MP_pre_ctrl]  = {"pre_ctrl:uint",
			eDI_MP_pre_ctrl, 0},
	[eDI_MP_line_num_post_frst]  = {"line_num_post_frst:ushort:5",
			eDI_MP_line_num_post_frst, 5},
	[eDI_MP_line_num_pre_frst]  = {"line_num_pre_frst:ushort:5",
			eDI_MP_line_num_pre_frst, 5},
	[eDI_MP_pd22_flg_calc_en]  = {"pd22_flg_calc_en:bool:true",
			eDI_MP_pd22_flg_calc_en, 1},
	[eDI_MP_mcen_mode]  = {"mcen_mode:ushort:1",
			eDI_MP_mcen_mode, 1},
	[eDI_MP_mcuv_en]  = {"mcuv_en:ushort:1",
			eDI_MP_mcuv_en, 1},
	[eDI_MP_mcdebug_mode]  = {"mcdebug_mode:ushort",
			eDI_MP_mcdebug_mode, 0},
	[eDI_MP_pldn_ctrl_rflsh]  = {"pldn_ctrl_rflsh:uint:1",
			eDI_MP_pldn_ctrl_rflsh, 1},
	[eDI_MP_4k_test]  = {"4_k_input:uint:1",
				eDI_MP_4k_test, 0},

	[eDI_MP_SUB_DI_E]  = {"di end-------",
				eDI_MP_SUB_DI_E, 0},
	/**************************************/
	[eDI_MP_SUB_NR_B]  = {"nr begin",
			eDI_MP_SUB_NR_B, 0},
	[EDI_MP_SUB_DELAY]  = {"delay:bool:true",
			EDI_MP_SUB_DELAY, 0},
	[EDI_MP_SUB_DBG_MODE]  = {"delay_mode:uint:0",
			EDI_MP_SUB_DBG_MODE, 0},
	[eDI_MP_cue_en]  = {"cue_en:bool:true",
			eDI_MP_cue_en, 1},
	[eDI_MP_invert_cue_phase]  = {"invert_cue_phase:bool",
			eDI_MP_invert_cue_phase, 0},
	[eDI_MP_cue_pr_cnt]  = {"cue_pr_cnt:uint",
			eDI_MP_cue_pr_cnt, 0},
	[eDI_MP_cue_glb_mot_check_en]  = {"cue_glb_mot_check_en:bool:true",
			eDI_MP_cue_glb_mot_check_en, 1},
	[eDI_MP_glb_fieldck_en]  = {"glb_fieldck_en:bool:true",
			eDI_MP_glb_fieldck_en, 1},
	[eDI_MP_dnr_pr]  = {"dnr_pr:bool",
			eDI_MP_dnr_pr, 0},
	[eDI_MP_dnr_dm_en]  = {"dnr_dm_en:bool",
			eDI_MP_dnr_dm_en, 0},
	[eDI_MP_SUB_NR_E]  = {"nr end-------",
			eDI_MP_SUB_NR_E, 0},
	/**************************************/
	[eDI_MP_SUB_PD_B]  = {"pd begin",
			eDI_MP_SUB_PD_B, 0},
	[eDI_MP_flm22_ratio]  = {"flm22_ratio:uint:200",
			eDI_MP_flm22_ratio, 200},
	[eDI_MP_pldn_cmb0]  = {"pldn_cmb0:uint:1",
			eDI_MP_pldn_cmb0, 1},
	[eDI_MP_pldn_cmb1]  = {"pldn_cmb1:uint",
			eDI_MP_pldn_cmb1, 0},
	[eDI_MP_flm22_sure_num]  = {"flm22_sure_num:uint:100",
			eDI_MP_flm22_sure_num, 100},
	[eDI_MP_flm22_glbpxlnum_rat]  = {"flm22_glbpxlnum_rat:uint:4",
			eDI_MP_flm22_glbpxlnum_rat, 4},
	[eDI_MP_flag_di_weave]  = {"flag_di_weave:int:1",
			eDI_MP_flag_di_weave, 1},
	[eDI_MP_flm22_glbpxl_maxrow]  = {"flm22_glbpxl_maxrow:uint:16",
			eDI_MP_flm22_glbpxl_maxrow, 16},
	[eDI_MP_flm22_glbpxl_minrow]  = {"flm22_glbpxl_minrow:uint:3",
			eDI_MP_flm22_glbpxl_minrow, 3},
	[eDI_MP_cmb_3point_rnum]  = {"cmb_3point_rnum:uint",
			eDI_MP_cmb_3point_rnum, 0},
	[eDI_MP_cmb_3point_rrat]  = {"cmb_3point_rrat:unit:32",
			eDI_MP_cmb_3point_rrat, 32},
	/********************************/
	[eDI_MP_pr_pd]  = {"pr_pd:uint",
			eDI_MP_pr_pd, 0},
	[eDI_MP_prt_flg]  = {"prt_flg:bool",
			eDI_MP_prt_flg, 0},
	[eDI_MP_flmxx_maybe_num]  = {"flmxx_maybe_num:uint:15",
	/* if flmxx level > flmxx_maybe_num */
	/* mabye flmxx: when 2-2 3-2 not detected */
			eDI_MP_flmxx_maybe_num, 15},
	[eDI_MP_flm32_mim_frms]  = {"flm32_mim_frms:int:6",
			eDI_MP_flm32_mim_frms, 6},
	[eDI_MP_flm22_dif01a_flag]  = {"flm22_dif01a_flag:int:1",
			eDI_MP_flm22_dif01a_flag, 1},
	[eDI_MP_flm22_mim_frms]  = {"flm22_mim_frms:int:60",
			eDI_MP_flm22_mim_frms, 60},
	[eDI_MP_flm22_mim_smfrms]  = {"flm22_mim_smfrms:int:40",
			eDI_MP_flm22_mim_smfrms, 40},
	[eDI_MP_flm32_f2fdif_min0]  = {"flm32_f2fdif_min0:int:11",
			eDI_MP_flm32_f2fdif_min0, 11},
	[eDI_MP_flm32_f2fdif_min1]  = {"flm32_f2fdif_min1:int:11",
			eDI_MP_flm32_f2fdif_min1, 11},
	[eDI_MP_flm32_chk1_rtn]  = {"flm32_chk1_rtn:int:25",
			eDI_MP_flm32_chk1_rtn, 25},
	[eDI_MP_flm32_ck13_rtn]  = {"flm32_ck13_rtn:int:8",
			eDI_MP_flm32_ck13_rtn, 8},
	[eDI_MP_flm32_chk2_rtn]  = {"flm32_chk2_rtn:int:16",
			eDI_MP_flm32_chk2_rtn, 16},
	[eDI_MP_flm32_chk3_rtn]  = {"flm32_chk3_rtn:int:16",
			eDI_MP_flm32_chk3_rtn, 16},
	[eDI_MP_flm32_dif02_ratio]  = {"flm32_dif02_ratio:int:8",
			eDI_MP_flm32_dif02_ratio, 8},
	[eDI_MP_flm22_chk20_sml]  = {"flm22_chk20_sml:int:6",
			eDI_MP_flm22_chk20_sml, 6},
	[eDI_MP_flm22_chk21_sml]  = {"flm22_chk21_sml:int:6",
			eDI_MP_flm22_chk21_sml, 6},
	[eDI_MP_flm22_chk21_sm2]  = {"flm22_chk21_sm2:int:10",
			eDI_MP_flm22_chk21_sm2, 10},
	[eDI_MP_flm22_lavg_sft]  = {"flm22_lavg_sft:int:4",
			eDI_MP_flm22_lavg_sft, 4},
	[eDI_MP_flm22_lavg_lg]  = {"flm22_lavg_lg:int:24",
			eDI_MP_flm22_lavg_lg, 24},
	[eDI_MP_flm22_stl_sft]  = {"flm22_stl_sft:int:7",
			eDI_MP_flm22_stl_sft, 7},
	[eDI_MP_flm22_chk5_avg]  = {"flm22_chk5_avg:int:50",
			eDI_MP_flm22_chk5_avg, 50},
	[eDI_MP_flm22_chk6_max]  = {"flm22_chk6_max:int:20",
			eDI_MP_flm22_chk6_max, 20},
	[eDI_MP_flm22_anti_chk1]  = {"flm22_anti_chk1:int:61",
			eDI_MP_flm22_anti_chk1, 61},
	[eDI_MP_flm22_anti_chk3]  = {"flm22_anti_chk3:int:140",
			eDI_MP_flm22_anti_chk3, 140},
	[eDI_MP_flm22_anti_chk4]  = {"flm22_anti_chk4:int:128",
			eDI_MP_flm22_anti_chk4, 128},
	[eDI_MP_flm22_anti_ck140]  = {"flm22_anti_ck140:int:32",
			eDI_MP_flm22_anti_ck140, 32},
	[eDI_MP_flm22_anti_ck141]  = {"flm22_anti_ck141:int:80",
			eDI_MP_flm22_anti_ck141, 80},
	[eDI_MP_flm22_frmdif_max]  = {"flm22_frmdif_max:int:50",
			eDI_MP_flm22_frmdif_max, 50},
	[eDI_MP_flm22_flddif_max]  = {"flm22_flddif_max:int:100",
			eDI_MP_flm22_flddif_max, 100},
	[eDI_MP_flm22_minus_cntmax]  = {"flm22_minus_cntmax:int:2",
			eDI_MP_flm22_minus_cntmax, 2},
	[eDI_MP_flagdif01chk]  = {"flagdif01chk:int:1",
			eDI_MP_flagdif01chk, 1},
	[eDI_MP_dif01_ratio]  = {"dif01_ratio:int:10",
			eDI_MP_dif01_ratio, 10},
	/*************vof_soft_top**************/
	[eDI_MP_cmb32_blw_wnd]  = {"cmb32_blw_wnd:int:180",
			eDI_MP_cmb32_blw_wnd, 180},
	[eDI_MP_cmb32_wnd_ext]  = {"cmb32_wnd_ext:int:11",
			eDI_MP_cmb32_wnd_ext, 11},
	[eDI_MP_cmb32_wnd_tol]  = {"cmb32_wnd_tol:int:4",
			eDI_MP_cmb32_wnd_tol, 4},
	[eDI_MP_cmb32_frm_nocmb]  = {"cmb32_frm_nocmb:int:40",
			eDI_MP_cmb32_frm_nocmb, 40},
	[eDI_MP_cmb32_min02_sft]  = {"cmb32_min02_sft:int:7",
			eDI_MP_cmb32_min02_sft, 7},
	[eDI_MP_cmb32_cmb_tol]  = {"cmb32_cmb_tol:int:10",
			eDI_MP_cmb32_cmb_tol, 10},
	[eDI_MP_cmb32_avg_dff]  = {"cmb32_avg_dff:int:48",
			eDI_MP_cmb32_avg_dff, 48},
	[eDI_MP_cmb32_smfrm_num]  = {"cmb32_smfrm_num:int:4",
			eDI_MP_cmb32_smfrm_num, 4},
	[eDI_MP_cmb32_nocmb_num]  = {"cmb32_nocmb_num:int:20",
			eDI_MP_cmb32_nocmb_num, 20},
	[eDI_MP_cmb22_gcmb_rnum]  = {"cmb22_gcmb_rnum:int:8",
			eDI_MP_cmb22_gcmb_rnum, 8},
	[eDI_MP_flmxx_cal_lcmb]  = {"flmxx_cal_lcmb:int:1",
			eDI_MP_flmxx_cal_lcmb, 1},
	[eDI_MP_flm2224_stl_sft]  = {"flm2224_stl_sft:int:7",
			eDI_MP_flm2224_stl_sft, 7},
	[eDI_MP_SUB_PD_E]  = {"pd end------",
			eDI_MP_SUB_PD_E, 0},
	/**************************************/
	[eDI_MP_SUB_MTN_B]  = {"mtn begin",
			eDI_MP_SUB_MTN_B, 0},
	[eDI_MP_force_lev]  = {"force_lev:int:0xff",
			eDI_MP_force_lev, 0xff},
	[eDI_MP_dejaggy_flag]  = {"dejaggy_flag:int:-1",
			eDI_MP_dejaggy_flag, -1},
	[eDI_MP_dejaggy_enable]  = {"dejaggy_enable:int:1",
			eDI_MP_dejaggy_enable, 1},
	[eDI_MP_cmb_adpset_cnt]  = {"cmb_adpset_cnt:int",
			eDI_MP_cmb_adpset_cnt, 0},
	[eDI_MP_cmb_num_rat_ctl4]  = {"cmb_num_rat_ctl4:int:64:0~255",
			eDI_MP_cmb_num_rat_ctl4, 64},
	[eDI_MP_cmb_rat_ctl4_minthd]  = {"cmb_rat_ctl4_minthd:int:64",
			eDI_MP_cmb_rat_ctl4_minthd, 64},
	[eDI_MP_small_local_mtn]  = {"small_local_mtn:uint:70",
			eDI_MP_small_local_mtn, 70},
	[eDI_MP_di_debug_readreg]  = {"di_debug_readreg:int",
			eDI_MP_di_debug_readreg, 0},
	[eDI_MP_SUB_MTN_E]  = {"mtn end----",
			eDI_MP_SUB_MTN_E, 0},
	/**************************************/
	[eDI_MP_SUB_3D_B]  = {"3d begin",
			eDI_MP_SUB_3D_B, 0},
	[eDI_MP_chessbd_vrate]  = {"chessbd_vrate:int:29",
				eDI_MP_chessbd_vrate, 29},
	[eDI_MP_det3d_debug]  = {"det3d_debug:bool:0",
				eDI_MP_det3d_debug, 0},
	[eDI_MP_SUB_3D_E]  = {"3d begin",
				eDI_MP_SUB_3D_E, 0},

	/**************************************/
	[eDI_MP_UI_T_END]  = {"module para top end ", eDI_MP_UI_T_END, 0}
};

bool div3_mp_uit_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(div3_mp_ui_top);
	if (idx >= tsize) {
		PR_ERR("div3_mp_uit_check:err:overflow:%d->%d\n",
		       idx, tsize);
		return false;
	}
	if (idx != div3_mp_ui_top[idx].id) {
		PR_ERR("%s:err:not map:%d->%d\n",
		       __func__, idx, div3_mp_ui_top[idx].id);
		return false;
	}
	dbg_init("\t%-15s=%d\n", div3_mp_ui_top[idx].name,
		 div3_mp_ui_top[idx].default_val);
	return true;
}

char *div3_mp_uit_get_name(enum eDI_MP_UI_T idx)
{
	return div3_mp_ui_top[idx].name;
}

void div3_mp_uit_init_val(void)
{
	int i;

	for (i = eDI_MP_UI_T_BEGIN; i < eDI_MP_UI_T_END; i++) {
		if (!div3_mp_uit_check(i))
			continue;
		dimp_set(i, div3_mp_ui_top[i].default_val);
	}
}
#if 0
int di_mp_uit_get(enum eDI_MP_UI_T idx)
{
	return get_datal()->mp_uit[idx];
}

void di_mp_uit_set(enum eDI_MP_UI_T idx, int val)
{
	get_datal()->mp_uit[idx] = val;
}
#endif

/**************************************
 *
 * module para x
 *	unsigned int
 **************************************/

const struct di_mp_uix_s div3_mpx[] = {
	/*same order with enum eDI_MP_UI*/

	/* module para for channel x*/
	[eDI_MP_UIX_BEGIN]  = {"module para x begin ", eDI_MP_UIX_BEGIN, 0},
	/*debug:	run_flag*/
	[eDI_MP_UIX_RUN_FLG]  = {"run_flag(0:run;1:pause;2:step)",
				eDI_MP_UIX_RUN_FLG, DI_RUN_FLAG_RUN},
	[eDI_MP_UIX_END]  = {"module para x end ", eDI_MP_UIX_END, 0}

};

bool div3_mp_uix_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(div3_mpx);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (idx != div3_mpx[idx].id) {
		PR_ERR("%s:err:not map:%d->%d\n",
		       __func__, idx, div3_mpx[idx].id);
		return false;
	}
	dbg_init("\t%-15s=%d\n", div3_mpx[idx].name, div3_mpx[idx].default_val);

	return true;
}

char *div3_mp_uix_get_name(enum eDI_MP_UIX_T idx)
{
	return div3_mpx[idx].name;
}

void div3_mp_uix_init_val(void)
{
	int i, ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		dbg_init("%s:ch[%d]\n", __func__, ch);
		for (i = eDI_MP_UIX_BEGIN; i < eDI_MP_UIX_END; i++) {
			if (ch == 0) {
				if (!div3_mp_uix_check(i))
					continue;
			}
			div3_mp_uix_set(ch, i, div3_mpx[i].default_val);
		}
	}
}

unsigned int div3_mp_uix_get(unsigned int ch, enum eDI_MP_UIX_T idx)
{
	return get_datal()->ch_data[ch].mp_uix[idx];
}

void div3_mp_uix_set(unsigned int ch, enum eDI_MP_UIX_T idx, unsigned int val)
{
	get_datal()->ch_data[ch].mp_uix[idx] = val;
}

bool div3_is_pause(unsigned int ch)
{
	unsigned int run_flag;

	run_flag = div3_mp_uix_get(ch, eDI_MP_UIX_RUN_FLG);

	if (run_flag == DI_RUN_FLAG_PAUSE	||
	    run_flag == DI_RUN_FLAG_STEP_DONE)
		return true;

	return false;
}

void div3_pause_step_done(unsigned int ch)
{
	unsigned int run_flag;

	run_flag = div3_mp_uix_get(ch, eDI_MP_UIX_RUN_FLG);
	if (run_flag == DI_RUN_FLAG_STEP) {
		div3_mp_uix_set(ch, eDI_MP_UIX_RUN_FLG,
			      DI_RUN_FLAG_STEP_DONE);
	}
}

void div3_pause(unsigned int ch, bool on)
{
	PR_INF("%s:%d\n", __func__, on);
	if (on)
		div3_mp_uix_set(ch, eDI_MP_UIX_RUN_FLG,
			      DI_RUN_FLAG_PAUSE);
	else
		div3_mp_uix_set(ch, eDI_MP_UIX_RUN_FLG,
			      DI_RUN_FLAG_RUN);
}

/**************************************
 *
 * summmary variable
 *
 **************************************/
const struct di_sum_s div3_sum_tab[] = {
	/*video_peek_cnt*/
	[eDI_SUM_O_PEEK_CNT] = {"o_peek_cnt", eDI_SUM_O_PEEK_CNT, 0},
	/*di_reg_unreg_cnt*/
	[eDI_SUM_REG_UNREG_CNT] = {
			"di_reg_unreg_cnt", eDI_SUM_REG_UNREG_CNT, 100},

	[eDI_SUM_NUB] = {"end", eDI_SUM_NUB, 0}
};

unsigned int div3_sum_get_tab_size(void)
{
	return ARRAY_SIZE(div3_sum_tab);
}

bool div3_sum_check(unsigned int ch, enum eDI_SUM id)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(div3_sum_tab);

	if (id >= tsize) {
		PR_ERR("%s:err:overflow:tsize[%d],id[%d]\n",
		       __func__, tsize, id);
		return false;
	}
	if (div3_sum_tab[id].index != id) {
		PR_ERR("%s:err:table:id[%d],tab_id[%d]\n",
		       __func__, id, div3_sum_tab[id].index);
		return false;
	}
	return true;
}

void div3_sum_reg_init(unsigned int ch)
{
	unsigned int tsize;
	int i;

	tsize = ARRAY_SIZE(div3_sum_tab);

	dbg_init("%s:ch[%d]\n", __func__, ch);
	for (i = 0; i < tsize; i++) {
		if (!div3_sum_check(ch, i))
			continue;
		dbg_init("\t:%d:name:%s,%d\n", i, div3_sum_tab[i].name,
			 div3_sum_tab[i].default_val);
		di_sum_set_l(ch, i, div3_sum_tab[i].default_val);
	}
}

void div3_sum_get_info(unsigned int ch,  enum eDI_SUM id, char **name,
		     unsigned int *pval)
{
	*name = div3_sum_tab[id].name;
	*pval = div3_sum_get(ch, id);
}

void div3_sum_set(unsigned int ch, enum eDI_SUM id, unsigned int val)
{
	if (!div3_sum_check(ch, id))
		return;

	di_sum_set_l(ch, id, val);
}

unsigned int div3_sum_inc(unsigned int ch, enum eDI_SUM id)
{
	if (!div3_sum_check(ch, id))
		return 0;
	return di_sum_inc_l(ch, id);
}

unsigned int div3_sum_get(unsigned int ch, enum eDI_SUM id)
{
	if (!div3_sum_check(ch, id))
		return 0;
	return di_sum_get_l(ch, id);
}

/**********************************/
void dimv3_sumx_clear(unsigned int ch)
{
	struct dim_sum_s *psumx = get_sumx(ch);

	memset(psumx, 0, sizeof(*psumx));
}

void dimv3_sumx_set(unsigned int ch)
{
	struct dim_sum_s *psumx = get_sumx(ch);
	struct di_mm_s *mm = dim_mm_get(ch);
	unsigned int pst_kee;

	psumx->b_pre_free	= listv3_count(ch, QUEUE_LOCAL_FREE);
	psumx->b_pre_ready	= div3_que_list_count(ch, QUE_PRE_READY);
	psumx->b_pst_free	= div3_que_list_count(ch, QUE_POST_FREE);
	psumx->b_pst_ready	= div3_que_list_count(ch, QUE_POST_READY);
	psumx->b_recyc		= listv3_count(ch, QUEUE_RECYCLE);
	psumx->b_display	= listv3_count(ch, QUEUE_DISPLAY);

	pst_kee	= div3_que_list_count(ch, QUE_POST_KEEP);

	if (pst_kee &&
	    (pst_kee + psumx->b_display) > (mm->cfg.num_post - 3))
		taskv3_send_cmd(LCMD1(ECMD_RL_KEEP_ALL, ch));
}

/****************************/
/*call by event*/
/****************************/
void dipv3_even_reg_init_val(unsigned int ch)
{
}

void dipv3_even_unreg_val(unsigned int ch)
{
}

/****************************/
static void dip_cma_init_val(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		/* CMA state */
		atomic_set(&pbm->cma_mem_state[ch], EDI_CMA_ST_IDL);

		/* CMA reg/unreg cmd */
		pbm->cma_reg_cmd[ch] = 0;
	}
}

void dipv3_cma_close(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();

	if (dipv3_cma_st_is_idl_all())
		return;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		if (!dipv3_cma_st_is_idle(ch)) {
			dimv3_cma_top_release(ch);
			PR_INF("%s:force release ch[%d]", __func__, ch);
			atomic_set(&pbm->cma_mem_state[ch], EDI_CMA_ST_IDL);

			pbm->cma_reg_cmd[ch] = 0;
		}
	}
}

static void dip_wq_cma_handler(struct work_struct *work)
{
	struct di_mng_s *pbm = get_bufmng();
	enum eDI_CMA_ST cma_st;
	bool do_flg;
	struct dim_wq_s *wq = container_of(work, struct dim_wq_s, wq_work);

	unsigned int ch = wq->ch;

	do_flg = false;
	cma_st = dipv3_cma_get_st(ch);
	dbg_wq("%s:ch[%d],cmd[%d],st[%d]\n",
	       __func__, ch, pbm->cma_reg_cmd[ch], cma_st);
	switch (cma_st) {
	case EDI_CMA_ST_IDL:
		if (pbm->cma_reg_cmd[ch]) {
			do_flg = true;
			/*set:alloc:*/
			atomic_set(&pbm->cma_mem_state[ch], EDI_CMA_ST_ALLOC);
			if (dimv3_cma_top_alloc(ch)) {
				atomic_set(&pbm->cma_mem_state[ch],
					   EDI_CMA_ST_READY);
			}
		}
		break;
	case EDI_CMA_ST_READY:

		if (!pbm->cma_reg_cmd[ch]) {
			do_flg = true;
			atomic_set(&pbm->cma_mem_state[ch],
				   EDI_CMA_ST_RELEASE);
			dimv3_cma_top_release(ch);
			if (div3_que_is_empty(ch, QUE_POST_KEEP))
				atomic_set(&pbm->cma_mem_state[ch],
					   EDI_CMA_ST_IDL);
			else
				atomic_set(&pbm->cma_mem_state[ch],
					   EDI_CMA_ST_PART);
		}
		break;
	case EDI_CMA_ST_PART:
		if (pbm->cma_reg_cmd[ch]) {
			do_flg = true;
			/*set:alloc:*/
			atomic_set(&pbm->cma_mem_state[ch], EDI_CMA_ST_ALLOC);
			if (dimv3_cma_top_alloc(ch)) {
				atomic_set(&pbm->cma_mem_state[ch],
					   EDI_CMA_ST_READY);
			}
		} else {
			do_flg = true;
			atomic_set(&pbm->cma_mem_state[ch],
				   EDI_CMA_ST_RELEASE);
			dimv3_cma_top_release(ch);
			if (div3_que_is_empty(ch, QUE_POST_KEEP))
				atomic_set(&pbm->cma_mem_state[ch],
					   EDI_CMA_ST_IDL);
			else
				atomic_set(&pbm->cma_mem_state[ch],
					   EDI_CMA_ST_PART);

		}

		break;
	case EDI_CMA_ST_ALLOC:	/*do*/
	case EDI_CMA_ST_RELEASE:/*do*/
	default:
		break;
	}
	if (!do_flg)
		PR_INF("\tch[%d],do nothing[%d]\n", ch, cma_st);
	else
		taskv3_send_ready();

	dbg_wq("%s:end\n", __func__);
}

static void dip_wq_prob(void)
{
	struct di_mng_s *pbm = get_bufmng();

	pbm->wq.wq_cma = create_singlethread_workqueue("deinterlace");
	INIT_WORK(&pbm->wq.wq_work, dip_wq_cma_handler);
}

static void dip_wq_ext(void)
{
	struct di_mng_s *pbm = get_bufmng();

	cancel_work_sync(&pbm->wq.wq_work);
	destroy_workqueue(pbm->wq.wq_cma);
	pr_info("%s:finish\n", __func__);
}

void dipv3_wq_cma_run(unsigned char ch, bool reg_cmd)
{
	struct di_mng_s *pbm = get_bufmng();

	dbg_wq("%s:ch[%d] [%d]\n", __func__, ch, reg_cmd);
	if (reg_cmd)
		pbm->cma_reg_cmd[ch] = 1;
	else
		pbm->cma_reg_cmd[ch] = 0;

	pbm->wq.ch = ch;
	queue_work(pbm->wq.wq_cma, &pbm->wq.wq_work);
}

bool dipv3_cma_st_is_ready(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();
	bool ret = false;

	if (atomic_read(&pbm->cma_mem_state[ch]) == EDI_CMA_ST_READY)
		ret = true;

	return ret;
}

bool dipv3_cma_st_is_idle(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();
	bool ret = false;

	if (atomic_read(&pbm->cma_mem_state[ch]) == EDI_CMA_ST_IDL)
		ret = true;

	return ret;
}

bool dipv3_cma_st_is_idl_all(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();
	bool ret = true;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		if (atomic_read(&pbm->cma_mem_state[ch]) != EDI_CMA_ST_IDL) {
			ret = true;
			break;
		}
	}
	return ret;
}

enum eDI_CMA_ST dipv3_cma_get_st(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();

	return atomic_read(&pbm->cma_mem_state[ch]);
}

const char * const div3_cma_state_name[] = {
	"IDLE",
	"do_alloc",
	"READY",
	"do_release",
	"PART"
};

const char *div3_cma_dbg_get_st_name(unsigned int ch)
{
	enum eDI_CMA_ST st = dipv3_cma_get_st(ch);
	const char *p = "overflow";

	if (st < ARRAY_SIZE(div3_cma_state_name))
		p = div3_cma_state_name[st];
	return p;
}

void dipv3_cma_st_set_ready_all(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++)
		atomic_set(&pbm->cma_mem_state[ch], EDI_CMA_ST_READY);
}

/****************************/
/*channel STATE*/
/****************************/
void dipv3_chst_set(unsigned int ch, enum EDI_TOP_STATE chst)
{
	struct di_mng_s *pbm = get_bufmng();

	atomic_set(&pbm->ch_state[ch], chst);
}

enum EDI_TOP_STATE dipv3_chst_get(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();

	return atomic_read(&pbm->ch_state[ch]);
}

void dipv3_chst_init(void)
{
	unsigned int ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++)
		dipv3_chst_set(ch, EDI_TOP_STATE_IDLE);
}

int dipv3_event_reg_chst(unsigned int ch)
{
	enum EDI_TOP_STATE chst;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	bool err_flg = false;
	bool ret = 0;
	struct di_ch_s		*pch;

	dbg_dbg("%s:ch[%d]\n", __func__, ch);

	if (get_reg_flag(ch)) {
		PR_ERR("no muti instance.\n");
		return -1;
	}
	pch = get_chdata(ch);
	chst = dipv3_chst_get(ch);
#if 0	/*move*/
	if (chst > eDI_TOP_STATE_NOPROB)
		set_flag_trig_unreg(ch, false);
#endif
	switch (chst) {
	case EDI_TOP_STATE_IDLE:

		queuev3_init2(ch);
		div3_que_init(ch);
		/*di_vframe_reg(ch);*/
		#if 1
		/*need call below two function before dvfm_fill_in*/
		dimv3_reg_cfg_sys(pch);
		dipv3_init_value_reg(ch);
		#endif
		dipv3_chst_set(ch, eDI_TOP_STATE_REG_STEP1);
		dimv3_htr_start(ch);
		taskv3_send_cmd(LCMD1(eCMD_REG, ch));
		dbg_dbg("reg ok\n");
		break;
	case eDI_TOP_STATE_REG_STEP1:
	case eDI_TOP_STATE_REG_STEP1_P1:
	case eDI_TOP_STATE_REG_STEP2:
	case EDI_TOP_STATE_READY:
	case eDI_TOP_STATE_BYPASS:
		PR_WARN("have reg\n");
		ret = false;
		break;
	case eDI_TOP_STATE_UNREG_STEP1:
	case eDI_TOP_STATE_UNREG_STEP2:
		/*wait*/
		ppre->reg_req_flag_cnt = 0;
		while (dipv3_chst_get(ch) != EDI_TOP_STATE_IDLE) {
			usleep_range(10000, 10001);
			if (ppre->reg_req_flag_cnt++ >
				dimv3_get_reg_unreg_cnt()) {
				dimv3_reg_timeout_inc();
				PR_ERR("%s,ch[%d] reg timeout!!!\n",
				       __func__, ch);
				err_flg = true;
				ret = false;
				break;
			}
		}
		if (!err_flg) {
		/*same as IDLE*/
			queuev3_init2(ch);
			div3_que_init(ch);
			/*di_vframe_reg(ch);*/
			#if 1
			/*need call below two function before dvfm_fill_in*/
			dimv3_reg_cfg_sys(pch);
			dipv3_init_value_reg(ch);
			#endif

			dipv3_chst_set(ch, eDI_TOP_STATE_REG_STEP1);
			taskv3_send_cmd(LCMD1(eCMD_REG, ch));
			dbg_dbg("reg retry ok\n");
		}
		break;
	case eDI_TOP_STATE_NOPROB:
	default:
		ret = false;
		PR_ERR("err: not prob[%d]\n", chst);

		break;
	}
	dbg_ev("ch[%d]:reg end\n", ch);

	return ret;
}

bool dipv3_event_unreg_chst(unsigned int ch)
{
	enum EDI_TOP_STATE chst, chst2;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	bool ret = false;
	bool err_flg = false;
	unsigned int cnt;

	chst = dipv3_chst_get(ch);
	dbg_reg("%s:ch[%d]:%s\n", __func__, ch, dipv3_chst_get_name(chst));
	#if 0
	if (chst > eDI_TOP_STATE_IDLE)
		set_reg_flag(ch, false);/*set_flag_trig_unreg(ch, true);*/
	#endif
	if (chst > eDI_TOP_STATE_NOPROB)
		set_flag_trig_unreg(ch, true);

	switch (chst) {
	case EDI_TOP_STATE_READY:

		//di_vframe_unreg(ch);
		dimv3_htr_stop(ch);
		/*trig unreg*/
		dipv3_chst_set(ch, eDI_TOP_STATE_UNREG_STEP1);
		taskv3_send_cmd(LCMD1(eCMD_UNREG, ch));
		/*debug only di_dbg = di_dbg|DBG_M_TSK;*/

		/*wait*/
		ppre->unreg_req_flag_cnt = 0;
		chst2 = dipv3_chst_get(ch);

		while (chst2 != EDI_TOP_STATE_IDLE) {
			taskv3_send_ready();
			usleep_range(10000, 10001);
			/*msleep(5);*/
			if (ppre->unreg_req_flag_cnt++
				> dimv3_get_reg_unreg_cnt()) {
				dimv3_reg_timeout_inc();
				PR_ERR("%s:ch[%d] unreg timeout!!!\n",
				       __func__, ch);
				/*dim_unreg_process();*/
				err_flg = true;
				break;
			}
			#if 0	/*debug only*/
			dbg_reg("\tch[%d]:s[%s],ecnt[%d]\n",
				ch,
				dipv3_chst_get_name(chst2),
				ppre->unreg_req_flag_cnt);
			#endif
			chst2 = dipv3_chst_get(ch);
		}

		/*debug only di_dbg = di_dbg & (~DBG_M_TSK);*/
		dbg_reg("%s:ch[%d] ready end\n", __func__, ch);
		#if 0
		if (!err_flg)
			set_reg_flag(ch, false);
		#endif
		break;
	case eDI_TOP_STATE_BYPASS:
		/*from bypass complet to unreg*/
		//di_vframe_unreg(ch);
		div3_unreg_variable(ch);

		set_reg_flag(ch, false);
		dimv3_htr_stop(ch);
		if (!get_reg_flag_all()) {
			div3_unreg_setting();
			dprev3_init();
			dpostv3_init();
		}
		dipv3_chst_set(ch, EDI_TOP_STATE_IDLE);
		ret = true;

		break;
	case EDI_TOP_STATE_IDLE:
		PR_WARN("have unreg\n");
		break;
	case eDI_TOP_STATE_REG_STEP1:
		dbg_dbg("%s:in reg step1\n", __func__);
		//di_vframe_unreg(ch);
		set_reg_flag(ch, false);

		dimv3_htr_stop(ch);
		dipv3_chst_set(ch, EDI_TOP_STATE_IDLE);

		ret = true;
		break;
	case eDI_TOP_STATE_REG_STEP1_P1:
		/*wait:*/
		cnt = 0;
		chst2 = dipv3_chst_get(ch);
		while (chst2 == eDI_TOP_STATE_REG_STEP1_P1 || cnt < 5) {
			taskv3_send_ready();
			usleep_range(3000, 3001);
			cnt++;
		}
		if (cnt >= 5)
			PR_ERR("%s:ch[%d] in p1 timeout\n", __func__, ch);

		set_reg_flag(ch, false);

		//di_vframe_unreg(ch);
		dimv3_htr_stop(ch);
		/*trig unreg*/
		dipv3_chst_set(ch, eDI_TOP_STATE_UNREG_STEP1);
		taskv3_send_cmd(LCMD1(eCMD_UNREG, ch));
		/*debug only di_dbg = di_dbg|DBG_M_TSK;*/

		/*wait*/
		ppre->unreg_req_flag_cnt = 0;
		chst2 = dipv3_chst_get(ch);

		while (chst2 != EDI_TOP_STATE_IDLE) {
			taskv3_send_ready();
			usleep_range(10000, 10001);
			/*msleep(5);*/
			if (ppre->unreg_req_flag_cnt++
				> dimv3_get_reg_unreg_cnt()) {
				dimv3_reg_timeout_inc();
				PR_ERR("%s:ch[%d] unreg timeout!!!\n",
				       __func__,
				       ch);
				/*di_unreg_process();*/
				err_flg = true;
				break;
			}

			chst2 = dipv3_chst_get(ch);
		}

		/*debug only di_dbg = di_dbg & (~DBG_M_TSK);*/
		dbg_reg("%s:ch[%d] ready end\n", __func__, ch);
		ret = true;

		break;
	case eDI_TOP_STATE_REG_STEP2:
		//di_vframe_unreg(ch);
		div3_unreg_variable(ch);
		set_reg_flag(ch, false);
		dimv3_htr_stop(ch);
		if (!get_reg_flag_all()) {
			div3_unreg_setting();
			dprev3_init();
			dpostv3_init();
		}

		dipv3_chst_set(ch, EDI_TOP_STATE_IDLE);

		ret = true;
		break;
	case eDI_TOP_STATE_UNREG_STEP1:
	case eDI_TOP_STATE_UNREG_STEP2:
		taskv3_send_cmd(LCMD1(eCMD_UNREG, ch));

		/*wait*/
		ppre->unreg_req_flag_cnt = 0;
		while (dipv3_chst_get(ch) != EDI_TOP_STATE_IDLE) {
			usleep_range(10000, 10001);
			if (ppre->unreg_req_flag_cnt++ >
				dimv3_get_reg_unreg_cnt()) {
				dimv3_reg_timeout_inc();
				PR_ERR("%s:unreg_reg_flag timeout!!!\n",
				       __func__);
				//di_vframe_unreg(ch);
				dimv3_htr_stop(ch);
				err_flg = true;
				break;
			}
		}
		break;
	case eDI_TOP_STATE_NOPROB:
	default:
		PR_ERR("err: not prob[%d]\n", chst);
		break;
	}
	if (err_flg)
		ret = false;

	dbg_ev("ch[%d] ret[%d]unreg end\n", ch, ret);

	return ret;
}

/*process for reg and unreg cmd*/
void dipv3_chst_process_reg(unsigned int ch)
{
	enum EDI_TOP_STATE chst;
	//struct vframe_s *vframe;
	void *vframe;
//	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	bool reflesh = true;
	struct di_hpre_s  *pre = get_hw_pre();
	struct di_ch_s		*pch;
	struct dim_dvfm_s	*pdvfm;

	while (reflesh) {
		reflesh = false;

	chst = dipv3_chst_get(ch);

	/*dbg_reg("%s:ch[%d]%s\n", __func__, ch, dipv3_chst_get_name(chst));*/

	switch (chst) {
	case eDI_TOP_STATE_NOPROB:
	case EDI_TOP_STATE_IDLE:
		break;
	case eDI_TOP_STATE_REG_STEP1:/*wait peek*/
		//dim_htr_start();
		pch = get_chdata(ch);
		vframe = pch->interf.opsi.peek(pch);//pw_vf_peek(ch);

		if (vframe) {
			dimv3_tr_ops.pre_get(0);
			set_flag_trig_unreg(ch, false);
			#if 0
			di_reg_variable(ch);

			/*?how  about bypass ?*/
			if (ppre->bypass_flag) {
				/* complete bypass */
				setv3_bypass2_complete(ch, true);
				if (!get_reg_flag_all()) {
					/*first channel reg*/
					dprev3_init();
					dpostv3_init();
					div3_reg_setting(ch, vframe);
				}
				dipv3_chst_set(ch, eDI_TOP_STATE_BYPASS);
				set_reg_flag(ch, true);
			} else {
				setv3_bypass2_complete(ch, false);
				if (!get_reg_flag_all()) {
					/*first channel reg*/
					dprev3_init();
					dpostv3_init();
					div3_reg_setting(ch, vframe);
				}
				dipv3_chst_set(ch, eDI_TOP_STATE_REG_STEP2);
			}
			#else
			dipv3_chst_set(ch, eDI_TOP_STATE_REG_STEP1_P1);
			#endif

			reflesh = true;
		}
		break;
	case eDI_TOP_STATE_REG_STEP1_P1:
		pch = get_chdata(ch);
		vframe = pch->interf.opsi.peek(pch);//pw_vf_peek(ch);
		if (!vframe) {
			PR_ERR("%s:p1 vfm nop\n", __func__);
			dipv3_chst_set(ch, eDI_TOP_STATE_REG_STEP1);

			break;
		}
		#if 0 /*move to reg_step1*/
		/*need call below two function before dvfm_fill_in*/
		dimv3_reg_cfg_sys(pch);
		dipv3_init_value_reg(ch);
		#endif
		pdvfm = pch->interf.op_dvfm_fill(pch);//dvfm_fill_in(pch);
		if (!pdvfm)
			break;

		//di_reg_variable(ch, pdvfm);
		/*pre ops*/
		set_h_ppre(ch);
		if (pre && pre->ops.reg_var)
			pre->ops.reg_var(pre);

		/*di_reg_process_irq(ch);*/ /*check if bypass*/

		/*?how  about bypass ?*/
		//if (ppre->bypass_flag) {
		if (pdvfm->wmode.need_bypass) {
			/* complete bypass */
			setv3_bypass2_complete(ch, true);
			di_reg_variable_needbypass(ch, pdvfm);
			if (!get_reg_flag_all()) {
				/*first channel reg*/
				dprev3_init();
				dpostv3_init();
				pdvfm->wmode.src_h = 480;/*only use for reg*/
				pdvfm->wmode.src_w = 720;
				div3_reg_setting(ch, vframe, &pdvfm->wmode);
				/*pre ops*/
				if (pre && pre->ops.reg_hw)
					pre->ops.reg_hw(pre);
			}
			dipv3_chst_set(ch, eDI_TOP_STATE_BYPASS);
			set_reg_flag(ch, true);
			dimv3_htr_con_update((DIM_HTM_REG_BIT << ch), false);
			dimv3_htr_con_update(DIM_HTM_W_INPUT, true);
		} else {
			setv3_bypass2_complete(ch, false);
			di_reg_variable_normal(ch, pdvfm);
			if (!get_reg_flag_all()) {
				/*first channel reg*/
				dprev3_init();
				dpostv3_init();
				div3_reg_setting(ch, vframe, &pdvfm->wmode);
				/*pre ops*/
				if (pre && pre->ops.reg_hw)
					pre->ops.reg_hw(pre);
			}
			/*this will cause first local buf not alloc*/
			/*dimv3_bypass_first_frame(ch);*/
			dipv3_chst_set(ch, eDI_TOP_STATE_REG_STEP2);
			dimv3_htr_con_update((DIM_HTM_REG_BIT << ch), false);
			dimv3_htr_con_update(DIM_HTM_W_INPUT, true);
			/*set_reg_flag(ch, true);*/
		}

		reflesh = true;
		break;
	case eDI_TOP_STATE_REG_STEP2:/*now no change to do*/
		if (dipv3_cma_get_st(ch) == EDI_CMA_ST_READY) {
			if (div3_cfg_top_get(EDI_CFG_first_bypass)) {
				if (get_sum_g(ch) == 0)
					dimv3_bypass_first_frame(ch);
				else
					PR_INF("ch[%d],g[%d]\n",
					       ch, get_sum_g(ch));
			}
			dipv3_chst_set(ch, EDI_TOP_STATE_READY);
			set_reg_flag(ch, true);
			/*move to step1 dimv3_bypass_first_frame(ch);*/
		}
		break;
	case EDI_TOP_STATE_READY:

		break;
	case eDI_TOP_STATE_BYPASS:
		/*do nothing;*/
		break;
	case eDI_TOP_STATE_UNREG_STEP1:

#if 0
		if (!get_reg_flag(ch)) {	/*need wait pre/post done*/
			dipv3_chst_set(ch, eDI_TOP_STATE_UNREG_STEP2);
			reflesh = true;
		}
#else
		/*debug only dbg_reg("%s:UNREG_STEP1\n", __func__);*/

		if (dprev3_can_exit(ch) && dpstv3_can_exit(ch)) {
			dipv3_chst_set(ch, eDI_TOP_STATE_UNREG_STEP2);
			set_reg_flag(ch, false);
			reflesh = true;
		}
#endif
		break;
	case eDI_TOP_STATE_UNREG_STEP2:
		/*debug only dbg_reg("%s:ch[%d]:UNREG_STEP2\n",__func__, ch);*/
		div3_unreg_variable(ch);

		/*pre ops*/
		set_h_ppre(ch);
		if (pre && pre->ops.unreg_var)
			pre->ops.unreg_var(pre);

		//dim_htr_stop(ch);
		if (!get_reg_flag_all()) {
			div3_unreg_setting();
			/*pre ops*/
			if (pre && pre->ops.unreg_hw)
				pre->ops.unreg_hw(pre);
			dprev3_init();
			dpostv3_init();
		}

		dipv3_chst_set(ch, EDI_TOP_STATE_IDLE);
		/*debug only dbg_reg("ch[%d]UNREG_STEP2 end\n",ch);*/
		break;
	}
	}
}

void dipv3_chst_process_ch(void)
{
	unsigned int ch;
	unsigned int chst;
	struct di_hpre_s  *pre = get_hw_pre();
	struct di_ch_s		*pch = NULL;
	struct dim_dvfm_s	*pdvfm;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		chst = dipv3_chst_get(ch);
		if (chst > EDI_TOP_STATE_IDLE)
			pch = get_chdata(ch);

		switch (chst) {
		case eDI_TOP_STATE_REG_STEP2:
			//pr_info("x2\n");
			if (dipv3_cma_get_st(ch) == EDI_CMA_ST_READY) {
				if (div3_cfg_top_get(EDI_CFG_first_bypass)) {
					if (get_sum_g(ch) == 0)
						dimv3_bypass_first_frame(ch);
					else
						PR_INF("ch[%d],g[%d]\n",
						       ch, get_sum_g(ch));
				}
				dipv3_chst_set(ch, EDI_TOP_STATE_READY);
				set_reg_flag(ch, true);
			}
			break;
#if 1
		case eDI_TOP_STATE_UNREG_STEP1:
			dvfmv3_recycle(pch);
			if (dprev3_can_exit(ch) && dpstv3_can_exit(ch)) {
				dipv3_chst_set(ch, eDI_TOP_STATE_UNREG_STEP2);
				set_reg_flag(ch, false);
			}

			break;
#endif
		case eDI_TOP_STATE_UNREG_STEP2:
			dbg_reg("%s:ch[%d]:at UNREG_STEP2\n", __func__, ch);
			div3_unreg_variable(ch);
			/*pre ops*/
			set_h_ppre(ch);

			//dim_htr_stop(ch);
			if (pre && pre->ops.unreg_var)
				pre->ops.unreg_var(pre);

			if (!get_reg_flag_all()) {
				div3_unreg_setting();
				if (pre && pre->ops.unreg_hw)
					pre->ops.unreg_hw(pre);
				dprev3_init();
				dpostv3_init();
			}

			dipv3_chst_set(ch, EDI_TOP_STATE_IDLE);
			dbg_reg("ch[%d]STEP2 end\n", ch);
			break;
		case EDI_TOP_STATE_READY:
			dvfmv3_recycle(pch);
			pch->interf.op_dvfm_fill(pch);//dvfm_fill_in(pch);
			dimv3_post_keep_back_recycle(ch);
			dimv3_sumx_set(ch);
			break;
		case eDI_TOP_STATE_BYPASS:
			//pr_info("x0\n");
			dvfmv3_recycle(pch);
			pdvfm = dvfmv3_peek(pch, QUED_T_IN);
			if (pdvfm && !pdvfm->wmode.need_bypass) {
				pr_info("x1\n");
				setv3_bypass2_complete(ch, false);
				di_reg_variable_normal(ch, pdvfm);
				dipv3_chst_set(ch, eDI_TOP_STATE_REG_STEP2);
				//dvfm_fill_in(pch);
				pch->interf.op_dvfm_fill(pch);
			}

			break;
		}
	}
}

bool dipv3_chst_change_2unreg(void)
{
	unsigned int ch;
	unsigned int chst;
	bool ret = false;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		chst = dipv3_chst_get(ch);
		dbg_poll("[%d]%d\n", ch, chst);
		if (chst == eDI_TOP_STATE_UNREG_STEP1) {
			dbg_reg("%s:ch[%d]to UNREG_STEP2\n", __func__, ch);
			set_reg_flag(ch, false);
			dipv3_chst_set(ch, eDI_TOP_STATE_UNREG_STEP2);
			ret = true;
		}
	}
	return ret;
}

/****************************************************************
 * tmode
 ****************************************************************/
void dimv3_tmode_preset(void)
{
	struct di_mng_s *pbm = get_bufmng();
	unsigned int ch;
	unsigned int cnt;

	/*EDIM_TMODE_1_PW_VFM*/
	cnt = min_t(size_t, DI_CHANNEL_NUB, cfgg(TMODE_1));
	for (ch = 0; ch < cnt; ch++)
		pbm->tmode_pre[ch] = EDIM_TMODE_1_PW_VFM;

	/*EDIM_TMODE_2_PW_OUT*/
	cnt += cfgg(TMODE_2);
	cnt = min_t(size_t, DI_CHANNEL_NUB, cnt);
	for (; ch < cnt; ch++)
		pbm->tmode_pre[ch] = EDIM_TMODE_2_PW_OUT;

	/*EDIM_TMODE_3_PW_LOCAL*/
	cnt += cfgg(TMODE_3);
	cnt = min_t(size_t, DI_CHANNEL_NUB, cnt);
	for (; ch < cnt; ch++)
		pbm->tmode_pre[ch] = EDIM_TMODE_3_PW_LOCAL;

	/*dbg*/
	PR_INF("dimv3_tmode_preset:\n");
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++)
		PR_INF("\tch[%d]:tmode[%d]\n", ch, pbm->tmode_pre[ch]);
}

void dim_tmode_set(unsigned int ch, enum EDIM_TMODE tmode)
{
	struct di_mng_s *pbm = get_bufmng();

	if (ch >= DI_CHANNEL_NUB)
		return;

	pbm->tmode_pre[ch] = tmode;
}

bool dimv3_tmode_is_localpost(unsigned int ch)
{
//	struct di_mng_s *pbm = get_bufmng();
	struct di_ch_s *pch;

	if (ch >= DI_CHANNEL_NUB) {
		PR_ERR("%s:ch[%d]\n", __func__, ch);
		return false;
	}
	pch = get_chdata(ch);

	if (pch->interf.tmode == EDIM_TMODE_2_PW_OUT)
		return false;
	return true;
}
#if 0
void di_reg_flg_check(void)
{
	int ch;
	unsigned int chst;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++)
		chst = dipv3_chst_get(ch);
}
#endif

void dipv3_hw_process(void)
{
	div3_dbg_task_flg = 5;
	dprev3_process();
	div3_dbg_task_flg = 6;
	prev3_mode_setting();
	div3_dbg_task_flg = 7;
	dpstv3_process();
	div3_dbg_task_flg = 8;
}

const char * const div3_top_state_name[] = {
	"NOPROB",
	"IDLE",
	"REG_STEP1",
	"REG_P1",
	"REG_STEP2",
	"READY",
	"BYPASS",
	"UNREG_STEP1",
	"UNREG_STEP2"
};

const char *dipv3_chst_get_name_curr(unsigned int ch)
{
	const char *p = "";
	enum EDI_TOP_STATE chst;

	chst = dipv3_chst_get(ch);

	if (chst < ARRAY_SIZE(div3_top_state_name))
		p = div3_top_state_name[chst];

	return p;
}

const char *dipv3_chst_get_name(enum EDI_TOP_STATE chst)
{
	const char *p = "";

	if (chst < ARRAY_SIZE(div3_top_state_name))
		p = div3_top_state_name[chst];

	return p;
}
/***********************************************
 *mm cfg
 **********************************************/
static const struct di_mm_cfg_s c_mm_cfg_normal = {
	.di_h	=	1088,
	.di_w	=	1920,
	.num_local	=	MAX_LOCAL_BUF_NUM,
	.num_post	=	MAX_POST_BUF_NUM,
};

/**********************************/
/* TIME OUT CHEKC api*/
/**********************************/

void div3_tout_int(struct di_time_out_s *tout, unsigned int thd)
{
	tout->en = false;
	tout->timer_start = 0;
	tout->timer_thd = thd;
}

bool div3_tout_contr(enum eDI_TOUT_CONTR cmd, struct di_time_out_s *tout)
{
	unsigned long ctimer;
	unsigned long diff;
	bool ret = false;

	ctimer = curv3_to_msecs();

	switch (cmd) {
	case eDI_TOUT_CONTR_EN:
		tout->en = true;
		tout->timer_start = ctimer;
		break;
	case eDI_TOUT_CONTR_FINISH:
		if (tout->en) {
			diff = ctimer - tout->timer_start;

			if (diff > tout->timer_thd) {
				tout->over_flow_cnt++;

				if (tout->over_flow_cnt > 0xfffffff0) {
					tout->over_flow_cnt = 0;
					tout->flg_over = 1;
				}
		#if 0
				if (tout->do_func)
					tout->do_func();

		#endif
				ret = true;
			}
			tout->en = false;
		}
		break;

	case eDI_TOUT_CONTR_CHECK:	/*if time is overflow, disable timer*/
		if (tout->en) {
			diff = ctimer - tout->timer_start;

			if (diff > tout->timer_thd) {
				tout->over_flow_cnt++;

				if (tout->over_flow_cnt > 0xfffffff0) {
					tout->over_flow_cnt = 0;
					tout->flg_over = 1;
				}
				#if 0
				if (tout->do_func)
					tout->do_func();

				#endif
				ret = true;
				tout->en = false;
			}
		}
		break;
	case eDI_TOUT_CONTR_CLEAR:
		tout->en = false;
		tout->timer_start = ctimer;
		break;
	case eDI_TOUT_CONTR_RESET:
		tout->en = true;
		tout->timer_start = ctimer;
		break;
	}

	return ret;
}

const unsigned int div3_ch2mask_table[DI_CHANNEL_MAX] = {
	DI_BIT0,
	DI_BIT1,
	DI_BIT2,
	DI_BIT3,
};
/****************************************
 *bit control
 ****************************************/
static const unsigned int bit_tab[32] = {
	DI_BIT0,
	DI_BIT1,
	DI_BIT2,
	DI_BIT3,
	DI_BIT4,
	DI_BIT5,
	DI_BIT6,
	DI_BIT7,
	DI_BIT8,
	DI_BIT9,
	DI_BIT10,
	DI_BIT11,
	DI_BIT12,
	DI_BIT13,
	DI_BIT14,
	DI_BIT15,
	DI_BIT16,
	DI_BIT17,
	DI_BIT18,
	DI_BIT19,
	DI_BIT20,
	DI_BIT21,
	DI_BIT22,
	DI_BIT23,
	DI_BIT24,
	DI_BIT25,
	DI_BIT26,
	DI_BIT27,
	DI_BIT28,
	DI_BIT29,
	DI_BIT30,
	DI_BIT31
};

void bsetv3(unsigned int *p, unsigned int bitn)
{
	*p = *p | bit_tab[bitn];
}

void bclrv3(unsigned int *p, unsigned int bitn)
{
	*p = *p & (~bit_tab[bitn]);
}

bool bgetv3(unsigned int *p, unsigned int bitn)
{
	return (*p & bit_tab[bitn]) ? true : false;
}

/****************************************/
/* do_table				*/
/****************************************/

/*for do_table_working*/
#define K_DO_TABLE_LOOP_MAX	15

const struct do_table_s dov3_table_def = {
	.ptab		= NULL,
	.data		= NULL,
	.size		= 0,
	.op_lst		= K_DO_TABLE_ID_STOP,
	.op_crr		= K_DO_TABLE_ID_STOP,
	.do_stop	= 0,
	.flg_stop	= 0,
	.do_pause	= 0,
	.do_step	= 0,
	.flg_repeat	= 0,

};

void dov3_table_init(struct do_table_s *pdo,
		   const struct do_table_ops_s *ptable,
		   unsigned int size_tab)
{
	memcpy(pdo, &dov3_table_def, sizeof(struct do_table_s));

	if (ptable) {
		pdo->ptab = ptable;
		pdo->size = size_tab;
	}
}

/*if change to async?*/
/* now only call in same thread */
void dov3_talbe_cmd(struct do_table_s *pdo, enum eDO_TABLE_CMD cmd)
{
	switch (cmd) {
	case eDO_TABLE_CMD_NONE:
		PR_INF("test:%s\n", __func__);
		break;
	case eDO_TABLE_CMD_STOP:
		pdo->do_stop = true;
		break;
	case eDO_TABLE_CMD_START:
		if (pdo->op_crr == K_DO_TABLE_ID_STOP) {
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr = K_DO_TABLE_ID_START;
			pdo->do_stop = false;
			pdo->flg_stop = false;
		} else if (pdo->op_crr == K_DO_TABLE_ID_PAUSE) {
			pdo->op_crr = pdo->op_lst;
			pdo->op_lst = K_DO_TABLE_ID_PAUSE;
			pdo->do_pause = false;
		} else {
			PR_INF("crr is [%d], not start\n", pdo->op_crr);
		}
		break;
	case eDO_TABLE_CMD_PAUSE:
		if (pdo->op_crr <= K_DO_TABLE_ID_STOP) {
			/*do nothing*/
		} else {
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr = K_DO_TABLE_ID_PAUSE;
			pdo->do_pause = true;
		}
		break;
	case eDO_TABLE_CMD_STEP:
		pdo->do_step = true;
		break;
	case eDO_TABLE_CMD_STEP_BACK:
		pdo->do_step = false;
		break;
	default:
		break;
	}
}

bool dov3_table_is_crr(struct do_table_s *pdo, unsigned int state)
{
	if (pdo->op_crr == state)
		return true;
	return false;
}

void dov3_table_working(struct do_table_s *pdo)
{
	const struct do_table_ops_s *pcrr;
	unsigned int ret = 0;
	unsigned int next;
	bool flash = false;
	unsigned int cnt = 0;	/*proction*/
	unsigned int lst_id;	/*dbg only*/
	char *name = "";	/*dbg only*/
	bool need_pr = false;	/*dbg only*/

	if (!pdo || !pdo->ptab) {
		PR_ERR("no pdo or ptab\n");
		return;
	}

	if (pdo->op_crr >= pdo->size) {
		PR_ERR("di:err:%s:crr=%d,size=%d\n",
		       __func__,
		       pdo->op_crr,
		       pdo->size);
		return;
	}

	pcrr = pdo->ptab + pdo->op_crr;

	if (pdo->name)
		name = pdo->name;
	/*stop ?*/
	if (pdo->do_stop &&
	    (pcrr->mark & K_DO_TABLE_CAN_STOP)) {
		dbg_dt("%s:do stop\n", name);

		/*do stop*/
		if (pcrr->do_stop_op)
			pcrr->do_stop_op(pdo->data);
		/*set status*/
		pdo->op_lst = pdo->op_crr;
		pdo->op_crr = K_DO_TABLE_ID_STOP;
		pdo->flg_stop = true;
		pdo->do_stop = false;

		return;
	}

	/*pause?*/
	if (pdo->op_crr == K_DO_TABLE_ID_STOP	||
	    pdo->op_crr == K_DO_TABLE_ID_PAUSE)
		return;

	do {
		flash = false;
		cnt++;
		if (cnt > K_DO_TABLE_LOOP_MAX) {
			PR_ERR("di:err:%s:loop more %d\n", name, cnt);
			break;
		}

		/*stop again? */
		if (pdo->do_stop &&
		    (pcrr->mark & K_DO_TABLE_CAN_STOP)) {
			/*do stop*/
			dbg_dt("%s: do stop in loop\n", name);
			if (pcrr->do_stop_op)
				pcrr->do_stop_op(pdo->data);
			/*set status*/
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr = K_DO_TABLE_ID_STOP;
			pdo->flg_stop = true;
			pdo->do_stop = false;

			break;
		}

		/*debug:*/
		lst_id = pdo->op_crr;
		need_pr = true;

		if (pcrr->con) {
			if (pcrr->con(pdo->data))
				ret = pcrr->do_op(pdo->data);
			else
				break;

		} else {
			ret = pcrr->do_op(pdo->data);
			dbg_dt("do_table:do:%d:ret=0x%x\n", pcrr->id, ret);
		}

		/*not finish, keep current status*/
		if ((ret & K_DO_TABLE_R_B_FINISH) == 0) {
			dbg_dt("%s:not finish,wait\n", __func__);
			break;
		}

		/*fix to next */
		if (ret & K_DO_TABLE_R_B_NEXT) {
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr++;
			if (pdo->op_crr >= pdo->size) {
				pdo->op_crr = pdo->flg_repeat ?
					K_DO_TABLE_ID_START
					: K_DO_TABLE_ID_STOP;
				dbg_dt("%s:to end,%d\n", __func__,
				       pdo->op_crr);
				break;
			}
			/*return;*/
			flash = true;
		} else {
			next = ((ret & K_DO_TABLE_R_B_OTHER) >>
					K_DO_TABLE_R_B_OTHER_SHIFT);
			if (next < pdo->size) {
				pdo->op_lst = pdo->op_crr;
				pdo->op_crr = next;
				if (next > K_DO_TABLE_ID_STOP)
					flash = true;
				else
					flash = false;
			} else {
				PR_ERR("%s: next[%d] err:\n",
				       __func__, next);
			}
		}
		/*debug 1:*/
		need_pr = false;
		if (lst_id != pdo->op_crr) {
			dbg_dt("do_table:%s:%s->%s\n", pdo->name,
			       pdo->ptab[lst_id].name,
			       pdo->ptab[pdo->op_crr].name);
		}

		pcrr = pdo->ptab + pdo->op_crr;
	} while (flash && !pdo->do_step);

	/*debug 2:*/
	if (need_pr) {
		if (lst_id != pdo->op_crr) {
			dbg_dt("do_table2:%s:%s->%s\n", pdo->name,
			       pdo->ptab[lst_id].name,
			       pdo->ptab[pdo->op_crr].name);
		}
	}
}

/********************************************************************
 * dvfm
 *******************************************************************/
void dvfm_prob(struct di_ch_s *pch)
{
	int i;
	struct dim_dvfm_s	*pdvfm;

	if (!pch)
		return;

	for (i = 0; i < DIM_K_VFM_NUM; i++) {
		pdvfm = &pch->dvfm[i];
		/*clear*/
		memset(pdvfm, 0, sizeof(*pdvfm));

		/*set code*/
//		pdvfm->code_name	= DIM_K_CODE_DVFM;
		pdvfm->index		= i;
	}
}

void dvfm_reg(struct di_ch_s *pch)
{
	dvfm_prob(pch);
}

struct dim_dvfm_s *dvfmv3_get(struct di_ch_s *pch, enum QUED_TYPE qtype)
{
	struct dim_dvfm_s	*pdvfm;
	unsigned int		dvfm_id;
	bool ret;

	if (!pch) {
		PR_ERR("%s:p is null\n", __func__);
		return NULL;
	}

	ret = qued_ops.out(pch, qtype, &dvfm_id);

	if (!ret ||
	    dvfm_id >= DIM_K_VFM_NUM) {
		PR_ERR("%s:ch[%d]:que:%s:get failed:ret[%d],id[0x%x]\n",
		       __func__,
		       pch->ch_id,
		       qued_ops.get_name(pch, qtype),
		       ret, dvfm_id);
		return NULL;
	}

	pdvfm = &pch->dvfm[dvfm_id];
	if (!pdvfm)
		return NULL;

	return pdvfm;
}

struct dim_dvfm_s *dvfmv3_peek(struct di_ch_s *pch, enum QUED_TYPE qtype)
{
	struct dim_dvfm_s	*pdvfm;
	unsigned int		dvfm_id;
	bool ret;

	if (!pch) {
		PR_ERR("%s:p is null\n", __func__);
		return NULL;
	}

	ret = qued_ops.peek(pch, qtype, &dvfm_id);

	if (ret && dvfm_id >= DIM_K_VFM_NUM) {
		PR_ERR("%s:ch[%d]:que:%s:get failed:ret[%d],id[%d]\n",
		       __func__,
		       pch->ch_id,
		       qued_ops.get_name(pch, qtype),
		       ret, dvfm_id);
		return NULL;
	}

	if (!ret) /*empty*/
		pdvfm = NULL;
	else
		pdvfm = &pch->dvfm[dvfm_id];

	return pdvfm;
}

#define VFM_NEED_BYPASS	(VIDTYPE_MVC		|	\
			 VIDTYPE_VIU_444	|	\
			 VIDTYPE_PIC)

/* for dvfm_fill_in use*/
bool dimv3_need_bypass2(struct di_in_inf_s *in_inf, struct vframe_s *vf)
{
	struct di_mm_s *mm;

	if (!in_inf || !vf)
		return true;

	mm = dim_mm_get(in_inf->ch);

	if (!mm)
		return true;

	/*vfm type*/
	if (vf->type & VFM_NEED_BYPASS)
		return true;
	if (vf->source_type == VFRAME_SOURCE_TYPE_PPMGR)
		return true;
#if 1
	if (vfv3_type_is_prog(vf->type)) {/*temp bypass p*/
		return true;
	}
#endif
	/*true bypass for 720p above*/
	if ((vf->flag & VFRAME_FLAG_GAME_MODE) &&
	    (vf->width > 720))
		return true;
	/*support G12A and TXLX platform*/
	if (vf->type & VIDTYPE_COMPRESS) {
		if (!dimhv3_afbc_is_supported())
			return true;
	}

	if ((in_inf->w > mm->cfg.di_w) ||
	    (in_inf->h > mm->cfg.di_h))
		return true;

	if ((dimp_get(eDI_MP_di_debug_flag) >> 20) & 0x1)
		return true;

	return false;
}

void dimv3_polic_cfg(unsigned int cmd, bool on)
{
	//struct dim_policy_s *pp;
#ifdef TST_NEW_INS_RUN_Q
	if (dil_get_diffver_flag() != 1)
		return;
#endif
#if 0
	pp = get_dpolicy();
	switch (cmd) {
	case K_DIM_BYPASS_CLEAR_ALL:
		pp->cfg_d32 = 0x0;
		break;
	case K_DIM_I_FIRST:
		pp->cfg_b.i_first = on;
		break;
	case K_DIM_BYPASS_ALL_P:
		pp->cfg_b.bypass_all_p = on;
		break;
	default:
		PR_WARN("%s:cmd is overflow[%d]\n", __func__, cmd);
		break;
	}
#endif
}
EXPORT_SYMBOL(dimv3_polic_cfg);

unsigned int dimv3_get_trick_mode(void)
{
	unsigned int trick_mode;

	if (dimp_get(eDI_MP_bypass_trick_mode)) {
		int trick_mode_fffb = 0;
		int trick_mode_i = 0;

		if (dimp_get(eDI_MP_bypass_trick_mode) & 0x1)
			query_video_status(0, &trick_mode_fffb);
		if (dimp_get(eDI_MP_bypass_trick_mode) & 0x2)
			query_video_status(1, &trick_mode_i);
		trick_mode =
			trick_mode_fffb | (trick_mode_i << 1);

		return trick_mode;
	}
	return 0;
}
/**********************************
 *diff with  dim_is_bypass
 *	delet di_vscale_skip_enable
 *	use vf_in replace ppre
 *	for dvfm_fill_in
 **********************************/
bool isv3_bypass2(struct vframe_s *vf_in, struct di_ch_s *pch,
		unsigned int *reason)
{
	if (div3_cfgx_get(pch->ch_id, eDI_CFGX_BYPASS_ALL)) {
		*reason = 1;
		return true;
	}
	#if 0
	/*?*/
	if (dimp_get(eDI_MP_bypass_trick_mode)) {
		int trick_mode_fffb = 0;
		int trick_mode_i = 0;

		if (dimp_get(eDI_MP_bypass_trick_mode) & 0x1)
			query_video_status(0, &trick_mode_fffb);
		if (dimp_get(eDI_MP_bypass_trick_mode) & 0x2)
			query_video_status(1, &trick_mode_i);
		pdvfm->wmode.trick_mode =
			trick_mode_fffb | (trick_mode_i << 1);
		if (pdvfm->wmode.trick_mode)
			return true;
	}
	#endif
	/* check vframe */
	//vf_in = pdvfm->vfm_in;
	if (!vf_in)
		return false;

	if ((vf_in->width < 16) || (vf_in->height < 16)) {
		*reason = 2;
		return true;
	}

	if (dimp_get(eDI_MP_bypass_3d)	&&
	    (vf_in->trans_fmt != 0)) {
		*reason = 3;
		return true;
	}

/*prot is conflict with di post*/
	if (vf_in->video_angle) {
		*reason = 4;
		return true;
	}

	return false;
}

void dimv3_reg_cfg_sys(struct di_ch_s *pch)
{
	//struct di_data_l_s *pdata = get_datal(void)
	bool bit10_pack_patch = false;
	unsigned int width_roundup = 2;
	struct di_mng_s *pbm = get_bufmng();
	unsigned int ch;

	if (!pch)
		return;

	/*set width_roundup*/
	bit10_pack_patch = (is_meson_gxtvbb_cpu()	||
			    is_meson_gxl_cpu()		||
			    is_meson_gxm_cpu());

	pch->cfgt.w_rdup = bit10_pack_patch ? 16 : width_roundup;

	/*set force_w | force_h*/
	pch->cfgt.f_w = dimp_get(eDI_MP_force_width);
	pch->cfgt.f_h = dimp_get(eDI_MP_force_height);
	if (dimp_get(eDI_MP_di_force_bit_mode) == 10) {
		if (pch->cfgt.f_w) {
			pch->cfgt.f_w =
				roundup(pch->cfgt.f_w, width_roundup);
		}
	}

	if (dimp_get(eDI_MP_di_force_bit_mode) == 10) {
		pch->cfgt.vfm_bitdepth = (BITDEPTH_Y10);
		if (dimp_get(eDI_MP_full_422_pack))
			pch->cfgt.vfm_bitdepth |= (FULL_PACK_422_MODE);
	} else {
		pch->cfgt.vfm_bitdepth = (BITDEPTH_Y8);
	}

	pch->cfgt.ens.en_32 = 0;
	if (dimp_get(eDI_MP_pre_hsc_down_en))
		pch->cfgt.ens.b.h_sc_down = 1;

	if (dimp_get(eDI_MP_pps_en))
		pch->cfgt.ens.b.pps_enable = 1;

	if (cfgg(PMODE) == 2) { /*use 2 i buf*/
		dimp_set(eDI_MP_prog_proc_config, 0x03);
	}

	/*tmode*/
	ch = pch->ch_id;
	pch->cfgt.tmode = pbm->tmode_pre[ch];
}

unsigned int topv3_bot_config(unsigned int vtype)
{
	unsigned int vfm_type = vtype;

	if ((vfm_type & VIDTYPE_TYPEMASK) ==
	    VIDTYPE_INTERLACE_TOP) {
		vfm_type &= (~VIDTYPE_TYPEMASK);
		vfm_type |= VIDTYPE_INTERLACE_BOTTOM;
	} else {
		vfm_type &= (~VIDTYPE_TYPEMASK);
		vfm_type |= VIDTYPE_INTERLACE_TOP;
	}
	return vfm_type;
}

void dvfmv3_recycle(struct di_ch_s *pch)
{
	int i;
	struct dim_dvfm_s	*pdvfm;
	unsigned int back_in;

	for (i = 0; i < DIM_K_VFM_NUM; i++) {
		pdvfm = dvfmv3_peek(pch, QUED_T_RECYCL);
		if (!pdvfm)
			break;
		pdvfm	= dvfmv3_get(pch, QUED_T_RECYCL);
//		back_c	= pdvfm->code_name;
		back_in	= pdvfm->index;

		memset(pdvfm, 0, sizeof(*pdvfm));
//		pdvfm->code_name	= back_c;
		pdvfm->index		= back_in;
		qued_ops.in(pch, QUED_T_FREE, pdvfm->index);
	}
}

/****************************/
void dipv3_init_value_reg(unsigned int ch)
{
	struct di_post_stru_s *ppost;
//	struct di_buf_s *keep_post_buf;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	struct di_ch_s *pch = get_chdata(ch);
//	struct di_data_l_s *pdt = get_datal();
//	struct dim_dvfm_s *pdvfm;
	int i;

	if (!pch)
		return;

	PR_INF("%s:\n", __func__);

	/*post*/
	ppost = get_post_stru(ch);
	/*keep buf:*/
	/*keep_post_buf = ppost->keep_buf_post;*/

	memset(ppost, 0, sizeof(struct di_post_stru_s));
	ppost->next_canvas_id = 1;

	/*pre*/
	memset(ppre, 0, sizeof(struct di_pre_stru_s));

	/*clear dvfm*/
	dvfm_reg(pch);

	/*qued*/
	qued_ops.reg(pch);

	/*put all dvfm to free*/
	for (i = 0; i < DIM_K_VFM_NUM; i++)
		qued_ops.in(pch, QUED_T_FREE, i);

	if ((pch->interf.tmode == EDIM_TMODE_2_PW_OUT ||
	     pch->interf.tmode == EDIM_TMODE_3_PW_LOCAL)) {
		/*que buf*/
		for (i = 0; i < DIM_K_BUF_IN_LIMIT; i++)
			qued_ops.in(pch, QUED_T_IS_FREE, i);

		for (i = 0; i < DIM_K_BUF_OUT_LIMIT; i++)
			qued_ops.in(pch, QUED_T_IS_PST_FREE, i);
	} else {
		PR_INF("%s:tmod[%d]\n", __func__, pch->interf.tmode);
	}
	/*last dvfm*/
	memset(&pch->lst_dvfm, 0, sizeof(struct dim_dvfm_s));

	di_bypass_state_set(ch, false);
}

static bool dip_init_value(void) /*call in prob*/
{
	unsigned int ch;
	struct di_post_stru_s *ppost;
	struct di_mm_s *mm;// = dim_mm_get();
	struct dim_mm_t_s *mmt = dim_mmt_get();
	//struct di_data_l_s *pdata = get_datal();
	struct di_ch_s *pch;
	bool ret = false;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		/*ch_data*/
		pch = get_chdata(ch);
		pch->ch_id = ch;

		/*ppost*/
		ppost = get_post_stru(ch);
		memset(ppost, 0, sizeof(struct di_post_stru_s));
		ppost->next_canvas_id = 1;

		/*que*/
		ret = div3_que_alloc(ch);
		if (ret) {
			pwv3_queue_clear(ch, QUE_POST_KEEP);
			pwv3_queue_clear(ch, QUE_POST_KEEP_BACK);
		}

		mm = dim_mm_get(ch);

		memcpy(&mm->cfg, &c_mm_cfg_normal, sizeof(struct di_mm_cfg_s));

		/*qued*/
		qued_ops.prob(pch);

		/*dvfm*/
		dvfm_prob(pch);
	}
	/*mmm top*/
	mmt->mem_start = 0;
	mmt->mem_size = 0;
	mmt->total_pages = NULL;
	set_current_channel(0);

	return ret;
}

/******************************************
 *	pq ops
 *****************************************/
void dipv3_init_pq_ops(void)
{
	di_attach_ops_pulldown(&get_datal()->ops_pd);
	di_attach_ops_3d(&get_datal()->ops_3d);
	di_attach_ops_nr(&get_datal()->ops_nr);
	di_attach_ops_mtn(&get_datal()->ops_mtn);

	dimv3_attach_to_local();
}

/**********************************/
void dipv3_clean_value(void)
{
	unsigned int ch;
	struct di_data_l_s *pdt = get_datal();
	struct di_ch_s *pch;

	if (!pdt)
		return;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		/*que*/
		div3_que_release(ch);

		/*qued*/
		pch = get_chdata(ch);
		qued_ops.remove(pch);
	}
}

bool dipv3_prob(void)
{
	bool ret = true;

	ret = dip_init_value();

	div3_cfgx_init_val();
	div3_cfg_top_init_val();
	div3_mp_uit_init_val();
	div3_mp_uix_init_val();

	dimv3_tmode_preset(); /*need before vframe*/

	devv3_vframe_init();
	didbgv3_fs_init();
	dip_wq_prob();
	dip_cma_init_val();
	dipv3_chst_init();

	dprev3_init();
	dpostv3_init();

	hprev3_prob();
	dipv3_init_pq_ops();


	return ret;
}

void dipv3_exit(void)
{
	dip_wq_ext();
	devv3_vframe_exit();
	dipv3_clean_value();
	didbgv3_fs_exit();
}

