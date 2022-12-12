#ifndef __PROTOCOL_BINARY_H__
#define __PROTOCOL_BINARY_H__

#include "command.h"

/* Reads a command from a client that is communicating using the binary
 * protocol.
 * It's responsibility of the consumer to free the pointers inside the
 * given command, if any of them is not NULL.
 */
void read_command_from_binary_client(int client_fd, struct Command *command);

/* Writes a command to a client that is communicating using the binary protocol.
 * Each response of the server is encoded in a Command structure, where the
 * command's `type` member determines the first token of the response the
 * command's `arg1` member (along with `arg1_size`) determine an optional
 * argument.
 * Returns -1 if something wrong happens, 0 otherwise.
 */
int write_command_to_binary_client(int client_fd,
                                   struct Command *response_command);

#endif