#include "sched.h"
#include "kmalloc.h"
#include "spinlock.h"
#include "task.h"

#define run_queue_root GET_PERCPU(__pcpu_run_queue_root)
#define blocked_list GET_PERCPU(__pcpu_blocked_list)
#define suspended_list GET_PERCPU(__pcpu_suspended_list)
#define min_vruntime GET_PERCPU(__pcpu_min_vruntime)

PERCPU_UNINIT struct rb_node *__pcpu_run_queue_root;
PERCPU_INIT struct list_head __pcpu_blocked_list, __pcpu_suspended_list;
PERCPU_UNINIT time_t __pcpu_min_vruntime;

struct rb_node *get_leftmost_node(void) {
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
}

static void run_queue_del(struct task *task) {
    rb_del(&task->sched_node.run_queue_node, &run_queue_root);
    update_min_vruntime();
}

void sched_run(bool save_state) {
    struct rb_node *node = get_leftmost_node();
    struct task *selected =
        node ? CONTAINER_OF(CONTAINER_OF(node, struct sched_node, run_queue_node), struct task,
                            sched_node)
             : NULL;

    if (selected) {
        run_queue_del(selected);
    }

    if (save_state) {
        save_state_and_switch_to(current_task, selected);
    } else {
        no_save_switch_to(selected);
    }
}

static void remove_prior(struct task *task) {
    switch (task->state) {
        case TASK_STATE_BLOCKED: {
            list_del(&task->sched_node.blocked_node);
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
            list_del(&task->sched_node.suspended_node);
            break;
        }
    }
}

void sched_ready_task_local(struct task *task) {
    sched_preempt_disable();

    remove_prior(task);
    update_state(task, TASK_STATE_READY);
    run_queue_insert(task);

    task->cpu = this_cpu();

    sched_preempt_enable();
}

void sched_suspend_task_local(struct task *task) {
    sched_preempt_disable();

    remove_prior(task);
    update_state(task, TASK_STATE_SUSPENDED);
    list_add_tail(&task->sched_node.suspended_node, &suspended_list);

    task->cpu = this_cpu();

    if (task == current_task) {
        sched_run(true);
    }

    sched_preempt_enable();
}

inline static void maybe_lock(spinlock_t *lk) {
    if (lk)
        spin_lock_irq_save(lk);
}

inline static void maybe_unlock(spinlock_t *lk) {
    if (lk)
        spin_unlock_irq_restore(lk);
}

void sched_block_task_local(struct task *task, struct list_head *wait_head,
                            spinlock_t *_Nullable lock) {
    sched_preempt_disable();

    maybe_lock(lock);

    // These three operations happen atomically with respect to the wait queue.
    remove_prior(task);
    update_state(task, TASK_STATE_BLOCKED);
    list_add_tail(&task->wait_queue_node, wait_head);

    maybe_unlock(lock);

    list_add_tail(&task->sched_node.blocked_node, &blocked_list);
    task->cpu = this_cpu();

    if (task == current_task) {
        sched_run(true);
    }

    sched_preempt_enable();
}

void sched_unblock_one(struct list_head *wait_head, spinlock_t *_Nullable lock) {
    sched_preempt_disable();

    struct task *task = NULL;

    maybe_lock(lock);

    if (!list_empty(wait_head)) {
        struct list_head *node = wait_head->next;
        list_del(node);

        task = CONTAINER_OF(node, struct task, wait_queue_node);
    }

    maybe_unlock(lock);

    if (task) {
        sched_ready_task_remote(task);
    }

    sched_preempt_enable();
}

void sched_unblock_all(struct list_head *wait_head, spinlock_t *_Nullable lock) {
    while (true) {
        // Try to unblock one task; if none left, exit loop
        sched_preempt_disable();

        struct task *task = NULL;

        maybe_lock(lock);

        if (!list_empty(wait_head)) {
            struct list_head *node = wait_head->next;
            list_del(node);
            task = CONTAINER_OF(node, struct task, wait_queue_node);
        }

        maybe_unlock(lock);

        sched_preempt_enable();

        if (!task) {
            break; // no more tasks to unblock
        }

        sched_ready_task_remote(task);
    }
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
        payload->task = task;
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
        payload->task = task;
        send_sgi(task->cpu, SCHED_SGI, payload);
    }

    // unpin happens by sgi recipient.
    sched_preempt_enable();
}

void sched_block_task_remote(struct task *task, struct list_head *wait_head,
                             spinlock_t *_Nullable lock) {
    sched_preempt_disable();
    guarantee_task_pin(task);

    if (task->cpu == this_cpu()) {

        sched_block_task_local(task, wait_head, lock);
        task_unpin(task);

    } else {
        struct sched_sgi_payload *payload = kmalloc(sizeof(*payload));
        payload->message_type = SCHED_MESSAGE_BLOCK_TASK;
        payload->task = task;
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
        __atomic_fetch_add(&current_task->preempt_counter, 1, __ATOMIC_ACQ_REL);
}

void sched_preempt_enable(void) {
    if (current_task)
        __atomic_fetch_sub(&current_task->preempt_counter, 1, __ATOMIC_RELEASE);
}

bool can_preempt(void) {
    return !current_task || __atomic_load_n(&current_task->preempt_counter, __ATOMIC_ACQUIRE) == 0;
}

time_t get_min_vruntime(void) { return min_vruntime; }
