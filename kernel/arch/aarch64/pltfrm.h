#ifndef KERNEL_AARCH64_PLATFORM_H_
#define KERNEL_AARCH64_PLATFORM_H_

#include "defines.h"
#include "types.h"

#define ADDRESS_PERCPU(var) ((typeof(var) *)((char *)&var + get_percpu_offset()))

uint64_t get_percpu_offset(void);

#endif
