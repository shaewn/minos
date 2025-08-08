#ifndef KERNEL_DTB_H_
#define KERNEL_DTB_H_

#include "fdt.h"

void build_dt_init(struct fdt_header *header);

struct dt_node *dt_search_init(struct dt_node *start, const char *path);
struct dt_node *dt_find_init(struct dt_node *parent, const char *name);
struct dt_prop *dt_findprop_init(struct dt_node *parent, const char *propname);

#endif
