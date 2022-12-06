#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parsers.h"

/* Frees the memory of the given Command struct.
 */
void destroy_command(struct Command *command) {
  free(command->arg1);
  free(command->arg2);
  free(command);
}

/* Prints the given Command struct to stdout.
 */
void print_command(struct Command *command) {
  printf("Command:\n");
  printf("Type: %d\n", command->type);
  printf("Arg1 (size %d): %s\n", command->arg1_size, command->arg1);
  printf("Arg2 (size %d): %s\n", command->arg2_size, command->arg2);
}

/* Reads an argument from the text client according to the text protocol
 * specification.
 * Allocates memory for the buffer where the text argument is going to be stored
 * and stores it in the `arg` pointer, and stores the size in `arg_size`.
 */
static void read_argument_from_text_client(char **buf, unsigned char **arg,
                                           uint32_t *arg_size) {
  char *token = strsep(buf, " ");
  *arg_size = (uint32_t)strlen(token);
  *arg = calloc(*arg_size, sizeof(unsigned char));
  strncpy((char *)*arg, token, *arg_size);
}

/* True if the strings are equal.
 */
static bool tokenEquals(const char *token, const char *command) {
  return 0 == strncmp(command, token, strnlen(command, 10));
}

/* Parses the data read from a client that connected through the text protocol
 * port. Returns a Command struct describing the data that was read.
 * The consumer must free the memory of the Command struct received.
 */
struct Command *parse_text(int client_fd) {
  struct Command *command = calloc(sizeof(struct Command), 1);

  // TODO: Figure out why using this kind of buffer definition breaks
  // everything.
  //   char buf[MAX_REQUEST_SIZE + 1];
  char *buf = calloc(MAX_REQUEST_SIZE + 1, sizeof(char));
  int bytes_read = read(client_fd, buf, MAX_REQUEST_SIZE + 1);

  // TODO: this might break if we read more characters after the newline
  // character, for example in a "burst" of data from a client... we might have
  // to do something different like continuously store in a buffer until a
  // newline is found and parse once we find a newline...
  if (bytes_read > MAX_REQUEST_SIZE) {
    command->type = EINVAL;
    return command;
  }

  // TODO: this might break if the above happens... we are assuming the reads
  // are line buffered
  buf[MAX_REQUEST_SIZE] = '\0';

  char *newline = strchr(buf, '\n');
  // Return an incorrect parse result if the command doesn't have a newline
  // character in it
  if (newline == NULL) {
    command->type = EINVAL;
    return command;
  }

  // We forcefully terminate the buffer at the newline, this might be wrong too,
  // related to the comments above.
  *newline = '\0';

  char *token = NULL;
  token = strsep(&buf, " ");

  if (tokenEquals(token, "PUT")) {
    command->type = PUT;
    read_argument_from_text_client(&buf, &command->arg1, &command->arg1_size);
    read_argument_from_text_client(&buf, &command->arg2, &command->arg2_size);
  } else if (tokenEquals(token, "DEL")) {
    command->type = DEL;
    read_argument_from_text_client(&buf, &command->arg1, &command->arg1_size);
    command->arg2 = NULL;
  } else if (tokenEquals(token, "GET")) {
    command->type = GET;
    read_argument_from_text_client(&buf, &command->arg1, &command->arg1_size);
    command->arg2 = NULL;
  } else if (tokenEquals(token, "TAKE")) {
    command->type = TAKE;
    read_argument_from_text_client(&buf, &command->arg1, &command->arg1_size);
    command->arg2 = NULL;
  } else if (tokenEquals(token, "STATS")) {
    command->type = STATS;
    command->arg1 = command->arg2 = NULL;
  } else {
    command->type = EINVAL;
    command->arg1 = command->arg2 = NULL;
  }

  free(buf);

  return command;
}

/* Reads an argument from the client file descriptor according to the binary
 * protocol specification.
 */
static unsigned char *read_argument_from_binary_client(int client_fd,
                                                       uint32_t *arg_size) {
  uint32_t argument_size;
  // TODO: error checking on the bytes read?
  int bytes_read = read(client_fd, &argument_size, 4);
  argument_size = ntohl(argument_size);

  unsigned char *argument = calloc(argument_size, sizeof(unsigned char));
  // TODO: error checking on the bytes read?
  bytes_read = read(client_fd, argument, argument_size);
  *arg_size = argument_size;
  return argument;
}

/* Parses the data read from a client that connected through the binary protocol
 * port. Returns a Command struct describing the data that was read.
 * The consumer must free the memory of the Command struct received.
 */
struct Command *parse_binary(int client_fd) {
  struct Command *command = calloc(1, sizeof(struct Command));

  // TODO: error checking on the bytes read?
  int bytes_read = read(client_fd, &command->type, 1);

  switch (command->type) {
  case PUT:
    command->arg1 =
        read_argument_from_binary_client(client_fd, &command->arg1_size);
    command->arg2 =
        read_argument_from_binary_client(client_fd, &command->arg2_size);

    break;
  case DEL:
    command->arg1 =
        read_argument_from_binary_client(client_fd, &command->arg1_size);

    break;
  case GET:
    command->arg1 =
        read_argument_from_binary_client(client_fd, &command->arg1_size);

    break;
  case TAKE:
    command->arg1 =
        read_argument_from_binary_client(client_fd, &command->arg1_size);

    break;
  case STATS:
    break;
  default:
    break;
  }

  return command;
}