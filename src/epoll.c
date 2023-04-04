#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"
#include "sockets.h"

// Frees and clears the pointer to the response content of the EventData
// instance.
void event_data_clear_response_content(struct EventData *event_data) {
  if (event_data->response_content != NULL) {
    bounded_data_destroy(event_data->response_content);
    event_data->response_content = NULL;
  }
}

// Resets the state of the client to handle a new request. This frees the read
// and write buffers should the be different from NULL.
void event_data_reset(struct EventData *event_data) {
  if (event_data->connection_type == TEXT) {
    // Initial state for a text client.
    event_data->client_state = TEXT_READY;
  } else {
    // Initial state for a binary client.
    event_data->client_state = BINARY_READY;
  }
  if (event_data->read_buffer != NULL) {
    bounded_data_destroy(event_data->read_buffer);
    event_data->read_buffer = NULL;
  }
  event_data->total_bytes_read = 0;
  event_data->response_type = BT_EINVAL;
  event_data_clear_response_content(event_data);
  event_data->total_bytes_written = 0;
  event_data->command_type = BT_EINVAL;
  event_data->arg_size = 0;
  if (event_data->arg1 != NULL) {
    bounded_data_destroy(event_data->arg1);
    event_data->arg1 = NULL;
  }
  if (event_data->arg2 != NULL) {
    bounded_data_destroy(event_data->arg2);
    event_data->arg2 = NULL;
  }
}

// Initializes an EventData struct.
void event_data_initialize(struct EventData *event_data, int fd,
                           enum ConnectionType connection_type) {
  event_data->fd = fd;
  event_data->connection_type = connection_type;
  strncpy(event_data->host, "UNINITIALIZED", NI_MAXHOST);
  strncpy(event_data->port, "UNINITIALIZED", NI_MAXSERV);
  event_data->read_buffer = NULL;
  event_data->response_content = NULL;
  event_data->command_type = BT_EINVAL;
  event_data->arg1 = NULL;
  event_data->arg2 = NULL;
  event_data_reset(event_data);
}

// Allocates memory for an EventData struct and initializes it with minimal
// data.
struct EventData *event_data_create(int fd,
                                    enum ConnectionType connection_type) {
  struct EventData *event_data = malloc(sizeof(struct EventData));
  if (event_data == NULL) {
    perror("event_data_create malloc");
    abort();
  }

  event_data_initialize(event_data, fd, connection_type);

  return event_data;
}

// Frees the allocated memory for an EventData struct and performs any required
// frees of the contained data.
static void event_data_destroy(struct EventData *event_data) {
  free(event_data);
}

// Closes the client associated to the given EventData struct and frees the
// resources of the struct.
void event_data_close_client(struct EventData *event_data) {
  close(event_data->fd);
  event_data_reset(event_data);
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
  event_data = event_data_create(text_fd, TEXT);
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
  event_data = event_data_create(binary_fd, BINARY);
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
char *connection_type_str(enum ConnectionType connection_type) {
  switch (connection_type) {
  case TEXT:
    return "TEXT";
  case BINARY:
    return "BINARY";
  default:
    return "UNKNOWN_CONNECTION_TYPE";
  }
}

// Returns a string representing the client state.
char *client_state_str(enum ClientState client_state) {
  switch (client_state) {
  case TEXT_READY:
    return "TEXT_READY";
  case TEXT_READING_INPUT:
    return "TEXT_READING_INPUT";
  case TEXT_WRITING_COMMAND:
    return "TEXT_WRITING_COMMAND";
  case TEXT_WRITING_CONTENT:
    return "TEXT_WRITING_CONTENT";
  case TEXT_WRITING_NEWLINE:
    return "TEXT_WRITING_NEWLINE";
  case BINARY_READY:
    return "BINARY_READY";
  case BINARY_READING_COMMAND:
    return "BINARY_READING_COMMAND";
  case BINARY_READING_ARG1_SIZE:
    return "BINARY_READING_ARG1_SIZE";
  case BINARY_READING_ARG1_DATA:
    return "BINARY_READING_ARG1_DATA";
  case BINARY_READING_ARG2_SIZE:
    return "BINARY_READING_ARG2_SIZE";
  case BINARY_READING_ARG2_DATA:
    return "BINARY_READING_ARG2_DATA";
  case BINARY_WRITING_COMMAND:
    return "BINARY_WRITING_COMMAND";
  case BINARY_WRITING_CONTENT_SIZE:
    return "BINARY_WRITING_CONTENT_SIZE";
  case BINARY_WRITING_CONTENT_DATA:
    return "BINARY_WRITING_CONTENT_DATA";
  default:
    return "UNKNOWN_CLIENT_STATE";
  }
}