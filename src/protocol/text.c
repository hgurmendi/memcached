#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "command.h"
#include "text.h"

/* Reads an argument from the text client according to the text protocol
 * specification.
 * Allocates memory for the buffer where the text argument is going to be stored
 * and stores it in the `arg` pointer, and stores the size in `arg_size`.
 * Returns true if it was able to successfully read the argument from the
 * buffer, false otherwise.
 * Mutates the char bufer pointed at by buf because we use strdup.
 */
static bool read_argument_from_text_client(char **buf, char **arg,
                                           uint32_t *arg_size) {
  if (*buf == NULL) {
    // We ran out of space-separated tokens in the text buffer, so we can't read
    // any more text arguments.
    return false;
  }

  char *token = strsep(buf, " ");
  *arg_size = strnlen(token, MAX_REQUEST_SIZE) + 1;
  if (*arg_size <= 1) {
    // Got an empty token... i.e. "GET "
    return false;
  }

  *arg = calloc(*arg_size, sizeof(char));
  if (*arg == NULL) {
    perror("Error allocating memory for text client argument");
    return false;
  }

  memcpy(*arg, token, *arg_size);
  return true;
}

/* True if the strings are equal.
 */
static bool tokenEquals(const char *token, const char *command) {
  return 0 == strncmp(command, token, strnlen(command, 10));
}

/* Parses the data read from a client that connected through the text protocol
 * port and saves the read data in the given Command struct.
 * It's responsibility of the consumer to free the pointers inside the given
 * command, if any of them is not NULL.
 */
void parse_text(int client_fd, struct Command *command) {
  // TODO: Figure out why using this kind of buffer definition breaks
  // everything.
  //   char buf[MAX_REQUEST_SIZE + 1];
  char *buf = calloc(MAX_REQUEST_SIZE + 1, sizeof(char));
  char *tofree = buf;
  int bytes_read = recv(client_fd, buf, MAX_REQUEST_SIZE + 1, 0);

  // TODO: this might break if we read more characters after the newline
  // character, for example in a "burst" of data from a client... we might have
  // to do something different like continuously store in a buffer until a
  // newline is found and parse once we find a newline...
  if (bytes_read > MAX_REQUEST_SIZE || bytes_read < 0) {
    command->type = BT_EINVAL;
    free(tofree);
    return;
  }

  // TODO: this might break if the above happens... we are assuming the reads
  // are line buffered
  buf[MAX_REQUEST_SIZE] = '\0';

  char *newline = strchr(buf, '\n');
  // Return an incorrect parse result if the command doesn't have a newline
  // character in it
  if (newline == NULL) {
    command->type = BT_EINVAL;
    free(tofree);
    return;
  }

  // We forcefully terminate the buffer at the newline, this might be wrong too,
  // related to the comments above.
  *newline = '\0';

  char *token = NULL;
  token = strsep(&buf, " ");

  if (tokenEquals(token, "PUT")) {
    if (read_argument_from_text_client(&buf, &command->arg1,
                                       &command->arg1_size) &&
        read_argument_from_text_client(&buf, &command->arg2,
                                       &command->arg2_size)) {
      command->type = BT_PUT;
    } else {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
  } else if (tokenEquals(token, "DEL")) {
    if (read_argument_from_text_client(&buf, &command->arg1,
                                       &command->arg1_size)) {
      command->type = BT_DEL;
    } else {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
  } else if (tokenEquals(token, "GET")) {
    if (read_argument_from_text_client(&buf, &command->arg1,
                                       &command->arg1_size)) {
      command->type = BT_GET;
    } else {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
  } else if (tokenEquals(token, "TAKE")) {
    if (read_argument_from_text_client(&buf, &command->arg1,
                                       &command->arg1_size)) {
      command->type = BT_TAKE;
    } else {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
  } else if (tokenEquals(token, "STATS")) {
    command->type = BT_STATS;
  } else {
    command->type = BT_EINVAL;
  }

  free(tofree);
}

/* true if the given char array is representable as text, false otherwise.
 */
bool is_text_representable(uint32_t size, char *arr) {
  for (int i = 0; i < size - 1; i++) {
    if (!isprint(arr[i])) {
      return false;
    }
  }

  return arr[size - 1] == '\0';
}
