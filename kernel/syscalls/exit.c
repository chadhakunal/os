#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/memory/memory_info.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"
#include "types.h"
#include "kernel/memory/page_allocator.h"

DEFINE_SYSCALL1(exit, int, status)
{
  /* Linux wait(2) status: exit code in bits 8..15, low byte 0 for normal exit. */
  int wait_status = (status & 0xff) << 8;

  debugk("exit: PID %llu exiting with status %d (wait %d)\n",
         current_task->pid, status, wait_status);

  task_cleanup(wait_status);
  schedule();

  printk("exit: ERROR - schedule() returned for PID %llu state=%d!\n",
         current_task->pid, current_task->state);
  panic("exit: returned from schedule()!");
  return 0;
}
