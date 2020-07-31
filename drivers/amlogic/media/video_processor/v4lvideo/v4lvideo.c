/*
 * drivers/amlogic/media/video_processor/v4lvideo/v4lvideo.c
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

#define DEBUG
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/cpu_version.h>
#include "v4lvideo.h"
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <linux/platform_device.h>
#include <linux/amlogic/major.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vfm/amlogic_fbc_hook_v1.h>
#include <linux/amlogic/media/di/di.h>
#include "../../common/vfm/vfm.h"

#define V4LVIDEO_MODULE_NAME "v4lvideo"

#define V4LVIDEO_VERSION "1.0"
#define RECEIVER_NAME "v4lvideo"
#define V4LVIDEO_DEVICE_NAME   "v4lvideo"
#define DUR2PTS(x) ((x) - ((x) >> 4))

static atomic_t global_set_cnt = ATOMIC_INIT(0);
static u32 alloc_sei = 1;

#define V4L2_CID_USER_AMLOGIC_V4LVIDEO_BASE  (V4L2_CID_USER_BASE + 0x1100)

static unsigned int video_nr_base = 30;
module_param(video_nr_base, uint, 0644);
MODULE_PARM_DESC(video_nr_base, "videoX start number, 30 is the base nr");

static unsigned int n_devs = 9;

module_param(n_devs, uint, 0644);
MODULE_PARM_DESC(n_devs, "number of video devices to create");

static unsigned int debug;
module_param(debug, uint, 0644);
MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int get_count;
module_param(get_count, uint, 0644);
MODULE_PARM_DESC(get_count, "get_count");

static unsigned int put_count;
module_param(put_count, uint, 0644);
MODULE_PARM_DESC(put_count, "put_count");

static unsigned int q_count;
module_param(q_count, uint, 0644);
MODULE_PARM_DESC(q_count, "q_count");

static unsigned int dq_count;
module_param(dq_count, uint, 0644);
MODULE_PARM_DESC(dq_count, "dq_count");

static unsigned int cts_use_di;
module_param(cts_use_di, uint, 0644);
MODULE_PARM_DESC(cts_use_di, "cts_use_di");

/*set 1 means video_composer use dec vf when di NR; only debug!!!*/
static unsigned int render_use_dec;
module_param(render_use_dec, uint, 0644);
MODULE_PARM_DESC(render_use_dec, "render_use_dec");

static unsigned int dec_count;
module_param(dec_count, uint, 0644);
MODULE_PARM_DESC(dec_count, "dec_count");

static unsigned int vf_dump;
module_param(vf_dump, uint, 0644);
MODULE_PARM_DESC(vf_dump, "vf_dump");

bool di_bypass_p;
/*dec set count mutex*/
struct mutex mutex_dec_count;

#define MAX_KEEP_FRAME 64
#define DI_NR_COUNT 1

struct keep_mem_info {
	void *handle;
	int keep_id;
	int count;
};

struct keeper_mgr {
	/* lock*/
	spinlock_t lock;
	struct keep_mem_info keep_list[MAX_KEEP_FRAME];
};

static struct keeper_mgr keeper_mgr_private;

void v4lvideo_dec_count_increase(void)
{
	mutex_lock(&mutex_dec_count);
	dec_count++;
	if (dec_count > DI_NR_COUNT && !di_bypass_p) {
		dim_polic_cfg(K_DIM_BYPASS_ALL_P, 1);
		di_bypass_p = true;
		pr_info("v4lvideo: di_p bypass 1\n");
	}
	if (dec_count > 9)
		pr_err("v4lvideo: dec count >9\n");
	mutex_unlock(&mutex_dec_count);
}
EXPORT_SYMBOL(v4lvideo_dec_count_increase);

void v4lvideo_dec_count_decrease(void)
{
	mutex_lock(&mutex_dec_count);
	if (dec_count == 0)
		pr_err("v4lvideo: dec count -1\n");
	dec_count--;
	if (dec_count <= DI_NR_COUNT && di_bypass_p) {
		dim_polic_cfg(K_DIM_BYPASS_ALL_P, 0);
		di_bypass_p = false;
		pr_info("v4lvideo: di_p bypass 0\n");
	}
	mutex_unlock(&mutex_dec_count);
}
EXPORT_SYMBOL(v4lvideo_dec_count_decrease);

static struct keeper_mgr *get_keeper_mgr(void)
{
	return &keeper_mgr_private;
}

void keeper_mgr_init(void)
{
	struct keeper_mgr *mgr = get_keeper_mgr();

	memset(mgr, 0, sizeof(struct keeper_mgr));
	spin_lock_init(&mgr->lock);
}

static const struct v4lvideo_fmt formats[] = {
	{.name = "RGB32 (LE)",
	.fourcc = V4L2_PIX_FMT_RGB32, /* argb */
	.depth = 32, },

	{.name = "RGB565 (LE)",
	.fourcc = V4L2_PIX_FMT_RGB565, /* gggbbbbb rrrrrggg */
	.depth = 16, },

	{.name = "RGB24 (LE)",
	.fourcc = V4L2_PIX_FMT_RGB24, /* rgb */
	.depth = 24, },

	{.name = "RGB24 (BE)",
	.fourcc = V4L2_PIX_FMT_BGR24, /* bgr */
	.depth = 24, },

	{.name = "12  Y/CbCr 4:2:0",
	.fourcc = V4L2_PIX_FMT_NV12,
	.depth = 12, },

	{.name = "12  Y/CrCb 4:2:0",
	.fourcc = V4L2_PIX_FMT_NV21,
	.depth = 12, },

	{.name = "YUV420P",
	.fourcc = V4L2_PIX_FMT_YUV420,
	.depth = 12, },

	{.name = "YVU420P",
	.fourcc = V4L2_PIX_FMT_YVU420,
	.depth = 12, }
};

static const struct v4lvideo_fmt *__get_format(u32 pixelformat)
{
	const struct v4lvideo_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

static const struct v4lvideo_fmt *get_format(struct v4l2_format *f)
{
	return __get_format(f->fmt.pix.pixelformat);
}

static void video_keeper_keep_mem(
	void *mem_handle, int type,
	int *id)
{
	int ret;
	int old_id = *id;
	struct keeper_mgr *mgr = get_keeper_mgr();
	int i;
	unsigned long flags;
	int have_samed = 0;
	int keep_id = -1;
	int slot_i = -1;

	if (!mem_handle)
		return;

	spin_lock_irqsave(&mgr->lock, flags);
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		if (!mgr->keep_list[i].handle && slot_i < 0) {
			slot_i = i;
		} else if (mem_handle == mgr->keep_list[i].handle) {
			mgr->keep_list[i].count++;
			have_samed = true;
			keep_id = mgr->keep_list[i].keep_id;
			break;
		}
	}

	if (have_samed) {
		spin_unlock_irqrestore(&mgr->lock, flags);
		*id = keep_id;
		/* pr_info("v4lvideo: keep same mem handle=%p, keep_id=%d\n", */
		/* mem_handle, keep_id); */
		return;
	}
	mgr->keep_list[slot_i].count++;
	if (mgr->keep_list[slot_i].count != 1)
		pr_err("v4lvideo: keep error mem handle=%p\n", mem_handle);
	mgr->keep_list[slot_i].handle = mem_handle;

	spin_unlock_irqrestore(&mgr->lock, flags);

	ret = codec_mm_keeper_mask_keep_mem(mem_handle,
					    type);
	if (ret > 0) {
		if (old_id > 0 && ret != old_id) {
			/* wait 80 ms for vsync post. */
			codec_mm_keeper_unmask_keeper(old_id, 0);
		}
		*id = ret;
	}
	mgr->keep_list[slot_i].keep_id = *id;
}

