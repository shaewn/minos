#include "die.h"
#include "endian.h"
#include "fdt.h"
#include "output.h"
#include "string.h"

uintptr_t dtb_addr;
extern char stack_top;

// TODO: Load the dtb into memory after stack_top.
#include "dtb.h"

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

struct dtb_search_info {
    struct fdt_header *header;
};

uint32_t *skip_begin(uint32_t *wp) {
    size_t name_bytes = string_len((char *)&wp[1]) + 1;
    size_t word_aligned_len = (name_bytes + 3) >> 2;
    return wp + word_aligned_len + 1;
}

char *nodename(uint32_t *begin) {
    return (char *)&begin[1];
}

uint32_t *skip_prop(uint32_t *wp) {
    uint32_t len = from_be32(wp[1]);
    uint32_t word_aligned_len = (len + 3) >> 2;

    return wp + word_aligned_len + 3;
}

char *propname(struct fdt_header *header, uint32_t *prop) {
    char *strings = (char *)header + from_be32(header->off_dt_strings);
    return strings + from_be32(prop[2]);
}

char *propdata(uint32_t *prop) {
    return (char *)&prop[3];
}

uint32_t *to_matching_end(uint32_t *wp) {
    int nesting = 0;

    do {
        switch (from_be32(*wp)) {
            case FDT_BEGIN_NODE:
                ++nesting;
                wp = skip_begin(wp);
                break;
            case FDT_END:
                nesting = 0;
                break;
            case FDT_END_NODE:
                --nesting;
                break;
            case FDT_PROP: {
                wp = skip_prop(wp);
                break;
            }
            case FDT_NOP:
                break;
        }
    } while (nesting);

    return wp;
}

uint32_t *skip_nops(uint32_t *wp) {
    while (from_be32(*wp) == FDT_NOP)
        ++wp;
    return wp;
}

/* wp points within the guy we're searching */
uint32_t *find_child_pref(uint32_t *wp, const char *pref) {
    int done = 0;

    while (!done) {
        switch (from_be32(*wp)) {
            case FDT_BEGIN_NODE: {
                char *s = (char *)&wp[1];
                if (string_begins(s, pref)) {
                    return wp;
                }

                wp = to_matching_end(wp) + 1;
                break;
            }

            case FDT_PROP: {
                wp = skip_prop(wp);
                break;
            }

            case FDT_NOP: {
                ++wp;
                break;
            }

            case FDT_END_NODE: {
                done = 1;
                break;
            }
        }
    }

    return NULL;
}

uint32_t *find_prop(struct fdt_header *header, uint32_t *wp, const char *name) {
    int done = 0;

    while (!done) {
        switch (from_be32(*wp)) {
            case FDT_BEGIN_NODE: {
                wp = to_matching_end(wp) + 1;
                break;
            }

            case FDT_END_NODE: {
                done = 1;
                break;
            }

            case FDT_NOP:
                ++wp;
                break;

            case FDT_PROP: {
                if (string_compare(name, propname(header, wp)) == 0) {
                    return wp;
                }

                wp = skip_prop(wp);
                break;
            }
        }
    }

    return NULL;
}

uint64_t manywords(uint32_t *ptr, uint32_t **new_ptr, uint32_t count) {
    uint32_t *scratch;
    if (!new_ptr) {
        new_ptr = &scratch;
    }

    switch (count) {
        case 1: {
            *new_ptr = ptr + 1;
            return from_be32(*ptr);
        }

        case 2: {
            *new_ptr = ptr + 2;
            return (uint64_t)from_be32(*ptr) << 32 | from_be32(ptr[1]);
        }
    }

    die();

    return -1;
}

void print_hexrange(uint64_t base, uint64_t size) {
    kputstr("0x");
    kputu(base, 16);
    kputstr("-0x");
    kputu(base+size, 16);
}

void identify_memory_mappings(struct fdt_header *header) {
    uint32_t struct_offset = from_be32(header->off_dt_struct);
    uint32_t string_offset = from_be32(header->off_dt_strings);

    uint32_t *structs = (uint32_t *)((char *)header + struct_offset);
    char *strings = (char *)header + string_offset;

    uint32_t *wp = skip_nops(structs);

    uint32_t *mem, *ptr = skip_begin(wp);

    uint32_t *pnwords_addr = find_prop(header, ptr, "#address-cells");
    uint32_t *pnwords_size = find_prop(header, ptr, "#size-cells");

    kputstr("addr and size words: ");
    uint32_t nwords_addr = from_be32(*(uint32_t *)propdata(pnwords_addr));
    uint32_t nwords_size = from_be32(*(uint32_t *)propdata(pnwords_size));
    kputu(nwords_addr, 10);
    kputstr(", ");
    kputu(nwords_size, 10);
    kputch('\n');

    if (nwords_addr > 2 || nwords_size > 2) {
        kputstr("FATAL: weird value for root #address-cells and/or #size-cells\n");
        die();
    }

    while ((mem = find_child_pref(ptr, "memory")) != NULL) {
        kputstr("Found memory node: ");
        kputstr(nodename(mem));
        kputch('\n');

        uint32_t *srchstrt = skip_begin(mem);
        uint32_t *reg = find_prop(header, srchstrt, "reg");
        uint32_t *reg_data = (uint32_t *)propdata(reg);

        uint64_t base = manywords(reg_data, &reg_data, nwords_addr);
        uint64_t size = manywords(reg_data, &reg_data, nwords_size);
        kputstr("Physical memory range: ");
        print_hexrange(base, size);
        kputch('\n');

        ptr = to_matching_end(mem) + 1;
    }

    kputstr("Memory loop done.\n");
}

void print_structures(struct fdt_header *header) {
    uint32_t struct_offset = from_be32(header->off_dt_struct);
    uint32_t string_offset = from_be32(header->off_dt_strings);

    uint32_t *structs = (uint32_t *)((char *)header + struct_offset);
    char *strings = (char *)header + string_offset;

    uint32_t *wp = structs;

    kputstr("Printing device tree:\n");

    int nesting = 0;
    do {
        switch (from_be32(*wp++)) {
            case FDT_END: {
                nesting = 0;
                break;
            }
            case FDT_BEGIN_NODE: {
                kputstr("BEGIN\n");
                putspacing(nesting);

                char *name = (char *)wp;
                size_t namebytes = string_len(name) + 1;
                size_t name_aligned_words = (namebytes + 3) >> 2;
                kputstr(name);
                kputch('\n');

                wp += name_aligned_words;

                ++nesting;

                break;
            }
            case FDT_END_NODE: {
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

                break;
            }
            case FDT_NOP: {
                break;
            }
        }
    } while (nesting);
}

// TODO: Put this in arch
uint64_t read_tcr(void) {
    uint64_t tcr;

    asm volatile("mrs %0, tcr_el1" : "=r" (tcr));
    return tcr;
}

int get_page_size(void) {
    int sizes[3] = {
        4096,
        65535,
        16384
    };

    return sizes[(read_tcr() >> 14) & 3];
}

void kinit(void) {
    kputstr("The devicetree blob begins at address 0x");
    kputu(dtb_addr, 16);
    kputch('\n');

    int page_size = get_page_size();
    kputstr("Hardware page size: ");
    kputu(page_size, 10);
    kputch('\n');

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
    // print_structures(header);
    identify_memory_mappings(header);
}
