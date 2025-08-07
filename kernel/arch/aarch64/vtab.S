    .section .vector, "ax"
    .global virtual_vector_base
    .p2align 11
virtual_vector_base:
    .fill (0x80 - (. - virtual_vector_base))
    bl irq_handler
    eret
    .fill (0x200 - (. - virtual_vector_base))
    bl kernel_ehandler
    eret
