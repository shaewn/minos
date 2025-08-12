#ifndef AARCH64_CPUID_H_
#define AARCH64_CPUID_H_

#include "cpu.h"

cpu_t assign_cpu_id(uint64_t mpidr);
cpu_t get_cpu_id(uint64_t mpidr);

uint32_t cpu_count(void);

cpu_t this_cpu(void);

void cache_cpu(void);

uint64_t get_mpidr(cpu_t cpu);
uint32_t get_affinities(uint64_t mpidr);

#endif
