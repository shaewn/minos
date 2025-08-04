    .section .vector, "ax"
    .global virtual_vector_base
virtual_vector_base:
    .fill 0x200
    bl kernel_ehandler
    eret
