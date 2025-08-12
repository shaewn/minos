#ifndef KERNEL_STARTUP_TASK_H_
#define KERNEL_STARTUP_TASK_H_

#include "cpu.h"
#include "types.h"
#include "config.h"

#define cpu_stacks GET_PERCPU(__pcpu_cpu_stacks)
extern PERCPU_UNINIT uintptr_t __pcpu_cpu_stacks[MAX_CPUS];

void create_startup_task(void);

#endif
