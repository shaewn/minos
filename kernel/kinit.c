#include "die.h"
#include "endian.h"
#include "fdt.h"
#include "output.h"
#include "string.h"

uintptr_t dtb_addr;

static uint64_t from_be64(uint64_t data) {
    return ktest_endian() == KLITTLE_ENDIAN ? kswap_order64(data) : data;
}

static uint32_t from_be32(uint32_t data) {
    return ktest_endian() == KLITTLE_ENDIAN ? kswap_order32(data) : data;
}

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

        kputstr("region at address 0x");
        kputu(address, 16);
        kputstr(" of size ");
        kputu(size, 10);
        kputstr(" bytes\n");

        ent++;
    }
}

void putspacing(int nesting) {
    for (int i = 0; i < nesting; i++) {
        kputstr("  ");
    }
}

void print_structures(struct fdt_header *header) {
    uint32_t struct_offset = from_be32(header->off_dt_struct);
    uint32_t string_offset = from_be32(header->off_dt_strings);

    uint32_t *structs = (uint32_t *)((char *)header + struct_offset);
    char *strings = (char *)header + string_offset;

    uint32_t *wp = structs;

    while (from_be32(*wp) == FDT_NOP)
        ++wp;
    if (from_be32(*wp++) != FDT_BEGIN_NODE) {
        kputstr("Invalid device tree.\n");
        die();
    }

    kputstr("Printing device tree:\n");

    int nesting = 1;

    enum {
        UNIMPORTANT,
        READING_MEM_NODE
    };

    int state = UNIMPORTANT;
    int stateval;

    uint32_t root_naddrcells, root_nsizecells;

    while (nesting) {
        switch (from_be32(*wp++)) {
            case FDT_BEGIN_NODE: {
                putspacing(nesting);

                char *name = (char *)wp;
                size_t namebytes = string_len(name) + 1;
                size_t name_aligned_words = (namebytes + 3) >> 2;
                kputstr(name);
                kputch('\n');

                wp += name_aligned_words;

                ++nesting;

                if (string_begins(name, "memory")) {
                    kputstr("BEGINING READING_MEM_NODE\n");
                    state = READING_MEM_NODE;
                    stateval = nesting;
                }

                break;
            }
            case FDT_END_NODE: {
                if (state == READING_MEM_NODE && nesting == stateval) {
                    state = UNIMPORTANT;
                    kputstr("ENDING READING_MEM_NODE\n");
                }

                --nesting;

                break;
            }
            case FDT_PROP: {
                uint32_t len = from_be32(*wp++);
                uint32_t nameoff = from_be32(*wp++);

                char *value_start = (char *)wp;
                uint32_t aligned_len_words = (len + 3) >> 2;
                wp += aligned_len_words;

                putspacing(nesting);

                const char *propname = strings + nameoff;

                kputstr("Property: ");
                kputstr(propname);
                kputstr(" of length ");
                kputu(len, 10);
                kputch('\n');

                if (nesting == 1) {
                    if (string_compare(propname, "#address-cells") == 0) {
                        root_naddrcells = from_be32(*(uint32_t *)value_start);
                        kputstr("root_naddrcells is ");
                        kputu(root_naddrcells, 10);
                        kputch('\n');
                    } else if (string_compare(propname, "#size-cells") == 0) {
                        root_nsizecells = from_be32(*(uint32_t *)value_start);
                        kputstr("root_nsizecells is ");
                        kputu(root_nsizecells, 10);
                        kputch('\n');
                    }
                }

                if (state == READING_MEM_NODE) {
                    if (string_compare(propname, "reg") == 0) {
                        uint32_t *value_ptr = (uint32_t *)value_start;
                    }
                }

                break;
            }
        }
    }
}

void kinit(void) {
    kputstr("Hello, world!\n");

    klockout(1);
    kputu_nolock(dtb_addr, 16);
    kputch('\n');
    klockout(0);

    struct fdt_header *header = (struct fdt_header *)dtb_addr;

    if (header->magic == from_be32(FDT_MAGIC)) {
        kputstr("We found the header!\n");
    } else {
        return;
    }

    klockout(1);
    kputstr_nolock("The total size of the device tree is: ");
    kputu_nolock(header->totalsize, 10);
    kputstr_nolock(" bytes\n");
    klockout(0);

    kputstr("Testing swaporder (non-swapped first, then swapped):\n");

    uint64_t swaptest = 0xfeedbeef;
    kputu(swaptest, 16);
    kputstr("\n");

    swaptest = kswap_order64(swaptest);

    kputu(swaptest, 16);
    kputstr("\n");

    if (ktest_endian() == KLITTLE_ENDIAN) {
        kputstr("We are little endian\n");
    } else {
        kputstr("We are big endian\n");
    }

    print_reserved_regions(header);
    print_structures(header);
}
