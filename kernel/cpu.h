#ifndef _CPU_H_
#define _CPU_H_

#include "config.h"
#include "macros.h"
#include "types.h"

#define PERCPU_UNINIT __attribute__((section(".percpu.bss")))
#define PERCPU_INIT __attribute__((section(".percpu.data")))

cpu_t this_cpu(void);

void cpu_idle_wait(void);
void cpu_signal_all(void);

typedef uint64_t cpu_affinity_t[BITS_TO_U64S(MAX_CPUS)];

void cpu_affinity_clear_all(cpu_affinity_t *aff);
void cpu_affinity_set_all(cpu_affinity_t *aff);
void cpu_affinity_set(cpu_affinity_t *aff, cpu_t cpu);
void cpu_affinity_clear(cpu_affinity_t *aff, cpu_t cpu);

#endif
