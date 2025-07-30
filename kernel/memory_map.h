#ifndef KERNEL_MEMORY_MAP_H_
#define KERNEL_MEMORY_MAP_H_

#include "types.h"

struct page {
    uintptr_t addr;
    uint64_t heap_index, page_index;
    uint8_t allocated;
};

void create_memory_map(void);

#endif
