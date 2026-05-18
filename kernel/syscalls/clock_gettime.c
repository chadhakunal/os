#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/drivers/rtc/rtc.h"
#include "kernel/user_data_access.h"
#include "errno.h"
#include "types.h"

struct kernel_timespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

DEFINE_SYSCALL2(clock_gettime, int, clk, struct kernel_timespec *, user_tp)
{
  if (user_tp == NULL) return -EFAULT;

  uint64_t ns = rtc_read_time_ns();

  struct kernel_timespec ts;
  ts.tv_sec  = (int64_t)(ns / 1000000000ULL);
  ts.tv_nsec = (int64_t)(ns % 1000000000ULL);

  if (copy_to_user(user_tp, &ts, sizeof(ts)) != 0) return -EFAULT;
  return 0;
}
