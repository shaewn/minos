#ifndef KERNEL_MEMORY_MAP_H_
#define KERNEL_MEMORY_MAP_H_

#include "types.h"
#include "spinlock.h"

void reserve_active_kernel_memory(void);
void vmap_memory_map(void);

/* returns -1 on failure, 0 on success */
int acquire_block(struct buddy_allocator * _Nonnull alloc, uint64_t order, uintptr_t * _Nonnull region_start, uintptr_t * _Nullable region_end);
void release_block(struct buddy_allocator * _Nonnull alloc, uintptr_t region_start);

int acquire_pages(struct buddy_allocator * _Nonnull alloc, uint64_t pages, uintptr_t * _Nonnull region_begin, uintptr_t * _Nullable region_end);
int acquire_bytes(struct buddy_allocator * _Nonnull alloc, uint64_t bytes, uintptr_t * _Nonnull region_begin, uintptr_t * _Nullable region_end);

int global_acquire_block(uint64_t order, uintptr_t * _Nonnull region_start, uintptr_t *_Nullable region_end);
int global_acquire_pages(uint64_t pages, uintptr_t * _Nonnull region_start, uintptr_t *_Nullable region_end);
int global_acquire_bytes(uint64_t bytes, uintptr_t * _Nonnull region_start, uintptr_t *_Nullable region_end);

void global_release_block(uintptr_t region_start);

void dump_memory_map(uint64_t min_order, uint64_t max_order);
void dump_allocated_blocks(struct heap_data *heap, uint64_t order);

#endif
