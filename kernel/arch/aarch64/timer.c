#include "timer.h"

#define SYS_REG_READ64(reg) ({ uint64_t value; asm volatile("mrs %0, " #reg :"=r"(value)); value; })
#define SYS_REG_WRITE64(reg, value) do { asm volatile("msr " #reg ", %0" :: "r"(value)); } while (0)

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

void timer_enable(void) {
    cntp_ctl_el0_write(cntp_ctl_el0_read() | 1);
}

void timer_disable(void) {
    cntp_ctl_el0_write(cntp_ctl_el0_read() & ~1ull);
}

void timer_set_imask(void) {
    cntp_ctl_el0_write(cntp_ctl_el0_read() | 2);
}

void timer_clear_imask(void) {
    cntp_ctl_el0_write(cntp_ctl_el0_read() & ~2ull);
}

void timer_set_counter(uint32_t val) {
    cntp_tval_el0_write(cntp_tval_el0_read() & ~0xffffffffull | val);
}

uint32_t timer_get_counter(void) {
    return cntp_tval_el0_read() & 0xffffffff;
}

uint32_t timer_gethz(void) {
    uint64_t cntfrq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(cntfrq));

    return cntfrq & 0xffffffff;
}

static void set_tval(int32_t value) {
    cntp_tval_el0_write(cntp_tval_el0_read() & ~0xffffffffull | value);
}

static int32_t get_tval(void) {
    return (int32_t) cntp_tval_el0_read();
}
