#include "defines.h"

    .section .init.text, "ax"
    .global sndry_enter
sndry_enter:
    // x0 contains a physical pointer to a struct sndry_ctx
    ldr x6, [x0] // percpu_base
    ldr x7, [x0, 8] // stack
    ldr x8, [x0, 16] // ttbr0
    ldr x9, [x0, 24] // ttbr1
    ldr x10, [x0, 32] // tcr

    msr tpidr_el1, x6
    mov sp, x7
    msr ttbr0_el1, x8
    msr ttbr1_el1, x9
    msr tcr_el1, x10

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
    bl kvmain2
    b .
