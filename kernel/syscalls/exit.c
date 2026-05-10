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
  debugk("exit: PID %llu exiting with status %d\n", current_task->pid, status);

  task_cleanup(status);
  schedule();

  panic("exit: returned from schedule()!");
  return 0;
}
