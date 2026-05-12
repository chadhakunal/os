#define DEBUG 1
#include "kernel/time/timer.h"
#include "lib/printk/printk.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"

struct virtual_time_t virtual_time;

void init_virtual_time() {
  virtual_time.os_ticks = 0;
  virtual_time.system_uptime = 0;
}

void timer_handler(uint64_t hardware_clock_ticks, bool in_supervisor_mode) {
  virtual_time.os_ticks += 1;
  virtual_time.system_uptime += TIMER_INTERVAL_CYCLES;
  current_task->runtime += TIMER_INTERVAL_CYCLES;

  // Only reschedule if interrupted in user mode
  // If interrupted in supervisor mode (syscall/kernel), defer scheduling until return to user
  if (!in_supervisor_mode) {
    schedule();
  }
}
