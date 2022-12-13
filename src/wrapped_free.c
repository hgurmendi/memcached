#include <stdint.h>
#include <stdlib.h>

uint64_t wrapped_free_mem = 0;

void wrapped_free(void *ptr, uint64_t size) {
  wrapped_free_mem += size;
  free(ptr);
}