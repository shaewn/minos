#include "ctdn_latch.h"
#include "cpu.h"

void ctdn_latch_set(volatile ctdn_latch_t *latch, uint32_t value) {
    __atomic_store_n(latch, value, __ATOMIC_RELEASE);
    cpu_signal_all(latch);
}

void ctdn_latch_decrement(volatile ctdn_latch_t *latch) {
    __atomic_fetch_sub(latch, 1, __ATOMIC_RELEASE);
    cpu_signal_all(latch);
}

void ctdn_latch_increment(volatile ctdn_latch_t *latch) {
    __atomic_fetch_add(latch, 1, __ATOMIC_RELAXED);
}

void ctdn_latch_wait(volatile ctdn_latch_t *latch) {
    while (__atomic_load_n(latch, __ATOMIC_ACQUIRE) != 0)
        cpu_idle_wait(latch);
}
