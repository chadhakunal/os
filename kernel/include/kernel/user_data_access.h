#ifndef USER_DATA_ACCESS_H
#define USER_DATA_ACCESS_H

#include "types.h"

int copy_to_user(void *user_dest, const void *kernel_src, size_t n);
int copy_from_user(void *kernel_dest, const void *user_src, size_t n);

static inline int copy_string_from_user(char *kernel_dest, const char *user_src, size_t max_len) {
  size_t i = 0;
  while (i < max_len - 1) {
    copy_from_user(&kernel_dest[i], &user_src[i], 1);
    if (kernel_dest[i] == '\0') {
      return i;
    }
    i++;
  }
  kernel_dest[i] = '\0';
  return i;
}

static inline int copy_string_to_user(char *user_dest, const char *kernel_src, size_t max_len) {
  size_t i = 0;
  while (i < max_len - 1 && kernel_src[i] != '\0') {
    copy_to_user(&user_dest[i], &kernel_src[i], 1);
    i++;
  }
  char null_term = '\0';
  copy_to_user(&user_dest[i], &null_term, 1);
  return i;
}

#endif
