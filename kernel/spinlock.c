#include "spinlock.h"
#include "pltfrm.h"
#include "cpu.h"
#include "memory.h"

#define UNLOCKED 0
#define LOCKED 1

void spin_lock_init(volatile spinlock_t *lock) {
    lock->holder = CPU_INVALID;
    __atomic_store_n(&lock->flag, UNLOCKED, __ATOMIC_RELEASE);
}

void spin_lock_irq_save(volatile spinlock_t *lock) {
    uint8_t expected;

    bool masked_val;

    while (1) {
        while (__atomic_load_n(&lock->flag, __ATOMIC_RELAXED) != UNLOCKED)
            cpu_idle_wait(lock);

        expected = UNLOCKED;

        masked_val = irqs_masked();
        mask_irqs();
        if (__atomic_compare_exchange_n(&lock->flag, &expected, LOCKED, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            break;
        }

        restore_irq_mask(masked_val);
    }

    lock->holder = this_cpu();
    lock->masked_val = masked_val;
}

void spin_unlock_irq_restore(volatile spinlock_t *lock) {
    cpu_t holder = lock->holder;
    if (holder != this_cpu()) {
        return;
    }
    // Nothing about the lock can change while we're the owner, since they must
    // acquire it by us releasing it.

    lock->holder = CPU_INVALID;

    bool masked_val = lock->masked_val;
    __atomic_store_n(&lock->flag, UNLOCKED, __ATOMIC_RELEASE);
    restore_irq_mask(masked_val);
    cpu_signal_all(lock);
}
