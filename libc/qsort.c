#include <stdlib.h>
#include <string.h>

static void swap_elems(char *a, char *b, size_t size, char *tmp) {
  memcpy(tmp, a,   size);
  memcpy(a,   b,   size);
  memcpy(b,   tmp, size);
}

static void qsort_impl(char *lo, size_t nmemb, size_t size,
                       int (*compar)(const void *, const void *),
                       char *tmp) {
  while (nmemb > 1) {
    char *hi    = lo + (nmemb - 1) * size;
    char *pivot = lo + (nmemb / 2) * size;

    /* Move pivot to end to keep the partition loop simple. */
    swap_elems(pivot, hi, size, tmp);
    pivot = hi;

    char *i = lo;
    char *j = hi - size;

    for (;;) {
      while (i <= j && compar(i, pivot) < 0) i += size;
      while (j >= i && compar(j, pivot) > 0) j -= size;
      if (i > j) break;
      if (i != j) swap_elems(i, j, size, tmp);
      i += size;
      j -= size;
    }

    /* Place pivot in its final position. */
    if (i != pivot) swap_elems(i, pivot, size, tmp);

    /* Recurse on the smaller partition; tail-loop on the larger
     * to keep stack depth O(log n). */
    size_t left  = (size_t)((i - lo) / size);
    size_t right = nmemb - left - 1;

    if (left <= right) {
      if (left > 1)
        qsort_impl(lo, left, size, compar, tmp);
      lo    = i + size;
      nmemb = right;
    } else {
      if (right > 1)
        qsort_impl(i + size, right, size, compar, tmp);
      nmemb = left;
    }
  }
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
  if (!base || nmemb <= 1 || size == 0)
    return;

  char *tmp = malloc(size);
  if (!tmp)
    return;

  qsort_impl((char *)base, nmemb, size, compar, tmp);
  free(tmp);
}

/* qsort_r thunk — no threads yet so a file-static context is sufficient. */
static int (*qsort_r_compar)(const void *, const void *, void *);
static void *qsort_r_arg;

static int qsort_r_thunk(const void *a, const void *b) {
  return qsort_r_compar(a, b, qsort_r_arg);
}

void qsort_r(void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *, void *), void *arg) {
  qsort_r_compar = compar;
  qsort_r_arg    = arg;
  qsort(base, nmemb, size, qsort_r_thunk);
}
