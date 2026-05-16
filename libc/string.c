#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void *memcpy(void *restrict dst, const void *restrict src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  if (d == s || n == 0) {
    return dst;
  }
  if (d < s) {
    for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
    }
  } else {
    for (size_t i = n; i > 0; i--) {
      d[i - 1] = s[i - 1];
    }
  }
  return dst;
}

void *memset(void *s, int c, size_t n) {
  unsigned char *d = s;
  unsigned char v = (unsigned char)c;
  for (size_t i = 0; i < n; i++) {
    d[i] = v;
  }
  return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *a = s1;
  const unsigned char *b = s2;
  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i]) {
      return (int)a[i] - (int)b[i];
    }
  }
  return 0;
}

void *memchr(const void *s, int c, size_t n) {
  const unsigned char *p = s;
  unsigned char ch = (unsigned char)c;
  for (size_t i = 0; i < n; i++) {
    if (p[i] == ch) {
      return (void *)(p + i);
    }
  }
  return NULL;
}

void *memccpy(void *restrict dst, const void *restrict src, int c, size_t n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  unsigned char ch = (unsigned char)c;

  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
    if (s[i] == ch) {
      return d + i + 1;
    }
  }
  return NULL;
}

void *memmem(const void *haystack, size_t haystacklen, const void *needle,
             size_t needlelen) {
  const unsigned char *h = haystack;
  const unsigned char *n = needle;

  if (needlelen == 0) {
    return (void *)haystack;
  }
  if (needlelen > haystacklen) {
    return NULL;
  }
  for (size_t i = 0; i <= haystacklen - needlelen; i++) {
    if (memcmp(h + i, n, needlelen) == 0) {
      return (void *)(h + i);
    }
  }
  return NULL;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  if (n == 0) {
    return 0;
  }
  while (n > 0 && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0) {
    return 0;
  }
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

size_t strlen(const char *s) {
  size_t len = 0;
  while (s[len]) {
    len++;
  }
  return len;
}

size_t strnlen(const char *s, size_t maxlen) {
  size_t len = 0;
  while (len < maxlen && s[len]) {
    len++;
  }
  return len;
}

char *strcpy(char *restrict dst, const char *restrict src) {
  char *ret = dst;
  while ((*dst++ = *src++)) {
    ;
  }
  return ret;
}

char *stpcpy(char *restrict dst, const char *restrict src) {
  while ((*dst = *src++) != '\0')
    dst++;
  return dst;
}

char *strncpy(char *restrict dst, const char *restrict src, size_t n) {
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }
  for (; i < n; i++) {
    dst[i] = '\0';
  }
  return dst;
}

char *stpncpy(char *restrict dst, const char *restrict src, size_t n) {
  char *d = dst;

  while (n > 0 && *src) {
    *d++ = *src++;
    n--;
  }
  if (n > 0) {
    *d++ = '\0';
    n--;
    while (n > 0) {
      *d++ = '\0';
      n--;
    }
    return d - 1;
  }
  return d;
}

char *strcat(char *restrict dst, const char *restrict src) {
  char *ret = dst;
  while (*dst) {
    dst++;
  }
  while ((*dst++ = *src++)) {
    ;
  }
  return ret;
}

char *strncat(char *restrict dst, const char *restrict src, size_t n) {
  char *ret = dst;
  while (*dst) {
    dst++;
  }
  while (n > 0 && *src) {
    *dst++ = *src++;
    n--;
  }
  *dst = '\0';
  return ret;
}

size_t strlcpy(char *restrict dst, const char *restrict src, size_t size) {
  size_t len = strlen(src);
  if (size == 0) {
    return len;
  }
  size_t copy = len < size - 1 ? len : size - 1;
  memcpy(dst, src, copy);
  dst[copy] = '\0';
  return len;
}

size_t strlcat(char *restrict dst, const char *restrict src, size_t size) {
  size_t dlen = strnlen(dst, size);
  size_t slen = strlen(src);
  if (dlen >= size) {
    return size + slen;
  }
  size_t avail = size - dlen - 1;
  size_t copy = slen < avail ? slen : avail;
  memcpy(dst + dlen, src, copy);
  dst[dlen + copy] = '\0';
  return dlen + slen;
}

char *strchr(const char *s, int c) {
  char ch = (char)c;
  while (*s) {
    if (*s == ch) {
      return (char *)s;
    }
    s++;
  }
  if (ch == '\0') {
    return (char *)s;
  }
  return NULL;
}

char *strrchr(const char *s, int c) {
  char ch = (char)c;
  const char *last = NULL;
  while (*s) {
    if (*s == ch) {
      last = s;
    }
    s++;
  }
  if (ch == '\0') {
    return (char *)s;
  }
  return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
  if (!*needle) {
    return (char *)haystack;
  }
  for (; *haystack; haystack++) {
    if (*haystack != *needle) {
      continue;
    }
    const char *h = haystack;
    const char *n = needle;
    while (*n && *h == *n) {
      h++;
      n++;
    }
    if (!*n) {
      return (char *)haystack;
    }
  }
  return NULL;
}

char *strpbrk(const char *s, const char *accept) {
  for (; *s; s++) {
    for (const char *a = accept; *a; a++) {
      if (*s == *a) {
        return (char *)s;
      }
    }
  }
  return NULL;
}

