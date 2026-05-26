#define DEBUG 0
#include "kernel/time/timer.h"
#include "lib/printk/printk.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/panic.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/drivers/virtio-blk.h"
#include "lib/list.h"

struct virtual_time_t virtual_time;

void init_virtual_time() {
  virtual_time.os_ticks = 0;
  virtual_time.system_uptime = 0;
}

void timer_handler(uint64_t hardware_clock_ticks, int from_user_mode) {
  // Check stack pointer to detect stack overflow.
  // Only valid after scheduler_ready: before that we're on the boot stack
  // which is in the kernel code region, not at KERNEL_STACK_VIRTUAL_BASE.
  uint64_t sp;
  asm volatile("mv %0, sp" : "=r"(sp));
  if (scheduler_ready &&
      (sp < KERNEL_STACK_VIRTUAL_BASE || sp > KERNEL_STACK_VIRTUAL_BASE + KERNEL_STACK_SIZE)) {
    panic("timer_handler: Stack overflow! SP out of kernel stack range");
  }

  virtual_time.os_ticks += 1;
  virtual_time.system_uptime += TIMER_INTERVAL_CYCLES;

  /* Everything below requires tasks to exist. */
  if (current_task == NULL)
    return;

  /* Check if a pending virtio I/O completed and unblock the waiting task. */
  virtio_blk_poll();

  /* Wake any tasks whose sleep deadline has passed. */
  struct list_node *node = scheduler.blocked_list->next;
  while (node != scheduler.blocked_list) {
    struct task_t *t = container_of(node, struct task_t, scheduler_list);
    node = node->next;
    if ((t->wait_reason == WAIT_SLEEP ||
         (t->wait_reason == WAIT_SIGNAL && t->sleep_until != 0)) &&
        virtual_time.os_ticks >= t->sleep_until)
      unblock_task(t);
  }

  current_task->runtime += TIMER_INTERVAL_CYCLES;

  /* CPU time accounting. */
  if (current_task == idle_task) {
    virtual_time.cpu_idle++;
  } else if (from_user_mode) {
    current_task->utime++;
    virtual_time.cpu_user++;
  } else {
    current_task->stime++;
    virtual_time.cpu_system++;
  }

  // Only preempt when interrupted from user mode OR when idle is running.
  // Calling switch_to() from within a kernel-mode trap on a real task corrupts
  // the trap frame saved on the kernel stack (trap_from_kernel saves the frame
  // at sp-288; after switch_to() sp points to the new task's stack, so the
  // epilogue restores from the wrong location). Idle is safe because it has no
  // meaningful kernel trap frame to preserve.
  if (from_user_mode || current_task == idle_task) {
    schedule();
    debugk("[TIMER] returned from schedule\n");
  }
}
