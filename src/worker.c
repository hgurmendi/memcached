#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"
#include "parsers.h"
#include "worker.h"

/* Closes the connection to the client.
 */
static void close_client(struct ClientEpollEventData *event_data) {
  printf("Closing client %s (%d)\n", event_data->host, event_data->fd);
  // Closing the descriptor will make epoll automatically remove it from the
  // set of file descriptors which are currently monitored.
  close(event_data->fd);
  // We also have to free the memory allocated for the event data.
  free(event_data);
}

/* Responds to a client interacting through the binary protocol. The response is
 * encoded in a Command structure, where the command's type is the first token
 * of the text response and any (optional) arguments are stored in the `arg1`
 * member. Returns -1 if there was an error or 0 if it's all right.
 *
 */
static int respond_binary_to_client(struct ClientEpollEventData *event_data,
                                    struct Command *response_command) {
  char response_code;

  // @TODO: error handling of the sends?

  switch (response_command->type) {
  case BT_EINVAL:
    response_code = BT_EINVAL;
    send(event_data->fd, &response_code, 1, 0);
    break;
  case BT_OK:
    // Write the first argument of the command as the "argument" of the OK
    // response.
    response_code = BT_OK;
    send(event_data->fd, &response_code, 1, 0);
    if (response_command->arg1 != NULL) {
      uint32_t marshalled_size = htonl(response_command->arg1_size);
      send(event_data->fd, &marshalled_size, sizeof(uint32_t), 0);
      send(event_data->fd, response_command->arg1, response_command->arg1_size,
           0);
    }
    break;
  case BT_ENOTFOUND:
    response_code = BT_ENOTFOUND;
    send(event_data->fd, &response_code, 1, 0);
    break;
  default:
    printf("Encountered unknown command type whem sending data to client...\n");
    return -1;
    break;
  }

  return 0;
}

/* Responds to a client interacting through the text protocol. The response is
 * encoded in a Command structure, where the command's type is the first token
 * of the text response and any (optional) arguments are stored in the `arg1`
 * member. Returns -1 if there was an error or 0 if it's all right.
 */
static int respond_text_to_client(struct ClientEpollEventData *event_data,
                                  struct Command *response_command) {
  int status;
  char write_buf[MAX_REQUEST_SIZE];
  int bytes_written = 0;

  // @TODO: Here we might have to return EBINARY when the argument is binary
  // (might have to encode that in the structure, or as an extra argument).

  switch (response_command->type) {
  case BT_EINVAL:
    bytes_written = snprintf(write_buf, MAX_REQUEST_SIZE, "EINVAL\n");
    break;
  case BT_OK:
    // Write the first argument of the command as the "argument" of the OK
    // response.
    if (response_command->arg1 != NULL) {
      bytes_written = snprintf(write_buf, MAX_REQUEST_SIZE, "OK %s\n",
                               response_command->arg1);
    } else {
      bytes_written = snprintf(write_buf, MAX_REQUEST_SIZE, "OK\n");
    }
    break;
  case BT_ENOTFOUND:
    bytes_written = snprintf(write_buf, MAX_REQUEST_SIZE, "ENOTFOUND\n");
    break;
  default:
    printf("Encountered unknown command type whem sending data to client...\n");
    return -1;
    break;
  }

  status = send(event_data->fd, write_buf, bytes_written, 0);
  if (status < 0) {
    perror("Error sending data to text client");
    return -1;
  }

  return 0;
}

/* Handles incoming data from a client connection. If the client closes the
 * connection, we close the file descriptor and epoll manages it accordingly.
 */
