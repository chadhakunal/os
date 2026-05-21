#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/memory/page_tables.h"
#include "kernel/panic.h"
#include "kernel/signal_jump_point.h"
#include "kernel/task/elf_loader.h"
#include "kernel/task/executable_loader.h"
#include "kernel/task/signal.h"
#include "kernel/task/task.h"
#include "kernel/user_data_access.h"
#include "kernel/filesystem/mode.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "lib/pool_allocator.h"
#include "lib/printk/printk.h"
#include "lib/string.h"
#include "types.h"
#include "errno.h"

#define AT_EMPTY_PATH 0x1000

DEFINE_POOL(execve_args_t, struct execve_args_t)

static struct execve_args_t *execve_copy_user_args(const char *pathname,
                                                   char **argv, char **envp) {
  if (!pathname || !argv)
    return NULL;

  struct execve_args_t *args = execve_args_t_alloc();
  if (!args)
    return NULL;

  copy_string_from_user(args->pathname, pathname, MAX_PATH_LEN);

  args->argc = 0;
  char *user_arg_ptr;
  while (args->argc < MAX_ARG_COUNT) {
    copy_from_user(&user_arg_ptr, &argv[args->argc], sizeof(char *));
    if (user_arg_ptr == NULL)
      break;
    copy_string_from_user(args->argv[args->argc], user_arg_ptr, MAX_ARG_LEN);
    args->argc++;
  }

  args->envc = 0;
  char *user_env_ptr;
  if (envp != NULL) {
    while (args->envc < MAX_ARG_COUNT) {
      copy_from_user(&user_env_ptr, &envp[args->envc], sizeof(char *));
      if (user_env_ptr == NULL)
        break;
      copy_string_from_user(args->envp[args->envc], user_env_ptr, MAX_ARG_LEN);
      args->envc++;
    }
  }

  return args;
}

