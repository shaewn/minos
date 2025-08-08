#include "die.h"
#include "fdt.h"
#include "gic.h"
#include "kmalloc.h"
#include "kvmalloc.h"
#include "macros.h"
#include "memory.h"
#include "output.h"
#include "pltfrm.h"
#include "prot.h"
#include "string.h"
#include "vmap.h"

extern struct fdt_header *fdt_header_phys;
struct fdt_header *fdt_header;
struct dt_node *rdt_root;

uintptr_t gicd_base_ptr, gicr_base_ptr;

struct phand_ent {
    struct phand_ent *next;
    struct dt_node *node;
    uint32_t phandle;
};
#define PHANDTAB_SIZE 128
struct phand_ent *phandle_table[PHANDTAB_SIZE];

void phandtab_insert(struct dt_node *node, uint32_t phandle) {
    uint32_t index = phandle % PHANDTAB_SIZE;

    struct phand_ent **ent = &phandle_table[index];

    struct phand_ent *new_ent = kmalloc(sizeof(*new_ent));
    new_ent->next = *ent;
    new_ent->node = node;
    new_ent->phandle = phandle;

    *ent = new_ent;
}

struct dt_node *phandtab_get(uint32_t phandle) {
    uint32_t index = phandle % PHANDTAB_SIZE;

    struct phand_ent *ent = phandle_table[index];

    while (ent) {
        if (ent->phandle == phandle) {
            return ent->node;
        }

        ent = ent->next;
    }

    return NULL;
}

static uint32_t from_be32(uint32_t data) {
    data = (data & 0xffff) << 16 | (data >> 16) & 0xffff;
    data = (data & 0x00ff00ff) << 8 | (data >> 8) & 0x00ff00ff;
    return data;
}

static uint32_t read_cell(struct dt_prop *prop) {
    return from_be32(*(uint32_t *)prop->data);
}

static struct dt_node *read_phandle(struct dt_prop *prop) {
    return phandtab_get(read_cell(prop));
}

#define DTN_FOREACH(node, var)                                                 \
    for (struct dt_node *var = (node)->first_child; var;                       \
         var = var->next_sibling)
#define DTP_FOREACH(node, var)                                                 \
    for (struct dt_prop *var = (node)->first_prop; var; var = var->next)

// recursive device tree
void build_rdt(void) {
    struct dt_node *current = NULL;

    const uint32_t *wp =
        (uint32_t *)((char *)fdt_header + from_be32(fdt_header->off_dt_struct));
    const char *strs =
        (const char *)fdt_header + from_be32(fdt_header->off_dt_strings);

    int nest = 0;

    do {
        switch (from_be32(*wp++)) {
        case FDT_BEGIN_NODE: {
            struct dt_node *new_node = kmalloc(sizeof(*new_node));
            clear_memory(new_node, sizeof(*new_node));

            new_node->name = (const char *)wp;
            new_node->parent = current;

            uint32_t advance = (string_len(new_node->name) + 1 + 3) >> 2;

            wp += advance;

            if (current) {
                if (current->last_child) {
                    current->last_child->next_sibling = new_node;
                } else {
                    current->first_child = new_node;
                }

                current->last_child = new_node;
            } else {
                rdt_root = new_node;
            }

            current = new_node;

            ++nest;

            break;
        }

        case FDT_END_NODE: {
            --nest;

            current = current->parent;
            break;
        }

        case FDT_PROP: {
            uint32_t len = from_be32(*wp++);
            uint32_t nameoff = from_be32(*wp++);

            struct dt_prop *prop = kmalloc(sizeof(*prop));
            prop->data = (const char *)wp;
            prop->name = strs + nameoff;
            prop->data_length = len;

            wp += (len + 3) >> 2;

            if (current->last_prop) {
                current->last_prop->next = prop;
            } else {
                current->first_prop = prop;
            }

            current->last_prop = prop;

            if (string_compare(prop->name, "phandle") == 0) {
                uint32_t value = from_be32(*(uint32_t *)prop->data);
                phandtab_insert(current, value);
            }

            break;
        }

        case FDT_NOP: {
            break;
        }
        }
    } while (nest);
}

static struct dt_prop *find_prop(struct dt_node *node, const char *name) {
    if (!node)
        return NULL;
    DTP_FOREACH(node, prop) {
        if (string_compare(prop->name, name) == 0) {
            return prop;
        }
    }

    return NULL;
}

static struct dt_node *find_child(struct dt_node *node, const char *prefix) {
    DTN_FOREACH(node, child) {
        if (string_begins(child->name, prefix)) {
            return child;
        }
    }

    return NULL;
}

