/*
 * drivers/amlogic/dvb/demux/sc2_demux/ts_output.c
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
#include <linux/vmalloc.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/dvb/dmx.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>

#include "sc2_control.h"
#include "ts_output.h"
#include "mem_desc.h"
#include "../../aucpu/aml_aucpu.h"
#include "../aml_dvb.h"
#include "../dmx_log.h"

#define MAX_TS_PID_NUM              1024
#define MAX_SID_NUM                 64
#define MAX_REMAP_NUM               32
#define MAX_ES_NUM                  64
#define MAX_OUT_ELEM_NUM            128
#define MAX_PCR_NUM                 16

#define MAX_READ_BUF_LEN			(64 * 1024)

static int ts_output_max_pid_num_per_sid = 16;

struct pid_entry {
	u8 used;
	u8 id;
	u16 pid;
	u16 pid_mask;
	u8 pid_user;
	u8 dmx_id;
	struct out_elem *pout;
	struct pid_entry *next;
};

struct out_elem {
	u8 used;
	u8 sid;
	u8 enable;
	enum output_format format;
	enum content_type type;
	int aud_type;

	struct pid_entry *pid_list;
	struct es_entry *es_pes;
	struct chan_id *pchan;
	struct chan_id *pchan1;
	struct pid_entry *pcrpid_list;

	char *cache;
	u16 cache_len;
	u16 remain_len;
	ts_output_cb cb;
	void *udata;
	u16 flush_time_ms;
//      spinlock_t  slock;
	unsigned int wakeup;
	int running;
	wait_queue_head_t wait_queue;
	struct task_struct *out_task;
	struct timer_list out_timer;
	u8 output_mode;

	s32 aucpu_handle;
	u8 aucpu_start;
	unsigned long aucpu_mem_phy;
	unsigned long aucpu_mem;
	unsigned int aucpu_mem_size;
	unsigned int aucpu_read_offset;
};

struct sid_entry {
	int used;
	int pid_entry_begin;
	int pid_entry_num;
};

struct remap_entry {
	int status;
	u8 stream_id;
	int remap_flag;
	int pid_entry;
	int pid;
	int pid_new;
};

struct es_entry {
	u8 used;
	u8 buff_id;
	u8 id;
	u8 dmx_id;
	int status;		//-1:off;
	int pid;
	struct out_elem *pout;
};

struct pcr_entry {
	u8 turn_on;
	u8 stream_id;
	int pcr_pid;
};

static struct pid_entry pid_table[MAX_TS_PID_NUM];
static struct sid_entry sid_table[MAX_SID_NUM];
static struct remap_entry remap_table[MAX_REMAP_NUM];
static struct es_entry es_table[MAX_ES_NUM];
static struct out_elem *out_elem_table;
static struct pcr_entry pcr_table[MAX_PCR_NUM];

#define dprint(fmt, args...) \
	dprintk(LOG_ERROR, debug_ts_output, "ts_output:" fmt, ## args)
#define dprintk_info(fmt, args...) \
	dprintk(LOG_ERROR, debug_ts_output, fmt, ## args)
#define pr_dbg(fmt, args...) \
	dprintk(LOG_DBG, debug_ts_output, "ts_output:" fmt, ## args)

MODULE_PARM_DESC(debug_ts_output, "\n\t\t Enable demux debug information");
static int debug_ts_output;
module_param(debug_ts_output, int, 0644);

MODULE_PARM_DESC(drop_dup, "\n\t\t drop duplicate packet");
static int drop_dup;
module_param(drop_dup, int, 0644);

#define READ_CACHE_SIZE      (188)

static int out_flush_time = 50;

struct out_elem *_find_free_elem(void)
{
	int i = 0;

	for (i = 0; i < MAX_OUT_ELEM_NUM; i++) {
		struct out_elem *pout = &out_elem_table[i];

		if (!pout->used)
			return pout;
	}
	return NULL;
}

static struct pid_entry *_find_pid_entry_slot(struct pid_entry *pid_list,
					      int pid)
{
	struct pid_entry *p = pid_list;

	while (p) {
		if (p->pid == pid)
			return p;
		p = p->next;
	}
	return NULL;
}

static struct pid_entry *_malloc_pid_entry_slot(int sid, int pid)
{
	int i = 0;
	int start = 0;
	int end = 0;
	struct pid_entry *pid_slot;
	int j = 0;
	int jump = 0;
	int row_start = 0;

	if (sid >= MAX_SID_NUM) {
		pr_dbg("%s error sid:%d\n", __func__, sid);
		return NULL;
	}

	start = sid_table[sid].pid_entry_begin;
	end = sid_table[sid].pid_entry_begin + sid_table[sid].pid_entry_num;
	for (i = start; i < end; i++) {
		if (i >= MAX_TS_PID_NUM) {
			pr_dbg("err sid:%d,pid start:%d, num:%d\n", sid,
			       sid_table[sid].pid_entry_begin,
			       sid_table[sid].pid_entry_num);
			return NULL;
		}

		pid_slot = &pid_table[i];

		if (!pid_slot->used) {
			/*check same pid, not at same row(there are 4 pid) */
			row_start = i / 4 * 4;
			for (j = row_start; j < row_start + 4; j++) {
				if (pid_table[j].used &&
				    pid_table[j].pid == pid) {
					jump = 1;
					break;
				}
			}
			if (jump) {
				pr_dbg("sid:%d at pos:%d, find pid:%d\n",
				       sid, j, pid);
				jump = 0;
				continue;
			}
			return pid_slot;
		}
	}
	pr_dbg("err sid:%d,pid start:%d, num:%d\n", sid,
	       sid_table[sid].pid_entry_begin, sid_table[sid].pid_entry_num);

	return NULL;
}

