#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "binary.h"

/* Reads an argument from the binary client according to the text protocol
 * specification and returns a BoundedData instance. If at any point the binary
 * protocol is broken a NULL pointer is returned, otherwise a pointer to a
 * BoundedData instance with the argument and its size is returned.
 */
static struct BoundedData *read_argument_from_binary_client(int client_fd) {
  int bytes_read;
  uint32_t arg_size;

  bytes_read = read(client_fd, &arg_size, sizeof(uint32_t));
  if (bytes_read != sizeof(uint32_t)) {
    perror("read_argument_from_binary_client read 1");
    return NULL;
  }

  char *arg = malloc(arg_size);
  if (arg == NULL) {
    perror("read_argument_from_binary_client malloc");
    return NULL;
  }

  bytes_read = read(client_fd, arg, arg_size);
  if (bytes_read != arg_size) {
    perror("read_argument_from_binary_client read 2");
    free(arg);
    return NULL;
  }

  return bounded_data_create_from_buffer(arg, arg_size);
}

/* Reads a command from a client that is communicating using the binary
 * protocol.
 * It's responsibility of the consumer to free the pointers inside the
 * given command, if any of them is not NULL.
 */
void read_command_from_binary_client(int client_fd, struct Command *command) {
  int command_type = 0;
  int bytes_read = read(client_fd, &command_type, 1);
  if (bytes_read == -1) {
    perror("read_command_from_binary_client read");
    command->type = BT_EINVAL;
    return;
  }

  command->type = command_type;

  switch (command->type) {
  case BT_PUT:
    command->arg1 = read_argument_from_binary_client(client_fd);
    if (command->arg1 == NULL) {
      command->type = BT_EINVAL;
      break;
    }
    command->arg2 = read_argument_from_binary_client(client_fd);
    if (command->arg2 == NULL) {
      // Clear the previous command
      command_destroy_args(command);
      command->type = BT_EINVAL;
      break;
    }
    break;
  case BT_DEL:
  case BT_GET:
  case BT_TAKE:
    command->arg1 = read_argument_from_binary_client(client_fd);
    if (command->arg1 == NULL) {
      command->type = BT_EINVAL;
      break;
    }
    break;
  case BT_STATS:
  default:
    break;
  }
}

/* Writes a command to a client that is communicating using the binary protocol.
 * Each response is encoded in a `Command` structure, where the `arg1` member
 * represents the optional payload (`NULL` means no payload). The payload is
 * only valid when sending a BT_OK response.
 * Returns -1 if something wrong happens, 0 otherwise.
 */
int write_command_to_binary_client(int client_fd,
                                   struct Command *response_command) {
  char response_code = response_command->type;
  int bytes_written;

  // These are the only valid options for the binary client.
  if (response_code != BT_EINVAL || response_code != BT_OK ||
      response_code != BT_ENOTFOUND) {
    fprintf(stderr, "Invalid response code in binary protocol response");
    return -1;
  }

  bytes_written = write(client_fd, &response_code, 1);
  if (bytes_written != 1) {
    perror("write_command_to_binary_client write 1");
    return -1;
  }

  // No argument with the command, the response has been already sent.
  if (response_command->arg1 == NULL) {
    return 0;
  }

  if (response_command->type != BT_OK) {
    fprintf(stderr,
            "Trying to send an argument to a response different from BT_OK");
    return -1;
  }

  uint32_t arg_size = response_command->arg1->size;
  uint32_t marshalled_size = htonl(arg_size);
  bytes_written = write(client_fd, &marshalled_size, sizeof(uint32_t));
  if (bytes_written != sizeof(uint32_t)) {
    perror("write_command_to_binary_client write 2");
    return -1;
  }

  bytes_written = write(client_fd, response_command->arg1->data, arg_size);
  if (bytes_written != arg_size) {
    perror("write_command_to_binary_client write 3");
    return -1;
  }

  return 0;
}