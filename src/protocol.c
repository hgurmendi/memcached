#include <errno.h>     // for errno
#include <stdio.h>     // for perror
#include <stdlib.h>    // for malloc
#include <string.h>    // for memcpy
#include <sys/types.h> // for ssize_t
#include <unistd.h>    // for write

#include "protocol.h"

// Adds the given client event back to the epoll interest list. Returns 0 if
// successful, -1 otherwise.
int epoll_mod_client(int epoll_fd, struct epoll_event *event,
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

// Writes the given buffer with the given size into the client socket's file
// descriptor, assuming that the amount of bytes in the value pointed at by
// `total_bytes_written` were already sent. If the whole buffer is correctly
// sent then CLIENT_WRITE_SUCCESS is returned and the value pointed at by
// `total_bytes_written` is updated to reflect this. If it's not possible to
// write the whole buffer, the value pointed at by `total_bytes_written` is
// updated to the new amount written and CLIENT_WRITE_INCOMPLETE is returned. If
// an error happens then CLIENT_WRITE_ERROR is returned.
int write_buffer(int fd, char *buffer, size_t buffer_size,
                 size_t *total_bytes_written) {
  while (*total_bytes_written < buffer_size) {
    char *remaining_buffer = buffer + *total_bytes_written;
    size_t remaining_bytes = buffer_size - *total_bytes_written;
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

    *total_bytes_written += nwritten;
  }

  return CLIENT_WRITE_SUCCESS;
}

// reads from the given file descriptor into the given buffer up to the given
// size, keeping track of the total bytes read in total_bytes_read. returns
// CLIENT_READ_ERROR if an error happens, CLIENT_READ_CLOSED if the client
// closes the connection, CLIENT_READ_INCOMPLETE if the file descriptor is not
// yet ready to finish reading, or CLIENT_READ_SUCCESS if the read was
// successfully finished.
int read_buffer(int fd, char *buffer, size_t buffer_size,
                size_t *total_bytes_read) {
  while (*total_bytes_read < buffer_size) {
    // Remaining amount of bytes to read into the buffer.
    size_t remaining_bytes = buffer_size - *total_bytes_read;
    // Pointer to the start of the "empty" read buffer.
    char *remaining_buffer = buffer + *total_bytes_read;

    ssize_t nread = read(fd, remaining_buffer, remaining_bytes);
    if (nread == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // The client is not ready to ready yet.
        return CLIENT_READ_INCOMPLETE;
      }

      // Some other error happened.
      perror("read_buffer read");
      return CLIENT_READ_ERROR;
    } else if (nread == 0) {
      // Client disconnected gracefully.
      return CLIENT_READ_CLOSED;
    }

    // Update the counter.
    *total_bytes_read += nread;
  }

  // We should've finished reading successfully!
  return CLIENT_READ_SUCCESS;
}

#define STATS_CONTENT_MAX_SIZE 256

// Handles the STATS command and mutates the EventData instance accordingly.
void handle_stats(struct EventData *event_data, struct WorkerArgs *args) {
  char stats_content[STATS_CONTENT_MAX_SIZE];
  struct WorkerStats aggregated_stats;

  worker_stats_initialize(&aggregated_stats);
  worker_stats_reduce(args->workers_stats, args->num_workers,
                      &aggregated_stats);
  uint64_t num_keys = hashtable_key_count(args->hashtable);

  int bytes_written =
      snprintf(stats_content, STATS_CONTENT_MAX_SIZE,
               "PUTS=%ld DELS=%ld GETS=%ld TAKES=%ld STATS=%ld KEYS=%ld",
               aggregated_stats.put_count, aggregated_stats.del_count,
               aggregated_stats.get_count, aggregated_stats.take_count,
               aggregated_stats.stats_count, num_keys);

  event_data->response_content =
      hashtable_malloc_evict_bounded_data(args->hashtable, bytes_written);
  if (event_data->response_content == NULL) {
    // Respond with BT_EUNK if the request can't be properly fulfilled due to
    // lack of memory.
    event_data->response_type = BT_EUNK;
  } else {
    memcpy(event_data->response_content->data, stats_content, bytes_written);
    event_data->response_type = BT_OK;
  }

  args->workers_stats[args->worker_id].stats_count++;
}

// Handles the DEL command and mutates the EventData instance accordingly.
// WARNING: does not free the `key` pointer.
void handle_del(struct EventData *event_data, struct WorkerArgs *args,
                struct BoundedData *key) {
  int rv = hashtable_remove(args->hashtable, key);
  if (rv == HT_FOUND) {
    event_data->response_type = BT_OK;
  } else {
    event_data->response_type = BT_ENOTFOUND;
  }
  args->workers_stats[args->worker_id].del_count++;
}

// Handles the GET command and mutates the EventData instance accordingly.
// WARNING: does not free the `key` pointer.
void handle_get(struct EventData *event_data, struct WorkerArgs *args,
                struct BoundedData *key) {
  struct BoundedData *value = NULL;
  int rv = hashtable_get(args->hashtable, key, &value);
  if (rv == HT_FOUND) {
    event_data->response_type = BT_OK;
    event_data->response_content = value;
  } else if (rv == HT_NOTFOUND) {
    event_data->response_type = BT_ENOTFOUND;
  } else {
    event_data->response_type = BT_EUNK;
  }
  args->workers_stats[args->worker_id].get_count++;
}

// Handles the TAKE command and mutates the EventData instance accordingly.
// WARNING: does not free the `key` pointer.
void handle_take(struct EventData *event_data, struct WorkerArgs *args,
                 struct BoundedData *key) {
  struct BoundedData *value = NULL;
  int rv = hashtable_take(args->hashtable, key, &value);
  if (rv == HT_FOUND) {
    event_data->response_type = BT_OK;
    event_data->response_content = value;
  } else {
    event_data->response_type = BT_ENOTFOUND;
  }
  args->workers_stats[args->worker_id].take_count++;
}

// Handles the PUT command and mutates the EventData instance accordingly.
// WARNING: the key and value pointer are owned by the hash table after the
// operation.
void handle_put(struct EventData *event_data, struct WorkerArgs *args,
                struct BoundedData *key, struct BoundedData *value) {
  int rv = hashtable_insert(args->hashtable, key, value);
  if (rv == HT_ERROR) {
    event_data->response_type = BT_EUNK;
  } else {
    event_data->response_type = BT_OK;
    args->workers_stats[args->worker_id].put_count++;
  }
}