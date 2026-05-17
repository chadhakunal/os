#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

static unsigned mktemp_seed = 0x12345678u;

static void mktemp_fill(char *tmpl, size_t len) {
  static const char chars[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

  mktemp_seed = mktemp_seed * 1664525u + 1013904223u;
  unsigned r = mktemp_seed;
  for (int i = 0; i < 6; i++) {
    unsigned idx = r % 62;
    r /= 62;
    tmpl[len - 6 + (size_t)i] = chars[idx];
  }
}

static int mktemp_valid(const char *tmpl) {
  size_t len = strlen(tmpl);
  return len >= 6 && strcmp(tmpl + len - 6, "XXXXXX") == 0;
}

int mkstemp(char *tmpl) {
  size_t len;
  int last_errno = EINVAL;

  if (!mktemp_valid(tmpl)) {
    errno = EINVAL;
    return -1;
  }
  len = strlen(tmpl);

  for (int tries = 0; tries < 100; tries++) {
    mktemp_fill(tmpl, len);
    int fd = open(tmpl, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0)
      return fd;
    if (errno)
      last_errno = errno;
  }
  errno = last_errno;
  return -1;
}

int mkostemp(char *tmpl, int flags) {
  size_t len;
  int last_errno = EINVAL;
  int oflags;

  if (!mktemp_valid(tmpl)) {
    errno = EINVAL;
    return -1;
  }
  len = strlen(tmpl);
  oflags = (flags & ~O_ACCMODE) | O_RDWR | O_CREAT | O_EXCL;

  for (int tries = 0; tries < 100; tries++) {
    mktemp_fill(tmpl, len);
    int fd = open(tmpl, oflags, 0600);
    if (fd >= 0)
      return fd;
    if (errno)
      last_errno = errno;
  }
  errno = last_errno;
  return -1;
}

char *mkdtemp(char *tmpl) {
  size_t len;
  int last_errno = EINVAL;

  if (!mktemp_valid(tmpl)) {
    errno = EINVAL;
    return NULL;
  }
  len = strlen(tmpl);

  for (int tries = 0; tries < 100; tries++) {
    mktemp_fill(tmpl, len);
    if (mkdir(tmpl, 0700) == 0)
      return tmpl;
    if (errno)
      last_errno = errno;
  }
  errno = last_errno;
  return NULL;
}

char *realpath(const char *path, char *resolved_path) {
  char buf[PATH_MAX];
  char tmp[PATH_MAX];

  if (!path) {
    errno = EINVAL;
    return NULL;
  }

  if (strcmp(path, ".") == 0) {
    if (!getcwd(buf, sizeof(buf)))
      return NULL;
    path = buf;
  } else if (path[0] != '/') {
    if (!getcwd(tmp, sizeof(tmp)))
      return NULL;
    if (strlen(tmp) + 1 + strlen(path) >= sizeof(buf)) {
      errno = ENAMETOOLONG;
      return NULL;
    }
    strcpy(buf, tmp);
    size_t tlen = strlen(buf);
    if (tlen > 0 && buf[tlen - 1] != '/') {
      buf[tlen++] = '/';
      buf[tlen] = '\0';
    }
    strcat(buf, path);
    path = buf;
  } else {
    if (strlen(path) >= sizeof(buf)) {
      errno = ENAMETOOLONG;
      return NULL;
    }
    strcpy(buf, path);
    path = buf;
  }

  size_t len = strlen(path);
  if (!resolved_path) {
    resolved_path = malloc(len + 1);
    if (!resolved_path) {
      errno = ENOMEM;
      return NULL;
    }
  } else if (len >= PATH_MAX) {
    errno = ENAMETOOLONG;
    return NULL;
  }

  memcpy(resolved_path, path, len + 1);
  return resolved_path;
}
