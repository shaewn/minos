    .text
    .global cmpxchg_byte

// extern uint8_t cmpxchg_byte(volatile uint8_t *value, uint8_t *expected, uint8_t desired);
cmpxchg_byte:
    ldrb w8, [x1]
    casab w8, w2, [x0]
    strb w8, [x1]
    ret
