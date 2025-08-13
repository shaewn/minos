#ifndef KERNEL_SCHED_H_
#define KERNEL_SCHED_H_

#include "cpu.h"
#include "list.h"
#include "rbt.h"
#include "time.h"

struct task;

struct sched_node {
    struct rb_node run_queue_node;
    struct list_head blocked_node;
    struct list_head suspended_node;
};

void wait_queue_insert(struct list_head *wait_head, struct task *task);
void wait_queue_del(struct task *task);

// if this call is from an interrupt, ensure that can_preempt() is true before calling sched_run()
// this function does not add current_task into the run queue..
void sched_run(bool save_state);

void sched_ready_task_local(struct task *task);
void sched_suspend_task_local(struct task *task);

void sched_block_task_local_no_switch(struct task *task);
void sched_block_task_local_on_queue(struct task *task, struct list_head *wait_head, volatile spinlock_t * _Nullable lock);

// for tasks that might not be on the current cpu.
void sched_ready_task_remote(struct task *task);
void sched_suspend_task_remote(struct task *task);

// lock is initially unlocked
void sched_block_task_remote(struct task *task, struct list_head *wait_head, volatile spinlock_t * _Nullable lock);

// lock is initially unlocked.
void sched_unblock_one(struct list_head *wait_head, volatile spinlock_t *_Nullable lock);
void sched_unblock_all(struct list_head *wait_head, volatile spinlock_t *_Nullable lock);

void sched_yield(void);

void sched_preempt_disable(void);
void sched_preempt_enable(void);
bool can_preempt(void);

time_t get_min_vruntime(void);

#endif
