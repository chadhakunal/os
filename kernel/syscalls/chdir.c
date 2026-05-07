#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/user_data_access.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "types.h"

#define MAX_PATH_LEN 256

DEFINE_SYSCALL1(chdir, const char *, user_path)
{
  char path[MAX_PATH_LEN];
  copy_string_from_user(path, user_path, MAX_PATH_LEN);

  // TODO: Implement chdir
  // For now, just return success
  return 0;
}
