#include "pltfrm.h"

    .section .init.text, "ax"
    .global _start
    .global early_die
_start:
    ldr x19, =uart_addr
    mov w9, '$'
    str w9, [x19]
    mov w9, '\n'
    str w9, [x19]

    ldr x9, =__stack_top_phys
    mov sp, x9

    // dtb address still in x0
    bl kinit
early_die: b .

    .equ uart_addr, 0x09000000

    .text
    .global die
die: b .
