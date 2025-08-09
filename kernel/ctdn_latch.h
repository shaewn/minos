#ifndef KERNEL_CTDN_LATCH_H_
#define KERNEL_CTDN_LATCH_H_

#include "types.h"

typedef uint32_t ctdn_latch_t;

void ctdn_latch_set(ctdn_latch_t *latch, uint32_t value);
void ctdn_latch_signal(ctdn_latch_t *latch);
void ctdn_latch_wait(ctdn_latch_t *latch);

#endif
