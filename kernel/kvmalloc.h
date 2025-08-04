#ifndef KERNEL_KVMALLOC_H_
#define KERNEL_KVMALLOC_H_

#include "types.h"

#define KVMALLOC_PERMANENT 0x1

void *kvmalloc(uint64_t pages, int flags);

#endif
