#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
  int no_newline = 0;
  int start = 1;

  if (argc > 1 && strcmp(argv[1], "-n") == 0) {
    no_newline = 1;
    start = 2;
  }

  for (int i = start; i < argc; i++) {
    if (i > start)
      putchar(' ');
    fputs(argv[i], stdout);
  }

  if (!no_newline)
    putchar('\n');

  return 0;
}
