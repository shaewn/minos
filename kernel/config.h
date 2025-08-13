#ifndef KERNEL_CONFIG_H_
#define KERNEL_CONFIG_H_

#include "arch/aarch64/defines.h"

#define MAX_CPUS 32
#define MAX_ID 4194304
#define MAX_PROC 1048576

// 64 KiB
#define KSTACK_SIZE (2 * PAGE_SIZE)

#define HZ 10

#endif
