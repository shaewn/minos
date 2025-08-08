#include "interrupts.h"
#include "../../pltfrm.h"
#include "bspinlock.h"
#include "cpu.h"
#include "kmalloc.h"
#include "list.h"
#include "macros.h"
#include "memory.h"
#include "gic.h"

struct global_handler {
    struct list_head node;
    interrupt_handler_t handler;
    void *context;
    uint64_t identifier;
};

#define NUM_SPIS (1019 - 32 + 1)
#define NUM_ESPIS (5119 - 4096 + 1)
#define NUM_GLOBAL_HANDLERS (NUM_SPIS + NUM_ESPIS)
static bspinlock_t global_handler_lists_lock;
static struct list_head global_handler_lists[NUM_GLOBAL_HANDLERS];

struct handler {
    interrupt_handler_t ih;
    void *context;
};

#define NUM_SGIS_AND_PPIS 32
#define NUM_EPPIS (1119 - 1056 + 1)
#define NUM_PRIVATE_HANDLERS (NUM_SGIS_AND_PPIS + NUM_EPPIS)
#define private_handlers GET_PERCPU(__pcpu_private_handlers)
static PERCPU_UNINIT struct handler __pcpu_private_handlers[NUM_PRIVATE_HANDLERS];

// private per-cpu.
bool is_private_interrupt(intid_t intid) { return intid < 32 || intid >= 1056 && intid <= 1119; }

// shared across all cpus.
bool is_global_interrupt(intid_t intid) {
    return intid >= 32 && intid <= 1019 || intid >= 4096 && intid <= 5119;
}

static struct handler *get_private_handler(intid_t intid) {
    if (intid <= 31) {
        return private_handlers + intid;
    }

    if (intid >= 1056 && intid <= 1119) {
        return private_handlers + intid - 1056 + NUM_SGIS_AND_PPIS;
    }

    return NULL;
}

struct list_head *get_global_handler_list(intid_t intid) {
    if (intid >= 32 && intid <= 1019) {
        return global_handler_lists + intid - 32;
    }

    if (intid >= 4096 && intid <= 5119) {
        return global_handler_lists + intid - 4096 + NUM_SPIS;
    }

    return NULL;
}

void establish_private_handler(intid_t intid, interrupt_handler_t handler, void *context) {
    if (!is_private_interrupt(intid)) {
        return;
    }

    struct handler * h = get_private_handler(intid);
    h->ih = handler;
    h->context = context;
}

void establish_global_handler(intid_t intid, interrupt_handler_t handler, handler_id_t identifier, void *context) {
    if (!is_global_interrupt(intid)) {
        return;
    }

    struct global_handler *ghandler = kmalloc(sizeof(*ghandler));
    ghandler->handler = handler;
    ghandler->identifier = identifier;
    ghandler->context = context;

    // If we were to get interrupted with the lock by a global (shared peripheral) interrupt,
    // it would also try to obtain the lock, causing a deadlock.
    int val = irqs_masked();
    mask_irqs();
    bspinlock_lock(&global_handler_lists_lock);

    struct list_head *list = get_global_handler_list(intid);

    list_add_tail(&ghandler->node, list);

    bspinlock_unlock(&global_handler_lists_lock);
    restore_irq_mask(val);
}

void deestablish_global_handler(intid_t intid, handler_id_t identifier) {
    if (!is_global_interrupt(intid))
        return;

    int val = irqs_masked();
    mask_irqs();
    bspinlock_lock(&global_handler_lists_lock);

    struct list_head *list = get_global_handler_list(intid);
    LIST_FOREACH(list, node) {
        struct global_handler *handler = LIST_ELEMENT(node, struct global_handler, node);
        if (handler->identifier == identifier) {
            list_del(&handler->node);
            break;
        }
    }

    bspinlock_unlock(&global_handler_lists_lock);
    restore_irq_mask(val);
}

void dispatch_irq(intid_t intid) {
    if (is_private_interrupt(intid)) {
        struct handler *h = get_private_handler(intid);

        if (h->ih != IH_NO_HANDLER) {
            h->ih(intid, h->context);
        }
    } else if (is_global_interrupt(intid)) {
        bspinlock_lock(&global_handler_lists_lock);

        struct list_head *list = get_global_handler_list(intid);

        LIST_FOREACH(list, node) {
            struct global_handler *handler = LIST_ELEMENT(node, struct global_handler, node);
            handler->handler(intid, handler->context);
        }

        bspinlock_unlock(&global_handler_lists_lock);
    }
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
    asm volatile("mrs %0, daif" : "=r"(daif));

    return (daif >> 6) & 2;
}

void restore_irq_mask(int val) {
    if (val)
        mask_irqs();
    else
        unmask_irqs();
}

void mask_irqs(void) { asm volatile("msr daifset, #2\nisb"); }

void unmask_irqs(void) { asm volatile("msr daifclr, #2\nisb"); }

extern uintptr_t get_rd_base(void);
extern uintptr_t get_sgi_base(void);

static void set_enabled_sgi_or_ppi(uint32_t intid, bool enabled) {
    uintptr_t sgi_base = get_sgi_base();

    uint32_t n = 0;
    uint32_t bit = intid;

    if (intid >= 1056) {
        // extended.
        n = (intid - 1024) / 32;
        bit = (intid - 1024) % 32;
    }

    uintptr_t reg_offset = enabled ? GICR_ISENABLER0 : GICR_ICENABLER0;
    uintptr_t reg = sgi_base + reg_offset + n * 4;
    mmio_write32(reg, mmio_read32(reg) | (1u << bit));
}

static void cfgr_sgi_or_ppi(uint32_t intid, bool edge_sensitive) {
    uintptr_t sgi_base = get_sgi_base();

    uint32_t field = (!!edge_sensitive) << 1;

    uint32_t n = 0;
    uint32_t shift = intid / 16;

    if (intid >= 1056) {
        n = (intid - 1024) / 16;
        shift = 2 * ((intid - 1024) % 16);
    }

    uintptr_t reg = sgi_base + GICR_ICFGR0 + n * 4;
    mmio_write32(reg, (mmio_read32(reg) & ~(0x3 << shift)) | (field << shift));
}

static void set_group_sgi_or_ppi(uint32_t intid, int grp) {
    // 0 or 1
    grp = !!grp;

    uint32_t n = intid >= 1056 ? intid - 1024 : intid;
    uint32_t bit = n % 32;
    n = n / 32;

    uintptr_t sgi_base = get_sgi_base();
    uintptr_t group_ptr = sgi_base + GICR_IGROUPR0 + n * 4;

    mmio_write32(group_ptr, (mmio_read32(group_ptr) & ~(1u << bit)) | grp << bit);
}

void irq_enable_private(intid_t intid, bool level_sensitive) {
    if (!is_private_interrupt(intid)) {
        return;
    }

    set_enabled_sgi_or_ppi(intid, true);
    cfgr_sgi_or_ppi(intid, !level_sensitive);
    set_group_sgi_or_ppi(intid, 1);
}

void irq_disable_private(intid_t intid) {
    set_enabled_sgi_or_ppi(intid, false);
}

void irq_enable_shared(intid_t intid, bool level_sensitive);
void irq_disable_shared(intid_t intid);
