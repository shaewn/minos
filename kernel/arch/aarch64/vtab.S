    .section .vector, "ax"
    .global virtual_vector_base
    .p2align 11
virtual_vector_base:
    .fill 0x200
    bl kernel_ehandler
    eret
