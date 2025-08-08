#include "rdt.h"
#include "die.h"
#include "fdt.h"
#include "kmalloc.h"
#include "memory.h"
#include "output.h"
#include "string.h"
#include "phandle_table.h"

extern struct fdt_header *fdt_header;
struct rdt_node *rdt_root;

// recursive device tree
void build_rdt(void) {
    struct rdt_node *current = NULL;

    const uint32_t *wp = (uint32_t *)((char *)fdt_header + FROM_BE_32(fdt_header->off_dt_struct));
    const char *strs = (const char *)fdt_header + FROM_BE_32(fdt_header->off_dt_strings);

    int nest = 0;

    do {
        switch (FROM_BE_32(*wp++)) {
            case FDT_BEGIN_NODE: {
                struct rdt_node *new_node = kmalloc(sizeof(*new_node));
                clear_memory(new_node, sizeof(*new_node));
                list_init(&new_node->child_list);
                list_init(&new_node->prop_list);

                new_node->name = (const char *)wp;
                new_node->parent = current;

                uint32_t advance = (string_len(new_node->name) + 1 + 3) >> 2;

                wp += advance;

                if (current) {
                    list_add_tail(&new_node->node, &current->child_list);
                } else {
                    rdt_root = new_node;
                }

                current = new_node;

                ++nest;

                break;
            }

            case FDT_END_NODE: {
                --nest;

                current = current->parent;
                break;
            }

            case FDT_PROP: {
                uint32_t len = FROM_BE_32(*wp++);
                uint32_t nameoff = FROM_BE_32(*wp++);

                struct rdt_prop *prop = kmalloc(sizeof(*prop));
                prop->data = (const char *)wp;
                prop->name = strs + nameoff;
                prop->data_length = len;

                wp += (len + 3) >> 2;

                list_add_tail(&prop->node, &current->prop_list);

                if (string_compare(prop->name, "phandle") == 0) {
                    uint32_t value = FROM_BE_32(*(uint32_t *)prop->data);
                    phandle_table_insert(current, value);
                }

                break;
            }

            case FDT_NOP: {
                break;
            }
        }
    } while (nest);
}

struct rdt_prop *rdt_find_prop(struct rdt_node *node, const char *name) {
    if (!node)
        return NULL;

    LIST_FOREACH(&node->prop_list, pnode) {
        struct rdt_prop *prop = CONTAINER_OF(pnode, struct rdt_prop, node);
        if (string_compare(prop->name, name) == 0) {
            return prop;
        }
    }

    return NULL;
}

struct rdt_node *rdt_find_child(struct rdt_node *node, const char *prefix) {
    LIST_FOREACH(&node->child_list, cnode) {
        struct rdt_node *child = CONTAINER_OF(cnode, struct rdt_node, node);
        if (string_begins(child->name, prefix)) {
            return child;
        }
    }

    return NULL;
}

void putspace(int amount) {
    for (int i = 0; i < amount; i++) {
        kputstr("    ");
    }
}

void print_stringlist(struct rdt_node *node, struct rdt_prop *prop, int depth) {
    putspace(depth);
    const char *d = prop->data;
    kprint("%s: '%s'", prop->name, d);

    d += string_len(d) + 1;
    while (d < (char *)prop->data + prop->data_length) {
        kprint(", '%s'", d);
        d += string_len(d) + 1;
    }

    kputstr("\n");
}

void print_reg(struct rdt_node *node, struct rdt_prop *prop, int depth) {
    putspace(depth);
    kprint("reg:\n");
    // If we have a reg field, our parent has #address-cells and #size-cells,
    // because they are not inherited.

    uint32_t address_cells = 2;
    uint32_t size_cells = 1;

    struct rdt_prop *acp, *scp;
    acp = rdt_find_prop(node->parent, "#address-cells");
    scp = rdt_find_prop(node->parent, "#size-cells");

    if (acp) {
        address_cells = FROM_BE_32(*(uint32_t *)acp->data);
    }

    if (scp) {
        size_cells = FROM_BE_32(*(uint32_t *)scp->data);
    }

    const uint32_t *d = (const uint32_t *)prop->data;

    while ((uintptr_t)d < (uintptr_t)prop->data + prop->data_length) {
        uint64_t addr, size;

        addr = FROM_BE_32(*d++);

        if (address_cells == 2) {
            addr <<= 32;
            addr |= FROM_BE_32(*d++);
        }

        putspace(depth + 1);
        kprint("addr: 0x%lx", addr);

        if (size_cells) {
            size = FROM_BE_32(*d++);

            if (size_cells == 2) {
                size <<= 32;
                size |= FROM_BE_32(*d++);
            }

            kprint(", size: 0x%lx\n", size);
        } else {
            kputstr("\n");
        }
    }
}

