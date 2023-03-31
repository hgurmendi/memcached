#ifndef __EPOLL_H__
#define __EPOLL_H__

// We need to define _GNU_SOURCE to get NI_MAXHOST and NI_MAXSERV
#define _GNU_SOURCE

#include <netdb.h> // for NI_MAXHOST

#include "binary_type.h"  // for struct BinaryType
#include "bounded_data.h" // for struct BoundedData

enum ClientState { READ_READY, READING, WRITE_READY, WRITING };

enum ConnectionType { BINARY, TEXT };

struct EventData {
  int fd;                              // File descriptor of the client socket.
  char host[NI_MAXHOST];               // IP address.
  char port[NI_MAXSERV];               // Port.
  enum ConnectionType connection_type; // Connection type of the client.
  enum ClientState client_state;
  struct BoundedData *read_buffer;
  size_t total_bytes_read;
  enum BinaryType response_type;
  struct BoundedData *write_buffer;
};

#define MAX_EPOLL_EVENTS 128

// Allocates memory for an uninitialized EventData struct.
struct EventData *event_data_create();

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
void event_data_reset_client(struct EventData *event_data);

#endif