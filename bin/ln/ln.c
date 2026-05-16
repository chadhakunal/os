#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static void usage(void) {
  printf("Usage: ln [-s] <target> <linkname>\n");
  printf("  -s  create a symbolic link instead of a hard link\n");
}

int main(int argc, char **argv) {
  int symlink_flag = 0;
  int optind = 1;

  if (optind < argc && strcmp(argv[optind], "-s") == 0) {
    symlink_flag = 1;
    optind++;
  }

  if (argc - optind != 2) {
    usage();
    return 1;
  }

  const char *target   = argv[optind];
  const char *linkname = argv[optind + 1];

  int ret;
  if (symlink_flag) {
    ret = symlink(target, linkname);
    if (ret < 0) {
      printf("ln: failed to create symlink '%s' -> '%s'\n", linkname, target);
      return 1;
    }
  } else {
    ret = link(target, linkname);
    if (ret < 0) {
      printf("ln: failed to create hard link '%s' -> '%s'\n", linkname, target);
      return 1;
    }
  }

  return 0;
}
