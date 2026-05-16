#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#define PAGE_SIZE   4096
#define NUM_BUCKETS 9
#define HDR_SIZE    sizeof(size_t)

static const size_t bucket_sizes[NUM_BUCKETS] = {
  8, 16, 32, 64, 128, 256, 512, 1024, 2048
};

static void *free_lists[NUM_BUCKETS];

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
