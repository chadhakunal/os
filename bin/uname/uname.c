#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct utsname {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
};

static int do_uname(struct utsname *u) {
  return (int)syscall1(160, (long)u);
}

int main(int argc, char **argv) {
  int flag_s = 0, flag_n = 0, flag_r = 0, flag_v = 0, flag_m = 0, flag_a = 0;

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      fprintf(stderr, "uname: unknown argument: %s\n", argv[i]);
      return 1;
    }
    for (int j = 1; argv[i][j]; j++) {
      switch (argv[i][j]) {
      case 's': flag_s = 1; break;
      case 'n': flag_n = 1; break;
      case 'r': flag_r = 1; break;
      case 'v': flag_v = 1; break;
      case 'm': flag_m = 1; break;
      case 'a': flag_a = 1; break;
      default:
        fprintf(stderr, "uname: unknown flag -%c\n", argv[i][j]);
        return 1;
      }
    }
  }

  /* default: -s */
  if (!flag_s && !flag_n && !flag_r && !flag_v && !flag_m && !flag_a)
    flag_s = 1;

  struct utsname u;
  if (do_uname(&u) < 0) {
    /* fallback to static strings if syscall fails */
    strncpy(u.sysname,  "sbunix",  64);
    strncpy(u.nodename, "sbunix",  64);
    strncpy(u.release,  "0.1.0",   64);
    strncpy(u.version,  "#1",      64);
    strncpy(u.machine,  "riscv64", 64);
  }

  int first = 1;
#define PRINT(field) do { \
    if (!first) putchar(' '); \
    fputs(field, stdout); \
    first = 0; \
  } while (0)

  if (flag_a || flag_s) PRINT(u.sysname);
  if (flag_a || flag_n) PRINT(u.nodename);
  if (flag_a || flag_r) PRINT(u.release);
  if (flag_a || flag_v) PRINT(u.version);
  if (flag_a || flag_m) PRINT(u.machine);
  putchar('\n');

  return 0;
}
