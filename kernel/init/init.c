#include "types.h"

#include "./dt.h"
#include "./endian.h"

uintptr_t kernel_brk;

[[noreturn]] extern void early_die(void);

extern void create_memory_map(void);

void kinit(struct fdt_header *header) {
    // TODO: Read reserved memory regions from the fdt

    extern char __init_heap_begin_phys;
    kernel_brk = (uintptr_t) &__init_heap_begin_phys;

    build_dt(header);
    create_memory_map();

    void do_idmap(void);
    do_idmap();

    early_die();
}
