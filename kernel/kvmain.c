#include "types.h"
#include "output.h"
#include "memory_map.h"

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
}
