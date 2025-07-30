#include "types.h"

uint32_t gethwpagesize(void) {
    uint64_t tcr_el1;
    asm volatile("mrs %0, tcr_el1" : "=r"(tcr_el1));

    // See A-profile architecture reference manual D24.2.182
    // We are accessing field TG0 ('translation granule')
    unsigned index = (tcr_el1 >> 14) & 3;

    uint32_t sizes[3] = {
        4096,
        65536,
        16384
    };

    return sizes[index];
}
