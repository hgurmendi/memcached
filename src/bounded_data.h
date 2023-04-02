#ifndef __BOUNDED_DATA_H__
#define __BOUNDED_DATA_H__

#include <stdbool.h>
#include <stdint.h>

// Struct that represents a buffer of arbitrary binary data with its size.
struct BoundedData {
  uint64_t size;
  char *data;
};

// Allocates memory for a BoundedData instance, save a reference to the given
// buffer as well as its size and return it. This essentially wraps an existing
// buffer with a BoundedData instance.
struct BoundedData *bounded_data_create_from_buffer(char *buffer,
                                                    uint64_t size);

// Allocates memory for a duplicate of the given buffer, copies it and then
// wraps it with a BoundedData instance. The difference with the function above
// is that this one makes a copy of the given buffer, whereas the other "takes
// ownership" of the given bufer.
struct BoundedData *bounded_data_create_from_buffer_duplicate(char *buffer,
                                                              uint64_t size);

// Allocates memory for a duplicate of the given string, copies it and then
// wraps it with a BoundedData instance.
struct BoundedData *bounded_data_create_from_string_duplicate(char *str);

// Allocates memory for a BoundedData instance and wraps a string with it,
// returning the BoundedData instance.
struct BoundedData *bounded_data_create_from_string(char *str);

// Allocates memory for a BoundedData instance and for a new buffer with the
// given size. Returns the BoundedData instance.
struct BoundedData *bounded_data_create(uint64_t size);

// True if the given BoundedData instances are equal byte-by-byte, false
// otherwise.
bool bounded_data_equals(struct BoundedData *a, struct BoundedData *b);

// Allocates memory for a copy of the given BoundedData, copies it and returns
// it.
struct BoundedData *bounded_data_duplicate(struct BoundedData *bounded_data);

// De-allocates memory for the given BoundedData instance.
void bounded_data_destroy(struct BoundedData *bounded_data);

// Prints the given BoundedData instance to standard output, no newline.
void bounded_data_print(struct BoundedData *bounded_data);

// true if the contents of the given BoundedData instance are representable as
// text, false otherwise.
bool bounded_data_is_text_representable(struct BoundedData *bounded_data);

// Returns the 64-bit FNV-1a hash for the given data.
// More information: https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
uint64_t bounded_data_hash(struct BoundedData *bounded_data);

#endif