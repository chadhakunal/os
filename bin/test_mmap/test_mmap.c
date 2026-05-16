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

/* -----------------------------------------------------------------------
 * Basic mmap / munmap
 * -------------------------------------------------------------------- */
static void test_anon_map_read_write(void) {
  void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  result("mmap returns valid pointer", p != (void *)-1 && p != NULL);
  if (p == (void *)-1 || !p) return;

  unsigned char *buf = (unsigned char *)p;
  for (int i = 0; i < 4096; i++) buf[i] = (unsigned char)(i & 0xff);
  int ok = 1;
  for (int i = 0; i < 4096; i++)
    if (buf[i] != (unsigned char)(i & 0xff)) { ok = 0; break; }
  result("write/read pattern across full page", ok);
  result("munmap succeeds", munmap(p, 4096) == 0);
}

static void test_zero_initialized(void) {
  void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == (void *)-1 || !p) { result("zero-init: mmap succeeded", 0); return; }
  unsigned char *buf = (unsigned char *)p;
  int ok = 1;
  for (int i = 0; i < 4096; i++)
    if (buf[i] != 0) { ok = 0; break; }
  result("anonymous mapping is zero-initialized", ok);
  munmap(p, 4096);
}

/* -----------------------------------------------------------------------
 * malloc: basic correctness
 * -------------------------------------------------------------------- */
static void test_malloc_returns_non_null(void) {
  void *p = malloc(1);
  result("malloc(1) returns non-null", p != NULL);
  free(p);
}

static void test_malloc_zero(void) {
  void *p = malloc(0);
  result("malloc(0) returns NULL", p == NULL);
}

static void test_malloc_writable(void) {
  int *p = (int *)malloc(sizeof(int) * 64);
  result("malloc region is writable", p != NULL);
  if (!p) return;
  for (int i = 0; i < 64; i++) p[i] = i;
  int ok = 1;
  for (int i = 0; i < 64; i++)
    if (p[i] != i) { ok = 0; break; }
  result("malloc region read/write correct", ok);
  free(p);
}

static void test_malloc_distinct_pointers(void) {
  void *a = malloc(64), *b = malloc(64), *c = malloc(64);
  result("three mallocs return distinct pointers",
         a && b && c && a != b && b != c && a != c);
  free(a); free(b); free(c);
}

static void test_malloc_no_overlap(void) {
  int *a = malloc(sizeof(int));
  int *b = malloc(sizeof(int));
  if (!a || !b) { result("no-overlap allocs succeeded", 0); return; }
  *a = 0xdeadbeef;
  *b = 0xcafebabe;
  result("adjacent mallocs don't overlap",
         *a == (int)0xdeadbeef && *b == (int)0xcafebabe);
  free(a); free(b);
}

/* -----------------------------------------------------------------------
 * malloc: recycling (free list)
 * -------------------------------------------------------------------- */
static void test_free_recycles(void) {
  void *a = malloc(64);
  result("first malloc(64) succeeds", a != NULL);
  if (!a) return;
  free(a);
  void *b = malloc(64);
  result("malloc(64) after free returns same address", a == b);
  free(b);
}

static void test_free_recycles_multiple(void) {
  void *ptrs[8];
  for (int i = 0; i < 8; i++) ptrs[i] = malloc(32);
  for (int i = 0; i < 8; i++) free(ptrs[i]);

  void *recycled[8];
  for (int i = 0; i < 8; i++) recycled[i] = malloc(32);

  // Every recycled pointer should match one of the originals
  int ok = 1;
  for (int i = 0; i < 8; i++) {
    int found = 0;
    for (int j = 0; j < 8; j++)
      if (recycled[i] == ptrs[j]) { found = 1; break; }
    if (!found) { ok = 0; break; }
  }
  result("8 freed slots recycled by subsequent mallocs", ok);
  for (int i = 0; i < 8; i++) free(recycled[i]);
}

/* -----------------------------------------------------------------------
 * malloc: size buckets
 * -------------------------------------------------------------------- */
static void test_all_buckets(void) {
  static const size_t sizes[] = { 1, 8, 9, 16, 17, 32, 64, 128, 256,
                                   512, 1024, 2048 };
  int n = (int)(sizeof(sizes) / sizeof(sizes[0]));
  int ok = 1;
  void *ptrs[12];
  for (int i = 0; i < n; i++) {
    ptrs[i] = malloc(sizes[i]);
    if (!ptrs[i]) { ok = 0; break; }
    memset(ptrs[i], 0xab, sizes[i]);
  }
  result("malloc succeeds for all sub-page bucket sizes", ok);
  for (int i = 0; i < n; i++) if (ptrs[i]) free(ptrs[i]);
}

static void test_large_alloc(void) {
  size_t sz = 8192;
  char *p = malloc(sz);
  result("malloc(8192) large alloc succeeds", p != NULL);
  if (!p) return;
  for (size_t i = 0; i < sz; i++) p[i] = (char)(i & 0x7f);
  int ok = 1;
  for (size_t i = 0; i < sz; i++)
    if (p[i] != (char)(i & 0x7f)) { ok = 0; break; }
  result("large alloc read/write correct", ok);
  free(p);
}

