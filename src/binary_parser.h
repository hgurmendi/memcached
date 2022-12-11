#ifndef __BINARY_PARSER_H__
#define __BINARY_PARSER_H__

#include "command.h"

/* Parses the data read from a client that connected through the binary protocol
 * port and saves the read data in the given Command struct.
 * It's responsibility of the consumer to free the pointers inside the given
 * command, if any of them is not NULL.
 */
void parse_binary(int client_fd, struct Command *command);

#endif