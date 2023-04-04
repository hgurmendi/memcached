#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bounded_data.h"
#include "utils.h"

// True if the given BoundedData instances are equal byte-by-byte, false
// otherwise.
bool bounded_data_equals(struct BoundedData *a, struct BoundedData *b) {
  if (a->size != b->size) {
    return false;
  }

  return memcmp(a->data, b->data, a->size) == 0;
}

// Allocates memory for a copy of the given BoundedData, copies it and returns
// it.
struct BoundedData *bounded_data_duplicate(struct BoundedData *bounded_data) {
  struct BoundedData *new_bounded_data = malloc(sizeof(struct BoundedData));
  if (new_bounded_data == NULL) {
    perror("bounded_data_duplicate malloc");
    abort();
  }

  new_bounded_data->size = bounded_data->size;
  new_bounded_data->data = malloc(bounded_data->size);
  if (new_bounded_data->data == NULL) {
    perror("bounded_data_duplicate malloc 2");
    free(new_bounded_data); // We free it just in case.
    abort();
  }

  memcpy(new_bounded_data->data, bounded_data->data, bounded_data->size);

  return new_bounded_data;
}

// De-allocates memory for the given BoundedData instance.
void bounded_data_destroy(struct BoundedData *bounded_data) {
  if (bounded_data->data != NULL) {
    free(bounded_data->data);
  }
  free(bounded_data);
}

// Prints the given BoundedData instance to standard output, no newline.
void bounded_data_print(struct BoundedData *bounded_data) {
  printf("%s", bounded_data->data);
}

// true if the contents of the given BoundedData instance are representable as
// text, false otherwise.
bool bounded_data_is_text_representable(struct BoundedData *bounded_data) {
  return is_text_representable(bounded_data->data, bounded_data->size);
}

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// Returns the 64-bit FNV-1a hash for the given data.
// More information: https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
uint64_t bounded_data_hash(struct BoundedData *bounded_data) {
  uint64_t hash = FNV_OFFSET;
  for (int i = 0; i < bounded_data->size; i++) {
    hash ^= (uint64_t)(unsigned char)bounded_data->data[i];
    hash *= FNV_PRIME;
  }
  return hash;
}