#include "sched.h"
#include "kmalloc.h"
#include "spinlock.h"
#include "task.h"

#define run_queue_root GET_PERCPU(__pcpu_run_queue_root)
#define blocked_list GET_PERCPU(__pcpu_blocked_list)
#define suspended_list GET_PERCPU(__pcpu_suspended_list)
#define min_vruntime GET_PERCPU(__pcpu_min_vruntime)

static PERCPU_UNINIT struct rb_node *__pcpu_run_queue_root;
static PERCPU_INIT struct list_head __pcpu_blocked_list = LIST_HEAD_INIT(__pcpu_blocked_list),
                                    __pcpu_suspended_list = LIST_HEAD_INIT(__pcpu_suspended_list);
static PERCPU_UNINIT time_t __pcpu_min_vruntime;

void wait_queue_insert(struct list_head *wait_head, struct task *task) {
    task_ref_inc(task);
    list_add_tail(&task->wait_queue_node, wait_head);
}

void wait_queue_del(struct task *task) {
    task_ref_dec(task);
    list_del(&task->wait_queue_node);
}

static struct rb_node *get_leftmost_node(void) {
    struct rb_node *current = run_queue_root;

    if (!current) {
        return NULL;
    }

    while (current->left) {
        current = current->left;
    }

    return current;
}

static void update_min_vruntime(void) {
    struct rb_node *node = get_leftmost_node();
    if (!node)
        return;

    struct task *task = CONTAINER_OF(CONTAINER_OF(node, struct sched_node, run_queue_node),
                                     struct task, sched_node);
    min_vruntime = task->vruntime;
}

inline static bool vruntime_before(time_t a, time_t b) {
    _Static_assert(sizeof(a) == 8, "time_t is not 64 bits.");
    // this relies on the fact that the run_queue will keep vruntimes close together.
    // close means a = b + c for some 0 <= c <= UINT64_MAX / 2 OR b = a + c for some 0 <= c <=
    // UINT64_MAX / 2 cases to consider to understand: 0) if a == b, obviously a is not before b.

    // 1) when a < b, a - b is very large when a and b are close and small when a and b are not
    // close. proof: assume a < b.
    // -- denotes computer subtraction, - denotes mathematical subtraction.
    // suppose a and b are close. then b - a <= 2^63
    // a - b < 0, so a -- b = (a - b) mod 2^64 = 2^64 + (a - b) = 2^64 - (b - a) >= 2^63
    // return true.
    // suppose a and b are not close. then b - a > 2^63
    // a - b < 0, so a -- b = (a - b) mod 2^64 = 2^64 + (a - b) = 2^64 - (b - a) < 2^63
    // return false.
    // end proof.

    // 2) when a > b, a - b is small when a and b are close, and large when a and b are far.
    // this is the same thing as 1), just stated with a and b flipped.

    // 3) if a and b are not close, and a < b, then 2^64 - 1 and b are close, and 2^63 and b are
    // close. this proposition justifies the reasoning behind our return values in cases a-d. proof:
    // suppose a and b are not close.
    // then b > a + 2^63.
    // Hence, b >= 2^63
    // also, b <= 2^64 - 1, so b - 2^63 <= 2^63 - 1
    // so b = 2^63 + (b - 2^63), and b - 2^63 <= 2^63 - 1 <= 2^63, meaning b and 2^63 are close.
    // note: -b <= -2^63, -b - 1 <= -2^63 - 1, and 2^64 - b - 1 <= 2^63 - 1
    // 2^64 - 1 = b + (2^64 - 1 - b), and as previously stated, 2^64 - b - 1 <= 2^63 - 1 <= 2^63.
    // Hence, 2^64 - 1 and b are close.
    // end proof.
    // from here, if we observe that when a >= 2^63, b - a <= 2^63 - 1, since b <= 2^64 - 1, so in
    // this case a and b are close. so a < 2^63. now, suppose a < k <= 2^63. we want to show that (a
    // - k) mod 2^64 is close to b. x := (a - k) mod 2^64 = 2^64 + (a - k) = 2^64 - k + a. since k
    // <= 2^63, -k + a >= -2^63 + a. Hence x >= 2^63 + a. also, k > a, so -k + a < 0. Hence x <
    // 2^64, or x <= 2^64 - 1. if x == b, x and b are trivially close. if x < b, b = x + (b - x),
    // and 0 < b - x <= b - 2^63 - a <= 2^63 - 1 - a <= 2^63. Hence, b and x are close. if b < x,
    // then x = b + (x - b), and 0 < x - b <= x - 2^63 <= 2^63 - 1 <= 2^63. Hence, b and x are
    // close. NOTE: this means that if a < b and a is not close to b, for any reasonable previous
    // difference k that would cause a previous value x to wrap to a, x will be close to b.
    // furthermore, when we aren't dealing with wrapping, provided that we maintain a small enough
    // maximum vruntime difference, the values will always be close. in essence, a and b not being
    // close serves as an indicator of when we have wrapped. as long as we maintain a difference in
    // active vruntimes of no more than 2^63, we will correctly handle wrapping vruntimes. finally,
    // this justifies cases c-d listed below.

    // if you're confused what everything i just wrote was,
    // 1-2 prove that cases a-d return the values expected (listed in the parenthesis).
    // 3 proves that as long as we maintain small enough (at most 2^63) maximum vruntime
    // differences, a and b not being close serves as an indicator that wrapping has occurred,
    // associating the informal reasoning of ci and di with c and d.

    // a) a and b are close, and a < b (should return true).
    // b) a and b are close, and a > b (should return false).
    // c) a and b are not close, and a < b (should return false ... a must have wrapped around
    // because we're so far apart) ci) when we have wrapped, and a < b, a is the one that wrapped.
    // in this case, we should return false, because a is not actually before b, despite being
    // numerically less. d) a and b are not close, and a > b (should return true ... b must have
    // wrapped around because we're so far apart) di) when we have wrapped, and b < a, b is the one
    // that wrapped. in this case, we should return true, because b is not actually before b,
    // despite being numerically less.
    return (a != b) && ((a - b) >= (UINT64_MAX / 2));
}

