#ifndef KERNEL_ENDIAN_H_
#define KERNEL_ENDIAN_H_

#include "types.h"


enum {
    KLITTLE_ENDIAN,
    KBIG_ENDIAN
};

int ktest_endian();
uint64_t kswap_order64(uint64_t in);
uint32_t kswap_order32(uint64_t in);
uint64_t from_be64(uint64_t data);
uint32_t from_be32(uint32_t data);

#endif
