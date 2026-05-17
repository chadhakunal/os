#include <libgen.h>
#include <string.h>

static char libgen_dot[] = ".";
static char libgen_slash[] = "/";

static char *rstrip_slashes(char *path, int *all_slashes) {
  char *end = path + strlen(path);
  while (end > path && end[-1] == '/') {
    end--;
    (*all_slashes)++;
  }
  *end = '\0';
  return end;
}

char *basename(char *path) {
  if (!path || !*path)
    return libgen_dot;

  int slash_run = 0;
  rstrip_slashes(path, &slash_run);

  if (*path == '\0') {
    if (slash_run > 0)
      return libgen_slash;
    return libgen_dot;
  }

  char *last = strrchr(path, '/');
  if (!last)
    return path;
  if (last[1] == '\0' && last == path)
    return libgen_slash;
  return last + 1;
}

char *dirname(char *path) {
  if (!path || !*path)
    return libgen_dot;

  int slash_run = 0;
  rstrip_slashes(path, &slash_run);

  if (*path == '\0') {
    if (slash_run > 0)
      return libgen_slash;
    return libgen_dot;
  }

  char *last = strrchr(path, '/');
  if (!last)
    return libgen_dot;

  if (last == path) {
    last[1] = '\0';
    return path;
  }

  *last = '\0';
  {
    char *end = path + strlen(path);
    while (end > path && end[-1] == '/')
      *--end = '\0';
  }

  if (*path == '\0')
    return libgen_dot;
  return path;
}
