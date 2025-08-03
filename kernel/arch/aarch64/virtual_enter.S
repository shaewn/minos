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
    ldr x8, =memory_map_phys_start
    str x9, [x8]

    ldr x9, =memory_map_end_init
    ldr x9, [x9]
    ldr x8, =memory_map_phys_end
    str x9, [x8]

    ldr x9, =fdt_header
    str x0, [x9]

    ldr x9, =kvmain
    br x9
    b .
