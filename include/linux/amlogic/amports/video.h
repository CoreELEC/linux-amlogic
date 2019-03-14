/* include/linux/amlogic/amports/video.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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
#ifndef VIDEO_HEADER_
#define VIDEO_HEADER_
#ifdef CONFIG_VSYNC_RDMA

int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val,
	u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
u32 RDMA_READ_REG(u32 adr);
int RDMA_SET_READ(u32 adr);
#endif

#define AMVIDEO_MAGIC  'X'

#define AMVIDEO_EXT_GET_CURRENT_VIDEOFRAME _IOR((AMVIDEO_MAGIC), 0x01, u32)
#define AMVIDEO_EXT_PUT_CURRENT_VIDEOFRAME _IO((AMVIDEO_MAGIC), 0x02)

#define AMVIDEO_EXT_CURRENT_VIDEOFRAME_GET_GE2D_FORMAT _IOR((AMVIDEO_MAGIC), 0x03, u32)
#define AMVIDEO_EXT_CURRENT_VIDEOFRAME_GET_SIZE _IOR((AMVIDEO_MAGIC), 0x04, u64)
#define AMVIDEO_EXT_CURRENT_VIDEOFRAME_GET_CANVAS0ADDR _IOR((AMVIDEO_MAGIC), 0x05, u32)
#define AMVIDEO_EXT_CURRENT_VIDEOFRAME_GET_DATA _IOR((AMVIDEO_MAGIC), 0x06, struct amvideo_grabber_data)

void try_free_keep_video(int flags);
void vh265_free_cmabuf(void);
void vh264_4k_free_cmabuf(void);
void vdec_free_cmabuf(void);

#define AMVIDEO_UPDATE_OSD_MODE	0x00000001

#ifdef CONFIG_AM_VIDEO
int amvideo_notifier_call_chain(unsigned long val, void *v);
#else
static inline int amvideo_notifier_call_chain(unsigned long val, void *v)
{
	return 0;
}
#endif
#endif


