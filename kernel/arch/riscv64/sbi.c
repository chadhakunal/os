#include "arch/riscv64/sbi.h"
#include "arch/riscv64/trap.h"
#include "kernel/time/timer.h"
#include "types.h"
#include "lib/printk/printk.h"

void sbi_set_timer(uint64_t stime_value) {
  register uint64_t a0 asm("a0") = stime_value;
  register uint64_t a7 asm("a7") = 0;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
}

uint64_t read_time(void) {
  uint64_t time;
  asm volatile("rdtime %0" : "=r"(time));
  return time;
}

void init_timer(void) {
  uint64_t sie, sip;
  asm volatile("csrr %0, sie" : "=r"(sie));
  asm volatile("csrr %0, sip" : "=r"(sip));

  uint64_t current_time = read_time();
  uint64_t next_timer = current_time + TIMER_INTERVAL_CYCLES;
  sbi_set_timer(next_timer);

  asm volatile("csrr %0, sip" : "=r"(sip));
}

void trap_timer_handler(struct trap_frame *tf) {
  // Reschedule the next timer interrupt (RISC-V/SBI specific)
  uint64_t next_timer = read_time() + TIMER_INTERVAL_CYCLES;
  sbi_set_timer(next_timer);

  timer_handler();
}
