#include "kvmalloc.h"
#include "pltfrm.h"
#include "cpu.h"
#include "output.h"

PERCPU_UNINIT uintptr_t __pcpu_kernel_vbrk;

#define kernel_vbrk GET_PERCPU(__pcpu_kernel_vbrk)

void *kvmalloc(uint64_t pages, int flags) {
    if (!(flags & KVMALLOC_PERMANENT)) {
        return NULL; // Unimplemented. Probably just a simple linear-explicit-free-list for kernel space will do.
    }

    kprint("kernel_vbrk before: 0x%lx\n", kernel_vbrk);

    void *ptr = (void *)kernel_vbrk;
    kernel_vbrk += pages * PAGE_SIZE;

    kprint("kernel_vbrk after: 0x%lx\n", kernel_vbrk);

    return ptr;
}
