#include "task.h"
#include "die.h"
#include "kmalloc.h"
#include "memory.h"
#include "kconsole.h"
#include "pltfrm.h"
#include "spinlock.h"

struct task *__pcpu_current_task PERCPU_UNINIT;

struct task *create_task(void) {
    struct task *task = kmalloc(sizeof(*task));
    task->preempt_counter = 0;
    task->runtime = 0;
    task->vruntime = get_min_vruntime(); // This can be set to 0.
    task->user_stack_base = 0;
    task->kernel_stack_base = (uintptr_t) kmalloc(KSTACK_SIZE);

    ctdn_latch_set(&task->ref_cnt, 1);
    task->mm = NULL;
    __atomic_store_n(&task->pin_count, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&task->migrate_lock, 0, __ATOMIC_RELEASE);

    list_init(&task->wait_list);
    spin_lock_init(&task->wait_list_lock);

    __atomic_store_n(&task->state, TASK_STATE_NEW_BORN, __ATOMIC_RELEASE);

    __atomic_store_n(&task->exiting, false, __ATOMIC_RELEASE);

    return task;
}

void free_task(struct task *task) {
    kfree((void *)task->kernel_stack_base);
    kfree(task);
}

// Indexed by nice + 20 (to shift from [-20..19] to [0..39])
static const uint32_t prio_to_weight[40] = {
    88761, 71755, 56483, 46273, 36291, 29154, 23254, 18705, 14949, 11916, // -20..-11
    9548,  7620,  6100,  4904,  3906,  3121,  2501,  1991,  1586,  1277,  // -10..-1
    1024,  820,   655,   526,   423,   335,   272,   215,   172,   137,   // 0..9
    110,   87,    70,    56,    45,    36,    29,    23,    18,    15     // 10..19
};

static void update_runtime_values(volatile struct task *task) {
    time_t current = timer_get_phys();

    time_t delta = current - task->state_begin;

    prio_t prio = KMAX(KMIN(task->prio, 19), -20);
    time_t vdelta = (delta * prio_to_weight[20]) / prio_to_weight[prio + 20];

    task->runtime += delta;
    task->vruntime += vdelta;
}

// Non-reentrant
void update_state(volatile struct task *task, task_state_t new_state) {
    int val = irqs_masked();
    mask_irqs();
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
    restore_irq_mask(val);
}

task_state_t get_state(volatile struct task *task) {
    return __atomic_load_n(&task->state, __ATOMIC_ACQUIRE);
}

bool task_pin(volatile struct task *task) {
    if (__atomic_load_n(&task->migrate_lock, __ATOMIC_ACQUIRE) == 0) {
        __atomic_fetch_add(&task->pin_count, 1, __ATOMIC_RELAXED);
        return true;
    }

    return false;
}

void guarantee_task_pin(volatile struct task *task) {
    while (!task_pin(task)) ;
}

void task_unpin(volatile struct task *task) {
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

PERCPU_UNINIT uintptr_t __pcpu_cpu_stacks[MAX_CPUS];

void task_exit(status_t exit_status) {
    sched_preempt_disable();

    current_task->exit_status = exit_status;
    __atomic_store_n(&current_task->exiting, true, __ATOMIC_RELEASE);

    sched_preempt_enable();

    sched_unblock_all(&current_task->wait_list, &current_task->wait_list_lock);

    task_ref_dec(current_task);
    ctdn_latch_wait(&current_task->ref_cnt);

    sched_preempt_disable();

    update_state(current_task, TASK_STATE_TERMINATED);

    mask_irqs();
    copy_memory((void *) (cpu_stacks[this_cpu()] - KSTACK_SIZE), (void *)current_task->kernel_stack_base, KSTACK_SIZE);
    swap_stack(current_task->kernel_stack_base, (uintptr_t)cpu_stacks[this_cpu()] - KSTACK_SIZE);

    struct task *the_task = current_task;
    current_task = NULL;
    free_task(the_task);

    sched_run(false);

    UNREACHABLE("task_exit");
}

status_t wait_for_task(struct task *task) {
    status_t exit_status;
    bool done = false;
    while (!done) {
        volatile spinlock_t *lock = &task->wait_list_lock;

        sched_preempt_disable();

        int val = irqs_masked();
        mask_irqs();
        spin_lock_irq(lock);

        bool exiting = __atomic_load_n(&task->exiting, __ATOMIC_ACQUIRE);

        if (exiting) {
            done = true;
            exit_status = task->exit_status;
            spin_unlock_irq(lock);

            task_ref_dec(task);
        } else {
            wait_queue_insert(&task->wait_list, current_task);
            sched_block_task_local_no_switch(current_task);

            spin_unlock_irq(lock);
            sched_run(true);
        }

        restore_irq_mask(val);
        sched_preempt_enable();
    }

    return exit_status;
}
