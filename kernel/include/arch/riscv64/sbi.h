#ifndef SBI_H
#define SBI_H

#include "types.h"

#define SBI_EXT_TIME 0x54494D45
#define TIMER_INTERVAL_CYCLES 100000

// Forward declaration
struct trap_frame;

void sbi_set_timer(uint64_t stime_value);
uint64_t read_time(void);
void init_timer(void);
void trap_timer_handler(struct trap_frame *tf);

#endif
