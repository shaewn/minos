#include "task.h"
#include "kmalloc.h"
#include "pltfrm.h"

struct task *__pcpu_current_task PERCPU_UNINIT;

struct task *create_task(void) {
    struct task *task = kmalloc(sizeof(*task));
    task->preempt_counter = 0;
    task->runtime = 0;
    task->vruntime = get_min_vruntime();
    task->mm = NULL;
    __atomic_store_n(&task->pin_count, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&task->migrate_lock, 0, __ATOMIC_RELEASE);

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

void update_runtime_values(struct task *task) {
    time_t current = timer_get_phys();

    time_t delta = current - task->state_begin;

    prio_t prio = KMAX(KMIN(task->prio, 19), -20);
    time_t vdelta = (delta * prio_to_weight[20]) / prio_to_weight[prio + 20];

    task->runtime += delta;
    task->vruntime += vdelta;
}

void update_state(struct task *task, task_state_t new_state) {
    switch (task->state) {
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

void task_unpin(struct task *task) {
    __atomic_fetch_sub(&task->pin_count, 1, __ATOMIC_RELEASE);
    cpu_signal_all();
}

void begin_migration(struct task *task) {
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&task->migrate_lock, &expected, 1, false, __ATOMIC_ACQ_REL,
                                     __ATOMIC_RELAXED)) {
        return;
    }

    while (__atomic_load_n(&task->pin_count, __ATOMIC_ACQUIRE) != 0) {
        cpu_idle_wait();
    }
}

void end_migration(struct task *task) {
    __atomic_store_n(&task->migrate_lock, 0, __ATOMIC_RELEASE);
}
