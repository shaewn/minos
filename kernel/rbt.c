#include "rbt.h"
#include "types.h"

#define RB_RED 0
#define RB_BLACK 1
#define RB_DOUBLE_BLACK 2

inline static void get_p_gp(struct rb_node *node, struct rb_node **p, struct rb_node **gp,
                            struct rb_node **s) {
    *p = node->parent;

    if (*p) {
        *gp = (*p)->parent;
    } else {
        *gp = NULL;
    }
}

void rb_link_node(struct rb_node *node, struct rb_node *parent, struct rb_node **pnode) {
    *pnode = node;
    node->parent = parent;
    node->left = node->right = NULL;
}

void rotate_right(struct rb_node **root, struct rb_node *p) {
    // p is called the parent in the insertion routine.
    // its children are the nodes under operation
    // its parent is the 'grandparent'

    struct rb_node **gp_link; // receives the ptr to the node replacing the grandparent.
    struct rb_node *ggp;

    struct rb_node *n2 = p->right;
    struct rb_node *gp = p->parent;

    if (gp->parent) {
        struct rb_node *l = gp->parent;
        if (l->left == gp) {
            gp_link = &l->left;
        } else {
            gp_link = &l->right;
        }

        ggp = l;
    } else {
        gp_link = root;
        ggp = NULL;
    }

    // must change ->right for p
    p->right = gp;

    // must change ->left for gp
    gp->left = n2;

    // must change either ->left or ->right for ggp
    *gp_link = p;

    // must change ->parent for p, gp, and n2
    p->parent = ggp;
    gp->parent = p;

    if (n2)
        n2->parent = gp;
}

void rotate_left(struct rb_node **root, struct rb_node *p) {
    // p is called the parent in the insertion routine.
    // its children are the nodes under operation
    // its parent is the 'grandparent'

    struct rb_node **gp_link; // receives the ptr to the node replacing the grandparent.
    struct rb_node *ggp;

    struct rb_node *n2 = p->left;
    struct rb_node *gp = p->parent;

    if (gp->parent) {
        struct rb_node *l = gp->parent;
        if (l->left == gp) {
            gp_link = &l->left;
        } else {
            gp_link = &l->right;
        }

        ggp = l;
    } else {
        gp_link = root;
        ggp = NULL;
    }

    // must change ->left for p
    p->left = gp;

    // must change ->right for gp
    gp->right = n2;

    // must change ->left or ->right for ggp
    *gp_link = p;

    // must change ->parent for p, gp, and n2
    p->parent = ggp;
    gp->parent = p;

    if (n2)
        n2->parent = gp;
}

void rb_insert_color(struct rb_node *node, struct rb_node **root) {
    node->blackness = RB_RED;

    while (node->parent && node->parent->blackness == RB_RED) {
        // NOTE: node->parent is NOT the root (because it's red).
        struct rb_node *p, *gp, *s;
        get_p_gp(node, &p, &gp, &s);

        int is_p_a_left = p == gp->left;
        int is_node_a_left = node == p->left;

        if (is_p_a_left) {
            if (is_node_a_left) {
                rotate_right(root, p);
                node->blackness = RB_BLACK;
            } else {
                rotate_left(root, node);
            }
        } else {
            if (!is_node_a_left) {
                rotate_left(root, p);
                node->blackness = RB_BLACK;
            } else {
                rotate_right(root, node);
            }
        }

        node = p;
    }

    if (!node->parent) {
        node->blackness = RB_BLACK;
    }
}

struct rb_node *greatest_lesser(struct rb_node *parent) {
    struct rb_node *node = parent->left;

    if (!node)
        return NULL;

    while (node->right) {
        node = node->right;
    }

    return node;
}

struct rb_node *least_greater(struct rb_node *parent) {
    struct rb_node *node = parent->right;

    if (!node)
        return NULL;

    while (node->left) {
        node = node->left;
    }

    return node;
}

int is_leaf(struct rb_node *node) { return !node->left && !node->right; }

inline static void fix_self_references(struct rb_node *node, struct rb_node *from,
                                       struct rb_node *to) {
    if (node->parent == from)
        node->parent = to;
    if (node->left == from)
        node->left = to;
    if (node->right == from)
        node->right = to;
}

inline static void assign_parent_to_children(struct rb_node *node) {
    if (node->left)
        node->left->parent = node;
    if (node->right)
        node->right->parent = node;
}

