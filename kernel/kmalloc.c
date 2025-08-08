#include "kmalloc.h"
#include "gpa.h"
#include "bspinlock.h"

static gpa_t default_allocator = GPA_DEF_INIT;
static bspinlock_t lock;

void *kmalloc2(size_t size, int flags) {
    bspinlock_lock(&lock);
    void *ptr = gpa_alloc(&default_allocator, size);
    bspinlock_unlock(&lock);

    // TODO: tlb shootdown
    return ptr;
}

void *kmalloc(size_t size) {
    return kmalloc2(size, 0);
}

void *krealloc(void *ptr, size_t new_size) {
    bspinlock_lock(&lock);
    void *ret = gpa_realloc(&default_allocator, ptr, new_size);
    bspinlock_unlock(&lock);
    return ret;
}

void kfree(void *ptr) {
    if (!ptr) return;
    
    bspinlock_lock(&lock);
    gpa_free(&default_allocator, ptr);
    bspinlock_unlock(&lock);
}
