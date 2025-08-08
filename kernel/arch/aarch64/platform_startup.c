#include "../../pltfrm.h"
#include "driver.h"
#include "cpu.h"
#include "die.h"
#include "fdt.h"
#include "gic.h"
#include "interrupts.h"
#include "kmalloc.h"
#include "kvmalloc.h"
#include "macros.h"
#include "memory.h"
#include "output.h"
#include "phandle_table.h"
#include "pltfrm.h"
#include "prot.h"
#include "rdt.h"
#include "string.h"
#include "timer.h"
#include "vmap.h"

extern struct fdt_header *fdt_header_phys;
struct fdt_header *fdt_header;

uintptr_t gicd_base_ptr, gicr_base_ptr;

PERCPU_UNINIT uintptr_t __pcpu_RD_base;
#define RD_base GET_PERCPU(__pcpu_RD_base)

static uint32_t get_affinity_value(void) {
    uint64_t mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));

    uint32_t aff = 0;
    aff |= EXTRACT(mpidr, 39, 32) << 24;
    aff |= EXTRACT(mpidr, 23, 16) << 16;
    aff |= EXTRACT(mpidr, 15, 8) << 8;
    aff |= EXTRACT(mpidr, 7, 0) << 0;

    return aff;
}

uintptr_t get_rd_base(void) {
    if (RD_base != 0)
        return RD_base;

    uintptr_t rd_base = gicr_base_ptr;

    uint32_t aff = get_affinity_value();

    while (1) {
        uintptr_t gicr_typer = rd_base + GICR_TYPER;

        // 'Last' bit set.
        if (mmio_read64(gicr_typer) & (1 << 4)) {
            break;
        }

        uint64_t typer = mmio_read64(gicr_typer);

        if (typer >> 32 == aff) {
            break;
        }

        rd_base += 1 << 17;
    }

    return RD_base = rd_base;
}

uintptr_t get_sgi_base(void) { return get_rd_base() + 0x10000; }

static void dsb_isb(void) { asm volatile("dsb ish\nisb\n"); }

static void isb(void) { asm volatile("isb"); }

static struct rdt_node *read_phandle(struct rdt_prop *prop) {
    return phandle_table_get(read_cell(prop));
}

static int is_compatible(struct rdt_node *node, const char *compat) {
    struct rdt_prop *compat_prop = rdt_find_prop(node, "compatible");
    if (!compat_prop)
        return 0;

    const char *sbase = compat_prop->data;

    uint32_t i = 0;

    while (i < compat_prop->data_length) {
        const char *s = sbase + i;

        if (string_compare(s, compat) == 0) {
            return 1;
        }

        i += string_len(s) + 1;
    }

    return 0;
}

struct rdt_node *find_primary_interrupt_controller(void) {
    extern struct rdt_node *rdt_root;
    struct rdt_prop *root_intp = rdt_find_prop(rdt_root, "interrupt-parent");

    if (root_intp) {
        return read_phandle(root_intp);
    }

    return NULL;
}

static uint32_t *read_reg(uint32_t *wp, uint32_t ac, uint32_t sc, uint64_t *addr, uint64_t *size) {
    if (ac) {
        *addr = FROM_BE_32(*wp++);
        if (ac == 2) {
            *addr <<= 32;
            *addr |= FROM_BE_32(*wp++);
        }
    }

    if (sc) {
        *size = FROM_BE_32(*wp++);
        if (sc == 2) {
            *size <<= 32;
            *size |= FROM_BE_32(*wp++);
        }
    }

    return wp;
}

int is_pend_sgi_or_ppi(uint32_t intid) {
    uintptr_t sgi_base = get_sgi_base();

    uint32_t n = 0;
    uint32_t bit = intid;

    if (intid >= 1056) {
        bit = (intid - 1024) / 32;
        bit = (intid - 1024) % 32;
    }

    uintptr_t reg = sgi_base + GICR_ISPENDR0 + n * 4;

    return (mmio_read32(reg) >> bit) & 1;
}

#define TIME_SLICE() (timer_gethz())
#define TIMER_INTID() ((intid_t)30)

static PERCPU_UNINIT struct timer_context {
    uint32_t count;
} __pcpu_timer_ctx;
#define timer_ctx GET_PERCPU(__pcpu_timer_ctx)

static void timer_handler(void *ctx, intid_t intid) {
    struct timer_context *tctx = ctx;
    kprint("Timer!");

    if (tctx->count) {
        kprint(" (%u)\n", tctx->count);
    } else {
        kputstr("\n");
    }

    ++tctx->count;

    timer_set_counter(TIME_SLICE());
    end_intid(intid);
}

static void timer_on_enable(void *context) {
    irq_enable_private(TIMER_INTID(), true);
}

static void timer_on_disable(void *context) {
    irq_disable_private(TIMER_INTID());
}

static PERCPU_INIT struct driver timer_driver = {
    "Generic Timer",
    {30},
    NULL,
    NULL,
    NULL,
    timer_on_enable,
    timer_on_disable,
    timer_handler
};

void setup_timer(void) {
    uint32_t freq_hz = timer_gethz();

    kprint("The timer operates at %u MHz (%u Hz)\n", freq_hz / 1000000, freq_hz);

    timer_driver.context = &timer_ctx;
    register_private_driver(&timer_driver);

    timer_set_counter(TIME_SLICE());
    timer_enable();

    while (1)
        ;
}

static void find_parent_ac_sc(struct rdt_node *node, uint32_t *ac, uint32_t *sc) {
    struct rdt_prop *acp = rdt_find_prop(node->parent, "#address-cells");
    struct rdt_prop *scp = rdt_find_prop(node->parent, "#size-cells");

    if (acp) {
        *ac = FROM_BE_32(*(uint32_t *)acp->data);
    } else {
        *ac = 2;
    }

    if (scp) {
        *sc = FROM_BE_32(*(uint32_t *)scp->data);
    } else {
        *sc = 1;
    }
}

