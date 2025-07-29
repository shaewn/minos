#ifndef KERNEL_MEMORY_H_
#define KERNEL_MEMORY_H_

#include "types.h"

void copy_memory(void *dst, const void *src, size_t size);
void clear_memory(void *dst, size_t size);
void set_memory(void *dst, int value, size_t size);

#endif
