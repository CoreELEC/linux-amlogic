/*
 * drivers/amlogic/media/di_multi_v3/di_prc.h
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

#ifndef __DI_PRC_H__
#define __DI_PRC_H__

bool dipv3_prob(void);
void dipv3_exit(void);

void dipv3_even_reg_init_val(unsigned int ch);
void dipv3_even_unreg_val(unsigned int ch);

/************************/
/* CMA */
/************************/
void dipv3_wq_cma_run(unsigned char ch, bool reg_cmd);
bool dipv3_cma_st_is_ready(unsigned int ch);
bool dipv3_cma_st_is_idle(unsigned int ch);
bool dipv3_cma_st_is_idl_all(void);
enum eDI_CMA_ST dipv3_cma_get_st(unsigned int ch);
void dipv3_cma_st_set_ready_all(void);
void dipv3_cma_close(void);
const char *div3_cma_dbg_get_st_name(unsigned int ch);

/*************************/
/* STATE*/
/*************************/
int dipv3_event_reg_chst(unsigned int ch);
bool dipv3_event_unreg_chst(unsigned int ch);
void dipv3_chst_process_reg(unsigned int ch);

void dipv3_hw_process(void);

void dipv3_chst_process_ch(void);
bool dipv3_chst_change_2unreg(void);

enum EDI_TOP_STATE dipv3_chst_get(unsigned int ch);
const char *dipv3_chst_get_name_curr(unsigned int ch);
const char *dipv3_chst_get_name(enum EDI_TOP_STATE chst);

/**************************************
 *
 * summmary variable
 *
 **************************************/
void div3_sum_reg_init(unsigned int ch);
void div3_sum_set(unsigned int ch, enum eDI_SUM id, unsigned int val);
unsigned int div3_sum_inc(unsigned int ch, enum eDI_SUM id);
unsigned int div3_sum_get(unsigned int ch, enum eDI_SUM id);
void div3_sum_get_info(unsigned int ch, enum eDI_SUM id, char **name,
		     unsigned int *pval);
unsigned int div3_sum_get_tab_size(void);
bool div3_sum_check(unsigned int ch, enum eDI_SUM id);

/**************************************
 *
 * cfg ctr top
 *	bool
 **************************************/
bool div3_cfg_top_check(unsigned int idx);
char *div3_cfg_top_get_name(enum eDI_CFG_TOP_IDX idx);
void div3_cfg_top_get_info(unsigned int idx, char **name);
void div3_cfg_top_init_val(void);
void div3_cfg_top_dts(void);
unsigned int div3_cfg_top_get(enum eDI_CFG_TOP_IDX id);
void div3_cfg_top_set(enum eDI_CFG_TOP_IDX id, unsigned int en);
void div3_cfgt_show_item_sel(struct seq_file *s);
void div3_cfgt_show_item_all(struct seq_file *s);
void div3_cfgt_show_val_sel(struct seq_file *s);
void div3_cfgt_show_val_all(struct seq_file *s);
void div3_cfgt_set_sel(unsigned int dbg_mode, unsigned int id);

/**************************************
 *
 * cfg ctr x
 *	bool
 **************************************/
char *div3_cfgx_get_name(enum eDI_CFGX_IDX idx);
void div3_cfgx_get_info(enum eDI_CFGX_IDX idx, char **name);
void div3_cfgx_init_val(void);
bool div3_cfgx_get(unsigned int ch, enum eDI_CFGX_IDX idx);
void div3_cfgx_set(unsigned int ch, enum eDI_CFGX_IDX idx, bool en);

/**************************************
 *
 * module para top
 *	int
 **************************************/
char *div3_mp_uit_get_name(enum eDI_MP_UI_T idx);
void div3_mp_uit_init_val(void);
//int di_mp_uit_get(enum eDI_MP_UI_T idx);
//void di_mp_uit_set(enum eDI_MP_UI_T idx, int val);

/**************************************
 *
 * module para x
 *	unsigned int
 **************************************/
char *div3_mp_uix_get_name(enum eDI_MP_UIX_T idx);
void div3_mp_uix_init_val(void);
unsigned int div3_mp_uix_get(unsigned int ch, enum eDI_MP_UIX_T idx);
void div3_mp_uix_set(unsigned int ch, enum eDI_MP_UIX_T idx,
		   unsigned int val);

/****************************************/
/* do_table				*/
/****************************************/
void dov3_table_init(struct do_table_s *pdo,
		   const struct do_table_ops_s *ptable,
		   unsigned int size_tab);
/* now only call in same thread */
void dov3_talbe_cmd(struct do_table_s *pdo, enum eDO_TABLE_CMD cmd);
void dov3_table_working(struct do_table_s *pdo);
bool dov3_table_is_crr(struct do_table_s *pdo, unsigned int state);

enum eDI_SUB_ID pwv3_ch_next_count(enum eDI_SUB_ID channel);

void dipv3_init_value_reg(unsigned int ch);

bool div3_is_pause(unsigned int ch);
void div3_pause_step_done(unsigned int ch);
void div3_pause(unsigned int ch, bool on);

void dimv3_tst_4k_reg_val(void);
unsigned int dimv3_get_trick_mode(void);

/****************************************
 *bit control
 ****************************************/
void bsetv3(unsigned int *p, unsigned int bitn);
void bclrv3(unsigned int *p, unsigned int bitn);
bool bgetv3(unsigned int *p, unsigned int bitn);

/**************************************
 * get and put in QUED_T_IN
 **************************************/
struct dim_dvfm_s *dvfmv3_peek(struct di_ch_s *pch, enum QUED_TYPE qtype);
struct dim_dvfm_s *dvfmv3_get(struct di_ch_s *pch, enum QUED_TYPE qtype);
void dvfmv3_recycle(struct di_ch_s *pch);
void dvfmv3_pre_bypassall(struct di_ch_s *pch);
void dimv3_reg_cfg_sys(struct di_ch_s *pch);

/***************************************
 *
 **************************************/
void dimv3_sumx_clear(unsigned int ch);
void dimv3_sumx_set(unsigned int ch);
/**************************************
 *
 **************************************/
bool dimv3_tmode_is_localpost(unsigned int ch);

bool dimv3_need_bypass2(struct di_in_inf_s *in_inf, struct vframe_s *vf);
bool isv3_bypass2(struct vframe_s *vf_in, struct di_ch_s *pch,
		unsigned int *reason);
unsigned int topv3_bot_config(unsigned int vtype);


#endif	/*__DI_PRC_H__*/
