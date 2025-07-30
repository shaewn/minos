#include "memory_map.h"
#include "output.h"
#include "pltfrm.h"
#include "buddy_util.h"
#include "die.h"
#include "string.h"

static struct heap_data *main_heap;
static struct buddy_allocator *alloc;

void *allocate(uint64_t order) {
    uintptr_t region;
    int result = acquire_block(alloc, order, &region, 0);
    if (result == -1) {
        KFATAL("Failed to acquire block of order %lu (%lu pages)\n", order, get_num_pages(order));
    }

    return (void *)region;
}

void free(void *ptr) {
    release_block(alloc, (uintptr_t) ptr);
}

void print_status(void) {
    struct page *page_first = HEAP_FIRST_PAGE(main_heap);

    kprint("\nBEGIN STATUS\n");

    for (uint64_t i = 0; i < main_heap->pages; i++) {
        extern char *kernel_brk;
        struct page *page = page_first + i;
        if (page->allocated && page->addr >= (uintptr_t) kernel_brk) {
            kprint("Page 0x%lx-0x%lx is allocated in an order %lu block.\n", page->addr, page->addr + gethwpagesize(), page->allocated - 1);
        }
    }

    kprint("END STATUS\n");
}

void test_memory(void) {
    extern char *memory_map_start;
    main_heap = (struct heap_data *)memory_map_start;

    alloc = HEAP_GET_BUDDY(main_heap);
    kprint("\n\n\n\nCommencing memory test.\n\n");

    int num_ints = 100;
    int num_chars = gethwpagesize() * get_num_pages(1);

    int (*ints)[num_ints] = allocate(0);

    print_status();

    char (*s)[num_chars] = allocate(1);

    print_status();

    for (int i = 0; i < num_ints; i++) {
        (*ints)[i] = i;
    }

    copy_string(*s, "Hello, world!\n");

    kprint("My string is %s\n", *s);

    free(s);

    print_status();

    for (int i = 0; i < num_ints; i++) {
        kprint("[%d] %d\n", i, (*ints)[i]);
    }

    free(ints);

    print_status();
}
