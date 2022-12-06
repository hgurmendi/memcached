#ifndef __PARSERS_H__
#define __PARSERS_H__

#include <stdint.h>

#define MAX_REQUEST_SIZE 2048

enum BINARY_TYPES {
  PUT = 11,
  DEL = 12,
  GET = 13,
  TAKE = 14,
  STATS = 21,
  OK = 101,
  EINVAL = 111,
  ENOTFOUND = 112,
  EBINARY = 113,
  EBIG = 114,
  EUNK = 115,
};

struct Command {
  enum BINARY_TYPES type;

  uint32_t arg1_size;
  unsigned char *arg1;

  uint32_t arg2_size;
  unsigned char *arg2;
};

/* Frees the memory of the given Command struct.
 */
void destroy_command(struct Command *command);

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