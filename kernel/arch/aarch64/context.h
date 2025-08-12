#ifndef AARCH64_CONTEXT_H_
#define AARCH64_CONTEXT_H_

struct task;

// interrupt handler utilities.
void ex_save_context_to(struct task *task);
void no_save_switch_to(struct task *new_task);
void save_state_and_switch_to(struct task *old_task, struct task *new_task);

#endif
