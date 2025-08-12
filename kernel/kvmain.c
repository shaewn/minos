#include "cpu.h"
#include "die.h"
#include "macros.h"
#include "memory.h"
#include "memory_map.h"
#include "memory_type.h"
#include "output.h"
#include "pltfrm.h"
#include "prot.h"
#include "types.h"
#include "vmap.h"
#include "kvmalloc.h"

uint64_t kernel_start, kernel_end, kernel_brk;

struct fdt_header *fdt_header_phys;

uintptr_t map_percpu(uintptr_t va_start) {
    extern char __percpu_begin, __percpu_end;
    uintptr_t percpu_begin = (uintptr_t)&__percpu_begin;
    uintptr_t percpu_end = (uintptr_t)&__percpu_end;

    uintptr_t offset = 0;

    while (percpu_begin + offset < percpu_end) {
        int r;
        uintptr_t reg;
        r = global_acquire_pages(1, &reg, NULL);
        if (r == -1) {
            KFATAL("Failed to acquire necessary memory\n");
        }

        r = vmap(va_start + offset, reg, PROT_RSYS | PROT_WSYS, MEMORY_TYPE_NORMAL, 0);
        if (r < 0) {
            KFATAL("vmap error: %d\n", r);
        }

        offset += PAGE_SIZE;
    }

    copy_memory((void *)va_start, (void *)percpu_begin, percpu_end - percpu_begin);

    return va_start + offset;
}

// Kernel virtual entry point (higher half)
void kvmain(uintptr_t vbrk) {
    void platform_basic_init(void);
    platform_basic_init();

    set_percpu_start(0);

    reserve_active_kernel_memory();

    void *percpu_copy = (void *)vbrk;

    vbrk = map_percpu(vbrk);
    set_percpu_start(percpu_copy);

    kvmalloc_init();

    void platform_startup(void);
    platform_startup();
}