static int _free_pid_entry_slot(struct pid_entry *pid_slot)
{
	if (pid_slot) {
		pid_slot->pid = 0;
		pid_slot->next = NULL;
		pid_slot->pid_mask = 0;
		pid_slot->pid_user = 0;
		pid_slot->used = 0;
	}
	return 0;
}

static void _free_pid_list(struct pid_entry *pid_list)
{
	struct pid_entry *p = pid_list;

	while (p) {
		pid_list = pid_list->next;

		_free_pid_entry_slot(p);

		p = pid_list;
	}
}

static struct es_entry *_malloc_es_entry_slot(void)
{
	int i = 0;

	for (i = 0; i < MAX_ES_NUM; i++) {
		struct es_entry *p = &es_table[i];

		if (p->used == 0) {
			p->used = 1;
			return p;
		}
	}

	return NULL;
}

static int _free_es_entry_slot(struct es_entry *es)
{
	if (es) {
		es->pid = 0;
		es->status = -1;
		es->used = 0;
		es->buff_id = 0;
	}
	return 0;
}

static void _timer_out_func(unsigned long arg)
{
	struct out_elem *pout = (struct out_elem *)arg;
	struct chan_id *pchan;

	if (pout->used && pout->cb) {
		if (pout->pchan1)
			pchan = pout->pchan1;
		else
			pchan = pout->pchan;

		if (SC2_bufferid_recv_data(pchan)) {
			pout->wakeup = 1;
			wake_up_interruptible(&pout->wait_queue);
		}
	}
	mod_timer(&pout->out_timer, jiffies + msecs_to_jiffies(out_flush_time));
}

static int _check_wakeup(struct out_elem *pout)
{
	if (pout->wakeup) {
		pout->wakeup = 0;
		return 1;
	}
	return 0;
}

static int ts_process(struct out_elem *pout)
{
	int ret = 0;
	int len = 0, w_size;
	char *pread;

	if (pout->remain_len == 0) {
		len = MAX_READ_BUF_LEN;
		ret = SC2_bufferid_read(pout->pchan, &pread, len);
		if (ret != 0) {
			w_size = pout->cb(pout, pread, ret, pout->udata);
			pr_dbg("%s send:%d, w:%d wwwwww\n", __func__, ret,
			       w_size);
			pout->remain_len = ret - w_size;
			if (pout->remain_len)
				memcpy(pout->cache, pread + w_size,
				       pout->remain_len);
		}
	} else {
		len = READ_CACHE_SIZE - pout->remain_len;
		ret = SC2_bufferid_read(pout->pchan, &pread, len);
		if (ret != 0) {
			memcpy(pout->cache + pout->remain_len, pread, len);
			pout->remain_len += len;
			if (ret == len) {
				ret = pout->remain_len;
				w_size =
				    pout->cb(pout, pout->cache, ret,
					     pout->udata);
				pr_dbg("%s send:%d, w:%d\n", __func__, ret,
				       w_size);
				pout->remain_len = ret - w_size;
				if (pout->remain_len)
					memmove(pout->cache,
						pout->cache + w_size,
						pout->remain_len);
			} else {
				return -1;
			}
		}
	}
	return 0;
}

static int _task_out_func(void *data)
{
	int timeout = 0;
	int ret = 0;
	int len = 0;
	struct out_elem *pout = (struct out_elem *)data;
	char *pread;

	pr_dbg("%s enter,line:%d\n", __func__, __LINE__);
	while (pout->running == TASK_RUNNING) {
		timeout = wait_event_interruptible_timeout(pout->wait_queue,
							   _check_wakeup(pout),
							   3 * HZ);
		if (timeout <= 0)
			continue;

		if (pout->running != TASK_RUNNING)
			break;

		if (pout->format == TS_FORMAT) {
			ts_process(pout);
		} else {
			len = MAX_READ_BUF_LEN;
			ret = SC2_bufferid_read(pout->pchan, &pread, len);
			if (ret != 0)
				pout->cb(pout, pread, ret, pout->udata);
		}
	}
	pout->running = TASK_IDLE;
	return 0;
}

//> 0: have dirty data
//0: success
//-1: exit
//-2: pid not same
static int get_non_sec_es_header(struct out_elem *pout, char *last_header,
				 char *cur_header,
				 struct dmx_non_sec_es_header *pheader)
{
	char *pts_dts = NULL;
	int ret = 0;
	int header_len = 0;
	int offset = 0;
	int pid = 0;
	unsigned int cur_es_bytes = 0;
	unsigned int last_es_bytes = 0;

	pr_dbg("%s enter\n", __func__);

	header_len = 16;
	offset = 0;
	while (header_len) {
		ret = SC2_bufferid_read(pout->pchan1, &pts_dts, header_len);
		pr_dbg("%s head ret:%d\n", __func__, ret);
		if (ret != 0) {
			memcpy((char *)(cur_header + offset), pts_dts, ret);
			header_len -= ret;
			offset += ret;
		}
		if (pout->running == TASK_DEAD)
			return -1;
	}
	pid = (cur_header[1] & 0x1f) << 8 | cur_header[0];
	if (pout->es_pes->pid != pid) {
		pr_dbg("%s pid diff req pid %d, ret pid:%d\n",
		       __func__, pout->es_pes->pid, pid);
		return -2;
	}
	pr_dbg("cur header: 0x%0x 0x%0x 0x%0x 0x%0x\n",
	       *(unsigned int *)cur_header,
	       *((unsigned int *)cur_header + 1),
	       *((unsigned int *)cur_header + 2),
	       *((unsigned int *)cur_header + 3));

	pr_dbg("last header: 0x%0x 0x%0x 0x%0x 0x%0x\n",
	       *(unsigned int *)last_header,
	       *((unsigned int *)last_header + 1),
	       *((unsigned int *)last_header + 2),
	       *((unsigned int *)last_header + 3));

