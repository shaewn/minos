#include "defines.h"

    .section .init.text, "ax"
    .global sndry_enter
sndry_enter:
    // x0 contains a physical pointer to a struct sndry_ctx
    ldr x21, [x0] // percpu_base
    ldr x7, [x0, 8] // stack
    ldr x8, [x0, 16] // ttbr0
    ldr x9, [x0, 24] // ttbr1
    ldr x10, [x0, 32] // tcr
    ldr x11, [x0, 40] // mair

    mov sp, x7
    msr ttbr0_el1, x8
    msr ttbr1_el1, x9
    msr tcr_el1, x10
    msr mair_el1, x11

    tlbi vmalle1
    dsb ish
    isb

    mrs x4, sctlr_el1
    mov x1, 5
    orr x4, x4, x1 // enable caching and mmu
    msr sctlr_el1, x4

    isb

    ldr x8, =sndry_virt_up
    br x8

    .text
sndry_virt_up:
    adrp x0, virtual_vector_base
    add x0, x0, :lo12:virtual_vector_base
    msr vbar_el1, x0

    mov x0, x21
    bl secondary_main
    b .