static void find_parent_ac_sc(struct dt_node *node, uint32_t *ac,
                              uint32_t *sc) {
    struct dt_prop *acp = find_prop(node->parent, "#address-cells");
    struct dt_prop *scp = find_prop(node->parent, "#size-cells");

    if (acp) {
        *ac = from_be32(*(uint32_t *)acp->data);
    } else {
        *ac = 2;
    }

    if (scp) {
        *sc = from_be32(*(uint32_t *)scp->data);
    } else {
        *sc = 1;
    }
}

void putspace(int amount) {
    for (int i = 0; i < amount; i++) {
        kputstr("    ");
    }
}

void print_stringlist(struct dt_node *node, struct dt_prop *prop, int depth) {
    putspace(depth);
    const char *d = prop->data;
    kprint("%s: '%s'", prop->name, d);

    d += string_len(d) + 1;
    while (d < prop->data + prop->data_length) {
        kprint(", '%s'", d);
        d += string_len(d) + 1;
    }

    kputstr("\n");
}

void print_reg(struct dt_node *node, struct dt_prop *prop, int depth) {
    putspace(depth);
    kprint("reg:\n");
    // If we have a reg field, our parent has #address-cells and #size-cells,
    // because they are not inherited.

    uint32_t address_cells = 2;
    uint32_t size_cells = 1;

    struct dt_prop *acp, *scp;
    acp = find_prop(node->parent, "#address-cells");
    scp = find_prop(node->parent, "#size-cells");

    if (acp) {
        address_cells = from_be32(*(uint32_t *)acp->data);
    }

    if (scp) {
        size_cells = from_be32(*(uint32_t *)scp->data);
    }

    const uint32_t *d = (const uint32_t *)prop->data;

    while ((uintptr_t)d < (uintptr_t)prop->data + prop->data_length) {
        uint64_t addr, size;

        addr = from_be32(*d++);

        if (address_cells == 2) {
            addr <<= 32;
            addr |= from_be32(*d++);
        }

        putspace(depth + 1);
        kprint("addr: 0x%lx", addr);

        if (size_cells) {
            size = from_be32(*d++);

            if (size_cells == 2) {
                size <<= 32;
                size |= from_be32(*d++);
            }

            kprint(", size: 0x%lx\n", size);
        } else {
            kputstr("\n");
        }
    }
}

void print_phandle(struct dt_node *node, struct dt_prop *prop, int depth) {
    putspace(depth);
    uint32_t handle = from_be32(*(uint32_t *)prop->data);
    kprint("phandle: %u\n", handle);
}

void print_interrupt_parent(struct dt_node *node, struct dt_prop *prop,
                            int depth) {
    putspace(depth);
    uint32_t handle = from_be32(*(uint32_t *)prop->data);
    const char *name = phandtab_get(handle)->name;
    kprint("interrupt-parent: &%s\n", name);
}

void print_cells(struct dt_node *node, struct dt_prop *prop, int depth) {
    putspace(depth);
    uint32_t value = from_be32(*(uint32_t *)prop->data);
    kprint("%s: %u\n", prop->name, value);
}

void print_string(struct dt_node *node, struct dt_prop *prop, int depth) {
    putspace(depth);
    kprint("%s: %s\n", prop->name, prop->data);
}

void print_interrupts(struct dt_node *node, struct dt_prop *prop, int depth) {
    struct dt_prop *interrupt_parent = find_prop(node, "interrupt-parent");
    if (!interrupt_parent)
        interrupt_parent = find_prop(rdt_root, "interrupt-parent");
    struct dt_node *intp = read_phandle(interrupt_parent);
    if (!intp)
        KFATAL("No interrupt parent found by phandle\n");
    struct dt_prop *icellsp = find_prop(intp, "#interrupt-cells");
    if (!icellsp)
        KFATAL("No #interrupt-cells\n");
    uint32_t icells = read_cell(icellsp);

    const uint32_t *ptr = (const uint32_t *)prop->data;

    putspace(depth);
    kprint("interrupts:");

    while ((uintptr_t)ptr < (uintptr_t)prop->data + prop->data_length) {
        kputstr(" (");
        for (int i = 0; i < icells; i++) {
            uint32_t val = from_be32(ptr[i]);
            kprint(" %u", val);
        }
        kputstr(" )");
        ptr += icells;
    }

    kputstr("\n");
}

