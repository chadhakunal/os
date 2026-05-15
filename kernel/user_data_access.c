#define DEBUG 0
#include "kernel/user_data_access.h"
#include "kernel/task/task.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "arch/riscv64/trap.h"
#include "lib/string.h"
#include "lib/printk/printk.h"

extern int kernel_fault_recovery_setup(struct kernel_fault_recovery_t *r);
extern void kernel_fault_recovery_jump(struct kernel_fault_recovery_t *r);

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

  if (kernel_fault_recovery_setup(&current_task->fault_recovery)) {
    // Returned via jump from fault handler — invalid user address
    asm volatile("csrw sstatus, %0" :: "r"(old_sstatus));
    return -1;
  }

  memcpy(user_dest, kernel_src, n);

  current_task->fault_recovery.active = 0;
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

  uint64_t old_sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(old_sstatus));
  asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));

  if (kernel_fault_recovery_setup(&current_task->fault_recovery)) {
    // Returned via jump from fault handler — invalid user address
    asm volatile("csrw sstatus, %0" :: "r"(old_sstatus));
    return -1;
  }

  memcpy(kernel_dest, user_src, n);

  current_task->fault_recovery.active = 0;
  asm volatile("csrw sstatus, %0" :: "r"(old_sstatus));
  debugk("copy_from_user: memcpy completed successfully\n");
  return 0;
}
