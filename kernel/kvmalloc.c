#include "kvmalloc.h"
#include "bspinlock.h"
#include "die.h"
#include "macros.h"
#include "memory.h"
#include "memory_map.h"
#include "output.h"
#include "pltfrm.h"
#include "prot.h"
#include "rbt.h"
#include "vmap.h"

static uintptr_t heap_start, heap_end;
static uintptr_t heap_meta_start, heap_meta_current;
static uintptr_t pheap;
static bspinlock_t vmalloc_lock;
static struct rb_node *root_node;

// TODO: Abstract this nonsense into an rbt user.
struct va_range {
    uintptr_t base, size;
};

struct vma_node {
    struct rb_node rb_node;
    struct va_range range;
};

#define NUM_BITFIELDS ((PAGE_SIZE / sizeof(struct vma_node) + 63) / 64)
#define NUM_NODES ((PAGE_SIZE - 8 * NUM_BITFIELDS) / sizeof(struct vma_node))

struct slab {
    // some ceiling division to figure out the max bits needed to store a complete page of vma_node
    // objects. free/allocated bits correspond as follows: bitfields[n].i -> nodes[n * 64 + i]'s f/a
    // (0/1) bit
    uint64_t bitfields[NUM_BITFIELDS];
    // as many nodes as can fit in the rest of the page.
    struct vma_node nodes[];
};

static struct slab *bump_meta(void) {
    uintptr_t old = heap_meta_current;

    struct slab *slab = (struct slab *)old;
    heap_meta_current += PAGE_SIZE;

    uintptr_t pa;
    int f;
    if ((f = global_acquire_pages(1, &pa, NULL)) == -1) {
        KFATAL("global_acquire_pages: %d\n", f);
    }

    if ((f = vmap(old, pa, PROT_RSYS | PROT_WSYS, MEMORY_TYPE_NORMAL, 0)) == -1) {
        KFATAL("vmap: %d\n", f);
    }

    clear_memory(slab->bitfields, sizeof(slab->bitfields));

    return slab;
}

static struct vma_node *allocate_from_slab(struct slab *slab) {
    for (uint32_t j = 0; j < NUM_NODES; j++) {
        uint32_t major = j / 64;
        uint32_t minor = j % 64;

        uint64_t mask = (uint64_t)1 << minor;
        if (!(slab->bitfields[major] & mask)) {
            // free, so
            // make allocated
            slab->bitfields[major] |= mask;
            return slab->nodes + j;
        }
    }

    return NULL;
}

static struct vma_node *allocate_node(void) {
    for (uintptr_t x = heap_meta_start; x < heap_meta_current; x += PAGE_SIZE) {
        struct slab *slab = (struct slab *)x;
        struct vma_node *node = allocate_from_slab(slab);
        if (node)
            return node;
    }

    // TODO: Check that we haven't exceeded the metadata restriction (KERNEL_HEAP_BEGIN)

    struct slab *new_slab = bump_meta();
    return allocate_from_slab(new_slab);
}

static void free_node(struct vma_node *node) {
    uintptr_t addr = (uintptr_t)node;
    uintptr_t page = (addr / PAGE_SIZE) * PAGE_SIZE;
    struct slab *slab = (struct slab *)page;

    size_t index = (addr - (uintptr_t)slab->nodes) / sizeof(struct vma_node);

    size_t major = index / 64;
    size_t minor = index % 64;

    slab->bitfields[major] &= ~((uint64_t)1 << minor);
}

void vma_tree_insert(struct rb_node **root, struct vma_node *node) {
    struct rb_node **current = root, *parent = NULL;

    while (*current) {
        parent = *current;
        struct vma_node *vn = CONTAINER_OF(*current, struct vma_node, rb_node);
        struct va_range *mine, *of_vn;
        mine = &node->range;
        of_vn = &vn->range;

        if (mine->base + mine->size < of_vn->base) {
            current = &(*current)->left;
        } else if (mine->base >= of_vn->base + of_vn->size) {
            current = &(*current)->right;
        } else {
            KFATAL("Unexpected scenario. Overlapping regions in vma_tree_insert\n");
        }
    }

    rb_link_node(&node->rb_node, parent, current);
    rb_insert_color(&node->rb_node, root);
}

struct vma_node *vma_tree_search_for_container(struct rb_node *root, uintptr_t address) {
    struct rb_node *current = root;

    while (current) {
        struct vma_node *vn = CONTAINER_OF(current, struct vma_node, rb_node);

        if (address < vn->range.base) {
            current = current->left;
        } else if (address >= vn->range.base + vn->range.size) {
            current = current->right;
        } else {
            return vn;
        }
    }

    return NULL;
}

void vma_tree_del(struct rb_node **root, struct vma_node *node) {
    if (!node)
        return;

    rb_del(&node->rb_node, root);
}

static void return_pages(uintptr_t address) {
    struct vma_node *node = vma_tree_search_for_container(root_node, address);
    if (!node) {
        KFATAL("Returning non-allocated virtual memory area.\n");
    }

    vma_tree_del(&root_node, node);
}

int find_free_space_visit(struct rb_node *node, uintptr_t start, size_t bytes,
                          uintptr_t *new_start) {
    struct vma_node *vn = CONTAINER_OF(node, struct vma_node, rb_node);

    if (start + bytes <= vn->range.base) {
        return 1;
    }

    *new_start = vn->range.base + vn->range.size;
    return 0;
}

int inorder_traverse_find_free_space(struct rb_node *root, size_t bytes, uintptr_t *out) {
    struct rb_node *prev = NULL;
    struct rb_node *curr = root;

    uintptr_t start = KERNEL_HEAP_BEGIN;

    while (curr != NULL) {
        struct rb_node *next;

        if (prev == get_parent(curr)) {
            // Descending from parent
            if (curr->left) {
                next = curr->left; // go left
            } else {
                if (find_free_space_visit(curr, start, bytes, &start)) {
                    *out = start;
                    return 1;
                }

                next = (curr->right) ? curr->right : get_parent(curr); // go right or up
            }

        } else if (prev == curr->left) {
            // Coming up from left child
            if (find_free_space_visit(curr, start, bytes, &start)) {
                *out = start;
                return 1;
            }

            next = (curr->right) ? curr->right : get_parent(curr);

        } else {
            // Coming up from right child
            next = get_parent(curr); // backtrack
        }

        prev = curr;
        curr = next;
    }

    if (start + bytes < KERNEL_HEAP_END) {
        *out = start;
        return 1;
    }

    return 0;
}

static void *request_pages(size_t pages) {
    size_t bytes = pages * PAGE_SIZE;

    uintptr_t out;
    if (inorder_traverse_find_free_space(root_node, bytes, &out)) {
        struct vma_node *node = allocate_node();
        node->range.base = out;
        node->range.size = bytes;

        vma_tree_insert(&root_node, node);

        return (void *)out;
    }

    return NULL;
}

void kvmalloc_init(void) {
    heap_start = KERNEL_HEAP_BEGIN;
    heap_end = KERNEL_HEAP_END;

    pheap = KERNEL_PERMANENT_HEAP_BEGIN;

    heap_meta_current = heap_meta_start = KERNEL_HEAP_META_BEGIN;
}

void *kvmalloc(size_t pages, int flags) {
    bspinlock_lock(&vmalloc_lock);
    void *ret;
    if (!(flags & KVMALLOC_PERMANENT)) {
        ret = request_pages(pages);
    } else {
        void *ptr = (void *)pheap;
        pheap += pages * PAGE_SIZE;

        ret = ptr;
    }

    bspinlock_unlock(&vmalloc_lock);
    return ret;
}
