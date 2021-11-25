/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef DOS_REGS_HEADERS__
#define DOS_REGS_HEADERS__

#define MC_CTRL_REG                  0x0900
#define MC_MB_INFO                   0x0901
#define MC_PIC_INFO                  0x0902
#define MC_HALF_PEL_ONE              0x0903
#define MC_HALF_PEL_TWO              0x0904
#define POWER_CTL_MC                 0x0905
#define MC_CMD                       0x0906
#define MC_CTRL0                     0x0907
#define MC_PIC_W_H                   0x0908
#define MC_STATUS0                   0x0909
#define MC_STATUS1                   0x090a
#define MC_CTRL1                     0x090b
#define MC_MIX_RATIO0                0x090c
#define MC_MIX_RATIO1                0x090d
#define MC_DP_MB_XY                  0x090e
#define MC_OM_MB_XY                  0x090f
#define PSCALE_RST                   0x0910
#define PSCALE_CTRL                  0x0911
#define PSCALE_PICI_W                0x912
#define PSCALE_PICI_H                0x913
#define PSCALE_PICO_W                0x914
#define PSCALE_PICO_H                0x915
#define PSCALE_BMEM_ADDR             0x091f
#define PSCALE_BMEM_DAT              0x0920

#define PSCALE_RBUF_START_BLKX       0x925
#define PSCALE_RBUF_START_BLKY       0x926
#define PSCALE_PICO_SHIFT_XY         0x928
#define PSCALE_CTRL1                 0x929
#define PSCALE_SRCKEY_CTRL0          0x92a
#define PSCALE_SRCKEY_CTRL1          0x92b
#define PSCALE_CANVAS_RD_ADDR        0x92c
#define PSCALE_CANVAS_WR_ADDR        0x92d
#define PSCALE_CTRL2                 0x92e

