#include "cpu_id.h"
#include "config.h"
#include "macros.h"

static uint64_t mpidr_table[MAX_CPUS];
static uint32_t current_index;

// stores aff3, aff2, aff1 in the least significant 24 bits of the entry.
// terminated by a -1 entry.
static int32_t routes_table[MAX_CPUS] = {-1};

cpu_t assign_cpu_id(uint64_t mpidr) {
    mpidr_table[current_index] = mpidr;

    int32_t route = get_affinities(mpidr) >> 8;
    uint32_t index = 0;
    while (routes_table[index] != route && routes_table[index] != -1) index++;

    if (routes_table[index] != route) {
        routes_table[index] = route;
        routes_table[index + 1] = -1;
    }

    return current_index++;
}

cpu_t get_cpu_id(uint64_t mpidr) {
    uint32_t affinities = get_affinities(mpidr);
    for (uint32_t i = 0; i < current_index; i++) {
        if (get_affinities(mpidr_table[i]) == affinities)
            return i;
    }

    return CPU_INVALID;
}

uint32_t cpu_count(void) { return current_index; }

cpu_t this_cpu(void) {
    uint64_t mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));

    return get_cpu_id(mpidr);
}

uint64_t get_mpidr(cpu_t cpu) {
    if (cpu >= current_index)
        cpu = 0;
    return (mpidr_table[cpu] & (0xffull << 32 | 0xffffffull));
}

uint32_t get_affinities(uint64_t mpidr) {
    uint32_t aff = 0;
    aff |= EXTRACT(mpidr, 39, 32) << 24;
    aff |= EXTRACT(mpidr, 23, 16) << 16;
    aff |= EXTRACT(mpidr, 15, 8) << 8;
    aff |= EXTRACT(mpidr, 7, 0) << 0;
    return aff;
}
