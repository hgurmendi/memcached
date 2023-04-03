#include <stdio.h>

#include "binary_protocol.h"
#include "epoll.h"    // for struct EventData
#include "protocol.h" // for read_buffer

// Handles writing the response for a binary client in whatever write state it
// is and returns CLIENT_WRITE_ERROR, CLIENT_WRITE_SUCCESS or
// CLIENT_WRITE_INCOMPLETE.
int handle_binary_client_response(struct WorkerArgs *args,
                                  struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  int rv;

  // Make sure we are entering in a valid state.
  switch (event_data->client_state) {
  case BINARY_WRITING_COMMAND:
  case BINARY_WRITING_CONTENT_DATA:
  case BINARY_WRITING_CONTENT_SIZE:
    break;
  default:
    worker_log(args, "Invalid state in handle_binary_client_response: %s\n",
               client_state_str(event_data->client_state));
    return CLIENT_READ_ERROR;
    break;
  }

  // Start handling the response by going through all the states in order.

  if (event_data->client_state == BINARY_WRITING_COMMAND) {
    int rv = write_buffer(event_data->fd, &(event_data->response_type), 1,
                          &(event_data->total_bytes_written));
    if (rv != CLIENT_WRITE_SUCCESS) {
      return rv;
    }

    // Close the connection after sending BT_EUNK.
    if (event_data->response_type == BT_EUNK) {
      // TODO: maybe change the return value?
      return CLIENT_READ_CLOSED;
    }

    // Reset the total bytes written counter and determine the next state
    // depending on whether the response has additional content or not.

    // - If the command is STATS then we can handle it immediately and start
    // writing the response, so we transition to BINARY_WRITING_COMMAND.
    // - If the command is DEL, GET, TAKE or PUT then we need to parse at least
    // one more command, so we transition to BINARY_READING_ARG1_SIZE.
    // - In any other case, the received command is invalid and we have to write
    // an EINVALID response, so we transition to BINARY_WRITING_COMMAND.

    event_data->total_bytes_written = 0;
    if (event_data->response_content != NULL) {
      event_data->client_state = BINARY_WRITING_CONTENT_SIZE;
    } else {
      return CLIENT_WRITE_SUCCESS;
    }
  }

  if (event_data->client_state == BINARY_WRITING_CONTENT_SIZE) {
    uint32_t content_size = htonl(event_data->response_content->size);
    rv = write_buffer(event_data->fd, (char *)&content_size,
                      sizeof(content_size), &(event_data->total_bytes_written));
    if (rv != CLIENT_WRITE_SUCCESS) {
      return rv;
    }

    // Reset the total bytes written counter and transition uncondinitionally to
    // BINARY_WRITING_CONTENT_DATA because we have to write the contents of the
    // response.
    event_data->total_bytes_written = 0;
    event_data->client_state = BINARY_WRITING_CONTENT_DATA;
  }

  if (event_data->client_state == BINARY_WRITING_CONTENT_DATA) {
    rv = write_buffer(event_data->fd, event_data->response_content->data,
                      event_data->response_content->size,
                      &(event_data->total_bytes_written));

    // Unconditionally return the return value of write_buffer since at this
    // point we either successfully finished writing the contents of the
    // response or an error happened or we can't finish writing to the client.
    return rv;
  }

  worker_log(args, "Invalid state reached in handle_binary_client_response");
  return CLIENT_WRITE_ERROR;
}