void swap_nodes(struct rb_node **root, struct rb_node *a, struct rb_node *b) {
    // things needing to be swapped:
    // links from above (->parent->left or ->parent->right OR *root)
    // link to above (->parent)
    // links from below (->left->parent and ->right->parent)
    // links to below (->left and ->right)
    // color

    // things to note: when transfering pointers from a to b,
    // if one of a's pointers points to b, that pointer needs to be made to point to a (from b)

    if (a == b)
        return;

    struct rb_node tmp = *a;

    struct rb_node **a_link, **b_link;

    int is_orig_a_left_child, is_orig_b_left_child;

    is_orig_a_left_child = a->parent && a->parent->left == a;
    is_orig_b_left_child = b->parent && b->parent->left == b;

    *a = *b;
    *b = tmp;

    // Fix any self-references.
    fix_self_references(a, a, b);
    fix_self_references(b, b, a);

    if (a->parent) {
        if (is_orig_b_left_child) {
            a_link = &a->parent->left;
        } else {
            a_link = &a->parent->right;
        }
    } else {
        a_link = root;
    }

    if (b->parent) {
        if (is_orig_a_left_child) {
            b_link = &b->parent->left;
        } else {
            b_link = &b->parent->right;
        }
    } else {
        b_link = root;
    }

    *a_link = a;
    *b_link = b;

    assign_parent_to_children(a);
    assign_parent_to_children(b);
}

inline static void get_p_s_cl_cr(struct rb_node *node, struct rb_node **p, struct rb_node **s,
                                 struct rb_node **cl, struct rb_node **cr) {
    *p = node->parent;

    if (*p) {
        if (node == (*p)->left) {
            *s = (*p)->right;
        } else {
            *s = (*p)->left;
        }
    } else {
        *s = NULL;
    }

    if (*s) {
        *cl = (*s)->left;
        *cr = (*s)->right;
    }
}

void rb_del(struct rb_node *node, struct rb_node **root) {
    while (!is_leaf(node)) {
        struct rb_node *successor = node->right ? least_greater(node) : greatest_lesser(node);

        swap_nodes(root, node, successor);
    }

    if (node->blackness == RB_RED) {
        goto unlink;
    }

    node->blackness = RB_DOUBLE_BLACK;

    struct rb_node *current = node;

    // https://medium.com/analytics-vidhya/deletion-in-red-black-rb-tree-92301e1474ea

    while (current->blackness == RB_DOUBLE_BLACK && current->parent) {
        struct rb_node *p, *s, *cl, *cr;
        get_p_s_cl_cr(current, &p, &s, &cl, &cr);

        // NOTE: We're guaranteed the sibling exists, since at some point, in current's subtree,
        // there was a non-nil black node. Hence, there must be a non-nil black node in the
        // sibling's subtree.
        if (s->blackness == RB_BLACK) {
            if ((!cl || cl->blackness == RB_BLACK) && (!cr || cr->blackness == RB_BLACK)) {
                // Both children of sibling are black (either nil or actually black).
                // Case 3.
                p->blackness++;
                if (s)
                    s->blackness = RB_RED;
                current->blackness = RB_BLACK;
                current = p;
            }

            if (current == p->left) {
                // Left mirror of 5 and 6.

                if ((!cr || cr->blackness == RB_BLACK) && cl && cl->blackness == RB_RED) {
                    // Case 5.
                    cl->blackness = RB_BLACK;
                    s->blackness = RB_RED;
                    rotate_right(root, cl);
                }

                if (cr && cr->blackness == RB_RED) {
                    s->blackness = p->blackness;
                    p->blackness = RB_BLACK;
                    rotate_left(root, s);
                    current->blackness = RB_BLACK;
                    cr->blackness = RB_BLACK;
                }
            } else {
                // Right mirror of 5 and 6.

                if ((!cl || cl->blackness == RB_BLACK) && cr && cr->blackness == RB_RED) {
                    // Case 5.
                    cr->blackness = RB_BLACK;
                    s->blackness = RB_RED;
                    rotate_left(root, cr);
                }

                if (cl && cl->blackness == RB_RED) {
                    s->blackness = p->blackness;
                    p->blackness = RB_BLACK;
                    rotate_right(root, s);
                    current->blackness = RB_BLACK;
                    cl->blackness = RB_BLACK;
                }
            }
        } else {
            // Case 4.
            p->blackness = RB_RED;
            s->blackness = RB_BLACK;

            if (s == p->left) {
                rotate_right(root, s);
            } else {
                rotate_left(root, s);
            }
        }
    }

    if (!current->parent) {
        current->blackness = RB_BLACK;
    }

unlink:;
    struct rb_node **pnode =
        node->parent ? node == node->parent->left ? &node->parent->left : &node->parent->right
                     : root;
    *pnode = NULL;
}
