#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
  const char spinner[] = {'|', '/', '-', '\\'};
  int i = 0;

  while (1) {
    printf("\r%c", spinner[i % 4]);
    i++;

    for (volatile int j = 0; j < 10000000; j++) {
      // Busy wait to slow down the animation
    }
  }

  return 0;
}
