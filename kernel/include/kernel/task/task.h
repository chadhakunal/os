#ifndef KERNEL_TASK
#define KERNEL_TASK

#include "arch/riscv64/trap.h"
#include "lib/pool_allocator.h"
#include "kernel/memory/page_tables.h"
#include "lib/list.h"
#include "kernel/filesystem/vfs/vfs.h"

// Global task tracking (defined in task.c)
extern struct task_t *current_task;  // Currently running task
extern struct task_t *idle_task;     // Idle task (PID 0)
extern struct task_t *init_task;     // First task (PID 1)
extern struct list_node task_list;   // Global list of all tasks

// VMA flags
#define VM_READ     0x0001
#define VM_WRITE    0x0002
#define VM_EXEC     0x0004
#define VM_SHARED   0x0008

enum task_state {
  TASK_RUNNING,
  TASK_READY,
  TASK_BLOCKED,
  TASK_ZOMBIE,
  TASK_TERMINATED
};

enum wait_reason {
  WAIT_NONE,
  WAIT_CHILD,
  WAIT_IO,
};

struct vma_t {
  size_t start_addr; // Virtual address Start
  size_t end_addr; // Virtual address end (can span multiple pages)
  uint64_t vm_flags; // VM_READ, VM_WRITE, VM_EXEC, etc.
  struct list_node sibling_vma;
  struct vnode_t *backing_file; // NULL if anonymous
  size_t offset; // File offset (in bytes, page-aligned)
};

struct mm_struct_t {
  page_table_t *root_satp;
  void *entry_addr;
  struct list_node vma_list;
};

struct files_table_t {
  struct list_node files_list;
};

struct files_list_t {
  struct file_t *files[32];
  uint32_t used_file_bitmap;
  struct list_node files_list;
};

struct kernel_context_t {
  uint64_t sp;
  uint64_t ra;
  uint64_t s[12];
  uint64_t stack_start;
};

struct task_t {
  struct kernel_context_t kernel_context;
  struct trap_frame tf;
  uint64_t pid;
  uint32_t uid;
  struct mm_struct_t mm_struct;
  struct list_node task_list; // Global task list
  struct files_table_t file_table;
  enum task_state state;
  uint64_t ppid;
  struct list_node scheduler_list; // A task will either be apart of the active, expired or blocked list
  uint64_t runtime; // The total runtime for the task
  uint64_t max_runtime; // max runtime a task can run before being moved to expired

  uint64_t pgid;
  int exit_status;
  enum wait_reason wait_reason;
  int64_t wait_pid;
  struct list_node wait_list;
};

DEFINE_POOL(task_t, struct task_t)
DEFINE_POOL(vma_t, struct vma_t)
DEFINE_POOL(files_list_t, struct files_list_t)
DEFINE_POOL(files_table_t, struct files_table_t)


void create_idle_task();

void create_init_process();

void create_second_task();

uint64_t fork_off();

/* Helper functions for wait/exit */
struct task_t *find_task_by_pid(uint64_t pid);
bool has_alive_children(struct task_t *parent, int64_t specific_pid);
struct task_t *find_zombie_child(struct task_t *parent, int64_t specific_pid);
void reap_zombie(struct task_t *zombie);

/* Set the current task and update tp register */
void set_current_task(struct task_t *task);

/* Switch to a task's page table */
void switch_to_page_table(struct task_t *task);

struct vma_t *find_vma(struct mm_struct_t *mm_struct, size_t vaddr);

int64_t file_backed_memory_map(struct mm_struct_t *mm_struct, size_t vaddr,
                                struct vnode_t *vnode, size_t offset,
                                size_t size, uint64_t vm_flags, bool eager);

int64_t anon_memory_map(struct mm_struct_t *mm_struct, size_t vaddr,
                        size_t size, uint64_t vm_flags, bool eager);

void switch_to(struct task_t *me, struct task_t *next);

struct file_t *find_file(struct files_table_t *file_table, int fd);

void clear_vmas(struct task_t *task);
void close_all_files(struct task_t *task);

#endif
