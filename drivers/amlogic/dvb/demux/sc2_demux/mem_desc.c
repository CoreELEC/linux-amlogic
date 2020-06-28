/*
 * drivers/amlogic/dvb/demux/sc2_demux/mem_desc.c
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

#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include "mem_desc.h"
#include "sc2_control.h"
#include "../aml_dvb.h"
#include "../dmx_log.h"

#define MEM_DESC_SLOT_USED		1
#define MEM_DESC_SLOT_FREE		0

#define MEM_DESC_EOC_USED		1
#define MEM_DESC_EOC_FREE		0

#define R_MAX_MEM_CHAN_NUM			32
#define W_MAX_MEM_CHAN_NUM			128

static struct chan_id chan_id_table_w[W_MAX_MEM_CHAN_NUM];
static struct chan_id chan_id_table_r[R_MAX_MEM_CHAN_NUM];

#define dprint_i(fmt, args...)   \
	dprintk(LOG_ERROR, debug_mem_desc, fmt, ## args)
#define dprint(fmt, args...)     \
	dprintk(LOG_ERROR, debug_mem_desc, "mem_desc:" fmt, ## args)
#define pr_dbg(fmt, args...)     \
	dprintk(LOG_DBG, debug_mem_desc, "mem_desc:" fmt, ## args)

MODULE_PARM_DESC(debug_mem_desc, "\n\t\t Enable debug mem desc information");
static int debug_mem_desc;
module_param(debug_mem_desc, int, 0644);

MODULE_PARM_DESC(loop_desc, "\n\t\t Enable loop mem desc information");
static int loop_desc = 0x1;
module_param(loop_desc, int, 0644);

#define DEFAULT_RCH_SYNC_NUM	8
static int rch_sync_num_last = -1;

MODULE_PARM_DESC(rch_sync_num, "\n\t\t Enable loop mem desc information");
static int rch_sync_num = DEFAULT_RCH_SYNC_NUM;
module_param(rch_sync_num, int, 0644);

unsigned long _alloc_buff(unsigned int len, int sec_level)
{
	unsigned long pages;

	if (sec_level) {
		dprint("%s not support sec_level:%d", __func__, sec_level);
		return 0;
	}
	pages = __get_free_pages(GFP_KERNEL, get_order(len));
	if (!pages) {
		dprint("%s fail\n", __func__);
		return -1;
	}
	return pages;
}

void _free_buff(unsigned long buf, unsigned int len, int sec_level)
{
	if (sec_level)
		dprint("%s not support sec_level:%d", __func__, sec_level);
	else
		free_pages(buf, get_order(len));
}

static int _bufferid_malloc_desc_mem(struct chan_id *pchan,
				     unsigned int mem_size, int sec_level)
{
	unsigned long mem;
	unsigned int mem_phy;
	union mem_desc *p;

	if ((mem_size >> 27) > 0) {
		dprint("%s fail, need support >=128M bytes\n", __func__);
		return -1;
	}
	if (pchan->mode == INPUT_MODE && sec_level != 0) {
		mem = 0;
		mem_size = 0;
		mem_phy = 0;
	} else {
		mem = _alloc_buff(mem_size, sec_level);
		if (mem == 0) {
			dprint("%s malloc fail\n", __func__);
			return -1;
		}
		dprint("%s malloc 0x%lx\n", __func__, mem);
		mem_phy = virt_to_phys((void *)mem);
		dprint("%s mem phy addr 0x%x\n", __func__, mem_phy);
	}
	p = (union mem_desc *)_alloc_buff(sizeof(union mem_desc), 0);
	if (p == 0) {
		_free_buff(mem, mem_size, sec_level);
		dprint("%s malloc 2 fail\n", __func__);
		return -1;
	}
	pchan->mem = mem;
	pchan->mem_phy = mem_phy;
	pchan->mem_size = mem_size;
	pchan->sec_level = sec_level;
	pchan->memdescs = p;
	pchan->memdescs_phy = virt_to_phys(p);

	pchan->memdescs->bits.address = mem_phy;
	pchan->memdescs->bits.byte_length = mem_size;
	pchan->memdescs->bits.loop = loop_desc;
	pchan->memdescs->bits.eoc = 1;

	pchan->memdescs_map = dma_map_single(aml_get_device(),
					     (void *)pchan->memdescs,
					     sizeof(union mem_desc),
					     DMA_TO_DEVICE);
	pr_dbg("flush mem descs to ddr\n");
	dprint("%s mem_desc phy addr 0x%x, memdsc:0x%lx\n", __func__,
	       pchan->memdescs_phy, (unsigned long)p);

	pchan->r_offset = 0;
	return 0;
}

static void _bufferid_free_desc_mem(struct chan_id *pchan)
{
	if (pchan->mem)
		_free_buff((unsigned long)pchan->mem,
			   pchan->mem_size, pchan->sec_level);
	if (pchan->memdescs)
		_free_buff((unsigned long)pchan->memdescs,
			   sizeof(union mem_desc), 0);
	if (pchan->memdescs_map)
		dma_unmap_single(aml_get_device(), pchan->memdescs_map,
				 pchan->mem_size, DMA_TO_DEVICE);
	pchan->mem = 0;
	pchan->mem_phy = 0;
	pchan->mem_size = 0;
	pchan->sec_level = 0;
	pchan->memdescs = NULL;
	pchan->memdescs_phy = 0;
	pchan->memdescs_map = 0;
}

static int _bufferid_alloc_chan_w_for_es(struct chan_id **pchan,
					 struct chan_id **pchan1)
{
	int i = 0;

#define ES_BUFF_ID_END  63
	for (i = ES_BUFF_ID_END; i >= 0; i--) {
		struct chan_id *pchan_tmp = &chan_id_table_w[i];
		struct chan_id *pchan1_tmp = &chan_id_table_w[i + 64];

		if (pchan_tmp->used == 0 && pchan1_tmp->used == 0) {
			pchan_tmp->used = 1;
			pchan1_tmp->used = 1;
			pchan_tmp->is_es = 1;
			pchan1_tmp->is_es = 1;
			*pchan = pchan_tmp;
			*pchan1 = pchan1_tmp;
			break;
		}
	}
	if (i >= 0) {
		dprint("find bufferid %d & %d for es\n", i, i + 64);
		return 0;
	}
	dprint("can't find bufferid for es\n");
	return -1;
}

static int _bufferid_alloc_chan_w_for_ts(struct chan_id **pchan)
{
	int i = 0;
	struct chan_id *pchan_tmp;

	for (i = 0; i < W_MAX_MEM_CHAN_NUM; i++) {
		pchan_tmp = &chan_id_table_w[i];
		if (pchan_tmp->used == 0) {
			pchan_tmp->used = 1;
			break;
		}
	}
	if (i == W_MAX_MEM_CHAN_NUM)
		return -1;
	*pchan = pchan_tmp;
	return 0;
}

static int _bufferid_alloc_chan_r_for_ts(struct chan_id **pchan, u8 req_id)
{
	struct chan_id *pchan_tmp;

	if (req_id >= R_MAX_MEM_CHAN_NUM)
		return -1;
	pchan_tmp = &chan_id_table_r[req_id];
	if (pchan_tmp->used == 1)
		return -1;

	pchan_tmp->used = 1;
	*pchan = pchan_tmp;
	return 0;
}

/**
 * chan init
 * \retval 0: success
 * \retval -1:fail.
 */
