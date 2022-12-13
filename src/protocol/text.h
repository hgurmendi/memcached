#ifndef __PROTOCOL_TEXT_H__
#define __PROTOCOL_TEXT_H__

#include "../hashtable/hashtable.h"
#include "command.h"

#define MAX_REQUEST_SIZE 2048

/* Reads a command from a client that is communicating using the text protocol.
 * It's responsibility of the consumer to free the pointers inside the
 * given command, if any of them is not NULL.
 */
void read_command_from_text_client(struct HashTable *hashtable, int client_fd,
                                   struct Command *command);

/* Writes a command to a client that is communicating using the text protocol.
 * Each response of the server is encoded in a Command structure, where the
 * command's `type` member determines the first token of the response the
 * command's `arg1` member (along with `arg1_size`) determine an optional
 * argument.
 * Returns -1 if something wrong happens, 0 otherwise.
 */
int write_command_to_text_client(int client_fd,
                                 struct Command *response_command);

/* true if the given char array is representable as text, false otherwise.
 */
bool is_text_representable(uint32_t size, char *arr);

#endif