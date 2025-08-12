    .text
    .global __aarch64_cas1_acq_rel
    .global __aarch64_cas1_acq
    .global __aarch64_cas1_rel

    .global __aarch64_cas4_acq_rel
    .global __aarch64_cas4_acq
    .global __aarch64_cas4_rel
    .global __aarch64_cas4_relax

    .global __aarch64_ldadd4_rel
    .global __aarch64_ldadd4_acq
    .global __aarch64_ldadd4_acq_rel
    .global __aarch64_ldadd4_relax
// uint8_t __aarch64_cas1_acq_rel(uint8_t expected, uint8_t desired, volatile uint8_t *ptr);
__aarch64_cas1_acq_rel:
    casalb w0, w1, [x2]
    // w0 contains pre-operation data.
    ret
__aarch64_cas1_acq:
    casab w0, w1, [x2]
    // w0 contains pre-operation data.
    ret
__aarch64_cas1_rel:
    caslb w0, w1, [x2]
    // w0 contains pre-operation data.
    ret


__aarch64_cas4_acq_rel:
    casal w0, w1, [x2]
    // w0 contains pre-operation data.
    ret
__aarch64_cas4_acq:
    casa w0, w1, [x2]
    // w0 contains pre-operation data.
    ret
__aarch64_cas4_rel:
    casl w0, w1, [x2]
    // w0 contains pre-operation data.
    ret
__aarch64_cas4_relax:
    cas w0, w1, [x2]
    // w0 contains pre-operation data.
    ret


// uint32_t __aarch64_ldadd4_rel(uint32_t addend, uint32_t *ptr)
__aarch64_ldadd4_rel:
    ldaddl w0, w1, [x1]
    mov w0, w1
    ret
__aarch64_ldadd4_acq:
    ldadda w0, w1, [x1]
    mov w0, w1
    ret
__aarch64_ldadd4_acq_rel:
    ldaddal w0, w1, [x1]
    mov w0, w1
    ret
__aarch64_ldadd4_relax:
    ldadd w0, w1, [x1]
    mov w0, w1
    ret
