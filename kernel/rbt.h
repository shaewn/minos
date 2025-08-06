#ifndef KERNEL_RBT_H_
#define KERNEL_RBT_H_

#include "types.h"

#define get_blackness(node_ptr) ((node_ptr)->parent_and_color & 0x3)
#define set_blackness(node_ptr, blackness)                                                         \
    ((node_ptr)->parent_and_color =                                                                \
         ((node_ptr)->parent_and_color & ~(uintptr_t)0x3) | ((blackness) & 0x3))

#define get_parent(node_ptr) ((struct rb_node *)((node_ptr)->parent_and_color & ~(uintptr_t)0x3))
#define set_parent(node_ptr, parent_ptr)                                                           \
    ((node_ptr)->parent_and_color = ((node_ptr)->parent_and_color & 0x3) | ((uintptr_t)(parent_ptr)))

// To be embedded in the structures of your desire.
// Use CONTAINER_OF to access your structure.
// Don't directly modify anything.
struct rb_node {
    uintptr_t parent_and_color;
    struct rb_node *left, *right;
};

void rb_link_node(struct rb_node *node, struct rb_node *parent, struct rb_node **pnode);
void rb_insert_color(struct rb_node *node, struct rb_node **root);
void rb_del(struct rb_node *node, struct rb_node **root);

#endif