static void run_queue_insert(struct task *task) {
    struct rb_node *rb = &task->sched_node.run_queue_node;

    struct rb_node *parent = NULL;
    struct rb_node **current = &run_queue_root;

    if (vruntime_before(task->vruntime, min_vruntime)) {
        task->vruntime = min_vruntime;
    }

    while (*current) {
        parent = *current;
        struct task *ctask = CONTAINER_OF(CONTAINER_OF(*current, struct sched_node, run_queue_node),
                                          struct task, sched_node);
        if (vruntime_before(task->vruntime, ctask->vruntime)) {
            current = &(*current)->left;
        } else if (task == ctask) {
            // can't insert same task more than once.
            return;
        } else {
            // On tie, prioritize guys who've been in the queue longer.
            current = &(*current)->right;
        }
    }

    rb_link_node(rb, parent, current);
    rb_insert_color(rb, &run_queue_root);

    update_min_vruntime();

    task_ref_inc(task);
    task->cpu = this_cpu();
}

static void run_queue_del(struct task *task) {
    rb_del(&task->sched_node.run_queue_node, &run_queue_root);
    update_min_vruntime();
    task_ref_dec(task);
}

static void block(struct task *task) {
    list_add_tail(&task->sched_node.blocked_node, &blocked_list);
    task_ref_inc(task);
    task->cpu = this_cpu();
}

static void unblock(struct task *task) {
    list_del(&task->sched_node.blocked_node);
    task_ref_dec(task);
}

static void suspend(struct task *task) {
    list_add_tail(&task->sched_node.suspended_node, &suspended_list);
    task_ref_inc(task);
    task->cpu = this_cpu();
}

static void unsuspend(struct task *task) {
    list_del(&task->sched_node.suspended_node);
    task_ref_dec(task);
}

void sched_run(bool save_state) {
    struct rb_node *node = get_leftmost_node();
    struct task *selected =
        node ? CONTAINER_OF(CONTAINER_OF(node, struct sched_node, run_queue_node), struct task,
                            sched_node)
             : NULL;

    if (selected != current_task) {
        if (selected)
            task_ref_inc(selected);
        if (current_task)
            task_ref_dec(current_task);
    }

    if (selected) {
        run_queue_del(selected);
    }

    if (save_state) {
        save_and_switch_to(current_task, selected);
    } else {
        no_save_switch_to(selected);
    }
}

static void remove_prior(struct task *task) {
    switch (get_state(task)) {
        case TASK_STATE_BLOCKED: {
            unblock(task);
            break;
        }

        case TASK_STATE_TERMINATED:
        case TASK_STATE_NEW_BORN:
        case TASK_STATE_RUNNING:
            // not coming from any other structure.
            break;

        case TASK_STATE_READY: {
            run_queue_del(task);
            break;
        }

        case TASK_STATE_SUSPENDED: {
            unsuspend(task);
            break;
        }
    }
}

void sched_ready_task_local(struct task *task) {
    sched_preempt_disable();
    int irqs = irqs_masked();
    mask_irqs();

    if (get_state(task) != TASK_STATE_READY) {
        run_queue_insert(task);
        remove_prior(task);
        update_state(task, TASK_STATE_READY);
    }

    restore_irq_mask(irqs);
    sched_preempt_enable();
}

void sched_suspend_task_local(struct task *task) {
    sched_preempt_disable();
    int irqs = irqs_masked();
    mask_irqs();

    if (get_state(task) != TASK_STATE_SUSPENDED) {
        suspend(task);
        remove_prior(task);
        update_state(task, TASK_STATE_SUSPENDED);
    }

    if (task == current_task) {
        sched_run(true);
    }

    restore_irq_mask(irqs);
    sched_preempt_enable();
}

inline static void maybe_lock(volatile spinlock_t *lk) {
    if (lk)
        spin_lock_irq(lk);
}

