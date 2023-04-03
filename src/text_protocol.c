#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"
#include "protocol.h"
#include "text_protocol.h"
#include "utils.h"
#include "worker_state.h"

#define COMMAND_BUFFER_SIZE 20

// Maximum request size for the text protocol.
#define MAX_TEXT_REQUEST_SIZE 2048

// Buffer size for the text protocol.
#define TEXT_REQUEST_BUFFER_SIZE (MAX_TEXT_REQUEST_SIZE + 5)

// Reads from the current client until a newline is found or until the maximum
// request size (which should be smaller than the size of the given buffer). If
// a newline character is found then it's replaced by a null character and
// CLIENT_READ_SUCCESS is returned. If the client closes the connection then
// CLIENT_READ_CLOSED is returned. If the client's read buffer has some space in
// it and the client is not ready for reading then CLIENT_READ_INCOMPLETE is
// returned. If an error happens then CLIENT_READ_ERROR is returned.
static int read_until_newline(int incoming_fd, char *buffer, size_t buffer_size,
                              size_t *total_bytes_read, size_t read_limit) {
  ssize_t nread = 0;

  while (*total_bytes_read < buffer_size) {
    // Remaining amount of bytes to read into the buffer.
    size_t remaining_bytes = buffer_size - *total_bytes_read;
    // Pointer to the start of the "empty" read buffer.
    char *remaining_buffer = buffer + *total_bytes_read;

    nread = read(incoming_fd, remaining_buffer, remaining_bytes);
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

    // Try to find a newline in the bytes we just read and see if it's within
    // the read limit.
    char *newline = memchr(remaining_buffer, '\n', nread);
    if (newline != NULL && (newline - buffer) < read_limit) {
      // Replace the character after the newline with a null char so that we can
      // manipulate the string up until that point.
      *(newline + 1) = '\0';
      return CLIENT_READ_SUCCESS;
    }

    // We didn't a newline in the bytes we just read, update the total bytes
    // read and keep reading if possible.
    *total_bytes_read += nread;
  }

  // We read all the bytes we are allowed for the buffer but we didn't find a
  // newline. We still return success because no error was found.
  return CLIENT_READ_SUCCESS;
}

// Mutates the given EventData struct when the contents are not appropriate for
// the text protocol, otherwise leave it untouched.
static void enforce_text_protocol_limitations(struct EventData *event_data) {
  if (event_data->response_type != BT_OK) {
    return;
  }

  if (event_data->response_content == NULL) {
    return;
  }

  if (event_data->response_content->size + strlen("OK \n") >
      MAX_TEXT_REQUEST_SIZE) {
    event_data->response_type = BT_EBIG;
    event_data_clear_response_content(event_data);
    return;
  }

  if (!is_text_representable(event_data->response_content->data,
                             event_data->response_content->size)) {
    event_data->response_type = BT_EBINARY;
    event_data_clear_response_content(event_data);
    return;
  }
}

// If the given string contains a newline convert it to a null character and
// return true. Otherwise return false. Make sure the given string is safe to
// use strchr on.
static bool remove_newline_if_found(char *token) {
  char *newline = strchr(token, '\n');
  if (newline != NULL) {
    *newline = '\0';
    return true;
  }
  return false;
}

