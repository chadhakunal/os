#include "types.h"
#include "kernel/drivers/uart.h"

void enable_virtual_memory(uint64_t addr)
{
    uint64_t satp;

    /*
     * satp layout (Sv48):
     *   [63:60] MODE = 8  (Sv39 — 3-level, 39-bit VA, 4KB pages)
     *   [59:44] ASID = 0  (This is used for iding the virtual address space, for caching)
     *   [43:0]  PPN  = physical page number of root page table
     */
    satp = (8ULL << 60) | (addr >> 12);

    uart_print("About to enable MMU\n");
    
    // Ensure all page table writes are visible before enabling the MMU
    asm volatile("sfence.vma zero, zero");
    uart_print("sfence.vma done\n");

    // Flush instruction cache before enabling paging
    asm volatile("fence.i");
    
    // Enable virtual memory
    asm volatile("csrw satp, %0" :: "r"(satp) : "memory");
    
    // Critical: TLB and instruction cache must be flushed after satp write
    asm volatile("sfence.vma zero, zero");
    asm volatile("fence.i");
    
    uart_print("Virtual memory enabled\n");
}
