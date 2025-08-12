#include "cpu.h"

void cpu_affinity_clear_all(cpu_affinity_t *aff) {
    for (size_t i = 0; i < ARRAY_LEN(*aff); i++) {
        (*aff)[i] = 0;
    }
}

void cpu_affinity_set_all(cpu_affinity_t *aff) {
    for (size_t i = 0; i < ARRAY_LEN(*aff); i++) {
        (*aff)[i] = 0xffffffffffffffff;
    }
}

void cpu_affinity_set(cpu_affinity_t *aff, cpu_t cpu) {
    if (cpu >= MAX_CPUS) return;

    size_t index = cpu / 64;
    size_t offset = cpu % 64;

    *(*aff + index) |= (1ull << offset);
}

void cpu_affinity_clear(cpu_affinity_t *aff, cpu_t cpu) {
    if (cpu >= MAX_CPUS) return;

    size_t index = cpu / 64;
    size_t offset = cpu % 64;

    *(*aff + index) &= ~(1ull << offset);
}
