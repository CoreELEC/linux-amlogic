/*
 * drivers/amlogic/media/di_multi_v3/di_que.h
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

#ifndef __DI_QUE_H__
#define __DI_QUE_H__

void queuev3_init(unsigned int channel, int local_buffer_num);
void queuev3_out(unsigned int channel, struct di_buf_s *di_buf);
void queuev3_in(unsigned int channel, struct di_buf_s *di_buf,
		int queue_idx);
int listv3_count(unsigned int channel, int queue_idx);
bool queuev3_empty(unsigned int channel, int queue_idx);
bool isv3_in_queue(unsigned int channel, struct di_buf_s *di_buf,
		   int queue_idx);
struct di_buf_s *getv3_di_buf_head(unsigned int channel,
				   int queue_idx);

void queuev3_init2(unsigned int channel);

/*new buf:*/
bool pwv3_queue_in(unsigned int ch, enum QUE_TYPE qtype,
		   unsigned int buf_index);
bool pwv3_queue_out(unsigned int ch, enum QUE_TYPE qtype,
		    unsigned int *buf_index);
bool pwv3_queue_empty(unsigned int ch, enum QUE_TYPE qtype);
void pwv3_queue_clear(unsigned int ch, enum QUE_TYPE qtype);

/******************************************/
/*new api*/
/******************************************/
union   uDI_QBUF_INDEX {
	unsigned int d32;
	struct {
		unsigned int index:8,	/*low*/
			     type:8,
			     reserved0:16;
	} b;
};

void div3_que_init(unsigned int ch);
bool div3_que_alloc(unsigned int ch);
void div3_que_release(unsigned int ch);

int div3_que_is_empty(unsigned int ch, enum QUE_TYPE qtype);
bool div3_que_out(unsigned int ch, enum QUE_TYPE qtype,
		  struct di_buf_s *di_buf);

struct di_buf_s *div3_que_out_to_di_buf(unsigned int ch,
					enum QUE_TYPE qtype);
bool div3_que_out_not_fifo(unsigned int ch, enum QUE_TYPE qtype,
			   struct di_buf_s *di_buf);

bool div3_que_in(unsigned int ch, enum QUE_TYPE qtype,
		 struct di_buf_s *di_buf);
bool div3_que_is_in_que(unsigned int ch, enum QUE_TYPE qtype,
			struct di_buf_s *di_buf);
struct di_buf_s *div3_que_peek(unsigned int ch, enum QUE_TYPE qtype);
bool div3_que_type_2_new(unsigned int q_type, enum QUE_TYPE *nqtype);
int div3_que_list_count(unsigned int ch, enum QUE_TYPE qtype);
bool div3_que_list(unsigned int ch, enum QUE_TYPE qtype,
		   unsigned int *outbuf, unsigned int *rsize);

struct di_buf_s *pwv3_qindex_2_buf(unsigned int ch, unsigned int qindex);

void queuev3_out_dbg(unsigned int channel, struct di_buf_s *di_buf);

#ifdef DIM_DEBUG_QUE_ERR
void dim_dbg_que_int(void);
void dim_dbg_que_disable(void);
void dim_dbg_que_out(void);
void trace_buf(struct di_buf_s *di_buf,
	       unsigned int q_api, unsigned int q_index,
	       unsigned int tag);

#endif
#endif	/*__DI_QUE_H__*/
