#include "dt.h"
#include "die.h"
#include "endian.h"
#include "memory.h"
#include "output.h"
#include "string.h"

extern char *kernel_brk;

#define ALLOC(size)                                                                                \
    ({                                                                                             \
        char *p = kernel_brk;                                                                      \
        kernel_brk += size;                                                                        \
        (void *)p;                                                                                 \
    })

struct dt_node *build_dt(struct fdt_header *header) {
    char *begin_structs = (char *)header + from_be32(header->off_dt_struct);
    char *begin_strings = (char *)header + from_be32(header->off_dt_strings);
    int nesting = 0;

    uint32_t *wp = (uint32_t *)begin_structs;

    struct dt_node *current_node = NULL;
    struct dt_node *root_node = NULL;

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
                    root_node = node;
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

    return root_node;
}

static void print_spacing(int amount) {
    for (int i = 0; i < amount; i++) {
        kputstr("  ");
    }
}

void print_dt(struct dt_node *root, int current_depth) {
    print_spacing(current_depth);
    kprint("DTB node %s:\n", root->name);

    for (struct dt_prop *prop = root->first_prop; prop; prop = prop->next) {
        print_spacing(current_depth);
        kprint("Property %s of length %u has bytes:", prop->name, prop->data_length);

        for (int i = 0; i < prop->data_length; i++) {
            kprint(" 0x%x", prop->data[i]);
        }

        kputch('\n');
    }

    for (struct dt_node *child = root->first_child; child; child = child->next_sibling) {
        print_dt(child, current_depth + 1);
    }
}
