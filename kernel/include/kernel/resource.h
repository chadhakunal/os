#ifndef KERNEL_RESOURCE_H
#define KERNEL_RESOURCE_H

#include "types.h"

#define RLIMIT_CPU      0
#define RLIMIT_FSIZE    1
#define RLIMIT_DATA     2
#define RLIMIT_STACK    3
#define RLIMIT_CORE     4
#define RLIMIT_NOFILE   7
#define RLIMIT_AS       9
#define RLIMIT_NLIMITS  10

#define RLIM_INFINITY (~0ULL)

#define DEFAULT_RLIMIT_NOFILE_SOFT 32
#define DEFAULT_RLIMIT_NOFILE_HARD 256

#define DEFAULT_RLIMIT_AS_SOFT   RLIM_INFINITY
#define DEFAULT_RLIMIT_AS_HARD   RLIM_INFINITY

#define DEFAULT_RLIMIT_FSIZE_SOFT RLIM_INFINITY
#define DEFAULT_RLIMIT_FSIZE_HARD RLIM_INFINITY

struct rlimit {
  uint64_t rlim_cur;
  uint64_t rlim_max;
};

struct task_rlimits_t {
  struct rlimit limits[RLIMIT_NLIMITS];
};

struct task_t;
struct files_table_t;
struct mm_struct_t;

bool rlimit_is_supported(int resource);

void rlimit_init_defaults(struct task_rlimits_t *rl);
void rlimit_copy(struct task_rlimits_t *dst, const struct task_rlimits_t *src);

uint32_t count_open_fds(struct files_table_t *file_table);
uint64_t count_vma_bytes(const struct mm_struct_t *mm);

/* Returns 0 if allowed, else negative errno. */
int rlimit_check_nofile(struct task_t *task, uint32_t need);
int rlimit_check_as(struct task_t *task, uint64_t additional_bytes);
int rlimit_check_fsize(struct task_t *task, uint64_t new_size);

#endif
