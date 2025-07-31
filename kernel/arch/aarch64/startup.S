#include "pltfrm.h"

    .section .init.text, "ax"
    .global _start
    .global early_die
_start:
    ldr x19, =uart_addr
    mov w9, 'h'
    str w9, [x19]

#if PAGE_SIZE == 4096
    mov x9, 0
#elif PAGE_SIZE == 16384
    mov x9, 2
#elif PAGE_SIZE == 65536
    mov x9, 1
#else
#error Invalid PAGE_SIZE for AArch64
#endif

    // Set proper page size.
    mrs x8, tcr_el1
    ldr x10, =0xc000
    and x8, x8, x10
    lsl x9, x9, #14
    orr x8, x8, x9

    msr tcr_el1, x8

    // Set stack pointer
    ldr x8, =__stack_top_phys
    mov sp, x8

    // dtb address still in x0
    bl kinit
early_die: b .

    .equ uart_addr, 0x09000000

    .text
    .global die
die: b .
