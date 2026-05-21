#define DEBUG 0
#include "lib/printk/printk.h"
#include "kernel/panic.h"
#include "arch/riscv64/trap.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/sbi.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/memory/page_fault.h"
#include "kernel/time/timer.h"
#include "kernel/drivers/plic.h"
#include "kernel/task/signal.h"

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
        if (tf->sstatus & SSTATUS_SPP) {
          return;
        }
        if (current_task->tf.sepc == 0 || current_task->tf.sp == 0) {
          panic("trap_handler: Timer interrupt - corrupted trap frame! sepc=%llx sp=%llx pid=%llu",
                current_task->tf.sepc, current_task->tf.sp, current_task->pid);
        }
        trap_return(&current_task->tf);
        break;
      case 9: {
        extern void handle_uart_interrupt(void);
        uint32_t irq = plic_claim();

        if (irq == PLIC_IRQ_UART) {
          handle_uart_interrupt();
        } else if (irq != 0) {
          debugk("trap: unexpected external IRQ %u\n", irq);
        }

        if (irq != 0)
          plic_complete(irq);

        if (tf->sstatus & SSTATUS_SPP) {
          return;
        }
        trap_return(&current_task->tf);
        break;
      }
      default:
        printk("Interrupt: Unknown interrupt: %llu\n", cause_code);
        break;
    }
  } else {
    switch (cause_code) {
      case 8:
        debugk("Syscall from %llu\n", current_task->pid);
        handle_syscall(tf);
        trap_return(&current_task->tf);
        return;
      case 12:
      case 13:
      case 15:
        handle_page_fault(tf->stval, cause_code, tf);
        if (tf->sstatus & SSTATUS_SPP) {
          return; /* kernel-mode fault: trap_vector restores kernel context */
        }
        trap_return(&current_task->tf);
        return;
      default:
        break;
    }

    /* All remaining exceptions: kill the task if user-mode, panic if kernel. */
    if (tf->sstatus & SSTATUS_SPP) {
      panic("Kernel exception: cause=%llu sepc=0x%llx stval=0x%llx",
            cause_code, tf->sepc, tf->stval);
    }

    int sig;
    switch (cause_code) {
      case 0:  /* instruction address misaligned */
      case 4:  /* load address misaligned */
      case 6:  /* store address misaligned */
        sig = SIGBUS;
        break;
      case 1:  /* instruction access fault */
      case 5:  /* load access fault */
      case 7:  /* store access fault */
        sig = SIGSEGV;
        break;
      case 2:  /* illegal instruction */
        sig = SIGILL;
        break;
      case 3:  /* breakpoint */
        sig = SIGTRAP;
        break;
      case 9:  /* environment call from U-mode (shouldn't reach here, but just in case) */
      default:
        sig = SIGKILL;
        break;
    }

    printk("pid %llu: signal %d (cause=%llu sepc=0x%llx stval=0x%llx)\n",
           current_task->pid, sig, cause_code, tf->sepc, tf->stval);
    send_signal(current_task, sig);
    trap_return(&current_task->tf);
    return;
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
