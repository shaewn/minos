#ifndef _CPU_H_
#define _CPU_H_

#include "types.h"

#define PERCPU_UNINIT __attribute__((section(".percpu.bss")))
#define PERCPU_INIT __attribute__((section(".percpu.data")))

cpu_t this_cpu(void);

void cpu_idle_wait(void);
void cpu_signal_all(void);

#endif
