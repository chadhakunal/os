#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "types.h"

DEFINE_SYSCALL3(openat, const char *, path, uint64_t, flags, uint64_t, mode)
{
  return 0;
}
