#include "memory_map.h"
#include "bspinlock.h"
#include "die.h"
#include "dt.h"
#include "dt_util.h"
#include "endian.h"
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

/*
   Structure of the memory map:

   - USEFUL STRUCTURES -
    */
struct heap_data {
    uint64_t addr, pages;

    /* offset from the START OF THIS STRUCTURE to the START of the page array for this heap */
    uint64_t pages_start;

    /* offset from the START OF THIS STRUCTURE to the START of the buddy allocator data for this heap */
    uint64_t buddy_start;
};

struct buddy_data {
    uint8_t allocated;
};

struct buddy_allocator {
    intptr_t heap_data_start;
    bspinlock_t lock;
    uint64_t num_orders;
    struct buddy_order {
        uint64_t size;
        uintptr_t offset;
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
   the heap stores the offset (in bytes) from the beginning of the heap_data structure to the beginning of its page array

   next is an array of pairs of struct buddy_allocator and struct buddy_data.
   the heap_data structure stores the offset (in bytes) from the beginning of the heap_data structure to the beginning of its buddy allocator
   the struct buddy_allocator contains a NULL terminated array of offsets from the beginning of the struct buddy_allocator to the first struct buddy_data in each order.

   next is a contiguous region of struct buddy_data's
   there are no terminators between buddy_data's for each order.

   WARNING: Make sure once you actually do the buddy allocation stuff, that you mark ALL OF THE MEMORY THAT WE'VE USED IN THE KERNEL SO FAR as allocated.

   */

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

        alloc->heap_data_start = (intptr_t) heap_data_start - (intptr_t) alloc;

        while (blocks_per_order) {
            data_ptr += sizeof(*alloc->orders);
            alloc->num_orders++;
            blocks_per_order >>= 1;
        }

        uint64_t order = 0;
        blocks_per_order = cur->pages;

        while (blocks_per_order) {
            alloc->orders[order].size = blocks_per_order;
            alloc->orders[order].offset = (uintptr_t) data_ptr - (uintptr_t) alloc;

            size_t increment = blocks_per_order * sizeof(struct buddy_data);
            clear_memory(data_ptr, increment);
            data_ptr += increment;

            order++;
            blocks_per_order >>= 1;
        }

        // Align to eight bytes.
        uintptr_t dpint = (uintptr_t) data_ptr;
        dpint = (dpint + 7) & ~(uintptr_t)7;
        data_ptr = (char *)dpint;
        ++cur;
    }

    uintptr_t dpint = (uintptr_t) data_ptr;
    // align to page size.
    dpint = (dpint + page_size - 1) & ~(uintptr_t)(page_size - 1);

    data_ptr = (char *)dpint;
    memory_map_end = data_ptr;
    kernel_brk = data_ptr;

    void dump_memory_map(void);
    dump_memory_map();
}

void dump_memory_map(void) {
    for (struct heap_data *heap = (struct heap_data *)memory_map_start; heap->pages; heap++) {
        kprint("Heap at 0x%lx has %lu pages.\n", heap->addr, heap->pages);

        kprint("First ten pages:\n");

        struct page *first_page = (struct page *)((char *)heap + heap->pages_start);

        for (uint64_t i = 0; i < heap->pages && i < 10; i++) {
            struct page *page = first_page + i;
            kprint("Page at 0x%lx is page %lu in heap %lu %s\n", page->addr, page->page_index, page->heap_index, page->allocated ? "allocated" : "free");
        }

        struct buddy_allocator *allocator = (struct buddy_allocator *) ((char *)heap + heap->buddy_start);
        for (uint64_t i = 0; i < allocator->num_orders; i++) {
            struct buddy_order *order = allocator->orders + i;
            kprint("Order %lu has %lu buddy blocks and starts at 0x%lx\n", order->size, (uintptr_t) allocator + order->offset);
        }
    }
}