/**/
#define MC_MPORT_CTRL                0x0940
#define MC_MPORT_DAT                 0x0941
#define MC_WT_PRED_CTRL              0x0942
#define MC_MBBOT_ST_EVEN_ADDR        0x0944
#define MC_MBBOT_ST_ODD_ADDR         0x0945
#define MC_DPDN_MB_XY                0x0946
#define MC_OMDN_MB_XY                0x0947
#define MC_HCMDBUF_H                 0x0948
#define MC_HCMDBUF_L                 0x0949
#define MC_HCMD_H                    0x094a
#define MC_HCMD_L                    0x094b
#define MC_IDCT_DAT                  0x094c
#define MC_CTRL_GCLK_CTRL            0x094d
#define MC_OTHER_GCLK_CTRL           0x094e
#define MC_CTRL2                     0x094f
#define MDEC_PIC_DC_MUX_CTRL         0x98d
#define MDEC_PIC_DC_CTRL             0x098e
#define MDEC_PIC_DC_STATUS           0x098f
#define ANC0_CANVAS_ADDR             0x0990
#define ANC1_CANVAS_ADDR             0x0991
#define ANC2_CANVAS_ADDR             0x0992
#define ANC3_CANVAS_ADDR             0x0993
#define ANC4_CANVAS_ADDR             0x0994
#define ANC5_CANVAS_ADDR             0x0995
#define ANC6_CANVAS_ADDR             0x0996
#define ANC7_CANVAS_ADDR             0x0997
#define ANC8_CANVAS_ADDR             0x0998
#define ANC9_CANVAS_ADDR             0x0999
#define ANC10_CANVAS_ADDR            0x099a
#define ANC11_CANVAS_ADDR            0x099b
#define ANC12_CANVAS_ADDR            0x099c
#define ANC13_CANVAS_ADDR            0x099d
#define ANC14_CANVAS_ADDR            0x099e
#define ANC15_CANVAS_ADDR            0x099f
#define ANC16_CANVAS_ADDR            0x09a0
#define ANC17_CANVAS_ADDR            0x09a1
#define ANC18_CANVAS_ADDR            0x09a2
#define ANC19_CANVAS_ADDR            0x09a3
#define ANC20_CANVAS_ADDR            0x09a4
#define ANC21_CANVAS_ADDR            0x09a5
#define ANC22_CANVAS_ADDR            0x09a6
#define ANC23_CANVAS_ADDR            0x09a7
#define ANC24_CANVAS_ADDR            0x09a8
#define ANC25_CANVAS_ADDR            0x09a9
#define ANC26_CANVAS_ADDR            0x09aa
#define ANC27_CANVAS_ADDR            0x09ab
#define ANC28_CANVAS_ADDR            0x09ac
#define ANC29_CANVAS_ADDR            0x09ad
#define ANC30_CANVAS_ADDR            0x09ae
#define ANC31_CANVAS_ADDR            0x09af
#define DBKR_CANVAS_ADDR             0x09b0
#define DBKW_CANVAS_ADDR             0x09b1
#define REC_CANVAS_ADDR              0x09b2
#define CURR_CANVAS_CTRL             0x09b3
#define MDEC_PIC_DC_THRESH           0x09b8
#define MDEC_PICR_BUF_STATUS         0x09b9
#define MDEC_PICW_BUF_STATUS         0x09ba
#define MCW_DBLK_WRRSP_CNT           0x09bb
#define MC_MBBOT_WRRSP_CNT           0x09bc
#define MDEC_PICW_BUF2_STATUS        0x09bd
#define WRRSP_FIFO_PICW_DBK          0x09be
#define WRRSP_FIFO_PICW_MC           0x09bf
#define AV_SCRATCH_0                 0x09c0
#define AV_SCRATCH_1                 0x09c1
#define AV_SCRATCH_2                 0x09c2
#define AV_SCRATCH_3                 0x09c3
#define AV_SCRATCH_4                 0x09c4
#define AV_SCRATCH_5                 0x09c5
#define AV_SCRATCH_6                 0x09c6
#define AV_SCRATCH_7                 0x09c7
#define AV_SCRATCH_8                 0x09c8
#define AV_SCRATCH_9                 0x09c9
#define AV_SCRATCH_A                 0x09ca
#define AV_SCRATCH_B                 0x09cb
#define AV_SCRATCH_C                 0x09cc
#define AV_SCRATCH_D                 0x09cd
#define AV_SCRATCH_E                 0x09ce
#define AV_SCRATCH_F                 0x09cf
#define AV_SCRATCH_G                 0x09d0
#define AV_SCRATCH_H                 0x09d1
#define AV_SCRATCH_I                 0x09d2
#define AV_SCRATCH_J                 0x09d3
#define AV_SCRATCH_K                 0x09d4
#define AV_SCRATCH_L                 0x09d5
#define AV_SCRATCH_M                 0x09d6
#define AV_SCRATCH_N                 0x09d7
#define WRRSP_CO_MB                  0x09d8
#define WRRSP_DCAC                   0x09d9
/*add from M8M2*/
#define WRRSP_VLD                    0x09da
#define MDEC_DOUBLEW_CFG0            0x09db
#define MDEC_DOUBLEW_CFG1            0x09dc
#define MDEC_DOUBLEW_CFG2            0x09dd
#define MDEC_DOUBLEW_CFG3            0x09de
#define MDEC_DOUBLEW_CFG4            0x09df
#define MDEC_DOUBLEW_CFG5            0x09e0
#define MDEC_DOUBLEW_CFG6            0x09e1
#define MDEC_DOUBLEW_CFG7            0x09e2
#define MDEC_DOUBLEW_STATUS          0x09e3
#define MDEC_EXTIF_CFG0              0x09e4
#define MDEC_EXTIF_CFG1              0x09e5
/*add from t7*/
#define MDEC_EXTIF_CFG2              0x09e6
#define MDEC_EXTIF_STS0              0x09e7
#define MDEC_PICW_BUFDW_CFG0         0x09e8
#define MDEC_PICW_BUFDW_CFG1         0x09e9
#define MDEC_CAV_LUT_DATAL           0x09ea
#define MDEC_CAV_LUT_DATAH           0x09eb
#define MDEC_CAV_LUT_ADDR            0x09ec
#define MDEC_CAV_CFG0                0x09ed

