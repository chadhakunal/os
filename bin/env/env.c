#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

int main(int argc, char **argv) {
  /* env [-i] [NAME=VALUE ...] [command [args...]] */
  int clear_env = 0;
  int i = 1;

  if (i < argc && strcmp(argv[i], "-i") == 0) {
    clear_env = 1;
    i++;
  }

  if (clear_env) {
    static char *empty_env[] = { NULL };
    environ = empty_env;
  }

  /* Consume NAME=VALUE pairs */
  while (i < argc && strchr(argv[i], '=') != NULL) {
    char *eq = strchr(argv[i], '=');
    char name[256];
    size_t nlen = (size_t)(eq - argv[i]);
    if (nlen >= sizeof(name)) { i++; continue; }
    memcpy(name, argv[i], nlen);
    name[nlen] = '\0';
    setenv(name, eq + 1, 1);
    i++;
  }

  if (i < argc) {
    /* Run a command */
    char *cmd = argv[i];
    char **cmd_argv = &argv[i];

    /* Resolve via PATH if not an absolute path */
    char resolved[512];
    if (strchr(cmd, '/') == NULL) {
      const char *path = getenv("PATH");
      if (!path) path = "/bin";
      char path_buf[1024];
      strncpy(path_buf, path, sizeof(path_buf) - 1);
      path_buf[sizeof(path_buf) - 1] = '\0';
      char *dir = path_buf;
      int found = 0;
      while (dir && *dir) {
        char *colon = strchr(dir, ':');
        if (colon) *colon = '\0';
        snprintf(resolved, sizeof(resolved), "%s/%s", dir, cmd);
        int fd = open(resolved, 0);
        if (fd >= 0) { close(fd); found = 1; break; }
        dir = colon ? colon + 1 : NULL;
      }
      if (!found) {
        fprintf(stderr, "env: %s: not found\n", cmd);
        return 127;
      }
      cmd = resolved;
    }

    execve(cmd, cmd_argv, environ);
    fprintf(stderr, "env: execve failed\n");
    return 126;
  }

  /* No command — print environment */
  for (char **ep = environ; ep && *ep; ep++)
    printf("%s\n", *ep);

  return 0;
}
