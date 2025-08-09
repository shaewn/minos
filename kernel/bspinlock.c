#include "bspinlock.h"
#include "atomic.h"
#include "cpu.h"

#define UNLOCKED 0
#define LOCKED 1

void bspinlock_lock(volatile bspinlock_t *lock) {
    uint8_t locked_expected = UNLOCKED;

    // return value: true iff still matches expected
    while (!cmpxchg_byte(&lock->flag, &locked_expected, LOCKED)) {
        locked_expected = UNLOCKED;
    }

    lock->holder = this_cpu();
}

void bspinlock_unlock(volatile bspinlock_t *lock) {
    if (lock->holder != this_cpu()) {
        return;
    }
    // Nothing about the lock can change while we're the owner, since they must acquire it by us
    // releasing it.

    uint8_t locked_expected = LOCKED;
    cmpxchg_byte(&lock->flag, &locked_expected, UNLOCKED);
}
