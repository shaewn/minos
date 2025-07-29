#ifndef KERNEL_DTB_H_
#define KERNEL_DTB_H_

struct dtb_prop {
    struct dtb_prop *next;
    const char *name, *data;
};

struct dtb_entry {
    const char *name;
    struct dtb_entry *parent, *first_child, *next_sibling;
    struct dtb_prop *first_prop;
};

#endif
