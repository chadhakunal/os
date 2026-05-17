#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define PAGE_SIZE   4096
#define NUM_BUCKETS 9
#define HDR_SIZE    sizeof(size_t)

/* Distinct from any bucket size (8..2048) or page-rounded mmap length. */
#define MALLOC_ALIGNED_TAG ((size_t)0xA11CE00A)
#define ALIGNED_META_WORDS 4

static void *aligned_map_alloc(size_t alignment, size_t size, char **out_map,
                               size_t *out_total) {
  size_t need = size + ALIGNED_META_WORDS * HDR_SIZE + alignment - 1;
  size_t total = (need + PAGE_SIZE - 1) & ~(size_t)(PAGE_SIZE - 1);
  char *map = mmap(NULL, total, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  char *p;
  size_t off;
  size_t pad;

  if (map == (void *)-1)
    return NULL;

  p = map + ALIGNED_META_WORDS * HDR_SIZE;
  off = (size_t)(p - map);
  pad = (alignment - (off % alignment)) % alignment;
  p += pad;

  *(size_t *)map = total;
  *out_map = map;
  *out_total = total;
  return p;
}

static const size_t bucket_sizes[NUM_BUCKETS] = {
  8, 16, 32, 64, 128, 256, 512, 1024, 2048
};

static void *free_lists[NUM_BUCKETS];

static int is_aligned_allocation(const void *ptr) {
  return *((const size_t *)ptr - 1) == MALLOC_ALIGNED_TAG;
}

static void *aligned_raw(const void *ptr) {
  return *((void *const *)ptr - 2);
}

static size_t aligned_user_size(const void *ptr) {
  return *((const size_t *)ptr - 4);
}

static size_t aligned_user_align(const void *ptr) {
  return *((const size_t *)ptr - 3);
}

static int bucket_index(size_t size) {
  for (int i = 0; i < NUM_BUCKETS; i++)
    if (size <= bucket_sizes[i]) return i;
  return -1;
}

static void *alloc_slab(size_t size) {
  int idx         = bucket_index(size);
  size_t obj_size = bucket_sizes[idx];
  size_t chunk    = HDR_SIZE + obj_size;
  size_t n        = PAGE_SIZE / chunk;

  char *slab = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (slab == (void *)-1) return NULL;

  for (size_t i = 0; i < n; i++) {
    char *hdr_ptr      = slab + i * chunk;
    char *obj_ptr      = hdr_ptr + HDR_SIZE;
    *(size_t *)hdr_ptr = obj_size;
    *(void **)obj_ptr  = free_lists[idx];
    free_lists[idx]    = obj_ptr;
  }
  return free_lists[idx];
}

void *malloc(size_t size) {
  if (size == 0) return NULL;

  int idx = bucket_index(size);

  if (idx >= 0) {
    if (!free_lists[idx] && !alloc_slab(size)) return NULL;
    void *obj       = free_lists[idx];
    free_lists[idx] = *(void **)obj;
    return obj;
  } else {
    size_t total = (HDR_SIZE + size + PAGE_SIZE - 1) & ~(size_t)(PAGE_SIZE - 1);
    char *p = mmap(NULL, total, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == (void *)-1) return NULL;
    *(size_t *)p = total;
    return p + HDR_SIZE;
  }
}

void free(void *ptr) {
  if (!ptr) return;

  if (is_aligned_allocation(ptr)) {
    char *map = aligned_raw(ptr);
    size_t total = *(size_t *)map;
    munmap(map, total);
    return;
  }

  size_t size_class = *((size_t *)ptr - 1);
  int idx = bucket_index(size_class);

  if (idx < 0) {
    munmap((char *)ptr - HDR_SIZE, size_class);
    return;
  }

  *(void **)ptr   = free_lists[idx];
  free_lists[idx] = ptr;
}

void *calloc(size_t nmemb, size_t size) {
  size_t total = nmemb * size;
  void *ptr = malloc(total);
  if (!ptr) return NULL;
  memset(ptr, 0, total);
  return ptr;
}

void *realloc(void *ptr, size_t size) {
  size_t old_size;
  void *n;

  if (!ptr)
    return malloc(size);
  if (size == 0) {
    free(ptr);
    return NULL;
  }

  if (is_aligned_allocation(ptr)) {
    char *map;
    size_t total;
    char *np;
    size_t align = aligned_user_align(ptr);

    old_size = aligned_user_size(ptr);
    np = aligned_map_alloc(align, size, &map, &total);
    if (!np)
      return NULL;
    if (old_size < size)
      size = old_size;
    memcpy(np, ptr, size);
    free(ptr);
    *((size_t *)np - 4) = size;
    *((size_t *)np - 3) = align;
    *((void **)np - 2) = map;
    *((size_t *)np - 1) = MALLOC_ALIGNED_TAG;
    return np;
  }

  old_size = *((size_t *)ptr - 1);
  if (bucket_index(size) >= 0 && bucket_index(old_size) >= 0 &&
      bucket_index(size) == bucket_index(old_size) && size <= old_size)
    return ptr;

  n = malloc(size);
  if (!n)
    return NULL;

  {
    size_t copy = size;
    size_t avail = old_size;
    if (bucket_index(old_size) < 0)
      avail = old_size - HDR_SIZE;
    if (copy > avail)
      copy = avail;
    memcpy(n, ptr, copy);
  }
  free(ptr);
  return n;
}

void *aligned_alloc(size_t alignment, size_t size) {
  char *map;
  size_t total;
  char *p;

  if (alignment < sizeof(void *) || (alignment & (alignment - 1)))
    return NULL;
  if (size == 0 || (size & (alignment - 1)))
    return NULL;

  p = aligned_map_alloc(alignment, size, &map, &total);
  if (!p)
    return NULL;

  *((size_t *)p - 4) = size;
  *((size_t *)p - 3) = alignment;
  *((void **)p - 2) = map;
  *((size_t *)p - 1) = MALLOC_ALIGNED_TAG;

  return p;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
  size_t rounded;
  void *p;

  if (!memptr)
    return EINVAL;
  if (alignment < sizeof(void *) || (alignment & (alignment - 1)))
    return EINVAL;

  if (size == 0) {
    *memptr = NULL;
    return 0;
  }

  rounded = (size + alignment - 1) & ~(alignment - 1);
  p = aligned_alloc(alignment, rounded);
  if (!p)
    return ENOMEM;

  *memptr = p;
  return 0;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size) {
  if (nmemb != 0 && size > SIZE_MAX / nmemb) {
    errno = ENOMEM;
    return NULL;
  }
  return realloc(ptr, nmemb * size);
}
