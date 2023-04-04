#ifndef __BOUNDED_DATA_H__
#define __BOUNDED_DATA_H__

#include <stdbool.h>
#include <stdint.h>

// Struct that represents a buffer of arbitrary binary data with its size.
struct BoundedData {
  uint64_t size;
  char *data;
};

// True if the given BoundedData instances are equal byte-by-byte, false
// otherwise.
bool bounded_data_equals(struct BoundedData *a, struct BoundedData *b);

// De-allocates memory for the given BoundedData instance.
void bounded_data_destroy(struct BoundedData *bounded_data);

// Prints the given BoundedData instance to standard output, no newline.
void bounded_data_print(struct BoundedData *bounded_data);

// Returns the 64-bit FNV-1a hash for the given data.
// More information: https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
uint64_t bounded_data_hash(struct BoundedData *bounded_data);

#endif