#include "interrupts.h"
#include "../../pltfrm.h"
#include "arch/aarch64/cpu_id.h"
#include "config.h"
#include "cpu.h"
#include "gic.h"
#include "kmalloc.h"
#include "list.h"
#include "macros.h"
#include "memory.h"
#include "kconsole.h"
#include "spinlock.h"

#define NUM_SPIS (1019 - 32 + 1)
#define NUM_ESPIS (5119 - 4096 + 1)
#define NUM_GLOBAL_HANDLERS (NUM_SPIS + NUM_ESPIS)

#define NUM_SGIS 16
#define NUM_SGIS_AND_PPIS 32
#define NUM_EPPIS (1119 - 1056 + 1)
#define NUM_PRIVATE_HANDLERS (NUM_SGIS_AND_PPIS + NUM_EPPIS)

#define MAILBOX_STATE_IDLE 0
#define MAILBOX_STATE_WRITE 1
#define MAILBOX_STATE_AWAIT_READ 2
#define MAILBOX_STATE_READ 3

#define private_handlers GET_PERCPU(__pcpu_private_handlers)

uintptr_t gicd_base_ptr, gicr_base_ptr;

struct global_handler {
    struct list_head node;
    interrupt_handler_t handler;
    void *context;
    uint64_t identifier;
};

struct sgi_mailbox {
    struct sgi_data data;
    uint32_t state;
};

struct sgi_mailboxes {
    struct sgi_mailbox per_interrupt[NUM_SGIS];
};

volatile struct sgi_mailboxes per_core_mailboxes[MAX_CPUS];

static volatile spinlock_t global_handler_lists_lock;
static struct list_head global_handler_lists[NUM_GLOBAL_HANDLERS];

struct handler {
    interrupt_handler_t ih;
    void *context;
};

static PERCPU_UNINIT struct handler __pcpu_private_handlers[NUM_PRIVATE_HANDLERS];

// private per-cpu.
bool is_private_interrupt(intid_t intid) { return intid < 32 || (intid >= 1056 && intid <= 1119); }

// shared across all cpus.
bool is_global_interrupt(intid_t intid) {
    return (intid >= 32 && intid <= 1019) || (intid >= 4096 && intid <= 5119);
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

    struct handler *h = get_private_handler(intid);
    h->ih = handler;
    h->context = context;
}

void establish_global_handler(intid_t intid, interrupt_handler_t handler, handler_id_t identifier,
                              void *context) {
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
    spin_lock_irq(&global_handler_lists_lock);

    struct list_head *list = get_global_handler_list(intid);

    list_add_tail(&ghandler->node, list);

    spin_unlock_irq(&global_handler_lists_lock);
    restore_irq_mask(val);
}

void deestablish_global_handler(intid_t intid, handler_id_t identifier) {
    if (!is_global_interrupt(intid))
        return;

    int val = irqs_masked();
    mask_irqs();
    spin_lock_irq(&global_handler_lists_lock);

    struct list_head *list = get_global_handler_list(intid);
    LIST_FOREACH(list, node) {
        struct global_handler *handler = LIST_ELEMENT(node, struct global_handler, node);
        if (handler->identifier == identifier) {
            list_del(&handler->node);
            break;
        }
    }

    spin_unlock_irq(&global_handler_lists_lock);
    restore_irq_mask(val);
}

