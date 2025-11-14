#include "user/user.h"

int
main(void)
{
  int before, after;
  char *mem;

  before = get_num_free_pages();
  printf("Free pages before allocation: %d\n", before);

  // Allocate 10 pages (40960 bytes)
  mem = sbrk(40960);
  if(mem == (char*)-1) {
    printf("sbrk failed\n");
    exit(1);
  }

  after = get_num_free_pages();
  printf("Free pages after allocation: %d\n", after);

  exit(0);
}