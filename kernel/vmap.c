#include "vmap.h"
#include "kvmalloc.h"
#include "pltfrm.h"
#include "prot.h"

int vmap_range(uintptr_t start_va, uintptr_t start_pa, size_t pages, uint64_t prot, memory_type_t memory_type, int flags) {
    for (size_t i = 0; i < pages; i++) {
        int r = vmap(start_va + i * PAGE_SIZE, start_pa + i * PAGE_SIZE, prot, memory_type, flags);
        if (r < 0) {
            return r;
        }
    }

    return 0;
}

int vumap_range(uintptr_t start_va, size_t pages) {
    for (size_t i = 0; i < pages; i++) {
        int r = vumap(start_va + i * PAGE_SIZE);
        if (r < 0) {
            return r;
        }
    }

    return 0;
}

void *ioremap(uintptr_t start_pa, size_t bytes) {
    size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    void *ptr = kvmalloc(pages, 0);
    if (!ptr) return NULL;

    if (vmap_range((uintptr_t) ptr, start_pa, pages, PROT_RSYS | PROT_WSYS, MEMORY_TYPE_DEVICE_STRICT, 0) < 0) return NULL;

    return ptr;
}
