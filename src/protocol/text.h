#ifndef __PROTOCOL_TEXT_H__
#define __PROTOCOL_TEXT_H__

#include "command.h"

#define MAX_REQUEST_SIZE 2048

/* Reads a command from a client that is communicating using the text protocol.
 * It's responsibility of the consumer to free the pointers inside the
 * given command, if any of them is not NULL.
 */
void read_command_from_text_client(int client_fd, struct Command *command);

/* true if the given char array is representable as text, false otherwise.
 */
bool is_text_representable(uint32_t size, char *arr);

#endif