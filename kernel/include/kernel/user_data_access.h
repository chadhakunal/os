#ifndef USER_DATA_ACCESS_H
#define USER_DATA_ACCESS_H

#define DEBUG 1
#include "types.h"
#include "lib/string.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "lib/printk/printk.h"

// Validate that address range is in user space (< END_USER_SPACE_ADDR)
static inline bool is_user_address(uint64_t addr, size_t len) {
  if (addr >= END_USER_SPACE_ADDR) {
    return false;
  }
  // Check for overflow and ensure end address is also in user space
  if (addr + len < addr || addr + len > END_USER_SPACE_ADDR) {
    return false;
  }
  return true;
}

static inline int copy_to_user(void *user_dest, const void *kernel_src, size_t n) {
  if (!is_user_address((uint64_t)user_dest, n)) {
    return -1;
  }
  // memcpy may trigger page faults on unmapped but valid pages (lazy allocation)
  // Page fault handler will allocate pages as needed or send SIGSEGV if invalid
  memcpy(user_dest, kernel_src, n);
  return 0;
}

static inline int copy_from_user(void *kernel_dest, const void *user_src, size_t n) {
  extern struct task_t *current_task;

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

static inline int copy_string_from_user(char *kernel_dest, const char *user_src, size_t max_len) {
  size_t i = 0;
  while (i < max_len - 1) {
    copy_from_user(&kernel_dest[i], &user_src[i], 1);
    if (kernel_dest[i] == '\0') {
      return i;
    }
    i++;
  }
  kernel_dest[i] = '\0';
  return i;
}

static inline int copy_string_to_user(char *user_dest, const char *kernel_src, size_t max_len) {
  size_t i = 0;
  while (i < max_len - 1 && kernel_src[i] != '\0') {
    copy_to_user(&user_dest[i], &kernel_src[i], 1);
    i++;
  }
  char null_term = '\0';
  copy_to_user(&user_dest[i], &null_term, 1);
  return i;
}

#endif