int SC2_bufferid_init(void)
{
	int i = 0;

	pr_dbg("%s enter\n", __func__);

	for (i = 0; i < W_MAX_MEM_CHAN_NUM; i++) {
		struct chan_id *pchan = &chan_id_table_w[i];

		memset(pchan, 0, sizeof(struct chan_id));
		pchan->id = i;
		pchan->mode = OUTPUT_MODE;
	}

	for (i = 0; i < R_MAX_MEM_CHAN_NUM; i++) {
		struct chan_id *pchan = &chan_id_table_r[i];

		memset(pchan, 0, sizeof(struct chan_id));
		pchan->id = i;
		pchan->mode = INPUT_MODE;
	}

	return 0;
}

/**
 * alloc chan
 * \param attr
 * \param pchan,if ts/es, return this channel
 * \param pchan1, if es, return this channel for pcr
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_alloc(struct bufferid_attr *attr,
		       struct chan_id **pchan, struct chan_id **pchan1)
{
	pr_dbg("%s enter\n", __func__);

	if (attr->mode == INPUT_MODE)
		return _bufferid_alloc_chan_r_for_ts(pchan, attr->req_id);

	if (attr->is_es)
		return _bufferid_alloc_chan_w_for_es(pchan, pchan1);
	else
		return _bufferid_alloc_chan_w_for_ts(pchan);
}

/**
 * dealloc chan & free mem
 * \param pchan:struct chan_id handle
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_dealloc(struct chan_id *pchan)
{
	pr_dbg("%s enter\n", __func__);
	if (pchan->mode == INPUT_MODE) {
		_bufferid_free_desc_mem(pchan);
		pchan->is_es = 0;
		pchan->used = 0;
	} else {
		_bufferid_free_desc_mem(pchan);
		pchan->is_es = 0;
		pchan->used = 0;
	}
	return 0;
}

/**
 * chan mem
 * \param pchan:struct chan_id handle
 * \param mem_size:used memory
 * \param sec_level: memory security level
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_mem(struct chan_id *pchan,
			 unsigned int mem_size, int sec_level)
{
	pr_dbg("%s mem_size:%d,sec_level:%d\n", __func__, mem_size, sec_level);
	_bufferid_malloc_desc_mem(pchan, mem_size, sec_level);
	return 0;
}

/**
 * set enable
 * \param pchan:struct chan_id handle
 * \param enable: 1/0
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_enable(struct chan_id *pchan, int enable)
{
	unsigned int tmp;

	if (pchan->enable == enable)
		return 0;

	pr_dbg("%s id:%d enable:%d\n", __func__, pchan->id, enable);

	pchan->enable = enable;

	tmp = pchan->memdescs_phy & 0xFFFFFFFF;
	wdma_config_enable(pchan->id, enable, tmp, pchan->mem_size);

	pr_dbg("######wdma start###########\n");
	pr_dbg("err:0x%0x, active:%d\n", wdma_get_err(pchan->id),
	       wdma_get_active(pchan->id));
	pr_dbg("wptr:0x%0x\n", wdma_get_wptr(pchan->id));
	pr_dbg("wr_len:0x%0x\n", wdma_get_wr_len(pchan->id, NULL));
	pr_dbg("######wdma end###########\n");

	return 0;
}

/**
 * recv data
 * \param pchan:struct chan_id handle
 * \retval 0: no data
 * \retval 1: recv data, it can call SC2_bufferid_read
 */
