#include "memory.h"

static void copy_memory_slow(void *dst, const void *src, size_t size);
static void clear_memory_slow(void *dst, size_t size);
static void set_memory_slow(void *dst, int value, size_t size);

void copy_memory(void *dst, const void *src, size_t size) {
    copy_memory_slow(dst, src, size);
}

void clear_memory(void *dst, size_t size) {
    clear_memory_slow(dst, size);
}

void set_memory(void *dst, int value, size_t size) {
    set_memory_slow(dst, value, size);
}

static void copy_memory_slow(void *dst, const void *src, size_t size) {
    char *p = dst;
    const char *s = src;

    for (size_t i = 0; i < size; i++) {
        p[i] = s[i];
    }
}

static void clear_memory_slow(void *dst, size_t size) {
    char *p = dst;
    for (size_t i = 0; i < size; i++) {
        p[i] = 0;
    }
}

static void set_memory_slow(void *dst, int value, size_t size) {
    char *p = dst;

    for (size_t i = 0; i < size; i++) {
        p[i] = 0;
    }
}

uint64_t mmio_read64(uintptr_t ptr) {
    return *(volatile uint64_t *)ptr;
}

void mmio_write64(uintptr_t ptr, uint64_t val) {
    *(volatile uint64_t *)ptr = val;
}

uint32_t mmio_read32(uintptr_t ptr) {
    return *(volatile uint32_t *)ptr;
}

void mmio_write32(uintptr_t ptr, uint32_t val) {
    *(volatile uint32_t *)ptr = val;
}
