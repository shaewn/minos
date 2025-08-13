#include "startup_task.h"
#include "arch/aarch64/rdt.h"
#include "kconsole.h"
#include "task.h"

void startup_task(void) {
    print_rdt();

    task_exit(0);
}

void create_startup_task(void) {
    struct task *the_task = create_task();

    // go around task_ref_dec, it may free the task.
    ctdn_latch_set(&the_task->ref_cnt, 0);

    the_task->cpu_regs.pc = (uint64_t)startup_task;

    cpu_affinity_clear_all(&the_task->affinity);
    cpu_affinity_set(&the_task->affinity, this_cpu());

    the_task->cpu_regs.sp = the_task->kernel_stack_base + KSTACK_SIZE;
    // EL1 with SP_EL1
    the_task->cpu_regs.pstate = 0x0000000000000005;

    update_state(the_task, TASK_STATE_NEW_BORN);
    sched_ready_task_local(the_task);
}
