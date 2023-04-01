#include <ctype.h>

#include "utils.h"

// true if the contents of the given char array are representable as text, false
// otherwise.
bool is_text_representable(char *arr, uint64_t arr_size) {

  // TODO: maybe change this to make sure that the characters are printable AND
  // are not whitespace (isspace). No need to check for terminating null
  // character.
  for (int i = 0; i < arr_size - 1; i++) {
    if (!isprint(arr[i])) {
      return false;
    }
  }
  // Also check it's terminated with a null character.
  return arr[arr_size - 1] == '\0';
}