#define HCODEC_MDEC_CAV_LUT_DATAL    0x09ea
#define HCODEC_MDEC_CAV_LUT_DATAH    0x09eb
#define HCODEC_MDEC_CAV_LUT_ADDR     0x09ec
#define HCODEC_MDEC_CAV_CFG0         0x09ed

/**/
#define DBLK_RST                     0x0950
#define DBLK_CTRL                    0x0951
#define DBLK_MB_WID_HEIGHT           0x0952
#define DBLK_STATUS                  0x0953
#define DBLK_CMD_CTRL                0x0954
#define MCRCC_CTL1                   0x0980
#define MCRCC_CTL2                   0x0981
#define MCRCC_CTL3                   0x0982
#define GCLK_EN                      0x0983
#define MDEC_SW_RESET                0x0984
#define VLD_STATUS_CTRL              0x0c00
#define MPEG1_2_REG                  0x0c01
#define F_CODE_REG                   0x0c02
#define PIC_HEAD_INFO                0x0c03
#define SLICE_VER_POS_PIC_TYPE       0x0c04
#define QP_VALUE_REG                 0x0c05
#define MBA_INC                      0x0c06
#define MB_MOTION_MODE               0x0c07
#define POWER_CTL_VLD                0x0c08
#define MB_WIDTH                     0x0c09
#define SLICE_QP                     0x0c0a
#define PRE_START_CODE               0x0c0b
#define SLICE_START_BYTE_01          0x0c0c
#define SLICE_START_BYTE_23          0x0c0d
#define RESYNC_MARKER_LENGTH         0x0c0e
#define DECODER_BUFFER_INFO          0x0c0f
#define FST_FOR_MV_X                 0x0c10
#define FST_FOR_MV_Y                 0x0c11
#define SCD_FOR_MV_X                 0x0c12
#define SCD_FOR_MV_Y                 0x0c13
#define FST_BAK_MV_X                 0x0c14
#define FST_BAK_MV_Y                 0x0c15
#define SCD_BAK_MV_X                 0x0c16
#define SCD_BAK_MV_Y                 0x0c17
#define VLD_DECODE_CONTROL           0x0c18
#define VLD_REVERVED_19              0x0c19
#define VIFF_BIT_CNT                 0x0c1a
#define BYTE_ALIGN_PEAK_HI           0x0c1b
#define BYTE_ALIGN_PEAK_LO           0x0c1c
#define NEXT_ALIGN_PEAK              0x0c1d
#define VC1_CONTROL_REG              0x0c1e
#define PMV1_X                       0x0c20
#define PMV1_Y                       0x0c21
#define PMV2_X                       0x0c22
#define PMV2_Y                       0x0c23
#define PMV3_X                       0x0c24
#define PMV3_Y                       0x0c25
#define PMV4_X                       0x0c26
#define PMV4_Y                       0x0c27
#define M4_TABLE_SELECT              0x0c28
#define M4_CONTROL_REG               0x0c29
#define BLOCK_NUM                    0x0c2a
#define PATTERN_CODE                 0x0c2b
#define MB_INFO                      0x0c2c
#define VLD_DC_PRED                  0x0c2d
#define VLD_ERROR_MASK               0x0c2e
#define VLD_DC_PRED_C                0x0c2f
#define LAST_SLICE_MV_ADDR           0x0c30
#define LAST_MVX                     0x0c31
#define LAST_MVY                     0x0c32
#define VLD_C38                      0x0c38
#define VLD_C39                      0x0c39
#define VLD_STATUS                   0x0c3a
#define VLD_SHIFT_STATUS             0x0c3b
#define VOFF_STATUS                  0x0c3c
#define VLD_C3D                      0x0c3d
#define VLD_DBG_INDEX                0x0c3e
#define VLD_DBG_DATA                 0x0c3f
#define VLD_MEM_VIFIFO_START_PTR     0x0c40
#define VLD_MEM_VIFIFO_CURR_PTR      0x0c41
#define VLD_MEM_VIFIFO_END_PTR       0x0c42
#define VLD_MEM_VIFIFO_BYTES_AVAIL   0x0c43
#define VLD_MEM_VIFIFO_CONTROL       0x0c44
#define VLD_MEM_VIFIFO_WP            0x0c45
#define VLD_MEM_VIFIFO_RP            0x0c46
#define VLD_MEM_VIFIFO_LEVEL         0x0c47
#define VLD_MEM_VIFIFO_BUF_CNTL      0x0c48
#define VLD_TIME_STAMP_CNTL          0x0c49
#define VLD_TIME_STAMP_SYNC_0        0x0c4a
#define VLD_TIME_STAMP_SYNC_1        0x0c4b
#define VLD_TIME_STAMP_0             0x0c4c
#define VLD_TIME_STAMP_1             0x0c4d
#define VLD_TIME_STAMP_2             0x0c4e
#define VLD_TIME_STAMP_3             0x0c4f
#define VLD_TIME_STAMP_LENGTH        0x0c50
#define VLD_MEM_VIFIFO_WRAP_COUNT    0x0c51
#define VLD_MEM_VIFIFO_MEM_CTL       0x0c52
#define VLD_MEM_VBUF_RD_PTR          0x0c53
#define VLD_MEM_VBUF2_RD_PTR         0x0c54
#define VLD_MEM_SWAP_ADDR            0x0c55
#define VLD_MEM_SWAP_CTL             0x0c56
// bit[12]  -- zero_use_cbp_blk
// bit[11]  -- mv_use_abs (only calculate abs)
// bit[10]  -- mv_use_simple_mode (every size count has same weight)
// bit[9]   -- use_simple_mode (every size count has same weight)
// bit[8]   -- reseet_all_count // write only
// bit[7:5] Reserved
// bit[4:0] pic_quality_rd_idx
#define VDEC_PIC_QUALITY_CTRL        0x0c57
// idx  -- read out
//   0  -- blk88_y_count // 4k will use 20 bits
//   1  -- qp_y_sum // 4k use 27 bits
//   2  -- intra_y_oount // 4k use 20 bits
//   3  -- skipped_y_count // 4k use 20 bits
//   4  -- coeff_non_zero_y_count // 4k use 20 bits
//   5  -- blk66_c_count // 4k will use 20 bits
//   6  -- qp_c_sum // 4k use 26 bits
//   7  -- intra_c_oount // 4k use 20 bits
//   8  -- skipped_cu_c_count // 4k use 20 bits
//   9  -- coeff_non_zero_c_count // 4k use 20 bits
//  10  -- { 1'h0, qp_c_max[6:0], 1'h0, qp_c_min[6:0],
//			1'h0, qp_y_max[6:0], 1'h0, qp_y_min[6:0]}
//  11  -- blk22_mv_count
//  12  -- {mvy_L1_count[39:32], mvx_L1_count[39:32],
//			mvy_L0_count[39:32], mvx_L0_count[39:32]}
//  13  -- mvx_L0_count[31:0]
//  14  -- mvy_L0_count[31:0]
//  15  -- mvx_L1_count[31:0]
//  16  -- mvy_L1_count[31:0]
//  17  -- {mvx_L0_max, mvx_L0_min} // format : {sign, abs[14:0]}
//  18  -- {mvy_L0_max, mvy_L0_min}
//  19  -- {mvx_L1_max, mvx_L1_min}
//  20  -- {mvy_L1_max, mvy_L1_min}
#define VDEC_PIC_QUALITY_DATA        0x0c58

