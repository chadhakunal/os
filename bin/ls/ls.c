#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Build a "drwxrwxrwx" mode string from a stat st_mode. */
static void fmt_mode(unsigned int mode, char out[11]) {
  /* file type */
  if      ((mode & 0170000) == 0040000) out[0] = 'd';
  else if ((mode & 0170000) == 0120000) out[0] = 'l';
  else                                  out[0] = '-';
  /* owner */
  out[1] = (mode & 0400) ? 'r' : '-';
  out[2] = (mode & 0200) ? 'w' : '-';
  out[3] = (mode & 0100) ? 'x' : '-';
  /* group (same as owner since we have one user) */
  out[4] = (mode & 0040) ? 'r' : '-';
  out[5] = (mode & 0020) ? 'w' : '-';
  out[6] = (mode & 0010) ? 'x' : '-';
  /* other */
  out[7] = (mode & 0004) ? 'r' : '-';
  out[8] = (mode & 0002) ? 'w' : '-';
  out[9] = (mode & 0001) ? 'x' : '-';
  out[10] = '\0';
}

/* Format seconds-since-epoch as "YYYY-MM-DD HH:MM" (no libc time needed). */
static void fmt_time(unsigned long long sec, char out[17]) {
  /* Gregorian calendar computation valid for 1970-2099. */
  unsigned int s   = sec % 60;  (void)s;
  unsigned int m   = (sec / 60) % 60;
  unsigned int h   = (sec / 3600) % 24;
  unsigned int day = (unsigned int)(sec / 86400); /* days since 1970-01-01 */

  /* Shift to March 1, 2000 as epoch for easier leap-year math. */
  day += 10957;   /* days from 1970-01-01 to 2000-01-01 */
  day += 60;      /* days from 2000-01-01 to 2000-03-01 */

  unsigned int era  = day / 146097;
  unsigned int doe  = day - era * 146097;
  unsigned int yoe  = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  unsigned int y    = yoe + era * 400;
  unsigned int doy  = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned int mp   = (5 * doy + 2) / 153;
  unsigned int d    = doy - (153 * mp + 2) / 5 + 1;
  unsigned int mo   = mp < 10 ? mp + 3 : mp - 9;
  y += (mo <= 2);

  /* Write "YYYY-MM-DD HH:MM" */
  out[0]  = '0' + (y / 1000) % 10;
  out[1]  = '0' + (y / 100)  % 10;
  out[2]  = '0' + (y / 10)   % 10;
  out[3]  = '0' + y % 10;
  out[4]  = '-';
  out[5]  = '0' + mo / 10;
  out[6]  = '0' + mo % 10;
  out[7]  = '-';
  out[8]  = '0' + d / 10;
  out[9]  = '0' + d % 10;
  out[10] = ' ';
  out[11] = '0' + h / 10;
  out[12] = '0' + h % 10;
  out[13] = ':';
  out[14] = '0' + m / 10;
  out[15] = '0' + m % 10;
  out[16] = '\0';
}

static int list_dir(const char *path, int long_fmt) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "ls: cannot open '%s'\n", path);
    return 1;
  }

  struct dirent buf[32];
  int n;
  while ((n = getdents(fd, buf, 32)) > 0) {
    for (int i = 0; i < n; i++) {
      if (!long_fmt) {
        printf("%s\n", buf[i].d_name);
        continue;
      }

      /* Build full path for stat. */
      char full[256];
      int plen = strlen(path);
      int nlen = strlen(buf[i].d_name);
      if (plen + nlen + 2 > (int)sizeof(full)) {
        printf("%s\n", buf[i].d_name);
        continue;
      }
      memcpy(full, path, plen);
      if (full[plen - 1] != '/') full[plen++] = '/';
      memcpy(full + plen, buf[i].d_name, nlen + 1);

      struct stat st;
      if (stat(full, &st) < 0) {
        /* Fall back to name-only if stat fails. */
        printf("%s\n", buf[i].d_name);
        continue;
      }

      char modestr[11];
      fmt_mode(st.st_mode, modestr);

      char timestr[17];
      fmt_time(st.st_mtime, timestr);

      /* Format: mode  nlinks  size  mtime  name */
      printf("%s %3u %8llu  %s  %s\n",
             modestr,
             (unsigned int)st.st_nlink,
             (unsigned long long)st.st_size,
             timestr,
             buf[i].d_name);
    }
  }

  close(fd);
  return 0;
}

int main(int argc, char **argv) {
  int long_fmt = 0;
  int ret = 0;
  int nfiles = 0;

  /* Collect non-flag args; flags first pass */
  const char *paths[16];
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      for (int j = 1; argv[i][j]; j++)
        if (argv[i][j] == 'l') long_fmt = 1;
    } else {
      if (nfiles < 16) paths[nfiles++] = argv[i];
    }
  }

  if (nfiles == 0) {
    ret = list_dir(".", long_fmt);
  } else {
    for (int i = 0; i < nfiles; i++) {
      if (list_dir(paths[i], long_fmt) != 0)
        ret = 1;
    }
  }

  return ret;
}