static void video_keeper_free_mem(
	int keep_id, int delayms)
{
	struct keeper_mgr *mgr = get_keeper_mgr();
	int i;
	unsigned long flags;
	int need_continue_keep = 0;

	if (keep_id <= 0) {
		pr_err("invalid keepid %d\n", keep_id);
		return;
	}

	spin_lock_irqsave(&mgr->lock, flags);
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		if (keep_id == mgr->keep_list[i].keep_id) {
			mgr->keep_list[i].count--;
			if (mgr->keep_list[i].count > 0) {
				need_continue_keep = 1;
				break;
			} else if (mgr->keep_list[i].count == 0) {
				mgr->keep_list[i].keep_id = -1;
				mgr->keep_list[i].handle = NULL;
			} else
				pr_err("v4lvideo: free mem err count =%d\n",
				       mgr->keep_list[i].count);
		}
	}
	spin_unlock_irqrestore(&mgr->lock, flags);

	if (need_continue_keep) {
		pr_info("v4lvideo: need_continue_keep keep_id=%d\n", keep_id);
		return;
	}
	codec_mm_keeper_unmask_keeper(keep_id, delayms);
}

static void vf_keep(struct file_private_data *file_private_data)
{
	struct vframe_s *vf_p;
	struct vframe_s *vf_ext_p = NULL;
	int type = MEM_TYPE_CODEC_MM;
	int keep_id = 0;
	int keep_head_id = 0;

	if (!file_private_data) {
		V4LVID_ERR("vf_keep error: file_private_data is NULL");
		return;
	}
	vf_p = file_private_data->vf_p;

	if (!vf_p) {
		V4LVID_ERR("vf_keep error: vf_p is NULL");
		return;
	}

	if (file_private_data->flag & V4LVIDEO_FLAG_DI_NR) {
		vf_ext_p = file_private_data->vf_ext_p;
		if (!vf_ext_p) {
			V4LVID_ERR("vf_keep error: vf_ext is NULL");
			return;
		}
		vf_p = vf_ext_p;
	}

	if (vf_p->type & VIDTYPE_SCATTER)
		type = MEM_TYPE_CODEC_MM_SCATTER;
	video_keeper_keep_mem(
		vf_p->mem_handle,
		type,
		&keep_id);
	video_keeper_keep_mem(
		vf_p->mem_head_handle,
		MEM_TYPE_CODEC_MM,
		&keep_head_id);

	file_private_data->keep_id = keep_id;
	file_private_data->keep_head_id = keep_head_id;
	file_private_data->is_keep = true;
}

static void vf_free(struct file_private_data *file_private_data)
{
	struct vframe_s *vf_p;
	struct vframe_s *vf;
	u32 flag;
	if (file_private_data->keep_id > 0) {
		video_keeper_free_mem(
				file_private_data->keep_id, 0);
		file_private_data->keep_id = -1;
	}
	if (file_private_data->keep_head_id > 0) {
		video_keeper_free_mem(
				file_private_data->keep_head_id, 0);
		file_private_data->keep_head_id = -1;
	}

	vf = &file_private_data->vf;
	vf_p = file_private_data->vf_p;
	flag = file_private_data->flag;
	if (flag & V4LVIDEO_FLAG_DI_DEC) {
		vf = &file_private_data->vf_ext;
		vf_p = file_private_data->vf_ext_p;
	}

	if (vf->type & VIDTYPE_DI_PW)
		dim_post_keep_cmd_release2(vf_p);
}

void init_file_private_data(struct file_private_data *file_private_data)
{
	if (file_private_data) {
		memset(&file_private_data->vf, 0, sizeof(struct vframe_s));
		memset(&file_private_data->vf_ext, 0, sizeof(struct vframe_s));
		file_private_data->vf_p = NULL;
		file_private_data->vf_ext_p = NULL;
		file_private_data->is_keep = false;
		file_private_data->keep_id = -1;
		file_private_data->keep_head_id = -1;
		file_private_data->file = NULL;
		file_private_data->flag = 0;
	} else {
		V4LVID_ERR("init_file_private_data is NULL!!");
	}
}

static DEFINE_SPINLOCK(devlist_lock);
static unsigned long v4lvideo_devlist_lock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&devlist_lock, flags);
	return flags;
}

static void v4lvideo_devlist_unlock(unsigned long flags)
{
	spin_unlock_irqrestore(&devlist_lock, flags);
}

static LIST_HEAD(v4lvideo_devlist);

int v4lvideo_assign_map(char **receiver_name, int *inst)
{
	unsigned long flags;
	struct v4lvideo_dev *dev = NULL;
	struct list_head *p;

	flags = v4lvideo_devlist_lock();

	list_for_each(p, &v4lvideo_devlist) {
		dev = list_entry(p, struct v4lvideo_dev, v4lvideo_devlist);

		if (dev->inst == *inst) {
			*receiver_name = dev->vf_receiver_name;
			v4lvideo_devlist_unlock(flags);
			return 0;
		}
	}

	v4lvideo_devlist_unlock(flags);

	return -ENODEV;
}
EXPORT_SYMBOL(v4lvideo_assign_map);

int v4lvideo_alloc_map(int *inst)
{
	unsigned long flags;
	struct v4lvideo_dev *dev = NULL;
	struct list_head *p;

	flags = v4lvideo_devlist_lock();

	list_for_each(p, &v4lvideo_devlist) {
		dev = list_entry(p, struct v4lvideo_dev, v4lvideo_devlist);

		if ((dev->inst >= 0) && (!dev->mapped)) {
			dev->mapped = true;
			*inst = dev->inst;
			v4lvideo_devlist_unlock(flags);
			return 0;
		}
	}

	v4lvideo_devlist_unlock(flags);
	return -ENODEV;
}

void v4lvideo_release_map(int inst)
{
	unsigned long flags;
	struct v4lvideo_dev *dev = NULL;
	struct list_head *p;

	flags = v4lvideo_devlist_lock();

	list_for_each(p, &v4lvideo_devlist) {
		dev = list_entry(p, struct v4lvideo_dev, v4lvideo_devlist);
		if ((dev->inst == inst) && (dev->mapped)) {
			dev->mapped = false;
			pr_debug("v4lvideo_release_map %d OK\n", dev->inst);
			break;
		}
	}

	v4lvideo_devlist_unlock(flags);
}

