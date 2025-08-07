#include "cpu.h"
#include "die.h"
#include "macros.h"
#include "memory.h"
#include "memory_map.h"
#include "memory_type.h"
#include "output.h"
#include "pltfrm.h"
#include "prot.h"
#include "types.h"
#include "vmap.h"
#include "kvmalloc.h"

uint64_t kernel_start, kernel_end, kernel_brk;

struct fdt_header *fdt_header_phys;

uintptr_t map_percpu(uintptr_t vbrk) {
    extern char __percpu_begin, __percpu_end;
    uintptr_t percpu_begin = (uintptr_t)&__percpu_begin;
    uintptr_t percpu_end = (uintptr_t)&__percpu_end;

    uintptr_t offset = 0;

    while (percpu_begin + offset < percpu_end) {
        int r;
        uintptr_t reg;
        r = global_acquire_pages(1, &reg, NULL);
        if (r == -1) {
            KFATAL("Failed to acquire necessary memory\n");
        }

        r = vmap(vbrk + offset, reg, PROT_RSYS | PROT_WSYS, MEMORY_TYPE_NORMAL, 0);
        if (r < 0) {
            KFATAL("vmap error: %d\n", r);
        }

        copy_memory((void *)(vbrk + offset), (void *)(percpu_begin + offset), PAGE_SIZE);

        offset += PAGE_SIZE;
    }

    return vbrk + offset;
}

void setup_timer(void) {
    uint64_t cntfrq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(cntfrq));

    uint32_t freq_hz = cntfrq & 0xffffffff;

    kprint("The timer operates at %u MHz (%u Hz)\n", freq_hz / 1000000, freq_hz);

    uint32_t tval_ticks = 1 /* second */ * freq_hz;
    uint64_t tval = tval_ticks;

    asm volatile("msr daifclr, 2");

    uint64_t daif;
    asm volatile("mrs %0, daif" : "=r"(daif));

    uint8_t daif_bits = daif >> 6;
    kprint("Daif bits are %b\n", daif_bits);

    daif_bits = 0;

    daif &= 0xf << 6;
    daif |= daif_bits << 6;

    daif = 0;

    asm volatile("msr cntp_tval_el0, %0" :: "r"(tval));

    uint64_t cntp_ctl = 1;
    asm volatile("msr cntp_ctl_el0, %0" : : "r"(cntp_ctl));

    while (1);
}

// Kernel virtual entry point (higher half)
void kvmain(uintptr_t vbrk) {
    init_print();

    reserve_active_kernel_memory();

    void *percpu_copy = (void *)vbrk;

    vbrk = map_percpu(vbrk);

    kvmalloc_init();

    void platform_startup(void);
    platform_startup();
}