void print_special(struct dt_node *node, struct dt_prop *prop, int depth) {
    const char *special_names[] = {"compatible",       "reg",
                                   "phandle",          "interrupt-parent",
                                   "#address-cells",   "#size-cells",
                                   "#interrupt-cells", "model",
                                   "serial-number",    "chassis-type",
                                   "bootargs",         "stdout-path",
                                   "stdin-path",       "enable-method",
                                   "interrupts"};

    void (*funcs[])(struct dt_node *node, struct dt_prop *prop, int depth) = {
        print_stringlist, print_reg,    print_phandle,   print_interrupt_parent,
        print_cells,      print_cells,  print_cells,     print_string,
        print_string,     print_string, print_string,    print_string,
        print_string,     print_string, print_interrupts};

    for (int i = 0; i < ARRAY_LEN(special_names); i++) {
        if (string_compare(special_names[i], prop->name) == 0) {
            funcs[i](node, prop, depth);
            break;
        }
    }
}

void print_rdt(struct dt_node *root, int depth) {
    putspace(depth);

    kprint("RDT Node: '%s'\n", root->name);

    DTP_FOREACH(root, prop) {
        putspace(depth + 1);
        kprint("Property: '%s'\n", prop->name);

        print_special(root, prop, depth + 2);
    }

    DTN_FOREACH(root, child) { print_rdt(child, depth + 1); }
}

static int is_compatible(struct dt_node *node, const char *compat) {
    struct dt_prop *compat_prop = find_prop(node, "compatible");
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

struct dt_node *find_primary_interrupt_controller(void) {

    struct dt_prop *root_intp = find_prop(rdt_root, "interrupt-parent");

    if (root_intp) {
        return read_phandle(root_intp);
    }

    return NULL;
}

static uint32_t *read_reg(uint32_t *wp, uint32_t ac, uint32_t sc,
                          uint64_t *addr, uint64_t *size) {
    if (ac) {
        *addr = from_be32(*wp++);
        if (ac == 2) {
            *addr <<= 32;
            *addr |= from_be32(*wp++);
        }
    }

    if (sc) {
        *size = from_be32(*wp++);
        if (sc == 2) {
            *size <<= 32;
            *size |= from_be32(*wp++);
        }
    }

    return wp;
}

#define SYS_REG_READ64(reg) ({ uint64_t value; asm volatile("mrs %0, " #reg :"=r"(value)); value; })
#define SYS_REG_WRITE64(reg, value) do { asm volatile("msr " #reg ", %0" :"=r"(value)); } while (0)

static uint64_t cntp_ctl_el0_read(void) {
    return SYS_REG_READ64(cntp_ctl_el0);
}

static void cntp_ctl_el0_write(uint64_t value) {
    SYS_REG_WRITE64(cntp_ctl_el0, value);
}

static uint64_t cntp_tval_el0_read() {
    return SYS_REG_READ64(cntp_tval_el0);
}

static void cntp_tval_el0_write(uint64_t value) {
    SYS_REG_WRITE64(cntp_tval_el0, value);
}

static void set_tval(int32_t value) {
    cntp_tval_el0_write(cntp_tval_el0_read() & ~0xffffffffull | value);
}

int32_t get_tval(void) {
    return (int32_t) cntp_tval_el0_read();
}

int check_timer_condition(void) {
    uint64_t ctl = cntp_ctl_el0_read();

    return (ctl >> 2) & 1;
}

static uintptr_t get_rd_base(void);

int is_pend_sgi_or_ppi(uint32_t intid) {
    uintptr_t sgi_base = get_rd_base() + 0x10000;

    uint32_t n = 0;
    uint32_t bit = intid;

    if (intid >= 1056) {
        bit = (intid - 1024) / 32;
        bit = (intid - 1024) % 32;
    }

    uintptr_t reg = sgi_base + GICR_ISPENDR0 + n * 4;

    return (mmio_read32(reg) >> bit) & 1;
}

void setup_timer(void) {
    uint64_t cntfrq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(cntfrq));

    uint32_t freq_hz = cntfrq & 0xffffffff;

    kprint("The timer operates at %u MHz (%u Hz)\n", freq_hz / 1000000,
           freq_hz);

    int32_t tval_ticks = 1 /* second */ * freq_hz;

    asm volatile("msr daifclr, #2");

    set_tval(tval_ticks);

    cntp_ctl_el0_write(cntp_ctl_el0_read() | 1);
}

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

static void dsb_isb(void) {
    asm volatile("dsb ish\nisb\n");
}

static void isb(void) {
    asm volatile("isb");
}

static uintptr_t get_rd_base(void) {
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

    return rd_base;
}

void enable_sgi_or_ppi(uint32_t intid) {
    uintptr_t rd_base = get_rd_base();
    uintptr_t sgi_base = rd_base + (1 << 16);

    uint32_t n = 0;
    uint32_t bit = intid;

    if (intid >= 1056) {
        // extended.
        n = (intid - 1024) / 32;
        bit = (intid - 1024) % 32;
    }

    uintptr_t reg = sgi_base + GICR_ISENABLER0 + n * 4;
    mmio_write32(reg, mmio_read32(reg) | (1u << bit));
}

