#include "kernel/time/timer.h"
#include "lib/printk/printk.h"

struct virtual_time_t virtual_time;

void init_virtual_time() {
  virtual_time.os_ticks = 0;
}

void timer_handler(uint64_t hardware_clock_ticks) {
  virtual_time.os_ticks += 1;

  // Now handle scheduling with the updated os_ticks
}
