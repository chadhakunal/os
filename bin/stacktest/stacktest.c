#include <stdio.h>
#include <unistd.h>

int depth = 0;

void fill_stack(int n) {
  char buffer[4096];

  depth++;
  printf("Stack depth: %d (approx %d KB used)\n", depth, depth * 4);

  for (int i = 0; i < 4096; i++) {
    buffer[i] = (char)i;
  }

  if (n > 0) {
    fill_stack(n - 1);
  }
  buffer[0] = 'h';
  printf("%s\n", buffer[0]);
  printf("Returning from depth %d\n", depth);
  depth--;
}

int main(int argc, char **argv) {
  printf("Stack test starting...\n");
  printf("Initial stack: 16KB\n");
  printf("Limit: 32KB\n");
  printf("Each recursion uses ~4KB\n");
  printf("\n");

  printf("This should work (use ~8KB, within initial 16KB):\n");
  fill_stack(2);
  printf("\n");

  printf("This should trigger stack growth (use ~20KB, needs growth):\n");
  fill_stack(5);
  printf("\n");

  printf("This should cause SIGSEGV (use ~40KB, exceeds 32KB limit):\n");
  fill_stack(10);
  printf("\n");

  printf("If you see this, test failed - should have received SIGSEGV\n");

  return 0;
}
