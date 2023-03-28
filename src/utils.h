#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdbool.h>
#include <stdint.h>

// true if the contents of the given char array are representable as text, false
// otherwise.
bool is_text_representable(char *arr, uint64_t arr_size);

#endif