void cpu_setup_interrupts(void) {
    uint64_t icc_sre;
    asm volatile("mrs %0, icc_sre_el1" : "=r"(icc_sre));
    icc_sre |= 1;
    asm volatile("msr icc_sre_el1, %0" ::"r"(icc_sre));
    isb();

    uintptr_t rd_base = get_rd_base();
    // rd_base is the register map base for this cpu's redistributor.

    uintptr_t gicr_waker_ptr = rd_base + GICR_WAKER;
    // Unset GICR_WAKER.ProcessorSleep
    mmio_write32(gicr_waker_ptr, mmio_read32(gicr_waker_ptr) & ~2u);

    // Wait until GICR_WAKER.ChildrenAsleep is not set.
    while (mmio_read32(gicr_waker_ptr) & 4)
        ;

    kprint("GICR_WAKER contains 0x%08x\n", mmio_read32(gicr_waker_ptr));

    uint64_t icc_pmr;
    asm volatile("mrs %0, icc_pmr_el1" : "=r"(icc_pmr));

    kprint("icc_pmr_el1 initially contains 0x%16lx\n", icc_pmr);

    icc_pmr |= 0xff;

    asm volatile("msr icc_pmr_el1, %0" ::"r"(icc_pmr));

    uint64_t icc_igrpen1;

    asm volatile("mrs %0, icc_igrpen1_el1" : "=r"(icc_igrpen1));

    kprint("icc_igrpen_el1 initially contains 0x%16lx\n", icc_igrpen1);

    icc_igrpen1 |= 1;

    asm volatile("msr icc_igrpen1_el1, %0" ::"r"(icc_igrpen1));
}

void setup_interrupts(void) {
    struct rdt_node *intc = find_primary_interrupt_controller();
    if (!intc) {
        KFATAL("Failed to identify primary interrupt controller.\n");
    }

    if (!rdt_find_prop(intc, "interrupt-controller")) {
        KFATAL("Primary interrupt controller does not have "
               "interrupt-controller property.\n");
    }

    kprint("Primary interrupt controller is &%s\n", intc->name);

    if (is_compatible(intc, "arm,gic-v3")) {
        kprint("GIC-v3 controller.\n");
    } else {
        KFATAL("Only GIC-v3 is supported.\n");
    }

    struct rdt_prop *reg = rdt_find_prop(intc, "reg");

    uint32_t *data = (uint32_t *)reg->data;

    uint32_t ac, sc;
    find_parent_ac_sc(intc, &ac, &sc);

    uint64_t gicd_base, gicd_size;
    data = read_reg(data, ac, sc, &gicd_base, &gicd_size);

    kprint("GICD: 0x%lx, 0x%lx\n", gicd_base, gicd_size);

    uint64_t gicr_base, gicr_size;
    data = read_reg(data, ac, sc, &gicr_base, &gicr_size);

    kprint("GICR: 0x%lx, 0x%lx\n", gicr_base, gicr_size);

    uint64_t gicd_pages = (gicd_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t gicr_pages = (gicr_size + PAGE_SIZE - 1) / PAGE_SIZE;

    uint64_t total_pages = gicd_pages + gicr_pages;

    void *base = kvmalloc(total_pages, KVMALLOC_PERMANENT);
    gicd_base_ptr = (uintptr_t)base;
    gicr_base_ptr = gicd_base_ptr + gicd_pages * PAGE_SIZE;
    vmap_range(gicd_base_ptr, gicd_base, gicd_pages, PROT_RSYS | PROT_WSYS,
               MEMORY_TYPE_DEVICE_STRICT, 0);
    vmap_range(gicr_base_ptr, gicr_base, gicr_pages, PROT_RSYS | PROT_WSYS,
               MEMORY_TYPE_DEVICE_STRICT, 0);

    cpu_setup_interrupts();

    uintptr_t gicd_ctlr = gicd_base_ptr + GICD_CTLR;
    mmio_write32(gicd_ctlr, mmio_read32(gicd_ctlr) | /* Enable bit */ 0x2);
    dsb_isb();

    unmask_irqs();

    setup_timer();
}

void platform_startup(void) {
    struct fdt_header *header = kvmalloc(1, 0);

    int r = vmap((uintptr_t)header, (uintptr_t)fdt_header_phys, PROT_RSYS,
                 MEMORY_TYPE_NON_CACHEABLE, 0);
    if (r < 0) {
        KFATAL("vmap failed: %d\n", r);
    }

    if (FROM_BE_32(header->magic) != FDT_MAGIC) {
        KFATAL("Failed reading header\n");
    }

    uint32_t total_size = FROM_BE_32(header->totalsize);
    kprint("Total size: %u\n", total_size);

    uint32_t total_pages = total_size / PAGE_SIZE;
    kprint("Total pages: %u\n", total_pages);

    vumap((uintptr_t)header);
    kvfree(header);

    // Now remap the full thing.

    // TODO: Make sure that we reserve the memory the fdt is occupying, otherwise our physical
    // memory allocator might allocate it.
    fdt_header = kvmalloc(total_pages, KVMALLOC_PERMANENT);

    for (uint32_t i = 0; i < total_pages; i++) {
        vmap((uintptr_t)fdt_header + i * PAGE_SIZE, (uintptr_t)fdt_header_phys + i * PAGE_SIZE,
             PROT_RSYS, MEMORY_TYPE_NON_CACHEABLE, 0);
    }

    build_rdt();
    print_rdt();
    setup_interrupts();
}
