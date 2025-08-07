#ifndef KERNEL_KMALLOC_H_
#define KERNEL_KMALLOC_H_

#include "types.h"

void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