#define VCOP_CTRL_REG                0x0e00
#define QP_CTRL_REG                  0x0e01
#define INTRA_QUANT_MATRIX           0x0e02
#define NON_I_QUANT_MATRIX           0x0e03
#define DC_SCALER                    0x0e04
#define DC_AC_CTRL                   0x0e05
#define DC_AC_SCALE_MUL              0x0e06
#define DC_AC_SCALE_DIV              0x0e07
#define POWER_CTL_IQIDCT             0x0e08
#define RV_AI_Y_X                    0x0e09
#define RV_AI_U_X                    0x0e0a
#define RV_AI_V_X                    0x0e0b
#define RV_AI_MB_COUNT               0x0e0c
#define NEXT_INTRA_DMA_ADDRESS       0x0e0d
#define IQIDCT_CONTROL               0x0e0e
#define IQIDCT_DEBUG_INFO_0          0x0e0f
#define DEBLK_CMD                    0x0e10
#define IQIDCT_DEBUG_IDCT            0x0e11
#define DCAC_DMA_CTRL                0x0e12
#define DCAC_DMA_ADDRESS             0x0e13
#define DCAC_CPU_ADDRESS             0x0e14
#define DCAC_CPU_DATA                0x0e15
#define DCAC_MB_COUNT                0x0e16
#define IQ_QUANT                     0x0e17
#define VC1_BITPLANE_CTL             0x0e18

