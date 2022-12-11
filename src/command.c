#include <stdio.h>
#include <stdlib.h>

#include "command.h"

/* Returns a string representation of the binary type.
 */
char *binary_type_str(int binary_type) {
  switch (binary_type) {
  case BT_PUT:
    return "PUT";
  case BT_DEL:
    return "DEL";
  case BT_GET:
    return "GET";
  case BT_TAKE:
    return "TAKE";
  case BT_STATS:
    return "STATS";
  case BT_OK:
    return "OK";
  case BT_EINVAL:
    return "EINVAL";
  case BT_ENOTFOUND:
    return "ENOTFOUND";
  case BT_EBINARY:
    return "EBINARY";
  case BT_EBIG:
    return "EBIG";
  case BT_EUNK:
    return "EUNK";
  default:
    return "Unknown binary type";
  }
}

/* Frees the memory of the arguments in the Command struct.
 */
void command_destroy_args(struct Command *command) {
  if (command->arg1 != NULL) {
    free(command->arg1);
    command->arg1 = NULL;
    command->arg1_size = 0;
  }

  if (command->arg2 != NULL) {
    free(command->arg2);
    command->arg2 = NULL;
    command->arg2_size = 0;
  }
}

/* Initializes the given Command struct to an empty state.
 */
void command_initialize(struct Command *command) {
  command->type = BT_OK;
  command->arg1 = command->arg2 = NULL;
  command->arg1_size = command->arg2_size = 0;
}

/* Prints the given Command struct to stdout.
 */
void command_print(struct Command *command) {
  printf("Command:\n");
  printf("Type: %s (%d)\n", binary_type_str(command->type), command->type);
  printf("Arg1 (size %d): %s\n", command->arg1_size, command->arg1);
  printf("Arg2 (size %d): %s\n", command->arg2_size, command->arg2);
}