int SC2_bufferid_recv_data(struct chan_id *pchan)
{
	unsigned int wr_offset = 0;

	if (!pchan)
		return 0;

	wr_offset = wdma_get_wr_len(pchan->id, NULL);
	if (wr_offset != pchan->r_offset)
		return 1;
	return 0;
}

/**
 * chan read
 * \param pchan:struct chan_id handle
 * \param pread:data addr
 * \param plen:data size addr
 * \retval >=0:read cnt.
 * \retval -1:fail.
 */
int SC2_bufferid_read(struct chan_id *pchan, char **pread, unsigned int len)
{
	unsigned int w_offset = 0;
	unsigned int w_offset_org = 0;
	unsigned int buf_len = len;
	unsigned int data_len = 0;
	int overflow = 0;

	w_offset_org = wdma_get_wr_len(pchan->id, &overflow);
	if (overflow) {
		wdma_clean_batch(pchan->id);
		pr_dbg("it produece batch end\n");
	}
	w_offset = w_offset_org % pchan->mem_size;
	if (w_offset != pchan->r_offset && w_offset != 0) {
		pr_dbg("%s w:0x%0x, r:0x%0x, wr_len:0x%0x\n", __func__,
		       (u32)w_offset, (u32)(pchan->r_offset),
		       (u32)w_offset_org);
		if (w_offset > pchan->r_offset) {
			data_len = min((w_offset - pchan->r_offset), buf_len);
			dma_sync_single_for_cpu(aml_get_device(),
						(dma_addr_t)(pchan->mem_phy +
							      pchan->r_offset),
						data_len, DMA_FROM_DEVICE);
			*pread = (char *)(pchan->mem + pchan->r_offset);
			pchan->r_offset += data_len;
		} else {
			unsigned int part1_len = 0;

			part1_len = pchan->mem_size - pchan->r_offset;
			data_len = min(part1_len, buf_len);
			*pread = (char *)(pchan->mem + pchan->r_offset);
			if (data_len < part1_len) {
				dma_sync_single_for_cpu(aml_get_device(),
							(dma_addr_t)
							(pchan->mem_phy +
							 pchan->r_offset),
							data_len,
							DMA_FROM_DEVICE);
				pchan->r_offset += data_len;
			} else {
				data_len = part1_len;
				dma_sync_single_for_cpu(aml_get_device(),
							(dma_addr_t)
							(pchan->mem_phy +
							 pchan->r_offset),
							data_len,
							DMA_FROM_DEVICE);
				pchan->r_offset = 0;
			}
		}
		pr_dbg("%s request:%d, ret:%d\n", __func__, len, data_len);
		return data_len;
	}
	return 0;
}

