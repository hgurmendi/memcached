#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> // for abort
#include <string.h> // for strlcat
#include <unistd.h>

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
void read_argument_from_text_client(char **buf, unsigned char **arg,
                                    uint32_t *arg_size) {
  char *token = strsep(buf, " ");
  *arg_size = (uint32_t)strlen(token);
  *arg = calloc(*arg_size, sizeof(unsigned char));
  strncpy((char *)*arg, token, *arg_size);
}

/* True if the string in token equals to the given command.
 */
bool tokenIsCommand(char *token, char *command) {
  return 0 == strncmp(command, token, strlen(command));
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

  if (tokenIsCommand(token, "PUT")) {
    command->type = PUT;
    read_argument_from_text_client(&buf, &command->arg1, &command->arg1_size);
    read_argument_from_text_client(&buf, &command->arg2, &command->arg2_size);
  } else if (tokenIsCommand(token, "DEL")) {
    command->type = DEL;
    read_argument_from_text_client(&buf, &command->arg1, &command->arg1_size);
    command->arg2 = NULL;
  } else if (tokenIsCommand(token, "GET")) {
    command->type = GET;
    read_argument_from_text_client(&buf, &command->arg1, &command->arg1_size);
    command->arg2 = NULL;
  } else if (tokenIsCommand(token, "TAKE")) {
    command->type = TAKE;
    read_argument_from_text_client(&buf, &command->arg1, &command->arg1_size);
    command->arg2 = NULL;
  } else if (tokenIsCommand(token, "STATS")) {
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
unsigned char *read_argument_from_binary_client(int client_fd,
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

int main(int argc, char *argv[]) {
  //   char buf[1000] = {0};
  //   int buf_len = strnlen(buf, sizeof(buf));

  //   // not efficient but i don't give a fuck
  //   for (int i = 1; i < argc; i++) {
  //     printf("This: %02d<%s>\n", i, argv[i]);
  //     strlcat(buf, argv[i], sizeof(buf));

  //     // Add a space at the end of each word
  //     buf_len += strnlen(argv[i], sizeof(buf));
  //     buf[buf_len] = ' ';
  //     buf_len++;
  //     buf[buf_len] = '\0';
  //   }

  //   // Replace the last space with a newline, if there is one.
  //   char *last_space = strrchr(buf, ' ');
  //   if (last_space != NULL) {
  //     *last_space = '\n';
  //   }

  //   parse_text(buf);

  int fd = open("resources/put_e_123.bin", O_RDONLY);

  struct Command *command = parse_binary(fd);

  print_command(command);

  free(command);

  close(fd);

  printf("ahora el protocolo de texto\n");

  fd = open("resources/get_diegote.txt", O_RDONLY);

  command = parse_text(fd);

  print_command(command);

  free(command);

  return EXIT_SUCCESS;
}