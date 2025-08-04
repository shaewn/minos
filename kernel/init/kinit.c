#include "types.h"

#include "./dt.h"
#include "./endian.h"

uintptr_t kernel_brk_init;
uintptr_t kernel_vbrk_init;

[[noreturn]] extern void early_die(void);

extern void create_memory_map(void);

void kinit(struct fdt_header *header) {
    // TODO: Read reserved memory regions from the fdt

    extern char __init_heap_begin_phys;
    kernel_brk_init = (uintptr_t) &__init_heap_begin_phys;

    build_dt(header);
    create_memory_map();

    // Platform specific.
    void do_idmap(void);
    do_idmap();

    // Platform specific.
    void map_higher_half(void);
    map_higher_half();

    [[noreturn]] void virtual_enter(struct fdt_header *);
    virtual_enter(header);
}
