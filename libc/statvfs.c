#include <sys/statvfs.h>
#include <errno.h>
#include <arch/riscv64/syscall.h>

/* Kernel statfs struct — matches struct vfs_statfs in the kernel. */
struct _statfs {
  long f_type;
  long f_bsize;
  long f_blocks;
  long f_bfree;
  long f_bavail;
  long f_files;
  long f_ffree;
  long f_fsid[2];
  long f_namelen;
  long f_frsize;
  long f_flags;
  long f_spare[4];
};

static void statfs_to_statvfs(const struct _statfs *s, struct statvfs *v) {
  v->f_bsize   = (unsigned long)s->f_bsize;
  v->f_frsize  = s->f_frsize ? (unsigned long)s->f_frsize : (unsigned long)s->f_bsize;
  v->f_blocks  = (fsblkcnt_t)s->f_blocks;
  v->f_bfree   = (fsblkcnt_t)s->f_bfree;
  v->f_bavail  = (fsblkcnt_t)s->f_bavail;
  v->f_files   = (fsfilcnt_t)s->f_files;
  v->f_ffree   = (fsfilcnt_t)s->f_ffree;
  v->f_favail  = (fsfilcnt_t)s->f_ffree;
  v->f_fsid    = (unsigned long)s->f_fsid[0];
  v->f_flag    = (unsigned long)s->f_flags;
  v->f_namemax = (unsigned long)s->f_namelen;
}

int statvfs(const char *path, struct statvfs *buf) {
  if (!path || !buf) { errno = EFAULT; return -1; }
  struct _statfs s;
  long ret = syscall2(SYS_statfs, path, &s);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  statfs_to_statvfs(&s, buf);
  return 0;
}

int fstatvfs(int fd, struct statvfs *buf) {
  /* No fstatfs syscall yet — return ENOSYS. */
  (void)fd; (void)buf;
  errno = ENOSYS;
  return -1;
}
