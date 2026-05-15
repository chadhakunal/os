#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
  if (ok) { printf("  [PASS] %s\n", name); passed++; }
  else     { printf("  [FAIL] %s\n", name); failed++; }
}

static void test_anon_map_read_write(void) {
  void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  result("mmap returns non-null", p != (void *)-1 && p != NULL);
  if (p == (void *)-1 || p == NULL) return;

  // Write a pattern and read it back
  unsigned char *buf = (unsigned char *)p;
  for (int i = 0; i < 4096; i++) buf[i] = (unsigned char)(i & 0xff);
  int ok = 1;
  for (int i = 0; i < 4096; i++)
    if (buf[i] != (unsigned char)(i & 0xff)) { ok = 0; break; }
  result("write/read pattern across full page", ok);

  result("munmap succeeds", munmap(p, 4096) == 0);
}

static void test_multi_page_map(void) {
  size_t size = 4096 * 4;
  void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  result("multi-page mmap returns valid pointer", p != (void *)-1 && p != NULL);
  if (p == (void *)-1 || p == NULL) return;

  // Touch every page
  unsigned char *buf = (unsigned char *)p;
  for (size_t i = 0; i < size; i += 4096)
    buf[i] = (unsigned char)(i / 4096);

  int ok = 1;
  for (size_t i = 0; i < size; i += 4096)
    if (buf[i] != (unsigned char)(i / 4096)) { ok = 0; break; }
  result("every page in multi-page map is writable", ok);

  result("munmap multi-page succeeds", munmap(p, size) == 0);
}

static void test_zero_initialized(void) {
  void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == (void *)-1 || p == NULL) {
    result("zero-initialized: mmap succeeded", 0);
    return;
  }
  unsigned char *buf = (unsigned char *)p;
  int ok = 1;
  for (int i = 0; i < 4096; i++)
    if (buf[i] != 0) { ok = 0; break; }
  result("anonymous mapping is zero-initialized", ok);
  munmap(p, 4096);
}

static void test_two_independent_maps(void) {
  void *a = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  void *b = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  result("two mmaps return distinct pointers",
         a != (void *)-1 && b != (void *)-1 && a != b);

  if (a != (void *)-1 && b != (void *)-1 && a != b) {
    *(int *)a = 0xdeadbeef;
    *(int *)b = 0xcafebabe;
    result("writes to separate maps don't interfere",
           *(int *)a == 0xdeadbeef && *(int *)b == (int)0xcafebabe);
    munmap(a, 4096);
    munmap(b, 4096);
  }
}

static void test_mmap_string_copy(void) {
  const char *msg = "hello mmap world";
  size_t len = strlen(msg) + 1;
  void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == (void *)-1 || p == NULL) {
    result("string copy: mmap succeeded", 0);
    return;
  }
  memcpy(p, msg, len);
  result("string written and read back via mmap", strcmp((char *)p, msg) == 0);
  munmap(p, 4096);
}

static void test_brk_sbrk(void) {
  void *start = sbrk(0);
  result("sbrk(0) returns valid pointer", start != (void *)-1 && start != NULL);
  if (start == (void *)-1 || start == NULL) return;

  void *p = sbrk(4096);
  result("sbrk(4096) returns old break", p == start);

  void *end = sbrk(0);
  result("sbrk(0) after sbrk(4096) advanced by one page",
         (char *)end == (char *)start + 4096);

  // Write into the newly allocated region
  if (p != (void *)-1) {
    memset(p, 0xab, 4096);
    unsigned char *buf = (unsigned char *)p;
    int ok = 1;
    for (int i = 0; i < 4096; i++)
      if (buf[i] != 0xab) { ok = 0; break; }
    result("sbrk region is writable", ok);
  }
}

static void test_malloc_basic(void) {
  void *p = malloc(64);
  result("malloc(64) returns non-null", p != NULL);
  if (!p) return;
  memset(p, 0x55, 64);
  unsigned char *buf = (unsigned char *)p;
  int ok = 1;
  for (int i = 0; i < 64; i++)
    if (buf[i] != 0x55) { ok = 0; break; }
  result("malloc region is writable", ok);
  free(p);
}

static void test_malloc_multiple(void) {
  void *a = malloc(128);
  void *b = malloc(128);
  void *c = malloc(128);
  result("multiple mallocs return distinct pointers",
         a && b && c && a != b && b != c && a != c);
  if (a && b && c) {
    *(int *)a = 1; *(int *)b = 2; *(int *)c = 3;
    result("multiple malloc regions don't overlap",
           *(int *)a == 1 && *(int *)b == 2 && *(int *)c == 3);
  }
  free(a); free(b); free(c);
}

static void test_calloc(void) {
  int *p = (int *)calloc(16, sizeof(int));
  result("calloc returns non-null", p != NULL);
  if (!p) return;
  int ok = 1;
  for (int i = 0; i < 16; i++)
    if (p[i] != 0) { ok = 0; break; }
  result("calloc region is zero-initialized", ok);
  free(p);
}

int main(void) {
  printf("\n=== mmap / munmap / brk / malloc Tests ===\n\n");

  printf("Anonymous mmap:\n");
  test_anon_map_read_write();
  test_multi_page_map();
  test_zero_initialized();
  test_two_independent_maps();
  test_mmap_string_copy();

  printf("\nbrk / sbrk:\n");
  test_brk_sbrk();

  printf("\nmalloc / calloc:\n");
  test_malloc_basic();
  test_malloc_multiple();
  test_calloc();

  printf("\n=== Test Summary ===\n");
  printf("Passed: %d\n", passed);
  printf("Failed: %d\n", failed);
  printf("\n");

  return failed > 0 ? 1 : 0;
}
