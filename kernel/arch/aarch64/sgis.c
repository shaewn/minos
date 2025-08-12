#include "sgis.h"
#include "interrupts.h"
#include "kmalloc.h"
#include "sched.h"
#include "task.h"

static void sched_sgi_handler(intid_t intid, void *context);

void setup_sgis(void) {
    establish_private_handler(SCHED_SGI, sched_sgi_handler, NULL);
    irq_enable_private(SCHED_SGI, false);
}

static void sched_sgi_handler(intid_t intid, void *context) {
    struct sgi_data data;
    accept_sgi(intid, &data);

    struct sched_sgi_payload *payload = data.payload;

    bool do_unpin = false;

    switch (payload->message_type) {
        case SCHED_MESSAGE_NONE:
            break;
        case SCHED_MESSAGE_READY_TASK: {
            // Scheduler obviously not running right now, and won't (interrupts disabled).
            // But this will disable/enable preemption anyway. Meh.
            sched_ready_task_local(payload->task);
            do_unpin = true;
            break;
        }

        case SCHED_MESSAGE_SUSPEND_TASK: {
            sched_suspend_task_local(payload->task);
            do_unpin = true;
            break;
        }

        case SCHED_MESSAGE_BLOCK_TASK: {
            sched_block_task_local(payload->task, payload->wait_head, payload->wait_head_lock);
            do_unpin = true;
            break;
        }
    }

    if (do_unpin) {
        task_unpin(payload->task);
    }

    kfree(payload);

    end_sgi(intid);
}
