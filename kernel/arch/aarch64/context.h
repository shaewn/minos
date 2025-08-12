#ifndef AARCH64_CONTEXT_H_
#define AARCH64_CONTEXT_H_

#include "types.h"

struct task;

// interrupt handler utilities.
void ex_save_context_to(struct task *task);
void no_save_switch_to(struct task *new_task);
void save_and_switch_to(struct task *old_task, struct task *new_task);

// if active_stack_base and task_stack_base are both 0, it will use the currently active stack
void save_context_no_switch(struct task *save_to, uintptr_t active_stack_base, uintptr_t task_stack_base);

void swap_stack(uintptr_t current_stack_base, uintptr_t new_stack_base);

#endif
