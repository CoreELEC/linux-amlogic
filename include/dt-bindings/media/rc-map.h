/*
 * rc-map.h - define RC map names used by RC drivers
 *
 * Copyright (c) 2010 by Mauro Carvalho Chehab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _DT_BINDINGS_RC_MAP_H
#define _DT_BINDINGS_RC_MAP_H

/**
 * constant rc_type for enum rc_type
 */

#define UNKNOWN     0
#define OTHER       1
#define RC5         2
#define RC5X        3
#define RC5_SZ      4
#define JVC         5
#define SONY12      6
#define SONY15      7
#define SONY20      8
#define NEC         9
#define NECX        10
#define NEC32       11
#define SANYO       12
#define MCE_KBD     13
#define RC6_0       14
#define RC6_6A_20   15
#define RC6_6A_24   16
#define RC6_6A_32   17
#define RC6_MCE     18
#define SHARP       19
#define XMP         20
#define CEC         21
#define IRMP        22

#endif
