#ifndef KERNEL_MEMORY_MAP_H_
#define KERNEL_MEMORY_MAP_H_

#include "types.h"
#include "bspinlock.h"

/* returns -1 on failure, 0 on success */
int acquire_block(struct buddy_allocator * _Nonnull alloc, uint64_t order, uintptr_t * _Nonnull region_start, uintptr_t * _Nullable region_end);
void release_block(struct buddy_allocator * _Nonnull alloc, uintptr_t region_begin);

#endif
