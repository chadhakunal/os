#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/resource.h"
#include "kernel/user_data_access.h"
#include "errno.h"

static bool rlimit_valid(const struct rlimit *rl) {
  if (rl->rlim_cur == RLIM_INFINITY || rl->rlim_max == RLIM_INFINITY)
    return true;
  return rl->rlim_cur <= rl->rlim_max;
}

DEFINE_SYSCALL2(setrlimit, int, resource, const struct rlimit *, rlim) {
  struct rlimit new_rl;
  struct rlimit *cur;

  if (rlim == NULL)
    return -EFAULT;

  if (resource < 0 || resource >= RLIMIT_NLIMITS)
    return -EINVAL;

  if (!rlimit_is_supported(resource))
    return -EINVAL;

  if (copy_from_user(&new_rl, rlim, sizeof(new_rl)) != 0)
    return -EFAULT;

  if (!rlimit_valid(&new_rl))
    return -EINVAL;

  cur = &current_task->rlimits.limits[resource];

  if (new_rl.rlim_max != RLIM_INFINITY && new_rl.rlim_max > cur->rlim_max)
    return -EPERM;

  if (new_rl.rlim_cur != RLIM_INFINITY && new_rl.rlim_cur > cur->rlim_max)
    return -EINVAL;

  if (new_rl.rlim_cur != RLIM_INFINITY && new_rl.rlim_max != RLIM_INFINITY &&
      new_rl.rlim_cur > new_rl.rlim_max)
    return -EINVAL;

  cur->rlim_cur = new_rl.rlim_cur;
  cur->rlim_max = new_rl.rlim_max;
  return 0;
}
