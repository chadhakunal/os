#include "kernel/time/timer.h"
#include "lib/printk/printk.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/drivers/virtio-blk.h"

struct virtual_time_t virtual_time;

void init_virtual_time() {
  virtual_time.os_ticks = 0;
  virtual_time.system_uptime = 0;
}

void timer_handler(uint64_t hardware_clock_ticks) {
  virtual_time.os_ticks += 1;
  virtual_time.system_uptime += TIMER_INTERVAL_CYCLES;

  /* Check if a pending virtio I/O completed and unblock the waiting task. */
  virtio_blk_poll();

  current_task->runtime += TIMER_INTERVAL_CYCLES;
  schedule();
}
