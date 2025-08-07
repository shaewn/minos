#ifndef KERNEL_DTB_H_
#define KERNEL_DTB_H_

#include "fdt.h"

struct dt_prop {
    struct dt_prop *next;
    const char *name, *data;
    size_t data_length;
};

struct dt_node {
    const char *name;
    struct dt_node *parent, *first_child, *last_child, *next_sibling;
    struct dt_prop *first_prop, *last_prop;
};

void build_dt_init(struct fdt_header *header);
void print_dt(struct dt_node *root, int current_depth);

struct dt_node *dt_search_init(struct dt_node *start, const char *path);
struct dt_node *dt_find_init(struct dt_node *parent, const char *name);
struct dt_prop *dt_findprop_init(struct dt_node *parent, const char *propname);

#endif
