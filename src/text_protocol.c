#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"
#include "protocol.h"
#include "text_protocol.h"
#include "worker_state.h"

#define COMMAND_BUFFER_SIZE 20

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
static int epoll_mod_client(int epoll_fd, struct epoll_event *event,
                            uint32_t event_flag) {
  struct EventData *event_data = event->data.ptr;
  event->events = event_flag | EPOLLET | EPOLLONESHOT;
  int rv = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, event_data->fd, event);
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

// Parses the well-formed text request from the client state and mutates the
// EventData struct inside the epoll event with the appropriate data for the
// response.
static void parse_text_request(struct WorkerArgs *args,
                               struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  char *token = event_data->read_buffer->data;

  char *command = strsep(&token, " "); // Should at least be an empty string.

  // By the default the response is BT_EINVAL.
  event_data->response_type = BT_EINVAL;

  if (!strcmp(command, binary_type_str(BT_STATS))) {
    handle_stats(event_data, args->hashtable);
    return;
  }

  char *first_arg = strsep(&token, " ");
  if (first_arg == NULL) {
    return;
  }

  if (!strcmp(command, binary_type_str(BT_DEL))) {
    struct BoundedData *key = bounded_data_create_from_string(first_arg);
    handle_del(event_data, args->hashtable, key);
    // The `key->data` pointer is not freeable through `free` because it points
    // to the middle of a pointer allocated with `malloc`. So we destroy the
    // `key` pointer now, but first we have to clear `key->data`, and the
    // original pointer will be cleared when the client state is reset.
    key->data = NULL;
    bounded_data_destroy(key);
    return;
  }

  if (!strcmp(command, binary_type_str(BT_GET))) {
    struct BoundedData *key = bounded_data_create_from_string(first_arg);
    handle_get(event_data, args->hashtable, key);
    // The `key->data` pointer is not freeable through `free` because it points
    // to the middle of a pointer allocated with `malloc`. So we destroy the
    // `key` pointer now, but first we have to clear `key->data`, and the
    // original pointer will be cleared when the client state is reset.
    key->data = NULL;
    bounded_data_destroy(key);
    return;
  }

  if (!strcmp(command, binary_type_str(BT_TAKE))) {
    struct BoundedData *key = bounded_data_create_from_string(first_arg);
    handle_take(event_data, args->hashtable, key);
    // The `key->data` pointer is not freeable through `free` because it points
    // to the middle of a pointer allocated with `malloc`. So we destroy the
    // `key` pointer now, but first we have to clear `key->data`, and the
    // original pointer will be cleared when the client state is reset.
    key->data = NULL;
    bounded_data_destroy(key);
    return;
  }

  char *second_arg = strsep(&token, " ");
  if (second_arg == NULL) {
    return;
  }

  if (!strcmp(command, binary_type_str(BT_PUT))) {
    // The BoundedData instances below will be "owned" by the hash table.
    struct BoundedData *key =
        bounded_data_create_from_string_duplicate(first_arg);
    struct BoundedData *value =
        bounded_data_create_from_string_duplicate(second_arg);
    handle_put(event_data, args->hashtable, key, value);
    return;
  }
}

// Writes or resumes the writing of a command of a text response. Returns
// CLIENT_WRITE_ERROR, CLIENT_WRITE_SUCCESS or CLIENT_WRITE_INCOMPLETE.
static int write_command(struct EventData *event_data) {
  char command_buf[COMMAND_BUFFER_SIZE];
  char *maybe_content_separator =
      event_data->response_content != NULL ? " " : "";
  int rv = snprintf(command_buf, COMMAND_BUFFER_SIZE, "%s%s",
                    binary_type_str(event_data->response_type),
                    maybe_content_separator);
  if (rv < 0) {
    perror("write_command snprintf");
    return CLIENT_WRITE_ERROR;
  }

  // Make sure we don't write the trailing '\0', hence the strnlen.
  size_t command_len = strnlen(command_buf, COMMAND_BUFFER_SIZE);
  return write_buffer(event_data->fd, command_buf, command_len,
                      &(event_data->total_bytes_written));
}

