#include <stdio.h>
#include <stddef.h>

int global_init = 42;
int global_zero = 0;
char *global_ptr = NULL;

int main(int argc, char *argv[]) {
  printf("testglobal: &global_init=%p, value=%d\n", &global_init, global_init);
  printf("testglobal: &global_zero=%p, value=%d\n", &global_zero, global_zero);
  printf("testglobal: &global_ptr=%p, value=%p\n", &global_ptr, global_ptr);

  return 0;
}
