/*
 * arch/arm/mach-meson/meson-smc.S
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

#include <linux/linkage.h>
#include <linux/amlogic/meson-secure.h>

.arch_extension sec

ENTRY(meson_smc1)
	stmfd sp!, {r2-r12, lr}
	mov r2, r1
	mov r1, r0
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_smc1)

ENTRY(meson_smc_hal_api)
	stmfd sp!, {r3-r12, lr}
	mov r2, r1
	mov r1, r0
	ldr r0, =CALL_TRUSTZONE_HAL_API
	dsb
	smc #0
	ldmfd sp!, {r3-r12, pc}
ENDPROC(meson_smc_hal_api)

ENTRY(meson_smc2)
	stmfd sp!, {r2-r12, lr}
	mov r1, r0
	ldr r0, =0x2
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_smc2)

ENTRY(meson_smc3)
	stmfd sp!, {r2-r12, lr}
	mov r2, r1
	mov r1, r0
	ldr r0, =0x3
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_smc3)

ENTRY(meson_read_corectrl)
	stmfd sp!, {r2-r12, lr}
	ldr r1, =TRUSTZONE_MON_CORE_RD_CTRL_INDEX
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_read_corectrl)

ENTRY(meson_modify_corectrl)
	stmfd sp!, {r2-r12, lr}
	mov r2, r0
	ldr r1, =TRUSTZONE_MON_CORE_WR_CTRL_INDEX
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_modify_corectrl)

ENTRY(meson_read_corestatus)
	stmfd sp!, {r2-r12, lr}
	ldr r1, =TRUSTZONE_MON_CORE_RD_STATUS0_INDEX
	add r1, r1, r0, lsl #1
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_read_corestatus)

ENTRY(meson_modify_corestatus)
	stmfd sp!, {r2-r12, lr}
	mov r2, r1
	ldr r1, =TRUSTZONE_MON_CORE_WR_STATUS0_INDEX
	add r1, r1, r0, lsl #1
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_modify_corestatus)

ENTRY(meson_auxcoreboot_addr)
	stmfd sp!, {r2-r12, lr}
	mov r3, r1
	mov r2, r0
	ldr r1, =TRUSTZONE_MON_CORE_BOOTADDR_INDEX
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_auxcoreboot_addr)

ENTRY(meson_suspend_firmware)
	stmfd sp!, {r2-r12, lr}
	mov r2, r0
	ldr r1, =TRUSTZONE_MON_SUSPNED_FIRMWARE
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_suspend_firmware)

ENTRY(meson_read_socrev1)
	stmfd sp!, {r2-r12, lr}
	ldr r1, =TRUSTZONE_MON_CORE_RD_SOC_REV1
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_read_socrev1)

ENTRY(meson_read_socrev2)
	stmfd sp!, {r2-r12, lr}
	ldr r1, =TRUSTZONE_MON_CORE_RD_SOC_REV2
	ldr r0, =CALL_TRUSTZONE_MON
	dsb
	smc #0
	ldmfd sp!, {r2-r12, pc}
ENDPROC(meson_read_socrev2)
