// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/registers/register_ops.h>
#include <linux/amlogic/media/registers/register_map.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/utils/log.h>
static struct chip_register_ops m8_ops[] __initdata = {
	{IO_DOS_BUS, 0, codecio_read_dosbus, codecio_write_dosbus},
	{IO_VC_BUS, 0, codecio_read_vcbus, codecio_write_vcbus},
	{IO_C_BUS, 0, codecio_read_cbus, codecio_write_cbus},
	{IO_HHI_BUS, 0, codecio_read_cbus, codecio_write_cbus},
	{IO_AO_BUS, 0, codecio_read_aobus, codecio_write_aobus},
	{IO_VPP_BUS, 0, codecio_read_vcbus, codecio_write_vcbus},
	{IO_PARSER_BUS, 0, codecio_read_parsbus, codecio_write_parsbus},
	{IO_AIU_BUS, 0, codecio_read_aiubus, codecio_write_aiubus},
	{IO_DEMUX_BUS, 0, codecio_read_demuxbus, codecio_write_demuxbus},
	{IO_RESET_BUS, 0, codecio_read_resetbus, codecio_write_resetbus},
	{IO_EFUSE_BUS, 0, codecio_read_efusebus, codecio_write_efusebus},
	{IO_NOC_BUS, 0, codecio_read_nocbus, codecio_write_nocbus},
};

static struct chip_register_ops ex_gx_ops[] __initdata = {
	/*
	 *#define HHI_VDEC_CLK_CNTL 0x1078
	 *to
	 *#define HHI_VDEC_CLK_CNTL 0x78
	 *on Gxbb.
	 *will changed later..
	 */
	{IO_HHI_BUS, -0x1000, codecio_read_hiubus, codecio_write_hiubus},
	{IO_DMC_BUS, 0, codecio_read_dmcbus, codecio_write_dmcbus},
};

int __init vdec_reg_ops_init(void)
{
	int  i = 0;
	int  t = 0;

	/*
	 * because of register range of the parser ,demux
	 * reset and aiu has be changed in the txlx platform,
	 * so we have must be offset the old range of regs.
	 *
	 * the new 0x3860 of the reg base minus the old 0x2900
	 * equal to the 0xf00 of the delta value
	 * #define PARSER_CONTROL 0x3860
	 * 0xf00 == (0x3800 - 0x2900)
	 *
	 * #define AIU_958_BPF 0x1400
	 * -0x100 == (0x1400 - 0x1500)
	 *
	 * #define FEC_INPUT_CONTROL 0x1802
	 * 0x200 == (0x1802 - 0x1602)
	 *
	 * #define RESET0_REGISTER 0x0401
	 * -0xd00 == (0x0401 - 0x1101)
	 */
	if (get_cpu_type() > MESON_CPU_MAJOR_ID_TXL) {
		for (i = 0; i < ARRAY_SIZE(m8_ops); i++) {
			switch (m8_ops[i].bus_type) {
			case IO_PARSER_BUS:
				m8_ops[i].ext_offset = 0xf00;
				break;

			case IO_AIU_BUS:
				m8_ops[i].ext_offset = -0x100;
				break;

			case IO_DEMUX_BUS:
				m8_ops[i].ext_offset = 0x200;
				break;

			case IO_RESET_BUS:
				t = get_cpu_type();
				if (t >= MESON_CPU_MAJOR_ID_SC2 &&
					t != MESON_CPU_MAJOR_ID_T5 &&
					t != MESON_CPU_MAJOR_ID_T5D &&
					t != MESON_CPU_MAJOR_ID_T5W)
					m8_ops[i].ext_offset = 0;
				else
					m8_ops[i].ext_offset = -0xd00;
				break;
			}
		}
	}
	register_reg_ops_mgr(m8_ops,
			     sizeof(m8_ops) / sizeof(struct chip_register_ops));

	register_reg_ex_ops_mgr(ex_gx_ops,
				sizeof(ex_gx_ops) /
				sizeof(struct chip_register_ops));
	return 0;
}

