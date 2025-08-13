#ifndef AARCH64_RDT_H_
#define AARCH64_RDT_H_

#include "types.h"
#include "list.h"

struct rdt_prop {
    const char *name;
    const void *data;
    size_t data_length;

    struct list_head node;
};

struct rdt_node {
    const char *name;
    struct rdt_node *parent;
    struct list_head child_list, prop_list;
    struct list_head node;
};

void build_rdt(void);

struct rdt_node *rdt_find_node(struct rdt_node *node, const char *path);
struct rdt_node *rdt_find_child(struct rdt_node *node, const char *prefix);
struct rdt_node *rdt_find_child_exact(struct rdt_node *node, const char *name);
struct rdt_prop *rdt_find_prop(struct rdt_node *node, const char *name);
bool rdt_node_compatible(struct rdt_node *node, const char *compat_str);
struct rdt_node *rdt_find_compatible(struct rdt_node *start_node, const char *compat_str);

uint32_t read_cell(struct rdt_prop *prop);

void print_rdt(void);

#endif