/**
 * write to channel
 * \param pchan:struct chan_id handle
 * \param buf: data addr
 * \param  count: write size
 * \param  isphybuf: isphybuf
 * \retval -1:fail
 * \retval written size
 */
int SC2_bufferid_write(struct chan_id *pchan, const char __user *buf,
		       unsigned int count, int isphybuf)
{
	unsigned int r = count;
	unsigned int len;
	unsigned int ret;
	const char __user *p = buf;
	unsigned int tmp;
	unsigned int times = 0;
	dma_addr_t dma_addr = 0;
	dma_addr_t dma_desc_addr = 0;
//      dma_addr_t dma_desc_addr = 0;

	pr_dbg("%s start w:%d\n", __func__, r);
	do {
	} while (!rdma_get_ready(pchan->id) && times++ < 20);

	if (rch_sync_num != rch_sync_num_last) {
		rdma_config_sync_num(rch_sync_num);
		rch_sync_num_last = rch_sync_num;
	}

	times = 0;
	while (r) {
		if (isphybuf) {
			len = count;
			pchan->enable = 1;
			tmp = (unsigned long)buf & 0xFFFFFFFF;
			pchan->memdescs->bits.address = tmp;
			pchan->memdescs->bits.byte_length = count;
			tmp = (unsigned long)(pchan->memdescs) & 0xFFFFFFFF;
			rdma_config_enable(pchan->id, 1, tmp, count, len);
			pr_dbg("%s isphybuf\n", __func__);
		} else {
			if (r > pchan->mem_size)
				len = pchan->mem_size;
			else
				len = r;
			if (copy_from_user((char *)(pchan->mem), p, len)) {
				dprint("copy_from user error\n");
				return -EFAULT;
			}
			dma_addr = dma_map_single(aml_get_device(),
						  (void *)pchan->mem,
						  pchan->mem_size,
						  DMA_TO_DEVICE);

			//set desc mem ==len for trigger data transfer.
			pchan->memdescs->bits.byte_length = len;
			dma_desc_addr = dma_map_single(aml_get_device(),
						       (void *)pchan->memdescs,
						       sizeof(union mem_desc),
						       DMA_TO_DEVICE);

			if (dma_mapping_error(aml_get_device(), dma_addr)) {
				dprint("mem dma_mapping error\n");
				dma_unmap_single(aml_get_device(),
						 dma_desc_addr,
						 sizeof(union mem_desc),
						 DMA_TO_DEVICE);
				return -EFAULT;
			}

			wmb();	/*Ensure pchan->mem contents visible */

			pr_dbg("%s, input data:0x%0x, des len:%d\n", __func__,
			       (*(char *)(pchan->mem)), len);
			pr_dbg("%s, desc data:0x%0x 0x%0x\n", __func__,
			       (*(unsigned int *)(pchan->memdescs)),
			       (*((unsigned int *)(pchan->memdescs) + 1)));

			pchan->enable = 1;
			tmp = pchan->memdescs_phy & 0xFFFFFFFF;
			rdma_config_enable(pchan->id, 1, tmp,
					   pchan->mem_size, len);
			dma_unmap_single(aml_get_device(),
					 dma_addr, pchan->mem_size,
					 DMA_TO_DEVICE);
			dma_unmap_single(aml_get_device(),
					 dma_desc_addr, sizeof(union mem_desc),
					 DMA_TO_DEVICE);
		}

		do {
		} while (!rdma_get_done(pchan->id));

		ret = rdma_get_rd_len(pchan->id);
		if (ret != len)
			dprint("%s, len not equal,ret:%d,w:%d\n",
			       __func__, ret, len);

		pr_dbg("#######rdma##########\n");
		pr_dbg("status:0x%0x\n", rdma_get_status(pchan->id));
		pr_dbg("err:0x%0x, len err:0x%0x, active:%d\n",
		       rdma_get_err(), rdma_get_len_err(), rdma_get_active());
		pr_dbg("pkt_sync:0x%0x\n", rdma_get_pkt_sync_status(pchan->id));
		pr_dbg("ptr:0x%0x\n", rdma_get_ptr(pchan->id));
		pr_dbg("cfg fifo:0x%0x\n", rdma_get_cfg_fifo());
		pr_dbg("#######rdma##########\n");

		/*disable */
		rdma_config_enable(pchan->id, 0, 0, 0, 0);
		rdma_clean(pchan->id);

		p += len;
		r -= len;
	}
	pr_dbg("%s end\n", __func__);
	rdma_config_ready(pchan->id);
	return count - r;
}