void v4lvideo_release_map_force(struct v4lvideo_dev *dev)
{
	unsigned long flags;

	flags = v4lvideo_devlist_lock();

	if (dev->mapped) {
		dev->mapped = false;
		pr_debug("v4lvideo_release_map %d OK\n", dev->inst);
	}

	v4lvideo_devlist_unlock(flags);
}

static struct ge2d_context_s *context;
static int canvas_src_id[3];
static int canvas_dst_id[3];

static int get_source_type(struct vframe_s *vf)
{
	enum vframe_source_type ret;
	int interlace_mode;

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if ((vf->source_type == VFRAME_SOURCE_TYPE_HDMI) ||
	    (vf->source_type == VFRAME_SOURCE_TYPE_CVBS)) {
		if ((vf->bitdepth & BITDEPTH_Y10) &&
		    (!(vf->type & VIDTYPE_COMPRESS)) &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL))
			ret = VDIN_10BIT_NORMAL;
		else
			ret = VDIN_8BIT_NORMAL;
	} else {
		if ((vf->bitdepth & BITDEPTH_Y10) &&
		    (!(vf->type & VIDTYPE_COMPRESS)) &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)) {
			if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_10BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_10BIT_BOTTOM;
			else
				ret = DECODER_10BIT_NORMAL;
		} else {
			if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_8BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_8BIT_BOTTOM;
			else
				ret = DECODER_8BIT_NORMAL;
		}
	}
	return ret;
}

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_YUV420;
	enum vframe_source_type soure_type;

	soure_type = get_source_type(vf);
	switch (soure_type) {
	case DECODER_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case DECODER_8BIT_BOTTOM:
	case DECODER_8BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444T & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420T & (3 << 3));
		break;
	case DECODER_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	case DECODER_10BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422B
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422B
					& (3 << 3));
		}
		break;
	case DECODER_10BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422T
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422T
					& (3 << 3));
		}
		break;
	case VDIN_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case VDIN_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	default:
		format = GE2D_FORMAT_M24_YUV420;
	}
	return format;
}

