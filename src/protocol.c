#include <errno.h>     // for errno
#include <stdio.h>     // for perror
#include <stdlib.h>    // for malloc
#include <sys/types.h> // for ssize_t
#include <unistd.h>    // for write

#include "protocol.h"

// Writes the given buffer with the given size into the client socket's file
// descriptor, assuming that the amount of bytes in the value pointed at by
// `total_bytes_written` were already sent. If the whole buffer is correctly
// sent then CLIENT_WRITE_SUCCESS is returned and the value pointed at by
// `total_bytes_written` is updated to reflect this. If it's not possible to
// write the whole buffer, the value pointed at by `total_bytes_written` is
// updated to the new amount written and CLIENT_WRITE_INCOMPLETE is returned. If
// an error happens then CLIENT_WRITE_ERROR is returned.
int write_buffer(int fd, char *buffer, size_t buffer_length,
                 size_t *total_bytes_written) {
  while (*total_bytes_written < buffer_length) {
    char *remaining_buffer = buffer + *total_bytes_written;
    size_t remaining_bytes = buffer_length - *total_bytes_written;
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

// Handles the STATS command and mutates the EventData instance accordingly.
void handle_stats(struct EventData *event_data, struct HashTable *hashtable) {
  uint64_t num_keys = bd_hashtable_key_count(hashtable);

  // Figure out the size of the buffer first.
  int buffer_size = snprintf(NULL, 0, "PUTS=%ld DELS=%ld GETS=%ld KEYS=%ld",
                             420UL, 69UL, 42069UL, num_keys) +
                    1;
  char *buffer = malloc(buffer_size);
  if (buffer == NULL) {
    perror("handle_stats mallloc");
    abort();
  }
  // Then write to the buffer.
  snprintf(buffer, buffer_size, "PUTS=%ld DELS=%ld GETS=%ld KEYS=%ld", 420UL,
           69UL, 42069UL, num_keys);

  event_data->response_type = BT_OK;
  event_data->response_content =
      bounded_data_create_from_buffer(buffer, buffer_size);
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