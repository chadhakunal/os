#ifndef USER_DATA_ACCESS_H
#define USER_DATA_ACCESS_H

#include "types.h"
#include "lib/string.h"

static inline int copy_to_user(void *user_dest, const void *kernel_src, size_t n) {
  memcpy(user_dest, kernel_src, n);
  return 0;
}

static inline int copy_from_user(void *kernel_dest, const void *user_src, size_t n) {
  memcpy(kernel_dest, user_src, n);
  return 0;
}

#endif
