#include "macros.h"
#include "pltfrm.h"
#include "../tt.h"
#include "types.h"

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
        (addr & ~0x3fffffffULL) | BLOCK_DESC | /* TTE_AF | */ MEM_ATTR_IDX_NORMAL;

    mair_el1_write(0x00ff);

    uint64_t tcr = tcr_el1_read();
    uint64_t mask = (1ULL << 6) - 1;
    tcr &= ~mask;

    // T0SZ
    tcr |= 16ULL;

    // cacheability of tables
    tcr |= 0b11ULL << 8;

    // hardware access flag
    tcr |= 1ULL << 39;

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
