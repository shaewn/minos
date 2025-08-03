#ifndef KERNEL_VMALLOC_H_
#define KERNEL_VMALLOC_H_

#include "types.h"

#define VSA_ERROR_INVALID_PROT -1

int vmap_page(void *vaddr, uint64_t backing_address, uint64_t prot);
void *vmalloc(void *hint, uint64_t pages);

#endif
