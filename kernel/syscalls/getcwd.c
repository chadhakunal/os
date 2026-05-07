#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/user_data_access.h"
#include "types.h"

DEFINE_SYSCALL2(getcwd, char *, buf, size_t, size)
{
  // TODO: Implement getcwd
  // For now, return "/" as a placeholder
  if (size < 2) {
    return -1;
  }

  char root[] = "/";
  copy_string_to_user(buf, root, size);

  return (uint64_t)buf;
}
