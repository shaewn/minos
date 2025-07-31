#include "memory_map.h"
#include "bspinlock.h"
#include "buddy_util.h"
#include "die.h"
#include "output.h"
#include "pltfrm.h"

extern char *kernel_brk;

/* physical address range for the memory map */
char *memory_map_start, *memory_map_end;

bspinlock_t mm_lock;

void dump_memory_map(uint64_t min_order, uint64_t max_order) {
    for (struct heap_data *heap = (struct heap_data *)memory_map_start; heap->pages; heap++) {
        kprint("Heap at 0x%lx has %lu pages.\n", heap->addr, heap->pages);
        void dump_allocated_blocks(struct heap_data * heap, uint64_t order);

        struct buddy_allocator *alloc = HEAP_GET_BUDDY(heap);

        for (uint64_t order = min_order; order < alloc->num_orders && order <= max_order; order++) {
            kputstr("\n");
            dump_allocated_blocks(heap, order);
            kputstr("\n");
        }
    }
}

void trickle_up_allocate_pages(struct buddy_allocator *alloc, uint64_t page_first,
                               uint64_t page_past_last);

void dump_allocated_blocks(struct heap_data *heap, uint64_t order) {
    struct buddy_allocator *alloc = (struct buddy_allocator *)((char *)heap + heap->buddy_start);

    struct buddy_order *buddy_order = alloc->orders + order;
    struct buddy_data *data_start =
        (struct buddy_data *)((char *)buddy_order + buddy_order->data_offset);
    struct page *heap_first_page = HEAP_FIRST_PAGE(heap);

    uint32_t page_size = PAGE_SIZE;

    for (uint64_t i = 0; i < buddy_order->num_blocks; i++) {
        struct buddy_data *data = data_start + i;
        if (data->allocated) {
            struct page *page_first, *page_last;
            uintptr_t first_addr, last_addr;

            page_first = heap_first_page + get_first_page(order, i);
            page_last = page_first + get_num_pages(order) - 1;
            first_addr = page_first->addr;
            last_addr = page_last->addr + page_size;
            kprint("Block %lu on order %lu is allocated (0x%lx-0x%lx)\n", i, order, first_addr,
                   last_addr);
        }
    }
}

void trickle_up_range_allocate(struct buddy_allocator *alloc, uint64_t order, uint64_t block_first,
                               uint64_t block_past_last);

/* allocate all of these pages and trickle up the allocations in the buddy tree. */
/* WARNING: Non-locking */
void trickle_up_allocate_pages(struct buddy_allocator *alloc, uint64_t page_first,
                               uint64_t page_past_last) {
    struct heap_data *heap = BUDDY_GET_HEAP(alloc);
    struct page *heap_first_page = HEAP_FIRST_PAGE(heap);
    struct page *first_page, *one_past_last_page;
    first_page = heap_first_page + page_first;
    one_past_last_page = heap_first_page + page_past_last;

    uint64_t num_pages = page_past_last - page_first;

    for (struct page *page = first_page; page != one_past_last_page; page++) {
        page->allocated = 1;
    }

    trickle_up_range_allocate(alloc, 0, page_first, page_past_last);
}

/* WARNING: Non-locking */
// NOTE: Trickle up does not set any values in the underlying pages.
void trickle_up_range_allocate(struct buddy_allocator *alloc, uint64_t bottom_order,
                               uint64_t block_first, uint64_t block_past_last) {
    uint64_t current_order = bottom_order;

    uint64_t num_blocks = block_past_last - block_first;
    uint64_t first_block = block_first;

    while (current_order < alloc->num_orders) {
        struct buddy_order *buddy_order = alloc->orders + current_order;
        struct buddy_data *data_first = BUDDY_ORDER_GET_DATA(buddy_order);

        for (uint64_t block = 0; block < num_blocks; block++) {
            struct buddy_data *data = data_first + (first_block + block);
            data->allocated = 1;
        }

        current_order++;
        first_block >>= 1;

        // If we have an odd number, we have to allocate the odd-one-out's buddy as well.
        num_blocks = (num_blocks + 1) >> 1;
    }
}

/* WARNING: Non-locking */
/* allocating - 1 or 0 depending on whether we're freeing or allocating */
void trickle_down_range(struct buddy_allocator *alloc, uint64_t top_order, uint64_t block_first,
                        uint64_t block_past_last, int allocating) {
    uint64_t order_counter = top_order + 1;

    uint64_t num_blocks = block_past_last - block_first;
    uint64_t first_block = block_first;

    uint64_t num_pages, first_page;

    if (allocating != 0 && allocating != 1) {
        KFATAL("Weird allocating value %d\n", allocating);
    }

    while (order_counter != 0) {
        uint64_t current_order = order_counter - 1;

        struct buddy_order *buddy_order = alloc->orders + current_order;
        struct buddy_data *data_first = BUDDY_ORDER_GET_DATA(buddy_order);

        for (uint64_t block = 0; block < num_blocks; block++) {
            struct buddy_data *data = data_first + (first_block + block);
            data->allocated = allocating;
        }

        num_pages = num_blocks;
        first_page = first_block;

        num_blocks <<= 1;
        first_block <<= 1;

        order_counter--;
    }

    struct heap_data *heap = BUDDY_GET_HEAP(alloc);
    struct page *heap_first_page = HEAP_FIRST_PAGE(heap);

    for (uint64_t page = 0; page < num_pages; page++) {
        struct page *cur_page = heap_first_page + first_page + page;

        if (allocating) {
            cur_page->allocated = top_order + 1;
        } else {
            cur_page->allocated = 0;
        }
    }
}

/* allocate contiguous physical memory region of order 'order'. the range allocated is
 * [*region_start, *region_end) */
