#include "kmalloc.h"
#include "gpa.h"
#include "spinlock.h"

static gpa_t default_allocator = GPA_DEF_INIT;
static volatile spinlock_t lock;

void *kmalloc2(size_t size, int flags) {
    spin_lock_irq_save(&lock);
    void *ptr = gpa_alloc(&default_allocator, size);
    spin_unlock_irq_restore(&lock);

    // TODO: tlb shootdown
    return ptr;
}

void *kmalloc(size_t size) {
    return kmalloc2(size, 0);
}

void *krealloc(void *ptr, size_t new_size) {
    spin_lock_irq_save(&lock);
    void *ret = gpa_realloc(&default_allocator, ptr, new_size);
    spin_unlock_irq_restore(&lock);
    return ret;
}

void kfree(void *ptr) {
    if (!ptr) return;
    
    spin_lock_irq_save(&lock);
    gpa_free(&default_allocator, ptr);
    spin_unlock_irq_restore(&lock);
}
