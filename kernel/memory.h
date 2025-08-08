#ifndef KERNEL_MEMORY_H_
#define KERNEL_MEMORY_H_

#include "types.h"

void copy_memory(void *dst, const void *src, size_t size);
void clear_memory(void *dst, size_t size);
void set_memory(void *dst, int value, size_t size);

uint64_t mmio_read64(uintptr_t ptr);
void mmio_write64(uintptr_t ptr, uint64_t value);

uint32_t mmio_read32(uintptr_t ptr);
void mmio_write32(uintptr_t ptr, uint32_t val);

#endif
