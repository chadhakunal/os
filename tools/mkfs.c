#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: mkfs <image> <size_mb>\n");
    return 1;
  }
  long mb = atol(argv[2]);
  if (mb <= 0) { fprintf(stderr, "invalid size\n"); return 1; }
  FILE *f = fopen(argv[1], "wb");
  if (!f) { perror(argv[1]); return 1; }
  fseek(f, mb * 1024L * 1024 - 1, SEEK_SET);
  fputc(0, f);
  fclose(f);
  return 0;
}
