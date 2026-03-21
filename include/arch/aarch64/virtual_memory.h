#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#include "types.h"

#define PTE_VALID       (1ULL << 0)

/*
Level	    bits[1:0] = 0b01	                bits[1:0] = 0b11
L0/L1/L2	Block descriptor (large mapping)	Table descriptor (points to next level)
L3	        Reserved/fault	                    Page descriptor (4KB page)
*/
#define PTE_TABLE       (1ULL << 1)

#define PTE_ATTRIDX(x)  ((uint64_t)(x) << 2)

#define PTE_AP_KERNEL       (0ULL << 6) // EL1 read/write, EL0 no access
#define PTE_AP_USER         (1ULL << 6) // EL1 read/write, EL0 read/write
#define PTE_AP_KERNEL_RO    (2ULL << 6) // EL1 read-only, EL0 no access
#define PTE_AP_RO           (3ULL << 6) // EL1 read-only, EL0 read-only

#define PTE_SH_NONE         (0ULL << 8) // Memory is not shared across cores
#define PTE_SH_OUTER        (2ULL << 8) // Memory is shared across outer cache levels.
#define PTE_SH_INNER        (3ULL << 8) // Memory is shared across all cores in the same cluster and their caches stay coherent.

#define PTE_AF (1ULL << 10)
#define PTE_NG (1ULL << 11)

#define PTE_ADDR(x) ((uint64_t)(x) & 0x0000FFFFFFFFF000ULL)

#define PT0_OFFSET(x) (((uint64_t)(x) >> 39) & 0x1FF)
#define PT1_OFFSET(x) (((uint64_t)(x) >> 30) & 0x1FF)
#define PT2_OFFSET(x) (((uint64_t)(x) >> 21) & 0x1FF)
#define PT3_OFFSET(x) (((uint64_t)(x) >> 12) & 0x1FF)

void enable_virtual_memory(uint64_t addr);

#endif