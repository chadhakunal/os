#define DEBUG 0
#include "kernel/user_data_access.h"
#include "kernel/task/task.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "arch/riscv64/trap.h"
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

  // Enable supervisor access to user memory (SUM bit in sstatus)
  uint64_t old_sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(old_sstatus));
  asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));

  // memcpy may trigger page faults on unmapped but valid pages (lazy allocation)
  // Page fault handler will allocate pages as needed or send SIGSEGV if invalid
  memcpy(user_dest, kernel_src, n);

  // Restore original sstatus (disable SUM for security)
  asm volatile("csrw sstatus, %0" :: "r"(old_sstatus));

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

  // Enable supervisor access to user memory (SUM bit in sstatus)
  uint64_t old_sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(old_sstatus));
  asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));

  // memcpy may trigger page faults on unmapped but valid pages (lazy allocation)
  // Page fault handler will allocate pages as needed or send SIGSEGV if invalid
  memcpy(kernel_dest, user_src, n);

  // Restore original sstatus (disable SUM for security)
  asm volatile("csrw sstatus, %0" :: "r"(old_sstatus));

  debugk("copy_from_user: memcpy completed successfully\n");
  return 0;
}
