#include <stdlib.h>
#include <stdint.h>

static unsigned short rand48_x[3] = {0x330e, 0, 0};
static uint64_t rand48_a = 0x5deece66dULL;
static uint64_t rand48_c = 0xbULL;

static uint64_t rand48_step(unsigned short x[3]) {
  uint64_t v = (uint64_t)x[0] | ((uint64_t)x[1] << 16) | ((uint64_t)x[2] << 32);

  v = (rand48_a * v + rand48_c) & 0xffffffffffffULL;
  x[0] = (unsigned short)(v & 0xffff);
  x[1] = (unsigned short)((v >> 16) & 0xffff);
  x[2] = (unsigned short)((v >> 32) & 0xffff);
  return v;
}

static int rand48_ilog2(uint32_t a) {
  return 31 - __builtin_clz(a);
}

static double rand48_to_double(uint64_t v) {
  union {
    uint64_t u;
    double d;
  } x;
  uint32_t a = (uint32_t)(v >> 16);

  if (!a) {
    x.u = 0;
    return x.d;
  }

  int p = rand48_ilog2(a);
  int exp = 1023 - 32 + p;
  uint64_t frac = ((uint64_t)a << (52 - 1 - p)) & ((1ULL << 52) - 1);

  x.u = ((uint64_t)exp << 52) | frac;
  return x.d;
}

void srand48(long seedval) {
  rand48_x[0] = 0x330e;
  rand48_x[1] = (unsigned short)(seedval & 0xffff);
  rand48_x[2] = (unsigned short)(((unsigned long)seedval >> 16) & 0xffff);
}

unsigned short *seed48(unsigned short xseed[3]) {
  static unsigned short old[3];

  old[0] = rand48_x[0];
  old[1] = rand48_x[1];
  old[2] = rand48_x[2];
  rand48_x[0] = xseed[0];
  rand48_x[1] = xseed[1];
  rand48_x[2] = xseed[2];
  return old;
}

void lcong48(unsigned short param[7]) {
  rand48_x[2] = param[0];
  rand48_x[1] = param[1];
  rand48_x[0] = param[2];
  rand48_a = ((uint64_t)param[3] << 32) | ((uint64_t)param[4] << 16) | param[5];
  rand48_c = param[6];
}

double drand48(void) {
  return rand48_to_double(rand48_step(rand48_x));
}

double erand48(unsigned short xsubi[3]) {
  return rand48_to_double(rand48_step(xsubi));
}

long nrand48(unsigned short xsubi[3]) {
  return (long)((rand48_step(xsubi) >> 17) & 0x7fffffffL);
}

long lrand48(void) {
  return nrand48(rand48_x);
}

long jrand48(unsigned short xsubi[3]) {
  uint64_t v = rand48_step(xsubi) >> 16;
  if (v & 0x80000000ULL)
    v |= ~0xffffffffULL;
  return (long)v;
}

long mrand48(void) {
  return jrand48(rand48_x);
}