static void test_large_alloc_freed(void) {
  // Allocate and free a large object; a second large alloc should not
  // alias the first (since munmap was called).
  char *a = malloc(8192);
  if (!a) { result("large free: first alloc ok", 0); return; }
  memset(a, 0x55, 8192);
  free(a);
  char *b = malloc(8192);
  result("large alloc after free succeeds", b != NULL);
  if (b) free(b);
}

/* -----------------------------------------------------------------------
 * calloc
 * -------------------------------------------------------------------- */
static void test_calloc_zero(void) {
  int *p = calloc(32, sizeof(int));
  result("calloc returns non-null", p != NULL);
  if (!p) return;
  int ok = 1;
  for (int i = 0; i < 32; i++)
    if (p[i] != 0) { ok = 0; break; }
  result("calloc region is zero-initialized", ok);
  free(p);
}

static void test_calloc_writable(void) {
  char *p = calloc(128, 1);
  if (!p) { result("calloc writable: alloc ok", 0); return; }
  memset(p, 0xff, 128);
  int ok = 1;
  for (int i = 0; i < 128; i++)
    if ((unsigned char)p[i] != 0xff) { ok = 0; break; }
  result("calloc region is writable after zeroing", ok);
  free(p);
}

/* -----------------------------------------------------------------------
 * malloc: stress
 * -------------------------------------------------------------------- */
static void test_stress_alloc_free(void) {
  // Interleaved alloc/free across different sizes
  void *a = malloc(8);
  void *b = malloc(64);
  void *c = malloc(512);
  void *d = malloc(2048);
  void *e = malloc(5000);

  result("stress: five mixed-size allocs succeed", a && b && c && d && e);

  if (a) { memset(a,   0x11, 8);    }
  if (b) { memset(b,   0x22, 64);   }
  if (c) { memset(c,   0x33, 512);  }
  if (d) { memset(d,   0x44, 2048); }
  if (e) { memset(e,   0x55, 5000); }

  int ok = 1;
  if (a) for (int i=0;i<8;   i++) if (((char*)a)[i] != 0x11) { ok=0; break; }
  if (b) for (int i=0;i<64;  i++) if (((char*)b)[i] != 0x22) { ok=0; break; }
  if (c) for (int i=0;i<512; i++) if (((char*)c)[i] != 0x33) { ok=0; break; }
  if (d) for (int i=0;i<2048;i++) if (((char*)d)[i] != 0x44) { ok=0; break; }
  if (e) for (int i=0;i<5000;i++) if (((char*)e)[i] != 0x55) { ok=0; break; }
  result("stress: all regions hold correct data", ok);

  free(a); free(b); free(c); free(d); free(e);

  // Re-allocate same sizes — should all succeed (recycled or new slabs)
  a = malloc(8);
  b = malloc(64);
  c = malloc(512);
  d = malloc(2048);
  e = malloc(5000);
  result("stress: re-alloc after free succeeds", a && b && c && d && e);
  free(a); free(b); free(c); free(d); free(e);
}

static void test_many_small_allocs(void) {
  // Force multiple slab pages to be allocated for one bucket
  void *ptrs[200];
  int ok = 1;
  for (int i = 0; i < 200; i++) {
    ptrs[i] = malloc(16);
    if (!ptrs[i]) { ok = 0; break; }
    *(int *)ptrs[i] = i;
  }
  result("200 x malloc(16) all succeed", ok);
  for (int i = 0; i < 200; i++) {
    if (ptrs[i] && *(int *)ptrs[i] != i) { ok = 0; break; }
  }
  result("200 x malloc(16) hold correct values", ok);
  for (int i = 0; i < 200; i++) if (ptrs[i]) free(ptrs[i]);
}

static void test_free_null(void) {
  free(NULL);
  result("free(NULL) does not crash", 1);
}

int main(void) {
  printf("\n=== mmap Tests ===\n\n");
  test_anon_map_read_write();
  test_zero_initialized();

  printf("\n=== malloc Basic ===\n\n");
  test_malloc_returns_non_null();
  test_malloc_zero();
  test_malloc_writable();
  test_malloc_distinct_pointers();
  test_malloc_no_overlap();

  printf("\n=== malloc Recycling ===\n\n");
  test_free_recycles();
  test_free_recycles_multiple();

  printf("\n=== malloc Size Buckets ===\n\n");
  test_all_buckets();
  test_large_alloc();
  test_large_alloc_freed();

  printf("\n=== calloc ===\n\n");
  test_calloc_zero();
  test_calloc_writable();

  printf("\n=== malloc Stress ===\n\n");
  test_stress_alloc_free();
  test_many_small_allocs();
  test_free_null();

  printf("\n=== Test Summary ===\n");
  printf("Passed: %d\n", passed);
  printf("Failed: %d\n", failed);
  printf("\n");
  return failed > 0 ? 1 : 0;
}
