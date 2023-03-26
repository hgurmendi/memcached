#include <stdbool.h>
#include <string.h>

#include "bounded_data.h"

// Allocates memory for a BoundedData instance, save a reference to the given
// buffer as well as its size and return it. This essentially wraps an existing
// buffer with a BoundedData instance.
struct BoundedData *bounded_data_create_from_buffer(char *buffer,
                                                    uint64_t size) {
  struct BoundedData *new_bounded_data = malloc(sizeof(struct BoundedData));
  if (new_bounded_data == NULL) {
    perror("bounded_data_create_from_buffer malloc");
    abort();
  }

  new_bounded_data->size = size;
  new_bounded_data->data = buffer;

  return new_bounded_data;
}

// Allocates memory for a duplicate of the given buffer, copies it and then
// wraps it with a BoundedData instance. The difference with the function above
// is that this one makes a copy of the given buffer, whereas the other "takes
// ownership" of the given bufer.
struct BoundedData *bounded_data_create_from_buffer_duplicate(char *buffer,
                                                              uint64_t size) {
  // First duplicate the given buffer.
  char *buffer_copy = malloc(sizeof(char) * size);
  if (buffer_copy == NULL) {
    perror("bounded_data_create_from_buffer_duplicate malloc");
    abort();
  }
  memcpy(buffer_copy, buffer, size);
  // Them just create a BoundedData instance from that copy.
  return bounded_data_create_from_buffer(buffer_copy, size);
}

// Allocates memory for a duplicate of the given string, copies it and then
// wraps it with a BoundedData instance.
struct BoundedData *bounded_data_create_from_string_duplicate(char *str) {
  return bounded_data_create_from_buffer_duplicate(str, strlen(str));
}

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
  free(bounded_data->data);
  free(bounded_data);
}

// Prints the given BoundedData instance to standard output, no newline.
void bounded_data_print(struct BoundedData *bounded_data) {
  printf("%s", bounded_data->data);
}