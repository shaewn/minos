#ifndef KERNEL_KVMALLOC_H_
#define KERNEL_KVMALLOC_H_

#include "types.h"

// This allocation will never be freed.
#define KVMALLOC_PERMANENT 0x1

// This allocation won't cause a TLB shootdown.
#define KVMALLOC_PRIVATE 0x2

void kvmalloc_init(void);
void *kvmalloc(uint64_t pages, int flags);

#endif
