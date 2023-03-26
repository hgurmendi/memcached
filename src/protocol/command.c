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
    bounded_data_destroy(command->arg1);
    command->arg1 = NULL;
  }
  if (command->arg2 != NULL) {
    bounded_data_destroy(command->arg2);
    command->arg2 = NULL;
  }
}

/* Initializes the given Command struct to an empty state.
 */
void command_initialize(struct Command *command) {
  command->type = BT_OK;
  command->arg1 = command->arg2 = NULL;
}

/* Prints the given Command struct to stdout.
 */
void command_print(struct Command *command) {
  printf("Command:\n");
  printf("Type: %s (%d)\n", binary_type_str(command->type), command->type);
  // TODO: Improve printing because BoundedData->data might not be printable.
  printf("Arg1 (size %d): %s\n", command->arg1->size, command->arg1->data);
  printf("Arg2 (size %d): %s\n", command->arg2->size, command->arg2->data);
}