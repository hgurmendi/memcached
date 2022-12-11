#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <stdint.h>

enum BINARY_TYPES {
  BT_PUT = 11,
  BT_DEL = 12,
  BT_GET = 13,
  BT_TAKE = 14,
  BT_STATS = 21,
  BT_OK = 101,
  BT_EINVAL = 111,
  BT_ENOTFOUND = 112,
  BT_EBINARY = 113,
  BT_EBIG = 114,
  BT_EUNK = 115,
};

struct Command {
  enum BINARY_TYPES type;

  uint32_t arg1_size;
  char *arg1;

  uint32_t arg2_size;
  char *arg2;
};

/* Returns a string representation of the binary type.
 */
char *binary_type_str(int binary_type);

/* Frees the memory of the arguments in the Command struct.
 */
void command_destroy_args(struct Command *command);

/* Initializes the given Command struct to an empty state.
 */
void command_initialize(struct Command *command);

/* Prints the given Command struct to stdout.
 */
void command_print(struct Command *command);

#endif
