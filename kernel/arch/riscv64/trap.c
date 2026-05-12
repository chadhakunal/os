#define DEBUG 0
#include "lib/printk/printk.h"
#include "kernel/panic.h"
#include "arch/riscv64/trap.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/sbi.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/memory/page_fault.h"
#include "kernel/time/timer.h"

void trap_handler(struct trap_frame *tf) {
  uint64_t cause_code = tf->scause & 0x7FFFFFFFFFFFFFFF;
  bool is_interrupt = (tf->scause >> 63) & 1;
  extern void trap_return(struct trap_frame *tf);

  if (is_interrupt) {
    switch (cause_code) {
      case 1:
        printk("Interrupt: Supervisor software interrupt\n");
        break;
      case 5:
        trap_timer_handler(tf);
        // If interrupted in supervisor mode, just return - don't reschedule
        // Rescheduling will happen when we return to user mode
        if (tf->sstatus & SSTATUS_SPP) {
          return;
        }
        // If interrupted in user mode, reschedule and return to user
        trap_return(&current_task->tf);
        break;
      case 9:
        extern void handle_uart_interrupt(void);
        handle_uart_interrupt();
        if (tf->sstatus & SSTATUS_SPP) {
          return;
        }
        trap_return(&current_task->tf);
        break;
      default:
        printk("Interrupt: Unknown interrupt: %llu\n", cause_code);
        break;
    }
  } else {
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
        debugk("Syscall from %llu\n", current_task->pid);
        handle_syscall(tf);
        trap_return(&current_task->tf);
        break;
      case 9:
        printk("Environment call from S-mode\n");
        break;
      case 12:
      case 13:
      case 15:
        handle_page_fault(tf->stval, cause_code, tf);
        trap_return(&current_task->tf);
        break;
      default:
        printk("Unknown exception: %llu\n", cause_code);
        break;
    }
  }

  panic("Unhandled trap");
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
