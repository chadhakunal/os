#define DEBUG 0
#include "kernel/time/timer.h"
#include "lib/printk/printk.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/panic.h"
#include "arch/riscv64/virtual_memory_init.h"

struct virtual_time_t virtual_time;

void init_virtual_time() {
  virtual_time.os_ticks = 0;
  virtual_time.system_uptime = 0;
}

void timer_handler(uint64_t hardware_clock_ticks, int from_user_mode) {
  // Check stack pointer to detect stack overflow
  uint64_t sp;
  asm volatile("mv %0, sp" : "=r"(sp));
  if (sp < KERNEL_STACK_VIRTUAL_BASE || sp >= KERNEL_STACK_VIRTUAL_BASE + KERNEL_STACK_SIZE) {
    panic("timer_handler: Stack overflow! SP out of kernel stack range");
  }

  debugk("[TIMER] tick=%llu, PID=%llu, runtime=%llu, sp=%llx, from_user=%d\n",
         virtual_time.os_ticks, current_task->pid, current_task->runtime, sp, from_user_mode);
  virtual_time.os_ticks += 1;
  virtual_time.system_uptime += TIMER_INTERVAL_CYCLES;

  current_task->runtime += TIMER_INTERVAL_CYCLES;

  // Only preempt when interrupted from user mode. Calling switch_to() from
  // within a kernel-mode trap corrupts the trap frame saved on the kernel
  // stack (trap_from_kernel in trap_vector.S saves the frame at sp-288; after
  // switch_to() sp points to the new task's stack, so the epilogue restores
  // from the wrong location).
  if (from_user_mode) {
    schedule();
    debugk("[TIMER] returned from schedule\n");
  }
}
