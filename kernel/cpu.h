#ifndef _CPU_H_
#define _CPU_H_

#include "types.h"

#define PERCPU_UNINIT __attribute__((section(".percpu.bss")))
#define PERCPU_INIT __attribute__((section(".percpu.data")))

cpu_t cpu_current(void);

#endif
