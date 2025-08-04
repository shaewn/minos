#ifndef KERNEL_VMALLOC_H_
#define KERNEL_VMALLOC_H_

#include "types.h"
#include "memory_type.h"

#define VMP_ERROR_INVALID_PROT -1
#define VMP_ERROR_TABLE_NOMEM -2
#define VMP_ERROR_ALREADY_MAPPED -3

int vmap_page(uintptr_t va, uintptr_t pa, uint64_t prot, memory_type_t memory_type);
void *vmalloc(uintptr_t hint, uint64_t pages);

#endif
