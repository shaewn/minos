#ifndef AARCH64_SECONDARY_CONTEXT_H_
#define AARCH64_SECONDARY_CONTEXT_H_

#include "types.h"

struct sndry_ctx {
    /* virtual */
    void *percpu_base;
    void *stack;

    /* physical */
    void *ttbr0;
    void *ttbr1;

    /* register values */
    uint64_t tcr;
    uint64_t mair;
};

#endif