static void do_vframe_afbc_soft_decode(struct v4l_data_t *v4l_data)
{
	int i, j, ret, y_size;
	short *planes[4];
	short *y_src, *u_src, *v_src, *s2c, *s2c1;
	u8 *tmp, *tmp1;
	u8 *y_dst, *vu_dst;
	int bit_10;
	struct timeval start, end;
	unsigned long time_use = 0;
	struct vframe_s *vf = NULL;
	struct file_private_data *file_private_data;

	file_private_data = v4l_data->file_private_data;
	if (file_private_data->flag & V4LVIDEO_FLAG_DI_NR)
		vf = &file_private_data->vf_ext;
	else
		vf = &file_private_data->vf;

	if ((vf->bitdepth & BITDEPTH_YMASK)  == (BITDEPTH_Y10))
		bit_10 = 1;
	else
		bit_10 = 0;

	y_size = vf->compWidth * vf->compHeight * sizeof(short);
	pr_debug("width: %d, height: %d, compWidth: %u, compHeight: %u.\n",
		vf->width, vf->height, vf->compWidth, vf->compHeight);
	for (i = 0; i < 4; i++) {
		planes[i] = vmalloc(y_size);
		pr_debug("plane %d size: %d, vmalloc addr: %p.\n",
			i, y_size, planes[i]);
	}

	do_gettimeofday(&start);
	ret = AMLOGIC_FBC_vframe_decoder_v1((void **)planes, vf, 0, 0);
	if (ret < 0) {
		pr_err("amlogic_fbc_lib.ko error %d", ret);
		goto free;
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	pr_debug("FBC Decompress time: %ldms\n", time_use);

	y_src = planes[0];
	u_src = planes[1];
	v_src = planes[2];

	y_dst = v4l_data->dst_addr;
	vu_dst = v4l_data->dst_addr + v4l_data->byte_stride * v4l_data->height;

	do_gettimeofday(&start);
	for (i = 0; i < vf->compHeight; i++) {
		for (j = 0; j < vf->compWidth; j++) {
			s2c = y_src + j;
			tmp = (u8 *)(s2c);
			if (bit_10)
				*(y_dst + j) = *s2c >> 2;
			else
				*(y_dst + j) = tmp[0];
		}

			y_dst += v4l_data->byte_stride;
			y_src += vf->compWidth;
	}

	for (i = 0; i < (vf->compHeight / 2); i++) {

		for (j = 0; j < vf->compWidth; j += 2) {
			s2c = v_src + j/2;
			s2c1 = u_src + j/2;
			tmp = (u8 *)(s2c);
			tmp1 = (u8 *)(s2c1);

			if (bit_10) {
				*(vu_dst + j) = *s2c >> 2;
				*(vu_dst + j + 1) = *s2c1 >> 2;
			} else {
				*(vu_dst + j) = tmp[0];
				*(vu_dst + j + 1) = tmp1[0];
			}
		}

			vu_dst += v4l_data->byte_stride;
			u_src += (vf->compWidth / 2);
			v_src += (vf->compWidth / 2);
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	pr_debug("bitblk time: %ldms\n", time_use);

free:
	for (i = 0; i < 4; i++)
		vfree(planes[i]);
}

void v4lvideo_data_copy(struct v4l_data_t *v4l_data)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	char name_buf[32];
	uint32_t write_size;

	struct config_para_ex_s ge2d_config;
	struct canvas_config_s dst_canvas_config[3];
	struct vframe_s *vf = NULL;
	const char *keep_owner = "ge2d_dest_comp";
	bool di_mode = false;
	bool is_10bit = false;
	struct file_private_data *file_private_data;

	file_private_data = v4l_data->file_private_data;
	if (file_private_data->flag & V4LVIDEO_FLAG_DI_NR)
		vf = &file_private_data->vf_ext;
	else
		vf = &file_private_data->vf;

	if (cts_use_di) {
		vf = &file_private_data->vf;
	}
	if ((vf->type & VIDTYPE_COMPRESS)) {
		do_vframe_afbc_soft_decode(v4l_data);
		return;
	}
	is_10bit = vf->bitdepth & BITDEPTH_Y10;
	di_mode = vf->type & VIDTYPE_DI_PW;
	if (is_10bit && (!di_mode)) {
		pr_err("vframe 10bit copy is not supported.\n");
		return;
	}

	memset(&ge2d_config, 0, sizeof(ge2d_config));
	memset(dst_canvas_config, 0, sizeof(dst_canvas_config));

	if (!context)
		context = create_ge2d_work_queue();

	if (context == NULL) {
		pr_err("create_ge2d_work_queue failed.\n");
		return;
	}

	dst_canvas_config[0].phy_addr = v4l_data->phy_addr[0];
	dst_canvas_config[0].width = v4l_data->byte_stride;

	dst_canvas_config[0].height = v4l_data->height;
	dst_canvas_config[0].block_mode = 0;
	dst_canvas_config[0].endian = 0;
	dst_canvas_config[1].phy_addr = v4l_data->phy_addr[0] +
		v4l_data->byte_stride * v4l_data->height;
	dst_canvas_config[1].width = v4l_data->byte_stride;
	dst_canvas_config[1].height = v4l_data->height/2;
	dst_canvas_config[1].block_mode = 0;
	dst_canvas_config[1].endian = 0;

	pr_debug("width:%u, height:%u, widht-align:%u, bytestride: %d, umm-width:%u, umm-height:%u, y_addr:%u, uv_addr:%u.,flag=%d\n",
		 vf->width, vf->height, ALIGN(vf->width, 32),
		 v4l_data->byte_stride, v4l_data->width,
		 v4l_data->height,
		 dst_canvas_config[0].phy_addr,
		 dst_canvas_config[1].phy_addr,
		 vf->flag);

	if (vf->canvas0Addr == (u32)-1) {
		if (canvas_src_id[0] <= 0)
			canvas_src_id[0] =
			canvas_pool_map_alloc_canvas(keep_owner);

		if (canvas_src_id[1] <= 0)
			canvas_src_id[1] =
			canvas_pool_map_alloc_canvas(keep_owner);

		if (canvas_src_id[0] <= 0 || canvas_src_id[1] <= 0) {
			pr_err("canvas pool alloc fail.%d, %d, %d.\n",
			canvas_src_id[0], canvas_src_id[1], canvas_src_id[2]);
			return;
		}

		canvas_config_config(canvas_src_id[0], &vf->canvas0_config[0]);
		canvas_config_config(canvas_src_id[1], &vf->canvas0_config[1]);
		ge2d_config.src_para.canvas_index = canvas_src_id[0] |
			canvas_src_id[1] << 8;
		pr_debug("src index: %d.\n", ge2d_config.src_para.canvas_index);

	} else {
		ge2d_config.src_para.canvas_index = vf->canvas0Addr;
		pr_debug("src1 : %d.\n", ge2d_config.src_para.canvas_index);
	}

	if (canvas_dst_id[0] <= 0)
		canvas_dst_id[0] = canvas_pool_map_alloc_canvas(keep_owner);
	if (canvas_dst_id[1] <= 0)
		canvas_dst_id[1] = canvas_pool_map_alloc_canvas(keep_owner);
	if (canvas_dst_id[0] <= 0 || canvas_dst_id[1] <= 0) {
		pr_err("canvas pool alloc dst fail. %d, %d.\n",
			canvas_dst_id[0], canvas_dst_id[1]);
		return;
	}
	canvas_config_config(canvas_dst_id[0], &dst_canvas_config[0]);
	canvas_config_config(canvas_dst_id[1], &dst_canvas_config[1]);

	ge2d_config.dst_para.canvas_index = canvas_dst_id[0] |
			canvas_dst_id[1] << 8;

	pr_debug("dst canvas index: %d.\n", ge2d_config.dst_para.canvas_index);

	ge2d_config.alu_const_color = 0;
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;
	ge2d_config.dst_xy_swap = 0;

	ge2d_config.src_key.key_enable = 0;
	ge2d_config.src_key.key_mask = 0;
	ge2d_config.src_key.key_mode = 0;

	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = get_input_format(vf);
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.x_rev = 0;
	ge2d_config.src_para.y_rev = 0;
	ge2d_config.src_para.color = 0xffffffff;
	ge2d_config.src_para.top = 0;
	ge2d_config.src_para.left = 0;
	ge2d_config.src_para.width = vf->width;
	if (vf->type & VIDTYPE_INTERLACE)
		ge2d_config.src_para.height = vf->height >> 1;
	else
		ge2d_config.src_para.height = vf->height;
	ge2d_config.src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.x_rev = 0;
	ge2d_config.dst_para.y_rev = 0;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.format = GE2D_FORMAT_M24_NV21 | GE2D_LITTLE_ENDIAN;
	ge2d_config.dst_para.width = vf->width;
	ge2d_config.dst_para.height = vf->height;

	if (ge2d_context_config_ex(context, &ge2d_config) < 0) {
		pr_err("ge2d_context_config_ex error.\n");
		return;
	}

	if (vf->type & VIDTYPE_INTERLACE)
		stretchblt_noalpha(context, 0, 0, vf->width, vf->height / 2,
			0, 0, vf->width, vf->height);
	else
		stretchblt_noalpha(context, 0, 0, vf->width, vf->height,
			0, 0, vf->width, vf->height);

	if (vf_dump) {
		snprintf(name_buf, sizeof(name_buf), "/data/tmp/%d-%d.raw",
				vf->width, vf->height);
		fs = get_fs();
		set_fs(KERNEL_DS);
		pos = 0;
		fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
		if (IS_ERR(fp)) {
			pr_err("create %s fail.\n", name_buf);
		} else {
			write_size = v4l_data->byte_stride *
				v4l_data->height * 3 / 2;
			vfs_write(fp, phys_to_virt(v4l_data->phy_addr[0]),
				write_size, &pos);
			pr_debug("write %u size to file.\n", write_size);
			filp_close(fp, NULL);
		}
		set_fs(fs);
	}
}

struct file_private_data *v4lvideo_get_vf(int fd)
{
	struct file *file_vf = NULL;
	struct file_private_data *file_private_data;

	file_vf = fget(fd);
	if (!file_vf) {
		pr_err("v4lvideo_get_vf file_vf is NULL\n");
		return NULL;
	}
	file_private_data = (struct file_private_data *)file_vf->private_data;
	fput(file_vf);
	return file_private_data;
}

static s32 v4lvideo_release_sei_data(struct vframe_s *vf)
{
	void *p;
	s32 ret = -2;
	u32 size = 0;

	if (!vf)
		return ret;

	p = get_sei_from_src_fmt(vf, &size);
	if (p) {
		vfree(p);
		atomic_dec(&global_set_cnt);
	}
	ret = clear_vframe_src_fmt(vf);
	return ret;
}

static s32 v4lvideo_import_sei_data(
	struct vframe_s *vf,
	struct vframe_s *dup_vf,
	char *provider)
{
	struct provider_aux_req_s req;
	s32 ret = -2;
	char *p;
	u32 try_count = 0;
	u32 max_count = 1;
	bool fmt_update = false;

	if (!vf || !dup_vf || !provider || !alloc_sei)
		return ret;

	if ((!(vf->flag & VFRAME_FLAG_DOUBLE_FRAM)) &&
	    (vf->type & VIDTYPE_DI_PW))
		return ret;

	if (!strcmp(provider, "dvbldec") && dup_vf->omx_index < 2)
		max_count = 10;

	while (try_count++ < max_count) {
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		vf_notify_provider_by_name(
			provider,
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);

		if (req.aux_buf && req.aux_size) {
			p = vmalloc(req.aux_size);
			if (p) {
				memcpy(p, req.aux_buf, req.aux_size);

				ret = update_vframe_src_fmt(dup_vf, (void *)p,
							    (u32)req.aux_size,
							    req.dv_enhance_exist
							    ? true : false,
							    provider, NULL);
				if (!ret) {
				/* FIXME: work around for sei/el out of sync */
					if ((dup_vf->src_fmt.fmt ==
					     VFRAME_SIGNAL_FMT_SDR) &&
					    !strcmp(provider, "dvbldec"))
						dup_vf->src_fmt.fmt =
							VFRAME_SIGNAL_FMT_DOVI;
					atomic_inc(&global_set_cnt);
				} else {
					vfree(p);
				}
			} else {
				ret = update_vframe_src_fmt(dup_vf, NULL,
							    0, false,
							    provider, NULL);
			}
			fmt_update = true;
			break;
		} else {
			if (max_count > 1)
				usleep_range(1000, 2000);
		}
	}
	if (!fmt_update) {
		ret = update_vframe_src_fmt(dup_vf, NULL, 0,
					    false, provider, NULL);
		if ((alloc_sei & 2) && max_count > 1)
			pr_info("try %d, no aux data\n", try_count);
	}

	if ((alloc_sei & 2) && max_count > 1)
		pr_info("sei try_count %d\n", try_count);
	if (alloc_sei & 2)
		pr_info("import sei: provider:%s, vf:%p, dup_vf:%p, req.aux_buf:%p, req.aux_size:%d, req.dv_enhance_exist:%d, vf->src_fmt.fmt:%d\n",
			provider, vf, dup_vf,
			req.aux_buf, req.aux_size,
			req.dv_enhance_exist, dup_vf->src_fmt.fmt);
	return ret;
}
/* ------------------------------------------------------------------
 * DMA and thread functions
 * ------------------------------------------------------------------
 */
unsigned int get_v4lvideo_debug(void)
{
	return debug;
}
EXPORT_SYMBOL(get_v4lvideo_debug);

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	dprintk(dev, 2, "%s\n", __func__);

	dev->provider_name = NULL;

	dprintk(dev, 2, "returning from %s\n", __func__);

	return 0;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	dprintk(dev, 2, "%s\n", __func__);

	/*
	 * Typical driver might need to wait here until dma engine stops.
	 * In this case we can abort imiedetly, so it's just a noop.
	 */
	v4l2q_init(&dev->input_queue, V4LVIDEO_POOL_SIZE,
		   (void **)&dev->v4lvideo_input_queue[0]);
	return 0;
}

