#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/time/timer.h"
#include "kernel/user_data_access.h"
#include "errno.h"
#include "types.h"

struct kernel_tms {
  int64_t tms_utime;
  int64_t tms_stime;
  int64_t tms_cutime;
  int64_t tms_cstime;
};

/* times() — returns elapsed ticks since boot, fills tms if non-NULL.
 * Tick rate is TICKS_PER_SEC (100 Hz), matching CLK_TCK. */
DEFINE_SYSCALL1(times, struct kernel_tms *, user_buf)
{
  if (user_buf != NULL) {
    struct kernel_tms tms;
    tms.tms_utime  = (int64_t)current_task->utime;
    tms.tms_stime  = (int64_t)current_task->stime;
    tms.tms_cutime = 0;
    tms.tms_cstime = 0;
    if (copy_to_user(user_buf, &tms, sizeof(tms)) != 0)
      return -EFAULT;
  }
  return (int64_t)virtual_time.os_ticks;
}