#define DOS_SW_RESET0                0x3f00
#define DOS_GCLK_EN0                 0x3f01
#define DOS_GEN_CTRL0                0x3f02
#define DOS_APB_ERR_CTRL             0x3f03
#define DOS_APB_ERR_STAT             0x3f04
#define DOS_VDEC_INT_EN              0x3f05
#define DOS_HCODEC_INT_EN            0x3f06
#define DOS_SW_RESET1                0x3f07
#define DOS_SW_RESET2                0x3f08
#define DOS_GCLK_EN1                 0x3f09
#define DOS_VDEC2_INT_EN             0x3f0a
#define DOS_VDIN_LCNT                0x3f0b
#define DOS_VDIN_FCNT                0x3f0c
#define DOS_VDIN_CCTL                0x3f0d
#define DOS_SCRATCH0                 0x3f10
#define DOS_SCRATCH1                 0x3f11
#define DOS_SCRATCH2                 0x3f12
#define DOS_SCRATCH3                 0x3f13
#define DOS_SCRATCH4                 0x3f14
#define DOS_SCRATCH5                 0x3f15
#define DOS_SCRATCH6                 0x3f16
#define DOS_SCRATCH7                 0x3f17
#define DOS_SCRATCH8                 0x3f18
#define DOS_SCRATCH9                 0x3f19
#define DOS_SCRATCH10                0x3f1a
#define DOS_SCRATCH11                0x3f1b
#define DOS_SCRATCH12                0x3f1c
#define DOS_SCRATCH13                0x3f1d
#define DOS_SCRATCH14                0x3f1e
#define DOS_SCRATCH15                0x3f1f
#define DOS_SCRATCH16                0x3f20
#define DOS_SCRATCH17                0x3f21
#define DOS_SCRATCH18                0x3f22
#define DOS_SCRATCH19                0x3f23
#define DOS_SCRATCH20                0x3f24
#define DOS_SCRATCH21                0x3f25
#define DOS_SCRATCH22                0x3f26
#define DOS_SCRATCH23                0x3f27
#define DOS_SCRATCH24                0x3f28
#define DOS_SCRATCH25                0x3f29
#define DOS_SCRATCH26                0x3f2a
#define DOS_SCRATCH27                0x3f2b
#define DOS_SCRATCH28                0x3f2c
#define DOS_SCRATCH29                0x3f2d
#define DOS_SCRATCH30                0x3f2e
#define DOS_SCRATCH31                0x3f2f
#define DOS_MEM_PD_VDEC              0x3f30
#define DOS_MEM_PD_VDEC2             0x3f31
#define DOS_MEM_PD_HCODEC            0x3f32
/*add from M8M2*/
#define DOS_MEM_PD_HEVC              0x3f33
#define DOS_SW_RESET3                0x3f34
#define DOS_GCLK_EN3                 0x3f35
#define DOS_HEVC_INT_EN              0x3f36

/**/
#define DOS_SW_RESET4                0x3f37
#define DOS_GCLK_EN4                 0x3f38
#define DOS_MEM_PD_WAVE420L          0x3f39
#define DOS_WAVE420L_CNTL_STAT       0x3f3a

/**/
#define DOS_VDEC_MCRCC_STALL_CTRL    0x3f40
#define DOS_VDEC_MCRCC_STALL2_CTRL   0x3f42
#define DOS_VDEC2_MCRCC_STALL_CTRL   0x3f41
#define DOS_VDEC2_MCRCC_STALL2_CTRL  0x3f43
#endif

