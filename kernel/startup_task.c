#include "startup_task.h"
#include "arch/aarch64/rdt.h"
#include "output.h"
#include "task.h"

PERCPU_UNINIT uintptr_t __pcpu_cpu_stacks[MAX_CPUS];

void startup_task(void) {
    print_rdt();

    for (int i = 0; i < 3; i++) {
        kprint("Running...\nRuntime: %016lx cycles\nVRuntime: %016lx cycles\n", current_task->runtime, current_task->vruntime);
        sched_yield();
    }

    struct task *task = clone_current();

    if (task) {
        kprint("Hello, I'm the parent.\nMy task pointer is %lx\n", current_task);
    } else {
        kprint("Hello, I'm the child.\nMy task pointer is %lx\n", current_task);
    }

    while (1) {
        asm volatile("wfi");
    }
}

void create_startup_task(void) {
    struct task *the_task = create_task();

    // go around task_ref_dec, it may free the task.
    ctdn_latch_set(&the_task->ref_cnt, 0);

    the_task->cpu_regs.pc = (uint64_t)startup_task;
    the_task->user_stack_base = 0;
    the_task->kernel_stack_base = cpu_stacks[this_cpu()] - KSTACK_SIZE;

    cpu_affinity_clear_all(&the_task->affinity);
    cpu_affinity_set(&the_task->affinity, this_cpu());

    the_task->cpu_regs.sp = cpu_stacks[this_cpu()];
    // EL1 with SP_EL1
    the_task->cpu_regs.pstate = 0x0000000000000005;

    update_state(the_task, TASK_STATE_NEW_BORN);
    sched_ready_task_local(the_task);
}
