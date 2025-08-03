#include "types.h"
#include "./pltfrm.h"
#include "die.h"

#if PAGE_SIZE != 4096
#error Unimplemented
#endif

uint64_t *access_table(const uint64_t *indices, int level) {
    // NOTE: All of this is very specific to 4KiB pages.
    uint64_t addr = 0xffff000000000000;
    uint64_t offset = 39;

    if (level < 0 || level > 4) {
        KFATAL("Weird 'level' value: %d\n", level);
    }

    int repeats = 4 - level;

    for (int i = 0; i < repeats; i++) {
        addr |= (uint64_t)RECURSIVE_INDEX << offset;
        offset -= 9;
    }

    for (int i = 0; i < level; i++) {
        uint64_t index = indices[i] & 0x1ff;
        addr |= index << offset;
        offset -= 9;
    }

    return (uint64_t *)addr;
}
