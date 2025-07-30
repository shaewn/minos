#include "die.h"
#include "endian.h"
#include "fdt.h"
#include "output.h"
#include "dt.h"
#include "memory_map.h"
#include "pltfrm.h"
#include "string.h"

char *dtb_addr;

extern char __init_heap_begin;

// Used for early allocations (before initialization of secondary cpus)
char *kernel_brk = &__init_heap_begin;

void print_reserved_regions(struct fdt_header *header) {
    uint32_t offset = from_be32(header->off_mem_rsvmap);
    char *rsvmap = (char *)header + offset;

    kputstr("The reserved memory maps begin at offset 0x");
    kputu(offset, 16);
    kputstr("\n\nPrinting reserved memory maps:\n");

    struct fdt_reserve_entry *ent = (struct fdt_reserve_entry *)rsvmap;
    while (ent->address != 0 || ent->size != 0) {
        uint64_t address, size;
        address = from_be64(ent->address);
        size = from_be64(ent->size);

        kprint("region at address 0x%lx of size %lu bytes.\n", address, size);

        ent++;
    }
}

void kinit(void) {
    // va_list used in kprint needs access to registers q0-q7
    init_print();
    kprint("The devicetree blob begins at address 0x%lx.\n", dtb_addr);

    kprint("Hardware page size: %u bytes.\n", gethwpagesize());

    struct fdt_header *header = (struct fdt_header *)dtb_addr;

    if (header->magic == from_be32(FDT_MAGIC)) {
        kputstr("We found the header!\n");
    } else {
        return;
    }

    kprint("The total size of the device tree is %u bytes.\n", from_be32(header->totalsize));

    kprint("We are %s endian.\n", ktest_endian() == KLITTLE_ENDIAN ? "little" : "big");

    print_reserved_regions(header);
    build_dt(header);

    struct dt_node *cpus_node = dt_search(NULL, "/cpus");
    if (!cpus_node) {
        KFATAL("Couldn't find /cpus in device tree\n");
    }

    kprint("Found cpus node: %s\n", cpus_node->name);
    create_memory_map();

    void test_memory(void);
    test_memory();
}
