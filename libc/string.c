#include <stdint.h>
#include <string.h>
#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t len) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  for (size_t i = 0; i < len; i++) {
    d[i] = s[i];
  }
  return dst;
}

void *memset(void *dest, int value, size_t n) {
  unsigned char *d = dest;
  for (size_t i = 0; i < n; i++) {
    d[i] = (unsigned char)value;
  }
  return dest;
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

char *strcpy(char *dst, const char *src) {
  char *ret = dst;
  while ((*dst++ = *src++)) {
    ;
  }
  return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }
  for (; i < n; i++) {
    dst[i] = '\0';
  }
  return dst;
}

char *strchr(const char *s, int c) {
  while (*s != '\0') {
    if (*s == (char)c) {
      return (char *)s;
    }
    s++;
  }
  if ((char)c == '\0') {
    return (char *)s;
  }
  return NULL;
}
