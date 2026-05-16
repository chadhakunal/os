#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/resource.h"
#include "kernel/user_data_access.h"
#include "errno.h"

DEFINE_SYSCALL2(getrlimit, int, resource, struct rlimit *, rlim) {
  if (rlim == NULL)
    return -EFAULT;

  if (resource < 0 || resource >= RLIMIT_NLIMITS)
    return -EINVAL;

  if (!rlimit_is_supported(resource))
    return -EINVAL;

  if (copy_to_user(rlim, &current_task->rlimits.limits[resource],
                   sizeof(struct rlimit)) < 0)
    return -EFAULT;

  return 0;
}
