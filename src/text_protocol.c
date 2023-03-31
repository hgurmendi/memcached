#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"
#include "protocol.h"
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

// Parses the well-formed text request from the client state and stores the
// response in the EventData struct inside the epoll event.
static void parse_text_request(struct WorkerArgs *args,
                               struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  char *buf_start = event_data->read_buffer->data;

  // Determine the token corresponding to the command.
  char *command_token = strsep(&buf_start, " ");

  // By the default the response should be BT_EINVAL and we only replace it when
  // appropriate.
  event_data->response_type = BT_EINVAL;

  if (!strcmp(command_token, binary_type_str(BT_STATS))) {
    worker_log(args, "Received STATS");
    event_data->response_type = BT_OK;
    // TODO implement the STATS command properly
    event_data->write_buffer = bounded_data_create_from_string_duplicate(
        "PUTS=111 DELS=99 GETS=381323 KEYS=132");
    return;
  }

  char *first_arg = strsep(&buf_start, " ");
  if (first_arg == NULL) {
    return;
  }

  if (!strcmp(command_token, binary_type_str(BT_DEL))) {
    worker_log(args, "Received DEL key=<%s>", first_arg);
    struct BoundedData *key = bounded_data_create_from_string(first_arg);
    int rv = bd_hashtable_remove(args->hashtable, key);
    if (rv == HT_FOUND) {
      event_data->response_type = BT_OK;
    } else {
      event_data->response_type = BT_ENOTFOUND;
    }
    key->data = NULL;
    bounded_data_destroy(key);
    return;
  }

  if (!strcmp(command_token, binary_type_str(BT_GET))) {
    worker_log(args, "Received GET key=<%s>", first_arg);
    struct BoundedData *key = bounded_data_create_from_string(first_arg);
    // TODO: after writing, destroy the buffer in `value`.
    struct BoundedData *value = NULL;
    int rv = bd_hashtable_get(args->hashtable, key, &value);
    if (rv == HT_FOUND) {
      event_data->response_type = BT_OK;
      event_data->write_buffer = value;
    } else {
      event_data->response_type = BT_ENOTFOUND;
    }
    key->data = NULL;
    bounded_data_destroy(key);
    return;
  }

  if (!strcmp(command_token, binary_type_str(BT_TAKE))) {
    worker_log(args, "Received TAKE key=<%s>", first_arg);
    struct BoundedData *key = bounded_data_create_from_string(first_arg);
    // TODO: after writing, destroy the buffer in `value`.
    struct BoundedData *value = NULL;
    int rv = bd_hashtable_take(args->hashtable, key, &value);
    if (rv == HT_FOUND) {
      event_data->response_type = BT_OK;
      event_data->write_buffer = value;
    } else {
      event_data->response_type = BT_ENOTFOUND;
    }
    key->data = NULL;
    bounded_data_destroy(key);
    return;
  }

  char *second_arg = strsep(&buf_start, " ");
  if (second_arg == NULL) {
    return;
  }

  if (!strcmp(command_token, binary_type_str(BT_PUT))) {
    worker_log(args, "Received PUT key=<%s> val=<%s>", first_arg, second_arg);
    // The BoundedData instances below are now "owned" by the hash table.
    struct BoundedData *key =
        bounded_data_create_from_string_duplicate(first_arg);
    struct BoundedData *value =
        bounded_data_create_from_string_duplicate(second_arg);
    bd_hashtable_insert(args->hashtable, key, value);
    event_data->response_type = BT_OK;
    return;
  }

  // Handle unknown command.
  worker_log(args, "Received unknown command");
}

#define CLIENT_WRITE_ERROR -1
#define CLIENT_WRITE_SUCCESS 1
#define CLIENT_WRITE_INCOMPLETE 2

