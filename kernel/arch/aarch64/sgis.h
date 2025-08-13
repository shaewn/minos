#ifndef AARCH64_SGIS_H_
#define AARCH64_SGIS_H_

#include "types.h"

#define SCHED_SGI 7

#define SCHED_MESSAGE_NONE 0
#define SCHED_MESSAGE_READY_TASK 1
#define SCHED_MESSAGE_SUSPEND_TASK 2
#define SCHED_MESSAGE_BLOCK_TASK 3

struct sched_sgi_payload {
    int message_type;

    struct task *task;
    struct list_head *wait_head;
    volatile spinlock_t *wait_head_lock;
};

void setup_sgis(void);

#endif
