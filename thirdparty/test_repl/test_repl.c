#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(void) {
  printf("isatty(0)=%d isatty(1)=%d\n", isatty(0), isatty(1));
  fflush(stdout);

  char buf[256];
  for (;;) {
    fputs(">>> ", stdout);
    fflush(stdout);
    char *s = fgets(buf, sizeof(buf), stdin);
    if (!s) {
      printf("fgets returned NULL (EOF)\n");
      break;
    }
    /* strip newline */
    int l = strlen(buf);
    if (l > 0 && buf[l-1] == '\n') buf[l-1] = '\0';
    printf("got: '%s'\n", buf);
    if (strcmp(buf, "quit") == 0) break;
  }
  return 0;
}
