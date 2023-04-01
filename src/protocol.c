#include <errno.h>     // for errno
#include <stdio.h>     // for perror
#include <stdlib.h>    // for malloc
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

    // TODO: remove this log after we test the resume behavior.
    if (nwritten != remaining_bytes) {
      printf("UNABLE TO WRITE ALL THE CONTENT, WE HAVE TO CONTINUE TRYING\n");
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

    // TODO: remove this log after we test the resume behavior.
    if (nread != remaining_bytes) {
      printf("UNABLE TO READ ALL THE CONTENT, WE HAVE TO CONTINUE TRYING\n");
    }

    // Update the counter.
    *total_bytes_read += nread;
  }

  // We should've finished reading successfully!
  return CLIENT_READ_SUCCESS;
}

#define STATS_CONTENT_MAX_SIZE 256

// Handles the STATS command and mutates the EventData instance accordingly.
void handle_stats(struct EventData *event_data, struct HashTable *hashtable) {
  char stats_content[STATS_CONTENT_MAX_SIZE];

  // TODO: fill the rest later.
  uint64_t num_puts = 420;
  uint64_t num_dels = 69;
  uint64_t num_gets = 42069;
  uint64_t num_takes = 123;
  uint64_t num_stats = 3010;
  uint64_t num_keys = bd_hashtable_key_count(hashtable);

  int bytes_written =
      snprintf(stats_content, STATS_CONTENT_MAX_SIZE,
               "PUTS=%ld DELS=%ld GETS=%ld TAKES=%ld STATS=%ld KEYS=%ld",
               num_puts, num_dels, num_gets, num_takes, num_stats, num_keys);

  event_data->response_type = BT_OK;
  // The response content doesn't include the trailing '\0'.
  event_data->response_content =
      bounded_data_create_from_buffer_duplicate(stats_content, bytes_written);
}

// Handles the DEL command and mutates the EventData instance accordingly.
// WARNING: does not free the `key` pointer.
void handle_del(struct EventData *event_data, struct HashTable *hashtable,
                struct BoundedData *key) {
  int rv = bd_hashtable_remove(hashtable, key);
  if (rv == HT_FOUND) {
    event_data->response_type = BT_OK;
  } else {
    event_data->response_type = BT_ENOTFOUND;
  }
}

// Handles the GET command and mutates the EventData instance accordingly.
// WARNING: does not free the `key` pointer.
void handle_get(struct EventData *event_data, struct HashTable *hashtable,
                struct BoundedData *key) {
  struct BoundedData *value = NULL;
  int rv = bd_hashtable_get(hashtable, key, &value);
  if (rv == HT_FOUND) {
    event_data->response_type = BT_OK;
    event_data->response_content = value;
  } else {
    event_data->response_type = BT_ENOTFOUND;
  }
}

// Handles the TAKE command and mutates the EventData instance accordingly.
// WARNING: does not free the `key` pointer.
void handle_take(struct EventData *event_data, struct HashTable *hashtable,
                 struct BoundedData *key) {
  struct BoundedData *value = NULL;
  int rv = bd_hashtable_take(hashtable, key, &value);
  if (rv == HT_FOUND) {
    event_data->response_type = BT_OK;
    event_data->response_content = value;
  } else {
    event_data->response_type = BT_ENOTFOUND;
  }
}

// Handles the PUT command and mutates the EventData instance accordingly.
// WARNING: the key and value pointer are owned by the hash table after the
// operation.
void handle_put(struct EventData *event_data, struct HashTable *hashtable,
                struct BoundedData *key, struct BoundedData *value) {
  bd_hashtable_insert(hashtable, key, value);
  event_data->response_type = BT_OK;
}