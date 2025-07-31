#include "dt_util.h"
#include "endian.h"

#define KFATAL(...) early_die()
extern void early_die(void);

uint32_t *single_or_double_word(uint32_t *ptr, uint32_t selector, uint64_t *dst) {
    switch (selector) {
        case 1:
            *dst = (uint64_t)from_be32(*ptr);
            return ptr + 1;
        case 2:
            *dst = (uint64_t)from_be32(*ptr) << 32 | from_be32(ptr[1]);
            return ptr + 2;
        default:
            KFATAL("Weird selector value: %u\n", selector);
    }
}

void read_reg(uint32_t naddr, uint32_t nsize, uint32_t *data, uint64_t *addr, uint64_t *size) {
    if (naddr > 2 || nsize > 2 || naddr == 0 || nsize == 0) {
        KFATAL("Weird number of address or size cells. %u and %u respectively.\n", naddr, nsize);
    }

    uint32_t *ptr = data;

    ptr = single_or_double_word(ptr, naddr, addr);
    ptr = single_or_double_word(ptr, nsize, size);
}
