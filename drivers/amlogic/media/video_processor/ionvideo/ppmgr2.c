/*
 * drivers/amlogic/media/video_processor/ionvideo/ppmgr2.c
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

#include "ionvideo.h"

#define IONVIDEO_CANVAS_COUNT 5
int ionvideo_canvas_id[IONVIDEO_CANVAS_COUNT] = {-1, -1, -1, -1, -1};

void ionvideo_alloc_canvas(void)
{
	const char *owner = "ionvideo";
	int i;

	if (ionvideo_canvas_id[0] != -1 &&
	    ionvideo_canvas_id[1] != -1 &&
	    ionvideo_canvas_id[2] != -1 &&
	    ionvideo_canvas_id[3] != -1 &&
	    ionvideo_canvas_id[4] != -1)
		return;

	for (i = 0; i < IONVIDEO_CANVAS_COUNT; i++) {
		if (ionvideo_canvas_id[i] == -1)
			ionvideo_canvas_id[i]
				= canvas_pool_map_alloc_canvas(owner);
	}

	for (i = 0; i < IONVIDEO_CANVAS_COUNT; i++) {
		if (ionvideo_canvas_id[i] == -1) {
			pr_err("ionvideo_alloc_canvas %x, %x, %x, %x, %x\n",
			       ionvideo_canvas_id[0],
			       ionvideo_canvas_id[1],
			       ionvideo_canvas_id[2],
			       ionvideo_canvas_id[3],
			       ionvideo_canvas_id[4]);
			break;
		}
	}
}

void ionvideo_free_canvas(void)
{
	int i;

	for (i = 0; i < IONVIDEO_CANVAS_COUNT; i++) {
		if (ionvideo_canvas_id[i] != -1) {
			canvas_pool_map_free_canvas(ionvideo_canvas_id[i]);
			ionvideo_canvas_id[i] = -1;
		}
	}
}

static inline void paint_mode_convert(int paint_mode, int *src_position,
					int *dst_paint_position,
					int *dst_plane_position)
{

	if (paint_mode == 0) { /* stretch full */
		dst_paint_position[0] = dst_plane_position[0];
		dst_paint_position[1] = dst_plane_position[1];
		dst_paint_position[2] = dst_plane_position[2];
		dst_paint_position[3] = dst_plane_position[3];
	} else if (paint_mode == 1) { /* keep size */
		dst_paint_position[0] =
			(dst_plane_position[2] - src_position[2]) >> 1;
		dst_paint_position[1] =
			(dst_plane_position[3] - src_position[3]) >> 1;
		dst_paint_position[2] = src_position[2];
		dst_paint_position[3] = src_position[3];
	} else if (paint_mode == 2) {
		int dw = 0, dh = 0;

		if (src_position[2] * dst_plane_position[3] >=
			dst_plane_position[2]
			* src_position[3]) { /* crop full */
			dh = dst_plane_position[3];
			dw = dh * src_position[2] / src_position[3];
		} else {
			dw = dst_plane_position[2];
			dh = dw * src_position[3] / src_position[2];
		}
		dst_paint_position[0] = (dst_plane_position[2] - dw) >> 1;
		dst_paint_position[1] = (dst_plane_position[3] - dh) >> 1;
		dst_paint_position[2] = dw;
		dst_paint_position[3] = dh;
	} else if (paint_mode == 3) { /* keep ration black */
		int dw = 0, dh = 0;

		if (src_position[2] * dst_plane_position[3] >=
			dst_plane_position[2] * src_position[3]) {
			dw = dst_plane_position[2];
			dh = dw * src_position[3] / src_position[2];
		} else {
			dh = dst_plane_position[3];
			dw = dh * src_position[2] / src_position[3];
		}
		dst_paint_position[0] = (dst_plane_position[2] - dw) >> 1;
		dst_paint_position[1] = (dst_plane_position[3] - dh) >> 1;
		dst_paint_position[2] = dw;
		dst_paint_position[3] = dh;
	} else if (paint_mode == 4) {
		;
	}
}

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422) {
		if ((vf->type & 3) == VIDTYPE_INTERLACE_BOTTOM) {
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		} else if ((vf->type & 3) == VIDTYPE_INTERLACE_TOP) {
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		} else {
			format = GE2D_FORMAT_S16_YUV422;
		}

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL) {
			if ((vf->bitdepth & BITDEPTH_Y10) &&
				(vf->bitdepth & FULL_PACK_422_MODE))
				format = GE2D_FORMAT_S16_10BIT_YUV422;
		}
	} else if (vf->type & VIDTYPE_VIU_NV21) {
		if ((vf->type & 3) == VIDTYPE_INTERLACE_BOTTOM) {
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		} else if ((vf->type & 3) == VIDTYPE_INTERLACE_TOP) {
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		} else {
			format = GE2D_FORMAT_M24_NV21;
		}
	} else {
		if ((vf->type & 3) == VIDTYPE_INTERLACE_BOTTOM) {
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420B & (3 << 3));
		} else if ((vf->type & 3) == VIDTYPE_INTERLACE_TOP) {
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		} else {
			format = GE2D_FORMAT_M24_YUV420;
		}
	}
	return format;
}

