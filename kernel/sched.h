#ifndef KERNEL_SCHED_H_
#define KERNEL_SCHED_H_

#include "cpu.h"
#include "list.h"
#include "rbt.h"
#include "time.h"

struct sched_node {
    struct rb_node run_queue_node;
    struct list_head blocked_node;
    struct list_head suspended_node;
};

/* if being called at the end of a time slice, one should check_preemptable() before running the scheduler. */
struct task *sched_run(void);

void sched_ready_task(struct task *task);
void sched_suspend_task(struct task *task);
void sched_block_task(struct task *task);

void preempt_disable(void);
void preempt_enable(void);
bool can_preempt(void);

time_t get_min_vruntime(void);

#endif
