#include "memory.h"
#include "macros.h"
#include "./tt.h"
#include "./pltfrm.h"

#include "memory_map.h"
#include "vmap.h"
#include "prot.h"

#if PAGE_SIZE != 4096
#error Unimplemented
#endif
#define NUM_LEVELS 4

uint64_t *access_table(const uint64_t *indices, int level);
void tlb_flush_addr(uint64_t addr);

int gethwprot(uint64_t prot) {
    prot &= 077;
    uint64_t prot_nx = prot & ~(uint64_t) (PROT_XUSR | PROT_XSYS);
    int flag;
    if (prot_nx == (PROT_RUSR | PROT_RSYS)) {
        flag = AP_RDONLY_ALL;
    } else if (prot_nx == PROT_RSYS) {
        flag = AP_RDONLY_PRIV;
    } else if (prot_nx == (PROT_RSYS | PROT_WSYS)) {
        flag = AP_RDWR_PRIV;
    } else if (prot_nx == (PROT_RSYS | PROT_WSYS | PROT_RUSR | PROT_WUSR)) {
        // Can't have user write privileges AND system execute privileges.
        if (prot & PROT_XSYS) {
            return -1;
        }

        flag = AP_RDWR_ALL_PXN;
    } else {
        return -1;
    }

    if (!(prot & PROT_XUSR)) {
        flag |= BLOCK_ATTR_UXN;
    }

    if (!(prot & PROT_XSYS)) {
        flag |= BLOCK_ATTR_PXN;
    }

    return flag;
}

void kretrieve_indices(uint64_t addr, uint64_t *indices) {
    indices[0] = EXTRACT(addr, 47, 39);
    indices[1] = EXTRACT(addr, 38, 30);
    indices[2] = EXTRACT(addr, 29, 21);
    indices[3] = EXTRACT(addr, 20, 12);
}

/*
uint64_t kbuild_address(uint64_t *indices) {
    uint64_t addr = KERNEL_VIRT_BEGIN;
    uint8_t offset = 39;
    for (int i = 0; i < NUM_LEVELS; i++) {
        uint64_t index = indices[i] & 0x1ff;
        addr |= index << offset;
        offset -= 9;
    }

    return addr;
}

// FIXME: VERY BROKEN.
uintptr_t first_addr_avail(uintptr_t start) {
    uint64_t indices[NUM_LEVELS];
    kretrieve_indices(start, indices);

    int visited[NUM_LEVELS];
    clear_memory(visited, sizeof(visited));

    int counter = 0;

    while (1) {
        uint64_t index = indices[counter];
        uint64_t *ptr = access_table(indices, counter);

        if (visited[counter]) {
            indices[counter] = (indices[counter] + 1) % (PAGE_SIZE / 8);
            if (indices[counter] == 0) {
                --counter;
            }
            visited[counter] = 0;
        }

        if (counter < 0 || !(ptr[index] & 1)) {
            for (int i = counter; i < NUM_LEVELS; i++) {
                indices[i] = 0;
            }

            break;
        }

        visited[counter] = 1;
        if (counter + 1 < NUM_LEVELS) {
            counter++;
        }
    }

    return kbuild_address(indices);
}
*/

/* returns a table descriptor */
static int create_new_table(uint64_t *descriptor) {
    uint64_t begin;
    if (global_acquire_pages(1, &begin, NULL) == -1) {
        return -1;
    }

    clear_memory((void *)begin, PAGE_SIZE);

    *descriptor = begin | TABLE_DESC | TTE_AF;

    return 0;
}

int vmap(uintptr_t va, uintptr_t pa, uint64_t prot, memory_type_t memory_type, int flags) {
    int hwprot = gethwprot(prot);

    if (hwprot == -1) {
        return VMAP_ERROR_INVALID_PROT;
    }

    uint64_t indices[NUM_LEVELS];
    kretrieve_indices(va, indices);

    const int nlevels = NUM_LEVELS;
    for (int level = 0; level < nlevels - 1; level++) {
        uint64_t *ptr = access_table(indices, level);
        uint64_t index = indices[level];

        if (!(ptr[index] & 1)) {
            uint64_t descriptor;
            if (create_new_table(&descriptor) == -1) {
                return VMAP_ERROR_TABLE_NOMEM;
            }

            ptr[index] |= descriptor;
        }
    }

    uint64_t *last = access_table(indices, nlevels - 1);
    uint64_t lasti = indices[nlevels - 1];

    uint64_t *slot = last + lasti;

    if (*slot & 1 && !(flags & VMAP_FLAG_REMAP)) {
        return VMAP_ERROR_ALREADY_MAPPED;
    }

    *slot = pa | PAGE_DESC | TTE_AF | hwprot;

    tlb_flush_addr(va);

    return 0;
}

int vumap(uintptr_t va) {
    uint64_t indices[NUM_LEVELS];
    kretrieve_indices(va, indices);

    // Validate the translation so we don't have to deal with an MMU fault.

    const int nlevels = NUM_LEVELS;

    uint64_t *ptr;

    for (int level = 0; level < nlevels; level++) {
        ptr = access_table(indices, level);

        uint64_t index = indices[level];

        if (!(ptr[index] & 1)) {
            return VUMAP_ERROR_NOT_MAPPED;
        }
    }

    uint64_t index = indices[nlevels - 1];
    ptr[index] = 0;

    tlb_flush_addr(va);

    // TODO: Map unmap higher levels if we were the only guy left?

    return 0;
}
