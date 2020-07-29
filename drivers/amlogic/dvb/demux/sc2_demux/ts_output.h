/*
 * drivers/amlogic/dvb/demux/sc2_demux/ts_output.h
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

#ifndef _TS_OUTPUT_H_
#define _TS_OUTPUT_H_

#include <linux/types.h>
#include "sc2_control.h"
#include <linux/dvb/dmx.h>

struct pts_dts {
	int pid;
	char pts_dts_flag;
	int es_bytes_from_beginning;
	u64 pts;
	u64 dts;
};

struct out_elem;

typedef int (*ts_output_cb) (struct out_elem *pout,
			     char *buf, int count, void *udata);

enum content_type {
	VIDEO_TYPE,
	AUDIO_TYPE,
	SUB_TYPE,
	TTX_TYPE,
	OTHER_TYPE
};

struct sid_info {
	int demod_sid;
	int local_sid;
};

/**
 * ts_output_init
 * \param dmxdev_num
 * \param info
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_init(int dmxdev_num, struct sid_info *info);

/**
 * ts_output_destroy
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_destroy(void);

/**
 * remap pid
 * \param sid: stream id
 * \param pid: orginal pid
 * \param new_pid: replace pid
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_remap_pid(int sid, int pid, int new_pid);

/**
 * set pid pcr
 * \param sid: stream id
 * \param pcrpid
 * \param pcr_num
 * \retval 0:success.
 * \retval -1:fail.
 * \note:pcrpid == -1, it will close
 */
int ts_output_set_pcr(int sid, int pcrpid, int pcr_num);

/**
 * get pcr value
 * \param pcrpid
 * \param pcr_num
 * \param pcr:pcr value
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_get_pcr(int pcr_num, uint64_t *pcr);

/**
 * open one output pipeline
 * \param sid:stream id.
 * \param format:output format.
 * \param type:input content type.
 * \param aud_type:just for audio format
 * \param output_mode:1 will output raw mode,just for ES.
 * \retval return out_elem.
 * \retval NULL:fail.
 */
struct out_elem *ts_output_open(int sid, u8 format,
				enum content_type type, int aud_type,
				int output_mode);

/**
 * close openned index
 * \param pout
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_close(struct out_elem *pout);

/**
 * add pid in stream
 * \param pout
 * \param pid:
 * \param pid_mask:0,matched all bits; 0x1FFF matched any PID
 * \param dmx_id: dmx_id
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_add_pid(struct out_elem *pout, int pid, int pid_mask, int dmx_id);

/**
 * remove pid in stream
 * \param pout
 * \param pid
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_remove_pid(struct out_elem *pout, int pid);

/**
 * set out elem mem
 * \param memsize
 * \param sec_level
 * \param pts_memsize
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_set_mem(struct out_elem *pout,
		      int memsize, int sec_level, int pts_memsize);

int ts_output_get_mem_info(struct out_elem *pout,
			   unsigned int *total_size,
			   unsigned int *buf_phy_start,
			   unsigned int *free_size, unsigned int *wp_offset);

/**
 * reset index pipeline, clear the buf
 * \param pout
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_reset(struct out_elem *pout);

/**
 * set callback for getting data
 * \param pout
 * \param cb
 * \param udata:private data
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_set_cb(struct out_elem *pout, ts_output_cb cb, void *udata);

int ts_output_sid_debug(void);
int ts_output_dump_info(char *buf);

#endif
