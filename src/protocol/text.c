#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../bounded_data.h"
#include "command.h"
#include "text.h"

/* True if the strings are equal.
 */
static bool tokenEquals(const char *token, const char *command) {
  return 0 == strncmp(command, token, strnlen(command, 10));
}

/* Reads a command from a client that is communicating using the text protocol.
 * It's responsibility of the consumer to free the pointers inside the
 * given command, if any of them is not NULL.
 */
void read_command_from_text_client(int client_fd, struct Command *command) {
  char buffer[REQUEST_BUFFER_SIZE];
  char *buf_start = buffer;
  int bytes_read = read(client_fd, buffer, REQUEST_BUFFER_SIZE);

  // If the call to `read` fails or we read more characters than we are allowed
  // to, return an invalid command.
  // If the call to `read` failed with errno EAGAIN something must be wrong
  // because the file descriptor should be ready for reading, so we log a
  // message.
  if (bytes_read == -1 || bytes_read > MAX_REQUEST_SIZE) {
    command->type = BT_EINVAL;
    if (errno == EAGAIN) {
      perror("read_command_from_text_client `read` should be ready");
    }
    return;
  }

  // Try to find a newline character within the bounds of the maximum allowed
  // size for the request. If we don't find a newline just return an invalid
  // command.
  char *newline = memchr(buffer, '\n', MAX_REQUEST_SIZE);
  if (newline == NULL) {
    command->type = BT_EINVAL;
    return;
  }

  // Now we know that the request has a newline character so it's probably well
  // formed. We replace the newline character with a terminating null character
  // so that we can tokenize the buffer with strsep.
  *newline = '\0';

  // We start tokenizing the buffer. We start with the first token that should
  // be the command of the request.
  char *command_token = strsep(&buf_start, " ");
  // I don't think the check below is necessary. I think that getting a NULl
  // pointer on the first call of strsep would mean that the received command is
  // an empty line.
  // if (token == NULL) {
  //   command->type = BT_EINVAL;
  //   return;
  // }

  // We read both argument tokens. Any of these might be NULL, in which case
  // that argument doesn't exist.
  char *arg1_token = strsep(&buf_start, " ");
  char *arg2_token = strsep(&buf_start, " ");

  // Parse PUT.
  if (tokenEquals(command_token, "PUT")) {
    if (arg1_token == NULL || arg2_token == NULL) {
      command->type = BT_EINVAL;
      return;
    }

    command->type = BT_PUT;
    command->arg1 = bounded_data_create_from_string_duplicate(arg1_token);
    command->arg2 = bounded_data_create_from_string_duplicate(arg2_token);
    return;
  }

  // Parse DEL.
  if (tokenEquals(command_token, "DEL")) {
    if (arg1_token == NULL) {
      command->type = BT_EINVAL;
      return;
    }

    command->type = BT_DEL;
    command->arg1 = bounded_data_create_from_string_duplicate(arg1_token);
    return;
  }

  // Parse GET.
  if (tokenEquals(command_token, "GET")) {
    if (arg1_token == NULL) {
      command->type = BT_EINVAL;
      return;
    }

    command->type = BT_GET;
    command->arg1 = bounded_data_create_from_string_duplicate(arg1_token);
    return;
  }

  // Parse TAKE.
  if (tokenEquals(command_token, "TAKE")) {
    if (arg1_token == NULL) {
      command->type = BT_EINVAL;
      return;
    }

    command->type = BT_TAKE;
    command->arg1 = bounded_data_create_from_string_duplicate(arg1_token);
    return;
  }

  // Parse STATS.
  if (tokenEquals(command_token, "STATS")) {
    command->type = BT_STATS;
    return;
  }

  // The request doesn't belong to a valid command.
  command->type = BT_EINVAL;
  return;
}

/* Writes a command to a client that is communicating using the text protocol.
 * Each response of the server is encoded in a Command structure, where the
 * command's `type` member determines the first token of the response the
 * command's `arg1` member (along with `arg1_size`) determine an optional
 * argument.
 * Returns -1 if something wrong happens, 0 otherwise.
 */
int write_command_to_text_client(int client_fd,
                                 struct Command *response_command) {
  int status;
  char write_buf[REQUEST_BUFFER_SIZE];
  int bytes_written = 0;

  switch (response_command->type) {
  case BT_EBINARY:
    bytes_written = snprintf(write_buf, REQUEST_BUFFER_SIZE, "EBINARY\n");
    break;
  case BT_EINVAL:
    bytes_written = snprintf(write_buf, REQUEST_BUFFER_SIZE, "EINVAL\n");
    break;
  case BT_OK:
    // Write the first argument of the command as the "argument" of the OK
    // response. We assume it's text representable.
    if (response_command->arg1 != NULL) {
      // First check if the response is too big, in which case we'll just return
      // EBIG.
      if (response_command->arg1->size > MAX_REQUEST_SIZE) {
        bytes_written = snprintf(write_buf, REQUEST_BUFFER_SIZE, "EBIG\n");
      } else {
        bytes_written = snprintf(write_buf, REQUEST_BUFFER_SIZE, "OK %s\n",
                                 response_command->arg1->data);
      }
    } else {
      bytes_written = snprintf(write_buf, REQUEST_BUFFER_SIZE, "OK\n");
    }
    break;
  case BT_ENOTFOUND:
    bytes_written = snprintf(write_buf, REQUEST_BUFFER_SIZE, "ENOTFOUND\n");
    break;
  default:
    printf("Encountered unknown command type whem sending data to client...\n");
    return -1;
    break;
  }

  status = write(client_fd, write_buf, bytes_written);
  if (status < 0) {
    perror("write_command_to_text_client write");
    return -1;
  }

  return 0;
}
