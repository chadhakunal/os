#define DEBUG 0
#include "kernel/user_data_access.h"
#include "kernel/task/task.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "lib/string.h"
#include "lib/printk/printk.h"

// Validate that address range is in user space (< END_USER_SPACE_ADDR)
static bool is_user_address(uint64_t addr, size_t len) {
  if (addr >= END_USER_SPACE_ADDR) {
    return false;
  }
  // Check for overflow and ensure end address is also in user space
  if (addr + len < addr || addr + len > END_USER_SPACE_ADDR) {
    return false;
  }
  return true;
}

int copy_to_user(void *user_dest, const void *kernel_src, size_t n) {
  if (!is_user_address((uint64_t)user_dest, n)) {
    return -1;
  }
  // memcpy may trigger page faults on unmapped but valid pages (lazy allocation)
  // Page fault handler will allocate pages as needed or send SIGSEGV if invalid
  memcpy(user_dest, kernel_src, n);
  return 0;
}

int copy_from_user(void *kernel_dest, const void *user_src, size_t n) {
  debugk("copy_from_user: PID=%llu, user_src=0x%llx, n=%llu\n",
         current_task->pid, (uint64_t)user_src, (uint64_t)n);

  if (!is_user_address((uint64_t)user_src, n)) {
    debugk("copy_from_user: Address validation FAILED\n");
    return -1;
  }

  debugk("copy_from_user: Address validation OK, calling memcpy\n");
  // memcpy may trigger page faults on unmapped but valid pages (lazy allocation)
  // Page fault handler will allocate pages as needed or send SIGSEGV if invalid
  memcpy(kernel_dest, user_src, n);
  debugk("copy_from_user: memcpy completed successfully\n");
  return 0;
}
