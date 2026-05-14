#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "kernel/task/elf_loader.h"
#include "kernel/task/executable_loader.h"
#include "kernel/task/signal.h"
#include "kernel/signal_jump_point.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"
#include "lib/string.h"
#include "lib/pool_allocator.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/user_data_access.h"
#include "kernel/memory/page_tables.h"
#include "types.h"

DEFINE_POOL(execve_args_t, struct execve_args_t)

DEFINE_SYSCALL3(execve, const char *, pathname, char **, argv, char **, envp)
{
  debugk("execve: pathname=%p, argv=%p, envp=%p\n", pathname, argv, envp);

  // Validate pointers
  if (!pathname || !argv) {
    return -1;
  }
  debugk("creating arg struct\n");
  // Allocate structure from pool
  struct execve_args_t *args = execve_args_t_alloc();
  debugk("args = %p\n", args);
  if (!args) {
    debugk("execve: failed to allocate args struct\n");
    return -1;
  }

  // Copy pathname
  debugk("execve: copying pathname from %p\n", pathname);
  copy_string_from_user(args->pathname, pathname, MAX_PATH_LEN);
  debugk("execve: pathname=%s\n", args->pathname);

  // Copy argv
  debugk("execve: copying argv from %p\n", argv);
  args->argc = 0;
  char *user_arg_ptr;
  while (args->argc < MAX_ARG_COUNT) {
    debugk("execve: reading argv[%d] pointer\n", args->argc);
    copy_from_user(&user_arg_ptr, &argv[args->argc], sizeof(char *));
    debugk("execve: argv[%d] pointer = %p\n", args->argc, user_arg_ptr);
    if (user_arg_ptr == NULL) break;

    copy_string_from_user(args->argv[args->argc], user_arg_ptr, MAX_ARG_LEN);
    debugk("execve: argv[%d]=%s\n", args->argc, args->argv[args->argc]);
    args->argc++;
  }

  // Copy envp
  args->envc = 0;
  char *user_env_ptr;
  if (envp != NULL) {
    while (args->envc < MAX_ARG_COUNT) {
      copy_from_user(&user_env_ptr, &envp[args->envc], sizeof(char *));
      if (user_env_ptr == NULL) break;

      copy_string_from_user(args->envp[args->envc], user_env_ptr, MAX_ARG_LEN);
      debugk("execve: envp[%d]=%s\n", args->envc, args->envp[args->envc]);
      args->envc++;
    }
  }

  debugk("execve: argc=%d, envc=%d\n", args->argc, args->envc);

  // Validate file exists and detect type before destroying current address space
  struct dentry_t *dentry;
  if (vfs_resolve_path(args->pathname, &dentry) != 0) {
    debugk("execve: failed to resolve path %s\n", args->pathname);
    execve_args_t_free(args);
    return -1;
  }

  int file_type = detect_file_type(dentry);
  if (file_type == FILE_TYPE_INVALID) {
    debugk("execve: invalid file type for %s\n", args->pathname);
    execve_args_t_free(args);
    return -1;
  }

  // For ELF files, validate before destroying address space
  // For scripts, validation happens during script processing
  if (file_type == FILE_TYPE_ELF && validate_elf(args->pathname) != 0) {
    debugk("execve: ELF validation failed for %s\n", args->pathname);
    execve_args_t_free(args);
    return -1;
  }

  clear_vmas(current_task);
  debugk("execve: cleared vmas\n");

  // Pending Signals cleared and blocked signals stay
  for (size_t i = 0; i < NUM_SIGS; i++) {
    struct sigaction_t *old_action = current_task->signal_state.actions[i];
    if (old_action != NULL &&
        old_action != (struct sigaction_t *)SIG_DEFAULT_HANDLER &&
        old_action != (struct sigaction_t *)SIG_IGNORE) {
      sigaction_t_free(old_action);
    }
    current_task->signal_state.actions[i] = (struct sigaction_t *)SIG_DEFAULT_HANDLER;
  }
  current_task->signal_state.pending = 0;
  current_task->signal_handler_depth = 0;
  debugk("execve: reset signal handlers\n");

  // Load executable (handles both ELF and scripts)
  if (load_executable(current_task, args) != 0) {
    debugk("execve: load_executable failed\n");
    execve_args_t_free(args);
    return -1;
  }
  debugk("execve: loaded executable, entry=%llx\n", current_task->mm_struct.entry_addr);

  // Re-map signal jump point — clear_vmas() removed it along with the old ELF mappings.
  void *sjp = get_signal_jump_point_page();
  if (sjp) {
    map_page(current_task->mm_struct.root_satp, SIGNAL_JUMP_POINT_ADDR,
             (uint64_t)sjp, PTE_VALID | PTE_U | PTE_R | PTE_X);
  }

  // Set up user stack with argc, argv, envp
  // Stack layout (growing downward from 0x80000000):
  // [high] env strings, arg strings, NULL, envp[], NULL, argv[], argc [low]

  uint64_t sp = DEFAULT_STACK_TOP;

  // First, calculate how much space we need
  size_t total_size = sizeof(uint64_t);  // argc
  total_size += (args->argc + 1) * sizeof(uint64_t);  // argv[] + NULL
  total_size += (args->envc + 1) * sizeof(uint64_t);  // envp[] + NULL

  for (int i = 0; i < args->argc; i++) {
    total_size += str_len(args->argv[i], MAX_ARG_LEN) + 1;
  }
  for (int i = 0; i < args->envc; i++) {
    total_size += str_len(args->envp[i], MAX_ARG_LEN) + 1;
  }

  // Align to 16 bytes
  total_size = (total_size + 15) & ~15;
  sp -= total_size;

  uint64_t *user_ptr = (uint64_t *)sp;
  uint64_t current_va = sp;

  // Write argc
  copy_to_user(user_ptr++, &args->argc, sizeof(uint64_t));
  current_va += sizeof(uint64_t);

  // Reserve space for argv[] and envp[] pointers
  uint64_t *argv_base = user_ptr;
  user_ptr += (args->argc + 1);
  current_va += (args->argc + 1) * sizeof(uint64_t);

  uint64_t *envp_base = user_ptr;
  user_ptr += (args->envc + 1);
  current_va += (args->envc + 1) * sizeof(uint64_t);

  char *str_ptr = (char *)user_ptr;
  uint64_t str_va = current_va;

  // Write argv strings and pointers
  for (int i = 0; i < args->argc; i++) {
    copy_to_user(&argv_base[i], &str_va, sizeof(uint64_t));
    size_t len = str_len(args->argv[i], MAX_ARG_LEN) + 1;
    copy_to_user(str_ptr, args->argv[i], len);
    str_ptr += len;
    str_va += len;
  }
  uint64_t null_ptr = 0;
  copy_to_user(&argv_base[args->argc], &null_ptr, sizeof(uint64_t));

  // Write envp strings and pointers
  for (int i = 0; i < args->envc; i++) {
    copy_to_user(&envp_base[i], &str_va, sizeof(uint64_t));
    size_t len = str_len(args->envp[i], MAX_ARG_LEN) + 1;
    copy_to_user(str_ptr, args->envp[i], len);
    str_ptr += len;
    str_va += len;
  }
  copy_to_user(&envp_base[args->envc], &null_ptr, sizeof(uint64_t));

  // Save argc before freeing args
  int argc = args->argc;

  // Free the pool-allocated args structure
  execve_args_t_free(args);

  // Update trap frame for new program
  current_task->tf.sp = sp;
  current_task->tf.sepc = (uint64_t)current_task->mm_struct.entry_addr;
  current_task->tf.a0 = argc;
  current_task->tf.a1 = sp + sizeof(uint64_t);  // Pointer to argv[]
  current_task->tf.a2 = sp + sizeof(uint64_t) + (argc + 1) * sizeof(uint64_t);  // Pointer to envp[]

  debugk("execve: sp=%llx, sepc=%llx\n", current_task->tf.sp, current_task->tf.sepc);
  debugk("execve: a0(argc)=%llu, a1(argv)=%llx, a2(envp)=%llx\n",
         current_task->tf.a0, current_task->tf.a1, current_task->tf.a2);

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

  debugk("execve: done, sp=%llx, sepc=%llx, argc=%llu\n",
         current_task->tf.sp, current_task->tf.sepc, current_task->tf.a0);

  extern void trap_return(struct trap_frame *tf);
  trap_return(&current_task->tf);

  panic("execve: returned from trap_return!");
  return 0;
}
