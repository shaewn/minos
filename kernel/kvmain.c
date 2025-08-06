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

struct fdt_header *fdt_header;

uintptr_t map_percpu(uintptr_t vbrk) {
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

        r = vmap(vbrk + offset, reg, PROT_RSYS | PROT_WSYS, MEMORY_TYPE_NORMAL, 0);
        if (r < 0) {
            KFATAL("vmap error: %d\n", r);
        }

        copy_memory((void *)(vbrk + offset), (void *)(percpu_begin + offset), PAGE_SIZE);

        offset += PAGE_SIZE;
    }

    return vbrk + offset;
}

extern PERCPU_UNINIT uintptr_t __pcpu_kernel_vbrk;

// Kernel virtual entry point (higher half)
void kvmain(uintptr_t vbrk) {
    init_print();

    reserve_active_kernel_memory();

    void *percpu_copy = (void *)vbrk;

    vbrk = map_percpu(vbrk);

    kvmalloc_init();

    void *vmbuffer1 = kvmalloc(1, 0);
    void *vmbuffer2 = kvmalloc(1, 0);

    kprint("vmbuffer1 is 0x%lx\nvmbuffer2 is 0x%lx\n", vmbuffer1, vmbuffer2);

    kprint("using vmbuffer1 should result in fault:\n");
    *(int *)vmbuffer1 = 10;

    asm volatile("svc 0");
}
