#ifndef TIMER_H
#define TIMER_H
#include "types.h"

#define TIMER_INTERVAL_CYCLES 100000

struct virtual_time_t {
  uint64_t os_ticks; // Total number of ticks since timer enabled (increases 1 every timer interrupt)
  uint64_t system_uptime;
  uint64_t cpu_user;   /* ticks any task spent in user mode */
  uint64_t cpu_system; /* ticks any task spent in kernel mode */
  uint64_t cpu_idle;   /* ticks idle task was running */
};

extern struct virtual_time_t virtual_time;

void init_virtual_time();

void timer_handler(uint64_t hardware_clock_ticks, int from_user_mode);

#endif
