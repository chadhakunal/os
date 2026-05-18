#include "kernel/resource.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "lib/list.h"
#include "lib/string.h"
#include "errno.h"

bool rlimit_is_supported(int resource) {
  switch (resource) {
    case RLIMIT_NOFILE:
    case RLIMIT_AS:
    case RLIMIT_FSIZE:
    case RLIMIT_CORE:
    case RLIMIT_STACK:
      return true;
    default:
      return false;
  }
}

void rlimit_init_defaults(struct task_rlimits_t *rl) {
  memset(rl, 0, sizeof(*rl));
  rl->limits[RLIMIT_NOFILE].rlim_cur = DEFAULT_RLIMIT_NOFILE_SOFT;
  rl->limits[RLIMIT_NOFILE].rlim_max = DEFAULT_RLIMIT_NOFILE_HARD;
  rl->limits[RLIMIT_AS].rlim_cur = DEFAULT_RLIMIT_AS_SOFT;
  rl->limits[RLIMIT_AS].rlim_max = DEFAULT_RLIMIT_AS_HARD;
  rl->limits[RLIMIT_FSIZE].rlim_cur = DEFAULT_RLIMIT_FSIZE_SOFT;
  rl->limits[RLIMIT_FSIZE].rlim_max = DEFAULT_RLIMIT_FSIZE_HARD;
  rl->limits[RLIMIT_CORE].rlim_cur = RLIM_INFINITY;
  rl->limits[RLIMIT_CORE].rlim_max = RLIM_INFINITY;
  rl->limits[RLIMIT_STACK].rlim_cur = 8ULL * 1024 * 1024;  /* 8 MB soft */
  rl->limits[RLIMIT_STACK].rlim_max = RLIM_INFINITY;
}

void rlimit_copy(struct task_rlimits_t *dst, const struct task_rlimits_t *src) {
  memcpy(dst, src, sizeof(*dst));
}

uint32_t count_open_fds(struct files_table_t *file_table) {
  uint32_t count = 0;

  if (file_table == NULL)
    return 0;

  list_for_each(&file_table->files_list, pos) {
    struct files_list_t *fl =
      container_of(pos, struct files_list_t, files_list);
    uint32_t bm = fl->used_file_bitmap;
    while (bm) {
      count += bm & 1U;
      bm >>= 1;
    }
  }
  return count;
}

uint64_t count_vma_bytes(const struct mm_struct_t *mm) {
  uint64_t total = 0;

  if (mm == NULL)
    return 0;

  list_for_each(&mm->vma_list, pos) {
    const struct vma_t *vma = container_of(pos, struct vma_t, sibling_vma);
    total += vma->end_addr - vma->start_addr;
  }
  return total;
}

int rlimit_check_nofile(struct task_t *task, uint32_t need) {
  struct rlimit *rl;
  uint32_t open_count;

  if (task == NULL)
    return -ESRCH;

  rl = &task->rlimits.limits[RLIMIT_NOFILE];
  if (rl->rlim_cur == RLIM_INFINITY)
    return 0;

  open_count = count_open_fds(&task->file_table);
  if (open_count + need > rl->rlim_cur)
    return -EMFILE;

  return 0;
}

int rlimit_check_as(struct task_t *task, uint64_t additional_bytes) {
  struct rlimit *rl;
  uint64_t used;

  if (task == NULL)
    return -ESRCH;

  rl = &task->rlimits.limits[RLIMIT_AS];
  if (rl->rlim_cur == RLIM_INFINITY)
    return 0;

  used = count_vma_bytes(&task->mm_struct);
  if (used + additional_bytes > rl->rlim_cur)
    return -ENOMEM;

  return 0;
}

int rlimit_check_fsize(struct task_t *task, uint64_t new_size) {
  struct rlimit *rl;

  if (task == NULL)
    return -ESRCH;

  rl = &task->rlimits.limits[RLIMIT_FSIZE];
  if (rl->rlim_cur == RLIM_INFINITY)
    return 0;

  if (new_size > rl->rlim_cur) {
    send_signal(task, SIGXFSZ);
    return -EFBIG;
  }

  return 0;
}
