#include <unistd.h>
#include <string.h>
#include <stdio.h>

char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

int getopt(int argc, char *const argv[], const char *optstring) {
  static int char_index = 1;
  int current_option;
  const char *option_def;

  if (char_index == 1) {
    if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
      return -1;
    }
    if (argv[optind][1] == '-' && argv[optind][2] == '\0') {
      optind++;
      return -1;
    }
  }

  optopt = current_option = argv[optind][char_index];

  if (current_option == ':' || (option_def = strchr(optstring, current_option)) == NULL) {
    if (opterr && *optstring != ':') {
      printf("%s: illegal option -- %c\n", argv[0], current_option);
    }
    if (argv[optind][++char_index] == '\0') {
      optind++;
      char_index = 1;
    }
    return '?';
  }

  if (option_def[1] == ':') {
    if (argv[optind][char_index + 1] != '\0') {
      optarg = &argv[optind++][char_index + 1];
    } else if (++optind >= argc) {
      if (opterr && *optstring != ':') {
        printf("%s: option requires an argument -- %c\n", argv[0], current_option);
      }
      char_index = 1;
      return *optstring == ':' ? ':' : '?';
    } else {
      optarg = argv[optind++];
    }
    char_index = 1;
  } else {
    if (argv[optind][++char_index] == '\0') {
      char_index = 1;
      optind++;
    }
    optarg = NULL;
  }

  return current_option;
}
