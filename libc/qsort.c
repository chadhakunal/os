#include <stdlib.h>
#include <string.h>

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
  if (!base || nmemb <= 1 || size == 0)
    return;

  char *lo = (char *)base;
  char *hi = lo + (nmemb - 1) * size;
  char *pivot = lo + (nmemb / 2) * size;
  char tmp[256];

  if (size > sizeof(tmp))
    return;

  memcpy(tmp, pivot, size);

  char *i = lo;
  char *j = hi;

  for (;;) {
    while (i <= j && compar(i, tmp) < 0)
      i += size;
    while (j >= i && compar(j, tmp) > 0)
      j -= size;
    if (i > j)
      break;
    if (i != j) {
      char swap[256];
      memcpy(swap, i, size);
      memcpy(i, j, size);
      memcpy(j, swap, size);
    }
    i += size;
    j -= size;
  }

  size_t left_count = (size_t)((i - lo) / size);
  if (left_count > 1)
    qsort(lo, left_count, size, compar);
  size_t right_start = (size_t)((i - lo) / size);
  if (nmemb - right_start > 1)
    qsort(lo + right_start * size, nmemb - right_start, size, compar);
}
