#include "types.h"
#include "kernel/drivers/uart.h"

void enable_virtual_memory(uint64_t addr)
{
    uint64_t satp;

    /*
     * satp layout (Sv39):
     *   [63:60] MODE = 8  (Sv39 — 3-level, 39-bit VA, 4KB pages)
     *   [59:44] ASID = 0
     *   [43:0]  PPN  = physical page number of root page table
     */
    satp = (8ULL << 60) | (addr >> 12);

    // Ensure all page table writes are visible before enabling the MMU
    asm volatile("sfence.vma zero, zero");
    uart_print("sfence.vma done\n");

    asm volatile("csrw satp, %0" :: "r"(satp));
    uart_print("satp Set\n");

    // Flush TLB after enabling paging
    asm volatile("sfence.vma zero, zero");

    uart_print("Virtual memory enabled\n");
}