void cfgr_sgi_or_ppi(uint32_t intid, int edge_sensitive) {
    uintptr_t rd_base = get_rd_base();
    uintptr_t sgi_base = rd_base + (1 << 16);

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

void set_group_sgi_or_ppi(uint32_t intid, int grp) {
    // 0 or 1
    grp = !!grp;

    uint32_t n = intid >= 1056 ? intid - 1024 : intid;
    uint32_t bit = n % 32;
    n = n / 32;

    uintptr_t sgi_base = get_rd_base() + 0x10000;
    uintptr_t group_ptr = sgi_base + GICR_IGROUPR0 + n * 4;

    mmio_write32(group_ptr, (mmio_read32(group_ptr) & ~(1u << bit)) | grp << bit);
}

void setup_interrupts(void) {
    uint64_t icc_sre;
    asm volatile("mrs %0, icc_sre_el1" : "=r"(icc_sre));
    icc_sre |= 1;
    asm volatile("msr icc_sre_el1, %0" :: "r"(icc_sre));
    isb();


    struct dt_node *intc = find_primary_interrupt_controller();
    if (!intc) {
        KFATAL("Failed to identify primary interrupt controller.\n");
    }

    if (!find_prop(intc, "interrupt-controller")) {
        KFATAL("Primary interrupt controller does not have "
               "interrupt-controller property.\n");
    }

    kprint("Primary interrupt controller is &%s\n", intc->name);

    if (is_compatible(intc, "arm,gic-v3")) {
        kprint("GIC-v3 controller.\n");
    } else {
        KFATAL("Only GIC-v3 is supported.\n");
    }

    struct dt_prop *reg = find_prop(intc, "reg");

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


    struct dt_node *timer_node = find_child(rdt_root, "timer");
    if (!timer_node) {
        KFATAL("Failed to find timer node\n");
    }

    struct dt_prop *icellsp = find_prop(intc, "#interrupt-cells");
    if (!icellsp) {
        KFATAL("No #interrupt-cells on primary intc.\n");
    }

    uint32_t icells = read_cell(icellsp);

    struct dt_prop *timer_interrupts = find_prop(timer_node, "interrupts");
    if (!timer_interrupts) {
        KFATAL("Timer is not an interrupt-generating device.\n");
    }

    for (uint32_t *p = (uint32_t *)timer_interrupts->data; (uintptr_t)p < (uintptr_t)timer_interrupts->data + timer_interrupts->data_length; p += icells) {
        uint32_t kind = from_be32(*p);
        uint32_t intid = from_be32(p[1]) + 16;
        uint32_t cfgr = from_be32(p[2]);

        enable_sgi_or_ppi(intid);
        cfgr_sgi_or_ppi(intid, cfgr == 1);

        kprint("enabling and configuring interrupt %u\n", intid);

        set_group_sgi_or_ppi(intid, 1);
    }

    uintptr_t gicd_ctlr = gicd_base_ptr + GICD_CTLR;
    kprint("GICD_CTLR contains 0x%x\n", mmio_read32(gicd_ctlr));

    mmio_write32(gicd_ctlr, mmio_read32(gicd_ctlr) | 2);

    kprint("The GICR_CTLR contains: 0x%08x\n", mmio_read32(rd_base + GICR_CTLR));

    dsb_isb();

    asm volatile("msr daifclr, 2");

    setup_timer();
}

void platform_startup(void) {
    struct fdt_header *header = kvmalloc(1, 0);

    int r = vmap((uintptr_t)header, (uintptr_t)fdt_header_phys, PROT_RSYS,
                 MEMORY_TYPE_NON_CACHEABLE, 0);
    if (r < 0) {
        KFATAL("vmap failed: %d\n", r);
    }

    if (from_be32(header->magic) != FDT_MAGIC) {
        KFATAL("Failed reading header\n");
    }

    uint32_t total_size = from_be32(header->totalsize);
    kprint("Total size: %u\n", total_size);

    uint32_t total_pages = total_size / PAGE_SIZE;
    kprint("Total pages: %u\n", total_pages);

    vumap((uintptr_t)header);
    kvfree(header);

    // Now remap the full thing.

    fdt_header = kvmalloc(total_pages, KVMALLOC_PERMANENT);

    for (uint32_t i = 0; i < total_pages; i++) {
        vmap((uintptr_t)fdt_header + i * PAGE_SIZE,
             (uintptr_t)fdt_header_phys + i * PAGE_SIZE, PROT_RSYS,
             MEMORY_TYPE_NON_CACHEABLE, 0);
    }

    build_rdt();
    print_rdt(rdt_root, 1);
    setup_interrupts();
}
