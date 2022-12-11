#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "binary_parser.h"
#include "command.h"
#include "epoll.h"
#include "hashtable/hashtable.h"
#include "text_parser.h"
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
  int status;

  // These are the only valid options for the binary client.
  if (response_command->type != BT_EINVAL || response_command->type != BT_OK ||
      response_command->type != BT_ENOTFOUND) {
    return -1;
  }

  status = send(event_data->fd, &response_code, 1, 0);
  if (status < 0) {
    perror("Error sending data to binary client");
    return -1;
  }

  if (response_command->arg1 == NULL) {
    return 0;
  }

  uint32_t marshalled_size = htonl(response_command->arg1_size);
  status = send(event_data->fd, &marshalled_size, sizeof(uint32_t), 0);
  if (status < 0) {
    perror("Error sending data to binary client");
    return -1;
  }

  status = send(event_data->fd, response_command->arg1,
                response_command->arg1_size, 0);
  if (status < 0) {
    perror("Error sending data to binary client");
    return -1;
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

  switch (response_command->type) {
  case BT_EBINARY:
    bytes_written = snprintf(write_buf, MAX_REQUEST_SIZE, "EBINARY\n");
    break;
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

static char *get_hashtable_ret_string(int ret) {
  switch (ret) {
  case HT_FOUND:
    return "FOUND";
  case HT_NOTFOUND:
    return "NOTFOUND";
  case HT_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN RETURN VALUE";
  }
}

/* Handles incoming data from a client connection. If the client closes the
 * connection, we close the file descriptor and epoll manages it accordingly.
 */
static void handle_client(struct ClientEpollEventData *event_data,
                          struct HashTable *hashtable) {
  struct Command received_command;
  struct Command response_command;
  int ret;

  command_initialize(&received_command);
  command_initialize(&response_command);

  if (event_data->connection_type == TEXT) {
    parse_text(event_data->fd, &received_command);
  } else {
    parse_binary(event_data->fd, &received_command);
  }

  command_print(&received_command);

  switch (received_command.type) {
  case BT_PUT:
    // Add the key-value pair to the cache and return OK.
    ret = hashtable_insert(hashtable, received_command.arg1_size,
                           received_command.arg1, received_command.arg2_size,
                           received_command.arg2);
    printf("PUT hash table result: %d (%s)\n", ret,
           get_hashtable_ret_string(ret));

    // Clean the memory from the received command. The key and value pointers
    // are now managed by the hash table.
    received_command.arg1_size = received_command.arg2_size = 0;
    received_command.arg1 = received_command.arg2 = NULL;

    response_command.type = BT_OK;
    break;
  case BT_DEL:
    // Remove the key-value pair corresponding to the key if it exists and
    // return OK, otherwise return ENOTFOUND.
    ret = hashtable_remove(hashtable, received_command.arg1_size,
                           received_command.arg1);
    printf("DEL hash table result: %d (%s)\n", ret,
           get_hashtable_ret_string(ret));

    if (ret == HT_NOTFOUND) {
      response_command.type = BT_ENOTFOUND;
    } else {
      response_command.type = BT_OK;
    }
    break;
  case BT_GET:
    // Return the value corresponding to they key if it exists and return OK
    // along with the value, otherwise return ENOTFOUND.
    // @TODO: Here we might want to also signal that the value was stored using
    // the binary protocol because in that case we have to actually send EBINARY
    // in the text protocol.
    ret = hashtable_get(hashtable, received_command.arg1_size,
                        received_command.arg1, &response_command.arg1_size,
                        &response_command.arg1);
    printf("GET hash table result: %d (%s)\n", ret,
           get_hashtable_ret_string(ret));

    if (ret == HT_NOTFOUND) {
      response_command.type = BT_ENOTFOUND;
      assert(response_command.arg1_size == 0);
      assert(response_command.arg1 == NULL);
    } else if (ret == HT_ERROR) {
      response_command.type = BT_EBIG;
      assert(response_command.arg1_size == 0);
      assert(response_command.arg1 == NULL);
    } else {
      // Check if the value is valid for a text client.
      if (event_data->connection_type == TEXT &&
          !is_text_representable(response_command.arg1_size,
                                 response_command.arg1)) {
        response_command.type = BT_EBINARY;
      } else {
        response_command.type = BT_OK;
      }
      // arg1_size and arg1 already contain the value size and value for the
      // corresponding key. It should be freed after being sent to the client.
    }
    break;
  case BT_TAKE:
    // Remove the key-value pair corresponding to the key if it exists and
    // return OK along with the value, otherwise return ENOTFOUND.
    // @TODO: Here we might want to also signal that the value was stored using
    // the binary protocol because in that case we have to actually send EBINARY
    // in the text protocol.
    ret = hashtable_take(hashtable, received_command.arg1_size,
                         received_command.arg1, &response_command.arg1_size,
                         &response_command.arg1);
    printf("TAKE hash table result: %d (%s)\n", ret,
           get_hashtable_ret_string(ret));

    if (ret == HT_NOTFOUND) {
      response_command.type = BT_ENOTFOUND;
      assert(response_command.arg1_size == 0);
      assert(response_command.arg1 == NULL);
    } else {
      // Check if the value is valid for a text client.
      if (event_data->connection_type == TEXT &&
          !is_text_representable(response_command.arg1_size,
                                 response_command.arg1)) {
        response_command.type = BT_EBINARY;
      } else {
        response_command.type = BT_OK;
      }
      // arg1_size and arg1 already contain the value size and value for the
      // corresponding key. It should be freed after being sent to the client.
    }
    break;
  case BT_STATS:
    // Return OK along with various statistics about the usage of the cache,
    // namely: number of PUTs, number of DELs, number of GETs, number of TAKEs,
    // number of STATSs, number of KEYs (i.e. key-value pairs) stored.
    response_command.type = BT_OK;
    response_command.arg1 =
        strdup("PUTS=XXX DELS=XXX GETS=XXX TAKES=XXX STATS=XXX KEYS=XXX");
    response_command.arg1_size =
        sizeof("PUTS=XXX DELS=XXX GETS=XXX TAKES=XXX STATS=XXX KEYS=XXX");
    hashtable_print(hashtable);
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

  // At this point the data should've been sent to the client, so we can safely
  // free the pointers in the arguments of the response command (which should be
  // only be the first one).
  // The following requests fill the first argument of the response command with
  // pointers that should be freed upon completion of the response:
  // * GET: pointer to a copy of the value of the requested key, if it exists.
  // * TAKE: pointer to the value of the the requested key, if it existed. The
  //   key-value pair is removed, naturally.
  // * STATS: pointer to the string with the stats.
  command_destroy_args(&response_command);

  // At this point, the received command shouldn't hold any pointers that don't
  // have to be freed immediately. If a new key-value pair was added through a
  // PUT request, both arguments of the received command should be set to NULL
  // so that they're not freed, since they're now managed by the hash table.
  command_destroy_args(&received_command);
}

static void *worker_func(void *worker_args) {
  struct WorkerArgs *args = (struct WorkerArgs *)worker_args;
  struct epoll_event events[MAX_EVENTS];

  printf("Starting worker \"%s\" (epoll %d)\n", args->name, args->epoll_fd);

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

        handle_client(event_data, args->hashtable);
      }
    }
  }

  free(worker_args);

  return NULL;
}

void initialize_workers(int num_workers, pthread_t *worker_threads,
                        int *worker_epoll_fds, struct HashTable *hashtable) {
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
    worker_args->hashtable = hashtable;

    int ret = pthread_create(&worker_threads[i], NULL, worker_func,
                             (void *)worker_args);
    if (ret != 0) {
      perror("pthread_create");
      abort();
    }
  }
}