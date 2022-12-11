#ifndef __TEXT_PARSER_H__
#define __TEXT_PARSER_H__

#include "command.h"

#define MAX_REQUEST_SIZE 2048

/* Parses the data read from a client that connected through the text protocol
 * port and saves the read data in the given Command struct.
 * It's responsibility of the consumer to free the pointers inside the given
 * command if any of them is not NULL.
 */
void parse_text(int client_fd, struct Command *command);

#endif