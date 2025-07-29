.global _start
.global die
_start:
    ldr x8, =dtb_addr
    str x0, [x8] // x0 contains the dtb address.

    ldr x8, =stack_top
    mov sp, x8

    bl kinit
spin: b spin

die: b spin

// .equ addr, 0x09000000
