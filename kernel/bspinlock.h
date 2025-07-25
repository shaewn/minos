#ifndef _BSPINLOCK_H_
#define _BSPINLOCK_H_

#include "cpu.h"
#include "types.h"

typedef struct bspinlock {
    uint8_t flag;
    cpu_t holder;
} bspinlock_t;

void bspinlock_lock(volatile bspinlock_t *lock);
void bspinlock_unlock(volatile bspinlock_t *lock);

#endif
