#include "memory_map.h"
#include "bspinlock.h"
#include "buddy_util.h"
#include "die.h"
#include "dt.h"
#include "dt_util.h"
#include "endian.h"
#include "macros.h"
#include "memory.h"
#include "output.h"
#include "pltfrm.h"
#include "string.h"

extern char *kernel_brk;

/* physical address range for the memory map */
char *memory_map_start, *memory_map_end;

bspinlock_t mm_lock;

#define ALLOC(size)                                                                                \
    ({                                                                                             \
        char *p = kernel_brk;                                                                      \
        kernel_brk += size;                                                                        \
        (void *)p;                                                                                 \
    })

void create_memory_map(void) {
    struct dt_node *root = dt_search(NULL, "/");
    if (!root) {
        KFATAL("I'm stupid.\n");
    }

    uint32_t page_size = gethwpagesize();

    // Make sure the break value is hardware page aligned,
    // so that once we're in virtual memory we can have the memory map mapped in.
    uintptr_t ptr = (uintptr_t)kernel_brk;
    ptr = (ptr + page_size - 1) & ~(uintptr_t)(page_size - 1);

    struct dt_prop *address_cells, *size_cells;
    address_cells = dt_findprop(root, "#address-cells");
    size_cells = dt_findprop(root, "#size-cells");

    uint32_t naddr = from_be32(*(uint32_t *)address_cells->data);
    uint32_t nsize = from_be32(*(uint32_t *)size_cells->data);

    struct heap_data *heap_data_start = (struct heap_data *)kernel_brk,
                     *heap_data_end = heap_data_start;

    memory_map_start = (char *)heap_data_start;

    for (struct dt_node *child = root->first_child; child; child = child->next_sibling) {
        if (string_begins(child->name, "memory")) {
            kprint("Found memory node: %s\n", child->name);

            struct dt_prop *reg_prop = dt_findprop(child, "reg");
            if (!reg_prop) {
                KFATAL("Memory node %s has no 'reg' property\n", child->name);
            }

            uint64_t addr, size;
            read_reg(naddr, nsize, (uint32_t *)reg_prop->data, &addr, &size);

            kprint("RAM Physical memory region: 0x%lx-0x%lx\n", addr, addr + size);

            uint64_t nr_pages = size / gethwpagesize();

            heap_data_end->pages = nr_pages;
            heap_data_end->addr = addr;
            ++heap_data_end;
        }
    }

    // 0-terminated.
    clear_memory(heap_data_end, sizeof(*heap_data_end));
    heap_data_end++;

    struct page *first_page_array = (struct page *)heap_data_end;
    struct page *current_page_item = first_page_array;

    uintptr_t offset = (uintptr_t)first_page_array - (uintptr_t)heap_data_start;

    uint64_t heap_index = 0, page_index;
    struct heap_data *cur = heap_data_start;
    while (cur->pages) {
        page_index = 0;

        uintptr_t addr = cur->addr;

        cur->pages_start = (uintptr_t)current_page_item - (uintptr_t)cur;

        while (page_index < cur->pages) {
            clear_memory(current_page_item, sizeof(*current_page_item));
            current_page_item->addr = addr;
            current_page_item->heap_index = heap_index;
            current_page_item->page_index = page_index;

            ++page_index;
            addr += page_size;
            current_page_item++;
        }

        cur++;
        heap_index++;
    }

    cur = heap_data_start;

    char *data_ptr = (char *)current_page_item;

    while (cur->pages) {
        uint64_t blocks_per_order = cur->pages;

        struct buddy_allocator *alloc = (struct buddy_allocator *)data_ptr;
        clear_memory(alloc, sizeof(*alloc));
        data_ptr += sizeof(*alloc);

        cur->buddy_start = (uintptr_t)alloc - (uintptr_t)cur;

        alloc->heap_data_offset = (intptr_t)cur - (intptr_t)alloc;

        while (blocks_per_order) {
            data_ptr += sizeof(*alloc->orders);
            alloc->num_orders++;
            blocks_per_order >>= 1;
        }

        uint64_t order = 0;
        blocks_per_order = cur->pages;

        while (blocks_per_order) {
            struct buddy_order *buddy_order = alloc->orders + order;
            buddy_order->num_blocks = blocks_per_order;
            buddy_order->data_offset = (uintptr_t)data_ptr - (uintptr_t)buddy_order;

            size_t increment = blocks_per_order * sizeof(struct buddy_data);
            clear_memory(data_ptr, increment);
            data_ptr += increment;

            order++;
            blocks_per_order >>= 1;
        }

        // Align to eight bytes.
        uintptr_t dpint = (uintptr_t)data_ptr;
        dpint = (dpint + 7) & ~(uintptr_t)7;
        data_ptr = (char *)dpint;
        ++cur;
    }

    uintptr_t dpint = (uintptr_t)data_ptr;
    // align to page size.
    dpint = (dpint + page_size - 1) & ~(uintptr_t)(page_size - 1);

    data_ptr = (char *)dpint;
    memory_map_end = data_ptr;
    kernel_brk = data_ptr;

    void reserve_active_kernel_memory(void);
    reserve_active_kernel_memory();

    void dump_memory_map(uint64_t min_order, uint64_t max_order);
    dump_memory_map(6, 20);
}

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

