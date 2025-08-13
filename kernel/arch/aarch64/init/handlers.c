#include "types.h"
#include "macros.h"

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

void early_ehandler(void) {
    uint64_t esr = esr_el1_read();
    uint64_t iss2, ec, il, iss;


    iss2 = EXTRACT(esr, 36, 32);
    il = EXTRACT(esr, 25, 25);
    iss = EXTRACT(esr, 24, 0);
    ec = EXTRACT(esr, 31, 26);
    ExceptionClass ex_cls = ec; 

    (void)iss2;
    (void)il;
    (void)iss;

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

    (void)str;
}

/* Utils */

static uint64_t esr_el1_read(void) {
    uint64_t syndrome;

    asm volatile("mrs %0, esr_el1" : "=r"(syndrome));

    return syndrome;
}
