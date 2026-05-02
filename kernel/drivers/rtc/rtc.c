#include "kernel/drivers/rtc/rtc.h"
#include "lib/printk/printk.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "platform.h"
#include "types.h"

#define RTC_PHYS_BASE 0x101000UL

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

    uint32_t time_low = rtc_read(RTC_TIME_LOW);
    uint32_t time_high = rtc_read(RTC_TIME_HIGH);

    return ((uint64_t)time_high << 32) | (uint64_t)time_low;
}

uint64_t rtc_read_time_sec(void) {
    uint64_t time_ns = rtc_read_time_ns();
    return time_ns / 1000000000ULL;
}

void rtc_init(void) {
    rtc_base = (volatile uint32_t *)MMIO_PHYS_TO_VIRT(RTC_PHYS_BASE);

    uint64_t time_ns = rtc_read_time_ns();
    uint64_t time_sec = time_ns / 1000000000ULL;

    printk("Initialized RTC: Current time: %llu seconds since epoch\n", time_sec);
    // TODO: Convert to human-readable date/time
}