void print_phandle(struct rdt_node *node, struct rdt_prop *prop, int depth) {
    putspace(depth);
    uint32_t handle = FROM_BE_32(*(uint32_t *)prop->data);
    kprint("phandle: %u\n", handle);
}

void print_interrupt_parent(struct rdt_node *node, struct rdt_prop *prop, int depth) {
    putspace(depth);
    uint32_t handle = FROM_BE_32(*(uint32_t *)prop->data);
    const char *name = phandle_table_get(handle)->name;
    kprint("interrupt-parent: &%s\n", name);
}

void print_cells(struct rdt_node *node, struct rdt_prop *prop, int depth) {
    putspace(depth);
    uint32_t value = FROM_BE_32(*(uint32_t *)prop->data);
    kprint("%s: %u\n", prop->name, value);
}

void print_string(struct rdt_node *node, struct rdt_prop *prop, int depth) {
    putspace(depth);
    kprint("%s: %s\n", prop->name, prop->data);
}

void print_interrupts(struct rdt_node *node, struct rdt_prop *prop, int depth) {
    struct rdt_prop *interrupt_parent = rdt_find_prop(node, "interrupt-parent");
    if (!interrupt_parent)
        interrupt_parent = rdt_find_prop(rdt_root, "interrupt-parent");
    struct rdt_node *intp = phandle_table_get(read_cell(interrupt_parent));
    if (!intp)
        KFATAL("No interrupt parent found by phandle\n");
    struct rdt_prop *icellsp = rdt_find_prop(intp, "#interrupt-cells");
    if (!icellsp)
        KFATAL("No #interrupt-cells\n");
    uint32_t icells = read_cell(icellsp);

    const uint32_t *ptr = (const uint32_t *)prop->data;

    putspace(depth);
    kprint("interrupts:");

    while ((uintptr_t)ptr < (uintptr_t)prop->data + prop->data_length) {
        kputstr(" (");
        for (int i = 0; i < icells; i++) {
            uint32_t val = FROM_BE_32(ptr[i]);
            kprint(" %u", val);
        }
        kputstr(" )");
        ptr += icells;
    }

    kputstr("\n");
}

void print_special(struct rdt_node *node, struct rdt_prop *prop, int depth) {
    const char *special_names[] = {
        "compatible",     "reg",           "phandle",          "interrupt-parent",
        "#address-cells", "#size-cells",   "#interrupt-cells", "model",
        "serial-number",  "chassis-type",  "bootargs",         "stdout-path",
        "stdin-path",     "enable-method", "interrupts"};

    void (*funcs[])(struct rdt_node *node, struct rdt_prop *prop, int depth) = {
        print_stringlist, print_reg,    print_phandle, print_interrupt_parent, print_cells,
        print_cells,      print_cells,  print_string,  print_string,           print_string,
        print_string,     print_string, print_string,  print_string,           print_interrupts};

    for (int i = 0; i < ARRAY_LEN(special_names); i++) {
        if (string_compare(special_names[i], prop->name) == 0) {
            funcs[i](node, prop, depth);
            break;
        }
    }
}

static void print_rdt_node(struct rdt_node *root, int depth) {
    putspace(depth);

    kprint("RDT Node: '%s'\n", root->name);

    LIST_FOREACH(&root->prop_list, pnode) {
        struct rdt_prop *prop = CONTAINER_OF(pnode, struct rdt_prop, node);
        putspace(depth + 1);
        kprint("Property: '%s'\n", prop->name);

        print_special(root, prop, depth + 2);
    }

    LIST_FOREACH(&root->child_list, cnode) {
        struct rdt_node *child = CONTAINER_OF(cnode, struct rdt_node, node);
        print_rdt_node(child, depth + 1);
    }
}

void print_rdt(void) {
    print_rdt_node(rdt_root, 1);
}

uint32_t read_cell(struct rdt_prop *prop) { return FROM_BE_32(*(uint32_t *)prop->data); }
