#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "../hashtable/hashtable.h"
#include "../wrapped_free.h"
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
static bool read_argument_from_text_client(struct HashTable *hashtable,
                                           char **buf, char **arg,
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

  *arg = hashtable_attempt_malloc(hashtable, *arg_size * sizeof(char));
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

/* Reads a command from a client that is communicating using the text protocol.
 * It's responsibility of the consumer to free the pointers inside the
 * given command, if any of them is not NULL.
 */
void read_command_from_text_client(struct HashTable *hashtable, int client_fd,
                                   struct Command *command) {
  // TODO: Figure out why using this kind of buffer definition breaks
  // everything.
  //   char buf[MAX_REQUEST_SIZE + 1];
  char *buf = hashtable_attempt_malloc(hashtable,
                                       (MAX_REQUEST_SIZE + 1) * sizeof(char));
  // TODO: Check result of attempt_malloc
  char *tofree = buf;
  int bytes_read = recv(client_fd, buf, MAX_REQUEST_SIZE + 1, 0);

  // TODO: this might break if we read more characters after the newline
  // character, for example in a "burst" of data from a client... we might have
  // to do something different like continuously store in a buffer until a
  // newline is found and parse once we find a newline...
  if (bytes_read > MAX_REQUEST_SIZE || bytes_read < 0) {
    command->type = BT_EINVAL;
    wrapped_free(tofree, (MAX_REQUEST_SIZE + 1) * sizeof(char));
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
    wrapped_free(tofree, (MAX_REQUEST_SIZE + 1) * sizeof(char));
    return;
  }

  // We forcefully terminate the buffer at the newline, this might be wrong too,
  // related to the comments above.
  *newline = '\0';

  char *token = NULL;
  token = strsep(&buf, " ");

  if (tokenEquals(token, "PUT")) {
    if (read_argument_from_text_client(hashtable, &buf, &command->arg1,
                                       &command->arg1_size) &&
        read_argument_from_text_client(hashtable, &buf, &command->arg2,
                                       &command->arg2_size)) {
      command->type = BT_PUT;
    } else {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
  } else if (tokenEquals(token, "DEL")) {
    if (read_argument_from_text_client(hashtable, &buf, &command->arg1,
                                       &command->arg1_size)) {
      command->type = BT_DEL;
    } else {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
  } else if (tokenEquals(token, "GET")) {
    if (read_argument_from_text_client(hashtable, &buf, &command->arg1,
                                       &command->arg1_size)) {
      command->type = BT_GET;
    } else {
      command->type = BT_EINVAL;
      command_destroy_args(command);
    }
  } else if (tokenEquals(token, "TAKE")) {
    if (read_argument_from_text_client(hashtable, &buf, &command->arg1,
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

  wrapped_free(tofree, (MAX_REQUEST_SIZE + 1) * sizeof(char));
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
  char write_buf[MAX_REQUEST_SIZE];
  int bytes_written = 0;

  switch (response_command->type) {
  case BT_EBINARY:
    bytes_written = snprintf(write_buf, MAX_REQUEST_SIZE, "EBINARY\n");
    break;
  case BT_EINVAL:
    bytes_written = snprintf(write_buf, MAX_REQUEST_SIZE, "EINVAL\n");
    break;
  case BT_OK:
    // Write the first argument of the command as the "argument" of the OK
    // response.
    if (response_command->arg1 != NULL) {
      bytes_written = snprintf(write_buf, MAX_REQUEST_SIZE, "OK %s\n",
                               response_command->arg1);
    } else {
      bytes_written = snprintf(write_buf, MAX_REQUEST_SIZE, "OK\n");
    }
    break;
  case BT_ENOTFOUND:
    bytes_written = snprintf(write_buf, MAX_REQUEST_SIZE, "ENOTFOUND\n");
    break;
  default:
    printf("Encountered unknown command type whem sending data to client...\n");
    return -1;
    break;
  }

  status = send(client_fd, write_buf, bytes_written, 0);
  if (status < 0) {
    perror("Error sending data to text client");
    return -1;
  }

  return 0;
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
