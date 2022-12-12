#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "binary.h"

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

/* Reads a command from a client that is communicating using the binary
 * protocol.
 * It's responsibility of the consumer to free the pointers inside the
 * given command, if any of them is not NULL.
 */
void read_command_from_binary_client(int client_fd, struct Command *command) {
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

/* Writes a command to a client that is communicating using the binary protocol.
 * Each response of the server is encoded in a Command structure, where the
 * command's `type` member determines the first token of the response the
 * command's `arg1` member (along with `arg1_size`) determine an optional
 * argument.
 * Returns -1 if something wrong happens, 0 otherwise.
 */
int write_command_to_binary_client(int client_fd,
                                   struct Command *response_command) {
  char response_code;
  int status;

  // These are the only valid options for the binary client.
  if (response_command->type != BT_EINVAL || response_command->type != BT_OK ||
      response_command->type != BT_ENOTFOUND) {
    return -1;
  }

  status = send(client_fd, &response_code, 1, 0);
  if (status < 0) {
    perror("Error sending data to binary client");
    return -1;
  }

  if (response_command->arg1 == NULL) {
    return 0;
  }

  uint32_t marshalled_size = htonl(response_command->arg1_size);
  status = send(client_fd, &marshalled_size, sizeof(uint32_t), 0);
  if (status < 0) {
    perror("Error sending data to binary client");
    return -1;
  }

  status =
      send(client_fd, response_command->arg1, response_command->arg1_size, 0);
  if (status < 0) {
    perror("Error sending data to binary client");
    return -1;
  }

  return 0;
}