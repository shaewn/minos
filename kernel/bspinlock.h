#ifndef _BSPINLOCK_H_
#define _BSPINLOCK_H_

#include "types.h"

void bspinlock_lock(volatile bspinlock_t *lock);
void bspinlock_unlock(volatile bspinlock_t *lock);

#endif
