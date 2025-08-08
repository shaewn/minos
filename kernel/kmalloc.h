#ifndef KERNEL_KMALLOC_H_
#define KERNEL_KMALLOC_H_

#include "types.h"

void *kmalloc(size_t size);
void *krealloc(void *ptr, size_t new_size);
void kfree(void *ptr);

#endif
