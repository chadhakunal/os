#ifndef SBI_H
#define SBI_H

#include "types.h"
#include "kernel/time/timer.h"

struct trap_frame;

void sbi_set_timer(uint64_t stime_value);
uint64_t read_hardware_timer(void);
void init_timer(void);
void trap_timer_handler(struct trap_frame *tf);

/* SBI System Reset Extension (EID 0x53525354 "SRST"). */
#define SBI_SRST_EID        0x53525354UL
#define SBI_SRST_FID_RESET  0x0UL
#define SBI_RESET_SHUTDOWN  0x00000000UL
#define SBI_RESET_COLD      0x00000001UL
#define SBI_RESET_WARM      0x00000002UL

void sbi_system_reset(uint32_t reset_type, uint32_t reset_reason);

#endif
