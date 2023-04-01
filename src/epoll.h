#ifndef __EPOLL_H__
#define __EPOLL_H__

// We need to define _GNU_SOURCE to get NI_MAXHOST and NI_MAXSERV
#define _GNU_SOURCE

#include <netdb.h> // for NI_MAXHOST

#include "binary_type.h"  // for struct BinaryType
#include "bounded_data.h" // for struct BoundedData

enum ClientState {
  // Text client states, in order:
  TEXT_READY,
  TEXT_READING_INPUT,
  TEXT_WRITING_COMMAND,
  TEXT_WRITING_CONTENT,
  TEXT_WRITING_NEWLINE,
  // Binary client states, in order:
  BINARY_READY,
  BINARY_READING_COMMAND,
  BINARY_READING_ARG1_SIZE,
  BINARY_READING_ARG1_DATA,
  BINARY_READING_ARG2_SIZE,
  BINARY_READING_ARG2_DATA,
  BINARY_WRITING_COMMAND,
  BINARY_WRITING_CONTENT_SIZE,
  BINARY_WRITING_CONTENT_DATA,
};

enum ConnectionType { BINARY, TEXT };

struct EventData {
  // Connection data:
  int fd;                              // File descriptor of the client socket.
  enum ConnectionType connection_type; // Connection type of the client.
  char host[NI_MAXHOST];               // IP address.
  char port[NI_MAXSERV];               // Port.
  // Client state:
  enum ClientState client_state;        // State of the client.
  struct BoundedData *read_buffer;      // Current read buffer of the client.
  size_t total_bytes_read;              // Total bytes read into the buffer.
  enum BinaryType response_type;        // Response command.
  struct BoundedData *response_content; // Current write buffer of the client.
  size_t total_bytes_written; // Total bytes written for the current state.
};

#define MAX_EPOLL_EVENTS 128

// Allocates memory for an EventData struct with some initial data.
struct EventData *event_data_create(int fd,
                                    enum ConnectionType connection_type);

// Initializes the epoll instance and adds both the text protocol socket and
// the binary protocol socket to the interest list. Returns the epoll file
// descriptor.
int epoll_initialize(int text_fd, int binary_fd);

// Closes the client associated to the given EventData struct and frees the
// resources of the struct.
void event_data_close_client(struct EventData *event_data);

// Returns a string representing the connection type.
char *connection_type_str(enum ConnectionType connection_type);

// Returns a string representing the client state.
char *client_state_str(enum ClientState client_state);

// Resets the state of the client to handle a new request.
void event_data_reset(struct EventData *event_data);

// Frees and clears the pointer to the response content of the EventData
// instance.
void event_data_clear_response_content(struct EventData *event_data);

#endif