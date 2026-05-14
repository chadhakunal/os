#include <stdio.h>
#include <unistd.h>

void fill_stack() {
  char buffer[4096];

  for (int i = 0; i < 4096; i++) {
    buffer[i] = (char)i;
  }

  printf("%s\n", buffer);
  printf("Printed full buffer returning\n");
}

int main(int argc, char **argv) {
  printf("Stack test starting...\n");

  fill_stack();
  printf("Stack test complete\n");
  return 0;
}
