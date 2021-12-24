/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_VPU_GLOBAL_MIF_H_
#define _MESON_VPU_GLOBAL_MIF_H_

#define VPP_OSD1_SCALE_CTRL                        0x1a73
#define VPP_OSD2_SCALE_CTRL                        0x1a74
#define VPP_OSD3_SCALE_CTRL                        0x1a75
#define VPP_OSD4_SCALE_CTRL                        0x1a76

#define VPP_VD1_DSC_CTRL                           0x1a83
#define VPP_VD2_DSC_CTRL                           0x1a84
#define VPP_VD3_DSC_CTRL                           0x1a85

#define MALI_AFBCD1_TOP_CTRL                       0x1a55
#define PATH_START_SEL                             0x1a8a

void fix_vpu_clk2_default_regs(void);
void osd_set_vpp_path_default(u32 osd_index, u32 vpp_index);
#endif
