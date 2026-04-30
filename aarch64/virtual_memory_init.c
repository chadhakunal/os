#include "arch/aarch64/virtual_memory_init.h"
#include "types.h"

#include "kernel/drivers/uart.h"

void enable_virtual_memory(uint64_t addr) {
  uint64_t mair;
  uint64_t tcr;
  uint64_t sctlr;

  /* MAIR_EL1 setup
     AttrIdx 0 → device memory
     AttrIdx 1 → normal memory */
  mair = (0x00ULL << 0) | (0xFFULL << 8);
  asm volatile("msr mair_el1, %0" ::"r"(mair));
  uart_print("MAIR EL1 Set\n");

  /* TCR_EL1 setup
     48-bit VA
     4KB pages
     inner shareable
     write-back cache */
  tcr = (16ULL << 0) | // T0SZ
        (1ULL << 8) |  // IRGN0
        (1ULL << 10) | // ORGN0
        (3ULL << 12) | // SH0
        (0ULL << 14);  // TG0 = 4KB

  asm volatile("msr tcr_el1, %0" ::"r"(tcr));
  uart_print("TCR EL1 Set\n");

  /* Set root page table */
  asm volatile("msr ttbr0_el1, %0" ::"r"(addr));
  uart_print("TTBR0 EL1 Set\n");

  /* Ensure register writes are visible */
  asm volatile("isb");

  /* Enable MMU */
  asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
  sctlr |= (1ULL << 0); // M bit
  uart_print("SCTLR EL1 is ");
  uart_print_hex(sctlr);

  asm volatile("msr sctlr_el1, %0" ::"r"(sctlr));

  asm volatile("isb");

  uart_print("SCTLR EL1 Set\n");
}
