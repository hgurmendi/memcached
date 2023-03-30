#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"
#include "text_protocol.h"
#include "worker_state.h"

#define CLIENT_READ_ERROR -1
#define CLIENT_READ_CLOSED 0
#define CLIENT_READ_SUCCESS 1
#define CLIENT_READ_INCOMPLETE 2

// Reads from the current client until a newline is found or until the client's
// read buffer is full. If a newline character is found then it's replaced by a
// null character and CLIENT_READ_SUCCESS is returned. If the client closes the
// connection then CLIENT_READ_CLOSED is returned. If the client's read buffer
// has some space in it and the client is not ready for reading then
// CLIENT_READ_INCOMPLETE is returned. If an error happens then
// CLIENT_READ_ERROR is returned.
static int read_until_newline(struct WorkerArgs *args,
                              struct EventData *event_data,
                              struct epoll_event *event) {
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
        // The file descriptor is not ready to keep reading. Since we're using
        // edge-triggered mode with one-shot we should add it again to the epoll
        // interest list via EPOLL_CTL_MOD.
        event->events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        int rv =
            epoll_ctl(args->epoll_fd, EPOLL_CTL_MOD, event_data->fd, event);
        if (rv == -1) {
          perror("read_until_newline epoll_ctl");
          return CLIENT_READ_ERROR;
        }

        // If adding it to the epoll interest list was successful, we have to
        // let the handler know that reading is not done yet.
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
                                struct EventData *event_data,
                                struct epoll_event *event) {
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

  int rv = read_until_newline(args, event_data, event);
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
    close_client(event_data);
    return;
  default:
    worker_log(args, "Unknown value.");
    close_client(event_data);
    return;
  }

  worker_log(args, "Now we should parse the request and respond, but for now "
                   "we keep reading");
  event->events = EPOLLIN | EPOLLET | EPOLLONESHOT;
  rv = epoll_ctl(args->epoll_fd, EPOLL_CTL_MOD, event_data->fd, event);
  if (rv == -1) {
    perror("handle_text_client_request epoll_ctl");
    return;
  }
}