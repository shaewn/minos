#ifndef KERNEL_RBT_H_
#define KERNEL_RBT_H_

// To be embedded in the structures of your desire.
// Use CONTAINER_OF to access your structure
struct rb_node {
    struct rb_node *parent;
    struct rb_node *left, *right;
    int blackness;
};

void rb_link_node(struct rb_node *node, struct rb_node *parent, struct rb_node **pnode);
void rb_insert_color(struct rb_node *node, struct rb_node **root);
void rb_del(struct rb_node *node, struct rb_node **root);

#endif
