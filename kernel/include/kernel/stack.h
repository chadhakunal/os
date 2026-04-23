#ifndef KERNEL_STACK_H
#define KERNEL_STACK_H

#include "types.h"

/* Get current stack pointer value */
static inline uint64_t get_sp(void) {
  uint64_t sp;
  asm volatile("mv %0, sp" : "=r"(sp));
  return sp;
}

/* Check if current stack pointer is within bounds
 * Returns 0 if OK, -1 if overflow detected */
int check_stack_bounds(uint64_t stack_bottom, uint64_t stack_top);

/* Print current stack usage */
void print_stack_usage(const char *label, uint64_t stack_bottom, uint64_t stack_top);

/* Panic if stack has overflowed */
void assert_stack_valid(uint64_t stack_bottom, uint64_t stack_top);

#endif
