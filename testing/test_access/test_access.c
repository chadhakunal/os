#include <stdio.h>
#include <unistd.h>
#include <errno.h>

static int pass = 0, fail = 0;

static void check(const char *desc, int got, int want) {
  if (got == want) {
    printf("  [PASS] %s\n", desc);
    pass++;
  } else {
    printf("  [FAIL] %s: got %d (errno=%d), want %d\n", desc, got, errno, want);
    fail++;
  }
}

int main(void) {
  printf("test_access\n");

  /* F_OK: existence */
  check("access(\"/bin/sh\", F_OK) == 0",  access("/bin/sh", F_OK),  0);
  check("access(\"/no/such\", F_OK) == -1", access("/no/such", F_OK), -1);

  /* R_OK: readable */
  check("access(\"/bin/sh\", R_OK) == 0",  access("/bin/sh", R_OK),  0);

  /* X_OK: executable */
  check("access(\"/bin/sh\", X_OK) == 0",  access("/bin/sh", X_OK),  0);

  /* W_OK: not writable on read-only tarfs root */
  check("access(\"/bin/sh\", W_OK) == -1", access("/bin/sh", W_OK), -1);

  /* faccessat with AT_FDCWD */
  check("faccessat(AT_FDCWD, \"/bin/sh\", F_OK, 0) == 0",
        faccessat(AT_FDCWD, "/bin/sh", F_OK, 0), 0);

  printf("%d passed, %d failed\n", pass, fail);
  return fail ? 1 : 0;
}
