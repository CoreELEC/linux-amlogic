/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_HDR10_PLUS_H
#define _AML_HDR10_PLUS_H

struct vframe_hdr10p_sei {
	u16 present_flag;
	u16 itu_t_t35_country_code;
	u16 itu_t_t35_terminal_provider_code;
	u16 itu_t_t35_terminal_provider_oriented_code;
	u16 application_identifier;
	u16 application_version;
	/*num_windows max is 3*/
	u16 num_windows;
	/*windows xy*/
	u16 window_upper_left_corner_x[3];
	u16 window_upper_left_corner_y[3];
	u16 window_lower_right_corner_x[3];
	u16 window_lower_right_corner_y[3];
	u16 center_of_ellipse_x[3];
	u16 center_of_ellipse_y[3];
	u16 rotation_angle[3];
	u16 semimajor_axis_internal_ellipse[3];
	u16 semimajor_axis_external_ellipse[3];
	u16 semiminor_axis_external_ellipse[3];
	u16 overlap_process_option[3];
	/*target luminance*/
	u32 tgt_sys_disp_max_lumi;
	u16 tgt_sys_disp_act_pk_lumi_flag;
	u16 num_rows_tgt_sys_disp_act_pk_lumi;
	u16 num_cols_tgt_sys_disp_act_pk_lumi;
	u16 tgt_sys_disp_act_pk_lumi[25][25];

	/*num_windows max is 3, e.g maxscl[num_windows][i];*/
	u32 maxscl[3][3];
	u32 average_maxrgb[3];
	u16 num_distribution_maxrgb_percentiles[3];
	u16 distribution_maxrgb_percentages[3][15];
	u32 distribution_maxrgb_percentiles[3][15];
	u16 fraction_bright_pixels[3];

	u16 mast_disp_act_pk_lumi_flag;
	u16 num_rows_mast_disp_act_pk_lumi;
	u16 num_cols_mast_disp_act_pk_lumi;
	u16 mast_disp_act_pk_lumi[25][25];
	/*num_windows max is 3, e.g knee_point_x[num_windows]*/
	u16 tone_mapping_flag[3];
	u16 knee_point_x[3];
	u16 knee_point_y[3];
	u16 num_bezier_curve_anchors[3];
	u16 bezier_curve_anchors[3][15];
	u16 color_saturation_mapping_flag[3];
	u16 color_saturation_weight[3];
};

struct hdr_plus_bits_s {
	u16 len_itu_t_t35_country_code;
	u16 len_itu_t_t35_terminal_provider_code;
	u16 len_itu_t_t35_terminal_provider_oriented_code;
	u16 len_application_identifier;
	u16 len_application_version;
	/*num_windows max is 3*/
	u16 len_num_windows;
	/*windows xy*/
	u16 len_window_upper_left_corner_x;
	u16 len_window_upper_left_corner_y;
	u16 len_window_lower_right_corner_x;
	u16 len_window_lower_right_corner_y;
	u16 len_center_of_ellipse_x;
	u16 len_center_of_ellipse_y;
	u16 len_rotation_angle;
	u16 len_semimajor_axis_internal_ellipse;
	u16 len_semimajor_axis_external_ellipse;
	u16 len_semiminor_axis_external_ellipse;
	u16 len_overlap_process_option;
	/*target luminance*/
	u16 len_tgt_sys_disp_max_lumi;
	u16 len_tgt_sys_disp_act_pk_lumi_flag;
	u16 len_num_rows_tgt_sys_disp_act_pk_lumi;
	u16 len_num_cols_tgt_sys_disp_act_pk_lumi;
	u16 len_tgt_sys_disp_act_pk_lumi;

	/*num_windows max is 3, e.g maxscl[num_windows][i];*/
	u16 len_maxscl;
	u16 len_average_maxrgb;
	u16 len_num_distribution_maxrgb_percentiles;
	u16 len_distribution_maxrgb_percentages;
	u16 len_distribution_maxrgb_percentiles;
	u16 len_fraction_bright_pixels;

	u16 len_mast_disp_act_pk_lumi_flag;
	u16 len_num_rows_mast_disp_act_pk_lumi;
	u16 len_num_cols_mast_disp_act_pk_lumi;
	u16 len_mast_disp_act_pk_lumi;
	/*num_windows max is 3, e.g knee_point_x[num_windows]*/
	u16 len_tone_mapping_flag;
	u16 len_knee_point_x;
	u16 len_knee_point_y;
	u16 len_num_bezier_curve_anchors;
	u16 len_bezier_curve_anchors;
	u16 len_color_saturation_mapping_flag;
	u16 len_color_saturation_weight;
};

extern uint debug_hdr;
extern struct vframe_hdr10p_sei hdr10p_sei;

#define HDR_PLUS_IEEE_OUI 0x90848B
#define SEI_SYNTAX 0x4

void hdr10p_hdmitx_vsif_parser(struct vframe_s *vf,
	struct hdr10plus_para *hdmitx_hdr10plus_param);
void hdr10p_parser_metadata(struct vframe_s *vf);
void hdr10p_debug(void);

#endif

