#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/task/task.h"
#include "lib/list.h"
#include "errno.h"

int64_t vfs_file_close(struct file_table_t *file_table, int fd) {
  if (fd < 0) {
    return -EBADF;
  }

  struct files_list_t *files_list = NULL;
  int local_fd = -1;
  int base_fd = 0;

  list_for_each(&file_table->files_list, pos) {
    files_list = container_of(pos, struct files_list_t, files_list);

    if (fd >= base_fd && fd < base_fd + 32) {
      local_fd = fd - base_fd;
      break;
    }
    base_fd += 32;
    files_list = NULL;
  }

  if (files_list == NULL) {
    return -EBADF;
  }

  if (!(files_list->used_file_bitmap & (1 << local_fd))) {
    return -EBADF;
  }

  struct file_t *file = files_list->files[local_fd];
  if (file == NULL) {
    return -EBADF;
  }

  file->refcount--;

  if (file->refcount == 0) {
    file_t_free(file);
  }

  files_list->used_file_bitmap &= ~(1 << local_fd);
  files_list->files[local_fd] = NULL;

  return 0;
}
