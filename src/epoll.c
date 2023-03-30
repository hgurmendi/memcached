#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"
#include "sockets.h"

// Allocates memory for an uninitialized EventData struct.
struct EventData *event_data_create() {
  struct EventData *event_data = malloc(sizeof(struct EventData));
  if (event_data == NULL) {
    perror("event_data_create malloc");
    abort();
  }

  // Just in case we initialize it with incorrect data.
  event_data->fd = -1;
  strncpy(event_data->host, "UNINITIALIZED", NI_MAXHOST);
  strncpy(event_data->port, "UNINITIALIZED", NI_MAXSERV);

  return event_data;
}

// Frees the allocated memory for an EventData struct and performs any required
// frees of the contained data.
static void event_data_destroy(struct EventData *event_data) {
  free(event_data);
}

// Closes the client associated to the given EventData struct and frees the
// resources of the struct.
void close_client(struct EventData *event_data) {
  close(event_data->fd);
  event_data_destroy(event_data);
}

// Initializes the epoll instance and adds both the text protocol socket and the
// binary protocol socket to the interest list. Returns the epoll file
// descriptor.
int epoll_initialize(int text_fd, int binary_fd) {
  struct epoll_event event;
  struct EventData *event_data = NULL;

  // Create the epoll instance.
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    perror("initialize_server epoll_create1");
    abort();
  }

  // Add the listen socket for the text protocol to the epoll interest list.
  event_data = event_data_create();
  event_data->fd = text_fd;
  event_data->connection_type = TEXT;
  strncpy(event_data->host, "text protocol fd", NI_MAXHOST);
  strncpy(event_data->host, "text protocol fd", NI_MAXSERV);
  event.data.ptr = event_data;
  event.events = EPOLLIN | EPOLLET;
  int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, text_fd, &event);
  if (ret == -1) {
    perror("initialize_server epoll_ctl text");
    abort();
  }

  // Add the listen socket for the binary protocol to the epoll interest list.
  event_data = event_data_create();
  event_data->fd = binary_fd;
  event_data->connection_type = BINARY;
  strncpy(event_data->host, "binary protocol fd", NI_MAXHOST);
  strncpy(event_data->host, "binary protocol fd", NI_MAXSERV);
  event.data.ptr = event_data;
  event.events = EPOLLIN | EPOLLET;
  ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, binary_fd, &event);
  if (ret == -1) {
    perror("initialize_server epoll_ctl binary");
    abort();
  }

  return epoll_fd;
}

// Returns a string representing the connection type.
char *connection_type_str(enum ConnectionTypes connection_type) {
  switch (connection_type) {
  case TEXT:
    return "TEXT";
  case BINARY:
    return "BINARY";
  default:
    return "UNKNOWN_CONNECTION_TYPE";
  }
}