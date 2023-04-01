#include <ctype.h>

#include "utils.h"

// true if the contents of the given char array are representable as text, false
// otherwise.
bool is_text_representable(char *arr, uint64_t arr_size) {
  for (int i = 0; i < arr_size - 1; i++) {
    if (isspace(arr[i]) || !isprint(arr[i])) {
      return false;
    }
  }
  return true;
}