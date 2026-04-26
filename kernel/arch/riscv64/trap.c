#include "lib/printk/printk.h"
#include "kernel/panic.h"
#include "arch/riscv64/trap.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"

/* NEVER RETURNS - either calls trap_return() or panic() */
void trap_handler(void) {
  // Access trap frame from current_task (tp register points to it)
  struct trap_frame *tf = &current_task->tf;

  uint64_t cause_code = tf->scause & 0x7FFFFFFFFFFFFFFF;
  bool is_interrupt = (tf->scause >> 63) & 1;

  // Only print trap info for non-syscall traps to avoid infinite recursion
  // (printk uses write() syscall which would cause another trap)
  if (is_interrupt || cause_code != 8) {
    printk("\n=== TRAP ===\n");
    printk("scause:  %llx\n", tf->scause);
    printk("sepc:    %llx\n", tf->sepc);
    printk("stval:   %llx\n", tf->stval);
    printk("sstatus: %llx\n", tf->sstatus);
  }

  if (is_interrupt) {
    printk("Interrupt: ");
    switch (cause_code) {
      case 1:  printk("Supervisor software interrupt\n"); break;
      case 5:  printk("Supervisor timer interrupt\n"); break;
      case 9:  printk("Supervisor external interrupt (UART)\n"); break;
      default: printk("Unknown interrupt: %llu\n", cause_code); break;
    }
  } else {
    // printk("Exception: ");
    switch (cause_code) {
      case 0:  printk("Instruction address misaligned\n"); break;
      case 1:  printk("Instruction access fault\n"); break;
      case 2:  printk("Illegal instruction\n"); break;
      case 3:  printk("Breakpoint\n"); break;
      case 4:  printk("Load address misaligned\n"); break;
      case 5:  printk("Load access fault\n"); break;
      case 6:  printk("Store address misaligned\n"); break;
      case 7:  printk("Store access fault\n"); break;
      case 8:
        // printk("Environment call from U-mode\n");
        handle_syscall(tf);
        break;
      case 9:  printk("Environment call from S-mode\n"); break;
      case 12: printk("Instruction page fault\n"); break;
      case 13: printk("Load page fault\n"); break;
      case 15: printk("Store page fault\n"); break;
      default: printk("Unknown exception: %llu\n", cause_code); break;
    }
  }

  // For syscalls, return to user mode
  if (!is_interrupt && cause_code == 8) {
    schedule();
    extern void trap_return(struct trap_frame *tf);
    trap_return(&current_task->tf);
    // Never returns
  }

  // For all other traps, print registers and panic
  printk("current_task = %p, pid = %llu\n", current_task, current_task->pid);
  // printk("\nRegisters:\n");
  // printk("ra:  %llx  sp:  %llx  gp:  %llx  tp:  %llx\n", tf->ra, tf->sp, tf->gp, tf->tp);
  // printk("t0:  %llx  t1:  %llx  t2:  %llx\n", tf->t0, tf->t1, tf->t2);
  // printk("s0:  %llx  s1:  %llx\n", tf->s0, tf->s1);
  // printk("a0:  %llx  a1:  %llx  a2:  %llx  a3:  %llx\n", tf->a0, tf->a1, tf->a2, tf->a3);
  // printk("a4:  %llx  a5:  %llx  a6:  %llx  a7:  %llx\n", tf->a4, tf->a5, tf->a6, tf->a7);
  // printk("s2:  %llx  s3:  %llx  s4:  %llx  s5:  %llx\n", tf->s2, tf->s3, tf->s4, tf->s5);
  // printk("s6:  %llx  s7:  %llx  s8:  %llx  s9:  %llx\n", tf->s6, tf->s7, tf->s8, tf->s9);
  // printk("s10: %llx  s11: %llx\n", tf->s10, tf->s11);
  // printk("t3:  %llx  t4:  %llx  t5:  %llx  t6:  %llx\n", tf->t3, tf->t4, tf->t5, tf->t6);
  //
  panic("TRAP OCCURRED");
}

void init_trap_handler(void) {
  extern void trap_vector(void);

  /* Set stvec to trap_vector */
  asm volatile("csrw stvec, %0" :: "r"(trap_vector));
}

void enable_interrupts(void) {
  /* Enable supervisor interrupts in sstatus */
  uint64_t sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(sstatus));
  sstatus |= SSTATUS_SIE;  /* Set SIE bit to enable interrupts */
  asm volatile("csrw sstatus, %0" :: "r"(sstatus));

  /* Enable external, timer, and software interrupts in sie register */
  uint64_t sie = (1UL << 9) |  /* SEIE - Supervisor external interrupt enable */
                 (1UL << 5) |  /* STIE - Supervisor timer interrupt enable */
                 (1UL << 1);   /* SSIE - Supervisor software interrupt enable */
  asm volatile("csrw sie, %0" :: "r"(sie));

  printk("Global interrupts enabled (sstatus.SIE=1, sie=0x%lx)\n", sie);
}

void disable_interrupts(void) {
  /* Disable supervisor interrupts in sstatus */
  uint64_t sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(sstatus));
  sstatus &= ~SSTATUS_SIE;  /* Clear SIE bit to disable interrupts */
  asm volatile("csrw sstatus, %0" :: "r"(sstatus));

  printk("Global interrupts disabled (sstatus.SIE=0)\n");
}