/* ------------------------------------------------------------------
 * Videobuf operations
 * ------------------------------------------------------------------
 */

static int vidioc_g_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memcpy(parms->parm.raw_data, (u8 *)&dev->am_parm,
	       sizeof(struct v4l2_amlogic_parm));

	return 0;
}

/* ------------------------------------------------------------------
 * IOCTL vidioc handling
 * ------------------------------------------------------------------
 */
static int vidioc_open(struct file *file)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	if (dev->fd_num > 0) {
		pr_err("vidioc_open error\n");
		return -EBUSY;
	}

	dev->fd_num++;
	dev->vf_wait_cnt = 0;

	v4l2q_init(&dev->input_queue,
		   V4LVIDEO_POOL_SIZE,
		   (void **)&dev->v4lvideo_input_queue[0]);

	//dprintk(dev, 2, "vidioc_open\n");
	V4LVID_DBG("v4lvideo open\n");
	return 0;
}

static int vidioc_close(struct file *file)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	V4LVID_DBG("vidioc_close!!!!\n");
	if (dev->mapped)
		v4lvideo_release_map_force(dev);

	if (dev->fd_num > 0)
		dev->fd_num--;

	return 0;
}

static ssize_t vidioc_read(struct file *file, char __user *data,
			   size_t count, loff_t *ppos)
{
	pr_info("v4lvideo read\n");
	return 0;
}

static int vidioc_querycap(struct file *file,
			   void *priv,
			   struct v4l2_capability *cap)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	strcpy(cap->driver, "v4lvideo");
	strcpy(cap->card, "v4lvideo");
	snprintf(cap->bus_info,
		 sizeof(cap->bus_info),
		 "platform:%s",
		 dev->v4l2_dev.name);
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
				| V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	const struct v4lvideo_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	f->fmt.pix.width = dev->width;
	f->fmt.pix.height = dev->height;

	f->fmt.pix.field = V4L2_FIELD_INTERLACED;
	f->fmt.pix.pixelformat = dev->fmt->fourcc;
	f->fmt.pix.bytesperline = (f->fmt.pix.width * dev->fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	if (dev->fmt->is_yuv)
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	else
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file,
				  void *priv,
				  struct v4l2_format *f)
{
	struct v4lvideo_dev *dev = video_drvdata(file);
	const struct v4lvideo_fmt *fmt;