inline static void maybe_unlock(volatile spinlock_t *lk) {
    if (lk)
        spin_unlock_irq(lk);
}

void sched_block_task_local_no_switch(struct task *task) {
    sched_preempt_disable();
    int irqs = irqs_masked();
    mask_irqs();

    if (get_state(task) != TASK_STATE_BLOCKED) {
        block(task);
        remove_prior(task);
        update_state(task, TASK_STATE_BLOCKED);
    }

    restore_irq_mask(irqs);
    sched_preempt_enable();
}

void sched_block_task_local_on_queue(struct task *task, struct list_head *wait_head,
                                     volatile spinlock_t *_Nullable lock) {
    sched_preempt_disable();
    int irqs = irqs_masked();
    mask_irqs();

    maybe_lock(lock);

    if (get_state(task) != TASK_STATE_BLOCKED) {
        // These three operations happen atomically with respect to the wait queue.
        wait_queue_insert(wait_head, task);
        block(task);
        remove_prior(task);
        update_state(task, TASK_STATE_BLOCKED);
    }

    maybe_unlock(lock);

    if (task == current_task) {
        sched_run(true);
    }

    restore_irq_mask(irqs);
    sched_preempt_enable();
}

void sched_unblock_one(struct list_head *wait_head, volatile spinlock_t *_Nullable lock) {
    sched_preempt_disable();

    struct task *task = NULL;

    maybe_lock(lock);

    if (!list_empty(wait_head)) {
        struct list_head *node = wait_head->next;
        task = CONTAINER_OF(node, struct task, wait_queue_node);
        wait_queue_del(task);
    }

    maybe_unlock(lock);

    if (task) {
        sched_ready_task_remote(task);
    }

    sched_preempt_enable();
}

void sched_unblock_all(struct list_head *wait_head, volatile spinlock_t *_Nullable lock) {
    while (true) {
        // Try to unblock one task; if none left, exit loop
        sched_preempt_disable();

        struct task *task = NULL;

        maybe_lock(lock);

        if (!list_empty(wait_head)) {
            struct list_head *node = wait_head->next;
            task = CONTAINER_OF(node, struct task, wait_queue_node);
            wait_queue_del(task);
        }

        maybe_unlock(lock);

        sched_preempt_enable();

        if (!task) {
            break; // no more tasks to unblock
        }

        sched_ready_task_remote(task);
    }
}

// WARNING: You must use this function EVERY time a task escapes the current context.
static struct task *prepare_for_escape(struct task *task) {
    task_ref_inc(task);
    return task;
}

void sched_ready_task_remote(struct task *task) {
    sched_preempt_disable();
    guarantee_task_pin(task);

    if (task->cpu == this_cpu()) {
        // we had to pin before checking cpu, otherwise it would've been racey.
        sched_ready_task_local(task);
        task_unpin(task);
    } else {
        struct sched_sgi_payload *payload = kmalloc(sizeof(*payload));
        payload->message_type = SCHED_MESSAGE_READY_TASK;
        payload->task = prepare_for_escape(task);
        send_sgi(task->cpu, SCHED_SGI, payload);
    }

    // unpin happens by sgi recipient.
    sched_preempt_enable();
}

void sched_suspend_task_remote(struct task *task) {
    sched_preempt_disable();
    guarantee_task_pin(task);

    if (task->cpu == this_cpu()) {
        sched_suspend_task_local(task);
        task_unpin(task);
    } else {
        struct sched_sgi_payload *payload = kmalloc(sizeof(*payload));
        payload->message_type = SCHED_MESSAGE_SUSPEND_TASK;
        payload->task = prepare_for_escape(task);
        send_sgi(task->cpu, SCHED_SGI, payload);
    }

    // unpin happens by sgi recipient.
    sched_preempt_enable();
}

void sched_block_task_remote(struct task *task, struct list_head *wait_head,
                             volatile spinlock_t *_Nullable lock) {
    sched_preempt_disable();
    guarantee_task_pin(task);

    if (task->cpu == this_cpu()) {

        spin_lock_irq(lock);
        sched_block_task_local_on_queue(task, wait_head, lock);
        task_unpin(task);

    } else {
        struct sched_sgi_payload *payload = kmalloc(sizeof(*payload));
        payload->message_type = SCHED_MESSAGE_BLOCK_TASK;
        payload->task = prepare_for_escape(task);
        payload->wait_head = wait_head;
        payload->wait_head_lock = lock;
        send_sgi(task->cpu, SCHED_SGI, payload);
    }

    // unpin happens by sgi recipient.
    sched_preempt_enable();
}

void sched_yield(void) {
    sched_preempt_disable();

    sched_ready_task_local(current_task);
    sched_run(true);

    sched_preempt_enable();
}

void sched_preempt_disable(void) {
    if (current_task)
        current_task->preempt_counter++;
}

void sched_preempt_enable(void) {
    if (current_task)
        current_task->preempt_counter--;
}

bool can_preempt(void) { return !current_task || current_task->preempt_counter == 0; }

time_t get_min_vruntime(void) { return min_vruntime; }
