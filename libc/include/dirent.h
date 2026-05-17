#ifndef DIRENT_H
#define DIRENT_H

#include <stdint.h>
#include <stddef.h>

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12

typedef uint16_t reclen_t;

struct dirent {
  uint32_t d_ino;
  char     d_name[256];
};

struct posix_dent {
  uint32_t      d_ino;
  reclen_t      d_reclen;
  unsigned char d_type;
  char          d_name[1];
};

typedef struct DIR DIR;

DIR           *opendir(const char *name);
DIR           *fdopendir(int fd);
int            closedir(DIR *dirp);
struct dirent *readdir(DIR *dirp);
int            readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
void           rewinddir(DIR *dirp);
void           seekdir(DIR *dirp, long loc);
long           telldir(DIR *dirp);
int            dirfd(DIR *dirp);
int            scandir(const char *dir, struct dirent ***namelist,
                       int (*filter)(const struct dirent *),
                       int (*compar)(const struct dirent **,
                                     const struct dirent **));
int            alphasort(const struct dirent **a, const struct dirent **b);
int            getdents(int fd, struct dirent *buf, unsigned int count);
ssize_t        posix_getdents(int fildes, void *buf, size_t nbyte, int flags);


#endif
