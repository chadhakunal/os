#ifndef _SYS_STATVFS_H
#define _SYS_STATVFS_H

#include <sys/types.h>

typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;

struct statvfs {
  unsigned long  f_bsize;    /* filesystem block size */
  unsigned long  f_frsize;   /* fragment size */
  fsblkcnt_t     f_blocks;   /* total blocks */
  fsblkcnt_t     f_bfree;    /* free blocks */
  fsblkcnt_t     f_bavail;   /* free blocks available to non-root */
  fsfilcnt_t     f_files;    /* total inodes */
  fsfilcnt_t     f_ffree;    /* free inodes */
  fsfilcnt_t     f_favail;   /* free inodes available to non-root */
  unsigned long  f_fsid;     /* filesystem id */
  unsigned long  f_flag;     /* mount flags */
  unsigned long  f_namemax;  /* maximum filename length */
};

/* f_flag bits */
#define ST_RDONLY  0x0001
#define ST_NOSUID  0x0002

int statvfs(const char *path, struct statvfs *buf);
int fstatvfs(int fd, struct statvfs *buf);

#endif