// Writes the given buffer with the given size into the client socket's file
// descriptor, assuming that the amount of bytes in the value pointed at by
// `total_bytes_written` were already sent. If the whole buffer is correctly
// sent then CLIENT_WRITE_SUCCESS is returned and the value pointed at by
// `total_bytes_written` is updated to reflect this. If it's not possible to
// write the whole buffer, the value pointed at by `total_bytes_written` is
// updated to the new amount written and CLIENT_WRITE_INCOMPLETE is returned. If
// an error happens then CLIENT_WRITE_ERROR is returned.
static int write_buffer(int fd, char *buffer, size_t buffer_length,
                        size_t *total_bytes_written) {
  while (*total_bytes_written < buffer_length) {
    char *remaining_buffer = buffer + *total_bytes_written;
    size_t remaining_bytes = buffer_length - *total_bytes_written;
    ssize_t nwritten = write(fd, remaining_buffer, remaining_bytes);
    if (nwritten == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Client file descriptor is not ready to receive data, we should
        // continue later.
        return CLIENT_WRITE_INCOMPLETE;
      }

      // Another error happened.
      perror("write_buffer write");
      return CLIENT_WRITE_ERROR;
    }

    // TODO: remove this log after we test the resume behavior.
    if (nwritten != remaining_bytes) {
      printf("UNABLE TO WRITE ALL THE CONTENT, WE HAVE TO CONTINUE TRYING\n");
    }

    *total_bytes_written += nwritten;
  }

  return CLIENT_WRITE_SUCCESS;
}

static int handle_text_client_response(struct WorkerArgs *args,
                                       struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;

  char response_prefix[20];
  char *maybe_content_separator = event_data->write_buffer != NULL ? " " : "";
  int rv = snprintf(response_prefix, sizeof(response_prefix), "%s%s",
                    binary_type_str(event_data->response_type),
                    maybe_content_separator);
  if (rv < 0) {
    perror("handle_text_client_response snprintf");
    return CLIENT_WRITE_ERROR;
  }
  size_t response_prefix_size =
      strnlen(response_prefix, sizeof(response_prefix) - 1);

  size_t total_bytes_written = 0;
  rv = write_buffer(event_data->fd, response_prefix, response_prefix_size,
                    &total_bytes_written);
  if (rv != CLIENT_WRITE_SUCCESS) {
    // Return if write wasn't successful.
    return rv;
  }

  //////////////
  // Should update the state to know that the prefix was sent.
  //////////////

  // Only write the content if there is something to write.
  if (event_data->write_buffer != NULL) {
    total_bytes_written = 0;
    // TODO: Should remove the trailing '\0' from the write buffer, if present?
    rv = write_buffer(event_data->fd, event_data->write_buffer->data,
                      event_data->write_buffer->size, &total_bytes_written);
    if (rv != CLIENT_WRITE_SUCCESS) {
      // Return if write wasn't successful.
      return rv;
    }
  }

  //////////////
  // Should update the state to know that the prefix was sent.
  //////////////

  total_bytes_written = 0;
  // Always write the trailing \n.
  rv = write_buffer(event_data->fd, "\n", 1, &total_bytes_written);
  if (rv != CLIENT_WRITE_SUCCESS) {
    // Return if write wasn't successful.
    return rv;
  }

  //////////////
  // Should update the state to know that the prefix was sent.
  //////////////

  return CLIENT_WRITE_SUCCESS;
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
    event_data_close_client(event_data);
    return;
  case CLIENT_READ_CLOSED:
    worker_log(args, "Client closed connection.");
    event_data_close_client(event_data);
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
    event_data_close_client(event_data);
    return;
  }

  worker_log(args, "Now we should parse the request and respond, but for now "
                   "we keep reading");

  // Parse the text request.
  parse_text_request(args, event);

  worker_log(args, "Should respond '%s %s'",
             binary_type_str(event_data->response_type),
             event_data->write_buffer != NULL ? event_data->write_buffer->data
                                              : "<EMPTY>");

  rv = handle_text_client_response(args, event);
  if (rv == CLIENT_WRITE_INCOMPLETE) {
    // Re-register the client in epoll so we can continue writing later.
    worker_log(args, "FOUND A CLIENT WHOSE WRITE IS INCOMPLETE");
  } else if (rv == CLIENT_WRITE_ERROR) {
    event_data_close_client(event_data);
    return;
  }

  // reset the client state so we can read again.
  event_data_reset_client(event_data);
  // re-register him to read again on next stimulus.
  epoll_mod_client(args, event);
}