size_t strspn(const char *s, const char *accept) {
  size_t count = 0;
  for (; *s; s++, count++) {
    int found = 0;
    for (const char *a = accept; *a; a++) {
      if (*s == *a) {
        found = 1;
        break;
      }
    }
    if (!found) {
      break;
    }
  }
  return count;
}

size_t strcspn(const char *s, const char *reject) {
  size_t count = 0;
  for (; *s; s++, count++) {
    for (const char *r = reject; *r; r++) {
      if (*s == *r) {
        return count;
      }
    }
  }
  return count;
}

char *strtok(char *restrict s, const char *restrict sep) {
  static char *saved;
  return strtok_r(s, sep, &saved);
}

char *strtok_r(char *restrict s, const char *restrict sep, char **restrict ptr) {
  if (s) {
    *ptr = s;
  } else if (!*ptr) {
    return NULL;
  }
  s = *ptr;
  while (*s && strchr(sep, *s)) {
    s++;
  }
  if (!*s) {
    *ptr = s;
    return NULL;
  }
  char *start = s;
  while (*s && !strchr(sep, *s)) {
    s++;
  }
  if (*s) {
    *s++ = '\0';
  }
  *ptr = s;
  return start;
}

char *strdup(const char *s) {
  size_t n = strlen(s) + 1;
  char *p = malloc(n);
  if (!p) {
    return NULL;
  }
  memcpy(p, s, n);
  return p;
}

char *strndup(const char *s, size_t n) {
  size_t len = strnlen(s, n);
  char *p = malloc(len + 1);
  if (!p) {
    return NULL;
  }
  memcpy(p, s, len);
  p[len] = '\0';
  return p;
}

int strcoll(const char *s1, const char *s2) {
  return strcmp(s1, s2);
}

size_t strxfrm(char *restrict dst, const char *restrict src, size_t n) {
  size_t len = strlen(src);
  if (n > 0) {
    size_t copy = len < n ? len : n - 1;
    memcpy(dst, src, copy);
    dst[copy] = '\0';
  }
  return len;
}

static const char *errno_messages[] = {
  [0] = "Success",
  [EPERM] = "Operation not permitted",
  [ENOENT] = "No such file or directory",
  [ESRCH] = "No such process",
  [EINTR] = "Interrupted system call",
  [EIO] = "I/O error",
  [ENXIO] = "No such device or address",
  [E2BIG] = "Argument list too long",
  [ENOEXEC] = "Exec format error",
  [EBADF] = "Bad file descriptor",
  [ECHILD] = "No child processes",
  [EAGAIN] = "Try again",
  [ENOMEM] = "Out of memory",
  [EACCES] = "Permission denied",
  [EFAULT] = "Bad address",
  [ENOTBLK] = "Not a block device",
  [EBUSY] = "Device or resource busy",
  [EEXIST] = "File exists",
  [EXDEV] = "Cross-device link",
  [ENODEV] = "No such device",
  [ENOTDIR] = "Not a directory",
  [EISDIR] = "Is a directory",
  [EINVAL] = "Invalid argument",
  [ENFILE] = "File table overflow",
  [EMFILE] = "Too many open files",
  [ENOTTY] = "Not a tty",
  [ETXTBSY] = "Text file busy",
  [EFBIG] = "File too large",
  [ENOSPC] = "No space left on device",
  [ESPIPE] = "Illegal seek",
  [EROFS] = "Read-only file system",
  [EMLINK] = "Too many links",
  [EPIPE] = "Broken pipe",
  [EDOM] = "Numerical argument out of domain",
  [ERANGE] = "Numerical result out of range",
  [EDEADLK] = "Resource deadlock avoided",
  [ENAMETOOLONG] = "File name too long",
  [ENOLCK] = "No locks available",
  [ENOSYS] = "Function not implemented",
  [ENOTEMPTY] = "Directory not empty",
  [ELOOP] = "Too many symbolic links",
};

#define ERRMSG_COUNT (sizeof(errno_messages) / sizeof(errno_messages[0]))

static const char unknown_err[] = "Unknown error";

char *strerror(int errnum) {
  if (errnum >= 0 && (size_t)errnum < ERRMSG_COUNT && errno_messages[errnum]) {
    return (char *)errno_messages[errnum];
  }
  return (char *)unknown_err;
}

int strerror_r(int errnum, char *buf, size_t buflen) {
  const char *msg = strerror(errnum);
  size_t len = strlen(msg);
  if (buflen == 0) {
    return ERANGE;
  }
  size_t copy = len < buflen - 1 ? len : buflen - 1;
  memcpy(buf, msg, copy);
  buf[copy] = '\0';
  if (len >= buflen) {
    return ERANGE;
  }
  return 0;
}

const char *strsignal(int sig) {
  switch (sig) {
  case SIGHUP: return "Hangup";
  case SIGINT: return "Interrupt";
  case SIGQUIT: return "Quit";
  case SIGILL: return "Illegal instruction";
  case SIGTRAP: return "Trace/breakpoint trap";
  case SIGABRT: return "Aborted";
  case SIGBUS: return "Bus error";
  case SIGFPE: return "Arithmetic exception";
  case SIGKILL: return "Killed";
  case SIGUSR1: return "User signal 1";
  case SIGSEGV: return "Segmentation fault";
  case SIGUSR2: return "User signal 2";
  case SIGPIPE: return "Broken pipe";
  case SIGALRM: return "Alarm clock";
  case SIGTERM: return "Terminated";
  case SIGCHLD: return "Child exited";
  default: return "Unknown signal";
  }
}
