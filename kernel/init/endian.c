#include "endian.h"

#pragma section(".init.text")

int ktest_endian() {
    return KLITTLE_ENDIAN;
}

uint64_t kswap_order64(uint64_t in) {
    uint64_t result = in;
    result = (result & 0xffffffff) << 32 | (result >> 32) & 0xffffffff;
    result = (result & 0x0000ffff0000ffff) << 16 | (result >> 16) & 0x0000ffff0000ffff;
    result = (result & 0x00ff00ff00ff00ff) << 8 | (result >> 8) & 0x00ff00ff00ff00ff;

    return result;
}

uint32_t kswap_order32(uint64_t in) {
    uint64_t result = in;
    result = (result & 0x0000ffff) << 16 | (result >> 16) & 0x0000ffff;
    result = (result & 0x00ff00ff) << 8 | (result >> 8) & 0x00ff00ff;

    return result;
}

uint64_t from_be64(uint64_t data) {
    return ktest_endian() == KLITTLE_ENDIAN ? kswap_order64(data) : data;
}

uint32_t from_be32(uint32_t data) {
    return ktest_endian() == KLITTLE_ENDIAN ? kswap_order32(data) : data;
}
