#include "arch/riscv64/syscalls/syscalls.h"
#include "lib/printk/printk.h"
#include "types.h"
#include "kernel/task/task.h"

// a7:    syscall number
// a0:    arg1 / return value
// a1:    arg2
// a2:    arg3
// a3:    arg4
// a4:    arg5
// a5:    arg6
// sepc:  saved PC — kernel must advance this by 4 past the ecall
// sp:    user stack pointer

void handle_syscall(struct trap_frame *tf) {
  uint64_t ret = -1;
  uint64_t syscall_num = tf->a7;

  // Enable supervisor access to user memory (SUM bit in sstatus)
  // This allows kernel to read/write user buffers during syscalls
  uint64_t old_sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(old_sstatus));
  asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));


  switch (syscall_num) {
    case SYS_read:
      debugk("syscall: read(fd=%llu, buf=%llx, count=%llu)\n", tf->a0, tf->a1, tf->a2);
      ret = sys_read(tf);
      break;

    case SYS_write:
      debugk("syscall: write(fd=%llu, buf=%llx, count=%llu)\n", tf->a0, tf->a1, tf->a2);
      ret = sys_write(tf);
      break;

    case SYS_close:
      debugk("syscall: close(fd=%llu)\n", tf->a0);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_openat:
      debugk("syscall: openat(dirfd=%lld, pathname=%llx, flags=%llu)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      ret = sys_openat(tf);
      break;

    case SYS_mmap:
      debugk("syscall: mmap(addr=%llx, len=%llu, prot=%llu, flags=%llu)\n", tf->a0, tf->a1, tf->a2, tf->a3);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_munmap:
      debugk("syscall: munmap(addr=%llx, len=%llu)\n", tf->a0, tf->a1);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_brk:
      debugk("syscall: brk(addr=%llx)\n", tf->a0);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_rt_sigaction:
      debugk("syscall: rt_sigaction(sig=%lld, act=%llx, oldact=%llx)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_exit:
      debugk("syscall: exit(status=%lld)\n", (int64_t)tf->a0);
      tf->sepc += 4;
      sys_exit(tf);  // Never returns
      break;

    case SYS_execve:
      debugk("syscall: execve(pathname=%llx, argv=%llx, envp=%llx)\n", tf->a0, tf->a1, tf->a2);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_waitpid:
      debugk("syscall: waitpid(pid=%lld, wstatus=%llx, options=%llu)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      ret = sys_waitpid(tf);
      break;

    case SYS_getpid:
      debugk("syscall: getpid()\n");
      tf->a0 = -1; // TODO: return current process PID
      break;

    case SYS_kill:
      debugk("syscall: kill(pid=%lld, sig=%lld)\n", (int64_t)tf->a0, (int64_t)tf->a1);
      tf->a0 = -1; // TODO: implement
      break;
    case SYS_fork:
      debugk("syscall: fork() from PID %llu\n", current_task->pid);
      // Increment sepc BEFORE fork so child gets the incremented value
      tf->sepc += 4;
      ret = sys_fork(tf);
      // fork_off() sets child's tf.a0 = 0, we need to set parent's tf.a0 = child PID
      tf->a0 = ret;
      debugk("fork() returning %llu to parent PID %llu\n", ret, current_task->pid);
      // Don't run the common tf->a0 = ret code at the end, we already did it
      asm volatile("csrw sstatus, %0" :: "r"(old_sstatus));
      return;
      break;

    case SYS_sched_yield:
      debugk("syscall: sched_yield()\n");
      ret = sys_sched_yield(tf);
      break;

    default:
      debugk("syscall: unknown syscall %llu\n", syscall_num);
      tf->a0 = -1; // ENOSYS
      break;
  }

  /* Advance PC past the ecall instruction */
  tf->sepc += 4;
  tf->a0 = ret;

  // Restore original sstatus (disable SUM for security)
  asm volatile("csrw sstatus, %0" :: "r"(old_sstatus));
}
