#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/trap.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "lib/printk/printk.h"

int64_t sys_rt_sigreturn(struct trap_frame *tf) {
  debugk("syscall: rt_sigreturn() - ENTERED for PID %llu, depth=%u\n",
         current_task->pid, current_task->signal_handler_depth);
  debugk("syscall: rt_sigreturn() - current PC=%llx, SP=%llx, RA=%llx\n",
         tf->sepc, tf->sp, tf->ra);

  if (current_task->signal_handler_depth == 0) {
    debugk("syscall: rt_sigreturn() - ERROR: not in signal handler!\n");
    return -1;
  }

  uint64_t frame_addr = tf->sp;
  debugk("syscall: rt_sigreturn() - copying saved context from frame_addr=%llx\n", frame_addr);

  copy_from_user(&current_task->tf, (void *)frame_addr, sizeof(struct trap_frame));

  uint64_t old_blocked_mask;
  copy_from_user(&old_blocked_mask, (void *)(frame_addr + sizeof(struct trap_frame) + 8), sizeof(uint64_t));
  current_task->signal_state.blocked = old_blocked_mask;

  current_task->signal_handler_depth--;

  printk("syscall: rt_sigreturn() - restored context, sp=%llx, sepc=%llx, ra=%llx, depth=%u\n",
         current_task->tf.sp, current_task->tf.sepc, current_task->tf.ra, current_task->signal_handler_depth);

  return 0;
}
