#include <stdio.h>
#include <string.h>

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

  if (!flag_s && !flag_n && !flag_r && !flag_v && !flag_m && !flag_a)
    flag_s = 1;

  int first = 1;
#define PRINT(s) do { if (!first) putchar(' '); fputs(s, stdout); first = 0; } while (0)

  if (flag_a || flag_s) PRINT("sbunix");
  if (flag_a || flag_n) PRINT("sbunix");
  if (flag_a || flag_r) PRINT("0.1.0");
  if (flag_a || flag_v) PRINT("#1");
  if (flag_a || flag_m) PRINT("riscv64");
  putchar('\n');

  return 0;
}
