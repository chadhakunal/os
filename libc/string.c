#include <string.h>
#include <types.h>

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
