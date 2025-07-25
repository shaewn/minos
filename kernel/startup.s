.global _start
_start:
    ldr x0, =stack_top
    mov sp, x0
    // ldr x8, =addr
    // mov w9, 'h'
    // str w9, [x8]
    bl kmain
spin: b spin

// .equ addr, 0x09000000