	fmt = get_format(f);
	if (!fmt) {
		dprintk(dev, 1, "Fourcc format (0x%08x) unknown.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	f->fmt.pix.field = V4L2_FIELD_INTERLACED;
	v4l_bound_align_image(&f->fmt.pix.width,
			      48,
			      MAX_WIDTH,
			      4,
			      &f->fmt.pix.height,
			      32,
			      MAX_HEIGHT,
			      0,
			      0);
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	if (fmt->is_yuv)
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	else
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	f->fmt.pix.priv = 0;
	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct v4lvideo_dev *dev = video_drvdata(file);

	int ret = vidioc_try_fmt_vid_cap(file, priv, f);

	if (ret < 0)
		return ret;

	dev->fmt = get_format(f);
	dev->width = f->fmt.pix.width;
	dev->height = f->fmt.pix.height;
	if ((dev->width == 0) || (dev->height == 0))
		V4LVID_ERR("ion buffer w/h info is invalid!!!!!!!!!!!\n");

	return 0;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct v4lvideo_dev *dev = video_drvdata(file);
	struct vframe_s *vf_p;
	struct vframe_s *vf_ext_p;
	struct file *file_vf = NULL;
	struct file_private_data *file_private_data = NULL;
	u32 flag;

	dev->v4lvideo_input[p->index] = *p;

	q_count++;
	file_vf = fget(p->m.fd);
	if (!file_vf) {
		pr_err("v4lvideo: qbuf fget fail\n");
		return 0;
	}
	file_private_data = (struct file_private_data *)(file_vf->private_data);
	if (!file_private_data) {
		pr_err("v4lvideo: qbuf file_private_data NULL\n");
		return 0;
	}
	vf_p = file_private_data->vf_p;
	vf_ext_p = file_private_data->vf_ext_p;
	flag = file_private_data->flag;

	mutex_lock(&dev->mutex_input);
	if (vf_p) {
		if (file_private_data->is_keep) {
			vf_free(file_private_data);
		} else {
			if (v4l2q_pop_specific(&dev->display_queue,
					       file_private_data)) {
				if (dev->receiver_register) {
					if (flag & V4LVIDEO_FLAG_DI_DEC)
						vf_p = vf_ext_p;
					vf_put(vf_p, dev->vf_receiver_name);
					put_count++;
				} else {
					vf_free(file_private_data);
					pr_err("vidioc_qbuf: vfm is unreg\n");
				}
			} else {
				pr_err("vidioc_qbuf: maybe in unreg\n");
			}
		}
	} else {
		dprintk(dev, 1,
			"vidioc_qbuf: vf is NULL, at the start of playback\n");
	}

	v4lvideo_release_sei_data(&file_private_data->vf);
	init_file_private_data(file_private_data);
	fput(file_vf);

	v4l2q_push(&dev->input_queue, &dev->v4lvideo_input[p->index]);
	mutex_unlock(&dev->mutex_input);

	return 0;
}

static void canvas_to_addr(struct vframe_s *vf)
{
	struct canvas_s src_cs0, src_cs1, src_cs2;

	if (vf->canvas0Addr == (u32)-1)
		return;

	canvas_read(vf->canvas0Addr & 0xff, &src_cs0);
	canvas_read(vf->canvas0Addr >> 8 & 0xff, &src_cs1);
	canvas_read(vf->canvas0Addr >> 16 & 0xff, &src_cs2);

	vf->canvas0_config[0].phy_addr = src_cs0.addr;
	vf->canvas0_config[0].width = src_cs0.width;
	vf->canvas0_config[0].height = src_cs0.height;
	vf->canvas0_config[0].block_mode = src_cs0.blkmode;
	vf->canvas0_config[0].endian = src_cs0.endian;

	vf->canvas0_config[1].phy_addr = src_cs1.addr;
	vf->canvas0_config[1].width = src_cs1.width;
	vf->canvas0_config[1].height = src_cs1.height;
	vf->canvas0_config[1].block_mode = src_cs1.blkmode;
	vf->canvas0_config[1].endian = src_cs1.endian;

	vf->canvas0_config[2].phy_addr = src_cs2.addr;
	vf->canvas0_config[2].width = src_cs2.width;
	vf->canvas0_config[2].height = src_cs2.height;
	vf->canvas0_config[2].block_mode = src_cs2.blkmode;
	vf->canvas0_config[2].endian = src_cs2.endian;

	if ((vf->type & VIDTYPE_MVC) &&
	    (vf->canvas1Addr != (u32)-1)) {
		canvas_read(vf->canvas1Addr & 0xff, &src_cs0);
		canvas_read(vf->canvas1Addr >> 8 & 0xff, &src_cs1);
		canvas_read(vf->canvas1Addr >> 16 & 0xff, &src_cs2);

		vf->canvas1_config[0].phy_addr = src_cs0.addr;
		vf->canvas1_config[0].width = src_cs0.width;
		vf->canvas1_config[0].height = src_cs0.height;
		vf->canvas1_config[0].block_mode = src_cs0.blkmode;
		vf->canvas1_config[0].endian = src_cs0.endian;

		vf->canvas1_config[1].phy_addr = src_cs1.addr;
		vf->canvas1_config[1].width = src_cs1.width;
		vf->canvas1_config[1].height = src_cs1.height;
		vf->canvas1_config[1].block_mode = src_cs1.blkmode;
		vf->canvas1_config[1].endian = src_cs1.endian;

		vf->canvas1_config[2].phy_addr = src_cs2.addr;
		vf->canvas1_config[2].width = src_cs2.width;
		vf->canvas1_config[2].height = src_cs2.height;
		vf->canvas1_config[2].block_mode = src_cs2.blkmode;
		vf->canvas1_config[2].endian = src_cs2.endian;
	}

	if (vf->type & VIDTYPE_VIU_NV21)
		vf->plane_num = 2;
	else if (vf->type & VIDTYPE_VIU_444)
		vf->plane_num = 1;
	else if (vf->type & VIDTYPE_VIU_422)
		vf->plane_num = 1;
	else
		vf->plane_num = 3;

	vf->canvas0Addr = (u32)-1;
	vf->canvas1Addr = (u32)-1;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct v4lvideo_dev *dev = video_drvdata(file);
	struct v4l2_buffer *buf = NULL;
	struct vframe_s *vf;
	struct file *file_vf = NULL;
	struct file_private_data *file_private_data = NULL;
	u64 pts_us64 = 0;
	u64 pts_tmp;
	char *provider_name = NULL;
	struct vframe_s *vf_ext = NULL;

	mutex_lock(&dev->mutex_input);
	buf = v4l2q_peek(&dev->input_queue);
	if (!buf) {
		dprintk(dev, 3, "No active queue to serve\n");
		mutex_unlock(&dev->mutex_input);
		return -EAGAIN;
	}

	vf = vf_peek(dev->vf_receiver_name);
	if (!vf) {
		dev->vf_wait_cnt++;
		mutex_unlock(&dev->mutex_input);
		return -EAGAIN;
	}
	vf = vf_get(dev->vf_receiver_name);
	if (!vf) {
		mutex_unlock(&dev->mutex_input);
		return -EAGAIN;
	}
	if (!dev->provider_name) {
		provider_name = vf_get_provider_name(
			dev->vf_receiver_name);
		while (provider_name) {
			if (!vf_get_provider_name(provider_name))
				break;
			provider_name =
				vf_get_provider_name(provider_name);
		}
		dev->provider_name = provider_name;
		pr_info("v4lvideo: provider name: %s\n",
			dev->provider_name ? dev->provider_name : "NULL");
	}

	get_count++;
	vf->omx_index = dev->frame_num;
	dev->am_parm.signal_type = vf->signal_type;
	dev->am_parm.master_display_colour
		= vf->prop.master_display_colour;

	buf = v4l2q_pop(&dev->input_queue);
	dev->vf_wait_cnt = 0;
	file_vf = fget(buf->m.fd);
	if (!file_vf) {
		mutex_unlock(&dev->mutex_input);
		pr_err("v4lvideo: dqbuf fget fail\n");
		return -EAGAIN;
	}
	file_private_data = (struct file_private_data *)(file_vf->private_data);
	if (!file_private_data) {
		mutex_unlock(&dev->mutex_input);
		pr_err("v4lvideo: file_private_data NULL\n");
		return -EAGAIN;
	}

	if (vf->flag & VFRAME_FLAG_DOUBLE_FRAM) {
		vf_ext = (struct vframe_s *)vf->vf_ext;
		vf_ext->omx_index = vf->omx_index;
		if (render_use_dec) {
			file_private_data->vf = *vf_ext;
			file_private_data->vf_p = vf_ext;
			file_private_data->vf_ext = *vf;
			file_private_data->vf_ext_p = vf;
			file_private_data->flag |= V4LVIDEO_FLAG_DI_DEC;
			canvas_to_addr(&file_private_data->vf);
			canvas_to_addr(&file_private_data->vf_ext);
		} else {
			file_private_data->vf = *vf;
			file_private_data->vf_p = vf;
			file_private_data->vf_ext = *vf_ext;
			file_private_data->vf_ext_p = vf_ext;
			file_private_data->flag |= V4LVIDEO_FLAG_DI_NR;
			canvas_to_addr(&file_private_data->vf);
			canvas_to_addr(&file_private_data->vf_ext);
			file_private_data->vf.canvas0_config[0].block_mode = 0;
			file_private_data->vf.canvas0_config[0].endian = 0;
			file_private_data->vf.vf_ext =
				&(file_private_data->vf_ext);
		}
		file_private_data->vf.vf_ext = &(file_private_data->vf_ext);
	} else {
		file_private_data->vf = *vf;
		file_private_data->vf_p = vf;
		canvas_to_addr(&file_private_data->vf);
	}

	v4lvideo_import_sei_data(
		vf, &file_private_data->vf,
		dev->provider_name);

	//pr_err("dqbuf: file_private_data=%p, vf=%p\n", file_private_data, vf);
	v4l2q_push(&dev->display_queue, file_private_data);
	fput(file_vf);
	mutex_unlock(&dev->mutex_input);

	if (vf->pts_us64) {
		dev->first_frame = 1;
		pts_us64 = vf->pts_us64;
	} else if (dev->first_frame == 0) {
		dev->first_frame = 1;
		pts_us64 = 0;
	} else {
		pts_tmp = DUR2PTS(vf->duration) * 100;
		do_div(pts_tmp, 9);
		pts_us64 = dev->last_pts_us64
			+ pts_tmp;
		pts_tmp = pts_us64 * 9;
		do_div(pts_tmp, 100);
		vf->pts = pts_tmp;
	}
	/*workrun for decoder i pts err, if decoder fix it, this should remove*/
	if ((!(vf->flag & VFRAME_FLAG_DOUBLE_FRAM))
	    && (vf->type & VIDTYPE_DI_PW || vf->type & VIDTYPE_INTERLACE)
	    && (vf->pts_us64 == dev->last_pts_us64)) {
		dprintk(dev, 1, "pts same\n");
		pts_tmp = DUR2PTS(vf->duration) * 100;
		do_div(pts_tmp, 9);
		pts_us64 = dev->last_pts_us64
			+ pts_tmp;
		pts_tmp = pts_us64 * 9;
		do_div(pts_tmp, 100);
		vf->pts = pts_tmp;
	}

	*p = *buf;
	p->timestamp.tv_sec = pts_us64 >> 32;
	p->timestamp.tv_usec = pts_us64 & 0xFFFFFFFF;
	dev->last_pts_us64 = pts_us64;

	if ((vf->type & VIDTYPE_COMPRESS) != 0) {
		p->timecode.type = vf->compWidth;
		p->timecode.flags = vf->compHeight;
	} else {
		p->timecode.type = vf->width;
		p->timecode.flags = vf->height;
	}
	p->sequence = dev->frame_num++;
	//pr_err("dqbuf: frame_num=%d\n", p->sequence);
	dq_count++;
	return 0;
}

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
static const struct v4l2_file_operations v4lvideo_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = vidioc_open,
	.release = vidioc_close,
	.read = vidioc_read,
	.poll = vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2,/* V4L2 ioctl handler */
	.mmap = vb2_fop_mmap,
};

static const struct v4l2_ioctl_ops v4lvideo_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_g_parm = vidioc_g_parm,
};

