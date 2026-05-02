#ifndef RTC_H
#define RTC_H

#include "types.h"

#define RTC_TIME_LOW    0x00
#define RTC_TIME_HIGH   0x04

void rtc_init(void);

uint64_t rtc_read_time_ns(void);

uint64_t rtc_read_time_sec(void);

#endif
