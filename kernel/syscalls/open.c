#include "syscalls/syscall_macros.h"
#include "syscalls/syscalls.h"
#include "types.h"

DEFINE_SYSCALL(openat, const char *, path, uint64_t, flags, uint64_t, mode)
{
  return 0;
}
