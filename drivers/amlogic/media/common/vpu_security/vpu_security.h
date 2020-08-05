/*
 * drivers/amlogic/media/common/vpu_security/vpu_security.h
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

#ifndef VPU_SECURITY_H_
#define VPU_SECURITY_H_

#define MODULE_NUM  6
struct vpu_secure_ins {
	struct mutex secure_lock;/*vpu secure mutex*/
	unsigned char registered;
	unsigned char secure_enable;
	unsigned char secure_status;
	unsigned char config_delay;
	int (*reg_wr_op)(u32 addr, u32 val);
	void (*secure_cb)(u32 arg);
};

struct vpu_security_device_info {
	const char *device_name;
	struct platform_device *pdev;
	struct class *clsp;
	int mismatch_cnt;
	int probed;
	struct vpu_secure_ins ins[MODULE_NUM];
};

enum cpu_type_e {
	MESON_CPU_MAJOR_ID_SC2_ = 0x1,
	MESON_CPU_MAJOR_ID_UNKNOWN,
};

struct sec_dev_data_s {
	enum cpu_type_e cpu_type;
};

#endif