// Handles reading the request from a binary client in whatever read state it
// is and returns CLIENT_READ_ERROR, CLIENT_READ_SUCCESS, CLIENT_READ_CLOSED or
// CLIENT_READ_INCOMPLETE.
int handle_binary_client_request(struct WorkerArgs *args,
                                 struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;
  int rv;

  // Make sure we are entering in a valid state.
  switch (event_data->client_state) {
  case BINARY_READY:
  case BINARY_READING_COMMAND:
  case BINARY_READING_ARG1_SIZE:
  case BINARY_READING_ARG1_DATA:
  case BINARY_READING_ARG2_SIZE:
  case BINARY_READING_ARG2_DATA:
    break;
  default:
    worker_log(args, "Invalid state in handle_binary_client_request: %s",
               client_state_str(event_data->client_state));
    return CLIENT_READ_ERROR;
    break;
  }

  // Start handling the request by going through all the states in order.

  if (event_data->client_state == BINARY_READY) {
    // Reset the total bytes read counter and transition unconditionally to
    // BINARY_READING_COMMAND to start reading the request.
    event_data->total_bytes_read = 0;
    event_data->client_state = BINARY_READING_COMMAND;
  }

  if (event_data->client_state == BINARY_READING_COMMAND) {
    rv = read_buffer(event_data->fd, &(event_data->command_type), 1,
                     &(event_data->total_bytes_read));
    if (rv != CLIENT_READ_SUCCESS) {
      return rv;
    }

    // Reset the total bytes read counter and determine the next state depending
    // on the command that was read:
    // - If the command is STATS then we can handle it immediately and start
    // writing the response, so we transition to BINARY_WRITING_COMMAND.
    // - If the command is DEL, GET, TAKE or PUT then we need to parse at least
    // one more command, so we transition to BINARY_READING_ARG1_SIZE.
    // - In any other case, the received command is invalid and we have to write
    // an EINVALID response, so we transition to BINARY_WRITING_COMMAND.

    event_data->total_bytes_read = 0;
    switch (event_data->command_type) {
    case BT_STATS:
      handle_stats(event_data, args);
      event_data->client_state = BINARY_WRITING_COMMAND;
      break;
    case BT_DEL:
    case BT_GET:
    case BT_TAKE:
    case BT_PUT:
      event_data->client_state = BINARY_READING_ARG1_SIZE;
      break;
    default:
      event_data->response_type = BT_EINVAL;
      event_data->client_state = BINARY_WRITING_COMMAND;
      break;
    }
  }

  if (event_data->client_state == BINARY_READING_ARG1_SIZE) {
    rv = read_buffer(event_data->fd, (char *)&(event_data->arg_size),
                     sizeof(event_data->arg_size),
                     &(event_data->total_bytes_read));
    if (rv != CLIENT_READ_SUCCESS) {
      return rv;
    }

    // Reset the total bytes read counter and prepare to read the contents of
    // the first argument based on the size that we just read. In order to do
    // that we have to allocate memory for the buffer where we'll read the
    // argument contents and then transition unconditionally to
    // BINARY_READING_ARG1_DATA.

    event_data->total_bytes_read = 0;
    // Convert the read size from network byte order to host byte order.
    event_data->arg_size = ntohl(event_data->arg_size);
    event_data->arg1 = hashtable_malloc_evict_bounded_data(
        args->hashtable, event_data->arg_size);
    if (event_data->arg1 == NULL) {
      // Respond with BT_EUNK if the request can't be properly fulfilled due to
      // lack of memory.
      event_data->response_type = BT_EUNK;
      event_data->client_state = BINARY_WRITING_COMMAND;
    } else {
      event_data->client_state = BINARY_READING_ARG1_DATA;
    }
  }

  if (event_data->client_state == BINARY_READING_ARG1_DATA) {
    rv = read_buffer(event_data->fd, event_data->arg1->data,
                     event_data->arg1->size, &(event_data->total_bytes_read));
    if (rv != CLIENT_READ_SUCCESS) {
      return rv;
    }

    // Reset the total bytes read counter and determine the next state depending
    // on the command that was originally read and the fact that we already read
    // an argument:
    // - If the command is DEL, GET or TAKE then we can handle it immediately
    // and start writing the response, so we transition to
    // BINARY_WRITING_COMMAND.
    // - If the command is PUT then we need to parse one more command, so we
    // transition to BINARY_READING_ARG2_SIZE.
    // - In any other case, we're in the presence of an invalid state, so we log
    // it just in case.

    event_data->total_bytes_read = 0;
    switch (event_data->command_type) {
    case BT_DEL:
      handle_del(event_data, args, event_data->arg1);
      event_data->client_state = BINARY_WRITING_COMMAND;
      break;
    case BT_GET:
      handle_get(event_data, args, event_data->arg1);
      event_data->client_state = BINARY_WRITING_COMMAND;
      break;
    case BT_TAKE:
      handle_take(event_data, args, event_data->arg1);
      event_data->client_state = BINARY_WRITING_COMMAND;
      break;
    case BT_PUT:
      event_data->client_state = BINARY_READING_ARG2_SIZE;
      break;
    default:
      worker_log(args, "Processing invalid command in state %s.",
                 client_state_str(event_data->client_state));
      return CLIENT_READ_ERROR;
    }
  }

  if (event_data->client_state == BINARY_READING_ARG2_SIZE) {
    rv = read_buffer(event_data->fd, (char *)&(event_data->arg_size),
                     sizeof(event_data->arg_size),
                     &(event_data->total_bytes_read));
    if (rv != CLIENT_READ_SUCCESS) {
      return rv;
    }

    // Reset the total bytes read counter and prepare to read the contents of
    // the second argument based on the size that we just read. In order to do
    // that we have to allocate memory for the buffer where we'll read the
    // argument contents and then transition unconditionally to
    // BINARY_READING_ARG2_DATA.

    event_data->total_bytes_read = 0;
    // Convert the read size from network byte order to host byte order.
    event_data->arg_size = ntohl(event_data->arg_size);
    event_data->arg2 = hashtable_malloc_evict_bounded_data(
        args->hashtable, event_data->arg_size);
    if (event_data->arg2 == NULL) {
      // Respond with BT_EUNK if the request can't be properly fulfilled due to
      // lack of memory.
      event_data->response_type = BT_EUNK;
      event_data->client_state = BINARY_WRITING_COMMAND;
    } else {
      event_data->client_state = BINARY_READING_ARG2_DATA;
    }
  }

  if (event_data->client_state == BINARY_READING_ARG2_DATA) {
    rv = read_buffer(event_data->fd, event_data->arg2->data,
                     event_data->arg2->size, &(event_data->total_bytes_read));
    if (rv != CLIENT_READ_SUCCESS) {
      return rv;
    }

    // If we're here we must be processing a PUT command, so we handle it
    // appropriately and start writing the response, so we transition to
    // BINARY_WRITING_COMMAND. Also both argument buffers will be owned by the
    // hash table now, so we have to set them to NULL in the client state so
    // they are not freed. If we're not processing a PUT command then we're in
    // the presence of a bad state, so we log it just in case.

    if (event_data->command_type == BT_PUT) {
      handle_put(event_data, args, event_data->arg1, event_data->arg2);
      // Both pointers will be owned by the hash table now.
      event_data->arg1 = NULL;
      event_data->arg2 = NULL;
      event_data->client_state = BINARY_WRITING_COMMAND;
    } else {
      worker_log(args, "Processing invalid command in state %s.",
                 client_state_str(event_data->client_state));
      return CLIENT_READ_ERROR;
    }
  }

  if (event_data->client_state == BINARY_WRITING_COMMAND) {
    // Reset the total bytes written and start handling the response.
    event_data->total_bytes_written = 0;
    return handle_binary_client_response(args, event);
  }

  worker_log(args, "Invalid state reached in handle_binary_client_request");
  return CLIENT_READ_ERROR;
}