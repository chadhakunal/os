#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/task/signal.h"
#include "kernel/time/timer.h"
#include "errno.h"

struct timespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

// RISC-V timer runs at 10 MHz; one tick = TIMER_INTERVAL_CYCLES hardware cycles.
#define TICKS_PER_SEC (10000000ULL / TIMER_INTERVAL_CYCLES)

DEFINE_SYSCALL2(nanosleep, const struct timespec *, req, struct timespec *, rem)
{
  if (!req)
    return -1;

  uint64_t ticks = (uint64_t)req->tv_sec * TICKS_PER_SEC
                 + (uint64_t)req->tv_nsec * TICKS_PER_SEC / 1000000000ULL;

  if (ticks == 0) {
    if (rem) { rem->tv_sec = 0; rem->tv_nsec = 0; }
    return 0;
  }

  current_task->sleep_until = virtual_time.os_ticks + ticks;

  if (!task_block(WAIT_SLEEP)) {
    if (rem) { rem->tv_sec = 0; rem->tv_nsec = 0; }
    return -EINTR;
  }

  if (rem) { rem->tv_sec = 0; rem->tv_nsec = 0; }
  return 0;
}
