#include "lib/printk/printk.h"
#include "kernel/panic.h"

void trap_handler(void) {
  uint64_t scause, sepc, stval;

  asm volatile("csrr %0, scause" : "=r"(scause));
  asm volatile("csrr %0, sepc" : "=r"(sepc));
  asm volatile("csrr %0, stval" : "=r"(stval));

  printk("\n=== TRAP ===\n");
  printk("scause: %llx\n", scause);
  printk("sepc (PC): %llx\n", sepc);
  printk("stval (bad addr): %llx\n", stval);

  uint64_t cause_code = scause & 0x7FFFFFFFFFFFFFFF;

  if (cause_code == 12) {
    printk("Instruction page fault!\n");
  } else if (cause_code == 13) {
    printk("Load page fault!\n");
  } else if (cause_code == 15) {
    printk("Store page fault!\n");
  }

  panic("TRAP OCCURRED");
}

void init_trap_handler(void) {
  extern void trap_vector(void);

  /* Set stvec to trap_vector */
  asm volatile("csrw stvec, %0" :: "r"(trap_vector));

  printk("Trap handler installed\n");
}
