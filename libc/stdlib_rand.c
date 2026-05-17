#include <stdlib.h>

static unsigned long rand_next = 1;

int rand(void) {
  rand_next = rand_next * 1103515245UL + 12345UL;
  return (int)((rand_next >> 16) % ((unsigned long)RAND_MAX + 1UL));
}

void srand(unsigned seed) {
  rand_next = seed;
}
