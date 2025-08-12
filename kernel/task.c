#include "task.h"
#include "kmalloc.h"
#include "memory.h"
#include "pltfrm.h"
#include "spinlock.h"

struct task *__pcpu_current_task PERCPU_UNINIT;

struct task *create_task(void) {
    struct task *task = kmalloc(sizeof(*task));
    task->preempt_counter = 0;
    task->runtime = 0;
    task->vruntime = get_min_vruntime(); // This can be set to 0.

    ctdn_latch_set(&task->ref_cnt, 1);
    task->mm = NULL;
    __atomic_store_n(&task->pin_count, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&task->migrate_lock, 0, __ATOMIC_RELEASE);

    list_init(&task->wait_list);
    spin_lock_init(&task->wait_list_lock);

    return task;
}

void free_task(struct task *task) { kfree(task); }

// Indexed by nice + 20 (to shift from [-20..19] to [0..39])
static const uint32_t prio_to_weight[40] = {
    88761, 71755, 56483, 46273, 36291, 29154, 23254, 18705, 14949, 11916, // -20..-11
    9548,  7620,  6100,  4904,  3906,  3121,  2501,  1991,  1586,  1277,  // -10..-1
    1024,  820,   655,   526,   423,   335,   272,   215,   172,   137,   // 0..9
    110,   87,    70,    56,    45,    36,    29,    23,    18,    15     // 10..19
};

static void update_runtime_values(struct task *task) {
    time_t current = timer_get_phys();

    time_t delta = current - task->state_begin;

    prio_t prio = KMAX(KMIN(task->prio, 19), -20);
    time_t vdelta = (delta * prio_to_weight[20]) / prio_to_weight[prio + 20];

    task->runtime += delta;
    task->vruntime += vdelta;
}

void update_state(struct task *task, task_state_t new_state) {
    switch (get_state(task)) {
        case TASK_STATE_RUNNING: {
            update_runtime_values(task);
            break;
        }

        default:
            break;
    }

    task->state_begin = timer_get_phys();
    __atomic_store_n(&task->state, new_state, __ATOMIC_RELEASE);
}

task_state_t get_state(struct task *task) {
    return __atomic_load_n(&task->state, __ATOMIC_ACQUIRE);
}

bool task_pin(struct task *task) {
    if (__atomic_load_n(&task->migrate_lock, __ATOMIC_ACQUIRE) == 0) {
        __atomic_fetch_add(&task->pin_count, 1, __ATOMIC_RELEASE);
        return true;
    }

    return false;
}

void guarantee_task_pin(struct task *task) {
    while (!task_pin(task)) ;
}

void task_unpin(struct task *task) {
    __atomic_fetch_sub(&task->pin_count, 1, __ATOMIC_RELEASE);
    cpu_signal_all(&task->pin_count);
}

void begin_migration(struct task *task) {
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&task->migrate_lock, &expected, 1, false, __ATOMIC_ACQ_REL,
                                     __ATOMIC_RELAXED)) {
        return;
    }

    while (__atomic_load_n(&task->pin_count, __ATOMIC_ACQUIRE) != 0) {
        cpu_idle_wait(&task->pin_count);
    }
}

void end_migration(struct task *task) {
    __atomic_store_n(&task->migrate_lock, 0, __ATOMIC_RELEASE);
    cpu_signal_all(&task->pin_count);
}

static uintptr_t duplicate_kernel_stack(void) {
    void *ptr = kmalloc(KSTACK_SIZE);

    copy_memory(ptr, (void *)current_task->kernel_stack_base, KSTACK_SIZE);

    return (uintptr_t) ptr;
}

struct task *clone_current(void) {
    sched_preempt_disable();

    struct task *new_task = create_task();

    uintptr_t new_stack_base = duplicate_kernel_stack();
    uintptr_t old_stack_base = current_task->kernel_stack_base;

    new_task->kernel_stack_base = new_stack_base;

    copy_memory(&new_task->affinity, &current_task->affinity, sizeof(cpu_affinity_t));

    new_task->prio = current_task->prio;

    sched_ready_task_local(new_task);

    save_context_no_switch(new_task, old_stack_base, new_stack_base);
    // new_task will resume exactly at this moment.

    if (current_task == new_task) {
        // don't sched_preempt_enable here, our preempt_counter is 0 cause we're fresh.
        new_task = NULL; // Make it easy to distinguish between the parent and child task.
    } else {
        sched_preempt_enable();
    }

    return new_task;
}

void task_exit(status_t status) {

}

status_t wait_for_task(struct task *task) {
    while (get_state(task) != TASK_STATE_TERMINATED) {
        sched_block_task_local(current_task, &task->wait_list, &task->wait_list_lock);
    }

    // TODO: Free the task.
}