static inline void ge2d_src_config(struct vframe_s *vf,
					struct config_para_ex_s *ge2d_config)
{
	struct vframe_s src_vf = *vf;
	struct canvas_s src_cs0, src_cs1, src_cs2;

	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(ionvideo_canvas_id[2],
				     &src_vf.canvas0_config[0]);

		if (src_vf.plane_num > 1)
			canvas_config_config(ionvideo_canvas_id[3],
					     &src_vf.canvas0_config[1]);

		if (src_vf.plane_num > 2)
			canvas_config_config(ionvideo_canvas_id[4],
					     &src_vf.canvas0_config[2]);

		src_vf.canvas0Addr =
			ionvideo_canvas_id[2]
			| (ionvideo_canvas_id[3] << 8)
			| (ionvideo_canvas_id[4] << 16);

		ge2d_config->src_planes[0].addr =
				src_vf.canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				src_vf.canvas0_config[0].width;
		ge2d_config->src_planes[0].h =
				src_vf.canvas0_config[0].height;
		ge2d_config->src_planes[1].addr =
				src_vf.canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				src_vf.canvas0_config[1].width;
		ge2d_config->src_planes[1].h =
				src_vf.canvas0_config[1].height << 1;

		if (src_vf.plane_num == 3) {
			ge2d_config->src_planes[2].addr =
					src_vf.canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
					src_vf.canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
					src_vf.canvas0_config[2].height << 1;
		}
	} else {
		canvas_read(src_vf.canvas0Addr & 0xff, &src_cs0);
		canvas_read(src_vf.canvas0Addr >> 8 & 0xff, &src_cs1);
		canvas_read(src_vf.canvas0Addr >> 16 & 0xff, &src_cs2);
		ge2d_config->src_planes[0].addr = src_cs0.addr;
		ge2d_config->src_planes[0].w = src_cs0.width;
		ge2d_config->src_planes[0].h = src_cs0.height;
		ge2d_config->src_planes[1].addr = src_cs1.addr;
		ge2d_config->src_planes[1].w = src_cs1.width;
		ge2d_config->src_planes[1].h = src_cs1.height;
		ge2d_config->src_planes[2].addr = src_cs2.addr;
		ge2d_config->src_planes[2].w = src_cs2.width;
		ge2d_config->src_planes[2].h = src_cs2.height;
	}
	/* data operating. */
	ge2d_config->alu_const_color = 0; /* 0x000000ff; */
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0; /* 0xff; */

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = src_vf.canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format(&src_vf);
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = src_vf.width;

	if (vf->type & VIDTYPE_INTERLACE)
		ge2d_config->src_para.height = src_vf.height >> 1;
	else
		ge2d_config->src_para.height = src_vf.height;

	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
/* ppmgr2_printk(2, "vf_width is %d , vf_height is %d type:%p\n",
 * vf->width, vf->height, (void *)vf->type);
 */
}

