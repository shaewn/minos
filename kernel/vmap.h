#ifndef KERNEL_VMAP_H_
#define KERNEL_VMAP_H_

#include "types.h"
#include "memory_type.h"

#define VMAP_ERROR_INVALID_PROT -1
#define VMAP_ERROR_TABLE_NOMEM -2
#define VMAP_ERROR_ALREADY_MAPPED -3

#define VMAP_FLAG_REMAP 0x1

#define VUMAP_ERROR_NOT_MAPPED -1

// uintptr_t first_addr_avail(uintptr_t start);
int vmap(uintptr_t va, uintptr_t pa, uint64_t prot, memory_type_t memory_type, int flags);
int vumap(uintptr_t va);

int vmap_range(uintptr_t start_va, uintptr_t start_pa, size_t pages, uint64_t prot, memory_type_t memory_type, int flags);
int vumap_range(uintptr_t start_va, size_t pages);

void *ioremap(uintptr_t start_pa, size_t bytes);

uintptr_t get_phys_mapping(uintptr_t va);

void enable_global_tlb_invalidate(void);

#endif