	cur_es_bytes = cur_header[7] << 24 |
	    cur_header[6] << 16 | cur_header[5] << 8 | cur_header[4];

	//when start, es_bytes not equal 0, there are dirty data
	if (last_header[0] == 0xff && last_header[1] == 0xff) {
		pr_dbg("%s dirty data:%d\n", __func__, cur_es_bytes);
		if (cur_es_bytes != 0)
			return cur_es_bytes;
	}

	pheader->pts_dts_flag = last_header[2] & 0x3;
	pheader->dts = last_header[3] & 0x1;
	pheader->dts <<= 32;
	pheader->dts |= last_header[11] << 24
	    | last_header[10] << 16 | last_header[9] << 8 | last_header[8];
	pheader->pts = last_header[3] >> 1 & 0x1;
	pheader->pts <<= 32;
	pheader->pts |= last_header[15] << 24
	    | last_header[14] << 16 | last_header[13] << 8 | last_header[12];

	last_es_bytes = last_header[7] << 24
	    | last_header[6] << 16 | last_header[5] << 8 | last_header[4];
	if (cur_es_bytes < last_es_bytes)
		pheader->len = 0xffffffff - last_es_bytes + cur_es_bytes;
	else
		pheader->len = cur_es_bytes - last_es_bytes;

	pr_dbg("%s len:%d,cur_es_bytes:%d, last_es_bytes:%d\n",
	       __func__, pheader->len, cur_es_bytes, last_es_bytes);
	pr_dbg("%s exit\n", __func__);
	return 0;
}

static int write_es_data(struct out_elem *pout, struct chan_id *pchan,
			 unsigned int len, unsigned int isdirty)
{
	int ret;
	char *ptmp;

	pr_dbg("%s chan id:%d, len:%d, isdirty:%d\n", __func__,
	       pchan->id, len, isdirty);
	while (len) {
		ret = SC2_bufferid_read(pout->pchan, &ptmp, len);
		if (ret != 0) {
			len -= ret;
			if (!isdirty)
				pout->cb(pout, ptmp, ret, pout->udata);
		}
		if (pout->running == TASK_DEAD)
			return -1;
	}
	pr_dbg("%s exit\n", __func__);
	return 0;
}

static void create_aucpu_inst(struct out_elem *pout)
{
	struct aml_aucpu_strm_buf src;
	struct aml_aucpu_strm_buf dst;
	struct aml_aucpu_inst_config cfg;
	int ret;

	/*now the audio will pass by aucpu */
	if (pout->type == AUDIO_TYPE && pout->aucpu_handle < 0) {
		src.phy_start = pout->pchan->mem_phy;
		src.buf_size = pout->pchan->mem_size;
		src.buf_flags = 0;

		pr_dbg("%s src aucpu phy:0x%lx, size:0x%0x\n",
		       __func__, src.phy_start, src.buf_size);

		pout->aucpu_mem_size = pout->pchan->mem_size;
		ret =
		    _alloc_buff(pout->aucpu_mem_size, 0, &pout->aucpu_mem,
				&pout->aucpu_mem_phy, 0);
		if (ret != 0)
			return;
		pr_dbg("%s dst aucpu mem:0x%lx, phy:0x%lx\n",
		       __func__, pout->aucpu_mem, pout->aucpu_mem_phy);

		dst.phy_start = pout->aucpu_mem_phy;
		dst.buf_size = pout->aucpu_mem_size;
		dst.buf_flags = 0;
		cfg.media_type = pout->aud_type;	/*MEDIA_MPX; */
		cfg.dma_chn_id = pout->pchan->id;
		cfg.config_flags = 0;
		pout->aucpu_handle = aml_aucpu_strm_create(&src, &dst, &cfg);
		if (pout->aucpu_handle < 0)
			dprint("%s create aucpu fail, ret:%d\n",
			       __func__, pout->aucpu_handle);
		else
			dprint("%s create aucpu inst success\n", __func__);

		pout->aucpu_read_offset = 0;
		pout->aucpu_start = 0;
	}
}

static unsigned int aucpu_read_process(struct out_elem *pout,
				       unsigned int w_offset,
				       char **pread, unsigned int len)
{
	unsigned int buf_len = len;
	unsigned int data_len = 0;

	pr_dbg("%s w:0x%0x, r:0x%0x\n", __func__,
	       (u32)w_offset, (u32)(pout->aucpu_read_offset));
	if (w_offset > pout->aucpu_read_offset) {
		data_len = min((w_offset - pout->aucpu_read_offset), buf_len);
		dma_sync_single_for_cpu(aml_get_device(),
					(dma_addr_t)(pout->aucpu_mem_phy +
						      pout->aucpu_read_offset),
					data_len, DMA_FROM_DEVICE);
		*pread = (char *)(pout->aucpu_mem + pout->aucpu_read_offset);
		pout->aucpu_read_offset += data_len;
	} else {
		unsigned int part1_len = 0;

		part1_len = pout->aucpu_mem_size - pout->aucpu_read_offset;
		data_len = min(part1_len, buf_len);
		*pread = (char *)(pout->aucpu_mem + pout->aucpu_read_offset);
		if (data_len < part1_len) {
			dma_sync_single_for_cpu(aml_get_device(), (dma_addr_t)
						(pout->aucpu_mem_phy +
						 pout->aucpu_read_offset),
						data_len, DMA_FROM_DEVICE);
			pout->aucpu_read_offset += data_len;
		} else {
			data_len = part1_len;
			dma_sync_single_for_cpu(aml_get_device(), (dma_addr_t)
						(pout->aucpu_mem_phy +
						 pout->aucpu_read_offset),
						data_len, DMA_FROM_DEVICE);
			pout->aucpu_read_offset = 0;
		}
	}
	pr_dbg("%s request:%d, ret:%d\n", __func__, len, data_len);
	return data_len;
}

