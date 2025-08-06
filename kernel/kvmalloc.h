#ifndef KERNEL_KVMALLOC_H_
#define KERNEL_KVMALLOC_H_

#include "types.h"

// This vma will never be freed.
#define KVMALLOC_PERMANENT 0x1

void kvmalloc_init(void);

/* reserves space in the kernel's virtual memory heap.
   does not allocate physical memory for the corresponding region, or establish a mapping to any physical memory.
   in order to acquire physical memory, see {global_}acquire_(block|pages|bytes).
   in order to establish a mapping between a virtual page and physical page, see vmap.
   */
void *kvmalloc(size_t pages, int flags);

#endif
