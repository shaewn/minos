#include "timer.h"
#include "arch/aarch64/interrupts.h"
#include "cpu.h"
#include "driver.h"
#include "memory.h"
#include "output.h"
#include "sched.h"
#include "task.h"

#define SYS_REG_READ64(reg)                                                                        \
    ({                                                                                             \
        uint64_t value;                                                                            \
        asm volatile("mrs %0, " #reg : "=r"(value));                                               \
        value;                                                                                     \
    })
#define SYS_REG_WRITE64(reg, value)                                                                \
    do {                                                                                           \
        asm volatile("msr " #reg ", %0" ::"r"(value));                                             \
    } while (0)

#define timer_ctx GET_PERCPU(__pcpu_timer_ctx)
#define min_context GET_PERCPU(__pcpu_min_context)

static uint64_t cntp_ctl_el0_read(void) { return SYS_REG_READ64(cntp_ctl_el0); }

static void cntp_ctl_el0_write(uint64_t value) { SYS_REG_WRITE64(cntp_ctl_el0, value); }

static uint64_t cntp_tval_el0_read() { return SYS_REG_READ64(cntp_tval_el0); }

static void cntp_tval_el0_write(uint64_t value) { SYS_REG_WRITE64(cntp_tval_el0, value); }

void timer_enable(void) { cntp_ctl_el0_write(cntp_ctl_el0_read() | 1); }

void timer_disable(void) { cntp_ctl_el0_write(cntp_ctl_el0_read() & ~1ull); }

void timer_set_imask(void) { cntp_ctl_el0_write(cntp_ctl_el0_read() | 2); }

void timer_clear_imask(void) { cntp_ctl_el0_write(cntp_ctl_el0_read() & ~2ull); }

void timer_set_counter(uint32_t val) {
    cntp_tval_el0_write(cntp_tval_el0_read() & ~0xffffffffull | val);
}

uint32_t timer_get_counter(void) { return cntp_tval_el0_read() & 0xffffffff; }

bool timer_check_cond(void) { return (cntp_ctl_el0_read() >> 2) & 1; }

uint32_t timer_gethz(void) {
    uint64_t cntfrq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(cntfrq));

    return cntfrq & 0xffffffff;
}

static void set_tval(int32_t value) {
    cntp_tval_el0_write(cntp_tval_el0_read() & ~0xffffffffull | value);
}

static int32_t get_tval(void) { return (int32_t)cntp_tval_el0_read(); }

time_t timer_get_phys(void) {
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

#define TIME_SLICE() (timer_gethz())
#define TIMER_INTID() ((intid_t)30)

static PERCPU_UNINIT struct timer_context {
    uint32_t count;
} __pcpu_timer_ctx;

PERCPU_UNINIT struct regs __pcpu_min_context;

void wfi_loop(void) {
    while (1) {
        asm volatile("wfi");
        kprint("We were interrupted. (cpu %u)\n", this_cpu());
    }
}

static void begin_slice(void) { timer_set_counter(TIME_SLICE()); }

void no_save_switch_to(struct task *task) {
    current_task = task;

    struct regs *regs, local_regs;

    if (task) {
        regs = &task->cpu_regs;
    } else {
        regs = &local_regs;
        regs->sp = min_context.sp;
        regs->pstate = min_context.pstate;
        regs->pc = (uint64_t)wfi_loop;
    }

    // TODO: Restore fp_regs and extra_regs.

    [[noreturn]] void restore_then_eret(uint64_t orig_sp_el1, struct regs *regs);
    update_state(task, TASK_STATE_RUNNING);
    begin_slice();
    restore_then_eret(min_context.sp, regs);
}

void ex_save_context_to(struct task *task) {
    struct regs *regs = &task->cpu_regs;
    copy_memory(regs, &min_context, sizeof(*regs));

    if (PSTATE_USES_SP_EL0(regs->pstate)) {
        asm volatile("mrs %0, sp_el0" : "=r"(regs->sp));
    }

    // FIXME: Save floating point/simd registers.
}

static void timer_handle(void) {
    kprint("Handling...\n");

    if (current_task) {
        ex_save_context_to(current_task);

        if (!can_preempt()) {
            kprint("Can't preempt, running same task...\n");
            no_save_switch_to(current_task);
        } else {
            sched_ready_task_local(current_task);
        }

        current_task = NULL;
    }

    sched_run(false);
}

static void timer_handler(void *ctx, intid_t intid) {
    end_intid(intid);
    struct timer_context *tctx = ctx;

    ++tctx->count;

    timer_handle();
}

static void timer_on_enable(void *context) {
    timer_disable();
    irq_enable_private(TIMER_INTID(), true);
    begin_slice();
    timer_enable();
}

static void timer_on_disable(void *context) { irq_disable_private(TIMER_INTID()); }

#define timer_driver GET_PERCPU(__pcpu_timer_driver)
static PERCPU_INIT struct driver __pcpu_timer_driver = {
    "Generic Timer", 1, {30}, NULL, NULL, NULL, timer_on_enable, timer_on_disable, timer_handler};

void timer_start(void) {
    uint32_t freq_hz = timer_gethz();

    timer_driver.context = &timer_ctx;
    enable_driver(register_private_driver(&timer_driver));

    wfi_loop();
}
