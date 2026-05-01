#include "arch/riscv64/sbi.h"
#include "types.h"
#include "lib/printk/printk.h"

void sbi_set_timer(uint64_t stime_value) {
  register uint64_t a0 asm("a0") = stime_value;
  register uint64_t a6 asm("a6") = 0;
  register uint64_t a7 asm("a7") = SBI_EXT_TIME;
  asm volatile("ecall" : "+r"(a0) : "r"(a6), "r"(a7) : "memory");
}

uint64_t read_time(void) {
  uint64_t time;
  asm volatile("rdtime %0" : "=r"(time));
  return time;
}

void init_timer(void) {
  uint64_t current_time = read_time();
  uint64_t next_timer = current_time + TIMER_INTERVAL_CYCLES;
  sbi_set_timer(next_timer);
  printk("Timer initialized: current=%llu, next=%llu\n", current_time, next_timer);
}
