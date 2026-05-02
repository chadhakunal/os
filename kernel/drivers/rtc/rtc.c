#include "kernel/drivers/rtc/rtc.h"
#include "lib/printk/printk.h"
#include "platform.h"
#include "types.h"

static volatile uint32_t *rtc_base = NULL;

static inline uint32_t rtc_read(uint32_t offset) {
    return rtc_base[offset / 4];
}

static inline void rtc_write(uint32_t offset, uint32_t value) {
    rtc_base[offset / 4] = value;
}

uint64_t rtc_read_time_ns(void) {
    if (!rtc_base) {
        return 0;
    }

    uint32_t time_high = rtc_read(RTC_TIME_HIGH);
    uint32_t time_low = rtc_read(RTC_TIME_LOW);

    return ((uint64_t)time_high << 32) | time_low;
}

uint64_t rtc_read_time_sec(void) {
    uint64_t time_ns = rtc_read_time_ns();
    return time_ns / 1000000000ULL;
}

void rtc_init(void) {
    if (platform.rtc.base == 0) {
        printk("RTC: No RTC device found in device tree\n");
        return;
    }

    rtc_base = (volatile uint32_t *)platform.rtc.base;

    printk("RTC: Initialized at 0x%llx\n", platform.rtc.base);

    // Read and display current time
    uint64_t time_ns = rtc_read_time_ns();
    uint64_t time_sec = time_ns / 1000000000ULL;

    printk("RTC: Current time: %llu seconds since epoch\n", time_sec);
    printk("RTC: Current time: %llu nanoseconds since epoch\n", time_ns);

    // TODO: Convert to human-readable date/time
}
