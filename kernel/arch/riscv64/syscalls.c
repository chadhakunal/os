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
    case SYS_getcwd:
      debugk("syscall: getcwd(buf=%llx, size=%llu)\n", tf->a0, tf->a1);
      ret = sys_getcwd(tf);
      break;

    case SYS_chdir:
      debugk("syscall: chdir(path=%llx)\n", tf->a0);
      ret = sys_chdir(tf);
      break;

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
      ret = sys_close(tf);
      break;

    case SYS_lseek:
      debugk("syscall: lseek(fd=%llu, offset=%lld, whence=%llu)\n", tf->a0, (int64_t)tf->a1, tf->a2);
      ret = sys_lseek(tf);
      break;

    case SYS_openat:
      debugk("syscall: openat(dirfd=%lld, pathname=%llx, flags=%llu)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      ret = sys_openat(tf);
      break;

    case SYS_mmap:
      debugk("syscall: mmap(addr=%llx, len=%llu, prot=%llu, flags=%llu, fd=%lld, off=%llu)\n", tf->a0, tf->a1, tf->a2, tf->a3, (int64_t)tf->a4, tf->a5);
      ret = sys_mmap(tf);
      break;

    case SYS_munmap:
      debugk("syscall: munmap(addr=%llx, len=%llu)\n", tf->a0, tf->a1);
      ret = sys_munmap(tf);
      break;

    case SYS_brk:
      debugk("syscall: brk(addr=%llx)\n", tf->a0);
      ret = sys_brk(tf);
      break;

    case SYS_rt_sigaction:
      debugk("syscall: rt_sigaction(sig=%lld, act=%llx, oldact=%llx, sigsetsize=%llu)\n", (int64_t)tf->a0, tf->a1, tf->a2, tf->a3);
      ret = sys_rt_sigaction(tf);
      break;

    case SYS_rt_sigreturn:
      debugk("syscall: rt_sigreturn()\n");
      sys_rt_sigreturn(tf);
      trap_return(&current_task->tf);

    case SYS_exit:
      debugk("syscall: exit(status=%lld)\n", (int64_t)tf->a0);
      tf->sepc += 4;
      sys_exit(tf);  // Never returns
      break;

    case SYS_execve:
      debugk("syscall: execve(pathname=%llx, argv=%llx, envp=%llx)\n", tf->a0, tf->a1, tf->a2);
      sys_execve(tf);
      break;

    case SYS_waitpid:
      debugk("syscall: waitpid(pid=%lld, wstatus=%llx, options=%llu)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      ret = sys_waitpid(tf);
      break;

    case SYS_getpid:
      debugk("syscall: getpid()\n");
      ret = sys_getpid(tf);
      break;

    case SYS_getppid:
      debugk("syscall: getppid()\n");
      ret = sys_getppid(tf);
      break;

    case SYS_kill:
      debugk("syscall: kill(pid=%lld, sig=%lld)\n", (int64_t)tf->a0, (int64_t)tf->a1);
      ret = sys_kill(tf);
      break;

    case SYS_ioctl:
      debugk("syscall: ioctl(fd=%lld, request=%llu, arg=%llx)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      ret = sys_ioctl(tf);
      break;

    case SYS_getdents:
      debugk("syscall: getdents(fd=%lld, buf=%llx, count=%llu)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      ret = sys_getdents(tf);
      break;

    case SYS_mkdirat:
      debugk("syscall: mkdirat(dirfd=%lld, path=%llx, mode=%llu)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      ret = sys_mkdirat(tf);
      break;

    case SYS_unlinkat:
      debugk("syscall: unlinkat(dirfd=%lld, path=%llx, flags=%llu)\n", (int64_t)tf->a0, tf->a1, tf->a2);
      ret = sys_unlinkat(tf);
      break;

    case SYS_dup2:
      debugk("syscall: dup2(oldfd=%lld, newfd=%lld)\n", (int64_t)tf->a0, (int64_t)tf->a1);
      ret = sys_dup2(tf);
      break;

    case SYS_fsync:
      debugk("syscall: fsync(fd=%lld)\n", (int64_t)tf->a0);
      ret = sys_fsync(tf);
      break;

    case SYS_setpgid:
      debugk("syscall: setpgid(pid=%lld, pgid=%lld)\n", (int64_t)tf->a0, (int64_t)tf->a1);
      ret = sys_setpgid(tf);
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

    case SYS_nanosleep:
      debugk("syscall: nanosleep(req=%llx, rem=%llx)\n", tf->a0, tf->a1);
      ret = sys_nanosleep(tf);
      break;

    case SYS_statfs:
      debugk("syscall: statfs(path=%llx, buf=%llx)\n", tf->a0, tf->a1);
      ret = sys_statfs(tf);
      break;

    case SYS_renameat:
      debugk("syscall: renameat(olddirfd=%lld, old=%llx, newdirfd=%lld, new=%llx)\n",
             (int64_t)tf->a0, tf->a1, (int64_t)tf->a2, tf->a3);
      ret = sys_renameat(tf);
      break;

    case SYS_linkat:
      debugk("syscall: linkat(olddirfd=%lld, old=%llx, newdirfd=%lld, new=%llx, flags=%lld)\n",
             (int64_t)tf->a0, tf->a1, (int64_t)tf->a2, tf->a3, (int64_t)tf->a4);
      ret = sys_linkat(tf);
      break;

    case SYS_symlinkat:
      debugk("syscall: symlinkat(target=%llx, newdirfd=%lld, linkpath=%llx)\n",
             tf->a0, (int64_t)tf->a1, tf->a2);
      ret = sys_symlinkat(tf);
      break;

    case SYS_readlinkat:
      debugk("syscall: readlinkat(dirfd=%lld, path=%llx, buf=%llx, bufsiz=%lld)\n",
             (int64_t)tf->a0, tf->a1, tf->a2, (int64_t)tf->a3);
      ret = sys_readlinkat(tf);
      break;

    case SYS_pipe:
      debugk("syscall: pipe(pipefd=%llx)\n", tf->a0);
      ret = sys_pipe(tf);
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
