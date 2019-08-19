/*
 * Driver for manage data between config.ini and bootloader blob bl301
 *
 * Copyright (C) 2019 Team CoreELEC
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "bl301_manager.h"

static u32 remotewakeup = 0xffffffff;
static u32 remotewakeupmask = 0xffffffff;
static u32 decode_type = 0;
static u32 enable_system_power = 0;

static int __init remote_wakeup_setup(char *str)
{
	int ret;

	if (str == NULL) {
		pr_info("%s no string\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(str, 16, &remotewakeup);
	if (ret) {
		remotewakeup = 0xffffffff;
		return -EINVAL;
	}

	return 0;
}
__setup("remotewakeup=", remote_wakeup_setup);

static int __init remote_wakeupmask_setup(char *str)
{
	int ret;

	if (str == NULL) {
		pr_info("%s no string\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(str, 16, &remotewakeupmask);
	if (ret) {
		remotewakeupmask = 0xffffffff;
		return -EINVAL;
	}

	return 0;
}
__setup("remotewakeupmask=", remote_wakeupmask_setup);

static int __init remote_decode_type_setup(char *str)
{
	int ret;

	if (str == NULL) {
		pr_info("%s no string\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(str, 16, &decode_type);
	if (ret) {
		decode_type = 0x0;
		return -EINVAL;
	}

	return 0;
}
__setup("decode_type=", remote_decode_type_setup);

static int __init enable_system_power_setup(char *str)
{
	int ret;

	if (str == NULL) {
		pr_info("%s no string\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(str, 10, &enable_system_power);
	if (ret) {
		enable_system_power = 0;
		return -EINVAL;
	}

	return 0;
}
__setup("enable_system_power=", enable_system_power_setup);

static struct platform_driver bl301_manager_driver = {
	.driver = {
		.name = DRIVER_NAME
	},
};

static int __init bl301_manager_init(void)
{
	int ret;

	ret = platform_driver_register(&bl301_manager_driver);
	if (ret < 0) {
			pr_info("%s: unable to register platform driver\n", DRIVER_NAME);
			return ret;
	}

	pr_info("%s: driver init\n", DRIVER_NAME);

	pr_info("%s: IR remote wake-up code: 0x%x\n", DRIVER_NAME, remotewakeup);
	pr_info("%s: IR remote wake-up code mask: 0x%x\n", DRIVER_NAME, remotewakeupmask);
	pr_info("%s: IR remote wake-up code protocol: 0x%x\n", DRIVER_NAME, decode_type);
	pr_info("%s: enable 5V system power on suspend/power off state: %d\n", DRIVER_NAME
		, enable_system_power);

	scpi_send_usr_data(SCPI_CL_REMOTE, &remotewakeup,
		sizeof(remotewakeup));
	scpi_send_usr_data(SCPI_CL_IRPROTO, &decode_type,
		sizeof(decode_type));
	scpi_send_usr_data(SCPI_CL_REMOTE_MASK, &remotewakeupmask,
		sizeof(remotewakeupmask));
	scpi_send_usr_data(SCPI_CL_5V_SYSTEM_POWER, &enable_system_power,
		sizeof(enable_system_power));

	return 0;
}

static void __exit bl301_manager_exit(void)
{
	platform_driver_unregister(&bl301_manager_driver);
	pr_info("%s: driver exited\n", DRIVER_NAME);
}

module_init(bl301_manager_init);
module_exit(bl301_manager_exit);

MODULE_AUTHOR("Portisch, Team CoreELEC");
MODULE_DESCRIPTION("Handle data between config.ini and bootloader blob bl301");
MODULE_LICENSE("GPL");
