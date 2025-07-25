#include "bspinlock.h"
#include "atomic.h"

#define UNLOCKED 0
#define LOCKED 1

void bspinlock_lock(volatile bspinlock_t *lock) {
    uint8_t locked_expected = UNLOCKED;

    // return value: true iff still matches expected
    while (!cmpxchg_byte(&lock->flag, &locked_expected, LOCKED)) {
        locked_expected = UNLOCKED;
    }

    lock->holder = cpu_current();
}

void bspinlock_unlock(volatile bspinlock_t *lock) {
    if (lock->holder != cpu_current()) {
        return;
    }
    // Nothing about the lock can change while we're the owner, since they must acquire it by us
    // releasing it.

    uint8_t locked_expected = LOCKED;
    cmpxchg_byte(&lock->flag, &locked_expected, UNLOCKED);
}
