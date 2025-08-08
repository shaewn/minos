#include "interrupts.h"
#include "../../pltfrm.h"
#include "bspinlock.h"
#include "cpu.h"
#include "macros.h"

#define NUM_SPIS (1019 - 32 + 1)
#define NUM_ESPIS (5119 - 4096 + 1)
#define NUM_GLOBAL_HANDLERS (NUM_SPIS + NUM_ESPIS)
static bspinlock_t global_handlers_lock;
static interrupt_handler_t global_handlers[NUM_GLOBAL_HANDLERS];

#define NUM_SGIS_AND_PPIS 32
#define NUM_EPPIS (1119 - 1056 + 1)
#define NUM_PRIVATE_HANDLERS (NUM_SGIS_AND_PPIS + NUM_EPPIS)
#define private_handlers GET_PERCPU(__pcpu_private_handlers)
static PERCPU_UNINIT interrupt_handler_t __pcpu_private_handlers[NUM_PRIVATE_HANDLERS];

static interrupt_handler_t *get_handler_ptr(intid_t intid) {
    if (intid < 32) {
        return private_handlers + intid;
    }

    if (intid < 1020) {
        return global_handlers + intid - 32;
    }

    if (intid >= 1056 && intid <= 1119) {
        return private_handlers + intid - 1056 + NUM_SGIS_AND_PPIS;
    }

    if (intid >= 4096 && intid <= 5119) {
        return global_handlers + intid - 4096 + NUM_SPIS;
    }

    return NULL;
}

// private per-cpu.
int is_private_interrupt(intid_t intid) { return intid < 32 || intid >= 1056 && intid <= 1119; }

void establish_private_handler(intid_t intid, interrupt_handler_t handler) {
    if (!is_private_interrupt(intid)) {
        return;
    }

    *get_handler_ptr(intid) = handler;
}

// shared across all cpus.
int is_global_interrupt(intid_t intid) {
    return intid >= 32 && intid <= 1019 || intid >= 4096 && intid <= 5119;
}

void establish_global_handler(intid_t intid, interrupt_handler_t handler) {
    if (intid < 32 || intid > 1019 && intid < 4096 || intid > 5119) {
        // Not a global interrupt.
        return;
    }

    bspinlock_lock(&global_handlers_lock);

    *get_handler_ptr(intid) = handler;

    bspinlock_unlock(&global_handlers_lock);
}

interrupt_handler_t get_handler(intid_t intid) {
    interrupt_handler_t *ptr = get_handler_ptr(intid);

    return ptr ? *ptr : IH_NO_HANDLER;
}

intid_t intid_ack(void) {
    uint64_t icc_iar1;
    asm volatile("mrs %0, icc_iar1_el1" : "=r"(icc_iar1));

    uint64_t icc_ctlr;
    asm volatile("mrs %0, icc_ctlr_el1" : "=r"(icc_ctlr));

    uint8_t id_bits_field = EXTRACT(icc_ctlr, 13, 11);
    uint32_t intid = EXTRACT(icc_iar1, id_bits_field ? 23 : 15, 0);

    return intid;
}

void end_intid(intid_t intid) {
    uint64_t icc_eoir1 = intid;
    asm volatile("msr icc_eoir1_el1, %0" ::"r"(icc_eoir1));
}

int irqs_masked(void) {
    uint64_t daif;
    asm volatile("mrs %0, daif" :"=r"(daif));

    return (daif >> 6) & 2;
}

void mask_irqs(void) {
    asm volatile("msr daifset, #2\nisb");
}

void unmask_irqs(void) {
    asm volatile("msr daifclr, #2\nisb");
}