static int aucpu_bufferid_read(struct out_elem *pout,
			       char **pread, unsigned int len)
{
	struct aml_aucpu_buf_upd upd;
	unsigned int w_offset = 0;

	if (aml_aucpu_strm_get_dst(pout->aucpu_handle, &upd)
	    >= 0) {
		w_offset = upd.phy_cur_ptr - pout->aucpu_mem_phy;
		if (pout->aucpu_read_offset != w_offset)
			return aucpu_read_process(pout, w_offset, pread, len);
	}
	return 0;
}

static int write_aucpu_es_data(struct out_elem *pout,
			       unsigned int len, unsigned int isdirty)
{
	int ret;
	char *ptmp;

	pr_dbg("%s chan id:%d, len:%d, isdirty:%d\n", __func__,
	       pout->pchan->id, len, isdirty);

	while (len) {
		if (!pout->aucpu_start &&
		    pout->format == ES_FORMAT &&
		    pout->type == AUDIO_TYPE && pout->aucpu_handle >= 0) {
			if (wdma_get_active(pout->pchan->id)) {
				ret = aml_aucpu_strm_start(pout->aucpu_handle);
				if (ret >= 0) {
					pr_dbg("aucpu start success\n");
					pout->aucpu_start = 1;
				} else {
					pr_dbg("aucpu start fail ret:%d\n",
					       ret);
				}
			}

			if (!pout->aucpu_start) {
				if (pout->running == TASK_DEAD)
					return -1;
				continue;
			}
		}
		ret = aucpu_bufferid_read(pout, &ptmp, len);
		if (ret != 0) {
			len -= ret;
			if (!isdirty)
				pout->cb(pout, ptmp, ret, pout->udata);
		} else {
			msleep(20);
		}

		if (pout->running == TASK_DEAD)
			return -1;
	}
	pr_dbg("%s exit\n", __func__);
	return 0;
}

static int write_aucpu_sec_es_data(struct out_elem *pout, struct chan_id *pchan,
				   struct dmx_non_sec_es_header *header)
{
	unsigned int len = header->len;
	struct dmx_sec_es_data sec_es_data;
	char *ptmp;
	int ret;
	unsigned int data_start = 0;
	unsigned int data_end = 0;

	memset(&sec_es_data, 0, sizeof(struct dmx_sec_es_data));
	sec_es_data.pts_dts_flag = header->pts_dts_flag;
	sec_es_data.dts = header->dts;
	sec_es_data.pts = header->pts;
	sec_es_data.buf_start = pout->aucpu_mem;
	sec_es_data.buf_end = pout->aucpu_mem + pout->aucpu_mem_size;

	while (len) {
		if (!pout->aucpu_start &&
		    pout->format == ES_FORMAT &&
		    pout->type == AUDIO_TYPE && pout->aucpu_handle >= 0) {
			if (wdma_get_active(pout->pchan->id)) {
				ret = aml_aucpu_strm_start(pout->aucpu_handle);
				if (ret >= 0) {
					pr_dbg("aucpu start success\n");
					pout->aucpu_start = 1;
				} else {
					pr_dbg("aucpu start fail ret:%d\n",
					       ret);
				}
			}

			if (!pout->aucpu_start) {
				if (pout->running == TASK_DEAD)
					return -1;
				continue;
			}
		}
		ret = aucpu_bufferid_read(pout, &ptmp, len);
		if (ret != 0) {
			if (data_start == 0)
				data_start = (unsigned long)ptmp;
			data_end = (unsigned long)ptmp + len;
			len -= ret;
		} else {
			msleep(20);
		}

		if (pout->running == TASK_DEAD)
			return -1;
	}
	sec_es_data.data_start = data_start;
	sec_es_data.data_end = data_end;
	pout->cb(pout, (char *)&sec_es_data,
		 sizeof(struct dmx_sec_es_data), pout->udata);
	return 0;
}

static int write_sec_video_es_data(struct out_elem *pout, struct chan_id *pchan,
				   struct dmx_non_sec_es_header *header)
{
	unsigned int len = header->len;
	struct dmx_sec_es_data sec_es_data;
	char *ptmp;
	int ret;
	unsigned int data_start = 0;
	unsigned int data_end = 0;

	memset(&sec_es_data, 0, sizeof(struct dmx_sec_es_data));
	sec_es_data.pts_dts_flag = header->pts_dts_flag;
	sec_es_data.dts = header->dts;
	sec_es_data.pts = header->pts;
	sec_es_data.buf_start = pout->pchan->mem;
	sec_es_data.buf_end = pout->pchan->mem + pout->pchan->mem_size;

	while (len) {
		ret = SC2_bufferid_read(pout->pchan, &ptmp, len);
		if (ret != 0) {
			if (data_start == 0)
				data_start = (unsigned long)ptmp;
			data_end = (unsigned long)ptmp + len;
			len -= ret;
		}
		if (pout->running == TASK_DEAD)
			return -1;
	}
	sec_es_data.data_start = data_start;
	sec_es_data.data_end = data_end;
	pout->cb(pout, (char *)&sec_es_data,
		 sizeof(struct dmx_sec_es_data), pout->udata);
	return 0;
}

