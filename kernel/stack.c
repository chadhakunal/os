#include "kernel/stack.h"
#include "lib/printk/printk.h"
#include "kernel/panic.h"

int check_stack_bounds(uint64_t stack_bottom, uint64_t stack_top) {
  uint64_t sp = get_sp();

  /* Stack grows downward, so:
   * stack_top is the highest address (start)
   * stack_bottom is the lowest address (end)
   * Valid range: stack_bottom <= sp <= stack_top */

  if (sp < stack_bottom || sp > stack_top) {
    return -1; /* Overflow/underflow */
  }
  return 0;
}

void print_stack_usage(const char *label, uint64_t stack_bottom, uint64_t stack_top) {
  uint64_t sp = get_sp();
  uint64_t stack_size = stack_top - stack_bottom;
  uint64_t used = stack_top - sp;
  uint64_t remaining = sp - stack_bottom;

  printk("[STACK %s] sp=0x%lx, size=%lu bytes, used=%lu bytes (%lu%%), remaining=%lu bytes\n",
         label, sp, stack_size, used, (used * 100) / stack_size, remaining);

  if (remaining < 1024) {
    printk("  WARNING: Less than 1KB of stack remaining!\n");
  }
}

void assert_stack_valid(uint64_t stack_bottom, uint64_t stack_top) {
  uint64_t sp = get_sp();

  if (sp < stack_bottom) {
    printk("STACK OVERFLOW! sp=0x%lx, stack_bottom=0x%lx, overflow by %ld bytes\n",
           sp, stack_bottom, stack_bottom - sp);
    panic("Stack overflow detected");
  }

  if (sp > stack_top) {
    printk("STACK UNDERFLOW! sp=0x%lx, stack_top=0x%lx\n", sp, stack_top);
    panic("Stack underflow detected");
  }
}
