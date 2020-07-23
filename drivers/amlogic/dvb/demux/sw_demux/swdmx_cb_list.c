/*
 * drivers/amlogic/dvb/demux/sw_demux/swdmx_cb_list.c
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

#include "swdemux_internal.h"

void swdmx_cb_list_add(struct swdmx_list *l, void *cb, void *data)
{
	struct swdmx_cb_entry *ent;

	SWDMX_ASSERT(l && cb);

	ent = swdmx_malloc(sizeof(struct swdmx_cb_entry));
	SWDMX_ASSERT(ent);

	ent->cb = cb;
	ent->data = data;

	swdmx_list_append(l, &ent->ln);
}

void swdmx_cb_list_remove(struct swdmx_list *l, void *cb, void *data)
{
	struct swdmx_cb_entry *ent, *nent;

	SWDMX_LIST_FOR_EACH_SAFE(ent, nent, l, ln) {
		if ((ent->cb == cb) && (ent->data == data)) {
			swdmx_list_remove(&ent->ln);
			swdmx_free(ent);
			break;
		}
	}
}

void swdmx_cb_list_clear(struct swdmx_list *l)
{
	struct swdmx_cb_entry *ent, *nent;

	SWDMX_LIST_FOR_EACH_SAFE(ent, nent, l, ln) {
		swdmx_list_remove(&ent->ln);
		swdmx_free(ent);
	}

	swdmx_list_init(l);
}
