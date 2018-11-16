/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


#ifndef __VIDEO_FIRMWARE_HEADER_
#define __VIDEO_FIRMWARE_HEADER_

#include "../../../common/firmware/firmware_type.h"
#include <linux/amlogic/media/utils/vformat.h>

#define FW_LOAD_FORCE	(0x1)
#define FW_LOAD_TRY	(0X2)

struct firmware_s {
	char name[32];
	unsigned int len;
	char data[0];
};

extern int get_decoder_firmware_data(enum vformat_e type,
	const char *file_name, char *buf, int size);
extern int get_data_from_name(const char *name, char *buf);
extern int get_firmware_data(unsigned int foramt, char *buf);
extern int video_fw_reload(int mode);

#endif
