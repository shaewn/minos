#include "./dt.h"
#include "./endian.h"
#include "./dt_util.h"
#include "types.h"
#include "macros.h"
#include "pltfrm.h"

#define kprint(...)
#define KFATAL(...) early_die()

extern struct dt_node *dt_root_init;
extern uintptr_t kernel_brk_init;

char *memory_map_start_init, *memory_map_end_init;

[[noreturn]] extern void early_die(void);

static int string_begins(const char *s, const char *pref);

void create_memory_map(void) {
    struct dt_node *root = dt_search(NULL, "/");
    if (!root) {
        early_die();
    }

    uint32_t page_size = PAGE_SIZE;

    // Make sure the break value is hardware page aligned,
    // so that once we're in virtual memory we can have the memory map mapped in.
    uintptr_t ptr = (uintptr_t)kernel_brk_init;
    ptr = (ptr + page_size - 1) & ~(uintptr_t)(page_size - 1);
    kernel_brk_init = ptr;

    struct dt_prop *address_cells, *size_cells;
    address_cells = dt_findprop(root, "#address-cells");
    size_cells = dt_findprop(root, "#size-cells");

    if (!address_cells || !size_cells) {
        early_die();
    }

    uint32_t naddr = from_be32(*(uint32_t *)address_cells->data);
    uint32_t nsize = from_be32(*(uint32_t *)size_cells->data);

    struct heap_data *heap_data_start = (struct heap_data *)kernel_brk_init,
                     *heap_data_end = heap_data_start;

    memory_map_start_init = (char *)heap_data_start;

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
    memory_map_end_init = data_ptr;
    kernel_brk_init = (uintptr_t)data_ptr;
}

static int string_begins(const char *s, const char *pref) {
    while (*pref && *s == *pref) ++s, ++pref;
    return !*pref;
}
