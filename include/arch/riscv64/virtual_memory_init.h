#ifndef VIRTUAL_MEMORY_INIT_H
#define VIRTUAL_MEMORY_INIT_H

#include "types.h"

/*
 * RISC-V Sv39 page table entry (PTE) format:
 *   [63:54] reserved
 *   [53:10] PPN (physical page number, 44 bits)
 *   [9:8]   RSW
 *   [7]     D  (dirty)
 *   [6]     A  (accessed)
 *   [5]     G  (global)
 *   [4]     U  (user-accessible)
 *   [3]     X  (executable)
 *   [2]     W  (writable)
 *   [1]     R  (readable)
 *   [0]     V  (valid)
 *
 * Non-leaf (table) entry: V=1, R=W=X=0
 * Leaf entry: V=1, at least one of R/W/X set
 */
#define PTE_VALID (1ULL << 0)
#define PTE_TABLE (0ULL) // non-leaf: R=W=X=0, only V set

#define PTE_R (1ULL << 1)
#define PTE_W (1ULL << 2)
#define PTE_X (1ULL << 3)
#define PTE_U (1ULL << 4) // user-accessible
#define PTE_G (1ULL << 5) // global
#define PTE_A (1ULL << 6) // accessed
#define PTE_D (1ULL << 7) // dirty

/* Access permission aliases to match aarch64 naming used in page_tables.c */
#define PTE_AP_KERNEL (PTE_R | PTE_W | PTE_A | PTE_D)
#define PTE_AP_USER (PTE_R | PTE_W | PTE_U | PTE_A | PTE_D)
#define PTE_AP_KERNEL_RO (PTE_R | PTE_A)
#define PTE_AP_RO (PTE_R | PTE_U | PTE_A)

/* No-ops: RISC-V has no MAIR attribute index or shareability fields */
#define PTE_SH_NONE (0ULL)
#define PTE_SH_OUTER (0ULL)
#define PTE_SH_INNER (0ULL)
#define PTE_AF (0ULL)
#define PTE_NG (0ULL)
#define PTE_ATTRIDX(x) (0ULL)

/*
 * PTE_ADDR: encode a physical address into a PTE.
 * PPN occupies bits [53:10] of the PTE, where PPN = PA >> 12.
 * So: PTE[53:10] = PA[55:12]  =>  shift PA right by 12, then left by 10.
 */
#define PTE_ADDR(x) (((uint64_t)(x) >> 12) << 10)

/*
 * PTE_DECODE: extract physical address from a PTE entry.
 * Reverse of PTE_ADDR: extract bits [53:10], shift right by 10, shift left
 * by 12.
 */
#define PTE_DECODE(x) (((uint64_t)(x) & 0x003FFFFFFFFFFC00ULL) << 2)

/*
 * Sv39 uses 3 levels (39-bit VA, 4KB pages):
 *   VPN[2] = VA[38:30]
 *   VPN[1] = VA[29:21]
 *   VPN[0] = VA[20:12]
 *
 * page_tables.c uses a 4-level walk (PT0..PT3). PT0_OFFSET always returns 0
 * so the root table acts as a single shared entry, collapsing to 3 effective
 * levels.
 */
#define PT0_OFFSET(x) (0ULL)
#define PT1_OFFSET(x) (((uint64_t)(x) >> 30) & 0x1FF) // VPN[2]
#define PT2_OFFSET(x) (((uint64_t)(x) >> 21) & 0x1FF) // VPN[1]
#define PT3_OFFSET(x) (((uint64_t)(x) >> 12) & 0x1FF) // VPN[0]

#define KERNEL_VIRTUAL_MEMORY_BASE                                             \
  0xFFFFFFFF80000000ULL // 510 GB Mark ie 254 GB After kernel area start
#define PHYS_VIRTUAL_MEMORY_BASE                                               \
  0xFFFFFFC000000000ULL // 256 GB Mark ie 0 GB After kernel area start
#define MMIO_VIRTUAL_MEMORY_BASE                                               \
  0xFFFFFFD000000000ULL // 320 GB Mark ie 64 GB After kernel area start

#define KERNEL_VIRT_OFFSET (KERNEL_VIRTUAL_MEMORY_BASE - _kernel_start)

void enable_virtual_memory(uint64_t addr);

#endif
