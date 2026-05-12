#ifndef DIRENT_H
#define DIRENT_H

#include <stdint.h>

struct dirent {
  uint32_t d_ino;
  char     d_name[256];
};

int getdents(int fd, struct dirent *buf, unsigned int count);

#endif
