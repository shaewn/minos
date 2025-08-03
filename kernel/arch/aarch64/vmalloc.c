#include "types.h"
#include "macros.h"
#include "./tt.h"

#include "memory_map.h"
#include "vmalloc.h"
#include "prot.h"

uint64_t *access_table(const uint64_t *indices, int level);

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

    return -1;
}

void retrieve_indices(uint64_t addr, uint64_t *indices) {
    indices[0] = EXTRACT(addr, 47, 39);
    indices[1] = EXTRACT(addr, 38, 30);
    indices[2] = EXTRACT(addr, 29, 21);
    indices[3] = EXTRACT(addr, 20, 12);
}

/* returns a table descriptor */
uint64_t create_new_table(void) {
}

int vmap_page(void *vaddr, uint64_t backing_address, uint64_t prot) {
    uint64_t va = (uint64_t) vaddr;

    int hwprot = gethwprot(prot);

    if (hwprot == -1) {
        return VSA_ERROR_INVALID_PROT;
    }

    uint64_t indices[4];
    retrieve_indices(va, indices);

    const int nlevels = 4;
    for (int level = 0; level < nlevels - 1; level++) {
        uint64_t *ptr = access_table(indices, level);
        uint64_t index = indices[level];

        if (!(ptr[index] & 1)) {
            ptr[index] |= create_new_table();
        }
    }
}

void *vmalloc(void *hint, uint64_t pages) {
}
