#include "list.h"

void list_init(struct list_head *head) {
    head->prev = head->next = head;
}

static void list_add(struct list_head *new, struct list_head *prev, struct list_head *next) {
    prev->next = new;
    new->prev = prev;
    new->next = next;
    next->prev = new;
}

void list_add_head(struct list_head *node, struct list_head *head) {
    list_add(node, head, head->next);
}

void list_add_tail(struct list_head *node, struct list_head *head) {
    list_add(node, head->prev, head);
}

static void list_do_del(struct list_head *node, struct list_head *prev, struct list_head *next) {
    prev->next = next;
    next->prev = prev;
}

void list_del(struct list_head *node) {
    list_do_del(node, node->prev, node->next);
}

bool list_empty(struct list_head *head) {
    return head->next == head;
}
