#ifndef __NEW_EPOLL_H__
#define __NEW_EPOLL_H__

#include <netdb.h>

#include "common.h"

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

#endif