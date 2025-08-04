    .text
    .global cmpxchg_byte

// extern uint8_t cmpxchg_byte(volatile uint8_t *value, uint8_t *expected, uint8_t desired);
cmpxchg_byte:
    ldrb w8, [x1]
    mov w9, w8
    casalb w8, w2, [x0]
    // w8 contains the pre-operation data at address `value'
    strb w8, [x1]

    // return true if we changed the value, false if we didn't change the value.
    // if the pre-operation data matches their expected data, we changed it.
    cmp w8, w9
    cset w0, eq
    ret
