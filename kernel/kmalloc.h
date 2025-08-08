#ifndef KERNEL_KMALLOC_H_
#define KERNEL_KMALLOC_H_

#include "types.h"

// Don't perform a tlb shootdown for the virtual address(es).
#define KMALLOC2_PRIVATE 0x1

void *kmalloc2(size_t size, int flags);

void *kmalloc(size_t size);
void *krealloc(void *ptr, size_t new_size);
void kfree(void *ptr);

#endif
