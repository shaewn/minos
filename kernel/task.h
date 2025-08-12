#ifndef KERNEL_TASK_H_
#define KERNEL_TASK_H_

#include "cpu.h"
#include "ctdn_latch.h"
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

typedef int32_t task_state_t, prio_t, id_t, status_t;

// TODO: Extract some of this into a struct task_stub, which will have a pointer to the struct task (optionally NULL, in which case the task would be a zombie)
struct task {
    // WARNING: If you move the regs fields to different offsets, you will have to change assembly code.
    struct regs cpu_regs;
    // struct fp_regs fp_regs;

    task_state_t state;
    prio_t prio;
    id_t tid;

    // WARNING: Every time you share a struct task * (i.e., give a reference to someone outside of the current context),
    // you MUST increment the reference count. The receiver of the struct task * must later decrement the reference count.
    ctdn_latch_t ref_cnt;

    // Not atomic. Only use from hosting cpu.
    uint32_t preempt_counter;

    uintptr_t kernel_stack_base;
    uintptr_t user_stack_base;

    cpu_t cpu;
    cpu_affinity_t affinity;

    // TODO: These things can survive even after we destroy everything else from the task.
    // (like Linux's Zombie Processes)
    struct list_head wait_queue_node;
    struct list_head wait_list;
    spinlock_t wait_list_lock;
    status_t exit_status;

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
void guarantee_task_pin(struct task *task);
void task_unpin(struct task *task);

void begin_migration(struct task *task);
void end_migration(struct task *task);

// returns NULL in the child task.
struct task *clone_current(void);

void task_exit(status_t status);
status_t wait_for_task(struct task *task);

inline static struct task *task_ref_inc(struct task *task) {
    ctdn_latch_increment(&task->ref_cnt);
    return task;
}

inline static void task_ref_dec(struct task *task) {
    ctdn_latch_decrement(&task->ref_cnt);
}

#endif
