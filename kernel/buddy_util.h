#ifndef KERNEL_BUDDY_UTIL_H_
#define KERNEL_BUDDY_UTIL_H_

#include "types.h"

uint64_t get_buddy(uint64_t idx);
uint64_t get_first_page(uint64_t order, uint64_t idx);
uint64_t get_num_pages(uint64_t order);
uint64_t get_block_index(uint64_t order, uint64_t page_index);
uint64_t get_block_count(uint64_t order, uint64_t page_count);

#endif
