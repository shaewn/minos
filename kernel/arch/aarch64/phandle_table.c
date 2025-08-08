#include "phandle_table.h"
#include "kmalloc.h"

struct phandle_ent {
    struct phandle_ent *next;
    struct rdt_node *node;
    uint32_t phandle;
};

#define PHANDTAB_SIZE 128
struct phandle_ent *phandle_table[PHANDTAB_SIZE];

void phandle_table_insert(struct rdt_node *node, uint32_t phandle) {
    uint32_t index = phandle % PHANDTAB_SIZE;

    struct phandle_ent **ent = &phandle_table[index];

    struct phandle_ent *new_ent = kmalloc(sizeof(*new_ent));
    new_ent->next = *ent;
    new_ent->node = node;
    new_ent->phandle = phandle;

    *ent = new_ent;
}

struct rdt_node *phandle_table_get(uint32_t phandle) {
    uint32_t index = phandle % PHANDTAB_SIZE;

    struct phandle_ent *ent = phandle_table[index];

    while (ent) {
        if (ent->phandle == phandle) {
            return ent->node;
        }

        ent = ent->next;
    }

    return NULL;
}
