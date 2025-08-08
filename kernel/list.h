#ifndef KERNEL_LIST_H_
#define KERNEL_LIST_H_

#include "macros.h"

struct list_head {
    struct list_head *prev, *next;
};

#define LIST_ELEMENT(node, type, member) CONTAINER_OF(node, type, member)

#define LIST_FOREACH(head, node_var)                                                               \
    for (struct list_head *node_var = (head)->next; node_var != (head); node_var = node_var->next)
#define LIST_FOREACH_REV(head, node_var)                                                           \
    for (struct list_head *node_var = (head)->prev; node_var != (head); node_var = node_var->prev)

#define LIST_HEAD_INIT(name) {&name, &name}
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

void list_init(struct list_head *head);

void list_add_head(struct list_head *node, struct list_head *head);
void list_add_tail(struct list_head *node, struct list_head *head);

void list_del(struct list_head *node);

#endif
