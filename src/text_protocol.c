#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"
#include "text_protocol.h"
#include "worker_state.h"

// Maximum request size for the text protocol.
#define MAX_TEXT_REQUEST_SIZE 2048

// Buffer size for the text protocol.
#define TEXT_REQUEST_BUFFER_SIZE (MAX_TEXT_REQUEST_SIZE + 2)

#define CLIENT_READ_ERROR -1
#define CLIENT_READ_CLOSED 0
#define CLIENT_READ_SUCCESS 1
#define CLIENT_READ_INCOMPLETE 2

// Adds the given client event back to the epoll interest list. Returns 0 if
// successful, -1 otherwise.
static int epoll_mod_client(struct WorkerArgs *args,
                            struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  event->events = EPOLLIN | EPOLLET | EPOLLONESHOT;
  int rv = epoll_ctl(args->epoll_fd, EPOLL_CTL_MOD, event_data->fd, event);
  if (rv == -1) {
    perror("epoll_mod_client epoll_ctl");
    return -1;
  }
  return 0;
}

// Reads from the current client until a newline is found or until the client's
// read buffer is full. If a newline character is found then it's replaced by a
// null character and CLIENT_READ_SUCCESS is returned. If the client closes the
// connection then CLIENT_READ_CLOSED is returned. If the client's read buffer
// has some space in it and the client is not ready for reading then
// CLIENT_READ_INCOMPLETE is returned. If an error happens then
// CLIENT_READ_ERROR is returned.
static int read_until_newline(struct WorkerArgs *args,
                              struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  ssize_t nread = 0;

  while (event_data->total_bytes_read < event_data->read_buffer->size) {
    // Remaining amount of bytes to read into the buffer.
    size_t remaining_bytes =
        event_data->read_buffer->size - event_data->total_bytes_read;
    // Pointer to the start of the "empty" read buffer.
    void *remaining_buffer =
        event_data->read_buffer->data + event_data->total_bytes_read;

    nread = read(event_data->fd, remaining_buffer, remaining_bytes);
    if (nread == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // The client is not ready to ready yet.
        return CLIENT_READ_INCOMPLETE;
      }

      // Some other error happened.
      perror("read_until_newline read");
      return CLIENT_READ_ERROR;
    } else if (nread == 0) {
      // Client disconnected gracefully.
      return CLIENT_READ_CLOSED;
    }

    // Find a newline in the bytes we just read.
    char *newline = memchr(remaining_buffer, '\n', nread);
    if (newline != NULL) {
      *newline = '\0';
      return CLIENT_READ_SUCCESS;
    }

    // We didn't a newline in the bytes we just read, update the total bytes
    // read and keep reading if possible.
    event_data->total_bytes_read += nread;
  }

  // We read all the bytes we are allowed for the buffer and didn't find the
  // newline, just return an error.
  return CLIENT_READ_ERROR;
}

void handle_text_client_request(struct WorkerArgs *args,
                                struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  worker_log(args, "Reading from fd %d (%s)", event_data->fd,
             connection_type_str(event_data->connection_type));

  if (event_data->client_state == READ_READY) {
    struct BoundedData *read_buffer =
        bounded_data_create(TEXT_REQUEST_BUFFER_SIZE);
    event_data->read_buffer = read_buffer;
    event_data->total_bytes_read = 0;
    event_data->client_state = READING;
  } else if (event_data->client_state == READING) {
    // do nothing, calling the read_until_newline function should pick up from
    // last time.
  }

  int rv = read_until_newline(args, event);
  switch (rv) {
  case CLIENT_READ_ERROR:
    worker_log(args, "Error reading from client, closing connection.");
    close_client(event_data);
    return;
  case CLIENT_READ_CLOSED:
    worker_log(args, "Client closed connection.");
    close_client(event_data);
    return;
  case CLIENT_READ_SUCCESS:
    worker_log(args, "Text protocol read success! <%s>",
               event_data->read_buffer->data);
    break;
  case CLIENT_READ_INCOMPLETE:
    worker_log(args, "Read incomplete, waiting for more data.");
    epoll_mod_client(args, event);
    return;
  default:
    worker_log(args, "Unknown value.");
    close_client(event_data);
    return;
  }

  worker_log(args, "Now we should parse the request and respond, but for now "
                   "we keep reading");

  // temporarily clean the read data so we can read again
  event_data->client_state = READ_READY;
  bounded_data_destroy(event_data->read_buffer);
  event_data->total_bytes_read = 0;
  // re-register him to read again on next stimulus
  epoll_mod_client(args, event);
}