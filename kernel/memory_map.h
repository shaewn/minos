#ifndef KERNEL_MEMORY_MAP_H_
#define KERNEL_MEMORY_MAP_H_

#include "types.h"
#include "bspinlock.h"

struct page {
    uintptr_t addr;
    uint64_t heap_index, page_index;

    /* when not allocated, allocate = 0 */
    /* when allocated, allocated = order of allocation + 1 */
    /* e.g., if this page is allocated in an order 0 block (i.e., it's allocated as a page), allocated = 1*/
    /* if this page is allocated in an order 1 block (i.e., in a pair of pages), allocated = 2 */
    uint64_t allocated;
};

/*
   Structure of the memory map:

   - USEFUL STRUCTURES -
    */

#define HEAP_FIRST_PAGE(heap) ((struct page *)((char *)(heap) + (heap)->pages_start))
#define HEAP_GET_BUDDY(heap) ((struct buddy_allocator *)((char *)(heap) + (heap)->buddy_start))

struct heap_data {
    uint64_t addr, pages;

    /* offset from the START OF THIS STRUCTURE to the START of the page array for this heap */
    uint64_t pages_start;

    /* offset from the START OF THIS STRUCTURE to the START of the buddy allocator data for this
     * heap */
    uint64_t buddy_start;
};

struct buddy_data {
    uint8_t allocated;
};

#define BUDDY_GET_HEAP(buddy) ((struct heap_data *)((char *)(buddy) + (buddy)->heap_data_offset))
#define BUDDY_ORDER_GET_DATA(order) ((struct buddy_data *)((char *)(order) + (order)->data_offset))
struct buddy_allocator {
    intptr_t heap_data_offset;
    bspinlock_t lock;
    uint64_t num_orders;
    struct buddy_order {
        uint64_t num_blocks;
        uintptr_t data_offset;
    } orders[];
};

/*
   at the beginning of the memory map,
   there is an array of struct heap_data,
   terminated by a fully zeroed out struct heap_data.

   each heap_data specifies a different contiguous physical address range (as provided by the DTB)

   next is several arrays of struct page.
   there is no terminator between the arrays.
   there is one array per heap.
   the heap stores the offset (in bytes) from the beginning of the heap_data structure to the
   beginning of its page array

   next is an array of pairs of struct buddy_allocator and struct buddy_data.
   the heap_data structure stores the offset (in bytes) from the beginning of the heap_data
   structure to the beginning of its buddy allocator the struct buddy_allocator contains a NULL
   terminated array of offsets from the beginning of the struct buddy_allocator to the first struct
   buddy_data in each order.

   next is a contiguous region of struct buddy_data's
   there are no terminators between buddy_data's for each order.

   */

void create_memory_map(void);

/* returns -1 on failure, 0 on success */
int acquire_block(struct buddy_allocator * _Nonnull alloc, uint64_t order, uintptr_t * _Nonnull region_start, uintptr_t * _Nullable region_end);
void release_block(struct buddy_allocator * _Nonnull alloc, uintptr_t region_begin);

#endif
