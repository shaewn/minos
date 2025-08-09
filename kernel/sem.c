#include "sem.h"
#include "cpu.h"

void sem_init(sem_t *sem, uint32_t value) {
    __atomic_store_n(sem, value, __ATOMIC_RELEASE);
}

void sem_post(sem_t *sem) {
    __atomic_fetch_add(sem, 1, __ATOMIC_RELEASE);
    cpu_signal_all();
}

void sem_wait(sem_t *sem) {
    uint32_t expected;
    while (1) {
        uint32_t val;
        while ((val = __atomic_load_n(sem, __ATOMIC_RELAXED)) == 0)
            cpu_idle_wait();

        uint32_t new_val = val - 1;

        if (__atomic_compare_exchange_n(sem, &val, new_val, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            break;
        }
    }
}
