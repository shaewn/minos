#include "defines.h"

    .section .init.text, "ax"
    .global virtual_enter

// [[noreturn]] void virtual_enter(struct fdt_header *)
virtual_enter:
    ldr x9, =kernel_brk_init
    ldr x9, [x9]
    ldr x8, =kernel_brk
    str x9, [x8]

    ldr x9, =__kernel_start_phys
    ldr x8, =kernel_start
    str x9, [x8]

    ldr x9, =__kernel_end_phys
    ldr x8, =kernel_end
    str x9, [x8]

    ldr x9, =memory_map_start_init
    ldr x9, [x9]
    mov x10, x9
    ldr x8, =memory_map_phys_start
    str x9, [x8]

    ldr x9, =memory_map_end_init
    ldr x9, [x9]
    mov x11, x9
    ldr x8, =memory_map_phys_end
    str x9, [x8]

    ldr x20, =__init_end

    ldr x9, =KERNEL_VIRT_BEGIN
    sub x6, x10, x20
    sub x7, x11, x20
    add x6, x6, x9
    add x7, x7, x9

    ldr x8, =memory_map_addr_start
    str x6, [x8]
    ldr x8, =memory_map_addr_end
    str x7, [x8]

    ldr x9, =fdt_header
    str x0, [x9]

    ldr x9, =__stack_top
    mov sp, x9

    ldr x0, =kernel_vbrk_init
    ldr x0, [x0]

    ldr x9, =virtual_vector_base
    msr vbar_el1, x9

    ldr x9, =vstart
    br x9

    .text
    // Trampoline, because we will un-idmap everything.
vstart:
    bl kvmain
    b .
