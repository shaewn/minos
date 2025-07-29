    .text
.global _start
.global die
_start:
    adrp x8, dtb_addr
    add x8, x8, :lo12:dtb_addr
    str x0, [x8] // x0 contains the dtb address.

    adrp x8, __stack_top
    add x8, x8, :lo12:__stack_top
    mov sp, x8

    bl kinit
die: b die

// .equ addr, 0x09000000
