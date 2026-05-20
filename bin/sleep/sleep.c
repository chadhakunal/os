#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: sleep <seconds>\n");
    return 1;
  }

  /* Parse seconds as a decimal number, supporting e.g. "1.5" */
  char *end;
  double secs = strtod(argv[1], &end);
  if (end == argv[1] || secs < 0) {
    fprintf(stderr, "sleep: invalid time interval '%s'\n", argv[1]);
    return 1;
  }

  struct timespec ts;
  ts.tv_sec  = (time_t)secs;
  ts.tv_nsec = (long)((secs - (double)ts.tv_sec) * 1000000000L);
  nanosleep(&ts, NULL);
  return 0;
}
