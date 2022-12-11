#ifndef __PARSERS_H__
#define __PARSERS_H__

#include <stdint.h>

#define MAX_REQUEST_SIZE 2048

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

char *binary_type_str(int binary_type);

/* Frees the memory of the given Command struct.
 */
void destroy_command(struct Command *command);

/* Initializes the given command to an empty state.
 */
void initialize_command(struct Command *command);

/* Prints the given Command struct to stdout.
 */
void print_command(struct Command *command);

/* Parses the data read from a client that connected through the text protocol
 * port. Returns a Command struct describing the data that was read.
 * The consumer must free the memory of the Command struct received.
 */
struct Command *parse_text(int client_fd);

/* Parses the data read from a client that connected through the binary protocol
 * port. Returns a Command struct describing the data that was read.
 * The consumer must free the memory of the Command struct received.
 */
struct Command *parse_binary(int client_fd);

#endif