static int _task_out_func_for_es(void *data)
{
	int timeout = 0;
	int ret = 0;
	struct out_elem *pout = (struct out_elem *)data;
	struct dmx_non_sec_es_header header;
	char cur_header[16];
	char last_header[16];
	unsigned int dirty_len = 0;
	unsigned int es_len = 0;
	char *ptmp;
	char *pcur_header;
	char *plast_header;

	pcur_header = (char *)cur_header;
	plast_header = (char *)last_header;
	memset(&cur_header, 0, sizeof(cur_header));
	memset(&last_header, 0, sizeof(last_header));
	last_header[0] = 0xff;
	last_header[1] = 0xff;
	pr_dbg("%s enter,line:%d\n", __func__, __LINE__);
	while (pout->running == TASK_RUNNING) {
		timeout = wait_event_interruptible_timeout(pout->wait_queue,
							   _check_wakeup(pout),
							   3 * HZ);
		if (timeout <= 0)
			continue;
loop:

		if (pout->running != TASK_RUNNING)
			break;

		ret = get_non_sec_es_header(pout,
					    plast_header, pcur_header, &header);
		if (ret == -1 || ret == -2) {
			continue;
		} else if (ret != 0) {
			dirty_len = ret;
			ret = write_es_data(pout, pout->pchan, dirty_len, 1);
			if (ret != 0)
				break;
			ptmp = plast_header;
			plast_header = pcur_header;
			pcur_header = ptmp;
			goto loop;
		}
		if (header.len == 0) {
			ptmp = plast_header;
			plast_header = pcur_header;
			pcur_header = ptmp;
			pr_dbg("header.len is 0, jump\n");
			goto loop;
		}
		pr_dbg("%s line:%d\n", __func__, __LINE__);
		if (pout->output_mode || pout->pchan->sec_level) {
			if (pout->type == VIDEO_TYPE) {
				ret = write_sec_video_es_data(pout,
							      pout->pchan,
							      &header);
				if (ret != 0)
					break;
			} else {
				//to do for use aucpu to handle non-video data
				if (pout->output_mode) {
					ret =
					    write_aucpu_sec_es_data(pout,
								    pout->pchan,
								    &header);
					if (ret != 0)
						break;
				} else {
					es_len = header.len;
					pr_dbg("aud pdts_flag:%d, pts:0x%lx,",
					       header.pts_dts_flag,
					       (unsigned long)header.pts);
					pr_dbg("dts:0x%lx, len:%d\n",
					       (unsigned long)header.dts,
					       header.len);

					pout->cb(pout, (char *)&header,
						 sizeof(struct
							dmx_non_sec_es_header),
						 pout->udata);
					ret =
					    write_aucpu_es_data(pout, es_len,
								0);
					if (ret != 0)
						break;
				}
			}
		} else {
			es_len = header.len;
			pr_dbg("pdts_flag:%d, pts:0x%lx, dts:0x%lx, len:%d\n",
			       header.pts_dts_flag, (unsigned long)header.pts,
			       (unsigned long)header.dts, header.len);
			pout->cb(pout, (char *)&header,
				 sizeof(struct dmx_non_sec_es_header),
				 pout->udata);
			ret = write_es_data(pout, pout->pchan, es_len, 0);
			if (ret != 0)
				break;
		}
		ptmp = plast_header;
		plast_header = pcur_header;
		pcur_header = ptmp;
		if (SC2_bufferid_recv_data(pout->pchan1))
			goto loop;
	}
	pout->running = TASK_IDLE;
	return 0;
}

/**
 * ts_output_init
 * \param dmxdev_num
 * \param info
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_init(int dmxdev_num, struct sid_info *info)
{
	int i = 0, j = 0;
	struct sid_entry *psid = NULL;
	int times = 0;
	u8 record_sid[32];
	u8 record_sid_num = 0;
	u8 same_flag = 0;

	do {
	} while (!tsout_get_ready() && times++ < 20);

	memset(&sid_table, 0, sizeof(sid_table));
	memset(&record_sid, 0, sizeof(record_sid));

	for (i = 0; i < dmxdev_num; i++) {
		for (j = 0; j < record_sid_num; j++) {
			if (info[i].demod_sid == record_sid[j])
				same_flag = 1;
		}

		if (!same_flag) {
			record_sid[record_sid_num] = info[i].demod_sid;
			record_sid_num++;
		}
		same_flag = 0;
		record_sid[record_sid_num] = info[i].local_sid;
		record_sid_num++;
	}
	ts_output_max_pid_num_per_sid =
	    MAX_TS_PID_NUM / (record_sid_num * 4) * 4;

	for (i = 0; i < record_sid_num; i++) {
		psid = &sid_table[record_sid[i]];
		psid->used = 1;
		psid->pid_entry_begin = ts_output_max_pid_num_per_sid * i;
		psid->pid_entry_num = ts_output_max_pid_num_per_sid;
//              dprint("%s sid:%d,pid start:%d, len:%d\n",
//                     __func__, record_sid[i],
//                     psid->pid_entry_begin, psid->pid_entry_num);
		tsout_config_sid_table(record_sid[i],
				       psid->pid_entry_begin / 4,
				       psid->pid_entry_num / 4);
	}

	memset(&pid_table, 0, sizeof(pid_table));
	for (i = 0; i < MAX_TS_PID_NUM; i++) {
		struct pid_entry *pid_slot = &pid_table[i];

		pid_slot->id = i;
	}

	memset(&es_table, 0, sizeof(es_table));
	for (i = 0; i < MAX_ES_NUM; i++) {
		struct es_entry *es_slot = &es_table[i];

		es_slot->id = i;
	}

	memset(&remap_table, 0, sizeof(remap_table));

	out_elem_table = vmalloc(sizeof(*out_elem_table)
				 * MAX_OUT_ELEM_NUM);
	if (!out_elem_table) {
		dprint("%s malloc fail\n", __func__);
		return -1;
	}
	memset(out_elem_table, 0, sizeof(struct out_elem) * MAX_OUT_ELEM_NUM);
//      memset(&out_elem_table, 0, sizeof(out_elem_table));
	memset(&pcr_table, 0, sizeof(pcr_table));

	return 0;
}

int ts_output_sid_debug(void)
{
	int i = 0;
	int dmxdev_num;
	struct sid_entry *psid = NULL;

	memset(&sid_table, 0, sizeof(sid_table));
	dmxdev_num = 32;
	/*for every dmx dev, it will use 2 sids,
	 * one is demod, another is local
	 */
	ts_output_max_pid_num_per_sid =
	    MAX_TS_PID_NUM / (2 * dmxdev_num * 4) * 4;

	for (i = 0; i < dmxdev_num; i++) {
		psid = &sid_table[i];
		psid->used = 1;
		psid->pid_entry_begin = ts_output_max_pid_num_per_sid * 2 * i;
		psid->pid_entry_num = ts_output_max_pid_num_per_sid;
		pr_dbg("%s sid:%d,pid start:%d, len:%d\n",
		       __func__, i, psid->pid_entry_begin, psid->pid_entry_num);
		tsout_config_sid_table(i,
				       psid->pid_entry_begin / 4,
				       psid->pid_entry_num / 4);

		psid = &sid_table[i + 32];
		psid->used = 1;
		psid->pid_entry_begin =
		    ts_output_max_pid_num_per_sid * (2 * i + 1);
		psid->pid_entry_num = ts_output_max_pid_num_per_sid;

		pr_dbg("%s sid:%d, pid start:%d, len:%d\n", __func__,
		       i + 32, psid->pid_entry_begin, psid->pid_entry_num);
		tsout_config_sid_table(i + 32,
				       psid->pid_entry_begin / 4,
				       psid->pid_entry_num / 4);
	}

	return 0;
}

