#include "macros.h"
#include "pltfrm.h"
#include "../tt.h"
#include "types.h"
#include "mem_idx.h"

extern uintptr_t kernel_brk_init;
extern char __kernel_start_phys;

void mair_el1_write(uint64_t value) { asm volatile("msr mair_el1, %0" : : "r"(value)); }

uint64_t tcr_el1_read(void) {
    uint64_t tcr;

    asm volatile("mrs %0, tcr_el1" : "=r"(tcr));

    return tcr;
}

void tcr_el1_write(uint64_t tcr) { asm volatile("msr tcr_el1, %0" : : "r"(tcr)); }

void ttbr0_el1_write(uint64_t addr) { asm volatile("msr ttbr0_el1, %0" : : "r"(addr)); }

uint64_t sctlr_el1_read(void) {
    uint64_t sctlr;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    return sctlr;
}

void sctlr_el1_write(uint64_t val) { asm volatile("msr sctlr_el1, %0" ::"r"(val)); }

#if PAGE_SIZE != 4096
#error Unimplemented
#endif

void do_idmap(void) {
    __attribute__((aligned(4096))) static uint64_t l0_table[512];
    __attribute__((aligned(4096))) static uint64_t l1_table[512];

    uint32_t indices[2];

    uint64_t addr = (uint64_t)&__kernel_start_phys;

    indices[0] = (addr >> 39) & 0x1ff;
    indices[1] = (addr >> 30) & 0x1ff;

    l0_table[indices[0]] = (uint64_t)l1_table | TABLE_DESC | AP_TABLE_NO_EL0;
    l1_table[indices[1]] =
        (addr & ~0x3fffffffULL) | BLOCK_DESC | TTE_AF | (MEM_NORMAL_IDX << TTE_MEM_ATTR_IDX_START);

    uint64_t mair_val = 0;
    mair_val |= MEM_ATTR_NORMAL << 8 * MEM_NORMAL_IDX;
    mair_val |= MEM_ATTR_DEV_STRICT << 8 * MEM_DEV_STRICT_IDX;
    mair_val |= MEM_ATTR_DEV_RELAXED << 8 * MEM_DEV_RELAXED_IDX;
    mair_val |= MEM_ATTR_NON_CACHEABLE << 8 * MEM_NON_CACHEABLE_IDX;
    mair_el1_write(mair_val);

    uint64_t tcr = tcr_el1_read();
    uint64_t mask = (1ULL << 6) - 1;
    tcr &= ~mask;

    // T0SZ
    tcr = REPLACE(tcr, TCR_T0SZ_END, TCR_T0SZ_START, 16);

    // cacheability of tables
    tcr = REPLACE(tcr, TCR_IRGN0_END, TCR_IRGN0_START, TCR_RGN_WB_RA_WA);

    // hardware access flag
    tcr |= TCR_HA;

    tcr_el1_write(tcr);

    ttbr0_el1_write((uint64_t)l0_table);

    asm volatile("tlbi vmalle1\n"
                 "dsb ish\n"
                 "isb\n");

    uint64_t sctlr = sctlr_el1_read();

    sctlr |= 0b101;

    sctlr_el1_write(sctlr);

    asm volatile("isb");
}
