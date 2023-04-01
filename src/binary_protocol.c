#include <stdio.h>

#include "binary_protocol.h"
#include "epoll.h"    // for struct EventData
#include "protocol.h" // for read_buffer

void handle_binary_client_response(struct WorkerArgs *args,
                                   struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;

  switch (event_data->client_state) {
  case BINARY_WRITING_COMMAND:
  case BINARY_WRITING_CONTENT_DATA:
  case BINARY_WRITING_CONTENT_SIZE:
    break;
  default:
    worker_log(args, "Invalid state in handle_binary_client_response: %s\n",
               client_state_str(event_data->client_state));
    event_data_close_client(event_data);
    return;
  }

  printf("We should return command <%s>\n",
         binary_type_str(event_data->response_type));

  if (event_data->response_content != NULL) {
    struct BoundedData *safe_to_print_content =
        bounded_data_create_from_buffer_duplicate(
            event_data->response_content->data,
            event_data->response_content->size + 1);
    safe_to_print_content->data[safe_to_print_content->size - 1] = '\0';
    printf("Also we should return content <%s>\n", safe_to_print_content->data);
    bounded_data_destroy(safe_to_print_content);
  }

  printf("DEBUG 1\n");

  int rv;
  size_t total_bytes_written;

  if (event_data->client_state == BINARY_WRITING_COMMAND) {
    printf("DEBUG 2\n");
    total_bytes_written = 0;
    int rv = write_buffer(event_data->fd, &(event_data->response_type), 1,
                          &total_bytes_written);
    if (rv != CLIENT_WRITE_SUCCESS) {
      printf("Error writing command\n");
      return;
    }
    if (event_data->response_content != NULL) {
      event_data->client_state = BINARY_WRITING_CONTENT_SIZE;
    }
    printf("DEBUG 3\n");
  }

  if (event_data->client_state == BINARY_WRITING_CONTENT_SIZE) {
    printf("DEBUG 4\n");
    uint32_t content_size = htonl(event_data->response_content->size);
    total_bytes_written = 0;
    rv = write_buffer(event_data->fd, (char *)&content_size,
                      sizeof(content_size), &total_bytes_written);
    if (rv != CLIENT_WRITE_SUCCESS) {
      printf("Error writing content size\n");
      return;
    }
    event_data->client_state = BINARY_WRITING_CONTENT_DATA;
    printf("DEBUG 5\n");
  }

  if (event_data->client_state == BINARY_WRITING_CONTENT_DATA) {
    printf("DEBUG 6\n");
    total_bytes_written = 0;
    rv = write_buffer(event_data->fd, event_data->response_content->data,
                      event_data->response_content->size, &total_bytes_written);
    if (rv != CLIENT_WRITE_SUCCESS) {
      printf("Error writing content size\n");
      return;
    }

    printf("Done, should clean up\n");
  }

  printf("DEBUG 7\n");
}

// Handles an unsuccessful read from a binary client.
static void handle_unsuccessful_read(int rv, struct WorkerArgs *args,
                                     struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;

  if (rv == CLIENT_READ_INCOMPLETE) {
    worker_log(args, "Read incomplete while in state <%s>, waiting for more",
               client_state_str(event_data->client_state));
    epoll_mod_client(args->epoll_fd, event, EPOLLIN);
    return;
  } else if (rv == CLIENT_READ_CLOSED) {
    worker_log(args, "Client closed the connection.");
    event_data_close_client(event_data);
    return;
  } else if (rv == CLIENT_READ_ERROR) {
    worker_log(args, "Error when reading from client.");
    event_data_close_client(event_data);
    return;
  }
}

// Handles the BINARY_READY client state.
static void handle_binary_ready(struct EventData *event_data) {
  // Transition to BINARY_READING_COMMAND and reset the read bytes counter.
  event_data->client_state = BINARY_READING_COMMAND;
  event_data->total_bytes_read = 0;
}

// Handles the BINARY_READING_COMMAND client state. Returns CLIENT_READ_ERROR,
// CLIENT_READ_CLOSED, CLIENT_READ_SUCCESS or CLIENT_READ_INCOMPLETE.
static int handle_binary_reading_command(struct EventData *event_data,
                                         struct HashTable *hashtable) {
  int rv = read_buffer(event_data->fd, &(event_data->command_type), 1,
                       &(event_data->total_bytes_read));
  if (rv != CLIENT_READ_SUCCESS) {
    return rv;
  }

  // Decide where to transition next and reset the read bytes counter.
  event_data->total_bytes_read = 0;
  switch (event_data->command_type) {
  case BT_STATS:
    // We can already handle the STATS command.
    handle_stats(event_data, hashtable);
    event_data->client_state = BINARY_WRITING_COMMAND;
    break;
  case BT_DEL:
  case BT_GET:
  case BT_TAKE:
  case BT_PUT:
    // We can't yet handle any of the other commands because we need at least an
    // argument.
    event_data->client_state = BINARY_READING_ARG1_SIZE;
    break;
  default:
    // The command we received is invalid, so we return an appropriate command.
    event_data->response_type = BT_EINVAL;
    event_data->client_state = BINARY_WRITING_COMMAND;
  }

  return rv;
}