int acquire_block(struct buddy_allocator *alloc, uint64_t order, uintptr_t *region_start,
                  uintptr_t *region_end) {
    uint32_t page_size = PAGE_SIZE;

    struct buddy_order *buddy_order = alloc->orders + order;

    if (!region_start) {
        KFATAL("region_start must not be NULL\n");
    }

    bspinlock_lock(&alloc->lock);

    struct buddy_data *data_first = BUDDY_ORDER_GET_DATA(buddy_order);
    struct buddy_data *choice = NULL;

    // block index of the choice
    uint64_t choice_index = 0;
    while (!choice && choice_index < buddy_order->num_blocks) {
        struct buddy_data *data = data_first + choice_index;

        if (!data->allocated) {
            choice = data;
        } else {
            choice_index++;
        }
    }

    int retval = -1;

    if (choice) {
        trickle_down_range(alloc, order, choice_index, choice_index + 1, 1);
        trickle_up_range_allocate(alloc, order, choice_index, choice_index + 1);
        retval = 0;

        uint64_t page_size = PAGE_SIZE;
        uint64_t first_page = get_first_page(order, choice_index);
        uint64_t num_pages = get_num_pages(order);

        struct heap_data *heap = BUDDY_GET_HEAP(alloc);

        *region_start = heap->addr + first_page * page_size;

        if (region_end) {
            *region_end = *region_start + num_pages * page_size;
        }
    }

    bspinlock_unlock(&alloc->lock);

    return retval;
}

void release_upward(struct buddy_allocator *alloc, uint64_t bottom_order, uint64_t block_index) {
    uint64_t me = block_index;
    uint64_t order = bottom_order;

    int done = 0;

    while (!done && order < alloc->num_orders) {
        uint64_t buddy = get_buddy(me);

        struct buddy_order *buddy_order = alloc->orders + order;
        struct buddy_data *data_first = BUDDY_ORDER_GET_DATA(buddy_order);
        struct buddy_data *me_data, *buddy_data;

        me_data = data_first + me;
        buddy_data = data_first + buddy;

        me_data->allocated = 0;

        if (buddy_data->allocated) {
            done = 1;
        }

        order++;
        me >>= 1;
    }
}

void release_block(struct buddy_allocator *alloc, uintptr_t region_start) {
    uint32_t page_size = PAGE_SIZE;

    struct heap_data *heap = BUDDY_GET_HEAP(alloc);

    if (region_start < heap->addr) {
        KFATAL("Page 0x%lx-0x%lx does not lie within heap the specified heap\n", region_start,
               region_start + page_size);
    }

    uintptr_t heap_offset = region_start - heap->addr;
    uint64_t first_page_index = heap_offset / page_size;

    if (first_page_index >= heap->pages) {
        KFATAL("Page 0x%lx-0x%lx does not lie within heap the specified heap\n", region_start,
               region_start + page_size);
    }

    struct page *page = HEAP_FIRST_PAGE(heap) + first_page_index;

    bspinlock_lock(&alloc->lock);

    if (!page->allocated) {
        KFATAL("Attempt to free non-allocated page 0x%lx-0x%lx\n", region_start,
               region_start + page_size);
    }

    uint64_t order = page->allocated - 1;

    struct buddy_order *buddy_order = alloc->orders + order;

    uint64_t block_index = get_block_index(order, first_page_index);

    trickle_down_range(alloc, order, block_index, block_index + 1, 0);
    release_upward(alloc, order, block_index);

    bspinlock_unlock(&alloc->lock);
}

uint64_t compute_order(uint64_t pages) {
    // Round 'pages' up to the next power of two.
    uint64_t trailing_ones = pages;
    trailing_ones--;

    trailing_ones |= trailing_ones >> 1;
    trailing_ones |= trailing_ones >> 2;
    trailing_ones |= trailing_ones >> 4;
    trailing_ones |= trailing_ones >> 8;
    trailing_ones |= trailing_ones >> 16;
    trailing_ones |= trailing_ones >> 32;

    // Now do a popcount on the trailing ones
    uint64_t mask = 0x5555555555555555;
    uint64_t num_ones = trailing_ones;
    num_ones = ((num_ones >> 1) & mask) + (num_ones & mask);
    mask = 0x3333333333333333;
    num_ones = ((num_ones >> 2) & mask) + (num_ones & mask);
    mask = 0x0f0f0f0f0f0f0f0f;
    num_ones = ((num_ones >> 4) & mask) + (num_ones & mask);
    mask = 0x00ff00ff00ff00ff;
    num_ones = ((num_ones >> 8) & mask) + (num_ones & mask);
    mask = 0x0000ffff0000ffff;
    num_ones = ((num_ones >> 16) & mask) + (num_ones & mask);
    mask = 0x00000000ffffffff;
    num_ones = ((num_ones >> 32) & mask) + (num_ones & mask);

    // number of bits after the highest set bit.
    uint64_t logarithm = num_ones;
    return logarithm;
}

int acquire_pages(struct buddy_allocator *alloc, uint64_t pages, uintptr_t *region_start, uintptr_t *region_end) {
    if (pages == 0) {
        return -1;
    }

    return acquire_block(alloc, compute_order(pages), region_start, region_end);
}

int acquire_bytes(struct buddy_allocator *alloc, uint64_t bytes, uintptr_t *region_start, uintptr_t *region_end) {
    if (bytes == 0) {
        return -1;
    }

    uint32_t page_size = PAGE_SIZE;

    uint64_t ceiled_pages = (bytes + page_size - 1) / page_size;
    return acquire_pages(alloc, ceiled_pages, region_start, region_end);
}
