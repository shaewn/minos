#include "pltfrm.h"
#include "die.h"
#include "fdt.h"
#include "output.h"
#include "vmap.h"
#include "kvmalloc.h"
#include "prot.h"

extern struct fdt_header *fdt_header_phys;
struct fdt_header *fdt_header;

static uint32_t from_be32(uint32_t data) {
    data = (data & 0xffff) << 16 | (data >> 16) & 0xffff;
    data = (data & 0x00ff00ff) << 8 | (data >> 8) & 0x00ff00ff;
    return data;
}

void platform_startup(void) {
    struct fdt_header *header = kvmalloc(1, 0);

    int r = vmap((uintptr_t) header, (uintptr_t) fdt_header_phys, PROT_RSYS, MEMORY_TYPE_NON_CACHEABLE, 0);
    if (r < 0) {
        KFATAL("vmap failed: %d\n", r);
    }

    if (from_be32(header->magic) != FDT_MAGIC) {
        KFATAL("Failed reading header\n");
    }

    uint32_t total_size = from_be32(header->totalsize);
    kprint("Total size: %u\n", total_size);

    uint32_t total_pages = total_size / PAGE_SIZE;
    kprint("Total pages: %u\n", total_pages);

    vumap((uintptr_t) header);
    kvfree(header);

    // Now remap the full thing.

    fdt_header = kvmalloc(total_pages, KVMALLOC_PERMANENT);
}