static const struct video_device v4lvideo_template = {
	.name = "v4lvideo",
	.fops = &v4lvideo_v4l2_fops,
	.ioctl_ops = &v4lvideo_ioctl_ops,
	.release = video_device_release,
};

static int v4lvideo_v4l2_release(void)
{
	struct v4lvideo_dev *dev;
	struct list_head *list;
	unsigned long flags;

	flags = v4lvideo_devlist_lock();

	while (!list_empty(&v4lvideo_devlist)) {
		list = v4lvideo_devlist.next;
		list_del(list);
		v4lvideo_devlist_unlock(flags);

		dev = list_entry(list, struct v4lvideo_dev, v4lvideo_devlist);

		v4l2_info(&dev->v4l2_dev,
			  "unregistering %s\n",
			  video_device_node_name(&dev->vdev));
		video_unregister_device(&dev->vdev);
		v4l2_device_unregister(&dev->v4l2_dev);
		kfree(dev);

		flags = v4lvideo_devlist_lock();
	}
	/* vb2_dma_contig_cleanup_ctx(v4lvideo_dma_ctx); */

	v4lvideo_devlist_unlock(flags);

	return 0;
}

static int video_receiver_event_fun(int type, void *data, void *private_data)
{
	struct v4lvideo_dev *dev = (struct v4lvideo_dev *)private_data;
	struct file_private_data *file_private_data = NULL;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		mutex_lock(&dev->mutex_input);
		dev->receiver_register = false;
		while (!v4l2q_empty(&dev->display_queue)) {
			file_private_data = v4l2q_pop(&dev->display_queue);
			vf_keep(file_private_data);
			/*pr_err("unreg:v4lvideo, keep last frame\n");*/
		}
		mutex_unlock(&dev->mutex_input);
		pr_err("unreg:v4lvideo\n");
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		mutex_lock(&dev->mutex_input);
		v4l2q_init(&dev->display_queue,
			   VF_POOL_SIZE,
			   (void **)&dev->v4lvideo_display_queue[0]);
		dev->receiver_register = true;
		dev->frame_num = 0;
		dev->first_frame = 0;
		dev->last_pts_us64 = U64_MAX;
		mutex_unlock(&dev->mutex_input);
		get_count = 0;
		put_count = 0;
		q_count = 0;
		dq_count = 0;
		pr_err("reg:v4lvideo\n");
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		if (dev->vf_wait_cnt > 1)
			return RECEIVER_INACTIVE;
		return RECEIVER_ACTIVE;
	}
	return 0;
}

static const struct vframe_receiver_op_s video_vf_receiver = {
	.event_cb = video_receiver_event_fun
};

static int __init v4lvideo_create_instance(int inst)
{
	struct v4lvideo_dev *dev;
	struct video_device *vfd;
	int ret;
	unsigned long flags;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	snprintf(dev->v4l2_dev.name,
		 sizeof(dev->v4l2_dev.name),
		 "%s-%03d",
		 V4LVIDEO_MODULE_NAME,
		 inst);
	ret = v4l2_device_register(NULL, &dev->v4l2_dev);
	if (ret)
		goto free_dev;

	dev->fmt = &formats[0];
	dev->width = 640;
	dev->height = 480;
	dev->fd_num = 0;

	vfd = &dev->vdev;
	*vfd = v4lvideo_template;
	vfd->dev_debug = debug;
	vfd->v4l2_dev = &dev->v4l2_dev;

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */
	ret = video_register_device(vfd,
				    VFL_TYPE_GRABBER,
				    inst + video_nr_base);
	if (ret < 0)
		goto unreg_dev;

	video_set_drvdata(vfd, dev);
	dev->inst = inst;
	snprintf(dev->vf_receiver_name,
		 ION_VF_RECEIVER_NAME_SIZE,
		 RECEIVER_NAME ".%x",
		 inst & 0xff);

	vf_receiver_init(&dev->video_vf_receiver,
			 dev->vf_receiver_name,
			 &video_vf_receiver, dev);
	vf_reg_receiver(&dev->video_vf_receiver);
	if (inst == 0 || inst == n_devs - 1)
		v4l2_info(&dev->v4l2_dev,
			  "V4L2 device registered as %s\n",
			  video_device_node_name(vfd));

	/* add to device list */
	flags = v4lvideo_devlist_lock();
	list_add_tail(&dev->v4lvideo_devlist, &v4lvideo_devlist);
	v4lvideo_devlist_unlock(flags);

	mutex_init(&dev->mutex_input);

	return 0;

unreg_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
free_dev:
	kfree(dev);
	return ret;
}