// Parses the well-formed text request from the client state and mutates the
// EventData struct inside the epoll event with the appropriate data for the
// response.
static void parse_text_request(struct WorkerArgs *args,
                               struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  char *token = event_data->read_buffer->data;
  int argument_count = 0;

  char *command = strsep(&token, " ");
  if (command != NULL && remove_newline_if_found(command)) {
    argument_count = 0;
  }
  char *first_arg = strsep(&token, " ");
  if (first_arg != NULL && remove_newline_if_found(first_arg)) {
    argument_count = 1;
  }
  char *second_arg = strsep(&token, " ");
  if (second_arg != NULL && remove_newline_if_found(second_arg)) {
    argument_count = 2;
  }

  // By the default the response is BT_EINVAL.
  event_data->response_type = BT_EINVAL;

  if (argument_count == 0 && !strcmp(command, binary_type_str(BT_STATS))) {
    handle_stats(event_data, args);
    return;
  }

  if (argument_count == 1 && !strcmp(command, binary_type_str(BT_DEL))) {
    size_t key_len = strlen(first_arg);
    if (key_len <= 0) {
      // Invalid DEL.
      return;
    }
    // Create an uninitialized BoundedData struct and wrap it around the
    // `first_arg` buffer.
    struct BoundedData *key =
        hashtable_malloc_evict_bounded_data(args->hashtable, 0);
    if (key == NULL) {
      // Respond with BT_EUNK if the request can't be properly fulfilled due to
      // lack of memory.
      event_data->response_type = BT_EUNK;
      return;
    }
    key->size = key_len;
    key->data = first_arg;

    handle_del(event_data, args, key);
    // The `key->data` pointer is not freeable through `free` because it points
    // to the middle of a pointer allocated with `malloc`. So we destroy the
    // `key` pointer now, but first we have to clear `key->data`, and the
    // original pointer will be cleared when the client state is reset.
    key->data = NULL;
    bounded_data_destroy(key);
    return;
  }

  if (argument_count == 1 && !strcmp(command, binary_type_str(BT_GET))) {
    size_t key_len = strlen(first_arg);
    if (key_len <= 0) {
      // Invalid GET.
      return;
    }
    // Create an uninitialized BoundedData struct and wrap it around the
    // `first_arg` buffer.
    struct BoundedData *key =
        hashtable_malloc_evict_bounded_data(args->hashtable, 0);
    if (key == NULL) {
      // Respond with BT_EUNK if the request can't be properly fulfilled due to
      // lack of memory.
      event_data->response_type = BT_EUNK;
      return;
    }
    key->size = key_len;
    key->data = first_arg;

    handle_get(event_data, args, key);
    // The `key->data` pointer is not freeable through `free` because it points
    // to the middle of a pointer allocated with `malloc`. So we destroy the
    // `key` pointer now, but first we have to clear `key->data`, and the
    // original pointer will be cleared when the client state is reset.
    key->data = NULL;
    bounded_data_destroy(key);
    enforce_text_protocol_limitations(event_data);
    return;
  }

  if (argument_count == 1 && !strcmp(command, binary_type_str(BT_TAKE))) {
    size_t key_len = strlen(first_arg);
    if (key_len <= 0) {
      // Invalid TAKE.
      return;
    }
    // Create an uninitialized BoundedData struct and wrap it around the
    // `first_arg` buffer.
    struct BoundedData *key =
        hashtable_malloc_evict_bounded_data(args->hashtable, 0);
    if (key == NULL) {
      // Respond with BT_EUNK if the request can't be properly fulfilled due to
      // lack of memory.
      event_data->response_type = BT_EUNK;
      return;
    }
    key->size = key_len;
    key->data = first_arg;

    handle_take(event_data, args, key);
    // The `key->data` pointer is not freeable through `free` because it points
    // to the middle of a pointer allocated with `malloc`. So we destroy the
    // `key` pointer now, but first we have to clear `key->data`, and the
    // original pointer will be cleared when the client state is reset.
    key->data = NULL;
    bounded_data_destroy(key);
    enforce_text_protocol_limitations(event_data);
    return;
  }

  if (argument_count == 2 && !strcmp(command, binary_type_str(BT_PUT))) {
    size_t key_len = strlen(first_arg);
    size_t value_len = strlen(second_arg);
    if (key_len <= 0 || value_len <= 0) {
      // Invalid insert.
      return;
    }
    // The BoundedData instances below will be "owned" by the hash table.
    struct BoundedData *key =
        hashtable_malloc_evict_bounded_data(args->hashtable, key_len);
    if (key == NULL) {
      // Respond with BT_EUNK if the request can't be properly fulfilled due to
      // lack of memory.
      event_data->response_type = BT_EUNK;
      return;
    }
    struct BoundedData *value =
        hashtable_malloc_evict_bounded_data(args->hashtable, value_len);
    if (value == NULL) {
      // Respond with BT_EUNK if the request can't be properly fulfilled due to
      // lack of memory.
      bounded_data_destroy(key);
      event_data->response_type = BT_EUNK;
      return;
    }
    memcpy(key->data, first_arg, key_len);
    memcpy(value->data, second_arg, value_len);

    handle_put(event_data, args, key, value);
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
  return write_buffer(event_data->fd, event_data->response_content->data,
                      event_data->response_content->size,
                      &(event_data->total_bytes_written));
}

// Writes or resumes the writing of the ending newline character of a response.
// Returns CLIENT_WRITE_ERROR, CLIENT_WRITE_SUCCESS or CLIENT_WRITE_INCOMPLETE.
static int write_newline(struct EventData *event_data) {
  return write_buffer(event_data->fd, "\n", 1,
                      &(event_data->total_bytes_written));
}

int handle_text_client_response(struct WorkerArgs *args,
                                struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  int rv;

  if (event_data->client_state == TEXT_WRITING_COMMAND) {
    rv = write_command(event_data);
    if (rv != CLIENT_WRITE_SUCCESS) {
      return rv;
    }

    // Close the connection after sending BT_EUNK.
    if (event_data->response_type == BT_EUNK) {
      // TODO: maybe change the return value?
      return CLIENT_READ_CLOSED;
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
      return rv;
    }
    event_data->total_bytes_written = 0;
    event_data->client_state = TEXT_WRITING_NEWLINE;
  }

  if (event_data->client_state == TEXT_WRITING_NEWLINE) {
    return write_newline(event_data);
  }

  worker_log(args, "Invalid state reached in handle_text_client_response");
  return CLIENT_WRITE_ERROR;
}

int handle_text_client_request(struct WorkerArgs *args,
                               struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;

  switch (event_data->client_state) {
  case TEXT_READY:
  case TEXT_READING_INPUT:
    break;
  default:
    worker_log(args, "Invalid state in handle_text_client_request: %s",
               client_state_str(event_data->client_state));
    return CLIENT_READ_ERROR;
    break;
  }

  // If we didn't start reading from the client yet, prepare everything and
  // begin.
  if (event_data->client_state == TEXT_READY) {
    event_data->read_buffer = hashtable_malloc_evict_bounded_data(
        args->hashtable, TEXT_REQUEST_BUFFER_SIZE);
    if (event_data->read_buffer == NULL) {
      // Respond with BT_EUNK if the request can't be properly fulfilled due to
      // lack of memory.
      event_data->response_type = BT_EUNK;
      event_data->client_state = TEXT_WRITING_COMMAND;
    } else {
      event_data->total_bytes_read = 0;
      event_data->client_state = TEXT_READING_INPUT;
    }
  }

  // TODO: add a guard here that checks the client state of TEXT_READING_INPUT
  // and in that case runs read_until_newline. We also should respond EINVAL
  // when the request is too long

  if (event_data->client_state == TEXT_READING_INPUT) {
    // Read from the client until a newline is found within the request size
    // limit and place a null character after it, or read until the buffer is
    // full but without a newline. Otherwise, return an error.
    int rv = read_until_newline(event_data->fd, event_data->read_buffer->data,
                                event_data->read_buffer->size,
                                &(event_data->total_bytes_read),
                                MAX_TEXT_REQUEST_SIZE);
    if (rv != CLIENT_READ_SUCCESS) {
      return rv;
    }

    // Parse the text request and mutate the struct EventData according to the
    // contents and whether it adheres to the protocol or not.
    parse_text_request(args, event);

    // Transition to writing the command.
    event_data->total_bytes_written = 0;
    event_data->client_state = TEXT_WRITING_COMMAND;
    return handle_text_client_response(args, event);
  }

  worker_log(args, "Invalid state reached in handle_text_client_request");
  return CLIENT_READ_ERROR;
}