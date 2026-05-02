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

    // According to Goldfish RTC spec, read LOW first to latch, then HIGH
    uint32_t time_low = rtc_read(RTC_TIME_LOW);
    uint32_t time_high = rtc_read(RTC_TIME_HIGH);

    return ((uint64_t)time_high << 32) | (uint64_t)time_low;
}

uint64_t rtc_read_time_sec(void) {
    uint64_t time_ns = rtc_read_time_ns();
    return time_ns / 1000000000ULL;
}

void rtc_init(void) {
    // Map RTC physical address to virtual address
    rtc_base = (volatile uint32_t *)MMIO_PHYS_TO_VIRT(RTC_PHYS_BASE);

    printk("RTC: Initialized at phys=0x%llx, virt=%p\n",
           (uint64_t)RTC_PHYS_BASE, rtc_base);

    // Read raw register values for debugging
    uint32_t time_low = rtc_read(RTC_TIME_LOW);
    uint32_t time_high = rtc_read(RTC_TIME_HIGH);
    printk("RTC: Raw registers - LOW=0x%x, HIGH=0x%x\n", time_low, time_high);

    // Read and display current time
    uint64_t time_ns = rtc_read_time_ns();
    uint64_t time_sec = time_ns / 1000000000ULL;

    printk("RTC: Current time: %llu seconds since epoch\n", time_sec);
    printk("RTC: Full 64-bit time_ns = 0x%llx\n", time_ns);
    printk("RTC: time_ns as decimal = %llu nanoseconds\n", time_ns);

    // Also print high and low parts separately for verification
    uint32_t ns_low = (uint32_t)(time_ns & 0xFFFFFFFF);
    uint32_t ns_high = (uint32_t)(time_ns >> 32);
    printk("RTC: time_ns split - HIGH=0x%x, LOW=0x%x\n", ns_high, ns_low);

    // TODO: Convert to human-readable date/time
}
