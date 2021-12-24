/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_OSD_SCALER_H_
#define _MESON_OSD_SCALER_H_

#define HW_OSD_SCALER_NUM 4

/*vpp osd scaler*/
#define VPP_OSD_VSC_PHASE_STEP 0x1dc0
#define VPP_OSD_VSC_INI_PHASE 0x1dc1
#define VPP_OSD_VSC_CTRL0 0x1dc2
#define VPP_OSD_HSC_PHASE_STEP 0x1dc3
#define VPP_OSD_HSC_INI_PHASE 0x1dc4
#define VPP_OSD_HSC_CTRL0 0x1dc5
#define VPP_OSD_HSC_INI_PAT_CTRL 0x1dc6
#define VPP_OSD_SC_DUMMY_DATA 0x1dc7
#define VPP_OSD_SC_CTRL0 0x1dc8
#define VPP_OSD_SCI_WH_M1 0x1dc9
#define VPP_OSD_SCO_H_START_END 0x1dca
#define VPP_OSD_SCO_V_START_END 0x1dcb
#define VPP_OSD_SCALE_COEF_IDX 0x1dcc
#define VPP_OSD_SCALE_COEF 0x1dcd

/* vpp osd2 scaler */
#define OSD2_VSC_PHASE_STEP 0x3d00
#define OSD2_VSC_INI_PHASE 0x3d01
#define OSD2_VSC_CTRL0 0x3d02
#define OSD2_HSC_PHASE_STEP 0x3d03
#define OSD2_HSC_INI_PHASE 0x3d04
#define OSD2_HSC_CTRL0 0x3d05
#define OSD2_HSC_INI_PAT_CTRL 0x3d06
#define OSD2_SC_DUMMY_DATA 0x3d07
#define OSD2_SC_CTRL0 0x3d08
#define OSD2_SCI_WH_M1 0x3d09
#define OSD2_SCO_H_START_END 0x3d0a
#define OSD2_SCO_V_START_END 0x3d0b
#define OSD2_SCALE_COEF_IDX 0x3d18
#define OSD2_SCALE_COEF 0x3d19

/* vpp osd34 scaler */
#define OSD34_VSC_PHASE_STEP 0x3d20
#define OSD34_VSC_INI_PHASE 0x3d21
#define OSD34_VSC_CTRL0 0x3d22
#define OSD34_HSC_PHASE_STEP 0x3d23
#define OSD34_HSC_INI_PHASE 0x3d24
#define OSD34_HSC_CTRL0 0x3d25
#define OSD34_HSC_INI_PAT_CTRL 0x3d26
#define OSD34_SC_DUMMY_DATA 0x3d27
#define OSD34_SC_CTRL0 0x3d28
#define OSD34_SCI_WH_M1 0x3d29
#define OSD34_SCO_H_START_END 0x3d2a
#define OSD34_SCO_V_START_END 0x3d2b
#define OSD34_SCALE_COEF_IDX 0x3d1e
#define OSD34_SCALE_COEF 0x3d1f

/* for t7 osd scaler */
#define T7_VPP_OSD_VSC_PHASE_STEP                     0x5a00
#define T7_VPP_OSD_VSC_INI_PHASE                      0x5a01
#define T7_VPP_OSD_VSC_CTRL0                          0x5a02
#define T7_VPP_OSD_HSC_PHASE_STEP                     0x5a03
#define T7_VPP_OSD_HSC_INI_PHASE                      0x5a04
#define T7_VPP_OSD_HSC_CTRL0                          0x5a05
#define T7_VPP_OSD_HSC_INI_PAT_CTRL                   0x5a06
#define T7_VPP_OSD_SC_DUMMY_DATA                      0x5a07
#define T7_VPP_OSD_SC_CTRL0                           0x5a08
#define T7_VPP_OSD_SCI_WH_M1                          0x5a09
#define T7_VPP_OSD_SCO_H_START_END                    0x5a0a
#define T7_VPP_OSD_SCO_V_START_END                    0x5a0b
#define T7_VPP_OSD_SCALE_COEF_IDX                     0x5a0c
#define T7_VPP_OSD_SCALE_COEF                         0x5a0d

#define T7_OSD2_VSC_PHASE_STEP                        0x5a40
#define T7_OSD2_VSC_INI_PHASE                         0x5a41
#define T7_OSD2_VSC_CTRL0                             0x5a42
#define T7_OSD2_HSC_PHASE_STEP                        0x5a43
#define T7_OSD2_HSC_INI_PHASE                         0x5a44
#define T7_OSD2_HSC_CTRL0                             0x5a45
#define T7_OSD2_HSC_INI_PAT_CTRL                      0x5a46
#define T7_OSD2_SC_DUMMY_DATA                         0x5a47
#define T7_OSD2_SC_CTRL0                              0x5a48
#define T7_OSD2_SCI_WH_M1                             0x5a49
#define T7_OSD2_SCO_H_START_END                       0x5a4a
#define T7_OSD2_SCO_V_START_END                       0x5a4b
#define T7_OSD2_SCALE_COEF_IDX                        0x5a4c
#define T7_OSD2_SCALE_COEF                            0x5a4d

