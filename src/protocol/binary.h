#ifndef __PROTOCOL_BINARY_H__
#define __PROTOCOL_BINARY_H__

#include "command.h"

/* Reads a command from a client that is communicating using the binary
 * protocol.
 * It's responsibility of the consumer to free the pointers inside the
 * given command, if any of them is not NULL.
 */
void read_command_from_binary_client(int client_fd, struct Command *command);

#endif