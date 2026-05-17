#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static char **env_ptrs;
static unsigned char *env_owned;
static size_t env_n;
static int env_mutable;

static int env_make_mutable(void) {
  if (env_mutable)
    return 0;

  size_t n = 0;
  if (environ)
    while (environ[n])
      n++;

  char **p = calloc(n + 1, sizeof(char *));
  unsigned char *o = calloc(n + 1, sizeof(unsigned char));
  if (!p || !o) {
    free(p);
    free(o);
    errno = ENOMEM;
    return -1;
  }

  for (size_t i = 0; i < n; i++) {
    p[i] = strdup(environ[i]);
    if (!p[i]) {
      for (size_t j = 0; j < i; j++)
        free(p[j]);
      free(p);
      free(o);
      errno = ENOMEM;
      return -1;
    }
    o[i] = 1;
  }

  env_ptrs = p;
  env_owned = o;
  env_n = n;
  environ = env_ptrs;
  env_mutable = 1;
  return 0;
}

static ssize_t env_find(const char *name) {
  size_t nlen = strlen(name);
  for (size_t i = 0; i < env_n; i++) {
    if (strncmp(env_ptrs[i], name, nlen) == 0 && env_ptrs[i][nlen] == '=')
      return (ssize_t)i;
  }
  return -1;
}

static int env_grow(void) {
  char **np = realloc(env_ptrs, (env_n + 2) * sizeof(char *));
  unsigned char *no = realloc(env_owned, env_n + 2);
  if (!np || !no) {
    free(np);
    free(no);
    errno = ENOMEM;
    return -1;
  }
  env_ptrs = np;
  env_owned = no;
  environ = env_ptrs;
  return 0;
}

char *getenv(const char *name) {
  if (!name || !*name || strchr(name, '='))
    return NULL;
  if (!environ)
    return NULL;

  size_t nlen = strlen(name);
  for (char **ep = environ; *ep; ep++) {
    if (strncmp(*ep, name, nlen) == 0 && (*ep)[nlen] == '=')
      return *ep + nlen + 1;
  }
  return NULL;
}

int setenv(const char *name, const char *value, int overwrite) {
  if (!name || !*name || strchr(name, '=')) {
    errno = EINVAL;
    return -1;
  }
  if (!value)
    value = "";

  if (env_make_mutable() < 0)
    return -1;

  ssize_t idx = env_find(name);
  if (idx >= 0 && !overwrite)
    return 0;

  size_t nlen = strlen(name);
  size_t vlen = strlen(value);
  char *entry = malloc(nlen + vlen + 2);
  if (!entry) {
    errno = ENOMEM;
    return -1;
  }
  memcpy(entry, name, nlen);
  entry[nlen] = '=';
  memcpy(entry + nlen + 1, value, vlen + 1);

  if (idx >= 0) {
    if (env_owned[(size_t)idx])
      free(env_ptrs[(size_t)idx]);
    env_ptrs[(size_t)idx] = entry;
    env_owned[(size_t)idx] = 1;
    return 0;
  }

  if (env_grow() < 0) {
    free(entry);
    return -1;
  }
  env_ptrs[env_n] = entry;
  env_owned[env_n] = 1;
  env_n++;
  env_ptrs[env_n] = NULL;
  return 0;
}

int unsetenv(const char *name) {
  if (!name || !*name || strchr(name, '=')) {
    errno = EINVAL;
    return -1;
  }
  if (!env_mutable)
    return 0;

  ssize_t idx = env_find(name);
  if (idx < 0)
    return 0;

  size_t i = (size_t)idx;
  if (env_owned[i])
    free(env_ptrs[i]);

  for (size_t j = i + 1; j <= env_n; j++)
    env_ptrs[j - 1] = env_ptrs[j];
  for (size_t j = i + 1; j < env_n; j++)
    env_owned[j - 1] = env_owned[j];
  env_n--;
  return 0;
}

int putenv(char *string) {
  char *eq;

  if (!string) {
    errno = EINVAL;
    return -1;
  }
  eq = strchr(string, '=');
  if (!eq || eq == string) {
    errno = EINVAL;
    return -1;
  }

  if (env_make_mutable() < 0)
    return -1;

  char name_buf[256];
  size_t nlen = (size_t)(eq - string);
  if (nlen >= sizeof(name_buf)) {
    errno = EINVAL;
    return -1;
  }
  memcpy(name_buf, string, nlen);
  name_buf[nlen] = '\0';

  ssize_t idx = env_find(name_buf);
  if (idx >= 0) {
    if (env_owned[(size_t)idx])
      free(env_ptrs[(size_t)idx]);
    env_ptrs[(size_t)idx] = string;
    env_owned[(size_t)idx] = 0;
    return 0;
  }

  if (env_grow() < 0)
    return -1;
  env_ptrs[env_n] = string;
  env_owned[env_n] = 0;
  env_n++;
  env_ptrs[env_n] = NULL;
  return 0;
}

char *secure_getenv(const char *name) {
  if (geteuid() != getuid() || getegid() != getgid())
    return NULL;
  return getenv(name);
}
