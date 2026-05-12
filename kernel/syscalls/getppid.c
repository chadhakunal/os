#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"

DEFINE_SYSCALL0(getppid) {
  return current_task->ppid;
}
