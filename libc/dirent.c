#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DIR_BUF_ENTRIES 32

struct DIR {
  int           fd;
  int           owns_fd;
  int           buf_count;
  int           buf_pos;
  int64_t       loc;
  struct dirent buf[DIR_BUF_ENTRIES];
  struct dirent current;
};

static int dir_refill(DIR *dir) {
  dir->buf_pos = 0;
  dir->buf_count = 0;

  int n = getdents(dir->fd, dir->buf, DIR_BUF_ENTRIES);
  if (n < 0)
    return -1;
  dir->buf_count = n;
  return 0;
}

DIR *fdopendir(int fd) {
  if (fd < 0) {
    errno = EBADF;
    return NULL;
  }

  DIR *dir = malloc(sizeof(DIR));
  if (!dir) {
    errno = ENOMEM;
    return NULL;
  }

  dir->fd = fd;
  dir->owns_fd = 1;
  dir->buf_count = 0;
  dir->buf_pos = 0;
  dir->loc = 0;

  if (dir_refill(dir) < 0) {
    int saved = errno;
    free(dir);
    errno = saved;
    return NULL;
  }

  return dir;
}

DIR *opendir(const char *name) {
  if (!name) {
    errno = EINVAL;
    return NULL;
  }

  int fd = open(name, O_RDONLY | O_DIRECTORY);
  if (fd < 0)
    return NULL;

  return fdopendir(fd);
}

int closedir(DIR *dirp) {
  if (!dirp) {
    errno = EINVAL;
    return -1;
  }

  int ret = 0;
  if (dirp->owns_fd && close(dirp->fd) < 0)
    ret = -1;
  free(dirp);
  return ret;
}

struct dirent *readdir(DIR *dirp) {
  if (!dirp) {
    errno = EINVAL;
    return NULL;
  }

  if (dirp->buf_pos >= dirp->buf_count) {
    if (dir_refill(dirp) < 0)
      return NULL;
    if (dirp->buf_count == 0)
      return NULL;
  }

  dirp->current = dirp->buf[dirp->buf_pos++];
  dirp->loc++;
  return &dirp->current;
}

int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result) {
  if (!dirp || !entry || !result) {
    *result = NULL;
    return EINVAL;
  }

  struct dirent *e = readdir(dirp);
  if (!e) {
    *result = NULL;
    if (errno)
      return errno;
    return 0;
  }

  memcpy(entry, e, sizeof(*entry));
  *result = entry;
  return 0;
}

void rewinddir(DIR *dirp) {
  if (!dirp)
    return;
  dirp->loc = 0;
  dirp->buf_pos = 0;
  dirp->buf_count = 0;
  lseek(dirp->fd, 0, SEEK_SET);
}

void seekdir(DIR *dirp, long loc) {
  if (!dirp)
    return;
  if (loc < 0)
    loc = 0;
  dirp->loc = loc;
  dirp->buf_pos = 0;
  dirp->buf_count = 0;
  lseek(dirp->fd, loc, SEEK_SET);
}

long telldir(DIR *dirp) {
  if (!dirp) {
    errno = EINVAL;
    return -1;
  }
  return (long)dirp->loc;
}

int dirfd(DIR *dirp) {
  if (!dirp) {
    errno = EINVAL;
    return -1;
  }
  return dirp->fd;
}

int alphasort(const struct dirent **a, const struct dirent **b) {
  return strcmp((*a)->d_name, (*b)->d_name);
}

static void dirent_ptr_qsort(struct dirent **base, int n,
                             int (*compar)(const struct dirent **,
                                           const struct dirent **)) {
  if (n <= 1)
    return;

  struct dirent *pivot = base[n / 2];
  int i = 0, j = n - 1;

  while (i <= j) {
    while (compar((const struct dirent **)&base[i], (const struct dirent **)&pivot) < 0)
      i++;
    while (compar((const struct dirent **)&base[j], (const struct dirent **)&pivot) > 0)
      j--;
    if (i <= j) {
      struct dirent *tmp = base[i];
      base[i] = base[j];
      base[j] = tmp;
      i++;
      j--;
    }
  }

  if (j > 0)
    dirent_ptr_qsort(base, j + 1, compar);
  if (i < n)
    dirent_ptr_qsort(base + i, n - i, compar);
}

int scandir(const char *dir, struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **,
                          const struct dirent **)) {
  DIR *d = opendir(dir);
  if (!d)
    return -1;

  int cap = 32;
  int count = 0;
  struct dirent **list = malloc((size_t)cap * sizeof(struct dirent *));
  if (!list) {
    closedir(d);
    errno = ENOMEM;
    return -1;
  }

  struct dirent *entry;
  while ((entry = readdir(d)) != NULL) {
    if (filter && !filter(entry))
      continue;

    struct dirent *copy = malloc(sizeof(struct dirent));
    if (!copy) {
      for (int i = 0; i < count; i++)
        free(list[i]);
      free(list);
      closedir(d);
      errno = ENOMEM;
      return -1;
    }
    memcpy(copy, entry, sizeof(struct dirent));

    if (count >= cap) {
      int new_cap = cap * 2;
      struct dirent **grown =
        malloc((size_t)new_cap * sizeof(struct dirent *));
      if (!grown) {
        free(copy);
        for (int i = 0; i < count; i++)
          free(list[i]);
        free(list);
        closedir(d);
        errno = ENOMEM;
        return -1;
      }
      memcpy(grown, list, (size_t)count * sizeof(struct dirent *));
      free(list);
      list = grown;
      cap = new_cap;
    }
    list[count++] = copy;
  }

  closedir(d);

  if (compar && count > 1)
    dirent_ptr_qsort(list, count, compar);

  *namelist = list;
  return count;
}

static size_t posix_dent_reclen(const char *name) {
  size_t need = offsetof(struct posix_dent, d_name) + strlen(name) + 1;
  return (need + 7) & ~(size_t)7;
}

ssize_t posix_getdents(int fildes, void *buf, size_t nbyte, int flags) {
  char *out = buf;
  size_t written = 0;

  (void)flags;

  if (!buf || fildes < 0) {
    errno = EINVAL;
    return -1;
  }

  while (written < nbyte) {
    struct dirent de;
    int n = getdents(fildes, &de, 1);
    if (n < 0)
      return written > 0 ? (ssize_t)written : -1;
    if (n == 0)
      break;

    size_t reclen = posix_dent_reclen(de.d_name);
    if (written + reclen > nbyte) {
      if (lseek(fildes, -1, SEEK_CUR) < 0)
        return -1;
      break;
    }

    struct posix_dent *ent = (struct posix_dent *)(out + written);
    ent->d_ino = de.d_ino;
    ent->d_reclen = (reclen_t)reclen;
    ent->d_type = DT_UNKNOWN;
    strcpy(ent->d_name, de.d_name);
    memset((char *)ent + offsetof(struct posix_dent, d_name) + strlen(de.d_name) + 1,
           0, reclen - (offsetof(struct posix_dent, d_name) + strlen(de.d_name) + 1));

    written += reclen;
  }

  return (ssize_t)written;
}
