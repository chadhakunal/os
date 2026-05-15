#define DEBUG 0
#include "kernel/user_data_access.h"
#include "kernel/task/task.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "arch/riscv64/trap.h"
#include "lib/string.h"
#include "lib/printk/printk.h"

static bool is_user_address(uint64_t addr, size_t len) {
  if (addr >= END_USER_SPACE_ADDR) {
    return false;
  }
  if (addr + len < addr || addr + len > END_USER_SPACE_ADDR) {
    return false;
  }
  return true;
}

int copy_to_user(void *user_dest, const void *kernel_src, size_t n) {
  if (!is_user_address((uint64_t)user_dest, n)) {
    return -1;
  }

  uint64_t old_sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(old_sstatus));
  asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));

  memcpy(user_dest, kernel_src, n);

  asm volatile("csrw sstatus, %0" :: "r"(old_sstatus));
  return 0;
}

int copy_from_user(void *kernel_dest, const void *user_src, size_t n) {
  if (!is_user_address((uint64_t)user_src, n)) {
    return -1;
  }

  uint64_t old_sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(old_sstatus));
  asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));

  memcpy(kernel_dest, user_src, n);

  asm volatile("csrw sstatus, %0" :: "r"(old_sstatus));
  return 0;
}
