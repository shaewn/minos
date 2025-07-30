#include "buddy_util.h"

uint64_t get_buddy(uint64_t idx) {
    return idx ^ (uint64_t)1;
}

uint64_t get_first_page(uint64_t order, uint64_t idx) {
    return idx << order;
}

uint64_t get_num_pages(uint64_t order) {
    return 1 << order;
}

uint64_t get_block_index(uint64_t order, uint64_t page_index) {
    return page_index >> order;
}

uint64_t get_block_count(uint64_t order, uint64_t page_count) {
    uint64_t pages_per_block = get_num_pages(order); // power of two

    return (page_count + pages_per_block - 1) >> order;
}
