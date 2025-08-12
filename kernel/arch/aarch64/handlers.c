#include "macros.h"
#include "interrupts.h"
#include "output.h"

typedef enum {
    EC_UNKNOWN = 0x0,
    EC_TRAPPED_WF = 0x1,
    EC_TRAPPED_SME_SVE_ADV_SIMD_FLT = 0x7,
    EC_ILL_STATE = 0xe,
    EC_AARCH64_SVC = 0x15,
    EC_TRAPPED_MSR_MRS_SYS = 0x18,
    EC_INSTR_ABRT_LOWER_LEVEL = 0x20,
    EC_INSTR_ABRT = 0x21,
    EC_PC_ALIGN = 0x22,
    EC_DATA_ABRT_LOWER_LEVEL = 0x24,
    EC_DATA_ABRT = 0x25,
    EC_SP_ALIGN = 0x26,
    EC_TRAPPED_FP_EX = 0x2c,
    EC_SERROR_INT = 0x2f,
    EC_BREAKPOINT_EX_LOWER_LEVEL = 0x30,
    EC_BRKAKPOINT_EX = 0x31,
    EC_SOFTWARE_STEP_LOWER_LEVEL = 0x32,
    EC_SOFTWARE_STEP = 0x33,
    EC_WATCHPOINT_LOWER_LEVEL = 0x34,
    EC_WATCHPOINT = 0x35,
    EC_BRK_INSTR = 0x3c,
} ExceptionClass;

static uint64_t esr_el1_read(void);

void kernel_ehandler(void) {
    kprint("We're in the exception handler!\n");

    uint64_t syndrome = esr_el1_read();

    kprint("The syndrome register contains: 0x%lx\n", syndrome);

    ExceptionClass ex_cls = (syndrome >> 26) & 0x3f;

    kprint("Exception class value is %d\n", ex_cls);

#define O(s) case s: str = #s; break;

    const char *str;

    switch (ex_cls) {
        O(EC_UNKNOWN)
        O(EC_TRAPPED_WF)
        O(EC_TRAPPED_SME_SVE_ADV_SIMD_FLT)
        O(EC_ILL_STATE)
        O(EC_AARCH64_SVC)
        O(EC_TRAPPED_MSR_MRS_SYS)
        O(EC_INSTR_ABRT_LOWER_LEVEL)
        O(EC_INSTR_ABRT)
        O(EC_PC_ALIGN)
        O(EC_DATA_ABRT_LOWER_LEVEL)
        O(EC_DATA_ABRT)
        O(EC_SP_ALIGN)
        O(EC_TRAPPED_FP_EX)
        O(EC_SERROR_INT)
        O(EC_BREAKPOINT_EX_LOWER_LEVEL)
        O(EC_BRKAKPOINT_EX)
        O(EC_SOFTWARE_STEP_LOWER_LEVEL)
        O(EC_SOFTWARE_STEP)
        O(EC_WATCHPOINT_LOWER_LEVEL)
        O(EC_WATCHPOINT)
        O(EC_BRK_INSTR)

        default: str = "unhandled exception class"; break;
    }

    kprint("Exception class: %s\n", str);

    uint64_t far;
    asm volatile("mrs %0, far_el1\n" : "=r"(far));

    switch (ex_cls) {
    case EC_INSTR_ABRT_LOWER_LEVEL:
    case EC_INSTR_ABRT:
    case EC_PC_ALIGN:
    case EC_DATA_ABRT_LOWER_LEVEL:
    case EC_DATA_ABRT:
    case EC_SP_ALIGN:
        kprint("FAR_EL1 contains: 0x%lx\n", far);
        break;
    default: break;
    }

    uint64_t elr;
    asm volatile("mrs %0, elr_el1\n" : "=r"(elr));
    kprint("ELR_EL1 contains: 0x%lx\n", elr);

    while (1);
}

void irq_handler(void) {
    intid_t intid = intid_ack();

    void dispatch_irq(intid_t intid);
    dispatch_irq(intid);
}

/* Utils */

static uint64_t esr_el1_read(void) {
    uint64_t syndrome;

    asm volatile("mrs %0, esr_el1" : "=r"(syndrome));

    return syndrome;
}
