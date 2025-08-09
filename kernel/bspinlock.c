#include "bspinlock.h"
#include "pltfrm.h"
#include "cpu.h"
#include "memory.h"

#define UNLOCKED 0
#define LOCKED 1

void bspinlock_lock(volatile bspinlock_t *lock) {
    uint8_t expected;

    while (1) {
        while (__atomic_load_n(&lock->flag, __ATOMIC_RELAXED) != UNLOCKED)
            cpu_idle_wait();

        expected = UNLOCKED;
        if (__atomic_compare_exchange_n(&lock->flag, &expected, LOCKED, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            break;
        }
    }

    lock->holder = this_cpu();
}

void bspinlock_unlock(volatile bspinlock_t *lock) {
    // Access once, use twice.
    cpu_t holder = lock->holder;
    if (holder != this_cpu()) {
        return;
    }
    // Nothing about the lock can change while we're the owner, since they must
    // acquire it by us releasing it.

    lock->holder = CPU_INVALID;

    __atomic_store_n(&lock->flag, UNLOCKED, __ATOMIC_RELEASE);
    cpu_signal_all();
}
