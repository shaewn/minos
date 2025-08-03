#include "pltfrm.h"

    .section .init.text, "ax"
    .global _start
    .global early_die
_start:
    adrp x9, vector_base
    add x9, x9, :lo12:vector_base
    msr vbar_el1, x9

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

    .section .init.vector, "ax"
    .p2align 11
vector_base:
    .fill 0x200
    bl early_ehandler
    b .
