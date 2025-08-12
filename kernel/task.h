#ifndef KERNEL_TASK_H_
#define KERNEL_TASK_H_

#include "cpu.h"
#include "types.h"
#include "pltfrm.h"
#include "sched.h"
#include <time.h>

#define current_task GET_PERCPU(__pcpu_current_task)

enum task_state {
    TASK_STATE_NEW_BORN,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SUSPENDED,
    TASK_STATE_TERMINATED
};

typedef int32_t task_state_t, prio_t, id_t;

struct task {
    struct regs cpu_regs;
    // struct fp_regs fp_regs;
    task_state_t state;
    prio_t prio;
    id_t tid;

    // Not atomic. Only use from hosting cpu.
    uint32_t preempt_counter;

    uintptr_t kernel_stack;
    uintptr_t user_stack;

    cpu_affinity_t affinity;

    // atomic.
    uint32_t pin_count;
    uint32_t migrate_lock;

    struct mm_info *mm;

    struct sched_node sched_node;
    time_t runtime, vruntime;
    time_t state_begin;
};

extern struct task *__pcpu_current_task PERCPU_UNINIT;

// Only creates the task struct, not indirectly stored data.
struct task *create_task(void);
// Only frees the task struct.
void free_task(struct task *task);

void update_state(struct task *task, task_state_t new_state);

// For use by non-hosting cpus.
task_state_t get_state(struct task *task);

bool task_pin(struct task *task);
void task_unpin(struct task *task);

void begin_migration(struct task *task);
void end_migration(struct task *task);

#endif
