#ifndef __EPOLL_H__
#define __EPOLL_H__

// We need to define _GNU_SOURCE to get NI_MAXHOST and NI_MAXSERV
#define _GNU_SOURCE

#include <netdb.h>

enum ConnectionTypes { BINARY, TEXT };

struct EventData {
  int fd;                               // File descriptor of the client socket.
  char host[NI_MAXHOST];                // IP address.
  char port[NI_MAXSERV];                // Port.
  enum ConnectionTypes connection_type; // Connection type of the client.
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
void close_client(struct EventData *event_data);

// Returns a string representing the connection type.
char *connection_type_str(enum ConnectionTypes connection_type);

#endif