#include <stdio.h>

#include "binary_protocol.h"
#include "epoll.h"    // for struct EventData
#include "protocol.h" // for read_buffer

void handle_binary_client_response(struct WorkerArgs *args,
                                   struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  printf("Weee writing the response!\n");

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
    event_data->client_state = BINARY_READING_COMMAND;
    event_data->total_bytes_read = 0; // Just in case, shouldn't be needed.
  }

  if (event_data->client_state == BINARY_READING_COMMAND) {
    rv = read_buffer(event_data->fd, &(event_data->command_type), 1,
                     &(event_data->total_bytes_read));
    if (rv == CLIENT_READ_INCOMPLETE) {
      worker_log(args, "Read incomplete while in %s, waiting for more",
                 client_state_str(event_data->client_state));
      epoll_mod_client(args->epoll_fd, event, EPOLLIN);
      return;
    } else if (rv == CLIENT_READ_CLOSED) {
      worker_log(args, "Client closed the connection.");
      return;
    } else if (rv == CLIENT_READ_ERROR) {
      worker_log(args, "Error when reading from client.");
      return;
    }

    event_data->total_bytes_read = 0; // Reset the read counter.

    // Decide where to transition next.
    switch (event_data->command_type) {
    case BT_STATS:
      // should answer right away.
      handle_stats(event_data, args->hashtable);
      event_data->client_state = BINARY_WRITING_COMMAND;
      break;
    case BT_DEL:
    case BT_GET:
    case BT_TAKE:
    case BT_PUT:
      // should read at least one command and continue
      event_data->client_state = BINARY_READING_ARG1_SIZE;
      break;
    default:
      // should return einval immediately.
      event_data->response_type = BT_EINVAL;
      event_data->client_state = BINARY_WRITING_COMMAND;
      // jump to handle write.
    }
  }

  if (event_data->client_state == BINARY_READING_ARG1_SIZE) {
    rv = read_buffer(event_data->fd, (char *)&(event_data->arg_size),
                     sizeof(event_data->arg_size),
                     &(event_data->total_bytes_read));
    if (rv == CLIENT_READ_INCOMPLETE) {
      worker_log(args, "Read incomplete while in %s, waiting for more",
                 client_state_str(event_data->client_state));
      epoll_mod_client(args->epoll_fd, event, EPOLLIN);
      return;
    } else if (rv == CLIENT_READ_CLOSED) {
      worker_log(args, "Client closed the connection.");
      return;
    } else if (rv == CLIENT_READ_ERROR) {
      worker_log(args, "Error when reading from client.");
      return;
    }
    // On success, convert from network byte order to host byte order:
    event_data->arg_size = ntohl(event_data->arg_size);
    event_data->client_state = BINARY_READING_ARG1_DATA;
    event_data->total_bytes_read = 0;
  }

  if (event_data->client_state == BINARY_READING_ARG2_SIZE) {
    printf("NOT IMPLEMENTED YET!\n");
  }

  if (event_data->client_state == BINARY_READING_ARG2_DATA) {
    printf("NOT IMPLEMENTED YET!\n");
  }

  if (event_data->client_state == BINARY_WRITING_COMMAND) {
    printf("We should start writing!\n");
    handle_binary_client_response(args, event);
  }
}