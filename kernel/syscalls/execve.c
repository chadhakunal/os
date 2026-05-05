#define DEBUG 1
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "kernel/task/elf_loader.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"
#include "lib/string.h"
#include "lib/pool_allocator.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/user_data_access.h"
#include "types.h"

#define MAX_ARG_COUNT 16
#define MAX_ARG_LEN 64
#define MAX_PATH_LEN 128

// Structure for execve arguments - must fit in one page (4096 bytes)
// Size: 128 + 8 + (16*64) + (16*64) = 128 + 8 + 1024 + 1024 = 2184 bytes
struct execve_args_t {
  char pathname[MAX_PATH_LEN];
  int argc;
  int envc;
  char argv[MAX_ARG_COUNT][MAX_ARG_LEN];
  char envp[MAX_ARG_COUNT][MAX_ARG_LEN];
};

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
  size_t path_len = 0;
  char c;
  while (path_len < MAX_PATH_LEN - 1) {
    copy_from_user(&c, &pathname[path_len], 1);
    if (c == '\0') break;
    args->pathname[path_len] = c;
    path_len++;
  }
  args->pathname[path_len] = '\0';
  debugk("execve: pathname=%s\n", args->pathname);

  // Copy argv
  debugk("execve: copying argv from %p\n", argv);
  args->argc = 0;
  while (args->argc < MAX_ARG_COUNT) {
    debugk("execve: reading argv[%d] pointer\n", args->argc);
    char *user_arg_ptr;
    copy_from_user(&user_arg_ptr, &argv[args->argc], sizeof(char *));
    debugk("execve: argv[%d] pointer = %p\n", args->argc, user_arg_ptr);
    if (user_arg_ptr == NULL) break;

    size_t arg_len = 0;
    while (arg_len < MAX_ARG_LEN - 1) {
      copy_from_user(&c, &user_arg_ptr[arg_len], 1);
      if (c == '\0') break;
      args->argv[args->argc][arg_len] = c;
      arg_len++;
    }
    args->argv[args->argc][arg_len] = '\0';
    debugk("execve: argv[%d]=%s\n", args->argc, args->argv[args->argc]);
    args->argc++;
  }

  // Copy envp
  args->envc = 0;
  if (envp != NULL) {
    while (args->envc < MAX_ARG_COUNT) {
      char *user_env_ptr;
      copy_from_user(&user_env_ptr, &envp[args->envc], sizeof(char *));
      if (user_env_ptr == NULL) break;

      size_t env_len = 0;
      while (env_len < MAX_ARG_LEN - 1) {
        copy_from_user(&c, &user_env_ptr[env_len], 1);
        if (c == '\0') break;
        args->envp[args->envc][env_len] = c;
        env_len++;
      }
      args->envp[args->envc][env_len] = '\0';
      debugk("execve: envp[%d]=%s\n", args->envc, args->envp[args->envc]);
      args->envc++;
    }
  }

  debugk("execve: argc=%d, envc=%d\n", args->argc, args->envc);

  // Validate ELF file exists and is valid before destroying current address space
  if (validate_elf(args->pathname) != 0) {
    debugk("execve: ELF validation failed for %s\n", args->pathname);
    execve_args_t_free(args);
    return -1;
  }

  // Clear current address space (preserves kernel stack and page table)
  clear_vmas(current_task);
  debugk("execve: cleared vmas\n");

  // Load new ELF executable
  load_elf(current_task, args->pathname);
  debugk("execve: loaded elf, entry=%llx\n", current_task->mm_struct.entry_addr);

  // Set up user stack with argc, argv, envp
  // Stack layout (growing downward from 0x80000000):
  // [high] env strings, arg strings, NULL, envp[], NULL, argv[], argc [low]

  uint64_t sp = 0x80000000;  // DEFAULT_STACK_TOP

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

  // Zero out saved registers for security
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

  // Return to new program (trap_return will jump to sepc)
  extern void trap_return(struct trap_frame *tf);
  trap_return(&current_task->tf);

  // Should never reach here
  panic("execve: returned from trap_return!");
  return 0;
}
