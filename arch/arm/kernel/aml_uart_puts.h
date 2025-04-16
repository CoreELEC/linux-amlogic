/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AML_UART_PUTS_H
#define __AML_UART_PUTS_H
.macro puts_uart_m base, tmp1, tmp2, str
    adr     \base, 3f
    ldr     \base, [\base]

    adr     \tmp1, 2f

    ldr     \tmp2, [\base, #8]
    orr     \tmp2, \tmp2, #(1 << 12)
    bic     \tmp2, \tmp2, #(3 << 27)
    dmb     sy
    str     \tmp2, [\base, #8]
    dmb     sy

1:
    ldr     \tmp2, [\base, #0xc]
    tst     \tmp2, #(1 << 21)
    bne     1b

    ldrb    \tmp2, [\tmp1], #1
    cmp     \tmp2, #0
    beq     4f

    strb    \tmp2, [\base]
    b       1b

2:
    .ascii  "\str"
    .byte   0
    .align  4

3:
    .word   CONFIG_DEBUG_UART_PHYS

4:
    nop
.endm
#endif /* __AML_UART_PUTS_H */
