#ifndef _KERNEL_ATOMIC_H_
#define _KERNEL_ATOMIC_H_

#include "types.h"

// Compare exchange with acquire, release semantics
extern uint8_t cmpxchg_byte(volatile uint8_t *value, uint8_t *expected, uint8_t desired);

#endif