int ts_output_destroy(void)
{
	if (out_elem_table)
		vfree(out_elem_table);

	out_elem_table = NULL;
	return 0;
}

/**
 * remap pid
 * \param sid: stream id
 * \param pid: orginal pid
 * \param new_pid: replace pid
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_remap_pid(int sid, int pid, int new_pid)
{
	return 0;
}

/**
 * set pid pcr
 * \param sid: stream id
 * \param pcr_num
 * \param pcrpid
 * \retval 0:success.
 * \retval -1:fail.
 * \note:pcrpid == -1, it will close
 */
int ts_output_set_pcr(int sid, int pcr_num, int pcrpid)
{
	pr_dbg("%s pcr_num:%d,pid:%d\n", __func__, pcr_num, pcrpid);
	if (pcr_num >= MAX_PCR_NUM) {
		dprint("%s num:%d invalid\n", __func__, pcr_num);
		return -1;
	}
	if (pcrpid == -1) {
		pcr_table[pcr_num].turn_on = 0;
		pcr_table[pcr_num].stream_id = -1;
		pcr_table[pcr_num].pcr_pid = -1;
		tsout_config_pcr_table(pcr_num, -1, sid);
	} else {
		pcr_table[pcr_num].turn_on = 1;
		pcr_table[pcr_num].stream_id = sid;
		pcr_table[pcr_num].pcr_pid = pcrpid;
		tsout_config_pcr_table(pcr_num, pcrpid, sid);
	}
	return 0;
}

/**
 * get pcr value
 * \param pcr_num
 * \param pcr:pcr value
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_get_pcr(int pcr_num, uint64_t *pcr)
{
	if (pcr_num >= MAX_PCR_NUM) {
		dprint("%s num:%d invalid\n", __func__, pcr_num);
		return -1;
	}
	if (!pcr_table[pcr_num].turn_on) {
		dprint("%s num:%d close\n", __func__, pcr_num);
		return -1;
	}
	tsout_config_get_pcr(pcr_num, pcr);
	return 0;
}

/**
 * open one output pipeline
 * \param sid:stream id.
 * \param format:output format.
 * \param type:input content type.
 * \param aud_type:input audio format
 * \param output_mode:1 will output raw mode,just for ES.
 * \retval return out_elem.
 * \retval NULL:fail.
 */
struct out_elem *ts_output_open(int sid, u8 format,
				enum content_type type, int aud_type,
				int output_mode)
{
	struct bufferid_attr attr;
	int ret = 0;
	struct out_elem *pout;

	pr_dbg("%s sid:%d, format:%d, type:%d", __func__, sid, format, type);
	pr_dbg("audio_type:%d, output_mode:%d\n", aud_type, output_mode);

	if (sid >= MAX_SID_NUM) {
		dprint("%s sid:%d fail\n", __func__, sid);
		return NULL;
	}
	pout = _find_free_elem();
	if (!pout) {
		dprint("%s find free elem sid:%d fail\n", __func__, sid);
		return NULL;
	}
	pout->format = format;
	pout->sid = sid;
	pout->pid_list = NULL;
	pout->output_mode = output_mode;
	pout->type = type;
	pout->aud_type = aud_type;

	memset(&attr, 0, sizeof(struct bufferid_attr));
	attr.mode = OUTPUT_MODE;

	if (format == ES_FORMAT) {
		attr.is_es = 1;
		ret = SC2_bufferid_alloc(&attr, &pout->pchan, &pout->pchan1);
		if (ret != 0) {
			dprint("%s sid:%d SC2_bufferid_alloc fail\n",
			       __func__, sid);
			return NULL;
		}
		pout->enable = 0;
		pout->wakeup = 0;
		pout->remain_len = 0;
		pout->cache_len = 0;
		pout->cache = NULL;
		pout->aucpu_handle = -1;
	} else {
		ret = SC2_bufferid_alloc(&attr, &pout->pchan, NULL);
		if (ret != 0) {
			dprint("%s sid:%d SC2_bufferid_alloc fail\n",
			       __func__, sid);
			return NULL;
		}
		pout->enable = 0;
		pout->wakeup = 0;
		pout->remain_len = 0;
		pout->cache_len = READ_CACHE_SIZE;
		pout->cache = kmalloc(pout->cache_len, GFP_KERNEL);
		if (!pout->cache)
			dprint("%s sid:%d kmalloc cache fail\n", __func__, sid);
	}

	init_waitqueue_head(&pout->wait_queue);
	pout->running = TASK_RUNNING;
	if (format == ES_FORMAT) {
		pout->out_task = kthread_run(_task_out_func_for_es,
					     (void *)pout, "out_sid%d",
					     pout->sid);
	} else {
		pout->out_task = kthread_run(_task_out_func,
					     (void *)pout, "out_sid%d",
					     pout->sid);
	}

	if (!pout->out_task)
		dprint("create ts out task fail\n");

	pout->flush_time_ms = out_flush_time;

	init_timer(&pout->out_timer);
	pout->out_timer.function = _timer_out_func;
	pout->out_timer.expires =
	    jiffies + msecs_to_jiffies(pout->flush_time_ms);
	pout->out_timer.data = (unsigned long)pout;
	add_timer(&pout->out_timer);

	pout->used = 1;
	pr_dbg("%s exit\n", __func__);
	return pout;
}

