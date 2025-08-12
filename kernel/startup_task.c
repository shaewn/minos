#include "startup_task.h"
#include "arch/aarch64/rdt.h"
#include "output.h"
#include "task.h"

PERCPU_UNINIT uintptr_t __pcpu_cpu_stacks[MAX_CPUS];

void startup_task(void) {
    print_rdt();

    while (1) {
        kprint("Running...\nRuntime: %016lx cycles\nVRuntime: %016lx cycles\n", current_task->runtime, current_task->vruntime);
        asm volatile("wfi");
    }
}

void create_startup_task(void) {
    struct task *the_task = create_task();

    the_task->cpu_regs.pc = (uint64_t)startup_task;
    the_task->user_stack = 0;
    the_task->kernel_stack = cpu_stacks[this_cpu()];

    cpu_affinity_clear_all(&the_task->affinity);
    cpu_affinity_set(&the_task->affinity, this_cpu());

    the_task->cpu_regs.sp = the_task->kernel_stack;
    // EL1 with SP_EL1
    the_task->cpu_regs.pstate = 0x0000000000000005;

    update_state(the_task, TASK_STATE_NEW_BORN);
    sched_ready_task(the_task);
}
