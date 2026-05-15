#pragma once

#include <types.h>

struct timespec {
  long tv_sec;
  long tv_nsec;
};

int nanosleep(const struct timespec *req, struct timespec *rem);
