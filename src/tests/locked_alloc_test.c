#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main() {
  char * c = malloc(20);
  assert(c);
  memcpy(c, "hello world", sizeof("hello world"));
  printf("%s\n", c);
  free(c);
  return 0;
}
