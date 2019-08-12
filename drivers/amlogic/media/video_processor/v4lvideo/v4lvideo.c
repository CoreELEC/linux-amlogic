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

#define V4LVIDEO_MODULE_NAME "v4lvideo"

#define V4LVIDEO_VERSION "1.0"
#define RECEIVER_NAME "v4lvideo"
#define V4LVIDEO_DEVICE_NAME   "v4lvideo"
#define DUR2PTS(x) ((x) - ((x) >> 4))

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

#define MAX_KEEP_FRAME 64

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
	struct vframe_s *vf_p = file_private_data->vf_p;
	int type = MEM_TYPE_CODEC_MM;
	int keep_id = 0;
	int keep_head_id = 0;

	if (!file_private_data) {
		V4LVID_ERR("vf_keep error: file_private_data is NULL");
		return;
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
	struct vframe_s vf;

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

	vf = file_private_data->vf;
	vf_p = file_private_data->vf_p;

	if (vf.type & VIDTYPE_DI_PW)
		dim_post_keep_cmd_release2(vf_p);
}

void init_file_private_data(struct file_private_data *file_private_data)
{
	if (file_private_data) {
		memset(&file_private_data->vf, 0, sizeof(struct vframe_s));
		file_private_data->vf_p = NULL;
		file_private_data->is_keep = false;
		file_private_data->keep_id = -1;
		file_private_data->keep_head_id = -1;
		file_private_data->file = NULL;
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

void v4lvideo_data_copy(struct v4l_data_t *v4l_data)
{
	struct vframe_s *vf = NULL;
	char *src_ptr_y = NULL;
	char *src_ptr_uv = NULL;
	char *dst_ptr = NULL;
	u32 src_phy_addr_y;
	u32 src_phy_addr_uv;
	u32 size_y;
	u32 size_uv;
	u32 size_pic;

	vf = v4l_data->vf;
	size_y = vf->width * vf->height;
	size_uv = size_y >> 1;
	size_pic = size_y + size_uv;

	if ((vf->canvas0Addr == vf->canvas1Addr) &&
	    (vf->canvas0Addr != 0) &&
	    (vf->canvas0Addr != -1)) {
		src_phy_addr_y = canvas_get_addr(canvasY(vf->canvas0Addr));
		src_phy_addr_uv = canvas_get_addr(canvasUV(vf->canvas0Addr));
	} else {
		src_phy_addr_y = vf->canvas0_config[0].phy_addr;
		src_phy_addr_uv = vf->canvas0_config[1].phy_addr;
	}
	dst_ptr = v4l_data->dst_addr;

	src_ptr_y = codec_mm_vmap(src_phy_addr_y, size_y);
	if (!src_ptr_y) {
		pr_err("src_phy_addr_y map fail size_y=%d\n", size_y);
		return;
	}
	memcpy(dst_ptr, src_ptr_y, size_y);
	codec_mm_unmap_phyaddr(src_ptr_y);

	src_ptr_uv = codec_mm_vmap(src_phy_addr_uv, size_uv);
	if (!src_ptr_uv) {
		pr_err("src_phy_addr_uv map fail size_uv=%d\n", size_uv);
		return;
	}
	memcpy(dst_ptr + size_y, src_ptr_uv, size_uv);
	codec_mm_unmap_phyaddr(src_ptr_uv);
}

struct vframe_s *v4lvideo_get_vf(int fd)
{
	struct file *file_vf = NULL;
	struct vframe_s *vf = NULL;
	struct file_private_data *file_private_data;

	file_vf = fget(fd);
	file_private_data = (struct file_private_data *)file_vf->private_data;
	vf = &file_private_data->vf;
	fput(file_vf);
	return vf;
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
	struct file *file_vf = NULL;
	struct file_private_data *file_private_data = NULL;

	dev->v4lvideo_input[p->index] = *p;

	file_vf = fget(p->m.fd);
	file_private_data = (struct file_private_data *)(file_vf->private_data);
	vf_p = file_private_data->vf_p;

	mutex_lock(&dev->mutex_input);
	if (vf_p) {
		if (file_private_data->is_keep) {
			vf_free(file_private_data);
		} else {
			if (v4l2q_pop_specific(&dev->display_queue,
					       file_private_data)) {
				if (dev->receiver_register) {
					vf_put(vf_p, dev->vf_receiver_name);
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
	init_file_private_data(file_private_data);
	fput(file_vf);

	v4l2q_push(&dev->input_queue, &dev->v4lvideo_input[p->index]);
	mutex_unlock(&dev->mutex_input);

	return 0;
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

	mutex_lock(&dev->mutex_input);
	buf = v4l2q_peek(&dev->input_queue);
	if (!buf) {
		dprintk(dev, 3, "No active queue to serve\n");
		mutex_unlock(&dev->mutex_input);
		return -EAGAIN;
	}
	mutex_unlock(&dev->mutex_input);

	vf = vf_peek(dev->vf_receiver_name);
	if (!vf) {
		dev->vf_wait_cnt++;
		return -EAGAIN;
	}
	vf = vf_get(dev->vf_receiver_name);
	if (!vf)
		return -EAGAIN;
	vf->omx_index = dev->frame_num;
	dev->am_parm.signal_type = vf->signal_type;
	dev->am_parm.master_display_colour
		= vf->prop.master_display_colour;

	mutex_lock(&dev->mutex_input);
	buf = v4l2q_pop(&dev->input_queue);
	dev->vf_wait_cnt = 0;
	file_vf = fget(buf->m.fd);
	file_private_data = (struct file_private_data *)(file_vf->private_data);
	file_private_data->vf = *vf;
	file_private_data->vf_p = vf;
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
	if ((vf->type & VIDTYPE_DI_PW ||
	     vf->type & VIDTYPE_INTERLACE) &&
	    (vf->pts_us64 == dev->last_pts_us64)) {
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
	.read = vb2_fop_read,
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
		mutex_unlock(&dev->mutex_input);
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
	struct file_private_data *private_date = NULL;
	int file_fd = get_unused_fd_flags(O_CLOEXEC);

	if (file_fd < 0) {
		pr_err("v4lvideo_alloc_fd: get unused fd fail\n");
		return -ENODEV;
	}

	private_date = kzalloc(sizeof(*private_date), GFP_KERNEL);
	if (!private_date) {
		put_unused_fd(file_fd);
		pr_err("v4lvideo_alloc_fd: private_date fail\n");
		return -ENOMEM;
	}
	init_file_private_data(private_date);

	file = anon_inode_getfile("v4lvideo_file",
				  &v4lvideo_file_fops,
				  private_date, 0);
	if (IS_ERR(file)) {
		kfree((u8 *)private_date);
		put_unused_fd(file_fd);
		pr_err("v4lvideo_alloc_fd: anon_inode_getfile fail\n");
		return -ENODEV;
	}
	fd_install(file_fd, file);
	*fd = file_fd;
	return 0;
}

static struct class_attribute ion_video_class_attrs[] = {
};

static struct class v4lvideo_class = {
	.name = "v4lvideo",
	.class_attrs = ion_video_class_attrs,
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
