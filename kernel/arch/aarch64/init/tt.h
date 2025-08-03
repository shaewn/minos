#ifndef AARCH64_TT_H_
#define AARCH64_TT_H_

#include "tcr.h"

#define BLOCK_DESC 0b01
#define TABLE_DESC 0b11
#define PAGE_DESC 0b11

#define TTE_AF (1 << 10)
#define MEM_ATTR_IDX_NORMAL (0 << 2)
#define MEM_ATTR_IDX_DEV_STRICT (1 << 2)

#define AP_TABLE_NO_EL0 (1ULL << 61)

#define AP_START 6

// Execution is enabled unless explicitly disabled

#define AP_RDWR_PRIV (0 << AP_START)

// disables privileged execution.
#define AP_RDWR_ALL_PXN (1 << AP_START)

#define AP_RDONLY_PRIV (2 << AP_START)
#define AP_RDONLY_ALL (3 << AP_START)

// upper attributes start at bit 50
#define BLOCK_UXN (1ULL << 54)
#define BLOCK_PXN (1ULL << 53)

#endif
