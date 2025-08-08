#ifndef AARCH64_PHANDLE_TABLE_H_
#define AARCH64_PHANDLE_TABLE_H_

#include "types.h"
#include "rdt.h"

void phandle_table_insert(struct rdt_node *node, uint32_t phandle);
struct rdt_node *phandle_table_get(uint32_t phandle);

#endif
