#ifndef AARCH64_TYPES_H_
#define AARCH64_TYPES_H_

#include "../../types.h"
#include "macros.h"

struct __attribute__((aligned(16))) regs {
    uint64_t x[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

#define PSTATE_USES_SP_EL0(pstate) (!(pstate & 1))
#define PSTATE_USES_SP_EL1(pstate) (!PSTATE_USES_SP_EL0(pstate))

// e.g., tpidr_el0, etc.
struct __attribute__((aligned(16))) extra_regs {
};

// v0-v31
struct __attribute__((aligned(16))) fp_regs {
    uint8_t bytes[32 * 16];
};

#endif
