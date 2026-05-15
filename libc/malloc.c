#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

static void *heap_ptr = (void *)0;

void *malloc(size_t size) {
  if (size == 0) return (void *)0;

  if (heap_ptr == (void *)0)
    heap_ptr = sbrk(0);

  // Align size to 8 bytes
  size = (size + 7) & ~(size_t)7;

  void *ptr = sbrk((long)size);
  if (ptr == (void *)-1) return (void *)0;

  return ptr;
}

void free(void *ptr) {
  (void)ptr;
}

void *calloc(size_t nmemb, size_t size) {
  size_t total = nmemb * size;
  void *ptr = malloc(total);
  if (!ptr) return (void *)0;
  memset(ptr, 0, total);
  return ptr;
}
