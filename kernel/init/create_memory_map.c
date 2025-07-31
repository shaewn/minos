#include "./dt.h"
#include "./endian.h"
#include "./dt_util.h"
#include "types.h"
#include "macros.h"
#include "pltfrm.h"

#define kprint(...)
#define KFATAL(...) early_die()

extern struct dt_node *dt_root;
extern uintptr_t kernel_brk;

char *memory_map_start_phys, *memory_map_end_phys;

[[noreturn]] extern void early_die(void);

static int string_begins(const char *s, const char *pref);
static int ranges_overlap(uintptr_t ab, uintptr_t ae, uintptr_t bb, uintptr_t be);

void create_memory_map(void) {
    struct dt_node *root = dt_search(NULL, "/");
    if (!root) {
        early_die();
    }

    uint32_t page_size = PAGE_SIZE;

    // Make sure the break value is hardware page aligned,
    // so that once we're in virtual memory we can have the memory map mapped in.
    uintptr_t ptr = (uintptr_t)kernel_brk;
    ptr = (ptr + page_size - 1) & ~(uintptr_t)(page_size - 1);
    kernel_brk = ptr;

    struct dt_prop *address_cells, *size_cells;
    address_cells = dt_findprop(root, "#address-cells");
    size_cells = dt_findprop(root, "#size-cells");

    if (!address_cells || !size_cells) {
        early_die();
    }

    uint32_t naddr = from_be32(*(uint32_t *)address_cells->data);
    uint32_t nsize = from_be32(*(uint32_t *)size_cells->data);

    struct heap_data *heap_data_start = (struct heap_data *)kernel_brk,
                     *heap_data_end = heap_data_start;

    memory_map_start_phys = (char *)heap_data_start;

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

            uint64_t nr_pages = size / page_size;

            heap_data_end->pages = nr_pages;
            heap_data_end->addr = addr;
            ++heap_data_end;
        }
    }

    // 0-terminated.
    heap_data_end->addr = 0;
    heap_data_end->pages = 0;
    heap_data_end->pages_start = 0;
    heap_data_end->buddy_start = 0;
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
            current_page_item->allocated = 0;
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
        alloc->lock.flag = 0;
        alloc->num_orders = 0;
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

            struct buddy_data *data = BUDDY_ORDER_GET_DATA(buddy_order);

            for (uint64_t i = 0; i < blocks_per_order; i++) {
                data[i].allocated = 0;
            }

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
    memory_map_end_phys = data_ptr;
    kernel_brk = (uintptr_t)data_ptr;

    void reserve_active_kernel_memory(void);
    reserve_active_kernel_memory();
}

void reserve_active_kernel_memory(void) {
    uintptr_t kmem_start, kmem_end;
    extern char __kernel_start_phys;
    kmem_start = (uintptr_t)&__kernel_start_phys;
    kmem_end = (uintptr_t)kernel_brk;

    uint32_t page_size = PAGE_SIZE;

    if (kmem_start & (page_size - 1) || kmem_end & (page_size - 1)) {
        KFATAL("kmem_start or kmem_end is not page aligned (0x%lx and 0x%lx, respectively)\n",
               kmem_start, kmem_end);
    }

    kprint("Pre-allocation kernel memory footprint: 0x%lx-0x%lx\n", kmem_start, kmem_end);

    for (struct heap_data *heap = (struct heap_data *)memory_map_start_phys; heap->pages; heap++) {
        uintptr_t heap_start, heap_end;
        heap_start = heap->addr;
        heap_end = heap->addr + heap->pages * page_size;

        if (heap_start == 0) {
            // Reserve one page so that we can pretend NULL pointers are invalid even in physical
            // address space.

            struct buddy_allocator *a = HEAP_GET_BUDDY(heap);

            for (uint64_t order = 0; order < a->num_orders; order++) {
                struct buddy_order *o = a->orders + order;
                BUDDY_ORDER_GET_DATA(o)->allocated = 1;
            }

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

            uint64_t order = 0;
            uint64_t num_blocks = page_past_last - page_first;
            uint64_t first_block = page_first;

            for (uint64_t i = page_first; i < page_past_last; i++) {
                struct page *page = HEAP_FIRST_PAGE(heap) + i;
                page->allocated = 1;
            }
        }
    }
}


static int string_begins(const char *s, const char *pref) {
    while (*pref && *s == *pref) ++s, ++pref;
    return !*pref;
}

/* pre: a_end != a_start && b_end != b_start */
static int ranges_overlap(uintptr_t a_start, uintptr_t a_end, uintptr_t b_start, uintptr_t b_end) {
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