static int64_t execve_run(struct dentry_t *dentry, struct execve_args_t *args) {
  struct vnode_t *exec_vnode = dentry->vnode->mounted_vnode
                               ? dentry->vnode->mounted_vnode
                               : dentry->vnode;

  if (IS_DIR(exec_vnode->permission_mode)) {
    execve_args_t_free(args);
    return -EISDIR;
  }

  if (!(exec_vnode->permission_mode & PERM_XUSR)) {
    execve_args_t_free(args);
    return -EACCES;
  }

  int file_type = detect_file_type(dentry);
  if (file_type == FILE_TYPE_INVALID) {
    execve_args_t_free(args);
    return -ENOEXEC;
  }

  if (file_type == FILE_TYPE_ELF && validate_elf_dentry(dentry) != 0) {
    execve_args_t_free(args);
    return -ENOEXEC;
  }

  vfs_files_table_close_on_exec(&current_task->file_table);
  clear_vmas(current_task);

  for (size_t i = 0; i < NUM_SIGS; i++) {
    struct sigaction_t *old_action = current_task->signal_state.actions[i];
    if (old_action != NULL &&
        old_action != (struct sigaction_t *)SIG_DEFAULT_HANDLER &&
        old_action != (struct sigaction_t *)SIG_IGNORE) {
      sigaction_t_free(old_action);
    }
    current_task->signal_state.actions[i] =
        (struct sigaction_t *)SIG_DEFAULT_HANDLER;
  }
  current_task->signal_state.pending = 0;
  current_task->signal_handler_depth = 0;

  if (load_executable_dentry(current_task, args, dentry) != 0) {
    execve_args_t_free(args);
    return -ENOEXEC;
  }

  void *sjp = get_signal_jump_point_page();
  if (sjp) {
    map_page(current_task->mm_struct.root_satp, SIGNAL_JUMP_POINT_ADDR,
             (uint64_t)sjp, PTE_VALID | PTE_U | PTE_R | PTE_X);
  }

  uint64_t sp = DEFAULT_STACK_TOP;
  size_t total_size = sizeof(uint64_t);
  total_size += (args->argc + 1) * sizeof(uint64_t);
  total_size += (args->envc + 1) * sizeof(uint64_t);

  for (int i = 0; i < args->argc; i++)
    total_size += str_len(args->argv[i], MAX_ARG_LEN) + 1;
  for (int i = 0; i < args->envc; i++)
    total_size += str_len(args->envp[i], MAX_ARG_LEN) + 1;

  total_size = (total_size + 15) & ~15;
  sp -= total_size;

  uint64_t *user_ptr = (uint64_t *)sp;
  uint64_t current_va = sp;

  copy_to_user(user_ptr++, &args->argc, sizeof(uint64_t));
  current_va += sizeof(uint64_t);

  uint64_t *argv_base = user_ptr;
  user_ptr += (args->argc + 1);
  current_va += (args->argc + 1) * sizeof(uint64_t);

  uint64_t *envp_base = user_ptr;
  user_ptr += (args->envc + 1);
  current_va += (args->envc + 1) * sizeof(uint64_t);

  char *str_ptr = (char *)user_ptr;
  uint64_t str_va = current_va;

  for (int i = 0; i < args->argc; i++) {
    copy_to_user(&argv_base[i], &str_va, sizeof(uint64_t));
    size_t len = str_len(args->argv[i], MAX_ARG_LEN) + 1;
    copy_to_user(str_ptr, args->argv[i], len);
    str_ptr += len;
    str_va += len;
  }
  uint64_t null_ptr = 0;
  copy_to_user(&argv_base[args->argc], &null_ptr, sizeof(uint64_t));

  for (int i = 0; i < args->envc; i++) {
    copy_to_user(&envp_base[i], &str_va, sizeof(uint64_t));
    size_t len = str_len(args->envp[i], MAX_ARG_LEN) + 1;
    copy_to_user(str_ptr, args->envp[i], len);
    str_ptr += len;
    str_va += len;
  }
  copy_to_user(&envp_base[args->envc], &null_ptr, sizeof(uint64_t));

  int argc = args->argc;
  execve_args_t_free(args);

  current_task->tf.sp = sp;
  current_task->tf.sepc = (uint64_t)current_task->mm_struct.entry_addr;
  current_task->tf.a0 = argc;
  current_task->tf.a1 = sp + sizeof(uint64_t);
  current_task->tf.a2 =
      sp + sizeof(uint64_t) + (argc + 1) * sizeof(uint64_t);

  current_task->tf.ra = 0;
  current_task->tf.gp = 0;
  current_task->tf.tp = 0;
  current_task->tf.t0 = 0;
  current_task->tf.t1 = 0;
  current_task->tf.t2 = 0;
  current_task->tf.s0 = 0;
  current_task->tf.s1 = 0;
  current_task->tf.s2 = 0;
  current_task->tf.s3 = 0;
  current_task->tf.s4 = 0;
  current_task->tf.s5 = 0;
  current_task->tf.s6 = 0;
  current_task->tf.s7 = 0;
  current_task->tf.s8 = 0;
  current_task->tf.s9 = 0;
  current_task->tf.s10 = 0;
  current_task->tf.s11 = 0;
  current_task->tf.a3 = 0;
  current_task->tf.a4 = 0;
  current_task->tf.a5 = 0;
  current_task->tf.a6 = 0;
  current_task->tf.a7 = 0;

  extern void trap_return(struct trap_frame *tf);
  trap_return(&current_task->tf);

  panic("execve: returned from trap_return!");
  return 0;
}

DEFINE_SYSCALL3(execve, const char *, pathname, char **, argv, char **, envp) {
  struct execve_args_t *args = execve_copy_user_args(pathname, argv, envp);
  if (!args)
    return -ENOMEM;

  struct dentry_t *dentry;
  if (vfs_resolve_path(args->pathname, &dentry) != 0) {
    execve_args_t_free(args);
    return -ENOENT;
  }

  return execve_run(dentry, args);
}

DEFINE_SYSCALL5(execveat, int, dirfd, const char *, pathname, char **, argv,
                char **, envp, int, flags) {
  if (flags & ~AT_EMPTY_PATH)
    return -EINVAL;

  struct execve_args_t *args = execve_copy_user_args(pathname, argv, envp);
  if (!args)
    return -ENOMEM;

  struct dentry_t *dentry;

  if ((flags & AT_EMPTY_PATH) && pathname && pathname[0] == '\0') {
    struct file_t *file = find_file(&current_task->file_table, dirfd);
    if (file == NULL) {
      execve_args_t_free(args);
      return -EBADF;
    }
    if (file->dentry == NULL) {
      execve_args_t_free(args);
      return -EBADF;
    }
    dentry = file->dentry;
    return execve_run(dentry, args);
  }

  char path[MAX_PATH_LEN];
  copy_string_from_user(path, pathname, MAX_PATH_LEN);

  struct dentry_t *start = task_dirfd_to_dentry(dirfd);
  if (start == (struct dentry_t *)-1) {
    execve_args_t_free(args);
    return -EBADF;
  }

  if (vfs_resolve_path_at(path, start, &dentry, VFS_RESOLVE_FOLLOW_ALL) != 0) {
    execve_args_t_free(args);
    return -ENOENT;
  }

  return execve_run(dentry, args);
}