// Writes or resumes the writing of a response's content. Returns
// CLIENT_WRITE_ERROR, CLIENT_WRITE_SUCCESS or CLIENT_WRITE_INCOMPLETE.
static int write_content(struct EventData *event_data) {
  char *write_buf = event_data->response_content->data;
  // Make sure we don't write the trailing '\0', hence the strnlen.
  size_t write_len = strnlen(write_buf, event_data->response_content->size);
  return write_buffer(event_data->fd, write_buf, write_len,
                      &(event_data->total_bytes_written));
}

// Writes or resumes the writing of the ending newline character of a response.
// Returns CLIENT_WRITE_ERROR, CLIENT_WRITE_SUCCESS or CLIENT_WRITE_INCOMPLETE.
static int write_newline(struct EventData *event_data) {
  return write_buffer(event_data->fd, "\n", 1,
                      &(event_data->total_bytes_written));
}

static int handle_text_client_response(struct WorkerArgs *args,
                                       struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  int rv;

  if (event_data->client_state == TEXT_WRITING_COMMAND) {
    rv = write_command(event_data);
    if (rv != CLIENT_WRITE_SUCCESS) {
      // Return error code if write wasn't successful.
      return rv;
    }
    event_data->total_bytes_written = 0;
    if (event_data->response_content != NULL) {
      // Transition to writing the content if there is something.
      event_data->client_state = TEXT_WRITING_CONTENT;
    } else {
      // Otherwise transition to writing the trailing newline.
      event_data->client_state = TEXT_WRITING_NEWLINE;
    }
  }

  if (event_data->client_state == TEXT_WRITING_CONTENT) {
    rv = write_content(event_data);
    if (rv != CLIENT_WRITE_SUCCESS) {
      // Return error code if write wasn't successful.
      return rv;
    }
    event_data->total_bytes_written = 0;
    // Transition to writing the trailing newline.
    event_data->client_state = TEXT_WRITING_NEWLINE;
  }

  if (event_data->client_state == TEXT_WRITING_NEWLINE) {
    rv = write_newline(event_data);
    if (rv != CLIENT_WRITE_SUCCESS) {
      // Return error code if write wasn't successful.
      return rv;
    }
    // Return a successful write!
    return CLIENT_WRITE_SUCCESS;
  }

  worker_log(args, "Invalid state reached in handle_text_client_response");
  return CLIENT_WRITE_ERROR;
}

void handle_text_client_request(struct WorkerArgs *args,
                                struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;

  switch (event_data->client_state) {
  case TEXT_READY:
  case TEXT_READING_INPUT:
    break;
  default:
    worker_log(args, "Invalid state in handle_text_client_request\n");
    event_data_close_client(event_data);
    return;
  }

  // If we didn't start reading from the client yet, prepare everything and
  // begin.
  if (event_data->client_state == TEXT_READY) {
    struct BoundedData *read_buffer =
        bounded_data_create(TEXT_REQUEST_BUFFER_SIZE);
    event_data->read_buffer = read_buffer;
    event_data->total_bytes_read = 0;
    event_data->client_state = TEXT_READING_INPUT;
  }

  // Read until a newline is found.
  int rv = read_until_newline(args, event);
  switch (rv) {
  case CLIENT_READ_SUCCESS:
    break;
  case CLIENT_READ_INCOMPLETE:
    worker_log(args, "Read incomplete, waiting for more data.");
    epoll_mod_client(args->epoll_fd, event, EPOLLIN);
    return;
  case CLIENT_READ_CLOSED:
    worker_log(args, "Client closed the connection.");
  case CLIENT_READ_ERROR:
  default:
    event_data_close_client(event_data);
    return;
  }

  // Parse the text request.
  parse_text_request(args, event);

  // Transition to writing the command.
  event_data->client_state = TEXT_WRITING_COMMAND;

  rv = handle_text_client_response(args, event);
  if (rv == CLIENT_WRITE_INCOMPLETE) {
    // Re-register the client in epoll so we can continue writing later.
    worker_log(args, "FOUND A CLIENT WHOSE WRITE IS INCOMPLETE");
    epoll_mod_client(args->epoll_fd, event, EPOLLOUT);
    return;
  } else if (rv == CLIENT_WRITE_ERROR) {
    event_data_close_client(event_data);
    return;
  }

  // reset the client state so we can read again.
  event_data_reset(event_data);
  // re-register him to read again on next stimulus.
  epoll_mod_client(args->epoll_fd, event, EPOLLIN);
}