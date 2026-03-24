#include "types.h"
#include "kernel/drivers/uart.h"
#include "lib/printk/printk.h"

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
    
    // Test: read current satp to verify register exists and is accessible
    uint64_t old_satp;
    asm volatile("csrr %0, satp" : "=r"(old_satp));
    printk("Current satp value read %lx\n", old_satp);
    
    // Ensure all writes are complete before touching paging
    asm volatile("" ::: "memory");
    
    // Flush all TLB entries
    asm volatile("sfence.vma zero, zero");
    uart_print("sfence.vma done\n");
    
    // Test: Try writing Sv39 MODE with our REAL root page table address
    uart_print("Testing Sv39 mode with REAL root PT...\n");
    asm volatile("csrw satp, %0" :: "r"(satp) : "memory");
    uart_print("Sv39 with real root PT write succeeded\n");
    
    // Now write our real value
    uart_print("About to write satp with Sv39 and real PPN...\n");
    asm volatile("csrw satp, %0" :: "r"(satp) : "memory");
    uart_print("satp write complete\n");
    
    // Small delay
    volatile int i = 0;
    while(i < 10) i++;
    
    // Critical: TLB and instruction cache must be flushed after satp write
    asm volatile("sfence.vma zero, zero");
    asm volatile("fence.i");
    
    uart_print("Virtual memory enabled\n");
}
