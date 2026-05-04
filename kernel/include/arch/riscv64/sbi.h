#ifndef SBI_H
#define SBI_H

#include "types.h"

#define TIMER_INTERVAL_CYCLES 100000

struct trap_frame;

void sbi_set_timer(uint64_t stime_value);
uint64_t read_hardware_timer(void);
void init_timer(void);
void trap_timer_handler(struct trap_frame *tf);

#endif
