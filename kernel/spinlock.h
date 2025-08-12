#ifndef KERNEL_SPINLOCK_H_
#define KERNEL_SPINLOCK_H_

#include "types.h"

void spin_lock_init(volatile spinlock_t *lock);

void spin_lock_irq_save(volatile spinlock_t *lock);
void spin_unlock_irq_restore(volatile spinlock_t *lock);

#endif
