#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "binary_parser.h"

/* Reads an argument from the binary client according to the text protocol
 * specification.
 * Allocates memory for the buffer where the binary argument is going to be
 * stored and stores it in the `arg` pointer, and stores the size in `arg_size`.
 * Returns true if it was able to successfully read the argument from the
 * buffer, false otherwise.
 */
static bool read_argument_from_binary_client(int client_fd, uint32_t *arg_size,
                                             char **arg) {
  int bytes_read;

  bytes_read = recv(client_fd, (void *)arg_size, 4, 0);
  if (bytes_read < 0) {
    perror("Error reading argument size from binary client.");
    return false;
  }

  *arg_size = ntohl(*arg_size);
  *arg = calloc(*arg_size, sizeof(char));
  if (*arg == NULL) {
    perror("Error allocating memory for binary client argument");
    return false;
  }

  bytes_read = recv(client_fd, (void *)*arg, *arg_size, 0);
  if (bytes_read < 0) {
    perror("Error reading argument from binary client.");
    return false;
  }

  return true;
}

/* Parses the data read from a client that connected through the binary protocol
 * port and saves the read data in the given Command struct.
 * It's responsibility of the consumer to free the pointers inside the given
 * command, if any of them is not NULL.
 */
void parse_binary(int client_fd, struct Command *command) {
  int bytes_read = recv(client_fd, &command->type, 1, 0);
  if (bytes_read < 0) {
    perror("Error reading command type from binary client");
    command->type = BT_EINVAL;
    return;
  }

  switch (command->type) {
  case BT_PUT:
    if (!read_argument_from_binary_client(client_fd, &command->arg1_size,
                                          &command->arg1) ||
        !read_argument_from_binary_client(client_fd, &command->arg2_size,
                                          &command->arg2)) {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
    break;
  case BT_DEL:
    if (!read_argument_from_binary_client(client_fd, &command->arg1_size,
                                          &command->arg1)) {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
    break;
  case BT_GET:
    if (!read_argument_from_binary_client(client_fd, &command->arg1_size,
                                          &command->arg1)) {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
    break;
  case BT_TAKE:
    if (!read_argument_from_binary_client(client_fd, &command->arg1_size,
                                          &command->arg1)) {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
    break;
  case BT_STATS:
    break;
  default:
    break;
  }
}