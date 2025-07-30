#ifndef KERNEL_DT_UTIL_H_
#define KERNEL_DT_UTIL_H_

#include "types.h"

uint32_t *single_or_double_word(uint32_t *ptr, uint32_t selector, uint64_t *dst);
void read_reg(uint32_t naddr, uint32_t nsize, uint32_t *data, uint64_t *addr, uint64_t *size);

#endif
