#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define DEG_3 31
#define SEP_3 3

static int32_t default_tbl[DEG_3 + 1];
static int32_t *state = default_tbl;
static int32_t *fptr = &default_tbl[SEP_3];
static int32_t *rptr = default_tbl;
static int32_t *end_ptr = &default_tbl[DEG_3];
static int rand_deg = DEG_3;
static int rand_sep = SEP_3;
static char *active_buf = (char *)default_tbl;

static void random_init(unsigned seed) {
  if (seed == 0)
    seed = 1;

  state[0] = (int32_t)seed;
  for (int i = 1; i < rand_deg; i++)
    state[i] = (int32_t)((uint32_t)1103515245U * (uint32_t)state[i - 1] + 12345U);

  fptr = &state[rand_sep];
  rptr = &state[0];
  end_ptr = &state[rand_deg];
}

static void random_configure(char *buf, size_t n) {
  if (n < 32) {
    rand_deg = 3;
    rand_sep = 1;
  } else if (n < 64) {
    rand_deg = 7;
    rand_sep = 3;
  } else if (n < 128) {
    rand_deg = 15;
    rand_sep = 3;
  } else {
    rand_deg = DEG_3;
    rand_sep = SEP_3;
  }

  state = (int32_t *)buf;
  end_ptr = &state[rand_deg];
  fptr = &state[rand_sep];
  rptr = &state[0];
}

void srandom(unsigned seed) {
  random_init(seed);
}

char *initstate(unsigned seed, char *arg_state, size_t n) {
  char *old = active_buf;

  if (!arg_state || n < 8)
    return NULL;

  random_configure(arg_state, n);
  random_init(seed);
  active_buf = arg_state;
  return old;
}

char *setstate(char *arg_state) {
  char *old = active_buf;

  if (!arg_state)
    return NULL;

  active_buf = arg_state;
  state = (int32_t *)arg_state;
  end_ptr = &state[rand_deg];
  fptr = &state[rand_sep];
  rptr = &state[0];
  return old;
}

long random(void) {
  int32_t val = (int32_t)((uint32_t)*fptr + (uint32_t)*rptr);
  int32_t result = (val >> 1) & 0x7fffffff;

  if (++fptr >= end_ptr)
    fptr = state;
  if (++rptr >= end_ptr)
    rptr = state;

  return (long)result;
}