/* pre: a_end != a_start && b_end != b_start */
int ranges_overlap(uintptr_t a_start, uintptr_t a_end, uintptr_t b_start, uintptr_t b_end) {
    return (a_start < b_end) && (b_start < a_end);
    /*
       For some reason this really tripped me up at first:
       initially, we can divide analysis into two cases: overlapping and non-overlapping.
       consider the overlapping case:

       if a is the lower region (i.e., lower base address), and b is the upper region,

       we know a_start <= b_start, a_start < a_end, b_start < b_end.
       hence, a_start < b_end.

       if a_end <= b_start, then these regions wouldn't be overlapping, so !(a_end <= b_start), or
       b_start < a_end. hence, a_start < b_end && b_start < a_end.

       so the function returns true.

       swapping the lower and upper regions is a symmetric case.

       consider the non-overlapping case.

       if a is the lower region, and b is the upper region,
       we know a_start <= b_start, a_start < a_end, b_start < b_end.

       so a_start < b_end.

       now, suppose b_start < a_end.
       the regions would then be overlapping, so it must be the case that !(b_start < a_end).

       so the function returns false.

       swapping the lower and upper regions is a symmetric case.

       Now, since overlapping input -> returns true and non-overlapping input -> returns false,
       we know the function works.

       */
}

void trickle_up_allocate_pages(struct buddy_allocator *alloc, uint64_t page_first,
                               uint64_t page_past_last);

void reserve_active_kernel_memory(void) {
    uintptr_t kmem_start, kmem_end;
    extern char __kernel_start;
    kmem_start = (uintptr_t)&__kernel_start;
    kmem_end = (uintptr_t)kernel_brk;

    uint32_t page_size = gethwpagesize();

    if (kmem_start & (page_size - 1) || kmem_end & (page_size - 1)) {
        KFATAL("kmem_start or kmem_end is not page aligned (0x%lx and 0x%lx, respectively)\n",
               kmem_start, kmem_end);
    }

    kprint("Pre-allocation kernel memory footprint: 0x%lx-0x%lx\n", kmem_start, kmem_end);

    for (struct heap_data *heap = (struct heap_data *)memory_map_start; heap->pages; heap++) {
        uintptr_t heap_start, heap_end;
        heap_start = heap->addr;
        heap_end = heap->addr + heap->pages * page_size;

        if (heap_start == 0) {
            // Reserve one page so that we can pretend NULL pointers are invalid even in physical
            // address space.

            trickle_up_allocate_pages(HEAP_GET_BUDDY(heap), 0, 1);
            kprint("Allocating page 0x%lx-0x%lx for NULL pointer protection.\n", heap_start, heap_start + page_size);
        } else if (ranges_overlap(kmem_start, kmem_end, heap_start, heap_end)) {
            // Go through all overlapping pages and mark them as allocated.
            uintptr_t overlap_start, overlap_end;
            overlap_start = KMAX(kmem_start, heap_start);
            overlap_end = KMIN(kmem_end, heap_end);

            uint64_t page_first, page_past_last;

            page_first = (overlap_start - heap_start) / page_size;
            page_past_last = (overlap_end - heap_start) / page_size;

            struct buddy_allocator *alloc = HEAP_GET_BUDDY(heap);
            trickle_up_allocate_pages(alloc, page_first, page_past_last);
        }
    }
}

void dump_allocated_blocks(struct heap_data *heap, uint64_t order) {
    struct buddy_allocator *alloc = (struct buddy_allocator *)((char *)heap + heap->buddy_start);

    struct buddy_order *buddy_order = alloc->orders + order;
    struct buddy_data *data_start =
        (struct buddy_data *)((char *)buddy_order + buddy_order->data_offset);
    struct page *heap_first_page = HEAP_FIRST_PAGE(heap);

    uint32_t page_size = gethwpagesize();

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
    uint32_t page_size = gethwpagesize();

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

        uint64_t page_size = gethwpagesize();
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
    uint32_t page_size = gethwpagesize();

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
