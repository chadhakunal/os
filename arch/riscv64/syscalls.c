#include "syscalls/syscalls.h"
#include "lib/printk/printk.h"
#include "kernel/syscalls/read.h"
#include "kernel/syscalls/write.h"
#include "kernel/syscalls/open.h"

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

  switch (syscall_num) {
    case SYS_read:
      printk("syscall: read(fd=%llu, buf=%llx, count=%llu)\n", tf->a0, tf->a1, tf->a2);
      tf->a0 = -1; // TODO: implement
      read
      break;

    case SYS_write:
      printk("syscall: write(fd=%llu, buf=%llx, count=%llu)\n", tf->a0, tf->a1, tf->a2);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_close:
      printk("syscall: close(fd=%llu)\n", tf->a0);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_openat:
      printk("syscall: openat(dirfd=%lld, pathname=%llx, flags=%llu)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      ret = sys_openat(tf);
      break;

    case SYS_mmap:
      printk("syscall: mmap(addr=%llx, len=%llu, prot=%llu, flags=%llu)\n", tf->a0, tf->a1, tf->a2, tf->a3);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_munmap:
      printk("syscall: munmap(addr=%llx, len=%llu)\n", tf->a0, tf->a1);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_brk:
      printk("syscall: brk(addr=%llx)\n", tf->a0);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_rt_sigaction:
      printk("syscall: rt_sigaction(sig=%lld, act=%llx, oldact=%llx)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_exit:
      printk("syscall: exit(status=%lld)\n", (int64_t)tf->a0);
      // TODO: terminate process
      tf->a0 = 0;
      break;

    case SYS_execve:
      printk("syscall: execve(pathname=%llx, argv=%llx, envp=%llx)\n", tf->a0, tf->a1, tf->a2);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_wait4:
      printk("syscall: wait4(pid=%lld, wstatus=%llx, options=%llu)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      tf->a0 = -1; // TODO: implement
      break;

    case SYS_getpid:
      printk("syscall: getpid()\n");
      tf->a0 = -1; // TODO: return current process PID
      break;

    case SYS_kill:
      printk("syscall: kill(pid=%lld, sig=%lld)\n", (int64_t)tf->a0, (int64_t)tf->a1);
      tf->a0 = -1; // TODO: implement
      break;

    default:
      printk("syscall: unknown syscall %llu\n", syscall_num);
      tf->a0 = -1; // ENOSYS
      break;
  }

  /* Advance PC past the ecall instruction */
  tf->sepc += 4;
}
