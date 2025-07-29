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

#endif
