#include <stdlib.h>

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *)) {
  if (!base || !key || !compar || nmemb == 0 || size == 0)
    return NULL;

  const char *lo = (const char *)base;
  const char *hi = lo + (nmemb - 1) * size;

  while (lo <= hi) {
    size_t mid = (size_t)((hi - lo) / size / 2);
    const void *p = lo + mid * size;
    int cmp = compar(key, p);

    if (cmp == 0)
      return (void *)p;
    if (cmp < 0) {
      if (p == base)
        break;
      hi = (const char *)p - size;
    } else {
      lo = (const char *)p + size;
    }
  }

  return NULL;
}
