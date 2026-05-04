#include "lib/printk/printk.h"
#include "kernel/panic.h"
#include "arch/riscv64/trap.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/sbi.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"

#define TIMER_INTERVAL_CYCLES 100000

/* NEVER RETURNS - either calls trap_return() or panic() */
void trap_handler(struct trap_frame *tf) {
  // tf points to either:
  // - &current_task->tf for user traps
  // - kernel stack for kernel traps
  uint64_t cause_code = tf->scause & 0x7FFFFFFFFFFFFFFF;
  bool is_interrupt = (tf->scause >> 63) & 1;

  // Don't print for timer interrupts
  if (!(is_interrupt && cause_code == 5)) {
    // printk("[trap_handler] current_task=%p, pid=%llu\n",
    //        current_task, current_task->pid);

    // printk("\n=== TRAP ===\n");
    // printk("scause:  %llx\n", tf->scause);
    // printk("sepc:    %llx\n", tf->sepc);
    // printk("stval:   %llx\n", tf->stval);
    // printk("sstatus: %llx\n", tf->sstatus);
    if (is_interrupt) {
      printk("\n=== TRAP ===\n");
      printk("scause:  %llx\n", tf->scause);
      printk("sepc:    %llx\n", tf->sepc);
      printk("stval:   %llx\n", tf->stval);
      printk("sstatus: %llx\n", tf->sstatus);
    } else if (cause_code != 8) {
      // Print for non-syscall exceptions
      printk("\n=== TRAP ===\n");
      printk("scause:  %llx\n", tf->scause);
      printk("sepc:    %llx\n", tf->sepc);
      printk("stval:   %llx\n", tf->stval);
      printk("sstatus: %llx\n", tf->sstatus);
    }
  }

  if (is_interrupt) {
    switch (cause_code) {
      case 1:
        printk("Interrupt: Supervisor software interrupt\n");
        break;
      case 5:
        printk("Timer interrupt time !\n");
        trap_timer_handler(tf);
        extern void trap_return(struct trap_frame *tf);

        printk("[trap] Timer interrupt: sstatus=%llx, SPP=%llu\n",
               tf->sstatus, (tf->sstatus >> 8) & 1);

        if (tf->sstatus & (1UL << 8)) {
          // Kernel mode timer interrupt
          tf->sepc += 4;
          printk("[trap] About to return from trap_handler, sepc now=%llx\n", tf->sepc);
          printk("[trap] Calling asm nop...\n");
          asm volatile("nop");
          printk("[trap] After nop, about to return\n");
          return;
          printk("[trap] THIS SHOULD NEVER PRINT\n");
        }

        printk("[trap] Came from user mode, calling trap_return\n");
        trap_return(&current_task->tf);
        break;
      case 9:
        printk("Interrupt: Supervisor external interrupt (UART)\n");
        break;
      default:
        printk("Interrupt: Unknown interrupt: %llu\n", cause_code);
        break;
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
        printk("Syscall from %llu\n", current_task->pid);
        handle_syscall(tf);

        extern void trap_return(struct trap_frame *tf);
        trap_return(&current_task->tf);
        break;
      case 9:  printk("Environment call from S-mode\n"); break;
      case 12: printk("Instruction page fault\n"); break;
      case 13: printk("Load page fault\n"); break;
      case 15: printk("Store page fault\n"); break;
      default: printk("Unknown exception: %llu\n", cause_code); break;
    }
  }

  // For syscalls, schedule and return to user mode
  // if (!is_interrupt && cause_code == 8) {
  //   static int syscall_count = 0;
  //   syscall_count++;
  //
  //   schedule();
  //   // schedule() returns here (possibly as a different task)
  //   // Return to user space
  //   extern void trap_return(struct trap_frame *tf);
  //   trap_return(&current_task->tf);
  // }

  // For all other traps, print registers and panic
  // printk("current_task = %p, pid = %llu\n", current_task, current_task->pid);
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
  uint64_t sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(sstatus));
  sstatus |= SSTATUS_SIE;
  asm volatile("csrw sstatus, %0" :: "r"(sstatus));

  uint64_t sie = (1UL << 9) |
                 (1UL << 5) |
                 (1UL << 1);
  asm volatile("csrw sie, %0" :: "r"(sie));

  uint64_t sip, sstatus_check, sie_check;
  asm volatile("csrr %0, sstatus" : "=r"(sstatus_check));
  asm volatile("csrr %0, sie" : "=r"(sie_check));
  asm volatile("csrr %0, sip" : "=r"(sip));
}

void disable_interrupts(void) {
  uint64_t sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(sstatus));
  sstatus &= ~SSTATUS_SIE;  /* Clear SIE bit to disable interrupts */
  asm volatile("csrw sstatus, %0" :: "r"(sstatus));
}
