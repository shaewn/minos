#ifndef KERNEL_GPA_H_
#define KERNEL_GPA_H_

#include "types.h"

#define GPA_INITIALIZER(user, acq, rel)                                        \
    { user, acq, rel, NULL, NULL, NULL, NULL }
#define GPA_DEF_INIT                                                           \
    { NULL, def_gpa_acquire, def_gpa_release, NULL, NULL, NULL, NULL }

typedef union block_header {
    struct {
        size_t metadata;
        union block_header *next, *prev;
    } free_block;
    struct {
        size_t metadata;
        size_t canary;
    } alloc_block;
} block_header_t;

typedef struct heap_region_header {
    size_t size;
    struct heap_region_header *next;
} heap_region_header_t;

typedef struct gpa {
    void *user;
    heap_region_header_t *(*acquire)(void *user, size_t size);
    void (*release)(void *user, heap_region_header_t *region);

    heap_region_header_t *first_region;
    block_header_t *first, *next, *last;
} gpa_t;

void gpa_init(gpa_t *gpa, void *user,
              heap_region_header_t *(*acquire)(void *user, size_t size),
              void (*release)(void *user, heap_region_header_t *region));
void gpa_deinit(gpa_t *gpa);

void *gpa_alloc(gpa_t *gpa, size_t size);
void gpa_free(gpa_t *gpa, void *ptr);
void *gpa_realloc(gpa_t *gpa, void *ptr, size_t new_data_size);

heap_region_header_t *def_gpa_acquire(void *user, size_t alloc_size);
void def_gpa_release(void *user, heap_region_header_t *region);

#endif