/**
 * close openned index
 * \param pout
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_close(struct out_elem *pout)
{
	del_timer_sync(&pout->out_timer);

	pout->running = TASK_DEAD;
	pout->wakeup = 1;
	wake_up_interruptible(&pout->wait_queue);
	pr_dbg("%s line:%d\n", __func__, __LINE__);

	while (pout->running != TASK_IDLE)
		msleep(20);

	pr_dbg("%s line:%d\n", __func__, __LINE__);

	if (pout->pchan) {
		SC2_bufferid_set_enable(pout->pchan, 0);
		SC2_bufferid_dealloc(pout->pchan);
		pout->pchan = NULL;
	}
	if (pout->pchan1) {
		SC2_bufferid_set_enable(pout->pchan1, 0);
		SC2_bufferid_dealloc(pout->pchan1);
		pout->pchan1 = NULL;
	}
	if (pout->pid_list)
		_free_pid_list(pout->pid_list);

	pout->used = 0;
	return 0;
}

/**
 * add pid in stream
 * \param pout
 * \param pid:
 * \param pid_mask:0,matched all bits; 0x1FFF matched any PID
 * \param dmx_id: dmx_id
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_add_pid(struct out_elem *pout, int pid, int pid_mask, int dmx_id)
{
	struct pid_entry *pid_slot = NULL;
	struct es_entry *es_pes = NULL;

	pr_dbg("%s pout:0x%lx pid:%d, pid_mask:%d\n",
	       __func__, (unsigned long)pout, pid, pid_mask);
	if (pout->format == ES_FORMAT || pout->format == PES_FORMAT) {
		es_pes = _malloc_es_entry_slot();
		if (!es_pes) {
			dprint("get es entry slot error\n");
			return -1;
		}
		es_pes->buff_id = pout->pchan->id;
		es_pes->pid = pid;
		es_pes->status = pout->format;
		es_pes->dmx_id = dmx_id;
		es_pes->pout = pout;
		pout->es_pes = es_pes;
		tsout_config_es_table(es_pes->buff_id, es_pes->pid,
				      pout->sid, 1, !drop_dup, pout->format);
	} else {
		pid_slot = _find_pid_entry_slot(pout->pid_list, pid);
		if (pid_slot) {
			pid_slot->pid_user++;
			pr_dbg("%s found same pid:0x%0x\n", __func__, pid);
			return 0;
		}
		pid_slot = _malloc_pid_entry_slot(pout->sid, pid);
		if (!pid_slot) {
			pr_dbg("malloc pid entry fail\n");
			return -1;
		}
		pid_slot->next = pout->pid_list;
		pid_slot->pid = pid;
		pid_slot->pid_mask = pid_mask;
		pid_slot->used = 1;

		pout->pid_list = pid_slot;
		pid_slot->pid_user++;
		pid_slot->dmx_id = dmx_id;
		pid_slot->pout = pout;
		pr_dbg("sid:%d, pid:0x%0x, mask:0x%0x\n",
		       pout->sid, pid_slot->pid, pid_slot->pid_mask);
		tsout_config_ts_table(pid_slot->pid, pid_slot->pid_mask,
				      pid_slot->id, pout->pchan->id);
	}

	if (pout->pchan)
		SC2_bufferid_set_enable(pout->pchan, 1);
	if (pout->pchan1)
		SC2_bufferid_set_enable(pout->pchan1, 1);
	return 0;
}

/**
 * remove pid in stream
 * \param pout
 * \param pid
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_remove_pid(struct out_elem *pout, int pid)
{
	struct pid_entry **cur_pid, *n_pid;

	pr_dbg("%s pout:0x%lx pid:0x%0x\n", __func__, (unsigned long)pout, pid);
	if (pout->format == ES_FORMAT || pout->format == PES_FORMAT) {
		if (pout->es_pes->pid != pid) {
			dprint("%s err,org pid:0x%0x, pid:0x%0x\n",
			       __func__, pout->es_pes->pid, pid);
			return -1;
		}
		tsout_config_es_table(pout->es_pes->buff_id, -1,
				      pout->sid, 1, !drop_dup, pout->format);
		_free_es_entry_slot(pout->es_pes);
		pout->es_pes = NULL;
		if (pout->aucpu_handle >= 0) {
			s32 ret;

			if (pout->aucpu_start) {
				ret = aml_aucpu_strm_stop(pout->aucpu_handle);
				if (ret >= 0)
					pr_dbg("aml_aucpu_strm_stop success\n");
				else
					pr_dbg("aucpu_stop fail ret:%d\n", ret);
				pout->aucpu_start = 0;
			}
			ret = aml_aucpu_strm_remove(pout->aucpu_handle);
			if (ret >= 0)
				pr_dbg("aucpu_strm_remove success\n");
			else
				pr_dbg("aucpu_strm_remove fail ret:%d\n", ret);
			pout->aucpu_handle = -1;

			_free_buff(pout->aucpu_mem_phy,
				   pout->aucpu_mem_size, 0, 0);
			pout->aucpu_mem = 0;
		}
	} else {
		cur_pid = &pout->pid_list;
		pr_dbg("%s line:%d\n", __func__, __LINE__);
		n_pid = *cur_pid;
		while (n_pid) {
			if (n_pid->pid == pid) {
				if (n_pid->pid_user == 1) {
					pr_dbg("%s line:%d\n",
					       __func__, __LINE__);
					*cur_pid = n_pid->next;
					break;
				}
				n_pid->pid_user--;
				return 0;
			}
			pr_dbg("%s line:%d\n", __func__, __LINE__);
			cur_pid = &n_pid->next;
			n_pid = *cur_pid;
		}
		tsout_config_ts_table(-1, n_pid->pid_mask,
				      n_pid->id, pout->pchan->id);
		_free_pid_entry_slot(n_pid);
	}
	pr_dbg("%s line:%d\n", __func__, __LINE__);

	return 0;
}

int ts_output_set_mem(struct out_elem *pout,
		      int memsize, int sec_level, int pts_memsize)
{
	if (pout && pout->pchan)
		SC2_bufferid_set_mem(pout->pchan, memsize, sec_level);

	if (pout && pout->pchan1)
		SC2_bufferid_set_mem(pout->pchan1, pts_memsize, 0);

	if (pout->pchan->sec_level)
		create_aucpu_inst(pout);
	return 0;
}

int ts_output_get_mem_info(struct out_elem *pout,
			   unsigned int *total_size,
			   unsigned int *buf_phy_start,
			   unsigned int *free_size, unsigned int *wp_offset)
{
	*total_size = pout->pchan->mem_size;
	*buf_phy_start = pout->pchan->mem_phy;
	*wp_offset = SC2_bufferid_get_wp_offset(pout->pchan);
	*free_size = SC2_bufferid_get_free_size(pout->pchan);
	return 0;
}

/**
 * reset index pipeline, clear the buf
 * \param pout
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_reset(struct out_elem *pout)
{
	return 0;
}

/**
 * set callback for getting data
 * \param pout
 * \param cb
 * \param udata:private data
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_set_cb(struct out_elem *pout, ts_output_cb cb, void *udata)
{
	pout->cb = cb;
	pout->udata = udata;
	return 0;
}

int ts_output_dump_info(char *buf)
{
	int i = 0;
	int count = 0;
	int r, total = 0;

	r = sprintf(buf, "********TS********\n");
	buf += r;
	total += r;

	for (i = 0; i < MAX_TS_PID_NUM; i++) {
		struct pid_entry *tmp = &pid_table[i];
		unsigned int total_size = 0;
		unsigned int buf_phy_start = 0;
		unsigned int free_size = 0;
		unsigned int wp_offset = 0;

		if (tmp->used) {
			r = sprintf(buf, "%d dmxid:%d sid:%d pid:0x%0x ",
				    count, tmp->dmx_id, tmp->pout->sid,
				    tmp->pid);
			buf += r;
			total += r;
			r = sprintf(buf, "mask:0x%0x tab_id:%d\n",
				    tmp->pid_mask, i);
			buf += r;
			total += r;

			ts_output_get_mem_info(tmp->pout,
					       &total_size,
					       &buf_phy_start,
					       &free_size, &wp_offset);

			r = sprintf(buf,
				    "mem: total:%d, buf_base:0x%0x, wp:0x%0x\n",
				    total_size, buf_phy_start, wp_offset);
			buf += r;
			total += r;

			count++;
		}
	}

	r = sprintf(buf, "********PES********\n");
	buf += r;
	total += r;

	count = 0;
	for (i = 0; i < MAX_ES_NUM; i++) {
		struct es_entry *es_slot = &es_table[i];
		unsigned int total_size = 0;
		unsigned int buf_phy_start = 0;
		unsigned int free_size = 0;
		unsigned int wp_offset = 0;

		if (es_slot->used && es_slot->status == PES_FORMAT) {
			r = sprintf(buf, "%d dmx_id:%d sid:%d type:%d,", count,
				    es_slot->dmx_id, es_slot->pout->sid,
				    es_slot->pout->type);
			buf += r;
			total += r;

			r = sprintf(buf, "pid:0x%0x\n", es_slot->pid);
			buf += r;
			total += r;

			ts_output_get_mem_info(es_slot->pout,
					       &total_size,
					       &buf_phy_start,
					       &free_size, &wp_offset);

			r = sprintf(buf,
				    "mem: total:%d, buf_base:0x%0x, wp:0x%0x\n",
				    total_size, buf_phy_start, wp_offset);
			buf += r;
			total += r;
			count++;
		}
	}

	r = sprintf(buf, "********ES********\n");
	buf += r;
	total += r;
	count = 0;
	for (i = 0; i < MAX_ES_NUM; i++) {
		struct es_entry *es_slot = &es_table[i];
		unsigned int total_size = 0;
		unsigned int buf_phy_start = 0;
		unsigned int free_size = 0;
		unsigned int wp_offset = 0;

		if (es_slot->used && es_slot->status == ES_FORMAT) {
			r = sprintf(buf, "%d dmx_id:%d sid:%d type:%d", count,
				    es_slot->dmx_id, es_slot->pout->sid,
				    es_slot->pout->type);
			buf += r;
			total += r;

			r = sprintf(buf, " pid:0x%0x\n", es_slot->pid);
			buf += r;
			total += r;

			ts_output_get_mem_info(es_slot->pout,
					       &total_size,
					       &buf_phy_start,
					       &free_size, &wp_offset);

			r = sprintf(buf,
				    "mem: total:%d, buf_base:0x%0x, wp:0x%0x\n",
				    total_size, buf_phy_start, wp_offset);
			buf += r;
			total += r;
			count++;
		}
	}
	return total;
}