static int ge2d_paint_dst(struct ge2d_context_s *context,
				struct config_para_ex_s *ge2d_config,
				int dst_canvas_id, int dst_pixel_format,
				int *src_position, int *dst_paint_position,
				int *dst_plane_position)
{
	struct canvas_s dst_cd;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = dst_plane_position[0];
	ge2d_config->dst_para.left = dst_plane_position[1];
	ge2d_config->dst_para.width = dst_plane_position[2];
	ge2d_config->dst_para.height = dst_plane_position[3];

	if (dst_pixel_format == GE2D_FORMAT_S8_Y) {
		canvas_read(dst_canvas_id & 0xff, &dst_cd);
		ge2d_config->dst_planes[0].addr = dst_cd.addr;
		ge2d_config->dst_planes[0].w = dst_cd.width;
		ge2d_config->dst_planes[0].h = dst_cd.height;
		ge2d_config->dst_para.canvas_index = dst_canvas_id & 0xff;
		ge2d_config->dst_para.format = dst_pixel_format
			| GE2D_LITTLE_ENDIAN;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			ppmgr2_printk(1, "Ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_position[0], src_position[1],
					src_position[2], src_position[3],
					dst_paint_position[0],
					dst_paint_position[1],
					dst_paint_position[2],
					dst_paint_position[3]);
		canvas_read(dst_canvas_id >> 8 & 0xff, &dst_cd);
		ge2d_config->dst_planes[0].addr = dst_cd.addr;
		ge2d_config->dst_planes[0].w = dst_cd.width;
		ge2d_config->dst_planes[0].h = dst_cd.height;
		ge2d_config->dst_para.canvas_index = dst_canvas_id >> 8 & 0xff;
		ge2d_config->dst_para.format = GE2D_FORMAT_S8_CB
			| GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = dst_paint_position[2] >> 1;
		ge2d_config->dst_para.height = dst_paint_position[3] >> 1;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			ppmgr2_printk(1, "Ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_position[0], src_position[1],
					src_position[2], src_position[3],
					dst_paint_position[0],
					dst_paint_position[1],
					dst_paint_position[2],
					dst_paint_position[3]);

		canvas_read(dst_canvas_id >> 16 & 0xff, &dst_cd);
		ge2d_config->dst_planes[0].addr = dst_cd.addr;
		ge2d_config->dst_planes[0].w = dst_cd.width;
		ge2d_config->dst_planes[0].h = dst_cd.height;
		ge2d_config->dst_para.canvas_index = dst_canvas_id >> 16 & 0xff;
		ge2d_config->dst_para.format = GE2D_FORMAT_S8_CR
			| GE2D_LITTLE_ENDIAN;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			ppmgr2_printk(1, "Ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_position[0], src_position[1],
					src_position[2], src_position[3],
					dst_paint_position[0],
					dst_paint_position[1],
					dst_paint_position[2],
					dst_paint_position[3]);
	} else {
		canvas_read(dst_canvas_id & 0xff, &dst_cd);
		ge2d_config->dst_planes[0].addr = dst_cd.addr;
		ge2d_config->dst_planes[0].w = dst_cd.width;
		ge2d_config->dst_planes[0].h = dst_cd.height;
		ge2d_config->dst_para.format = dst_pixel_format
			| GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.canvas_index = dst_canvas_id;

		if ((dst_paint_position[2] > dst_cd.width)
				|| (dst_paint_position[3] > dst_cd.height)) {
			pr_info("ppmgr2-dev: error: id %d,width %d,height %d, ",
				dst_canvas_id, dst_cd.width, dst_cd.height);
			pr_info("ppmgr2-dev: dst_width %d,dst_height %d\n",
				dst_paint_position[2], dst_paint_position[3]);
			ppmgr2_printk(1, "error case : dst addr:%p\n",
					(void *)dst_cd.addr);
			return -1;
		}
		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			ppmgr2_printk(1, "Ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_position[0], src_position[1],
					src_position[2], src_position[3],
					dst_paint_position[0],
					dst_paint_position[1],
					dst_paint_position[2],
					dst_paint_position[3]);
	}
/* ppmgr2_printk(2, "dst addr:%p w:%d h:%d canvas_id:%p format:%p\n",
 * (void *)dst_cd.addr, dst_cd.width, dst_cd.height,
 * (void *)dst_canvas_id,
 * (void *)ge2d_config->dst_para.format);
 */
	ppmgr2_printk(2, "dst plane w:%d h:%d paint w:%d h:%d\n",
			dst_plane_position[2], dst_plane_position[3],
			dst_paint_position[2], dst_paint_position[3]);

	return 0;
}

