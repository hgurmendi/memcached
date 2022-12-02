#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> // for abort
#include <string.h> // for strlcat
#include <unistd.h>

#define MAX_REQUEST_SIZE 2048
#define MAX_COMMAND_SIZE 10

// Compile regularly with cc test_parse.c

// todo: cambiar el nombre
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

void destroy_command(struct Command *command) {
  free(command->arg1);
  free(command->arg2);
  free(command);
}

void print_command(struct Command *command) {
  printf("Command:\n");
  printf("Type: %d\n", command->type);
  printf("Arg1 (size %d): %s\n", command->arg1_size, command->arg1);
  printf("Arg2 (size %d): %s\n", command->arg2_size, command->arg2);
}

void read_argument_from_text_client(char **buf, unsigned char *arg,
                                    uint32_t *arg_size) {
  char *token = strsep(buf, " ");
  printf("Este es el token papa: <%s>\n", token);
  size_t argument_size = strlen(token);
  *arg_size = (uint32_t)argument_size;
  arg = calloc(*arg_size, sizeof(unsigned char));
  strncpy((char *)arg, token, *arg_size);
  printf("Este es el argumento y su dir: <%s> <%p>\n", (char *)arg, arg);
}

bool tokenIsCommand(char *token, char *command) {
  return strncmp(command, token, MAX_COMMAND_SIZE) == 0;
}

struct Command *parse_text(int client_fd) {
  // TODO: el cliente de esta funcion libera la memoria.
  struct Command *command = calloc(sizeof(struct Command), 1);

  // TODO: Figure out why using this kind of buffer definition breaks
  // everything.
  //   char buf[MAX_REQUEST_SIZE + 1];
  char *buf = calloc(MAX_REQUEST_SIZE + 1, sizeof(char));
  int bytes_read = read(client_fd, buf, MAX_REQUEST_SIZE + 1);

  if (bytes_read > MAX_REQUEST_SIZE) {
    printf("Todo mal wacho, la request es muy grande\n");
    abort();
  }

  // because we're going to use strchr, we make sure there is a null character
  // in the string
  buf[MAX_REQUEST_SIZE] = '\0';

  char *newline = strchr(buf, '\n');

  if (newline == NULL) {
    printf("Todo mal wacho, la request no tiene un newline\n");
    abort();
  }

  // Terminamos el string en el newline, total ya sabemos que está.
  *newline = '\0';

  char *token = NULL;

  printf("Parsing the buffer <%s>\n", buf);

  token = strsep(&buf, " ");

  printf("First token: <%s>\n", token);
  if (tokenIsCommand(token, "PUT")) {
    command->type = PUT;

    printf("AAAA\n");
    read_argument_from_text_client(&buf, command->arg1, &command->arg1_size);
    read_argument_from_text_client(&buf, command->arg2, &command->arg2_size);
    printf("A ver: <%s> (%p) <%s> (%p)\n", command->arg1, command->arg1,
           command->arg2, command->arg2);
  } else if (tokenIsCommand(token, "DEL")) {
    command->type = DEL;

    read_argument_from_text_client(&buf, command->arg1, &command->arg1_size);
    command->arg2 = NULL;
  } else if (tokenIsCommand(token, "GET")) {
    command->type = GET;

    read_argument_from_text_client(&buf, command->arg1, &command->arg1_size);
    command->arg2 = NULL;
  } else if (tokenIsCommand(token, "TAKE")) {
    command->type = TAKE;

    read_argument_from_text_client(&buf, command->arg1, &command->arg1_size);
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
  int bytes_read = read(client_fd, &argument_size, 4);
  argument_size = ntohl(argument_size);

  printf("El primer argumento mide %d\n", argument_size);

  unsigned char *argument = calloc(argument_size, sizeof(unsigned char));

  bytes_read = read(client_fd, argument, argument_size);
  printf("Leimos %d bytes del file.\n", bytes_read);

  printf("El segundo argumento es <%s>\n", argument);

  *arg_size = argument_size;
  return argument;
}

struct Command *parse_binary(int client_fd) {
  // on the real version of this function, we have to continuously read from the
  // client in chunks because the data is arbitrarily large (actually, its size
  // in bytes has to be represented by a 32bit integer).

  struct Command *command = calloc(1, sizeof(struct Command));

  int bytes_read = read(client_fd, &command->type, 1);
  printf("Leimos %d bytes del file.\n", bytes_read);

  switch (command->type) {
  case PUT:
    printf("Es un PUT\n");

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