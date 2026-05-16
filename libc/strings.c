#include <strings.h>
#include <string.h>

int bcmp(const void *s1, const void *s2, size_t n) {
  return memcmp(s1, s2, n);
}

void bcopy(const void *src, void *dest, size_t n) {
  memmove(dest, src, n);
}

void bzero(void *s, size_t n) {
  memset(s, 0, n);
}

void explicit_bzero(void *s, size_t n) {
  volatile unsigned char *p = s;
  while (n--) {
    *p++ = 0;
  }
}

static int ascii_tolower(int c) {
  if (c >= 'A' && c <= 'Z') {
    return c + ('a' - 'A');
  }
  return c;
}

int strcasecmp(const char *s1, const char *s2) {
  while (*s1 && ascii_tolower((unsigned char)*s1) == ascii_tolower((unsigned char)*s2)) {
    s1++;
    s2++;
  }
  return ascii_tolower((unsigned char)*s1) - ascii_tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
  if (n == 0) {
    return 0;
  }
  while (n > 0 && *s1 &&
         ascii_tolower((unsigned char)*s1) == ascii_tolower((unsigned char)*s2)) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0) {
    return 0;
  }
  return ascii_tolower((unsigned char)*s1) - ascii_tolower((unsigned char)*s2);
}
