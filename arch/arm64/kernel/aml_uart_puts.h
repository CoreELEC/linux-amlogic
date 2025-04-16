/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AML_UART_PUTS_H
#define __AML_UART_PUTS_H

//puts_uart_m - print string to uart
//@base: save uart register base address
//@tmp1 @tmp2: temp register
//@str: string to print
.macro puts_uart_m base tmp1 tmp2 str
	adr \base, 3f
	ldr \base, [\base]
        adr \tmp1, 2f

        ldr \tmp2, [\base,#8]
        orr \tmp2, \tmp2, #(1 << 12)      //set tx_en
        bic \tmp2, \tmp2, #(3 << 27)      //clear rx_int_en/tx_int_en
        dmb sy
        str \tmp2, [\base, #8]
        dmb sy
1:
        ldr \tmp2, [\base, #0xc]    //get status
        tbnz \tmp2, #21, 1b         //test tx full
        dmb sy
        ldrb \tmp2, [\tmp1], #1
        cmp \tmp2, #0x0             //is string end?
        b.eq 4f
        str \tmp2, [\base]          //put char
        b 1b
2:
	.ascii "\str"
	.long 0			    //str doesn't contain '\0'
	.align 8                    //@base must 8 bytes aligned
3:
	.long CONFIG_AMLOGIC_DEBUG_UART_PHYS	    //low  4 bytes of @base
	.long 0                     //high 4 bytes of @base
4:
	nop
.endm

#endif /* __AML_UART_PUTS_H */
