#ifndef TIMER_H
#define TIMER_H
#include "types.h"

struct virtual_time_t {
  uint64_t os_ticks; // Total number of ticks since timer enabled (increases 1 every timer interrupt)
};

extern struct virtual_time_t virtual_time;

void init_virtual_time();

void timer_handler(uint64_t hardware_clock_ticks);

#endif
