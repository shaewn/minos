#include "die.h"
#include "macros.h"
#include "types.h"
#include "output.h"
#include "memory_map.h"
#include "vmalloc.h"
#include "prot.h"
#include "memory_type.h"

uint64_t kernel_start, kernel_end, kernel_brk;

struct fdt_header *fdt_header;

// Kernel virtual entry point (higher half)
void kvmain(void) {
    init_print();

    int x = 1;
    x += 10;
    x /= 4;

    kprint("Hello, world! We made it into the higher address space!\n%d\n", x);

    reserve_active_kernel_memory();
    dump_memory_map(0, 0);

    uintptr_t va = 0xffff800000000000;

    uint64_t indices[4];
    void retrieve_indices(uint64_t *indices, int levels);
    retrieve_indices(indices, ARRAY_LEN(indices));

    int e = vmap_page(va, 0x09000000, PROT_RSYS | PROT_WSYS, MEMORY_TYPE_DEVICE_STRICT);
    if (e < 0) {
        KFATAL("vmap_page error: %d\n", e);
    }

    volatile int *p = (int *)va;
    *p = 'a';
    *p = '\n';
}
