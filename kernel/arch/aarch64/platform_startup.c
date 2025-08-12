#include "../../pltfrm.h"
#include "arch/aarch64/sgis.h"
#include "sched.h"
#include "arch/aarch64/cpu_id.h"
#include "config.h"
#include "cpu.h"
#include "ctdn_latch.h"
#include "die.h"
#include "driver.h"
#include "fdt.h"
#include "gic.h"
#include "interrupts.h"
#include "kmalloc.h"
#include "kvmalloc.h"
#include "macros.h"
#include "memory.h"
#include "memory_map.h"
#include "output.h"
#include "phandle_table.h"
#include "pltfrm.h"
#include "prot.h"
#include "rdt.h"
#include "secondary_context.h"
#include "startup_task.h"
#include "string.h"
#include "timer.h"
#include "vmap.h"

extern struct fdt_header *fdt_header_phys;
struct fdt_header *fdt_header;

extern uintptr_t gicd_base_ptr, gicr_base_ptr;

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
    if (RD_base != 0) {
        return RD_base;
    }

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

static const uint32_t *read_reg(const uint32_t *wp, uint32_t ac, uint32_t sc, uint64_t *addr,
                                uint64_t *size) {
    if (ac && addr) {
        *addr = FROM_BE_32(*wp++);
        if (ac == 2) {
            *addr <<= 32;
            *addr |= FROM_BE_32(*wp++);
        }
    }

    if (sc && size) {
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

    uint64_t icc_pmr;
    asm volatile("mrs %0, icc_pmr_el1" : "=r"(icc_pmr));

    icc_pmr |= 0xff;

    asm volatile("msr icc_pmr_el1, %0" ::"r"(icc_pmr));

    uint64_t icc_igrpen1;

    asm volatile("mrs %0, icc_igrpen1_el1" : "=r"(icc_igrpen1));

    icc_igrpen1 |= 1;

    asm volatile("msr icc_igrpen1_el1, %0" ::"r"(icc_igrpen1));

    unmask_irqs();

    asm volatile("isb");
}

void setup_interrupts(void) {
    // FIXME: Disable ALL interrupts, global and private, upon initialization.
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

    const uint32_t *data = (const uint32_t *)reg->data;

    uint32_t ac, sc;
    find_parent_ac_sc(intc, &ac, &sc);

    uint64_t gicd_base, gicd_size;
    data = read_reg(data, ac, sc, &gicd_base, &gicd_size);

    uint64_t gicr_base, gicr_size;
    data = read_reg(data, ac, sc, &gicr_base, &gicr_size);

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
}

void *percpu_copy(void) {
    extern char __percpu_begin, __percpu_end;
    uintptr_t percpu_begin = (uintptr_t)&__percpu_begin;
    uintptr_t percpu_end = (uintptr_t)&__percpu_end;
    size_t pages = (percpu_end - percpu_begin + PAGE_SIZE - 1) & ~(size_t)(PAGE_SIZE - 1);
    void *ptr = kvmalloc(pages, KVMALLOC_PERMANENT);

    extern uintptr_t map_percpu(uintptr_t base);
    map_percpu((uintptr_t)ptr);

    return ptr;
}

void *alloc_stack(void) {
    size_t pages = (KSTACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    void *ptr = kvmalloc(pages, KVMALLOC_PERMANENT);

    for (size_t i = 0; i < pages; i++) {
        uintptr_t reg;
        int r = global_acquire_pages(1, &reg, NULL);
        if (r == -1) {
            KFATAL("Failed to allocate seconary cpu stack.\n");
        }

        r = vmap((uintptr_t)ptr + i * PAGE_SIZE, reg, PROT_RSYS | PROT_WSYS, MEMORY_TYPE_NORMAL, 0);

        if (r < 0) {
            KFATAL("Failed to map stack memory.\n");
        }
    }

    return ptr + KSTACK_SIZE;
}

ctdn_latch_t startup_latch;

void bring_up_secondary(void) {
    struct rdt_node *cpus_node = rdt_find_node(NULL, "/cpus");
    if (!cpus_node)
        KFATAL("Failed to find cpus_node");

    struct rdt_prop *ac = rdt_find_prop(cpus_node, "#address-cells");
    uint32_t cells = ac ? read_cell(ac) : 2;

    LIST_FOREACH(&cpus_node->child_list, cnode) {
        struct rdt_node *child = CONTAINER_OF(cnode, struct rdt_node, node);

        if (string_begins(child->name, "cpu@")) {
            struct rdt_prop *reg = rdt_find_prop(child, "reg");
            uint64_t mpidr;
            read_reg(reg->data, cells, 0, &mpidr, NULL);

            assign_cpu_id(mpidr);
        }
    }

    extern char __stack_top;
    cpu_stacks[this_cpu()] = (uintptr_t)&__stack_top;

    struct sndry_ctx *ctxs = kmalloc(sizeof(*ctxs) * cpu_count());

    uint64_t ttbr0, ttbr1, tcr, mair;
    asm volatile("mrs %0, ttbr0_el1\n"
                 "mrs %1, ttbr1_el1\n"
                 "mrs %2, tcr_el1\n"
                 "mrs %3, mair_el1\n"
                 : "=r"(ttbr0), "=r"(ttbr1), "=r"(tcr), "=r"(mair));

    void *entry_point;
    asm volatile("ldr %0, =sndry_enter" : "=r"(entry_point));

    ctdn_latch_set(&startup_latch, cpu_count());

    for (cpu_t cpu = 0; cpu < cpu_count(); cpu++) {
        if (cpu == this_cpu()) continue;

        ctxs[cpu].percpu_base = percpu_copy();
        ctxs[cpu].stack = alloc_stack();
        cpu_stacks[cpu] = (uintptr_t) ctxs[cpu].stack;
        ctxs[cpu].ttbr1 = (void *)ttbr1;
        ctxs[cpu].ttbr0 = (void *)ttbr0;
        ctxs[cpu].tcr = tcr;
        ctxs[cpu].mair = mair;
    }

    for (cpu_t cpu = 0; cpu < cpu_count(); cpu++) {
        if (cpu == this_cpu())
            continue;

        uint64_t mpidr = get_mpidr(cpu);

        uintptr_t context = get_phys_mapping((uintptr_t)(ctxs + cpu));

        int64_t ret;
        asm volatile("ldr x0, =0xc4000003\n"
                     "mov x1, %[mpidr]\n"
                     "mov x2, %[entry_point]\n"
                     "mov x3, %[context]\n"
                     "hvc 0\n"
                     "mov %[ret], x0"

                     : [ret] "=r"(ret)
                     : [mpidr] "r"(mpidr), [entry_point] "r"(entry_point), [context] "r"(context)
                     : "x0", "x1", "x2", "x3", "memory"
        );

        if (ret < 0) {
            kprint("Failed to start cpu %u: %ld\n", cpu, ret);
        }
    }

    // WARNING: No new virtual memory mappings until this wait has completed.
    ctdn_latch_decrement(&startup_latch);
    ctdn_latch_wait(&startup_latch);

    kfree(ctxs);

    kprint("The primary cpu is %u. We have %u cpus.\n", this_cpu(), cpu_count());
}

void wfi_loop(void);

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

    uint32_t total_pages = total_size / PAGE_SIZE;

    vumap((uintptr_t)header);
    kvfree(header);

    // Now remap the full thing.

    // TODO: Make sure that we reserve the memory the fdt is occupying,
    // otherwise our physical memory allocator might allocate it.
    fdt_header = kvmalloc(total_pages, KVMALLOC_PERMANENT);

    for (uint32_t i = 0; i < total_pages; i++) {
        vmap((uintptr_t)fdt_header + i * PAGE_SIZE, (uintptr_t)fdt_header_phys + i * PAGE_SIZE,
             PROT_RSYS, MEMORY_TYPE_NON_CACHEABLE, 0);
    }

    build_rdt();
    // print_rdt();

    setup_interrupts();
    setup_sgis();

    bring_up_secondary();

    create_startup_task();

    timer_start();

    wfi_loop();
}

void platform_basic_init();

void secondary_main(void *pcpu_start) {
    platform_basic_init();
    set_percpu_start(pcpu_start);

    cpu_setup_interrupts();
    setup_sgis();

    ctdn_latch_decrement(&startup_latch);

    wfi_loop();
    // timer_start();
}

void platform_basic_init(void) { asm volatile("msr cpacr_el1, %x0" ::"r"(0x23330000)); }
