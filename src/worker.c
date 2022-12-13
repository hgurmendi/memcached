#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll.h"
#include "hashtable/hashtable.h"
#include "parameters.h"
#include "protocol/binary.h"
#include "protocol/command.h"
#include "protocol/text.h"
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

// Creates the string response in the stack, then allocates enough memory for it
// and mutates the `response_size` and `response` pointers if there is no
// problem with the memory allocation.
void generate_stats_response(struct WorkerStats *workers_stats, int num_workers,
                             uint64_t keys_count, uint32_t *response_size,
                             char **response) {
  struct WorkerStats summary;
  char buf[MAX_REQUEST_SIZE];
  int bytes_written = 0;

  worker_stats_merge(workers_stats, num_workers, &summary);

  bytes_written =
      snprintf(buf, MAX_REQUEST_SIZE,
               "PUTS=%lu DELS=%lu GETS=%lu TAKES=%lu STATS=%lu KEYS=%lu",
               summary.put_count, summary.del_count, summary.get_count,
               summary.take_count, summary.stats_count, keys_count);

  char *duplicate = strdup(buf);
  if (duplicate == NULL) {
    perror("strdup stats response");
    return;
  }

  *response = duplicate;
  *response_size = bytes_written + 1;
}

static void print_something(void *value) {
  struct BucketNode *node = (struct BucketNode *)value;
  printf("%s", node->key);
}

/* Handles incoming data from a client connection. If the client closes the
 * connection, we close the file descriptor and epoll manages it accordingly.
 */
static void handle_client(struct ClientEpollEventData *event_data,
                          struct HashTable *hashtable,
                          struct WorkerStats *stats,
                          struct WorkerStats *workers_stats, int num_workers) {
  struct Command received_command;
  struct Command response_command;
  int ret;

  command_initialize(&received_command);
  command_initialize(&response_command);

  if (event_data->connection_type == TEXT) {
    read_command_from_text_client(event_data->fd, &received_command);
  } else {
    read_command_from_binary_client(event_data->fd, &received_command);
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

    stats->put_count += 1;
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

    stats->del_count += 1;
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

    stats->get_count += 1;
    break;
  case BT_TAKE:
    // for testing eviction!
    if (0 ==
        strncmp(received_command.arg1, "EVICT", received_command.arg1_size)) {
      // evit the shit out.
      int eviction_successful = hashtable_evict(hashtable);
      printf("Eviction result: %s\n",
             eviction_successful ? "Success!" : "Failed!");
      break;
    }

    // / for testing eviction

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

    stats->take_count += 1;
    break;
  case BT_STATS:
    // Return OK along with various statistics about the usage of the cache,
    // namely: number of PUTs, number of DELs, number of GETs, number of TAKEs,
    // number of STATSs, number of KEYs (i.e. key-value pairs) stored.
    generate_stats_response(
        workers_stats, num_workers, hashtable_key_count(hashtable),
        &response_command.arg1_size, &response_command.arg1);
    response_command.type = BT_OK;

    if (response_command.arg1_size == 0 || response_command.arg1 == NULL) {
      printf("Error generating the response for the STATS command.\n");

      response_command.type = BT_OK;
      response_command.arg1 = strdup("ERROR GENERATING STATS");
      response_command.arg1_size = sizeof("ERROR GENERATING STATS");
    }

    stats->stats_count += 1;

    hashtable_print(hashtable);

    printf("LRU queue:\n");
    queue_print(hashtable->lru_queue, print_something);

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
    write_command_to_text_client(event_data->fd, &response_command);
  } else {
    printf("Responding binary data\n");
    write_command_to_binary_client(event_data->fd, &response_command);
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

        handle_client(event_data, args->hashtable, args->stats,
                      args->workers_stats, args->num_workers);
      }
    }
  }

  free(worker_args);

  return NULL;
}

void initialize_workers(int num_workers, pthread_t *worker_threads,
                        int *worker_epoll_fds, struct HashTable *hashtable,
                        struct WorkerStats *workers_stats) {
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
    struct WorkerArgs *worker_args = malloc(sizeof(struct WorkerArgs));
    if (worker_args == NULL) {
      perror("malloc worker_args");
      abort();
    }
    snprintf(worker_args->name,
             sizeof(worker_args->name) / sizeof(worker_args->name[0]),
             "Worker %d", i);
    worker_args->epoll_fd = worker_epoll_fds[i];
    worker_args->hashtable = hashtable;
    worker_args->stats = &workers_stats[i];
    worker_args->workers_stats = workers_stats;
    worker_args->num_workers = num_workers;
    worker_stats_initialize(worker_args->stats);

    int ret = pthread_create(&worker_threads[i], NULL, worker_func,
                             (void *)worker_args);
    if (ret != 0) {
      perror("pthread_create");
      abort();
    }
  }
}

/* Initializes an instance of a WorkerStats struct with all its values in
 * zero.
 */
void worker_stats_initialize(struct WorkerStats *stats) {
  stats->put_count = 0;
  stats->del_count = 0;
  stats->get_count = 0;
  stats->take_count = 0;
  stats->stats_count = 0;
}

/* Takes an array of WorkerStats structs and the number of structs in the array
 * and merges (i.e. sums) each member field into the destination struct.
 */
void worker_stats_merge(struct WorkerStats *workers_stats, int num_worker_stats,
                        struct WorkerStats *destination_stats) {
  worker_stats_initialize(destination_stats);
  for (int i = 0; i < num_worker_stats; i++) {
    destination_stats->put_count += workers_stats[i].put_count;
    destination_stats->del_count += workers_stats[i].del_count;
    destination_stats->get_count += workers_stats[i].get_count;
    destination_stats->take_count += workers_stats[i].take_count;
    destination_stats->stats_count += workers_stats[i].stats_count;
  }
}