void dispatch_irq(intid_t intid) {
    if (is_private_interrupt(intid)) {
        struct handler *h = get_private_handler(intid);

        if (h->ih != IH_NO_HANDLER) {
            h->ih(intid, h->context);
        }
    } else if (is_global_interrupt(intid)) {
        spin_lock_irq(&global_handler_lists_lock);

        struct list_head *list = get_global_handler_list(intid);

        LIST_FOREACH(list, node) {
            struct global_handler *handler = LIST_ELEMENT(node, struct global_handler, node);
            handler->handler(intid, handler->context);
        }

        spin_unlock_irq(&global_handler_lists_lock);
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

    // TODO: Make sure that the distributor is forwarding this private interrupt to the CPU's interface for it.
    set_enabled_sgi_or_ppi(intid, true);
    cfgr_sgi_or_ppi(intid, !level_sensitive);
    set_group_sgi_or_ppi(intid, 1);
}

void irq_disable_private(intid_t intid) { set_enabled_sgi_or_ppi(intid, false); }

void irq_enable_shared(intid_t intid, bool level_sensitive);
void irq_disable_shared(intid_t intid);

static void swap_state(volatile struct sgi_mailbox *mailbox, uint32_t from_state,
                       uint32_t to_state) {
    uint32_t expected;

    while (1) {
        while (__atomic_load_n(&mailbox->state, __ATOMIC_RELAXED) != MAILBOX_STATE_IDLE)
            cpu_idle_wait(&mailbox->state);

        expected = from_state;
        if (__atomic_compare_exchange_n(&mailbox->state, &expected, to_state, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED))
            break;
    }
}

int accept_sgi(intid_t intid, struct sgi_data *data_buf) {
    intid &= 0xf;

    volatile struct sgi_mailbox *mailbox = &per_core_mailboxes[this_cpu()].per_interrupt[intid];
    uint32_t expected = MAILBOX_STATE_AWAIT_READ;
    if (!__atomic_compare_exchange_n(&mailbox->state, &expected, MAILBOX_STATE_READ, false,
                                     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return -1;
    }

    data_buf->payload = mailbox->data.payload;
    data_buf->sender = mailbox->data.sender;

    return 0;
}

int end_sgi(intid_t intid) {
    intid &= 0xff;

    volatile struct sgi_mailbox *mailbox = &per_core_mailboxes[this_cpu()].per_interrupt[intid];
    uint32_t expected = MAILBOX_STATE_READ;
    if (!__atomic_compare_exchange_n(&mailbox->state, &expected, MAILBOX_STATE_IDLE, false,
                                     __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
        return -1;
    }

    return 0;
}

// TODO: Make sgi mailboxes queued.
static void send_to_mailbox(cpu_t target, intid_t intid, void *payload) {
    volatile struct sgi_mailbox *mailbox = &per_core_mailboxes[target].per_interrupt[intid];
    swap_state(mailbox, MAILBOX_STATE_IDLE, MAILBOX_STATE_WRITE);

    mailbox->data.payload = payload;
    mailbox->data.sender = this_cpu();

    __atomic_store_n(&mailbox->state, MAILBOX_STATE_AWAIT_READ, __ATOMIC_RELEASE);
}

void send_all_sgi(intid_t intid, void *payload) {
    intid &= 0xf;
    cpu_t current = this_cpu();

    for (cpu_t cpu = 0; cpu < cpu_count(); cpu++) {
        if (cpu == current) {
            continue;
        }

        send_to_mailbox(cpu, intid, payload);
    }

    asm volatile("msr icc_sgi1r_el1, %0" ::"r"(0x0000010000000000 | intid << 24));
}

void send_sgi(cpu_t target, intid_t intid, void *payload) {
    intid &= 0xf;
    send_to_mailbox(target, intid, payload);

    uint32_t affinities = get_affinities(get_mpidr(target));

    uint64_t sgi_val = 0;

    uint64_t aff3 = EXTRACT(affinities, 31, 24);
    uint64_t aff2 = EXTRACT(affinities, 23, 16);
    uint64_t aff1 = EXTRACT(affinities, 15, 8);
    uint64_t aff0 = EXTRACT(affinities, 7, 0);

    sgi_val |= aff3 << 48;
    sgi_val |= aff2 << 32;
    sgi_val |= aff1 << 16;

    uint8_t range = (aff0 / 16) & 0xf;
    uint8_t index = aff0 % 16;

    sgi_val = REPLACE(sgi_val, 47, 44, range);
    sgi_val = REPLACE(sgi_val, 27, 24, intid);
    sgi_val |= 1ull << index;

    // Interrupt routing mode: 0 -> routed to the PEs specified by Aff3.Aff2.Aff1.<target list>

    asm volatile("msr icc_sgi1r_el1, %0" ::"r"(sgi_val));
}