#define T7_OSD34_VSC_PHASE_STEP                       0x5a80
#define T7_OSD34_VSC_INI_PHASE                        0x5a81
#define T7_OSD34_VSC_CTRL0                            0x5a82
#define T7_OSD34_HSC_PHASE_STEP                       0x5a83
#define T7_OSD34_HSC_INI_PHASE                        0x5a84
#define T7_OSD34_HSC_CTRL0                            0x5a85
#define T7_OSD34_HSC_INI_PAT_CTRL                     0x5a86
#define T7_OSD34_SC_DUMMY_DATA                        0x5a87
#define T7_OSD34_SC_CTRL0                             0x5a88
#define T7_OSD34_SCI_WH_M1                            0x5a89
#define T7_OSD34_SCO_H_START_END                      0x5a8a
#define T7_OSD34_SCO_V_START_END                      0x5a8b
#define T7_OSD34_SCALE_COEF_IDX                       0x5a8c
#define T7_OSD34_SCALE_COEF                           0x5a8d

#define T7_OSD4_VSC_PHASE_STEP                        0x5ac0
#define T7_OSD4_VSC_INI_PHASE                         0x5ac1
#define T7_OSD4_VSC_CTRL0                             0x5ac2
#define T7_OSD4_HSC_PHASE_STEP                        0x5ac3
#define T7_OSD4_HSC_INI_PHASE                         0x5ac4
#define T7_OSD4_HSC_CTRL0                             0x5ac5
#define T7_OSD4_HSC_INI_PAT_CTRL                      0x5ac6
#define T7_OSD4_SC_DUMMY_DATA                         0x5ac7
#define T7_OSD4_SC_CTRL0                              0x5ac8
#define T7_OSD4_SCI_WH_M1                             0x5ac9
#define T7_OSD4_SCO_H_START_END                       0x5aca
#define T7_OSD4_SCO_V_START_END                       0x5acb
#define T7_OSD4_SCALE_COEF_IDX                        0x5acc
#define T7_OSD4_SCALE_COEF                            0x5acd

/*macro define for chip const*/
/*bank length is related to scale fifo:4 line 1920??*/
#define OSD_SCALE_BANK_LENGTH 4
#define OSD_SCALE_LINEBUFFER 1920

#define OSD_ZOOM_WIDTH_BITS 18
#define OSD_ZOOM_HEIGHT_BITS 20
#define OSD_ZOOM_TOTAL_BITS 24
#define OSD_PHASE_BITS 16

#define OSD_SCALER_COEFF_H 1
#define OSD_SCALER_COEFF_V 0

enum scaler_coef_e {
	COEFS_BICUBIC_SHARP = 0,
	COEFS_BICUBIC,
	COEFS_BILINEAR,
	COEFS_2POINT_BINILEAR,
	COEFS_3POINT_TRIANGLE_SHARP,
	COEFS_3POINT_TRIANGLE,
	COEFS_4POINT_TRIANGLE,
	COEFS_4POINT_BSPLINE,
	COEFS_3POINT_BSPLINE,
	COEFS_REPEATE,
	COEFS_MAX
};

enum osd_scaler_f2v_vphase_type_e {
	OSD_SCALER_F2V_IT2IT = 0,
	OSD_SCALER_F2V_IB2IB,
	OSD_SCALER_F2V_IT2IB,
	OSD_SCALER_F2V_IB2IT,
	OSD_SCALER_F2V_P2IT,
	OSD_SCALER_F2V_P2IB,
	OSD_SCALER_F2V_IT2P,
	OSD_SCALER_F2V_IB2P,
	OSD_SCALER_F2V_P2P,
	OSD_SCALER_F2V_TYPE_MAX
};

struct osd_scaler_f2v_vphase_s {
	u8 rcv_num;
	u8 rpt_num;
	u16 phase;
};

struct osd_scaler_reg_s {
	u32 vpp_osd_scale_coef_idx;
	u32 vpp_osd_scale_coef;
	u32 vpp_osd_vsc_phase_step;
	u32 vpp_osd_vsc_ini_phase;
	u32 vpp_osd_vsc_ctrl0;
	u32 vpp_osd_hsc_phase_step;
	u32 vpp_osd_hsc_ini_phase;
	u32 vpp_osd_hsc_ctrl0;
	u32 vpp_osd_hsc_ini_pat_ctrl;
	u32 vpp_osd_sc_dummy_data;
	u32 vpp_osd_sc_ctrl0;
	u32 vpp_osd_sci_wh_m1;
	u32 vpp_osd_sco_h_start_end;
	u32 vpp_osd_sco_v_start_end;
};

#endif