static void handle_client(struct ClientEpollEventData *event_data) {
  struct Command received_command;
  struct Command response_command;

  initialize_command(&received_command);
  initialize_command(&response_command);

  if (event_data->connection_type == TEXT) {
    printf("Handling text data\n");
    parse_text(event_data->fd, &received_command);
  } else {
    printf("Handling binary data\n");
    parse_binary(event_data->fd, &received_command);
  }

  printf("Received command:\n");
  print_command(&received_command);

  switch (received_command.type) {
  case BT_PUT:
    // Add the key-value pair to the cache and return OK.
    response_command.type = BT_OK;
    break;
  case BT_DEL:
    // Remove the key-value pair corresponding to the key if it exists and
    // return OK, otherwise return ENOTFOUND.
    response_command.type = BT_ENOTFOUND;
    break;
  case BT_GET:
    // Return the value corresponding to they key if it exists and return OK
    // along with the value, otherwise return ENOTFOUND.
    // @TODO: Here we might want to also signal that the value was stored using
    // the binary protocol because in that case we have to actually send EBINARY
    // in the text protocol.
    response_command.type = BT_OK;
    response_command.arg1 = strdup("This_is_the_returned_value");
    response_command.arg1_size = sizeof("This_is_the_returned_value");
    break;
  case BT_TAKE:
    // Remove the key-value pair corresponding to the key if it exists and
    // return OK along with the value, otherwise return ENOTFOUND.
    // @TODO: Here we might want to also signal that the value was stored using
    // the binary protocol because in that case we have to actually send EBINARY
    // in the text protocol.
    response_command.type = BT_OK;
    response_command.arg1 = strdup("This_is_the_returned_value");
    response_command.arg1_size = sizeof("This_is_the_returned_value");
    break;
  case BT_STATS:
    // Return OK along with various statistics about the usage of the cache,
    // namely: number of PUTs, number of DELs, number of GETs, number of TAKEs,
    // number of STATSs, number of KEYs (i.e. key-value pairs) stored.
    response_command.type = BT_OK;
    response_command.arg1 =
        strdup("PUTS=111 DELS=99 GETS=381323 TAKES=1234 STATS=123 KEYS=132");
    response_command.arg1_size =
        sizeof("PUTS=111 DELS=99 GETS=381323 TAKES=1234 STATS=123 KEYS=132");
    break;
  case BT_EINVAL:
    // Error parsing the request, just return EINVAL.
    response_command.type = BT_EINVAL;
    break;
  default:
    printf("Encountered unknown command type when analyzing the client's "
           "command.\n");
    response_command.type = BT_EINVAL;
    break;
  }

  if (event_data->connection_type == TEXT) {
    printf("Responding text data\n");
    respond_text_to_client(event_data, &response_command);
  } else {
    printf("Responding binary data\n");
    respond_binary_to_client(event_data, &response_command);
  }

  destroy_command_args(&response_command);
  // TODO: we might not want to free the memory of the received command since
  // we'll use the already allocated memory for storing the key and/or values.
  destroy_command_args(&received_command);
}

static void *worker_func(void *worker_args) {
  struct WorkerArgs *args;

  args = (struct WorkerArgs *)worker_args;

  printf("Starting worker \"%s\" (epoll %d)\n", args->name, args->epoll_fd);

  // Allocate enough zeroed memory for all the simultaneous events that we'll
  // be listening to.
  struct epoll_event *events;
  events = calloc(MAX_EVENTS, sizeof(struct epoll_event));
  if (events == NULL) {
    perror("events calloc");
    abort();
  }

  // Continuously poll for events that should be handled by the worker.
  while (true) {
    int num_events;

    num_events = epoll_wait(args->epoll_fd, events, MAX_EVENTS, -1);
    // @TODO? error check `epoll_wait`?
    for (int i = 0; i < num_events; i++) {
      // Interpret the event data.
      struct ClientEpollEventData *event_data = get_event_data(events[i]);

      if (is_epoll_error(events[i])) {
        fprintf(stderr, "%s: epoll error\n", args->name);
        close_client(event_data);
      } else {
        // Handle the data incoming from the client.
        printf("[%s](%d): handling data from %s:%s (%s)\n", args->name,
               args->epoll_fd, event_data->host, event_data->port,
               event_data->connection_type == TEXT ? "Text connection"
                                                   : "Binary connection");

        handle_client(event_data);
      }
    }
  }

  free(events);
  free(worker_args);

  return 0;
}

void initialize_workers(int num_workers, pthread_t *worker_threads,
                        int *worker_epoll_fds) {
  for (int i = 0; i < num_workers; i++) {
    printf("Creating worker %d...\n", i);

    // Create an epoll instance for the worker.
    worker_epoll_fds[i] = epoll_create1(0);
    if (worker_epoll_fds[i] == -1) {
      perror("epoll_create1");
      abort();
    }

    // Prepare the arguments for each worker...
    // free responsability is of the caller
    struct WorkerArgs *worker_args = calloc(1, sizeof(struct WorkerArgs));
    if (worker_args == NULL) {
      perror("calloc worker_args");
      abort();
    }
    snprintf(worker_args->name, sizeof(worker_args->name), "Worker %d", i);
    worker_args->epoll_fd = worker_epoll_fds[i];

    int ret = pthread_create(&worker_threads[i], NULL, worker_func,
                             (void *)worker_args);
    if (ret != 0) {
      perror("pthread_create");
      abort();
    }
  }
}