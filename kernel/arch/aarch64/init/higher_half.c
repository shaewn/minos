#include "macros.h"
#include "pltfrm.h"
#include "../tt.h"
#include "uart.h"
#include "mem_idx.h"

#if PAGE_SIZE != 4096
#error Unimplemented
#endif

#define ISOLATE_ADDR(x) ((x) & ONES_IN_RANGE(47, LOG_PAGE_SIZE))

extern uintptr_t kernel_brk_init;

static void ttbr1_el1_write(uint64_t val);

// returns a descriptor
uint64_t new_table(void) {
    uint64_t addr = kernel_brk_init;
    kernel_brk_init += PAGE_SIZE;

    return addr | TABLE_DESC | TTE_AF;
}

void init_table(uint64_t addr, uint64_t count, uint64_t flags) {
    for (uint64_t i = 0; i < count; i++) {
        uint64_t *entry = (uint64_t *)addr + i;
        *entry = flags;
    }
}

int add_page(uint64_t *base_table, uint64_t *indices, int nlevels, uint64_t pa, uint64_t flags) {
    uint64_t *parent_table = base_table;
    int created = 0;
    int i;

    for (i = 0; i < nlevels - 1; i++) {
        uint64_t index = indices[i];
        uint64_t descriptor = parent_table[index];

        if (!(descriptor & 1)) {
            descriptor |= new_table();
            init_table(ISOLATE_ADDR(descriptor), PAGE_SIZE / sizeof(uint64_t), 0);
            created++;

            parent_table[index] = descriptor;
        }

        uint64_t next_addr = ISOLATE_ADDR(descriptor);
        parent_table = (uint64_t *)next_addr;
    }

    uint64_t *page_ent = parent_table + indices[i];

    if (!(*page_ent & 1)) {
        // if the page is invalid, create the mapping.
        *page_ent = ISOLATE_ADDR(pa) | PAGE_DESC | TTE_AF | flags;
    }

    return created;
}

static void retrieve_indices(uint64_t addr, uint64_t *indices) {
    indices[0] = EXTRACT(addr, 47, 39);
    indices[1] = EXTRACT(addr, 38, 30);
    indices[2] = EXTRACT(addr, 29, 21);
    indices[3] = EXTRACT(addr, 20, 12);
}

extern char __init_end, __readonly_begin_phys, __readonly_end_phys;

void set_control_registers(void) {
    uint64_t tcr;
    asm volatile("mrs %[tcr], tcr_el1" : [tcr] "=r"(tcr));

    tcr = REPLACE(tcr, TCR_IRGN1_END, TCR_IRGN1_START, TCR_RGN_WB_RA_WA);
    tcr = REPLACE(tcr, TCR_T1SZ_END, TCR_T1SZ_START, 16);
    tcr = REPLACE(tcr, TCR_SH1_END, TCR_SH1_START, TCR_SH_INNER_SHAREABLE);

    asm volatile("msr tcr_el1, %[tcr]\n"
                 "tlbi vmalle1\n"
                 "dsb ish\n"
                 "isb\n"
                 :
                 : [tcr] "r"(tcr));
}

void map_higher_half(void) {
    kernel_brk_init += PAGE_SIZE - 1;
    kernel_brk_init &= ~(uint64_t)(PAGE_SIZE - 1);

    uint64_t scan_begin = kernel_brk_init;

    uint64_t root_table = ISOLATE_ADDR(new_table());

    // WARNING: Page size specific.
    init_table(root_table, 512, AP_TABLE_NO_EL0);

    uint64_t ex_beg = (uint64_t)&__readonly_begin_phys;
    uint64_t ex_end = (uint64_t)&__readonly_end_phys;

    uint64_t addr = ex_beg;
    uint64_t virt = 0xffff000000000000;
    uint64_t indices[4];

    while (addr < scan_begin) {
        // 0xffff000000002de0
        uint64_t flags = (MEM_NORMAL_IDX << TTE_MEM_ATTR_IDX_START);
        if (ex_beg <= addr && addr < ex_end) {
            flags |= AP_RDONLY_PRIV;
        } else {
            flags |= AP_RDWR_PRIV | BLOCK_ATTR_PXN;
        }

        retrieve_indices(virt, indices);
        add_page((uint64_t *)root_table, indices, ARRAY_LEN(indices), addr, flags);

        addr += PAGE_SIZE;
        virt += PAGE_SIZE;
    }

    retrieve_indices(UART_ADDR, indices);
    add_page((uint64_t *)root_table, indices, ARRAY_LEN(indices), UART_PHYS_ADDR, MEM_DEV_STRICT_IDX << TTE_MEM_ATTR_IDX_START);

    ((uint64_t *)root_table)[RECURSIVE_INDEX] = root_table | TABLE_DESC | AP_TABLE_NO_EL0;

    /*
    addr = scan_begin;

    while (addr < kernel_brk_init) {
        extern void early_die(void);
        if (addr >= 0x800000000000) early_die();
        virt = 0xffff800000000000 | addr;

        uint32_t flags = MEM_ATTR_IDX_NORMAL | AP_RDWR_PRIV;

        retrieve_indices(virt, indices);
        add_page((uint64_t *)root_table, indices, ARRAY_LEN(indices), addr, flags);

        addr += PAGE_SIZE;
    }

    */

    ttbr1_el1_write(root_table);
    set_control_registers();
}

static void ttbr1_el1_write(uint64_t val) { asm volatile("msr ttbr1_el1, %0" ::"r"(val)); }