static const struct file_operations v4lvideo_file_fops;

static int v4lvideo_file_release(struct inode *inode, struct file *file)
{
	struct file_private_data *file_private_data = file->private_data;
	/*pr_err("v4lvideo_file_release\n");*/

	if (file_private_data) {
		if (file_private_data->is_keep)
			vf_free(file_private_data);
		v4lvideo_release_sei_data(&file_private_data->vf);
		memset(file_private_data, 0, sizeof(struct file_private_data));
		kfree((u8 *)file_private_data);
		file->private_data = NULL;
	}
	return 0;
}

static const struct file_operations v4lvideo_file_fops = {
	.release = v4lvideo_file_release,
	//.poll = v4lvideo_file_poll,
	//.unlocked_ioctl = v4lvideo_file_ioctl,
	//.compat_ioctl = v4lvideo_file_ioctl,
};

int v4lvideo_alloc_fd(int *fd)
{
	struct file *file = NULL;
	struct file_private_data *private_data = NULL;
	int file_fd = get_unused_fd_flags(O_CLOEXEC);

	if (file_fd < 0) {
		pr_err("v4lvideo_alloc_fd: get unused fd fail\n");
		return -ENODEV;
	}

	private_data = kzalloc(sizeof(*private_data), GFP_KERNEL);
	if (!private_data) {
		put_unused_fd(file_fd);
		pr_err("v4lvideo_alloc_fd: private_date fail\n");
		return -ENOMEM;
	}
	init_file_private_data(private_data);

	file = anon_inode_getfile("v4lvideo_file",
				  &v4lvideo_file_fops,
				  private_data, 0);
	if (IS_ERR(file)) {
		kfree((u8 *)private_data);
		put_unused_fd(file_fd);
		pr_err("v4lvideo_alloc_fd: anon_inode_getfile fail\n");
		return -ENODEV;
	}
	fd_install(file_fd, file);
	*fd = file_fd;
	return 0;
}

static ssize_t sei_cnt_show(
	struct class *class, struct class_attribute *attr, char *buf)
{
	ssize_t r;
	int cnt;

	cnt = atomic_read(&global_set_cnt);
	r = sprintf(buf, "allocated sei buffer cnt: %d\n", cnt);
	return r;
}

static ssize_t sei_cnt_store(
	struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	pr_info("set sei_cnt val:%d\n", val);
	atomic_set(&global_set_cnt, val);
	return count;
}

static ssize_t alloc_sei_show(
	struct class *class, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "alloc sei: %d\n", alloc_sei);
}

static ssize_t alloc_sei_store(
	struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		alloc_sei = val;
	else
		alloc_sei = 0;
	pr_info("set alloc_sei val:%d\n", alloc_sei);
	return count;
}

static struct class_attribute v4lvideo_class_attrs[] = {
	__ATTR(sei_cnt,
	       0664,
	       sei_cnt_show,
	       sei_cnt_store),
	__ATTR(alloc_sei,
	       0664,
	       alloc_sei_show,
	       alloc_sei_store),
	__ATTR_NULL
};

static struct class v4lvideo_class = {
	.name = "v4lvideo",
	.class_attrs = v4lvideo_class_attrs,
};

static int v4lvideo_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int v4lvideo_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long v4lvideo_ioctl(struct file *file,
			   unsigned int cmd,
			   ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case V4LVIDEO_IOCTL_ALLOC_ID:{
			u32 v4lvideo_id = 0;

			ret = v4lvideo_alloc_map(&v4lvideo_id);
			if (ret != 0)
				break;
			put_user(v4lvideo_id, (u32 __user *)argp);
		}
		break;
	case V4LVIDEO_IOCTL_FREE_ID:{
			u32 v4lvideo_id;

			get_user(v4lvideo_id, (u32 __user *)argp);
			v4lvideo_release_map(v4lvideo_id);
		}
		break;
	case V4LVIDEO_IOCTL_ALLOC_FD:{
			u32 v4lvideo_fd = 0;

			ret = v4lvideo_alloc_fd(&v4lvideo_fd);
			if (ret != 0)
				break;
			put_user(v4lvideo_fd, (u32 __user *)argp);
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long v4lvideo_compat_ioctl(struct file *file,
				  unsigned int cmd,
				  ulong arg)
{
	long ret = 0;

	ret = v4lvideo_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif
static const struct file_operations v4lvideo_fops = {
	.owner = THIS_MODULE,
	.open = v4lvideo_open,
	.release = v4lvideo_release,
	.unlocked_ioctl = v4lvideo_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = v4lvideo_compat_ioctl,
#endif
	.poll = NULL,
};

static int __init v4lvideo_init(void)
{
	int ret = -1, i;
	struct device *devp;

	mutex_init(&mutex_dec_count);

	keeper_mgr_init();

	ret = class_register(&v4lvideo_class);
	if (ret < 0)
		return ret;

	ret = register_chrdev(V4LVIDEO_MAJOR, "v4lvideo", &v4lvideo_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for v4lvideo device\n");
		goto error1;
	}

	devp = device_create(&v4lvideo_class,
			     NULL,
			     MKDEV(V4LVIDEO_MAJOR, 0),
			     NULL,
			     V4LVIDEO_DEVICE_NAME);
	if (IS_ERR(devp)) {
		pr_err("failed to create v4lvideo device node\n");
		ret = PTR_ERR(devp);
		return ret;
	}

	if (n_devs <= 0)
		n_devs = 1;
	for (i = 0; i < n_devs; i++) {
		ret = v4lvideo_create_instance(i);
		if (ret) {
			/* If some instantiations succeeded, keep driver */
			if (i)
				ret = 0;
			break;
		}
	}

	if (ret < 0) {
		V4LVID_ERR("v4lvideo: error %d while loading driver\n", ret);
		goto error1;
	}

	return 0;

error1:
	unregister_chrdev(V4LVIDEO_MAJOR, "v4lvideo");
	class_unregister(&v4lvideo_class);
	return ret;
}

static void __exit v4lvideo_exit(void)
{
	v4lvideo_v4l2_release();
	device_destroy(&v4lvideo_class, MKDEV(V4LVIDEO_MAJOR, 0));
	unregister_chrdev(V4LVIDEO_MAJOR, V4LVIDEO_DEVICE_NAME);
	class_unregister(&v4lvideo_class);
}

MODULE_DESCRIPTION("Video Technology Magazine V4l Video Capture Board");
MODULE_AUTHOR("Amlogic, Jintao Xu<jintao.xu@amlogic.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(V4LVIDEO_VERSION);

module_init(v4lvideo_init);
module_exit(v4lvideo_exit);