static inline void ge2d_mirror_config(int dst_mirror,
					struct config_para_ex_s *ge2d_config)
{
	if (dst_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (dst_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}
}

static inline void ge2d_angle_config(int dst_angle,
					struct config_para_ex_s *ge2d_config)
{
	if (dst_angle == 1) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (dst_angle == 2) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (dst_angle == 3) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else {
		ge2d_config->dst_xy_swap = 0;
	}
}

/*
 * use ppmgr2 need to init struct ge2d_context_s,
 * pixel_format, canvas_width, canvas_height,
 * phy_addr, buffer_size, canvas_number.
 */
int ppmgr2_init(struct ppmgr2_device *ppd)
{
	int i = 0;
	/* switch_mod_gate_by_name("ge2d", 1); */
	ppd->context = create_ge2d_work_queue();
	if (!ppd->context) {
		ppmgr2_printk(1, "create ge2d work queue error!\n");
		return -1;
	}
	ppmgr2_printk(2, "ppmgr2_init!\n");
	ppd->paint_mode = 0;
	ppd->angle = 0;
	ppd->mirror = 0;
	ppd->ge2d_fmt = 0;
	ppd->dst_width = 0;
	ppd->dst_height = 0;
	for (i = 0; i < PPMGR2_MAX_CANVAS; i++) {
		ppd->phy_addr[i] = NULL;
		ppd->canvas_id[i] = -1;
	}
	return 0;
}

int ppmgr2_canvas_config(struct ppmgr2_device *ppd, int index)
{
	int canvas_width;
	int canvas_height;
	void *phy_addr = NULL;

	if (IS_ERR_OR_NULL(ppd)) {
		ppmgr2_printk(1, "ppd is NULL or error\n");
		return -1;
	}

	canvas_width = ppd->dst_buffer_width;
	canvas_height = ppd->dst_buffer_height;

	if (index >= PPMGR2_MAX_CANVAS) {
		pr_info("ppmgr2-dev: canvas index too large! %d>=%d\n",
			index, PPMGR2_MAX_CANVAS);
		return -1;
	}

	phy_addr = ppd->phy_addr[index];

	if (IS_ERR_OR_NULL(phy_addr)) {
		ppmgr2_printk(1, "NULL physical address!\n");
		return -1;
	}

	if (ppd->ge2d_fmt == GE2D_FORMAT_M24_NV21 || ppd->ge2d_fmt
		== GE2D_FORMAT_M24_NV12) {
		canvas_config(ionvideo_canvas_id[0], (ulong)phy_addr,
			      canvas_width, canvas_height, CANVAS_ADDR_NOWRAP,
			      CANVAS_BLKMODE_LINEAR);
		canvas_config(
			ionvideo_canvas_id[1],
			(ulong)(phy_addr + (canvas_width * canvas_height)),
			canvas_width, canvas_height >> 1, CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
		ppd->canvas_id[index] = ionvideo_canvas_id[0]
			| (ionvideo_canvas_id[1] << 8);
	} else {
		int bpp = 0;

		if (ppd->ge2d_fmt == GE2D_FORMAT_S32_ABGR) {
			bpp = 4;
		} else if (ppd->ge2d_fmt == GE2D_FORMAT_S24_BGR
			|| ppd->ge2d_fmt == GE2D_FORMAT_S24_RGB) {
			bpp = 3;
		} else if (ppd->ge2d_fmt == GE2D_FORMAT_S16_RGB_565) {
			bpp = 2;
		} else {
			ppmgr2_printk(1, "Not support format!\n");
			return -1;
		}
		canvas_config(ionvideo_canvas_id[0], (ulong)phy_addr,
			      canvas_width * bpp, canvas_height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		ppd->canvas_id[index] = ionvideo_canvas_id[0];

	}
	ppmgr2_printk(2, "canvas[%d] phy_addr:%p width:%d height:%d\n", index,
			phy_addr, canvas_width, canvas_height);

	return 0;
}

void ppmgr2_set_angle(struct ppmgr2_device *ppd, int angle)
{
	ppd->angle = angle;
}

void ppmgr2_set_mirror(struct ppmgr2_device *ppd, int mirror)
{
	ppd->mirror = mirror;
}

void ppmgr2_set_paint_mode(struct ppmgr2_device *ppd, int paint_mode)
{
	ppd->paint_mode = paint_mode;
}

int ppmgr2_process(struct vframe_s *vf, struct ppmgr2_device *ppd, int index)
{
	int ret = 0;
	struct vframe_s *src_vf = vf;
	int src_position[4] = {0};
	int dst_paint_position[4] = {0}, dst_plane_position[4] = {0};
	int dst_pixel_format = ppd->ge2d_fmt;
	struct ge2d_context_s *context = ppd->context;
	struct config_para_ex_s *ge2d_config = &(ppd->ge2d_config);
	int angle = (ppd->angle + src_vf->orientation) % 4;
	int dst_canvas_id = 0;

	if (ppd->phy_addr[index] == NULL) {
		ppmgr2_printk(1, "NULL physical address!\n");
		return -1;
	}

	if (src_vf->type & VIDTYPE_INTERLACE) {
		if ((ppd->bottom_first && src_vf->type & 0x2)
			|| (ppd->bottom_first == 0
			&& (src_vf->type & 0x2) == 0)) {
			return -EAGAIN;
		}
	}

	mutex_lock(ppd->ge2d_canvas_mutex);
	ionvideo_alloc_canvas();
	ppmgr2_canvas_config(ppd, index);
	dst_canvas_id = ppd->canvas_id[index];
	src_position[0] = 0;
	src_position[1] = 0;
	src_position[2] = src_vf->width;
	src_position[3] = src_vf->height;
	if (src_position[2] == 0 || src_position[3] == 0) {
		ppmgr2_printk(1, "Source frame error!\n");
		mutex_unlock(ppd->ge2d_canvas_mutex);
		return -1;
	}
	dst_plane_position[0] = 0;
	dst_plane_position[1] = 0;
	dst_plane_position[2] = ppd->dst_width;
	dst_plane_position[3] = ppd->dst_height;

	ge2d_src_config(src_vf, ge2d_config);

	ge2d_mirror_config(ppd->mirror, ge2d_config);
	ge2d_angle_config(angle, ge2d_config);
	paint_mode_convert(ppd->paint_mode, src_position, dst_paint_position,
				dst_plane_position);

	if (src_vf->type & VIDTYPE_INTERLACE)
		src_position[3] = src_vf->height >> 1;

	ret = ge2d_paint_dst(context, ge2d_config, dst_canvas_id,
				dst_pixel_format, src_position,
				dst_paint_position, dst_plane_position);

	mutex_unlock(ppd->ge2d_canvas_mutex);

	return ret;
}

void ppmgr2_release(struct ppmgr2_device *ppd)
{
	if (ppd->context)
		destroy_ge2d_work_queue(ppd->context);

	/* switch_mod_gate_by_name("ge2d", 0); */
	ppmgr2_printk(2, "ppmgr2_release!\n");
}

int v4l_to_ge2d_format(int v4l2_format)
{
	int format = GE2D_FORMAT_M24_NV21;

	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB32:
		format = GE2D_FORMAT_S32_ABGR;
		break;
	case V4L2_PIX_FMT_RGB565:
		format = GE2D_FORMAT_S16_RGB_565;
		break;
	case V4L2_PIX_FMT_BGR24:
		format = GE2D_FORMAT_S24_RGB;
		break;
	case V4L2_PIX_FMT_RGB24:
		format = GE2D_FORMAT_S24_BGR;
		break;
	case V4L2_PIX_FMT_NV12:
		format = GE2D_FORMAT_M24_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
		format = GE2D_FORMAT_M24_NV21;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		format = GE2D_FORMAT_S8_Y;
		break;
	default:
		break;
	}
	return format;
}
