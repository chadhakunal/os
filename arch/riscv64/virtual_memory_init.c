#include "kernel/drivers/uart.h"
#include "lib/printk/printk.h"
#include "types.h"

void enable_virtual_memory(uint64_t addr) {
  uint64_t satp;

  /*
   * satp layout (Sv48):
   *   [63:60] MODE = 8  (Sv39 — 3-level, 39-bit VA, 4KB pages)
   *   [59:44] ASID = 0  (This is used for iding the virtual address space, for
   * caching) [43:0]  PPN  = physical page number of root page table
   */
  satp = (8ULL << 60) | (addr >> 12);

  // Ensure all writes are complete before touching paging
  asm volatile("" ::: "memory");

  // Flush all TLB entries
  asm volatile("sfence.vma zero, zero");

  // Now write our real value
  asm volatile("csrw satp, %0" ::"r"(satp) : "memory");

  // Critical: TLB and instruction cache must be flushed after satp write
  asm volatile("sfence.vma zero, zero");
  asm volatile("fence.i");
}
