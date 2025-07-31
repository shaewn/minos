#include "./dt.h"
#include "endian.h"

struct dt_node *dt_root;
extern uintptr_t kernel_brk;

#define KFATAL(...) early_die()

extern void early_die(void);

#define ALLOC(size)                                                                                \
    ({                                                                                             \
        char *p = (char *)kernel_brk;                                                              \
        kernel_brk += size;                                                                        \
        (void *)p;                                                                                 \
    })

static void clear_memory(void *dst, size_t size) {
    char *s = dst;
    for (size_t i = 0; i < size; i++) {
        s[i] = 0;
    }
}

static size_t string_len(const char *s) {
    size_t size = 0;
    while (*s++) size++;
    return size;
}

static int string_compare(const char *a, const char *b) {
    while (*a && *a == *b) ++a, ++b;
    return *a - *b;
}

void build_dt(struct fdt_header *header) {
    char *begin_structs = (char *)header + from_be32(header->off_dt_struct);
    char *begin_strings = (char *)header + from_be32(header->off_dt_strings);
    int nesting = 0;

    uint32_t *wp = (uint32_t *)begin_structs;

    struct dt_node *current_node = NULL;

    do {
        switch (from_be32(*wp++)) {
            case FDT_BEGIN_NODE: {
                ++nesting;

                struct dt_node *node = ALLOC(sizeof(*node));
                clear_memory(node, sizeof(*node));
                node->parent = current_node;
                node->name = (char *)wp;

                if (node->parent) {
                    struct dt_node *p = node->parent->last_child;
                    if (p) {
                        p->next_sibling = node;
                    } else {
                        node->parent->first_child = node;
                    }

                    node->parent->last_child = node;
                } else {
                    dt_root = node;
                }

                current_node = node;

                // skip past the name.
                wp += ((string_len(node->name) + 1) + 3) >> 2;

                break;
            }

            case FDT_END_NODE: {
                --nesting;
                current_node = current_node->parent;
                break;
            }

            case FDT_PROP: {
                uint32_t len = from_be32(*wp++);
                uint32_t nameoff = from_be32(*wp++);

                struct dt_prop *prop = ALLOC(sizeof(*prop));
                clear_memory(prop, sizeof(*prop));
                prop->data = (char *)wp;
                prop->data_length = len;
                prop->name = begin_strings + nameoff;

                if (!current_node) {
                    KFATAL("ERROR: NULL pointer\n");
                }

                struct dt_prop *p = current_node->last_prop;
                if (p) {
                    p->next = prop;
                } else {
                    current_node->first_prop = prop;
                }

                current_node->last_prop = prop;

                // skip past the data.
                wp += (len + 3) >> 2;

                break;
            }

            case FDT_NOP: {
                break;
            }
        }
    } while (nesting);
}

struct dt_node *dt_search(struct dt_node *start, const char *path) {
    if (!path) {
        KFATAL("ERROR: path is a NULL pointer\n");
    }

    struct dt_node *current = start;

    if (*path == '/') {
        current = dt_root;
        while (*path == '/')
            ++path;
    } else if (!start) {
        KFATAL("ERROR: start is a NULL pointer and the path is relative\n");
    }

    char buf[256];

    const char *p = path, *s = path;

    while (*s) {
        while (*s && *s != '/')
            ++s;

        size_t len = 0;
        while (p != s) buf[len++] = *p, ++p, ++s;
        buf[len] = 0;

        struct dt_node *child = dt_find(current, buf);
        if (!child) {
            return NULL;
        }

        while (*s == '/')
            ++s;

        p = s;
        current = child;
    }

    return current;
}

struct dt_node *dt_find(struct dt_node *parent, const char *name) {
    struct dt_node *start = parent ? parent : dt_root;
    for (struct dt_node *child = start->first_child; child; child = child->next_sibling) {
        if (string_compare(child->name, name) == 0) {
            return child;
        }
    }

    return NULL;
}

struct dt_prop *dt_findprop(struct dt_node *parent, const char *propname) {
    for (struct dt_prop *prop = parent->first_prop; prop; prop = prop->next) {
        const char *a, *b;
        a = prop->name;
        b = propname;
        while (*a && *a == *b) ++a, ++b;

        if (*a == *b) {
            return prop;
        }
    }

    return NULL;
}
