#ifndef KERNEL_MACROS_H_
#define KERNEL_MACROS_H_

#include "types.h"

#include <stddef.h>

#define KMIN(a, b) ((a) < (b) ? (a) : (b))
#define KMAX(a, b) ((a) > (b) ? (a) : (b))
#define ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))

#define ONES_IN_RANGE(h, l) (((1ULL << (h - l + 1)) - 1) << l)
#define EXTRACT(a, h, l) ((a & ONES_IN_RANGE(h, l)) >> l)
#define CLEAR(a, h, l) (a & ~ONES_IN_RANGE(h, l))
#define REPLACE(a, h, l, new_bits) (CLEAR(a, h, l) | (((typeof(a))new_bits) << l))

#define OFFSET_OF(type, member) offsetof(type, member)
#define CONTAINER_OF(ptr, type, member) ((type *)((char *)(ptr) - OFFSET_OF(type, member)))

#define SWAP_ORDER_32(val)                                                                         \
    ({                                                                                             \
        uint32_t data = val;                                                                       \
        data = (data & 0xffff) << 16 | ((data >> 16) & 0xffff);                                      \
        data = (data & 0x00ff00ff) << 8 | ((data >> 8) & 0x00ff00ff);                                \
    })

#define FROM_BE_32(val) SWAP_ORDER_32(val)

#define BITS_TO_U64S(bits) (((bits) + 63) / 64)

#endif
