#include "rbt.h"
#include "types.h"

#define RB_RED 0
#define RB_BLACK 1
#define RB_DOUBLE_BLACK 2

inline static void get_p_gp(struct rb_node *node, struct rb_node **p, struct rb_node **gp,
                            struct rb_node **s) {
    *p = get_parent(node);

    if (*p) {
        *gp = get_parent(*p);
    } else {
        *gp = NULL;
    }
}

void rb_link_node(struct rb_node *node, struct rb_node *parent, struct rb_node **pnode) {
    *pnode = node;
    set_parent(node, parent);
    node->left = node->right = NULL;
}

static void rotate_right(struct rb_node **root, struct rb_node *p) {
    // p is called the parent in the insertion routine.
    // its children are the nodes under operation
    // its parent is the 'grandparent'

    struct rb_node **gp_link; // receives the ptr to the node replacing the grandparent.
    struct rb_node *ggp;

    struct rb_node *n2 = p->right;
    struct rb_node *gp = get_parent(p);

    if (get_parent(gp)) {
        struct rb_node *l = get_parent(gp);
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
    set_parent(p, ggp);
    set_parent(gp, p);

    if (n2)
        set_parent(n2, gp);
}

static void rotate_left(struct rb_node **root, struct rb_node *p) {
    // p is called the parent in the insertion routine.
    // its children are the nodes under operation
    // its parent is the 'grandparent'

    struct rb_node **gp_link; // receives the ptr to the node replacing the grandparent.
    struct rb_node *ggp;

    struct rb_node *n2 = p->left;
    struct rb_node *gp = get_parent(p);

    if (get_parent(gp)) {
        struct rb_node *l = get_parent(gp);
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
    set_parent(p, ggp);
    set_parent(gp, p);

    if (n2)
        set_parent(n2, gp);
}

void rb_insert_color(struct rb_node *node, struct rb_node **root) {
    set_blackness(node, RB_RED);

    while (get_parent(node) && get_blackness(get_parent(node)) == RB_RED) {
        // NOTE: node->parent is NOT the root (because it's red).
        struct rb_node *p, *gp, *s;
        get_p_gp(node, &p, &gp, &s);

        int is_p_a_left = p == gp->left;
        int is_node_a_left = node == p->left;

        if (is_p_a_left) {
            if (is_node_a_left) {
                rotate_right(root, p);
                set_blackness(node, RB_BLACK);
            } else {
                rotate_left(root, node);
            }
        } else {
            if (!is_node_a_left) {
                rotate_left(root, p);
                set_blackness(node, RB_BLACK);
            } else {
                rotate_right(root, node);
            }
        }

        node = p;
    }

    if (!get_parent(node)) {
        set_blackness(node, RB_BLACK);
    }
}

static struct rb_node *greatest_lesser(struct rb_node *parent) {
    struct rb_node *node = parent->left;

    if (!node)
        return NULL;

    while (node->right) {
        node = node->right;
    }

    return node;
}

static struct rb_node *least_greater(struct rb_node *parent) {
    struct rb_node *node = parent->right;

    if (!node)
        return NULL;

    while (node->left) {
        node = node->left;
    }

    return node;
}

inline static int is_leaf(struct rb_node *node) { return !node->left && !node->right; }

inline static void fix_self_references(struct rb_node *node, struct rb_node *from,
                                       struct rb_node *to) {
    if (get_parent(node) == from)
        set_parent(node, to);
    if (node->left == from)
        node->left = to;
    if (node->right == from)
        node->right = to;
}

inline static void assign_parent_to_children(struct rb_node *node) {
    if (node->left)
        set_parent(node->left, node);
    if (node->right)
        set_parent(node->right, node);
}

static void swap_nodes(struct rb_node **root, struct rb_node *a, struct rb_node *b) {
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

    is_orig_a_left_child = get_parent(a) && get_parent(a)->left == a;
    is_orig_b_left_child = get_parent(b) && get_parent(b)->left == b;

    *a = *b;
    *b = tmp;

    // Fix any self-references.
    fix_self_references(a, a, b);
    fix_self_references(b, b, a);

    if (get_parent(a)) {
        if (is_orig_b_left_child) {
            a_link = &get_parent(a)->left;
        } else {
            a_link = &get_parent(a)->right;
        }
    } else {
        a_link = root;
    }

    if (get_parent(b)) {
        if (is_orig_a_left_child) {
            b_link = &get_parent(b)->left;
        } else {
            b_link = &get_parent(b)->right;
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
    *p = get_parent(node);

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

    if (get_blackness(node) == RB_RED) {
        goto unlink;
    }

    set_blackness(node, RB_DOUBLE_BLACK);

    struct rb_node *current = node;

    // https://medium.com/analytics-vidhya/deletion-in-red-black-rb-tree-92301e1474ea

    while (get_blackness(current) == RB_DOUBLE_BLACK && get_parent(current)) {
        struct rb_node *p, *s, *cl, *cr;
        get_p_s_cl_cr(current, &p, &s, &cl, &cr);

        // NOTE: We're guaranteed the sibling exists, since at some point, in current's subtree,
        // there was a non-nil black node. Hence, there must be a non-nil black node in the
        // sibling's subtree.
        if (get_blackness(s) == RB_BLACK) {
            if ((!cl || get_blackness(cl) == RB_BLACK) && (!cr || get_blackness(cr) == RB_BLACK)) {
                // Both children of sibling are black (either nil or actually black).
                // Case 3.
                set_blackness(p, get_blackness(p) + 1);
                if (s)
                    set_blackness(s, RB_RED);
                set_blackness(current, RB_BLACK);
                current = p;
            }

            if (current == p->left) {
                // Left mirror of 5 and 6.

                if ((!cr || get_blackness(cr) == RB_BLACK) && cl && get_blackness(cl) == RB_RED) {
                    // Case 5.
                    set_blackness(cl, RB_BLACK);
                    set_blackness(s, RB_RED);
                    rotate_right(root, cl);
                    // Case 6 will happen next.
                } else if (cr && get_blackness(cr) == RB_RED) {
                    // Case 6.
                    set_blackness(s, get_blackness(p));
                    set_blackness(p, RB_BLACK);
                    rotate_left(root, s);
                    set_blackness(current, RB_BLACK);
                    set_blackness(cr, RB_BLACK);
                }
            } else {
                // Right mirror of 5 and 6.

                if ((!cl || get_blackness(cl) == RB_BLACK) && cr && get_blackness(cr) == RB_RED) {
                    // Case 5.
                    set_blackness(cr, RB_BLACK);
                    set_blackness(s, RB_RED);
                    rotate_left(root, cr);
                } else if (cl && get_blackness(cl) == RB_RED) {
                    set_blackness(s, get_blackness(p));
                    set_blackness(p, RB_BLACK);
                    rotate_right(root, s);
                    set_blackness(current, RB_BLACK);
                    set_blackness(cl, RB_BLACK);
                }
            }
        } else {
            // Case 4.
            set_blackness(p, RB_RED);
            set_blackness(s, RB_BLACK);

            if (s == p->left) {
                rotate_right(root, s);
            } else {
                rotate_left(root, s);
            }
        }
    }

    if (!get_parent(current)) {
        set_blackness(current, RB_BLACK);
    }

unlink:;
    struct rb_node *parent = get_parent(node);
    struct rb_node **pnode =
        parent ? node == parent->left ? &parent->left : &parent->right
                     : root;
    *pnode = NULL;
}
