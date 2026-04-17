#ifndef KERNEL_TASK
#define KERNEL_TASK

#include "trap.h"
#include "lib/pool_allocator.h"

struct vma_t {
  // TODO: Permissions
  void *start_addr;
  void *end_addr;
  struct vma *next_vma;
}

struct mm_struct_t {
  page_table_t *root_satp;
  struct vma_t *vma_list;
}

struct task_t {
  struct trap_frame tf;
  void *kernel_stack;
  uint64_t pid;
  uint32_t uid;
  struct mm_struct_t mm_struct;
  struct task_t *next_task;
};

DEFINE_POOL(task_t, struct task_t)
DEFINE_POOL(vma_t, struct vma_t)

#endif
