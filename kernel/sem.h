#ifndef KERNEL_SEM_H_
#define KERNEL_SEM_H_

#include "types.h"

typedef uint32_t sem_t;

void sem_init(volatile sem_t *sem, uint32_t value);
void sem_post(volatile sem_t *sem);
void sem_wait(volatile sem_t *sem);

#endif