// Handles the BINARY_READING_ARG1_SIZE client state. Returns CLIENT_READ_ERROR,
// CLIENT_READ_CLOSED, CLIENT_READ_SUCCESS or CLIENT_READ_INCOMPLETE.
static int handle_binary_reading_arg1_size(struct EventData *event_data) {
  int rv = read_buffer(event_data->fd, (char *)&(event_data->arg_size),
                       sizeof(event_data->arg_size),
                       &(event_data->total_bytes_read));
  if (rv != CLIENT_READ_SUCCESS) {
    return rv;
  }

  // Convert the read size from network byte order to host byte order.
  event_data->arg_size = ntohl(event_data->arg_size);
  // Then allocate memory for reading the argument and transition accordingly,
  // resetting the bytes read counter.
  event_data->arg1 = bounded_data_create(event_data->arg_size);
  event_data->arg_size = 0; // Clear it just in case.
  event_data->client_state = BINARY_READING_ARG1_DATA;
  event_data->total_bytes_read = 0;

  return rv;
}

// Handles the BINARY_READING_ARG1_DATA client state. Returns CLIENT_READ_ERROR,
// CLIENT_READ_CLOSED, CLIENT_READ_SUCCESS or CLIENT_READ_INCOMPLETE.
static int handle_binary_reading_arg1_data(struct EventData *event_data,
                                           struct HashTable *hashtable) {
  int rv = read_buffer(event_data->fd, event_data->arg1->data,
                       event_data->arg1->size, &(event_data->total_bytes_read));
  if (rv != CLIENT_READ_SUCCESS) {
    return rv;
  }

  // Decide where to transition next and reset the read bytes counter.
  event_data->total_bytes_read = 0;
  switch (event_data->command_type) {
  case BT_DEL:
    // We can handle the DEL command.
    handle_del(event_data, hashtable, event_data->arg1);
    event_data->client_state = BINARY_WRITING_COMMAND;
    break;
  case BT_GET:
    // We can handle the GET command.
    handle_get(event_data, hashtable, event_data->arg1);
    event_data->client_state = BINARY_WRITING_COMMAND;
    break;
  case BT_TAKE:
    // We can handle the TAKE command.
    handle_take(event_data, hashtable, event_data->arg1);
    event_data->client_state = BINARY_WRITING_COMMAND;
    break;
  case BT_PUT:
    // We can't yet handle the PUT command because we need another argument.
    event_data->client_state = BINARY_READING_ARG2_SIZE;
    break;
  default:
    // The command we received is invalid, so we return an appropriate command.
    event_data->response_type = BT_EINVAL;
    event_data->client_state = BINARY_WRITING_COMMAND;
  }

  return rv;
}

void handle_binary_client_request(struct WorkerArgs *args,
                                  struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  int rv;

  switch (event_data->client_state) {
  case BINARY_READY:
  case BINARY_READING_COMMAND:
  case BINARY_READING_ARG1_SIZE:
  case BINARY_READING_ARG1_DATA:
  case BINARY_READING_ARG2_SIZE:
  case BINARY_READING_ARG2_DATA:
    break;
  default:
    worker_log(args, "Invalid state in handle_binary_client_request: %s\n",
               client_state_str(event_data->client_state));
    event_data_close_client(event_data);
  }

  if (event_data->client_state == BINARY_READY) {
    handle_binary_ready(event_data);
  }

  if (event_data->client_state == BINARY_READING_COMMAND) {
    rv = handle_binary_reading_command(event_data, args->hashtable);
    if (rv != CLIENT_READ_SUCCESS) {
      handle_unsuccessful_read(rv, args, event);
      return;
    }
  }

  if (event_data->client_state == BINARY_READING_ARG1_SIZE) {
    rv = handle_binary_reading_arg1_size(event_data);
    if (rv != CLIENT_READ_SUCCESS) {
      handle_unsuccessful_read(rv, args, event);
      return;
    }
  }

  if (event_data->client_state == BINARY_READING_ARG1_DATA) {
    rv = handle_binary_reading_arg1_data(event_data, args->hashtable);
    if (rv != CLIENT_READ_SUCCESS) {
      handle_unsuccessful_read(rv, args, event);
      return;
    }
  }

  if (event_data->client_state == BINARY_READING_ARG2_SIZE) {
    // TODO
    printf("Implement reading ARG2 size\n");
  }

  if (event_data->client_state == BINARY_READING_ARG2_DATA) {
    // TODO
    printf("Implement reading ARG2 data\n");
  }

  if (event_data->client_state == BINARY_WRITING_COMMAND) {
    printf("A response is ready to be sent!\n");
    handle_binary_client_response(args, event);
    return;
  }

  printf("WTFFFFFF\n");
}